// D3-T3: the in-process screening solver agrees with CalculiX and is marked as screening.
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>

#include "structura/evaluate.hpp"

using namespace structura;
namespace fs = std::filesystem;

TEST_CASE("D3-T3: screening evaluation against the CalculiX evaluation") {
  DesignSpec spec = DesignSpec::load((fs::path(STRUCTURA_ROOT) / "docs" / "examples" / "bracket_20kg.json").string());
  Values p = {{"thickness", 6}, {"plate_width", 50}, {"rib_count", 1}, {"rib_thickness", 4}, {"fillet_radius", 3}};
  EvalOptions o;
  o.mesh_size = 6;
  o.threads = 4;
  nlohmann::json ccx = evaluate_design(spec, p, o);
  o.screen = true;
  nlohmann::json scr = evaluate_design(spec, p, o);
  REQUIRE(ccx["status"] == "ok");
  REQUIRE(scr["status"] == "ok");
  CHECK(scr["screening"] == true);
  CHECK(ccx["screening"] == false);
  double d1 = ccx["metrics"]["max_disp_mm"], d2 = scr["metrics"]["max_disp_mm"];
  // screening reports the raw peak over every node, so compare it with CalculiX's raw peak, not its evaluation-region peak
  double s1 = ccx["load_cases"]["service"]["max_von_mises_raw_MPa"], s2 = scr["metrics"]["max_von_mises_MPa"];
  MESSAGE("max displacement " << d1 << " vs " << d2 << " mm; peak von Mises " << s1 << " vs " << s2 << " MPa; time " << ccx["seconds"]
                              << " s (CalculiX) vs " << scr["seconds"] << " s (screening)");
  CHECK(std::fabs(d2 - d1) / d1 < 0.03);
  CHECK(std::fabs(s2 - s1) / s1 < 0.02);  // same recovery as CalculiX: integration points extrapolated to the nodes
}
