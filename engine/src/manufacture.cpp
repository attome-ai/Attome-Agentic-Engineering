#include "structura/manufacture.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <random>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <ShapeAnalysis_Surface.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>

#include "structura/fea.hpp"
#include "structura/gate.hpp"
#include "structura/occ.hpp"

namespace fs = std::filesystem;

namespace structura {
namespace {

double val(const nlohmann::json& section, const char* key) { return section.at(key).at("value").get<double>(); }

double dot(const gp_Vec& v, const std::array<double, 3>& d) { return v.X() * d[0] + v.Y() * d[1] + v.Z() * d[2]; }
double dot(const gp_Pnt& p, const std::array<double, 3>& d) { return p.X() * d[0] + p.Y() * d[1] + p.Z() * d[2]; }

std::array<double, 3> unit(std::array<double, 3> d) {
  double m = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (m < 1e-12) throw std::runtime_error("direction has zero length");
  return {d[0] / m, d[1] / m, d[2] / m};
}

// A rule result: margin > 0 means satisfied, as a fraction of the limit.
nlohmann::json rule(const std::string& id, const std::string& what, double value, const std::string& op, double limit, const std::string& unit) {
  double scale = std::abs(limit) > 1e-12 ? std::abs(limit) : 1.0;  // a zero limit (a count) uses the absolute difference
  double margin = op == ">=" ? (value - limit) / scale : (limit - value) / scale;
  return {{"id", id}, {"rule", what}, {"value", value}, {"op", op}, {"limit", limit}, {"unit", unit}, {"margin", margin}, {"passed", margin >= -1e-9}};
}

// outward normal of face f at the 3D point p (orientation-aware), via the point's UV on the surface
gp_Vec normal_at(const TopoDS_Face& f, const gp_Pnt& p) {
  Handle(Geom_Surface) s = BRep_Tool::Surface(f);
  ShapeAnalysis_Surface sas(s);
  gp_Pnt2d uv = sas.ValueOfUV(p, 1e-6);
  BRepGProp_Face prop(f);
  gp_Pnt q;
  gp_Vec n;
  prop.Normal(uv.X(), uv.Y(), q, n);
  double m = n.Magnitude();
  return m > 1e-12 ? n / m : gp_Vec(0, 0, 0);
}

struct CornerScan {
  int sharp_internal_along_tool = 0;            // concave edges with no radius, parallel to an approach direction
  std::optional<double> min_internal_radius;    // smallest concave fillet radius parallel to an approach direction
  int small_internal_radii = 0;                 // such fillets smaller than the tool radius
  int holes = 0;                                // concave cylinders spanning at least half a turn (drilled)
  nlohmann::json sharp_points = nlohmann::json::array();  // midpoint and length of each sharp internal edge
};

CornerScan scan_corners(const TopoDS_Shape& shape, const std::vector<std::array<double, 3>>& approach, double tool_r) {
  CornerScan out;
  auto along_tool = [&](const gp_Vec& t) {
    for (auto& a : approach)
      if (std::abs(dot(t, a)) > 0.9) return true;
    return false;
  };
  // concave fillets and holes: cylindrical faces whose outward normal points towards the axis
  for (auto& f : faces_of(shape)) {
    BRepAdaptor_Surface s(f);
    if (s.GetType() != GeomAbs_Cylinder) continue;
    gp_Cylinder c = s.Cylinder();
    gp_Pnt p = centre(f);
    gp_Vec n(normal(f));
    gp_Vec ax(c.Axis().Direction());
    gp_Vec r(c.Axis().Location(), p);
    r -= ax * r.Dot(ax);
    if (n.Dot(r) >= 0) continue;  // convex (a rounded outside edge)
    if (!along_tool(ax)) continue;
    double span = s.LastUParameter() - s.FirstUParameter();
    if (span >= std::numbers::pi - 1e-2) {
      ++out.holes;
      continue;
    }
    double rad = c.Radius();
    out.min_internal_radius = out.min_internal_radius ? std::min(*out.min_internal_radius, rad) : rad;
    if (rad < tool_r - 1e-6) ++out.small_internal_radii;
  }
  // sharp concave edges: face 2 rises on the outward side of face 1
  TopTools_IndexedDataMapOfShapeListOfShape map;
  TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, map);
  for (int i = 1; i <= map.Extent(); ++i) {
    const TopTools_ListOfShape& fl = map.FindFromIndex(i);
    if (fl.Extent() != 2) continue;
    TopoDS_Edge e = TopoDS::Edge(map.FindKey(i));
    if (BRep_Tool::Degenerated(e)) continue;
    BRepAdaptor_Curve cv(e);
    if (cv.GetType() != GeomAbs_Line) continue;  // straight edges only: internal corners of prismatic features
    double u = 0.5 * (cv.FirstParameter() + cv.LastParameter());
    gp_Pnt p;
    gp_Vec t;
    cv.D1(u, p, t);
    if (t.Magnitude() < 1e-12) continue;
    t.Normalize();
    if (!along_tool(t)) continue;
    TopoDS_Face f1 = TopoDS::Face(fl.First()), f2 = TopoDS::Face(fl.Last());
    gp_Vec n1 = normal_at(f1, p), n2 = normal_at(f2, p);
    if (n1.Magnitude() < 0.5 || n2.Magnitude() < 0.5) continue;
    if (std::abs(n1.Dot(n2)) > 0.995) continue;  // tangent (smooth) junction
    // concave (internal) iff a point just off the edge along n1 - n2 lies inside the material: at an internal corner the
    // material fills three quadrants around the edge, at an external corner only one
    gp_Vec off = n1 - n2;
    off.Normalize();
    gp_Pnt q(p.X() + 0.05 * off.X(), p.Y() + 0.05 * off.Y(), p.Z() + 0.05 * off.Z());
    BRepClass3d_SolidClassifier cls(shape, q, 1e-6);
    if (cls.State() == TopAbs_IN) {
      ++out.sharp_internal_along_tool;
      out.sharp_points.push_back({{"at", {p.X(), p.Y(), p.Z()}}, {"length_mm", edge_length(e)}});
    }
  }
  return out;
}

}  // namespace

nlohmann::json load_processes(const std::string& path) {
  fs::path p = path.empty() ? fs::path(STRUCTURA_ROOT) / "data" / "processes.json" : fs::path(path);
  std::ifstream in(p);
  if (!in) throw std::runtime_error("cannot read " + p.string());
  return nlohmann::json::parse(in);
}

double support_volume(const TopoDS_Shape& shape, const std::array<double, 3>& dir, double max_deg) {
  auto d = unit(dir);
  BRepMesh_IncrementalMesh mesher(shape, 0.1, false, 0.3, true);
  struct Tri { gp_Pnt a, b, c; };
  std::vector<Tri> tris;
  double zmin = 1e300;
  for (auto& f : faces_of(shape)) {
    TopLoc_Location loc;
    Handle(Poly_Triangulation) t = BRep_Tool::Triangulation(f, loc);
    if (t.IsNull()) continue;
    bool rev = f.Orientation() == TopAbs_REVERSED;
    for (int i = 1; i <= t->NbTriangles(); ++i) {
      int n1, n2, n3;
      t->Triangle(i).Get(n1, n2, n3);
      if (rev) std::swap(n2, n3);
      Tri x{t->Node(n1).Transformed(loc), t->Node(n2).Transformed(loc), t->Node(n3).Transformed(loc)};
      for (auto* q : {&x.a, &x.b, &x.c}) zmin = std::min(zmin, dot(*q, d));
      tris.push_back(x);
    }
  }
  double limit = std::cos(max_deg * std::numbers::pi / 180);
  double vol = 0;
  for (auto& x : tris) {
    gp_Vec n = gp_Vec(x.a, x.b).Crossed(gp_Vec(x.a, x.c));
    double a2 = n.Magnitude();
    if (a2 < 1e-12) continue;
    double down = -dot(n, d) / a2;
    if (down <= limit) continue;
    double h = (dot(x.a, d) + dot(x.b, d) + dot(x.c, d)) / 3 - zmin;
    if (h <= 1e-6) continue;  // on the bed
    vol += a2 / 2 * down * h;  // projected area times height above the bed
  }
  return vol;
}

double undercut_area(const TopoDS_Shape& shape, const std::vector<std::array<double, 3>>& approach, int samples, unsigned seed) {
  std::vector<TopoDS_Face> faces;
  std::vector<double> weights;
  double total = 0;
  for (auto& f : faces_of(shape)) {
    double a = area(f);
    if (a > 1e-9) {
      faces.push_back(f);
      weights.push_back(a);
      total += a;
    }
  }
  if (faces.empty()) return 0;
  std::vector<std::array<double, 3>> dirs;
  for (auto& a : approach) dirs.push_back(unit(a));
  std::mt19937 rng(seed);
  std::discrete_distribution<size_t> pick(weights.begin(), weights.end());
  int used = 0, blocked = 0;
  for (int k = 0; k < samples; ++k) {
    const TopoDS_Face& f = faces[pick(rng)];
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    std::uniform_real_distribution<double> du(u0, u1), dv(v0, v1);
    BRepGProp_Face prop(f);
    for (int tries = 0; tries < 50; ++tries) {  // rejection sampling inside the trimmed face
      double u = du(rng), v = dv(rng);
      BRepClass_FaceClassifier cls(f, gp_Pnt2d(u, v), 1e-7);
      if (cls.State() != TopAbs_IN) continue;
      gp_Pnt p;
      gp_Vec n;
      prop.Normal(u, v, p, n);
      double m = n.Magnitude();
      if (m < 1e-12) break;
      n /= m;
      bool reachable = false;
      for (auto& a : dirs) {
        if (dot(n, a) < -1e-3) continue;  // faces away from this tool direction
        gp_Pnt start(p.X() + 1e-2 * n.X(), p.Y() + 1e-2 * n.Y(), p.Z() + 1e-2 * n.Z());
        BRepIntCurveSurface_Inter inter;
        inter.Init(shape, gp_Lin(start, gp_Dir(a[0], a[1], a[2])), 1e-6);
        bool hit = false;
        for (; inter.More(); inter.Next())
          if (inter.W() > 1e-3) { hit = true; break; }
        if (!hit) { reachable = true; break; }
      }
      ++used;
      if (!reachable) ++blocked;
      break;
    }
  }
  return used ? total * blocked / used : 0;
}

double process_cost(const TopoDS_Shape& shape, const std::string& material, const std::array<double, 3>& print_dir,
                    const nlohmann::json& processes, const std::string& process, nlohmann::json* detail) {
  if (process != "fdm" && process != "cnc") throw std::runtime_error("unknown process '" + process + "' (fdm or cnc)");
  const auto& P = processes.at(process);
  auto ok = P.at("materials").at("value").get<std::vector<std::string>>();
  if (std::find(ok.begin(), ok.end(), material) == ok.end()) throw std::runtime_error(process + " is not set up for " + material);
  Material mat = load_material(material);
  double price = processes.at("material_price_per_kg").at(material).at("value");
  double V = volume(shape), hours = 0, kg = 0;
  nlohmann::json d;
  if (process == "fdm") {
    double vs = support_volume(shape, print_dir, val(P, "max_overhang_deg"));
    double printed = V + val(P, "support_fill") * vs;  // mm^3 of plastic deposited
    hours = printed / val(P, "volumetric_rate_mm3_s") / 3600 + val(P, "setup_min") / 60;
    kg = printed * mat.density * 1e3;  // density tonne/mm^3 -> kg
    d = {{"part_volume_mm3", V}, {"support_volume_mm3", vs}, {"printed_volume_mm3", printed}};
  } else {
    double margin = val(P, "stock_margin_mm");
    auto bb = bbox(shape);
    double stock = (bb[3] - bb[0] + 2 * margin) * (bb[4] - bb[1] + 2 * margin) * (bb[5] - bb[2] + 2 * margin);
    double setups = static_cast<double>(P.at("approach_directions").at("value").size());
    hours = (stock - V) / val(P, "material_removal_rate_mm3_s") / 3600 + setups * val(P, "setup_min") / 60;
    kg = stock * mat.density * 1e3;
    d = {{"part_volume_mm3", V}, {"stock_volume_mm3", stock}, {"removed_volume_mm3", stock - V}, {"setups", setups}};
  }
  double cm = kg * price, cmach = hours * val(P, "machine_rate_per_h");
  d["time_h"] = hours;
  d["cost"] = {{"material", cm}, {"machine", cmach}, {"total", cm + cmach}, {"currency", processes.value("currency", "USD")}};
  d["process"] = process;
  if (detail) *detail = d;
  return cm + cmach;
}

nlohmann::json fdm_report(const TopoDS_Shape& shape, const std::string& material, const std::array<double, 3>& print_dir,
                          const nlohmann::json& processes) {
  const auto& P = processes.at("fdm");
  auto bb = bbox(shape);
  std::array<double, 3> ext{bb[3] - bb[0], bb[4] - bb[1], bb[5] - bb[2]};
  auto bv = P.at("build_volume_mm").at("value").get<std::array<double, 3>>();
  std::array<double, 3> se = ext, sb = bv;
  std::sort(se.begin(), se.end());
  std::sort(sb.begin(), sb.end());
  double fit = 0;  // largest extent / build size ratio after sorting both (<= 1 fits in some axis-aligned orientation)
  for (int i = 0; i < 3; ++i) fit = std::max(fit, se[i] / sb[i]);

  GateLimits lim;
  lim.print_direction = unit(print_dir);
  lim.max_overhang_deg = val(P, "max_overhang_deg");
  GateResult g = gate(shape, lim);
  double nozzle = val(P, "nozzle_mm"), min_wall = val(P, "min_wall_nozzles") * nozzle;
  nlohmann::json c;
  process_cost(shape, material, print_dir, processes, "fdm", &c);
  // print orientation: support needed along each axis direction (the strength analysis assumes the spec's direction)
  nlohmann::json orient = nlohmann::json::array();
  std::array<double, 3> best_dir = lim.print_direction;
  double best_vs = c["support_volume_mm3"];
  for (std::array<double, 3> a : {std::array<double, 3>{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}}) {
    double v = support_volume(shape, a, val(P, "max_overhang_deg"));
    orient.push_back({{"direction", a}, {"support_volume_mm3", v}});
    if (v < best_vs - 1e-6) { best_vs = v; best_dir = a; }
  }
  nlohmann::json rules = nlohmann::json::array();
  rules.push_back(rule("FDM1", "fits the build volume (largest extent / build size, any axis-aligned orientation)", fit, "<=", 1.0, "1"));
  if (g.min_wall_mm) rules.push_back(rule("FDM2", "minimum wall at least " + std::to_string(int(val(P, "min_wall_nozzles"))) + " nozzle widths", *g.min_wall_mm, ">=", min_wall, "mm"));
  return {{"process", "fdm"},
          {"material", material},
          {"print_direction", lim.print_direction},
          {"rules", rules},
          {"overhang_area_mm2", g.overhang_area_mm2 ? nlohmann::json(*g.overhang_area_mm2) : nlohmann::json()},
          {"support_volume_mm3", c["support_volume_mm3"]},
          {"part_volume_mm3", c["part_volume_mm3"]},
          {"time_h", c["time_h"]},
          {"cost", c["cost"]},
          {"orientations", orient},
          {"least_support_direction", best_dir},
          {"inputs", P},
          {"price_per_kg", processes.at("material_price_per_kg").at(material)},
          {"notes", {"Support volume is an upper bound: columns are counted down to the bed even where they would land on the part.",
                     "Time is volume / average deposition rate plus setup; a slicer gives the exact time."}}};
}

nlohmann::json cnc_report(const TopoDS_Shape& shape, const std::string& material, const nlohmann::json& processes) {
  const auto& P = processes.at("cnc");
  auto approach = P.at("approach_directions").at("value").get<std::vector<std::array<double, 3>>>();
  double tool_r = val(P, "tool_radius_mm");
  nlohmann::json c;
  process_cost(shape, material, {0, 0, 1}, processes, "cnc", &c);
  double total_area = area(shape);
  double under = undercut_area(shape, approach);
  CornerScan cs = scan_corners(shape, approach, tool_r);

  nlohmann::json rules = nlohmann::json::array();
  // sampling resolves about 1/3000 of the surface; below 0.5% it is treated as none
  rules.push_back(rule("CNC1", "surface reachable by the tool from the setup directions (undercut share of the surface)", under / total_area, "<=", 0.005, "1"));
  rules.push_back(rule("CNC2", "no sharp internal corner along the tool axis (a round tool leaves its radius)", cs.sharp_internal_along_tool, "<=", 0, "edges"));
  if (cs.min_internal_radius)
    rules.push_back(rule("CNC3", "internal corner radius along the tool axis at least the tool radius", *cs.min_internal_radius, ">=", tool_r, "mm"));
  return {{"process", "cnc"},
          {"material", material},
          {"rules", rules},
          {"undercut_area_mm2", under},
          {"sharp_internal_corners", cs.sharp_internal_along_tool},
          {"sharp_internal_corner_edges", cs.sharp_points},
          {"internal_radii_below_tool", cs.small_internal_radii},
          {"drilled_holes", cs.holes},
          {"stock_volume_mm3", c["stock_volume_mm3"]},
          {"removed_volume_mm3", c["removed_volume_mm3"]},
          {"setups", c["setups"]},
          {"time_h", c["time_h"]},
          {"cost", c["cost"]},
          {"inputs", P},
          {"price_per_kg", processes.at("material_price_per_kg").at(material)},
          {"notes", {"Stock is the bounding box plus the margin on every side; time is removed volume / average removal rate plus setups.",
                     "Pocket depth-to-width is not checked yet."}}};
}

nlohmann::json manufacture_report(const TopoDS_Shape& shape, const std::string& material, const std::array<double, 3>& print_dir,
                                  const nlohmann::json& processes) {
  nlohmann::json reports = nlohmann::json::object(), advice = nlohmann::json::array();
  for (const char* name : {"fdm", "cnc"}) {
    auto ok = processes.at(name).at("materials").at("value").get<std::vector<std::string>>();
    if (std::find(ok.begin(), ok.end(), material) == ok.end()) {
      advice.push_back({{"process", name}, {"fits", false}, {"reason", std::string(name) + " is not set up for " + material}});
      continue;
    }
    reports[name] = std::string(name) == "fdm" ? fdm_report(shape, material, print_dir, processes) : cnc_report(shape, material, processes);
  }
  std::string best;
  double best_cost = 1e300;
  for (auto& [name, r] : reports.items()) {
    bool ok = true;
    nlohmann::json closest;
    for (auto& x : r["rules"]) {
      ok = ok && x["passed"].get<bool>();
      if (closest.is_null() || x["margin"].get<double>() < closest["margin"].get<double>()) closest = x;
    }
    double c = r["cost"]["total"];
    advice.push_back({{"process", name}, {"fits", ok}, {"closest_rule", closest}, {"cost", c}, {"time_h", r["time_h"]}});
    if (ok && c < best_cost) {
      best_cost = c;
      best = name;
    }
  }
  reports["advice"] = advice;
  reports["recommended_process"] = best.empty() ? nlohmann::json() : nlohmann::json(best);
  return reports;
}

}  // namespace structura
