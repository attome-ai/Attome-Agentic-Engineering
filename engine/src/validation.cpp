#include "structura/validation.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <sstream>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Ax2.hxx>
#include <gp_Elips.hxx>

#include "structura/compile.hpp"
#include "structura/fea.hpp"
#include "structura/mesh.hpp"
#include "structura/occ.hpp"

namespace fs = std::filesystem;

namespace structura {
namespace {

using clk = std::chrono::steady_clock;

// Box [0,lx] x [0,ly] x [0,lz] with its six faces named xmin, xmax, ymin, ymax, zmin, zmax.
CompiledPart box_part(double lx, double ly, double lz) {
  ojson j = {{"name", "box"},
             {"features", ojson::array({{{"id", "b"}, {"type", "plate"},
                                         {"params", {{"x", 0}, {"y", 0}, {"z", 0}, {"length", lx}, {"width", ly}, {"thickness", lz}}}}})},
             {"faces", {{"xmin", {{"kind", "plane"}, {"normal", "-x"}, {"at", 0}}}, {"xmax", {{"kind", "plane"}, {"normal", "x"}, {"at", lx}}},
                        {"ymin", {{"kind", "plane"}, {"normal", "-y"}, {"at", 0}}}, {"ymax", {{"kind", "plane"}, {"normal", "y"}, {"at", ly}}},
                        {"zmin", {{"kind", "plane"}, {"normal", "-z"}, {"at", 0}}}, {"zmax", {{"kind", "plane"}, {"normal", "z"}, {"at", lz}}}}}};
  return compile_graph(DesignGraph::from_json(j));
}

Mesh mesh_of(const CompiledPart& p, double size, const std::string& dir) {
  MeshOptions o;
  o.size = size;
  o.threads = 4;
  return mesh_shape(p.shape, p.named_faces(), dir, o);
}

// Quarter elliptic annulus (inner 2 x 1, outer 3.25 x 2.75) extruded between z0 and z1, optionally split at z = 0.
TopoDS_Shape elliptic_quarter(double z0, double z1, bool split_at_zero) {
  gp_Ax2 ax(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
  TopoDS_Face fo = BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Elips(ax, 3.25, 2.75)).Edge()).Wire());
  TopoDS_Face fi = BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Elips(ax, 2.0, 1.0)).Edge()).Wire());
  TopoDS_Shape ann = BRepAlgoAPI_Cut(fo, fi).Shape();
  TopoDS_Shape quad = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, -1), 4, 4, 2).Shape();
  TopoDS_Shape q = BRepAlgoAPI_Common(ann, quad).Shape();
  TopoDS_Face face = faces_of(q).at(0);
  auto prism = [&](double a, double b) {
    TopoDS_Shape f = face;
    gp_Trsf t;
    t.SetTranslation(gp_Vec(0, 0, a));
    f = f.Moved(TopLoc_Location(t));
    return BRepPrimAPI_MakePrism(f, gp_Vec(0, 0, b - a)).Shape();
  };
  if (!split_at_zero) return prism(z0, z1);
  return BRepAlgoAPI_Fuse(prism(z0, 0), prism(0, z1)).Shape();  // keeps the edges at z = 0 on the outer face
}

std::map<std::string, std::vector<TopoDS_Face>> classify_annulus(const TopoDS_Shape& s, double ztop, double zbot) {
  std::map<std::string, std::vector<TopoDS_Face>> g;
  for (auto& f : faces_of(s)) {
    auto b = bbox(f);
    bool planar = BRepAdaptor_Surface(f).GetType() == GeomAbs_Plane;
    if (planar) {
      gp_Dir n = normal(f);
      if (n.X() < -0.999 && std::fabs(b[3]) < 1e-6) g["xzero"].push_back(f);
      else if (n.Y() < -0.999 && std::fabs(b[4]) < 1e-6) g["yzero"].push_back(f);
      else if (n.Z() > 0.999 && std::fabs(b[5] - ztop) < 1e-6) g["top"].push_back(f);
      else if (n.Z() < -0.999 && std::fabs(b[2] - zbot) < 1e-6) g["bottom"].push_back(f);
    } else if (b[3] > 3.2) {
      g["outer"].push_back(f);
    }
  }
  return g;
}

int nearest_node(const Mesh& m, double x, double y, double z) {
  int best = -1;
  double bd = 1e300;
  for (auto& [id, c] : m.nodes) {
    double d = (c[0] - x) * (c[0] - x) + (c[1] - y) * (c[1] - y) + (c[2] - z) * (c[2] - z);
    if (d < bd) { bd = d; best = id; }
  }
  return best;
}

ValidationRow finish(ValidationRow r, clk::time_point t0) {
  r.error_pct = 100 * (r.value - r.reference) / std::fabs(r.reference);
  r.pass = std::fabs(r.error_pct) <= r.tolerance_pct;
  r.seconds = std::chrono::duration<double>(clk::now() - t0).count();
  return r;
}

}  // namespace

std::vector<std::string> validation_cases() {
  return {"cantilever_static", "nafems_le10", "nafems_le1", "euler_column", "cantilever_modal", "thermal_slab", "thermal_fin"};
}

ValidationRow run_validation_case(const std::string& id, const std::string& workdir, int threads) {
  auto t0 = clk::now();
  fs::path dir = fs::path(workdir) / id;
  fs::remove_all(dir);
  SolveOptions so;
  so.threads = threads;
  so.timeout_s = 900;
  ValidationRow r;
  r.id = id;
  Material steel = load_material("S235");

  if (id == "cantilever_static" || id == "cantilever_modal") {
    CompiledPart p = box_part(100, 10, 10);
    r.mesh_size = 2.5;
    Mesh m = mesh_of(p, r.mesh_size, (dir / "mesh").string());
    r.dof = m.dof();
    double I = 10.0 * 1000.0 / 12.0, A = 100.0, L = 100.0;
    if (id == "cantilever_static") {
      LoadCase lc;
      lc.supports = {{"xmin", "123"}};
      lc.loads = {{"xmax", "force", {0, 0, -100}, 0}};
      Result res = solve_static(m, steel, lc, (dir / "run").string(), so);
      double tip = 0;
      for (int n : m.nsets.at("xmax")) tip += res.disp.at(n)[2];
      tip /= m.nsets.at("xmax").size();
      double G = steel.E / (2 * (1 + steel.nu));
      r.name = "Cantilever 100 x 10 x 10 mm, 100 N tip load";
      r.quantity = "tip deflection";
      r.unit = "mm";
      r.reference = 100.0 * std::pow(L, 3) / (3 * steel.E * I) + 100.0 * L / ((5.0 / 6.0) * G * A);
      r.reference_source = "Timoshenko beam: PL^3/3EI + PL/(kGA), k = 5/6";
      r.value = -tip;
      r.tolerance_pct = 2;
    } else {
      ModalResult res = solve_modal(m, steel, {{"xmin", "123"}}, 4, (dir / "run").string(), so);
      double beta = 1.8751040687;
      r.name = "Cantilever 100 x 10 x 10 mm, first natural frequency";
      r.quantity = "f1";
      r.unit = "Hz";
      r.reference = beta * beta / (2 * std::numbers::pi) * std::sqrt(steel.E * I / (steel.density * A * std::pow(L, 4)));
      r.reference_source = "Euler-Bernoulli cantilever: (1.875^2 / 2 pi) sqrt(EI / (rho A L^4))";
      r.value = res.frequencies_hz.at(0);
      r.tolerance_pct = 3;
    }
    return finish(r, t0);
  }

  if (id == "euler_column") {
    CompiledPart p = box_part(200, 10, 10);
    r.mesh_size = 3;
    Mesh m = mesh_of(p, r.mesh_size, (dir / "mesh").string());
    r.dof = m.dof();
    LoadCase lc;
    lc.supports = {{"xmin", "123"}};
    lc.loads = {{"xmax", "force", {-1000, 0, 0}, 0}};
    BuckleResult res = solve_buckle(m, steel, lc, 2, (dir / "run").string(), so);
    double I = 10.0 * 1000.0 / 12.0, L = 200.0;
    r.name = "Fixed-free column 200 x 10 x 10 mm, 1000 N axial";
    r.quantity = "buckling load factor";
    r.unit = "1";
    r.reference = std::pow(std::numbers::pi, 2) * steel.E * I / (4 * L * L) / 1000.0;
    r.reference_source = "Euler: P_cr = pi^2 EI / (2L)^2 for a fixed-free column";
    r.value = res.factors.at(0);
    r.tolerance_pct = 3;
    return finish(r, t0);
  }

  if (id == "nafems_le10") {
    TopoDS_Shape s = elliptic_quarter(-0.3, 0.3, true);
    r.mesh_size = 0.075;
    MeshOptions o;
    o.size = r.mesh_size;
    o.threads = 4;
    Mesh m = mesh_shape(s, classify_annulus(s, 0.3, -0.3), (dir / "mesh").string(), o);
    std::vector<int> mid;
    for (int n : m.nsets.at("outer"))
      if (std::fabs(m.nodes.at(n)[2]) < 1e-9) mid.push_back(n);
    m.nsets["midline"] = mid;
    r.dof = m.dof();
    LoadCase lc;
    lc.supports = {{"xzero", "1"}, {"yzero", "2"}, {"outer", "12"}, {"midline", "3"}};
    lc.loads = {{"top", "pressure", {0, 0, 0}, 1.0}};
    Material mat = steel;
    mat.E = 210000;
    mat.nu = 0.3;
    Result res = solve_static(m, mat, lc, (dir / "run").string(), so);
    r.name = "NAFEMS LE10 thick plate under 1 MPa pressure";
    r.quantity = "sigma_yy at D (2, 0, 0.3)";
    r.unit = "MPa";
    r.reference = -5.38;
    r.reference_source = "NAFEMS LE10 target";
    r.value = res.stress.at(nearest_node(m, 2.0, 0.0, 0.3))[1];
    r.tolerance_pct = 2;
    return finish(r, t0);
  }

  if (id == "nafems_le1") {
    TopoDS_Shape s = elliptic_quarter(0.0, 0.1, false);
    r.mesh_size = 0.05;
    MeshOptions o;
    o.size = r.mesh_size;
    o.threads = 4;
    Mesh m = mesh_shape(s, classify_annulus(s, 0.1, 0.0), (dir / "mesh").string(), o);
    r.dof = m.dof();
    LoadCase lc;
    lc.supports = {{"xzero", "1"}, {"yzero", "2"}, {"bottom", "3"}};
    lc.loads = {{"outer", "pressure", {0, 0, 0}, -10.0}};  // 10 MPa outward traction
    Material mat = steel;
    mat.E = 210000;
    mat.nu = 0.3;
    Result res = solve_static(m, mat, lc, (dir / "run").string(), so);
    double a = res.stress.at(nearest_node(m, 2.0, 0.0, 0.0))[1], b = res.stress.at(nearest_node(m, 2.0, 0.0, 0.1))[1];
    r.name = "NAFEMS LE1 elliptic membrane, 10 MPa outward on the outer edge";
    r.quantity = "sigma_yy at D (2, 0)";
    r.unit = "MPa";
    r.reference = 92.7;
    r.reference_source = "NAFEMS LE1 target";
    r.value = (a + b) / 2;
    r.tolerance_pct = 3;
    return finish(r, t0);
  }

  if (id == "thermal_slab") {
    CompiledPart p = box_part(10, 10, 10);
    r.mesh_size = 1.5;
    Mesh m = mesh_of(p, r.mesh_size, (dir / "mesh").string());
    r.dof = m.nodes.size();
    ThermalCase tc;
    tc.temperatures = {{"xmin", 100.0}, {"xmax", 0.0}};
    Material al = load_material("AL6061T6");
    ThermalResult res = solve_thermal(m, al, tc, (dir / "run").string(), so);
    double worst = 0, at_mid = 0;
    int nmid = 0;
    for (auto& [n, t] : res.temperature) {
      double x = m.nodes.at(n)[0];
      worst = std::max(worst, std::fabs(t - 100 * (1 - x / 10)));
      if (std::fabs(x - 5) < 0.75) { at_mid += t - 100 * (1 - x / 10) + 50; ++nmid; }
    }
    r.name = "Slab 10 mm, faces held at 100 and 0, no other heat flow";
    r.quantity = "temperature at mid-plane (worst node error " + std::to_string(worst).substr(0, 6) + ")";
    r.unit = "degC";
    r.reference = 50;
    r.reference_source = "1D conduction: linear profile";
    r.value = nmid ? at_mid / nmid : 0;
    r.tolerance_pct = 0.5;
    return finish(r, t0);
  }

  if (id == "thermal_fin") {
    double L = 50, t = 2, w = 20, h = 1e-5;  // h = 10 W/(m^2 K)
    CompiledPart p = box_part(L, t, w);
    r.mesh_size = 1.0;
    Mesh m = mesh_of(p, r.mesh_size, (dir / "mesh").string());
    r.dof = m.nodes.size();
    Material al = load_material("AL6061T6");
    ThermalCase tc;
    tc.temperatures = {{"xmin", 100.0}};
    tc.films = {{"ymin", h, 0}, {"ymax", h, 0}, {"zmin", h, 0}, {"zmax", h, 0}};
    ThermalResult res = solve_thermal(m, al, tc, (dir / "run").string(), so);
    double tip = 0;
    for (int n : m.nsets.at("xmax")) tip += res.temperature.at(n);
    tip /= m.nsets.at("xmax").size();
    double P = 2 * (w + t), A = w * t, mL = std::sqrt(h * P / (al.k * A)) * L;
    r.name = "Aluminium fin 50 x 2 x 20 mm, base 100, film 10 W/m^2K to 0, insulated tip";
    r.quantity = "tip temperature";
    r.unit = "degC";
    r.reference = 100 / std::cosh(mL);
    r.reference_source = "1D fin with insulated tip: T_tip = T_b / cosh(mL)";
    r.value = tip;
    r.tolerance_pct = 3;
    return finish(r, t0);
  }
  throw std::runtime_error("unknown validation case " + id);
}

nlohmann::json to_json(const std::vector<ValidationRow>& rows) {
  nlohmann::json out = nlohmann::json::array();
  for (auto& r : rows)
    out.push_back({{"id", r.id}, {"name", r.name}, {"quantity", r.quantity}, {"unit", r.unit}, {"reference", r.reference},
                   {"reference_source", r.reference_source}, {"value", r.value}, {"error_pct", r.error_pct},
                   {"tolerance_pct", r.tolerance_pct}, {"pass", r.pass}, {"dof", r.dof}, {"mesh_size", r.mesh_size},
                   {"seconds", r.seconds}});
  return out;
}

std::string to_markdown(const std::vector<ValidationRow>& rows, const std::string& machine) {
  std::ostringstream o;
  o.precision(5);
  size_t passed = 0;
  for (auto& r : rows) passed += r.pass;
  o << "# Validation report\n\nGenerated by `structura validate`; do not edit by hand. " << machine << "\n\n"
    << "**" << passed << " of " << rows.size() << " cases pass.** Every case runs the real engine path: geometry on "
    << "OpenCASCADE, mesh by gmsh (quadratic tetrahedra), solve by CalculiX.\n\n"
    << "| Case | Quantity | Reference | Engine | Error | Tolerance | Result | DOF | Mesh size | Time (s) |\n"
    << "|---|---|---|---|---|---|---|---|---|---|\n";
  for (auto& r : rows) {
    char err[32], sec[32];
    std::snprintf(err, sizeof err, "%+.2f%%", r.error_pct);
    std::snprintf(sec, sizeof sec, "%.1f", r.seconds);
    o << "| " << r.name << " | " << r.quantity << " (" << r.unit << ") | " << r.reference << " | " << r.value << " | " << err
      << " | " << r.tolerance_pct << "% | " << (r.pass ? "pass" : "**FAIL**") << " | " << r.dof << " | " << r.mesh_size << " | "
      << sec << " |\n";
  }
  o << "\nReferences:\n\n";
  for (auto& r : rows) o << "- **" << r.id << "**: " << r.reference_source << "\n";
  return o.str();
}

}  // namespace structura
