# Judge Q&A sheet (P7-t4)

Short answers to the questions a judge is most likely to ask, each with the file that proves it. Rule: if the
evidence is not in the repository, the answer says "not yet" instead of claiming it.

## What it is

**Q: In one sentence?**
You describe a part in plain language; the agent turns it into a checkable spec, generates the geometry, simulates it
with a real finite-element solver, optimizes it, verifies the answer with error bars and gives you an interactive
report and the files to make it.

**Q: Is the AI doing the physics?**
No. The language model writes the spec, composes the design and explains the results; every number comes from
deterministic tools it cannot edit: OpenCASCADE geometry, gmsh meshing, CalculiX solving, our C++ engine for gating,
checks and trust. No result in the report comes from a trained surrogate. (`structura/agent/mcp_server.py`, server
instructions: "Never state a number the tools did not return.")

**Q: What is actually new? SimScale and others have AI agents.**
We found no shipped product that combines, on one open stack: brief to checkable spec, numeric multi-objective
optimization with an interactive Pareto front, structural and thermal physics on the same parametric model, and a
visible trust record per design. SimScale's agent sets up and explains simulations and runs Bayesian optimization over
Onshape configurations, but does not generate geometry from scratch and shows no Pareto front (its October 2026
blog). The two closest research systems (Physics-in-the-Loop, arXiv 2605.19717; Hephaestus-CCX, arXiv 2605.17448)
close the loop on real FEA but are structural-only, without multi-objective optimization or interactive results.
Each technique exists; the contribution is the verified, explainable loop. (`docs/research/competitive-landscape-2026-11.md`)

## Can we trust the numbers?

**Q: How do you know the solver is right?**
A validation suite of 7 textbook and NAFEMS cases runs through the same engine path: LE10 +0.14%, LE1 −0.40%,
cantilever −0.80%, Euler buckling +0.06%, first natural frequency −0.26%, thermal slab exact, fin −0.002%
(`docs/validation.md`). An independent solver we wrote (tet10 on Eigen) agrees with CalculiX to 6×10⁻⁵ % on the
bracket (`docs/benchmarks.md`).

**Q: How do you know the mesh is fine enough?**
Every recommended design gets a three-level mesh convergence study (Roache's Grid Convergence Index) and error bars on
each requirement. A requirement counts as proven only if its margin exceeds its numerical error and the convergence is
well-behaved. Grade A needs every requirement proven, every independent check (equilibrium, energy balance, heat
balance) passing and errors below 5%. (`engine/src/trust.cpp`, `report.html` trust panel)

**Q: Did the optimizer really find the best design, or just a good one?**
We check: a headline result must change by less than 2% when the search budget doubles (decision D8). That rule caught
a real case: the heat sink at 150 evaluations was verified but 62% heavier than the optimum found at 300 and confirmed
at 600. (`docs/decisions.md` D8, `docs/benchmarks.md`)

**Q: Can I reproduce a result?**
Yes. `structura reproduce --dir <trust folder>` re-solves from the stored spec, parameters and hashes; all three
headline results reproduce bit-identically. (`docs/benchmarks.md`, reproducibility)

**Q: What about stress singularities at sharp corners?**
Peak stress at a sharp re-entrant corner does not converge, so it is not used for the strength check: stress within
3 mm of load and support faces is excluded (Saint-Venant) and rib tips are blunted (decision D6). When convergence is
still poor the trust record says so (the uploaded-part redesign is grade B for exactly this reason).

## Results

> **Corrected (2026-11-04, decision D10):** an earlier result here (234 g, safety factor 2.06, grade A) was wrong: its peak stress sat at a sharp corner where each rib tip met the arm, which three uniform meshes could not see. Bracket family version 3 fillets those corners and the trust record now refines locally to catch this. **The lightest design converges at 162-165 g** (192.5, 179.6, 162.2 and 164.7 g at 200, 400, 800 and 1600 evaluations), but those designs are only grade B or unverified: sharp edges along the rib flanks raise the local stress about 12%. The grade A design below is the lightest one proven so far.


**Q: Show me a result.**
- 20 kg PETG shelf bracket: the hand design (99 g) is unsafe by a factor of 4 (SF 0.48); the best verified design so far is
  179.6 g with SF 2.07 ± 0.5%, grade A; lighter designs near 162-165 g exist at grade B. (`benchmarks/p2_demo_400.json`)
- 10 W fan-cooled heat sink: 103.7 g hand design → 27.2 g (−74%), 66.3 °C against a 75 °C limit, grade A, the same at 300 and 600
  evaluations. It is the lightest corner of the design space, so the bound decides it, not the limit. (`benchmarks/heatsink_fan60_demo_300.json`)
- An uploaded over-built bracket: 354 g → 328 g (−7.3%) with its bolt holes frozen, grade B. (`benchmarks/a3_redesign_demo.json`)
- A 100 kg PETG brief: the system says no design in the family can do it (best SF 0.67 at the strongest corner) and
  that the strongest design carries about 33 kg. It never returns a design that fails. (`benchmarks/bracket_100kg_demo.json`)

**Q: Why is the optimized bracket heavier than the hand design?**
Because the hand design fails the brief. "Optimized" means the lightest design that is proven to meet every
requirement, not the lightest shape.

**Q: Has anything been tested physically?**
Not yet. The protocol, the rig and the predictions are fixed in advance: arm-end stiffness 59.8 N/mm for the hand
design against 389.9 N/mm for the recommended one (6.5×), with pass criteria. (`docs/test-protocol-h3.md`) [update
this answer when the test is done]

**Q: How often does the pipeline fail?**
Of 500 random bracket designs, 499 passed the geometry gate (the rejected one had 0.02 mm gaps) and all 499 meshed
(`docs/benchmarks.md`). Free-form LLM CAD meshed 76–88% in Physics-in-the-Loop; the typed design grammar removes that
failure class. End-to-end success on ten scripted briefs, per stage: `benchmarks/brief_suite.json`.

## AI and geometry

**Q: Does the AI generate the 3D model?**
Yes, by composing it in a typed design grammar (14 feature types with bounded parameters and named faces) rather than
writing CAD code, so syntax and API errors cannot happen and every parameter can be optimized. For shapes the grammar
cannot express, it can write CadQuery code as a last resort, behind the same geometry gate. It can propose several
candidates, rank them and look at six rendered views before spending solver time.

**Q: Which model? Does it depend on one vendor?**
The engine is model-agnostic: anything that speaks MCP can drive it. We use Claude (Opus class) for the agent. How
well a model turns briefs into specs is measured on our 50-brief benchmark (English and Arabic, `benchmarks/brief_bench/`).
[fill in the measured score]

**Q: Can it improve a part I already have?**
Yes: upload a STEP file; holes and faces are detected, the holes are frozen, the part is analysed as-is (this
reproduces the native result exactly) and redesigned. (`benchmarks/a3_redesign_demo.json`)

## Openness, cost, limits

**Q: Licences?**
Engine and agent code: ours. OpenCASCADE (LGPL-2.1 with exception, linked dynamically); gmsh and CalculiX (GPL) run as
separate programs, never linked; Optuna, Ax, BoTorch, SALib, three.js, MCP SDK: MIT; Eigen: MPL-2.0. Every licence was
checked against its LICENSE file. (`docs/licences.md`)

**Q: What does it cost to run?**
No solver licences and no training data. It runs on one desktop (the demo machine: 16-core CPU, 64 GB, RTX 5090); a
bracket evaluation takes about 1-4 s, a full study about 4-8 minutes.

**Q: What are the limits?**
- Two part families today (bracket, heat sink), plus uploaded and free-form parts analysed as fixed shapes.
- Linear elastic statics, buckling, modal and steady conduction; no contact, plasticity, fatigue or CFD yet.
- Convection uses a channel correlation at a stated fan speed, not a flow simulation.
- 3D-printed strength uses a conservative knock-down until the physical test calibrates it.
- Manufacturing costs use stated assumptions (`data/processes.json`), not quotes.

**Q: Why not just use a surrogate model like PhysicsX or SimAI?**
Surrogates need 30-100+ simulations of similar parts to train and give no error bars a judge can check. Our
evaluations are fast enough to run the real solver every time, and every result carries its numerical error.
