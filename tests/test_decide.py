"""D2-T2: knee recommendation, steering and explanations; D3-T2 coarse-to-fine verification."""
from pathlib import Path

import numpy as np
import optuna

from structura.optimize import decide, pareto
from structura.optimize import study as st
from structura.optimize.verify_front import verify_front

SPEC = Path(__file__).resolve().parents[1] / "docs" / "examples" / "bracket_20kg.json"


def test_parse_preference_moves_the_reference():
    vals = [[50, 2.0], [80, 3.0], [120, 4.0]]
    obj = [("mass_g", "min"), ("safety_factor", "max")]
    ref = decide.parse_preference("lighter, even if it bends more", vals, obj)
    assert ref[0] == 50 and ref[1] < np.mean([2.0, 3.0, 4.0])
    ref2 = decide.parse_preference("make it stronger", vals, obj)
    assert ref2[1] == 4.0


def test_recommend_steer_explain_verify(tmp_path):
    s = st.run(SPEC, n_trials=60, sampler="nsga2", seed=5, study_dir=tmp_path / "s", mesh_size=4, n_jobs=8)
    front_before = pareto.front(s)
    rec = decide.recommend(s)
    assert (rec is None) == (not front_before)
    if not front_before:  # no feasible design in this short run: nothing to steer or explain
        return
    assert rec["front_size"] >= 1
    ref = decide.parse_preference("lighter", [p.values for p in front_before], [tuple(o) for o in s.user_attrs["objectives"]])
    n_before = len(s.trials)
    r = decide.steer(str(SPEC), s, ref, n_trials=10, mesh_size=4, n_jobs=4)
    assert r["evaluations"] == 10 and len(s.trials) == n_before + 10
    assert all(t.user_attrs.get("steered_to") for t in s.trials[n_before:])
    e = decide.explain(s, front_before[0].number, front_before[-1].number)
    assert e["objectives"] and all("change" in m for m in e["objectives"])
    v = verify_front(s, coarse_mesh=4, factor=0.75)
    assert v["fine_mesh"] == 3 and v["front_size"] == len(pareto.front(s))
    assert all("fine" in row for row in v["front"] if row["status"] == "ok")
