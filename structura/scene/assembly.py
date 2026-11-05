"""Assembly animation (Level 2): the planner's order played in 3D, each part travelling in along its insertion path.

Usage:
  micromamba run -n attome-ae python -m structura.scene.assembly runs/assembly/shelf            # interactive window
  micromamba run -n attome-ae python -m structura.scene.assembly runs/assembly/shelf --mp4 out.mp4
  micromamba run -n attome-ae python -m structura.scene.assembly runs/assembly/shelf --instructions runs/assembly/shelf/instructions

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


# ISO 4017 hex head width across flats (mm), by thread diameter: the spanner size
HEX_AF = {3: 5.5, 4: 7, 5: 8, 6: 10, 8: 13, 10: 16, 12: 18, 16: 24, 20: 30}

AR_NAMES = {"wall": "الجدار", "plug": "خابور الجدار", "bracket": "الكتيفة", "bolt": "البرغي", "shelf_board": "لوح الرف",
            "crate": "الصندوق", "board": "اللوح", "nut": "الصامولة", "washer": "الوردة"}

TEXT = {
    "en": {"title": "Assembly instructions", "parts": "Parts", "tools": "Tools", "step": "Step", "done": "Done",
           "in_place": "Already in place: {}.",
           "checked": "Order and paths from the planner. Replay check: every move tested at {f} positions per step with exact "
                      "solid intersection ({n} checks), largest overlap {v:.3g} mm³."},
    "ar": {"title": "تعليمات التجميع", "parts": "القطع", "tools": "الأدوات", "step": "الخطوة", "done": "اكتمل التجميع",
           "in_place": "مثبّت مسبقاً: {}.",
           "checked": "الترتيب والمسارات من المخطِّط. فحص الإعادة: اختُبرت كل حركة في {f} موضعاً لكل خطوة بتقاطع المجسّمات "
                      "الدقيق ({n} فحصاً)، وأكبر تداخل {v:.3g} مم³."},
}


def ray_hits_box(p, d, lo, hi) -> bool:
    """True if the ray p + s d (s >= 0) meets the axis-aligned box [lo, hi] (slab test)."""
    t0, t1 = 0.0, np.inf
    for i in range(3):
        if abs(d[i]) < 1e-12:
            if not lo[i] <= p[i] <= hi[i]:
                return False
            continue
        a, b = (lo[i] - p[i]) / d[i], (hi[i] - p[i]) / d[i]
        t0, t1 = max(t0, min(a, b)), min(t1, max(a, b))
    return t0 <= t1


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
        self.folder = folder
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
        tp = self.label.GetTextProperty()  # a light backing keeps the caption readable over dark parts
        tp.SetBackgroundColor(1, 1, 1)
        tp.SetBackgroundOpacity(0.8)
        rc = self.plan.get("replay_check") or {}
        foot = "assembly order planned on the exact geometry"
        if rc:
            foot += (f"; replay check: {rc.get('boolean_checks', 0)} exact solid intersections along the paths, "
                     f"largest overlap {rc.get('max_overlap_mm3', 0.0):.3g} mm³")
        self.pl.add_text(foot, position="lower_left", font_size=9, color="#555555")
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
        self.label.SetText(2, f"step {k} of {n}: {self.sentence(s)}" if t < n else
                           f"assembled: {n} steps")

    def video(self, path: Path, seconds_per_step: float = 1.6, fps: int = 30):
        n = len(self.steps)
        self.pl.open_movie(str(path), framerate=fps, quality=8)
        frames = int(n * seconds_per_step * fps)
        for i in range(frames + fps):
            self.show(min(n, i / (seconds_per_step * fps)))
            self.pl.write_frame()
        self.pl.close()

    def instructions(self, out: Path):
        """A printable instruction sheet in English and Arabic: parts list with counts, tools, then one picture per step
        (framed on the part being moved, half-way along its path) and one sentence each."""
        out.mkdir(parents=True, exist_ok=True)
        n = len(self.steps)
        title = self.plan.get("assembly") or self.folder.name.replace("_", " ")
        rc = self.plan.get("replay_check") or {}
        pics = []
        overview = self.pl.camera_position
        view = np.array(overview[0]) - np.array(overview[1])
        view /= np.linalg.norm(view)
        for s in self.steps:
            k = s["step"]
            self.show(k - 0.5)
            self.label.SetText(2, f"step {k} of {n}: {self.sentence(s, 'en')}")
            # frame this step: the part's whole path plus its own size, so a small plug is as legible as the board
            b = np.array(self.parts[s["part"]]["mesh"].bounds).reshape(3, 2)
            end = self.parts[s["part"]]["base"]
            lo = np.minimum(b[:, 0], end.min(axis=0))
            hi = np.maximum(b[:, 1], end.max(axis=0))
            c = (lo + hi) / 2
            r = max(float(np.linalg.norm(hi - lo)), 60.0)
            self.pl.camera_position = [tuple(c + view * 2.4 * r), tuple(c), (0, 0, 1)]
            self.pl.render()
            pics.append(f"step_{k:02d}.png")
            self.pl.screenshot(str(out / pics[-1]))
        self.show(n)
        self.pl.camera_position = overview
        self.pl.render()
        self.pl.screenshot(str(out / "assembled.png"))
        written = []
        for lang in ("en", "ar"):
            T = TEXT[lang]
            lines = [f"# {T['title']}: {title}", ""]
            if rc:
                lines += [T["checked"].format(n=rc.get("boolean_checks", 0), f=rc.get("frames_per_step", 0),
                                              v=rc.get("max_overlap_mm3", 0.0)), ""]
            lines += [f"## {T['parts']}", ""]
            for count, label in self.parts_list(lang):
                lines.append(f"- {count} × {label}")
            lines += ["", f"## {T['tools']}", ""] + [f"- {t}" for t in self.tools(lang)] + [""]
            fixed = [d["name"] for d in self.plan["part_list"] if d.get("fixed")]
            if fixed:
                lines += [T["in_place"].format(", ".join(self.pname(f, lang) for f in fixed)), ""]
            for s, png in zip(self.steps, pics):
                lines += [f"## {T['step']} {s['step']}", "", f"![{s['step']}]({png})", "", self.sentence(s, lang), ""]
            lines += [f"## {T['done']}", "", "![assembled](assembled.png)", ""]
            body = "\n".join(lines)
            if lang == "ar":
                body = '<div dir="rtl" lang="ar">\n\n' + body + "\n</div>\n"
            path = out / ("instructions.md" if lang == "en" else "instructions.ar.md")
            path.write_text(body, encoding="utf-8")
            written.append(path)
        return written[0]

    def pname(self, name: str, lang: str) -> str:
        """A part's name for the reader: bolt_2 -> 'bolt 2' / 'البرغي 2'."""
        base, _, num = name.rpartition("_")
        if not num.isdigit():
            base, num = name, ""
        base = AR_NAMES.get(base, base.replace("_", " ")) if lang == "ar" else base.replace("_", " ")
        return f"{base} {num}".strip()

    def parts_list(self, lang: str):
        """(count, description) for the parts to fit, identical parts counted together (fixed surroundings excluded)."""
        groups: dict[str, int] = {}
        for s in self.steps:
            d = self.desc[s["part"]]
            dia, length = d.get("diameter") or 0, d.get("length") or 0
            if d["kind"] == "bolt":
                key = f"M{dia:g} bolt, {length:g} mm long" if lang == "en" else f"برغي M{dia:g} بطول {length:g} مم"
            elif d.get("fastener") and dia:
                key = f"wall plug {dia:g} mm, {length:g} mm long" if lang == "en" else f"خابور جدار {dia:g} مم بطول {length:g} مم"
            else:
                key = self.pname(s["part"], lang)
            groups[key] = groups.get(key, 0) + 1
        return [(c, k) for k, c in groups.items()]

    def tools(self, lang: str):
        """Tools that follow from the parts: a drill and bit for wall plugs, a hammer to seat them, a spanner per bolt size."""
        out = []
        plug_parts = [self.desc[s["part"]] for s in self.steps
                      if self.desc[s["part"]].get("fastener") and self.desc[s["part"]]["kind"] != "bolt" and self.desc[s["part"]].get("diameter")]
        for dia in sorted({d["diameter"] for d in plug_parts}):
            depth = max(d.get("length") or 0 for d in plug_parts if d["diameter"] == dia)
            out.append(f"drill with a {dia:g} mm masonry bit, holes at least {depth:g} mm deep" if lang == "en" else
                       f"مثقاب بلقمة خرسانة {dia:g} مم، بعمق لا يقل عن {depth:g} مم")
        if plug_parts:
            out.append("hammer, to tap the plugs flush" if lang == "en" else "مطرقة لدقّ الخوابير حتى تستوي مع السطح")
        bolts = sorted({self.desc[s["part"]].get("diameter") or 0 for s in self.steps if self.desc[s["part"]]["kind"] == "bolt"} - {0})
        for dia in bolts:
            af = HEX_AF.get(dia)
            if af:
                out.append(f"{af:g} mm spanner or socket (M{dia:g}, ISO 4017 hex head)" if lang == "en" else
                           f"مفتاح ربط {af:g} مم (M{dia:g}، رأس سداسي ISO 4017)")
            else:
                out.append(f"spanner for M{dia:g}" if lang == "en" else f"مفتاح ربط لبرغي M{dia:g}")
        return out

    def direction(self, s, lang: str) -> str:
        d = np.array(s["insert_direction"], float)
        key = tuple(int(round(v)) for v in d)
        if key == (0, 0, -1):
            return "straight down" if lang == "en" else "إلى الأسفل مباشرة"
        if key == (0, 0, 1):
            return "upward" if lang == "en" else "إلى الأعلى"
        # toward a fixed part (the wall) if the move points at it: the reader knows where the wall is, not where -x is
        here = self.parts[s["part"]]["base"].mean(axis=0)
        for f in (q["name"] for q in self.plan["part_list"] if q.get("fixed")):
            if ray_hits_box(here, d, self.parts[f]["base"].min(axis=0), self.parts[f]["base"].max(axis=0)):
                return f"toward the {self.pname(f, 'en')}" if lang == "en" else f"نحو {self.pname(f, 'ar')}"
        axis = {(1, 0, 0): "+x", (-1, 0, 0): "-x", (0, 1, 0): "+y", (0, -1, 0): "-y"}.get(key, str(d.round(3).tolist()))
        return f"along {axis}" if lang == "en" else f"على المحور {axis}"

    def sentence(self, s, lang: str = "en") -> str:
        name = self.pname(s["part"], lang)
        kind = self.desc[s["part"]]["kind"]
        go = self.direction(s, lang)
        t = s["travel_mm"]
        fastener = self.desc[s["part"]].get("fastener")
        if lang == "ar":
            if kind == "bolt":
                return f"اربط {name} بتحريكه {go} حتى يستقر رأسه (المسار {t:.0f} مم)."
            if fastener:
                return f"ادفع {name} في ثقبه بتحريكه {go} حتى يستوي مع السطح (المسار {t:.0f} مم)."
            verb = "أنزِل" if go.startswith("إلى الأسفل") else "حرّك"
            return f"{verb} {name} {go} إلى الموضع المحدد (المسار {t:.0f} مم)."
        if kind == "bolt":
            return f"Drive in {name}, moving {go}, until the head seats (path {t:.0f} mm)."
        if fastener:
            return f"Push {name} into its hole, moving {go}, until flush (path {t:.0f} mm)."
        if go == "straight down":
            return f"Lower {name} straight down into place (path {t:.0f} mm)."
        return f"Bring {name} in, moving {go}, into place (path {t:.0f} mm)."

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
    ap.add_argument("--instructions", help="folder for a printable instruction sheet (instructions.md + one PNG per step)")
    ap.add_argument("--t", type=float, default=0.0)
    a = ap.parse_args()
    sc = AssemblyScene(Path(a.folder), off_screen=bool(a.mp4 or a.png or a.instructions))
    if a.instructions:
        print(f"wrote {sc.instructions(Path(a.instructions))}")
    elif a.png:
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
