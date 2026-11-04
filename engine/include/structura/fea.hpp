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
  std::map<int, std::array<double, 6>> stress;  // node -> sxx syy szz sxy syz szx (MPa); not saved to fields.bin
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

// ---------------------------------------------------------------- modal and buckling (C1, before 3 Feb)
struct ModalResult {
  std::vector<double> frequencies_hz;
  std::string solver_version;
  double runtime_s = 0;
};
ModalResult solve_modal(const Mesh& m, const Material& mat, const std::vector<Support>& supports, int modes,
                        const std::string& workdir, const SolveOptions& o = {});

struct BuckleResult {
  std::vector<double> factors;  // load multipliers at which the structure buckles
  std::string solver_version;
  double runtime_s = 0;
};
BuckleResult solve_buckle(const Mesh& m, const Material& mat, const LoadCase& lc, int modes, const std::string& workdir,
                          const SolveOptions& o = {});

// ---------------------------------------------------------------- steady heat transfer (C2 base)
// Thermal inputs use watts consistently: k in W/(mm K), film h in W/(mm^2 K), flux in W/mm^2, power in W.
struct FixedTemperature {
  std::string group;
  double t = 0;
};
struct Film {
  std::string group;
  double h = 0, sink = 0;
};
struct Flux {
  std::string group;
  double q = 0;      // W/mm^2 into the part; if power > 0 it is converted using the face area
  double power = 0;  // W
};
struct ThermalCase {
  std::string name = "thermal";
  std::vector<FixedTemperature> temperatures;
  std::vector<Film> films;
  std::vector<Flux> fluxes;
  static ThermalCase from_json(const nlohmann::json& j);
};
struct ThermalResult {
  std::map<int, double> temperature;  // node -> temperature
  double t_max = 0, t_min = 0;
  std::string solver_version;
  double runtime_s = 0;
  nlohmann::json summary() const;
};
ThermalResult solve_thermal(const Mesh& m, const Material& mat, const ThermalCase& tc, const std::string& workdir,
                            const SolveOptions& o = {});

// .frd reading: last block of a result type ("DISP", "STRESS") -> node -> values
std::map<int, std::vector<double>> read_frd_block(const std::string& path, const std::string& name);
double von_mises(const std::vector<double>& s);  // sxx syy szz sxy syz szx

}  // namespace structura
