"""B3: the flexure family against beam theory.

A right-circular notch hinge (families/flexure.json), clamped at x = 0, 10 N down on the end face at x = 60, aluminium
6061-T6, quadratic tetrahedra. The FEA end deflection is the load-weighted mean, 2 x strain energy / force. The closed
form integrates Euler-Bernoulli bending plus Timoshenko shear (factor 6/5) along the bar with the true notch height
h(x) = t + 2R - 2 sqrt(R^2 - (x - 30)^2), using E for a narrow bar (plane stress) and E / (1 - nu^2) for a wide one
(plane strain); a real bar lies between them. Beam theory treats the notch as a slowly varying section, so the gap is
expected to grow as t/R grows.

Usage: python benchmarks/flexure_check.py [--mesh 1.0]  ->  benchmarks/flexure_check.json
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from structura import engine  # noqa: E402

L, XC, F = 60.0, 30.0, 10.0
MAT = json.loads((ROOT / "data" / "materials.json").read_text())["materials"]["AL6061T6"] if "materials" in json.loads(
    (ROOT / "data" / "materials.json").read_text()) else json.loads((ROOT / "data" / "materials.json").read_text())["AL6061T6"]
E, NU = MAT["E"], MAT["nu"]
CASES = [(1.2, 10, 10), (2, 5, 10), (2, 10, 10), (3, 5, 10), (4, 3, 10), (2, 5, 20)]  # (web t, notch R, width b)


def beam(t, R, b, Eeff):
    x = np.linspace(0, L, 600001)
    h = np.full_like(x, t + 2 * R)
    m = np.abs(x - XC) < R
    h[m] = t + 2 * R - 2 * np.sqrt(R**2 - (x[m] - XC) ** 2)
    i, a = b * h**3 / 12, b * h
    bend = np.trapezoid(F * (L - x) ** 2 / (Eeff * i), x)
    shear = np.trapezoid(1.2 * F / (Eeff / (2 * (1 + NU)) * a), x)
    return bend + shear


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mesh", type=float, default=1.0)
    a = ap.parse_args()
    rows = []
    for t, R, b in CASES:
        job = {"family": "flexure", "params": {"web_thickness": t, "notch_radius": R, "width": b}, "material": "AL6061T6",
               "mesh": {"size": min(a.mesh, t / 2), "order": 2}, "solver_threads": 4,
               "load_cases": [{"name": "tip", "supports": [{"group": "clamp_face"}], "loads": [{"group": "load_face", "force": [0, 0, -F]}]}]}
        jp = ROOT / "runs" / "flexure_check" / f"t{t}_R{R}_b{b}.json"
        jp.parent.mkdir(parents=True, exist_ok=True)
        jp.write_text(json.dumps(job))
        r = engine.call("solve", job=str(jp), out=str(jp.with_suffix("")), timeout=1800)["results"]["tip"]
        fea = 2 * r["strain_energy_Nmm"] / F
        lo, hi = beam(t, R, b, E / (1 - NU**2)), beam(t, R, b, E)
        rows.append({"web_mm": t, "notch_radius_mm": R, "width_mm": b, "t_over_R": round(t / R, 3), "nodes": r["nodes"],
                     "fea_mm": round(fea, 6), "beam_plane_strain_mm": round(lo, 6), "beam_plane_stress_mm": round(hi, 6),
                     "fea_vs_plane_stress": round(fea / hi - 1, 4), "fea_vs_plane_strain": round(fea / lo - 1, 4),
                     "equilibrium_error": r["equilibrium_error"], "seconds": r["runtime_s"]})
        print(rows[-1], flush=True)
    (ROOT / "benchmarks" / "flexure_check.json").write_text(json.dumps(
        {"material": "AL6061T6", "E_MPa": E, "nu": NU, "force_N": F, "mesh_mm": f"min({a.mesh}, web / 2)", "rows": rows}, indent=1))


if __name__ == "__main__":
    main()
