"""E4: which model does each agent step, what it used, and what it cost.

The routes are a config file (data/model_routes.json), not code. No LLM ever runs inside the numeric loop. A route is
ordered: the second failure of a provider call moves the step to the next model, and the switch is logged. Token use is
reported by whoever calls the model (the MCP client owns that call) and added to the study's hash-chained log. Cost is
computed only from prices the owner states in the file; with no stated price the study reports tokens and no cost,
rather than a guessed figure.
"""
from __future__ import annotations

import json
from pathlib import Path

from . import studies as S

ROOT = Path(__file__).resolve().parents[2]
ROUTES = ROOT / "data" / "model_routes.json"
FAILURES_BEFORE_SWITCH = 2


def load_routes(path: Path = ROUTES) -> dict:
    d = json.loads(Path(path).read_text(encoding="utf-8"))
    for step, r in d["routes"].items():
        if not r.get("models"):
            raise ValueError(f"route for step {step!r} names no model")
    return d


def route(step: str, failures: int = 0, routes: dict | None = None) -> dict:
    """The model for this step after `failures` failed provider calls; every model exhausted means stop and report."""
    d = routes or load_routes()
    if step not in d["routes"]:
        raise KeyError(f"no route for step {step!r}; known steps: {sorted(d['routes'])}")
    models = d["routes"][step]["models"]
    i = failures // FAILURES_BEFORE_SWITCH
    if i >= len(models):
        return {"step": step, "model": None, "exhausted": True, "action": "stop and report: every model on this route failed; the demo cache covers a full outage"}
    return {"step": step, "model": models[i], "exhausted": False, "switched": i > 0, "basis": d["routes"][step].get("basis", "")}


def record_usage(study_id: str, step: str, model: str, input_tokens: int, output_tokens: int, seconds: float | None = None) -> dict:
    if min(input_tokens, output_tokens) < 0:
        raise ValueError("token counts cannot be negative")
    S.log(study_id, "llm_usage", "ok", llm_step=step, model=model, input_tokens=input_tokens, output_tokens=output_tokens, seconds=seconds)
    return study_cost(study_id)


def study_cost(study_id: str, routes: dict | None = None) -> dict:
    d = routes or load_routes()
    prices, by_step, total_in, total_out, cost, unpriced = d.get("prices_usd_per_million_tokens", {}), {}, 0, 0, 0.0, set()
    for e in S.timeline(study_id):
        if e.get("step") != "llm_usage":
            continue
        i, o, m = e.get("input_tokens", 0), e.get("output_tokens", 0), e.get("model", "?")
        row = by_step.setdefault(e.get("llm_step", "?"), {"input_tokens": 0, "output_tokens": 0, "calls": 0, "models": []})
        row["input_tokens"] += i
        row["output_tokens"] += o
        row["calls"] += 1
        if m not in row["models"]:
            row["models"].append(m)
        total_in += i
        total_out += o
        p = prices.get(m)
        if p:
            cost += (i * p["input"] + o * p["output"]) / 1e6
        else:
            unpriced.add(m)
    return {"input_tokens": total_in, "output_tokens": total_out, "by_step": by_step,
            "cost_usd": None if unpriced or not by_step else round(cost, 4),
            "cost_note": f"no price stated in data/model_routes.json for: {sorted(unpriced)}" if unpriced else "from the prices stated in data/model_routes.json",
            "token_cap": d.get("token_cap_per_study"), "over_cap": bool(d.get("token_cap_per_study")) and total_in + total_out > d["token_cap_per_study"]}
