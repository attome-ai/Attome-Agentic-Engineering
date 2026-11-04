"""Check the per-feature implementation plans in docs/roadmap/<ID>.json.

A less capable model executes these tasks, so every task must be complete and literal. This script refuses a plan
when a required field is missing, a task id or `after` link is wrong, a `read_first` path does not exist in the repo,
or a step uses vague wording. It prints one line per problem and exits 1 if there is any.

Usage: python docs/tools/check_roadmap_tasks.py [DIR] [ID ...]
"""
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DIR = REPO / "docs" / "roadmap"
TOP = ["id", "size", "summary", "reuse", "prereq", "tasks", "acceptance", "risks"]
TASK = ["id", "area", "task", "where", "size", "read_first", "steps", "verify", "done_when", "do_not", "stop_and_ask_if"]
AREAS = {"CAD", "Mesh", "Solver", "Optimize", "Verify", "Agent/MCP", "UI", "Data", "Tests", "Docs", "Ops", "Hardware"}
SIZES = {"S", "M", "L", "XL"}
VAGUE = re.compile(r"\b(as appropriate|and so on|etc\.?|if needed|where appropriate|as needed|consider|maybe|somehow|TBD|TODO)\b", re.I)
PATH = re.compile(r"^((?:\.claude|structura|dashboard|docs|tests|scripts|benchmarks|experiments|demo)"
                  r"/[^\s§:(,]*)")


def path_exists(p):
    p = p.rstrip("/.")
    if any(ch in p for ch in "*?<>{"):
        return True  # a pattern or placeholder; not checked
    return (REPO / p).exists()


def check(fp, all_task_ids, created):
    probs = []
    try:
        d = json.loads(fp.read_text(encoding="utf-8"))
    except Exception as e:
        return [f"invalid JSON: {e}"]
    fid = fp.stem
    for k in TOP:
        if k not in d or d[k] in (None, "", []) and k not in ("prereq", "reuse"):
            probs.append(f"missing {k}")
    if d.get("id") != fid:
        probs.append(f"id {d.get('id')} does not match file name")
    if d.get("size") not in SIZES:
        probs.append(f"bad overall size {d.get('size')}")
    tasks = d.get("tasks") or []
    if not 1 <= len(tasks) <= 30:
        probs.append(f"{len(tasks)} tasks (expected 1-30)")
    for i, t in enumerate(tasks, 1):
        tid = t.get("id", f"#{i}")
        for k in TASK:
            if not t.get(k):
                probs.append(f"{tid}: missing {k}")
        if t.get("id") and not re.fullmatch(rf"{fid}-T\d+", t["id"]):
            probs.append(f"{tid}: id should be {fid}-T<n>")
        if t.get("area") and t["area"] not in AREAS:
            probs.append(f"{tid}: bad area {t['area']}")
        if t.get("size") and t["size"] not in {"S", "M", "L"}:
            probs.append(f"{tid}: bad size {t['size']} (split XL work)")
        steps = t.get("steps") or []
        lo = 1
        if steps and not lo <= len(steps) <= 14:
            probs.append(f"{tid}: {len(steps)} steps (expected {lo}-14)")
        for s in steps + [t.get("task", ""), t.get("done_when", "")]:
            m = VAGUE.search(s or "")
            if m:
                probs.append(f"{tid}: vague wording '{m.group(0)}' in: {s[:80]}")
        for p in t.get("read_first") or []:
            m = PATH.match(p.strip("`"))
            if m and not path_exists(m.group(1)) and m.group(1).rstrip("/.") not in created:
                probs.append(f"{tid}: read_first path not found: {m.group(1)}")
        for a in t.get("after") or []:
            if a not in all_task_ids:
                probs.append(f"{tid}: after {a} is not a known task id")
    return probs


def main():
    args = sys.argv[1:]
    d = DIR
    if args and Path(args[0]).is_dir():
        d = Path(args.pop(0))
    files = sorted(d.glob("*.json"))
    all_ids, created = set(), set()
    for fp in files:
        try:
            for t in json.loads(fp.read_text(encoding="utf-8")).get("tasks", []):
                all_ids.add(t.get("id"))
                for s in [t.get("where", "")] + list(t.get("steps") or []):
                    created |= {x.rstrip("/.,;:)") for x in re.findall(r"(?:structura|dashboard|docs|tests|scripts|benchmarks|experiments|demo)/[\w./-]+", s or "")}
        except Exception:
            pass
    bad = 0
    for fp in files:
        if args and fp.stem not in args:
            continue
        for p in check(fp, all_ids, created):
            print(f"{fp.stem}: {p}")
            bad += 1
    print(f"checked {len(files)} plans, {bad} problems")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
