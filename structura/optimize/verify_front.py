"""Coarse-to-fine re-solve of the Pareto front (D3-T2).

Optimization runs on coarse meshes; every Pareto design is re-solved with the mesh size halved, one at a time (to
bound memory), and the change in each objective is recorded. If the ranking of the front changes on the fine mesh,
the result says so: coarse numbers are never reported as final.

With --delivery-spec the front is re-solved on the geometry that will be delivered instead (decision D10): the search runs
on a simple family that meshes fast, and the small fillets it leaves out can lower the safety factor. The first design,
lightest first, that still meets every requirement is the one to verify.

Usage: python -m structura.optimize.verify_front <study_dir> [--factor 0.5] [--delivery-spec SPEC --mesh 3]
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import optuna

from structura.engine import EngineError

from . import pareto
from .evaluate import evaluate, load_spec


def verify_front(study: optuna.Study, coarse_mesh: float, factor: float = 0.5) -> dict:
    spec = load_spec(study.user_attrs["spec_path"])
    front = pareto.front(study)
    fine = coarse_mesh * factor
    rows = []
    for p in front:
        ev = evaluate(spec, {k: float(v) for k, v in p.params.items()}, mesh_size=fine, threads=4)
        row = {"trial": p.number, "params": p.params, "coarse": p.values, "status": ev.status}
        if ev.status == "ok":
            row["fine"] = ev.objectives
            row["change_pct"] = [100 * (f - c) / abs(c) if c else None for c, f in zip(p.values, ev.objectives)]
            row["feasible_fine"] = ev.feasible
        rows.append(row)
    order_c = [r["trial"] for r in sorted(rows, key=lambda r: r["coarse"][0])]
    ok = [r for r in rows if "fine" in r]
    order_f = [r["trial"] for r in sorted(ok, key=lambda r: r["fine"][0])]
    changed = order_c[: len(order_f)] != order_f
    return {"coarse_mesh": coarse_mesh, "fine_mesh": fine, "front": rows, "ranking_changed": changed,
            "still_feasible": sum(1 for r in ok if r.get("feasible_fine")), "front_size": len(rows)}


def verify_delivery(study: optuna.Study, delivery_spec: str, mesh: float | None = None, threads: int = 8) -> dict:
    """Re-solve the feasible front on the delivered geometry, lightest first. Every design keeps its search parameters."""
    spec = load_spec(delivery_spec)
    rows = []
    for p in sorted(pareto.front(study), key=lambda p: p.values[0]):
        row = {"trial": p.number, "params": p.params, "search": p.values}
        try:
            ev = evaluate(spec, {k: float(v) for k, v in p.params.items()}, mesh_size=mesh, threads=threads)
        except EngineError as e:  # a small fillet may not build on this design; that is a result, not a crash
            rows.append({**row, "status": "build_fail", "reason": str(e)[:200]})
            continue
        row["status"] = ev.status
        if ev.status == "ok":
            row.update(delivered=ev.objectives, feasible=ev.feasible, change_pct=[100 * (d - c) / abs(c) if c else None for c, d in zip(p.values, ev.objectives)])
        else:
            row["reason"] = ev.reason[:200]
        rows.append(row)
        print(row["trial"], "search", [round(v, 3) for v in p.values], "delivered", [round(v, 3) for v in row.get("delivered", [])], row.get("feasible", ev.status), flush=True)
    ok = [r for r in rows if r.get("feasible")]
    return {"delivery_spec": str(delivery_spec), "mesh": mesh, "front": rows, "front_size": len(rows), "built": sum(1 for r in rows if r["status"] == "ok"),
            "still_feasible": len(ok), "lightest_feasible": ok[0] if ok else None}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("study_dir")
    ap.add_argument("--coarse", type=float, default=6.0)
    ap.add_argument("--factor", type=float, default=0.5)
    ap.add_argument("--delivery-spec", help="re-solve the front on this spec's geometry instead")
    ap.add_argument("--mesh", type=float, help="mesh size for --delivery-spec (default: the spec's)")
    a = ap.parse_args()
    d = Path(a.study_dir)
    storage = f"sqlite:///{(d / 'study.db').resolve().as_posix()}"
    names = optuna.study.get_all_study_names(storage)
    study = optuna.load_study(study_name=names[0], storage=storage)
    if a.delivery_spec:
        out = verify_delivery(study, a.delivery_spec, a.mesh)
        (d / "verify_delivery.json").write_text(json.dumps(out, indent=2))
        print("built", out["built"], "| still feasible", out["still_feasible"], "of", out["front_size"], "| lightest:", out["lightest_feasible"])
        return
    out = verify_front(study, a.coarse, a.factor)
    (d / "verify_front.json").write_text(json.dumps(out, indent=2))
    for r in out["front"]:
        print(r["trial"], "coarse", r["coarse"], "fine", r.get("fine"), "change %", r.get("change_pct"))
    print("ranking changed:", out["ranking_changed"], "| still feasible on the fine mesh:", out["still_feasible"], "of", out["front_size"])


if __name__ == "__main__":
    main()
