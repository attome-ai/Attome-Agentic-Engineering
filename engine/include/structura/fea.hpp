// Structural analysis with CalculiX: materials, load cases on named groups, the input deck, the run, and the
// solver-independent result schema. Units: mm, N, MPa, tonne, s.
#pragma once
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "structura/expr.hpp"
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
  std::string kind = "fixed";  // fixed: every node of the face is held; bolted: each connected face (one bolt bore) is held
                               // through a distributing coupling to a reference node, so the bore can deform (no false peak)
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

// C3: checked before any solve; a setup that fails is refused with the reason instead of solved.
struct SetupReport {
  bool ok = false;
  int restrained = 0;  // rigid-body motions the supports stop, out of 6
  std::vector<std::string> problems;
  nlohmann::json to_json() const;
};
SetupReport check_setup(const Mesh& m, const LoadCase& lc);

// C1: bolted supports. connected_faces splits a face group into its separate faces (one per bolt bore), as lists of
// triangles. write_bolted_support ties each bore's mean translation to a new reference node (ids from `next_id`) and
// returns those nodes for the caller to hold in *BOUNDARY; `held` are nodes other supports fix.
std::set<int> fixed_support_nodes(const Mesh& m, const std::vector<Support>& supports);
std::vector<std::vector<std::vector<int>>> connected_faces(const Mesh& m, const std::string& group);
std::vector<int> write_bolted_support(std::ostream& d, const Mesh& m, const Support& s, int& next_id, const std::set<int>& held);

// ---------------------------------------------------------------- results (C1-T1)
struct Result {
  double mass_t = 0;               // tonne
  double volume_mm3 = 0;
  double max_von_mises_MPa = 0;      // peak over the evaluation region (see SolveOptions::exclude_near_bc_mm)
  double max_von_mises_raw_MPa = 0;  // raw peak over every node
  double excluded_mm = 0;
  std::array<double, 3> peak_at{0, 0, 0};  // mm, where max_von_mises_MPa occurs
  double max_disp_mm = 0;
  std::array<double, 3> applied_N{0, 0, 0};
  std::array<double, 3> reaction_N{0, 0, 0};
  double equilibrium_error = 0;    // |applied + reaction| / |applied|
  double strain_energy = -1;       // N mm, total internal energy reported by CalculiX (-1 = not available)
  double external_work = -1;       // N mm, half the work of the applied nodal forces (-1 = not available)
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
  // Saint-Venant: stress within this distance (mm) of a load or support face is a modelling artefact of how the
  // load/support is applied; max_von_mises_MPa excludes it, max_von_mises_raw_MPa keeps the raw peak. 0 = no exclusion.
  double exclude_near_bc_mm = 0;
  std::string solver;  // "" = automatic (SPOOLES, or ITERATIVE CHOLESKY above 600,000 unknowns); or a CalculiX solver name
};
std::string write_deck(const Mesh& m, const Material& mat, const LoadCase& lc, std::map<int, std::array<double, 3>>* applied = nullptr);
Result solve_static(const Mesh& m, const Material& mat, const LoadCase& lc, const std::string& workdir, const SolveOptions& o = {});

// ---------------------------------------------------------------- modal and buckling (C1, before 3 Feb)
// A carried mass (e.g. a crate on a shelf) lumped evenly onto the nodes of a named face group; tonne.
struct PointMass {
  std::string group;
  double mass_t = 0;
};

struct ModalResult {
  std::vector<double> frequencies_hz;
  std::vector<std::map<int, std::array<double, 3>>> shapes;  // mode shape per frequency (node -> displacement, arbitrary scale)
  std::string solver_version;
  double runtime_s = 0;
};
ModalResult solve_modal(const Mesh& m, const Material& mat, const std::vector<Support>& supports, int modes,
                        const std::string& workdir, const SolveOptions& o = {}, const std::vector<PointMass>& masses = {});

struct BuckleResult {
  std::vector<double> factors;  // load multipliers at which the structure buckles
  std::string solver_version;
  double runtime_s = 0;
};
BuckleResult solve_buckle(const Mesh& m, const Material& mat, const LoadCase& lc, int modes, const std::string& workdir,
                          const SolveOptions& o = {});

// ---------------------------------------------------------------- structural dynamics over time (real-physics animation)
// The load case is applied suddenly at t = 0 and held; implicit direct time integration with Rayleigh damping
// at the stated ratio between two frequencies (alpha and beta fitted at f1_hz and f2_hz).
struct DynamicResult {
  std::vector<double> times;                                   // s, one per stored frame
  std::vector<std::map<int, std::array<double, 3>>> disp;      // node -> displacement (mm) per frame
  std::vector<std::map<int, double>> von_mises;                // node -> MPa per frame
  double dt = 0, duration = 0, damping_ratio = 0, alpha = 0, beta = 0;
  std::string solver_version;
  double runtime_s = 0;
};
DynamicResult solve_dynamic(const Mesh& m, const Material& mat, const LoadCase& lc, double duration_s, double dt_s, double damping_ratio,
                            double f1_hz, double f2_hz, int store_every, const std::string& workdir, const SolveOptions& o = {},
                            const std::vector<PointMass>& masses = {});

// ---------------------------------------------------------------- steady heat transfer (C2 base)
// Thermal inputs use watts consistently: k in W/(mm K), film h in W/(mm^2 K), flux in W/mm^2, power in W.
struct FixedTemperature {
  std::string group;
  double t = 0;
};
// Convection coefficient from a correlation instead of a fixed number (C2). Geometry inputs are expressions over the
// design parameters (mm), resolved per design by resolve_films(). Model "plate_fin_channel": laminar forced flow between
// parallel fins, Teertstra, Yovanovich, Culham and Lemczyk (1999) composite model, with air properties at the film
// temperature (Incropera Table A.4).
struct FilmCorrelation {
  std::string model = "plate_fin_channel";
  double velocity_m_s = 0;          // approach velocity of the air
  std::string spacing_mm, fin_thickness_mm, length_mm;  // channel gap between fins, fin thickness, flow length
  double film_temperature = -1e9;   // degC for the air properties; default sink + 20
  // fan operating point instead of a fixed velocity (when fan_max_pressure_Pa > 0): the fan's linear pressure-flow
  // curve dp = Pmax (1 - Q / Qmax) meets the fin channels' pressure drop; assumes the fan is ducted onto the fins
  double fan_max_pressure_Pa = 0, fan_max_flow_m3_s = 0;
  std::string fin_height_mm, channels;  // expressions: channel height and number of channels
};
struct Film {
  std::string group;
  double h = 0, sink = 0;           // h in W/(mm^2 K)
  std::shared_ptr<FilmCorrelation> correlation;  // when set, h is computed per design
};
struct AirProperties {
  double k, nu, Pr;  // W/(m K), m^2/s, 1
};
AirProperties air_properties(double T_C);  // dry air at 1 atm, linear in Incropera Table A.4 between 250 and 400 K
// h in W/(mm^2 K) for one design; `info` (optional) receives Re, Nu, h in W/(m^2 K) and the inputs.
// Channel velocity (m/s) where a fan with a linear curve meets the channels' pressure drop: laminar parallel-plate friction
// 12 mu L V / b^2 plus entry and exit losses 1.5 rho V^2 / 2. `info` receives the operating point.
double fan_channel_velocity(double fan_max_pressure_Pa, double fan_max_flow_m3_s, double channels, double spacing_mm, double fin_height_mm,
                            double length_mm, double film_T_C, nlohmann::json* info = nullptr);
// h from a known channel velocity (the core of the plate-fin channel model)
double channel_h(double channel_velocity_m_s, double spacing_mm, double length_mm, double film_T_C, nlohmann::json* info = nullptr);
double plate_fin_channel_h(double velocity_m_s, double spacing_mm, double fin_thickness_mm, double length_mm, double film_T_C,
                           nlohmann::json* info = nullptr);
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
  // Copy with every correlation film's h computed for these design parameters; `info` receives one entry per such film.
  ThermalCase resolved(const Values& params, nlohmann::json* info = nullptr) const;
  bool has_correlation() const;
};
struct ThermalResult {
  std::map<int, double> temperature;  // node -> temperature
  double t_max = 0, t_min = 0;
  double heat_in_W = 0, heat_out_W = 0;  // power applied, and convected out through the films (energy balance check)
  std::string solver_version;
  double runtime_s = 0;
  nlohmann::json summary() const;
};
ThermalResult solve_thermal(const Mesh& m, const Material& mat, const ThermalCase& tc, const std::string& workdir,
                            const SolveOptions& o = {});

// .frd reading: last block of a result type ("DISP", "STRESS") -> node -> values
std::map<int, std::vector<double>> read_frd_block(const std::string& path, const std::string& name);
// every block with that name, in file order (e.g. one DISP block per vibration mode)
std::vector<std::map<int, std::vector<double>>> read_frd_blocks(const std::string& path, const std::string& name);
double von_mises(const std::vector<double>& s);  // sxx syy szz sxy syz szx

}  // namespace structura
