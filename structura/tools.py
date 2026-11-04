"""Locate and run the external solver executables (gmsh, ccx) as subprocesses.

gmsh and CalculiX are GPL. They are only ever run as separate processes, never imported into
distributed code (docs/licences.md).
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_exe(name: str) -> str:
    """Return the full path of an executable from the active environment or PATH."""
    candidates = [name, f"{name}.exe"]
    env_bin = Path(sys.prefix) / ("Library/bin" if os.name == "nt" else "bin")
    for c in candidates:
        p = env_bin / c
        if p.exists():
            return str(p)
    for c in candidates:
        found = shutil.which(c)
        if found:
            return found
    raise FileNotFoundError(f"{name} not found in {env_bin} or on PATH")


def run(args: list[str], cwd: str | Path | None = None, timeout: float = 600,
        threads: int | None = None) -> subprocess.CompletedProcess:
    """Run a command, capturing output. OMP_NUM_THREADS is set when threads is given."""
    env = dict(os.environ)
    if threads is not None:
        env["OMP_NUM_THREADS"] = str(threads)
        env["CCX_NPROC_EQUATION_SOLVER"] = str(threads)
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, timeout=timeout, env=env)
