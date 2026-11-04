// F2-T4: CalculiX and the in-house solver agree on the bracket (same mesh, loads and supports).
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/fea.hpp"
#include "structura/fem.hpp"

using namespace structura;
namespace fs = std::filesystem;

TEST_CASE("F2-T4: CalculiX and the in-house solver agree within 3% on the bracket") {
  CompiledPart p = compile_graph(load_family("bracket"));
  fs::path dir = fs::temp_directory_path() / "structura_crosscheck";
  fs::remove_all(dir);
  MeshOptions mo;
  mo.size = 4;
  Mesh m = mesh_shape(p.shape, p.named_faces(), (dir / "mesh").string(), mo);
  LoadCase lc = LoadCase::from_json(nlohmann::json::parse(
      R"({"supports": [{"group": "mount_holes"}, {"group": "wall_face", "dofs": "1"}],
          "loads": [{"group": "load_face", "force": [0, 0, -981]}]})"));
  Material petg = load_material("PETG");
  Result ccx = solve_static(m, petg, lc, (dir / "ccx").string(), {4, 600});
  FemResult own = fem_static(m, petg, lc);
  double tip_ccx = 0, tip_own = 0, worst = 0;
  for (int n : m.nsets.at("load_face")) {
    tip_ccx += ccx.disp.at(n)[2];
    tip_own += own.disp.at(n)[2];
  }
  for (auto& [n, u] : ccx.disp) {
    auto& v = own.disp.at(n);
    worst = std::max(worst, std::sqrt(std::pow(u[0] - v[0], 2) + std::pow(u[1] - v[1], 2) + std::pow(u[2] - v[2], 2)));
  }
  double rel_max = std::fabs(own.max_disp_mm - ccx.max_disp_mm) / ccx.max_disp_mm;
  double rel_tip = std::fabs(tip_own - tip_ccx) / std::fabs(tip_ccx);
  MESSAGE("max displacement: CalculiX " << ccx.max_disp_mm << " mm, in-house " << own.max_disp_mm << " mm (" << 100 * rel_max
          << "%); load-face mean uz differs by " << 100 * rel_tip << "%; worst nodal difference " << worst << " mm; DOF "
          << own.dof << "; in-house assemble " << own.assemble_s << " s + solve " << own.solve_s << " s; CalculiX " << ccx.runtime_s << " s");
  CHECK(rel_max < 0.03);
  CHECK(rel_tip < 0.03);
}
