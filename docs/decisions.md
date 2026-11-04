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
