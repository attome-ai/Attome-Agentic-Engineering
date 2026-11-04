// B4-T2: repairable failures pass the gate after repair, every change is logged, and unsafe repairs are rejected.
#include <doctest/doctest.h>

#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>

#include "structura/occ.hpp"
#include "structura/repair.hpp"

using namespace structura;

namespace {
// 20 x 20 x 10 block with one vertical edge rounded by a 0.1 mm fillet (edges shorter than a 0.3 mm feature)
TopoDS_Shape block_with_tiny_fillet(double r) {
  TopoDS_Shape b = BRepPrimAPI_MakeBox(20, 20, 10).Shape();
  BRepFilletAPI_MakeFillet mk(b);
  for (auto& e : edges_of(b)) {
    auto bb = bbox(e);
    if (std::fabs(bb[0] - 20) < 1e-9 && std::fabs(bb[3] - 20) < 1e-9 && std::fabs(bb[1] - 20) < 1e-9 && std::fabs(bb[4] - 20) < 1e-9) {
      mk.Add(r, e);
      break;
    }
  }
  mk.Build();
  return mk.Shape();
}
}  // namespace

TEST_CASE("B4-T2: a tiny fillet fails the gate, is removed by repair, and the result passes") {
  TopoDS_Shape s = block_with_tiny_fillet(0.1);
  GateLimits lim;
  RepairOptions opt;
  RepairGateResult r = repair_and_gate(s, lim, opt);
  CHECK_FALSE(r.before.ok);
  CHECK(r.repair.changed);
  CHECK(r.repair.accepted);
  CHECK(r.ok);
  CHECK(r.repair.log.size() >= 1);
  CHECK(r.repair.log[0]["operation"] == "defeature");
  CHECK(std::fabs(volume(r.shape) - 2000.0 * 2) < 0.01);  // back to the sharp 20 x 20 x 10 block
}

TEST_CASE("B4-T2: repair refuses to touch a frozen face") {
  TopoDS_Shape s = block_with_tiny_fillet(0.1);
  RepairOptions opt;
  for (auto& f : faces_of(s))
    if (area(f) < opt.min_feature * 20 * 2) opt.frozen.push_back(f);  // freeze the fillet face itself
  REQUIRE(opt.frozen.size() == 1);
  RepairResult r = repair(s, opt);
  // the frozen fillet face is never offered for removal, so nothing is defeatured and the fillet survives
  for (auto& e : r.log) CHECK(e["operation"] != "defeature");
  CHECK(volume(r.shape) == doctest::Approx(volume(s)).epsilon(1e-9));
}

TEST_CASE("B4-T2: a valid part is left alone") {
  TopoDS_Shape b = BRepPrimAPI_MakeBox(20, 20, 10).Shape();
  RepairGateResult r = repair_and_gate(b, {}, {});
  CHECK(r.ok);
  CHECK_FALSE(r.repair.changed);
}
