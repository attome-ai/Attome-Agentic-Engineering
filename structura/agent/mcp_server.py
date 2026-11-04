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
from mcp.server.mcpserver import MCPServer

from structura import engine
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
6. verify the chosen design and report its trust grade; export the bundle.
Never state a number the tools did not return. Never tune parameters yourself inside a loop: that is the optimizer's job."""

mcp = MCPServer(name="attome-ae", instructions=INSTRUCTIONS, version="0.1.0")
_jobs: dict[str, dict] = {}


def _ok(**kw) -> dict:
    return {"ok": True, **kw}


def _err(kind: str, message: str, hint: str = "", **kw) -> dict:
    """Typed error with a pointer to what to try (feeds the repair loop, E3)."""
    return {"ok": False, "error": kind, "message": message, "hint": hint, **kw}


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
    return _ok(objectives=s.user_attrs.get("objectives"), method=s.user_attrs.get("method"),
               front=[{"trial": p.number, "params": p.params, "values": p.values} for p in front],
               recommended=decide.recommend(s), summary=summary(s))


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
