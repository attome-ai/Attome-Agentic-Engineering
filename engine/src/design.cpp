#include "structura/design.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace structura {

double round6(double v) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.6g", v);
  return std::strtod(buf, nullptr);
}

std::string utc_now() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

Design::Design(DesignGraph g, const Values& overrides, std::string created)
    : graph(std::move(g)), created_at(created.empty() ? utc_now() : std::move(created)) {
  for (auto& [k, v] : graph.values(overrides)) params[k] = round6(v);
}

std::string Design::hash() const {
  nlohmann::json p = nlohmann::json::object();
  for (auto& [k, v] : params) p[k] = v;
  nlohmann::json payload = {{"graph", nlohmann::json::parse(graph.canonical_json())}, {"params", p}};
  return sha256_hex(payload.dump());
}

CompiledPart Design::build() const { return compile_graph(graph, params); }

void Design::save(const std::string& folder) const {
  fs::create_directories(folder);
  std::ofstream(fs::path(folder) / "graph.json") << graph.to_json().dump(2);
  ojson meta = {{"template", graph.name}, {"template_version", graph.version}, {"params", params},
                {"created_at", created_at}, {"hash", hash()}};
  std::ofstream(fs::path(folder) / "params.json") << meta.dump(2);
}

Design Design::load(const std::string& folder) {
  DesignGraph g = DesignGraph::load((fs::path(folder) / "graph.json").string());
  std::ifstream in(fs::path(folder) / "params.json");
  ojson meta = ojson::parse(in);
  Values v;
  for (auto& [k, x] : meta.at("params").items()) v[k] = x.get<double>();
  Design d(g, v, meta.at("created_at").get<std::string>());
  if (d.hash() != meta.at("hash").get<std::string>()) throw std::runtime_error("design in " + folder + " does not match its stored hash");
  return d;
}

std::string families_dir() {
  if (const char* env = std::getenv("STRUCTURA_FAMILIES")) return env;
  return (fs::path(STRUCTURA_ROOT) / "families").string();
}

DesignGraph load_family(const std::string& name) {
  fs::path p = fs::path(families_dir()) / (name + ".json");
  if (!fs::exists(p)) throw GraphError("no part family '" + name + "' in " + families_dir());
  DesignGraph g = DesignGraph::load(p.string());
  auto used = g.referenced_names();
  std::string unused;
  for (auto& [k, _] : g.parameters)
    if (!used.count(k)) unused += (unused.empty() ? "" : ", ") + k;
  if (!unused.empty()) throw GraphError("family '" + name + "' declares parameters it never uses: " + unused);
  return g;
}

}  // namespace structura
