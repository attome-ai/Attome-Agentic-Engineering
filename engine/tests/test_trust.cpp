// F1-T1, F1-T2: trust record, GCI error bars, independent checks, grade and reproduction.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "structura/trust.hpp"

using namespace structura;
namespace fs = std::filesystem;

TEST_CASE("F1-T2: grid convergence index on a known second-order sequence") {
  // f(h) = 1 + 0.1 h^2 at h = 1, 1.5, 2.25
  auto f = [](double h) { return 1 + 0.1 * h * h; };
  Gci g = grid_convergence(f(1), f(1.5), f(2.25), 1, 1.5, 2.25);
  CHECK(g.monotone);
  CHECK(g.order == doctest::Approx(2.0).epsilon(1e-6));
  // GCI = 1.25 |e21 / f1| / (r^p - 1) with e21 = 0.125, r = 1.5, p = 2
  CHECK(g.gci == doctest::Approx(1.25 * (0.125 / 1.1) / (2.25 - 1)).epsilon(1e-6));
}

TEST_CASE("F1-T1: trust record for the demo bracket") {
  fs::path sp = fs::path(STRUCTURA_ROOT) / "docs" / "examples" / "bracket_20kg.json";
  DesignSpec spec = DesignSpec::load(sp.string());
  TrustOptions o;
  o.fine_size = 3;
  o.workdir = (fs::temp_directory_path() / "structura_trust_test").string();
  o.spec_path = sp.string();
  fs::remove_all(o.workdir);
  Values free = {{"thickness", 8}, {"plate_width", 50}, {"rib_count", 2}, {"rib_thickness", 5}, {"fillet_radius", 3}};
  nlohmann::json t = build_trust(spec, free, o);
  MESSAGE(t.dump(1).substr(0, 3000));
  REQUIRE(t.contains("grade"));
  CHECK(fs::exists(fs::path(o.workdir) / "trust.json"));
  CHECK(t["requirements"].size() == 3);
  for (auto& r : t["requirements"]) CHECK(r.contains("numerical_error"));
  bool energy = false, equilibrium = false;
  for (auto& c : t["independent_checks"]) {
    std::string n = c["check"];
    if (n.find("strain energy") != std::string::npos) { energy = true; CHECK(c["passed"] == true); }
    if (n.find("equilibrium") != std::string::npos) { equilibrium = true; CHECK(c["passed"] == true); }
  }
  CHECK(energy);
  CHECK(equilibrium);
  // D10: the sharp rib-tip corner is found and caps the grade, whatever the uniform meshes say
  REQUIRE(t.contains("peak_probe"));
  CHECK(t["peak_probe"]["singular"] == true);
  CHECK(t["peak_probe"]["refined_twice_peak_MPa"].get<double>() > t["peak_probe"]["uniform_mesh_peak_MPa"].get<double>());
  CHECK(t["grade"] != "A");
  CHECK(!t["assumptions"].empty());
  CHECK(t["grade_rule"].get<std::string>().find("A:") == 0);
  // a verified status is only possible with grade A or B
  if (t["status"] == "verified") CHECK((t["grade"] == "A" || t["grade"] == "B"));

  SUBCASE("F1-T2: the evidence bundle reproduces") {
    nlohmann::json r = reproduce(o.workdir);
    CHECK(r["match"] == true);
  }
}
