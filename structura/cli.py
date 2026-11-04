"""I2: the headless door, for batch runs and CI.

  python -m structura.cli run SPEC.json --accept [--budget 120] [--mesh 4] [--fine 3] [--out runs/NAME]

Stages: feasibility, optimize, verify the lightest design still feasible on the finer mesh, report. Writes a result.json
beside the report. Exit code 0 when a verified design is delivered or the brief is correctly reported infeasible at the
feasibility stage, 1 when a study fails, 2 when the spec was not accepted.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

from structura import engine
from structura.sdk import Study, StudyError


def run(a) -> int:
    if not a.accept:
        print("refused: a study starts only from an accepted spec. Read the spec card, then pass --accept.", file=sys.stderr)
        print(json.dumps(engine.call("spec", spec=a.spec), indent=2))
        return 2
    study = Study.from_spec(a.spec)
    study.accept("command line (--accept)")
    result = {"study_id": study.id, "spec": str(a.spec), "budget": a.budget}
    out = Path(a.out or f"runs/cli/{study.id}")
    out.mkdir(parents=True, exist_ok=True)
    f = study.feasibility(a.mesh)
    result["feasibility"] = {k: f.get(k) for k in ("feasible", "problems", "inconclusive") if k in f}
    if f.get("feasible") is False:
        result["outcome"] = "reported infeasible"
        (out / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 0
    study.optimize(a.budget, a.method, a.mesh)
    res = study.results()
    result["method"], result["summary"] = res.get("method"), res.get("summary")
    if not res["front"]:
        result["outcome"] = "no feasible design found"
        result["closest_infeasible"] = res.get("closest_infeasible")
        (out / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 1
    pick = res["recommended"]
    t = study.verify(pick["params"], a.fine)
    result["design"] = {"params": pick["params"], "values": pick["values"]}
    result["trust"] = {k: t.get(k) for k in ("grade", "status", "reasons", "requirements", "reproduce")}
    report = study.report()
    shutil.copyfile(report, out / "report.html")
    result["report"] = str(out / "report.html")
    result["outcome"] = f"design delivered, grade {t.get('grade')}"
    (out / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if t.get("status") == "verified" else 1


def main() -> int:
    ap = argparse.ArgumentParser(prog="attome")
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run", help="run a study headless from a spec file")
    r.add_argument("spec")
    r.add_argument("--accept", action="store_true", help="I have read the spec card and accept it")
    r.add_argument("--budget", type=int, default=120)
    r.add_argument("--mesh", type=float, default=4.0, help="search mesh size (mm)")
    r.add_argument("--fine", type=float, default=3.0, help="verification mesh size (mm)")
    r.add_argument("--method", default=None, help="nsga2, nsga3, botorch_qnehvi; default: chosen from the measured evaluation cost")
    r.add_argument("--out", default=None)
    a = ap.parse_args()
    try:
        return run(a)
    except (StudyError, engine.EngineError) as e:
        print(json.dumps({"outcome": "failed", "error": str(e), "detail": getattr(e, "detail", None)}, indent=2), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
