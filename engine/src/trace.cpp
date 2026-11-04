#include "structura/trace.hpp"

#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

namespace structura {

nlohmann::json trace(const DesignSpec& spec, const nlohmann::json& ev, const std::string& evidence_file) {
  nlohmann::json rows = nlohmann::json::array();
  for (size_t i = 0; i < spec.requirements.size(); ++i) {
    const Requirement& r = spec.requirements[i];
    nlohmann::json row = {{"id", r.id}, {"source", r.source}, {"spec_field", "requirements[" + std::to_string(i) + "]"},
                          {"checker", r.quantity + " " + r.op + " " + std::to_string(r.value) + " " + quantities().at(r.quantity).unit},
                          {"evidence", evidence_file}};
    bool found = false;
    for (auto& c : ev.value("checks", nlohmann::json::array()))
      if (c["id"] == r.id) {
        row["value"] = c["value"];
        row["margin"] = c["margin"];
        row["passed"] = c["passed"];
        row["load_case"] = c["load_case"];
        found = true;
      }
    if (!found) {
      row["passed"] = false;
      row["note"] = "not evaluated (status " + ev.value("status", std::string("unknown")) + ")";
    }
    rows.push_back(row);
  }
  return rows;
}

namespace {
void diff_rec(const nlohmann::json& a, const nlohmann::json& b, const std::string& path, nlohmann::json& out) {
  if (a.type() != b.type() || (!a.is_object() && !a.is_array())) {
    if (a != b) out.push_back({{"path", path.empty() ? "/" : path}, {"old", a}, {"new", b}});
    return;
  }
  if (a.is_object()) {
    for (auto& [k, v] : a.items()) diff_rec(v, b.contains(k) ? b[k] : nlohmann::json(), path + "/" + k, out);
    for (auto& [k, v] : b.items())
      if (!a.contains(k)) out.push_back({{"path", path + "/" + k}, {"old", nullptr}, {"new", v}});
    return;
  }
  size_t n = std::max(a.size(), b.size());
  for (size_t i = 0; i < n; ++i)
    diff_rec(i < a.size() ? a[i] : nlohmann::json(), i < b.size() ? b[i] : nlohmann::json(), path + "/" + std::to_string(i), out);
}
}  // namespace

nlohmann::json spec_diff(const nlohmann::json& a, const nlohmann::json& b) {
  nlohmann::json out = nlohmann::json::array();
  diff_rec(a, b, "", out);
  return out;
}

std::string save_spec_version(const nlohmann::json& spec, const std::string& folder) {
  fs::create_directories(folder);
  int n = 0;
  static const std::regex re("spec\\.v(\\d+)\\.json");
  for (auto& e : fs::directory_iterator(folder)) {
    std::smatch m;
    std::string name = e.path().filename().string();
    if (std::regex_match(name, m, re)) n = std::max(n, std::stoi(m[1]));
  }
  // requirement ids must never be renumbered between versions
  if (n > 0) {
    std::ifstream in(fs::path(folder) / ("spec.v" + std::to_string(n) + ".json"));
    auto prev = nlohmann::json::parse(in);
    std::map<std::string, std::string> prev_q;
    for (auto& r : prev.value("requirements", nlohmann::json::array())) prev_q[r["id"]] = r["quantity"];
    const nlohmann::json reqs = spec.value("requirements", nlohmann::json::array());
    for (auto& r : reqs) {
      std::string id = r["id"].get<std::string>(), q = r["quantity"].get<std::string>();
      if (prev_q.count(id) && prev_q[id] != q)
        throw SpecError("requirement " + id + " changed quantity between versions; give it a new id instead");
    }
  }
  fs::path p = fs::path(folder) / ("spec.v" + std::to_string(n + 1) + ".json");
  std::ofstream(p) << spec.dump(2);
  return p.string();
}

}  // namespace structura
