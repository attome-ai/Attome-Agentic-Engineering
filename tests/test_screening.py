"""D1-T3 and D2-T1: Sobol screening, the stopping rule, Pareto front and hypervolume."""
from pathlib import Path

import numpy as np
import optuna

from structura.optimize import pareto
from structura.optimize.evaluate import load_spec
from structura.optimize.screening import StopWhenFlat, sobol

SPEC = Path(__file__).resolve().parents[1] / "docs" / "examples" / "bracket_20kg.json"


def test_sobol_screening_ranks_parameters():
    spec = load_spec(SPEC)
    r = sobol(spec, n_base=8, mesh_size=6, n_jobs=8)
    assert r["samples"] == 8 * (len(spec.free) + 2)
    mass = r["indices"]["mass_g"]
    # thickness drives mass far more than the fillet radius does
    assert mass["thickness"]["ST"] > mass["fillet_radius"]["ST"]
    for k, why in r["reasons"].items():
        assert "below 0.02" in why and k in r["frozen"]


def _study_with(points):
    s = optuna.create_study(directions=["minimize", "maximize"])
    dist = {"x": optuna.distributions.FloatDistribution(0, 1)}
    for i, (m, sf, feas) in enumerate(points):
        s.add_trial(optuna.trial.create_trial(params={"x": i / 100}, distributions=dist, values=[m, sf],
                                              user_attrs={"feasible": feas}))
    return s


def test_front_and_hypervolume():
    s = _study_with([(100, 2.0, True), (120, 3.0, True), (110, 1.5, True), (90, 4.0, False), (150, 3.5, True)])
    f = pareto.front(s)
    assert [p.values for p in f] == [[100, 2.0], [120, 3.0], [150, 3.5]]  # (110, 1.5) dominated; infeasible excluded
    hv = pareto.hypervolume(f, np.array([400.0, 0.0]))
    # rectangles: (400-100)*2 + (400-120)*(3-2) + (400-150)*(3.5-3)
    assert abs(hv - (600 + 280 + 125)) < 1e-6
    k = pareto.knee(f)
    assert k is not None and k.values in ([100, 2.0], [120, 3.0], [150, 3.5])


def test_stop_rule_fires_on_flat_hypervolume():
    s = optuna.create_study(directions=["minimize", "maximize"])

    def objective(trial):
        trial.suggest_float("x", 0, 1)
        trial.set_user_attr("feasible", True)
        return 100.0, 2.0  # the front never improves

    cb = StopWhenFlat(np.array([400.0, 0.0]), window=20, tol=0.01, min_trials=30)
    s.optimize(objective, n_trials=200, callbacks=[cb])
    assert len(s.trials) < 200
    assert s.user_attrs.get("stop_reason") and "hypervolume improved" in s.user_attrs["stop_reason"]
