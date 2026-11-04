"""E1-T1: the MCP server exposes the engine as tools; the agent workflow runs end to end with the approval gate."""
import json
import time
from pathlib import Path

import anyio
import mcp

from structura.agent import mcp_server as srv

SPEC = Path(__file__).resolve().parents[1] / "docs" / "examples" / "bracket_20kg.json"
TOOLS = {"list_families", "describe_grammar", "plan_study", "create_study", "accept_spec", "check_feasibility",
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
    assert j["state"] == "done", j
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
