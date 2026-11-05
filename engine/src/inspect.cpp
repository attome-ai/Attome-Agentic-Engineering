#include "structura/inspect.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <numbers>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepTools.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>

#include "structura/design.hpp"
#include "structura/occ.hpp"

namespace fs = std::filesystem;

namespace structura {
namespace {

// "x", "-z", ... when v is within 1e-6 of a coordinate axis, else ""
std::string axis_name(const gp_Dir& d) {
  const char* n[3] = {"x", "y", "z"};
  double c[3] = {d.X(), d.Y(), d.Z()};
  for (int i = 0; i < 3; ++i)
    if (std::abs(std::abs(c[i]) - 1) < 1e-6) return (c[i] < 0 ? "-" : "") + std::string(n[i]);
  return "";
}

double coord(const gp_Pnt& p, const std::string& ax) {
  char a = ax.back();
  return a == 'x' ? p.X() : a == 'y' ? p.Y() : p.Z();
}

double r6(double v) { return std::round(v * 1e6) / 1e6; }

// true when the face's outward normal points towards the cylinder axis (a hole or an internal fillet); tested at a real
// surface point, because the centre of mass of a full cylinder lies on its axis
bool concave_cylinder(const TopoDS_Face& f, const gp_Cylinder& cy) {
  double u0, u1, v0, v1;
  BRepTools::UVBounds(f, u0, u1, v0, v1);
  BRepGProp_Face prop(f);
  gp_Pnt p;
  gp_Vec n;
  prop.Normal(0.5 * (u0 + u1), 0.5 * (v0 + v1), p, n);
  gp_Vec ax(cy.Axis().Direction()), rv(cy.Axis().Location(), p);
  rv -= ax * rv.Dot(ax);
  return n.Dot(rv) < 0;
}

}  // namespace

nlohmann::json inspect_shape(const TopoDS_Shape& shape) {
  auto bb = bbox(shape);
  double total_area = area(shape);
  nlohmann::json faces = nlohmann::json::array();
  struct Cyl { int face; gp_Ax1 axis; double r; double span; double area; gp_Pnt c; TopoDS_Face f; };
  std::vector<Cyl> concave;
  int idx = 0;
  for (auto& f : faces_of(shape)) {
    ++idx;
    BRepAdaptor_Surface s(f);
    gp_Pnt c = centre(f);
    double a = area(f);
    nlohmann::json x = {{"id", idx}, {"area_mm2", a}, {"centre", {c.X(), c.Y(), c.Z()}}, {"bbox", bbox(f)}};
    switch (s.GetType()) {
      case GeomAbs_Plane: {
        gp_Dir n = normal(f);
        x["type"] = "plane";
        x["normal"] = {n.X(), n.Y(), n.Z()};
        std::string an = axis_name(n);
        if (!an.empty()) x["selector"] = {{"kind", "plane"}, {"normal", an}, {"at", r6(coord(c, an))}};
        break;
      }
      case GeomAbs_Cylinder: {
        gp_Cylinder cy = s.Cylinder();
        gp_Vec ax(cy.Axis().Direction());
        bool inner = concave_cylinder(f, cy);
        x["type"] = "cylinder";
        x["radius"] = cy.Radius();
        x["axis"] = {ax.X(), ax.Y(), ax.Z()};
        x["concave"] = inner;
        double span = s.LastUParameter() - s.FirstUParameter();
        if (inner) concave.push_back({idx, cy.Axis(), cy.Radius(), span, a, c, f});
        break;
      }
      case GeomAbs_Cone: x["type"] = "cone"; break;
      case GeomAbs_Sphere: x["type"] = "sphere"; break;
      case GeomAbs_Torus: x["type"] = "torus"; break;
      default: x["type"] = "freeform";
    }
    faces.push_back(x);
  }

  // holes: concave cylinder faces on one axis line with one radius, together wrapping (nearly) a full turn
  std::vector<std::vector<int>> groups;
  std::vector<bool> used(concave.size(), false);
  for (size_t i = 0; i < concave.size(); ++i) {
    if (used[i]) continue;
    std::vector<int> g{static_cast<int>(i)};
    used[i] = true;
    for (size_t j = i + 1; j < concave.size(); ++j) {
      if (used[j] || std::abs(concave[j].r - concave[i].r) > 1e-6) continue;
      gp_Lin li(concave[i].axis);
      if (concave[i].axis.Direction().IsParallel(concave[j].axis.Direction(), 1e-6) && li.Distance(concave[j].axis.Location()) < 1e-6) {
        g.push_back(static_cast<int>(j));
        used[j] = true;
      }
    }
    groups.push_back(g);
  }
  nlohmann::json holes = nlohmann::json::array(), fillets = nlohmann::json::array();
  for (auto& g : groups) {
    double span = 0, a = 0;
    nlohmann::json ids = nlohmann::json::array();
    for (int k : g) {
      span += concave[k].span;
      a += concave[k].area;
      ids.push_back(concave[k].face);
    }
    const Cyl& c0 = concave[g[0]];
    gp_Dir d = c0.axis.Direction();
    double depth = a / (span * c0.r);  // lateral area = span * r * depth
    // the axis point nearest the centroid of the hole's faces
    gp_Pnt mid(0, 0, 0);
    for (int k : g) mid.SetXYZ(mid.XYZ() + concave[k].c.XYZ() / static_cast<double>(g.size()));
    gp_Vec v(c0.axis.Location(), mid);
    gp_Pnt on = c0.axis.Location().Translated(gp_Vec(d) * v.Dot(gp_Vec(d)));
    nlohmann::json h = {{"faces", ids}, {"diameter_mm", 2 * c0.r}, {"axis", {d.X(), d.Y(), d.Z()}}, {"centre", {on.X(), on.Y(), on.Z()}},
                        {"depth_mm", depth}, {"wrap_deg", span * 180 / std::numbers::pi}};
    std::string an = axis_name(d);
    if (!an.empty()) h["selector"] = {{"kind", "cylinder"}, {"axis", an.back() == 'x' ? "x" : an.back() == 'y' ? "y" : "z"}, {"radius", r6(c0.r)}};
    // a hole wraps the full turn: angular coverage around the shared axis in one frame (36 bins of 10 degrees), so
    // co-axial segments of a fillet split by ribs (each a quarter turn of the same angles) are not mistaken for a hole
    gp_Dir xd = std::abs(d.X()) < 0.9 ? gp_Dir(gp_Vec(d).Crossed(gp_Vec(1, 0, 0))) : gp_Dir(gp_Vec(d).Crossed(gp_Vec(0, 1, 0)));
    gp_Dir yd(gp_Vec(d).Crossed(gp_Vec(xd)));
    std::vector<bool> bins(36, false);
    for (int k : g) {
      double u0, u1, v0, v1;
      BRepTools::UVBounds(concave[k].f, u0, u1, v0, v1);
      BRepAdaptor_Surface su(concave[k].f);
      for (int i = 0; i <= 72; ++i) {
        gp_Pnt q = su.Value(u0 + (u1 - u0) * i / 72.0, 0.5 * (v0 + v1));
        gp_Vec rv(c0.axis.Location(), q);
        double ang = std::atan2(rv.Dot(gp_Vec(yd)), rv.Dot(gp_Vec(xd)));
        int b = static_cast<int>(std::floor((ang + std::numbers::pi) / (2 * std::numbers::pi) * 36)) % 36;
        bins[b] = true;
      }
    }
    int covered = static_cast<int>(std::count(bins.begin(), bins.end(), true));
    h["wrap_deg"] = covered * 10.0;
    if (covered >= 35) holes.push_back(h);
    else fillets.push_back({{"faces", ids}, {"radius_mm", c0.r}, {"wrap_deg", covered * 10.0}});
  }
  // patterns: holes of the same diameter on parallel axes
  nlohmann::json patterns = nlohmann::json::array();
  std::vector<bool> taken(holes.size(), false);
  for (size_t i = 0; i < holes.size(); ++i) {
    if (taken[i]) continue;
    nlohmann::json members = nlohmann::json::array({static_cast<int>(i)});
    gp_Dir di(holes[i]["axis"][0].get<double>(), holes[i]["axis"][1].get<double>(), holes[i]["axis"][2].get<double>());
    for (size_t j = i + 1; j < holes.size(); ++j) {
      gp_Dir dj(holes[j]["axis"][0].get<double>(), holes[j]["axis"][1].get<double>(), holes[j]["axis"][2].get<double>());
      if (!taken[j] && std::abs(holes[j]["diameter_mm"].get<double>() - holes[i]["diameter_mm"].get<double>()) < 1e-6 && di.IsParallel(dj, 1e-6)) {
        members.push_back(static_cast<int>(j));
        taken[j] = true;
      }
    }
    if (members.size() > 1) patterns.push_back({{"holes", members}, {"diameter_mm", holes[i]["diameter_mm"]}, {"count", members.size()}});
  }
  // mating candidates: the largest planar faces (at least 5% of the surface)
  nlohmann::json mating = nlohmann::json::array();
  for (auto& f : faces)
    if (f["type"] == "plane" && f["area_mm2"].get<double>() >= 0.05 * total_area) mating.push_back(f);
  std::sort(mating.begin(), mating.end(), [](const nlohmann::json& a, const nlohmann::json& b) { return a["area_mm2"].get<double>() > b["area_mm2"].get<double>(); });

  int not_aligned = 0;
  for (auto& f : faces)
    if (f["type"] == "plane" && !f.contains("selector")) ++not_aligned;
  return {{"valid", is_valid(shape)},
          {"solids", solids_of(shape).size()},
          {"volume_mm3", volume(shape)},
          {"area_mm2", total_area},
          {"envelope", bb},
          {"faces", faces},
          {"holes", holes},
          {"hole_patterns", patterns},
          {"internal_fillets", fillets},
          {"mating_candidates", mating},
          {"planes_not_axis_aligned", not_aligned},
          {"notes", {"Selectors are axis-aligned (plane normal x/y/z, cylinder axis x/y/z); a face that is not axis-aligned has no selector "
                     "and cannot carry a load or support yet. Align the part to the axes before upload if needed.",
                     "Hole depth is estimated from the lateral area; check it against the drawing."}}};
}

nlohmann::json draft_family(const std::string& name, const std::string& step_path, const nlohmann::json& inspection,
                            const TopoDS_Shape& shape) {
  std::ifstream in(step_path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + step_path);
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  nlohmann::json faces = nlohmann::json::object();
  int k = 0;
  for (auto& m : inspection.at("mating_candidates")) {
    if (!m.contains("selector")) continue;
    nlohmann::json sel = m["selector"];
    sel["doc"] = "planar face " + std::to_string(m["id"].get<int>()) + ", " + std::to_string(static_cast<int>(m["area_mm2"].get<double>())) + " mm^2";
    faces["face_" + std::to_string(m["id"].get<int>())] = sel;
    if (++k >= 6) break;
  }
  int h = 0;
  for (auto& p : inspection.at("holes")) {
    if (!p.contains("selector")) continue;
    nlohmann::json sel = p["selector"];
    sel["doc"] = "hole d" + std::to_string(p["diameter_mm"].get<double>()).substr(0, 5) + " mm (all holes of this diameter on this axis)";
    std::string key = "holes_d" + std::to_string(static_cast<int>(std::round(p["diameter_mm"].get<double>() * 10)));
    if (!faces.contains(key)) faces[key] = sel;
    ++h;
  }
  // frozen interfaces: material around every hole stays
  nlohmann::json preserve = nlohmann::json::array();
  for (auto& p : inspection.at("holes")) {
    double d = p["diameter_mm"], depth = p["depth_mm"];
    gp_Pnt c(p["centre"][0].get<double>(), p["centre"][1].get<double>(), p["centre"][2].get<double>());
    gp_Dir ax(p["axis"][0].get<double>(), p["axis"][1].get<double>(), p["axis"][2].get<double>());
    double lo[3], hi[3], cc[3] = {c.X(), c.Y(), c.Z()}, aa[3] = {ax.X(), ax.Y(), ax.Z()};
    for (int i = 0; i < 3; ++i) {
      double half = std::abs(aa[i]) * (depth / 2 + 1e-3) + (1 - std::abs(aa[i])) * 1.5 * d;  // along the axis: the hole; across: 3 d
      lo[i] = cc[i] - half;
      hi[i] = cc[i] + half;
    }
    TopoDS_Shape box = BRepPrimAPI_MakeBox(gp_Pnt(lo[0], lo[1], lo[2]), gp_Pnt(hi[0], hi[1], hi[2])).Shape();
    BRepAlgoAPI_Common com(shape, box);
    double v = com.IsDone() ? volume(com.Shape()) : 0;
    preserve.push_back({lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], v});
  }
  return {{"name", name},
          {"version", "1"},
          {"preserve", preserve},
          {"doc", "Uploaded part " + fs::path(step_path).filename().string() + " (A3): analysed as-is; redesign features are added after the user "
                  "confirms which interfaces are frozen."},
          {"parameters", nlohmann::json::object()},
          {"features", {{{"id", "part"}, {"type", "import_step"}, {"doc", "the uploaded solid"},
                         {"params", {{"file", step_path}, {"sha256", sha256_hex(bytes)}}}}}},
          {"faces", faces}};
}

}  // namespace structura
