"""Assembly animation (Level 2): the planner's order played in 3D, each part travelling in along its insertion path.

Usage:
  micromamba run -n attome-ae python -m structura.scene.assembly runs/assembly/shelf            # interactive window
  micromamba run -n attome-ae python -m structura.scene.assembly runs/assembly/shelf --mp4 out.mp4

Reads the planner's output folder (`attome_assembly plan ... --out DIR`): plan.json and one STL per part, in place.
The paths are the planner's straight insertion moves, which it proved collision-free on the exact geometry; bolts
turn as they go in (two turns per step, for legibility). Fixed parts (the wall) are always shown. A time slider runs
over the whole sequence; space plays and pauses; Blender-style navigation as in the physics scene.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

COLORS = {"wall": "#e4dfd6", "bracket": "#4a4f57", "shelf_board": "#c08a55", "crate": "#8a6b4a"}


def ease(t: float) -> float:
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


class AssemblyScene:
    def __init__(self, folder: Path, off_screen: bool, size=(1500, 900)):
        import pyvista as pv
        self.pv = pv
        self.plan = json.loads((folder / "plan.json").read_text())
        if not self.plan.get("ok"):
            raise SystemExit(f"no assembly plan: {self.plan.get('message') or self.plan.get('error')} {self.plan.get('locked') or ''}")
        self.steps = self.plan["steps"]
        self.desc = {d["name"]: d for d in self.plan["part_list"]}
        self.pl = pv.Plotter(off_screen=off_screen, window_size=size, lighting="three lights")
        self.pl.set_background("#f3f1ec", top="#ffffff")
        self.parts = {}
        for name, d in self.desc.items():
            mesh = pv.read(str(folder / f"{name}.stl")).clean()
            color = d.get("color") or COLORS.get(name) or ("#9aa0a6" if d.get("fastener") else "#7a8088")
            metal = d["kind"] == "bolt"
            actor = self.pl.add_mesh(mesh, color=color, smooth_shading=False, pbr=metal, metallic=0.9 if metal else 0.0,
                                     roughness=0.3 if metal else 0.6)
            actor.GetMapper().SetInputData(mesh)  # draw this mesh itself, so moving it moves what is shown
            self.parts[name] = {"mesh": mesh, "base": mesh.points.copy(), "actor": actor, "kind": d["kind"]}
        self.step_of = {s["part"]: s for s in self.steps}
        self.label = self.pl.add_text("", position="upper_left", font_size=12, color="#1c1c1a")
        self.pl.add_text("assembly order planned on the exact geometry: every move is collision-free",
                         position="lower_left", font_size=9, color="#555555")
        # the insertion path of the part now moving: a line from where it starts to where it ends, and an arrow head
        self.path = pv.PolyData(np.zeros((2, 3)), lines=np.array([2, 0, 1]))
        self.path_actor = self.pl.add_mesh(self.path, color="#d9480f", line_width=3)
        self.path_actor.GetMapper().SetInputData(self.path)
        self.head = pv.Cone(direction=(1, 0, 0), height=8, radius=3, resolution=24).triangulate()
        self.head_base = self.head.points.copy()
        self.head_actor = self.pl.add_mesh(self.head, color="#d9480f")
        self.head_actor.GetMapper().SetInputData(self.head)
        # frame the parts that move (the wall is context, not the subject)
        movable = [self.parts[s["part"]]["mesh"].bounds for s in self.steps]
        lo = np.min([[b[0], b[2], b[4]] for b in movable], axis=0)
        hi = np.max([[b[1], b[3], b[5]] for b in movable], axis=0)
        c = (lo + hi) / 2
        r = float(np.linalg.norm(hi - lo))
        self.centre = c
        self.pl.camera_position = [tuple(c + np.array([1.1, -1.3, 0.75]) * r), tuple(c), (0, 0, 1)]

    def show(self, t: float):
        """t runs from 0 (everything apart) to len(steps) (assembled); step k moves during [k-1, k]."""
        n = len(self.steps)
        t = min(max(t, 0.0), float(n))
        for name, p in self.parts.items():
            s = self.step_of.get(name)
            if s is None:  # fixed part: always in place
                continue
            k = s["step"]
            a = ease(t - (k - 1))  # 0 before its step, 1 after
            d = np.array(s["insert_direction"], float)
            offset = -d * s["travel_mm"] * (1 - a)  # starts back along its path, ends in place
            pts = p["base"].copy()
            if p["kind"] == "bolt" and a < 1:  # turn about its own axis while going in
                c = p["base"].mean(axis=0)
                ang = 2 * 2 * np.pi * (1 - a)
                ax = d / np.linalg.norm(d)
                r = pts - c
                cosang, sinang = np.cos(ang), np.sin(ang)
                pts = c + r * cosang + np.cross(ax, r) * sinang + np.outer(r @ ax, ax) * (1 - cosang)
            p["mesh"].points = pts + offset
            p["mesh"].Modified()
            p["actor"].SetVisibility(t > k - 1 - 1e-9 or k == 1)  # a part appears when its step starts
        k = min(int(np.floor(t)) + 1, n)
        s = self.steps[k - 1]
        moving = t < n
        if moving:
            p = self.parts[s["part"]]
            d = np.array(s["insert_direction"], float)
            end = p["base"].mean(axis=0)
            start = end - d * s["travel_mm"]
            self.path.points = np.array([start, end])
            self.path.Modified()
            # cone points along +x by construction: rotate it onto d and put its tip just short of the end point
            x = np.array([1.0, 0, 0])
            v = np.cross(x, d)
            sv, cv = np.linalg.norm(v), float(x @ d)
            if sv < 1e-9:
                R = np.eye(3) if cv > 0 else np.diag([-1.0, -1.0, 1.0])
            else:
                K = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
                R = np.eye(3) + K + K @ K * ((1 - cv) / sv**2)
            self.head.points = self.head_base @ R.T + (start + d * (0.55 * s["travel_mm"]))
            self.head.Modified()
        self.path_actor.SetVisibility(moving)
        self.head_actor.SetVisibility(moving)
        d = s["insert_direction"]
        axis = {(1, 0, 0): "+x", (-1, 0, 0): "-x", (0, 1, 0): "+y", (0, -1, 0): "-y", (0, 0, 1): "up", (0, 0, -1): "down"}.get(
            tuple(int(round(v)) for v in d), str(d))
        verb = "drive in" if self.desc[s["part"]]["kind"] == "bolt" else ("lower onto place" if axis == "down" else "push in")
        self.label.SetText(2, f"step {k} of {n}: {verb} {s['part'].replace('_', ' ')} (moving {axis})" if t < n else
                           f"assembled: {n} steps")

    def video(self, path: Path, seconds_per_step: float = 1.6, fps: int = 30):
        n = len(self.steps)
        self.pl.open_movie(str(path), framerate=fps, quality=8)
        frames = int(n * seconds_per_step * fps)
        for i in range(frames + fps):
            self.show(min(n, i / (seconds_per_step * fps)))
            self.pl.write_frame()
        self.pl.close()

    def interactive(self):
        from structura.scene.__main__ import blender_navigation
        n = len(self.steps)
        state = {"t": 0.0, "play": False}

        def on_slider(v):
            state["t"] = float(v)
            self.show(state["t"])
            self.pl.render()

        slider = self.pl.add_slider_widget(on_slider, [0, n], value=0, title="assembly step", pointa=(0.2, 0.92), pointb=(0.8, 0.92),
                                           style="modern", interaction_event="always")

        def toggle():
            state["play"] = not state["play"]

        def tick(_step=None):
            if state["play"]:
                state["t"] = (state["t"] + 1 / 45) % (n + 0.5)
                slider.GetRepresentation().SetValue(min(state["t"], n))
                self.show(state["t"])
                self.pl.render()

        self.pl.add_key_event("space", toggle)
        blender_navigation(self.pl, self.centre)
        self.show(0)
        self.pl.add_timer_event(max_steps=10_000_000, duration=33, callback=tick)
        self.pl.show()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--mp4")
    ap.add_argument("--png")
    ap.add_argument("--t", type=float, default=0.0)
    a = ap.parse_args()
    sc = AssemblyScene(Path(a.folder), off_screen=bool(a.mp4 or a.png))
    if a.png:
        sc.show(a.t)
        sc.pl.screenshot(a.png)
        print(f"wrote {a.png}")
    elif a.mp4:
        sc.video(Path(a.mp4))
        print(f"wrote {a.mp4}")
    else:
        sc.interactive()


if __name__ == "__main__":
    main()
