# Environment

Recorded 2026-11-04 by task F2-T1.

## Install (Windows)

1. Download micromamba 2.10.0 (single executable) from
   `https://github.com/mamba-org/micromamba-releases/releases/latest/download/micromamba-win-64`
   to `%USERPROFILE%\micromamba\micromamba.exe`.
2. Set `MAMBA_ROOT_PREFIX=%USERPROFILE%\micromamba\root`.
3. From the repository root: `micromamba create -y -f environment.yml` (took 1 min 43 s on the dev machine).
4. Run anything with `micromamba run -n attome-ae <command>`; check with `micromamba run -n attome-ae python -m pytest tests/test_smoke.py -q`.

## Versions installed (conda-forge, win-64)

| Package | Version | Notes |
|---|---|---|
| python | 3.12.16 | |
| cadquery | 2.7.0 | on OCP 7.8.1.2 / OCCT 7.8.1 |
| build123d | 0.9.1 | |
| gmsh | 4.15.0 | executable `gmsh.exe`, run as a subprocess |
| calculix | 2.23 | executable `ccx.exe`, run as a subprocess |
| meshio | 5.3.5 | |
| scikit-fem | 12.0.2 | |
| numpy | 2.5.4 | |
| scipy | 1.18.1 | |
| pydantic | 2.14.2 | |
| optuna | 5.0.0 | |
| pymoo | 0.6.2 | |
| pyvista | 0.49.2 | |
| pytest | 9.2.0 | |

## CalculiX notes found while setting up

- `ccx -v` prints the version and exits with code 201 by design; the smoke test solves a one-element cube instead.
- Version 2.23 does not create the `NALL` node set automatically; decks must define every set they reference.

## Development machine

AMD Ryzen 9 9950X3D (16 cores, 32 threads), 61.6 GB RAM, NVIDIA GeForce RTX 5090, Windows 11 Pro.
This is not the 8 GB / 4-core minimum machine that J1 promises; runtimes measured here are labelled with the
machine, and the minimum-hardware run is a separate measurement.

## C++ engine build (decision D1)

- Compiler: Visual Studio 2022 Community, MSVC 19.44.35221 (x64), Windows SDK 10.0.26100.
- Build tools from the environment: CMake (system, 4.2.0-rc1), Ninja 1.13.2, nlohmann_json 3.12.0, doctest 2.5.3;
  OpenCASCADE 7.8.1 headers, import libraries and DLLs come from the `occt` package already in the environment.
- Build: `scripts\build_engine.cmd` (Release by default). It calls `vcvars64.bat` (override with `VCVARS`) and
  configures CMake with the environment's `Library` folder as the prefix.
- Run tests: `micromamba run -n attome-ae engine\build\engine_tests.exe` (the environment puts the OCCT DLLs and
  gmsh on PATH). A single task's cases: `engine_tests.exe -tc="C1-T2*"`.
- First full build: about 2 minutes on the development machine; the engine test suite runs in about 46 s, most of it
  the meshing failure-ladder case.
