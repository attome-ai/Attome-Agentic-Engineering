# Benchmarks

Measured results only. Each table states the machine, the date and the command that produced it.

## NAFEMS LE10: thick plate under pressure (task F2-T2)

Measured 2026-11-04 on the development machine (AMD Ryzen 9 9950X3D, 16 cores, 61.6 GB RAM, Windows 11),
CalculiX 2.23 (conda-forge, SPOOLES solver, 16 threads), gmsh 4.15.0, quadratic tetrahedra (C3D10).
Command: `micromamba run -n attome-ae python benchmarks/le10/le10.py`

Reference: sigma_yy at point D (2, 0, 0.3) = −5.38 MPa. Quarter model; pressure applied as a distributed face
load (`*DSLOAD`) on the element faces of the upper surface; stress read from the nodal stresses in the `.frd`.

| Mesh | Size (m) | Nodes | C3D10 elements | DOF | sigma_yy at D (MPa) | Error vs −5.38 | Mesh time (s) | Solve time (s) |
|---|---|---|---|---|---|---|---|---|
| coarse | 0.3 | 2359 | 1262 | 7077 | −5.533 | −2.85% | 0.2 | 0.2 |
| medium | 0.15 | 10118 | 6071 | 30354 | −5.435 | −1.02% | 0.3 | 0.8 |
| fine | 0.075 | 58593 | 38421 | 175779 | −5.367 | +0.25% | 1.1 | 8.7 |

What this shows:
- The result converges monotonically to the reference; the fine-mesh error (0.25%) is inside the 2% exit check of P1.
- gmsh's INP writer orders C3D10 nodes the way CalculiX expects (a wrong order would not converge to the reference).
- Runtime: about 0.8 s at 30k DOF and 8.7 s at 176k DOF on this machine. The research estimated 1 s at 50k DOF and
  10-60 s at 500k DOF; these measurements are consistent with that, on a much faster machine than a typical laptop.
- The demo machine is this development machine (it also has an NVIDIA RTX 5090); no separate minimum laptop is planned.

Notes found while building it:
- gmsh's INP export names element sets after geometry entities, not physical groups; faces are found through the
  physical groups' node sets instead.
- CalculiX 2.23 does not print nodal stresses with `*NODE PRINT ... S`; stresses are read from the `.frd` file.

## Cantilever against beam theory (task C1-T2)

Measured 2026-11-04, same machine, CalculiX 2.23 through the C++ engine (`engine_tests.exe -tc="C1-T2: cantilever*"`).
100 x 10 x 10 mm S235 beam, root face fixed, 100 N downward spread over the tip face, C3D10 at 2.5 mm (20,388 DOF).

| Quantity | Value |
|---|---|
| Tip deflection (mean over the tip face) | 0.19044 mm |
| Euler-Bernoulli PL^3/3EI | 0.19048 mm |
| With shear correction (Timoshenko, k = 5/6) | 0.19196 mm |
| Error against the shear-corrected value | -0.79% (limit 2%) |
| Reaction equilibrium error | below 0.5% |
| Solve time (4 threads) | 0.42 s |

## Bracket evaluation job (C1-T2 end to end)

`structura solve --job docs/examples/bracket_job.json`: default bracket (6 mm, one rib), PETG, 981 N (100 kg) on the
arm's end face, bolts fixed and the wall face supported normal to the wall. Build, gate, mesh (10,389 C3D10,
60,690 DOF) and solve take 2.1 s in total; solve alone 1.2 s.

| Result | Value |
|---|---|
| Mass | 96.4 g |
| Max displacement | 13.5 mm |
| Peak von Mises | 186 MPa (PETG yield 50 MPa: this design fails at 100 kg) |
| Equilibrium error | 0.0 |

With the bolts as the only support the bracket pivots about the bottom holes (224 mm, 824 MPa); the wall bearing
support is required for a realistic result.

## In-house solver cross-check (task F2-T4)

Same bracket mesh (62,448 DOF), loads and supports as the golden case, solved by CalculiX and by the engine's own
quadratic-tetrahedron solver (Eigen, SimplicialLDLT, single thread).

| Quantity | CalculiX | In-house | Difference |
|---|---|---|---|
| Max displacement | 13.5133 mm | 13.5133 mm | 6e-5 % |
| Load-face mean vertical displacement | | | 1e-5 % |
| Worst nodal displacement difference | | | 5.0e-5 mm |
| Time | 1.26 s (4 threads) | 0.16 s assemble + 2.14 s solve | |

The agreement confirms the CalculiX deck writer, node ordering and result parsing independently of CalculiX.

## Gate, repair and mesh sweep (task B4-T3)

`structura meshsweep --family bracket --n 500 --seed 2 --size 4`, 2026-11-04, development machine, 433 s in total.

| Stage | Result |
|---|---|
| Random in-bounds designs | 500 |
| Pass the gate (after safe repair) | 499 (99.8%) |
| Gate-passed designs that mesh | 499 of 499 (**100%**, target at least 99%) |
| Mesh on the first ladder attempt (Delaunay) | 499 of 499 |
| Designs needing repair to pass | 0 |

The one rejected design has three 8.7 mm ribs in a 35 mm wide plate, leaving gaps of about 0.02 mm between ribs: a
real defect, correctly refused. For comparison, Physics-in-the-Loop (arXiv 2605.19717) reports 76-88% meshing after
96-97.5% code execution with free-form LLM CAD; the difference comes from the typed grammar and the gate.

The mesh-ability model stays untrained (`data/meshability_bracket.json`): with no meshing failures there is nothing
to learn, so it predicts every gated shape meshable and says so. It will train automatically when a family produces
failures.

## Trust record and screening path (tasks F1, D3-T3)

Test design (8 mm, 50 mm wide, 2 ribs, 20 kg spec at factor 1.5), three mesh levels 3 / 4.5 / 6.75 mm:

| Quantity | Grid Convergence Index (fine) | Observed order |
|---|---|---|
| Max displacement | 0.11% | 4.0 (capped) |
| Mass | 0.0% (converged to round-off) | |
| Peak von Mises | **14.3%** | 2.1 |

Peak stress does not converge cleanly because it sits at sharp re-entrant corners and the edge of the loaded face
(a stress singularity); the trust panel reports this rather than hiding it. Follow-up: measure stress at a defined
distance from singular edges. Equilibrium error 1e-13 and strain-energy balance 5e-8 (Clapeyron) pass. The design
fails R1 (safety factor 0.97 against 2.0), so its grade is C: the record behaves as intended.

Screening path (in-house solver, same coarse mesh, `--screen 1`): max displacement 4.7587 mm in both solvers, peak von
Mises 51.921 vs 51.922 MPa. It is not faster at this size (1.52 s against 1.37 s for CalculiX with mesh I/O): Eigen's
single-threaded LDLT costs about what CalculiX's multithreaded SPOOLES does. Its value today is the in-process cross-check;
speed-up would need an iterative solver with factorization reuse.

## Optimizer comparison (task D1-T3)

Bracket, 20 kg spec, coarse mesh 6 mm (about 1 s per evaluation), hypervolume of the feasible front with reference
point (400 g, safety factor 0). Equal solver calls, from `benchmarks/optimizer_at_budget.py --calls 40`:

| Method | Seeds | Hypervolume at 40 calls | Wall clock to 40 calls |
|---|---|---|---|
| Random search | 3 | 566 | 18 s |
| NSGA-II (constrained) | 3 | 573 | 15 s |
| Optuna default sampler | 3 | 561 | 13 s |
| BoTorch qNEHVI (Ax) | 1 | **733** | 1,735 s |

At 120 calls the GA methods reached 594-693. BoTorch finds better designs per solver call but its model fitting costs
about 43 s per trial against 1 s per evaluation, so the portfolio now chooses by measured evaluation cost (decision
D5). The full 120-call run was stopped after the first BoTorch seed; partial results are in
`benchmarks/optimizer_compare.json`.

## P2 exit demo: 20 kg bracket brief

> **Under review (2026-11-04):** the 20 kg bracket result (234 g, safety factor 2.06, grade A) is not valid as stated. Its peak stress sits at the sharp junction of the rib tip and the arm, where stress does not converge: it rises from 12.1 MPa at 3 mm to 13.8 MPa at 2 mm (SF 1.82), and with realistic 1.5 mm fillets at that junction the first solve gives 16.7 MPa (SF 1.50). The convergence study behind grade A (3 / 4.5 / 6.75 mm) was too coarse to see it. Being fixed: filleted rib junctions in the bracket family, a stricter stress-convergence rule, and a re-run of the optimization.
 (`benchmarks/p2_demo.py`, bracket family v2)

200 NSGA-II evaluations at 4 mm (the portfolio chose NSGA-II from the measured evaluation cost), front re-solved at 3 mm,
knee design verified with three mesh levels. About 4.5 minutes in total on the development machine.

| | Mass | Safety factor | Max displacement | Result |
|---|---|---|---|---|
| Hand baseline (6 mm, 1 rib) | 96 g | 0.37 | 5.0 mm | fails R1 (factor 5 short) and R2 |
| Recommended knee design | 234 g | 2.06 | below 3 mm | all requirements met, **trust grade A (verified)** |

Front: 4 designs, 3 still feasible on the fine mesh, ranking unchanged. The honest framing: the baseline is not lighter
than a safe design, it is unsafe; the optimizer finds the lightest design that is proven to meet the brief.
Golden result regenerated for bracket v2 (geometry change, decision D6).

## Thermal demo: heat sink for a 10 W module (`benchmarks/heatsink_demo.py`)

Plate-fin heat sink family (base 60 x 60 mm, 4-16 fins), aluminium 6061-T6, 10 W into the base, 35 C air at an
assumed 25 W/m^2K (low end of forced convection, Incropera Table 1.1), requirement peak temperature <= 75 C,
objectives minimum mass and minimum peak temperature. 150 NSGA-II evaluations at 2.5 mm, front re-solved at 1.5 mm.

| | Mass | Peak temperature | Result |
|---|---|---|---|
| Hand baseline (4 mm base, 8 fins 2 x 25 mm) | 103.7 g | 49.2 C | meets the requirement with a large margin |
| Lightest optimized design (2.5 mm base, 4 fins 2.5 x 12 mm) | **44.3 g (-57%)** | 74.6 C | meets it, **trust grade A** |

Front: 13 designs, all still feasible on the fine mesh. Default design's trust record: peak temperature numerical error
0.002%, heat balance (heat convected out against 10 W in) 2e-8. About 4 minutes on the development machine.
The fixed convection coefficient is an approximation stated in the trust record's assumptions; a fin-array correlation
and later CFD (C5) replace it.

## Infeasible brief: 100 kg PETG bracket (`benchmarks/p2_demo.py --spec docs/examples/bracket_100kg.json --name bracket_100kg_demo`)

Same family and pipeline as the 20 kg demo, 981 N service load, SF >= 2 required. Measured 2026-11-04, 182 s of
optimization.

| | Mass | Safety factor | Result |
|---|---|---|---|
| Hand baseline | 96 g | 0.07 | fails |
| Best of 200 NSGA-II evaluations | 324 g | 0.55 | fails; 0 of 200 feasible |
| Strongest corner of the bounds (`benchmarks/bracket_100kg_feasibility.json`) | 354 g | 0.67 | fails: no design in the family can meet the brief |

The corner check bounds what the search can reach (the search, trading mass against the 250 g cap, did not reach that
corner). `propose_relaxation` now also reports the load at which the strongest design meets the requirement: 33% of
the brief's load, about 33 kg (linear statics: safety factor scales with 1/load).

## Physical-test predictions (H3, `benchmarks/gauge_point.py`)

Arm-end displacement at 294.3 N (service load at factor 1.5), force-weighted over the loaded nodes, from the stored
fine-mesh bundles of the 20 kg demo (`benchmarks/h3_predictions.json`):

| Design | Arm-end displacement | Stiffness | Strain energy check (F d / 2 vs CalculiX) |
|---|---|---|---|
| Baseline | 4.919 mm | 59.8 N/mm | 723.77 vs 723.77 N mm |
| Recommended (trial 183) | 0.755 mm | 389.9 N/mm | 111.06 vs 111.06 N mm |

Ratio 6.5×. Protocol and pass criteria: `docs/test-protocol-h3.md`.

## Fan-cooled heat sink with the fin-channel correlation (C2), and budget convergence (D8)

`benchmarks/heatsink_demo.py --spec docs/examples/heatsink_10w_fan.json --name heatsink_fan_demo[_300|_600] --evals N`.
Convection from the plate-fin channel correlation (Teertstra et al. 1999) at an assumed 1.5 m/s approach velocity,
recomputed for every design from its fin spacing (typically 20-26 W/m^2K for this family). Same requirement (peak
temperature <= 75 C), same seed.

| Budget | Lightest design meeting 75 C (fine mesh) | Peak temperature | Front designs feasible on the fine mesh | Wall clock |
|---|---|---|---|---|
| 150 | 55.8 g (9 fins) | 52.7 C | 17 of 17 | 400 s |
| 300 | **34.4 g** (5 fins, 1.3 mm, 13.8 mm tall, 2.0 mm base) | 71.3 C | 27 of 27 | 497 s |
| 600 | 34.6 g (5 fins) | 69.2 C | 62 of 62 | |

Baseline (4 mm base, 8 fins 2 x 25 mm): 103.7 g, 48.4 C. Converged result: **34.4 g, -67%**, trust grade A (heat balance
and mesh convergence pass). The 150-evaluation result was verified and still 62% heavier than the optimum, because
the search had sampled only one design between 35 and 55 g. That led to decision D8 (double the budget before calling
a result converged).

With the correlation the old 44.3 g fixed-h design reaches 78.9 C: the correlation gives 22.5 W/m^2K for its wide
4-fin channels, below the 25 W/m^2K that had been assumed.

## Manufacturing check and cost (H1, `structura manufacture`)

Recommended 20 kg bracket (trial 183), PETG, inputs from `data/processes.json` (all stated assumptions):

| Process | Fits | Closest rule | Time | Cost |
|---|---|---|---|---|
| FDM, printed upright (+z) | yes | build volume 48% used | 14.7 h | 30.4 USD (material 8.3, machine 22.1) |
| CNC 3-axis, two setups | no | 5 sharp internal corners along the tool; 1.4% of the surface undercut | 2.7 h | 190 USD |

Support printed upright: about 520 cm^3, 2.8 times the part (the arm overhangs). The least-support direction is +x;
the strength analysis assumed +z layers, so a re-analysis is needed before changing the orientation.
Unit tests on shapes with known answers: support volume of a 40 x 10 mm shelf at 45 mm = 18,000 mm^3 exactly;
undercut area from the top = 950 mm^2 within 8%; the four sharp corners of a square pocket.

## Budget convergence of the headline results (D8)

| Result | Budget | Best objective | Double budget | Change | Converged |
|---|---|---|---|---|---|
| 20 kg bracket, lightest proven design | 200 | 234.1 g (grade A) | 400: 233.6 g (grade B: margin 1.3% below its numerical error) | -0.2% | yes |
| Heat sink, fan correlation | 300 | 34.4 g (grade A) | 600: 34.6 g | +0.5% | yes |
| Heat sink, fixed h = 25 W/m^2K | 150 | 44.3 g | 300: 42.0 g | -5.2% | no (superseded by the correlation spec) |

## Live demo rehearsal (P7, `scripts/demo.py`)

Preflight (`scripts/demo.py preflight`): engine, CalculiX and gmsh found, viewer vendored (no network needed), fallback
report present, validation 7 of 7. Rehearsal 2026-11-04 on the development machine, 20 kg brief through the MCP tool
functions, 200 evaluations (`benchmarks/demo_rehearsals.json`):

| Step | Seconds |
|---|---|
| create study, accept spec | 0 |
| feasibility check (two corners) | 3.9 |
| baseline evaluation | 2.3 |
| optimization, 200 evaluations in the background | 350 |
| verify (three mesh levels, grade A) | 21.9 |
| manufacturing advice | 0.4 |
| report (fine-mesh bundles of the front) | 79 |
| **total** | **458 (7.6 min)** |

Recommended: 231.4 g, safety factor 2.22, grade A, FDM. NSGA-II runs in parallel, so a live run is not
bit-identical to the stored P2 run (234.1 g); both meet the brief with grade A. On a 4-core laptop the optimization
will take longer: rehearse there and lower --budget if needed (D8: report the budget used).

## Uploaded STEP part analysed as-is (A3)

`structura inspect --step part.step --family-out families/uploads/NAME.json` on the recommended bracket exported as
STEP (`runs/p2_demo/opt/report_bundles/trial_183/design/part.step`): two 6.6 mm bolt holes along x found as a
pattern of 2, the wall face (-x at 0) and the arm faces among the mating candidates; the arm-end face (692 mm^2) named
by the user. Analysed with the 20 kg load case and no free parameters, at 3 mm:

| Path | Mass | Safety factor | Max displacement |
|---|---|---|---|
| Native family (`bracket`, trial 183 parameters) | 234.143 g | 2.0642 | 0.77616 mm |
| Uploaded STEP (`import_step`, faces from inspect) | 234.143 g | 2.0642 | 0.77616 mm |

Identical to the last digit shown: importing, face selection and the solve lose nothing. The import pins the file's
SHA-256, so a changed file is refused rather than served from the cache. Redesigning an uploaded part (template fit,
or operations inside its envelope) is the next step; today an uploaded part is analysed and becomes the baseline.

## Improving an uploaded part (A3, `benchmarks/a3_redesign_demo.py`)

An over-built bracket (12 mm plate, 80 mm wide, three 10 mm ribs, 6 mm corner fillet) arrives as a STEP file.
Inspection finds the two 6.6 mm bolt holes (frozen: the material around each must stay) and the faces; the user names
the arm end (load) and the wall face (bearing). The improvement brief keeps the 20 kg load and SF >= 2 and replaces the
from-scratch 250 g cap with "lighter than the uploaded part". Redesign: one lightening pocket in the arm top, its
length, width and depth searched by NSGA-II (120 evaluations at 4 mm, front re-solved at 3 mm), about 12 minutes.

| | Mass | Safety factor | Max displacement |
|---|---|---|---|
| Uploaded part as-is | 354.1 g | 3.20 | 0.46 mm |
| Lightest redesign feasible on the fine mesh (pocket 108.8 x 33.7 x 5.6 mm) | **328.2 g (-7.3%)** | 2.32 | 0.66 mm |

Front: 14 designs, all still feasible on the fine mesh. Trust grade **B**, not A: the safety factor's numerical error
is 8.2% (the pocket's 1 mm corner radius concentrates stress), and R1 is "not proven": its margin (0.32) is larger
than the error (0.19), but the safety factor does not converge monotonically over the three meshes and an error of 5%
or more from oscillating convergence is not trusted. The trust record now gives that reason; it used to say the margin
was smaller than the error, which was wrong for this case. A larger pocket
corner radius, or pockets in the ribs and wall plate, are the next redesign operations.

Two defects found and fixed on the way: the hole detector summed the angular spans of co-axial segments, so a corner
fillet cut into four quarter-cylinders by three ribs looked like a full hole and was frozen (now measured as angular
coverage, test A3-T3); and the engine's family listing did not include uploaded families.

## Free-form CAD fallback (B1, MCP tool `freeform_cad`)

For shapes the design grammar cannot express, the agent writes CadQuery code that assigns `result`. It runs in its own
Python process (`python -I`, time limit, its own folder), is exported to STEP, inspected like an uploaded part (holes
frozen, faces named) and gated by the engine; failures come back typed (`freeform_code`, `freeform_result`,
`freeform_geometry`, `freeform_timeout`) with the Python/CadQuery message for repair. Tested with a plate, two holes
and a lofted boss (a loft is not in the grammar): gate passes, both holes found. The shape is fixed geometry: it is
analysed as-is and improved with the uploaded-part operations or by revising the code; the grammar stays the first
choice (the agent's instructions say so).

Best of N (`compose_candidates`): the agent proposes several design graphs at once; each is built, gated and solved
once at its default parameters against the study's spec (coarse mesh, a screening) and they come back ranked (gate
pass, meets every requirement, first objective), with the reason each loser lost (a missing load or support face, a
graph error, a gate reason, or the failed requirement). Rejected graphs leave no family file behind, and the engine's
family listing skips unloadable files.

Visual review (`render_views`): the engine's STL of a family design or a graph is rendered offline (matplotlib) in
six views (front, back, left, right, top, isometric) with the overall size, about 2 s, and returned to the agent as
an image over MCP so it can check the shape against the brief before solving. Example:
`docs/screenshots/render_views_bracket.png`.

## Brief benchmark (P3-t1, `benchmarks/brief_bench/`): dataset and scorer ready, model run pending

50 hand-written briefs with gold spec fields: 31 brackets, 12 heat sinks and 7 out of scope (a gear, a propeller,
a bicycle frame...: the right answer is "no family fits"); 15 in Arabic, 3 mixed, and imperial units (lb, F) to test
conversion. Ten fields per brief (family, material, load in N, safety factor, deflection, power, temperature limit,
fan speed, objective, process). The scorer reports all fields, **stated fields** (only those the brief gives: an empty
answer scores 58% on all fields but 0% here, so this is the honest number) and unit correctness, with every miss.
Self-check: the gold answers score 100%; no answers score 0% on stated fields. The model run needs an API key:
`python -m benchmarks.brief_bench.run extract --model claude-opus-5-5 --out runs/brief_bench/opus`, then `score`.
Image briefs (a sketch or photo) are not in this set yet.

## Reproducibility (P3-x2, `structura reproduce --dir TRUST_DIR`)

Each trust record stores the spec path and hash, the parameters and the fine-mesh metrics; `reproduce` refuses a
changed spec and re-solves. 2026-11-04:

| Result | Reproduced | Largest relative difference |
|---|---|---|
| 20 kg bracket, recommended design (`runs/p2_demo/trust`) | yes | 0 (bit-identical: mass, displacement, stress, SF) |
| Fan heat sink, 34.4 g design (`runs/heatsink_fan_demo_300/trust`) | yes | 0 |
| Uploaded-part redesign (`runs/a3_demo/trust`) | yes | 0 |

The search itself is not replayed trial for trial: NSGA-II evaluates in parallel, so the order of finished trials
varies between runs (the live rehearsal found 231.4 g where the stored run found 234.1 g; both grade A). What is
delivered, a design with its verified numbers, reproduces exactly.

## Offline readiness (P7-t1, static check; the network-off test is still to do)

No network access in the engine, the Python layer, the reports or the workbench (search of `engine/src`, `structura/`,
`scripts/` for HTTP clients and external URLs: none; three.js is embedded in each page, no web fonts or CDNs). What
needs the internet: the AI agent (Claude runs in the cloud). So with the network off the demo can show the engine,
the workbench, live optimization and the reports, and the agent part needs a connection or the recorded video.
Test to run on the demo machine: turn Wi-Fi off, then `python scripts/demo.py preflight`, `python scripts/demo.py live
--budget 60`, open `runs/p2_demo/report.html` and `python -m structura.app`.

## Frequency and buckling requirements in specs (C3)

`first_frequency_hz` and `buckling_factor` were declared spec quantities but never computed, so a spec using them
would have failed every design. Evaluations now run CalculiX frequency (3 modes) and buckling analyses on the same mesh
for each structural load case, only when the spec asks for the quantity (test C3-T2 checks both directions). The
recommended 20 kg bracket: first natural frequency 391.5 Hz (then 769 and 852 Hz), buckling factor 55.8; the
evaluation takes about 23 s instead of 4 s with both analyses. Solver accuracy for both is in the validation suite
(buckling +0.06%, first frequency -0.26%).

## Ten scripted briefs end to end (P3-t0, `benchmarks/brief_suite.py`)

Ten briefs (`benchmarks/briefs/`): six brackets (PLA, PETG, aluminium; 10 to 100 kg; one with a 1 mm deflection limit)
and four heat sinks (5 to 40 W, fan 1.5 or 2.5 m/s); two are expected to be infeasible (100 kg PETG, 40 W below 50 C).
The spec for each brief was written by the agent from the brief text. Stages: spec, feasibility, optimize (150
evaluations), verify (front on a finer mesh), trust.

**First run (2026-11-04): 6 of 10, and three defects found.**
- The heat-sink corner with 1.2 mm fins measured 1.1999999999 mm and failed the 1.2 mm minimum wall; the feasibility
  check then called all four heat-sink briefs infeasible ("corner could not be evaluated"). Fixed: gate tolerance
  (test B4-T4), and an unevaluable corner is now inconclusive instead of infeasible.
- One of those four was "expected infeasible", so it was counted a success for the wrong reason; with the fix the
  suite must reach that verdict from physics.
- "30 kg shelf, lightest possible" found no feasible design because of a default 250 g mass cap the brief never asked
  for. Decision D9: unstated defaults are hard requirements only when they protect the user.
First-run details: the brackets that ran gave grade A (10 kg PLA, 15 kg PETG with 1 mm, 50 kg aluminium) and grade B
(20 kg PETG, safety factor converging non-monotonically); 100 kg PETG was correctly reported infeasible in 10 s.

**Second run (fixed engine, D9 specs): 7 of 10.** The heat-sink fix worked (5 W, 10 W and 15 W: grade A). Misses:
- b02 (20 kg PETG): verified but grade C: the lightest design has a stress raiser (54% numerical error). Fix: deliver the
  lightest *proven* design, trying the next one when verification fails.
- b03 (30 kg PETG): no feasible design in 150 evaluations, closest SF 1.875; the feasible region is a sliver near the
  strongest corner. Fix: the search now starts from the corners of the design space.
- h04 (40 W below 50 C, expected infeasible): a "grade A" design with 16 fins and 1.36 mm gaps. Not credible: the
  model keeps the fan's 1.5 m/s approach velocity fixed, so dense fins get 4.2 m/s in the channels; a real fan's flow
  falls with the fins' pressure drop and the air bypasses the sink. The optimizer exploited a stated assumption.
  Fix: the fan's operating point (its pressure-flow curve against the fin channels' pressure drop) replaces the fixed
  velocity; the heat-sink results are re-run with it.

## Real-physics animation: a 20 kg crate set down on the shelf (`structura dynamic`, `python -m structura.scene`)

Recommended 20 kg bracket (trial 183), 6 mm mesh, PETG with print knock-down. The crate's 20 kg is lumped on the loaded
face as mass (its inertia moves with the bracket) and its weight is applied at once at t = 0; implicit time integration
in CalculiX with 3% Rayleigh damping (assumed), 85 frames over 181 ms, 372 s solve.

| Check | Solver | Hand calculation | Difference |
|---|---|---|---|
| Bounce frequency with the crate | 22.12 Hz | sqrt(k/m)/2 pi = 21.97 Hz (k = 381 N/mm from the static solve) | 0.7% |
| Dynamic amplification | 1.90 | single mass, 3% damping: 1 + exp(-pi z / sqrt(1 - z^2)) = 1.91 | 0.5% |
| Time of the first peak | 23.6 ms | half a period: 22.6 ms | one time step |

Peak stress 16.3 MPa against 8.6 MPa for a gently placed crate: still below the 25 MPa effective yield (margin 1.5
instead of 2.9); the spec's 1.5 load factor covers it. Video: `runs/videos/bracket_20kg_crate_set_down.mp4`.

An earlier version modelled the crate as a massless force: deflections were right but the motion ran at the bracket's
own 770 Hz instead of 22 Hz. Fixed by the carried mass. The shelf board and crate are drawn rigid and follow a line fit
to the solved displacement of the arm's top face.

## BoTorch on the GPU (RTX 5090) against the CPU (`benchmarks/botorch_device.py`)

20 kg bracket, 40 calls, 6 mm mesh, Ax qNEHVI. Model seconds per trial = wall clock minus evaluation time (4 in
parallel), per trial; the machine was shared with other jobs during both runs.

| Device | Seeds | Model seconds per trial | Hypervolume at 40 calls |
|---|---|---|---|
| CUDA (RTX 5090) | 3 | 31.8 (25.5 / 31.7 / 38.2) | 661 (624 / 724 / 636) |
| CPU (Ryzen 9 9950X3D) | 1 | 19.3 | 654 |

The GPU does not speed up model fitting at this size (a few dozen points, five parameters): transfer and kernel launch
overhead outweigh the work. The CPU stays the default (`STRUCTURA_TORCH_DEVICE` unset) and the optimizer-choice rule
(D5) is unchanged. Over three seeds BoTorch reaches a hypervolume of 661 at 40 calls against about 565-573 for random
search and NSGA-II: still better per solver call, by less than the single-seed 733 suggested.
