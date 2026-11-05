"""E1-T1: the MCP server exposes the engine as tools; the agent workflow runs end to end with the approval gate."""
import json
import time
from pathlib import Path

import anyio
import mcp

from structura import engine
from structura.agent import mcp_server as srv
from structura.agent import studies as S
ROOT = Path(__file__).resolve().parents[1]

SPEC = Path(__file__).resolve().parents[1] / "docs" / "examples" / "bracket_20kg.json"
TOOLS = {"build_report", "derive_load", "rank_questions", "propose_relaxation", "list_families", "describe_grammar", "plan_study", "create_study", "accept_spec", "check_feasibility",
         "compose_design", "evaluate_design", "run_optimization", "get_job", "get_results", "steer", "explain", "verify", "manufacturing_advice", "upload_part", "add_face", "add_lightening_pocket", "freeform_cad", "compose_candidates", "render_views", "pick_face", "select_face", "read_brief_image", "annotate"}


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
    demo = json.loads((Path(__file__).resolve().parents[1] / "benchmarks" / "p2_demo.json").read_text())
    knee = demo["recommended_knee"]["params"]
    v = srv.verify(sid, {k: float(x) for k, x in knee.items()}, fine_mesh=3.0)
    assert v["ok"] and v["grade"] in ("A", "B", "C")
    assert (Path(v["folder"]) / "trust.json").exists()
    assert v["grade"] == demo["knee_trust"]["grade"]  # the tool reproduces the grade stored with the P2 demo


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
    lo = r["load_options"][0]
    assert lo["id"] == "R1" and 0 < lo["load_fraction"] < 1  # the strongest design meets SF 2 at about a third of 100 kg
    assert not srv.S.accepted(sid)  # a proposal never inherits the old acceptance
    assert srv.run_optimization(sid, budget=4)["error"] == "approval"


def test_manufacturing_advice(tmp_path, monkeypatch):
    monkeypatch.setattr(srv.S, "STUDIES", tmp_path / "studies")
    sid = "mfg-test"
    srv.create_study(json.loads(SPEC.read_text()), study_id=sid)
    r = srv.manufacturing_advice(sid, {"thickness": 11.773, "plate_width": 58.81, "rib_count": 2, "rib_thickness": 7.673, "fillet_radius": 2.778})
    assert r["ok"] and r["recommended_process"] == "fdm"
    assert r["fdm"]["cost"]["total"] > 0 and r["cnc"]["sharp_internal_corners"] > 0
    al = srv.manufacturing_advice(sid, {"thickness": 8}, material="AL6061T6")
    assert al["ok"] and "fdm" not in al and any(a["process"] == "fdm" and not a["fits"] for a in al["advice"])


def test_upload_and_analyse_a_step_part(tmp_path, monkeypatch):
    """A3: an uploaded STEP is inspected, its faces named, and analysed as-is through the normal study tools."""
    from structura import engine
    monkeypatch.setattr(srv.S, "STUDIES", tmp_path / "studies")
    engine.call("build", family="bracket", out=str(tmp_path / "b"))  # default bracket as the "uploaded" file
    name = "pytest_upload"
    try:
        up = srv.upload_part(str(tmp_path / "b" / "part.step"), name)
        assert up["ok"] and up["valid"] and len(up["holes"]) == 2 and up["hole_patterns"][0]["count"] == 2
        end = next(f for f in up["faces"] if f.get("selector", {}).get("normal") == "x" and abs(f["selector"]["at"] - 120) < 1e-6)
        assert srv.add_face(up["family"], end["id"], "arm_end")["ok"]
        hole_face = next(k for k in up["named_faces"] if k.startswith("holes_"))
        spec = json.loads(SPEC.read_text())
        spec.update(id="upload-test", family=up["family"], free_params={})
        inferred = up["inferred_supports"]  # C3: read from the part: the two bolt holes, and the wall face they press against
        assert [s.get("kind") for s in inferred] == ["bolted", None] and inferred[0]["group"] == hole_face and inferred[1]["dofs"] == "1"
        spec["load_cases"][0]["supports"] = [{k: v for k, v in s.items() if k != "why"} for s in inferred]
        spec["load_cases"][0]["loads"] = [{"group": "arm_end", "force": [0, 0, -196.2]}]
        sid = "upload-test"
        srv.create_study(spec, study_id=sid)
        srv.accept_spec(sid, "owner")
        r = srv.evaluate_design(sid, {})
        assert r["ok"] and r["status"] == "ok" and r["metrics"]["mass_g"] > 0
        job = srv.run_optimization(sid, budget=4)
        if job.get("ok"):  # the job starts, then refuses a spec with nothing to optimize
            import time
            for _ in range(60):
                j = srv.get_job(job["job_id"])
                if j.get("state") != "running":
                    break
                time.sleep(0.5)
            assert j["state"] == "error"
    finally:
        for p in (ROOT / "families" / "uploads").glob(f"{name}.*"):
            p.unlink()


def test_freeform_cad_fallback():
    """B1: model-written CadQuery runs isolated, is gated and inspected; errors come back structured for repair."""
    code = '''
plate = cq.Workplane("XY").box(80, 40, 10, centered=(False, False, False))
plate = plate.faces(">Z").workplane(centerOption="CenterOfBoundBox").pushPoints([(-20, 0), (20, 0)]).hole(6.5)
boss = (cq.Workplane("XY").workplane(offset=10).center(40, 20).rect(20, 20)
        .workplane(offset=15).circle(6).loft())  # a loft: not in the design grammar
result = plate.union(boss)
'''
    name = "pytest_freeform"
    try:
        r = srv.freeform_cad(code, name)
        assert r["ok"], r
        assert r["gate"]["ok"] and len(r["holes"]) == 2
        assert r["family"] == f"uploads/{name}"
        bad = srv.freeform_cad("result = cq.Workplane('XY').box(10, 10)", name)  # missing argument
        assert not bad["ok"] and bad["error"] == "freeform_code"
        none = srv.freeform_cad("x = cq.Workplane('XY').box(10, 10, 10)", name)
        assert not none["ok"] and none["error"] == "freeform_result"
    finally:
        for p in (ROOT / "families" / "uploads").glob(f"{name}.*"):
            p.unlink()


def test_compose_candidates_best_of_n(tmp_path, monkeypatch):
    """B1: several candidate graphs are gated, solved and ranked; broken ones come back with the reason."""
    import copy
    monkeypatch.setattr(srv.S, "STUDIES", tmp_path / "studies")
    sid = "bestofn"
    srv.create_study(json.loads(SPEC.read_text()), study_id=sid)
    base = json.loads((ROOT / "families" / "bracket.json").read_text())
    thick = copy.deepcopy(base)
    thick["parameters"]["thickness"]["default"] = 12
    thick["parameters"]["rib_count"]["default"] = 3
    noface = copy.deepcopy(base)
    noface["faces"].pop("load_face")
    broken = copy.deepcopy(base)
    broken["features"][0]["type"] = "no_such_feature"
    try:
        r = srv.compose_candidates(sid, [{"name": "default", "graph": base}, {"name": "thick", "graph": thick},
                                         {"name": "noface", "graph": noface}, {"name": "broken", "graph": broken}])
        assert r["ok"]
        by = {x["name"]: x for x in r["ranked"]}
        assert by["default"]["stage"] == "solved" and by["thick"]["stage"] == "solved"
        assert by["thick"]["metrics"]["safety_factor"] > by["default"]["metrics"]["safety_factor"]
        assert by["noface"]["stage"] == "faces" and "load_face" in by["noface"]["reason"]
        assert by["broken"]["stage"] not in ("solved", "faces")
        assert [x["name"] for x in r["ranked"]][-2:] != ["default", "thick"]  # failures rank last
        assert r["ranked"][0]["stage"] == "solved"
    finally:
        for p in (ROOT / "families" / "uploads").glob("cand_bestofn_*.json"):
            p.unlink()


def test_render_views_returns_an_image():
    """B1: the agent can look at a design; the image travels over the protocol."""
    async def go():
        async with mcp.Client(srv.mcp) as c:
            r = await c.call_tool("render_views", {"family": "bracket", "params": {"rib_count": 2}})
            kinds = [x.type for x in r.content]
            assert "image" in kinds and "text" in kinds
            img = next(x for x in r.content if x.type == "image")
            assert img.mime_type == "image/png" and len(img.data) > 10000
    anyio.run(go)


def test_results_name_the_conflict_when_nothing_is_feasible(tmp_path, monkeypatch):
    """D9: with no feasible design, get_results returns the closest one and the requirements it misses."""
    import time
    monkeypatch.setattr(srv.S, "STUDIES", tmp_path / "studies")
    sid = "conflict"
    srv.create_study(json.loads((SPEC.parent / "bracket_100kg.json").read_text()), study_id=sid)
    srv.accept_spec(sid, "owner")
    job = srv.run_optimization(sid, budget=8, method="random", mesh_size=6)["job_id"]
    for _ in range(240):
        if srv.get_job(job).get("state") != "running":
            break
        time.sleep(0.5)
    r = srv.get_results(sid)
    assert r["ok"] and r["front"] == []
    c = r["closest_infeasible"]
    assert c and any(f["id"] == "R1" and f["quantity"] == "safety_factor" for f in c["fails"])


def _bracket_study():
    spec = json.loads((ROOT / "docs" / "examples" / "bracket_20kg.json").read_text(encoding="utf-8"))
    return srv.create_study(spec)["study_id"], spec


def test_c3_click_a_face_adds_a_load_that_follows_the_design():
    sid, spec = _bracket_study()
    face = engine.call("build", family="bracket")["named_faces"]["load_face"][0]
    picked = srv.pick_face(sid, face["centre"])
    assert picked["ok"] and "load_face" in picked["already_named"] and len(picked["triangles"]) >= 9
    r = srv.select_face(sid, face["centre"], "extra_load", "load", force=[0, 0, -10])
    assert r["ok"] and r["needs_acceptance"] and r["spec_version"] == "spec.v2.json"
    assert not S.accepted(sid)  # a new spec version needs a new acceptance
    new = json.loads(S.latest_spec_path(sid).read_text(encoding="utf-8"))
    assert new["family"] == f"picked/{sid}" and new["version"] == spec.get("version", 1) + 1
    assert any(x["group"] == "extra_load" for x in new["load_cases"][0]["loads"])
    # the signature finds the same face on another design: both names mean one face
    other = engine.call("build", family=new["family"], set=["thickness=12"])["named_faces"]
    assert other["extra_load"][0]["centre"] == other["load_face"][0]["centre"]
    assert any(e["step"] == "select_face" for e in S.timeline(sid))


def test_c3_a_bad_click_changes_nothing():
    sid, _ = _bracket_study()
    assert srv.pick_face(sid, [1000, 1000, 1000])["ok"] is False
    face = engine.call("build", family="bracket")["named_faces"]["load_face"][0]
    assert srv.select_face(sid, face["centre"], "load_face", "support")["ok"] is False  # the name is taken
    assert srv.select_face(sid, face["centre"], "no_force", "load")["ok"] is False
    assert S.latest_spec_path(sid).name == "spec.v1.json"


def test_a1_a_picture_reaches_the_agent(tmp_path):
    png = tmp_path / "sketch.png"
    png.write_bytes(bytes.fromhex("89504e470d0a1a0a0000000d4948445200000001000000010806000000" "1f15c4890000000d49444154789c6360000002000001e5273bb60000000049454e44ae426082"))
    async def go():
        async with mcp.Client(srv.mcp) as c:
            r = await c.call_tool("read_brief_image", {"path": str(png)})
            assert any(getattr(x, "type", "") == "image" for x in r.content)
            bad = await c.call_tool("read_brief_image", {"path": str(tmp_path / "brief.txt")})
            assert "PNG" in "".join(getattr(x, "text", "") for x in bad.content)
    anyio.run(go)
