"""Decision support on the Pareto front (D2-T2): recommended knee point, steering toward a target, explanations.

steer() takes a reference point in objective space (the agent turns a sentence such as "lighter, even if it bends
more" into one through its tool; parse_preference() is the deterministic fallback for a few phrasings) and runs a
focused search: each candidate minimizes the weighted Chebyshev distance to the reference, with the spec's hard
requirements still enforced, and every evaluation is added to the main study tagged with the reference. Steering
never changes the spec's hard limits.
explain() returns the data the agent turns into a paragraph: which parameters differ, how much each matters
(Sobol total index when available) and how every objective moves.
"""
from __future__ import annotations

import math
import re

import numpy as np
import optuna

from . import pareto
from .evaluate import SpecInfo, evaluate, load_spec
from .portfolio import _record
from .study import suggest


def recommend(study: optuna.Study) -> dict | None:
    f = pareto.front(study)
    k = pareto.knee(f)
    if k is None:
        return None
    others = [p for p in f if p.number != k.number]
    return {"trial": k.number, "params": k.params, "values": k.values, "front_size": len(f),
            "neighbours": [{"trial": p.number, "values": p.values} for p in sorted(others, key=lambda p: abs(p.number - k.number))[:2]]}


def parse_preference(text: str, front_values: list[list[float]], objectives: list[tuple[str, str]]) -> list[float]:
    """Deterministic fallback: turn a short preference into a reference point from the current front's range."""
    vals = np.array(front_values, float)
    lo, hi, mid = vals.min(axis=0), vals.max(axis=0), vals.mean(axis=0)
    ref = mid.copy()
    t = text.lower()
    for i, (q, sense) in enumerate(objectives):
        better = lo[i] if sense == "min" else hi[i]
        worse = hi[i] if sense == "min" else lo[i]
        if q.startswith("mass") and re.search(r"\b(light|lighter|lightest|less material|cheaper)\b", t):
            ref[i] = better
        elif q.startswith("mass") and re.search(r"\b(heavier is fine|weight does not matter)\b", t):
            ref[i] = worse
        elif q == "safety_factor" and re.search(r"\b(strong|stronger|safer|stiffer|sturdier)\b", t):
            ref[i] = better
        elif q == "safety_factor" and re.search(r"\b(bends more|less safe|weaker is fine)\b", t):
            ref[i] = (mid[i] + worse) / 2
    return ref.tolist()


def steer(spec_path: str, study: optuna.Study, reference: list[float], n_trials: int = 30, mesh_size: float | None = None,
          seed: int = 0, n_jobs: int = 4) -> dict:
    spec: SpecInfo = load_spec(spec_path)
    ref = np.asarray(reference, float)
    pts = pareto.front(study) or pareto._points(study, feasible_only=False)
    scale = np.ptp(np.array([p.values for p in pts], float), axis=0) if len(pts) > 1 else np.abs(ref)
    scale = np.where(scale > 0, scale, 1.0)
    signs = np.array([1.0 if s == "min" else -1.0 for _, s in spec.objectives])

    def objective(trial: optuna.Trial):
        params = suggest(trial, spec)
        ev = evaluate(spec, params, mesh_size=mesh_size)
        _record(study, spec, params, ev, 0.0, extra={"steered_to": list(map(float, ref))}, method="steer")
        if ev.status != "ok":
            trial.set_user_attr("violations", [1.0] * len(spec.requirements))
            return float("inf")
        trial.set_user_attr("violations", ev.violations)
        d = signs * (np.array(ev.objectives) - ref) / scale  # positive = worse than the reference
        return float(np.max(np.maximum(d, 0)) + 1e-3 * np.sum(d))

    focus = optuna.create_study(direction="minimize",
                                sampler=optuna.samplers.TPESampler(seed=seed, constraints_func=lambda t: t.user_attrs["violations"]))
    before = len(study.trials)
    focus.optimize(objective, n_trials=n_trials, n_jobs=n_jobs)
    added = study.trials[before:]
    study.set_user_attr("last_reference_point", list(map(float, ref)))
    try:
        best = focus.best_value
    except ValueError:  # no feasible steered design yet
        best = None
    return {"reference": ref.tolist(), "evaluations": len(added), "best_distance": best}


def explain(study: optuna.Study, a: int, b: int, sobol: dict | None = None) -> dict:
    ta, tb = study.trials[a], study.trials[b]
    diff = []
    for k in sorted(set(ta.params) | set(tb.params)):
        va, vb = ta.params.get(k), tb.params.get(k)
        if va is None or vb is None or not math.isclose(va, vb, rel_tol=1e-9, abs_tol=1e-12):
            st = None
            if sobol:
                st = {q: sobol["indices"][q][k]["ST"] for q in sobol.get("indices", {}) if k in sobol["indices"][q]}
            diff.append({"parameter": k, "a": va, "b": vb, "sobol_total": st})
    objs = study.user_attrs.get("objectives", [])
    moves = []
    if ta.values and tb.values:
        for i, (q, sense) in enumerate(objs):
            moves.append({"quantity": q, "a": ta.values[i], "b": tb.values[i], "change": tb.values[i] - ta.values[i],
                          "better_for_b": (tb.values[i] < ta.values[i]) == (sense == "min")})
    return {"a": a, "b": b, "parameters_that_differ": diff, "objectives": moves}
