"""Sobol screening and the stopping rule (D1-T3).

screening.sobol() runs a Saltelli sample of the free parameters on coarse meshes and returns first-order and total
Sobol indices per parameter for every objective. Parameters whose total index is below 0.02 for every objective are
frozen at their best sampled value; the list and the reason go to the study log.
StopWhenFlat is an Optuna callback: stop when hypervolume improved less than 1% over the last 20 trials.
"""
from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor

import numpy as np
import optuna
from SALib.analyze import sobol as sobol_analyze
from SALib.sample import sobol as sobol_sample

from . import pareto
from .evaluate import SpecInfo, evaluate


def sobol(spec: SpecInfo, n_base: int = 16, mesh_size: float = 6.0, threads: int = 1, n_jobs: int = 8, seed: int = 0) -> dict:
    names = list(spec.free)
    problem = {"num_vars": len(names), "names": names, "bounds": [list(spec.free[k]) for k in names]}
    X = sobol_sample.sample(problem, n_base, calc_second_order=False, seed=seed)

    def run(row):
        p = {k: (float(round(v)) if k in spec.integer else float(v)) for k, v in zip(names, row)}
        return p, evaluate(spec, p, mesh_size=mesh_size, threads=threads)

    with ThreadPoolExecutor(max_workers=n_jobs) as pool:
        results = list(pool.map(run, X))
    ok = [ev.status == "ok" for _, ev in results]
    out = {"samples": len(X), "failed": int(len(X) - sum(ok)), "parameters": names, "indices": {}, "frozen": {}, "reasons": {}}
    if sum(ok) < len(X):
        # Saltelli needs every row; failed designs get the worst observed value so they cannot look attractive
        pass
    for oi, (q, sense) in enumerate(spec.objectives):
        vals = np.array([ev.objectives[oi] if ev.status == "ok" else np.nan for _, ev in results], float)
        if np.isnan(vals).all():
            continue
        worst = np.nanmax(vals) if sense == "min" else np.nanmin(vals)
        vals = np.where(np.isnan(vals), worst, vals)
        si = sobol_analyze.analyze(problem, vals, calc_second_order=False, print_to_console=False, seed=seed)
        out["indices"][q] = {k: {"S1": float(si["S1"][i]), "ST": float(si["ST"][i])} for i, k in enumerate(names)}
    # best sampled design (feasible first, then by first objective)
    feasible = [(p, ev) for p, ev in results if ev.status == "ok" and ev.feasible]
    pool_best = feasible or [(p, ev) for p, ev in results if ev.status == "ok"]
    q0, s0 = spec.objectives[0]
    best_p = (min if s0 == "min" else max)(pool_best, key=lambda x: x[1].objectives[0])[0] if pool_best else None
    for k in names:
        st = [out["indices"][q][k]["ST"] for q in out["indices"]]
        if st and max(st) < 0.02 and best_p is not None:
            out["frozen"][k] = best_p[k]
            out["reasons"][k] = f"total Sobol index below 0.02 for every objective (max {max(st):.4f}); frozen at {best_p[k]:.4g}"
    return out


class StopWhenFlat:
    """Optuna callback: stop when hypervolume improved less than `tol` (relative) over the last `window` trials."""

    def __init__(self, ref_minimized, window: int = 20, tol: float = 0.01, min_trials: int = 30):
        self.ref = np.asarray(ref_minimized, float)
        self.window, self.tol, self.min_trials = window, tol, min_trials
        self.reason = None

    def __call__(self, study: optuna.Study, trial: optuna.trial.FrozenTrial) -> None:
        hist = pareto.hypervolume_history(study, self.ref)
        if len(hist) < max(self.min_trials, self.window + 1):
            return
        old, new = hist[-self.window - 1], hist[-1]
        if new <= 0:
            return
        gain = (new - old) / new
        if gain < self.tol:
            self.reason = f"hypervolume improved {100 * gain:.2f}% over the last {self.window} trials (threshold {100 * self.tol:g}%)"
            study.set_user_attr("stop_reason", self.reason)
            study.stop()
