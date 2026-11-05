"""Build the shelf assembly description from a real bracket design: wall (fixed, drilled), two wall plugs, the bracket,
two M6 bolts, the shelf board and a crate. Hole positions come from the engine's inspection of the bracket's STEP.

Usage: micromamba run -n attome-ae python assembly/examples/make_shelf.py [k=v ...]   (bracket parameters, optional)
Writes assembly/examples/shelf/ (bracket.step, shelf.json).
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from structura import engine  # noqa: E402

OUT = ROOT / "assembly" / "examples" / "shelf"


def main():
    params = dict(a.split("=") for a in sys.argv[1:])
    OUT.mkdir(parents=True, exist_ok=True)
    b = engine.call("build", family="bracket", set=[f"{k}={v}" for k, v in params.items()], out=str(OUT / "bracket_build"))
    step = OUT / "bracket_build" / "part.step"
    ins = engine.call("inspect", step=str(step))
    x0, y0, z0, x1, y1, z1 = ins["envelope"]
    holes = [h for h in ins["holes"] if abs(abs(h["axis"][0]) - 1) < 1e-6]  # the wall-plate bolt holes run along x
    if not holes:
        raise SystemExit("no bolt holes along x found in the bracket")
    t = max(h["depth_mm"] for h in holes)  # wall-plate thickness = hole depth
    w = y1 - y0
    parts = [{"name": "wall", "kind": "box", "fixed": True, "at": [-60, y0 - 260, z0 - 200], "size": [60, w + 520, z1 - z0 + 450],
              "holes": [{"at": [-36, h["centre"][1], h["centre"][2]], "axis": [1, 0, 0], "diameter": 8.0, "depth": 36} for h in holes],
              "color": "#e4dfd6"}]
    for i, h in enumerate(holes, 1):
        _, cy, cz = h["centre"]
        parts.append({"name": f"plug_{i}", "kind": "tube", "fastener": True, "at": [-32, cy, cz], "axis": [1, 0, 0],
                      "diameter": 8.0, "inner_diameter": 6.2, "length": 32, "color": "#d9d9d9"})
    parts.append({"name": "bracket", "kind": "step", "file": "bracket_build/part.step", "color": "#4a4f57"})
    for i, h in enumerate(holes, 1):
        _, cy, cz = h["centre"]
        parts.append({"name": f"bolt_{i}", "kind": "bolt", "at": [t, cy, cz], "axis": [-1, 0, 0], "diameter": 6.0,
                      "length": t + 26, "color": "#9aa0a6"})
    parts.append({"name": "shelf_board", "kind": "box", "at": [0, y0 + w / 2 - 200, z1], "size": [250, 400, 18], "color": "#c08a55",
                  "mass_kg": 0.9})  # pine, 250 x 400 x 18 mm at about 500 kg/m3
    parts.append({"name": "crate", "kind": "box", "at": [70, y0 + w / 2 - 70, z1 + 18], "size": [120, 140, 110], "color": "#8a6b4a",
                  "mass_kg": 20.0})  # the 20 kg load of brief b02
    j = {"name": "wall shelf: bracket, plugs, M6 bolts, board, crate", "bracket_params": b["params"], "parts": parts}
    (OUT / "shelf.json").write_text(json.dumps(j, indent=2))
    print(f"wrote {OUT / 'shelf.json'}: {len(parts)} parts, {len(holes)} bolt holes, wall plate {t:.1f} mm")


if __name__ == "__main__":
    main()
