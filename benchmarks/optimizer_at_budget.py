"""Compare the optimizer benchmark runs at an equal number of solver calls (and report wall-clock cost).

Usage: micromamba run -n attome-ae python benchmarks/optimizer_at_budget.py [--calls 40]
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
import optuna

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from structura.optimize import pareto  # noqa: E402

REF = np.array([400.0, 0.0])
optuna.logging.set_verbosity(optuna.logging.ERROR)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--calls", type=int, default=40)
    a = ap.parse_args()
    rows = {}
    for d in sorted((ROOT / "runs" / "bench").iterdir()):
        db = d / "study.db"
        if not db.exists():
            continue
        url = f"sqlite:///{db.resolve().as_posix()}"
        s = optuna.load_study(study_name=optuna.study.get_all_study_names(url)[0], storage=url)
        h = pareto.hypervolume_history(s, REF)
        trials = sorted(s.trials, key=lambda t: t.number)
        wall = None
        if len(trials) >= a.calls and trials[0].datetime_start and trials[a.calls - 1].datetime_complete:
            wall = (trials[a.calls - 1].datetime_complete - trials[0].datetime_start).total_seconds()
        rows[d.name] = {"trials": len(h), f"hv_at_{a.calls}": h[a.calls - 1] if len(h) >= a.calls else None,
                        "hv_final": h[-1] if h else 0.0, f"wall_seconds_to_{a.calls}": wall}
    by = {}
    for name, r in rows.items():
        m = name.rsplit("-", 1)[0]
        v = r[f"hv_at_{a.calls}"]
        if v is not None:
            by.setdefault(m, []).append((v, r[f"wall_seconds_to_{a.calls}"]))
    summary = {m: {"mean_hv": float(np.mean([x for x, _ in v])), "seeds": len(v),
                   "mean_wall_s": float(np.mean([w for _, w in v if w is not None])) if any(w for _, w in v) else None}
               for m, v in by.items()}
    out = {"calls": a.calls, "reference_minimized": REF.tolist(), "runs": rows, "summary": summary}
    (ROOT / "benchmarks" / f"optimizer_compare_at{a.calls}.json").write_text(json.dumps(out, indent=1))
    print(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
