"""P3-t1 image briefs: hand-sketch-style drawings of brackets and heat sinks, generated from random requirements.

There is no second person on this project, so the briefs are generated, not drawn: every requirement is drawn at random
(seeded) and written on the sketch the way a person would scribble it (kg or lb, C or F, "printed" or "CNC", "lightest"
or "cheap", some fields left out, and distracting geometry dimensions that are not spec fields). The gold answers go to
image_gold.json, which the model under test must not read before it has answered. The bias that remains: the same model
wrote this generator, so it knows the drawing style and the field rules; the values themselves it has not seen.

Usage: python -m benchmarks.brief_bench.make_images [--n 10] [--seed S]
Writes benchmarks/brief_bench/images/ib01.png ... and benchmarks/brief_bench/image_gold.json. Prints nothing else.
"""
from __future__ import annotations

import argparse
import json
import random
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = Path(__file__).resolve().parent


def gold(family, objective):
    return {"family": family, "material": None, "load_N": None, "sf": None, "disp_mm": None, "power_W": None,
            "t_max_C": None, "fan_m_s": None, "objective": objective, "process": None}


def bracket(r: random.Random):
    notes = []
    obj, word = r.choice([("min_mass", "as light as possible"), ("min_mass", "lightest"), ("min_cost", "cheap please"),
                          ("max_safety", "strongest possible")])
    g = gold("bracket", obj)
    mat = r.choice(["PETG", "PLA", "AL6061T6", "S235", None])
    if mat:
        notes.append({"PETG": "PETG", "PLA": "PLA", "AL6061T6": r.choice(["alu 6061", "aluminium"]), "S235": "steel"}[mat])
        g["material"] = mat
    proc = {"PETG": ["fdm", None], "PLA": ["fdm", None], "AL6061T6": ["cnc", None], "S235": ["cnc", None], None: ["fdm", "cnc", None]}[mat]
    proc = r.choice(proc)
    if proc:
        notes.append(r.choice(["3D printed", "printed"]) if proc == "fdm" else r.choice(["machined", "CNC"]))
        g["process"] = proc
    unit = r.choice(["kg", "kg", "lb", "N"])
    if unit == "kg":
        m = r.choice([3, 5, 8, 10, 12, 15, 20, 25, 30, 40])
        load_txt, g["load_N"] = f"{m} kg", round(m * 9.81, 3)
    elif unit == "lb":
        m = r.choice([10, 15, 20, 25, 40, 50])
        load_txt, g["load_N"] = f"{m} lb", round(m * 4.448, 3)
    else:
        n = r.choice([100, 150, 200, 250, 300, 400])
        load_txt, g["load_N"] = f"{n} N", float(n)
    if r.random() < 0.5:
        sf = r.choice([1.5, 2, 2.5, 3])
        notes.append(r.choice([f"SF {sf:g}", f"safety factor {sf:g}"]))
        g["sf"] = float(sf)
    if r.random() < 0.4:
        d = r.choice([0.5, 1, 1.5, 2, 3])
        notes.append(r.choice([f"max sag {d:g} mm", f"deflection < {d:g} mm"]))
        g["disp_mm"] = float(d)
    notes.append(word)
    r.shuffle(notes)
    return g, load_txt, notes, {"arm": r.choice([80, 100, 120, 150]), "height": r.choice([60, 80, 100]), "holes": r.choice([2, 3])}


def heatsink(r: random.Random):
    notes = []
    obj, word = r.choice([("min_mass", "lightest"), ("min_mass", "as light as possible"), ("min_temperature", "as cool as possible")])
    g = gold("heatsink", obj)
    p = r.choice([5, 8, 10, 12, 15, 20, 25])
    g["power_W"] = float(p)
    if r.random() < 0.3:
        f = r.choice([140, 150, 158, 167, 176])
        notes.append(f"keep below {f} F")
        g["t_max_C"] = round((f - 32) * 5 / 9, 3)
    else:
        c = r.choice([55, 60, 65, 70, 75, 80, 85])
        notes.append(r.choice([f"max {c} °C", f"< {c} °C"]))
        g["t_max_C"] = float(c)
    if r.random() < 0.6:
        v = r.choice([1, 1.5, 2, 2.5, 3])
        notes.append(f"fan {v:g} m/s")
        g["fan_m_s"] = float(v)
    else:
        notes.append("small fan")
    if r.random() < 0.5:
        notes.append(r.choice(["aluminium", "alu 6061"]))
        g["material"] = "AL6061T6"
    if r.random() < 0.4:
        notes.append(r.choice(["machined", "CNC"]))
        g["process"] = "cnc"
    notes.append(word)
    r.shuffle(notes)
    return g, f"{p} W", notes, {"base": r.choice([40, 50, 60, 80]), "fins": r.choice([6, 8, 10, 12])}


def wobble(r, xs, ys, k=0.6):
    return [x + r.uniform(-k, k) for x in xs], [y + r.uniform(-k, k) for y in ys]


def draw_bracket(ax, r, load_txt, geo):
    arm, h = geo["arm"], geo["height"]
    s = 100 / max(arm, h)
    A, Hh, t = arm * s, h * s, 8
    poly = [(0, 0), (t, 0), (t, Hh - t), (A, Hh - t), (A, Hh), (0, Hh), (0, 0)]
    ax.plot(*wobble(r, [p[0] for p in poly], [p[1] for p in poly]), "k", lw=2)
    ax.plot(*wobble(r, [t, A * 0.85, t], [Hh * 0.3, Hh - t, Hh - t]), "k", lw=1.5)  # gusset
    for i in range(9):  # wall hatching
        y = Hh * i / 8
        ax.plot([-4, 0], [y - 4, y], "k", lw=0.8)
    for i in range(geo["holes"]):
        ax.add_patch(plt.Circle((t / 2, Hh * (0.15 + 0.12 * i)), 1.6, fill=False, lw=1))
    ax.annotate("", xy=(A - 4, Hh + 1), xytext=(A - 4, Hh + 22), arrowprops=dict(arrowstyle="->", lw=2))
    ax.text(A - 2, Hh + 14, load_txt, fontsize=15)
    ax.annotate("", xy=(t, Hh + 6), xytext=(A, Hh + 6), arrowprops=dict(arrowstyle="<->", lw=0.8))
    ax.text(A / 2 - 6, Hh + 8, f"{geo['arm']}", fontsize=10)
    ax.text(-16, Hh / 2, f"{geo['height']}", fontsize=10, rotation=90)
    ax.text(t + 3, Hh * 0.08, "wall", fontsize=9)


def draw_heatsink(ax, r, p_txt, geo):
    B, n = 100, geo["fins"]
    ax.plot(*wobble(r, [0, B, B, 0, 0], [0, 0, 8, 8, 0]), "k", lw=2)
    for i in range(n):
        x = 3 + i * (B - 6) / (n - 1)
        ax.plot(*wobble(r, [x - 1, x - 1, x + 1, x + 1], [8, 55, 55, 8], 0.4), "k", lw=1.3)
    ax.add_patch(plt.Rectangle((35, -10), 30, 10, fill=False, hatch="//", lw=1.2))
    ax.text(68, -9, f"chip {p_txt}", fontsize=14)
    ax.annotate("", xy=(B + 2, 32), xytext=(B + 24, 32), arrowprops=dict(arrowstyle="<-", lw=1.5))
    ax.text(B + 4, 36, "air", fontsize=10)
    ax.annotate("", xy=(0, 62), xytext=(B, 62), arrowprops=dict(arrowstyle="<->", lw=0.8))
    ax.text(B / 2 - 8, 64, f"{geo['base']} mm", fontsize=10)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=10)
    ap.add_argument("--seed", type=int, default=20261106)
    a = ap.parse_args()
    r = random.Random(a.seed)
    out = HERE / "images"
    out.mkdir(exist_ok=True)
    kinds = ["bracket"] * (a.n - a.n * 2 // 5) + ["heatsink"] * (a.n * 2 // 5)
    r.shuffle(kinds)
    golds = {}
    for i, k in enumerate(kinds, 1):
        bid = f"ib{i:02d}"
        g, main_txt, notes, geo = (bracket if k == "bracket" else heatsink)(r)
        with plt.xkcd(scale=1, length=100, randomness=2):
            fig, ax = plt.subplots(figsize=(7, 5.2), dpi=110)
            (draw_bracket if k == "bracket" else draw_heatsink)(ax, r, main_txt, geo)
            # notes in a column right of the part, clear of the load label and the airflow arrow
            y0 = 100 if k == "heatsink" else min(95, 0.7 * 100 * geo["height"] / max(geo["arm"], geo["height"]))
            for j, note in enumerate(notes):
                ax.text(112 + r.uniform(-2, 2), y0 - 13 * j, note, fontsize=13, rotation=r.uniform(-4, 4))
            ax.set_xlim(-25, 185)
            ax.set_ylim(-25, 135)
            ax.set_aspect("equal")
            ax.axis("off")
            fig.savefig(out / f"{bid}.png")
            plt.close(fig)
        golds[bid] = g
    (HERE / "image_gold.json").write_text(json.dumps({"seed": a.seed, "gold": golds}, indent=1))


if __name__ == "__main__":
    main()
