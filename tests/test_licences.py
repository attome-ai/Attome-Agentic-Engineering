"""J2: automatic licence audit. Fails on an AGPL package, on an excluded project, on a direct dependency missing from
docs/licences.md, and on a GPL library imported or linked into our own code (GPL tools run only as subprocesses)."""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
LICENCES = (ROOT / "docs" / "licences.md").read_text(encoding="utf-8").lower()
ENV = (ROOT / "environment.yml").read_text(encoding="utf-8")
CONDA_META = Path(sys.prefix) / "conda-meta"
EXCLUDED = {"fluidx3d", "prusaslicer"}  # docs/licences.md, Excluded


def installed() -> list[dict]:
    if not CONDA_META.is_dir():
        pytest.skip("not running inside the conda environment")
    return [json.loads(f.read_text(encoding="utf-8")) for f in CONDA_META.glob("*.json")]


def direct_dependencies() -> list[str]:
    names = []
    for line in ENV.split("dependencies:")[1].splitlines():
        m = re.match(r"\s*-\s*([A-Za-z0-9_.-]+)", line)
        if m:
            names.append(m.group(1).lower())
    return names


def test_no_agpl_or_excluded_package_installed():
    pkgs = installed()
    bad = [(p["name"], p.get("license")) for p in pkgs if "agpl" in str(p.get("license", "")).lower() or p["name"].lower() in EXCLUDED]
    assert not bad, f"forbidden packages in the environment: {bad}"


def test_every_direct_dependency_is_in_the_licence_table():
    table = LICENCES.split("## dependencies")[1].split("## excluded")[0]
    aliases = {"nlohmann_json": "nlohmann/json", "calculix": "calculix ccx", "occt": "occt", "mcp": "mcp python sdk", "ax-platform": "ax (ax-platform)"}
    missing = [d for d in direct_dependencies() if aliases.get(d, d) not in table]
    assert not missing, f"add these to docs/licences.md with a checked LICENSE file: {missing}"


def test_gpl_tools_are_never_imported_or_linked():
    pattern = re.compile(r"^\s*(import|from)\s+(gmsh|ccx|calculix|pygccx)\b", re.M)
    offenders = [str(p) for p in (ROOT / "structura").rglob("*.py") if pattern.search(p.read_text(encoding="utf-8"))]
    assert not offenders, f"GPL module imported: {offenders}"
    cmake = (ROOT / "engine" / "CMakeLists.txt").read_text(encoding="utf-8").lower()
    assert "find_package(gmsh" not in cmake and "gmsh.lib" not in cmake
    for src in (ROOT / "engine").glob("src/**/*.cpp"):
        assert "gmsh.h" not in src.read_text(encoding="utf-8"), src
