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
  bool straight = false;  // straight-sided quadratic elements (mid-side nodes not moved onto curved surfaces)
};
// Delaunay first: gmsh's default and the most robust. HXT runs single-threaded because with several threads it can
// hang on Windows (found in the validation suite: 120 s timeouts on simple boxes). Curved quadratic elements can fold
// near tight fillets (CalculiX: "nonpositive jacobian"), so the second rung repeats Delaunay with straight sides.
const Rung kLadder[] = {{"delaunay", 1, 1.0, false}, {"delaunay-straight", 1, 1.0, true}, {"hxt", 10, 1.0, true},
                        {"frontal", 4, 1.0, true}, {"delaunay-straight", 1, 1.5, true}};

// Smallest ratio detJ(point) / detJ(straight corner tet) over the corners and the four Gauss points of a quadratic
// tetrahedron (CalculiX/VTK node order). A value <= 0 means the curved element is inverted somewhere.
double quad_tet_jacobian_ratio(const std::map<int, std::array<double, 3>>& nodes, const std::vector<int>& t) {
  if (t.size() < 11) return 1.0;
  const double a = 0.5854101966249685, b = 0.1381966011250105;
  const double pts[8][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1},
                            {a, b, b, b}, {b, a, b, b}, {b, b, a, b}, {b, b, b, a}};
  const int edge[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {1, 3}, {2, 3}};
  double X[10][3];
  for (int i = 0; i < 10; ++i)
    for (int c = 0; c < 3; ++c) X[i][c] = nodes.at(t[1 + i])[c];
  auto det3 = [](const double m[3][3]) {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  };
  double lin[3][3];
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) lin[r][c] = X[r + 1][c] - X[0][c];
  double d0 = det3(lin);
  if (std::fabs(d0) < 1e-300) return 0.0;
  double worst = 1e300;
  for (auto& L : pts) {
    double g[10][4] = {};
    for (int i = 0; i < 4; ++i) g[i][i] = 4 * L[i] - 1;
    for (int e = 0; e < 6; ++e) {
      g[4 + e][edge[e][0]] = 4 * L[edge[e][1]];
      g[4 + e][edge[e][1]] = 4 * L[edge[e][0]];
    }
    double J[3][3] = {};
    for (int r = 0; r < 3; ++r)       // d/dxi_r with xi = (L2, L3, L4), L1 = 1 - sum
      for (int i = 0; i < 10; ++i) {
        double dn = g[i][r + 1] - g[i][0];
        for (int c = 0; c < 3; ++c) J[r][c] += dn * X[i][c];
      }
    worst = std::min(worst, det3(J) / d0);
  }
  return worst;
}

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
                     const std::vector<std::pair<std::string, std::vector<std::vector<double>>>>& groups, bool straight) {
  std::ostringstream g;
  g << "SetFactory(\"OpenCASCADE\");\n"
    << "Geometry.OCCImportLabels = 0;\n"
    << "v() = ShapeFromFile(\"" << step << "\");\n"
    << "Mesh.Algorithm3D = " << algo << ";\n"
    << "Mesh.ElementOrder = " << o.order << ";\n"
    << "Mesh.SecondOrderLinear = " << (straight ? 1 : 0) << ";\n"
    << "Mesh.HighOrderOptimize = 0;\n"
    << "Mesh.MeshSizeMax = " << num(size) << ";\n"
    << "Mesh.MeshSizeMin = " << num(size * o.size_min_factor) << ";\n"
    << "Mesh.MeshSizeFromCurvature = " << o.curvature_points << ";\n"
    << "Mesh.SaveGroupsOfNodes = 1;\n"
    << "Mesh.Optimize = 1;\n"
    << "Physical Volume(\"EALL\") = Volume{:};\n"
    // every surface is its own physical group, so its triangles come out as element set Surface<tag>;
    // named faces are matched to these surfaces afterwards by area and centroid (works for curved faces)
    << "s[] = Surface{:};\n"
    << "For i In {0:#s[]-1}\n  Physical Surface(Sprintf(\"SF%g\", s[i])) = {s[i]};\nEndFor\n";
  (void)groups;
  return g.str();
}

struct SurfStat {
  double area = 0;
  double cx = 0, cy = 0, cz = 0;
  std::vector<const std::vector<int>*> tris;
};

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
    std::ofstream(fs::path(workdir) / "mesh.geo") << geo_text(step.generic_string(), size, rung.code, opt, groups, rung.straight);
    ProcessResult pr = run_process({gmsh, "mesh.geo", "-3", "-format", "inp", "-o", "mesh.inp", "-nt", std::to_string(rung.code == 10 ? 1 : opt.threads)},
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
    // per gmsh surface: triangles, area and area-weighted centroid
    std::map<std::string, SurfStat> surf;
    for (auto& b : d.blocks) {
      if (upper(b.type) != tri_type) continue;
      SurfStat& st = surf[b.elset];
      for (auto& r : b.rows) {
        double a = tri_area(m.nodes, r);
        auto& p = m.nodes.at(r[1]);
        auto& q = m.nodes.at(r[2]);
        auto& s3 = m.nodes.at(r[3]);
        st.area += a;
        st.cx += a * (p[0] + q[0] + s3[0]) / 3;
        st.cy += a * (p[1] + q[1] + s3[1]) / 3;
        st.cz += a * (p[2] + q[2] + s3[2]) / 3;
        st.tris.push_back(&r);
      }
    }
    for (auto& [_, st] : surf)
      if (st.area > 0) { st.cx /= st.area; st.cy /= st.area; st.cz /= st.area; }
    nlohmann::json checks = nlohmann::json::object();
    std::vector<std::string> bad;
    for (auto& [name, faces] : named) {
      auto& out = m.surfaces[name];
      std::set<int> ids;
      double want = 0, got = 0;
      bool good = !faces.empty();
      std::set<std::string> used_surf;
      for (auto& f : faces) {
        double af = area(f);
        gp_Pnt cf = centre(f);
        want += af;
        std::string best;
        double bd = 1e300;
        for (auto& [key, st] : surf) {
          if (used_surf.count(key) || std::fabs(st.area - af) > opt.area_tol * af) continue;
          double dd = std::sqrt((st.cx - cf.X()) * (st.cx - cf.X()) + (st.cy - cf.Y()) * (st.cy - cf.Y()) + (st.cz - cf.Z()) * (st.cz - cf.Z()));
          if (dd < bd) { bd = dd; best = key; }
        }
        if (best.empty() || bd > 0.05 * std::sqrt(af) + 1e-6) { good = false; continue; }
        used_surf.insert(best);
        got += surf[best].area;
        for (auto* t : surf[best].tris) {
          out.push_back(*t);
          ids.insert(t->begin() + 1, t->end());
        }
      }
      good = good && want > 0 && std::fabs(got - want) <= opt.area_tol * want;
      checks[name] = {{"faces", faces.size()}, {"expected_mm2", std::round(want * 1000) / 1000},
                      {"meshed_mm2", std::round(got * 1000) / 1000}, {"ok", good}};
      if (!good) bad.push_back(name);
      m.nsets[name] = std::vector<int>(ids.begin(), ids.end());
    }
    double qmin = 1e300, qsum = 0, jmin = 1e300;
    size_t inverted = 0;
    for (auto& t : m.tets) {
      double q = tet_quality(m.nodes, t);
      qmin = std::min(qmin, q);
      qsum += q;
      double jr = quad_tet_jacobian_ratio(m.nodes, t);
      jmin = std::min(jmin, jr);
      inverted += jr <= 0;
    }
    if (inverted > 0) {  // the solver would reject these elements; try the next rung (straight-sided quadratics)
      att.update({{"ok", false}, {"log", std::to_string(inverted) + " curved elements are inverted (min Jacobian ratio " +
                                             std::to_string(jmin) + ")"}});
      attempts.push_back(att);
      continue;
    }
    att.update({{"ok", true}, {"nodes", m.nodes.size()}, {"elements", m.tets.size()}, {"dof", m.dof()},
                {"min_quality", qmin}, {"mean_quality", qsum / m.tets.size()}, {"min_jacobian_ratio", jmin}});
    attempts.push_back(att);
    m.report = {{"attempts", attempts}, {"algorithm", rung.algo}, {"size", size}, {"order", opt.order},
                {"straight_sided", rung.straight}, {"nodes", m.nodes.size()}, {"elements", m.tets.size()}, {"dof", m.dof()},
                {"min_quality", qmin}, {"mean_quality", qsum / m.tets.size()}, {"min_jacobian_ratio", jmin}, {"groups", checks}};
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
