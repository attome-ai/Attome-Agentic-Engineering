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
