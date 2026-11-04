"""B4-T1: the gate rejects invalid, multi-solid, out-of-space, thin and tiny-hole shapes with named reasons."""
import cadquery as cq

from structura.geometry.gate import GateLimits, gate
from structura.templates.bracket import BRACKET

DS = [0, 0, 0, 120, 80, 100]


def test_default_bracket_passes():
    r = gate(BRACKET.build().shape, GateLimits(design_space=DS))
    assert r.ok, r.reasons
    assert r.min_wall_mm is not None and 3.0 <= r.min_wall_mm <= 6.5


def test_two_solids_rejected():
    a = cq.Solid.makeBox(10, 10, 10)
    b = cq.Solid.makeBox(10, 10, 10, pnt=cq.Vector(20, 0, 0))
    r = gate(cq.Compound.makeCompound([a, b]))
    assert not r.ok and any("2 solids" in x for x in r.reasons)


def test_outside_design_space_rejected():
    r = gate(cq.Solid.makeBox(10, 10, 10, pnt=cq.Vector(-5, 0, 0)), GateLimits(design_space=[0, 0, 0, 50, 50, 50]))
    assert not r.ok and any("design space" in x for x in r.reasons)


def test_keep_out_rejected():
    r = gate(cq.Solid.makeBox(10, 10, 10), GateLimits(keep_out=[[5, 5, 5, 20, 20, 20]]))
    assert not r.ok and any("keep-out" in x for x in r.reasons)


def test_thin_wall_rejected():
    r = gate(cq.Solid.makeBox(40, 40, 0.8), GateLimits(min_wall=1.2))
    assert not r.ok and any("thinner" in x for x in r.reasons)
    assert abs(r.min_wall_mm - 0.8) < 0.01


def test_small_hole_rejected():
    s = cq.Workplane().box(20, 20, 5).faces(">Z").workplane().hole(1.0).val()
    r = gate(s, GateLimits(min_hole_diameter=2.0))
    assert not r.ok and any("hole of diameter 1.00" in x for x in r.reasons)


def test_boss_is_not_a_hole():
    s = cq.Workplane().box(20, 20, 5).faces(">Z").workplane().circle(0.8).extrude(3).val()
    r = gate(s, GateLimits(min_hole_diameter=2.0, min_wall=1.0))
    assert not any("hole" in x for x in r.reasons)


def test_overhang_reported_as_warning():
    # a T shape: the cap overhangs its stem
    s = cq.Solid.makeBox(10, 10, 20).fuse(cq.Solid.makeBox(40, 10, 5, pnt=cq.Vector(-15, 0, 20))).clean()
    r = gate(s, GateLimits())
    assert r.ok and r.overhang_area_mm2 > 100 and r.warnings
    r2 = gate(s, GateLimits(require_support_free=True))
    assert not r2.ok
