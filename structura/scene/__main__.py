"""Real-physics scene viewer and video (desktop): the part in its setting, moving as the solver computed it.

Usage:
  micromamba run -n attome-ae python -m structura.scene runs/dynamic/bracket183            # interactive window
  micromamba run -n attome-ae python -m structura.scene runs/dynamic/bracket183 --mp4 out.mp4
Options: --scale (deformation magnification, default: peak shown as about 4% of the part size), --seconds (video
length of the simulated event), --width/--height.

Reads the engine's dynamic bundle (`structura dynamic ...`): mesh.vtu, frames.bin, dynamic.json. Every frame of the
part's motion and stress is a solved time step; the wall, shelf board and crate are context, placed from the design
parameters and moved rigidly with the arm end's computed deflection. On screen: real time, slow-motion factor,
deformation magnification.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np


def load_bundle(folder: Path):
    import pyvista as pv
    meta = json.loads((folder / "dynamic.json").read_text())
    grid = pv.read(str(folder / "mesh.vtu"))
    n = meta["nodes"]
    if grid.n_points != n:
        raise SystemExit(f"mesh has {grid.n_points} points but the frames have {n} nodes")
    frames = np.fromfile(folder / "frames.bin", dtype=np.float32).reshape(meta["frames"], n, 4)
    return meta, grid, frames


def nodes_in_box(points: np.ndarray, bbox, tol=0.5) -> np.ndarray:
    lo, hi = np.array(bbox[:3]) - tol, np.array(bbox[3:]) + tol
    return np.where(np.all((points >= lo) & (points <= hi), axis=1))[0]


class Scene:
    """Bracket on a wall with a shelf board and a 20 kg crate; other families get the part and markers only."""

    def __init__(self, folder: Path, scale: float | None, off_screen: bool, size=(1600, 900)):
        import pyvista as pv
        self.pv = pv
        self.meta, self.grid, self.frames = load_bundle(folder)
        self.surface = self.grid.extract_surface(nonlinear_subdivision=0, algorithm="dataset_surface")  # corner nodes only
        # map surface points to solved nodes by coordinates: vtkOriginalPointIds from this filter does not point at the
        # right points for quadratic cells (checked: 0.04% agree), coordinates match to 1e-5 mm
        from scipy.spatial import cKDTree
        dist, self.surf_ids = cKDTree(self.grid.points).query(self.surface.points)
        if dist.max() > 1e-3:
            raise SystemExit(f"surface points do not match mesh nodes (max distance {dist.max():.2e} mm)")
        self.base = self.surface.points.copy()
        pts = self.grid.points
        lo, hi = pts.min(0), pts.max(0)
        self.size = float(np.linalg.norm(hi - lo))
        peak = max(self.meta["peak"]["max_disp_mm"], 1e-9)
        self.scale = scale or round(0.15 * self.size / peak, -1) or 10.0
        self.vm_max = float(self.frames[:, :, 3].max())
        load = next((m for m in self.meta["markers"] if m["kind"] == "load"), None)
        self.load_nodes = nodes_in_box(pts, load["bbox"]) if load else np.array([], int)
        self.top_nodes = np.where(pts[:, 2] > hi[2] - 0.5)[0]  # the arm's top face: the shelf board rests on it
        self.lo, self.hi = lo, hi
        self.pl = pv.Plotter(off_screen=off_screen, window_size=size, lighting="three lights")
        self.pl.set_background("#eef0f2", top="#ffffff")
        self._build()

    # ------------------------------------------------------------------ context
    def _build(self):
        pv, pl, p = self.pv, self.pl, self.meta["params"]
        lo, hi = self.lo, self.hi
        part = self.surface.copy()
        part["von Mises (MPa)"] = self.frames[0, self.surf_ids, 3]
        self.part = part
        pl.add_mesh(part, scalars="von Mises (MPa)", cmap="viridis", clim=[0, self.vm_max], smooth_shading=True,
                    scalar_bar_args={"title": "von Mises (MPa), solved per time step", "vertical": True,
                                     "position_x": 0.88, "position_y": 0.25, "height": 0.5, "fmt": "%.1f"},
                    specular=0.3, specular_power=20)
        self.bracket = "thickness" in p and "plate_width" in p
        if self.bracket:
            t, w = p["thickness"], p["plate_width"]
            top = hi[2]
            # wall behind the wall plate (the -x face sits at x = 0)
            pl.add_mesh(pv.Box(bounds=(-30, 0, -110, w + 260, -160, top + 220)), color="#e4dfd6", smooth_shading=True, specular=0.05)
            # two bolt heads at the family's hole positions (plate_width / 4 and 3 / 4, 12 mm up)
            for yy in (w / 4, 3 * w / 4):
                pl.add_mesh(pv.Cylinder(center=(t + 2.0, yy, 12.0), direction=(1, 0, 0), radius=5.5, height=4.0, resolution=6),
                            color="#7d8288", metallic=0.8, roughness=0.35, pbr=True)
            # shelf board on the arm and a 20 kg crate on it, near the arm end where the load acts
            self.board = pv.Box(bounds=(0, 250, -150, w + 150, top, top + 18))
            self.crate = pv.Box(bounds=(70, 190, w / 2 - 70, w / 2 + 70, top + 18, top + 18 + 110))
            self.board_base, self.crate_base = self.board.points.copy(), self.crate.points.copy()
            pl.add_mesh(self.board, color="#b98a5a", smooth_shading=True, specular=0.1)
            pl.add_mesh(self.crate, color="#8a6b4a", smooth_shading=True, specular=0.05, show_edges=True, edge_color="#5a4630")
            # side view at shelf height: the bracket's L profile and its stress face the camera, wall on the left
            pl.camera_position = [(560, -560, -150), (70, w / 2, 70), (0, 0, 1)]  # below the shelf, three-quarter view
        else:
            pl.camera_position = "iso"
        pl.enable_anti_aliasing("ssaa")
        m = self.meta
        self.info = pl.add_text("", position="upper_left", font_size=11, color="#1c1c1a")
        pl.add_text(f"{m['spec']} · {m['event']}\n"
                    f"first frequencies {', '.join(f'{f:.0f}' for f in m['frequencies_hz'])} Hz · damping {100 * m['damping']['ratio']:.0f}% (assumed)\n"
                    f"static deflection {m['static']['max_disp_mm']:.2f} mm · peak {m['peak']['max_disp_mm']:.2f} mm "
                    f"(dynamic amplification {m['dynamic_amplification']:.2f}) · peak stress {m['peak']['max_von_mises_MPa']:.1f} MPa "
                    f"vs effective yield {m['material']['yield_effective']:.0f} MPa",
                    position="lower_left", font_size=9, color="#444444")

    # ------------------------------------------------------------------ one moment
    def show(self, f: float, slow: float):
        """f: fractional frame index (frames are interpolated linearly between solved steps)."""
        n = len(self.frames)
        f = min(max(f, 0.0), n - 1.0)
        k0 = int(np.floor(f))
        k1 = min(k0 + 1, n - 1)
        a = f - k0
        fr = (1 - a) * self.frames[k0] + a * self.frames[k1]
        u = fr[self.surf_ids, :3] * self.scale
        self.part.points = self.base + u
        self.part["von Mises (MPa)"] = fr[self.surf_ids, 3]
        if self.bracket and len(self.top_nodes) > 2:
            # the board rests on the arm's top face: fit the solved vertical displacement there as offset + slope along x,
            # magnified like the part, and move the board (and the crate on it) with that line
            xs = self.grid.points[self.top_nodes, 0]
            slope, offset = np.polyfit(xs, fr[self.top_nodes, 2] * self.scale, 1)
            ang = np.arctan(slope)
            c, s = np.cos(ang), np.sin(ang)
            for mesh, base in ((self.board, self.board_base), (self.crate, self.crate_base)):
                q = base.copy()
                x, z = q[:, 0].copy(), q[:, 2] - self.hi[2]
                q[:, 0] = x * c - z * s
                q[:, 2] = self.hi[2] + offset + x * s + z * c
                mesh.points = q
        t_ms = 1000 * float(np.interp(f, np.arange(n), self.meta["times_s"]))
        self.info.SetText(2, f"t = {t_ms:6.2f} ms (real time)    slowed {slow:,.0f}x    deformation shown x{self.scale:.0f}")

    # ------------------------------------------------------------------ outputs
    def video(self, path: Path, seconds: float, fps: int = 30, hold: float = 0.8):
        slow = seconds / self.meta["duration_s"]
        self.pl.open_movie(str(path), framerate=fps, quality=8)
        total = int(seconds * fps)
        for _ in range(int(hold * fps)):  # the scene at rest before the load arrives
            self.show(0, slow)
            self.pl.write_frame()
        for i in range(total):
            self.show(i / max(total - 1, 1) * (len(self.frames) - 1), slow)
            self.pl.write_frame()
        for _ in range(int(hold * fps)):
            self.pl.write_frame()
        self.pl.close()

    def still(self, path: Path, f: float, seconds: float):
        self.show(f, seconds / self.meta["duration_s"])
        self.pl.screenshot(str(path))
        self.pl.close()

    def interactive(self, seconds: float):
        n = len(self.frames)
        slow = seconds / self.meta["duration_s"]
        state = {"f": 0.0, "play": False}

        def on_slider(v):
            state["f"] = float(v)
            self.show(state["f"], slow)

        slider = self.pl.add_slider_widget(on_slider, [0, n - 1], value=0, title="time step", pointa=(0.25, 0.08),
                                           pointb=(0.75, 0.08), style="modern")

        def toggle():
            state["play"] = not state["play"]

        def tick(_step=None):
            if state["play"]:
                state["f"] = (state["f"] + (n - 1) / (seconds * 30)) % (n - 1)
                slider.GetRepresentation().SetValue(state["f"])
                self.show(state["f"], slow)
                self.pl.render()

        self.pl.add_key_event("space", toggle)
        self.pl.add_text("space: play / pause · drag: rotate · slider: time", position="upper_right", font_size=9)
        self.show(0, slow)
        self.pl.add_timer_event(max_steps=10_000_000, duration=33, callback=tick)
        self.pl.show()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--mp4")
    ap.add_argument("--png", help="write one still image (with --frame) instead of a video")
    ap.add_argument("--frame", type=float, default=0)
    ap.add_argument("--scale", type=float)
    ap.add_argument("--seconds", type=float, default=8.0, help="playback length of the simulated event")
    ap.add_argument("--width", type=int, default=1600)
    ap.add_argument("--height", type=int, default=900)
    a = ap.parse_args()
    sc = Scene(Path(a.folder), a.scale, off_screen=bool(a.mp4 or a.png), size=(a.width, a.height))
    if a.png:
        sc.still(Path(a.png), a.frame, a.seconds)
        print(f"wrote {a.png}")
    elif a.mp4:
        sc.video(Path(a.mp4), a.seconds)
        print(f"wrote {a.mp4}")
    else:
        sc.interactive(a.seconds)


if __name__ == "__main__":
    main()
