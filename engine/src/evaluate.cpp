#include "structura/evaluate.hpp"
#include "structura/manufacture.hpp"

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


// Where loads, supports and heat inputs act, for the viewer's markers (G1): centroid and bounding box of each group's
// mesh nodes, with the force vector or the fixed directions; appended to the skin file.
void add_markers(const std::string& skin_path, const Mesh& m, const nlohmann::json& items) {
  std::ifstream in(skin_path);
  if (!in) return;
  nlohmann::json skin = nlohmann::json::parse(in);
  in.close();
  nlohmann::json out = nlohmann::json::array();
  for (auto& it : items) {
    auto ns = m.nsets.find(it["group"].get<std::string>());
    if (ns == m.nsets.end() || ns->second.empty()) continue;
    double c[3] = {0, 0, 0}, lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (int n : ns->second) {
      auto& x = m.nodes.at(n);
      for (int i = 0; i < 3; ++i) { c[i] += x[i]; lo[i] = std::min(lo[i], x[i]); hi[i] = std::max(hi[i], x[i]); }
    }
    double k = static_cast<double>(ns->second.size());
    nlohmann::json mk = it;
    mk["at"] = {c[0] / k, c[1] / k, c[2] / k};
    mk["bbox"] = {lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]};
    out.push_back(mk);
  }
  skin["markers"] = out;
  std::ofstream(skin_path) << skin.dump();
}


bool spec_uses(const DesignSpec& spec, const std::string& q) {
  for (auto& o : spec.objectives)
    if (o.quantity == q) return true;
  for (auto& r : spec.requirements)
    if (r.quantity == q) return true;
  return false;
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
  lim.preserve = part.graph.preserve;
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
  so.exclude_near_bc_mm = spec.stress_exclude_near_bc_mm;
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
      cases[lc.name] = r.summary();
      cases[lc.name]["factor"] = slc.factor_range[1];
      // further analyses on the same mesh, only when the spec asks for their quantity
      if (spec_uses(spec, "buckling_factor")) {
        BuckleResult br = solve_buckle(m, eff, lc, 1, (fs::path(work) / ("buckle_" + lc.name)).string(), so);
        met["buckling_factor"] = br.factors.at(0);
        cases[lc.name]["buckling_factors"] = br.factors;
      }
      if (spec_uses(spec, "first_frequency_hz")) {
        ModalResult mr = solve_modal(m, eff, lc.supports, 3, (fs::path(work) / ("modal_" + lc.name)).string(), so);
        met["first_frequency_hz"] = mr.frequencies_hz.at(0);
        cases[lc.name]["frequencies_hz"] = mr.frequencies_hz;
      }
      mt.by_case[lc.name] = met;
      if (o.save_bundle) {
        r.save((fs::path(work) / ("result_" + lc.name)).string());
        write_vtu(m, r, (fs::path(work) / ("result_" + lc.name) / "result.vtu").string());
        write_skin(m, r, (fs::path(work) / ("result_" + lc.name) / "skin.json").string(), eff.yield);
        nlohmann::json items = nlohmann::json::array();
        for (auto& l : lc.loads) items.push_back({{"kind", "load"}, {"group", l.group}, {"force", l.force}, {"pressure", l.pressure}});
        for (auto& sp : lc.supports) items.push_back({{"kind", "support"}, {"group", sp.group}, {"dofs", sp.dofs}});
        add_markers((fs::path(work) / ("result_" + lc.name) / "skin.json").string(), m, items);
      }
    }
    for (auto& tc : spec.thermal_cases) {  // steady heat transfer on the same mesh (C2)
      std::string dir = (fs::path(work) / ("thermal_" + tc.name)).string();
      nlohmann::json films_info;
      ThermalCase tcr = tc.resolved(d.params, &films_info);  // correlation films get this design's h
      ThermalResult tr = solve_thermal(m, eff, tcr, dir, so);
      if (o.save_bundle) {
        fs::create_directories(fs::path(work) / ("result_" + tc.name));
        write_skin_scalar(m, tr.temperature, "temperature", "degC", (fs::path(work) / ("result_" + tc.name) / "skin.json").string());
        nlohmann::json items = nlohmann::json::array();
        for (auto& f : tcr.fluxes) items.push_back({{"kind", "heat"}, {"group", f.group}, {"power", f.power}});
        add_markers((fs::path(work) / ("result_" + tc.name) / "skin.json").string(), m, items);
      }
      double mass_g = volume(rg.shape) * eff.density * 1e6;
      mt.by_case[tc.name] = {{"peak_temperature", tr.t_max}, {"mass_g", mass_g}};
      cases[tc.name] = tr.summary();
      cases[tc.name]["mass_g"] = mass_g;
      if (!films_info.empty()) cases[tc.name]["films"] = films_info;
    }
  } catch (const std::exception& e) {
    out["error"] = e.what();
    return finish("solve_fail");
  }
  if (!spec.process.empty()) {  // manufacturing cost (H1): the same for every case, so it is added to each
    try {
      nlohmann::json detail;
      double c = process_cost(rg.shape, spec.material, spec.print_direction, load_processes(), spec.process, &detail);
      for (auto& [cname, met] : mt.by_case) met["cost"] = c;
      out["manufacturing"] = detail;
    } catch (const std::exception& e) {
      out["error"] = std::string("cost model: ") + e.what();
      return finish("solve_fail");
    }
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
