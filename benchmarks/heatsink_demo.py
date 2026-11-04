"""P6 thermal demo: lightest heat sink keeping a 10 W module below 75 C in 35 C air, with the knee design's trust record.

Usage: micromamba run -n attome-ae python benchmarks/heatsink_demo.py [--evals 150]
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
from structura.optimize import decide, pareto, portfolio  # noqa: E402
from structura.optimize.evaluate import evaluate, load_spec  # noqa: E402
from structura.optimize.verify_front import verify_front  # noqa: E402

SPEC = ROOT / "docs" / "examples" / "heatsink_10w.json"
BASELINE = {"base_thickness": 4, "fin_count": 8, "fin_thickness": 2, "fin_height": 25}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--evals", type=int, default=150)
    a = ap.parse_args()
    out = ROOT / "runs" / "heatsink_demo"
    shutil.rmtree(out, ignore_errors=True)
    spec = load_spec(SPEC)
    t0 = time.perf_counter()
    base = evaluate(spec, BASELINE, mesh_size=1.5, threads=4, use_cache=False)
    study = portfolio.run(SPEC, budget=a.evals, seed=11, study_dir=out / "opt", mesh_size=2.5, n_jobs=8)
    vf = verify_front(study, coarse_mesh=2.5, factor=0.6)
    rec = decide.recommend(study)
    lightest = min((r for r in vf["front"] if r.get("feasible_fine")), key=lambda r: r["fine"][0], default=None)
    pick = lightest or (rec and {"params": rec["params"]})
    trust = engine.call("trust", spec=str(SPEC), set=[f"{k}={float(v)}" for k, v in pick["params"].items()], out=str(out / "trust"),
                        **{"fine-size": 1.5}, timeout=3600) if pick else None
    res = {"evaluations": len(study.trials), "method": study.user_attrs.get("method"), "method_reason": study.user_attrs.get("method_reason"),
           "seconds": round(time.perf_counter() - t0, 1), "baseline": {"params": BASELINE, "metrics": base.raw.get("metrics"), "feasible": base.feasible},
           "front_size": vf["front_size"], "front_still_feasible_on_fine_mesh": vf["still_feasible"],
           "lightest_feasible_fine": lightest, "recommended_knee": rec,
           "trust_of_lightest": {k: trust.get(k) for k in ("grade", "status", "reasons", "requirements")} if trust else None}
    (ROOT / "benchmarks" / "heatsink_demo.json").write_text(json.dumps(res, indent=2))
    print("baseline", base.raw.get("metrics"))
    print("lightest feasible (fine)", lightest and {"params": lightest["params"], "fine": lightest["fine"]})
    print("front", vf["front_size"], "still feasible", vf["still_feasible"], "| trust", trust and trust.get("grade"), trust and trust.get("reasons"))
    print("method", res["method"], "|", res["seconds"], "s")


if __name__ == "__main__":
    main()
