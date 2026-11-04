"""Method portfolio (D1-T2, revised by decision D5): choose the search method from the problem, run it, keep one history.

Bayesian optimization (BoTorch qNEHVI through Ax) when one evaluation costs about as much as fitting its model or more
(measured on the centre design before the study) and the budget is under 200; otherwise NSGA-II for two objectives and
NSGA-III for three or four.
Integer parameters are native in both paths; the spec's requirements become Ax outcome constraints (feasibility-aware
search) or Optuna constraint functions. Every Ax trial is also written into the study's Optuna SQLite storage, so the
history and Pareto views read one source whichever method ran. The choice and its reason are stored on the study.
"""
from __future__ import annotations

import json
import logging
import math
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import optuna

from . import study as st
from .evaluate import SpecInfo, evaluate, load_spec

logging.getLogger("ax").setLevel(logging.ERROR)


# Measured on the bracket (decision D5): BoTorch qNEHVI reached 28% more hypervolume than NSGA-II at 40 solver calls,
# but its model fitting cost about 43 s per trial against about 1 s per coarse evaluation (115x the wall clock).
BO_MODEL_SECONDS = 43.0


def choose(spec: SpecInfo, budget: int, eval_seconds: float | None = None) -> tuple[str, str]:
    """Bayesian optimization when an evaluation costs about as much as fitting the model (or more), else NSGA."""
    n = len(spec.objectives)
    if n <= 3 and budget < 200 and (eval_seconds is None or eval_seconds >= 0.5 * BO_MODEL_SECONDS):
        cost = "unmeasured" if eval_seconds is None else f"{eval_seconds:.1f} s"
        return "botorch_qnehvi", (f"evaluations are expensive ({cost} each, model fit about {BO_MODEL_SECONDS:.0f} s) and the budget is "
                                  f"{budget}: a Gaussian-process model (BoTorch qNEHVI) spends each evaluation where it most improves the front")
    why = (f"evaluations are cheap ({eval_seconds:.1f} s each against about {BO_MODEL_SECONDS:.0f} s of model fitting per trial)"
           if eval_seconds is not None else f"budget {budget} >= 200")
    if n <= 2:
        return "nsga2", f"{why}: NSGA-II runs many evaluations in parallel for the same wall clock"
    return "nsga3", f"{why}: NSGA-III keeps diversity with three or more objectives"


def measure_eval_seconds(spec: SpecInfo, mesh_size: float | None) -> float:
    """Time one uncached evaluation of the centre of the design space."""
    centre = {k: (float(round((lo + hi) / 2)) if k in spec.integer else (lo + hi) / 2) for k, (lo, hi) in spec.free.items()}
    t0 = time.perf_counter()
    evaluate(spec, centre, mesh_size=mesh_size, use_cache=False)
    return time.perf_counter() - t0


def _ax_objective(spec: SpecInfo) -> str:
    return ", ".join(("-" if sense == "min" else "") + q for q, sense in spec.objectives)


def _ax_constraints(spec: SpecInfo) -> list[str]:
    return [f"{r['quantity']} {r['op']} {float(r['value'])}" for r in spec.requirements]


def _record(study: optuna.Study, spec: SpecInfo, params: dict, ev, seconds: float, extra: dict | None = None,
            method: str = "botorch_qnehvi") -> None:
    dists = {k: (optuna.distributions.IntDistribution(int(lo), int(hi)) if k in spec.integer
                 else optuna.distributions.FloatDistribution(lo, hi)) for k, (lo, hi) in spec.free.items()}
    attrs = {"status": ev.status, "seconds": round(seconds, 3), "method": method, **(extra or {})}
    if ev.status == "ok":
        attrs.update(violations=ev.violations, feasible=ev.feasible, metrics=ev.raw["metrics"], checks=ev.raw["checks"],
                     hash=ev.raw["hash"])
        t = optuna.trial.create_trial(params={k: (int(v) if k in spec.integer else v) for k, v in params.items()},
                                      distributions=dists, values=ev.objectives, user_attrs=attrs)
    else:
        attrs.update(reason=ev.reason[:500], violations=[1.0] * len(spec.requirements))
        t = optuna.trial.create_trial(params={k: (int(v) if k in spec.integer else v) for k, v in params.items()},
                                      distributions=dists, state=optuna.trial.TrialState.FAIL, user_attrs=attrs)
    study.add_trial(t)


def run_ax(spec: SpecInfo, budget: int, seed: int, study: optuna.Study, mesh_size: float | None, threads: int,
           batch: int = 4, on_trial=None) -> None:
    from ax import Client
    from ax.api.configs import RangeParameterConfig

    client = Client(random_seed=seed)
    client.configure_experiment(parameters=[
        RangeParameterConfig(name=k, bounds=(lo, hi), parameter_type="int" if k in spec.integer else "float")
        for k, (lo, hi) in spec.free.items()])
    client.configure_optimization(objective=_ax_objective(spec), outcome_constraints=_ax_constraints(spec))
    metrics_needed = {q for q, _ in spec.objectives} | {r["quantity"] for r in spec.requirements}
    done = 0
    with ThreadPoolExecutor(max_workers=batch) as pool:
        while done < budget:
            trials = client.get_next_trials(max_trials=min(batch, budget - done))
            futures = {}
            for idx, p in trials.items():
                params = {k: float(v) for k, v in p.items()}
                futures[idx] = (params, time.perf_counter(), pool.submit(evaluate, spec, params, mesh_size, threads))
            for idx, (params, t0, fut) in futures.items():
                ev = fut.result()
                if ev.status == "ok" and all(q in ev.raw["metrics"] for q in metrics_needed):
                    client.complete_trial(trial_index=idx, raw_data={q: float(ev.raw["metrics"][q]) for q in metrics_needed})
                else:
                    client.mark_trial_failed(trial_index=idx, failed_reason=ev.reason[:200])
                _record(study, spec, params, ev, time.perf_counter() - t0)
                done += 1
                if on_trial:
                    on_trial(ev.status == "ok" and ev.feasible)


def run(spec_path: str | Path, budget: int, seed: int = 0, method: str | None = None, study_dir: str | Path | None = None,
        mesh_size: float | None = None, threads: int = 1, n_jobs: int = 4, on_trial=None) -> optuna.Study:
    """on_trial(feasible: bool) is called after every finished evaluation (progress reporting without touching storage)."""
    spec = load_spec(spec_path)
    chosen, reason = (method, "method set by the caller") if method else choose(spec, budget, measure_eval_seconds(spec, mesh_size))
    study_dir = Path(study_dir or Path("runs") / f"{spec.id}-{chosen}-{seed}")
    study_dir.mkdir(parents=True, exist_ok=True)
    if chosen in ("nsga2", "nsga3", "random", "tpe", "default"):
        cbs = [lambda study, t: on_trial(bool(t.user_attrs.get("feasible")))] if on_trial else None
        s = st.run(spec_path, budget, sampler=chosen, seed=seed, study_dir=study_dir, mesh_size=mesh_size, threads=threads,
                   n_jobs=n_jobs, callbacks=cbs)
    elif chosen == "botorch_qnehvi":
        storage = f"sqlite:///{(study_dir / 'study.db').resolve().as_posix()}"
        s = optuna.create_study(study_name=study_dir.name, storage=storage, load_if_exists=True, directions=spec.directions)
        s.set_user_attr("spec_path", str(spec_path))
        s.set_user_attr("spec_hash", spec.hash)
        s.set_user_attr("objectives", spec.objectives)
        s.set_user_attr("requirements", [r["id"] for r in spec.requirements])
        s.set_user_attr("sampler", chosen)
        run_ax(spec, budget, seed, s, mesh_size, threads, batch=n_jobs, on_trial=on_trial)
        (study_dir / "summary.json").write_text(json.dumps(st.summary(s), indent=2))
    else:
        raise ValueError(f"unknown method {chosen!r}")
    s.set_user_attr("method", chosen)
    s.set_user_attr("method_reason", reason)
    return s
