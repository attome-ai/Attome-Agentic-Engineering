"""D1-T3 benchmark: random search, NSGA-II, Optuna's default sampler and the Attome AE portfolio on the same spec,
budget and seeds; hypervolume against solver calls is written to benchmarks/optimizer_compare.json.

Budget: 120 evaluations by default. At 200 the portfolio's own rule picks NSGA-II (the comparison would be NSGA-II
against itself); at 120 it picks Bayesian optimization, so the benchmark tests the choice it actually makes.

Usage (repository root):
    micromamba run -n attome-ae python benchmarks/optimizer_compare.py [--evals 120] [--seeds 3] [--methods ...]
"""
import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from structura.optimize import pareto, portfolio  # noqa: E402
from structura.optimize.evaluate import load_spec  # noqa: E402

SPEC = ROOT / "docs" / "examples" / "bracket_20kg.json"
REF = np.array([400.0, 0.0])  # minimized objectives: mass_g up to 400 g, -safety_factor up to 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--evals", type=int, default=120)
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--methods", nargs="*", default=["random", "nsga2", "default", "portfolio"])
    ap.add_argument("--mesh", type=float, default=6.0)
    ap.add_argument("--out", default=str(ROOT / "benchmarks" / "optimizer_compare.json"))
    a = ap.parse_args()
    spec = load_spec(SPEC)
    out_path = Path(a.out)
    results = json.loads(out_path.read_text()) if out_path.exists() else {}
    results.setdefault("meta", {})
    results["meta"].update({"spec": str(SPEC.relative_to(ROOT)), "evals": a.evals, "seeds": a.seeds, "mesh_size": a.mesh,
                            "reference_minimized": REF.tolist(), "objectives": spec.objectives,
                            "portfolio_choice": portfolio.choose(spec, a.evals)})
    for m in a.methods:
        for seed in range(a.seeds):
            key = f"{m}/{seed}"
            if key in results.get("runs", {}):
                continue
            t0 = time.perf_counter()
            sdir = ROOT / "runs" / "bench" / f"{m}-{seed}"
            if sdir.exists():
                import shutil
                shutil.rmtree(sdir)
            study = portfolio.run(SPEC, budget=a.evals, seed=100 + seed, method=None if m == "portfolio" else m,
                                  study_dir=sdir, mesh_size=a.mesh, threads=1, n_jobs=4 if m == "portfolio" else 8)
            hist = pareto.hypervolume_history(study, REF)
            trials = study.get_trials()
            results.setdefault("runs", {})[key] = {
                "method": study.user_attrs.get("method"), "hv_history": hist, "final_hv": hist[-1] if hist else 0.0,
                "trials": len(trials), "feasible": sum(1 for t in trials if t.user_attrs.get("feasible")),
                "failed": sum(1 for t in trials if t.user_attrs.get("status") not in (None, "ok")),
                "seconds": round(time.perf_counter() - t0, 1)}
            out_path.write_text(json.dumps(results, indent=1))
            print(f"{key}: final HV {results['runs'][key]['final_hv']:.2f} in {results['runs'][key]['seconds']} s", flush=True)
    summary = {}
    for m in a.methods:
        hv = [results["runs"][f"{m}/{s}"]["final_hv"] for s in range(a.seeds) if f"{m}/{s}" in results.get("runs", {})]
        if hv:
            summary[m] = {"mean_final_hv": float(np.mean(hv)), "std": float(np.std(hv)), "seeds": len(hv)}
    results["summary"] = summary
    out_path.write_text(json.dumps(results, indent=1))
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
