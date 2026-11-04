# Attome Agentic Engineering: proposal draft (AIDTC 2027)

Draft v0.1, 2026-11-04. Due 10 January 2027; live demo 3 February 2027. Solo entry.

**Rule for this document:** every number comes from a stored run in this repository, and its source is given next to
it (`benchmarks/*.json`, `docs/validation.json`, `docs/benchmarks.md`, `runs/`). Numbers marked **[pending]** have
not been measured yet and must be measured or removed before submission. The command that reproduces each result is
listed in [Evidence index](#evidence-index).

---

## 1. Problem

Designing a mechanical part that is both light and proven safe takes an engineer three separate skills and three
separate tools: CAD to draw it, simulation to check it, and optimization to improve it. Each round trip between them
is manual: redraw, re-mesh, re-run, compare. Small companies and students therefore skip the loop. They over-build
parts "to be safe" (heavy, expensive to print or machine), or they under-build them and find out when the part breaks.

AI tools in 2026 attack pieces of this problem, but none closes the whole loop in a way an engineer can check:

- **Text-to-CAD tools** (Zoo, Adam) produce geometry from a sentence, but do not simulate it.
- **Simulation agents** (SimScale's Engineering AI agent, free since August 2026) set up and explain a simulation
  from a prompt, but do not generate geometry from scratch; their shipped optimization is Bayesian search over
  existing Onshape configurations, with no Pareto front shown
  ([SimScale text-to-CAD blog, Oct 2026](https://www.simscale.com/blog/text-to-cad/)).
- **Physics-AI vendors** (PhysicsX, Neural Concept, Ansys SimAI) predict physics with trained surrogates. They need
  30–100+ customer simulations to train, are sold to enterprises behind demo requests, and publish little accuracy
  evidence ([Ansys SimAI](https://ansys.synopsys.com/products/ai/simai)).
- **The two closest research systems (May 2026)** close the loop on real FEA but are structural-only, run no numeric
  multi-objective optimization, and show results as static figures. Physics-in-the-Loop (Berger et al.) reached the
  target safety-factor band in 59.0% of runs with FEA feedback ([arXiv 2605.19717](https://arxiv.org/html/2605.19717)).
  The Hephaestus-CCX study (Son, Park et al.) found that **none of 400 first attempts** by frontier agents met every
  requirement ([arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)). Both papers also show free-form LLM CAD
  breaking the mesher: Physics-in-the-Loop meshed only 76–88% of generated parts.

The gap is not "an AI that draws parts". It is **a design loop whose answer you can trust**: a brief in plain
language, a checkable list of requirements, real physics on the real geometry, an optimizer that shows the
trade-off, and evidence for every number.

## 2. Solution

Attome Agentic Engineering turns a plain-language brief into an optimized part that has been verified by real
finite-element simulation, and shows the user why it is the right answer.

> "A wall shelf bracket that holds 20 kg, as light as possible, printed in PETG."

The user says this to Claude. The engine's tools, exposed to Claude through the Model Context Protocol (MCP), then:

1. **Turn the brief into a spec card**: numbered requirements, each with its source (the brief, a standard, or a stated
   default), load cases evaluated at the top of their factor range, and the material with its print knock-down. The
   user accepts the card before any optimization runs (`docs/examples/bracket_20kg.json`).
2. **Check feasibility first**: the corners of the design space are evaluated before the search. A brief that no design
   in the family can meet is reported as infeasible, with the nearest relaxation, instead of producing a wrong answer.
3. **Generate geometry from a typed design grammar**: 14 feature types with bounded parameters and named faces, built
   on OpenCASCADE. A geometric gate (valid solid, design space, keep-outs, minimum wall, hole and edge sizes,
   overhang) rejects or repairs a design before it reaches the mesher.
4. **Simulate with a real solver**: gmsh meshes the part into quadratic tetrahedra and CalculiX solves it (static
   stress, buckling, modal, steady thermal). Nothing is predicted by a trained surrogate in the reported results.
5. **Optimize for several objectives at once**: NSGA-II, or Bayesian optimization (BoTorch) when evaluations are
   expensive, chosen from the *measured* cost of an evaluation (decision D5). The result is a Pareto front, for example
   mass against safety factor.
6. **Verify the answer**: the Pareto designs are re-solved on a finer mesh; the recommended design gets a trust
   record (mesh convergence with error bars, equilibrium, energy balance, heat balance, validation suite) and a grade.
7. **Show it**: a single offline HTML report with a 3D viewer of stress or temperature fields, a clickable Pareto
   front, design-vs-baseline comparison, optimization history, the trust record, the agent timeline and the spec card.
   The user can export STEP, STL and VTU (ParaView) files.

The LLM never touches the physics: it composes and steers, while the solver, the gate and the checks are
deterministic tools it cannot edit. This is the design rule both 2026 papers recommend.

### What is new

We found no shipped product or published system that combines, on one open stack:

- natural language → checkable spec → **numeric multi-objective optimization with an interactive Pareto front**;
- **structural and thermal** physics on the same parametric model;
- a **visible trust record** for each recommended design (grid convergence error bars, independent physics checks,
  a standing validation suite, an A/B/C grade);
- laptop operation with **open-source solvers**, no solver licences and no training data.

Each technique exists separately (prior art in [section 6](#6-prior-art)); the contribution is the integrated,
verified, explainable loop.

## 3. Technical specifications

| Layer | Implementation | Licence |
|---|---|---|
| Engine | C++20 library and CLI (`engine/`), 52 C++ test cases | project licence |
| Geometry | OpenCASCADE 7.8.1 (B-rep, STEP/STL) | LGPL 2.1-only with the Open CASCADE exception, linked dynamically |
| Meshing | gmsh 4.15, quadratic tetrahedra, subprocess only | GPL 2.0+ (subprocess, not linked) |
| Solver | CalculiX 2.23 (C3D10 / DC3D10), subprocess only | GPL 2.0+ (subprocess, not linked) |
| Cross-check solver | In-house quadratic-tetrahedron solver on Eigen | MPL 2.0 (Eigen) |
| Optimization | Optuna (NSGA-II/III), Ax/BoTorch (qNEHVI), SALib (Sobol screening) | MIT / MIT / MIT |
| Agent layer | Python MCP server, 18 tools (`structura/agent/mcp_server.py`) | MIT (MCP SDK) |
| Report | Self-contained HTML, Three.js 0.149 vendored, offline | MIT |

Licence details: `docs/licences.md`. Environment: `environment.yml` (conda-forge).

**Units and conventions:** mm, N, MPa, tonne, °C. CalculiX is unit-free, so units are fixed by the deck writer and
recorded in each result.

**Families available today:** wall bracket (`families/bracket.json`, 5 parameters) and plate-fin heat sink
(`families/heatsink.json`, 4 parameters).

**Design rules recorded as decisions** (`docs/decisions.md`): D1 C++ engine; D2 gmsh driven by `.geo` scripts;
D3 mounting holes below the ribs; D4 feasibility checked at the corners; D5 optimizer chosen by measured cost; D6 blunt
rib tips and a stress exclusion zone near load and support faces (Saint-Venant); D7 search on a mesh no coarser than
4 mm.

## 4. Feasibility: measured results

All measurements: development machine (AMD Ryzen 9 9950X3D, 16 cores, 61.6 GB RAM, Windows 11), 2026-11-04.
The 8 GB / 4-core minimum laptop has **[pending]** measurements.

### 4.1 The solver gives the right answers (validation suite)

Source: `docs/validation.md`, `docs/validation.json`; command `structura validate`. **7 of 7 cases pass.**

| Case | Reference | Engine | Error |
|---|---|---|---|
| NAFEMS LE10 thick plate, σyy at D | −5.38 MPa | −5.3726 MPa | +0.14% |
| NAFEMS LE1 elliptic membrane, σyy at D | 92.7 MPa | 92.326 MPa | −0.40% |
| Cantilever tip deflection (Timoshenko) | 0.19196 mm | 0.19044 mm | −0.80% |
| Euler buckling, fixed-free column | 10.795 | 10.801 | +0.06% |
| Cantilever first natural frequency | 835.52 Hz | 833.34 Hz | −0.26% |
| Thermal slab, mid-plane temperature | 50 °C | 50 °C | 0.00% |
| Aluminium fin, tip temperature | 92.296 °C | 92.294 °C | −0.002% |

Independent cross-check: the engine's own solver and CalculiX agree to 6×10⁻⁵ % on the bracket's maximum
displacement (62,448 DOF; `docs/benchmarks.md`, task F2-T4).

### 4.2 Generated geometry meshes reliably

Source: `docs/benchmarks.md` (task B4-T3); command `structura meshsweep --family bracket --n 500 --seed 2 --size 4`.

| Stage | Result |
|---|---|
| Random in-bounds designs | 500 |
| Pass the gate | 499 (99.8%); the rejected one has 0.02 mm gaps between ribs, a real defect |
| Gated designs that mesh | 499 of 499 (100%) |

For comparison, Physics-in-the-Loop meshed 76–88% of its free-form LLM-generated parts. The difference comes from the
typed grammar and the gate, not from a better mesher.

### 4.3 End to end: the 20 kg bracket brief

Source: `benchmarks/p2_demo.json`, `runs/p2_demo/`; command `python benchmarks/p2_demo.py`. About 4.5 minutes in
total, 200 evaluations (NSGA-II, chosen from the measured 4.2 s per evaluation).

| | Mass | Safety factor | Max displacement | Requirements |
|---|---|---|---|---|
| Hand baseline (6 mm plate, 1 rib) | 96.3 g | 0.37 | 4.96 mm | fails R1 (SF ≥ 2) and R2 (≤ 3 mm) |
| Recommended design (trial 183: 11.8 mm plate, 2 ribs of 7.7 mm) | 234.1 g | 2.06 ± 1.5% | 0.78 ± 0.3% mm | all pass, **trust grade A** |

What this shows, stated honestly: the hand design is not a lighter version of a safe design, it is **unsafe** by a
factor of 5. The system finds the lightest design in the family that is *proven* to meet the brief; the error bars come
from a three-level grid convergence study (Roache's GCI), and the margin on R1 (3.2%) exceeds its numerical error
(1.5%), so the requirement is proven rather than merely met. 3 of the 4 Pareto designs stayed feasible on the finer
mesh, and their ranking did not change.

Converged (decision D8): the same run with 400 evaluations found 233.6 g (−0.2%); that design meets the brief but
only at grade B (its safety-factor margin, 1.3%, is smaller than its numerical error), so the proven 234.1 g design
stays the recommendation (`benchmarks/p2_demo_400.json`).

### 4.4 Second physics on the same pipeline: a fan-cooled 10 W heat sink

Source: `benchmarks/heatsink_fan_demo_300.json` (converged, checked by `benchmarks/heatsink_fan_demo_600.json`),
`runs/heatsink_fan_demo_300/`; command `python benchmarks/heatsink_demo.py --spec docs/examples/heatsink_10w_fan.json
--name heatsink_fan_demo_300 --evals 300`. About 8 minutes.

| | Mass | Peak temperature | Requirement (≤ 75 °C) |
|---|---|---|---|
| Hand baseline (4 mm base, 8 fins 2 × 25 mm) | 103.7 g | 48.4 °C | pass, large margin |
| Optimized (2.0 mm base, 5 fins 1.3 × 13.8 mm) | **34.4 g (−67%)** | 71.3 °C | pass, **trust grade A** |

Convection is not a guessed number: the coefficient comes from the plate-fin channel correlation of Teertstra et al.
(1999), recomputed for every design from its fin spacing, at a stated fan velocity of 1.5 m/s (to be confirmed
against a fan datasheet). 27 of 27 Pareto designs stayed feasible on the finer mesh; the heat balance closes to
better than 10⁻⁷.

**Convergence (D8).** 150 evaluations gave 55.8 g, 300 gave 34.4 g and 600 gave 34.6 g. The 150-evaluation design
was verified and still 62% heavier than the optimum: verification proves a design meets the brief, not that the search
found the best one, so every headline result is re-run at double the budget. An earlier run with a fixed coefficient
of 25 W/m²K gave 44.3 g at 150 and 42.0 g at 300 evaluations (not converged, superseded by the correlation).

### 4.5 Infeasible briefs are reported, not faked

Source: `benchmarks/bracket_100kg_demo.json`, `benchmarks/bracket_100kg_feasibility.json`; commands in the
[Evidence index](#evidence-index).

The same bracket brief at 100 kg (PETG, SF ≥ 2) has **no feasible design** in the family:

| Check | Result |
|---|---|
| Hand baseline | SF 0.07, 24.8 mm deflection |
| 200 optimizer evaluations | 0 feasible; best SF 0.55 (324 g) |
| Feasibility check (strongest corner of the design space: 12 mm plate, 80 mm wide, 3 ribs of 10 mm) | SF 0.67, so no design in the bounds can reach 2 |

The engine reports this *before* spending an optimization budget (the corner check takes seconds) and the agent
proposes what the user can change: the strongest design meets SF 2 at **33% of the load (about 33 kg)**, or the
material, the bounds or the family can change. It never returns a design that fails the brief. The corner check is
also a tighter bound than the search: the optimizer, trading mass against the 250 g cap, never reached the 354 g corner.

### 4.6 Choosing the optimizer by measured cost

Source: `docs/benchmarks.md` (task D1-T3), `benchmarks/optimizer_compare_at40.json`.

| Method | Seeds | Hypervolume at 40 solver calls | Wall clock |
|---|---|---|---|
| Random search | 3 | 566 | 18 s |
| NSGA-II | 3 | 573 | 15 s |
| Optuna default | 3 | 561 | 13 s |
| BoTorch qNEHVI | 1 | **733** | 1,735 s |

BoTorch finds better designs per solver call but costs about 43 s of model fitting per trial. With 1–4 s evaluations,
NSGA-II wins on wall clock, so the portfolio picks by measured evaluation cost. **[pending]**: BoTorch with 3 seeds on
an expensive-evaluation case.

### 4.7 Risks and how they are handled

| Risk | Handling |
|---|---|
| A live solve or mesh fails during the demo | Content-hash cache of every evaluation; the scripted brief is precomputed and replayable offline |
| Peak stress at sharp corners does not converge (singularity) | Blunt rib tips and a 3 mm exclusion zone near load and support faces (D6); the trust record reports non-convergence instead of hiding it |
| Coarse search mesh under-predicts stress | Search at ≤ 4 mm, verify at 3 mm (D7); the measured under-prediction at 6 mm was about 29% |
| LLM misreads the brief | The spec card is shown and accepted before any search; each requirement names its source |
| Overclaiming novelty | Claims are phrased as "we found no shipped product that…" against the cited prior art |

## 5. Prototype

### 5.1 Software (working today)

- The interactive study reports for the bracket (`runs/p2_demo/report.html`, 9.6 MB) and the fan-cooled heat sink
  (`runs/heatsink_fan_demo_300/report.html`, 11.7 MB) open offline in any browser. Each shows the stress or
  temperature field, the clickable Pareto front, the trust record, the optimization history, the agent timeline and
  how to make the part (FDM and CNC fit, time and cost). Screenshots: `docs/screenshots/`.
- Claude drives the full flow through the MCP server registered in `.mcp.json`: list families, derive loads, rank
  questions, create the study, accept the spec, check feasibility, run the optimization, verify, build the report.

### 5.2 Physical test **[pending: H3]**

The recommended bracket and the hand baseline are printed in PETG and loaded with known weights until failure or
until 2 × the 20 kg service load. The test compares measured failure load and deflection with the predicted values.
The test protocol and bill of materials are to be written; the printer and the weights are the owner's.

### 5.3 Demo video **[pending]**

Three minutes: the brief typed into Claude, the spec card, the optimization running, the Pareto front, the stress
field of the chosen design, the trust grade, and the printed part under load.

## 6. Prior art

| System | What it does | What it does not do (as of Nov 2026) |
|---|---|---|
| SimScale Engineering AI agent | Prompt → physics setup, mesh, solve, explanation; FEA, CFD, thermal, EM; free; Bayesian optimization over Onshape configurations | No geometry from scratch; no Pareto front shown ([1], [2]) |
| Physics-in-the-Loop (Berger et al.) | CadQuery → gmsh → torch-fem with planner, CAD and reviewer agents | Structural only; no numeric multi-objective optimization; static figures ([3]) |
| Self-Improving CAD Agents with FEA Feedback / Hephaestus-CCX (Son, Park et al.) | CadQuery → gmsh → CalculiX inside Claude Code and Codex; executable requirement checkers; 50-brief benchmark | Structural only; no Pareto optimization; static renders ([4]) |
| Synera | Multi-agent engineering workflows across 80+ tools | Physics through the customer's licensed tools ([5]) |
| Autodesk Fusion, Altair, Siemens, Ansys | Mature topology/generative optimization and Pareto exploration | Natural-language intent is minimal; commercial licences |

Benchmark against prior art **[pending]**: run the Hephaestus-CCX briefs that fit our families, if its kit is
public, and report execution, meshing, FEA and requirement-pass rates separately.

References:

1. SimScale, "Text-to-CAD", 30 Oct 2026: https://www.simscale.com/blog/text-to-cad/
2. SimScale, Engineering AI agent open to the community: https://www.simscale.com/press/engineering-ai-agent-open-to-community/
3. Berger et al., Physics-in-the-Loop, arXiv 2605.19717: https://arxiv.org/html/2605.19717
4. Son, Park et al., Self-Improving CAD Generation Agents with FEA Feedback, arXiv 2605.17448: https://arxiv.org/html/2605.17448v2
5. Synera: https://www.synera.ai/

Full landscape with scored feature matrix: `docs/research/competitive-landscape-2026-11.md`.

## 7. Plan to the live demo (3 February 2027)

From `docs/agentic-engineering-roadmap.html`:

- **Physical test predictions** (`benchmarks/h3_predictions.json`, `python benchmarks/gauge_point.py`): stiffness at
  the arm end 59.8 N/mm (baseline) against 389.9 N/mm (recommended), ratio 6.5×. Protocol: `docs/test-protocol-h3.md`.
- **Before 10 January:** physical test, video, Hephaestus-CCX benchmark, measurements on an 8 GB laptop.
- **11–24 January (P6):** improve an uploaded STEP part (keep its holes, make it lighter), a surrogate for faster
  screening. (Print rules, the FDM/CNC cost model and the fin-channel correlation are already built.)
- **25 January – 3 February (P7):** demo hardening: precomputed cache, offline mode, rehearsals.

## Evidence index

| Result | File | Command |
|---|---|---|
| Validation suite (7 cases) | `docs/validation.json` | `engine/build/structura validate` |
| Solver cross-check | `docs/benchmarks.md` § F2-T4 | `engine_tests -tc="F2-T4*"` |
| Gate and mesh sweep | `benchmarks/gate_mesh_sweep*.json` | `structura meshsweep --family bracket --n 500 --seed 2 --size 4` |
| Optimizer comparison | `benchmarks/optimizer_compare_at40.json` | `python benchmarks/optimizer_at_budget.py --calls 40` |
| 20 kg bracket | `benchmarks/p2_demo.json` | `python benchmarks/p2_demo.py` |
| 100 kg bracket (infeasible) | `benchmarks/bracket_100kg_demo.json`, `benchmarks/bracket_100kg_feasibility.json` | `python benchmarks/p2_demo.py --spec docs/examples/bracket_100kg.json --name bracket_100kg_demo` |
| Heat sink (fan, correlation) | `benchmarks/heatsink_fan_demo_300.json`, `_600.json` | `python benchmarks/heatsink_demo.py --spec docs/examples/heatsink_10w_fan.json --name heatsink_fan_demo_300 --evals 300` |
| Bracket at double budget | `benchmarks/p2_demo_400.json` | `python benchmarks/p2_demo.py --evals 400 --name p2_demo_400` |
| Reports | `runs/*/report.html` | `python -m structura.report.build ...` |

## Before submission

- [ ] Replace every **[pending]** with a measured value or remove the claim.
- [ ] Physical test done; photos and measured loads added to 5.2.
- [ ] Video recorded and linked.
- [ ] Measurements repeated on the 8 GB minimum laptop.
- [ ] Re-read section 6 against the latest SimScale release notes (the window is short).
- [ ] Convert to the competition's required format (check the AIDTC template; none is in the repo yet).
