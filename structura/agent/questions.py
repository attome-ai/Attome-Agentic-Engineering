"""Questions that change the design (A5).

Before asking anything, run a quick one-at-a-time sweep over each unknown of the brief on the template's default
design (coarse mesh) and rank the unknowns by how much they move the governing result. Only unknowns above the
threshold become questions (at most three, ordered by impact), each with its stake stated; the rest take defaults.

An unknown is a name, the spec edits for its low and high case, and a plain-language question, e.g.
    {"name": "load direction", "question": "Is the load straight down, or can it also push sideways?",
     "low": {}, "high": {"add_load": [0, -0.5, 0]}}
Supported edits: "load_scale" (multiply the first load), "add_load" (add a force fraction [fx, fy, fz] of the first
load's magnitude), "material" (material id), "factor" (top of the factor range).
"""
from __future__ import annotations

import copy
import json
import math
import tempfile
from pathlib import Path

from structura import engine


def _apply(spec: dict, edit: dict) -> dict:
    s = copy.deepcopy(spec)
    lc = s["load_cases"][0]
    load = lc["loads"][0]
    if "load_scale" in edit:
        load["force"] = [c * edit["load_scale"] for c in load["force"]]
    if "add_load" in edit:
        mag = math.sqrt(sum(c * c for c in load["force"]))
        load["force"] = [c + k * mag for c, k in zip(load["force"], edit["add_load"])]
    if "material" in edit:
        s["material"] = edit["material"]
    if "factor" in edit:
        lc["factor_range"] = [1.0, edit["factor"]]
    return s


def _governing(spec: dict, params: dict, mesh: float) -> dict:
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "spec.json"
        p.write_text(json.dumps(spec))
        r = engine.call("evaluate", spec=str(p), set=[f"{k}={v}" for k, v in params.items()], **{"mesh-size": mesh, "threads": 4})
    return r.get("metrics", {}) if r.get("status") == "ok" else {}


def rank(spec: dict, unknowns: list[dict], params: dict | None = None, quantity: str = "safety_factor", mesh: float = 6.0,
         threshold: float = 0.10, max_questions: int = 3) -> dict:
    """Rank unknowns by relative change in `quantity` between their low and high case."""
    if params is None:
        fam = next(f for f in engine.families() if f["name"] == spec["family"])
        params = {k: (fam["parameters"][k]["default"]) for k in spec["free_params"]}
    rows = []
    for u in unknowns:
        lo = _governing(_apply(spec, u.get("low", {})), params, mesh)
        hi = _governing(_apply(spec, u.get("high", {})), params, mesh)
        a, b = lo.get(quantity), hi.get(quantity)
        effect = abs(b - a) / max(abs(a), abs(b), 1e-12) if a is not None and b is not None else None
        rows.append({"name": u["name"], "question": u["question"], "low": a, "high": b, "effect": effect})
    ranked = sorted([r for r in rows if r["effect"] is not None], key=lambda r: -r["effect"])
    ask = [r for r in ranked if r["effect"] >= threshold][:max_questions]
    for r in ask:
        r["stake"] = (f"this changes the {quantity.replace('_', ' ')} by about {100 * r['effect']:.0f}% "
                      f"({r['low']:.3g} vs {r['high']:.3g}) on the default design")
    defaults = [r["name"] for r in ranked if r not in ask]
    return {"quantity": quantity, "threshold": threshold, "ask": ask, "use_defaults_for": defaults, "all": rows,
            "params_used": params}
