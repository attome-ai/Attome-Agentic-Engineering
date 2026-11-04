"""Python access to the C++ engine (engine/build/structura.exe) for the AI layer.

Every engine command prints one JSON object. Engine failures come back as {"error": kind, "message": ...} and are
raised here as EngineError with that kind, so the agent's repair loop can act on the type of failure.
"""
from __future__ import annotations

import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class EngineError(RuntimeError):
    def __init__(self, kind: str, message: str, detail: dict | None = None):
        super().__init__(f"{kind}: {message}")
        self.kind = kind
        self.detail = detail or {}


def engine_exe() -> str:
    env = os.environ.get("STRUCTURA_ENGINE")
    if env:
        return env
    name = "structura.exe" if os.name == "nt" else "structura"
    p = ROOT / "engine" / "build" / name
    if not p.exists():
        raise FileNotFoundError(f"engine not built: {p} (run scripts/build_engine.cmd)")
    return str(p)


def call(command: str, *args: str, timeout: float = 600, **opts) -> dict:
    """Run `structura <command> --k v ...`; opts with list values repeat the flag (e.g. set=["a=1", "b=2"])."""
    argv = [engine_exe(), command, *args]
    for k, v in opts.items():
        for item in (v if isinstance(v, (list, tuple)) else [v]):
            argv += [f"--{k}", str(item)]
    env = dict(os.environ)
    env.setdefault("CONDA_PREFIX", sys.prefix)
    r = subprocess.run(argv, capture_output=True, text=True, timeout=timeout, env=env)
    try:
        out = json.loads(r.stdout)
    except json.JSONDecodeError:
        raise EngineError("engine", f"non-JSON output (exit {r.returncode}) from {' '.join(map(shlex.quote, argv))}: "
                                    f"{(r.stdout + r.stderr)[-1500:]}") from None
    if isinstance(out, dict) and "error" in out:
        raise EngineError(out["error"], out.get("message", ""), out)
    return out


def build(family: str, params: dict[str, float] | None = None, out: str | Path | None = None) -> dict:
    opts = {"family": family, "set": [f"{k}={v}" for k, v in (params or {}).items()]}
    if out:
        opts["out"] = str(out)
    return call("build", **opts)


def gate(family: str, params: dict[str, float] | None = None) -> dict:
    return call("gate", family=family, set=[f"{k}={v}" for k, v in (params or {}).items()])


def mesh(family: str, params: dict[str, float] | None, out: str | Path, size: float = 4.0, order: int = 2) -> dict:
    return call("mesh", family=family, set=[f"{k}={v}" for k, v in (params or {}).items()], out=str(out), size=size, order=order)


def families() -> list[dict]:
    return call("families")


def describe() -> dict:
    return call("describe")
