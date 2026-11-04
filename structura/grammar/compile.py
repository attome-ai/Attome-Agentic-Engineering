"""Compile a DesignGraph into a CadQuery solid and resolve its named faces.

compile_graph() never catches CadQuery or OCCT errors to hide them: a failure is raised as CompileError naming the
feature that caused it, so the agent's repair loop gets a precise message.
"""
from __future__ import annotations

from dataclasses import dataclass, field

import cadquery as cq

from .expr import evaluate
from .features import AXES, FEATURES
from .graph import DesignGraph, FaceSelector


class CompileError(RuntimeError):
    def __init__(self, feature_id: str, message: str):
        super().__init__(f"feature {feature_id!r}: {message}")
        self.feature_id = feature_id


@dataclass
class CompiledPart:
    shape: cq.Shape
    values: dict[str, float]
    graph: DesignGraph
    prims: dict[str, list] = field(default_factory=dict, repr=False)

    def solids(self) -> list[cq.Solid]:
        return list(self.shape.Solids())

    def named_faces(self) -> dict[str, list[cq.Face]]:
        return {name: select_faces(self.shape, sel, self.values) for name, sel in self.graph.faces.items()}

    def design_space(self) -> list[float] | None:
        ds = self.graph.design_space
        return [evaluate(c, self.values) for c in ds] if ds else None


def _apply(part, solid, op, fid):
    try:
        if part is None:
            if op == "cut":
                raise CompileError(fid, "cannot cut before any material exists")
            return solid
        out = part.fuse(solid) if op == "add" else part.cut(solid)
        return out.clean()
    except CompileError:
        raise
    except Exception as e:  # OCCT boolean failure
        raise CompileError(fid, f"boolean {op} failed: {e}") from e


def compile_graph(graph: DesignGraph, overrides: dict[str, float] | None = None) -> CompiledPart:
    resolved = graph.resolve(overrides)
    values = graph.values(overrides)
    part = None
    prims: dict[str, list] = {}
    for f, v in resolved:
        ft = FEATURES[f.type]
        try:
            if ft.modifies:
                if part is None:
                    raise CompileError(f.id, "needs an existing part")
                part = ft.build(part, v)
                prims[f.id] = []
                continue
            new = ft.build(prims[v["target"]], v) if ft.pattern else ft.build(v)
        except CompileError:
            raise
        except Exception as e:
            raise CompileError(f.id, str(e) or type(e).__name__) from e
        prims[f.id] = new
        for solid, op in new:
            part = _apply(part, solid, op, f.id)
    if part is None:
        raise CompileError(graph.features[-1].id, "graph produced no geometry")
    solids = part.Solids()
    if len(solids) == 1:
        part = solids[0]
    return CompiledPart(shape=part, values=values, graph=graph, prims=prims)


def select_faces(shape: cq.Shape, sel: FaceSelector, values: dict[str, float], tol: float = 1e-4) -> list[cq.Face]:
    region = [evaluate(c, values) for c in sel.region] if sel.region else None

    def inside(c):
        if region is None:
            return True
        x0, y0, z0, x1, y1, z1 = region
        return x0 - tol <= c.x <= x1 + tol and y0 - tol <= c.y <= y1 + tol and z0 - tol <= c.z <= z1 + tol

    out = []
    if sel.kind == "plane":
        n = cq.Vector(*AXES[sel.normal])
        at = evaluate(sel.at, values)
        sign = 1 if not sel.normal.startswith("-") else -1
        for f in shape.Faces():
            if f.geomType() != "PLANE":
                continue
            c = f.Center()
            if f.normalAt(c).dot(n) < 1 - 1e-6:
                continue
            coord = c.dot(n) * sign  # coordinate along the unsigned axis
            if abs(coord - at) <= tol * max(1.0, abs(at)) and inside(c):
                out.append(f)
    else:
        a = cq.Vector(*AXES[sel.axis])
        r = evaluate(sel.radius, values)
        for f in shape.Faces():
            if f.geomType() != "CYLINDER":
                continue
            ad = f._geomAdaptor()
            cyl = ad.Cylinder()
            d = cyl.Axis().Direction()
            if abs(abs(d.X() * a.x + d.Y() * a.y + d.Z() * a.z) - 1) > 1e-6:
                continue
            if abs(cyl.Radius() - r) > tol * max(1.0, r):
                continue
            if inside(f.Center()):
                out.append(f)
    return out
