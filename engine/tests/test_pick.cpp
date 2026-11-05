// C3: a clicked face keeps its identity when the parameters change, and refuses rather than guesses when it cannot.
#include <doctest/doctest.h>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/occ.hpp"

using namespace structura;

namespace {
ojson signature_of(const nlohmann::json& picked) { return ojson::parse(picked.at("selector").at("signature").dump()); }
gp_Pnt centre_of(const TopoDS_Face& f) { return centre(f); }
}  // namespace

TEST_CASE("C3-T1: a click on the wall face names that face, and it is found again on a different design") {
  DesignGraph g = load_family("bracket");
  CompiledPart a = compile_graph(g);
  TopoDS_Face wall = a.named_faces().at("load_face").at(0);
  gp_Pnt c = centre_of(wall);
  auto picked = pick_face(a, {c.X(), c.Y(), c.Z()});
  CHECK(picked["distance_mm"].get<double>() < 1e-6);
  CHECK(picked["area_mm2"].get<double>() == doctest::Approx(area(wall)));
  CHECK(picked["triangles"].size() >= 9);
  bool named = false;
  for (auto& n : picked["already_named"]) named = named || n == "load_face";
  CHECK(named);
  // same face on the part it was picked from
  auto same = select_picked(a.shape, "p", signature_of(picked));
  REQUIRE(same.size() == 1);
  CHECK(same[0].IsSame(wall));
  // and on a thicker plate: the face moved, but it is still the load face
  CompiledPart b = compile_graph(g, {{"thickness", 12.0}});
  TopoDS_Face wall_b = b.named_faces().at("load_face").at(0);
  auto again = select_picked(b.shape, "p", signature_of(picked));
  REQUIRE(again.size() == 1);
  CHECK(again[0].IsSame(wall_b));
}

TEST_CASE("C3-T1: a signature no face matches is refused, naming the face") {
  CompiledPart a = compile_graph(load_family("bracket"));
  ojson sig = {{"surface", "plane"}, {"direction", {0.57735, 0.57735, 0.57735}}, {"where", {0.5, 0.5, 0.5}}, {"area_fraction", 0.1}};
  CHECK_THROWS_WITH_AS(select_picked(a.shape, "my_face", sig), doctest::Contains("my_face"), CompileError);
}

TEST_CASE("C3-T1: a signature midway between two equal faces is refused rather than guessed") {
  CompiledPart a = compile_graph(load_family("bracket"));
  auto holes = a.named_faces().at("mount_holes");
  REQUIRE(holes.size() >= 2);
  gp_Pnt c0 = centre_of(holes[0]), c1 = centre_of(holes[1]);
  ojson s0 = signature_of(pick_face(a, {c0.X(), c0.Y(), c0.Z()})), s1 = signature_of(pick_face(a, {c1.X(), c1.Y(), c1.Z()}));
  ojson mid = s0;
  for (int i = 0; i < 3; ++i) mid["where"][i] = (s0["where"][i].get<double>() + s1["where"][i].get<double>()) / 2;
  CHECK_THROWS_WITH_AS(select_picked(a.shape, "between", mid), doctest::Contains("ambiguous"), CompileError);
}

TEST_CASE("C3-T1: a picked selector travels through the design graph and its JSON") {
  DesignGraph g = load_family("bracket");
  CompiledPart a = compile_graph(g);
  gp_Pnt c = centre_of(a.named_faces().at("load_face").at(0));
  auto picked = pick_face(a, {c.X(), c.Y(), c.Z()});
  ojson j = ojson::parse(g.to_json().dump());
  j["faces"]["my_face"] = ojson::parse(picked["selector"].dump());
  DesignGraph g2 = DesignGraph::from_json(j);
  CompiledPart b = compile_graph(g2);
  auto named = b.named_faces();
  REQUIRE(named.at("my_face").size() == 1);
  CHECK(named.at("my_face")[0].IsSame(named.at("load_face").at(0)));
  ojson back = ojson::parse(g2.to_json().dump());
  CHECK(back["faces"]["my_face"]["kind"] == "picked");
  CHECK(back["faces"]["my_face"]["signature"]["surface"] == "plane");
}
