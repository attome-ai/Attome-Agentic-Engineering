// A2-T1, A2-T2, A2-T3: spec validation and checks, worst-case load cases, and the feasibility check.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "structura/evaluate.hpp"
#include "structura/spec.hpp"

using namespace structura;
namespace fs = std::filesystem;

namespace {
nlohmann::json example(const std::string& name) {
  std::ifstream in(fs::path(STRUCTURA_ROOT) / "docs" / "examples" / (name + ".json"));
  return nlohmann::json::parse(in);
}
}  // namespace

TEST_CASE("A2-T1: the example spec loads and bad specs are rejected with a reason") {
  DesignSpec s = DesignSpec::from_json(example("bracket_20kg"));
  CHECK(s.free_params.size() == 5);
  CHECK(s.requirements.size() == 3);
  CHECK(s.effective_material().yield == doctest::Approx(25.0));  // PETG 50 MPa x 0.5 print knock-down
  auto bad = [](auto mutate, const char* expect) {
    nlohmann::json j = example("bracket_20kg");
    mutate(j);
    CHECK_THROWS_WITH_AS(DesignSpec::from_json(j), doctest::Contains(expect), SpecError);
  };
  bad([](nlohmann::json& j) { j["material"] = "unobtainium"; }, "unknown material");
  bad([](nlohmann::json& j) { j["free_params"]["thickness"] = {1, 12}; }, "exceed the family");
  bad([](nlohmann::json& j) { j["requirements"][0]["quantity"] = "happiness"; }, "unknown quantity");
  bad([](nlohmann::json& j) { j["load_cases"][0]["loads"][0]["group"] = "nowhere"; }, "not a named face");
  bad([](nlohmann::json& j) { j["requirements"][1]["id"] = "R1"; }, "used twice");
}

TEST_CASE("A2-T1: checks give pass/fail with margins") {
  DesignSpec s = DesignSpec::from_json(example("bracket_20kg"));
  Metrics m;
  m.overall = {{"safety_factor", 3.0}, {"max_disp_mm", 4.5}, {"mass_g", 100}};
  auto c = s.check(m);
  REQUIRE(c.size() == 3);
  CHECK(c[0].passed);
  CHECK(c[0].margin == doctest::Approx(0.5));    // (3 - 2) / 2
  CHECK_FALSE(c[1].passed);
  CHECK(c[1].margin == doctest::Approx(-0.5));   // (3 - 4.5) / 3
  CHECK(c[2].passed);
}

TEST_CASE("A2-T2: loads are evaluated at the high end of their factor range and the governing case is named") {
  nlohmann::json j = example("bracket_20kg");
  nlohmann::json second = j["load_cases"][0];
  second["name"] = "sideways";
  second["factor_range"] = {1.0, 1.0};
  second["loads"][0]["force"] = {0, -100, 0};
  j["load_cases"].push_back(second);
  DesignSpec s = DesignSpec::from_json(j);
  EvalOptions o;
  o.mesh_size = 6;
  o.threads = 4;
  nlohmann::json ev = evaluate_design(s, {{"thickness", 6}, {"plate_width", 50}, {"rib_count", 1}, {"rib_thickness", 4}, {"fillet_radius", 3}}, o);
  REQUIRE(ev["status"] == "ok");
  CHECK(ev["load_cases"]["service"]["applied_N"][2].get<double>() == doctest::Approx(-196.2 * 1.5).epsilon(1e-6));
  CHECK(ev["load_cases"]["sideways"]["applied_N"][1].get<double>() == doctest::Approx(-100).epsilon(1e-6));
  CHECK(ev["governing_case"]["max_von_mises_MPa"] == "service");
  CHECK(ev["checks"].size() == 3);
}

TEST_CASE("A2-T3: an impossible spec is stopped with the best achievable value") {
  DesignSpec s = DesignSpec::from_json(example("bracket_100kg"));
  EvalOptions o;
  o.mesh_size = 6;
  o.threads = 4;
  nlohmann::json f = check_feasibility(s, o);
  MESSAGE(f.dump());
  CHECK_FALSE(f["feasible"].get<bool>());
  bool r1 = false;
  for (auto& p : f["problems"]) r1 = r1 || p["id"] == "R1";
  CHECK(r1);
}
