// H1: manufacturing rules and cost on shapes with known answers.
#include <doctest/doctest.h>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pnt.hxx>

#include <filesystem>
#include <map>
#include "structura/manufacture.hpp"
#include "structura/occ.hpp"
#include "structura/spec.hpp"

using namespace structura;

namespace {
TopoDS_Shape box(double x0, double y0, double z0, double x1, double y1, double z1) {
  return BRepPrimAPI_MakeBox(gp_Pnt(x0, y0, z0), gp_Pnt(x1, y1, z1)).Shape();
}
// a 10 x 10 x 50 column with a 50 x 10 x 5 shelf on top reaching 40 mm out in +x
TopoDS_Shape shelf() { return BRepAlgoAPI_Fuse(box(0, 0, 0, 10, 10, 50), box(0, 0, 45, 50, 10, 50)).Shape(); }
// 50 x 50 x 20 block with a 20 x 20 x 10 square pocket milled from the top
TopoDS_Shape pocket() { return BRepAlgoAPI_Cut(box(0, 0, 0, 50, 50, 20), box(15, 15, 10, 35, 35, 21)).Shape(); }
}  // namespace

TEST_CASE("H1-T1: support volume is zero for a box on the bed and projected area times height under a shelf") {
  CHECK(support_volume(box(0, 0, 0, 100, 50, 20), {0, 0, 1}, 45) == doctest::Approx(0).epsilon(1e-9));
  CHECK(support_volume(shelf(), {0, 0, 1}, 45) == doctest::Approx(40 * 10 * 45).epsilon(1e-6));
  // printed upside down the shelf lies on the bed and the column stands on it: nothing overhangs
  CHECK(support_volume(shelf(), {0, 0, -1}, 45) == doctest::Approx(0).epsilon(1e-9));
}

TEST_CASE("H1-T2: undercut area by ray casting along the tool directions") {
  CHECK(undercut_area(box(0, 0, 0, 100, 50, 20), {{0, 0, 1}, {0, 0, -1}}) == doctest::Approx(0));
  // from the top only: the downward faces (shelf underside 400 mm^2, column bottom 100 mm^2) and the column side under the
  // shelf (10 x 45 = 450 mm^2, the tool cannot get beneath the shelf) cannot be reached
  double u = undercut_area(shelf(), {{0, 0, 1}}, 6000);
  CHECK(u == doctest::Approx(950).epsilon(0.08));
  // flipping the part for a second setup reaches them
  CHECK(undercut_area(shelf(), {{0, 0, 1}, {0, 0, -1}}, 3000) == doctest::Approx(0));
}

TEST_CASE("H1-T3: CNC finds the four sharp internal corners of a square pocket; costs follow the stated inputs") {
  nlohmann::json P = load_processes();
  nlohmann::json r = cnc_report(pocket(), "AL6061T6", P);
  CHECK(r["sharp_internal_corners"].get<int>() == 4);
  bool cnc2 = true;
  for (auto& x : r["rules"])
    if (x["id"] == "CNC2") cnc2 = x["passed"];
  CHECK_FALSE(cnc2);
  double m = P["cnc"]["stock_margin_mm"]["value"];
  CHECK(r["stock_volume_mm3"].get<double>() == doctest::Approx((50 + 2 * m) * (50 + 2 * m) * (20 + 2 * m)));
  CHECK(r["removed_volume_mm3"].get<double>() == doctest::Approx(r["stock_volume_mm3"].get<double>() - (50 * 50 * 20 - 20 * 20 * 10)));
  // a comb (fins on a base, fused, so the fin ends and the base end form one non-convex face) has no internal corner
  // along z: the fin-to-base junctions run along x
  TopoDS_Shape comb = box(0, 0, 0, 60, 60, 3);
  for (int i = 0; i < 4; ++i) comb = BRepAlgoAPI_Fuse(comb, box(0, i * 19.0, 3, 60, i * 19.0 + 3, 20)).Shape();
  CHECK(cnc_report(comb, "AL6061T6", P)["sharp_internal_corners"].get<int>() == 0);
  // a plain box has no internal corners
  CHECK(cnc_report(box(0, 0, 0, 100, 50, 20), "AL6061T6", P)["sharp_internal_corners"].get<int>() == 0);
}

TEST_CASE("H1-T4: FDM report, process compatibility and advice") {
  nlohmann::json P = load_processes();
  nlohmann::json f = fdm_report(shelf(), "PETG", {0, 0, 1}, P);
  double V = 10 * 10 * 50 + 40 * 10 * 5;
  CHECK(f["part_volume_mm3"].get<double>() == doctest::Approx(V));
  double printed = V + 0.15 * 40 * 10 * 45;
  CHECK(f["time_h"].get<double>() == doctest::Approx(printed / 5.0 / 3600 + 10.0 / 60).epsilon(1e-6));
  CHECK(f["cost"]["material"].get<double>() == doctest::Approx(printed * 1.27e-9 * 1e3 * 25).epsilon(0.05));

  nlohmann::json all = manufacture_report(shelf(), "PETG", {0, 0, 1}, P);
  CHECK(all.contains("fdm"));
  CHECK(all.contains("cnc"));
  nlohmann::json al = manufacture_report(shelf(), "AL6061T6", {0, 0, 1}, P);
  CHECK_FALSE(al.contains("fdm"));  // FDM is not set up for aluminium
  CHECK(al["recommended_process"] == "cnc");
}

TEST_CASE("H1-T5: cost as a spec quantity needs a process; process_cost matches the report") {
  nlohmann::json j = nlohmann::json::parse(R"J({
    "version": 1, "id": "h1-test", "family": "bracket", "material": "PETG",
    "free_params": {"thickness": [4, 10]},
    "objectives": [{"quantity": "mass_g", "sense": "min"}, {"quantity": "cost", "sense": "min"}],
    "requirements": [{"id": "R1", "quantity": "safety_factor", "op": ">=", "value": 2}],
    "load_cases": [{"name": "service", "supports": [{"group": "mount_holes"}], "loads": [{"group": "load_face", "force": [0, 0, -100]}]}]})J");
  CHECK_THROWS_AS(DesignSpec::from_json(j), SpecError);
  j["process"] = "fdm";
  CHECK(DesignSpec::from_json(j).process == "fdm");
  j["process"] = "laser";
  CHECK_THROWS_AS(DesignSpec::from_json(j), SpecError);

  nlohmann::json P = load_processes();
  nlohmann::json d;
  double c = process_cost(shelf(), "PETG", {0, 0, 1}, P, "fdm", &d);
  CHECK(c == doctest::Approx(fdm_report(shelf(), "PETG", {0, 0, 1}, P)["cost"]["total"].get<double>()));
  CHECK_THROWS(process_cost(shelf(), "AL6061T6", {0, 0, 1}, P, "fdm"));  // not a printable material
}

TEST_CASE("H1-T2: the print direction follows tension across the layers and support") {
  DesignSpec spec = DesignSpec::load((std::filesystem::path(STRUCTURA_ROOT) / "docs" / "examples" / "bracket_20kg.json").string());
  std::map<std::string, double> p = {{"thickness", 8}, {"plate_width", 50}, {"rib_count", 2}, {"rib_thickness", 5}, {"fillet_radius", 3}};
  nlohmann::json r = orientation_report(spec, p, (std::filesystem::temp_directory_path() / "structura_orient").string(), 4);
  REQUIRE(r["directions"].size() == 6);
  auto tension = [&](int i) { return r["directions"][i]["tension_across_layers_MPa"].get<double>(); };
  // the arm bends in the x-z plane: stress runs along x, so layers stacked along x are pulled apart, layers stacked along y are not
  CHECK(tension(2) > tension(4));
  // opposite directions give the same layers, so the same tension
  CHECK(tension(2) == doctest::Approx(tension(3)));
  CHECK(r["recommended"].contains("direction"));
  if (r["recommended"]["layers_ok"].get<bool>())
    for (auto& d : r["directions"])
      if (d["layers_ok"].get<bool>()) CHECK(r["recommended"]["support_volume_mm3"].get<double>() <= d["support_volume_mm3"].get<double>());
}
