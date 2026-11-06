"""B3: the flexure family builds and passes the gate at every corner of its bounds, with the web it promises."""
import itertools
import json
from pathlib import Path

from structura import engine

ROOT = Path(__file__).resolve().parents[1]


def test_flexure_corners():
    fam = json.loads((ROOT / "families" / "flexure.json").read_text())
    names = list(fam["parameters"])
    for corner in itertools.product(*[(fam["parameters"][n]["low"], fam["parameters"][n]["high"]) for n in names]):
        p = dict(zip(names, corner))
        sets = [f"{k}={v}" for k, v in p.items()]
        b = engine.call("build", family="flexure", set=sets)
        assert b["valid"] and b["solids"] == 1, p
        assert engine.call("gate", family="flexure", set=sets)["ok"], p
        t, R, w = p["web_thickness"], p["notch_radius"], p["width"]
        # bar minus two half-discs: 60 * w * (t + 2R) - pi R^2 w
        assert abs(b["volume_mm3"] / (60 * w * (t + 2 * R) - 3.141592653589793 * R**2 * w) - 1) < 1e-6, p
        assert {"clamp_face", "load_face"} <= set(b["named_faces"]), p
