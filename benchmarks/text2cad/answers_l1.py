"""Answers to the 60 public Text2CAD-Bench L1 prompts (geometric style), written by Opus 5.5 in a Claude Code session
from the prompt text alone (not through the API; one pass, no reference shapes: the ground truth is not public).

The system's path: the design grammar first (`grammar`: a list of features, built by the C++ engine), and CadQuery
through the gated freeform fallback (`cadquery`) only when the grammar cannot express the shape (spheres, cones, lofts,
planes at an angle, fillets or chamfers on round edges). Where a prompt leaves the placement open, the footprint is
centred on the origin with the bottom at z = 0. Lengths in mm.
"""


def P(x, y, z, l, w, t, op="add"):
    return {"type": "plate", "params": {"x": x, "y": y, "z": z, "length": l, "width": w, "thickness": t, "op": op}}


def B(x, y, z, d, h, axis="z"):
    return {"type": "boss", "params": {"x": x, "y": y, "z": z, "diameter": d, "height": h, "axis": axis}}


def H(x, y, z, d, depth, axis="-z"):
    return {"type": "hole", "params": {"x": x, "y": y, "z": z, "diameter": d, "depth": depth, "axis": axis}}


def S(pts, z, h, op="add"):
    return {"type": "extrude_sketch", "params": {"points": pts, "z": z, "height": h, "op": op}}


def F(r, axis, region):
    return {"type": "fillet", "params": {"radius": r, "edge_axis": axis, "region": region}}


G = {
    "L1_2": [B(0, 0, 0, 40, 120), H(0, 0, 120, 25, 120)],
    "L1_3": [P(-25, -25, -25, 50, 50, 50), H(0, 0, 25, 20, 50)],
    "L1_4": [B(0, 0, 0, 80, 20), B(0, 0, 20, 40, 30)],
    "L1_5": [P(-30, -30, 0, 60, 60, 10), B(0, 0, 10, 30, 50)],
    "L1_6": [B(0, 0, 0, 100, 5), H(0, 0, 5, 50, 5)],
    "L1_7": [P(-50, -10, 0, 100, 20, 20), P(-10, -50, 0, 20, 100, 20)],
    "L1_10": [P(-50, -25, 0, 100, 50, 30), P(-50, -10, 15, 100, 20, 15, "cut")],
    "L1_11": [B(0, 0, 0, 60, 40), H(15, 0, 40, 10, 40)],
    "L1_12": [B(0, 0, 0, 90, 15), P(-15, -15, 0, 30, 30, 15, "cut")],
    "L1_13": [B(-40, 0, 0, 20, 80, "x"), B(0, 0, 0, 20, 40)],
    "L1_16": [P(-60, -20, 0, 120, 40, 10), H(-40, 0, 10, 10, 10), H(40, 0, 10, 10, 10)],
    "L1_17": [P(-30, 0, 0, 60, 20, 30), H(0, 0, 0, 40, 20, "y")],
    "L1_19": [B(0, 0, 0, 80, 20), H(0, 0, 20, 60, 20), B(0, 0, 0, 40, 20), H(0, 0, 20, 20, 20)],
    "L1_21": [B(0, 0, 0, 50, 35), B(0, 0, 35, 40, 10), B(0, 0, 45, 50, 35)],
    "L1_23": [B(0, 0, 0, 50, 50), B(40, 0, 0, 30, 50)],
    "L1_24": [P(0, 0, 0, 10, 40, 60), P(0, 0, 0, 50, 40, 10)],
    "L1_25": [B(0, 0, 0, 40, 40), H(0, 0, 40, 20, 10), H(0, 0, 40, 10, 40)],
    "L1_27": [P(0, 0, 0, 20, 20, 30), P(0, 0, 30, 100, 20, 10)],
    "L1_29": [B(0, 0, 0, 60, 10), B(0, 0, 10, 40, 10), B(0, 0, 20, 20, 10)],
    "L1_30": [B(0, 0, 0, 30, 60), H(0, -15, 30, 10, 30, "y")],
    "L1_33": [B(-50, 0, 0, 60, 100, "x"), H(-50, 0, 0, 40, 100, "x"), P(-50, -30, -30, 100, 60, 30, "cut")],
    "L1_34": [P(-50, -30, 0, 100, 60, 20), P(-50, -15, 20, 100, 30, 20)],
    "L1_36": [B(0, 0, 0, 80, 20), H(10, 0, 20, 40, 20)],
    "L1_37": [B(0, 0, 0, 80, 15), P(-40, -5, 10, 80, 10, 5, "cut"), P(-5, -40, 10, 10, 80, 5, "cut")],
    "L1_38": [S([[30, 0], [0, 15], [-30, 0], [0, -15]], 0, 80)],
    "L1_39": [P(0, -20, 0, 60, 40, 10), B(60, 0, 0, 40, 10)],
    "L1_40": [B(0, 0, 0, 30, 20), B(0, 0, 20, 15, 20), B(0, 0, 40, 30, 20)],
    "L1_41": [B(0, 0, 0, 100, 10), H(35, 0, 10, 10, 10), H(0, 35, 10, 10, 10), H(-35, 0, 10, 10, 10), H(0, -35, 10, 10, 10)],
    "L1_43": [P(-50, -25, 0, 100, 50, 30), P(-50, -15, 10, 100, 30, 20, "cut")],
    "L1_44": [P(-20, -20, 0, 40, 40, 40), H(0, 0, 40, 20, 40), H(0, 0, 40, 30, 10)],
    "L1_46": [S([[0, 0], [30, 0], [0, 40]], 0, 60)],
    "L1_47": [B(0, 0, 0, 40, 80), P(15, -5, 20, 10, 10, 40, "cut")],
    "L1_48": [P(-20, -20, 0, 40, 40, 20), B(0, 0, 20, 20, 20)],
    "L1_49": [P(-20, -20, -20, 40, 40, 40), H(0, 0, 20, 10, 40), H(20, 0, 0, 10, 40, "-x"), H(0, 20, 0, 10, 40, "-y")],
    "L1_50": [B(0, 0, 0, 20, 50, "x"), B(0, 0, 0, 20, 50, "z")],
    "L1_51": [B(0, 0, 0, 60, 10), H(30, 0, 10, 20, 10)],
    "L1_52": [P(-60, -25, 0, 120, 50, 6), F(4, "z", [-61, -26, -1, 61, 26, 7]),
              P(-31, -9, 0, 62, 18, 6, "cut"), H(-31, 0, 6, 18, 6), H(31, 0, 6, 18, 6)],
    "L1_57": [P(-40, -30, 0, 80, 60, 30), P(-10, 0, 10, 50, 30, 20, "cut"), F(4, "y", [-11, -1, 9, -9, 31, 11])],
    "L1_58": [P(-45, -25, 0, 90, 50, 30), P(-45, -10, 18, 90, 20, 12, "cut"), F(2, "x", [-46, -11, 17, 46, 11, 19])],
}

TET = '''
def tet(*p):
    v = [cq.Vector(*q) for q in p]
    faces = [cq.Face.makeFromWires(cq.Wire.makePolygon([v[a], v[b], v[c]], close=True))
             for a, b, c in ((0, 1, 2), (0, 1, 3), (0, 2, 3), (1, 2, 3))]
    return cq.Solid.makeSolid(cq.Shell.makeShell(faces))
'''

C = {
    "L1_1": '''
r = 25 / 0.8660254037844386
body = cq.Workplane("XY").polygon(6, 2 * r).extrude(20).faces(">Z").edges().chamfer(2)
result = body.cut(cq.Workplane("XY").circle(10).extrude(20))
''',
    "L1_8": '''
box = cq.Workplane("XY").box(80, 80, 40, centered=(True, True, False))
result = box.cut(cq.Workplane("XY").sphere(20).translate((0, 0, 40)))
''',
    "L1_9": '''
rod = cq.Workplane("XY").circle(15).extrude(100)
result = rod.union(cq.Workplane("XY").sphere(20).translate((0, 0, 115)))
''',
    "L1_14": '''
base = cq.Workplane("XY").circle(25).extrude(20)
result = base.union(cq.Workplane("XY").add(cq.Solid.makeCone(25, 0, 40, cq.Vector(0, 0, 20))))
''',
    "L1_15": TET + '''
cube = cq.Workplane("XY").box(50, 50, 50, centered=False)
result = cube.cut(cq.Workplane("XY").add(tet((50, 50, 50), (35, 50, 50), (50, 35, 50), (50, 50, 35))))
''',
    "L1_18": '''
body = cq.Workplane("XY").circle(15).extrude(60)
body = body.union(cq.Workplane("XY").sphere(15)).union(cq.Workplane("XY").sphere(15).translate((0, 0, 60)))
result = body
''',
    "L1_20": '''
result = cq.Workplane("XZ").polyline([(0, 0), (80, 0), (80, 5), (0, 30)]).close().extrude(-40)
''',
    "L1_22": '''
body = cq.Workplane("XY").rect(60, 60).workplane(offset=30).rect(40, 40).loft()
result = body.cut(cq.Workplane("XY").circle(10).extrude(30))
''',
    "L1_26": '''
ball = cq.Workplane("XY").sphere(30)
result = ball.cut(cq.Workplane("XY").box(80, 80, 40, centered=(True, True, False)).translate((0, 0, 15)))
''',
    "L1_28": '''
result = cq.Workplane("XY").box(60, 60, 60).cut(cq.Workplane("XY").sphere(30.5))
''',
    "L1_31": '''
result = cq.Workplane("XY").sphere(25).cut(cq.Workplane("XY").box(20, 20, 60))
''',
    "L1_32": TET + '''
block = cq.Workplane("XY").box(80, 50, 30, centered=False)
a = tet((0, 0, 30), (10, 0, 30), (0, 10, 30), (0, 0, 20))
b = tet((80, 50, 0), (70, 50, 0), (80, 40, 0), (80, 50, 10))
result = block.cut(cq.Workplane("XY").add(a)).cut(cq.Workplane("XY").add(b))
''',
    "L1_35": '''
body = cq.Workplane("XY").circle(20).extrude(40)
result = body.cut(cq.Workplane("XY").add(cq.Solid.makeCone(15, 0, 15, cq.Vector(0, 0, 40), cq.Vector(0, 0, -1))))
''',
    "L1_42": '''
import math
cyl = cq.Workplane("XY").circle(15).extrude(50)
# cutting plane at 30 degrees to the base, through the highest point of the side at x = 15, z = 50
cutter = cq.Workplane("XY").box(200, 200, 200, centered=(True, True, False)).val()
cutter = cutter.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 1, 0), -30).translate(cq.Vector(15, 0, 50))
result = cyl.cut(cq.Workplane("XY").add(cutter))
''',
    "L1_45": '''
shell = cq.Workplane("XY").sphere(50).cut(cq.Workplane("XY").sphere(40))
result = shell.cut(cq.Workplane("XY").box(120, 120, 60, centered=(True, True, False)).translate((0, 0, -60)))
''',
    "L1_53": '''
body = cq.Workplane("XY").ellipse(40, 25).extrude(25).faces(">Z").edges().chamfer(0.8)
result = body.cut(cq.Workplane("XY").center(10, 0).circle(8).extrude(25))
''',
    "L1_54": '''
result = cq.Workplane("XY").sphere(25).cut(cq.Workplane("XY").circle(7).extrude(60).translate((0, 0, -30)))
''',
    "L1_55": '''
body = cq.Workplane("XY").add(cq.Solid.makeCone(35, 20, 60)).faces("<Z").edges().chamfer(2)
result = body.cut(cq.Workplane("XY").circle(10).extrude(60))
''',
    "L1_56": '''
body = cq.Workplane("XY").rect(80, 80).workplane(offset=45).rect(50, 50).loft()
body = body.cut(cq.Workplane("XY").box(30, 30, 15, centered=(True, True, False)).translate((0, 0, 30)))
result = body.edges(cq.selectors.BoxSelector((-16, -16, 44), (16, 16, 46))).chamfer(1)
''',
    "L1_59": '''
s = cq.Workplane("XY").circle(30).extrude(80)
s = s.cut(cq.Workplane("XY").circle(31).circle(27).extrude(10).translate((0, 0, 35))).val()
edges = [e for e in s.Edges() if e.geomType() == "CIRCLE" and abs(e.radius() - 30) < 1e-6 and abs(e.Center().z - 40) > 1]
result = s.fillet(1, edges)
''',
    "L1_60": '''
s = cq.Workplane("XY").circle(50).extrude(12)
s = s.cut(cq.Workplane("XY").pushPoints([(35, 0), (0, 35), (-35, 0), (0, -35)]).circle(5).extrude(12)).val()
edges = [e for e in s.Edges() if e.geomType() == "CIRCLE" and abs(e.radius() - 5) < 1e-6 and abs(e.Center().z - 12) < 1e-6]
result = s.chamfer(0.8, None, edges)
''',
}

assert not set(G) & set(C) and len(G) + len(C) == 60, (len(G), len(C))
