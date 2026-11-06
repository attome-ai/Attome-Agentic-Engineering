"""Answers to the 30 public Text2CAD-Bench L3 prompts (sweeps, lofts, shells, gears, free-form), written by Opus 5.5 in a
Claude Code session from the geometric prompt alone. Same path as L1 (answers_l1.py): the design grammar where it can
express the shape, otherwise CadQuery through the gated fallback. Where a prompt contradicts itself or leaves a size
open, the choice is stated in a comment. REPAIRS records each answer that failed and was fixed from the error message
(the benchmark allows three attempts with error feedback).
"""
from answers_l1 import B, H, P  # noqa: F401

G = {
    # L-shaped bearing housing: base 100 x 100 x 15, upright plate 20 thick along x = 0..20 up to z = 120,
    # bearing boss OD 70 / bore 40 along x with its axis 80 above the base top (z = 95), base hub OD 70 x 10 with a 50 bore,
    # one 15 mm rib, four 12 mm corner holes, M8 grease hole (6.8 tap drill) from the top into the bore.
    "L3_28": [P(0, 0, 0, 100, 100, 15), P(0, 0, 15, 20, 100, 105), B(50, 50, 15, 70, 10),
              B(0, 50, 95, 70, 40, "x"),
              {"type": "rib", "params": {"x": 20, "y": 57.5, "z": 15, "leg_a": 60, "leg_a_dir": "x", "leg_b": 70,
                                         "leg_b_dir": "z", "thickness": 15, "tip_cut": 0}},
              H(50, 50, 25, 50, 25), H(0, 50, 95, 40, 40, "x"),
              H(12, 12, 15, 12, 15), H(88, 12, 15, 12, 15), H(12, 88, 15, 12, 15), H(88, 88, 15, 12, 15),
              H(10, 50, 120, 6.8, 6)],
}

GEAR = '''
import math
def gear_outline(m, z, pa_deg, rf, n=12):
    pa = math.radians(pa_deg)
    rp, rb, ra = m * z / 2, m * z / 2 * math.cos(pa), m * z / 2 + m
    inv = lambda a: math.tan(a) - a
    psi = math.pi / (2 * z)
    def phi(r):  # half angle of the tooth at radius r (involute part)
        r = max(r, rb)
        return psi + inv(pa) - inv(math.acos(rb / r))
    pts = []
    for k in range(z):
        c = 2 * math.pi * k / z
        pb = phi(rb)
        pts.append((rf * math.cos(c - pb), rf * math.sin(c - pb)))
        for i in range(n + 1):
            r = rb + (ra - rb) * i / n
            a = c - phi(r)
            pts.append((r * math.cos(a), r * math.sin(a)))
        for i in range(n, -1, -1):
            r = rb + (ra - rb) * i / n
            a = c + phi(r)
            pts.append((r * math.cos(a), r * math.sin(a)))
        pts.append((rf * math.cos(c + pb), rf * math.sin(c + pb)))
        nxt = 2 * math.pi * (k + 1) / z - pb
        for i in range(1, 4):
            a = c + pb + (nxt - c - pb) * i / 4
            pts.append((rf * math.cos(a), rf * math.sin(a)))
    return pts
'''

C = {
    # paraboloid reflector Y^2 + Z^2 = 200 (X - 100), X 100..200, axis through (100, 75, 0); a surface, given 1 mm thickness
    "L3_1": '''
import math
xs = [100 + 100 * (i / 60) ** 2 for i in range(61)]
inner = [(x, math.sqrt(200 * (x - 100))) for x in xs]
outer = [(x, r + 1) for x, r in inner][::-1]
pts = inner + outer
prof = cq.Workplane("XY").moveTo(*pts[0]).spline(inner[1:], includeCurrent=True).lineTo(*outer[0]).spline(outer[1:], includeCurrent=True).close()
result = prof.revolve(360, (0, 0, 0), (1, 0, 0)).translate((0, 75, 0))
''',
    # corrugated fin on a base whose top is the Y = 0 plane; fin centreline Y = 5 sin(2 pi X / 20) + 5, 1 mm thick, 50 along Z
    "L3_2": '''
import math
base = cq.Workplane("XY").box(100, 3, 50, centered=False).translate((0, -3, 0))
xs = [i * 0.5 for i in range(201)]
up = [(x, 5 * math.sin(2 * math.pi * x / 20) + 5 + 0.5) for x in xs]
lo = [(x, 5 * math.sin(2 * math.pi * x / 20) + 5 - 0.5) for x in xs][::-1]
fin = cq.Workplane("XY").polyline(up + lo).close().extrude(50)
result = base.union(fin)
''',
    # spur gear m3 z20 20 deg, 15 wide, bore 20, keyway 6 x 3 centred on a tooth space; root fillet 0.9 not modelled
    "L3_3": GEAR + '''
g = cq.Workplane("XY").polyline(gear_outline(3, 20, 20, 26.25)).close().extrude(15)
a = math.pi / 20  # middle of the first tooth space
key = cq.Workplane("XY").box(13, 6, 15, centered=(False, True, False)).rotate((0, 0, 0), (0, 0, 1), math.degrees(a))
result = g.cut(cq.Workplane("XY").circle(10).extrude(15)).cut(key)
''',
    # drone arm: loft of four sections on planes normal to X, centred on the Bezier spine, shelled 1.5 mm with open ends
    "L3_4": '''
def bez(t):
    P = [(0, 0), (30, 20), (100, 25), (150, 5)]
    u = 1 - t
    return tuple(u**3 * P[0][i] + 3 * u * u * t * P[1][i] + 3 * u * t * t * P[2][i] + t**3 * P[3][i] for i in range(2))
def y_at(X):
    lo, hi = 0.0, 1.0
    for _ in range(60):
        mid = (lo + hi) / 2
        lo, hi = (mid, hi) if bez(mid)[0] < X else (lo, mid)
    return bez(lo)[1]
secs = [(0, 7.5, 7.5), (50, 12.5, 10), (100, 11, 9), (150, 10, 10)]
wires = []
for X, a, b in secs:
    c = cq.Vector(X, y_at(X), 0)
    wires.append(cq.Wire.makeEllipse(a, b, c, cq.Vector(1, 0, 0), cq.Vector(0, 1, 0)))
s = cq.Solid.makeLoft(wires)
result = cq.Workplane("XY").add(s).faces("<X or >X").shell(-1.5)
''',
    # 2:1 ellipsoidal head, OD 200, straight flange 50, dome semi-axes 100 x 50 (the prompt's "depth 100" contradicts its
    # own 2:1 ratio; the standard 2:1 head is used), wall 10 inward, nozzle 40/30 x 30 at the apex, open into the head
    "L3_5": '''
import math
def arc(a, b, z0, n=40):
    return [(a * math.cos(math.pi / 2 * i / n), z0 + b * math.sin(math.pi / 2 * i / n)) for i in range(n + 1)]
outer = arc(100, 50, 50)
inner = arc(90, 40, 50)[::-1]
w = (cq.Workplane("XZ").moveTo(90, 0).lineTo(100, 0).lineTo(100, 50).spline(outer[1:], includeCurrent=True)
     .lineTo(*inner[0]).spline(inner[1:], includeCurrent=True).close())
head = w.revolve(360, (0, 0, 0), (0, 1, 0))
noz = cq.Workplane("XY").circle(20).circle(15).extrude(40).translate((0, 0, 90))  # repair: was 2 solids
result = head.union(noz).cut(cq.Workplane("XY").circle(15).extrude(40).translate((0, 0, 85)))
''',
    # cam with a closed sine groove: centreline z = 50 + 30 sin(phi) on the surface, 6 wide, 4 deep, round bottom R3
    "L3_6": '''
import math
cyl = cq.Workplane("XY").circle(20).extrude(100)
def groove(rc):
    path = cq.Workplane("XY").parametricCurve(lambda t: (rc * math.cos(2 * math.pi * t), rc * math.sin(2 * math.pi * t),
                                                         50 + 30 * math.sin(2 * math.pi * t)), N=200, makeWire=True)
    p0 = cq.Vector(rc, 0, 50)
    t0 = cq.Vector(0, rc * 2 * math.pi, 30 * 2 * math.pi).normalized()
    prof = cq.Workplane(cq.Plane(origin=p0, xDir=cq.Vector(1, 0, 0), normal=t0)).circle(3)
    return prof.sweep(path, isFrenet=True)
result = cyl.cut(groove(19)).cut(groove(21.5))
''',
    # orbiting scroll: involute of a circle (rb 3), 0..1080 deg, wall 4 thick x 25 high on an 80 x 5 disc; the start is
    # closed by a round end
    "L3_7": '''
import math
rb, t0, t1 = 3.0, 0.9, 6 * math.pi
def P(t, off):
    x, y = rb * (math.cos(t) + t * math.sin(t)), rb * (math.sin(t) - t * math.cos(t))
    return (x - off * math.sin(t), y + off * math.cos(t))
ts = [t0 + (t1 - t0) * i / 300 for i in range(301)]
outer = [P(t, 2) for t in ts]
inner = [P(t, -2) for t in ts][::-1]
c = P(t0, 0)
cap = [(c[0] - 2 * math.cos(math.pi / 2 + t0 + a), c[1] - 2 * math.sin(math.pi / 2 + t0 + a)) for a in [math.pi * i / 8 for i in range(1, 8)]]
wall = cq.Workplane("XY").polyline(outer + inner + cap).close().extrude(25).translate((0, 0, 5))
result = cq.Workplane("XY").circle(40).extrude(5).union(wall)
''',
    # Laval nozzle: converging Bezier (inlet r 25, throat r 10, 30 long, arms 15), diverging paraboloid r = 10 + 0.008 (x-30)^2
    # to r 30 at x 80; wall 3 outward; axis X
    "L3_8": '''
def bz(t):
    P = [(0, 25), (15, 25), (15, 10), (30, 10)]
    u = 1 - t
    return tuple(u**3 * P[0][i] + 3 * u * u * t * P[1][i] + 3 * u * t * t * P[2][i] + t**3 * P[3][i] for i in range(2))
inner = [bz(i / 40) for i in range(41)] + [(30 + 50 * i / 40, 10 + 0.008 * (50 * i / 40) ** 2) for i in range(1, 41)]
outer = [(x, r + 3) for x, r in inner][::-1]
result = cq.Workplane("XY").polyline(inner + outer).close().revolve(360, (0, 0, 0), (1, 0, 0))
''',
    # vase: loft from a 60 circle (z 0) through a six-lobed ring r = 35 + 5 cos(6 phi) (z 75, a peak on +X) to an 80 circle (z 150)
    "L3_9": '''
import math
mid = [cq.Vector((35 + 5 * math.cos(6 * a)) * math.cos(a), (35 + 5 * math.cos(6 * a)) * math.sin(a), 75)
       for a in [2 * math.pi * i / 120 for i in range(120)]]
w = [cq.Wire.makeCircle(30, cq.Vector(0, 0, 0), cq.Vector(0, 0, 1)), cq.Wire.assembleEdges([cq.Edge.makeSpline(mid, periodic=True)]),
     cq.Wire.makeCircle(40, cq.Vector(0, 0, 150), cq.Vector(0, 0, 1))]
result = cq.Solid.makeLoft(w)
''',
    # concentrator: inner Z = R^2 / 80 up to z 80 (R 80), wall 5 along the normal, top flange R 85..95 x 5 with four 5 mm
    # holes on R 90 (one on +Y), 10 mm hole at the bottom
    "L3_10": '''
import math
inner = [(80 * i / 60, (80 * i / 60) ** 2 / 80) for i in range(61)]
def off(R, z):
    n = (-(2 * R / 80), 1)
    l = math.hypot(*n)
    return (R - 5 * n[0] / l, z - 5 * n[1] / l)
outer = [off(R, z) for R, z in inner][::-1]
top = outer[0]
pts = inner + [(top[0], 80)] + outer
bowl = cq.Workplane("XZ").polyline(pts).close().revolve(360, (0, 0, 0), (0, 1, 0))
fl = cq.Workplane("XY").circle(95).circle(top[0] - 1).extrude(5).translate((0, 0, 75))
holes = cq.Workplane("XY").pushPoints([(0, 90), (-90, 0), (0, -90), (90, 0)]).circle(2.5).extrude(10).translate((0, 0, 72))
result = bowl.union(fl).cut(holes).cut(cq.Workplane("XY").circle(5).extrude(20).translate((0, 0, -10)))
''',
    # S-shaped intake: loft from the bottom ellipse (120 x 80) through a section at the Bezier midpoint (normal 45 deg) to the
    # 60 circle tilted 45 deg at (60, 0, 120); 3 mm wall with both ports open; lip 5 wide x 2 thick at the top port
    "L3_11": '''
import math
n = cq.Vector(math.sqrt(0.5), 0, math.sqrt(0.5))
w = [cq.Wire.makeEllipse(60, 40, cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), cq.Vector(1, 0, 0)),
     cq.Wire.makeEllipse(45, 35, cq.Vector(30, 0, 60), n, cq.Vector(math.sqrt(0.5), 0, -math.sqrt(0.5))),
     cq.Wire.makeCircle(30, cq.Vector(60, 0, 120), n)]
s = cq.Solid.makeLoft(w)
ends = [f for f in s.Faces() if f.geomType() == "PLANE"]
pipe = s.shell(-3, kind="intersection") if False else s.shell(ends, -3)
top = cq.Vector(60, 0, 120)
lip = cq.Solid.makeCylinder(35, 2, top - n * 2, n).cut(cq.Solid.makeCylinder(27, 2, top - n * 2, n))
result = pipe.fuse(lip)
''',
    # cylinder 80 x 60, top cut by a plane at 30 deg, highest on -Y; paraboloid x^2 + y^2 = 40 z recessed normal to it,
    # opening 60 (depth 22.5); semicircular R2 groove at z = 30 + 5 sin(6 phi) around the side
    "L3_12": '''
import math
cyl = cq.Workplane("XY").circle(40).extrude(60)
t = math.tan(math.radians(30))
cut = cq.Workplane("XY").box(300, 300, 300, centered=(True, True, False)).val()
cut = cut.rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), -30).translate(cq.Vector(0, -40, 60))
body = cyl.cut(cq.Workplane("XY").add(cut))
zc = 60 - t * 40
n = cq.Vector(0, math.sin(math.radians(30)), math.cos(math.radians(30)))
par = [(30 * i / 40, (30 * i / 40) ** 2 / 40) for i in range(41)]
cutter = cq.Workplane("XZ").polyline(par + [(30, 40), (0, 40)]).close().revolve(360, (0, 0, 0), (0, 1, 0)).val()
cutter = cutter.rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), -30).translate(cq.Vector(0, 0, zc) - n * 22.5)
body = body.cut(cq.Workplane("XY").add(cutter))
def half(u0, u1):  # repair: a closed sweep cut left an invalid solid; two open halves do not
    f = lambda u: (40 * math.cos(2 * math.pi * u), 40 * math.sin(2 * math.pi * u), 30 + 5 * math.sin(12 * math.pi * u))
    path = cq.Workplane("XY").parametricCurve(f, N=200, start=u0, stop=u1, makeWire=True)
    p0, p1 = cq.Vector(*f(u0)), cq.Vector(*f(u0 + 1e-6))
    pl = cq.Plane(origin=p0, xDir=cq.Vector(0, 0, 1).cross(p1 - p0).normalized(), normal=(p1 - p0).normalized())
    return cq.Workplane(pl).circle(2).sweep(path, isFrenet=True)
result = body.cut(half(0, 0.5)).cut(half(0.5, 1.0))
''',
    # gear as L3_3, 30 wide, plus a ring recess (45 / 32) on each face; the recess depth is not given: 5 mm
    "L3_13": GEAR + '''
g = cq.Workplane("XY").polyline(gear_outline(3, 20, 20, 26.25)).close().extrude(30)
a = math.pi / 20
key = cq.Workplane("XY").box(13, 6, 30, centered=(False, True, False)).rotate((0, 0, 0), (0, 0, 1), math.degrees(a))
ring = cq.Workplane("XY").circle(22.5).circle(16).extrude(5)
g = g.cut(ring).cut(ring.translate((0, 0, 25)))
result = g.cut(cq.Workplane("XY").circle(10).extrude(30)).cut(key)
''',
    # base 100 x 50 x 5; five fins along X at Y = 5, 15, ..., 45, each centred on y0 + 2 sin(0.2 pi x), 1 thick, 30 high
    "L3_14": '''
import math
r = cq.Workplane("XY").box(100, 50, 5, centered=False)
xs = [i * 0.5 for i in range(201)]
for y0 in (5, 15, 25, 35, 45):
    up = [(x, y0 + 2 * math.sin(0.2 * math.pi * x) + 0.5) for x in xs]
    lo = [(x, y0 + 2 * math.sin(0.2 * math.pi * x) - 0.5) for x in xs][::-1]
    r = r.union(cq.Workplane("XY").polyline(up + lo).close().extrude(30).translate((0, 0, 5)))
result = r
''',
    # 2:1 head: inner semi-ellipse 100 x 50 over a 25 straight section, wall 8 outward, nozzle 40/30 x 30 at the apex
    "L3_15": '''
import math
def arc(a, b, z0, n=40):
    return [(a * math.cos(math.pi / 2 * i / n), z0 + b * math.sin(math.pi / 2 * i / n)) for i in range(n + 1)]
outer = arc(108, 58, 25)
inner = arc(100, 50, 25)[::-1]
w = (cq.Workplane("XZ").moveTo(100, 0).lineTo(108, 0).lineTo(108, 25).spline(outer[1:], includeCurrent=True)
     .lineTo(*inner[0]).spline(inner[1:], includeCurrent=True).close())
head = w.revolve(360, (0, 0, 0), (0, 1, 0))
noz = cq.Workplane("XY").circle(20).circle(15).extrude(43).translate((0, 0, 70))  # repair: was 2 solids
result = head.union(noz).cut(cq.Workplane("XY").circle(15).extrude(40).translate((0, 0, 70)))
''',
    # mouse cover: loft of half-elliptic arches along X under the Bezier spine (0,0,0) (20,0,35) (80,0,35) (110,0,0), widest
    # (60) at a third of the length, shelled 2 mm open at the bottom, thumb rest cut on the left (-Y)
    "L3_16": '''
import math
def bz(t):
    P = [(0, 0), (20, 35), (80, 35), (110, 0)]
    u = 1 - t
    return tuple(u**3 * P[0][i] + 3 * u * u * t * P[1][i] + 3 * u * t * t * P[2][i] + t**3 * P[3][i] for i in range(2))
def z_at(X):
    lo, hi = 0.0, 1.0
    for _ in range(60):
        m = (lo + hi) / 2
        lo, hi = (m, hi) if bz(m)[0] < X else (lo, m)
    return bz(lo)[1]
wires = []
for X in (3, 20, 37, 60, 85, 106):
    w = 60 - 30 * abs(X - 37) / 70
    h = max(z_at(X), 4)
    pts = [cq.Vector(X, w / 2 * math.cos(math.pi * i / 24), h * math.sin(math.pi * i / 24)) for i in range(25)]
    e1 = cq.Edge.makeSpline(pts)
    e2 = cq.Edge.makeLine(pts[-1], pts[0])
    wires.append(cq.Wire.assembleEdges([e1, e2]))
s = cq.Solid.makeLoft(wires)
shell = cq.Workplane("XY").add(s).faces("<Z").shell(-2)
result = shell.cut(cq.Workplane("XY").sphere(25).translate((50, -45, 12)))
''',
    # cam 50 x 100 with a closed sine groove y = 15 sin(theta) + 50, 8 wide, 5 deep, round bottom R4
    "L3_17": '''
import math
cyl = cq.Workplane("XY").circle(25).extrude(100)
def groove(rc):
    path = cq.Workplane("XY").parametricCurve(lambda t: (rc * math.cos(2 * math.pi * t), rc * math.sin(2 * math.pi * t),
                                                         50 + 15 * math.sin(2 * math.pi * t)), N=200, makeWire=True)
    t0 = cq.Vector(0, rc * 2 * math.pi, 15 * 2 * math.pi).normalized()
    prof = cq.Workplane(cq.Plane(origin=cq.Vector(rc, 0, 50), xDir=cq.Vector(1, 0, 0), normal=t0)).circle(4)
    return prof.sweep(path, isFrenet=True)
result = cyl.cut(groove(24)).cut(groove(27))
''',
    # lattice lampshade: ellipsoid shell (semi-axes 40, 40, 60) 3 thick, kept only on 12 meridian ribs and 3 rings (all 4
    # wide), 10 mm hole at the top, cut below z = -50
    "L3_18": '''
import math
def ell(a, b):
    pts = [(a * math.cos(t), b * math.sin(t)) for t in [-math.pi / 2 + math.pi * i / 60 for i in range(61)]]
    return cq.Workplane("XZ").moveTo(0, -b).spline(pts[1:], includeCurrent=True).close().revolve(360, (0, 0, 0), (0, 1, 0))
shell = ell(40, 60).cut(ell(37, 57))
keep = None
for k in range(12):
    slab = cq.Workplane("XY").box(100, 4, 140, centered=(False, True, True)).rotate((0, 0, 0), (0, 0, 1), 30 * k)
    keep = slab if keep is None else keep.union(slab)
for zc in (-30, 0, 30):
    keep = keep.union(cq.Workplane("XY").box(120, 120, 4).translate((0, 0, zc)))
lat = shell.intersect(keep)
lat = lat.cut(cq.Workplane("XY").circle(5).extrude(20).translate((0, 0, 50)))
result = lat.cut(cq.Workplane("XY").box(120, 120, 20, centered=(True, True, False)).translate((0, 0, -70)))
''',
    # fixed scroll: base 120 x 10, Archimedean wall r = 20 + 3.5 theta (0..3 pi), 4 thick, 25 high, round ends, 8 mm hole
    "L3_19": '''
import math
def p(t):
    r = 20 + 3.5 * t
    return r * math.cos(t), r * math.sin(t)
def nrm(t):
    dx = 3.5 * math.cos(t) - (20 + 3.5 * t) * math.sin(t)
    dy = 3.5 * math.sin(t) + (20 + 3.5 * t) * math.cos(t)
    l = math.hypot(dx, dy)
    return -dy / l, dx / l
ts = [3 * math.pi * i / 300 for i in range(301)]
o = [(p(t)[0] + 2 * nrm(t)[0], p(t)[1] + 2 * nrm(t)[1]) for t in ts]
i_ = [(p(t)[0] - 2 * nrm(t)[0], p(t)[1] - 2 * nrm(t)[1]) for t in ts][::-1]
def cap(t, sgn):  # repair: half circle c + 2 sgn (cos a n + sin a t); sgn +1 at the end (outer to inner), -1 at the start
    c, n = p(t), nrm(t)
    tg = (n[1], -n[0])  # forward tangent
    return [(c[0] + 2 * sgn * (math.cos(a) * n[0] + math.sin(a) * tg[0]), c[1] + 2 * sgn * (math.cos(a) * n[1] + math.sin(a) * tg[1]))
            for a in [math.pi * k / 8 for k in range(1, 8)]]
pts = o + cap(ts[-1], 1) + i_ + cap(ts[0], -1)
wall = cq.Workplane("XY").polyline(pts).close().extrude(25).translate((0, 0, 10))
result = cq.Workplane("XY").circle(60).extrude(10).union(wall).cut(cq.Workplane("XY").circle(4).extrude(35))
''',
    # fan: hub 30 x 20; one blade from r 14 to 75, airfoil chord 25 -> 15, pitch 45 -> 15 deg (linear), lofted on three sections
    "L3_20": '''
import math
def foil(c, ang, X):
    pts = []
    for i in range(40):
        th = 2 * math.pi * i / 40
        x = 0.5 * (1 + math.cos(th))
        yt = 0.6 * 0.12 * (0.2969 * math.sqrt(x) - 0.126 * x - 0.3516 * x**2 + 0.2843 * x**3 - 0.1015 * x**4)
        yc = 0.02 * (2 * 0.4 * x - x * x) / 0.16 if x < 0.4 else 0.02 * ((1 - 0.8) + 0.8 * x - x * x) / 0.36
        y = yc + (yt if th < math.pi else -yt) * 5
        u, v = (x - 0.4) * c, y * c
        a = math.radians(ang)
        pts.append(cq.Vector(X, u * math.cos(a) - v * math.sin(a), 10 + u * math.sin(a) + v * math.cos(a)))
    return cq.Wire.assembleEdges([cq.Edge.makeSpline(pts, periodic=True)])
blade = cq.Solid.makeLoft([foil(25, 45, 14), foil(20, 30, 44.5), foil(15, 15, 75)])
result = cq.Workplane("XY").circle(15).extrude(20).union(cq.Workplane("XY").add(blade))
''',
    # Laval nozzle, 100 long on X: inlet arc R50 over 20 mm to a 10 throat, parabolic divergent to 40 at x 100, outer wall an
    # elliptical arc (max OD 60), 60 x 5 flanges at both ends
    "L3_21": '''
import math
conv = [(x, 5 + 50 - math.sqrt(50**2 - (20 - x) ** 2)) for x in [20 * i / 20 for i in range(21)]]
div = [(20 + 80 * i / 40, 5 + 15 / 6400 * (80 * i / 40) ** 2) for i in range(1, 41)]
inner = conv + div
outer = [(x, 30 * math.sqrt(max(0.0, 1 - ((x - 50) / 73.5) ** 2))) for x, _ in inner][::-1]
body = cq.Workplane("XY").polyline(inner + outer).close().revolve(360, (0, 0, 0), (1, 0, 0))
f1 = cq.Workplane("YZ").circle(30).circle(conv[0][1]).extrude(5)
f2 = cq.Workplane("YZ").circle(30).circle(20).extrude(5).translate((95, 0, 0))
result = body.union(f1).union(f2)
''',
    # motor mount cage: bottom ring 60/50 x 5 with four lugs (M4 holes on r 36), top plate 40 x 4 at z 40 with a 10 hole and
    # four 3 mm holes on r 12, eight 4 mm struts, a mid ring at z 20; the fillets are not modelled
    "L3_22": '''
import math
r = cq.Workplane("XY").circle(30).circle(25).extrude(5)
for k in range(4):
    a = math.pi / 2 * k
    lug = cq.Workplane("XY").box(14, 10, 5, centered=(False, True, False)).translate((27, 0, 0)).rotate((0, 0, 0), (0, 0, 1), math.degrees(a))
    r = r.union(lug)
r = r.cut(cq.Workplane("XY").pushPoints([(36 * math.cos(math.pi / 2 * k), 36 * math.sin(math.pi / 2 * k)) for k in range(4)]).circle(2.25).extrude(5))
top = cq.Workplane("XY").circle(20).extrude(4).translate((0, 0, 40))
r = r.union(top)
for k in range(8):
    a = math.pi / 4 * k
    p0 = cq.Vector(27.5 * math.cos(a), 27.5 * math.sin(a), 2.5)
    p1 = cq.Vector(17 * math.cos(a), 17 * math.sin(a), 42)
    d = p1 - p0
    r = r.union(cq.Workplane("XY").add(cq.Solid.makeCylinder(2, d.Length, p0, d.normalized())))
mid = cq.Workplane("XY").circle(24.5).circle(20.5).extrude(3).translate((0, 0, 20.5))
r = r.union(mid)
r = r.cut(cq.Workplane("XY").circle(5).extrude(10).translate((0, 0, 37)))
result = r.cut(cq.Workplane("XY").pushPoints([(12, 0), (0, 12), (-12, 0), (0, -12)]).circle(1.5).extrude(10).translate((0, 0, 37)))
''',
    # manifold 200 x 100 x 80: ten 10 mm outlets (two rows of five) from the top to two 12 mm channels along X at z 40,
    # three 20 mm inlets from the front (y = 0) crossing both, countersunk corner holes, four 60 x 20 x 15 slots
    # (two on the bottom, two on the back)
    "L3_23": '''
b = cq.Workplane("XY").box(200, 100, 80, centered=False)
outs = [(20 + 40 * i, y) for i in range(5) for y in (35, 65)]
b = b.cut(cq.Workplane("XY").pushPoints(outs).circle(5).extrude(40).translate((0, 0, 40)))
for y in (35, 65):
    b = b.cut(cq.Workplane("YZ").center(y, 40).circle(6).extrude(180).translate((10, 0, 0)))
for x in (50, 100, 150):
    b = b.cut(cq.Workplane("XZ").center(x, 40).circle(10).extrude(-71))
b = b.faces(">Z").workplane(origin=(0, 0, 80)).pushPoints([(10, 10), (190, 10), (10, 90), (190, 90)]).cskHole(9, 18, 90)
b = b.cut(cq.Workplane("XY").pushPoints([(50, 50), (150, 50)]).slot2D(60, 20).extrude(15))
result = b.cut(cq.Workplane("XZ").pushPoints([(50, 40), (150, 40)]).slot2D(60, 20).extrude(15).translate((0, 100, 0)))
''',
    # helical HTD pulley: rim OD 120, 40 wide, 47 round grooves 3.4 deep twisted by 40 tan 15 / R (10.2 deg); rim 8 thick;
    # hub 45 / 25 with a 6 mm keyway; five elliptic spokes 12 x 6 twisting 90 deg; fillets and chamfers not modelled
    "L3_24": '''
import math
R, n, d = 60, 47, 3.4
pts = []
for k in range(n):
    c = 2 * math.pi * k / n
    w = 2 * math.pi / n
    for i in range(12):
        a = c + w * i / 12
        s = (a - c) / w
        r = R - d * math.sin(math.pi * s / 0.55) if s < 0.55 else R
        pts.append((r * math.cos(a), r * math.sin(a)))
twist = math.degrees(40 * math.tan(math.radians(15)) / (R - d / 2))
rim = cq.Workplane("XY").polyline(pts).close().twistExtrude(40, twist).cut(cq.Workplane("XY").circle(52).extrude(40))
hub = cq.Workplane("XY").circle(22.5).circle(12.5).extrude(40)
hub = hub.cut(cq.Workplane("XY").box(6, 6, 40, centered=(True, False, False)).translate((0, 10, 0)))
body = rim.union(hub)
for k in range(5):
    a = 2 * math.pi * k / 5
    u = cq.Vector(math.cos(a), math.sin(a), 0)
    t = cq.Vector(-math.sin(a), math.cos(a), 0)
    w1 = cq.Wire.makeEllipse(6, 3, u * 21 + cq.Vector(0, 0, 20), u, cq.Vector(0, 0, 1))
    w2 = cq.Wire.makeEllipse(6, 3, u * 53 + cq.Vector(0, 0, 20), u, t)
    body = body.union(cq.Workplane("XY").add(cq.Solid.makeLoft([w1, w2], True)))
result = body
''',
    # cylinder head: liner 120 / 90 x 100, 15 fins 2 thick to OD 160, hemispherical cover (R 60, 8 thick) with a 14 mm plug
    # hole, two oblique ports (24 / 16, 40 long) at different elevations, four 10 mm bolt holes on R 70 through the height
    "L3_25": '''
import math
b = cq.Workplane("XY").circle(60).circle(45).extrude(100)
for i in range(15):
    z = 100 / 15 * (i + 0.5) - 1
    b = b.union(cq.Workplane("XY").circle(80).circle(59).extrude(2).translate((0, 0, z)))
dome = cq.Workplane("XY").sphere(60).cut(cq.Workplane("XY").sphere(52)).cut(cq.Workplane("XY").box(200, 200, 100, centered=(True, True, False)).translate((0, 0, -100)))
b = b.union(dome.translate((0, 0, 100)))
for az, el in ((0, 40), (180, 55)):
    a, e = math.radians(az), math.radians(el)
    dvec = cq.Vector(math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))
    p0 = cq.Vector(0, 0, 100) + dvec * 50
    b = b.union(cq.Workplane("XY").add(cq.Solid.makeCylinder(12, 40, p0, dvec)))  # repair: bore after the union
    b = b.cut(cq.Workplane("XY").add(cq.Solid.makeCylinder(8, 60, p0 - dvec * 10, dvec)))
b = b.cut(cq.Workplane("XY").circle(7).extrude(30).translate((0, 0, 150)))
result = b.cut(cq.Workplane("XY").pushPoints([(70, 0), (0, 70), (-70, 0), (0, -70)]).circle(5).extrude(100))
''',
    # two plate cams 15 thick, 20 apart: main r 50..90 (90 deg modified-sine rise, 90 return, 180 dwell), secondary the
    # conjugate r2(theta) = 140 - r1(theta + 180); 25 bore with a 6 x 6 keyway; 1 mm chamfers. Two separate solids, as described.
    "L3_26": '''
import math
K = 4 + math.pi
def ms(x):
    if x < 1 / 8:
        return math.pi * x / K - math.sin(4 * math.pi * x) / (4 * K)
    if x < 7 / 8:
        return 2 / K + math.pi * x / K - 9 * math.sin(math.pi * (4 * x + 1) / 3) / (4 * K)
    return 4 / K + math.pi * x / K - math.sin(4 * math.pi * x) / (4 * K)
def r1(th):
    th %= 2 * math.pi
    if th < math.pi / 2:
        return 50 + 40 * ms(th / (math.pi / 2))
    if th < math.pi:
        return 90 - 40 * ms((th - math.pi / 2) / (math.pi / 2))
    return 50
def cam(rf, z0):
    pts = [cq.Vector(rf(t) * math.cos(t), rf(t) * math.sin(t), 0) for t in [2 * math.pi * i / 360 for i in range(360)]]
    w = cq.Wire.assembleEdges([cq.Edge.makeSpline(pts, periodic=True)])
    s = cq.Workplane("XY").add(cq.Solid.extrudeLinear(cq.Face.makeFromWires(w), cq.Vector(0, 0, 15)))
    s = s.edges().chamfer(1)
    s = s.cut(cq.Workplane("XY").circle(12.5).extrude(15)).cut(cq.Workplane("XY").box(6, 18.5, 15, centered=(True, False, False)))
    return s.translate((0, 0, z0))
result = cam(r1, 0).union(cam(lambda t: 140 - r1(t + math.pi), 35))
''',
    # six-way valve: 60 cube rounded R20, 40 internal sphere; top/bottom pipes 40 OD x 30 (bore 30) with 60 x 60 x 8 square
    # flanges; four side pipes 30 OD x 25 (bore 20) with 50 x 6 round flanges; 6 mm bolt holes; eight 10 x 10 x 4 ribs
    "L3_27": '''
import math
b = cq.Workplane("XY").box(60, 60, 60).edges().fillet(20)
for s in (1, -1):
    p = cq.Workplane("XY").circle(20).extrude(40).translate((0, 0, 20 if s > 0 else -60))  # repair: overlap the cube
    f = cq.Workplane("XY").box(60, 60, 8).translate((0, 0, s * 56))
    f = f.cut(cq.Workplane("XY").pushPoints([(22, 22), (-22, 22), (22, -22), (-22, -22)]).circle(3).extrude(10).translate((0, 0, s * 56 - 5)))
    b = b.union(p).union(f)
    for k in range(4):
        rib = cq.Workplane("XZ").polyline([(17, s * 18), (32, s * 18), (17, s * 42)]).close().extrude(2).translate((0, 2, 0))  # repair: ribs floated
        b = b.union(rib.rotate((0, 0, 0), (0, 0, 1), 90 * k + 45))
for k in range(4):
    a = 90 * k
    p = cq.Workplane("YZ").circle(15).extrude(35).translate((20, 0, 0))  # repair: overlap the cube
    f = cq.Workplane("YZ").circle(25).extrude(6).translate((52, 0, 0))
    f = f.cut(cq.Workplane("YZ").pushPoints([(18, 0), (0, 18), (-18, 0), (0, -18)]).circle(3).extrude(6).translate((52, 0, 0)))
    b = b.union(p.union(f).rotate((0, 0, 0), (0, 0, 1), a))
b = b.cut(cq.Workplane("XY").sphere(20))
b = b.cut(cq.Workplane("XY").circle(15).extrude(140).translate((0, 0, -70)))
for k in range(4):
    b = b.cut(cq.Workplane("YZ").circle(10).extrude(60).translate((0, 0, 0)).rotate((0, 0, 0), (0, 0, 1), 90 * k))
result = b
''',
    # conical wave spring: rectangle 8 (radial) x 1.2 (axial) swept along R = 40 - 1.5 n, Z = 8 n + 2.5 sin(6 theta) f(theta)
    # over 5 turns, the wave fading to zero over the last quarter turn at each end; profile kept flat (fixed normal Z)
    "L3_29": '''
import math
T = 10 * math.pi
def f(t):
    return min(1.0, t / (math.pi / 2), (T - t) / (math.pi / 2))
def pt(u):
    t = u * T
    R = 40 - 1.5 * t / (2 * math.pi)
    return (R * math.cos(t), R * math.sin(t), 8 * t / (2 * math.pi) + 2.5 * math.sin(6 * t) * f(t))
# repair (found by a size check, not an error): parametricCurve returned a one-turn path here and the fixed-normal sweep
# a near-empty solid (383 mm3 against about 11,000); an explicit spline and the corrected-Frenet sweep give 11,184 mm3
path = cq.Workplane("XY").add(cq.Wire.assembleEdges([cq.Edge.makeSpline([cq.Vector(*pt(i / 2000)) for i in range(2001)])]))
prof = cq.Workplane("XZ", origin=(40, 0, 0)).rect(8, 1.2)
result = prof.sweep(path, isFrenet=False)
''',
    # slide base 150 x 100 x 30; dovetail rail along X (60 at the base, 15 high, 60 deg flanks, so 77.3 on top), T-slots on
    # both long sides (mouth 10 x 6 deep, head 16 x 6), counterbored 30 hole (40 x 5) in the rail, countersunk 8 mm corner
    # holes, 2 x 1 relief grooves at the rail root
    "L3_30": '''
import math
b = cq.Workplane("XY").box(150, 100, 30, centered=False)
w2 = 60 + 2 * 15 / math.tan(math.radians(60))
rail = cq.Workplane("YZ").polyline([(20, 30), (80, 30), (50 + w2 / 2, 45), (50 - w2 / 2, 45)]).close().extrude(150)
b = b.union(rail)
for y0, s in ((0, 1), (100, -1)):
    mouth = cq.Workplane("XY").box(150, 6, 10, centered=(False, False, True)).translate((0, y0 if s > 0 else y0 - 6, 15))
    head = cq.Workplane("XY").box(150, 6, 16, centered=(False, False, True)).translate((0, y0 + 6 if s > 0 else y0 - 12, 15))
    b = b.cut(mouth).cut(head)
b = b.cut(cq.Workplane("XY").box(150, 2, 1, centered=False).translate((0, 18, 29)))
b = b.cut(cq.Workplane("XY").box(150, 2, 1, centered=False).translate((0, 80, 29)))
b = b.cut(cq.Workplane("XY").center(75, 50).circle(15).extrude(45))
b = b.cut(cq.Workplane("XY").center(75, 50).circle(20).extrude(5).translate((0, 0, 40)))
result = b.faces("<Z").workplane(origin=(0, 0, 0)).pushPoints([(10, -10), (140, -10), (10, -90), (140, -90)]).cskHole(8, 16, 90)
''',
}

REPAIRS = {  # first-attempt errors (the benchmark allows three attempts with the error fed back) and the change made
    "L3_12": {"error": "the solid is not valid (OCCT check failed)", "fix": "the closed groove sweep split into two open halves"},
    "L3_19": {"error": "the solid is not valid (OCCT check failed)", "fix": "round wall ends built from the wall's normal and tangent"},
    "L3_25": {"error": "the solid is not valid (OCCT check failed)", "fix": "port pipes fused solid, then bored through the wall"},
    "L3_5": {"error": "2 solids (the engine's gate needs one)", "fix": "the nozzle reaches into the dome wall"},
    "L3_15": {"error": "2 solids (the engine's gate needs one)", "fix": "the nozzle reaches into the dome wall"},
    "L3_27": {"error": "11 solids (the engine's gate needs one)", "fix": "ribs and pipes overlap the rounded cube"},
    "L3_29": {"error": "none: valid, but a size check found 383 mm3 against about 11,000 (outside the benchmark's protocol)",
              "fix": "explicit spline path and the corrected-Frenet sweep"},
}

assert not set(G) & set(C) and len(G) + len(C) == 30, (len(G), len(C))

FIRST_ATTEMPT = {
    "invalid_paper_definition": ["L3_12", "L3_19", "L3_25"],
    "invalid_rate": "3 of 30 (10.0%)",
    "valid_but_rejected_by_the_engine_gate": ["L3_5", "L3_15", "L3_27"],
    "failing_our_gate": "6 of 30 (20.0%)",
    "valid_but_wrong_found_by_a_size_check": ["L3_29"],
    "after_one_repair_round": "0 of 30 invalid; one answer (L3_26) is two solids because the prompt describes two separate cams",
    "note": "L3_28 first crashed with a missing DLL because the harness ran outside the environment; not counted against the answer",
}
