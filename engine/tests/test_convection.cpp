// C2: convection coefficient from the plate-fin channel correlation (Teertstra et al. 1999) instead of a fixed h.
#include <cmath>

#include <doctest/doctest.h>

#include "structura/fea.hpp"
#include "structura/spec.hpp"

using namespace structura;

TEST_CASE("C2: air properties match Incropera Table A.4 at the table points and interpolate between them") {
  auto a = air_properties(300 - 273.15);
  CHECK(a.k == doctest::Approx(26.3e-3));
  CHECK(a.nu == doctest::Approx(15.89e-6));
  CHECK(a.Pr == doctest::Approx(0.707));
  auto m = air_properties(325 - 273.15);
  CHECK(m.k == doctest::Approx((26.3e-3 + 30.0e-3) / 2));
  CHECK_THROWS(air_properties(200));  // outside the table: refuse rather than extrapolate
}

TEST_CASE("C2: plate-fin channel h against a hand calculation and both limits of the composite model") {
  // 4 fins of 2.5 mm across 60 mm: gap 16.667 mm, 60 mm long, 1.5 m/s, film 50 degC (hand value computed separately)
  nlohmann::json info;
  double h = plate_fin_channel_h(1.5, (60.0 - 2.5) / 3 - 2.5, 2.5, 60, 50, &info);
  CHECK(info["h_W_m2K"].get<double>() == doctest::Approx(22.518).epsilon(1e-3));
  CHECK(info["Re_star"].get<double>() == doctest::Approx(438.34).epsilon(1e-3));
  CHECK(h == doctest::Approx(22.518e-6).epsilon(1e-3));  // engine units W/(mm^2 K)
  CHECK(info["laminar"].get<bool>());

  auto a = air_properties(50);
  // narrow, long channel: fully developed limit Nu = Re* Pr / 2
  nlohmann::json n;
  plate_fin_channel_h(0.2, 1.0, 0.0, 500, 50, &n);
  double rs = n["Re_star"];
  CHECK(n["Nu_b"].get<double>() == doctest::Approx(rs * a.Pr / 2).epsilon(0.02));
  // wide, short channel: developing (flat-plate) limit
  nlohmann::json w;
  plate_fin_channel_h(5.0, 40.0, 0.0, 20, 50, &w);
  double rw = w["Re_star"];
  double dev = 0.664 * std::sqrt(rw) * std::cbrt(a.Pr) * std::sqrt(1 + 3.65 / std::sqrt(rw));
  CHECK(w["Nu_b"].get<double>() == doctest::Approx(dev).epsilon(0.02));
  // more air, more cooling; a narrower gap at the same velocity raises h in the developing regime
  CHECK(plate_fin_channel_h(3.0, 10, 2, 60, 50) > plate_fin_channel_h(1.0, 10, 2, 60, 50));
  CHECK_THROWS(plate_fin_channel_h(1.0, 0.0, 2, 60, 50));
}

TEST_CASE("C2: a correlation film is resolved per design and validated at spec load") {
  nlohmann::json tc = {{"name", "steady"},
                       {"fluxes", {{{"group", "base_bottom"}, {"power", 10.0}}}},
                       {"films", {{{"group", "exposed_py"}, {"sink", 35.0},
                                   {"correlation", {{"velocity_m_s", 1.5},
                                                    {"spacing_mm", "(60 - fin_thickness) / (fin_count - 1) - fin_thickness"},
                                                    {"fin_thickness_mm", "fin_thickness"}, {"length_mm", 60}}}}}}};
  ThermalCase t = ThermalCase::from_json(tc);
  REQUIRE(t.has_correlation());
  nlohmann::json info;
  ThermalCase r4 = t.resolved({{"fin_count", 4}, {"fin_thickness", 2.5}}, &info);
  ThermalCase r12 = t.resolved({{"fin_count", 12}, {"fin_thickness", 2.5}});
  CHECK(info["exposed_py"]["spacing_mm"].get<double>() == doctest::Approx(16.6667).epsilon(1e-4));
  CHECK(r4.films[0].h > 0);
  CHECK(r12.films[0].h != doctest::Approx(r4.films[0].h));  // h follows the design

  nlohmann::json both = tc;
  both["films"][0]["h"] = 2.5e-5;
  CHECK_THROWS(ThermalCase::from_json(both));

  nlohmann::json spec = nlohmann::json::parse(R"J({
    "version": 1, "id": "c2-test", "family": "heatsink", "material": "AL6061T6", "apply_print_knockdown": false,
    "free_params": {"fin_count": [4, 16]},
    "objectives": [{"quantity": "mass_g", "sense": "min"}, {"quantity": "peak_temperature", "sense": "min"}],
    "requirements": [{"id": "R1", "quantity": "peak_temperature", "op": "<=", "value": 75}],
    "thermal_cases": []})J");
  spec["thermal_cases"].push_back(tc);
  CHECK_NOTHROW(DesignSpec::from_json(spec));
  spec["thermal_cases"][0]["films"][0]["correlation"]["spacing_mm"] = "fin_gap * 2";
  CHECK_THROWS_AS(DesignSpec::from_json(spec), SpecError);
}
