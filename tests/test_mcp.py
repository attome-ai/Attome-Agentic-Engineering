"""E1-T1: the MCP server exposes the engine as tools; the agent workflow runs end to end with the approval gate."""
import json
import time
from pathlib import Path

import anyio
import mcp

from structura.agent import mcp_server as srv

SPEC = Path(__file__).resolve().parents[1] / "docs" / "examples" / "bracket_20kg.json"
TOOLS = {"build_report", "derive_load", "rank_questions", "propose_relaxation", "list_families", "describe_grammar", "plan_study", "create_study", "accept_spec", "check_feasibility",
         "compose_design", "evaluate_design", "run_optimization", "get_job", "get_results", "steer", "explain", "verify"}


def test_tools_over_the_protocol():
    async def go():
        async with mcp.Client(srv.mcp) as c:
            tools = await c.list_tools()
            names = {t.name for t in tools.tools}
            assert TOOLS <= names
            for t in tools.tools:
                assert t.description and len(t.description) > 40  # docs in context
            r = await c.call_tool("list_families", {})
            assert "bracket" in json.dumps(r.model_dump())
    anyio.run(go)


def test_workflow_with_approval_gate(tmp_path, monkeypatch):
    monkeypatch.setattr(srv.S, "STUDIES", tmp_path / "studies")
    spec = json.loads(SPEC.read_text())
    sid = "bracket-20kg-test"
    assert srv.plan_study(sid, "bracket", "linear static", "min mass, max safety factor", 16)["ok"]
    r = srv.create_study(spec, study_id=sid)
    assert r["ok"] and r["needs_acceptance"] and r["spec_card"]["requirements"]
    bad = dict(spec, material="unobtainium")
    e = srv.create_study(bad, study_id=sid + "-bad")
    assert not e["ok"] and e["error"] == "spec" and "material" in e["message"]
    f = srv.check_feasibility(sid)
    assert f["ok"] and f["feasible"] is True
    refused = srv.run_optimization(sid, budget=16, method="nsga2")
    assert not refused["ok"] and refused["error"] == "approval"
    assert srv.accept_spec(sid, "owner", "test acceptance")["ok"]
    job = srv.run_optimization(sid, budget=16, method="nsga2")["job_id"]
    for _ in range(600):
        j = srv.get_job(job)
        if j["state"] != "running":
            break
        time.sleep(0.5)
    assert j["state"] == "done", srv._jobs[job]
    res = srv.get_results(sid)
    assert res["ok"] and res["summary"]["trials"] == 16
    if len(res["front"]) >= 2:
        ex = srv.explain(sid, res["front"][0]["trial"], res["front"][-1]["trial"])
        assert ex["ok"] and ex["objectives"]
    steps = [e["step"] for e in srv.S.timeline(sid)]
    for s in ("plan", "spec", "feasibility", "accept", "optimize"):
        assert s in steps
    c = srv.compose_design(family="bracket", params={"thickness": 40}, study_id=sid)
    assert not c["ok"] and c["error"] == "graph"


def test_derive_load_and_playbooks():
    r = srv.derive_load(20, "variable_action", supports=2)
    assert r["ok"] and abs(r["nominal_N"] - 98.067) < 0.01 and r["factor"] == 1.5 and "EN 1990" in r["source"]
    assert not srv.derive_load(-1)["ok"]
    from structura.agent import playbooks as PB
    assert PB.classify("wall of 0.80 mm is thinner than the minimum 1.2 mm")["kind"] == "gate.thin_wall"
    b = PB.RepairBudget("gate")
    b.record("wall of 0.8 mm is thinner than the minimum")
    step = b.record("wall of 0.9 mm is thinner than the minimum")
    assert step["escalate_model"] and not step["stop"]
    for _ in range(8):
        step = b.record("wall is thinner than the minimum")
    assert step["stop"] and step["next"] == "stop and report"
    e = srv.compose_design(family="bracket", params={"thickness": 40})
    assert e["playbook"] == "graph.bounds" and e["actions"]


def test_rank_questions_finds_the_load_that_matters():
    spec = json.loads(SPEC.read_text())
    unknowns = [
        {"name": "load magnitude", "question": "Is 20 kg the most it will ever hold?", "low": {}, "high": {"load_scale": 2.0}},
        {"name": "material", "question": "PETG or PLA?", "low": {}, "high": {"material": "PLA"}},
    ]
    r = srv.rank_questions(spec, unknowns)
    assert r["ok"]
    names = [q["name"] for q in r["ask"]]
    assert names and names[0] == "load magnitude"  # doubling the load halves the safety factor
    assert all("stake" in q for q in r["ask"])


def test_verify_tool_writes_a_graded_trust_record(tmp_path, monkeypatch):
    monkeypatch.setattr(srv.S, "STUDIES", tmp_path / "studies")
    sid = "verify-test"
    srv.create_study(json.loads(SPEC.read_text()), study_id=sid)
    knee = json.loads((Path(__file__).resolve().parents[1] / "benchmarks" / "p2_demo.json").read_text())["recommended_knee"]["params"]
    v = srv.verify(sid, {k: float(x) for k, x in knee.items()}, fine_mesh=3.0)
    assert v["ok"] and v["grade"] in ("A", "B", "C")
    assert (Path(v["folder"]) / "trust.json").exists()
    assert v["grade"] == "A" and v["status"] == "verified"  # the P2 demo's recommended design


def test_propose_relaxation_for_the_100kg_brief(tmp_path, monkeypatch):
    monkeypatch.setattr(srv.S, "STUDIES", tmp_path / "studies")
    spec = json.loads((SPEC.parent / "bracket_100kg.json").read_text())
    sid = "relax-test"
    srv.create_study(spec, study_id=sid)
    srv.accept_spec(sid, "owner")
    r = srv.propose_relaxation(sid)
    assert r["ok"] and not r["feasible"] and r["needs_acceptance"]
    ch = {c["id"]: c for c in r["changes"]}
    assert "R1" in ch and ch["R1"]["new"] < 2.0
    assert not srv.S.accepted(sid)  # a proposal never inherits the old acceptance
    assert srv.run_optimization(sid, budget=4)["error"] == "approval"
