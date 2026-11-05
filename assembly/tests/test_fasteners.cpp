// Fasteners: resistances against published Eurocode values, thread data against the ISO formula, and the wall-plate
// statics against a hand calculation.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>

#include "attome/fasteners.hpp"

using namespace attome;
using nlohmann::json;

TEST_CASE("EN 1993-1-8 resistances of an M12 8.8 bolt match the published tables (48.6 kN tension, 32.4 kN shear)") {
  Resistance r = en1993_resistance(iso_thread(12), bolt_grade("8.8"));
  CHECK(r.Ft_Rd / 1000 == doctest::Approx(48.6).epsilon(0.002));
  CHECK(r.Fv_Rd / 1000 == doctest::Approx(32.4).epsilon(0.002));
}

TEST_CASE("tabulated stress areas agree with the ISO 898-1 formula As = pi/4 ((d2 + d3)/2)^2") {
  for (double d : {3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 16.0, 20.0, 24.0, 30.0}) {
    Thread t = iso_thread(d);
    double d2 = d - 0.649519 * t.pitch, d3 = d - 1.226869 * t.pitch;
    double As = M_PI / 4 * std::pow((d2 + d3) / 2, 2);
    CHECK(t.As == doctest::Approx(As).epsilon(0.01));
  }
}

TEST_CASE("sizes and grades outside the tables are refused, not guessed") {
  CHECK_THROWS(iso_thread(7));
  CHECK_THROWS(bolt_grade("12.9"));  // not covered by EN 1993-1-8
}

TEST_CASE("wall plate with two bolts: tension and shear match the hand calculation") {
  // plate 6 mm thick on the wall face x = 0, bottom edge z = 0; M6 bolts at z = 30 and z = 70; 10 kg centred 100 mm out
  json j = json::parse(R"J({"parts": [
    {"name": "wall", "kind": "box", "at": [-40, -50, -20], "size": [40, 100, 140], "fixed": true},
    {"name": "plate", "kind": "box", "at": [0, -25, 0], "size": [6, 50, 100]},
    {"name": "bolt_1", "kind": "bolt", "at": [6, 0, 30], "axis": [-1, 0, 0], "diameter": 6, "length": 25},
    {"name": "bolt_2", "kind": "bolt", "at": [6, 0, 70], "axis": [-1, 0, 0], "diameter": 6, "length": 25},
    {"name": "weight", "kind": "box", "at": [80, -20, 100], "size": [40, 40, 10], "mass_kg": 10}]})J");
  auto parts = load_assembly(j, ".");
  json r = wall_bolt_check(parts, j);
  REQUIRE(r["evaluated"].get<bool>());
  CHECK(r["held_part"] == "plate");
  CHECK(r["load_N"].get<double>() == doctest::Approx(98.1));
  CHECK(r["lever_mm"].get<double>() == doctest::Approx(100.0).epsilon(1e-6));
  // M = 98.1 N x 100 mm = 9810 N mm; sum z^2 = 30^2 + 70^2 = 5800 mm^2
  json top = r["bolts"][1];
  CHECK(top["tension_N"].get<double>() == doctest::Approx(9810.0 * 70 / 5800).epsilon(1e-4));
  CHECK(r["bolts"][0]["tension_N"].get<double>() == doctest::Approx(9810.0 * 30 / 5800).epsilon(1e-4));
  CHECK(top["shear_N"].get<double>() == doctest::Approx(49.05));
  // unstated grade: the protective 4.6 is assumed and flagged
  CHECK(top["grade"] == "4.6");
  CHECK_FALSE(top["grade_stated"].get<bool>());
  // 4.6, M6: Ft,Rd = 0.9 x 400 x 20.1 / 1.25 = 5788.8 N, Fv,Rd = 0.6 x 400 x 20.1 / 1.25 = 3859.2 N
  CHECK(top["interaction"].get<double>() == doctest::Approx(49.05 / 3859.2 + (9810.0 * 70 / 5800) / (1.4 * 5788.8)).epsilon(1e-4));
  CHECK(r["verdict"] == "pass");
}

TEST_CASE("a plug without a datasheet load leaves the verdict incomplete; an overloaded plug fails") {
  const char* base = R"J({"parts": [
    {"name": "wall", "kind": "box", "at": [-40, -50, -20], "size": [40, 100, 140], "fixed": true},
    {"name": "plate", "kind": "box", "at": [0, -25, 0], "size": [6, 50, 100]},
    {"name": "plug", "kind": "tube", "fastener": true, "at": [-32, 0, 70], "axis": [1, 0, 0], "diameter": 8, "inner_diameter": 6.2, "length": 32 PLUG},
    {"name": "bolt", "kind": "bolt", "at": [6, 0, 70], "axis": [-1, 0, 0], "diameter": 6, "length": 25},
    {"name": "weight", "kind": "box", "at": [80, -20, 100], "size": [40, 40, 10], "mass_kg": 10}]})J";
  auto with = [&](const std::string& extra) {
    std::string s = base;
    s.replace(s.find(" PLUG"), 5, extra);
    json j = json::parse(s);
    return wall_bolt_check(load_assembly(j, "."), j);
  };
  json unknown = with("");
  CHECK(unknown["verdict"] == "incomplete");
  CHECK(unknown["bolts"][0]["plug"] == "plug");
  // one bolt at z = 70 carries M = 9810 N mm alone: T = 9810 x 70 / 4900 = 140.1 N
  json weak = with(R"(, "allowed_tension_kN": 0.1)");
  CHECK(weak["verdict"] == "fail");
  CHECK(weak["bolts"][0]["tension_N"].get<double>() == doctest::Approx(140.14).epsilon(1e-3));
  json strong = with(R"(, "allowed_tension_kN": 0.5)");
  CHECK(strong["verdict"] == "pass");
}
