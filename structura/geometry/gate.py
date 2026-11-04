"""Geometry validity gate, run on every candidate before meshing.

Hard checks (any failure rejects the shape, with a named reason): valid B-rep, exactly one solid, positive volume,
inside the design space, outside keep-out boxes, minimum wall thickness, minimum feature (edge) size, minimum hole
diameter. Print overhang is measured and reported; it rejects only when the limits demand a support-free part.
gate() never repairs anything; repair is a separate step whose result is gated again.
"""
from __future__ import annotations

import math
import random
from dataclasses import dataclass, field

import cadquery as cq
from OCP.BRepClass import BRepClass_FaceClassifier
from OCP.BRepGProp import BRepGProp_Face
from OCP.BRepIntCurveSurface import BRepIntCurveSurface_Inter
from OCP.gp import gp_Dir, gp_Lin, gp_Pnt, gp_Pnt2d, gp_Vec
from OCP.TopAbs import TopAbs_IN


@dataclass
class GateLimits:
    design_space: list[float] | None = None           # [xmin, ymin, zmin, xmax, ymax, zmax] mm
    keep_out: list[list[float]] = field(default_factory=list)
    min_wall: float = 1.2                             # mm, three FDM perimeters at 0.4 mm
    min_feature: float = 0.3                          # mm, shortest allowed edge
    min_hole_diameter: float = 2.0                    # mm
    print_direction: tuple[float, float, float] = (0.0, 0.0, 1.0)
    max_overhang_deg: float = 45.0                    # from vertical
    require_support_free: bool = False
    wall_samples: int = 200
    seed: int = 0


@dataclass
class GateResult:
    ok: bool
    reasons: list[str]
    warnings: list[str]
    volume_mm3: float
    solids: int
    min_wall_mm: float | None
    min_edge_mm: float | None
    overhang_area_mm2: float | None

    def as_dict(self):
        return self.__dict__.copy()


def _surface_samples(shape: cq.Shape, n: int, seed: int):
    """Points exactly on the B-rep faces with outward normals, sampled in proportion to face area."""
    faces = [f for f in shape.Faces() if f.Area() > 1e-9]
    if not faces:
        return []
    rng = random.Random(seed)
    picks = rng.choices(faces, weights=[f.Area() for f in faces], k=n)
    out = []
    for f in picks:
        u0, u1, v0, v1 = f._uvBounds()
        prop = BRepGProp_Face(f.wrapped)
        for _ in range(50):  # rejection sampling inside the trimmed face
            u, v = rng.uniform(u0, u1), rng.uniform(v0, v1)
            if BRepClass_FaceClassifier(f.wrapped, gp_Pnt2d(u, v), 1e-7).State() != TopAbs_IN:
                continue
            p, vec = gp_Pnt(), gp_Vec()
            prop.Normal(u, v, p, vec)
            m = vec.Magnitude()
            if m < 1e-12:
                continue
            out.append(((p.X(), p.Y(), p.Z()), (vec.X() / m, vec.Y() / m, vec.Z() / m)))
            break
    return out


def _wall_thickness(shape: cq.Shape, n: int, seed: int) -> float | None:
    best = None
    for cen, nrm in _surface_samples(shape, n, seed):
        inward = gp_Dir(-nrm[0], -nrm[1], -nrm[2])
        start = gp_Pnt(cen[0] - 1e-3 * nrm[0], cen[1] - 1e-3 * nrm[1], cen[2] - 1e-3 * nrm[2])
        inter = BRepIntCurveSurface_Inter()
        inter.Init(shape.wrapped, gp_Lin(start, inward), 1e-6)
        dmin = None
        while inter.More():
            w = inter.W()
            if w > 1e-4 and (dmin is None or w < dmin):
                dmin = w
            inter.Next()
        if dmin is not None:
            t = dmin + 1e-3
            best = t if best is None else min(best, t)
    return best


def _overhang_area(shape: cq.Shape, d, max_deg: float) -> float:
    verts, tris = shape.tessellate(0.1, 0.3)
    P = [(v.x, v.y, v.z) for v in verts]
    zmin = min(p[0] * d[0] + p[1] * d[1] + p[2] * d[2] for p in P)
    limit = math.cos(math.radians(max_deg))  # facets facing down more steeply than max_deg from vertical
    area = 0.0
    for a, b, c in tris:
        pa, pb, pc = P[a], P[b], P[c]
        u = [pb[i] - pa[i] for i in range(3)]
        w = [pc[i] - pa[i] for i in range(3)]
        nrm = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
        a2 = math.sqrt(sum(x * x for x in nrm))
        if a2 < 1e-12:
            continue
        down = -(nrm[0] * d[0] + nrm[1] * d[1] + nrm[2] * d[2]) / a2
        height = min(p[0] * d[0] + p[1] * d[1] + p[2] * d[2] for p in (pa, pb, pc))
        if down > limit and height > zmin + 1e-6:
            area += a2 / 2
    return area


def gate(shape: cq.Shape, limits: GateLimits | None = None) -> GateResult:
    lim = limits or GateLimits()
    reasons, warnings = [], []
    solids = list(shape.Solids())
    volume = sum(s.Volume() for s in solids)
    if not shape.isValid():
        reasons.append("invalid B-rep")
    if len(solids) != 1:
        reasons.append(f"{len(solids)} solids; exactly one connected solid is required")
    if volume <= 0:
        reasons.append("non-positive volume")
    min_wall = min_edge = overhang = None
    if not reasons:
        bb = shape.BoundingBox()
        if lim.design_space:
            x0, y0, z0, x1, y1, z1 = lim.design_space
            t = 1e-6
            if bb.xmin < x0 - t or bb.ymin < y0 - t or bb.zmin < z0 - t or bb.xmax > x1 + t or bb.ymax > y1 + t or bb.zmax > z1 + t:
                reasons.append(f"outside the design space: bounding box [{bb.xmin:.2f}, {bb.ymin:.2f}, {bb.zmin:.2f}, "
                               f"{bb.xmax:.2f}, {bb.ymax:.2f}, {bb.zmax:.2f}] exceeds {lim.design_space}")
        for i, ko in enumerate(lim.keep_out):
            box = cq.Solid.makeBox(ko[3] - ko[0], ko[4] - ko[1], ko[5] - ko[2], pnt=cq.Vector(ko[0], ko[1], ko[2]))
            inter = shape.intersect(box)
            if sum(s.Volume() for s in inter.Solids()) > 1e-6:
                reasons.append(f"intrudes into keep-out zone {i}")
        edges = [e.Length() for e in shape.Edges()]
        min_edge = min(edges) if edges else None
        if min_edge is not None and min_edge < lim.min_feature:
            reasons.append(f"edge of {min_edge:.3f} mm is shorter than the minimum feature size {lim.min_feature} mm")
        for f in shape.Faces():
            if f.geomType() == "CYLINDER":
                r = f._geomAdaptor().Cylinder().Radius()
                # concave cylinders (holes) have their axis outside the material: the normal points to the axis
                u0, u1, v0, v1 = f._uvBounds()
                p = f._geomAdaptor().Value((u0 + u1) / 2, (v0 + v1) / 2)
                n = f.normalAt()  # at the UV centre, oriented with the face
                ax = f._geomAdaptor().Cylinder().Axis()
                o, dv = ax.Location(), ax.Direction()
                # vector from the surface point to its foot on the axis
                w = (p.X() - o.X(), p.Y() - o.Y(), p.Z() - o.Z())
                t = w[0] * dv.X() + w[1] * dv.Y() + w[2] * dv.Z()
                foot = (o.X() + t * dv.X(), o.Y() + t * dv.Y(), o.Z() + t * dv.Z())
                to_axis = cq.Vector(foot[0] - p.X(), foot[1] - p.Y(), foot[2] - p.Z())
                wraps = (u1 - u0) >= math.pi - 1e-6  # holes wrap at least half way; corner fillets about 90 deg
                if wraps and n.dot(to_axis) > 0 and 2 * r < lim.min_hole_diameter - 1e-9:
                    reasons.append(f"hole of diameter {2 * r:.2f} mm is below the minimum {lim.min_hole_diameter} mm")
        min_wall = _wall_thickness(shape, lim.wall_samples, lim.seed)
        if min_wall is not None and min_wall < lim.min_wall:
            reasons.append(f"wall of {min_wall:.2f} mm is thinner than the minimum {lim.min_wall} mm")
        overhang = _overhang_area(shape, lim.print_direction, lim.max_overhang_deg)
        if overhang > 1e-6:
            msg = f"{overhang:.0f} mm^2 overhangs more than {lim.max_overhang_deg:g} degrees for the print direction"
            (reasons if lim.require_support_free else warnings).append(msg)
    return GateResult(ok=not reasons, reasons=reasons, warnings=warnings, volume_mm3=volume, solids=len(solids),
                      min_wall_mm=min_wall, min_edge_mm=min_edge, overhang_area_mm2=overhang)
