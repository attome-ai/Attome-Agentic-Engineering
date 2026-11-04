"""Attome AE MCP server (E1): the engine as typed tools, resources and prompts for Claude.

Run: micromamba run -n attome-ae python -m structura.agent.mcp_server      (stdio, for Claude Code / Claude Desktop)

Rules the tools enforce, whatever the model asks:
  - numbers come from the engine; the optimizer searches, the solver decides, no tool lets a model set a result;
  - optimization starts only after the user accepted the current spec version (accept_spec);
  - the trust record is written by the engine only.
"""
from __future__ import annotations

import json
import threading
import traceback
import uuid
from pathlib import Path

import optuna
from mcp.server.mcpserver import Image, MCPServer

from structura import engine
from structura.agent import loads as L
from structura.agent import playbooks as PB
from structura.agent import questions as Q
from structura.agent import routing
from structura.agent import studies as S
from structura.optimize import decide, pareto, portfolio

INSTRUCTIONS = """You are driving the Attome AE engineering engine. Workflow:
1. plan_study: write the study plan (family, physics, objectives, budget) before any other tool.
2. create_study with a DesignSpec; show the user the spec card (requirements with their sources and every default).
3. check_feasibility. If infeasible, say which requirement and the best achievable value; propose the smallest
   relaxation or another family, and let the user decide.
4. Ask the user to accept the spec card; call accept_spec only with their answer.
5. run_optimization (it runs in the background); poll get_job; then get_results for the front and the recommended
   design; use steer when the user states a preference; explain to compare two designs.
6. verify the chosen design and report its trust grade; build_report and give the user the file.
Geometry, in order of preference: an existing family (list_families); a design graph from the grammar (compose_design,
describe_grammar); for the user's own part, upload_part then add_face / add_lightening_pocket; only when the grammar
cannot express the shape, freeform_cad with CadQuery code (fixed geometry, gated like any other part). For a new
design, propose several candidates (compose_candidates), look at the best with render_views, then optimize it.
Never state a number the tools did not return. Never tune parameters yourself inside a loop: that is the optimizer's job."""

ROOT = Path(__file__).resolve().parents[2]
mcp = MCPServer(name="attome-ae", instructions=INSTRUCTIONS, version="0.1.0")
_jobs: dict[str, dict] = {}


def _ok(**kw) -> dict:
    return {"ok": True, **kw}


def _err(kind: str, message: str, hint: str = "", **kw) -> dict:
    """Typed error with the matching repair playbook (E3): concrete next actions, not a reflection prompt."""
    pb = PB.classify(message)
    return {"ok": False, "error": kind, "message": message, "hint": hint, "playbook": pb["kind"], "actions": pb["actions"],
            "repair_budget": PB.BUDGET, **kw}


def _study(study_id: str) -> optuna.Study | None:
    d = S.folder(study_id) / "opt"
    db = d / "study.db"
    if not db.exists():
        return None
    url = f"sqlite:///{db.resolve().as_posix()}"
    try:  # readers never create tables: the optimization thread may be creating them right now
        storage = optuna.storages.RDBStorage(url, skip_table_creation=True, skip_compatibility_check=True)
        names = optuna.study.get_all_study_names(storage)
        return optuna.load_study(study_name=names[0], storage=storage) if names else None
    except Exception:
        return None


# ------------------------------------------------------------------ discovery
@mcp.tool(description="List part families: parameters (unit, bounds, integer), named faces for loads and supports, and what each family is for. "
                      "Example: list_families() -> [{name: 'bracket', parameters: {thickness: {low: 3, high: 12, unit: 'mm'}}, faces: ['mount_holes', 'load_face', ...]}]")
def list_families() -> list[dict]:
    return engine.families()


@mcp.tool(description="Describe the design-graph grammar: every feature type with its parameters, kinds and bounds. Use before compose_design.")
def describe_grammar() -> dict:
    return engine.describe()


# ------------------------------------------------------------------ brief helpers
@mcp.tool(description="Turn a mass in a use case into a design force with the working shown, e.g. derive_load(20, 'variable_action', supports=2) "
                      "-> 98.1 N per bracket, factor 1.5 (EN 1990). Cases: static, variable_action (people, stored goods), permanent_action, "
                      "sudden_application. Use the returned spec_hint for the load and factor_range; never invent a factor.")
def derive_load(mass_kg: float, case: str = "variable_action", supports: int = 1) -> dict:
    try:
        return _ok(**L.derive(mass_kg, case, supports))
    except ValueError as e:
        return _err("load", str(e))


@mcp.tool(description="Rank the brief's unknowns by how much they change the design before asking the user anything. Give the draft spec and a "
                      "list of unknowns, each {name, question, low: {...}, high: {...}} with edits load_scale, add_load [fx,fy,fz] fraction, "
                      "material, or factor. Returns at most three questions worth asking, each with its stake; default the rest and say so.")
def rank_questions(spec: dict, unknowns: list[dict], quantity: str = "safety_factor") -> dict:
    try:
        return _ok(**Q.rank(spec, unknowns, quantity=quantity))
    except engine.EngineError as e:
        return _err(e.kind, str(e))


# ------------------------------------------------------------------ plan, spec, approval
@mcp.tool(description="Record the study plan before running anything: family, physics, objectives and evaluation budget, in one sentence each. "
                      "Example: plan_study(study_id, family='bracket', physics='linear static', objectives='min mass, max safety factor', budget=120)")
def plan_study(study_id: str, family: str, physics: str, objectives: str, budget: int, notes: str = "") -> dict:
    S.log(study_id, "plan", "ok", family=family, physics=physics, objectives=objectives, budget=budget, notes=notes)
    return _ok(study_id=study_id, plan={"family": family, "physics": physics, "objectives": objectives, "budget": budget})


@mcp.tool(description="Create a study from a DesignSpec (JSON object). The engine validates it against the family, the materials table and the "
                      "known quantities (mass_g, max_von_mises_MPa, max_disp_mm, safety_factor, first_frequency_hz, buckling_factor, "
                      "peak_temperature). Every requirement needs an id (R1, R2, ...) and a source (brief sentence or 'default: ...'). "
                      "See docs/examples/bracket_20kg.json for a complete spec. Returns the spec card to show the user.")
def create_study(spec: dict, study_id: str | None = None) -> dict:
    sid = study_id or S.new_id(spec.get("id", "study"))
    path = S.save_spec(sid, spec)
    try:
        card = engine.call("spec", spec=str(path))
    except engine.EngineError as e:
        path.unlink(missing_ok=True)
        S.log(sid, "spec", "rejected", message=str(e))
        return _err("spec", str(e), "fix the field named in the message; bounds must sit inside the family's bounds")
    S.log(sid, "spec", "ok", version=path.name, spec_hash=card["hash"])
    return _ok(study_id=sid, spec_version=path.name, spec_card=card, needs_acceptance=True)


@mcp.tool(description="Record that the user accepted the current spec card. Call only with the user's explicit answer; optimization refuses to "
                      "start without it. A new spec version needs a new acceptance.")
def accept_spec(study_id: str, accepted_by: str, note: str = "") -> dict:
    rec = S.accept(study_id, accepted_by, note)
    S.log(study_id, "accept", "ok", **rec)
    return _ok(acceptance=rec)


@mcp.tool(description="Check whether any design within the spec's bounds can meet the requirements (evaluates the most- and least-material corners). "
                      "If infeasible, report the requirement and the best achievable value, and propose the smallest relaxation.")
def check_feasibility(study_id: str, mesh_size: float = 6.0) -> dict:
    path = S.latest_spec_path(study_id)
    try:
        r = engine.call("feasibility", spec=str(path), **{"mesh-size": mesh_size, "threads": 4})
    except engine.EngineError as e:
        S.log(study_id, "feasibility", "error", message=str(e))
        return _err(e.kind, str(e))
    S.log(study_id, "feasibility", "feasible" if r["feasible"] else "infeasible", problems=r["problems"])
    return _ok(**r)


@mcp.tool(description="When check_feasibility says infeasible: draft the smallest relaxation that makes the spec feasible, as a new spec version "
                      "with a diff. It is NOT accepted: show the diff, let the user choose (relax, change material or family, or stop), then "
                      "call accept_spec only with their answer.")
def propose_relaxation(study_id: str, mesh_size: float = 6.0, margin: float = 0.05) -> dict:
    path = S.latest_spec_path(study_id)
    spec = json.loads(path.read_text(encoding="utf-8"))
    f = engine.call("feasibility", spec=str(path), **{"mesh-size": mesh_size, "threads": 4})
    if f["feasible"]:
        return _ok(feasible=True, message="the spec is already feasible; nothing to relax")
    changes, load_options = [], []
    for prob in f["problems"]:
        if prob.get("quantity") == "safety_factor" and prob.get("op") == ">=" and "best_achievable" in prob:
            # linear statics: the safety factor scales with 1/load, so the strongest design meets the limit at this load fraction
            scale = float(prob["best_achievable"]) / float(prob["limit"])
            load_options.append({"id": prob["id"], "load_fraction": round(scale, 4),
                                 "load_cases": {lc["name"]: [round(x * scale, 2) for ld in lc.get("loads", []) for x in ld.get("force", [])]
                                                for lc in spec.get("load_cases", [])},
                                 "why": f"the strongest design in the bounds reaches safety factor {float(prob['best_achievable']):.3g}; it meets "
                                        f"{prob['limit']} at {100 * scale:.0f}% of the brief's load (linear analysis: safety factor scales with 1/load)"})
        if "best_achievable" not in prob:
            continue
        best = float(prob["best_achievable"])
        for r in spec["requirements"]:
            if r["id"] == prob["id"]:
                new = best * (1 - margin) if r["op"] == ">=" else best * (1 + margin)
                changes.append({"id": r["id"], "quantity": r["quantity"], "old": r["value"], "new": round(new, 4),
                                "why": f"best achievable in the bounds is {best:.4g}; {100 * margin:.0f}% inside it"})
                r["value"] = round(new, 4)
                r["source"] = (r.get("source", "") + f"; relaxed from {changes[-1]['old']} (proposal, needs user acceptance)").strip("; ")
    if not changes:
        return _err("relaxation", "the infeasibility is not a numeric limit that can be relaxed", problems=f["problems"])
    spec["version"] = int(spec.get("version", 1)) + 1
    newp = S.save_spec(study_id, spec)
    S.log(study_id, "propose_relaxation", "drafted", changes=changes, spec_version=newp.name)
    return _ok(feasible=False, proposed_spec_version=newp.name, changes=changes, needs_acceptance=True, load_options=load_options,
               alternatives=["reduce the load (see load_options)", "change the material (list it in data/materials.json)",
                             "widen the free-parameter bounds", "choose another part family"])


# ------------------------------------------------------------------ uploaded parts (A3)
@mcp.tool(description="Upload a STEP part to analyse or improve (A3). Stores it, detects its interfaces (holes and hole patterns, large planar "
                      "faces, envelope) and writes a part family 'uploads/<name>' that imports it, with those faces named. Show the user the "
                      "holes and faces, ask which carry the load and the supports and which interfaces must stay unchanged, add any face "
                      "they name with add_face, then create a study on that family. With no free parameters the study analyses the part "
                      "as-is (evaluate_design with params {}) and becomes the baseline of a redesign.")
def upload_part(step_path: str, name: str) -> dict:
    import re
    import shutil
    if not re.fullmatch(r"[a-z][a-z0-9_]{0,40}", name):
        return _err("upload", "name must be lowercase letters, digits and _ (start with a letter)")
    src = Path(step_path)
    if not src.exists():
        return _err("upload", f"no file at {step_path}")
    up = ROOT / "families" / "uploads"
    up.mkdir(parents=True, exist_ok=True)
    dst = up / f"{name}.step"
    shutil.copyfile(src, dst)
    try:
        r = engine.call("inspect", step=str(dst), **{"family-out": str(up / f"{name}.json"), "name": name})
    except engine.EngineError as e:
        return _err(e.kind, str(e))
    fam = r["family"]
    fam["features"][0]["params"]["file"] = f"families/uploads/{name}.step"  # repository-relative, portable
    (up / f"{name}.json").write_text(json.dumps(fam, indent=2), encoding="utf-8")
    return _ok(family=f"uploads/{name}", valid=r["valid"], solids=r["solids"], volume_mm3=r["volume_mm3"], envelope=r["envelope"],
               holes=[{k: h[k] for k in ("diameter_mm", "axis", "centre", "depth_mm")} for h in r["holes"]], hole_patterns=r["hole_patterns"],
               named_faces={k: v.get("doc") for k, v in fam["faces"].items()},
               faces=[{k: f[k] for k in ("id", "type", "area_mm2", "centre") if k in f} | ({"selector": f["selector"]} if "selector" in f else {})
                      for f in r["faces"]],
               notes=r["notes"])


@mcp.tool(description="Name one more face of an uploaded part family (for a load or support the user chose), by its face id from upload_part.")
def add_face(family: str, face_id: int, name: str, doc: str = "") -> dict:
    p = ROOT / "families" / f"{family}.json"
    if not family.startswith("uploads/") or not p.exists():
        return _err("upload", "only uploaded families can be edited")
    fam = json.loads(p.read_text(encoding="utf-8"))
    step = ROOT / fam["features"][0]["params"]["file"]
    r = engine.call("inspect", step=str(step))
    f = next((x for x in r["faces"] if x["id"] == face_id), None)
    if f is None or "selector" not in f:
        return _err("upload", f"face {face_id} does not exist or is not axis-aligned (no selector)")
    fam["faces"][name] = {**f["selector"], "doc": doc or f"face {face_id}"}
    p.write_text(json.dumps(fam, indent=2), encoding="utf-8")
    return _ok(family=family, face=name, selector=f["selector"])


@mcp.tool(description="Redesign an uploaded part (A3): add a lightening pocket cut down from one of its top (+z) planar faces, with "
                      "length, width and depth as free parameters bounded by that face, leaving `margin_mm` of wall around it. Frozen "
                      "interfaces (material around every detected hole) are enforced by the gate. Then create a study on the family "
                      "with these parameters free; mass vs safety factor trades how much material the pocket removes.")
def add_lightening_pocket(family: str, face_id: int, margin_mm: float = 3.0, floor_mm: float = 2.0) -> dict:
    p = ROOT / "families" / f"{family}.json"
    if not family.startswith("uploads/") or not p.exists():
        return _err("upload", "only uploaded families can be edited")
    fam = json.loads(p.read_text(encoding="utf-8"))
    r = engine.call("inspect", step=str(ROOT / fam["features"][0]["params"]["file"]))
    f = next((x for x in r["faces"] if x["id"] == face_id), None)
    if f is None or f.get("selector", {}).get("normal") != "z":
        return _err("upload", f"face {face_id} is not a top (+z) planar face; a pocket is cut downward from one")
    x0, y0, _, x1, y1, z_top = f["bbox"]
    # local thickness: the nearest downward-facing face below the pocket area (else the bottom of the envelope)
    px0, px1, py0, py1 = x0 + margin_mm, x1 - margin_mm, y0 + margin_mm, y1 - margin_mm
    below = [g["bbox"][5] for g in r["faces"] if g.get("selector", {}).get("normal") == "-z" and g["bbox"][5] < z_top - 1e-6
             and g["bbox"][0] < px1 and g["bbox"][3] > px0 and g["bbox"][1] < py1 and g["bbox"][4] > py0]
    z_floor = max(below) if below else r["envelope"][2]
    lx, ly, dz = px1 - px0, py1 - py0, z_top - z_floor - floor_mm
    if min(lx, ly, dz) < 2:
        return _err("upload", "the face is too small for a pocket with this margin and floor")
    k = f"p{face_id}"
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    fam["parameters"].update({
        f"{k}_length": {"unit": "mm", "low": 2.0, "high": round(lx, 3), "default": round(lx / 2, 3), "doc": f"pocket length (x) in face {face_id}"},
        f"{k}_width": {"unit": "mm", "low": 2.0, "high": round(ly, 3), "default": round(ly / 2, 3), "doc": f"pocket width (y) in face {face_id}"},
        f"{k}_depth": {"unit": "mm", "low": 0.5, "high": round(dz, 3), "default": round(dz / 2, 3), "doc": f"pocket depth below face {face_id}"}})
    fam["features"] = [x for x in fam["features"] if x["id"] != k] + [{
        "id": k, "type": "pocket", "doc": f"lightening pocket centred in face {face_id}",
        "params": {"x": f"{cx} - {k}_length / 2", "y": f"{cy} - {k}_width / 2", "z_top": z_top,
                   "length": f"{k}_length", "width": f"{k}_width", "depth": f"{k}_depth", "corner_radius": 1.0}}]
    p.write_text(json.dumps(fam, indent=2), encoding="utf-8")
    return _ok(family=family, parameters={n: fam["parameters"][n] for n in fam["parameters"] if n.startswith(k)},
               frozen_interfaces=len(fam.get("preserve", [])))


@mcp.tool(description="LAST RESORT for shapes the design grammar cannot express (B1): write CadQuery code that assigns the final solid to "
                      "`result` (cq is imported; units mm). It runs in its own process with a time limit, is exported to STEP, inspected "
                      "like an uploaded part (holes frozen, faces named) and checked by the geometry gate. Returns the gate verdict, the "
                      "detected faces and holes, or a structured error to repair. The shape is fixed: optimize it afterwards with "
                      "the uploaded-part operations (add_lightening_pocket) or by revising the code. Prefer compose_design whenever "
                      "the grammar can express the part.")
def freeform_cad(code: str, name: str, timeout_s: float = 120.0) -> dict:
    import re
    import subprocess
    import sys
    if not re.fullmatch(r"[a-z][a-z0-9_]{0,40}", name):
        return _err("freeform", "name must be lowercase letters, digits and _ (start with a letter)")
    work = ROOT / "runs" / "freeform" / name
    work.mkdir(parents=True, exist_ok=True)
    (work / "model.py").write_text(code, encoding="utf-8")
    step = work / "part.step"
    step.unlink(missing_ok=True)
    runner = Path(__file__).with_name("freeform_runner.py")
    try:
        p = subprocess.run([sys.executable, "-I", str(runner), str(work / "model.py"), str(step)], cwd=str(work),
                           capture_output=True, text=True, timeout=timeout_s)
    except subprocess.TimeoutExpired:
        return _err("freeform_timeout", f"the code ran longer than {timeout_s:.0f} s", "simplify the geometry or the loops")
    lines = [ln for ln in p.stdout.splitlines() if ln.startswith("{")]
    info = json.loads(lines[-1]) if lines else {"error": "runner", "message": (p.stderr or p.stdout)[-1500:]}
    if p.returncode != 0 or not step.exists():
        return _err("freeform_" + info.get("error", "runner"), info.get("message", "failed"),
                    "fix the code and call freeform_cad again; the message is the CadQuery/Python error")
    up = upload_part(str(step), name)
    if not up.get("ok"):
        return up
    if up.get("solids") != 1:
        return _err("freeform_geometry", f"the result has {up.get('solids')} solids; the engine needs exactly one",
                    "fuse the bodies (union) or remove loose pieces", family=up["family"])
    try:
        g = engine.call("gate", family=up["family"])
    except engine.EngineError as e:
        return _err(e.kind, str(e), family=up["family"])
    S_log = {"family": up["family"], "gate_ok": g.get("ok"), "reasons": g.get("reasons")}
    return _ok(family=up["family"], code_file=str(work / "model.py"), gate=g, volume_mm3=up["volume_mm3"], envelope=up["envelope"],
               holes=up["holes"], hole_patterns=up["hole_patterns"], named_faces=up["named_faces"], faces=up["faces"],
               next=("create a study on this family (free_params {} analyses it as-is), naming load and support faces with add_face"
                     if g.get("ok") else "the gate rejected the shape: fix the reasons in the code and call freeform_cad again"),
               summary=S_log)


# ------------------------------------------------------------------ geometry
@mcp.tool(description="Build a design from a design graph (JSON) or a family with parameters, and run the geometry gate. Returns volume, named faces "
                      "and gate reasons. Use describe_grammar for feature types. Errors are typed: graph (bad value or reference), compile "
                      "(geometry failed; names the feature), gate (shape rejected; reasons say why).")
def compose_design(family: str | None = None, params: dict | None = None, graph: dict | None = None, study_id: str | None = None) -> dict:
    try:
        if graph is not None:
            d = S.ROOT / "runs" / "graphs"
            d.mkdir(parents=True, exist_ok=True)
            gp = d / f"{uuid.uuid4().hex[:10]}.json"
            gp.write_text(json.dumps(graph))
            sets = [f"{k}={v}" for k, v in (params or {}).items()]
            b = engine.call("build", graph=str(gp), set=sets)
            g = engine.call("gate", graph=str(gp), set=sets)
        else:
            b = engine.build(family, params)
            g = engine.gate(family, params)
    except engine.EngineError as e:
        if study_id:
            S.log(study_id, "compose", "error", kind=e.kind, message=str(e))
        return _err(e.kind, str(e), {"graph": "check the parameter named in the message against describe_grammar",
                                     "compile": "change or remove the feature named in the message",
                                     }.get(e.kind, ""))
    if study_id:
        S.log(study_id, "compose", "ok" if g["ok"] else "gate_fail", hash=b["hash"], gate=g["reasons"])
    return _ok(build=b, gate=g)


@mcp.tool(description="Best of N (B1): propose several candidate design graphs for the study's brief at once (3-5 is typical). Each is "
                      "built, gated and solved once at its default parameters against the study's spec; they come back ranked (gate "
                      "pass, then meets every requirement, then the first objective) with the reason each loser lost. Every graph needs "
                      "the face names the spec's load cases use and parameters with bounds; the winner's family name can go into a new "
                      "spec version (family + its parameters as free_params) for the optimizer.")
def compose_candidates(study_id: str, candidates: list[dict], mesh_size: float = 6.0) -> dict:
    import re
    spec = json.loads(S.latest_spec_path(study_id).read_text(encoding="utf-8"))
    groups = {g for lc in spec.get("load_cases", []) for g in [x["group"] for x in lc.get("supports", []) + lc.get("loads", [])]}
    groups |= {x["group"] for tc in spec.get("thermal_cases", []) for k in ("films", "fluxes", "temperatures") for x in tc.get(k, [])}
    sense = (spec.get("objectives") or [{"quantity": "mass_g", "sense": "min"}])[0]
    up = ROOT / "families" / "uploads"
    up.mkdir(parents=True, exist_ok=True)
    tag = re.sub(r"[^a-z0-9_]", "_", study_id.lower())[:24]
    rows = []
    for i, c in enumerate(candidates):
        name = re.sub(r"[^a-z0-9_]", "_", str(c.get("name") or f"c{i}").lower())[:24] or f"c{i}"
        fam = f"uploads/cand_{tag}_{name}"
        row = {"name": name, "family": fam}
        graph = dict(c.get("graph") or {}, name=f"cand_{tag}_{name}")
        missing = sorted(groups - set((graph.get("faces") or {}).keys()))
        if missing:
            rows.append(row | {"stage": "faces", "reason": f"the graph does not name the faces the load cases use: {', '.join(missing)}"})
            continue
        (up / f"cand_{tag}_{name}.json").write_text(json.dumps(graph, indent=2), encoding="utf-8")
        try:
            g = engine.call("gate", family=fam)
        except engine.EngineError as e:
            (up / f"cand_{tag}_{name}.json").unlink(missing_ok=True)  # never leave an unloadable family behind
            rows.append(row | {"stage": e.kind, "reason": str(e)[:400]})
            continue
        if not g.get("ok"):
            rows.append(row | {"stage": "gate", "reason": "; ".join(g.get("reasons", []))})
            continue
        sp = dict(spec, id=f"{spec['id']}-cand-{name}", family=fam, free_params={})
        spath = S.folder(study_id) / "candidates" / f"{name}.json"
        spath.parent.mkdir(parents=True, exist_ok=True)
        spath.write_text(json.dumps(sp, indent=2), encoding="utf-8")
        try:
            r = engine.call("evaluate", spec=str(spath), **{"mesh-size": mesh_size, "threads": 4})
        except engine.EngineError as e:
            rows.append(row | {"stage": e.kind, "reason": str(e)[:400]})
            continue
        if r.get("status") != "ok":
            rows.append(row | {"stage": r.get("status"), "reason": r.get("error") or json.dumps(r.get("gate"))[:300]})
            continue
        failed = [f"{c2['id']} {c2['quantity']} {c2['value']:.4g} (limit {c2['op']} {c2['limit']})" for c2 in r["checks"] if not c2["passed"]]
        rows.append(row | {"stage": "solved", "feasible": r["feasible"], "metrics": r["metrics"],
                           "reason": "meets every requirement" if r["feasible"] else "fails " + "; ".join(failed)})
    key_q, flip = sense["quantity"], (1 if sense["sense"] == "min" else -1)
    order = {"solved": 0, "gate": 1}
    rows.sort(key=lambda r: (order.get(r["stage"], 2), not r.get("feasible", False),
                             flip * (r.get("metrics") or {}).get(key_q, float("inf")) if r.get("metrics") else float("inf")))
    for rank, r in enumerate(rows, 1):
        r["rank"] = rank
    S.log(study_id, "compose_candidates", "ok", n=len(rows), winner=rows[0]["family"] if rows else None,
          ranking=[(r["name"], r["stage"], r.get("feasible")) for r in rows])
    return _ok(ranked=rows, winner=rows[0] if rows and rows[0]["stage"] == "solved" else None,
               note="ranked at default parameters on a coarse mesh: a screening, not a verdict; optimize the winner, then verify")


@mcp.tool(description="Look at a design before solving it (B1 visual review): renders front, back, left, right, top and isometric "
                      "views with the overall size, from a family (+ params) or a design graph. Returns the image and its file path. "
                      "Check it against the brief: right overall shape, holes where expected, no stray or floating bodies, plausible "
                      "proportions. Reject and revise candidates that do not match before spending solver time on them.")
def render_views(family: str | None = None, params: dict | None = None, graph: dict | None = None, title: str = ""):
    from structura.agent.render import render
    out = ROOT / "runs" / "renders" / uuid.uuid4().hex[:10]
    sets = [f"{k}={v}" for k, v in (params or {}).items()]
    try:
        if graph is not None:
            out.mkdir(parents=True, exist_ok=True)
            gp = out / "graph_in.json"
            gp.write_text(json.dumps(graph))
            engine.call("build", graph=str(gp), set=sets, out=str(out))
        elif family:
            engine.call("build", family=family, set=sets, out=str(out))
        else:
            return _err("render", "give a family (with params) or a design graph")
    except engine.EngineError as e:
        return _err(e.kind, str(e))
    png = render(out / "part.stl", out / "views.png", title or (family or "design graph"))
    return [json.dumps({"ok": True, "file": str(png)}), Image(path=str(png))]


# ------------------------------------------------------------------ evaluation and optimization
@mcp.tool(description="Evaluate one candidate against the study's spec: build, gate, mesh, solve every load case at its worst factor, check every "
                      "requirement. Use for a baseline or a user-proposed design; do not use it to search (run_optimization does that).")
def evaluate_design(study_id: str, params: dict, mesh_size: float = 4.0) -> dict:
    path = S.latest_spec_path(study_id)
    try:
        r = engine.call("evaluate", spec=str(path), set=[f"{k}={v}" for k, v in params.items()], **{"mesh-size": mesh_size, "threads": 4})
    except engine.EngineError as e:
        S.log(study_id, "evaluate", "error", message=str(e))
        return _err(e.kind, str(e))
    S.log(study_id, "evaluate", r.get("status"), params=params, metrics=r.get("metrics"), feasible=r.get("feasible"))
    return _ok(**{k: r.get(k) for k in ("status", "metrics", "checks", "feasible", "governing_case", "hash", "gate")})


@mcp.tool(description="Manufacturing check and cost for one design of the study's family: FDM printing (build volume, minimum wall, overhang, "
                      "support volume per print direction, time, cost) and 3-axis CNC (undercuts, internal corners against the tool, stock, "
                      "time, cost), with advice on which process fits and which rule is closest to its limit. Every rate and price comes "
                      "from data/processes.json and is an assumption the user should replace with their own; say so when quoting a cost.")
def manufacturing_advice(study_id: str, params: dict, material: str | None = None) -> dict:
    spec = json.loads(S.latest_spec_path(study_id).read_text(encoding="utf-8"))
    mat = material or spec.get("material", "PETG")
    try:
        r = engine.call("manufacture", family=spec["family"], set=[f"{k}={v}" for k, v in params.items()], material=mat,
                        **{"print-dir": json.dumps(spec.get("print_direction", [0, 0, 1]))})
    except engine.EngineError as e:
        return _err(e.kind, str(e))
    S.log(study_id, "manufacturing_advice", "done", params=params, material=mat, recommended=r.get("recommended_process"))
    return _ok(material=mat, recommended_process=r.get("recommended_process"), advice=r.get("advice"),
               **{p: {k: v for k, v in r[p].items() if k != "inputs"} for p in ("fdm", "cnc") if p in r})


def _run_job(job_id: str, study_id: str, budget: int, method: str | None, mesh_size: float):
    job = _jobs[job_id]

    def progress(feasible: bool):
        job["trials"] = job.get("trials", 0) + 1
        job["feasible"] = job.get("feasible", 0) + int(feasible)

    try:
        s = portfolio.run(S.latest_spec_path(study_id), budget=budget, method=method, study_dir=S.folder(study_id) / "opt",
                          mesh_size=mesh_size, n_jobs=8 if method not in (None, "botorch_qnehvi") else 4, on_trial=progress)
        job.update(state="done", method=s.user_attrs.get("method"), reason=s.user_attrs.get("method_reason"))
        S.log(study_id, "optimize", "done", job=job_id, method=job["method"], trials=len(s.trials))
    except Exception as e:  # reported, never swallowed
        job.update(state="error", message=str(e), trace=traceback.format_exc()[-1500:])
        S.log(study_id, "optimize", "error", job=job_id, message=str(e))


@mcp.tool(description="Start the optimization in the background (method chosen from the budget unless given: botorch_qnehvi, nsga2, nsga3, random). "
                      "Refuses until the user accepted the spec. Returns a job id; poll get_job. Typical: budget=120, mesh_size=6 for the search.")
def run_optimization(study_id: str, budget: int = 120, method: str | None = None, mesh_size: float = 6.0) -> dict:
    if not S.accepted(study_id):
        return _err("approval", "the current spec version has not been accepted by the user",
                    "show the spec card and call accept_spec with the user's answer")
    job_id = uuid.uuid4().hex[:8]
    _jobs[job_id] = {"state": "running", "study_id": study_id, "budget": budget}
    S.log(study_id, "optimize", "started", job=job_id, budget=budget, method=method or "auto", mesh_size=mesh_size)
    threading.Thread(target=_run_job, args=(job_id, study_id, budget, method, mesh_size), daemon=True).start()
    return _ok(job_id=job_id)


@mcp.tool(description="Progress of a background optimization job: state (running, done, error), trials so far and feasible count.")
def get_job(job_id: str) -> dict:
    job = _jobs.get(job_id)
    if not job:
        return _err("job", f"unknown job {job_id}")
    # progress comes from the job itself: opening the study database while the job creates it races on the schema
    return _ok(**{"trials": 0, "feasible": 0, **{k: v for k, v in job.items() if k != "trace"}})


@mcp.tool(description="Results of the study: feasible Pareto front (each point's parameters and objectives), the recommended knee design, and counts by status.")
def get_results(study_id: str) -> dict:
    s = _study(study_id)
    if s is None:
        return _err("results", "no optimization has run for this study yet")
    front = pareto.front(s)
    from structura.optimize.study import summary
    closest = None
    if not front:  # nothing meets every requirement: name the conflict instead of returning nothing
        ok = [t for t in s.trials if t.user_attrs.get("status") == "ok" and t.user_attrs.get("violations")]
        if ok:
            t = min(ok, key=lambda t: sum(t.user_attrs["violations"]))
            failed = [c for c in t.user_attrs.get("checks", []) if not c.get("passed")]
            closest = {"trial": t.number, "params": t.params, "metrics": t.user_attrs.get("metrics"),
                       "fails": [{k: c[k] for k in ("id", "quantity", "value", "op", "limit")} for c in failed],
                       "hint": "the requirements conflict for this family: tell the user which ones and let them choose what to relax "
                               "(a default the user never stated is the first candidate)"}
    return _ok(objectives=s.user_attrs.get("objectives"), method=s.user_attrs.get("method"),
               front=[{"trial": p.number, "params": p.params, "values": p.values} for p in front],
               recommended=decide.recommend(s), closest_infeasible=closest, summary=summary(s))


@mcp.tool(description="Steer the search toward a preference. Give `reference` (target value per objective, in the spec's objective order) or `preference` "
                      "text such as 'lighter, even if it bends more'. Hard requirements still apply. Runs n_trials focused evaluations.")
def steer(study_id: str, preference: str | None = None, reference: list[float] | None = None, n_trials: int = 20) -> dict:
    s = _study(study_id)
    if s is None:
        return _err("results", "run an optimization first")
    if reference is None:
        f = pareto.front(s)
        if not f:
            return _err("results", "no feasible designs yet to steer from")
        reference = decide.parse_preference(preference or "", [p.values for p in f], [tuple(o) for o in s.user_attrs["objectives"]])
    r = decide.steer(str(S.latest_spec_path(study_id)), s, reference, n_trials=n_trials, mesh_size=6)
    S.log(study_id, "steer", "ok", preference=preference, reference=reference, evaluations=r["evaluations"])
    return _ok(**r)


@mcp.tool(description="Explain two designs (trial numbers): which parameters differ and how each objective moves. Turn the data into one paragraph for the user.")
def explain(study_id: str, trial_a: int, trial_b: int) -> dict:
    s = _study(study_id)
    if s is None:
        return _err("results", "run an optimization first")
    return _ok(**decide.explain(s, trial_a, trial_b))


@mcp.tool(description="Verify a design: three mesh levels, numerical error bars (GCI), equilibrium and energy checks, assumptions and a grade (A/B/C) "
                      "written by the engine to trust.json. Report the grade and any 'not proven' requirement to the user.")
def verify(study_id: str, params: dict, fine_mesh: float = 3.0) -> dict:
    out = S.folder(study_id) / "verify" / uuid.uuid4().hex[:8]
    try:
        t = engine.call("trust", spec=str(S.latest_spec_path(study_id)), set=[f"{k}={v}" for k, v in params.items()],
                        out=str(out), **{"fine-size": fine_mesh}, timeout=1800)
    except engine.EngineError as e:
        S.log(study_id, "verify", "error", message=str(e))
        return _err(e.kind, str(e))
    S.log(study_id, "verify", t.get("status"), grade=t.get("grade"), folder=str(out))
    return _ok(folder=str(out), **{k: t.get(k) for k in ("grade", "status", "reasons", "requirements", "independent_checks", "assumptions", "reproduce")})


@mcp.tool(description="Write the interactive study report (one offline HTML file: 3D fields, Pareto explorer, comparison with the baseline, trust "
                      "panel, history, timeline). Give the baseline design's parameters when there is one. Returns the file path to give the user.")
def build_report(study_id: str, baseline: dict | None = None) -> dict:
    from structura.report.build import build
    d = S.folder(study_id)
    if not (d / "opt" / "study.db").exists():
        return _err("results", "run an optimization first")
    verifies = sorted((d / "verify").glob("*/trust.json"), key=lambda p: p.stat().st_mtime) if (d / "verify").exists() else []
    trust_dir = str(verifies[-1].parent) if verifies else None
    out = build(str(S.latest_spec_path(study_id)), str(d / "opt"), str(d / "report.html"),
                {k: float(v) for k, v in baseline.items()} if baseline else None, trust_dir, study_id)
    S.log(study_id, "report", "ok", file=str(out))
    return _ok(file=str(out), size_mb=round(out.stat().st_size / 1e6, 1))


@mcp.tool(description="Check whether an uploaded STEP part matches a part family: the family's closest design and its overlap with the part (intersection over union; 0.95 or more "
                      "counts as a fit). A fit means the family's parameters can be optimized instead of cutting pockets into the solid. Example: fit_part_to_family('part.step', 'bracket').")
def fit_part_to_family(step_path: str, family: str) -> dict:
    try:
        return _ok(**engine.call("fit", step=step_path, family=family, timeout=900))
    except engine.EngineError as e:
        return _err(e.kind, str(e))


@mcp.tool(description="Choose the print direction for a design: each of the six axis directions with its tension across the layers (printed parts are weakest there) and "
                      "support volume, the recommended one and why. Set the spec's print_direction to it before a final verify. Example: choose_print_direction('s1', {'thickness': 8, ...}).")
def choose_print_direction(study_id: str, params: dict) -> dict:
    try:
        r = engine.call("orientations", spec=str(S.latest_spec_path(study_id)), set=[f"{k}={v}" for k, v in params.items()], timeout=600)
    except engine.EngineError as e:
        return _err(e.kind, str(e))
    S.log(study_id, "print_direction", "ok", recommended=r["recommended"]["direction"])
    return _ok(**r)


@mcp.tool(description="Which model to use for an agent step (plan, spec_extraction, compose, repair, log_parsing, report_drafting, explanation), given how many "
                      "provider calls for this step have failed. Two failures move the step to the next model on its route; returns exhausted when none is left.")
def model_route(step: str, failures: int = 0) -> dict:
    try:
        return _ok(**routing.route(step, failures))
    except KeyError as e:
        return _err("route", str(e))


@mcp.tool(description="Record the tokens one model call used, so the study reports them per step (and cost, only when prices are stated in data/model_routes.json). "
                      "Example: record_usage('s1', 'compose', 'claude-opus-5-5', 3200, 900). Returns the study's totals and whether it is over its token cap.")
def record_usage(study_id: str, step: str, model: str, input_tokens: int, output_tokens: int, seconds: float | None = None) -> dict:
    try:
        return _ok(**routing.record_usage(study_id, step, model, input_tokens, output_tokens, seconds))
    except ValueError as e:
        return _err("usage", str(e))


# ------------------------------------------------------------------ resources and prompts
@mcp.resource("study://{study_id}/spec", description="The study's current spec (JSON).", mime_type="application/json")
def spec_resource(study_id: str) -> str:
    return S.latest_spec_path(study_id).read_text(encoding="utf-8")


@mcp.resource("study://{study_id}/timeline", description="Every step taken in the study, in order (JSON lines).", mime_type="application/json")
def timeline_resource(study_id: str) -> str:
    return json.dumps(S.timeline(study_id))


@mcp.prompt(description="Start a design study from a plain-language brief.")
def design_brief(brief: str) -> str:
    return (f"Design brief: {brief}\n\nFollow the workflow in the server instructions: plan_study, then create_study with a spec whose "
            "requirements each cite the sentence of the brief (or the default) they come from, then check_feasibility, then ask me to "
            "accept the spec card before optimizing.")


def main():
    mcp.run("stdio")


if __name__ == "__main__":
    main()
