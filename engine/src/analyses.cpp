// Modal, buckling and steady heat-transfer analyses with CalculiX (subprocess).
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include "fea_detail.hpp"
#include "structura/fea.hpp"
#include "structura/process.hpp"

namespace fs = std::filesystem;

namespace structura {
using namespace detail;

namespace {

struct Run {
  std::string version;
  double seconds = 0;
};

Run run_ccx(const std::string& deck, const std::string& workdir, const SolveOptions& o) {
  fs::create_directories(workdir);
  std::ofstream(fs::path(workdir) / "job.inp") << deck;
  std::string t = std::to_string(o.threads);
  ProcessResult pr = run_process({find_exe("ccx"), "-i", "job"}, workdir, o.timeout_s,
                                 {{"OMP_NUM_THREADS", t}, {"CCX_NPROC_EQUATION_SOLVER", t}, {"CCX_NPROC_STIFFNESS", t}});
  if (pr.timed_out) throw std::runtime_error("CalculiX timed out after " + num(o.timeout_s) + " s");
  if (pr.exit_code != 0 || pr.output.find("*ERROR") != std::string::npos)
    throw std::runtime_error("CalculiX failed (exit " + std::to_string(pr.exit_code) + "): " +
                             pr.output.substr(pr.output.size() > 2000 ? pr.output.size() - 2000 : 0));
  Run r;
  r.seconds = pr.seconds;
  std::smatch m;
  if (std::regex_search(pr.output, m, std::regex("Version ([0-9.]+)"))) r.version = m[1];
  return r;
}

void structural_preamble(std::ostream& d, const Mesh& m, const Material& mat, const std::vector<Support>& supports) {
  write_nodes_elements(d, m, "C3D10");
  for (auto& s : supports) write_nset(d, m, s.group);
  d << "*MATERIAL,NAME=MAT\n*ELASTIC\n" << num(mat.E) << "," << num(mat.nu) << "\n*DENSITY\n" << num(mat.density) << "\n";
  d << "*SOLID SECTION,ELSET=EALL,MATERIAL=MAT\n*BOUNDARY\n";
  for (auto& s : supports)
    for (char c : s.dofs) d << set_name(s.group) << "," << c << "," << c << "\n";
}

// Rows of numbers that follow a header line containing `marker` in a .dat file.
std::vector<std::vector<double>> dat_table(const std::string& dat, const std::string& marker) {
  std::vector<std::vector<double>> rows;
  size_t pos = dat.rfind(marker);
  if (pos == std::string::npos) return rows;
  std::istringstream in(dat.substr(pos));
  std::string line;
  std::getline(in, line);
  bool started = false;
  static const std::regex row("^\\s*(\\d+)((\\s+[-+0-9.Ee]+)+)\\s*$");
  while (std::getline(in, line)) {
    std::smatch mm;
    if (std::regex_match(line, mm, row)) {
      started = true;
      std::istringstream ss(line);
      std::vector<double> v;
      double x;
      while (ss >> x) v.push_back(x);
      rows.push_back(v);
    } else if (started && line.find_first_not_of(" \t\r") == std::string::npos) {
      break;
    }
  }
  return rows;
}

}  // namespace

ModalResult solve_modal(const Mesh& m, const Material& mat, const std::vector<Support>& supports, int modes,
                        const std::string& workdir, const SolveOptions& o) {
  std::ostringstream d;
  structural_preamble(d, m, mat, supports);
  d << "*STEP\n*FREQUENCY\n" << modes << "\n*NODE FILE\nU\n*END STEP\n";
  Run run = run_ccx(d.str(), workdir, o);
  std::string dat = read_file(fs::path(workdir) / "job.dat");
  ModalResult r;
  r.solver_version = run.version;
  r.runtime_s = run.seconds;
  for (auto& row : dat_table(dat, "E I G E N V A L U E   O U T P U T"))
    if (row.size() >= 4) r.frequencies_hz.push_back(row[3]);  // mode, eigenvalue, rad/time, cycles/time
  if (r.frequencies_hz.empty()) throw std::runtime_error("no eigenvalue output in job.dat");
  return r;
}

BuckleResult solve_buckle(const Mesh& m, const Material& mat, const LoadCase& lc, int modes, const std::string& workdir,
                          const SolveOptions& o) {
  std::ostringstream d;
  structural_preamble(d, m, mat, lc.supports);
  d << "*STEP\n*BUCKLE\n" << modes << "\n";
  std::map<int, std::array<double, 3>> forces;
  for (auto& l : lc.loads) {
    if (l.kind != "force") throw std::runtime_error("buckling supports force loads only");
    for (auto& [n, f] : distribute_force(m, l.group, l.force))
      for (int c = 0; c < 3; ++c) forces[n][c] += f[c];
  }
  d << "*CLOAD\n";
  for (auto& [n, f] : forces)
    for (int c = 0; c < 3; ++c)
      if (std::fabs(f[c]) > 0) d << n << "," << (c + 1) << "," << num(f[c]) << "\n";
  d << "*NODE FILE\nU\n*END STEP\n";
  Run run = run_ccx(d.str(), workdir, o);
  std::string dat = read_file(fs::path(workdir) / "job.dat");
  BuckleResult r;
  r.solver_version = run.version;
  r.runtime_s = run.seconds;
  for (auto& row : dat_table(dat, "B U C K L I N G   F A C T O R   O U T P U T"))
    if (row.size() >= 2) r.factors.push_back(row[1]);
  if (r.factors.empty()) throw std::runtime_error("no buckling factor output in job.dat");
  return r;
}

AirProperties air_properties(double T_C) {
  // Incropera, DeWitt, Bergman, Lavine, Fundamentals of Heat and Mass Transfer, Table A.4 (air at 1 atm)
  static const double T[] = {250, 300, 350, 400};
  static const double K[] = {22.3e-3, 26.3e-3, 30.0e-3, 33.8e-3};
  static const double NU[] = {11.44e-6, 15.89e-6, 20.92e-6, 26.41e-6};
  static const double PR[] = {0.720, 0.707, 0.700, 0.690};
  double tk = T_C + 273.15;
  if (tk < 250 || tk > 400) throw std::runtime_error("air properties: film temperature " + std::to_string(T_C) + " degC is outside 250-400 K");
  int i = tk >= 350 ? 2 : tk >= 300 ? 1 : 0;
  double w = (tk - T[i]) / (T[i + 1] - T[i]);
  return {K[i] + w * (K[i + 1] - K[i]), NU[i] + w * (NU[i + 1] - NU[i]), PR[i] + w * (PR[i + 1] - PR[i])};
}

double plate_fin_channel_h(double velocity_m_s, double spacing_mm, double fin_thickness_mm, double length_mm, double film_T_C,
                           nlohmann::json* info) {
  if (spacing_mm <= 0 || length_mm <= 0 || fin_thickness_mm < 0) throw std::runtime_error("plate_fin_channel: spacing and length must be positive");
  AirProperties a = air_properties(film_T_C);
  double b = spacing_mm * 1e-3, L = length_mm * 1e-3;
  double v_ch = velocity_m_s * (spacing_mm + fin_thickness_mm) / spacing_mm;  // continuity: the fins block part of the inlet
  double re_b = v_ch * b / a.nu;
  double re_star = re_b * b / L;  // channel Reynolds number scaled by the gap-to-length ratio
  // Teertstra et al. (1999): fully developed limit Re* Pr / 2 blended with the developing (flat plate) limit
  double fd = re_star * a.Pr / 2.0;
  double dev = 0.664 * std::sqrt(re_star) * std::cbrt(a.Pr) * std::sqrt(1.0 + 3.65 / std::sqrt(re_star));
  double nu_b = std::pow(std::pow(fd, -3.0) + std::pow(dev, -3.0), -1.0 / 3.0);
  double h_si = nu_b * a.k / b;  // W/(m^2 K)
  if (info)
    *info = {{"model", "plate_fin_channel (Teertstra et al. 1999)"}, {"velocity_m_s", velocity_m_s}, {"channel_velocity_m_s", v_ch},
             {"spacing_mm", spacing_mm}, {"fin_thickness_mm", fin_thickness_mm}, {"length_mm", length_mm}, {"film_temperature_C", film_T_C},
             {"Re_b", re_b}, {"Re_star", re_star}, {"Nu_b", nu_b}, {"h_W_m2K", h_si},
             {"laminar", re_b < 2300}, {"air", {{"k", a.k}, {"nu", a.nu}, {"Pr", a.Pr}, {"source", "Incropera Table A.4"}}}};
  return h_si * 1e-6;  // W/(mm^2 K)
}

bool ThermalCase::has_correlation() const {
  for (auto& f : films)
    if (f.correlation) return true;
  return false;
}

ThermalCase ThermalCase::resolved(const Values& params, nlohmann::json* info) const {
  ThermalCase out = *this;
  nlohmann::json all = nlohmann::json::object();
  for (auto& f : out.films) {
    if (!f.correlation) continue;
    const FilmCorrelation& c = *f.correlation;
    nlohmann::json one;
    f.h = plate_fin_channel_h(c.velocity_m_s, evaluate(c.spacing_mm, params), evaluate(c.fin_thickness_mm, params),
                              evaluate(c.length_mm, params), c.film_temperature, &one);
    all[f.group] = one;
  }
  if (info) *info = all;
  return out;
}

ThermalCase ThermalCase::from_json(const nlohmann::json& j) {
  ThermalCase tc;
  tc.name = j.value("name", std::string("thermal"));
  for (auto& x : j.value("temperatures", nlohmann::json::array())) tc.temperatures.push_back({x.at("group"), x.at("t")});
  for (auto& x : j.value("films", nlohmann::json::array())) {
    Film f{x.at("group"), x.value("h", 0.0), x.value("sink", 0.0), nullptr};
    if (x.contains("correlation")) {
      if (x.contains("h")) throw std::runtime_error("film '" + f.group + "': give either h or a correlation, not both");
      const auto& c = x.at("correlation");
      auto fc = std::make_shared<FilmCorrelation>();
      fc->model = c.value("model", std::string("plate_fin_channel"));
      if (fc->model != "plate_fin_channel") throw std::runtime_error("film '" + f.group + "': unknown correlation model '" + fc->model + "'");
      auto str = [&](const char* k) { return c.at(k).is_string() ? c.at(k).get<std::string>() : std::to_string(c.at(k).get<double>()); };
      fc->velocity_m_s = c.at("velocity_m_s");
      fc->spacing_mm = str("spacing_mm");
      fc->fin_thickness_mm = str("fin_thickness_mm");
      fc->length_mm = str("length_mm");
      fc->film_temperature = c.value("film_temperature", f.sink + 20.0);
      if (fc->velocity_m_s <= 0) throw std::runtime_error("film '" + f.group + "': velocity_m_s must be positive");
      f.correlation = fc;
    } else if (!x.contains("h")) {
      throw std::runtime_error("film '" + f.group + "' needs h or a correlation");
    }
    tc.films.push_back(f);
  }
  for (auto& x : j.value("fluxes", nlohmann::json::array())) tc.fluxes.push_back({x.at("group"), x.value("q", 0.0), x.value("power", 0.0)});
  if (tc.temperatures.empty() && tc.films.empty()) throw std::runtime_error("thermal case '" + tc.name + "' needs a fixed temperature or a film");
  return tc;
}

nlohmann::json ThermalResult::summary() const {
  double bal = heat_in_W > 0 ? std::fabs(heat_out_W - heat_in_W) / heat_in_W : -1;
  return {{"t_max", t_max}, {"t_min", t_min}, {"heat_in_W", heat_in_W}, {"heat_out_W", heat_out_W}, {"heat_balance_error", bal},
          {"solver_version", solver_version}, {"runtime_s", runtime_s},
          {"nodes", temperature.size()}, {"units", {{"temperature", "as given in the case (K or degC)"}, {"power", "W"}}}};
}

ThermalResult solve_thermal(const Mesh& m, const Material& mat, const ThermalCase& tc, const std::string& workdir,
                            const SolveOptions& o) {
  if (mat.k <= 0) throw std::runtime_error("material " + mat.id + " has no conductivity");
  std::ostringstream d;
  write_nodes_elements(d, m, "DC3D10");
  for (auto& t : tc.temperatures) write_nset(d, m, t.group);
  d << "*MATERIAL,NAME=MAT\n*CONDUCTIVITY\n" << num(mat.k) << "\n*DENSITY\n" << num(mat.density) << "\n";
  d << "*SOLID SECTION,ELSET=EALL,MATERIAL=MAT\n";
  if (!tc.temperatures.empty()) {
    d << "*BOUNDARY\n";
    for (auto& t : tc.temperatures) d << set_name(t.group) << ",11,11," << num(t.t) << "\n";
  }
  d << "*STEP\n*HEAT TRANSFER,STEADY STATE\n1.,1.\n";
  for (auto& f : tc.films) {
    d << "*FILM\n";
    for (auto& [e, face] : element_faces(m, f.group)) d << e << ",F" << face << "," << num(f.sink) << "," << num(f.h) << "\n";
  }
  for (auto& fx : tc.fluxes) {
    double q = fx.q;
    if (fx.power > 0) {
      double a = 0;
      for (auto& tri : m.surfaces.at(fx.group)) a += tri_area(m.nodes, tri);
      q = fx.power / a;
    }
    d << "*DFLUX\n";
    for (auto& [e, face] : element_faces(m, fx.group)) d << e << ",S" << face << "," << num(q) << "\n";
  }
  d << "*NODE FILE\nNT\n*END STEP\n";
  Run run = run_ccx(d.str(), workdir, o);
  ThermalResult r;
  r.solver_version = run.version;
  r.runtime_s = run.seconds;
  r.t_max = -1e300;
  r.t_min = 1e300;
  for (auto& [n, v] : read_frd_block((fs::path(workdir) / "job.frd").string(), "NDTEMP")) {
    r.temperature[n] = v[0];
    r.t_max = std::max(r.t_max, v[0]);
    r.t_min = std::min(r.t_min, v[0]);
  }
  // energy balance: heat in through fluxes, heat out through films (quadratic triangles: corner weights 0, mid-sides 1/3)
  for (auto& fx : tc.fluxes) {
    double a = 0;
    for (auto& tri : m.surfaces.at(fx.group)) a += tri_area(m.nodes, tri);
    r.heat_in_W += fx.power > 0 ? fx.power : fx.q * a;
  }
  for (auto& f : tc.films)
    for (auto& tri : m.surfaces.at(f.group)) {
      double tavg = tri.size() >= 7 ? (r.temperature.at(tri[4]) + r.temperature.at(tri[5]) + r.temperature.at(tri[6])) / 3
                                    : (r.temperature.at(tri[1]) + r.temperature.at(tri[2]) + r.temperature.at(tri[3])) / 3;
      r.heat_out_W += f.h * (tavg - f.sink) * tri_area(m.nodes, tri);
    }
  return r;
}

}  // namespace structura
