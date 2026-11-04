// Structural analysis with CalculiX: materials, load cases on named groups, the input deck, the run, and the
// solver-independent result schema. Units: mm, N, MPa, tonne, s.
#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "structura/mesh.hpp"

namespace structura {

// ---------------------------------------------------------------- materials (C1-T2, A2)
struct Material {
  std::string id, name, source;
  double E = 0, nu = 0, density = 0, yield = 0, k = 0;
  bool verify = true;
};
Material load_material(const std::string& id);  // from <repo>/data/materials.json

// ---------------------------------------------------------------- loads and supports (C3-T1)
struct Support {
  std::string group;      // named face group
  std::string dofs = "123";  // fixed translations
};
struct Load {
  std::string group;
  std::string kind = "force";    // force: total force vector spread over the face; pressure: MPa normal to the face
  std::array<double, 3> force{0, 0, 0};  // N
  double pressure = 0;                    // MPa, positive pushes into the surface
};
struct LoadCase {
  std::string name = "service";
  std::vector<Support> supports;
  std::vector<Load> loads;
  static LoadCase from_json(const nlohmann::json& j);
};

// Consistent nodal forces for a total force spread uniformly (by area) over a group's quadratic triangles:
// corner nodes take 0 and mid-side nodes A/3 of each triangle's share. The sum equals the requested force.
std::map<int, std::array<double, 3>> distribute_force(const Mesh& m, const std::string& group, const std::array<double, 3>& total);

// ---------------------------------------------------------------- results (C1-T1)
struct Result {
  double mass_t = 0;               // tonne
  double volume_mm3 = 0;
  double max_von_mises_MPa = 0;
  double max_disp_mm = 0;
  std::array<double, 3> applied_N{0, 0, 0};
  std::array<double, 3> reaction_N{0, 0, 0};
  double equilibrium_error = 0;    // |applied + reaction| / |applied|
  std::map<int, std::array<double, 3>> disp;  // node -> (ux, uy, uz) mm
  std::map<int, double> von_mises;            // node -> MPa (nodal, extrapolated)
  std::string solver = "CalculiX", solver_version, load_case;
  nlohmann::json mesh_report;
  double runtime_s = 0;
  nlohmann::json summary() const;   // scalars with units
  void save(const std::string& dir) const;           // result.json + fields.bin
  static Result load(const std::string& dir);
};

// ---------------------------------------------------------------- CalculiX (C1-T2)
struct SolveOptions {
  int threads = 1;
  double timeout_s = 600;
};
std::string write_deck(const Mesh& m, const Material& mat, const LoadCase& lc, std::map<int, std::array<double, 3>>* applied = nullptr);
Result solve_static(const Mesh& m, const Material& mat, const LoadCase& lc, const std::string& workdir, const SolveOptions& o = {});

// .frd reading: last block of a result type ("DISP", "STRESS") -> node -> values
std::map<int, std::vector<double>> read_frd_block(const std::string& path, const std::string& name);
double von_mises(const std::vector<double>& s);  // sxx syy szz sxy syz szx

}  // namespace structura
