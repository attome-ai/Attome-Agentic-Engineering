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
  nlohmann::json setup = nlohmann::json::object();
  for (auto& slc : spec.load_cases) {  // C3: refuse a wrong setup before solving anything
    SetupReport sr = check_setup(m, scaled(slc));
    setup[slc.lc.name] = sr.to_json();
    if (!sr.ok) {
      out["setup_check"] = setup;
      out["error"] = "setup of load case '" + slc.lc.name + "': " + sr.problems.front();
      return finish("setup_fail");
    }
  }
  out["setup_check"] = setup;
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
        if (o.save_bundle && !mr.shapes.empty()) {  // mode 1 for the viewer's vibration animation (G2), normalized to 1 mm peak
          Result rm;
          double peak = 0;
          for (auto& [n, u] : mr.shapes[0]) peak = std::max(peak, std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]));
          for (auto& [n, u] : mr.shapes[0]) {
            std::array<double, 3> v = {u[0] / (peak > 0 ? peak : 1), u[1] / (peak > 0 ? peak : 1), u[2] / (peak > 0 ? peak : 1)};
            rm.disp[n] = v;
            rm.von_mises[n] = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);  // relative amplitude, for colour
          }
          rm.max_disp_mm = 1;
          rm.max_von_mises_MPa = 1;
          std::string mp = (fs::path(work) / ("result_" + lc.name) / "mode1.json").string();
          fs::create_directories(fs::path(mp).parent_path());
          write_skin(m, rm, mp, 0);
          std::ifstream min(mp);
          nlohmann::json mj = nlohmann::json::parse(min);
          min.close();
          std::ofstream(mp) << nlohmann::json({{"frequency_hz", mr.frequencies_hz.at(0)}, {"disp", mj["disp"]}, {"amplitude", mj["von_mises"]}}).dump();
        }
      }
      mt.by_case[lc.name] = met;
      if (o.save_bundle) {
        r.save((fs::path(work) / ("result_" + lc.name)).string());
        write_vtu(m, r, (fs::path(work) / ("result_" + lc.name) / "result.vtu").string());
        write_skin(m, r, (fs::path(work) / ("result_" + lc.name) / "skin.json").string(), eff.yield);
        write_volume(m, r.von_mises, "von_mises", "MPa", (fs::path(work) / ("result_" + lc.name) / "volume.json").string());
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
        write_volume(m, tr.temperature, "temperature", "degC", (fs::path(work) / ("result_" + tc.name) / "volume.json").string());
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
  nlohmann::json problems = nlohmann::json::array(), inconclusive = nlohmann::json::array();
  for (auto& r : spec.requirements) {
    bool mass_like = r.quantity == "mass_g" || r.quantity == "cost";
    const nlohmann::json& ev = mass_like ? light : strong;
    if (ev.value("status", "") != "ok") {  // a corner that cannot be built or solved proves nothing either way
      inconclusive.push_back({{"id", r.id}, {"reason", "the " + std::string(mass_like ? "least" : "most") +
                                                         "-material corner could not be evaluated (" + ev.value("status", std::string("?")) +
                                                         "); the search decides this requirement"}});
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
  return {{"feasible", problems.empty()}, {"problems", problems}, {"inconclusive", inconclusive}, {"conclusive", inconclusive.empty()}, {"most_material", {{"params", most}, {"metrics", strong.value("metrics", nlohmann::json())}, {"status", strong.value("status", "")}}},
          {"least_material", {{"params", least}, {"metrics", light.value("metrics", nlohmann::json())}, {"status", light.value("status", "")}}},
          {"direction", direction}, {"method", "evaluate the most-material and least-material corners of the free parameters"}};
}


nlohmann::json dynamic_bundle(const DesignSpec& spec, const Values& free_params, const std::string& out, const DynamicOptions& o) {
  if (spec.load_cases.empty()) throw std::runtime_error("dynamic: the spec has no structural load case");
  fs::create_directories(out);
  Values params = all_params(spec, free_params);
  DesignGraph g = load_family(spec.family);
  Design d(g, params);
  CompiledPart part = d.build();
  write_step(part.shape, (fs::path(out) / "part.step").string());
  write_stl(part.shape, (fs::path(out) / "part.stl").string());
  MeshOptions mo;
  mo.size = o.mesh_size > 0 ? o.mesh_size : spec.mesh_size;
  mo.threads = o.threads;
  Mesh m = mesh_shape(part.shape, part.named_faces(), (fs::path(out) / "mesh").string(), mo);
  Material eff = spec.effective_material();
  LoadCase lc = spec.load_cases[0].lc;  // nominal load: the real event, not the design factor
  SolveOptions so;
  so.threads = o.threads;
  so.timeout_s = 3600;  // a time simulation runs hundreds of increments
  so.exclude_near_bc_mm = spec.stress_exclude_near_bc_mm;
  Result st = solve_static(m, eff, lc, (fs::path(out) / "static").string(), so);
  write_vtu(m, st, (fs::path(out) / "mesh.vtu").string());
  // the load is a carried mass (a crate): its inertia goes into the dynamics, its weight is the load case's force
  std::vector<PointMass> masses;
  double carried_kg = 0;
  if (o.carried_mass)
    for (auto& l : lc.loads) {
      double fn = std::sqrt(l.force[0] * l.force[0] + l.force[1] * l.force[1] + l.force[2] * l.force[2]);
      masses.push_back({l.group, fn / 9810.0});  // N / (mm/s^2) = tonne
      carried_kg += fn / 9.81;
    }
  ModalResult mr = solve_modal(m, eff, lc.supports, 3, (fs::path(out) / "modal").string(), so, masses);
  // time step from the highest computed mode (the vertical response of a bracket is often its second mode, not the
  // first), duration from the lowest; damping held near the stated ratio across all computed modes
  // time step from the highest computed mode (the vertical response of a bracket is often its second mode, not the
  // first), duration from the lowest; damping held near the stated ratio across all computed modes
  double f1 = mr.frequencies_hz.front(), fmax = mr.frequencies_hz.back();
  double dt = 1.0 / (fmax * o.steps_per_period), duration = o.periods / f1;
  DynamicResult dr = solve_dynamic(m, eff, lc, duration, dt, o.damping_ratio, f1, fmax, o.store_every, (fs::path(out) / "dynamic").string(), so,
                                   masses);

  // frames.bin: float32 [frame][node][ux, uy, uz, vm], nodes in ascending id order
  std::vector<int> ids;
  for (auto& [n, _] : m.nodes) ids.push_back(n);
  std::ofstream fb(fs::path(out) / "frames.bin", std::ios::binary);
  double peak_disp = 0, peak_vm = 0, peak_t = 0;
  for (size_t k = 0; k < dr.disp.size(); ++k) {
    for (int n : ids) {
      auto it = dr.disp[k].find(n);
      std::array<double, 3> u = it == dr.disp[k].end() ? std::array<double, 3>{0, 0, 0} : it->second;
      auto iv = dr.von_mises[k].find(n);
      float rec[4] = {static_cast<float>(u[0]), static_cast<float>(u[1]), static_cast<float>(u[2]),
                      static_cast<float>(iv == dr.von_mises[k].end() ? 0.0 : iv->second)};
      fb.write(reinterpret_cast<const char*>(rec), sizeof rec);
      double mag = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
      if (mag > peak_disp) { peak_disp = mag; peak_t = dr.times[k]; }
      if (rec[3] > peak_vm) peak_vm = rec[3];
    }
  }
  fb.close();
  nlohmann::json markers = nlohmann::json::array();
  auto group_box = [&](const std::string& grp, nlohmann::json mk) {
    auto ns = m.nsets.find(grp);
    if (ns == m.nsets.end() || ns->second.empty()) return;
    double c[3] = {0, 0, 0}, lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (int n : ns->second) {
      auto& x = m.nodes.at(n);
      for (int i = 0; i < 3; ++i) { c[i] += x[i]; lo[i] = std::min(lo[i], x[i]); hi[i] = std::max(hi[i], x[i]); }
    }
    double k = static_cast<double>(ns->second.size());
    mk["group"] = grp;
    mk["at"] = {c[0] / k, c[1] / k, c[2] / k};
    mk["bbox"] = {lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]};
    markers.push_back(mk);
  };
  for (auto& l : lc.loads) group_box(l.group, {{"kind", "load"}, {"force", l.force}});
  for (auto& sp : lc.supports) group_box(sp.group, {{"kind", "support"}, {"dofs", sp.dofs}});
  double static_disp = st.max_disp_mm;
  nlohmann::json j = {
      {"spec", spec.id}, {"params", d.params}, {"hash", d.hash()}, {"load_case", lc.name},
      {"event", o.carried_mass ? "a crate of the load case's weight set down at once at t = 0 (its mass lumped on the loaded face)"
                               : "the load case's nominal force applied suddenly at t = 0 and held"},
      {"carried_mass_kg", carried_kg},
      {"hand_check", {{"stiffness_N_per_mm", st.max_disp_mm > 0 ? (carried_kg * 9.81) / st.max_disp_mm : 0.0},
                      {"note", "single-mass estimate f = sqrt(k / m) / 2 pi with k = weight / static deflection"},
                      {"frequency_hz", (carried_kg > 0 && st.max_disp_mm > 0)
                                           ? std::sqrt((carried_kg * 9.81 / (st.max_disp_mm * 1e-3)) / carried_kg) / (2 * 3.14159265358979) : 0.0}}},
      {"mesh_size", mo.size}, {"nodes", ids.size()}, {"node_ids", ids}, {"frames", dr.times.size()}, {"times_s", dr.times},
      {"dt_s", dt}, {"duration_s", duration}, {"frequencies_hz", mr.frequencies_hz},
      {"damping", {{"ratio", o.damping_ratio}, {"rayleigh_alpha_per_s", dr.alpha}, {"rayleigh_beta_s", dr.beta},
                   {"note", "Rayleigh damping fitted to the stated ratio at the lowest and highest computed frequencies: an assumption, "
                            "not measured; CalculiX's implicit integrator adds a little numerical damping"}}},
      {"static", {{"max_disp_mm", static_disp}, {"max_von_mises_MPa", st.max_von_mises_raw_MPa}}},
      {"peak", {{"max_disp_mm", peak_disp}, {"max_von_mises_MPa", peak_vm}, {"time_s", peak_t}}},
      {"dynamic_amplification", static_disp > 0 ? peak_disp / static_disp : 0.0},
      {"material", {{"id", eff.id}, {"E", eff.E}, {"density", eff.density}, {"yield_effective", eff.yield}}},
      {"markers", markers}, {"solver_version", dr.solver_version}, {"solve_seconds", dr.runtime_s},
      {"files", {{"mesh", "mesh.vtu"}, {"frames", "frames.bin"}, {"part", "part.stl"}}}};
  std::ofstream(fs::path(out) / "dynamic.json") << j.dump(1);
  j.erase("node_ids");
  j.erase("times_s");
  return j;
}

}  // namespace structura
