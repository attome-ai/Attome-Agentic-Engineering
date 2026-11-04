#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "structura/design.hpp"
#include "structura/mesh.hpp"
#include "structura/occ.hpp"
#include "structura/process.hpp"

namespace fs = std::filesystem;

namespace structura {
namespace {

struct Rung {
  const char* algo;
  int code;
  double scale;
};
const Rung kLadder[] = {{"hxt", 10, 1.0}, {"delaunay", 1, 1.0}, {"frontal", 4, 1.0}, {"delaunay", 1, 1.5}};

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

std::string num(double v) {
  std::ostringstream o;
  o.precision(12);
  o << v;
  return o.str();
}

std::string geo_text(const std::string& step, double size, int algo, const MeshOptions& o,
                     const std::vector<std::pair<std::string, std::vector<std::vector<double>>>>& groups) {
  std::ostringstream g;
  g << "SetFactory(\"OpenCASCADE\");\n"
    << "Geometry.OCCImportLabels = 0;\n"
    << "v() = ShapeFromFile(\"" << step << "\");\n"
    << "Mesh.Algorithm3D = " << algo << ";\n"
    << "Mesh.ElementOrder = " << o.order << ";\n"
    << "Mesh.SecondOrderLinear = 0;\n"
    << "Mesh.HighOrderOptimize = 0;\n"
    << "Mesh.MeshSizeMax = " << num(size) << ";\n"
    << "Mesh.MeshSizeMin = " << num(size * o.size_min_factor) << ";\n"
    << "Mesh.MeshSizeFromCurvature = " << o.curvature_points << ";\n"
    << "Mesh.SaveGroupsOfNodes = 1;\n"
    << "Mesh.Optimize = 1;\n"
    << "e = 1e-4;\n"
    << "Physical Volume(\"EALL\") = Volume{:};\n";
  for (size_t gi = 0; gi < groups.size(); ++gi) {
    std::string parts;
    for (size_t bi = 0; bi < groups[gi].second.size(); ++bi) {
      auto& b = groups[gi].second[bi];
      std::string var = "g" + std::to_string(gi) + "_" + std::to_string(bi);
      g << var << "[] = Surface In BoundingBox{" << num(b[0]) << " - e, " << num(b[1]) << " - e, " << num(b[2]) << " - e, "
        << num(b[3]) << " + e, " << num(b[4]) << " + e, " << num(b[5]) << " + e};\n";
      parts += (parts.empty() ? "" : ", ") + var + "[]";
    }
    g << "Physical Surface(\"" << groups[gi].first << "\") = {" << parts << "};\n";
  }
  return g.str();
}

std::string tail(const std::string& s, size_t n = 2000) { return s.size() > n ? s.substr(s.size() - n) : s; }

}  // namespace

double tri_area(const std::map<int, std::array<double, 3>>& nodes, const std::vector<int>& t) {
  auto& a = nodes.at(t[1]);
  auto& b = nodes.at(t[2]);
  auto& c = nodes.at(t[3]);
  double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  double n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
  return 0.5 * std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
}

double tet_quality(const std::map<int, std::array<double, 3>>& nodes, const std::vector<int>& t) {
  const std::array<double, 3>* p[4] = {&nodes.at(t[1]), &nodes.at(t[2]), &nodes.at(t[3]), &nodes.at(t[4])};
  double a[3], b[3], c[3];
  for (int k = 0; k < 3; ++k) {
    a[k] = (*p[1])[k] - (*p[0])[k];
    b[k] = (*p[2])[k] - (*p[0])[k];
    c[k] = (*p[3])[k] - (*p[0])[k];
  }
  double vol = std::fabs(a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6;
  static const int E[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
  double l2 = 0;
  for (auto& e : E)
    for (int k = 0; k < 3; ++k) l2 += std::pow((*p[e[0]])[k] - (*p[e[1]])[k], 2);
  double lrms = std::sqrt(l2 / 6);
  return lrms > 0 ? 6 * std::sqrt(2.0) * vol / std::pow(lrms, 3) : 0.0;
}

Mesh mesh_shape(const TopoDS_Shape& shape, const std::map<std::string, std::vector<TopoDS_Face>>& named,
                const std::string& workdir, const MeshOptions& opt) {
  fs::create_directories(workdir);
  fs::path step = fs::absolute(fs::path(workdir) / "part.step");
  write_step(shape, step.string());
  std::vector<std::pair<std::string, std::vector<std::vector<double>>>> groups;
  for (auto& [name, faces] : named) {
    if (faces.empty()) continue;
    std::vector<std::vector<double>> boxes;
    for (auto& f : faces) boxes.push_back(bbox(f));
    groups.emplace_back(name, boxes);
  }
  std::string gmsh = find_exe("gmsh");
  nlohmann::json attempts = nlohmann::json::array();
  const std::string vol_type = opt.order == 2 ? "C3D10" : "C3D4";
  const std::string tri_type = opt.order == 2 ? "CPS6" : "CPS3";
  for (const auto& rung : kLadder) {
    double size = opt.size * rung.scale;
    fs::path out = fs::path(workdir) / "mesh.inp";
    std::error_code ec;
    fs::remove(out, ec);
    std::ofstream(fs::path(workdir) / "mesh.geo") << geo_text(step.generic_string(), size, rung.code, opt, groups);
    ProcessResult pr = run_process({gmsh, "mesh.geo", "-3", "-format", "inp", "-o", "mesh.inp", "-nt", std::to_string(opt.threads)},
                                   workdir, opt.timeout_s);
    nlohmann::json att = {{"algorithm", rung.algo}, {"size", size}, {"seconds", std::round(pr.seconds * 100) / 100}};
    bool ok = !pr.timed_out && pr.exit_code == 0 && fs::exists(out) && fs::file_size(out) > 0 &&
              pr.output.find("Error") == std::string::npos;
    if (!ok) {
      att["ok"] = false;
      att["log"] = pr.timed_out ? std::string("timed out") : tail(pr.output);
      attempts.push_back(att);
      continue;
    }
    std::ifstream in(out);
    std::stringstream ss;
    ss << in.rdbuf();
    InpData d = parse_inp(ss.str());
    Mesh m;
    for (auto& b : d.blocks)
      if (upper(b.type) == vol_type) m.tets.insert(m.tets.end(), b.rows.begin(), b.rows.end());
    if (m.tets.empty()) {
      att["ok"] = false;
      att["log"] = "no volume elements written";
      attempts.push_back(att);
      continue;
    }
    std::set<int> used;
    for (auto& t : m.tets) used.insert(t.begin() + 1, t.end());
    for (auto& [id, c] : d.nodes)
      if (used.count(id)) m.nodes[id] = c;
    std::vector<const std::vector<int>*> tris;
    for (auto& b : d.blocks)
      if (upper(b.type) == tri_type)
        for (auto& r : b.rows) tris.push_back(&r);
    nlohmann::json checks = nlohmann::json::object();
    std::vector<std::string> bad;
    for (auto& [name, faces] : named) {
      auto it = d.nsets.find(upper(name));
      std::set<int> ids;
      if (it != d.nsets.end()) ids.insert(it->second.begin(), it->second.end());
      auto& surf = m.surfaces[name];
      for (auto* t : tris)
        if (ids.count((*t)[1]) && ids.count((*t)[2]) && ids.count((*t)[3])) surf.push_back(*t);
      double want = 0, got = 0;
      for (auto& f : faces) want += area(f);
      for (auto& t : surf) got += tri_area(m.nodes, t);
      bool good = want > 0 && std::fabs(got - want) <= opt.area_tol * want;
      checks[name] = {{"faces", faces.size()}, {"expected_mm2", std::round(want * 1000) / 1000},
                      {"meshed_mm2", std::round(got * 1000) / 1000}, {"ok", good}};
      if (!good) bad.push_back(name);
      m.nsets[name] = std::vector<int>(ids.begin(), ids.end());
    }
    double qmin = 1e300, qsum = 0;
    for (auto& t : m.tets) {
      double q = tet_quality(m.nodes, t);
      qmin = std::min(qmin, q);
      qsum += q;
    }
    att.update({{"ok", true}, {"nodes", m.nodes.size()}, {"elements", m.tets.size()}, {"dof", m.dof()},
                {"min_quality", qmin}, {"mean_quality", qsum / m.tets.size()}});
    attempts.push_back(att);
    m.report = {{"attempts", attempts}, {"algorithm", rung.algo}, {"size", size}, {"order", opt.order},
                {"nodes", m.nodes.size()}, {"elements", m.tets.size()}, {"dof", m.dof()}, {"min_quality", qmin},
                {"mean_quality", qsum / m.tets.size()}, {"groups", checks}};
    if (!bad.empty()) {
      std::string names;
      for (auto& b : bad) names += (names.empty() ? "" : ", ") + b;
      throw MeshError("named faces [" + names + "] do not match their meshed surfaces (area check)", attempts);
    }
    return m;
  }
  throw MeshError("meshing failed after " + std::to_string(attempts.size()) + " attempts", attempts);
}

}  // namespace structura
