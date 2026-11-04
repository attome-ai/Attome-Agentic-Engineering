#include "structura/trust.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "structura/design.hpp"
#include "structura/evaluate.hpp"

namespace fs = std::filesystem;

namespace structura {
namespace {

std::string pct(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f%%", 100 * v);
  return b;
}

double rep_size(const nlohmann::json& ev) {
  double vol = 0;
  for (auto& [_, c] : ev["load_cases"].items()) vol = std::max(vol, c.value("volume_mm3", 0.0));
  if (vol <= 0 && ev["metrics"].contains("mass_g") && ev["material"].value("density", 0.0) > 0)  // thermal-only studies
    vol = ev["metrics"]["mass_g"].get<double>() * 1e-6 / ev["material"]["density"].get<double>();
  double n = ev["mesh"].value("elements", 1.0);
  return std::cbrt(vol / std::max(1.0, n));
}

nlohmann::json validation_status() {
  fs::path p = fs::path(STRUCTURA_ROOT) / "docs" / "validation.json";
  if (!fs::exists(p)) return {{"ran", false}};
  std::ifstream in(p);
  auto j = nlohmann::json::parse(in);
  return {{"ran", true}, {"passed", j.value("passed", 0)}, {"cases", j.value("cases", 0)}, {"date", j.value("date", "")},
          {"all_passed", j.value("passed", 0) == j.value("cases", -1)}};
}

}  // namespace

nlohmann::json Gci::to_json() const {
  return {{"fine", fine}, {"medium", medium}, {"coarse", coarse}, {"h", {h1, h2, h3}}, {"observed_order", order},
          {"gci_fine", gci}, {"monotone", monotone}};
}

Gci grid_convergence(double f1, double f2, double f3, double h1, double h2, double h3) {
  Gci g{f1, f2, f3, h1, h2, h3};
  double e21 = f2 - f1, e32 = f3 - f2;
  double r21 = h2 / h1, r32 = h3 / h2;
  if (std::fabs(e21) < 1e-14 * std::max(1.0, std::fabs(f1))) {  // converged to round-off
    g.monotone = true;
    g.order = 0;
    g.gci = 0;
    return g;
  }
  g.monotone = e32 / e21 > 0;
  double s = e32 / e21 > 0 ? 1.0 : -1.0;
  double p = 2.0;
  for (int it = 0; it < 100; ++it) {  // fixed-point iteration for the observed order (Celik et al. 2008)
    double q = std::log((std::pow(r21, p) - s) / (std::pow(r32, p) - s));
    double pn = std::fabs(std::log(std::fabs(e32 / e21)) + q) / std::log(r21);
    if (!std::isfinite(pn)) break;
    if (std::fabs(pn - p) < 1e-8) { p = pn; break; }
    p = pn;
  }
  p = std::min(std::max(p, 0.5), 4.0);  // keep the estimate physical; reported as used
  g.order = p;
  g.gci = 1.25 * std::fabs(e21 / f1) / (std::pow(r21, p) - 1);
  return g;
}

nlohmann::json build_trust(const DesignSpec& spec, const Values& free, const TrustOptions& o) {
  double h = o.fine_size > 0 ? o.fine_size : spec.mesh_size / 2;
  std::string work = o.workdir.empty() ? (fs::temp_directory_path() / "structura_trust").string() : o.workdir;
  fs::create_directories(work);
  EvalOptions eo;
  eo.threads = o.threads;
  // fine level keeps the full evidence bundle; medium and coarse keep only their evaluation summaries
  eo.mesh_size = h;
  eo.workdir = (fs::path(work) / "fine").string();
  eo.save_bundle = true;
  nlohmann::json fine = evaluate_design(spec, free, eo);
  eo.save_bundle = false;
  eo.mesh_size = h * o.ratio;
  eo.workdir = (fs::path(work) / "medium").string();
  nlohmann::json medium = evaluate_design(spec, free, eo);
  eo.mesh_size = h * o.ratio * o.ratio;
  eo.workdir = (fs::path(work) / "coarse").string();
  nlohmann::json coarse = evaluate_design(spec, free, eo);

  nlohmann::json t;
  t["written_by"] = "structura engine (agents cannot write this file)";
  t["date"] = utc_now();
  t["spec"] = {{"id", spec.id}, {"version", spec.version}, {"hash", spec.hash()}, {"path", o.spec_path}};
  t["params"] = fine.value("params", nlohmann::json());
  t["design_hash"] = fine.value("hash", "");
  t["mesh_sizes"] = {h, h * o.ratio, h * o.ratio * o.ratio};
  if (fine.value("status", "") != "ok" || medium.value("status", "") != "ok" || coarse.value("status", "") != "ok") {
    t["status"] = "not_verified";
    t["grade"] = "C";
    t["reasons"] = {"an evaluation level did not complete: fine " + fine.value("status", std::string("?")) + ", medium " +
                    medium.value("status", std::string("?")) + ", coarse " + coarse.value("status", std::string("?"))};
    std::ofstream(fs::path(work) / "trust.json") << t.dump(2);
    return t;
  }

  // numerical error bars
  double h1 = rep_size(fine), h2 = rep_size(medium), h3 = rep_size(coarse);
  nlohmann::json err = nlohmann::json::object();
  std::map<std::string, Gci> gci;
  for (auto& [q, _] : fine["metrics"].items()) {  // every governing quantity the spec produced
    if (q == "safety_factor" || !medium["metrics"].contains(q) || !coarse["metrics"].contains(q)) continue;
    Gci g = grid_convergence(fine["metrics"][q], medium["metrics"][q], coarse["metrics"][q], h1, h2, h3);
    gci[q] = g;
    err[q] = g.to_json();
  }
  if (gci.count("max_von_mises_MPa")) gci["safety_factor"] = gci["max_von_mises_MPa"];  // SF = yield / peak stress
  t["numerical_error"] = err;

  // requirements with margins, proven or not
  nlohmann::json reqs = nlohmann::json::array();
  bool all_pass = true, all_proven = true;
  double worst_gci_gov = 0;
  for (auto& c : fine["checks"]) {
    std::string q = c["quantity"];
    double value = c["value"], limit = c["limit"];
    double e = gci.count(q) ? gci[q].gci : 0.0;
    bool monotone = gci.count(q) ? gci[q].monotone : true;
    double room = std::fabs(value - limit), error_abs = e * std::fabs(value);
    bool passed = c["passed"];
    // a value converged to round-off may oscillate; monotone convergence is only required when the error matters
    // oscillatory convergence is accepted when its error is small (ASME V&V 20 style); it is still reported
    bool proven = passed && room > error_abs && (monotone || e < 0.05);
    all_pass = all_pass && passed;
    all_proven = all_proven && proven;
    worst_gci_gov = std::max(worst_gci_gov, e);
    reqs.push_back({{"id", c["id"]}, {"quantity", q}, {"value", value}, {"limit", limit}, {"op", c["op"]}, {"unit", c["unit"]},
                    {"margin", c["margin"]}, {"numerical_error", e}, {"governing_case", c["load_case"]},
                    {"result", !passed ? "fail" : (proven ? "pass" : "not proven")}});
  }
  t["requirements"] = reqs;

  // independent checks
  nlohmann::json checks = nlohmann::json::array();
  bool independent_ok = true;
  for (auto& [name, c] : fine["load_cases"].items()) {
    if (!c.contains("equilibrium_error") && !c.contains("heat_balance_error")) continue;
    if (c.contains("heat_balance_error")) {  // thermal case: heat out through the films equals heat in
      double hb = c.value("heat_balance_error", 1.0);
      bool hok = hb >= 0 && hb < 0.01;
      checks.push_back({{"check", "heat balance (" + name + ")"}, {"value", hb}, {"limit", 0.01}, {"passed", hok},
                        {"detail", "heat in " + std::to_string(c.value("heat_in_W", 0.0)) + " W, convected out " +
                                       std::to_string(c.value("heat_out_W", 0.0)) + " W"}});
      independent_ok = independent_ok && hok;
      continue;
    }
    double eq = c.value("equilibrium_error", 1.0);
    bool ok = eq < 0.005;
    checks.push_back({{"check", "reaction equilibrium (" + name + ")"}, {"value", eq}, {"limit", 0.005}, {"passed", ok},
                      {"detail", "sum of reactions balances applied loads within " + pct(eq)}});
    independent_ok = independent_ok && ok;
    double se = c.value("strain_energy_Nmm", -1.0), w = c.value("external_work_Nmm", -1.0);
    if (se > 0 && w > 0) {
      double rel = std::fabs(se - w) / w;
      bool eok = rel < 0.01;
      checks.push_back({{"check", "strain energy = half the external work (" + name + ")"}, {"value", rel}, {"limit", 0.01}, {"passed", eok},
                        {"detail", "strain energy " + std::to_string(se) + " N mm vs " + std::to_string(w) + " N mm"}});
      independent_ok = independent_ok && eok;
    } else {
      checks.push_back({{"check", "energy balance (" + name + ")"}, {"passed", nullptr}, {"detail", "not available for pressure loads"}});
    }
  }
  t["independent_checks"] = checks;

  // assumptions in plain words
  nlohmann::json as = nlohmann::json::array();
  if (!spec.load_cases.empty())
    as.push_back("Linear elastic, isotropic material and small deflections (no yielding, buckling or contact is modelled in this result).");
  for (auto& tc : spec.thermal_cases)
    as.push_back("Thermal case '" + tc.name + "': steady conduction in the part with a fixed convection coefficient on the exposed faces "
                 "(no airflow is simulated); constant conductivity.");
  as.push_back("Material: " + fine["material"].value("name", std::string("?")) + "; source: " + fine["material"].value("source", std::string("?")) +
               (fine["material"].value("verify", true) ? " (values to be confirmed against the datasheet)" : ""));
  for (auto& slc : spec.load_cases) {
    as.push_back("Load case '" + slc.lc.name + "' evaluated at " + std::to_string(slc.factor_range[1]).substr(0, 4) + " times its nominal load.");
    for (auto& s : slc.lc.supports)
      if (s.dofs != "123")
        as.push_back("Face '" + s.group + "' is held only in direction(s) " + s.dofs +
                     " (bearing contact modelled as two-sided; a real wall pushes but does not pull).");
      else
        as.push_back("Face '" + s.group + "' is fully fixed (bolts modelled as rigid bores).");
  }
  t["assumptions"] = as;

  nlohmann::json vs = validation_status();
  t["validation_suite"] = vs;

  // grade with its written rule
  bool val_ok = vs.value("ran", false) && vs.value("all_passed", false);
  std::string grade = "C";
  if (all_pass && all_proven && independent_ok && val_ok && worst_gci_gov < 0.05) grade = "A";
  else if (all_pass && independent_ok && val_ok && worst_gci_gov <= 0.10) grade = "B";
  t["grade"] = grade;
  t["grade_rule"] = "A: every requirement passes and is proven (margin larger than its numerical error), every independent check "
                    "passes, numerical error on governing values below 5% and the validation suite passed. B: as A but error up "
                    "to 10% and requirements may be not proven. C: anything else.";
  t["status"] = (grade == "A" || grade == "B") ? "verified" : "not_verified";
  std::vector<std::string> why;
  if (!all_pass) why.push_back("a requirement fails");
  if (all_pass && !all_proven) why.push_back("a requirement margin is smaller than its numerical error");
  if (!independent_ok) why.push_back("an independent check fails");
  if (!val_ok) why.push_back("the validation suite has not passed in full");
  if (worst_gci_gov >= 0.05) why.push_back("numerical error on a governing value is " + pct(worst_gci_gov));
  t["reasons"] = why;

  // evidence and reproduction
  nlohmann::json evidence = {{"spec_hash", spec.hash()}, {"design_hash", t["design_hash"]}, {"params", t["params"]},
                             {"mesh_fine", fine["mesh"]}, {"metrics_fine", fine["metrics"]}};
  t["evidence_hash"] = sha256_hex(evidence.dump());
  t["fine_metrics"] = fine["metrics"];
  t["fine_mesh_size"] = h;
  t["reproduce"] = "structura reproduce --dir " + fs::absolute(work).generic_string();
  std::ofstream(fs::path(work) / "trust.json") << t.dump(2);
  return t;
}

nlohmann::json reproduce(const std::string& dir, double tol) {
  std::ifstream in(fs::path(dir) / "trust.json");
  if (!in) throw std::runtime_error("no trust.json in " + dir);
  auto t = nlohmann::json::parse(in);
  std::string sp = t["spec"].value("path", std::string());
  if (sp.empty()) throw std::runtime_error("trust.json does not record the spec path");
  DesignSpec spec = DesignSpec::load(sp);
  if (spec.hash() != t["spec"]["hash"].get<std::string>()) throw std::runtime_error("the spec file changed since the trust record was written");
  Values free;
  for (auto& [k, _] : spec.free_params) free[k] = t["params"].at(k).get<double>();
  EvalOptions eo;
  eo.mesh_size = t["fine_mesh_size"];
  eo.threads = 4;
  nlohmann::json ev = evaluate_design(spec, free, eo);
  nlohmann::json rows = nlohmann::json::array();
  bool match = ev.value("status", "") == "ok";
  if (match)
    for (auto& [q, v] : t["fine_metrics"].items()) {
      double a = v, b = ev["metrics"][q];
      double rel = std::fabs(b - a) / std::max(1e-12, std::fabs(a));
      rows.push_back({{"quantity", q}, {"recorded", a}, {"reproduced", b}, {"relative_difference", rel}});
      match = match && rel <= tol;
    }
  return {{"match", match}, {"tolerance", tol}, {"rows", rows}};
}

}  // namespace structura
