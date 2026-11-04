"""F2: the standing validation suite, run by `pytest -m validation` before a commit. Runs the real engine path."""
import json
import os
import subprocess
from pathlib import Path

import pytest

from structura import engine

ROOT = Path(__file__).resolve().parents[1]
pytestmark = pytest.mark.validation


def test_validation_cases_all_pass(tmp_path):
    r = engine.call("validate", work=str(tmp_path / "w"), json=str(tmp_path / "v.json"), timeout=1800)
    failed = [row for row in r["rows"] if not row.get("pass")]
    assert not failed, failed
    assert r["passed"] == r["cases"] >= 7


@pytest.mark.parametrize("cases", ["*golden*,*F2-T5*", "*cross*,*F2-T4*"])
def test_engine_golden_and_cross_check(cases):
    exe = Path(engine.engine_exe()).with_name("engine_tests.exe")
    r = subprocess.run([str(exe), f"-tc={cases}"], capture_output=True, text=True, timeout=1800, cwd=ROOT, env=dict(os.environ))
    assert r.returncode == 0, r.stdout[-1500:]
    assert "test cases:" in r.stdout and " 0 failed" in r.stdout and "| 0 passed" not in r.stdout


def test_last_validation_record_is_stored_and_passing():
    d = json.loads((ROOT / "docs" / "validation.json").read_text(encoding="utf-8"))
    assert d["passed"] == d["cases"], "docs/validation.json records a failure"
