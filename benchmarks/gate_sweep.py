"""B4-T1 sweep: 200 random in-bounds bracket designs through the gate; pass rate and reasons to gate_sweep.json.

Usage (from the repository root): micromamba run -n attome-ae python benchmarks/gate_sweep.py [--n 200]
"""
import argparse
import collections
import json
import random
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from structura.geometry.gate import GateLimits, gate  # noqa: E402
from structura.grammar.compile import CompileError  # noqa: E402
from structura.templates.base import Template  # noqa: E402


def random_params(t: Template, rng: random.Random) -> dict:
    out = {}
    for k, p in t.free.items():
        out[k] = float(rng.randint(int(p.low), int(p.high))) if p.integer else rng.uniform(p.low, p.high)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=200)
    ap.add_argument("--family", default="bracket")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    t = Template.load(a.family)
    rng = random.Random(a.seed)
    reasons = collections.Counter()
    rows, passed = [], 0
    t0 = time.perf_counter()
    for i in range(a.n):
        p = random_params(t, rng)
        try:
            part = t.build(p)
        except CompileError as e:
            reasons["compile: " + str(e).split(":")[1].strip()[:60]] += 1
            rows.append({"params": p, "ok": False, "reasons": [str(e)]})
            continue
        r = gate(part.shape, GateLimits(design_space=part.design_space()))
        passed += r.ok
        for x in r.reasons:
            reasons[x.split(" of ")[0] if x.startswith(("wall", "edge", "hole")) else x[:60]] += 1
        rows.append({"params": p, "ok": r.ok, "reasons": r.reasons, "min_wall_mm": r.min_wall_mm})
    out = {"family": a.family, "n": a.n, "seed": a.seed, "passed": passed, "pass_rate": passed / a.n,
           "seconds": round(time.perf_counter() - t0, 1), "reasons": dict(reasons), "designs": rows}
    (ROOT / "benchmarks" / "gate_sweep.json").write_text(json.dumps(out, indent=1))
    print(f"{passed}/{a.n} passed ({100 * passed / a.n:.1f}%) in {out['seconds']} s; reasons: {dict(reasons)}")


if __name__ == "__main__":
    main()
