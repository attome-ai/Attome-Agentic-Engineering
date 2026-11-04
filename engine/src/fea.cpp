#include "structura/fea.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

#include "structura/process.hpp"
#include "fea_detail.hpp"

namespace fs = std::filesystem;

namespace structura {
namespace detail {

std::string num(double v) {
  char b[64];
  std::snprintf(b, sizeof b, "%.12g", v);
  return b;
}

std::string set_name(const std::string& g) {
  std::string s;
  for (char c : g) s += std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : '_';
  return s;
}

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

double tet_volume(const Mesh& m, const std::vector<int>& t) {
  auto& a = m.nodes.at(t[1]);
  auto& b = m.nodes.at(t[2]);
  auto& c = m.nodes.at(t[3]);
  auto& d = m.nodes.at(t[4]);
  double u[3], v[3], w[3];
  for (int k = 0; k < 3; ++k) { u[k] = b[k] - a[k]; v[k] = c[k] - a[k]; w[k] = d[k] - a[k]; }
  return std::fabs(u[0] * (v[1] * w[2] - v[2] * w[1]) - u[1] * (v[0] * w[2] - v[2] * w[0]) + u[2] * (v[0] * w[1] - v[1] * w[0])) / 6;
}

// CalculiX C3D10 faces by local corner nodes (1-based).
extern const int kFaces[4][3];
const int kFaces[4][3] = {{1, 2, 3}, {1, 4, 2}, {2, 4, 3}, {3, 4, 1}};

std::vector<std::pair<int, int>> element_faces(const Mesh& m, const std::string& group) {
  std::map<std::array<int, 3>, std::pair<int, int>> by_face;
  for (auto& t : m.tets)
    for (int f = 0; f < 4; ++f) {
      std::array<int, 3> k = {t[kFaces[f][0]], t[kFaces[f][1]], t[kFaces[f][2]]};
      std::sort(k.begin(), k.end());
      by_face[k] = {t[0], f + 1};
    }
  std::vector<std::pair<int, int>> out;
  for (auto& tri : m.surfaces.at(group)) {
    std::array<int, 3> k = {tri[1], tri[2], tri[3]};
    std::sort(k.begin(), k.end());
    auto it = by_face.find(k);
    if (it == by_face.end()) throw std::runtime_error("surface triangle " + std::to_string(tri[0]) + " of " + group + " has no tet face");
    out.push_back(it->second);
  }
  return out;
}

void write_id_list(std::ostream& d, const std::vector<int>& ids) {
  for (size_t i = 0; i < ids.size(); ++i) d << ids[i] << ((i + 1) % 16 == 0 || i + 1 == ids.size() ? "\n" : ",");
}

std::vector<int> write_nset(std::ostream& d, const Mesh& m, const std::string& group) {
  auto it = m.nsets.find(group);
  if (it == m.nsets.end() || it->second.empty()) throw std::runtime_error("group '" + group + "' has no nodes");
  d << "*NSET,NSET=" << set_name(group) << "\n";
  write_id_list(d, it->second);
  return it->second;
}

void write_nodes_elements(std::ostream& d, const Mesh& m, const std::string& element_type) {
  d << "*NODE\n";
  for (auto& [id, c] : m.nodes) d << id << "," << num(c[0]) << "," << num(c[1]) << "," << num(c[2]) << "\n";
  d << "*ELEMENT,TYPE=" << element_type << ",ELSET=EALL\n";
  for (auto& t : m.tets)
    for (size_t i = 0; i < t.size(); ++i) d << t[i] << (i + 1 < t.size() ? (i == 7 ? ",\n" : ",") : "\n");
}

}  // namespace detail
using namespace detail;

Material load_material(const std::string& id) {
  fs::path p = fs::path(STRUCTURA_ROOT) / "data" / "materials.json";
  std::ifstream in(p);
  if (!in) throw std::runtime_error("cannot open " + p.string());
  auto j = nlohmann::json::parse(in);
  if (!j["materials"].contains(id)) {
    std::string known;
    for (auto& [k, _] : j["materials"].items()) known += (known.empty() ? "" : ", ") + k;
    throw std::runtime_error("unknown material '" + id + "'; known: " + known);
  }
  auto& m = j["materials"][id];
  return {id, m.value("name", id), m.value("source", ""), m.at("E").get<double>(), m.at("nu").get<double>(),
          m.at("density").get<double>(), m.value("yield", 0.0), m.value("k", 0.0), m.value("verify", true)};
}

LoadCase LoadCase::from_json(const nlohmann::json& j) {
  LoadCase lc;
  lc.name = j.value("name", std::string("service"));
  for (auto& s : j.value("supports", nlohmann::json::array())) lc.supports.push_back({s.at("group"), s.value("dofs", std::string("123"))});
  for (auto& l : j.value("loads", nlohmann::json::array())) {
    Load x;
    x.group = l.at("group");
    x.kind = l.value("kind", std::string("force"));
    if (x.kind == "force") x.force = l.at("force").get<std::array<double, 3>>();
    else if (x.kind == "pressure") x.pressure = l.at("pressure").get<double>();
    else throw std::runtime_error("load kind must be force or pressure, got " + x.kind);
    lc.loads.push_back(x);
  }
  if (lc.supports.empty()) throw std::runtime_error("load case '" + lc.name + "' has no supports");
  if (lc.loads.empty()) throw std::runtime_error("load case '" + lc.name + "' has no loads");
  return lc;
}

std::map<int, std::array<double, 3>> distribute_force(const Mesh& m, const std::string& group, const std::array<double, 3>& total) {
  auto it = m.surfaces.find(group);
  if (it == m.surfaces.end() || it->second.empty()) throw std::runtime_error("load group '" + group + "' has no meshed faces");
  double atot = 0;
  for (auto& t : it->second) atot += tri_area(m.nodes, t);
  std::map<int, std::array<double, 3>> f;
  for (auto& t : it->second) {
    if (t.size() < 7) throw std::runtime_error("force distribution needs quadratic (6-node) surface triangles");
    double share = tri_area(m.nodes, t) / atot / 3.0;
    for (int k = 4; k <= 6; ++k)
      for (int c = 0; c < 3; ++c) f[t[k]][c] += total[c] * share;
  }
  return f;
}

std::string write_deck(const Mesh& m, const Material& mat, const LoadCase& lc, std::map<int, std::array<double, 3>>* applied) {
  std::ostringstream d;
  d << "** Attome AE engine deck: " << lc.name << "\n*NODE\n";
  for (auto& [id, c] : m.nodes) d << id << "," << num(c[0]) << "," << num(c[1]) << "," << num(c[2]) << "\n";
  d << "*ELEMENT,TYPE=C3D10,ELSET=EALL\n";
  for (auto& t : m.tets) {
    for (size_t i = 0; i < t.size(); ++i) d << t[i] << (i + 1 < t.size() ? (i == 7 ? ",\n" : ",") : "\n");
  }
  std::set<int> support_nodes;
  for (auto& s : lc.supports) {
    auto it = m.nsets.find(s.group);
    if (it == m.nsets.end() || it->second.empty()) throw std::runtime_error("support group '" + s.group + "' has no nodes");
    d << "*NSET,NSET=" << set_name(s.group) << "\n";
    for (size_t i = 0; i < it->second.size(); ++i) d << it->second[i] << ((i + 1) % 16 == 0 || i + 1 == it->second.size() ? "\n" : ",");
    support_nodes.insert(it->second.begin(), it->second.end());
  }
  d << "*NSET,NSET=SUPPORTS\n";
  {
    size_t i = 0;
    for (int n : support_nodes) d << n << ((++i) % 16 == 0 || i == support_nodes.size() ? "\n" : ",");
  }
  std::map<int, std::array<double, 3>> forces;
  std::vector<std::pair<std::string, std::vector<std::pair<int, int>>>> pressure_faces;
  for (auto& l : lc.loads) {
    if (l.kind == "force") {
      for (auto& [n, f] : distribute_force(m, l.group, l.force))
        for (int c = 0; c < 3; ++c) forces[n][c] += f[c];
    } else {
      pressure_faces.emplace_back(set_name(l.group) + "_P", element_faces(m, l.group));
    }
  }
  for (auto& [name, faces] : pressure_faces) {
    d << "*SURFACE,NAME=" << name << ",TYPE=ELEMENT\n";
    for (auto& [e, f] : faces) d << e << ",S" << f << "\n";
  }
  d << "*MATERIAL,NAME=MAT\n*ELASTIC\n" << num(mat.E) << "," << num(mat.nu) << "\n*DENSITY\n" << num(mat.density) << "\n";
  d << "*SOLID SECTION,ELSET=EALL,MATERIAL=MAT\n*BOUNDARY\n";
  for (auto& s : lc.supports)
    for (char c : s.dofs) d << set_name(s.group) << "," << c << "," << c << "\n";
  d << "*STEP\n*STATIC\n";
  if (!forces.empty()) {
    d << "*CLOAD\n";
    for (auto& [n, f] : forces)
      for (int c = 0; c < 3; ++c)
        if (std::fabs(f[c]) > 0) d << n << "," << (c + 1) << "," << num(f[c]) << "\n";
  }
  size_t pi = 0;
  for (auto& l : lc.loads)
    if (l.kind == "pressure") d << "*DSLOAD\n" << pressure_faces[pi++].first << ",P," << num(l.pressure) << "\n";
  d << "*NODE FILE\nU\n*EL FILE\nS\n*NODE PRINT,NSET=SUPPORTS,TOTALS=ONLY\nRF\n*END STEP\n";
  if (applied) *applied = forces;
  return d.str();
}

std::map<int, std::vector<double>> read_frd_block(const std::string& path, const std::string& name) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::map<int, std::vector<double>> out, cur;
  bool inside = false;
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind(" -4", 0) == 0) {
      std::istringstream ss(line.substr(3));
      std::string n;
      ss >> n;
      inside = n == name;
      cur.clear();
      continue;
    }
    if (!inside) continue;
    if (line.rfind(" -1", 0) == 0) {
      int node = std::stoi(line.substr(3, 10));
      std::vector<double> v;
      for (size_t pos = 13; pos + 12 <= line.size(); pos += 12) v.push_back(std::stod(line.substr(pos, 12)));
      cur[node] = v;
    } else if (line.rfind(" -3", 0) == 0) {
      out = cur;
      inside = false;
    }
  }
  if (out.empty()) throw std::runtime_error("no " + name + " block in " + path);
  return out;
}

double von_mises(const std::vector<double>& s) {
  double a = s[0] - s[1], b = s[1] - s[2], c = s[2] - s[0];
  return std::sqrt(0.5 * (a * a + b * b + c * c) + 3 * (s[3] * s[3] + s[4] * s[4] + s[5] * s[5]));
}

Result solve_static(const Mesh& m, const Material& mat, const LoadCase& lc, const std::string& workdir, const SolveOptions& o) {
  fs::create_directories(workdir);
  std::map<int, std::array<double, 3>> applied;
  std::ofstream(fs::path(workdir) / "job.inp") << write_deck(m, mat, lc, &applied);
  std::string t = std::to_string(o.threads);
  ProcessResult pr = run_process({find_exe("ccx"), "-i", "job"}, workdir, o.timeout_s,
                                 {{"OMP_NUM_THREADS", t}, {"CCX_NPROC_EQUATION_SOLVER", t}, {"CCX_NPROC_STIFFNESS", t}});
  if (pr.timed_out) throw std::runtime_error("CalculiX timed out after " + num(o.timeout_s) + " s");
  bool bad = pr.exit_code != 0 || pr.output.find("*ERROR") != std::string::npos;
  if (bad) throw std::runtime_error("CalculiX failed (exit " + std::to_string(pr.exit_code) + "): " + pr.output.substr(pr.output.size() > 2000 ? pr.output.size() - 2000 : 0));
  Result r;
  r.load_case = lc.name;
  r.runtime_s = pr.seconds;
  std::smatch vm;
  if (std::regex_search(pr.output, vm, std::regex("Version ([0-9.]+)"))) r.solver_version = vm[1];
  std::string frd = (fs::path(workdir) / "job.frd").string();
  for (auto& [n, v] : read_frd_block(frd, "DISP")) {
    r.disp[n] = {v[0], v[1], v[2]};
    r.max_disp_mm = std::max(r.max_disp_mm, std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
  }
  for (auto& [n, v] : read_frd_block(frd, "STRESS")) {
    double s = von_mises(v);
    r.von_mises[n] = s;
    r.stress[n] = {v[0], v[1], v[2], v[3], v[4], v[5]};
    r.max_von_mises_MPa = std::max(r.max_von_mises_MPa, s);
  }
  for (auto& [_, f] : applied)
    for (int c = 0; c < 3; ++c) r.applied_N[c] += f[c];
  for (auto& l : lc.loads)  // pressure resultant: -p * outward area vector, outward taken from the owning tet
    if (l.kind == "pressure") {
      std::map<std::array<int, 3>, int> opposite;  // sorted face corners -> the tet's fourth corner
      for (auto& tt : m.tets)
        for (int f = 0; f < 4; ++f) {
          std::array<int, 3> k = {tt[kFaces[f][0]], tt[kFaces[f][1]], tt[kFaces[f][2]]};
          std::sort(k.begin(), k.end());
          int other = 0;
          for (int q = 1; q <= 4; ++q)
            if (q != kFaces[f][0] && q != kFaces[f][1] && q != kFaces[f][2]) other = tt[q];
          opposite[k] = other;
        }
      for (auto& tri : m.surfaces.at(l.group)) {
        auto& a = m.nodes.at(tri[1]);
        auto& b = m.nodes.at(tri[2]);
        auto& c = m.nodes.at(tri[3]);
        double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        double n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        std::array<int, 3> k = {tri[1], tri[2], tri[3]};
        std::sort(k.begin(), k.end());
        auto& o = m.nodes.at(opposite.at(k));
        double inward = n[0] * (o[0] - a[0]) + n[1] * (o[1] - a[1]) + n[2] * (o[2] - a[2]);
        double sgn = inward > 0 ? -1.0 : 1.0;  // make n outward
        for (int q = 0; q < 3; ++q) r.applied_N[q] += -l.pressure * 0.5 * sgn * n[q];
      }
    }
  std::string dat = read_file(fs::path(workdir) / "job.dat");
  std::smatch tm;
  if (std::regex_search(dat, tm, std::regex("total force \\(fx,fy,fz\\) for set SUPPORTS[^\\n]*\\n\\s*\\n?\\s*([-+0-9.Ee]+)\\s+([-+0-9.Ee]+)\\s+([-+0-9.Ee]+)")))
    r.reaction_N = {std::stod(tm[1]), std::stod(tm[2]), std::stod(tm[3])};
  else
    throw std::runtime_error("no reaction totals in job.dat");
  double fa = std::sqrt(r.applied_N[0] * r.applied_N[0] + r.applied_N[1] * r.applied_N[1] + r.applied_N[2] * r.applied_N[2]);
  double s0 = r.applied_N[0] + r.reaction_N[0], s1 = r.applied_N[1] + r.reaction_N[1], s2 = r.applied_N[2] + r.reaction_N[2];
  r.equilibrium_error = fa > 0 ? std::sqrt(s0 * s0 + s1 * s1 + s2 * s2) / fa : 0;
  for (auto& t : m.tets) r.volume_mm3 += tet_volume(m, t);
  r.mass_t = r.volume_mm3 * mat.density;
  r.mesh_report = m.report;
  return r;
}

nlohmann::json Result::summary() const {
  return {{"mass_t", mass_t}, {"mass_g", mass_t * 1e6}, {"volume_mm3", volume_mm3}, {"max_von_mises_MPa", max_von_mises_MPa},
          {"max_disp_mm", max_disp_mm}, {"applied_N", applied_N}, {"reaction_N", reaction_N},
          {"equilibrium_error", equilibrium_error}, {"solver", solver}, {"solver_version", solver_version},
          {"load_case", load_case}, {"runtime_s", runtime_s}, {"nodes", disp.size()},
          {"units", {{"length", "mm"}, {"force", "N"}, {"stress", "MPa"}, {"mass", "tonne"}, {"time", "s"}}}};
}

void Result::save(const std::string& dir) const {
  fs::create_directories(dir);
  nlohmann::json j = summary();
  j["mesh_report"] = mesh_report;
  std::ofstream(fs::path(dir) / "result.json") << j.dump(2);
  std::ofstream f(fs::path(dir) / "fields.bin", std::ios::binary);
  const char magic[4] = {'S', 'F', 'L', 'D'};
  f.write(magic, 4);
  uint32_t n = static_cast<uint32_t>(disp.size());
  f.write(reinterpret_cast<const char*>(&n), 4);
  for (auto& [id, u] : disp) {
    int32_t i = id;
    double vmv = von_mises.count(id) ? von_mises.at(id) : 0.0;
    f.write(reinterpret_cast<const char*>(&i), 4);
    f.write(reinterpret_cast<const char*>(u.data()), 24);
    f.write(reinterpret_cast<const char*>(&vmv), 8);
  }
}

Result Result::load(const std::string& dir) {
  Result r;
  std::ifstream in(fs::path(dir) / "result.json");
  auto j = nlohmann::json::parse(in);
  r.mass_t = j.at("mass_t");
  r.volume_mm3 = j.at("volume_mm3");
  r.max_von_mises_MPa = j.at("max_von_mises_MPa");
  r.max_disp_mm = j.at("max_disp_mm");
  r.applied_N = j.at("applied_N");
  r.reaction_N = j.at("reaction_N");
  r.equilibrium_error = j.at("equilibrium_error");
  r.solver = j.at("solver");
  r.solver_version = j.at("solver_version");
  r.load_case = j.at("load_case");
  r.runtime_s = j.at("runtime_s");
  r.mesh_report = j.value("mesh_report", nlohmann::json());
  std::ifstream f(fs::path(dir) / "fields.bin", std::ios::binary);
  char magic[4];
  f.read(magic, 4);
  if (std::string(magic, 4) != "SFLD") throw std::runtime_error("bad fields.bin in " + dir);
  uint32_t n = 0;
  f.read(reinterpret_cast<char*>(&n), 4);
  for (uint32_t k = 0; k < n; ++k) {
    int32_t i;
    std::array<double, 3> u;
    double vmv;
    f.read(reinterpret_cast<char*>(&i), 4);
    f.read(reinterpret_cast<char*>(u.data()), 24);
    f.read(reinterpret_cast<char*>(&vmv), 8);
    r.disp[i] = u;
    r.von_mises[i] = vmv;
  }
  return r;
}

}  // namespace structura
