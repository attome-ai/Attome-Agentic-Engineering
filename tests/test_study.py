"""D1-T1: a 20-trial study completes and every trial has a status; failures are failed trials, not scores."""
from pathlib import Path

import optuna

from structura.optimize import study as st
from structura.optimize.evaluate import evaluate, load_spec

SPEC = Path(__file__).resolve().parents[1] / "docs" / "examples" / "bracket_20kg.json"


def test_single_evaluation():
    spec = load_spec(SPEC)
    assert spec.directions == ["minimize", "maximize"]
    assert "rib_count" in spec.integer
    ev = evaluate(spec, {"thickness": 6, "plate_width": 50, "rib_count": 1, "rib_thickness": 4, "fillet_radius": 3}, mesh_size=6)
    assert ev.status == "ok"
    assert len(ev.objectives) == 2 and ev.objectives[0] > 0
    assert len(ev.violations) == 3


def test_twenty_trial_study(tmp_path):
    study = st.run(SPEC, n_trials=20, sampler="nsga2", seed=1, study_dir=tmp_path / "s", mesh_size=6, threads=2, n_jobs=4)
    trials = study.get_trials()
    assert len(trials) == 20
    for t in trials:
        assert "status" in t.user_attrs
        if t.user_attrs["status"] != "ok":
            assert t.state == optuna.trial.TrialState.FAIL
            assert t.user_attrs.get("reason")
    assert (tmp_path / "s" / "study.db").exists()
    assert (tmp_path / "s" / "summary.json").exists()


def test_cache_serves_repeats(tmp_path, monkeypatch):
    import time
    from structura.optimize import cache
    monkeypatch.setattr(cache, "CACHE_DIR", tmp_path / "cache")
    spec = load_spec(SPEC)
    p = {"thickness": 7, "plate_width": 45, "rib_count": 2, "rib_thickness": 5, "fillet_radius": 2}
    a = evaluate(spec, p, mesh_size=6)
    t0 = time.perf_counter()
    b = evaluate(spec, p, mesh_size=6)
    assert time.perf_counter() - t0 < 0.1
    assert b.raw.get("from_cache") and a.objectives == b.objectives and a.violations == b.violations
    assert evaluate(spec, dict(p, thickness=7.5), mesh_size=6).raw.get("from_cache") is None


def test_audit_log_is_hash_chained(tmp_path, monkeypatch):
    """I1: an edited or removed timeline entry is detected."""
    import json
    from structura.agent import studies as S
    monkeypatch.setattr(S, "STUDIES", tmp_path)
    for i in range(4):
        S.log("audit", "step", "ok", n=i)
    assert S.verify_log("audit") == {"ok": True, "entries": 4, "head": S.timeline("audit")[-1]["hash"]}
    p = S.folder("audit") / "timeline.jsonl"
    lines = p.read_text(encoding="utf-8").splitlines()
    edited = json.loads(lines[1]); edited["n"] = 99
    p.write_text("\n".join([lines[0], json.dumps(edited)] + lines[2:]) + "\n", encoding="utf-8")
    r = S.verify_log("audit")
    assert not r["ok"] and r["broken_at"] == 1 and "edited" in r["reason"]
    p.write_text("\n".join([lines[0]] + lines[2:]) + "\n", encoding="utf-8")  # entry removed
    r = S.verify_log("audit")
    assert not r["ok"] and r["broken_at"] == 1 and "previous" in r["reason"]
