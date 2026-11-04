"""A3 demo: improve an uploaded part. An over-built bracket arrives as a STEP file; it is analysed as-is, its bolt holes
are frozen, a lightening pocket is added in the arm and sized by the optimizer under the 20 kg brief.

Usage: micromamba run -n attome-ae python benchmarks/a3_redesign_demo.py [--evals 120]
Writes runs/a3_demo/ and benchmarks/a3_redesign_demo.json.
"""
import argparse
import json
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from structura import engine  # noqa: E402
from structura.agent import mcp_server as srv  # noqa: E402
from structura.optimize import decide, portfolio  # noqa: E402
from structura.optimize.evaluate import evaluate, load_spec  # noqa: E402
from structura.optimize.verify_front import verify_front  # noqa: E402

OVERBUILT = {"thickness": 12, "plate_width": 80, "rib_count": 3, "rib_thickness": 10, "fillet_radius": 6}
NAME = "a3_overbuilt_bracket"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--evals", type=int, default=120)
    a = ap.parse_args()
    out = ROOT / "runs" / "a3_demo"
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    t0 = time.perf_counter()
    # 1. the "uploaded" file
    engine.call("build", family="bracket", set=[f"{k}={v}" for k, v in OVERBUILT.items()], out=str(out / "upload"))
    up = srv.upload_part(str(out / "upload" / "part.step"), NAME)
    assert up["ok"], up
    family = up["family"]
    end = next(f for f in up["faces"] if f.get("selector", {}).get("normal") == "x" and abs(f["selector"]["at"] - 120) < 1e-6)
    srv.add_face(family, end["id"], "arm_end", "arm end face: the shelf load acts here")
    wall = next(f for f in up["faces"] if f.get("selector", {}).get("normal") == "-x" and abs(f["selector"]["at"]) < 1e-6)
    srv.add_face(family, wall["id"], "wall_face", "back of the wall plate, bearing on the wall")
    holes = next(k for k in up["named_faces"] if k.startswith("holes_"))
    base_spec = json.loads((ROOT / "docs" / "examples" / "bracket_20kg.json").read_text())
    base_spec.update(id="a3-overbuilt-as-is", family=family, free_params={},
                     brief="Make my bracket (uploaded STEP) lighter; same bolt holes; it must still hold 20 kg. Printed in PETG.")
    lc = base_spec["load_cases"][0]
    lc["supports"] = [{"group": holes}, {"group": "wall_face", "dofs": "1"}]
    lc["loads"] = [{"group": "arm_end", "force": [0, 0, -196.2]}]
    (out / "spec_as_is.json").write_text(json.dumps(base_spec, indent=2))
    # 2. as-is analysis (the baseline)
    base = evaluate(load_spec(out / "spec_as_is.json"), {}, mesh_size=3, threads=4, use_cache=False)
    print("as-is", base.status, base.raw.get("metrics"), "feasible", base.feasible)
    # 3. redesign: a pocket in the top of the arm (the highest +z face)
    top = max((f for f in up["faces"] if f.get("selector", {}).get("normal") == "z"), key=lambda f: (f["selector"]["at"], f["area_mm2"]))
    pk = srv.add_lightening_pocket(family, top["id"])
    assert pk["ok"], pk
    spec = dict(base_spec, id="a3-overbuilt-redesign", free_params={k: [v["low"], v["high"]] for k, v in pk["parameters"].items()})
    # an improvement brief: the mass requirement is "lighter than the uploaded part", not the from-scratch 250 g cap
    spec["requirements"] = [r if r["quantity"] != "mass_g" else
                            dict(r, value=round(base.raw["metrics"]["mass_g"], 1), source="brief: lighter than the uploaded part (as-is mass)")
                            for r in base_spec["requirements"]]
    spec_path = out / "spec_redesign.json"
    spec_path.write_text(json.dumps(spec, indent=2))
    study = portfolio.run(spec_path, budget=a.evals, seed=5, study_dir=out / "opt", mesh_size=4, n_jobs=8)
    vf = verify_front(study, coarse_mesh=4, factor=0.75)
    lightest = min((r for r in vf["front"] if r.get("feasible_fine")), key=lambda r: r["fine"][0], default=None)
    trust = engine.call("trust", spec=str(spec_path), set=[f"{k}={float(v)}" for k, v in lightest["params"].items()],
                        out=str(out / "trust"), **{"fine-size": 3}, timeout=3600) if lightest else None
    res = {"evaluations": len(study.trials), "method": study.user_attrs.get("method"), "seconds": round(time.perf_counter() - t0, 1),
           "uploaded": {"params_used_to_make_it": OVERBUILT, "holes": up["holes"], "frozen_interfaces": pk["frozen_interfaces"]},
           "as_is": {"metrics": base.raw.get("metrics"), "feasible": base.feasible},
           "pocket_parameters": pk["parameters"], "front_size": vf["front_size"], "front_still_feasible_on_fine_mesh": vf["still_feasible"],
           "lightest_feasible_fine": lightest, "recommended_knee": decide.recommend(study),
           "trust_of_lightest": {k: trust.get(k) for k in ("grade", "status", "reasons", "requirements")} if trust else None}
    (ROOT / "benchmarks" / "a3_redesign_demo.json").write_text(json.dumps(res, indent=2))
    print("pocket", pk["parameters"])
    print("lightest feasible (fine)", lightest and {"params": lightest["params"], "fine": lightest["fine"]})
    print("front", vf["front_size"], "still feasible", vf["still_feasible"], "| trust", trust and trust.get("grade"), trust and trust.get("reasons"))
    print("seconds", res["seconds"])


if __name__ == "__main__":
    main()
