# Attome Agentic Engineering

Describe a part in plain language; get back an optimized design that has been checked with real physics simulation,
with the evidence to trust it and an interactive report to explore it.

> "A wall shelf bracket that holds 20 kg, as light as possible, printed in PETG."

Claude (or any MCP client) turns the brief into a checkable spec, composes the geometry, and drives a C++ engine that
gates, meshes and solves every candidate with CalculiX, searches for the best trade-off, verifies the answer with
numerical error bars and writes the report. The language model never produces a number: every result comes from a
deterministic tool it cannot edit.

Entry for AIDTC 2027 (Gulf University, Bahrain). Status: working prototype; see `docs/proposal/proposal-draft.md`.

## Results (measured, each traceable to a stored run)

> **Corrected (2026-11-04, decision D10):** an earlier result here (234 g, safety factor 2.06, grade A) was wrong: its peak stress sat at a sharp corner where each rib tip met the arm, which three uniform meshes could not see. Bracket family version 3 fillets those corners and the trust record now refines locally to catch this. Those lighter numbers did not survive: 162-165 g designs from the search have sharp edges along the rib flanks, and with the 0.5 mm fillets a printed part needs there they fall below the required safety factor (164.9 g: 1.91; 168.6 to 170.2 g: 1.96 to 1.99), and the earlier 179.6 g design no longer passes the geometry gate. **The result below is the delivered geometry, fillets included: 184.4 g, grade A.** A 174.1 g design reaches 2.02, grade B, because its peak still moves 2.4% under refinement. Its error bar comes from local refinement (decision D10, for the owner's review); the uniform meshes alone gave 24%. This front comes from the 1600-evaluation search, and the 2% budget rule has not been re-checked on the delivered geometry.


| Brief | Hand design | Result | Trust |
|---|---|---|---|
| 20 kg PETG shelf bracket | 99 g, safety factor 0.48 (unsafe) | 184.4 g with its fillets, safety factor 2.05 ± 0.1% (see above) | grade A |
| 10 W heat sink, small fan, below 75 °C | 103.7 g | 27.2 g (−74%), 66.3 °C, same at 300 and 600 evaluations; the lightest corner of the design space, so the limit does not govern | grade A |
| Uploaded over-built bracket, same bolt holes | 372 g | 316.9 g (−14.8%), safety factor 2.38 ± 7%; the lightest search result, 305.9 g, failed at 1.96 once its peak settled | grade B |
| 30 W heat sink, same fan, below 75 °C | 103.7 g | 44.1 g (−57%), 74.4 °C: the limit decides it. Not converged by the 2% budget rule; with 12 or more fins nothing under 42.8 g passes | grade A |
| 100 kg PETG bracket | – | reported infeasible: the family's strongest design carries about 38 kg | – |

Solver validation: 7 of 7 cases pass (NAFEMS LE10 +0.14%, LE1 −0.40%, cantilever −0.80%, buckling +0.06%, modal
−0.26%, thermal slab and fin within 0.002%). An independent in-house solver agrees with CalculiX to 6×10⁻⁵ %.
500 random designs: 499 pass the geometry gate, all of them mesh. Details: `docs/benchmarks.md`, `docs/validation.md`.

## What it does

- **Brief → spec card**: from text or a picture (a sketch's accuracy is not measured yet); requirements with sources, load cases at their design factor, material with print knock-down;
  infeasible briefs are detected before any search and a relaxation is proposed.
- **Geometry**: a typed design grammar (14 feature types, bounded parameters, named faces) on OpenCASCADE; best-of-N
  candidates and a six-view visual review; uploaded STEP parts (holes detected and frozen, redesigned); free-form
  CadQuery as a gated last resort.
- **Physics**: CalculiX static stress, buckling, modal and steady heat transfer; convection from a fin-channel
  correlation; geometry gate and repair; mesh fallback ladder.
- **Optimization**: NSGA-II or BoTorch chosen from the measured cost of an evaluation; Pareto fronts; steering by
  preference; a result counts as converged only if doubling the budget changes it by less than 2%.
- **Trust**: three-level mesh convergence with error bars per requirement, equilibrium, energy and heat-balance
  checks, validation suite status, an A/B/C grade, a `reproduce` command and a hash-chained audit log.
- **Manufacturing**: FDM and 3-axis CNC rules (support volume, undercuts, internal corners), time and cost from stated
  rates, process advice; STEP, STL and VTU export.
- **Visualization**: a single offline HTML report (3D fields with load and support markers, cut plane, mode shapes,
  side-by-side comparison, Pareto explorer, parallel coordinates, history, trust panel, PDF export) and a hands-on
  workbench with live optimization. Replay the search through real rebuilt geometry, or click any agent step to see the
  spec and design as they stood then.
- **Hands on**: served by the REST door, the report lets you drag a target on the Pareto chart to steer the search and
  click a face to add a load or support; the spec records the face so it is found again when the design changes, and
  a new spec version needs a new acceptance.

## Quick start (Windows)

```
micromamba create -f environment.yml          # conda-forge: OpenCASCADE, gmsh, CalculiX, Optuna, Ax, ...
scripts\build_engine.cmd                       # C++ engine -> engine\build\structura.exe
micromamba run -n attome-ae python -m structura.app          # hands-on workbench in the browser
micromamba run -n attome-ae python scripts/demo.py preflight # check the machine
micromamba run -n attome-ae python scripts/demo.py live      # the 20 kg brief end to end (about 8 minutes)
```

On Linux or macOS, or to avoid installing anything but Docker (a 4-core, 8 GB limit is measured in `docs/benchmarks.md`):

```
docker build -t attome-ae .
docker run --rm -p 127.0.0.1:8765:8765 attome-ae python -m structura.app.rest --host 0.0.0.0 --port 8765
```

With Claude Code in this folder, the MCP server in `.mcp.json` gives the agent the engine's tools; ask it to design
a part.

Useful engine commands: `structura validate`, `structura evaluate --spec docs/examples/bracket_20kg.json --set
thickness=8`, `structura trust ...`, `structura manufacture ...`, `structura inspect --step part.step`,
`structura reproduce --dir runs/p2_demo/trust`.

## Layout

| Path | Contents |
|---|---|
| `engine/` | C++20 engine: grammar, gate, mesh, solve, trust, manufacturing, inspection, CLI, tests |
| `structura/` | Python: optimizers, MCP agent server, reports, workbench |
| `families/` | part families as design graphs (bracket, delivered bracket, heat sink, notch flexure) |
| `data/` | materials, manufacturing rates, vendored three.js |
| `docs/` | feature plan, roadmap, decisions, benchmarks, validation, licences, proposal, judge Q&A |
| `benchmarks/` | scripts and stored results behind every number |

## Licences

Engine and agent code are this project's. OpenCASCADE (LGPL-2.1 with exception) is linked dynamically; gmsh and
CalculiX (GPL) run as separate programs and are never linked; the Python libraries are MIT or similar. Every licence
was checked against its LICENSE file: `docs/licences.md`.
