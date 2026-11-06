"""Chamfer distance and IoU against Text2CAD-Bench reference shapes, once they are available.

Put the reference STEP files in runs/text2cad/gold/<id>.step (the benchmark gives them on request only). Then
`python benchmarks/text2cad/metrics.py` scores every answer that has a reference and adds the results to
benchmarks/text2cad_l1.json.

Chamfer distance follows the paper: 30,000 points sampled uniformly on each surface, both shapes scaled into a unit
bounding box, the mean nearest-neighbour distance taken in both directions and added, times 1000. The paper does not say
whether the distances are squared; they are not squared here. IoU is exact (OCCT boolean volumes in the prompt's own
millimetre frame), not the paper's 256^3 voxels, which differ slightly on thin walls.

`--selftest` checks the code: a shape against itself (CD 0, IoU 1) and a 10 mm cube against one shifted 5 mm (IoU 1/3).
"""
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
GOLD = ROOT / "runs" / "text2cad" / "gold"


def _tris(shape, tol=0.05):
    verts, tris = shape.tessellate(tol, 0.2)
    v = np.array([[p.x, p.y, p.z] for p in verts])
    return v, np.array(tris)


def sample(shape, n=30000, seed=0):
    v, t = _tris(shape)
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    area = 0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
    rng = np.random.default_rng(seed)
    k = rng.choice(len(t), n, p=area / area.sum())
    u, w = rng.random(n), rng.random(n)
    flip = u + w > 1
    u[flip], w[flip] = 1 - u[flip], 1 - w[flip]
    return a[k] + u[:, None] * (b[k] - a[k]) + w[:, None] * (c[k] - a[k])


def _unit(p):
    lo, hi = p.min(0), p.max(0)
    return (p - (lo + hi) / 2) / max(hi - lo)


def chamfer(s1, s2):
    from scipy.spatial import cKDTree
    p, q = _unit(sample(s1)), _unit(sample(s2, seed=1))
    return 1000 * (cKDTree(q).query(p)[0].mean() + cKDTree(p).query(q)[0].mean())


def iou(s1, s2):
    inter = s1.intersect(s2).Volume()
    return inter / (s1.Volume() + s2.Volume() - inter)


def selftest():
    import cadquery as cq
    a = cq.Workplane("XY").box(10, 10, 10).val()
    b = a.translate(cq.Vector(5, 0, 0))
    assert abs(iou(a, a) - 1) < 1e-9 and abs(iou(a, b) - 1 / 3) < 1e-9
    # two independent 30k samples of one unit cube: point spacing about sqrt(6 / 30000) = 0.014, so CD about 14, not 0;
    # this floor is in the paper's numbers too
    assert chamfer(a, a) < 20, chamfer(a, a)
    print("selftest ok: IoU 1 and 1/3; CD of a shape with itself", round(chamfer(a, a), 2))


def main():
    import cadquery as cq
    res_path = ROOT / "benchmarks" / "text2cad_l1.json"
    res = json.loads(res_path.read_text())
    scored = 0
    for r in res["rows"]:
        g, s = GOLD / f"{r['id']}.step", ROOT / "runs" / "text2cad" / r["id"] / "part.step"
        if r["valid"] and g.exists() and s.exists():
            a, b = cq.importers.importStep(str(s)).val(), cq.importers.importStep(str(g)).val()
            r["chamfer_x1000"], r["iou"] = round(chamfer(a, b), 2), round(iou(a, b), 4)
            scored += 1
    if not scored:
        print(f"no reference shapes in {GOLD}; nothing scored")
        return
    sc = [r for r in res["rows"] if "iou" in r]
    res["metrics"] = {"scored": scored, "mean_chamfer_x1000": round(float(np.mean([r["chamfer_x1000"] for r in sc])), 2),
                      "mean_iou": round(float(np.mean([r["iou"] for r in sc])), 3)}
    res_path.write_text(json.dumps(res, indent=1))
    print(res["metrics"])


if __name__ == "__main__":
    selftest() if "--selftest" in sys.argv else main()
