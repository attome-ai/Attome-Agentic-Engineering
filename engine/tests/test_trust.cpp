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

TEST_CASE("F1: a stress requirement is judged on the settled peak when that is worse") {
  auto req = [] {
    return nlohmann::json::array({{{"quantity", "safety_factor"}, {"value", 2.21}, {"limit", 2.0}, {"op", ">="}, {"margin", 0.105}, {"numerical_error", 0.01}, {"result", "pass"}},
                                  {{"quantity", "mass_g"}, {"value", 165.0}, {"limit", 250.0}, {"op", "<="}, {"margin", 0.34}, {"numerical_error", 0.0}, {"result", "pass"}}});
  };
  SUBCASE("a coarse mesh that missed the peak does not pass the design") {
    auto r = req();
    CHECK(apply_settled_peak(r, 13.0, 25.0));  // 25 / 13 = 1.92
    CHECK(r[0]["result"] == "fail");
    CHECK(r[0]["value"].get<double>() == doctest::Approx(25.0 / 13.0));
    CHECK(r[0]["uniform_mesh_value"].get<double>() == doctest::Approx(2.21));
    CHECK(r[0]["margin"].get<double>() < 0);
    CHECK(r[1]["result"] == "pass");  // mass is not a stress
  }
  SUBCASE("a settled peak below the uniform mesh's changes nothing") {
    auto r = req();
    CHECK_FALSE(apply_settled_peak(r, 11.0, 25.0));
    CHECK(r[0]["value"].get<double>() == doctest::Approx(2.21));
    CHECK_FALSE(r[0].contains("uniform_mesh_value"));
  }
  SUBCASE("a margin smaller than the numerical error is not proven") {
    auto r = req();
    r[0]["numerical_error"] = 0.05;
    CHECK(apply_settled_peak(r, 12.3, 25.0));  // 2.033, room 0.033, error 0.10
    CHECK(r[0]["result"] == "not proven");
  }
  SUBCASE("the error then comes from the refinement, and the uniform meshes' error is kept in the record") {
    auto r = req();
    r[0]["numerical_error"] = 0.24;
    r[0]["result"] = "not proven";
    CHECK(apply_settled_peak(r, 12.0, 25.0, 11.96));  // 2.083; 3 x 0.33% = 1%, room 0.083, error 0.021
    CHECK(r[0]["numerical_error"].get<double>() == doctest::Approx(0.01));
    CHECK(r[0]["uniform_mesh_error"].get<double>() == doctest::Approx(0.24));
    CHECK(r[0]["result"] == "pass");
  }
  SUBCASE("a peak still moving between the two levels stays not proven") {
    auto r = req();
    CHECK(apply_settled_peak(r, 12.0, 25.0, 11.0));  // 2.083; 3 x 8.3% = 25%, larger than the 4% margin
    CHECK(r[0]["result"] == "not proven");
  }
}
