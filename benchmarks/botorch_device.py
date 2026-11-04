"""BoTorch (Ax qNEHVI) on CPU against GPU: seconds of model work per trial and hypervolume at an equal number of calls.

Run when the GPU is free:
  micromamba run -n attome-ae python benchmarks/botorch_device.py --device cuda --calls 40 --seeds 3
  micromamba run -n attome-ae python benchmarks/botorch_device.py --device cpu  --calls 40 --seeds 1
Writes benchmarks/botorch_device_<device>.json. If GPU fitting is several times faster than the 43 s measured on CPU,
lower BO_MODEL_SECONDS in structura/optimize/portfolio.py accordingly (decision D5: the method is chosen by measured cost).
"""
import argparse
import json
import os
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

SPEC = ROOT / "docs" / "examples" / "bracket_20kg.json"
REF = np.array([400.0, 0.0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", choices=["cpu", "cuda"], required=True)
    ap.add_argument("--calls", type=int, default=40)
    ap.add_argument("--seeds", type=int, default=3)
    a = ap.parse_args()
    if a.device == "cuda":
        os.environ["STRUCTURA_TORCH_DEVICE"] = "cuda"
    from structura.optimize import pareto, portfolio
    rows = []
    for seed in range(a.seeds):
        out = ROOT / "runs" / "bench_device" / f"{a.device}-{seed}"
        t0 = time.perf_counter()
        s = portfolio.run(SPEC, budget=a.calls, seed=seed, method="botorch_qnehvi", study_dir=out, mesh_size=6, n_jobs=4)
        wall = time.perf_counter() - t0
        evals = sum(t.user_attrs.get("seconds", 0) for t in s.trials)
        hv = pareto.hypervolume_history(s, REF)
        rows.append({"seed": seed, "wall_s": round(wall, 1), "evaluation_s": round(evals, 1),
                     "model_s_per_trial": round((wall - evals / 4) / max(1, len(s.trials)), 2),  # 4 evaluations run in parallel
                     "hv_at_calls": hv[-1] if hv else 0.0})
        print(rows[-1], flush=True)
    res = {"device": a.device, "calls": a.calls, "runs": rows,
           "mean_model_s_per_trial": float(np.mean([r["model_s_per_trial"] for r in rows])),
           "mean_hv": float(np.mean([r["hv_at_calls"] for r in rows]))}
    (ROOT / "benchmarks" / f"botorch_device_{a.device}.json").write_text(json.dumps(res, indent=2))
    print(json.dumps({k: res[k] for k in ("device", "mean_model_s_per_trial", "mean_hv")}))


if __name__ == "__main__":
    main()
