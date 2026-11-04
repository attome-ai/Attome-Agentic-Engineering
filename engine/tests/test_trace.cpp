// A2-T4: every requirement is traceable to its evidence and spec changes are diffable.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "structura/trace.hpp"

using namespace structura;
namespace fs = std::filesystem;

namespace {
nlohmann::json example() {
  std::ifstream in(fs::path(STRUCTURA_ROOT) / "docs" / "examples" / "bracket_20kg.json");
  return nlohmann::json::parse(in);
}
}  // namespace

TEST_CASE("A2-T4: trace rows link requirement, source, checker and evidence") {
  DesignSpec s = DesignSpec::from_json(example());
  nlohmann::json ev = {{"status", "ok"},
                       {"checks", {{{"id", "R1"}, {"value", 3.1}, {"margin", 0.55}, {"passed", true}, {"load_case", "service"}},
                                   {{"id", "R2"}, {"value", 1.2}, {"margin", 0.6}, {"passed", true}, {"load_case", "service"}}}}};
  auto rows = trace(s, ev, "runs/x/evaluation.json");
  REQUIRE(rows.size() == 3);
  CHECK(rows[0]["id"] == "R1");
  CHECK(rows[0]["source"].get<std::string>().find("safety factor 2") != std::string::npos);
  CHECK(rows[0]["evidence"] == "runs/x/evaluation.json");
  CHECK(rows[0]["passed"] == true);
  CHECK(rows[2]["passed"] == false);  // R3 has no check in this evaluation
  CHECK(rows[2].contains("note"));
}

TEST_CASE("A2-T4: versions are numbered, diffs list changed fields, ids are never repurposed") {
  fs::path dir = fs::temp_directory_path() / "structura_spec_versions";
  fs::remove_all(dir);
  nlohmann::json a = example();
  save_spec_version(a, dir.string());
  nlohmann::json b = a;
  b["requirements"][1]["value"] = 2.0;
  std::string p2 = save_spec_version(b, dir.string());
  CHECK(p2.find("spec.v2.json") != std::string::npos);
  auto d = spec_diff(a, b);
  REQUIRE(d.size() == 1);
  CHECK(d[0]["path"] == "/requirements/1/value");
  CHECK(d[0]["old"] == 3.0);
  CHECK(d[0]["new"] == 2.0);
  nlohmann::json c = b;
  c["requirements"][1]["quantity"] = "mass_g";
  CHECK_THROWS_AS(save_spec_version(c, dir.string()), SpecError);
}
