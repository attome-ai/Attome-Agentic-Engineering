"""Build the self-contained interactive study report (I1, G1-G6): one HTML file that opens offline in any browser.

Every number shown comes from a stored engine run: the Pareto designs and the baseline are re-solved on the fine mesh
with their skin fields, the history comes from the study storage, the trust panel from the engine's trust.json.

Usage: python -m structura.report.build --spec SPEC --study STUDY_DIR [--baseline k=v ...] [--trust DIR] [--study-id ID] --out FILE
"""
from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

import optuna

from structura import engine
from structura.optimize import decide, pareto
from structura.optimize.evaluate import evaluate, load_spec

ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = Path(__file__).with_name("template.html")
THREE = ROOT / "data" / "vendor" / "three-0.149.0.min.js"


def _load_study(study_dir: Path) -> optuna.Study:
    url = f"sqlite:///{(study_dir / 'study.db').resolve().as_posix()}"
    return optuna.load_study(study_name=optuna.study.get_all_study_names(url)[0], storage=url)


def _bundle(spec, params: dict, mesh: float, folder: Path) -> dict:
    """Fine-mesh evaluation with its skin; reuses an existing bundle folder."""
    ev_path = folder / "evaluation.json"
    if not ev_path.exists():
        evaluate(spec, params, mesh_size=mesh, threads=4, workdir=folder, bundle=True)
    ev = json.loads(ev_path.read_text())
    case = next(iter(ev["load_cases"]))
    skin = json.loads((folder / f"result_{case}" / "skin.json").read_text())
    return {"metrics": ev["metrics"], "feasible": ev["feasible"], "checks": ev["checks"], "hash": ev["hash"], "skin": skin}


def build(spec_path: str, study_dir: str, out: str, baseline: dict | None = None, trust_dir: str | None = None,
          study_id: str | None = None, mesh: float = 3.0, max_designs: int = 6) -> Path:
    spec = load_spec(spec_path)
    card = engine.call("spec", spec=str(spec_path))
    study = _load_study(Path(study_dir))
    cache = Path(study_dir) / "report_bundles"
    front = pareto.front(study)
    rec = decide.recommend(study)
    chosen = sorted(front, key=lambda p: p.values[0])
    if len(chosen) > max_designs:  # keep the ends and the recommended one, then spread the rest
        keep = {chosen[0].number, chosen[-1].number, rec["trial"] if rec else -1}
        step = max(1, len(chosen) // max_designs)
        keep |= {p.number for p in chosen[::step]}
        chosen = [p for p in chosen if p.number in keep][:max_designs]
    designs = []
    for p in chosen:
        b = _bundle(spec, {k: float(v) for k, v in p.params.items()}, mesh, cache / f"trial_{p.number}")
        designs.append({"trial": p.number, "params": p.params, "on_front": True, "recommended": bool(rec and rec["trial"] == p.number), **b})
    base = None
    if baseline:
        base = {"trial": "baseline", "params": baseline, **_bundle(spec, baseline, mesh, cache / "baseline")}
    hist = []
    for t in sorted(study.get_trials(deepcopy=False), key=lambda t: t.number):
        m = t.user_attrs.get("metrics") or {}
        hist.append({"n": t.number, "status": t.user_attrs.get("status", t.state.name), "feasible": bool(t.user_attrs.get("feasible")), **m})
    trust = None
    if trust_dir and (Path(trust_dir) / "trust.json").exists():
        trust = json.loads((Path(trust_dir) / "trust.json").read_text())
    timeline = []
    if study_id:
        from structura.agent import studies as S
        for e in S.timeline(study_id):
            detail = {k: v for k, v in e.items() if k not in ("time", "t", "step", "outcome")}
            timeline.append({"time": e["time"], "step": e["step"], "outcome": e["outcome"], "detail": json.dumps(detail)[:160]})
    else:
        timeline = [{"time": "", "step": "optimize", "outcome": f"{study.user_attrs.get('method', '?')}: {len(hist)} evaluations",
                     "detail": study.user_attrs.get("method_reason", "")},
                    {"time": "", "step": "verify front", "outcome": f"{len(designs)} designs re-solved at {mesh} mm"}]
        if trust:
            timeline.append({"time": "", "step": "trust", "outcome": f"grade {trust.get('grade')}", "detail": "; ".join(trust.get("reasons", []))})
    manufacturing = None
    rec_design = next((d for d in designs if d["recommended"]), designs[0] if designs else None)
    if rec_design:
        raw_spec = json.loads(Path(spec_path).read_text(encoding="utf-8"))
        try:
            m = engine.call("manufacture", family=raw_spec["family"], set=[f"{k}={float(v)}" for k, v in rec_design["params"].items()],
                            material=raw_spec.get("material", "PETG"), **{"print-dir": json.dumps(raw_spec.get("print_direction", [0, 0, 1]))})
            manufacturing = {k: v for k, v in m.items() if k not in ("fdm", "cnc")}
            for p in ("fdm", "cnc"):
                if p in m:
                    manufacturing[p] = {k: v for k, v in m[p].items() if k not in ("inputs", "price_per_kg")}
        except engine.EngineError as e:
            manufacturing = {"advice": [{"process": "all", "fits": False, "reason": f"manufacturing check failed: {e}"}]}
    req_sf = next((r["value"] for r in card["requirements"] if r["quantity"] == "safety_factor" and r["op"] == ">="), None)
    units = {"mass_g": "g", "safety_factor": "", "max_disp_mm": "mm", "max_von_mises_MPa": "MPa", "peak_temperature": "degC"}
    objectives = [{"quantity": q, "sense": sense, "unit": units.get(q, "")} for q, sense in spec.objectives]
    data = {"title": f"{card['id']}: {spec.objectives[0][0]} vs {spec.objectives[1][0]}" if len(spec.objectives) > 1 else card["id"],
            "brief": card.get("brief") or json.loads(Path(spec_path).read_text()).get("brief", ""),
            "spec": {"id": card["id"], "version": card["version"], "hash": card["hash"], "requirements": card["requirements"]},
            "required_sf": req_sf, "objectives": objectives, "designs": designs, "manufacturing": manufacturing, "baseline": base, "history": hist, "trust": trust, "timeline": timeline,
            "generated": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    html = TEMPLATE.read_text(encoding="utf-8")
    html = html.replace("__THREE__", THREE.read_text(encoding="utf-8")).replace("__DATA__", json.dumps(data).replace("</", "<\\/"))
    o = Path(out)
    o.parent.mkdir(parents=True, exist_ok=True)
    o.write_text(html, encoding="utf-8")
    return o


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True)
    ap.add_argument("--study", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--baseline", nargs="*", default=[])
    ap.add_argument("--trust")
    ap.add_argument("--study-id")
    a = ap.parse_args()
    base = {k: float(v) for k, v in (x.split("=") for x in a.baseline)} if a.baseline else None
    p = build(a.spec, a.study, a.out, base, a.trust, a.study_id)
    print(f"wrote {p} ({p.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
