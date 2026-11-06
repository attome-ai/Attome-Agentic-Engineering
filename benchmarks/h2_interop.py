"""H2: every delivery format opened by a program we did not write, and measured against the CAD solid.

STL and GLB: Blender's own importers (benchmarks/h2_blender_check.py, run headless). 3MF: lib3mf, the 3MF Consortium's
reference library. DXF: ezdxf's reader and auditor. STEP: re-read by CadQuery, which shares the OCCT kernel with the
engine, so it is a weaker check. PDF: structure only (header, trailer, page count); no outside PDF reader is installed.

Usage: python benchmarks/h2_interop.py  ->  benchmarks/h2_interop.json (design: delivered bracket, thickness 11.78)
"""
import json
import re
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from structura import engine  # noqa: E402

BLENDER = Path(r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe")
OUT = ROOT / "runs" / "h2_interop"
SETS = ["thickness=11.78"]


def mesh_volume(v, t):
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    return abs(np.einsum("ij,ij->i", a, np.cross(b, c)).sum() / 6)


def read_3mf(p):
    import lib3mf
    w = lib3mf.get_wrapper()
    m = w.CreateModel()
    m.QueryReader("3mf").ReadFromFile(str(p))
    it = m.GetMeshObjects()
    it.MoveNext()
    mo = it.GetCurrentMeshObject()
    v = np.array([[x.Coordinates[0], x.Coordinates[1], x.Coordinates[2]] for x in mo.GetVertices()])
    t = np.array([[x.Indices[0], x.Indices[1], x.Indices[2]] for x in mo.GetTriangleIndices()])
    bi = m.GetBuildItems()
    bi.MoveNext()
    tr = bi.GetCurrent().GetObjectTransform().Fields
    M = np.array([[tr[r][c] for c in range(3)] for r in range(4)])  # 3MF: row vectors, last row the translation
    vw = v @ M[:3] + M[3]
    return {"reader": f"lib3mf {'.'.join(map(str, w.GetLibraryVersion()[:3]))}", "vertices": len(v), "triangles": len(t),
            "manifold": bool(mo.IsManifoldAndOriented()), "volume_mm3": round(mesh_volume(v, t), 1),
            "bbox_min_mm": vw.min(0).round(3).tolist(), "bbox_max_mm": vw.max(0).round(3).tolist(), "unit": m.GetUnit()}


def read_dxf(p):
    import ezdxf
    doc = ezdxf.readfile(str(p))
    aud = doc.audit()
    kinds = {}
    for e in doc.modelspace():
        kinds[e.dxftype()] = kinds.get(e.dxftype(), 0) + 1
    return {"reader": f"ezdxf {ezdxf.__version__}", "dxf_version": doc.dxfversion, "entities": kinds,
            "audit_errors": len(aud.errors), "audit_fixes": len(aud.fixes)}


def read_pdf(p):
    b = p.read_bytes()
    return {"header": b[:8].decode("latin1"), "ends_with_eof": b.rstrip().endswith(b"%%EOF"),
            "pages": len(re.findall(rb"/Type\s*/Page[^s]", b)), "bytes": len(b)}


def read_step(p):
    import cadquery as cq
    s = cq.importers.importStep(str(p)).val()
    return {"reader": "CadQuery (OCCT, same kernel as the engine)", "valid": bool(s.isValid()), "solids": len(s.Solids()),
            "volume_mm3": round(s.Volume(), 3)}


def main():
    ref = engine.call("build", family="bracket_delivered", set=SETS)
    engine.call("export", family="bracket_delivered", set=SETS, out=str(OUT))
    bj = OUT / "blender.json"
    bj.unlink(missing_ok=True)
    subprocess.run([str(BLENDER), "-b", "--factory-startup", "--python", str(ROOT / "benchmarks" / "h2_blender_check.py"), "--",
                    str(OUT), str(bj)], capture_output=True, timeout=600)
    bl = json.loads(bj.read_text())
    V = ref["volume_mm3"]
    res = {"design": {"family": "bracket_delivered", "set": SETS, "brep_volume_mm3": round(V, 3), "bbox_mm": ref["bbox"]},
           "step": read_step(OUT / "part.step"), "stl_blender": bl["part.stl"], "glb_blender": bl["part.glb"],
           "3mf": read_3mf(OUT / "part.3mf"), "dxf": read_dxf(OUT / "drawing.dxf"), "pdf": read_pdf(OUT / "drawing.pdf"),
           "blender": bl["blender"]}
    res["stl_blender"]["volume_error"] = round(bl["part.stl"]["volume"] / V - 1, 6)
    res["glb_blender"]["volume_error"] = round(bl["part.glb"]["volume"] * 1e9 / V - 1, 6)  # metres^3 -> mm^3
    res["3mf"]["volume_error"] = round(res["3mf"]["volume_mm3"] / V - 1, 6)
    res["step"]["volume_error"] = round(res["step"]["volume_mm3"] / V - 1, 9)
    (ROOT / "benchmarks" / "h2_interop.json").write_text(json.dumps(res, indent=1))
    print(json.dumps(res, indent=1))


if __name__ == "__main__":
    main()
