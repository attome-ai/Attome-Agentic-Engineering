"""P3-t1: extract spec fields from the 50 briefs with a model, then score them against the gold fields.

  extract: python -m benchmarks.brief_bench.run extract --model claude-opus-5-5 --out runs/brief_bench/opus
           (needs ANTHROPIC_API_KEY and `pip install anthropic`; the model sees only the brief and the field rules)
  score:   python -m benchmarks.brief_bench.run score runs/brief_bench/opus
           writes benchmarks/brief_bench_<name>.json: per-field accuracy, unit correctness, and every miss.
Targets (feature plan A1): at least 90% of spec fields and 100% of units correct.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from benchmarks.brief_bench.briefs import BRIEFS  # noqa: E402

FIELDS = ["family", "material", "load_N", "sf", "disp_mm", "power_W", "t_max_C", "fan_m_s", "objective", "process"]
NUMERIC = {"load_N", "sf", "disp_mm", "power_W", "t_max_C", "fan_m_s"}

INSTRUCTIONS = """You turn an engineering brief (English or Arabic) into spec fields for a design engine. Answer with one JSON
object and nothing else, with exactly these keys:
  family: "bracket" (wall/shelf brackets and supports) or "heatsink" (plate-fin heat sinks), or null if neither fits
  material: "PETG", "PLA", "AL6061T6" (any aluminium), "S235" (any steel), or null if the brief does not name one
  load_N: total load in newtons (kg x 9.81, lb x 4.448), or null
  sf: required safety factor if the brief states one, else null
  disp_mm: maximum deflection in mm if stated, else null
  power_W: heat input in watts, or null
  t_max_C: maximum temperature in degrees Celsius (convert Fahrenheit), or null
  fan_m_s: air speed in m/s if a number is given, else null
  objective: "min_mass" (light), "min_cost" (cheap), "max_safety" (strongest), or "min_temperature" (coolest); null if family is null
  process: "fdm" if printed, "cnc" if machined, else null
Use null for anything the brief does not say; do not guess defaults."""


def extract(model: str, out: Path) -> None:
    if not os.environ.get("ANTHROPIC_API_KEY"):
        sys.exit("ANTHROPIC_API_KEY is not set")
    import anthropic  # noqa: PLC0415 - optional dependency, only for this command
    client = anthropic.Anthropic()
    out.mkdir(parents=True, exist_ok=True)
    for bid, brief, _ in BRIEFS:
        msg = client.messages.create(model=model, max_tokens=400, system=INSTRUCTIONS, messages=[{"role": "user", "content": brief}])
        text = "".join(b.text for b in msg.content if getattr(b, "type", "") == "text").strip()
        start, end = text.find("{"), text.rfind("}")
        try:
            pred = json.loads(text[start:end + 1])
        except Exception:  # noqa: BLE001
            pred = {"_unparsed": text}
        (out / f"{bid}.json").write_text(json.dumps(pred, ensure_ascii=False, indent=2), encoding="utf-8")
        print(bid, "ok" if "_unparsed" not in pred else "unparsed", flush=True)


def same(field: str, gold, pred) -> bool:
    if gold is None or pred is None:
        return gold is None and pred is None
    if field in NUMERIC:
        try:
            return abs(float(pred) - float(gold)) <= 0.01 * abs(float(gold))
        except (TypeError, ValueError):
            return False
    return str(pred).strip().lower() == str(gold).strip().lower()


def score(pred_dir: Path) -> dict:
    per = {f: [0, 0] for f in FIELDS}
    units = [0, 0]
    stated = [0, 0]  # fields the brief states (gold not null): saying nothing scores 0 here
    misses = []
    for bid, brief, gold in BRIEFS:
        p = pred_dir / f"{bid}.json"
        pred = json.loads(p.read_text(encoding="utf-8")) if p.exists() else {}
        for f in FIELDS:
            ok = same(f, gold[f], pred.get(f))
            per[f][0] += ok
            per[f][1] += 1
            if gold[f] is not None:
                stated[0] += ok
                stated[1] += 1
            if f in NUMERIC and gold[f] is not None:
                units[0] += ok
                units[1] += 1
            if not ok:
                misses.append({"id": bid, "field": f, "gold": gold[f], "pred": pred.get(f)})
    total_ok = sum(v[0] for v in per.values())
    total = sum(v[1] for v in per.values())
    res = {"predictions": str(pred_dir), "fields_correct": f"{total_ok} of {total} ({100 * total_ok / total:.1f}%)",
           "stated_fields_correct": f"{stated[0]} of {stated[1]} ({100 * stated[0] / max(1, stated[1]):.1f}%)",
           "units_correct": f"{units[0]} of {units[1]} ({100 * units[0] / max(1, units[1]):.1f}%)",
           "per_field": {f: f"{a} of {b}" for f, (a, b) in per.items()}, "misses": misses}
    return res


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("extract")
    e.add_argument("--model", required=True)
    e.add_argument("--out", required=True)
    s = sub.add_parser("score")
    s.add_argument("pred_dir")
    a = ap.parse_args()
    if a.cmd == "extract":
        extract(a.model, Path(a.out))
    else:
        r = score(Path(a.pred_dir))
        name = Path(a.pred_dir).name
        (ROOT / "benchmarks" / f"brief_bench_{name}.json").write_text(json.dumps(r, ensure_ascii=False, indent=2), encoding="utf-8")
        print(r["fields_correct"], "| stated fields", r["stated_fields_correct"], "| units", r["units_correct"])
        print(json.dumps(r["per_field"]))


if __name__ == "__main__":
    main()
