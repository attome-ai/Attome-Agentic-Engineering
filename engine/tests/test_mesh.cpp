// C4-T1 (C++): meshing returns a mesh with checked named groups, or a logged failure after four attempts.
#include <doctest/doctest.h>

#include <filesystem>

#include <BRepPrimAPI_MakeBox.hxx>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/mesh.hpp"

using namespace structura;
namespace fs = std::filesystem;

TEST_CASE("C4-T1: bracket default meshes with checked groups") {
  CompiledPart p = compile_graph(load_family("bracket"));
  auto named = p.named_faces();
  std::map<std::string, std::vector<TopoDS_Face>> use = {{"mount_holes", named.at("mount_holes")}, {"load_face", named.at("load_face")}};
  fs::path dir = fs::temp_directory_path() / "structura_test_mesh";
  fs::remove_all(dir);
  MeshOptions o;
  o.size = 4.0;
  Mesh m = mesh_shape(p.shape, use, dir.string(), o);
  CHECK(m.tets.size() > 1000);
  CHECK(m.report["min_quality"].get<double>() > 0.05);
  for (auto& [name, g] : m.report["groups"].items()) CHECK_MESSAGE(g["ok"].get<bool>(), name);
  CHECK(!m.surfaces.at("load_face").empty());
  CHECK(!m.surfaces.at("mount_holes").empty());
  for (auto& t : m.tets) CHECK(t.size() == 11);
}

TEST_CASE("C4-T1: an unmeshable input fails after every ladder attempt is logged") {
  // A shape whose named group cannot exist forces nothing; instead point gmsh at a broken STEP via a 0-size box.
  fs::path dir = fs::temp_directory_path() / "structura_test_mesh_fail";
  fs::remove_all(dir);
  MeshOptions o;
  o.size = 1e-9;  // gmsh rejects or times out on an impossible size
  o.timeout_s = 10;
  TopoDS_Shape b = BRepPrimAPI_MakeBox(1, 1, 1).Shape();
  bool threw = false;
  try {
    mesh_shape(b, {}, dir.string(), o);
  } catch (const MeshError& e) {
    threw = true;
    CHECK(e.attempts.size() == 5);  // delaunay, delaunay straight-sided, hxt, frontal, coarser delaunay
  }
  CHECK(threw);
}
