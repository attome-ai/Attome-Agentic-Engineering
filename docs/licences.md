# Licences

Verified 2026-11-04 by task J2-T1 from each project's LICENSE file (URL below) and cross-checked against the
conda-forge package metadata of the installed version. Re-check when a version changes.

## Corrections to the research

None. Every licence the research marked as unverified (‡) matches the LICENSE file. One detail added: OCCT is
LGPL-2.1 **only** (with the Open CASCADE exception), not "or later".

## Rules

- **GPL tools run only as subprocesses.** CalculiX (`ccx.exe`) and gmsh (`gmsh.exe`) are called as separate
  programs exchanging files. Neither the gmsh library nor its Python module is linked or imported (decision D2).
- **OCCT is LGPL-2.1**: the C++ engine links it dynamically (DLLs from the environment), which keeps the engine's
  own licence free to choose; distributing the engine means shipping the OCCT DLLs with their licence and source offer. This keeps
  the Attome AE core free to use any licence.
- **No AGPL dependencies**, and nothing that is not listed in this file.
- **Distributing binaries** of ccx or gmsh (for example in an installer) requires offering their source.
- This is the project's reading of the licences, not legal advice.

## Dependencies

| Package | Version | Licence | LICENSE file | Checked |
|---|---|---|---|---|
| Python | 3.12.16 | PSF (Python-2.0) | conda-forge metadata | 2026-11-04 |
| CadQuery | 2.7.0 | Apache-2.0 | https://raw.githubusercontent.com/CadQuery/cadquery/master/LICENSE | 2026-11-04 |
| build123d | 0.9.1 | Apache-2.0 | https://raw.githubusercontent.com/gumyr/build123d/dev/LICENSE | 2026-11-04 |
| OCP | 7.8.1.2 | Apache-2.0 | https://raw.githubusercontent.com/CadQuery/OCP/master/LICENSE | 2026-11-04 |
| OCCT (Open CASCADE) | 7.8.1 | LGPL-2.1-only with Open CASCADE exception 1.0 | https://raw.githubusercontent.com/Open-Cascade-SAS/OCCT/master/LICENSE_LGPL_21.txt and OCCT_LGPL_EXCEPTION.txt | 2026-11-04 |
| gmsh | 4.15.0 | GPL-2.0-or-later with exceptions (subprocess only) | https://gmsh.info/LICENSE.txt | 2026-11-04 |
| CalculiX ccx | 2.23 | GPL-2.0 (conda-forge: GPL-2.0-or-later) (subprocess only) | https://raw.githubusercontent.com/Dhondtguido/CalculiX/master/LICENSE | 2026-11-04 |
| meshio | 5.3.5 | MIT | https://raw.githubusercontent.com/nschloe/meshio/main/LICENSE.txt | 2026-11-04 |
| scikit-fem | 12.0.2 | BSD-3-Clause | https://raw.githubusercontent.com/kinnala/scikit-fem/master/LICENSE | 2026-11-04 |
| NumPy | 2.5.4 | BSD-3-Clause | https://raw.githubusercontent.com/numpy/numpy/main/LICENSE.txt | 2026-11-04 |
| SciPy | 1.18.1 | BSD-3-Clause | https://raw.githubusercontent.com/scipy/scipy/main/LICENSE.txt | 2026-11-04 |
| Pydantic | 2.14.2 | MIT | https://raw.githubusercontent.com/pydantic/pydantic/main/LICENSE | 2026-11-04 |
| Optuna | 5.0.0 | MIT | https://raw.githubusercontent.com/optuna/optuna/master/LICENSE | 2026-11-04 |
| pymoo | 0.6.2 | Apache-2.0 | https://raw.githubusercontent.com/anyoptimization/pymoo/main/LICENSE | 2026-11-04 |
| PyVista | 0.49.2 | MIT | https://raw.githubusercontent.com/pyvista/pyvista/main/LICENSE | 2026-11-04 |
| VTK | 9.3.1 | BSD-3-Clause | https://gitlab.kitware.com/vtk/vtk/-/raw/master/Copyright.txt | 2026-11-04 |
| pytest | 9.2.0 | MIT | https://raw.githubusercontent.com/pytest-dev/pytest/main/LICENSE | 2026-11-04 |
| nlohmann/json | 3.12.0 | MIT | https://raw.githubusercontent.com/nlohmann/json/develop/LICENSE.MIT | 2026-11-04 |
| doctest | 2.5.3 | MIT | https://raw.githubusercontent.com/doctest/doctest/master/LICENSE.txt | 2026-11-04 |
| Ninja | 1.13.2 | Apache-2.0 | https://raw.githubusercontent.com/ninja-build/ninja/master/COPYING | 2026-11-04 |
| Eigen (in-house solver, F2-T4/D3-T3) | 5.0.1 | MPL-2.0 | https://gitlab.com/libeigen/eigen/-/raw/master/COPYING.MPL2 | 2026-11-04 |
| Ax (ax-platform) | 1.2.4 | MIT | https://raw.githubusercontent.com/facebook/Ax/main/LICENSE | 2026-11-04 |
| BoTorch | 0.17.2 | MIT | https://raw.githubusercontent.com/pytorch/botorch/main/LICENSE | 2026-11-04 |
| PyTorch | 2.8.0 | BSD-3-Clause | https://raw.githubusercontent.com/pytorch/pytorch/main/LICENSE | 2026-11-04 |
| SALib | 1.6.0 | MIT | https://raw.githubusercontent.com/SALib/SALib/main/LICENSE.md | 2026-11-04 |
| three.js (report viewer, vendored UMD build) | 0.149.0 | MIT | https://raw.githubusercontent.com/mrdoob/three.js/dev/LICENSE | 2026-11-04 |
| plotly.js (dashboard) | not yet installed | MIT | https://raw.githubusercontent.com/plotly/plotly.js/master/LICENSE | 2026-11-04 |

Planned later and to be verified when added: FastAPI (I2), the MCP Python SDK (E1),
Netgen and fTetWild (C4 fallbacks), OpenFOAM, SU2 and XLB (C5).

## Excluded

| Project | Licence | Why excluded | Source |
|---|---|---|---|
| FluidX3D | Custom | "Commercial use is not allowed. Military use is not allowed." and "You may not train AI models on the source code of this software" | https://raw.githubusercontent.com/ProjectPhysX/FluidX3D/master/LICENSE.md |
| JAX-FEM | GPL-3.0 | Would be imported in process (not a subprocess); the research found commercial use needs contacting the author | https://raw.githubusercontent.com/deepmodeling/jax-fem/main/LICENSE |
| TetGen | AGPL-3.0 (per research, not re-fetched) | AGPL: no AGPL dependencies | research report |
| Palabos | AGPL-3.0 (per research, not re-fetched) | AGPL: no AGPL dependencies | research report |
| PrusaSlicer, CuraEngine | AGPL-3.0 (per research, not re-fetched) | Never bundled or hosted; may only be called if the user has installed them (H1, after 3 Feb) | research report |
