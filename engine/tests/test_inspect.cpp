// A3: interface detection on an uploaded STEP part, and importing it into the design grammar.
#include <filesystem>
#include <fstream>

#include <doctest/doctest.h>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/graph.hpp"
#include "structura/inspect.hpp"
#include "structura/occ.hpp"

using namespace structura;
namespace fs = std::filesystem;

namespace {
// 80 x 40 x 10 plate with two 6.5 mm through holes along z at (20, 20) and (60, 20)
TopoDS_Shape plate_with_holes() {
  TopoDS_Shape s = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), gp_Pnt(80, 40, 10)).Shape();
  for (double x : {20.0, 60.0})
    s = BRepAlgoAPI_Cut(s, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, 20, -1), gp_Dir(0, 0, 1)), 3.25, 12).Shape()).Shape();
  return s;
}
}  // namespace

TEST_CASE("A3-T1: inspect finds the holes, their pattern, the envelope and the mating faces") {
  nlohmann::json r = inspect_shape(plate_with_holes());
  CHECK(r["valid"].get<bool>());
  CHECK(r["solids"].get<int>() == 1);
  REQUIRE(r["holes"].size() == 2);
  for (auto& h : r["holes"]) {
    CHECK(h["diameter_mm"].get<double>() == doctest::Approx(6.5));
    CHECK(h["depth_mm"].get<double>() == doctest::Approx(10).epsilon(1e-3));
    CHECK(h["selector"]["axis"] == "z");
  }
  REQUIRE(r["hole_patterns"].size() == 1);
  CHECK(r["hole_patterns"][0]["count"].get<int>() == 2);
  CHECK(r["envelope"][3].get<double>() == doctest::Approx(80).epsilon(1e-4));
  // the two large faces (top and bottom) come first among the mating candidates
  auto& m = r["mating_candidates"];
  REQUIRE(m.size() >= 2);
  CHECK(std::abs(m[0]["normal"][2].get<double>()) == doctest::Approx(1));
  CHECK(r["planes_not_axis_aligned"].get<int>() == 0);
}

TEST_CASE("A3-T2: the draft family imports the STEP file, pins its content and names its faces") {
  fs::path dir = fs::temp_directory_path() / "structura_a3";
  fs::create_directories(dir);
  std::string step = (dir / "plate.step").string();
  write_step(plate_with_holes(), step);
  nlohmann::json fam = draft_family("plate_upload", step, inspect_shape(plate_with_holes()));
  CHECK(fam["features"][0]["type"] == "import_step");
  CHECK(fam["features"][0]["params"]["sha256"].get<std::string>().size() == 64);
  CHECK(fam["faces"].contains("holes_d65"));

  DesignGraph g = DesignGraph::from_json(fam);
  CompiledPart part = compile_graph(g, {});
  CHECK(volume(part.shape) == doctest::Approx(80 * 40 * 10 - 2 * 3.14159265 * 3.25 * 3.25 * 10).epsilon(1e-4));
  auto named = part.named_faces();
  CHECK(named.at("holes_d65").size() == 2);  // one cylindrical face per hole

  // a changed file is refused: results can never be reused for different geometry
  { std::ofstream(step, std::ios::app) << "\n"; }
  CHECK_THROWS(compile_graph(g, {}));
}
