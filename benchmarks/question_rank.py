"""A5: run rank_questions on the structural briefs in benchmarks/briefs and record which questions it would ask.

The same four unknowns go to every brief, so the differences come from the brief. Thermal briefs are left out: the
supported edits act on a load case.

Usage: python -m benchmarks.question_rank [--out benchmarks/question_rank.json]
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from structura.agent import questions as Q

ROOT = Path(__file__).resolve().parents[1]
UNKNOWNS = [
    {"name": "load direction", "question": "Is the load straight down, or can it also push sideways?", "low": {}, "high": {"add_load": [0, 0.5, 0]}},
    {"name": "load size", "question": "Could the load be 30% more than stated?", "low": {}, "high": {"load_scale": 1.3}},
    {"name": "safety factor on the load", "question": "Is this a variable load (factor 1.5) or a fixed one (factor 1.1)?", "low": {"factor": 1.1}, "high": {"factor": 1.5}},
    {"name": "material", "question": "Is PETG certain, or could it be printed in PLA?", "low": {"material": "PETG"}, "high": {"material": "PLA"}},
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "benchmarks" / "question_rank.json"))
    a = ap.parse_args()
    rows = []
    for p in sorted((ROOT / "benchmarks" / "briefs").glob("*.json")):
        spec = json.loads(p.read_text(encoding="utf-8"))
        if not isinstance(spec, dict) or not spec.get("load_cases"):
            continue
        r = Q.rank(spec, UNKNOWNS)
        row = {"brief": p.stem, "asked": [x["name"] for x in r["ask"]], "defaulted": r["use_defaults_for"],
               "effects": {x["name"]: None if x["effect"] is None else round(x["effect"], 3) for x in r["all"]}}
        rows.append(row)
        print(json.dumps(row), flush=True)
    asked = [len(r["asked"]) for r in rows]
    out = {"briefs": len(rows), "threshold": 0.10, "max_questions": 3, "questions_asked": asked, "unknowns_offered": len(UNKNOWNS), "rows": rows,
           "note": "Effect = relative change in safety factor between an unknown's two cases on the family's default design, 6 mm mesh. "
                   "Not the change in mass at the required safety factor that the plan names, and no user has answered these questions. "
                   "All six briefs give the same ranking: they share a family and its default design, and in a linear analysis the relative effect does not depend on the load. "
                   "So this shows the tool runs, not that it tells briefs apart. Ranking on safety factor alone also misses stiffness: the sideways load moves it 2% but moves displacement 17%."}
    Path(a.out).write_text(json.dumps(out, indent=1) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
