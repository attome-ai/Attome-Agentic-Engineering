"""D1-T2: both portfolio paths run on the bracket with integer parameters and constraints; the choice is logged."""
from pathlib import Path

import optuna

from structura.optimize import portfolio
from structura.optimize.evaluate import load_spec

SPEC = Path(__file__).resolve().parents[1] / "docs" / "examples" / "bracket_20kg.json"


def test_choice_rules():
    spec = load_spec(SPEC)
    assert portfolio.choose(spec, 100, eval_seconds=60)[0] == "botorch_qnehvi"   # expensive evaluations
    assert portfolio.choose(spec, 100, eval_seconds=1.2)[0] == "nsga2"           # cheap evaluations (D5)
    assert portfolio.choose(spec, 500, eval_seconds=60)[0] == "nsga2"            # large budget


def test_botorch_path(tmp_path):
    s = portfolio.run(SPEC, budget=16, seed=3, method="botorch_qnehvi", study_dir=tmp_path / "bo", mesh_size=6, threads=1, n_jobs=4)
    assert s.user_attrs["method"] == "botorch_qnehvi"
    trials = s.get_trials()
    assert len(trials) == 16
    ok = [t for t in trials if t.state == optuna.trial.TrialState.COMPLETE]
    assert ok and all(float(t.params["rib_count"]).is_integer() for t in ok)
    assert all("violations" in t.user_attrs for t in trials)


def test_nsga_path(tmp_path):
    s = portfolio.run(SPEC, budget=16, seed=3, method="nsga2", study_dir=tmp_path / "ga", mesh_size=6, n_jobs=4)
    assert s.user_attrs["method"] == "nsga2"
    assert len(s.get_trials()) == 16


def test_auto_choice_measures_and_picks_nsga_for_cheap_evaluations(tmp_path):
    s = portfolio.run(SPEC, budget=8, seed=4, study_dir=tmp_path / "auto", mesh_size=6, n_jobs=4)
    assert s.user_attrs["method"] == "nsga2"
    assert "cheap" in s.user_attrs["method_reason"]
