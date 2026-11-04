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

> **Corrected (2026-11-04, decision D10):** an earlier result here (234 g, safety factor 2.06, grade A) was wrong: its peak stress sat at a sharp corner where each rib tip met the arm, which three uniform meshes could not see. Bracket family version 3 fillets those corners and the trust record now refines locally to catch this. **The search is not converged yet:** 200, 400 and 800 evaluations found 192.5, 179.6 and 162.2 g, so the numbers below are the best verified so far, not the final optimum.


| Brief | Hand design | Result | Trust |
|---|---|---|---|
| 20 kg PETG shelf bracket | 99 g, safety factor 0.48 (unsafe) | 179.6 g, safety factor 2.07 ± 0.5% (search not converged; see above) | grade A |
| 10 W heat sink, small fan, below 75 °C | 103.7 g | 27.2 g (−74%), 66.3 °C, same at 300 and 600 evaluations; the lightest corner of the design space, so the limit does not govern | grade A |
| Uploaded over-built bracket, same bolt holes | 354 g | 328 g (−7.3%), from before the stricter trust check; to be re-run | grade B |
| 100 kg PETG bracket | – | reported infeasible: the family's strongest design carries about 33 kg | – |

Solver validation: 7 of 7 cases pass (NAFEMS LE10 +0.14%, LE1 −0.40%, cantilever −0.80%, buckling +0.06%, modal
−0.26%, thermal slab and fin within 0.002%). An independent in-house solver agrees with CalculiX to 6×10⁻⁵ %.
500 random designs: 499 pass the geometry gate, all of them mesh. Details: `docs/benchmarks.md`, `docs/validation.md`.

## What it does

- **Brief → spec card**: requirements with sources, load cases at their design factor, material with print knock-down;
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
- **Visualization**: a single offline HTML report (3D fields with load and support markers, side-by-side comparison,
  Pareto explorer, parallel coordinates, history, trust panel, PDF export) and a hands-on workbench with live
  optimization.

## Quick start (Windows)

```
micromamba create -f environment.yml          # conda-forge: OpenCASCADE, gmsh, CalculiX, Optuna, Ax, ...
scripts\build_engine.cmd                       # C++ engine -> engine\build\structura.exe
micromamba run -n attome-ae python -m structura.app          # hands-on workbench in the browser
micromamba run -n attome-ae python scripts/demo.py preflight # check the machine
micromamba run -n attome-ae python scripts/demo.py live      # the 20 kg brief end to end (about 8 minutes)
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
| `families/` | part families as design graphs (bracket, heat sink) |
| `data/` | materials, manufacturing rates, vendored three.js |
| `docs/` | feature plan, roadmap, decisions, benchmarks, validation, licences, proposal, judge Q&A |
| `benchmarks/` | scripts and stored results behind every number |

## Licences

Engine and agent code are this project's. OpenCASCADE (LGPL-2.1 with exception) is linked dynamically; gmsh and
CalculiX (GPL) run as separate programs and are never linked; the Python libraries are MIT or similar. Every licence
was checked against its LICENSE file: `docs/licences.md`.
