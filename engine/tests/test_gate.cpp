// B4-T1 (C++): the gate rejects invalid, multi-solid, out-of-space, thin and tiny-hole shapes with named reasons.
#include <doctest/doctest.h>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/gate.hpp"

using namespace structura;

namespace {
TopoDS_Shape box(double x, double y, double z, double dx, double dy, double dz) {
  return BRepPrimAPI_MakeBox(gp_Pnt(x, y, z), dx, dy, dz).Shape();
}
bool has(const std::vector<std::string>& v, const std::string& s) {
  for (auto& x : v)
    if (x.find(s) != std::string::npos) return true;
  return false;
}
}  // namespace

TEST_CASE("B4-T1: default bracket passes") {
  CompiledPart p = compile_graph(load_family("bracket"));
  GateLimits l;
  l.design_space = p.design_space();
  GateResult r = gate(p.shape, l);
  CHECK_MESSAGE(r.ok, (r.reasons.empty() ? "" : r.reasons[0]));
  REQUIRE(r.min_wall_mm);
  CHECK(*r.min_wall_mm >= 3.0);
  CHECK(*r.min_wall_mm <= 6.5);
}

TEST_CASE("B4-T1: two solids rejected") {
  TopoDS_Compound c;
  BRep_Builder b;
  b.MakeCompound(c);
  b.Add(c, box(0, 0, 0, 10, 10, 10));
  b.Add(c, box(20, 0, 0, 10, 10, 10));
  GateResult r = gate(c);
  CHECK(!r.ok);
  CHECK(has(r.reasons, "2 solids"));
}

TEST_CASE("B4-T1: outside design space rejected") {
  GateLimits l;
  l.design_space = std::vector<double>{0, 0, 0, 50, 50, 50};
  GateResult r = gate(box(-5, 0, 0, 10, 10, 10), l);
  CHECK(!r.ok);
  CHECK(has(r.reasons, "design space"));
}

TEST_CASE("B4-T1: keep-out rejected") {
  GateLimits l;
  l.keep_out = {{5, 5, 5, 20, 20, 20}};
  GateResult r = gate(box(0, 0, 0, 10, 10, 10), l);
  CHECK(!r.ok);
  CHECK(has(r.reasons, "keep-out"));
}

TEST_CASE("B4-T1: thin wall rejected and measured") {
  GateResult r = gate(box(0, 0, 0, 40, 40, 0.8));
  CHECK(!r.ok);
  CHECK(has(r.reasons, "thinner"));
  REQUIRE(r.min_wall_mm);
  CHECK(*r.min_wall_mm == doctest::Approx(0.8).epsilon(0.02));
}

TEST_CASE("B4-T1: small hole rejected, small boss is not a hole") {
  TopoDS_Shape plate = box(-10, -10, 0, 20, 20, 5);
  TopoDS_Shape holed = BRepAlgoAPI_Cut(plate, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, -1), gp_Dir(0, 0, 1)), 0.5, 7).Shape()).Shape();
  GateResult r = gate(holed);
  CHECK(!r.ok);
  CHECK(has(r.reasons, "hole of diameter 1.00"));
  TopoDS_Shape boss = BRepAlgoAPI_Fuse(plate, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 5), gp_Dir(0, 0, 1)), 0.8, 3).Shape()).Shape();
  GateLimits l;
  l.min_wall = 1.0;
  GateResult r2 = gate(boss, l);
  CHECK(!has(r2.reasons, "hole"));
}

TEST_CASE("B4-T1: overhang reported as warning unless support-free is required") {
  TopoDS_Shape t = BRepAlgoAPI_Fuse(box(0, 0, 0, 10, 10, 20), box(-15, 0, 20, 40, 10, 5)).Shape();
  GateResult r = gate(t);
  CHECK(r.ok);
  REQUIRE(r.overhang_area_mm2);
  CHECK(*r.overhang_area_mm2 > 100);
  CHECK(!r.warnings.empty());
  GateLimits l;
  l.require_support_free = true;
  CHECK(!gate(t, l).ok);
}

TEST_CASE("B4-T4: a wall exactly at the minimum passes (floating-point tolerance)") {
  // heat sink at its thinnest allowed fin: 1.2 mm measures as 1.1999999999999 mm and was rejected against a 1.2 mm minimum
  CompiledPart part = compile_graph(load_family("heatsink"), {{"base_thickness", 2}, {"fin_count", 4}, {"fin_thickness", 1.2}, {"fin_height", 10}});
  GateLimits lim;
  lim.min_wall = 1.2;
  GateResult r = gate(part.shape, lim);
  CHECK(r.ok);
  lim.min_wall = 1.25;  // a real shortfall still fails
  CHECK_FALSE(gate(part.shape, lim).ok);
}
