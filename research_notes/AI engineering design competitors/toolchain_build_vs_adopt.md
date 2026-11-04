# Open-Source Engineering Toolchain for an LLM-Agent Design/Simulate/Optimize Platform: Build vs Adopt (as of Nov 2026)

Scope note: this research ran on a limited tool budget (about 20 calls). Licenses marked **[verified]** were confirmed from the official LICENSE/README or official site in this session. Licenses marked **[prior knowledge, re-check]** come from the researcher's background knowledge (cutoff mid-2026) and were NOT re-fetched; the report writer should present them as "to be confirmed from the repo LICENSE file before shipping". All runtimes not cited are labelled ESTIMATE.

## Q1. CAD kernels / geometry (OCCT via CadQuery vs build123d vs FreeCAD; implicit/SDF: libfive, sdf, PicoGK)

### Takeaway
Adopt build123d (Apache-2.0, on OCCT/OCP) as the agent-facing parametric CAD language; it already has a sandboxed MCP server with a published (author-reported) benchmark gain. Add PicoGK (Apache-2.0, voxel/OpenVDB, actively released in 2026) only if lattices/implicit geometry become part of the demo. FreeCAD is too heavy to embed for a solo dev's agent loop.

### Cited Findings
- build123d-mcp is an open-source MCP server (PyPI) that lets an LLM create build123d models, render previews, measure geometry, fix mistakes and export STEP/STL/SVG/DXF; it ships with "defence-in-depth sandboxing" rather than raw exec() — [PyPI build123d-mcp](https://pypi.org/project/build123d-mcp/); [GitHub pzfreo/build123d-mcp](https://github.com/pzfreo/build123d-mcp)
- Author-reported (not independently verified): on the CADGenBench leaderboard (June 2026), GPT-5.5 driven through build123d-mcp scored 0.457 vs 0.360 when writing scripts without the feedback loop — [GitHub pzfreo/build123d-mcp](https://github.com/pzfreo/build123d-mcp)
- PicoGK (LEAP 71) is listed as Apache-2.0 for both PicoGK (C#) and PicoGKRuntime (C++), built on OpenVDB and GLFW; latest release reported as v2.2.0 (5 June 2026). Verified only via aggregators/mirrors, not the official LICENSE page — [awesome.ecosyste.ms](https://awesome.ecosyste.ms/projects/github.com%2Fleap71%2FPicoGK); [ecosyste.ms repos](https://repos.ecosyste.ms/hosts/GitHub/repositories/leap71%2FPicoGK); [PicoGKRuntime mirror](https://github.imc.re/leap71/PicoGKRuntime); [gitpedia](https://gitpedia.pages.dev/wiki/leap71/PicoGK)
- Licenses [prior knowledge, re-check]: OCCT = LGPL-2.1 with an additional OCCT exception (dev.opencascade.org); CadQuery = Apache-2.0; build123d = Apache-2.0; OCP bindings = Apache-2.0; FreeCAD = LGPL-2.0-or-later; libfive = MPL-2.0 for the core library, GPL-2+ for Studio GUI; fogleman/sdf = MIT.

### Inferences
- Comparison table (licenses as flagged above):

| Tool | License | Strength | Python API | Maturity | Windows | Verdict |
|---|---|---|---|---|---|---|
| build123d (OCCT) | Apache-2.0 / OCCT LGPL | B-rep, STEP export, algebra+builder modes; LLM-friendly code; MCP exists | native | active, 0.x-1.x | pip wheels (OCP) | **ADOPT (primary)** |
| CadQuery (OCCT) | Apache-2.0 | fluent API, larger corpus of examples online (better LLM priors) | native | mature | conda/pip | Adopt as fallback; same kernel |
| FreeCAD | LGPL-2+ | full GUI app, FEM workbench (CalculiX/Elmer) | embedded Python, heavy | mature (1.x) | yes | Do not embed; use for manual inspection only |
| PicoGK | Apache-2.0 (aggregator-verified) | voxel/implicit, lattices, robust booleans | C# (no first-class Python) | active (v2.2.0 Jun 2026) | yes (C#/.NET) | Optional phase-2 for lattices |
| libfive | MPL-2.0 core | F-rep/SDF meshing | Python bindings | slow-moving | builds | Optional |
| fogleman/sdf | MIT | simple SDF->mesh in Python | native | hobby-grade | yes | Prototyping lattices only |
- OCCT LGPL is fine for hosted SaaS and for dynamic linking in a shipped product; only modifications to OCCT itself must be released.
- LLM priors: CadQuery has more public example code; build123d has the MCP + sandbox already. Recommend build123d with a CadQuery fallback since both produce OCP shapes.

### Gaps
- Did not fetch official LICENSE files for OCCT, CadQuery, build123d, FreeCAD, libfive this session.
- No independent replication of the CADGenBench number.

## Q2. Meshing (Gmsh, Netgen, TetGen, fTetWild, meshio)

### Takeaway
Use Gmsh (GPL-2+) as an out-of-process tool for the demo — fine for hosted SaaS, but for a shipped/on-prem product either keep it as a separate executable or switch to Netgen (LGPL) via its Python API. Use fTetWild (MPL-2.0) as a robustness fallback for dirty/STL geometry; avoid TetGen (AGPL) in any hosted product.

### Cited Findings
- Gmsh is GPL v2 or later, with an exception allowing combination with Netgen, METIS, OpenCASCADE and ParaView code; "people who make modified versions of Gmsh are not obligated to grant this special exception". The LICENSE text itself mentions no commercial license option [verified] — [gmsh.info LICENSE.txt](https://gmsh.info/LICENSE.txt)
- Feenox's LE10 benchmark notes that converting a Gmsh mesh to CalculiX format took more than five minutes on the fine mesh (excluded from timings) and needed Gmsh >= 4.9 — [Feenox NAFEMS LE10](https://www.seamplex.com/feenox/tests/nafems/le10/)
- Licenses [prior knowledge, re-check]: Netgen/NGSolve = LGPL-2.1; TetGen 1.6 = AGPL-3.0 (commercial licence sold by WIAS); fTetWild/TetWild = MPL-2.0; meshio = MIT; pygmsh = GPL-3.0 (note: pygmsh wrapper itself is GPL).

### Inferences
| Tool | License | Robustness on generated CAD | Python API | Windows | Verdict |
|---|---|---|---|---|---|
| Gmsh | GPL-2+ (verified) | Very good on OCCT STEP (uses OCCT natively); 2nd-order tets, size fields, physical groups | official `gmsh` pip wheel | yes | **ADOPT for demo, call via subprocess / separate process** |
| Netgen (ngsolve/netgen-mesher) | LGPL-2.1 | good on OCCT geometry; native OCC import | `netgen` pip | yes | **Commercial-safe alternative** |
| fTetWild | MPL-2.0 | best for broken/self-intersecting STL ("mesh anything") but slower, linear tets | wrappers (pytetwild) | builds | Fallback for SDF/lattice output |
| TetGen | AGPL-3 | fast, quality tets from clean PLC | tetgen (pyvista) | yes | Avoid for hosted product (AGPL network clause) |
| meshio | MIT | format conversion | native | yes | Adopt |
- GPL implication: running Gmsh as a server-side process in SaaS triggers no distribution obligation (GPL, unlike AGPL, has no network clause). Importing the `gmsh` Python module into your own process and then *distributing* that program arguably makes the combined work GPL — keep it in a separate worker process with file/CLI boundaries if a downloadable product is ever shipped. (Legal interpretation, not legal advice.)
- Avoid the slow-path conversion seen in Feenox: write CalculiX `.inp` directly from Gmsh node/element arrays (meshio or custom writer) — this is part of the thin custom layer.

### Gaps
- No published quantitative robustness comparison (failure rate) of Gmsh vs Netgen vs fTetWild on LLM-generated CAD was found.

## Q3. Structural/thermal FEA solvers (coverage, verification, speed, Python, license, adjoint/AD)

### Takeaway
For the Feb 2027 demo, adopt **CalculiX (GPL, subprocess)** as the "real physics, industry-recognised" solver for linear static, modal, buckling, nonlinear/contact, steady/transient heat and thermo-mechanical — it is the only candidate that covers all of these out-of-the-box with an Abaqus-like input deck an LLM can write. Pair it with **scikit-fem (BSD)** as an in-process fast path / cross-check for linear elasticity and heat. Defer differentiable solvers (JAX-FEM is GPL-3; FEniCSx+dolfin-adjoint is LGPL but weak on Windows) to topology optimization later.

### Cited Findings
- JAX-FEM: GPL v3; "For commercial use, contact Tianju Xue"; supports 2D/3D elements, Dirichlet/Neumann/Robin BCs, heat, elasticity, hyperelasticity, plasticity, multiphysics, PETSc integration; examples include topology optimization and optimal thermal control; paper in Computer Physics Communications (2023) — [GitHub deepmodeling/jax-fem](https://github.com/deepmodeling/jax-fem); [PyPI jax-fem](https://pypi.org/project/jax-fem)
- NVIDIA Warp moved to Apache-2.0 (previously a source-visible proprietary licence); FEM examples (warp.fem) live in the same repo under warp/examples/fem — [heise](https://heise.de/-10381872); [GitHub NVIDIA/warp](https://github.com/Nvidia/warp)
- deal.II: 9.5/9.6 documentation describes LGPL; the repo LICENSE.md says the project is "in the process of relicensing" to dual Apache-2.0-with-LLVM-exception + LGPL-2.1-or-later. No confirmation found of the release in which the switch completed — [deal.II LICENSE.md](https://www.github.com/dealii/dealii/blob/master/LICENSE.md)
- Elmer: GPL overall with ElmerSolver library claimed as LGPL-2.1, but the per-module license file groups the fem kernel under GPL-2+; sources conflict — [elmerfem mirror](https://github.com/luisjoserod/elmerfem); [OSArch wiki](https://wiki.osarch.org/elmer/); contradicted by [ELMER-LICENSES file](https://www.github.com/go-smart/goosefoot/blob/master/ELMER-LICENSES)
- FEniCSx on Windows: native conda-forge win-64 builds exist (fenics-libdolfinx 0.10.0, April 2026) but are described as beta/"highly experimental" and lack PETSc/petsc4py on Windows; latest stable FEniCSx is 0.11 (June 2026) — [conda-forge listing](https://anaconda.org/conda-forge/fenics-libdolfinx/files?channel=main&page=30); [FEniCS Discourse](https://fenicsproject.discourse.group/t/making-fenicsx-more-available-on-windows-machines/14327); [fenicsproject.org/download](https://fenicsproject.org/download/)
- DOLFINx pyamg demo solves linear elasticity with smoothed-aggregation AMG using rigid-body modes as near-nullspace (relevant to non-PETSc Windows use) — [DOLFINx pyamg demo](https://docs.fenicsproject.org/dolfinx/main/python/demos/demo_pyamg.html)
- Verification pedigree: NAFEMS LE10 reference σyy = -5.38 MPa at point D. Feenox's LE10 parametric study compares FeenoX, Sparselizard, code_aster and CalculiX (CalculiX run with Spooles, iterative scaling and iterative Cholesky), but the summary page does not list CalculiX numbers; FeenoX reached -5.379 MPa on a 205k-node mesh — [Feenox LE10](https://www.seamplex.com/feenox/tests/nafems/le10/); [Fino LE10 case](https://www.seamplex.com/fino/cases/012-nafems-le10)
- A CalculiX theory-benchmark sheet (Starnini, 2013) includes NAFEMS LE1 among its cases — [mecway forum upload](https://mecway.com/forum/uploads/FileUpload/a1/8bcdebc1a1dcb6638a62ede72c6ba5.pdf)
- CalculiX MCP: a FastMCP server in "CAE-Agent-Hub" reportedly parses .inp decks, edits design variables, runs ccx, reads .dat, exports result_mesh.json, extracts modal frequencies/mode shapes, and runs two-stage sizing; the same hub has a FreeCAD -> Elmer -> ParaView workflow. Aggregator listing showed 0 tools, so status unclear — [TypingMind MCP listing](https://www.typingmind.com/mcp/calculix)
- Licenses [prior knowledge, re-check]: CalculiX = GPL-2; FEniCSx/dolfinx = LGPL-3.0+; Firedrake = LGPL-3; pyadjoint/dolfin-adjoint = LGPL-3; scikit-fem = BSD-3; MFEM = BSD-3; NGSolve = LGPL-2.1; code_aster = GPL-3; Kratos = BSD (exact variant unverified); MOOSE = LGPL-2.1; SfePy = BSD-3; torch-fem = MIT; PyMAPDL = MIT wrapper but requires proprietary licensed ANSYS MAPDL (not open source).

### Inferences
| Solver | License | Lin. static | Nonlin/contact | Modal/buckling | Thermal (steady/transient) | Thermo-mech | AD/adjoint | Speed notes | Python | Windows | Verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|
| CalculiX ccx | GPL-2 | yes | yes (plasticity, contact) | yes/yes | yes/yes | yes (coupled) | no | Direct: SPOOLES/PARDISO/PaStiX; multithreaded | file I/O (.inp/.frd); pygccx, ccx2paraview | prebuilt Windows binaries (bConverged, Mecway, FreeCAD bundle) | **ADOPT (primary, subprocess)** |
| scikit-fem | BSD-3 | yes | DIY | DIY (scipy eigsh) | yes | DIY | no (but simple to hand-derive adjoint for compliance) | SciPy direct or pyamg; pure-Python assembly vectorised | native, tiny deps | pip, excellent | **ADOPT (fast path + verification cross-check)** |
| FEniCSx | LGPL-3 | yes | yes (via UFL, user-built) | yes (SLEPc) | yes | yes | via pyadjoint (legacy-oriented) | excellent with PETSc; MPI | native | beta, no PETSc on Windows | Use via WSL2/Docker if needed |
| Firedrake + pyadjoint | LGPL-3 | yes | yes | yes | yes | yes | **yes, mature adjoint** | PETSc | native | Linux/WSL only | Later, for shape/TO gradients |
| JAX-FEM | GPL-3 (commercial: contact author) | yes | hyperelastic, plasticity | limited | yes | yes | **yes (JAX AD)** | GPU | native | JAX CPU on Windows; GPU via WSL | Research only; license blocks shipping |
| torch-fem | MIT | yes | plasticity (some) | ? | yes | ? | **yes (PyTorch AD)** | GPU | native | pip | Watch; good permissive differentiable option |
| warp.fem | Apache-2.0 | yes (user-built) | DIY | DIY | yes | DIY | yes (Warp AD) | NVIDIA GPU | native | yes | Only if GPU available |
| MFEM | BSD-3 | yes | yes | yes | yes | yes | partial | GPU (CUDA/HIP), HPC | PyMFEM | hard to build on Windows | Overkill for demo |
| deal.II | LGPL-2.1 (moving to Apache-2.0+LLVM dual) | yes | yes | yes | yes | yes | Sacado AD | HPC | none official | WSL | Overkill (C++) |
| NGSolve | LGPL-2.1 | yes | yes | yes | yes | yes | symbolic shape derivatives | good | native | pip on Windows | **Strong permissive alternative** to CalculiX if a GPL-free core is needed |
| Elmer | GPL/LGPL (conflicting) | yes | yes | yes | yes | yes | no | MPI | weak | Windows installers | Thermal/multiphysics alternative |
| code_aster | GPL-3 | yes | best-in-class | yes | yes | yes | no | MUMPS/PETSc | Python command files | Linux (Windows via salome_meca container) | Too heavy for solo dev |
| Kratos | BSD | yes | yes | yes | yes | yes | adjoint apps exist | MPI | native | Windows wheels | Possible, steep learning curve |
| MOOSE | LGPL-2.1 | yes | yes | yes | yes | yes | limited | HPC | input files | Linux/WSL | Overkill |
| SfePy | BSD-3 | yes | some | yes | yes | yes | no | SciPy/PETSc | native | pip | Alternative to scikit-fem |
- Why CalculiX first: input decks are Abaqus-like text — highly LLM-writable (LLMs have seen many Abaqus examples), covers every phase-1/phase-2 physics, widely used in FreeCAD/PrePoMax/Mecway, and has prior NAFEMS-style benchmark usage. Downside: no gradients, file I/O overhead, GPL.
- GPL via subprocess: invoking `ccx` as a separate executable with file-based I/O is generally treated as "mere aggregation", so the orchestration code can stay proprietary; distributing ccx binaries requires shipping/offering its source. Hosted SaaS has no source-disclosure trigger for GPL (it would for AGPL). (Interpretation, not legal advice.)
- Permissive fallback if GPL becomes a problem commercially: NGSolve (LGPL) or scikit-fem (BSD) with your own verified element library.

### Gaps
- No CalculiX-specific LE1/LE10 error numbers retrieved (they exist in Feenox detailed report pages and the Starnini sheet, not opened).
- torch-fem physics coverage and license not verified this session.
- No official NAFEMS-certified validation report found for any open-source code in this session.

## Q4. CFD (later phase)

### Takeaway
For later CFD: OpenFOAM (GPL, subprocess) for general RANS; SU2 (LGPL) when adjoint shape optimization is wanted; XLB (Apache-2.0, JAX/Warp) for GPU lattice-Boltzmann. **Do not use FluidX3D** — its license forbids commercial use, military use, and AI-training use. Avoid Palabos (AGPL) for a hosted product.

### Cited Findings
- FluidX3D license [verified]: commercial use not allowed without explicit permission (no selling or paid service deriving value from it); military use forbidden; redistribution/modification allowed with conditions (altered source must be published if binaries/results of altered versions are published); explicitly forbids use of the code to train AI models — [FluidX3D LICENSE.md](https://github.com/ProjectPhysX/FluidX3D/blob/master/LICENSE.md)
- XLB (Autodesk): Apache-2.0 badge; backends JAX (CUDA/TPU, distributed multi-GPU), NVIDIA Warp (single-GPU, "state-of-the-art single-GPU performance"), Neon (multi-GPU / grid refinement, Linux, custom Warp fork); ~518 stars; no tagged releases — [GitHub Autodesk/XLB](https://github.com/Autodesk/XLB)
- Licenses [prior knowledge, re-check]: OpenFOAM = GPL-3; SU2 = LGPL-2.1 with discrete adjoint; Basilisk = GPL-3; Palabos = AGPL-3.

### Inferences
| Tool | License | Physics | GPU | Adjoint | Windows | Verdict |
|---|---|---|---|---|---|---|
| OpenFOAM | GPL-3 | broad FV, RANS/LES, CHT | limited | adjointOptimisation (ESI) | WSL/Docker/blueCFD | Later; subprocess |
| SU2 | LGPL-2.1 | compressible/incompressible RANS, CHT | partial | **yes, discrete adjoint** | Windows binaries | Later; best for shape opt |
| XLB | Apache-2.0 | LBM | **yes** | JAX-differentiable | via WSL for GPU | Later; GPU path |
| FluidX3D | non-commercial, no-AI-training | LBM | yes (OpenCL) | no | yes | **Excluded (license)** |
| Palabos | AGPL-3 | LBM | some | no | builds | Avoid (AGPL) |
| Basilisk | GPL-3 | adaptive FV | no | no | Linux | Niche |

### Gaps
- Did not verify OpenFOAM/SU2/Palabos/Basilisk licence files this session; SU2 GPU status in 2026 not checked.

## Q5. Topology optimization libraries and density-to-CAD

### Takeaway
For the demo, a home-grown SIMP loop on scikit-fem or a structured-grid voxel solver (the classic 88/top3d formulation) is the lowest-risk path; DL4TO (Apache-2.0) and PyTopo3D (MIT per index) are permissive reference implementations. Convert density fields to geometry via marching cubes -> smoothing -> STL/voxel->CAD (e.g., dl4to4ocp to OCP/build123d, MIT).

### Cited Findings
- DL4TO (deep learning for 3D topology optimization, PyTorch): Apache License per README — [GitHub dl4to/dl4to](https://github.com/dl4to/dl4to); [awesome.ecosyste.ms](https://awesome.ecosyste.ms/projects/github.com%2Fdl4to%2Fdl4to)
- dl4to4ocp (bridge from DL4TO to OCP/CadQuery/build123d) listed as MIT — [awesome.ecosyste.ms dl4to4ocp](https://awesome.ecosyste.ms/projects/github.com%2Fyeicor-3d%2Fdl4to4ocp)
- PyTopo3D (Kim & Kang, arXiv:2504.05604, April 2025), code at github.com/jihoonkim888/PyTopo3D; package listed as MIT with ~19 stars (unverified from LICENSE) — [arXiv 2504.05604](https://arxiv.org/abs/2504.05604v1); [ecosyste.ms pytopo3d](https://packages.ecosyste.ms/registries/pypi.org/packages/pytopo3d)
- JAX-FEM includes topology-optimization examples (GPL-3) — [GitHub deepmodeling/jax-fem](https://github.com/deepmodeling/jax-fem)

### Inferences
- TopOpt-in-PETSc (Aage et al.) and TOuNN licences were not verified; treat as unknown.
- Density->CAD pipeline (custom layer): threshold/filter density -> marching cubes (scikit-image, BSD) -> Taubin smoothing -> re-mesh with fTetWild -> re-verify with CalculiX. Producing editable B-rep from densities remains an open problem; for the demo, deliver STL + verified re-analysis rather than B-rep.

### Gaps
- No comparison of TO library speed or quality found.

## Q6. Optimization (pymoo, Optuna, BoTorch/Ax, surrogates)

### Takeaway
Use pymoo (NSGA-II/III, Apache-2.0) for the multi-objective outer loop with cheap evaluations, and Optuna (MIT) for its storage/dashboard and trial history when evaluations are expensive; add BoTorch/Ax (qNEHVI) only when each FEA costs >~10 s and budgets are <200 evaluations. SMT (BSD) for surrogate models.

### Cited Findings
- No new sources were fetched for this question in this session (tool budget). Licenses [prior knowledge, re-check]: pymoo = Apache-2.0; Optuna = MIT (NSGA-II/III, MOTPE/TPE multi-objective, GPSampler in 4.x); BoTorch = MIT; Ax = MIT; SMT = BSD-3; scikit-learn = BSD-3.

### Inferences
- Optuna's RDB storage gives "design history" for free (SQLite), useful for the browser history panel; pymoo gives richer MO algorithms and Pareto utilities. They can coexist (Optuna stores trials; pymoo for final non-dominated sorting).
- Speed: optimizer overhead is negligible relative to FEA except for BoTorch GP fitting (seconds per iteration, grows with trial count) — ESTIMATE.

### Gaps
- No benchmark sources retrieved for pymoo vs Optuna vs BoTorch.

## Q7. Visualization (Three.js, vtk.js, trame, PyVista->glTF, Plotly)

### Takeaway
Three.js (MIT) with a custom per-vertex scalar colormap shader is the most flexible for a polished demo; vtk.js (BSD-3) gives FEA-native features (scalar bars, warp-by-vector, clipping) with less work; trame (Apache-2.0) is fastest to build but ties the UI to a Python server session. Plotly (MIT) for Pareto fronts.

### Cited Findings
- The CalculiX MCP in CAE-Agent-Hub exports a viewer-renderable result_mesh.json and mode shapes, showing the "export surface mesh + nodal fields as JSON" pattern — [TypingMind MCP listing](https://www.typingmind.com/mcp/calculix)
- Licenses [prior knowledge, re-check]: Three.js = MIT; vtk.js = BSD-3; trame = Apache-2.0; PyVista = MIT; VTK = BSD-3; Plotly.js/plotly.py = MIT.

### Inferences
- Efficient browser fields: extract only the boundary (skin) triangles of the tet mesh server-side (PyVista `extract_surface`), send positions + per-node scalar arrays (von Mises, |u|, T) as binary Float32 (glTF with custom attributes or raw ArrayBuffers), colour in a shader with a 1D LUT texture; displacement scaling done in the vertex shader. For 500k-DOF models the skin is typically 1-2 orders smaller than the volume — ESTIMATE.

### Gaps
- No performance benchmarks of vtk.js vs Three.js for large FE fields found.

## Q8. Agent integration (MCP servers, LLM control)

### Takeaway
MCP servers already exist for build123d (sandboxed, benchmarked) and CalculiX (CAE-Agent-Hub, maturity unclear). Code-as-CAD plus text-deck solvers is the right abstraction for LLMs; the platform's own MCP tools should expose high-level, schema-validated operations (generate_part, mesh, solve_static, solve_thermal, evaluate_design) rather than raw solver decks.

### Cited Findings
- build123d-mcp supports Claude, Cursor, VS Code, Continue, Cline, Codex CLI; renders previews, measures, exports — [GitHub pzfreo/build123d-mcp](https://github.com/pzfreo/build123d-mcp)
- CalculiX FastMCP server and FreeCAD->Elmer->ParaView agent hub — [TypingMind MCP listing](https://www.typingmind.com/mcp/calculix)

### Inferences
- Agent reliability comes from tight feedback (render + measure + validation errors), which is what build123d-mcp's reported gain reflects; replicate the same pattern for simulation (sanity checks: reaction-force balance, mass, max displacement vs beam theory).

### Gaps
- Did not inspect the CAE-Agent-Hub GitHub repo directly (licence, activity).

## Q9. Build vs adopt

### Takeaway
**Do not build a general FEA engine.** Adopt CalculiX (+ scikit-fem cross-check), Gmsh/Netgen, build123d, pymoo/Optuna, Three.js/vtk.js. **Build a thin "engine" layer** (the actual product moat): typed design/result schema, orchestration, parameter caching, verification harness, sanity checks, and agent tools. A small custom linear-tet elasticity + heat kernel is feasible as a *verification/fast-path* component, not a replacement.

### Cited Findings
- scikit-fem's own README notes that the linear solve usually dominates total time, so Python assembly is not the bottleneck — [GitHub kinnala/scikit-fem](https://github.com/kinnala/scikit-fem)
- Elasticity AMG needs the six rigid body modes in 3D to converge well — [arXiv 1907.04417](https://arxiv.org/pdf/1907.04417); demonstrated in [DOLFINx pyamg demo](https://docs.fenicsproject.org/dolfinx/main/python/demos/demo_pyamg.html)
- NAFEMS LE10 reference value -5.38 MPa usable as a regression target; FeenoX on a 20k-DOF tet mesh gave about -5.416 MPa (~0.7% error) — [Feenox LE10](https://www.seamplex.com/feenox/tests/nafems/le10/)

### Inferences
- ESTIMATE of solo-dev effort: linear tet10 elasticity + steady heat in NumPy/SciPy (or wrapping scikit-fem) with BCs, loads, von Mises recovery: ~1-2 weeks; transient heat: +2-4 days; verification suite (patch test, cantilever vs Euler-Bernoulli/Timoshenko, LE1 elliptic membrane, LE10 thick plate, analytical thick cylinder, 1D slab conduction): ~1 week. Modal, buckling, contact, plasticity, thermo-mechanical coupling to production quality: months each — this is where adopting CalculiX wins decisively.
- What to write (value-add): (1) DesignSpec/ResultSchema (JSON) independent of solver; (2) CAD->mesh->deck adapters with physical-group tagging for BCs; (3) content-hash cache (geometry params -> mesh -> results); (4) verification harness running LE1/LE10/cantilever in CI and cross-checking CalculiX vs scikit-fem on every new physics feature; (5) sanity checks per run (equilibrium, units, mesh quality, convergence); (6) agent MCP tools; (7) field export (skin mesh + binary arrays).
- What to wrap: CalculiX (subprocess), Gmsh (separate process), pymoo/Optuna.
- What to adopt as-is: build123d/OCCT, meshio, PyVista, Three.js/vtk.js, Plotly.

### Gaps
- No published effort data for solo FEA development; estimates are reasoned.

## Q10. Efficiency (runtimes 50k-500k DOF; strategies)

### Takeaway
No controlled published benchmark compares CalculiX vs FEniCSx vs scikit-fem at 50k-500k DOF on a laptop. ESTIMATE: with a multithreaded direct solver (PARDISO/PaStiX), 50k DOF solves in ~1 s and 500k DOF in roughly 10-60 s and several GB RAM; scikit-fem with SciPy's default SuperLU will be much slower and memory-hungry at 500k (prefer pyamg+CG with rigid-body modes, or pypardiso). Mesh/I-O overhead often rivals solve time at these sizes.

### Cited Findings
- CalculiX solver anecdotes conflict: a TUM thesis reports PaStiX (GPU-enhanced) 2.6-12.7x faster than the earlier PARDISO path depending on the deck; other users report MKL PARDISO 2x faster than PaStiX, PaStiX only faster up to ~1M nodes, and PaStiX needing more memory; SPOOLES limited to "a few million equations" — [TUM thesis](https://mediatum.ub.tum.de/doc/1575696/1575696.pdf); [CalculiX forum: PaStiX vs PARDISO](https://calculix.discourse.group/t/pastix-vs-pardiso-performance/177); [CalculiX forum: SPOOLES vs PARDISO](https://calculix.discourse.group/t/spooles-vs-pardiso-performance/203/5); [CalculiX forum: PaStiX performance](https://calculix.discourse.group/t/pastix-solver-performance/2525)
- Forum claim of up to 8x speed-up for static contact with PaStiX on GPU (needs high-end GPU; unconfirmed) — [CalculiX forum](https://calculix.discourse.group/t/pastix-solver-performance/2525)
- FEniCSx guidance: ~100k-1M DOFs per process for good parallel efficiency (anecdotal); assemble and factorise once for repeated linear solves with MUMPS — [FEniCS Discourse](https://fenicsproject.discourse.group/t/improving-parallel-scalability-of-linear-elasticity-code-in-fenicsx/17862)
- FeenoX LE10 fine mesh (205,441 nodes, ~616k DOF): 1 m 34 s wall time — a real data point for a comparable code — [Feenox LE10](https://www.seamplex.com/feenox/tests/nafems/le10/)
- scikit-fem's own small Laplace benchmark: 8,000 DOF solve ~0.16 s with SciPy direct — [scikit-fem fork README](https://github.com/gatling-nrl/scikit-fem)
- An OpenBenchmarking dolfinx profile (Jan 2026) reports elasticity runs of ~26-61 s but without DOF counts — [OpenBenchmarking dolfinx](https://mail.openbenchmarking.org/test/pts/dolfinx&eval=338b515a30e9df39c82bb97214b59ac9faaa71d0)

### Inferences
- Strategies: (1) coarse-to-fine — optimize on coarse linear-tet meshes (~20-50k DOF), verify Pareto candidates on fine quadratic meshes; (2) reuse factorization when only loads change (scikit-fem/SciPy `splu`/pypardiso; not available through ccx's file interface); (3) parallel evaluations — run N ccx processes each with 1-2 threads (better throughput than one process with many threads at these sizes, ESTIMATE); (4) cache by parameter hash; (5) surrogate (GP/SMT) after ~50-100 true evaluations; (6) early rejection via cheap checks (mass, volume, beam-theory estimate).
- Benchmark task for the team: run LE10 at 3 mesh densities on the actual Windows laptop with ccx (SPOOLES vs PARDISO build) and scikit-fem (pypardiso / pyamg) to get real numbers for the proposal.

### Gaps
- No direct, controlled CalculiX vs FEniCSx vs scikit-fem runtime comparison exists in the sources found; all laptop-runtime figures above except FeenoX's are estimates.
