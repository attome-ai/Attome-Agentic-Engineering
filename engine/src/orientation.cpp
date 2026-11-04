// H1: choosing the print direction. Printed parts are weakest across their layers, so the direction matters twice: it sets
// the support material, and it sets how much of the stress pulls layers apart. One solve gives the stress tensor; every
// candidate direction is then scored from it: tension normal to the layers (d . S . d) and the support volume.
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/manufacture.hpp"
#include "structura/spec.hpp"

namespace fs = std::filesystem;

namespace structura {

nlohmann::json orientation_report(const DesignSpec& spec, const Values& free_params, const std::string& workdir, int threads) {
  if (spec.load_cases.empty()) throw std::runtime_error("the print direction is chosen from stress, so the spec needs a structural load case");
  Values v = spec.fixed_params;
  for (auto& [k, x] : free_params) v[k] = x;
  CompiledPart part = Design(load_family(spec.family), v).build();
  fs::remove_all(workdir);
  MeshOptions mo;
  mo.size = spec.mesh_size;
  mo.threads = threads;
  Mesh m = mesh_shape(part.shape, part.named_faces(), (fs::path(workdir) / "mesh").string(), mo);
  const SpecLoadCase& slc = spec.load_cases.front();
  LoadCase lc = slc.lc;
  for (auto& l : lc.loads) {
    for (auto& f : l.force) f *= slc.factor_range[1];
    l.pressure *= slc.factor_range[1];
  }
  SolveOptions so;
  so.threads = threads;
  so.exclude_near_bc_mm = spec.stress_exclude_near_bc_mm;
  Result r = solve_static(m, spec.effective_material(), lc, (fs::path(workdir) / "solve").string(), so);
  const Material eff = spec.effective_material();
  double required_sf = 1;
  for (auto& q : spec.requirements)
    if (q.quantity == "safety_factor") required_sf = q.value;
  const double allowed = eff.yield / required_sf;
  nlohmann::json processes = load_processes();
  const double max_deg = processes.at("fdm").at("max_overhang_deg").at("value").get<double>();
  static const std::array<std::array<double, 3>, 6> kDirs = {{{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}}};
  nlohmann::json rows = nlohmann::json::array();
  int best = -1, least_tension = 0;
  double best_support = std::numeric_limits<double>::max(), lowest = std::numeric_limits<double>::max();
  for (size_t i = 0; i < kDirs.size(); ++i) {
    auto& d = kDirs[i];
    double peak = 0;
    for (auto& [n, s] : r.stress) {
      auto vm = r.von_mises.find(n);
      if (vm != r.von_mises.end() && vm->second > r.max_von_mises_MPa) continue;  // inside the excluded region next to a load or support
      const double nn = d[0] * d[0] * s[0] + d[1] * d[1] * s[1] + d[2] * d[2] * s[2] + 2 * (d[0] * d[1] * s[3] + d[1] * d[2] * s[4] + d[2] * d[0] * s[5]);
      peak = std::max(peak, nn);
    }
    const double support = support_volume(part.shape, d, max_deg);
    const bool layers_ok = peak <= allowed;
    rows.push_back({{"direction", d}, {"tension_across_layers_MPa", peak}, {"allowed_MPa", allowed}, {"layers_ok", layers_ok}, {"support_volume_mm3", support}});
    if (peak < lowest) {
      lowest = peak;
      least_tension = static_cast<int>(i);
    }
    if (layers_ok && support < best_support) {
      best_support = support;
      best = static_cast<int>(i);
    }
  }
  const int pick = best >= 0 ? best : least_tension;
  return {{"directions", rows}, {"recommended", rows[pick]},
          {"reason", best >= 0 ? "the least support among the directions where tension across the layers stays within the yield of the printed material divided by the required safety factor"
                               : "no direction keeps tension across the layers within the allowed stress: this is the least-tension direction, and the design needs review"},
          {"allowed_MPa", allowed}, {"load_case", lc.name},
          {"note", "Mesh-level stress at the spec's mesh size, away from load and support faces; the support volume is an upper bound. The stress analysis itself uses the same conservative knock-down in every direction."}};
}

}  // namespace structura
