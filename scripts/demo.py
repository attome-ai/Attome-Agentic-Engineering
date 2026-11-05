"""Live demo runner and rehearsal (P7): the scripted 20 kg bracket brief, driven through the same MCP tool functions
Claude calls, with a preflight check, per-step timing and a precomputed fallback.

Usage:
  micromamba run -n attome-ae python scripts/demo.py preflight
  micromamba run -n attome-ae python scripts/demo.py live [--budget 200] [--limit-min 12]
  micromamba run -n attome-ae python scripts/demo.py fallback

`live` records each rehearsal in benchmarks/demo_rehearsals.json (step, seconds, outcome) so the timing on the demo
laptop is known before 3 February. If a step fails or the run exceeds --limit-min, it prints the precomputed report
to open instead (runs/p2_demo/report.html, built from the stored P2 run).
"""
from __future__ import annotations

import argparse
import json
import platform
import shutil
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

SPEC = ROOT / "docs" / "examples" / "bracket_20kg.json"
BASELINE = {"thickness": 6, "plate_width": 50, "rib_count": 1, "rib_thickness": 4, "fillet_radius": 3}
FALLBACK = ROOT / "runs" / "p2_demo_delivered" / "report.html"
LOG = ROOT / "benchmarks" / "demo_rehearsals.json"


def preflight() -> list[tuple[str, bool, str]]:
    from structura import engine
    checks = []
    exe = engine.engine_exe()
    checks.append(("engine executable", Path(exe).exists(), exe))
    for tool in ("ccx", "gmsh"):
        p = shutil.which(tool) or shutil.which(tool + ".exe")
        checks.append((f"{tool} on PATH", bool(p), p or "not found"))
    three = ROOT / "data" / "vendor" / "three-0.149.0.min.js"
    checks.append(("viewer library vendored (no network needed)", three.exists(), str(three)))
    checks.append(("precomputed fallback report", FALLBACK.exists(), str(FALLBACK)))
    try:
        v = engine.call("validate", timeout=600)
        n = int(v.get("cases", 0))
        checks.append(("validation suite", n > 0 and v.get("passed") == n, f"{v.get('passed')} of {n} cases pass"))
    except Exception as e:  # noqa: BLE001
        checks.append(("validation suite", False, str(e)[:200]))
    try:
        import psutil  # optional
        gb = psutil.virtual_memory().total / 2**30
        checks.append(("memory", gb >= 7.5, f"{gb:.1f} GB"))
    except ImportError:
        checks.append(("memory", True, "psutil not installed; not checked"))
    return checks


def live(budget: int, limit_min: float) -> dict:
    from structura.agent import mcp_server as srv
    steps, t_start = [], time.perf_counter()
    sid = "demo-" + datetime.now().strftime("%Y%m%d-%H%M%S")

    def step(name, fn, *a, **kw):
        t0 = time.perf_counter()
        try:
            r = fn(*a, **kw)
            ok = not (isinstance(r, dict) and r.get("ok") is False)
        except Exception as e:  # noqa: BLE001
            r, ok = {"error": str(e)}, False
        dt = time.perf_counter() - t0
        steps.append({"step": name, "seconds": round(dt, 1), "ok": ok})
        print(f"[{time.perf_counter() - t_start:6.0f} s] {name}: {'ok' if ok else 'FAILED'} ({dt:.1f} s)", flush=True)
        if not ok:
            raise RuntimeError(f"{name}: {json.dumps(r)[:300]}")
        return r

    outcome = "ok"
    try:
        step("create_study", srv.create_study, json.loads(SPEC.read_text()), study_id=sid)
        step("accept_spec", srv.accept_spec, sid, "demo")
        f = step("check_feasibility", srv.check_feasibility, sid)
        print("   feasible:", f.get("feasible"))
        b = step("evaluate baseline", srv.evaluate_design, sid, BASELINE)
        print("   baseline:", {k: round(v, 2) for k, v in (b.get("metrics") or {}).items()}, "feasible", b.get("feasible"))
        job = step("run_optimization", srv.run_optimization, sid, budget=budget, mesh_size=4.0)["job_id"]
        t0 = time.perf_counter()
        while True:
            j = srv.get_job(job)
            if j.get("state") != "running":
                break
            if time.perf_counter() - t_start > limit_min * 60:
                raise RuntimeError(f"time limit of {limit_min} min reached during the optimization")
            print(f"   trials {j.get('trials')}/{budget}, feasible {j.get('feasible')}", flush=True)
            time.sleep(10)
        steps.append({"step": "optimization (background)", "seconds": round(time.perf_counter() - t0, 1), "ok": j.get("state") == "done"})
        if j.get("state") != "done":
            raise RuntimeError(f"optimization ended in state {j.get('state')}: {j.get('error')}")
        res = step("get_results", srv.get_results, sid)
        rec = res.get("recommended")
        if not rec:
            raise RuntimeError("no feasible design found")
        print("   recommended:", {k: round(float(v), 2) for k, v in rec["params"].items()}, [round(x, 3) for x in rec["values"]])
        v = step("verify", srv.verify, sid, rec["params"])
        print("   trust grade:", v.get("grade"), v.get("reasons"))
        m = step("manufacturing_advice", srv.manufacturing_advice, sid, rec["params"])
        print("   process:", m.get("recommended_process"))
        r = step("build_report", srv.build_report, sid, BASELINE)
        print("   report:", r.get("file"))
    except Exception as e:  # noqa: BLE001
        outcome = f"fallback: {e}"
        print(f"\n!! {e}\n!! open the precomputed report instead: {FALLBACK}")
    total = round(time.perf_counter() - t_start, 1)
    rec_ = {"time": datetime.now(timezone.utc).isoformat(timespec="seconds"), "machine": platform.node(), "processor": platform.processor(),
            "budget": budget, "study_id": sid, "total_seconds": total, "outcome": outcome, "steps": steps}
    log = json.loads(LOG.read_text()) if LOG.exists() else []
    log.append(rec_)
    LOG.write_text(json.dumps(log, indent=2))
    print(f"\ntotal {total} s, outcome {outcome}; logged to {LOG.relative_to(ROOT)}")
    return rec_


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["preflight", "live", "fallback"])
    ap.add_argument("--budget", type=int, default=200)
    ap.add_argument("--limit-min", type=float, default=12)
    a = ap.parse_args()
    if a.mode == "preflight":
        bad = 0
        for name, ok, detail in preflight():
            print(f"{'PASS' if ok else 'FAIL'}  {name}: {detail}")
            bad += not ok
        sys.exit(1 if bad else 0)
    if a.mode == "fallback":
        print(FALLBACK if FALLBACK.exists() else "fallback report missing: run benchmarks/p2_demo.py and structura.report.build")
        return
    live(a.budget, a.limit_min)


if __name__ == "__main__":
    main()
