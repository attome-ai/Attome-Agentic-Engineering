"""The grammar's feature types: parameter schemas with physical bounds, and how each one builds geometry.

All lengths are millimetres, angles degrees. A feature produces one or more primitive solids that are added to or
cut from the part. Edge features (fillet, chamfer) and shell modify the part itself. Pattern features repeat the
primitives of an earlier feature.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Callable

import cadquery as cq

AXES = {"x": (1, 0, 0), "y": (0, 1, 0), "z": (0, 0, 1), "-x": (-1, 0, 0), "-y": (0, -1, 0), "-z": (0, 0, -1)}

# kind -> (low, high, unit) physical bounds checked after expressions are evaluated
KINDS = {
    "length": (0.05, 5000.0, "mm"),
    "pos": (-5000.0, 5000.0, "mm"),
    "count": (1, 500, "1"),
    "angle": (0.0, 360.0, "deg"),
}


@dataclass
class P:
    kind: str                     # length | pos | count | angle | axis | op | points | vec | bbox | ref
    doc: str
    default: object = None        # None means required


@dataclass
class FeatureType:
    name: str
    doc: str
    params: dict[str, P]
    build: Callable = field(repr=False)
    modifies: bool = False        # True for fillet, chamfer, shell (act on the current part)
    pattern: bool = False         # True for linear_pattern, circular_pattern


def V(t):
    return cq.Vector(*t)


def _box(x, y, z, lx, ly, lz):
    return cq.Solid.makeBox(lx, ly, lz, pnt=V((x, y, z)))


def _cyl(c, r, h, axis):
    return cq.Solid.makeCylinder(r, h, pnt=V(c), dir=V(AXES[axis]))


def _orth(axis):
    """Two unit directions orthogonal to an axis, in a fixed order."""
    a = axis.lstrip("-")
    return {"x": ((0, 1, 0), (0, 0, 1)), "y": ((1, 0, 0), (0, 0, 1)), "z": ((1, 0, 0), (0, 1, 0))}[a]


def _add(v, w, s=1.0):
    return tuple(a + s * b for a, b in zip(v, w))


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


# ------------------------------------------------------------------ builders: (values) -> [(solid, op)]

def b_plate(v):
    return [(_box(v["x"], v["y"], v["z"], v["length"], v["width"], v["thickness"]), v["op"])]


def b_extrude_sketch(v):
    pts = [tuple(p[:2]) for p in v["points"]]
    wp = cq.Workplane("XY", origin=(0, 0, v["z"])).polyline(pts).close().extrude(v["height"])
    return [(wp.val(), v["op"])]


def b_boss(v):
    return [(_cyl((v["x"], v["y"], v["z"]), v["diameter"] / 2, v["height"], v["axis"]), "add")]


def b_hole(v):
    return [(_cyl((v["x"], v["y"], v["z"]), v["diameter"] / 2, v["depth"], v["axis"]), "cut")]


def b_hole_pattern(v):
    u, w = _orth(v["axis"])
    out = []
    for i in range(int(v["n1"])):
        for j in range(int(v["n2"])):
            c = _add(_add((v["x"], v["y"], v["z"]), u, i * v["d1"]), w, j * v["d2"])
            out.append((_cyl(c, v["diameter"] / 2, v["depth"], v["axis"]), "cut"))
    return out


def b_rib(v):
    a, b = AXES[v["leg_a_dir"]], AXES[v["leg_b_dir"]]
    if abs(sum(x * y for x, y in zip(a, b))) > 1e-9:
        raise ValueError("rib legs must be perpendicular")
    n = _cross(a, b)
    o = (v["x"], v["y"], v["z"])
    pts = [V(o), V(_add(o, a, v["leg_a"])), V(_add(o, b, v["leg_b"]))]
    wire = cq.Wire.makePolygon(pts, close=True)
    return [(cq.Solid.extrudeLinear(wire, [], V(tuple(c * v["thickness"] for c in n))), "add")]


def b_pocket(v):
    s = _box(v["x"], v["y"], v["z_top"] - v["depth"], v["length"], v["width"], v["depth"] + 1e-3)
    r = v["corner_radius"]
    if r > 0:
        if r >= min(v["length"], v["width"]) / 2:
            raise ValueError("pocket corner_radius must be less than half the pocket's smaller side")
        edges = [e for e in s.Edges() if abs(abs(e.tangentAt(0.5).z) - 1) < 1e-6]
        s = s.fillet(r, edges)
    return [(s, "cut")]


def b_sweep_path(v):
    pts = [V(tuple(p)) for p in v["points"]]
    if len(pts) < 2:
        raise ValueError("sweep_path needs at least two points")
    path = cq.Wire.makePolygon(pts, close=False)
    d = (pts[1] - pts[0]).normalized()
    plane = cq.Plane(origin=pts[0], normal=d)
    prof = cq.Workplane(plane).circle(v["radius"])
    s = prof.sweep(cq.Workplane().add(path), transition="round").val()
    return [(s, v["op"])]


def b_fin_array(v):
    out = []
    for i in range(int(v["count"])):
        out.append((_box(v["x"], v["y"] + i * v["pitch"], v["z"], v["length"], v["fin_thickness"], v["height"]), "add"))
    return out


def _edges_in(part, axis, box):
    xmin, ymin, zmin, xmax, ymax, zmax = box
    a = V(AXES[axis])
    out = []
    for e in part.Edges():
        if e.geomType() != "LINE":
            continue
        t = e.tangentAt(0.5)
        if abs(abs(t.dot(a)) - 1) > 1e-6:
            continue
        c = e.Center()
        if xmin <= c.x <= xmax and ymin <= c.y <= ymax and zmin <= c.z <= zmax:
            out.append(e)
    return out


def m_fillet(part, v):
    edges = _edges_in(part, v["edge_axis"], v["region"])
    if not edges:
        raise ValueError("fillet selects no edges")
    return part.fillet(v["radius"], edges)


def m_chamfer(part, v):
    edges = _edges_in(part, v["edge_axis"], v["region"])
    if not edges:
        raise ValueError("chamfer selects no edges")
    return part.chamfer(v["length"], None, edges)


def m_shell(part, v):
    n = V(AXES[v["open_side"]])
    faces = [f for f in part.Faces() if f.geomType() == "PLANE" and f.normalAt().dot(n) > 1 - 1e-6]
    if not faces:
        raise ValueError("shell finds no face on the open side")
    best = max(f.Center().dot(n) for f in faces)
    faces = [f for f in faces if abs(f.Center().dot(n) - best) < 1e-6]
    return part.shell(faces, -v["thickness"])


def p_linear(prims, v):
    out = []
    for k in range(1, int(v["count"])):
        d = V((k * v["dx"], k * v["dy"], k * v["dz"]))
        out += [(s.translate(d), op) for s, op in prims]
    return out


def p_circular(prims, v):
    out = []
    n = int(v["count"])
    step = v["angle"] / n if abs(v["angle"] - 360) < 1e-9 else v["angle"] / max(n - 1, 1)
    c, ax = V((v["cx"], v["cy"], v["cz"])), V(AXES[v["axis"]])
    for k in range(1, n):
        out += [(s.rotate(c, c + ax, k * step), op) for s, op in prims]
    return out


OP = P("op", "add or cut", "add")
FEATURES: dict[str, FeatureType] = {f.name: f for f in [
    FeatureType("plate", "Rectangular block from its minimum corner.", {
        "x": P("pos", "min corner x"), "y": P("pos", "min corner y"), "z": P("pos", "min corner z"),
        "length": P("length", "size along x"), "width": P("length", "size along y"),
        "thickness": P("length", "size along z"), "op": OP}, b_plate),
    FeatureType("extrude_sketch", "Closed polygon in an XY plane at height z, extruded along +z.", {
        "points": P("points", "polygon corners [[x, y], ...]"), "z": P("pos", "sketch plane height"),
        "height": P("length", "extrusion height"), "op": OP}, b_extrude_sketch),
    FeatureType("boss", "Cylinder added to the part.", {
        "x": P("pos", "base centre x"), "y": P("pos", "base centre y"), "z": P("pos", "base centre z"),
        "diameter": P("length", "diameter"), "height": P("length", "height"), "axis": P("axis", "direction", "z")}, b_boss),
    FeatureType("hole", "Cylindrical hole cut from the part.", {
        "x": P("pos", "entry centre x"), "y": P("pos", "entry centre y"), "z": P("pos", "entry centre z"),
        "diameter": P("length", "diameter"), "depth": P("length", "depth"), "axis": P("axis", "drilling direction", "-z")}, b_hole),
    FeatureType("hole_pattern", "Grid of holes: n1 along the first and n2 along the second axis orthogonal to the drilling axis.", {
        "x": P("pos", "first hole x"), "y": P("pos", "first hole y"), "z": P("pos", "first hole z"),
        "diameter": P("length", "diameter"), "depth": P("length", "depth"), "axis": P("axis", "drilling direction", "-z"),
        "n1": P("count", "holes along the first direction", 1), "d1": P("pos", "spacing along the first direction", 0.0),
        "n2": P("count", "holes along the second direction", 1), "d2": P("pos", "spacing along the second direction", 0.0)},
        b_hole_pattern),
    FeatureType("rib", "Triangular gusset with its right-angle corner at (x, y, z), legs along two perpendicular axes, extruded by thickness along their cross product.", {
        "x": P("pos", "corner x"), "y": P("pos", "corner y"), "z": P("pos", "corner z"),
        "leg_a": P("length", "first leg length"), "leg_a_dir": P("axis", "first leg direction"),
        "leg_b": P("length", "second leg length"), "leg_b_dir": P("axis", "second leg direction"),
        "thickness": P("length", "rib thickness")}, b_rib),
    FeatureType("pocket", "Rectangular pocket cut downward from z_top, with optional rounded vertical corners.", {
        "x": P("pos", "min corner x"), "y": P("pos", "min corner y"), "z_top": P("pos", "top of the pocket"),
        "length": P("length", "size along x"), "width": P("length", "size along y"), "depth": P("length", "depth"),
        "corner_radius": P("pos", "vertical corner radius (0 for sharp)", 0.0)}, b_pocket),
    FeatureType("fillet", "Rounds straight edges parallel to edge_axis whose midpoints lie inside region.", {
        "radius": P("length", "fillet radius"), "edge_axis": P("axis", "edge direction"),
        "region": P("bbox", "[xmin, ymin, zmin, xmax, ymax, zmax]")}, None, modifies=True),
    FeatureType("chamfer", "Chamfers straight edges parallel to edge_axis whose midpoints lie inside region.", {
        "length": P("length", "chamfer length"), "edge_axis": P("axis", "edge direction"),
        "region": P("bbox", "[xmin, ymin, zmin, xmax, ymax, zmax]")}, None, modifies=True),
    FeatureType("shell", "Hollows the part to a wall thickness, opening the outermost planar face(s) facing open_side.", {
        "thickness": P("length", "wall thickness"), "open_side": P("axis", "direction of the open face")}, None, modifies=True),
    FeatureType("linear_pattern", "Repeats the primitives of feature `target` count times, each offset by (dx, dy, dz).", {
        "target": P("ref", "id of an earlier feature"), "count": P("count", "total number including the original"),
        "dx": P("pos", "offset x", 0.0), "dy": P("pos", "offset y", 0.0), "dz": P("pos", "offset z", 0.0)},
        None, pattern=True),
    FeatureType("circular_pattern", "Repeats the primitives of feature `target` count times around an axis through (cx, cy, cz).", {
        "target": P("ref", "id of an earlier feature"), "count": P("count", "total number including the original"),
        "cx": P("pos", "axis point x", 0.0), "cy": P("pos", "axis point y", 0.0), "cz": P("pos", "axis point z", 0.0),
        "axis": P("axis", "rotation axis", "z"), "angle": P("angle", "total angle; 360 spaces copies evenly", 360.0)},
        None, pattern=True),
    FeatureType("sweep_path", "Circular profile swept along a polyline path.", {
        "points": P("points", "path points [[x, y, z], ...]"), "radius": P("length", "profile radius"), "op": OP},
        b_sweep_path),
    FeatureType("fin_array", "Row of rectangular fins along +y from (x, y, z): each fin is length (x) by fin_thickness (y) by height (z).", {
        "x": P("pos", "first fin min x"), "y": P("pos", "first fin min y"), "z": P("pos", "fin base z"),
        "count": P("count", "number of fins"), "pitch": P("length", "fin spacing along y"),
        "length": P("length", "fin length along x"), "fin_thickness": P("length", "fin thickness"),
        "height": P("length", "fin height")}, b_fin_array),
]}
FEATURES["fillet"].build = m_fillet
FEATURES["chamfer"].build = m_chamfer
FEATURES["shell"].build = m_shell
FEATURES["linear_pattern"].build = p_linear
FEATURES["circular_pattern"].build = p_circular
