#include "structura/spec.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <filesystem>
#include <fstream>

#include "structura/design.hpp"

namespace fs = std::filesystem;

namespace structura {

const std::map<std::string, Quantity>& quantities() {
  static const std::map<std::string, Quantity> q = {
      {"mass_g", {"g", false, "part mass"}},
      {"max_von_mises_MPa", {"MPa", false, "peak von Mises stress over the part"}},
      {"max_disp_mm", {"mm", false, "largest displacement magnitude"}},
      {"safety_factor", {"1", true, "effective yield strength / peak von Mises stress"}},
      {"first_frequency_hz", {"Hz", true, "lowest natural frequency"}},
      {"buckling_factor", {"1", true, "load multiplier at the first buckling mode"}},
      {"peak_temperature", {"degC", false, "highest temperature in the part"}},
      {"cost", {"currency", false, "manufacturing cost from the cost model"}},
  };
  return q;
}

nlohmann::json Check::to_json() const {
  return {{"id", id}, {"quantity", quantity}, {"op", op}, {"unit", unit}, {"value", value}, {"limit", limit},
          {"margin", margin}, {"passed", passed}, {"load_case", load_case}};
}

DesignSpec DesignSpec::from_json(const nlohmann::json& j) {
  DesignSpec s;
  s.raw = j;
  try {
    s.id = j.at("id");
    s.version = j.value("version", 1);
    s.brief = j.value("brief", std::string());
    s.family = j.at("family");
  } catch (const std::exception& e) {
    throw SpecError(std::string("spec needs id and family: ") + e.what());
  }
  DesignGraph g = load_family(s.family);
  const nlohmann::json fp = j.value("fixed_params", nlohmann::json::object());
  for (auto& [k, v] : fp.items()) {
    if (!g.param(k)) throw SpecError("fixed parameter '" + k + "' is not a parameter of family " + s.family);
    s.fixed_params[k] = v.get<double>();
  }
  const nlohmann::json free = j.at("free_params");
  for (auto& [k, v] : free.items()) {
    const Param* p = g.param(k);
    if (!p) throw SpecError("free parameter '" + k + "' is not a parameter of family " + s.family);
    std::array<double, 2> b = v.get<std::array<double, 2>>();
    if (b[0] > b[1]) throw SpecError("free parameter '" + k + "' has low > high");
    if (b[0] < p->low - 1e-9 || b[1] > p->high + 1e-9)
      throw SpecError("free parameter '" + k + "' bounds [" + std::to_string(b[0]) + ", " + std::to_string(b[1]) +
                      "] exceed the family's [" + std::to_string(p->low) + ", " + std::to_string(p->high) + "] " + p->unit);
    if (s.fixed_params.count(k)) throw SpecError("parameter '" + k + "' is both fixed and free");
    s.free_params[k] = b;
  }
  // no free parameters is allowed: the spec then analyses one fixed design (an uploaded part as-is, A3); the optimizer refuses it
  s.material = j.value("material", std::string("PETG"));
  try {
    load_material(s.material);
  } catch (const std::exception& e) {
    throw SpecError(e.what());
  }
  s.apply_print_knockdown = j.value("apply_print_knockdown", true);
  if (j.contains("print_direction")) s.print_direction = j["print_direction"].get<std::array<double, 3>>();
  s.process = j.value("process", std::string());
  if (!s.process.empty() && s.process != "fdm" && s.process != "cnc") throw SpecError("process must be fdm or cnc, not '" + s.process + "'");
  const nlohmann::json tcs = j.value("thermal_cases", nlohmann::json::array());
  for (auto& tc : tcs) {
    ThermalCase t;
    try {
      t = ThermalCase::from_json(tc);
    } catch (const std::exception& e) {
      throw SpecError(e.what());
    }
    auto named = [&](const std::string& grp) { return std::any_of(g.faces.begin(), g.faces.end(), [&](auto& f) { return f.first == grp; }); };
    for (auto& x : t.temperatures) if (!named(x.group)) throw SpecError("thermal group '" + x.group + "' is not a named face of family " + s.family);
    for (auto& x : t.films) if (!named(x.group)) throw SpecError("film group '" + x.group + "' is not a named face of family " + s.family);
    for (auto& x : t.films) {
      if (!x.correlation) continue;
      for (const std::string* e : {&x.correlation->spacing_mm, &x.correlation->fin_thickness_mm, &x.correlation->length_mm}) {
        std::set<std::string> names;
        try {
          names = names_in(*e);
        } catch (const std::exception& ex) {
          throw SpecError("film '" + x.group + "' correlation expression '" + *e + "': " + ex.what());
        }
        for (auto& n : names)
          if (std::none_of(g.parameters.begin(), g.parameters.end(), [&](auto& p) { return p.first == n; }))
            throw SpecError("film '" + x.group + "' correlation uses '" + n + "', which is not a parameter of family " + s.family);
      }
    }
    for (auto& x : t.fluxes) if (!named(x.group)) throw SpecError("flux group '" + x.group + "' is not a named face of family " + s.family);
    s.thermal_cases.push_back(t);
  }
  const nlohmann::json lcs = j.value("load_cases", nlohmann::json::array());
  for (auto& lc : lcs) {
    SpecLoadCase x;
    try {
      x.lc = LoadCase::from_json(lc);
    } catch (const std::exception& e) {
      throw SpecError(e.what());
    }
    if (lc.contains("factor_range")) x.factor_range = lc["factor_range"].get<std::array<double, 2>>();
    if (x.factor_range[0] <= 0 || x.factor_range[0] > x.factor_range[1]) throw SpecError("load case '" + x.lc.name + "' has a bad factor_range");
    for (auto& sup : x.lc.supports)
      if (!std::any_of(g.faces.begin(), g.faces.end(), [&](auto& f) { return f.first == sup.group; }))
        throw SpecError("support group '" + sup.group + "' is not a named face of family " + s.family);
    for (auto& l : x.lc.loads)
      if (!std::any_of(g.faces.begin(), g.faces.end(), [&](auto& f) { return f.first == l.group; }))
        throw SpecError("load group '" + l.group + "' is not a named face of family " + s.family);
    s.load_cases.push_back(x);
  }
  if (s.load_cases.empty() && s.thermal_cases.empty()) throw SpecError("spec has no load cases or thermal cases");
  for (auto& o : j.value("objectives", nlohmann::json::array())) {
    Objective x{o.at("quantity"), o.value("sense", std::string("min"))};
    if (!quantities().count(x.quantity)) throw SpecError("unknown objective quantity '" + x.quantity + "'");
    if (x.sense != "min" && x.sense != "max") throw SpecError("objective sense must be min or max");
    s.objectives.push_back(x);
  }
  std::set<std::string> ids;
  for (auto& r : j.value("requirements", nlohmann::json::array())) {
    Requirement x{r.at("id"), r.at("quantity"), r.at("op"), r.at("value"), r.value("source", std::string())};
    if (!quantities().count(x.quantity)) throw SpecError("requirement " + x.id + ": unknown quantity '" + x.quantity + "'");
    if (x.op != "<=" && x.op != ">=") throw SpecError("requirement " + x.id + ": op must be <= or >=");
    if (!ids.insert(x.id).second) throw SpecError("requirement id " + x.id + " is used twice");
    s.requirements.push_back(x);
  }
  if (s.requirements.empty()) throw SpecError("spec has no requirements");
  s.mesh_size = j.value("mesh", nlohmann::json::object()).value("size", 4.0);
  s.stress_exclude_near_bc_mm = j.value("stress_evaluation", nlohmann::json::object()).value("exclude_near_bc_mm", 3.0);
  if (s.stress_exclude_near_bc_mm < 0) throw SpecError("stress_evaluation.exclude_near_bc_mm must not be negative");
  bool uses_cost = std::any_of(s.objectives.begin(), s.objectives.end(), [](auto& o) { return o.quantity == "cost"; }) ||
                   std::any_of(s.requirements.begin(), s.requirements.end(), [](auto& r) { return r.quantity == "cost"; });
  if (uses_cost && s.process.empty()) throw SpecError("the cost quantity needs a process (\"process\": \"fdm\" or \"cnc\")");
  return s;
}

DesignSpec DesignSpec::load(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw SpecError("cannot open " + path);
  return from_json(nlohmann::json::parse(in));
}

std::string DesignSpec::hash() const { return sha256_hex(raw.dump()); }

Material DesignSpec::effective_material() const {
  Material m = load_material(material);
  if (!apply_print_knockdown) return m;
  fs::path p = fs::path(STRUCTURA_ROOT) / "data" / "materials.json";
  std::ifstream in(p);
  auto j = nlohmann::json::parse(in);
  auto& mj = j["materials"][material];
  if (mj.contains("print_knockdown")) {
    auto& k = mj["print_knockdown"];
    m.E *= k.value("modulus", 1.0);
    m.yield *= k.value("strength", 1.0);
    m.name += " (printed, knock-down applied)";
    m.source += "; print knock-down: " + k.value("source", std::string("unsourced"));
  }
  return m;
}

std::map<std::string, double> structural_metrics(const Result& r, const Material& eff) {
  return {{"mass_g", r.mass_t * 1e6}, {"max_von_mises_MPa", r.max_von_mises_MPa}, {"max_disp_mm", r.max_disp_mm},
          {"safety_factor", r.max_von_mises_MPa > 0 ? eff.yield / r.max_von_mises_MPa : 1e9}};
}

std::vector<Check> DesignSpec::check(const Metrics& m) const {
  std::vector<Check> out;
  for (auto& r : requirements) {
    Check c;
    c.id = r.id;
    c.quantity = r.quantity;
    c.op = r.op;
    c.unit = quantities().at(r.quantity).unit;
    c.limit = r.value;
    auto it = m.overall.find(r.quantity);
    if (it == m.overall.end()) {
      c.passed = false;
      c.margin = -1;
      c.load_case = "not evaluated";
      out.push_back(c);
      continue;
    }
    c.value = it->second;
    c.load_case = m.governing_case.count(r.quantity) ? m.governing_case.at(r.quantity) : "";
    double denom = std::fabs(r.value) > 0 ? std::fabs(r.value) : 1.0;
    c.margin = r.op == "<=" ? (r.value - c.value) / denom : (c.value - r.value) / denom;
    c.passed = c.margin >= 0;
    out.push_back(c);
  }
  return out;
}

}  // namespace structura
