#include "structura/evaluate.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/export.hpp"
#include "structura/fem.hpp"
#include "structura/occ.hpp"
#include "structura/process.hpp"
#include "structura/repair.hpp"

namespace fs = std::filesystem;

namespace structura {
namespace {

using clk = std::chrono::steady_clock;

std::string fmt3(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.3g", v);
  return b;
}

std::string unit_suffix(const std::string& q) {
  const std::string& u = quantities().at(q).unit;
  return u == "1" ? std::string() : " " + u;
}

Values all_params(const DesignSpec& spec, const Values& free) {
  Values v = spec.fixed_params;
  for (auto& [k, b] : spec.free_params) {
    auto it = free.find(k);
    if (it == free.end()) throw SpecError("missing value for free parameter '" + k + "'");
    if (it->second < b[0] - 1e-9 || it->second > b[1] + 1e-9)
      throw SpecError("free parameter '" + k + "' = " + std::to_string(it->second) + " outside the spec's [" + std::to_string(b[0]) +
                      ", " + std::to_string(b[1]) + "]");
    v[k] = it->second;
  }
  for (auto& [k, _] : free)
    if (!spec.free_params.count(k)) throw SpecError("'" + k + "' is not a free parameter of the spec");
  return v;
}

LoadCase scaled(const SpecLoadCase& s) {
  LoadCase lc = s.lc;
  double f = s.factor_range[1];
  for (auto& l : lc.loads) {
    for (double& c : l.force) c *= f;
    l.pressure *= f;
  }
  return lc;
}

}  // namespace

nlohmann::json evaluate_design(const DesignSpec& spec, const Values& free_params, const EvalOptions& o) {
  auto t0 = clk::now();
  nlohmann::json out;
  Values params = all_params(spec, free_params);
  DesignGraph g = load_family(spec.family);
  Design d(g, params);
  out["hash"] = d.hash();
  out["spec_hash"] = spec.hash();
  out["params"] = d.params;
  // a unique folder per evaluation, so parallel runs of the same design never share solver files
  static std::atomic<unsigned> counter{0};
  std::string unique = d.hash().substr(0, 12) + "_" + std::to_string(current_pid()) + "_" + std::to_string(counter++);
  std::string work = o.workdir.empty() ? (fs::temp_directory_path() / ("structura_eval_" + unique)).string() : o.workdir;
  auto finish = [&](const std::string& status) {
    out["status"] = status;
    out["seconds"] = std::chrono::duration<double>(clk::now() - t0).count();
    if (!o.keep_files && !o.save_bundle) {
      std::error_code ec;
      if (o.workdir.empty()) {
        fs::remove_all(work, ec);
      } else {
        fs::remove_all(fs::path(work) / "mesh", ec);
        for (auto& lc : spec.load_cases) fs::remove_all(fs::path(work) / ("solve_" + lc.lc.name), ec);
      }
    }
    return out;
  };
  CompiledPart part;
  try {
    part = d.build();
  } catch (const std::exception& e) {
    out["error"] = e.what();
    return finish("build_fail");
  }
  GateLimits lim;
  lim.design_space = part.design_space();
  lim.print_direction = spec.print_direction;
  RepairGateResult rg = repair_and_gate(part.shape, lim, RepairOptions{});
  out["gate"] = rg.to_json();
  if (!rg.ok) return finish("gate_fail");
  MeshOptions mo;
  mo.size = o.mesh_size > 0 ? o.mesh_size : spec.mesh_size;
  mo.threads = o.threads;
  Mesh m;
  try {
    m = mesh_shape(rg.shape, part.named_faces(), (fs::path(work) / "mesh").string(), mo);
  } catch (const MeshError& e) {
    out["error"] = e.what();
    out["mesh_attempts"] = e.attempts;
    return finish("mesh_fail");
  }
  out["mesh"] = m.report;
  Material eff = spec.effective_material();
  out["material"] = {{"id", eff.id}, {"name", eff.name}, {"E", eff.E}, {"nu", eff.nu}, {"density", eff.density},
                     {"yield_effective", eff.yield}, {"source", eff.source}, {"verify", eff.verify}};
  Metrics mt;
  SolveOptions so;
  so.threads = o.threads;
  nlohmann::json cases = nlohmann::json::object();
  try {
    for (auto& slc : spec.load_cases) {
      LoadCase lc = scaled(slc);
      if (o.screen) {  // in-house solver, in process
        FemResult fr = fem_static(m, eff, lc);
        double mass_g = volume(rg.shape) * eff.density * 1e6;
        std::map<std::string, double> met = {{"mass_g", mass_g}, {"max_von_mises_MPa", fr.max_von_mises_MPa}, {"max_disp_mm", fr.max_disp_mm},
                                             {"safety_factor", fr.max_von_mises_MPa > 0 ? eff.yield / fr.max_von_mises_MPa : 1e9}};
        mt.by_case[lc.name] = met;
        cases[lc.name] = {{"solver", "in-house screening (Eigen)"}, {"max_von_mises_MPa", fr.max_von_mises_MPa}, {"max_disp_mm", fr.max_disp_mm},
                          {"mass_g", mass_g}, {"dof", fr.dof}, {"runtime_s", fr.assemble_s + fr.solve_s}, {"factor", slc.factor_range[1]}};
        continue;
      }
      std::string dir = (fs::path(work) / ("solve_" + lc.name)).string();
      Result r = solve_static(m, eff, lc, dir, so);
      auto met = structural_metrics(r, eff);
      mt.by_case[lc.name] = met;
      cases[lc.name] = r.summary();
      cases[lc.name]["factor"] = slc.factor_range[1];
      if (o.save_bundle) {
        r.save((fs::path(work) / ("result_" + lc.name)).string());
        write_vtu(m, r, (fs::path(work) / ("result_" + lc.name) / "result.vtu").string());
      }
    }
  } catch (const std::exception& e) {
    out["error"] = e.what();
    return finish("solve_fail");
  }
  for (auto& [cname, met] : mt.by_case)
    for (auto& [q, v] : met) {
      bool hib = quantities().at(q).higher_is_better;
      auto it = mt.overall.find(q);
      if (it == mt.overall.end() || (hib ? v < it->second : v > it->second)) {
        mt.overall[q] = v;
        mt.governing_case[q] = cname;
      }
    }
  auto checks = spec.check(mt);
  nlohmann::json jc = nlohmann::json::array();
  bool feasible = true;
  for (auto& c : checks) {
    jc.push_back(c.to_json());
    feasible = feasible && c.passed;
  }
  out["load_cases"] = cases;
  out["screening"] = o.screen;
  out["metrics"] = mt.overall;
  out["governing_case"] = mt.governing_case;
  out["checks"] = jc;
  out["feasible"] = feasible;
  nlohmann::json obj = nlohmann::json::object();
  for (auto& ob : spec.objectives) obj[ob.quantity] = mt.overall.count(ob.quantity) ? nlohmann::json(mt.overall.at(ob.quantity)) : nlohmann::json();
  out["objectives"] = obj;
  if (o.save_bundle) {
    d.save((fs::path(work) / "design").string());
    export_bundle(rg.shape, (fs::path(work) / "design").string());
    std::ofstream(fs::path(work) / "evaluation.json") << out.dump(2);
  }
  return finish("ok");
}

nlohmann::json check_feasibility(const DesignSpec& spec, const EvalOptions& o) {
  DesignGraph g = load_family(spec.family);
  // for each free parameter, which bound gives more material (volume) with the others at mid-range
  Values mid;
  for (auto& [k, b] : spec.free_params) {
    double v = (b[0] + b[1]) / 2;
    if (g.param(k)->integer) v = std::round(v);
    mid[k] = v;
  }
  Values most, least;
  nlohmann::json direction = nlohmann::json::object();
  for (auto& [k, b] : spec.free_params) {
    Values lo = mid, hi = mid;
    lo[k] = b[0];
    hi[k] = b[1];
    Values a = spec.fixed_params, c = spec.fixed_params;
    for (auto& [kk, vv] : lo) a[kk] = vv;
    for (auto& [kk, vv] : hi) c[kk] = vv;
    double vlo = volume(compile_graph(g, a).shape), vhi = volume(compile_graph(g, c).shape);
    most[k] = vhi >= vlo ? b[1] : b[0];
    least[k] = vhi >= vlo ? b[0] : b[1];
    direction[k] = vhi >= vlo ? "more material at high" : "more material at low";
  }
  EvalOptions oo = o;
  oo.workdir = o.workdir.empty() ? "" : (fs::path(o.workdir) / "most").string();
  nlohmann::json strong = evaluate_design(spec, most, oo);
  oo.workdir = o.workdir.empty() ? "" : (fs::path(o.workdir) / "least").string();
  nlohmann::json light = evaluate_design(spec, least, oo);
  nlohmann::json problems = nlohmann::json::array();
  for (auto& r : spec.requirements) {
    bool mass_like = r.quantity == "mass_g" || r.quantity == "cost";
    const nlohmann::json& ev = mass_like ? light : strong;
    if (ev.value("status", "") != "ok") {
      problems.push_back({{"id", r.id}, {"reason", "the " + std::string(mass_like ? "least" : "most") +
                                                     "-material corner could not be evaluated: " + ev.value("status", std::string("?"))}});
      continue;
    }
    for (auto& c : ev["checks"])
      if (c["id"] == r.id && !c["passed"].get<bool>())
        problems.push_back({{"id", r.id}, {"quantity", r.quantity}, {"limit", r.value}, {"op", r.op},
                            {"best_achievable", c["value"]},
                            {"reason", std::string("even the ") + (mass_like ? "lightest" : "strongest") + " design in the spec's bounds gives " +
                                           fmt3(c["value"].get<double>()) + unit_suffix(r.quantity) + " against the requirement " + r.quantity + " " +
                                           r.op + " " + fmt3(r.value) + unit_suffix(r.quantity)}});
  }
  return {{"feasible", problems.empty()}, {"problems", problems}, {"most_material", {{"params", most}, {"metrics", strong.value("metrics", nlohmann::json())}, {"status", strong.value("status", "")}}},
          {"least_material", {{"params", least}, {"metrics", light.value("metrics", nlohmann::json())}, {"status", light.value("status", "")}}},
          {"direction", direction}, {"method", "evaluate the most-material and least-material corners of the free parameters"}};
}

}  // namespace structura
