// C1-T1, C3-T1, C1-T2: result schema, loads on named faces, and the CalculiX static solve.
#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/export.hpp"
#include "structura/fea.hpp"

using namespace structura;
namespace fs = std::filesystem;

namespace {

// 100 x 10 x 10 mm beam along x with named end faces.
DesignGraph beam_graph() {
  return DesignGraph::from_json(ojson::parse(R"({
    "name": "beam", "features": [{"id": "b", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": 100, "width": 10, "thickness": 10}}],
    "faces": {"root": {"kind": "plane", "normal": "-x", "at": 0}, "tip": {"kind": "plane", "normal": "x", "at": 100},
              "top": {"kind": "plane", "normal": "z", "at": 10}}})"));
}

Mesh beam_mesh(const std::string& tag, double size) {
  CompiledPart p = compile_graph(beam_graph());
  fs::path dir = fs::temp_directory_path() / ("structura_" + tag);
  fs::remove_all(dir);
  MeshOptions o;
  o.size = size;
  return mesh_shape(p.shape, p.named_faces(), dir.string(), o);
}

}  // namespace

TEST_CASE("C1-T1: results round-trip with units and fields") {
  Result r;
  r.mass_t = 1.5e-4;
  r.volume_mm3 = 1000;
  r.max_von_mises_MPa = 12.5;
  r.max_disp_mm = 0.2;
  r.applied_N = {0, 0, -100};
  r.reaction_N = {0, 0, 100};
  r.solver_version = "2.23";
  r.load_case = "service";
  r.disp[7] = {0.1, 0.2, -0.3};
  r.von_mises[7] = 9.0;
  fs::path dir = fs::temp_directory_path() / "structura_result_rt";
  fs::remove_all(dir);
  r.save(dir.string());
  Result q = Result::load(dir.string());
  CHECK(q.max_von_mises_MPa == doctest::Approx(12.5));
  CHECK(q.disp.at(7)[2] == doctest::Approx(-0.3));
  CHECK(q.von_mises.at(7) == doctest::Approx(9.0));
  CHECK(r.summary()["units"]["stress"] == "MPa");
}

TEST_CASE("C3-T1: a total force spreads over the named face and sums exactly") {
  CompiledPart p = compile_graph(load_family("bracket"));
  fs::path dir = fs::temp_directory_path() / "structura_bc";
  fs::remove_all(dir);
  MeshOptions o;
  o.size = 4;
  Mesh m = mesh_shape(p.shape, p.named_faces(), dir.string(), o);
  auto f = distribute_force(m, "load_face", {0, 0, -1000});
  double sx = 0, sy = 0, sz = 0;
  for (auto& [n, v] : f) { sx += v[0]; sy += v[1]; sz += v[2]; }
  CHECK(std::fabs(sz + 1000) < 1e-6 * 1000);  // within 0.1% is the requirement; exact by construction
  CHECK(std::fabs(sx) < 1e-9);
  CHECK(std::fabs(sy) < 1e-9);
  std::set<int> face_nodes(m.nsets.at("load_face").begin(), m.nsets.at("load_face").end());
  for (auto& [n, _] : f) CHECK(face_nodes.count(n));
  CHECK_THROWS(distribute_force(m, "no_such_face", {0, 0, 1}));
}

TEST_CASE("C1-T2: cantilever tip deflection within 2% of beam theory") {
  Mesh m = beam_mesh("cantilever", 2.5);
  Material steel = load_material("S235");
  LoadCase lc = LoadCase::from_json(nlohmann::json::parse(R"({"name": "tip", "supports": [{"group": "root"}],
      "loads": [{"group": "tip", "force": [0, 0, -100]}]})"));
  Result r = solve_static(m, steel, lc, (fs::temp_directory_path() / "structura_cantilever_run").string(), {4, 300});
  double I = 10.0 * std::pow(10.0, 3) / 12.0;
  double euler = 100.0 * std::pow(100.0, 3) / (3 * steel.E * I);
  double G = steel.E / (2 * (1 + steel.nu));
  double timoshenko = euler + 100.0 * 100.0 / ((5.0 / 6.0) * G * 100.0);  // shear correction for a solid model
  double tip = 0;
  for (int n : m.nsets.at("tip")) tip += r.disp.at(n)[2];
  tip /= m.nsets.at("tip").size();
  MESSAGE("cantilever tip " << -tip << " mm; Euler-Bernoulli " << euler << " mm; with shear " << timoshenko
          << " mm; error vs shear-corrected " << 100 * (-tip - timoshenko) / timoshenko << " %; DOF " << m.dof()
          << "; solve " << r.runtime_s << " s");
  CHECK(std::fabs(-tip - timoshenko) / timoshenko < 0.02);
  CHECK(r.equilibrium_error < 0.005);
  CHECK(r.solver_version == "2.23");
  CHECK(r.mass_t * 1e9 == doctest::Approx(10000 * 7.85).epsilon(0.001));  // g: 10 cm^3 * 7.85 g/cm^3 -> stored as tonne
}

TEST_CASE("C1: largest principal stress on the skin matches beam theory: tension on top, none below") {
  Mesh m = beam_mesh("cantilever_p1", 2.5);
  LoadCase lc = LoadCase::from_json(nlohmann::json::parse(R"({"name": "tip", "supports": [{"group": "root"}],
      "loads": [{"group": "tip", "force": [0, 0, -100]}]})"));
  Result r = solve_static(m, load_material("S235"), lc, (fs::temp_directory_path() / "structura_cantilever_p1_run").string(), {4, 300});
  auto path = fs::temp_directory_path() / "structura_cantilever_p1_skin.json";
  write_skin(m, r, path.string(), 235);
  auto j = nlohmann::json::parse(std::ifstream(path));
  auto floats = [](const std::string& s) {
    const std::string T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string bytes;
    unsigned buf = 0;
    int bits = 0;
    for (char c : s) {
      if (c == '=') break;
      buf = buf << 6 | static_cast<unsigned>(T.find(c));
      bits += 6;
      if (bits >= 8) {
        bits -= 8;
        bytes += static_cast<char>(buf >> bits & 255);
      }
    }
    std::vector<float> out(bytes.size() / 4);
    std::memcpy(out.data(), bytes.data(), out.size() * 4);
    return out;
  };
  auto pos = floats(j.at("positions").get<std::string>()), p1 = floats(j.at("max_principal").get<std::string>());
  double top = 0, bottom = 0;
  int n_top = 0, n_bottom = 0;
  for (size_t i = 0; i < p1.size(); ++i) {  // the middle of the span, away from both ends: M = 100 N x 50 mm, I = 833.3 mm^4, c = 5 mm, so 30 MPa
    if (std::fabs(pos[3 * i] - 50) > 3 || std::fabs(pos[3 * i + 1] - 5) > 3) continue;
    if (std::fabs(pos[3 * i + 2] - 10) < 1e-6) {
      top += p1[i];
      ++n_top;
    } else if (std::fabs(pos[3 * i + 2]) < 1e-6) {
      bottom += p1[i];
      ++n_bottom;
    }
  }
  REQUIRE(n_top > 0);
  REQUIRE(n_bottom > 0);
  MESSAGE("largest principal at mid-span: top " << top / n_top << " MPa (beam theory 30), bottom " << bottom / n_bottom << " MPa");
  CHECK(top / n_top == doctest::Approx(30).epsilon(0.05));
  CHECK(std::fabs(bottom / n_bottom) < 1.5);
}

TEST_CASE("C1-T2: bracket under 1000 N balances its reactions") {
  CompiledPart p = compile_graph(load_family("bracket"));
  fs::path dir = fs::temp_directory_path() / "structura_bracket_solve";
  fs::remove_all(dir);
  MeshOptions o;
  o.size = 4;
  Mesh m = mesh_shape(p.shape, p.named_faces(), (dir / "mesh").string(), o);
  LoadCase lc = LoadCase::from_json(nlohmann::json::parse(R"({"supports": [{"group": "mount_holes"}],
      "loads": [{"group": "load_face", "force": [0, 0, -1000]}]})"));
  Result r = solve_static(m, load_material("PETG"), lc, (dir / "run").string(), {4, 300});
  CHECK(r.equilibrium_error < 0.005);
  CHECK(r.applied_N[2] == doctest::Approx(-1000).epsilon(1e-6));
  CHECK(r.max_von_mises_MPa > 1);
  CHECK(r.max_disp_mm > 0);
}

TEST_CASE("C1-T2: pressure resultant is computed with outward normals") {
  Mesh m = beam_mesh("pressure", 4);
  LoadCase lc = LoadCase::from_json(nlohmann::json::parse(R"({"supports": [{"group": "root"}],
      "loads": [{"group": "top", "kind": "pressure", "pressure": 0.1}]})"));
  Result r = solve_static(m, load_material("S235"), lc, (fs::temp_directory_path() / "structura_pressure_run").string(), {4, 300});
  CHECK(r.applied_N[2] == doctest::Approx(-0.1 * 1000).epsilon(0.01));  // pushes down on the top face
  CHECK(r.equilibrium_error < 0.01);
}
