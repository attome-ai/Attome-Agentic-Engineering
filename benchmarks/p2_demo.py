"""P2 exit demo: hand baseline vs optimized front on the 20 kg bracket brief, with the knee design's trust record.

Usage: micromamba run -n attome-ae python benchmarks/p2_demo.py [--evals 200]
Writes runs/p2_demo/ (study, verify_front.json, trust/) and benchmarks/p2_demo.json.
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

SPEC = ROOT / "docs" / "examples" / "bracket_20kg.json"
BASELINE = {"thickness": 6, "plate_width": 50, "rib_count": 1, "rib_thickness": 4, "fillet_radius": 3}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--evals", type=int, default=200)
    a = ap.parse_args()
    out = ROOT / "runs" / "p2_demo"
    shutil.rmtree(out, ignore_errors=True)
    spec = load_spec(SPEC)
    t0 = time.perf_counter()
    base = evaluate(spec, BASELINE, mesh_size=3, threads=4, use_cache=False)
    study = portfolio.run(SPEC, budget=a.evals, seed=7, study_dir=out / "opt", mesh_size=4, n_jobs=8)
    t_opt = time.perf_counter() - t0
    front = pareto.front(study)
    vf = verify_front(study, coarse_mesh=4, factor=0.75)
    (out / "verify_front.json").write_text(json.dumps(vf, indent=2))
    rec = decide.recommend(study)
    # lightest fine-mesh-feasible design that is at least as safe as the baseline
    base_sf = base.raw["metrics"]["safety_factor"] if base.status == "ok" else 0
    cands = [r for r in vf["front"] if r.get("feasible_fine") and r["fine"][1] >= max(base_sf, 0)]
    best_light = min(cands, key=lambda r: r["fine"][0]) if cands else None
    knee_params = {k: float(v) for k, v in rec["params"].items()} if rec else None
    trust = engine.call("trust", spec=str(SPEC), set=[f"{k}={v}" for k, v in knee_params.items()], out=str(out / "trust"),
                        **{"fine-size": 3}, timeout=3600) if knee_params else None
    res = {
        "spec": str(SPEC.relative_to(ROOT)), "evaluations": len(study.trials), "method": study.user_attrs.get("method"),
        "method_reason": study.user_attrs.get("method_reason"), "optimize_seconds": round(t_opt, 1),
        "baseline": {"params": BASELINE, "status": base.status, "metrics": base.raw.get("metrics"), "feasible": base.feasible,
                     "checks": base.raw.get("checks")},
        "front_size": len(front), "front_ranking_changed_on_fine_mesh": vf["ranking_changed"],
        "front_still_feasible_on_fine_mesh": f"{vf['still_feasible']} of {vf['front_size']}",
        "lightest_at_least_as_safe_as_baseline": best_light,
        "recommended_knee": rec,
        "knee_trust": {k: trust.get(k) for k in ("grade", "status", "reasons", "requirements", "numerical_error")} if trust else None,
    }
    (ROOT / "benchmarks" / "p2_demo.json").write_text(json.dumps(res, indent=2))
    print(json.dumps({k: res[k] for k in ("evaluations", "method", "front_size", "front_ranking_changed_on_fine_mesh",
                                           "front_still_feasible_on_fine_mesh")}, indent=1))
    print("baseline", base.status, base.raw.get("metrics"), "feasible", base.feasible)
    print("lightest >= baseline SF", best_light and {"params": best_light["params"], "fine": best_light["fine"]})
    print("knee", rec and rec["values"], "trust grade", trust and trust.get("grade"), trust and trust.get("reasons"))


if __name__ == "__main__":
    main()
