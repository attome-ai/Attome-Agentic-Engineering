"""P3-t0: the ten scripted briefs end to end, with success and time recorded per stage.

Stages: spec (the engine accepts the spec) -> feasibility (corner check) -> optimize -> verify (front re-solved on a
finer mesh) -> trust (grade of the lightest design still feasible on the finer mesh). A brief expected to be infeasible
succeeds when the system says so at the feasibility stage (or finds nothing feasible) instead of returning a design.

Usage: micromamba run -n attome-ae python benchmarks/brief_suite.py [--budget 150] [--only b01,h02]
Writes benchmarks/brief_suite.json and runs/brief_suite/<id>/.
"""
import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from structura import engine  # noqa: E402
from structura.optimize import portfolio  # noqa: E402
from structura.optimize.verify_front import verify_front  # noqa: E402

BRIEFS = ROOT / "benchmarks" / "briefs"
MESH = {"bracket": (4.0, 0.75, 3.0), "heatsink": (2.5, 0.6, 1.5)}  # search mesh, fine factor, trust fine size


def run_one(b: dict, budget: int) -> dict:
    spec = BRIEFS / f"{b['id']}.json"
    out = ROOT / "runs" / "brief_suite" / b["id"]
    rec = {"id": b["id"], "brief": b["brief"], "expect": b["expect"], "stages": {}}

    def stage(name, fn):
        t0 = time.perf_counter()
        try:
            r = fn()
            rec["stages"][name] = {"ok": True, "seconds": round(time.perf_counter() - t0, 1)}
            return r
        except Exception as e:  # noqa: BLE001
            rec["stages"][name] = {"ok": False, "seconds": round(time.perf_counter() - t0, 1), "error": str(e)[:300]}
            raise

    try:
        stage("spec", lambda: engine.call("spec", spec=str(spec)))
        search, factor, trust_size = MESH[b["family"]]
        f = stage("feasibility", lambda: engine.call("feasibility", spec=str(spec), **{"mesh-size": search, "threads": 4}))
        rec["feasible_by_corners"] = f["feasible"]
        if not f["feasible"]:
            rec["problems"] = [p.get("reason") for p in f.get("problems", [])]
            rec["outcome"] = "reported infeasible"
            rec["success"] = b["expect"] == "infeasible"
            return rec
        study = stage("optimize", lambda: portfolio.run(spec, budget=budget, seed=3, study_dir=out / "opt", mesh_size=search, n_jobs=8))
        rec["method"] = study.user_attrs.get("method")
        rec["evaluations"] = len(study.trials)
        rec["feasible_found"] = sum(1 for t in study.trials if t.user_attrs.get("feasible"))
        if rec["feasible_found"] == 0:
            ok = [t for t in study.trials if t.user_attrs.get("status") == "ok" and t.user_attrs.get("violations")]
            if ok:
                t = min(ok, key=lambda t: sum(t.user_attrs["violations"]))
                rec["closest"] = {"metrics": t.user_attrs.get("metrics"),
                                  "fails": [f"{c['id']} {c['quantity']} {c['value']:.4g} {c['op']} {c['limit']}"
                                            for c in t.user_attrs.get("checks", []) if not c.get("passed")]}
            rec["outcome"] = "no feasible design found"
            rec["success"] = b["expect"] == "infeasible"
            return rec
        vf = stage("verify", lambda: verify_front(study, coarse_mesh=search, factor=factor))
        rec["front"] = f"{vf['still_feasible']} of {vf['front_size']} still feasible on the finer mesh"
        ranked = sorted((r for r in vf["front"] if r.get("feasible_fine")), key=lambda r: r["fine"][0])
        if not ranked:
            rec["outcome"] = "no design stays feasible on the finer mesh"
            rec["success"] = False
            return rec
        # deliver the lightest *proven* design: verify in order of mass, take the first graded A or B (at most three tries)
        rec["verification_attempts"] = []
        t = None
        for i, light in enumerate(ranked[:3]):
            t = stage("trust", lambda: engine.call("trust", spec=str(spec), set=[f"{k}={float(v)}" for k, v in light["params"].items()],
                                                  out=str(out / f"trust{i}"), **{"fine-size": trust_size}, timeout=3600))
            rec["verification_attempts"].append({"objectives_fine": light["fine"], "grade": t.get("grade"), "reasons": t.get("reasons")})
            rec["design"] = {"params": light["params"], "objectives_fine": light["fine"]}
            if t.get("grade") in ("A", "B"):
                break
        rec["grade"] = t.get("grade")
        rec["reasons"] = t.get("reasons")
        rec["outcome"] = f"verified design, grade {t.get('grade')}"
        rec["success"] = b["expect"] == "feasible" and t.get("grade") in ("A", "B")
    except Exception as e:  # noqa: BLE001
        rec["outcome"] = f"pipeline error: {str(e)[:200]}"
        rec["success"] = False
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--budget", type=int, default=150)
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    briefs = json.loads((BRIEFS / "briefs.json").read_text())
    if a.only:
        keep = a.only.split(",")
        briefs = [b for b in briefs if any(b["id"].startswith(k) for k in keep)]
    out_path = ROOT / "benchmarks" / "brief_suite.json"
    prev = {r["id"]: r for r in json.loads(out_path.read_text())["runs"]} if out_path.exists() else {}
    for b in briefs:
        t0 = time.perf_counter()
        r = run_one(b, a.budget)
        r["seconds"] = round(time.perf_counter() - t0, 1)
        prev[b["id"]] = r
        print(f"{b['id']}: {r['outcome']} | success {r['success']} | {r['seconds']} s", flush=True)
        runs = [prev[k] for k in sorted(prev)]
        stages = ["spec", "feasibility", "optimize", "verify", "trust"]
        per = {s: f"{sum(1 for x in runs if x['stages'].get(s, {}).get('ok'))} of {sum(1 for x in runs if s in x['stages'])}" for s in stages}
        out_path.write_text(json.dumps({"budget": a.budget, "success": f"{sum(1 for x in runs if x['success'])} of {len(runs)}",
                                        "stage_success": per, "runs": runs}, indent=2))


if __name__ == "__main__":
    main()
