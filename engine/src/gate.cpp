#include "structura/gate.hpp"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <random>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>

#include "structura/occ.hpp"

namespace structura {
namespace {
std::string f2(double v) {
  char b[64];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}
std::string g(double v) {
  char b[64];
  std::snprintf(b, sizeof b, "%g", v);
  return b;
}
}  // namespace

GateLimits GateLimits::from_json(const nlohmann::json& j) {
  GateLimits l;
  if (j.contains("design_space") && !j["design_space"].is_null()) l.design_space = j["design_space"].get<std::vector<double>>();
  if (j.contains("keep_out")) l.keep_out = j["keep_out"].get<std::vector<std::vector<double>>>();
  if (j.contains("preserve")) l.preserve = j["preserve"].get<std::vector<std::vector<double>>>();
  l.min_wall = j.value("min_wall", l.min_wall);
  l.min_feature = j.value("min_feature", l.min_feature);
  l.min_hole_diameter = j.value("min_hole_diameter", l.min_hole_diameter);
  if (j.contains("print_direction")) l.print_direction = j["print_direction"].get<std::array<double, 3>>();
  l.max_overhang_deg = j.value("max_overhang_deg", l.max_overhang_deg);
  l.require_support_free = j.value("require_support_free", l.require_support_free);
  l.wall_samples = j.value("wall_samples", l.wall_samples);
  l.seed = j.value("seed", l.seed);
  return l;
}

nlohmann::json GateResult::to_json() const {
  auto opt = [](const std::optional<double>& v) { return v ? nlohmann::json(*v) : nlohmann::json(); };
  return {{"ok", ok}, {"reasons", reasons}, {"warnings", warnings}, {"volume_mm3", volume_mm3}, {"solids", solids},
          {"min_wall_mm", opt(min_wall_mm)}, {"min_edge_mm", opt(min_edge_mm)}, {"overhang_area_mm2", opt(overhang_area_mm2)}};
}

std::optional<double> wall_thickness(const TopoDS_Shape& shape, int samples, unsigned seed) {
  std::vector<TopoDS_Face> faces;
  std::vector<double> weights;
  for (auto& f : faces_of(shape)) {
    double a = area(f);
    if (a > 1e-9) { faces.push_back(f); weights.push_back(a); }
  }
  if (faces.empty()) return std::nullopt;
  std::mt19937 rng(seed);
  std::discrete_distribution<size_t> pick(weights.begin(), weights.end());
  std::optional<double> best;
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
      if (m < 1e-12) continue;
      n /= m;
      gp_Pnt start(p.X() - 1e-3 * n.X(), p.Y() - 1e-3 * n.Y(), p.Z() - 1e-3 * n.Z());
      BRepIntCurveSurface_Inter inter;
      inter.Init(shape, gp_Lin(start, gp_Dir(-n)), 1e-6);
      std::optional<double> dmin;
      for (; inter.More(); inter.Next()) {
        double w = inter.W();
        if (w > 1e-4 && (!dmin || w < *dmin)) dmin = w;
      }
      if (dmin) {
        double t = *dmin + 1e-3;
        best = best ? std::min(*best, t) : t;
      }
      break;
    }
  }
  return best;
}

double overhang_area(const TopoDS_Shape& shape, const std::array<double, 3>& d, double max_deg) {
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
      for (auto* p : {&x.a, &x.b, &x.c}) zmin = std::min(zmin, p->X() * d[0] + p->Y() * d[1] + p->Z() * d[2]);
      tris.push_back(x);
    }
  }
  double limit = std::cos(max_deg * std::numbers::pi / 180);
  double out = 0;
  for (auto& x : tris) {
    gp_Vec n = gp_Vec(x.a, x.b).Crossed(gp_Vec(x.a, x.c));
    double a2 = n.Magnitude();
    if (a2 < 1e-12) continue;
    double down = -(n.X() * d[0] + n.Y() * d[1] + n.Z() * d[2]) / a2;
    double h = std::min({x.a.X() * d[0] + x.a.Y() * d[1] + x.a.Z() * d[2], x.b.X() * d[0] + x.b.Y() * d[1] + x.b.Z() * d[2],
                         x.c.X() * d[0] + x.c.Y() * d[1] + x.c.Z() * d[2]});
    if (down > limit && h > zmin + 1e-6) out += a2 / 2;
  }
  return out;
}

GateResult gate(const TopoDS_Shape& shape, const GateLimits& lim) {
  GateResult r;
  auto sol = solids_of(shape);
  r.solids = sol.size();
  for (auto& s : sol) r.volume_mm3 += volume(s);
  if (!is_valid(shape)) r.reasons.push_back("invalid B-rep");
  if (sol.size() != 1) r.reasons.push_back(std::to_string(sol.size()) + " solids; exactly one connected solid is required");
  if (r.volume_mm3 <= 0) r.reasons.push_back("non-positive volume");
  if (r.reasons.empty()) {
    auto b = bbox(shape);
    if (lim.design_space) {
      auto& ds = *lim.design_space;
      double t = 1e-6;
      if (b[0] < ds[0] - t || b[1] < ds[1] - t || b[2] < ds[2] - t || b[3] > ds[3] + t || b[4] > ds[4] + t || b[5] > ds[5] + t)
        r.reasons.push_back("outside the design space: bounding box [" + f2(b[0]) + ", " + f2(b[1]) + ", " + f2(b[2]) + ", " +
                            f2(b[3]) + ", " + f2(b[4]) + ", " + f2(b[5]) + "] exceeds [" + g(ds[0]) + ", " + g(ds[1]) + ", " +
                            g(ds[2]) + ", " + g(ds[3]) + ", " + g(ds[4]) + ", " + g(ds[5]) + "]");
    }
    for (size_t i = 0; i < lim.keep_out.size(); ++i) {
      auto& k = lim.keep_out[i];
      TopoDS_Shape box = BRepPrimAPI_MakeBox(gp_Pnt(k[0], k[1], k[2]), k[3] - k[0], k[4] - k[1], k[5] - k[2]).Shape();
      BRepAlgoAPI_Common c(shape, box);
      if (c.IsDone() && volume(c.Shape()) > 1e-6) r.reasons.push_back("intrudes into keep-out zone " + std::to_string(i));
    }
    for (size_t i = 0; i < lim.preserve.size(); ++i) {
      auto& k = lim.preserve[i];
      TopoDS_Shape box = BRepPrimAPI_MakeBox(gp_Pnt(k[0], k[1], k[2]), k[3] - k[0], k[4] - k[1], k[5] - k[2]).Shape();
      BRepAlgoAPI_Common c(shape, box);
      double v = c.IsDone() ? volume(c.Shape()) : 0;
      if (v < k[6] * (1 - 1e-4) - 1e-6)
        r.reasons.push_back("removes material from frozen interface " + std::to_string(i) + " (" + f2(v) + " of " + f2(k[6]) + " mm^3 kept)");
    }
    for (auto& e : edges_of(shape)) {
      double l = edge_length(e);
      r.min_edge_mm = r.min_edge_mm ? std::min(*r.min_edge_mm, l) : l;
    }
    if (r.min_edge_mm && *r.min_edge_mm < lim.min_feature)
      r.reasons.push_back("edge of " + f2(*r.min_edge_mm) + " mm is shorter than the minimum feature size " + g(lim.min_feature) + " mm");
    for (auto& f : faces_of(shape)) {
      BRepAdaptor_Surface su(f);
      if (su.GetType() != GeomAbs_Cylinder) continue;
      double u0, u1, v0, v1;
      BRepTools::UVBounds(f, u0, u1, v0, v1);
      if (u1 - u0 < std::numbers::pi - 1e-6) continue;  // holes wrap at least half way; corner fillets about 90 deg
      gp_Cylinder cyl = su.Cylinder();
      gp_Pnt p = su.Value((u0 + u1) / 2, (v0 + v1) / 2);
      gp_Dir n = normal(f);
      gp_Ax1 ax = cyl.Axis();
      gp_Vec w(ax.Location(), p);
      double t = w.Dot(gp_Vec(ax.Direction()));
      gp_Pnt foot = ax.Location().Translated(gp_Vec(ax.Direction()) * t);
      gp_Vec to_axis(p, foot);
      if (to_axis.Dot(gp_Vec(n)) > 0 && 2 * cyl.Radius() < lim.min_hole_diameter - 1e-9)
        r.reasons.push_back("hole of diameter " + f2(2 * cyl.Radius()) + " mm is below the minimum " + g(lim.min_hole_diameter) + " mm");
    }
    r.min_wall_mm = wall_thickness(shape, lim.wall_samples, lim.seed);
    if (r.min_wall_mm && *r.min_wall_mm < lim.min_wall)
      r.reasons.push_back("wall of " + f2(*r.min_wall_mm) + " mm is thinner than the minimum " + g(lim.min_wall) + " mm");
    r.overhang_area_mm2 = overhang_area(shape, lim.print_direction, lim.max_overhang_deg);
    if (*r.overhang_area_mm2 > 1e-6) {
      char b[128];
      std::snprintf(b, sizeof b, "%.0f mm^2 overhangs more than %g degrees for the print direction", *r.overhang_area_mm2, lim.max_overhang_deg);
      (lim.require_support_free ? r.reasons : r.warnings).push_back(b);
    }
  }
  r.ok = r.reasons.empty();
  return r;
}

}  // namespace structura
