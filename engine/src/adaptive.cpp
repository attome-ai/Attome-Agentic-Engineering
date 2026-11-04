// C1, C4: adaptive refinement. Solve, estimate the error per element from how much the recovered stress varies across
// it, shrink the mesh only where that variation is large (through a gmsh background size map), and repeat until the peak
// stress in the evaluation region changes by less than the tolerance. Elements next to load and support faces are left
// out: the stress there is an artefact of how the load is applied (decision D6), so refining them would chase a singularity.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>

#include "structura/adaptive.hpp"
#include "structura/compile.hpp"
#include "structura/gate.hpp"
#include "structura/repair.hpp"

namespace fs = std::filesystem;

namespace structura {

namespace {

// Per-element relative error indicator, target element size and the node sizes written to the size map.
size_t write_size_map(const Mesh& m, const Result& r, const LoadCase& lc, double exclude_mm, const AdaptiveOptions& o, const std::string& path) {
  std::set<int> bc;
  for (auto& sp : lc.supports) bc.insert(m.nsets.at(sp.group).begin(), m.nsets.at(sp.group).end());
  for (auto& l : lc.loads) bc.insert(m.nsets.at(l.group).begin(), m.nsets.at(l.group).end());
  const double d = std::max(exclude_mm, 1e-6);
  std::map<std::array<long long, 3>, std::vector<int>> grid;
  auto cell = [&](const std::array<double, 3>& c) {
    return std::array<long long, 3>{(long long)std::floor(c[0] / d), (long long)std::floor(c[1] / d), (long long)std::floor(c[2] / d)};
  };
  for (int n : bc) grid[cell(m.nodes.at(n))].push_back(n);
  auto near_bc = [&](int n) {
    if (exclude_mm <= 0) return false;
    auto& c = m.nodes.at(n);
    auto k = cell(c);
    for (long long i = -1; i <= 1; ++i)
      for (long long j = -1; j <= 1; ++j)
        for (long long q = -1; q <= 1; ++q) {
          auto it = grid.find({k[0] + i, k[1] + j, k[2] + q});
          if (it == grid.end()) continue;
          for (int b : it->second) {
            auto& p = m.nodes.at(b);
            if (std::pow(p[0] - c[0], 2) + std::pow(p[1] - c[1], 2) + std::pow(p[2] - c[2], 2) < d * d) return true;
          }
        }
    return false;
  };
  std::map<int, double> node_h;
  size_t refined = 0;
  const double peak = std::max(r.max_von_mises_MPa, 1e-12);
  for (auto& t : m.tets) {
    double lo = 1e300, hi = -1e300, edges = 0;
    bool excluded = false;
    for (size_t i = 1; i < t.size(); ++i) {
      auto it = r.von_mises.find(t[i]);
      if (it != r.von_mises.end()) {
        lo = std::min(lo, it->second);
        hi = std::max(hi, it->second);
      }
      excluded = excluded || near_bc(t[i]);
    }
    static const int E[6][2] = {{1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};
    for (auto& e : E) {
      auto& a = m.nodes.at(t[e[0]]);
      auto& b = m.nodes.at(t[e[1]]);
      edges += std::sqrt(std::pow(a[0] - b[0], 2) + std::pow(a[1] - b[1], 2) + std::pow(a[2] - b[2], 2));
    }
    const double h = edges / 6, eta = hi > lo ? (hi - lo) / peak : 0;
    double scale = 1.25;  // coarsen a little where the field is smooth
    if (!excluded && eta > o.eta_target) {
      scale = std::max(0.5, o.eta_target / eta);
      ++refined;
    } else if (excluded) {
      scale = 1.0;
    }
    const double target = std::clamp(h * scale, o.min_size, o.start_size);
    for (int i = 1; i <= 4; ++i) {
      auto it = node_h.find(t[i]);
      if (it == node_h.end() || target < it->second) node_h[t[i]] = target;
    }
  }
  std::ofstream f(path);
  if (!f) throw std::runtime_error("cannot write " + path);
  f.precision(7);
  f << "View \"size\" {\n";
  for (auto& t : m.tets) {
    auto& a = m.nodes.at(t[1]);
    auto& b = m.nodes.at(t[2]);
    auto& c = m.nodes.at(t[3]);
    auto& e = m.nodes.at(t[4]);
    f << "SS(" << a[0] << "," << a[1] << "," << a[2] << "," << b[0] << "," << b[1] << "," << b[2] << "," << c[0] << "," << c[1] << "," << c[2] << ","
      << e[0] << "," << e[1] << "," << e[2] << "){" << node_h[t[1]] << "," << node_h[t[2]] << "," << node_h[t[3]] << "," << node_h[t[4]] << "};\n";
  }
  f << "};\n";
  return refined;
}

}  // namespace

AdaptiveResult solve_adaptive(const TopoDS_Shape& shape, const std::map<std::string, std::vector<TopoDS_Face>>& named, const Material& mat,
                              const LoadCase& lc, const std::string& workdir, const AdaptiveOptions& o, const SolveOptions& so) {
  AdaptiveResult out;
  std::string size_map;
  double previous = -1;
  for (int level = 0; level < o.max_levels; ++level) {
    fs::path dir = fs::path(workdir) / ("level" + std::to_string(level));
    MeshOptions mo;
    mo.size = o.start_size * (level ? 1.25 : 1.0);
    mo.threads = so.threads;
    mo.size_map = size_map;
    mo.size_min_factor = o.min_size / mo.size;
    out.mesh = mesh_shape(shape, named, (dir / "mesh").string(), mo);
    SetupReport setup = check_setup(out.mesh, lc);
    if (!setup.ok) throw std::runtime_error("setup: " + setup.problems.front());
    out.result = solve_static(out.mesh, mat, lc, (dir / "solve").string(), so);
    AdaptiveLevel lv;
    lv.level = level;
    lv.elements = out.mesh.tets.size();
    lv.dof = out.mesh.dof();
    lv.peak_MPa = out.result.max_von_mises_MPa;
    lv.max_disp_mm = out.result.max_disp_mm;
    lv.seconds = out.result.runtime_s;
    lv.change = previous > 0 ? std::abs(lv.peak_MPa - previous) / previous : -1;
    size_map = fs::absolute(dir / "size.pos").string();
    lv.elements_refined = write_size_map(out.mesh, out.result, lc, so.exclude_near_bc_mm, o, size_map);
    out.levels.push_back(lv);
    previous = lv.peak_MPa;
    if (lv.change >= 0 && lv.change < o.tolerance) {
      out.converged = true;
      break;
    }
    if (lv.elements_refined == 0) {  // nothing left to refine: the field is resolved at this mesh
      out.converged = level > 0;
      break;
    }
  }
  return out;
}

nlohmann::json AdaptiveResult::to_json() const {
  nlohmann::json lv = nlohmann::json::array();
  for (auto& l : levels)
    lv.push_back({{"level", l.level}, {"elements", l.elements}, {"dof", l.dof}, {"peak_von_mises_MPa", l.peak_MPa}, {"max_disp_mm", l.max_disp_mm},
                  {"elements_refined", l.elements_refined}, {"change_from_previous", l.change}, {"solve_seconds", l.seconds}});
  return {{"converged", converged}, {"levels", lv}, {"peak_von_mises_MPa", levels.empty() ? 0.0 : levels.back().peak_MPa},
          {"max_disp_mm", levels.empty() ? 0.0 : levels.back().max_disp_mm}, {"equilibrium_error", result.equilibrium_error}};
}

}  // namespace structura
