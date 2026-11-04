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
    mode_file = folder / f"result_{case}" / "mode1.json"
    mode = json.loads(mode_file.read_text()) if mode_file.exists() else None  # vibration mode 1 (G2), when the spec asked for it
    volume_file = folder / f"result_{case}" / "volume.json"
    volume = json.loads(volume_file.read_text()) if volume_file.exists() else None  # corner nodes and field for the clip plane (G2)
    return {"metrics": ev["metrics"], "feasible": ev["feasible"], "checks": ev["checks"], "hash": ev["hash"], "skin": skin, "mode": mode, "volume": volume}


def build(spec_path: str, study_dir: str, out: str, baseline: dict | None = None, trust_dir: str | None = None,
          study_id: str | None = None, mesh: float = 3.0, max_designs: int = 6) -> Path:
    spec = load_spec(spec_path)
    card = engine.call("spec", spec=str(spec_path))
    study = _load_study(Path(study_dir))
    # bundles depend on the mesh size: keep one folder per size (the 3 mm folder keeps its old name)
    cache = Path(study_dir) / "report_bundles" if abs(mesh - 3.0) < 1e-9 else Path(study_dir) / f"report_bundles_{mesh:g}mm"
    front = pareto.front(study)
    rec = decide.recommend(study)
    trust = None
    if trust_dir and (Path(trust_dir) / "trust.json").exists():
        trust = json.loads((Path(trust_dir) / "trust.json").read_text())
    if trust and trust.get("params"):  # the recommended design is the one the trust record verified
        def same(p):
            return all(abs(float(p.params.get(k, 1e300)) - float(v)) <= 1e-4 * max(1.0, abs(float(v))) for k, v in trust["params"].items())
        match = next((p for p in front if same(p)), None)
        if match:
            rec = {"trial": match.number, "params": match.params, "values": list(match.values)}
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
        base["volume"] = None  # large, and the cut view is for the recommended design
    hist = []
    for t in sorted(study.get_trials(deepcopy=False), key=lambda t: t.number):
        m = t.user_attrs.get("metrics") or {}
        hist.append({"n": t.number, "status": t.user_attrs.get("status", t.state.name), "feasible": bool(t.user_attrs.get("feasible")), "params": {k: float(v) for k, v in t.params.items()}, **m})
    timeline = []
    if study_id:
        from structura.agent import studies as S
        for e in S.timeline(study_id):
            detail = {k: v for k, v in e.items() if k not in ("time", "t", "step", "outcome", "prev", "hash")}
            timeline.append({"time": e["time"], "step": e["step"], "outcome": e["outcome"], "detail": json.dumps(detail)[:160]})
        from structura.agent import routing
        use = routing.study_cost(study_id)
        if use["by_step"]:
            cost = "no price stated" if use["cost_usd"] is None else f"{use['cost_usd']} USD"
            timeline.append({"time": "", "step": "model use", "outcome": f"{use['input_tokens']} tokens in, {use['output_tokens']} out, cost: {cost}",
                             "detail": "; ".join(f"{k}: {v['calls']} calls, {v['input_tokens'] + v['output_tokens']} tokens ({', '.join(v['models'])})" for k, v in use["by_step"].items())})
        chain = S.verify_log(study_id)
        timeline.append({"time": "", "step": "audit log", "outcome": "chain verified" if chain["ok"] else "CHAIN BROKEN",
                         "detail": (f"{chain['entries']} entries, head {chain.get('head', '')[:12]}" if chain["ok"]
                                    else f"entry {chain.get('broken_at')}: {chain.get('reason')}")})
    else:
        timeline = [{"time": "", "step": "optimize", "outcome": f"{study.user_attrs.get('method', '?')}: {len(hist)} evaluations",
                     "detail": study.user_attrs.get("method_reason", "")},
                    {"time": "", "step": "verify front", "outcome": f"{len(designs)} designs re-solved at {mesh} mm"}]
        if trust:
            timeline.append({"time": "", "step": "trust", "outcome": f"grade {trust.get('grade')}", "detail": "; ".join(trust.get("reasons", []))})
    manufacturing = None
    rec_design = next((d for d in designs if d["recommended"]), designs[0] if designs else None)
    for d in designs:  # the volume field is large: keep it for the recommended design and the baseline only
        if d is not rec_design:
            d["volume"] = None
    ref = pareto.reference_point(study)
    hv = pareto.hypervolume_history(study, ref) if ref is not None else None
    sobol_file = Path(study_dir) / "sobol.json"
    sobol = json.loads(sobol_file.read_text()) if sobol_file.exists() else None
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
            "required_sf": req_sf, "objectives": objectives, "designs": designs, "manufacturing": manufacturing, "baseline": base, "history": hist, "hypervolume": hv, "sobol": sobol, "trust": trust, "timeline": timeline,
            "generated": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    html = TEMPLATE.read_text(encoding="utf-8")
    html = html.replace("__THREE__", THREE.read_text(encoding="utf-8")).replace("__DATA__", json.dumps(data).replace("</", "<\\/"))
    o = Path(out)
    o.parent.mkdir(parents=True, exist_ok=True)
    o.write_text(html, encoding="utf-8")
    return o


def to_pdf(html_path: str | Path, pdf_path: str | Path | None = None) -> Path:
    """Print the report to PDF with headless Chrome or Edge (I1); the page's #print mode freezes the 3D view first."""
    import shutil
    import subprocess
    src = Path(html_path).resolve()
    pdf = Path(pdf_path) if pdf_path else src.with_suffix(".pdf")
    cands = [shutil.which("chrome"), shutil.which("msedge"), r"C:\Program Files\Google\Chrome\Application\chrome.exe",
             r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe", "/usr/bin/google-chrome", "/usr/bin/chromium"]
    exe = next((c for c in cands if c and Path(c).exists()), None)
    if not exe:
        raise RuntimeError("no Chrome or Edge found for PDF export")
    subprocess.run([exe, "--headless=new", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--virtual-time-budget=20000",
                    "--no-pdf-header-footer", f"--print-to-pdf={pdf.resolve()}", src.as_uri() + "#print"],
                   check=True, capture_output=True, timeout=180)
    return pdf


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True)
    ap.add_argument("--study", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--baseline", nargs="*", default=[])
    ap.add_argument("--trust")
    ap.add_argument("--study-id")
    ap.add_argument("--pdf", action="store_true", help="also print the report to PDF (needs Chrome or Edge)")
    ap.add_argument("--sobol", action="store_true", help="run the Sobol screening first and show it (about 110 coarse evaluations)")
    ap.add_argument("--mesh", type=float, default=3.0, help="mesh size (mm) of the designs shown")
    a = ap.parse_args()
    base = {k: float(v) for k, v in (x.split("=") for x in a.baseline)} if a.baseline else None
    if a.sobol and not (Path(a.study) / "sobol.json").exists():
        from structura.optimize import screening
        spec = load_spec(a.spec)
        (Path(a.study) / "sobol.json").write_text(json.dumps(screening.sobol(spec, mesh_size=max(a.mesh * 2, 6.0)), indent=2))
    p = build(a.spec, a.study, a.out, base, a.trust, a.study_id, mesh=a.mesh)
    print(f"wrote {p} ({p.stat().st_size / 1e6:.1f} MB)")
    if a.pdf:
        q = to_pdf(p)
        print(f"wrote {q} ({q.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
