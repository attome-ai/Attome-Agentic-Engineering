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
- The 8 GB / 4-core minimum machine (J1) has not been measured yet.

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

## P2 exit demo: 20 kg bracket brief (`benchmarks/p2_demo.py`, bracket family v2)

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
