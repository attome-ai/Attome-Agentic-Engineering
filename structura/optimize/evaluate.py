"""One candidate evaluation through the C++ engine (D1-T1).

evaluate() returns the engine's evaluation plus what the optimizer needs: objective values in the spec's order and
directions, constraint violations (one per requirement, > 0 means violated), and a status. No LLM is called here.
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from pathlib import Path

from structura import engine

from . import cache


@dataclass
class SpecInfo:
    path: str
    id: str
    hash: str
    free: dict[str, tuple[float, float]]
    integer: set[str]
    objectives: list[tuple[str, str]]          # (quantity, "min" | "max")
    requirements: list[dict]
    raw: dict = field(repr=False, default_factory=dict)

    @property
    def directions(self) -> list[str]:
        return ["minimize" if s == "min" else "maximize" for _, s in self.objectives]


def load_spec(path: str | Path) -> SpecInfo:
    s = engine.call("spec", spec=str(path))
    fam = next(f for f in engine.families() if f["name"] == s["family"])
    integer = {k for k, p in fam["parameters"].items() if p.get("integer")}
    return SpecInfo(path=str(path), id=s["id"], hash=s["hash"],
                    free={k: (float(v[0]), float(v[1])) for k, v in s["free_params"].items()},
                    integer=integer, objectives=[(o["quantity"], o["sense"]) for o in s["objectives"]],
                    requirements=s["requirements"], raw=s)


@dataclass
class Evaluation:
    status: str                       # ok | gate_fail | mesh_fail | solve_fail | build_fail
    params: dict[str, float]
    objectives: list[float] | None    # spec order; None unless status == ok
    violations: list[float] | None    # one per requirement, > 0 violated
    feasible: bool
    raw: dict

    @property
    def reason(self) -> str:
        if self.status == "gate_fail":
            g = self.raw.get("gate", {}).get("gate_after", {})
            return "; ".join(g.get("reasons", [])) or "gate failed"
        return self.raw.get("error", self.status)


def evaluate(spec: SpecInfo, params: dict[str, float], mesh_size: float | None = None, threads: int = 1,
             workdir: str | Path | None = None, bundle: bool = False, use_cache: bool = True) -> Evaluation:
    k = cache.key(spec.hash, params, mesh_size)
    raw = cache.get(k) if use_cache and not bundle else None
    if raw is None:
        opts = {"spec": spec.path, "set": [f"{p}={v}" for p, v in params.items()], "threads": threads}
        if mesh_size:
            opts["mesh-size"] = mesh_size
        if workdir:
            opts["out"] = str(workdir)
        if bundle:
            opts["bundle"] = 1
        raw = engine.call("evaluate", **opts)
        if use_cache:
            cache.put(k, raw)
    else:
        raw = dict(raw, from_cache=True)
    if raw.get("status") != "ok":
        return Evaluation(raw.get("status", "unknown"), params, None, None, False, raw)
    objs = []
    for q, _ in spec.objectives:
        v = raw["objectives"].get(q)
        objs.append(float(v) if v is not None and math.isfinite(v) else float("nan"))
    by_id = {c["id"]: c for c in raw["checks"]}
    viol = [max(0.0, -float(by_id[r["id"]]["margin"])) if r["id"] in by_id else 1.0 for r in spec.requirements]
    return Evaluation("ok", params, objs, viol, bool(raw.get("feasible")), raw)


def dump(e: Evaluation) -> str:
    return json.dumps({"status": e.status, "params": e.params, "objectives": e.objectives, "violations": e.violations,
                       "feasible": e.feasible}, sort_keys=True)
