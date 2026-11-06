"""P3-t3: Text2CAD-Bench (arXiv 2605.18430) through the system's path, on the public preview prompts.

Each answer (benchmarks/text2cad/answers_l1.py) is built: grammar answers by the C++ engine (`structura build`),
CadQuery answers by the isolated freeform runner the agent uses (60 s limit, the benchmark's timeout). Every STEP is then
read back and checked the same way: a valid B-rep with non-zero volume. The invalid rate follows the paper's definition
(code or runtime error, time-out, or empty or degenerate output); a valid result with more than one solid is valid CAD
and is counted as valid, but flagged, because the engine's analyses need exactly one solid.

Chamfer distance and IoU need the benchmark's reference shapes, which are not public ("contact us"); `metrics.py` is
ready for them and writes nothing until they are present.

Usage: python benchmarks/text2cad/run.py [--level L1|L3]  ->  benchmarks/text2cad_<level>.json and runs/text2cad/<id>/part.step
"""
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(Path(__file__).parent))
from structura import engine  # noqa: E402
import importlib  # noqa: E402
import argparse  # noqa: E402

RUNS = ROOT / "runs" / "text2cad"
RUNNER = ROOT / "structura" / "agent" / "freeform_runner.py"


def build_grammar(pid, feats, out):
    g = {"name": f"t2c_{pid}", "version": "1", "doc": f"Text2CAD-Bench {pid}", "parameters": {},
         "features": [dict(f, id=f"f{i}") for i, f in enumerate(feats)], "faces": {}}
    gp = out / "graph.json"
    gp.write_text(json.dumps(g, indent=1))
    try:
        r = engine.call("build", graph=str(gp), out=str(out))
    except engine.EngineError as e:
        return {"error": f"{e.kind}: {str(e)[:300]}"}
    return {"engine": {k: r.get(k) for k in ("solids", "valid", "volume_mm3", "bbox")}}


def build_cadquery(code, out):
    (out / "model.py").write_text(code, encoding="utf-8")
    step = out / "part.step"
    step.unlink(missing_ok=True)
    try:
        p = subprocess.run([sys.executable, "-I", str(RUNNER), str(out / "model.py"), str(step)], cwd=str(out),
                           capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return {"error": "timeout: over 60 s"}
    if p.returncode != 0 or not step.exists():
        lines = [ln for ln in p.stdout.splitlines() if ln.startswith("{")]
        msg = json.loads(lines[-1])["message"] if lines else (p.stderr or p.stdout)
        return {"error": msg.strip().splitlines()[-1][:300] if msg.strip() else "no output"}
    return {}


def check(step):
    import cadquery as cq
    s = cq.importers.importStep(str(step)).val()
    solids = s.Solids()
    vol = sum(x.Volume() for x in solids)
    bb = s.BoundingBox()
    ok = bool(s.isValid()) and len(solids) >= 1 and vol > 1e-6
    return {"valid": ok, "solids": len(solids), "volume_mm3": round(vol, 3),
            "bbox_mm": [round(v, 3) for v in (bb.xmin, bb.ymin, bb.zmin, bb.xmax, bb.ymax, bb.zmax)]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--level", default="L1", choices=["L1", "L3"])
    lvl = ap.parse_args().level
    A = importlib.import_module(f"answers_{lvl.lower()}")
    ids = sorted(set(A.G) | set(A.C), key=lambda s: int(s.split("_")[1]))
    rows = []
    for pid in ids:
        out = RUNS / pid
        out.mkdir(parents=True, exist_ok=True)
        t0 = time.perf_counter()
        path = "grammar" if pid in A.G else "cadquery"
        r = build_grammar(pid, A.G[pid], out) if path == "grammar" else build_cadquery(A.C[pid], out)
        row = {"id": pid, "path": path}
        if "error" in r:
            row.update(valid=False, error=r["error"])
        else:
            try:
                row.update(check(out / "part.step"))
            except Exception as e:  # noqa: BLE001
                row.update(valid=False, error=f"read-back: {e}"[:300])
        row["seconds"] = round(time.perf_counter() - t0, 2)
        rows.append(row)
        print(pid, path, "valid" if row["valid"] else "INVALID", row.get("solids", ""), row.get("error", ""), flush=True)
    from volumes_l1 import V, TOL
    V = V if lvl == "L1" else {}
    vol_fail = []
    for r in rows:
        if r["id"] in V and r.get("volume_mm3"):
            r["volume_expected_mm3"] = round(V[r["id"]], 3)
            r["volume_error"] = round(r["volume_mm3"] / V[r["id"]] - 1, 6)
            if abs(r["volume_error"]) > TOL.get(r["id"], 1e-4):
                vol_fail.append(r["id"])
    n = len(rows)
    inv = [r for r in rows if not r["valid"]]
    by = {p: {"answers": sum(1 for r in rows if r["path"] == p), "invalid": sum(1 for r in inv if r["path"] == p)}
          for p in ("grammar", "cadquery")}
    out = {"benchmark": f"Text2CAD-Bench {lvl}, geometric prompts, public preview ({len(ids)} of {200 if lvl == 'L1' else 100})",
           "source": "https://huggingface.co/datasets/AICAD/Text2CAD-Bench (release.csv, CC BY 4.0)",
           "model": "claude-opus-5-5 in a Claude Code session, one pass, not through the API",
           "invalid_rate": f"{len(inv)} of {n} ({100 * len(inv) / n:.1f}%)", "by_path": by,
           "multi_solid": [r["id"] for r in rows if r.get("solids", 1) > 1],
           "volume_check": {"checked": sum(1 for r in rows if "volume_error" in r), "off": vol_fail,
                            "note": "closed-form volume from the prompt's dimensions (volumes_l1.py); checks the build, not the reading"},
           "published_geo_invalid_rate": ({"GPT-5.2": "11.1%", "Claude-4.5-Sonnet": "20.3%", "DeepSeek-V3.2": "13.3%"} if lvl == "L1"
                                          else "about 70-90% for most models; Claude-4.5-Sonnet about 70% (paper, L3)"),
           "repairs": getattr(A, "REPAIRS", {}),
           "first_attempt": getattr(A, "FIRST_ATTEMPT", None),
           "not_measured": "Chamfer distance and IoU: the reference shapes are not public; see metrics.py",
           "rows": rows}
    (ROOT / "benchmarks" / f"text2cad_{lvl.lower()}.json").write_text(json.dumps(out, indent=1))
    print(out["invalid_rate"], by, "volume check off:", [(i, next(r["volume_error"] for r in rows if r["id"] == i)) for i in vol_fail])


if __name__ == "__main__":
    main()
