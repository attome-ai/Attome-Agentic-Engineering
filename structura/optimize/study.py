"""Optuna study over a spec's free parameters (D1-T1).

Every trial runs the engine's full evaluation; status, checks and metrics are stored as trial attributes in a SQLite
study (runs/<study>/study.db) so the history view and the trust panel can read them. Failed evaluations are stored as
failed trials with their reason, never as good scores. The LLM is never called inside the loop.
"""
from __future__ import annotations

import json
import math
import time
from pathlib import Path

import optuna

from .evaluate import SpecInfo, evaluate, load_spec

optuna.logging.set_verbosity(optuna.logging.WARNING)


def make_sampler(name: str, seed: int, n_objectives: int):
    if name == "random":
        return optuna.samplers.RandomSampler(seed=seed)
    if name == "nsga2":
        return optuna.samplers.NSGAIISampler(seed=seed, constraints_func=constraints)
    if name == "nsga3":
        return optuna.samplers.NSGAIIISampler(seed=seed, constraints_func=constraints)
    if name == "tpe":
        return optuna.samplers.TPESampler(seed=seed, constraints_func=constraints, multivariate=True)
    if name == "default":
        return None  # Optuna's own default for the objective count
    raise ValueError(f"unknown sampler {name!r}")


def constraints(trial: optuna.trial.FrozenTrial) -> list[float]:
    """Optuna convention: values <= 0 are satisfied."""
    return trial.user_attrs.get("violations", [1.0])


def suggest(trial: optuna.Trial, spec: SpecInfo) -> dict[str, float]:
    out = {}
    for k, (lo, hi) in spec.free.items():
        out[k] = float(trial.suggest_int(k, int(lo), int(hi))) if k in spec.integer else trial.suggest_float(k, lo, hi)
    return out


def objective_fn(spec: SpecInfo, mesh_size: float | None, threads: int):
    def fn(trial: optuna.Trial):
        params = suggest(trial, spec)
        t0 = time.perf_counter()
        ev = evaluate(spec, params, mesh_size=mesh_size, threads=threads)
        trial.set_user_attr("status", ev.status)
        trial.set_user_attr("seconds", round(time.perf_counter() - t0, 3))
        if ev.status != "ok":
            trial.set_user_attr("reason", ev.reason[:500])
            trial.set_user_attr("violations", [1.0] * len(spec.requirements))
            return [float("nan")] * len(spec.objectives)  # recorded as a failed trial
        trial.set_user_attr("violations", ev.violations)
        trial.set_user_attr("feasible", ev.feasible)
        trial.set_user_attr("metrics", ev.raw["metrics"])
        trial.set_user_attr("checks", ev.raw["checks"])
        trial.set_user_attr("hash", ev.raw["hash"])
        return ev.objectives
    return fn


def run(spec_path: str | Path, n_trials: int, sampler: str = "nsga2", seed: int = 0, study_dir: str | Path | None = None,
        mesh_size: float | None = None, threads: int = 1, n_jobs: int = 1, study_name: str | None = None,
        callbacks: list | None = None, seeds: list[dict] | None = None) -> optuna.Study:
    """seeds: designs evaluated first (e.g. the corners of the design space), so the search starts from known points."""
    spec = load_spec(spec_path)
    study_dir = Path(study_dir or Path("runs") / f"{spec.id}-{sampler}-{seed}")
    study_dir.mkdir(parents=True, exist_ok=True)
    storage = f"sqlite:///{(study_dir / 'study.db').resolve().as_posix()}"
    study = optuna.create_study(study_name=study_name or study_dir.name, storage=storage, load_if_exists=True,
                                directions=spec.directions, sampler=make_sampler(sampler, seed, len(spec.objectives)))
    study.set_user_attr("spec_path", str(spec_path))
    study.set_user_attr("spec_hash", spec.hash)
    study.set_user_attr("objectives", spec.objectives)
    study.set_user_attr("requirements", [r["id"] for r in spec.requirements])
    study.set_user_attr("sampler", sampler)
    if seeds and not study.trials:
        for p in seeds:
            study.enqueue_trial({k: (int(round(v)) if k in spec.integer else float(v)) for k, v in p.items() if k in spec.free})
    study.optimize(objective_fn(spec, mesh_size, threads), n_trials=n_trials, n_jobs=n_jobs, catch=(), callbacks=callbacks)
    (study_dir / "summary.json").write_text(json.dumps(summary(study), indent=2))
    return study


def summary(study: optuna.Study) -> dict:
    trials = study.get_trials(deepcopy=False)
    states = {}
    for t in trials:
        key = t.user_attrs.get("status", t.state.name)
        states[key] = states.get(key, 0) + 1
    feasible = [t for t in trials if t.user_attrs.get("feasible")]
    return {"trials": len(trials), "by_status": states, "feasible": len(feasible),
            "objectives": study.user_attrs.get("objectives")}
