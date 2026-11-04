"""Pareto front, hypervolume and decision support over a study (D2-T1, D2-T2).

All objectives are turned into minimization (maximized ones are negated) before dominance and hypervolume. Only
feasible, completed trials can be on the front. Hypervolume uses pymoo's exact indicator with an explicit reference
point, so runs are only compared with the same reference.
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
import optuna
from pymoo.indicators.hv import HV


@dataclass
class Point:
    number: int
    params: dict
    values: list[float]      # in the spec's own directions
    minimized: np.ndarray    # every objective as minimization
    metrics: dict


def _points(study: optuna.Study, feasible_only: bool = True) -> list[Point]:
    dirs = study.directions
    out = []
    for t in study.get_trials(deepcopy=False, states=(optuna.trial.TrialState.COMPLETE,)):
        if feasible_only and not t.user_attrs.get("feasible"):
            continue
        if t.values is None or any(not math.isfinite(v) for v in t.values):
            continue
        mz = np.array([v if d == optuna.study.StudyDirection.MINIMIZE else -v for v, d in zip(t.values, dirs)])
        out.append(Point(t.number, dict(t.params), list(t.values), mz, t.user_attrs.get("metrics", {})))
    return out


def non_dominated(points: list[Point]) -> list[Point]:
    front = []
    for p in points:
        dominated = any(np.all(q.minimized <= p.minimized) and np.any(q.minimized < p.minimized) for q in points)
        if not dominated:
            front.append(p)
    return sorted(front, key=lambda p: tuple(p.minimized))


def front(study: optuna.Study) -> list[Point]:
    return non_dominated(_points(study))


def hypervolume(points: list[Point], ref_minimized: np.ndarray) -> float:
    pts = [p.minimized for p in points if np.all(p.minimized < ref_minimized)]
    if not pts:
        return 0.0
    return float(HV(ref_point=np.asarray(ref_minimized, float))(np.array(pts)))


def reference_point(study: optuna.Study) -> np.ndarray | None:
    """A reference point just beyond the worst feasible value of each objective, for hypervolume in reports."""
    pts = _points(study)
    if not pts:
        return None
    worst = np.max([p.minimized for p in pts], axis=0)
    best = np.min([p.minimized for p in pts], axis=0)
    return worst + 0.1 * np.maximum(worst - best, 1e-9)


def hypervolume_history(study: optuna.Study, ref_minimized: np.ndarray) -> list[float]:
    """Hypervolume of the feasible front after each trial, in trial order."""
    trials = sorted(study.get_trials(deepcopy=False), key=lambda t: t.number)
    dirs = study.directions
    seen, hist = [], []
    for t in trials:
        if t.state == optuna.trial.TrialState.COMPLETE and t.user_attrs.get("feasible") and t.values is not None:
            mz = np.array([v if d == optuna.study.StudyDirection.MINIMIZE else -v for v, d in zip(t.values, dirs)])
            seen.append(Point(t.number, dict(t.params), list(t.values), mz, {}))
        hist.append(hypervolume(non_dominated(seen), ref_minimized) if seen else 0.0)
    return hist


def knee(points: list[Point]) -> Point | None:
    """The front point with the largest change in trade-off slope between its neighbours (normalized objectives)."""
    f = sorted(points, key=lambda p: p.minimized[0])
    if len(f) < 3:
        return f[0] if f else None
    m = np.array([p.minimized for p in f], float)
    lo, hi = m.min(axis=0), m.max(axis=0)
    span = np.where(hi - lo > 0, hi - lo, 1.0)
    n = (m - lo) / span
    best, best_i = -1.0, 1
    for i in range(1, len(f) - 1):
        a, b, c = n[i - 1], n[i], n[i + 1]
        v1, v2 = b - a, c - b
        ang = math.atan2(v1[1], v1[0]) - math.atan2(v2[1], v2[0])
        bend = abs((ang + math.pi) % (2 * math.pi) - math.pi)
        if bend > best:
            best, best_i = bend, i
    return f[best_i]
