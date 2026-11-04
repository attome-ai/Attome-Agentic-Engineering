"""I2: the Python door. `from structura import Study` drives the same tools the agent uses, with the same approval gate.

    study = Study.from_spec("docs/examples/bracket_20kg.json")
    study.accept("me")
    study.optimize(budget=120)
    print(study.verify(study.recommended()["params"])["grade"])
    print(study.report())
"""
from __future__ import annotations

import json
import time
from pathlib import Path

from structura import engine


class StudyError(RuntimeError):
    """A tool refused or failed; `detail` holds its typed error and repair playbook."""

    def __init__(self, detail: dict):
        super().__init__(f"{detail.get('error')}: {detail.get('message')}")
        self.detail = detail


def _checked(r: dict) -> dict:
    if not r.get("ok", False):
        raise StudyError(r)
    return r


class Study:
    def __init__(self, study_id: str):
        self.id = study_id

    @classmethod
    def from_spec(cls, spec: str | Path | dict, study_id: str | None = None) -> "Study":
        from structura.agent import mcp_server as srv
        data = json.loads(Path(spec).read_text(encoding="utf-8")) if not isinstance(spec, dict) else spec
        return cls(_checked(srv.create_study(data, study_id))["study_id"])

    def card(self) -> dict:
        from structura.agent import studies as S
        return engine.call("spec", spec=str(S.latest_spec_path(self.id)))

    def accept(self, by: str, note: str = "") -> "Study":
        from structura.agent import mcp_server as srv
        _checked(srv.accept_spec(self.id, by, note))
        return self

    def feasibility(self, mesh_size: float = 6.0) -> dict:
        from structura.agent import mcp_server as srv
        return _checked(srv.check_feasibility(self.id, mesh_size))

    def optimize(self, budget: int = 120, method: str | None = None, mesh_size: float = 4.0, poll_s: float = 1.0) -> dict:
        """Runs to the end; raises StudyError if the job fails. Use the MCP or REST door for a background run."""
        from structura.agent import mcp_server as srv
        job = _checked(srv.run_optimization(self.id, budget, method, mesh_size))["job_id"]
        while True:
            j = _checked(srv.get_job(job))
            if j["state"] == "done":
                return j
            if j["state"] == "error":
                raise StudyError({"error": "optimize", "message": j.get("message")})
            time.sleep(poll_s)

    def results(self) -> dict:
        from structura.agent import mcp_server as srv
        return _checked(srv.get_results(self.id))

    def recommended(self) -> dict | None:
        return self.results().get("recommended")

    def verify(self, params: dict, fine_mesh: float = 3.0) -> dict:
        from structura.agent import mcp_server as srv
        return _checked(srv.verify(self.id, params, fine_mesh))

    def report(self, baseline: dict | None = None) -> str:
        from structura.agent import mcp_server as srv
        return _checked(srv.build_report(self.id, baseline))["file"]

    def timeline(self) -> list[dict]:
        from structura.agent import studies as S
        return S.timeline(self.id)
