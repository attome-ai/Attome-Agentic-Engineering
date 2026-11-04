// H2-T1: a design bundle opens in ParaView and a slicer; the STEP re-imports as one valid solid.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/export.hpp"
#include "structura/occ.hpp"

using namespace structura;
namespace fs = std::filesystem;

TEST_CASE("H2-T1: STEP and STL export, STEP re-imports as one valid solid") {
  CompiledPart p = compile_graph(load_family("bracket"), {{"thickness", 7}});
  fs::path dir = fs::temp_directory_path() / "structura_export";
  fs::remove_all(dir);
  auto files = export_bundle(p.shape, dir.string());
  for (auto& f : files) CHECK(fs::file_size(f) > 1000);
  TopoDS_Shape back = read_step((dir / "part.step").string());
  CHECK(solids_of(back).size() == 1);
  CHECK(is_valid(back));
  CHECK(volume(back) == doctest::Approx(volume(p.shape)).epsilon(1e-6));
  // binary STL: 80-byte header + count + 50 bytes per triangle
  std::ifstream s(dir / "part.stl", std::ios::binary);
  char header[80];
  uint32_t n = 0;
  s.read(header, 80);
  s.read(reinterpret_cast<char*>(&n), 4);
  CHECK(n > 100);
  CHECK(fs::file_size(dir / "part.stl") == 84 + 50ull * n);
}

TEST_CASE("H2-T1: VTU holds every node and tet with displacement and von Mises") {
  DesignGraph g = DesignGraph::from_json(ojson::parse(R"({"name": "beam", "features": [{"id": "b", "type": "plate",
      "params": {"x": 0, "y": 0, "z": 0, "length": 50, "width": 10, "thickness": 10}}],
      "faces": {"root": {"kind": "plane", "normal": "-x", "at": 0}, "tip": {"kind": "plane", "normal": "x", "at": 50}}})"));
  CompiledPart p = compile_graph(g);
  fs::path dir = fs::temp_directory_path() / "structura_export_vtu";
  fs::remove_all(dir);
  MeshOptions o;
  o.size = 5;
  Mesh m = mesh_shape(p.shape, p.named_faces(), (dir / "mesh").string(), o);
  LoadCase lc = LoadCase::from_json(nlohmann::json::parse(R"({"supports": [{"group": "root"}], "loads": [{"group": "tip", "force": [0, 0, -10]}]})"));
  Result r = solve_static(m, load_material("S235"), lc, (dir / "run").string(), {2, 120});
  write_vtu(m, r, (dir / "result.vtu").string());
  std::ifstream in(dir / "result.vtu");
  std::stringstream ss;
  ss << in.rdbuf();
  std::string x = ss.str();
  CHECK(x.find("NumberOfPoints=\"" + std::to_string(m.nodes.size()) + "\"") != std::string::npos);
  CHECK(x.find("NumberOfCells=\"" + std::to_string(m.tets.size()) + "\"") != std::string::npos);
  CHECK(x.find("Name=\"displacement_mm\"") != std::string::npos);
  CHECK(x.find("Name=\"von_mises_MPa\"") != std::string::npos);
}
