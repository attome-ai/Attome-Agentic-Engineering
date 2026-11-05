# Decisions

Each decision: date, what was decided, why, and what it changes. Newest last.

## D1. The engine is C++; Python is only the AI layer (2026-11-04, owner)

**Decision.** Geometry (design-graph grammar and compiler on OpenCASCADE), the geometry gate, meshing, solver input
decks, result parsing, verification checks and field export are a C++20 engine in `engine/`: a static library
(`structura_core`), a command-line tool `structura` with JSON in and out, and doctest tests. Python remains only for the
AI layer in `structura/`: optimizers (Optuna, pymoo, BoTorch/Ax, SALib) and the agent/MCP server, which call the
engine through `structura/engine.py`.

**Why.** Owner's choice of language for the engine. The heavy computation was already C/C++/Fortran (OCCT, gmsh,
CalculiX); the optimization and agent libraries the plan relies on exist only in Python, so they stay there.

**What changed.**
- The Python grammar, family loader, design store, gate and mesher written earlier on 2026-11-04 were ported to C++
  with every test ported; the C++ suite passes (21 test cases, 10,796 assertions) and the Python versions were removed.
- Part families moved to `families/` at the repository root, read by the engine.
- gmsh still runs only as `gmsh.exe` on a generated `.geo` script and CalculiX as `ccx.exe`, so no GPL code is linked.
- The planned scikit-fem cross-check and screening solver (F2-T4, D3-T3) become an in-house C++ linear tetrahedral
  solver (with Eigen, MPL-2.0), independent of CalculiX.
- Build: Visual Studio 2022 (MSVC 19.44), CMake, Ninja; `scripts\build_engine.cmd`.
- Roadmap tasks were re-pathed from `structura/*.py` to `engine/src/*.cpp`; engine tasks are verified with
  `engine_tests.exe -tc="<task id>*"`, and each task's doctest cases are named with the task id.

## D2. gmsh is driven by .geo scripts, not its Python API (2026-11-04)

**Decision.** The mesher writes a `.geo` file and runs `gmsh.exe`; named faces become physical groups selected by
bounding box, and each group's meshed area is checked against the faces it should contain.

**Why.** Keeps GPL gmsh out of process (no import, no linking) and the same path works from C++ and Python. The area
check catches a bounding box that selected the wrong faces.

## D3. Mounting holes sit below the ribs in the bracket family (2026-11-04)

**Decision.** The bracket's two M6 holes are in a row at z = 12 mm, at a quarter and three quarters of the width,
and the ribs reach 70% of the wall height.

**Why.** The first gate sweep found that with the holes on the centre line a rib could cut into a bolt hole, leaving
slivers under 0.2 mm and blocking the bolt. After the change 200 of 200 random designs pass the gate.

## D4. Feasibility is checked with real physics at the parameter corners, not beam formulas (2026-11-04)

**Decision.** `check_feasibility` evaluates the most-material and least-material corners of the spec's free
parameters (direction found by comparing volumes) with the full engine path on a coarse mesh. If even the strongest
design fails a strength or stiffness requirement, or the lightest still breaks the mass limit, the spec is reported
infeasible with the best achievable value.

**Why.** It works for any part family without hand-written formulas per family, and it is exact for the model
rather than an estimate. Cost: two coarse evaluations (about 3 s for the bracket). The planned step "compare the
estimate with 30 solved designs" no longer applies because nothing is estimated.

**First result.** The 100 kg PETG bracket brief is infeasible within the family bounds: the strongest design
(12 mm, 80 mm wide, 3 ribs, 354 g) reaches a safety factor of 0.89 against the required 2.0 with the printed-part
knock-down applied. The demo therefore uses the 20 kg brief (`docs/examples/bracket_20kg.json`).

## D5. The optimizer is chosen by measured evaluation cost, not by budget alone (2026-11-04)

**Evidence.** Bracket, 20 kg spec, coarse mesh (about 1 s per evaluation), equal solver calls
(`benchmarks/optimizer_compare_at40.json`): mean hypervolume at 40 calls was random 566, NSGA-II 573, Optuna default 561
(3 seeds each) and BoTorch qNEHVI 733 (1 seed, +28% over NSGA-II; better than the GA methods reach at 120 calls, about
650). Wall clock to 40 calls: 13-18 s for the GA methods, 1,735 s for BoTorch (about 43 s of model fitting per trial).
The full 120-call, 3-seed run was stopped after the first BoTorch seed because it would have taken over 4 hours.

**Decision.** Before a study the portfolio times one evaluation of the centre design. Bayesian optimization is used
when one evaluation costs at least about half the model-fitting time and the budget is under 200; otherwise NSGA-II
(NSGA-III for three or four objectives). Cheap structural studies therefore run NSGA-II in parallel; fine-mesh,
thermal and later CFD studies get Bayesian optimization, where saving solver calls is worth the model cost.

**Open.** BoTorch was measured on one seed only; repeat with three seeds on an expensive (fine-mesh) evaluation.

## D6. Bracket ribs end in blunt tips; stress is checked away from load and support faces (2026-11-04)

**Evidence.** The first trust record of the optimized knee design showed a 76% Grid Convergence Index on peak stress:
the peak sat where each triangular rib's knife-edge tip meets the arm (a true singularity, stress growing 10.6 -> 11.7 ->
13.1 MPa as the mesh refined). Root fillets were tried and removed (their blend ends create 0.03 mm edges that the gate
rightly rejects; gate pass fell to 88.5%).

**Decision.** The grammar's `rib` gains `tip_cut`; bracket family version 2 cuts 3 mm off each acute tip. The strength
check uses the peak von Mises stress at least 3 mm from any load or support face (Saint-Venant; spec field
`stress_evaluation.exclude_near_bc_mm`), and the raw peak is reported alongside. Result: gate pass back to 200 of 200, and
the peak-stress error fell to 3.8%. The grammar's fillet can now round two edge sets in one operation.

## D7. The search mesh is no coarser than 4 mm for stress requirements (2026-11-04)

**Evidence.** With a 6 mm search mesh the safety factor of every Pareto design fell 28-30% on the 3 mm verification mesh
and 0 of 3 front designs stayed feasible; 3 mm and 4.5 mm agree within about 1%.

**Decision.** Studies search at the spec's mesh size (4 mm for the bracket) and verify the front at 3 mm. Result on
the 20 kg brief: 3 of 4 front designs stay feasible on the fine mesh, the ranking does not change, and the recommended
design (234 g, safety factor 2.06) gets trust grade A. The cache key now includes every family file and the materials
table, so geometry changes never serve stale results.

## D8. A reported optimum must survive doubling the search budget (2026-11-04)

**Evidence.** Fan-cooled heat sink (`docs/examples/heatsink_10w_fan.json`), same seed and pipeline: the lightest design
that meets 75 C was 55.8 g after 150 evaluations and 34.4 g after 300 (-38%). At 150 the search had sampled only one
design between 35 and 55 g; every number in that result was verified (grade A) and still not the optimum. Verification
proves a design meets the brief; it does not prove the search found the best one.

**Decision.** A headline result (proposal, demo, report) states its budget and is re-run at double the budget; it is
reported as converged only if the best objective changes by less than 2% between the two. Otherwise the larger run is
reported and the check repeats. Runs: `benchmarks/heatsink_fan_demo*.json`.

## D9. Defaults the user did not state are hard requirements only when they protect the user (2026-11-04)

**Evidence.** In the ten-brief suite, "a bracket for a 30 kg shelf, PETG, lightest possible" found no feasible design:
the spec carried the default 250 g mass cap, and every design strong enough for 30 kg is heavier. The brief never
asked for a cap; "lightest possible" is already the objective. A default decided the outcome without the user knowing.

**Decision.** Safety and serviceability defaults stay hard requirements (safety factor 2 on knocked-down strength,
deflection limits), each labelled with its source. Defaults that only restate the objective (a mass cap for "as light
as possible") are not added. When nothing is feasible, the agent's results name the closest design and the
requirements it misses (`get_results.closest_infeasible`), so the user sees which requirements conflict.

## D10. Trust checks local refinement; bracket ribs end in a filleted tip (2026-11-04)

**Evidence.** Adaptive refinement of the recommended 20 kg bracket (family version 2, 250.8 g, reported safety factor 2.32,
grade A) showed the peak stress growing without limit at the corner where each rib's blunt tip meets the arm: 11 -> 17 ->
28 -> 44 -> 52 MPa as the smallest element shrank from 4 to 0.15 mm. Three uniform meshes cannot see this, because none of
them resolves the corner, so their Grid Convergence Index looked fine. Rounding that corner gives a finite answer, and it
is not small: with a 1.5 mm fillet the peak settles at about 21 MPa, a true safety factor of 1.2 against the required 2.
The previous grade A was wrong about this design.

**Decision.** (1) The trust record refines locally twice, the second time to half the element size. If the peak grows by
more than 10% it is a sharp corner: the record says so and where, the design cannot be grade A, and the report states
that its stress is a value at the stated mesh size. (2) Bracket family version 3: ribs run 95% of the arm and end in a
6 mm tip with a 3 mm fillet, at both the arm end and the wall end. On the same design (before the wall-end fillet) this gives 266 g, a settled peak of 10 MPa (safety factor 2.5) and 0.39 mm
deflection, against 251 g, 21 MPa and 0.71 mm with the short rib and small fillet (`docs/benchmarks.md`).

**Still open.** The sharp edges along each rib's flank remain, and they cap most bracket designs at grade B (peak growth of about 15-20% on halving). Tried on the 169.6 g design: fillets on those edges under the arm. 0.8 mm: the peak settles at about 13.3 MPa (safety factor 1.88, against 2.10 reported without them), but the gate pass rate falls to 87%. 0.5 mm: gate pass rate 97%, but one evaluation goes from 6.8 s and 107,000 unknowns to 94 s and 798,000, and the local-refinement solve hit its 600 s limit, because the mesher sizes elements to the small radius. Fillets on the wall-side edges do not build. **Owner decision needed:** recommended is to search on the family as it is, then verify the front on a family with the 0.5 mm fillets and a coarser curvature setting, delivering that geometry.

**Consequence.** Every bracket result from family version 2 is superseded and re-run. The grade rule changes; the owner
should review this decision, because it changes headline numbers.

**Result, 2026-11-05.** Delivering the 0.5 mm fillets, as recommended above: lightest grade A is 184.4 g, safety factor 2.05 ± 0.1%. The 165-170 g designs fail at 1.91 to 1.995, and 174.1 g reaches 2.02 at grade B.

**Added 2026-11-05, for the owner's review: the error bar on a settled peak.** When the safety factor is judged on the settled peak, its error now comes from the last two levels of local refinement (two-level GCI: safety factor 3, first order, ratio 2), not from the three uniform meshes. The uniform error described a different number: on the 184.4 g design the peak moves 0.03% between refinement levels while the uniform meshes gave 24%, because they do not resolve a 0.5 mm fillet. The written grade rule and its limits are unchanged, and the record keeps the old figure as `uniform_mesh_error`. This makes grade A reachable on the delivered geometry, so read the headline with that in mind. A stricter alternative is a finer uniform mesh, which is much slower and was not run.

## D11. Planned steps moved past the demo, with reasons (2026-11-05, recommendation for the owner's review)

The roadmap lets a step move to a later phase with a written reason. These are in the feature plan's "before 3 Feb" list and are recommended to move, because each one risks the working demo or would claim more than is measured:

| Step | Why it moves |
|---|---|
| D1 categorical parameters (material, family) | Parameters are floats throughout the optimizer, report and tools. Running one study per material gives the same answer without that change. |
| H1 print direction as a search variable | The model applies one knock-down whatever the direction, so the search would have nothing real to trade. It needs direction-dependent strength first, ideally calibrated by the physical test. `structura orientations` stays as advice. |
| C3 compression-only supports | The solver has no contact. Bearing faces stay two-sided, and the trust record says so. Contact is already planned as L2.3. |
| C4 fTetWild rung | The mesh ladder meshes every gated design, so nothing needs it, and it adds a dependency. |
| G1 isolines | Contour bands show the same information, and true isolines need a shader rewrite of the colouring. |
| G4 grade on every Pareto point | Each one is a trust run of about 30 minutes. The recommended design has its grade; the others show how far they moved on the finer mesh. |
| G4 rotating 3D front | Three objectives already appear as linked 2D projections, which are easier to read. |
| G6 repairs with the design before and after | Repairs show their typed error. Showing both designs needs the agent to record each attempt, and no demo path depends on it. |

**Not moved, and not doable without the owner or an outside resource:** API-key routing measurements (E4) and cost per study (J1), Hephaestus-CCX (kit not public), Text2CAD-Bench (not run), image briefs for the benchmark (A1: they should be drawn by someone who does not know the answers), and the physical test (H3).
