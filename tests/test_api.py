"""I2: REST, SDK and CLI reach the same tools, with the approval gate intact and typed errors."""
import json
import subprocess
import sys
import threading
import urllib.error
import urllib.request
from pathlib import Path

import pytest

from structura import Study, sdk
from structura.agent import studies as S
from structura.app import rest

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "docs" / "examples" / "bracket_20kg.json"


@pytest.fixture(scope="module")
def server():
    srv = rest.serve(0)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{srv.server_address[1]}"
    srv.shutdown()


def post(url, body):
    req = urllib.request.Request(url, data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return json.loads(r.read())


def test_openapi_covers_every_tool(server):
    doc = json.loads(urllib.request.urlopen(server + "/openapi.json").read())
    names = {t["name"] for t in json.loads(urllib.request.urlopen(server + "/api/tools").read())}
    assert len(names) >= 20
    for n in names:
        assert f"/api/tools/{n}" in doc["paths"]


def test_rest_calls_the_same_tool(server):
    r = post(server + "/api/tools/derive_load", {"mass_kg": 20})
    assert abs(r["nominal_N"] - 196.133) < 0.01  # 20 kg x 9.80665


def test_rest_errors_are_typed(server):
    with pytest.raises(urllib.error.HTTPError) as e:
        post(server + "/api/tools/no_such_tool", {})
    assert e.value.code == 404
    with pytest.raises(urllib.error.HTTPError) as e:
        post(server + "/api/tools/derive_load", {"wrong_argument": 1})
    assert e.value.code == 400


def test_rest_approval_gate_and_events(server):
    spec = json.loads(SPEC.read_text(encoding="utf-8"))
    sid = post(server + "/api/tools/create_study", {"spec": spec})["study_id"]
    refused = post(server + "/api/tools/run_optimization", {"study_id": sid, "budget": 4})
    assert refused["ok"] is False and refused["playbook"] == "approval"
    stream = urllib.request.urlopen(f"{server}/api/studies/{sid}/events?once=1", timeout=30).read().decode()
    assert "event: step" in stream and '"spec"' in stream and "event: end" in stream


def test_sdk_refuses_without_acceptance_then_runs(tmp_path):
    study = Study.from_spec(SPEC)
    with pytest.raises(sdk.StudyError) as e:
        study.optimize(budget=4, mesh_size=8)
    assert e.value.detail["playbook"] == "approval"
    study.accept("pytest")
    done = study.optimize(budget=8, method="nsga2", mesh_size=8, poll_s=0.2)
    assert done["state"] == "done"
    assert any(e["step"] == "optimize" for e in study.timeline())


def test_cli_needs_acceptance():
    r = subprocess.run([sys.executable, "-m", "structura.cli", "run", str(SPEC)], capture_output=True, text=True, cwd=ROOT, timeout=120)
    assert r.returncode == 2
    assert "accept" in r.stderr
