"""Predicted displacement at the load point of a stored evaluation, for comparison with a physical test (H3).

Reads the solver deck (job.inp, *CLOAD lines) and the stored nodal displacements (fields.bin) of one load case and
returns the force-weighted mean displacement of the loaded nodes, sum(F_i . u_i) / |F|: the displacement that does the
work of the applied load, which is what a gauge on the load line measures. Stiffness = |F| / that displacement.

Usage: python benchmarks/gauge_point.py BUNDLE_DIR [BUNDLE_DIR ...] [--case service] [--json OUT]
"""
import argparse
import json
import struct
from pathlib import Path


def read_fields(path: Path) -> dict:
    b = path.read_bytes()
    assert b[:4] == b"SFLD", f"{path}: not a fields.bin file"
    (n,) = struct.unpack_from("<I", b, 4)
    out, off = {}, 8
    for _ in range(n):
        nid, ux, uy, uz, _vm = struct.unpack_from("<i4d", b, off)
        out[nid] = (ux, uy, uz)
        off += 36
    return out


def read_cloads(path: Path) -> dict:
    loads, on = {}, False
    for line in path.read_text().splitlines():
        if line.startswith("*"):
            on = line.upper().startswith("*CLOAD")
            continue
        if on and line.strip():
            nid, dof, val = (x.strip() for x in line.split(",")[:3])
            loads.setdefault(int(nid), [0.0, 0.0, 0.0])[int(dof) - 1] += float(val)
    return loads


def gauge(bundle: Path, case: str = "service") -> dict:
    loads = read_cloads(bundle / f"solve_{case}" / "job.inp")
    disp = read_fields(bundle / f"result_{case}" / "fields.bin")
    f = [sum(v[i] for v in loads.values()) for i in range(3)]
    fmag = sum(x * x for x in f) ** 0.5
    work = sum(sum(F[i] * disp[n][i] for i in range(3)) for n, F in loads.items())
    d = work / fmag  # displacement along the load direction
    ev = json.loads((bundle / "evaluation.json").read_text())
    return {"bundle": str(bundle), "load_case": case, "force_N": fmag, "load_nodes": len(loads),
            "gauge_disp_mm": d, "stiffness_N_per_mm": fmag / d if d else None,
            "max_disp_mm": ev["metrics"].get("max_disp_mm"), "mass_g": ev["metrics"].get("mass_g"),
            "safety_factor": ev["metrics"].get("safety_factor")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bundles", nargs="+")
    ap.add_argument("--case", default="service")
    ap.add_argument("--json")
    a = ap.parse_args()
    rows = [gauge(Path(b), a.case) for b in a.bundles]
    for r in rows:
        print(f"{r['bundle']}: F {r['force_N']:.1f} N, gauge {r['gauge_disp_mm']:.4f} mm, "
              f"stiffness {r['stiffness_N_per_mm']:.1f} N/mm (max disp {r['max_disp_mm']:.4f} mm)")
    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=2))


if __name__ == "__main__":
    main()
