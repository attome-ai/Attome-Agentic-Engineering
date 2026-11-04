"""Content-hash cache for evaluations (D3-T1).

Key = sha256(spec hash, parameters rounded to 6 significant digits, mesh size, engine build). The engine build is the
engine executable's size and modification time, so rebuilding the engine (a possible change in physics) never serves
stale results. Timeouts and engine crashes are not cached; gate, mesh and solve failures are, because they are
deterministic outcomes of the design.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

from structura import engine

ROOT = Path(__file__).resolve().parents[2]
CACHE_DIR = Path(os.environ.get("STRUCTURA_CACHE", ROOT / "runs" / "cache"))
CACHEABLE = {"ok", "gate_fail", "mesh_fail", "solve_fail", "build_fail"}


def _round(v: float) -> float:
    return float(f"{float(v):.6g}")


def engine_build() -> str:
    try:
        st = os.stat(engine.engine_exe())
        return f"{st.st_size}-{int(st.st_mtime)}"
    except FileNotFoundError:
        return "unknown"


def key(spec_hash: str, params: dict[str, float], mesh_size: float | None) -> str:
    payload = {"spec": spec_hash, "params": {k: _round(v) for k, v in sorted(params.items())},
               "mesh": _round(mesh_size) if mesh_size else None, "engine": engine_build()}
    return hashlib.sha256(json.dumps(payload, sort_keys=True).encode()).hexdigest()


def get(k: str) -> dict | None:
    p = CACHE_DIR / k[:2] / f"{k}.json"
    if not p.exists():
        return None
    try:
        return json.loads(p.read_text())
    except json.JSONDecodeError:
        return None


def put(k: str, raw: dict) -> None:
    if raw.get("status") not in CACHEABLE:
        return
    p = CACHE_DIR / k[:2] / f"{k}.json"
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(".tmp")
    tmp.write_text(json.dumps(raw))
    tmp.replace(p)
