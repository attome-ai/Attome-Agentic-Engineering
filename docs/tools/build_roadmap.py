"""Build docs/attome-v2-roadmap.html: the phased plan from today to every feature at its v2 score.

Feature names, scores, design steps, today's state and dependencies come from feature-plan.md; the phases,
their workstreams, goals, demos, risks and exit checks live here. Every feature must sit in exactly one
workstream of one phase (the script refuses otherwise). Ticks are kept in the browser (localStorage, keyed by
item id), so regenerating the page keeps them; use Export/Import to move them. Tasks recorded as built in
`docs/roadmap-done.json` (task id -> date) are ticked for every reader; a browser's own ticks win.

Usage: python docs/tools/build_roadmap.py
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MD = ROOT / "feature-plan.md"
OUT = ROOT / "agentic-engineering-roadmap.html"
IMPL = ROOT / "roadmap"
PROTOCOL = IMPL / "EXECUTOR.md"
SHIPPED = ROOT / "roadmap-done.json"


def slug(t):
    return re.sub(r"-+", "-", re.sub(r"[^a-z0-9]+", "-", t.lower())).strip("-")


def ids(spec):
    """'A1-A15 A17 B3' -> ['A1', ..., 'A15', 'A17', 'B3']"""
    out = []
    for tok in spec.split():
        m = re.fullmatch(r"([A-Z]+)(\d+)-\1?(\d+)", tok)
        if m:
            out += [f"{m.group(1)}{i}" for i in range(int(m.group(2)), int(m.group(3)) + 1)]
        else:
            out.append(tok)
    return out


DONE = [
    "Every design step of the feature is built, or moved to a later phase with a written reason",
    "Tests cover the feature's behaviour, including the failure paths (bad geometry, mesh failure, solver error)",
    "Runs on the Windows demo machine from a clean environment created from environment.yml",
    "The validation suite (F2) is still green after the change",
    "Units are recorded with every number, and nothing is shown as verified unless its check ran",
    "Visible in the dashboard where the feature has output a user can see",
    "Documented in docs/ and in the README's feature list",
    "Runtime of the feature's main path measured and written down",
]

PHASES = [
    dict(
        key="P0", title="Set up and measure (4–9 Nov)",
        goal="A clean, reproducible environment on the demo machine, the repository layout, and real solver timings before any feature work.",
        why="Every later number in the proposal depends on the toolchain working on Windows and on measured runtimes, not estimates.",
        demo="One command recreates the environment; a script solves NAFEMS LE10 at three mesh densities and prints error against −5.38 MPa and runtime.",
        risks=[
            "CalculiX on Windows comes only from conda-forge (2.22/2.23); if it fails, fall back to WSL2 the same day instead of building from source.",
            "Licence assumptions from the research are partly from memory (‡); verify each LICENSE file now, before code depends on it.",
        ],
        tasks=[
            "Owner reviews and signs docs/feature-plan.md v0.1",
            "Local clone of attome-ai/Attome-Agentic-Engineering with this docs/ folder committed",
            "environment.yml with Python 3.12, cadquery, build123d, gmsh, calculix, meshio, pyvista, optuna, pymoo, scikit-fem, pytest",
            "Repository layout created: structura/, tests/, benchmarks/, dashboard/, docs/",
        ],
        lanes=[("Validation baseline", "F2"), ("Licences", "J2")],
        exit=[
            "environment.yml recreates a working environment on a clean Windows account",
            "LE10 runtime and error table at three mesh densities committed to docs/",
            "Licence table verified against every repository's LICENSE file",
        ],
    ),
    dict(
        key="P1", title="Feature grammar and physics pipeline (10–30 Nov)",
        goal="The design-graph grammar compiles a bracket into CAD, and it goes through the gate, mesh, CalculiX solve and parsed results with no human step.",
        why="This is the foundation the research says breaks most often (meshing); nothing above it is worth building until it is reliable.",
        demo="Run one command with bracket parameters and get a STEP file, mass, peak von Mises, max displacement and reaction-force balance in under a minute.",
        risks=[
            "gmsh second-order node order differs from CalculiX C3D10; test against the cantilever case before trusting any stress.",
            "Converting meshes through generic formats is slow (FeenoX saw more than five minutes); write the .inp deck directly from gmsh arrays.",
        ],
        tasks=[
            "Grammar compiler builds every first-release feature at its bound corners",
            "Bracket family defined as a design graph runs end to end from a Python call",
            "Pipeline success rate measured over 200 random parameter sets (execute, gate, mesh, solve)",
        ],
        lanes=[("Geometry", "B3 B4"), ("Mesh and solve", "C1 C3 C4"), ("Export", "H2")],
        exit=[
            "Cantilever within 2% of beam theory; LE10 within 2% of −5.38 MPa on the fine mesh",
            "At least 99% of gated bracket designs mesh, and at least 95% of all in-bounds designs mesh and solve",
            "Median coarse-mesh evaluation time recorded",
        ],
    ),
    dict(
        key="P2", title="Optimization and trust (1–12 Dec)",
        goal="A typed, versioned spec with several load cases, worst-case evaluation and a feasibility check drives a multi-objective search that returns a verified Pareto front with a trust record per design.",
        why="Numeric Pareto optimization and visible verification are the two gaps no competitor fills for non-experts.",
        demo="From a spec file: 200 evaluations, a Pareto front of mass vs. safety factor, the chosen design re-solved on a fine mesh, its trust record printed.",
        risks=[
            "Optimization on a mesh that is too coarse ranks designs wrongly; the fine-mesh re-solve of the front shows whether the ranking held.",
            "The spec grows into a general CAE language; keep it to static loads, supports, one material and the template's parameters.",
        ],
        tasks=[
            "Baseline bracket designed by hand and solved, for the comparison",
            "Random search vs. NSGA-II compared on the same budget (hypervolume)",
        ],
        lanes=[("Spec", "A2"), ("Optimization", "D1 D2 D3"), ("Trust", "F1")],
        exit=[
            "Pareto front beats the hand baseline on mass at equal safety factor, or the shortfall is reported",
            "Every delivered design has a trust record with all checks run",
        ],
    ),
    dict(
        key="P3", title="Agent loop and text-to-CAD (13–24 Dec)",
        goal="Claude drives the whole pipeline from a natural-language brief over MCP, repairing its own failures inside a budget.",
        why="Natural-language intent joined to Pareto optimization is the white space; the agent is what makes it usable by a non-expert.",
        demo="In Claude Code: 'design a wall bracket for 100 kg, as light as possible, printed in PETG' ends in a verified design and a short report, with every step logged.",
        risks=[
            "The agent tunes numbers itself instead of calling the optimizer; tool descriptions and the system prompt forbid it and the tests check it.",
            "Cost per study grows with repairs; record tokens per step and route repairs to a cheaper model.",
        ],
        tasks=[
            "Ten scripted briefs run end to end; success rate per stage recorded",
            "Brief benchmark: 50 hand-written briefs with gold specs (English and Arabic, text and image), scored per field",
            "Load-derivation table (structura/loads.py) with a source for every factor",
            "Text2CAD-Bench run through the grammar path; invalid rate, Chamfer distance and IoU recorded",
        ],
        lanes=[("Brief and questions", "A1 A5"), ("Text-to-CAD", "B1"), ("Agent", "E1 E2 E3 E4"), ("Run history", "I1")],
        exit=[
            "At least 8 of 10 scripted briefs finish with a verified design without human help",
            "Brief benchmark: at least 90% of spec fields and 100% of units correct",
            "Every run is reproducible from its stored spec, script and hashes",
        ],
    ),
    dict(
        key="P4", title="Visualization (26 Dec – 4 Jan)",
        goal="A browser dashboard where design and simulation are easy to see: fields on the 3D part, live history, a clickable Pareto front, before/after and the agent timeline.",
        why="The owner's focus and the judges' first impression; research systems show only static figures and vendors hide their viewers behind demos.",
        demo="Open the dashboard, start a brief, watch the history fill, click a Pareto point, see its stress field and trust record, compare it with the baseline.",
        risks=[
            "Sending full volume meshes to the browser is slow; export only the skin mesh with per-vertex scalars.",
            "The phase is ten days because the grammar week moved into P1; build the views in order G1, G4, G5, G3, G6, G2 and stop polishing at the phase end.",
        ],
        tasks=[
            "Dashboard served locally with one command",
        ],
        lanes=[("Viewer", "G1 G2"), ("Explorers", "G3 G4 G5 G6")],
        exit=[
            "A person who has not seen the project can explain the chosen design's trade-off from the dashboard alone",
            "Viewer loads a 200k-triangle field in under one second and orbits at 60 frames per second on the demo machine",
        ],
    ),
    dict(
        key="P5", title="Proposal (5–10 Jan, due 10 Jan)",
        goal="Submit the proposal with measured results, a physical test and a short demo video.",
        why="The proposal is judged on evidence; every number in it must come from P0–P4 runs.",
        demo="The submitted proposal PDF and a three-minute video.",
        risks=[
            "Printing and testing take longer than planned; print both parts in P2 and test them in P4, leaving only the write-up here.",
            "Overclaiming novelty; use the research's prior-art list and phrase the claim as 'we found no shipped product that…'.",
        ],
        tasks=[
            "Proposal sections written: problem, solution, technical specifications, feasibility, prototype",
            "Benchmark run on Hephaestus-CCX if its kit is public, reported by stage",
            "Prior-art section cites both May 2026 papers and SimScale",
            "Three-minute demo video recorded",
            "Proposal submitted before 10 January 2027",
        ],
        lanes=[("Physical test", "H3"), ("Cost story", "J1")],
        exit=[
            "Proposal submitted, with every number traceable to a stored run",
        ],
    ),
    dict(
        key="P6", title="Thermal and improve-an-existing-part (11–24 Jan)",
        goal="Second physics on the same pipeline (heat sink), redesign of an uploaded part, the gated free-form fallback, print rules and the surrogate.",
        why="Thermal on the same parametric model and 'improve my part' are the two capabilities that turn one demo into a platform story.",
        demo="Upload a bracket STEP and get a lighter redesign with the same holes; run a heat-sink brief and get a mass vs. peak-temperature front.",
        risks=[
            "A fixed film coefficient can be challenged by judges; state its source and present results as relative improvement.",
            "Fitting a template to arbitrary STEP files is open-ended; support only parts whose family has a template and say so.",
        ],
        tasks=[
            "Heat-sink thermal validation case passes against the analytical slab",
        ],
        lanes=[("Thermal", "C2"), ("Improve existing", "A3"), ("Print rules", "H1"),
               ("API", "I2"), ("Surrogate", "C6")],
        exit=[
            "Two demo briefs (bracket, heat sink) and one upload-and-improve run finish end to end",
        ],
    ),
    dict(
        key="P7", title="Demo hardening (25 Jan – 3 Feb, demo 3 Feb)",
        goal="The live demo cannot fail: cached runs, offline fallback, rehearsed script and answers to likely questions.",
        why="Presentation is 10% of the score and a failed live run costs far more than that.",
        demo="Full rehearsal on the demo machine with Wi-Fi off, using cached results where the live LLM is unavailable.",
        risks=[
            "Venue network fails; the content-hash cache replays the scripted brief and the dashboard works offline.",
        ],
        tasks=[
            "Scripted demo brief precomputed and cached",
            "Offline mode tested with network disabled",
            "Backup video on a USB stick",
            "Printed baseline and optimized parts packed with the test plot",
            "Judge Q&A sheet: prior art, verification, licences, limits",
            "Three full rehearsals timed",
        ],
        lanes=[],
        exit=[
            "Two consecutive rehearsals finish on time with no live failure",
        ],
    ),
    dict(
        key="P8", title="After the competition: up the factory ladder",
        goal="From components to assemblies, products, manufacturing processes and finally the factory, reusing the same spec → generate → simulate → optimize → verify core.",
        why="The long-term ambition is an AI-designed factory; each level reuses the one below, so the order matters.",
        demo="Defined per level when the level is planned.",
        risks=[
            "Starting a higher level before the lower one is reliable; each level starts only when the previous level's exit checks pass.",
        ],
        tasks=[
            "Level 2 plan: assemblies with fasteners, contact and modal analysis",
            "Level 3 plan: product-level multiphysics with CFD (C5)",
            "Level 4 plan: manufacturing process design (toolpaths, print time, cost)",
            "Level 5 plan: factory layout, throughput and energy (discrete-event simulation)",
        ],
        lanes=[("Native CAD", "A4"), ("Later physics and geometry", "C5 B2")],
        exit=[
            "Each level has its own signed feature plan before work starts",
        ],
    ),
]



# Workload model. Developer-days before 3 Feb 2027 per feature (AI-assisted: Claude writes most code, the owner
# reviews, runs and tests). Work a feature's plan marks "after 3 Feb" is not counted. Estimates, not measurements.
EFFORT = {
    "F2": 5, "J2": 2,
    "B3": 7, "B4": 4, "C1": 6, "C3": 3, "C4": 4, "H2": 2,
    "A2": 5, "D1": 5, "D2": 5, "D3": 4, "F1": 5,
    "A1": 7, "A5": 3, "B1": 5, "E1": 5, "E2": 3, "E3": 3, "E4": 2, "I1": 4,
    "G1": 4, "G2": 3, "G3": 4, "G4": 3, "G5": 2, "G6": 3,
    "H3": 5, "J1": 3,
    "C2": 4, "A3": 5, "H1": 5, "I2": 3, "C6": 3,
    "A4": 0, "B2": 0, "C5": 0,
}
# Working days available per phase (6 working days a week, full time) and days for the phase's own tasks.
CAPACITY = {"P0": 5, "P1": 18, "P2": 10, "P3": 10, "P4": 8, "P5": 5, "P6": 12, "P7": 8, "P8": 0}
TASK_DAYS = {"P0": 1, "P1": 1, "P2": 1, "P3": 1, "P4": 0.5, "P5": 3, "P6": 0, "P7": 5, "P8": 0}

SKIP_STEPS = ("Why", "Performance")


def load_features():
    text = MD.read_text(encoding="utf-8")
    secs = re.findall(r"^### ([A-Z]+\d+)\. (.*?)\n(.*?)(?=^### |^## |\Z)", text, re.M | re.S)
    known = {s[0] for s in secs}
    feats = {}
    for fid, name, body in secs:
        v2 = int(re.search(r"\*\*Attome AE:\*\* (\d+)", body).group(1))
        t = re.search(r"\*\*Today \(score (\d+)\)", body)
        comp = [int(x) for x in re.findall(r"(\d+)",
                                            re.search(r"\*\*Competitors:\*\*(.*)", body).group(1))]
        plan = body.split("**Today")[0]
        steps = []
        for title, rest in re.findall(r"^- \*\*(.+?)\*\*\s*(.*)$", plan, re.M):
            title = title.rstrip(".")
            if title.startswith(SKIP_STEPS):
                continue
            rest = re.sub(r"`([^`]+)`", r"\1", rest)
            steps.append([title, rest if len(rest) <= 260 else rest[:257].rsplit(" ", 1)[0] + "…"])
        if not steps:
            para = re.search(r"\*\*Plan:\*\*\s*(.*?)(?=\n\n|\Z)", plan, re.S)
            sents = re.split(r"(?<=[.;])\s+(?=[A-Z])", re.sub(r"`([^`]+)`", r"\1", para.group(1))) if para else []
            for s in sents:
                if s.startswith("Designed") or len(s) < 25:
                    continue
                steps.append([s.split(",")[0].split(":")[0][:60].rstrip("."), s if len(s) <= 260 else s[:257].rsplit(" ", 1)[0] + "…"])
        deps = sorted({x for x in re.findall(r"\b([A-Z]{1,2}\d{1,2})\b(?!-D)", plan) if x in known and x != fid},
                      key=lambda x: (re.match(r"[A-Z]+", x).group(0), int(re.search(r"\d+", x).group(0))))
        today = ""
        tm = re.search(r"\*\*Today \(score \d+\):\*\*\s*\n+((?:- .*\n?)+)", body)
        if tm:
            first = [ln[2:] for ln in tm.group(1).splitlines() if ln.startswith("- ")]
            today = re.sub(r"`([^`]+)`", r"\1", first[0]) if first else ""
            if len(today) > 240:
                today = today[:237].rsplit(" ", 1)[0] + "…"
        letter = re.match(r"[A-Z]+", fid).group(0)
        feats[fid] = dict(name=name, v2=v2, today=int(t.group(1)) if t else 0, best=max(comp),
                          anchor=f"{letter.lower()}-{slug(name)}", steps=steps, deps=deps, now=today)
    return feats


def main():
    feats = load_features()
    seen = {}
    for pi, p in enumerate(PHASES):
        p["lanes_out"] = []
        for lname, spec in p["lanes"]:
            lst = ids(spec)
            for f in lst:
                if f not in feats:
                    sys.exit(f"{p['key']}/{lname}: unknown feature {f}")
                if f in seen:
                    sys.exit(f"{f} is in both {seen[f][1]} and {p['key']}")
                seen[f] = (pi, p["key"])
            p["lanes_out"].append([lname, lst])
    missing = [f for f in feats if f not in seen]
    if missing:
        sys.exit(f"features in no phase: {' '.join(missing)}")
    for f, d in feats.items():
        d["phase"] = seen[f][0]
    impl = {}
    for fp in sorted(IMPL.glob("*.json")):
        try:
            impl[fp.stem] = json.loads(fp.read_text(encoding="utf-8"))
        except Exception as e:
            sys.exit(f"{fp.name}: {e}")
    unknown = [k for k in impl if k not in feats]
    if unknown:
        sys.exit(f"implementation plans for unknown features: {' '.join(unknown)}")
    days = {"S": 2, "M": 7, "L": 25}
    efforts = []
    for p in PHASES:
        tot = sum(days.get(t.get("size"), 0) for _, lst in p["lanes_out"] for f in lst for t in impl.get(f, {}).get("tasks", []))
        planned = sum(1 for _, lst in p["lanes_out"] for f in lst if f in impl)
        efforts.append([round(tot / 5), planned])
    missing_effort = [f for f in feats if f not in EFFORT]
    if missing_effort:
        sys.exit(f"features with no effort estimate: {' '.join(missing_effort)}")
    loads = [sum(EFFORT[f] for _, lst in p["lanes_out"] for f in lst) + TASK_DAYS[p["key"]] for p in PHASES]
    data = {
        "phases": [dict(key=p["key"], title=p["title"], goal=p["goal"], why=p["why"], demo=p["demo"],
                        risks=p["risks"], tasks=p["tasks"], exit=p["exit"], lanes=p["lanes_out"],
                        effort=efforts[i], load=loads[i], days=CAPACITY[p["key"]]) for i, p in enumerate(PHASES)],
        "features": feats,
        "impl": impl,
        "done": DONE,
        "shipped": json.loads(SHIPPED.read_text(encoding="utf-8")) if SHIPPED.exists() else {},
        "protocol": PROTOCOL.read_text(encoding="utf-8") if PROTOCOL.exists() else "",
    }
    page = TEMPLATE.replace("/*DATA*/null", json.dumps(data, ensure_ascii=False).replace("</", "<\\/"))
    OUT.write_text(page, encoding="utf-8", newline="\n")
    nsteps = sum(len(d["steps"]) for d in feats.values())
    late = sum(1 for d in feats.values() for x in d["deps"] if feats[x]["phase"] > d["phase"])
    ntasks = sum(len(v.get("tasks", [])) for v in impl.values())
    print(f"wrote {OUT.name}: {len(PHASES)} phases, {len(feats)} features, {nsteps} design steps, "
          f"{late} links to a later-phase feature, {len(impl)} implementation plans with {ntasks} tasks")


TEMPLATE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Attome AE Roadmap</title>
<style>
:root{--bg:#f7f7f5;--panel:#fff;--ink:#1d1d1b;--muted:#6b6b66;--line:#e2e1dc;--accent:#2f6f5e;--accent-soft:#e3efe9;--done:#8a8a84;--bar:#d9d8d2;--warn:#a2531b;--warn-soft:#f6e8dc;--chip:#efeee9}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#151614;--panel:#1d1f1c;--ink:#ecebe6;--muted:#a3a29b;--line:#30322e;--accent:#6fbfa5;--accent-soft:#22352e;--done:#7c7b75;--bar:#33352f;--warn:#e3a26f;--warn-soft:#3a2a1d;--chip:#2a2c28}}
:root[data-theme="dark"]{--bg:#151614;--panel:#1d1f1c;--ink:#ecebe6;--muted:#a3a29b;--line:#30322e;--accent:#6fbfa5;--accent-soft:#22352e;--done:#7c7b75;--bar:#33352f;--warn:#e3a26f;--warn-soft:#3a2a1d;--chip:#2a2c28}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,"Noto Sans Arabic",sans-serif}
main{max-width:1040px;margin:0 auto;padding:24px 16px 80px}
h1{font-size:26px;margin:0 0 4px}
.sub{color:var(--muted);margin:0 0 16px}
.summary{position:sticky;top:0;z-index:5;background:var(--bg);padding:12px 0;border-bottom:1px solid var(--line);margin-bottom:16px}
.stats{display:flex;flex-wrap:wrap;gap:8px 24px;align-items:baseline}
.stat b{font-size:22px}
.stat span{color:var(--muted);font-size:13px;margin-left:4px}
.bar{height:8px;background:var(--bar);border-radius:4px;overflow:hidden;margin-top:8px}
.bar i{display:block;height:100%;background:var(--accent);width:0;transition:width .2s}
.tools{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
button,.btn{font:inherit;font-size:13px;padding:5px 10px;border:1px solid var(--line);border-radius:6px;background:var(--panel);color:var(--ink);cursor:pointer}
button:hover,.btn:hover{border-color:var(--accent)}
input[type=search]{font:inherit;font-size:13px;padding:5px 10px;border:1px solid var(--line);border-radius:6px;background:var(--panel);color:var(--ink);min-width:200px;flex:1}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;margin:0 0 14px;overflow:hidden}
.card>summary{list-style:none;cursor:pointer;padding:14px 16px;display:grid;grid-template-columns:auto 1fr auto;gap:4px 12px;align-items:center}
.card>summary::-webkit-details-marker{display:none}
.pk{font-weight:700;color:var(--accent);font-size:14px}
.pt{font-weight:650;font-size:17px}
.pc{color:var(--muted);font-size:13px;white-space:nowrap}
.pbar{grid-column:1/-1}
.body{padding:0 16px 16px}
.goal{margin:0 0 6px}
.why{color:var(--muted);margin:0 0 8px;font-size:14px}
.demo{background:var(--accent-soft);border-radius:8px;padding:8px 12px;font-size:14px;margin:8px 0}
.risks{font-size:14px;margin:6px 0 0;padding-left:18px;list-style:disc;color:var(--ink)}
.risks li{margin:2px 0}
h3{font-size:13px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);margin:18px 0 6px}
h4{font-size:14px;margin:14px 0 4px;display:flex;gap:8px;align-items:baseline}
h4 small{color:var(--muted);font-weight:400}
ul.ck{list-style:none;margin:0;padding:0}
.row{display:flex;gap:10px;align-items:flex-start;padding:5px 4px;border-radius:6px}
.row:hover{background:var(--accent-soft)}
.row label{display:flex;gap:10px;align-items:flex-start;flex:1;cursor:pointer;min-width:0}
input[type=checkbox]{margin-top:3px;width:16px;height:16px;accent-color:var(--accent);flex:none}
.done>.row .txt{color:var(--done);text-decoration:line-through}
.fid{font-weight:650;min-width:44px;display:inline-block}
.sc{color:var(--muted);font-size:12px;white-space:nowrap;padding-left:8px}
.more{font-size:12px;padding:2px 8px;white-space:nowrap}
.detail{margin:2px 0 8px 30px;padding:8px 12px;border-left:2px solid var(--line);font-size:14px}
.detail p{margin:4px 0}
.steps .row{padding:3px 4px}
.steps .txt b{font-weight:600}
.chips{display:flex;flex-wrap:wrap;gap:4px;margin:4px 0}
.chip{font-size:12px;padding:1px 7px;border-radius:10px;background:var(--chip);color:var(--ink);text-decoration:none}
.chip.late{background:var(--warn-soft);color:var(--warn)}
.lbl{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.04em}
a{color:var(--accent)}
.hidden{display:none!important}
.note{color:var(--muted);font-size:13px;margin-top:24px}
.dod li{margin:3px 0}
.tag{font-size:11px;color:var(--muted);border:1px solid var(--line);border-radius:4px;padding:0 4px;margin-right:6px;white-space:nowrap}
.where{display:block;font-size:12px;color:var(--muted)}
.tbody{margin:2px 0 10px 30px;padding:8px 12px;border-left:2px solid var(--accent);font-size:14px;background:var(--panel)}
.tbody ol{margin:4px 0;padding-left:22px}.tbody ol li{margin:3px 0}
pre{background:var(--chip);padding:8px 10px;border-radius:6px;overflow-x:auto;font-size:12.5px;white-space:pre-wrap;word-break:break-word}
code{font-size:.92em;background:var(--chip);padding:0 3px;border-radius:3px;word-break:break-word}
.md ol{padding-left:22px}.md p{margin:6px 0}
.sub2{margin:8px 0}.sub2>summary{cursor:pointer}
.warn{color:var(--warn)}
#next{background:var(--accent);color:var(--bg);border-color:var(--accent);font-weight:600}
@media (max-width:600px){.card>summary{grid-template-columns:auto 1fr}.pc{grid-column:1/-1}.sc{display:none}.detail{margin-left:8px}}
.wl{display:inline-block;margin-left:6px;padding:1px 8px;border-radius:999px;font-size:.85em;border:1px solid var(--line)}.wl-over{color:var(--warn);background:var(--warn-soft);font-weight:600}.wl-ok{color:var(--accent)}.tblw{overflow-x:auto}.wlt{border-collapse:collapse;width:100%;font-size:.92em}.wlt th,.wlt td{padding:6px 8px;border-bottom:1px solid var(--line);text-align:left}.wlt .n{text-align:right;white-space:nowrap}.wlt tr.tot td{font-weight:600}
</style>
</head>
<body>
<main>
<h1>Attome AE roadmap</h1>
<p class="sub">From today to every feature at its designed score, in twelve phases. Each phase has workstreams, a demo, risks and exit checks. Each feature opens to its implementation plan: ordered tasks with files to read, numbered steps, the command that proves it works, what not to do and when to stop and ask. The plan is written to be executed by a model that follows it literally; read the executor protocol first. Ticks stay in this browser; use Export to keep or share them.</p>
<div class="summary">
  <div class="stats">
    <div class="stat"><b id="pct">0%</b><span>done</span></div>
    <div class="stat"><b id="cnt">0</b><span id="cntof">of 0 items</span></div>
    <div class="stat"><b id="score">0</b><span>Attome AE score now (ticked features at their target, the rest at today's score)</span></div>
  </div>
  <div class="bar"><i id="pbar"></i></div>
  <div class="tools">
    <button type="button" id="next">Next task &#9656;</button>
    <input type="search" id="q" placeholder="Find a feature or task, e.g. ZATCA, forecast, Q14">
    <button type="button" id="open">Expand all phases</button>
    <button type="button" id="hide">Hide done</button>
    <button type="button" id="exp">Export ticks</button>
    <label class="btn">Import ticks<input type="file" id="imp" accept="application/json" class="hidden"></label>
    <button type="button" id="theme">Theme</button>
  </div>
  <p class="why" id="nextinfo"></p>
</div>
<details class="card" id="proto"><summary><span class="pk">READ</span><span class="pt">Executor protocol (rules for whoever does the work)</span><span class="pc"></span></summary><div class="body md" id="protobody"></div></details>
<details class="card" id="dod"><summary><span class="pk">DoD</span><span class="pt">Definition of done for every feature</span><span class="pc"></span></summary><div class="body"><ul class="risks dod" id="dodlist"></ul><p class="why">How the page counts: progress counts implementation tasks, features, phase tasks and exit checks. Design steps are reference and do not count. Linked features are the other features a design names; an orange chip means that feature is planned for a later phase, so build only what this feature needs from it now. Workload per phase is the sum of each feature's estimated developer-days before 3 February plus the phase's own tasks; see the Load card.</p></div></details>
<div id="workload"></div>
<div id="phases"></div>
<p class="note">Generated from <code>feature-plan.md</code> and <code>roadmap/*.json</code> by <code>tools/build_roadmap.py</code>; the plans are checked by <code>tools/check_roadmap_tasks.py</code>. Each feature links to its full design in <code>agentic-engineering-feature-matrix.html</code>.</p>
</main>
<script>
const DATA=/*DATA*/null;
const KEY="attome-ae-roadmap";
let ticks={};
try{ticks=JSON.parse(localStorage.getItem(KEY)||"{}")||{}}catch(e){ticks={}}
for(const [id,date] of Object.entries(DATA.shipped||{})){if(!(id in ticks))ticks[id]=date}
function save(){try{localStorage.setItem(KEY,JSON.stringify(ticks))}catch(e){}}
const esc=s=>String(s==null?"":s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
const code=s=>esc(s).replace(/`([^`]+)`/g,"<code>$1</code>");
let hideDone=false;
function md(t){
  const out=[];let list=null,inCode=false,buf=[];
  const close=()=>{if(list){out.push(`</${list}>`);list=null}};
  for(const ln of t.split("\n")){
    if(ln.startsWith("```")){if(inCode){out.push(`<pre>${esc(buf.join("\n"))}</pre>`);buf=[];inCode=false}else{close();inCode=true}continue}
    if(inCode){buf.push(ln);continue}
    let m;
    if((m=ln.match(/^#{1,3} (.*)/))){close();out.push(`<h3>${code(m[1])}</h3>`)}
    else if((m=ln.match(/^\d+\. (.*)/))){if(list!=="ol"){close();out.push("<ol>");list="ol"}out.push(`<li>${code(m[1])}</li>`)}
    else if((m=ln.match(/^\s*- (.*)/))){if(list!=="ul"){close();out.push('<ul class="risks">');list="ul"}out.push(`<li>${code(m[1])}</li>`)}
    else if(ln.trim()===""){close()}
    else{if(list){out[out.length-1]=out[out.length-1].replace(/<\/li>$/," "+code(ln.trim())+"</li>")}else out.push(`<p>${code(ln)}</p>`)}
  }
  close();return out.join("");
}
function li(id,label,text,extra,cls){
  const on=!!ticks[id];
  return `<li data-id="${esc(id)}" data-text="${esc(text.toLowerCase())}" class="${cls||""} ${on?"done":""}"><div class="row"><label><input type="checkbox" ${on?"checked":""}><span class="txt">${label}</span></label>${extra||""}</div></li>`;
}
const chip=(x,d)=>{const f=DATA.features[x];if(!f)return `<span class="chip">${esc(x)}</span>`;const late=d&&f.phase>d.phase;return `<a class="chip ${late?"late":""}" href="#f-${x}" title="${esc(f.name)}">${esc(x)} · ${esc(DATA.phases[f.phase].key)}</a>`};
const tchip=t=>`<a class="chip" href="#t-${esc(t)}">${esc(t)}${ticks[t]?" ✓":""}</a>`;
function taskBody(t){
  const L=(a)=>(a||[]).map(x=>`<li>${code(x)}</li>`).join("");
  return `<div class="tbody">${t.after&&t.after.length?`<p class="lbl">Do first</p><div class="chips">${t.after.map(tchip).join("")}</div>`:""}<p class="lbl">Read first</p><ul class="risks">${L(t.read_first)}</ul><p class="lbl">Steps</p><ol>${(t.steps||[]).map(s=>`<li>${code(String(s).replace(/^\d+\.\s*/,""))}</li>`).join("")}</ol><p class="lbl">Verify</p><pre>${esc((t.verify||[]).join("\n"))}</pre><p><span class="lbl">Done when</span> ${code(t.done_when)}</p><p class="lbl">Do not</p><ul class="risks">${L(t.do_not)}</ul><p><span class="lbl">Stop and ask if</span> ${code(t.stop_and_ask_if)}</p></div>`;
}
function implBlock(id){
  const p=DATA.impl[id];const d=DATA.features[id];
  if(!p)return `<p class="warn">Implementation plan not written yet.</p>`;
  const tasks=(p.tasks||[]).map(t=>li(t.id,`<span class="fid" id="t-${esc(t.id)}">${esc(t.id.split("-").pop())}</span><span class="tag">${esc(t.area)} · ${esc(t.size)}</span> ${code(t.task)}<span class="where">${code(t.where)}</span>`,t.id+" "+t.task,`<button type="button" class="more" data-tb="${esc(t.id)}">steps ▾</button>`,"task")).join("");
  return `<p><span class="lbl">Plan · size ${esc(p.size)}</span><br>${code(p.summary)}</p>${p.prereq&&p.prereq.length?`<p class="lbl">Must exist first</p><div class="chips">${p.prereq.map(x=>chip(x,d)).join("")}</div>`:""}${p.reuse&&p.reuse.length?`<p class="lbl">Reuse from today's code</p><ul class="risks">${p.reuse.map(x=>`<li>${code(x)}</li>`).join("")}</ul>`:""}<p class="lbl">Tasks in order (${(p.tasks||[]).length})</p><ul class="ck">${tasks}</ul><p class="lbl">Acceptance</p><ul class="risks">${(p.acceptance||[]).map(x=>`<li>${code(x)}</li>`).join("")}</ul><p class="lbl">Risks</p><ul class="risks">${(p.risks||[]).map(x=>`<li>${code(x)}</li>`).join("")}</ul>`;
}
function featureDetail(id){
  const d=DATA.features[id];
  const steps=d.steps.map(s=>`<li><b>${esc(s[0])}.</b> ${esc(s[1])}</li>`).join("");
  const deps=d.deps.map(x=>chip(x,d)).join("");
  return `<div class="detail">${d.now?`<p><span class="lbl">Today</span> ${esc(d.now)}</p>`:""}${implBlock(id)}${steps?`<details class="sub2"><summary class="lbl">Design steps from the feature plan (${d.steps.length})</summary><ul class="risks">${steps}</ul></details>`:""}${deps?`<details class="sub2"><summary class="lbl">Linked features (${d.deps.length})</summary><div class="chips">${deps}</div></details>`:""}<p><a href="agentic-engineering-feature-matrix.html#${d.anchor}" target="_blank" rel="noopener">Full design and how the leaders do it</a></p></div>`;
}
const tasksOf=id=>(DATA.impl[id]&&DATA.impl[id].tasks)||[];
const tasksDone=id=>tasksOf(id).filter(t=>ticks[t.id]).length;
function featureRow(id){
  const d=DATA.features[id];const n=tasksOf(id).length;const p=DATA.impl[id];
  const text=id+" "+d.name+" "+tasksOf(id).map(t=>t.task).join(" ");
  const extra=`<span class="sc">${d.today} → ${d.v2}${p?` · ${esc(p.size)}`:""}</span><button type="button" class="more" data-more="${id}">${n?`<span data-td="${id}">${tasksDone(id)}</span>/${n} tasks`:"plan"} ▾</button>`;
  return li(id,`<span class="fid" id="f-${id}">${esc(id)}</span>${esc(d.name)}`,text,extra,"feat");
}
function phaseItems(p){
  const it=[];p.tasks.forEach((x,i)=>it.push(`${p.key}-t${i}`));
  p.lanes.forEach(([n,ids])=>ids.forEach(f=>{it.push(f);tasksOf(f).forEach(t=>it.push(t.id))}));
  p.exit.forEach((x,i)=>it.push(`${p.key}-x${i}`));return it;
}
function renderWorkload(){
  const ps=DATA.phases.filter(p=>p.days);
  const L=ps.reduce((a,p)=>a+p.load,0), C=ps.reduce((a,p)=>a+p.days,0);
  const rows=ps.map(p=>{const o=p.load-p.days;return `<tr><td>${esc(p.key)}</td><td>${esc(p.title)}</td><td class="n">${p.load}</td><td class="n">${p.days}</td><td class="n ${o>0?"wl-over":"wl-ok"}">${o>0?"+"+o:"fits"}</td></tr>`}).join("");
  const o=L-C;
  document.getElementById("workload").innerHTML=`<details class="card" open><summary><span class="pk">Load</span><span class="pt">Workload to 3 February: about ${L} developer-days for ${C} available${o>0?` · over by ${o} (${Math.round(o/C*100)}%)`:""}</span><span class="pc"></span></summary><div class="body"><div class="tblw"><table class="wlt"><thead><tr><th>Phase</th><th>Title</th><th class="n">Load (days)</th><th class="n">Available</th><th class="n">Balance</th></tr></thead><tbody>${rows}<tr class="tot"><td></td><td>Total before 3 February</td><td class="n">${L}</td><td class="n">${C}</td><td class="n ${o>0?"wl-over":"wl-ok"}">${o>0?"+"+o:"fits"}</td></tr></tbody></table></div><p class="why">Load is the sum of each feature's estimated developer-days before 3 February (AI-assisted: Claude writes most code, the owner reviews, runs and tests) plus the phase's own tasks. Available assumes 6 full working days a week. Work a feature marks "after 3 Feb" is not counted. These are estimates; replace them with measured days as phases finish.</p></div></details>`;
}
function render(){
  renderWorkload();
  document.getElementById("dodlist").innerHTML=DATA.done.map(x=>`<li>${esc(x)}</li>`).join("");
  document.getElementById("protobody").innerHTML=md(DATA.protocol||"");
  const root=document.getElementById("phases");
  root.innerHTML=DATA.phases.map((p,pi)=>{
    const t=p.tasks.map((x,i)=>li(`${p.key}-t${i}`,esc(x),x)).join("");
    const lanes=p.lanes.map(([name,ids])=>`<h4>${esc(name)} <small>${ids.length} features · ${ids.reduce((a,f)=>a+tasksOf(f).length,0)} tasks</small></h4><ul class="ck">${ids.map(featureRow).join("")}</ul>`).join("");
    const x=p.exit.map((e,i)=>li(`${p.key}-x${i}`,esc(e),e)).join("");
    const n=p.lanes.reduce((a,l)=>a+l[1].length,0);
    const over=p.load-p.days;
    const eff=p.days?` <span class="wl ${over>0?"wl-over":"wl-ok"}">Workload: about ${p.load} of ${p.days} working days${over>0?` · over by ${over}`:""}</span>`:(p.load?"":` <span class="wl">After the competition; not counted in the workload.</span>`);
    return `<details class="card phase" data-phase="${pi}" ${pi===0?"open":""}><summary><span class="pk">${esc(p.key)}</span><span class="pt">${esc(p.title)}</span><span class="pc" data-pc></span><div class="bar pbar"><i data-pb></i></div></summary><div class="body"><p class="goal">${esc(p.goal)}</p><p class="why">${esc(p.why)}${eff}</p><div class="demo"><span class="lbl">Demo at the end</span><br>${esc(p.demo)}</div><h3>Risks and how they are handled</h3><ul class="risks">${p.risks.map(r=>`<li>${esc(r)}</li>`).join("")}</ul>${t?`<h3>Phase tasks</h3><ul class="ck">${t}</ul>`:""}<h3>Workstreams (${p.lanes.length}) · ${n} features</h3>${lanes}<h3>Exit checks (owner reviews)</h3><ul class="ck">${x}</ul></div></details>`;
  }).join("");
  wire(root);update();filter();
}
function openFeature(id){
  const el=document.querySelector(`li[data-id="${id}"]`);if(!el)return null;
  const ph=el.closest(".phase");if(ph)ph.open=true;
  if(!el.querySelector(":scope>.detail")){el.insertAdjacentHTML("beforeend",featureDetail(id));wire(el)}
  return el;
}
function openTask(tid){
  const f=tid.replace(/-T\d+$/,"");const fe=openFeature(f);if(!fe)return null;
  const el=fe.querySelector(`li[data-id="${tid}"]`);
  if(el&&!el.querySelector(":scope>.tbody")){const t=tasksOf(f).find(x=>x.id===tid);if(t)el.insertAdjacentHTML("beforeend",taskBody(t))}
  return el;
}
function wire(scope){
  scope.querySelectorAll("li[data-id]").forEach(el=>{
    if(el.dataset.wired)return;el.dataset.wired=1;
    el.querySelector(":scope>.row input").addEventListener("change",e=>{
      const id=el.dataset.id;if(e.target.checked)ticks[id]=new Date().toISOString().slice(0,10);else delete ticks[id];
      el.classList.toggle("done",e.target.checked);save();
      const m=id.match(/^(.+)-T\d+$/);if(m){const s=document.querySelector(`[data-td="${m[1]}"]`);if(s)s.textContent=tasksDone(m[1])}
      update();
    });
  });
  scope.querySelectorAll("[data-more]").forEach(b=>{
    if(b.dataset.wired)return;b.dataset.wired=1;
    b.addEventListener("click",()=>{const el=b.closest("li");const o=el.querySelector(":scope>.detail");if(o){o.remove();return}openFeature(b.dataset.more)});
  });
  scope.querySelectorAll("[data-tb]").forEach(b=>{
    if(b.dataset.wired)return;b.dataset.wired=1;
    b.addEventListener("click",()=>{const el=b.closest("li");const o=el.querySelector(":scope>.tbody");if(o){o.remove();return}openTask(b.dataset.tb)});
  });
}
function update(){
  let all=0,done=0;
  DATA.phases.forEach((p,pi)=>{
    const it=phaseItems(p);const d=it.filter(x=>ticks[x]).length;all+=it.length;done+=d;
    const ph=document.querySelector(`.phase[data-phase="${pi}"]`);
    ph.querySelector("[data-pc]").textContent=`${d} / ${it.length}`;
    ph.querySelector("[data-pb]").style.width=(it.length?100*d/it.length:0)+"%";
  });
  const ids=Object.keys(DATA.features);
  const score=ids.reduce((s,id)=>s+(ticks[id]?DATA.features[id].v2:DATA.features[id].today),0)/ids.length;
  document.getElementById("pct").textContent=Math.round(all?100*done/all:0)+"%";
  document.getElementById("cnt").textContent=done;
  document.getElementById("cntof").textContent=`of ${all} items`;
  document.getElementById("score").textContent=Math.round(score);
  document.getElementById("pbar").style.width=(all?100*done/all:0)+"%";
}
function nextTask(){
  for(const p of DATA.phases){
    for(let i=0;i<p.tasks.length;i++){const id=`${p.key}-t${i}`;if(!ticks[id])return {id,kind:"phase task",text:p.tasks[i]}}
    for(const [n,ids] of p.lanes)for(const f of ids){
      for(const t of tasksOf(f)){if(ticks[t.id])continue;if((t.after||[]).every(a=>ticks[a]))return {id:t.id,kind:"task",text:t.task}}
      if(!ticks[f]&&tasksOf(f).length&&tasksOf(f).every(t=>ticks[t.id]))return {id:f,kind:"feature check: all tasks done, check the Definition of done and acceptance for",text:DATA.features[f].name};
    }
    for(let i=0;i<p.exit.length;i++){const id=`${p.key}-x${i}`;if(!ticks[id])return {id,kind:"exit check (owner reviews)",text:p.exit[i]}}
  }
  return null;
}
function filter(){
  const q=document.getElementById("q").value.trim().toLowerCase();
  document.querySelectorAll(".phase").forEach(ph=>{
    let any=false;
    ph.querySelectorAll("li[data-id]").forEach(el=>{
      if(el.closest(".detail"))return;
      const show=(!q||el.dataset.text.includes(q))&&!(hideDone&&ticks[el.dataset.id]);
      el.classList.toggle("hidden",!show);if(show)any=true;
    });
    ph.querySelectorAll("h4").forEach(h=>{const ul=h.nextElementSibling;h.classList.toggle("hidden",!!ul&&![...ul.children].some(x=>!x.classList.contains("hidden")))});
    ph.classList.toggle("hidden",!!q&&!any);if(q&&any)ph.open=true;
  });
}
function flash(el){if(!el)return;el.scrollIntoView({block:"center"});const r=el.querySelector(".row");if(r)r.animate([{background:"var(--accent-soft)"},{background:"transparent"}],{duration:1600})}
document.getElementById("next").addEventListener("click",()=>{
  const n=nextTask();const info=document.getElementById("nextinfo");
  if(!n){info.textContent="Everything is ticked.";return}
  info.textContent=`Next: ${n.kind} ${n.id}: ${n.text}`;
  let el;if(/-T\d+$/.test(n.id))el=openTask(n.id);else if(DATA.features[n.id])el=openFeature(n.id);else{el=document.querySelector(`li[data-id="${n.id}"]`);if(el)el.closest(".phase").open=true}
  flash(el);
});
document.getElementById("q").addEventListener("input",filter);
document.getElementById("open").addEventListener("click",e=>{const ph=[...document.querySelectorAll(".phase")];const o=!ph.every(p=>p.open);ph.forEach(p=>p.open=o);e.target.textContent=o?"Collapse all phases":"Expand all phases"});
document.getElementById("hide").addEventListener("click",e=>{hideDone=!hideDone;e.target.textContent=hideDone?"Show done":"Hide done";filter()});
document.getElementById("exp").addEventListener("click",()=>{const a=document.createElement("a");a.href=URL.createObjectURL(new Blob([JSON.stringify(ticks,null,1)],{type:"application/json"}));a.download="attome-ae-roadmap-ticks.json";a.click()});
document.getElementById("imp").addEventListener("change",e=>{const f=e.target.files[0];if(!f)return;f.text().then(t=>{try{const o=JSON.parse(t);if(o&&typeof o==="object"){ticks=o;save();render()}}catch(err){}})});
document.getElementById("theme").addEventListener("click",()=>{const r=document.documentElement;const cur=r.dataset.theme||(matchMedia("(prefers-color-scheme: dark)").matches?"dark":"light");r.dataset.theme=cur==="dark"?"light":"dark"});
document.addEventListener("click",e=>{const a=e.target.closest('a[href^="#f-"],a[href^="#t-"]');if(!a)return;e.preventDefault();const h=a.getAttribute("href");flash(h.startsWith("#t-")?openTask(h.slice(3)):openFeature(h.slice(3)))});
render();
</script>
</body>
</html>
"""

if __name__ == "__main__":
    main()
