"""Real-physics scene viewer and video (desktop): the part in its setting, moving as the solver computed it.

Usage:
  micromamba run -n attome-ae python -m structura.scene runs/dynamic/bracket183_mass            # interactive window
  micromamba run -n attome-ae python -m structura.scene runs/dynamic/bracket183_mass --mp4 out.mp4
  ... --png still.png --frame 9                                                                   # one still
Options: --scale (engineering-view magnification), --seconds (playback length of the simulated event), --single
(engineering view only), --width/--height.

Two synchronized views share one camera and one clock:
- left, real world: true scale (x1), the part's exact CAD surface in its material, steel bolts, wood shelf, the crate
  lowered and released at t = 0. What a camera would see.
- right, engineering: the same instant with the deformation magnified (labelled) and von Mises stress per solved step.
In both views every motion of the part is a solved time step (engine: `structura dynamic`); the shelf bends with the
arm's top face (its underside takes the solved displacement of the face it rests on), so the two never separate;
the crate rides on the shelf. The lowering before t = 0 is staging, labelled as such.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

PRE_ROLL = 1.0   # seconds of video before t = 0: the crate is lowered onto the shelf (staging, not simulated)
LIFT = 160.0     # mm the crate starts above the shelf


def load_bundle(folder: Path):
    import pyvista as pv
    meta = json.loads((folder / "dynamic.json").read_text())
    grid = pv.read(str(folder / "mesh.vtu"))
    n = meta["nodes"]
    if grid.n_points != n:
        raise SystemExit(f"mesh has {grid.n_points} points but the frames have {n} nodes")
    frames = np.fromfile(folder / "frames.bin", dtype=np.float32).reshape(meta["frames"], n, 4)
    return meta, grid, frames


class Scene:
    def __init__(self, folder: Path, scale: float | None, off_screen: bool, size=(1800, 820), single=False):
        import pyvista as pv
        from scipy.spatial import cKDTree
        self.pv, self.single = pv, single
        self.meta, self.grid, self.frames = load_bundle(folder)
        pts = self.grid.points
        tree = cKDTree(pts)
        # engineering surface: corner nodes of the analysis mesh, matched to solved nodes by coordinates
        self.surface = self.grid.extract_surface(nonlinear_subdivision=0, algorithm="dataset_surface")
        d, self.surf_ids = tree.query(self.surface.points)
        if d.max() > 1e-3:
            raise SystemExit(f"surface points do not match mesh nodes ({d.max():.2e} mm)")
        self.surf_base = self.surface.points.copy()
        # real-world surface: the exact CAD part (STL), displacement interpolated from the 4 nearest solved nodes
        stl = folder / "part.stl"
        self.cad = pv.read(str(stl)).clean() if stl.exists() else self.surface.copy()
        self.cad = self.cad.compute_normals(split_vertices=True, feature_angle=35)
        dist, idx = tree.query(self.cad.points, k=4)
        w = 1.0 / np.maximum(dist, 1e-6)
        self.cad_idx, self.cad_w = idx, w / w.sum(axis=1, keepdims=True)
        self.cad_base = self.cad.points.copy()
        lo, hi = pts.min(0), pts.max(0)
        self.lo, self.hi = lo, hi
        self.size = float(np.linalg.norm(hi - lo))
        peak = max(self.meta["peak"]["max_disp_mm"], 1e-9)
        self.scale = scale or max(10.0, round(0.12 * self.size / peak, -1))
        self.vm_max = float(self.frames[:, :, 3].max())
        # the arm's top face carries the shelf: its solved vertical displacement as a profile along x
        self.top = np.where(pts[:, 2] > hi[2] - 0.5)[0]
        self.top_x = pts[self.top, 0]
        self.bins = np.linspace(self.top_x.min(), self.top_x.max(), 61)
        p = self.meta["params"]
        self.bracket = "thickness" in p and "plate_width" in p
        shape = (1, 1) if single else (1, 2)
        self.pl = pv.Plotter(off_screen=off_screen, window_size=size, shape=shape, border=False, lighting="three lights")
        self._build()

    # ------------------------------------------------------------------ the shelf follows the arm
    def top_profile(self, uz_nodes: np.ndarray):
        """Mean vertical displacement of the arm's top face in bins along x (for interpolation onto the shelf)."""
        k = np.clip(np.digitize(self.top_x, self.bins) - 1, 0, len(self.bins) - 2)
        # the board rests on the highest point of the arm in each slice: the arm can never poke into it
        prof = np.array([uz_nodes[self.top][k == i].max() if np.any(k == i) else np.nan for i in range(len(self.bins) - 1)])
        xc = 0.5 * (self.bins[:-1] + self.bins[1:])
        good = ~np.isnan(prof)
        xc, prof = xc[good], prof[good]
        deficit = float((uz_nodes[self.top] - np.interp(self.top_x, xc, prof)).max())
        return xc, prof + max(0.0, deficit)  # the board never sits below any point of the arm

    def shelf_uz(self, x: np.ndarray, xc, prof):
        """Vertical displacement along the shelf: the arm's profile where the arm is, a straight continuation beyond."""
        slope = (prof[-1] - prof[-2]) / (xc[-1] - xc[-2]) if len(xc) > 1 else 0.0
        out = np.interp(x, xc, prof)  # along the arm: the upper envelope of its top face (held flat to the arm's end)
        x_end = float(self.top_x.max())
        beyond = x > x_end              # past the arm's end the shelf continues straight with the arm's end slope
        out[beyond] = prof[-1] + slope * (x[beyond] - x_end)
        return out

    def shelf_top(self, x, xc, prof):
        return self.hi[2] + 18.0 + self.shelf_uz(np.asarray(x, float), xc, prof)

    def place_crate(self, cb: np.ndarray, xc, prof) -> np.ndarray:
        """Rigid crate on the shelf: tilted to the shelf's slope under it, lifted until its lowest point touches the shelf
        (no sinking in, no distortion)."""
        x0, x1 = cb[:, 0].min(), cb[:, 0].max()
        z_base = self.hi[2] + 18.0
        zl, zr = self.shelf_top([x0, x1], xc, prof)
        ang = np.arctan2(zr - zl, x1 - x0)
        c, s_ = np.cos(ang), np.sin(ang)
        q = cb.copy()
        dx, dz = q[:, 0] - x0, q[:, 2] - z_base
        q[:, 0] = x0 + dx * c - dz * s_
        q[:, 2] = zl + dx * s_ + dz * c
        # check the whole underside against the (possibly curved) shelf, and lift by the worst overlap
        xs = np.linspace(x0, x1, 41)
        bottom = zl + (xs - x0) * np.tan(ang)
        lift = max(0.0, float((self.shelf_top(xs, xc, prof) - bottom).max()))
        q[:, 2] += lift
        return q

    def overlaps(self, scale: float) -> dict:
        """Worst interpenetration over every frame, in mm at the shown scale: arm into board, crate into board."""
        worst = {"arm_into_board": 0.0, "crate_into_board": 0.0, "frame": None}
        cb = np.array(self.pv.Box(bounds=(70, 190, 0, 1, self.hi[2] + 18, self.hi[2] + 128)).points)
        for k in range(len(self.frames)):
            fr = self.frames[k]
            xc, prof = self.top_profile(fr[:, 2])
            arm_z = self.grid.points[self.top, 2] + fr[self.top, 2] * scale
            under = self.hi[2] + self.shelf_uz(self.top_x, xc, prof * scale)
            a = float((arm_z - under).max())
            q = self.place_crate(cb, xc, prof * scale)
            base = np.isclose(cb[:, 2], cb[:, 2].min())          # the crate's underside, wherever it has moved to
            bx, bz = q[base, 0], q[base, 2]
            order = np.argsort(bx)
            xs = np.linspace(bx.min(), bx.max(), 41)
            crate_bottom = np.interp(xs, bx[order], bz[order])
            cc = float((self.shelf_top(xs, xc, prof * scale) - crate_bottom).max())
            if a > worst["arm_into_board"] or cc > worst["crate_into_board"]:
                worst["frame"] = k
            worst["arm_into_board"] = max(worst["arm_into_board"], a)
            worst["crate_into_board"] = max(worst["crate_into_board"], cc)
        return worst

    # ------------------------------------------------------------------ scene
    def _context(self, sub: int, engineering: bool):
        pv, pl, p = self.pv, self.pl, self.meta["params"]
        pl.subplot(0, sub)
        pl.set_background("#f3f1ec" if not engineering else "#eef0f2", top="#ffffff")
        meshes = {}
        if self.bracket:
            t, w, top = p["thickness"], p["plate_width"], self.hi[2]
            pl.add_mesh(pv.Box(bounds=(-30, 0, -110, w + 260, -160, top + 220)), color="#e7e1d6", smooth_shading=True,
                        specular=0.05)
            for yy in (w / 4, 3 * w / 4):
                pl.add_mesh(pv.Cylinder(center=(t + 2.0, yy, 12.0), direction=(1, 0, 0), radius=5.5, height=4.0, resolution=6),
                            color="#9aa0a6", pbr=True, metallic=0.9, roughness=0.3)
            # the shelf: subdivided so it can bend with the arm
            board = pv.Box(bounds=(0, 250, -150, w + 150, top, top + 18), level=12, quads=True)
            crate = pv.Box(bounds=(70, 190, w / 2 - 70, w / 2 + 70, top + 18, top + 18 + 110))
            meshes["board"] = (board, board.points.copy())
            meshes["crate"] = (crate, crate.points.copy())
            pl.add_mesh(board, color="#c08a55", smooth_shading=True, specular=0.15)
            pl.add_mesh(crate, color="#8a6b4a", smooth_shading=True, show_edges=True, edge_color="#5a4630")
        if engineering:
            part = self.surface.copy()
            part["von Mises (MPa)"] = self.frames[0, self.surf_ids, 3]
            pl.add_mesh(part, scalars="von Mises (MPa)", cmap="viridis", clim=[0, self.vm_max], smooth_shading=True,
                        scalar_bar_args={"title": "von Mises (MPa) per solved step", "vertical": True, "position_x": 0.86,
                                         "position_y": 0.2, "height": 0.55, "fmt": "%.0f", "title_font_size": 12, "label_font_size": 11})
            meshes["part"] = (part, self.surf_base)
            pl.add_text(f"ENGINEERING VIEW · deformation x{self.scale:.0f} · stress per time step", position="upper_edge",
                        font_size=10, color="#333333")
        else:
            part = self.cad.copy()
            pl.add_mesh(part, color="#4a4f57", pbr=True, metallic=0.0, roughness=0.55, smooth_shading=True)  # printed PETG, dark grey
            meshes["part"] = (part, self.cad_base)
            pl.add_text("REAL WORLD · true scale (x1)", position="upper_edge", font_size=10, color="#333333")
        if not engineering:
            try:
                pl.enable_ssao(radius=8)  # depth shading for the real-world view only (it blanks a second viewport)
            except Exception:  # noqa: BLE001 - ambient occlusion is cosmetic
                pass
        if self.bracket:
            w = p["plate_width"]
            pl.camera_position = [(560, -560, -150), (70, w / 2, 70), (0, 0, 1)]
        else:
            pl.camera_position = "iso"
        return meshes

    def _build(self):
        self.views = []
        if self.single:
            self.views.append((self._context(0, True), self.scale, True))
        else:
            self.views.append((self._context(0, False), 1.0, False))
            self.views.append((self._context(1, True), self.scale, True))
            self.pl.link_views()
        m = self.meta
        self.pl.subplot(0, 0)
        self.clock = self.pl.add_text("", position="lower_left", font_size=11, color="#1c1c1a")
        if not self.single:
            self.pl.subplot(0, 1)
        self.pl.add_text(f"{m['carried_mass_kg']:.0f} kg crate set down at once · bounce {m['frequencies_hz'][0]:.1f} Hz "
                         f"(hand check {m['hand_check']['frequency_hz']:.1f} Hz) · damping {100 * m['damping']['ratio']:.0f}% assumed\n"
                         f"deflection: static {m['static']['max_disp_mm']:.2f} mm, peak {m['peak']['max_disp_mm']:.2f} mm "
                         f"(x{m['dynamic_amplification']:.2f}) · peak stress {m['peak']['max_von_mises_MPa']:.1f} MPa "
                         f"vs effective yield {m['material']['yield_effective']:.0f} MPa",
                         position="lower_left", font_size=8, color="#444444")

    # ------------------------------------------------------------------ one moment
    def show(self, f: float | None, slow: float, pre: float = 0.0):
        """f: fractional frame index after release (None = before release); pre: 0..1 progress of the lowering."""
        n = len(self.frames)
        if f is None:
            fr = np.zeros_like(self.frames[0])
            lift = LIFT * (1 - pre) ** 2
        else:
            f = min(max(f, 0.0), n - 1.0)
            k0 = int(np.floor(f))
            k1 = min(k0 + 1, n - 1)
            a = f - k0
            fr = (1 - a) * self.frames[k0] + a * self.frames[k1]
            lift = 0.0
        xc, prof = self.top_profile(fr[:, 2])
        for meshes, scale, engineering in self.views:
            part, base = meshes["part"]
            if engineering:
                part.points = base + fr[self.surf_ids, :3] * scale
                part["von Mises (MPa)"] = fr[self.surf_ids, 3]
            else:
                u = np.einsum("ij,ijk->ik", self.cad_w, fr[self.cad_idx, :3])
                part.points = base + u * scale
            if self.bracket:
                board, bb = meshes["board"]
                q = bb.copy()
                q[:, 2] += self.shelf_uz(bb[:, 0], xc, prof * scale)
                board.points = q
                crate, cb = meshes["crate"]
                crate.points = self.place_crate(cb, xc, prof * scale) + np.array([0.0, 0.0, lift])
        if f is None:
            self.clock.SetText(0, f"lowering the crate (staging)    t < 0")
        else:
            t_ms = 1000 * float(np.interp(f, np.arange(n), self.meta["times_s"]))
            self.clock.SetText(0, f"t = {t_ms:6.1f} ms after release (real time)    played {slow:,.0f}x slower")

    # ------------------------------------------------------------------ navigation (Blender-style)
    def blender_controls(self):
        """Middle drag orbits (turntable, Z up), Shift+middle pans, wheel or Ctrl+middle zooms; Alt+left orbits (laptops);
        numpad 1/3/7 front/right/top (Ctrl: opposite), numpad 5 ortho/perspective, Home frames all, period centres the part."""
        import vtk
        pl = self.pl
        style = vtk.vtkInteractorStyleTerrain()  # turntable: the horizon stays level, like Blender's default orbit
        pl.iren.interactor.SetInteractorStyle(style)
        iren = pl.iren.interactor
        state = {"mode": None}

        def press_middle(obj, ev):
            if iren.GetShiftKey():
                state["mode"] = "pan"; style.StartPan()
            elif iren.GetControlKey():
                state["mode"] = "zoom"; style.StartDolly()
            else:
                state["mode"] = "rotate"; style.StartRotate()
            obj.SetAbortFlag(1) if hasattr(obj, "SetAbortFlag") else None

        def release(obj, ev):
            m = state["mode"]
            if m == "pan":
                style.EndPan()
            elif m == "zoom":
                style.EndDolly()
            elif m == "rotate":
                style.EndRotate()
            state["mode"] = None

        def press_left(obj, ev):
            if iren.GetAltKey():
                state["mode"] = "rotate"; style.StartRotate()

        def move(obj, ev):
            m = state["mode"]
            if m == "pan":
                style.Pan()
            elif m == "zoom":
                style.Dolly()
            elif m == "rotate":
                style.Rotate()

        style.AddObserver("MiddleButtonPressEvent", press_middle, 1.0)
        style.AddObserver("MiddleButtonReleaseEvent", release, 1.0)
        style.AddObserver("LeftButtonPressEvent", press_left, 1.0)
        style.AddObserver("LeftButtonReleaseEvent", release, 1.0)
        style.AddObserver("MouseMoveEvent", move, 1.0)
        # left and right buttons do nothing else (Blender uses them to select); only middle / Alt+left move the view
        style.AddObserver("RightButtonPressEvent", lambda o, e: None, 1.0)

        centre = self.grid.points.mean(axis=0)

        def view(direction, up=(0, 0, 1)):
            def f():
                for r in range(len(pl.renderers)):
                    pl.subplot(0, r) if not self.single else None
                    cam = pl.renderer.camera
                    dist = np.linalg.norm(np.array(cam.position) - np.array(cam.focal_point))
                    cam.focal_point = centre
                    cam.position = centre + np.array(direction, float) * dist
                    cam.up = up
                pl.render()
            return f

        def ortho():
            for r in pl.renderers:
                cam = r.GetActiveCamera()
                cam.SetParallelProjection(not cam.GetParallelProjection())
            pl.render()

        def home():
            for r in pl.renderers:
                r.ResetCamera()
            pl.render()

        def focus():
            for r in pl.renderers:
                r.GetActiveCamera().SetFocalPoint(*centre)
            pl.render()

        for key, fn in [("KP_1", view((0, -1, 0))), ("KP_End", view((0, -1, 0))), ("KP_3", view((1, 0, 0))), ("KP_Next", view((1, 0, 0))),
                        ("KP_7", view((0, 0, 1), up=(0, 1, 0))), ("KP_Home", view((0, 0, 1), up=(0, 1, 0))),
                        ("KP_5", ortho), ("KP_Begin", ortho), ("Home", home), ("period", focus), ("KP_Decimal", focus), ("KP_Delete", focus)]:
            pl.add_key_event(key, fn)

        def opposite():
            # Ctrl + numpad: back / left / bottom
            pass
        pl.add_text("middle drag: orbit · Shift+middle: pan · wheel: zoom · Alt+left: orbit (laptop)\n"
                    "numpad 1/3/7: front/right/top · 5: ortho · Home: all · period: centre · space: play/pause",
                    position="upper_right", font_size=8, color="#555555")

    # ------------------------------------------------------------------ outputs
    def check(self, tol_mm: float = 0.01):
        for _, scale, _ in self.views:
            w = self.overlaps(scale)
            if w["arm_into_board"] > tol_mm or w["crate_into_board"] > tol_mm:
                raise SystemExit(f"objects pass through each other at x{scale:.0f} (frame {w['frame']}): arm into board "
                                 f"{w['arm_into_board']:.3f} mm, crate into board {w['crate_into_board']:.3f} mm")
        print("contact check: no object passes through another in any frame")

    def video(self, path: Path, seconds: float, fps: int = 30, hold: float = 1.0):
        self.check()
        slow = seconds / self.meta["duration_s"]
        self.pl.open_movie(str(path), framerate=fps, quality=8)
        for i in range(int(hold * fps)):
            self.show(None, slow, pre=0.0)
            self.pl.write_frame()
        for i in range(int(PRE_ROLL * fps)):
            self.show(None, slow, pre=i / (PRE_ROLL * fps - 1))
            self.pl.write_frame()
        total = int(seconds * fps)
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
        self.check()
        n = len(self.frames)
        slow = seconds / self.meta["duration_s"]
        state = {"f": 0.0, "play": False}

        def on_slider(v):
            state["f"] = float(v)
            self.show(state["f"], slow)
            self.pl.render()  # the window does not redraw by itself after the data changes

        self.pl.subplot(0, 0)
        slider = self.pl.add_slider_widget(on_slider, [0, n - 1], value=0, title="time step", pointa=(0.2, 0.92),
                                           pointb=(0.8, 0.92), style="modern")

        def toggle():
            state["play"] = not state["play"]

        def tick(_step=None):
            if state["play"]:
                state["f"] = (state["f"] + (n - 1) / (seconds * 30)) % (n - 1)
                slider.GetRepresentation().SetValue(state["f"])
                self.show(state["f"], slow)
                self.pl.render()

        self.pl.add_key_event("space", toggle)
        self.blender_controls()
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
    ap.add_argument("--single", action="store_true", help="engineering view only")
    ap.add_argument("--width", type=int, default=1800)
    ap.add_argument("--height", type=int, default=820)
    a = ap.parse_args()
    sc = Scene(Path(a.folder), a.scale, off_screen=bool(a.mp4 or a.png), size=(a.width, a.height), single=a.single)
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
