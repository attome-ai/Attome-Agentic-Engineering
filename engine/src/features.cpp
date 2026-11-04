// Geometry builders for the grammar's feature types. Lengths in mm, angles in degrees.
#include <cmath>
#include <numbers>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "structura/compile.hpp"
#include "structura/occ.hpp"

namespace structura {
namespace {

gp_Dir axis_dir(const std::string& a) {
  if (a == "x") return gp_Dir(1, 0, 0);
  if (a == "y") return gp_Dir(0, 1, 0);
  if (a == "z") return gp_Dir(0, 0, 1);
  if (a == "-x") return gp_Dir(-1, 0, 0);
  if (a == "-y") return gp_Dir(0, -1, 0);
  return gp_Dir(0, 0, -1);
}

// Two unit directions orthogonal to an axis, in a fixed order.
std::pair<gp_Vec, gp_Vec> orth(const std::string& a) {
  char c = a.back();
  if (c == 'x') return {gp_Vec(0, 1, 0), gp_Vec(0, 0, 1)};
  if (c == 'y') return {gp_Vec(1, 0, 0), gp_Vec(0, 0, 1)};
  return {gp_Vec(1, 0, 0), gp_Vec(0, 1, 0)};
}

TopoDS_Shape box(double x, double y, double z, double lx, double ly, double lz) {
  return BRepPrimAPI_MakeBox(gp_Pnt(x, y, z), lx, ly, lz).Shape();
}

TopoDS_Shape cylinder(const gp_Pnt& c, double r, double h, const std::string& axis) {
  return BRepPrimAPI_MakeCylinder(gp_Ax2(c, axis_dir(axis)), r, h).Shape();
}

TopoDS_Shape prism_polygon(const std::vector<gp_Pnt>& pts, const gp_Vec& v) {
  BRepBuilderAPI_MakePolygon poly;
  for (auto& p : pts) poly.Add(p);
  poly.Close();
  TopoDS_Face face = BRepBuilderAPI_MakeFace(poly.Wire());
  return BRepPrimAPI_MakePrism(face, v).Shape();
}

bool parallel_line(const TopoDS_Edge& e, const gp_Dir& a, gp_Pnt& mid) {
  BRepAdaptor_Curve c(e);
  if (c.GetType() != GeomAbs_Line) return false;
  double t = (c.FirstParameter() + c.LastParameter()) / 2;
  mid = c.Value(t);
  return std::fabs(std::fabs(c.Line().Direction().Dot(a)) - 1) < 1e-6;
}

std::vector<TopoDS_Edge> edges_in(const TopoDS_Shape& part, const std::string& axis, const std::vector<double>& r) {
  std::vector<TopoDS_Edge> out;
  gp_Dir a = axis_dir(axis);
  for (auto& e : edges_of(part)) {
    gp_Pnt m;
    if (!parallel_line(e, a, m)) continue;
    if (r[0] <= m.X() && m.X() <= r[3] && r[1] <= m.Y() && m.Y() <= r[4] && r[2] <= m.Z() && m.Z() <= r[5]) out.push_back(e);
  }
  return out;
}

std::vector<Prim> one(TopoDS_Shape s, bool cut) { return {Prim{std::move(s), cut}}; }

}  // namespace

std::vector<Prim> build_primitives(const Resolved& r, const std::map<std::string, std::vector<Prim>>& earlier) {
  const std::string& t = r.type->name;
  auto cut_op = [&] { return r.str.count("op") && r.s("op") == "cut"; };

  if (t == "plate") return one(box(r.n("x"), r.n("y"), r.n("z"), r.n("length"), r.n("width"), r.n("thickness")), cut_op());

  if (t == "extrude_sketch") {
    std::vector<gp_Pnt> pts;
    for (auto& p : r.pts.at("points")) pts.emplace_back(p[0], p[1], r.n("z"));
    if (pts.size() < 3) throw std::runtime_error("a sketch needs at least three points");
    return one(prism_polygon(pts, gp_Vec(0, 0, r.n("height"))), cut_op());
  }

  if (t == "boss")
    return one(cylinder(gp_Pnt(r.n("x"), r.n("y"), r.n("z")), r.n("diameter") / 2, r.n("height"), r.s("axis")), false);

  if (t == "hole")
    return one(cylinder(gp_Pnt(r.n("x"), r.n("y"), r.n("z")), r.n("diameter") / 2, r.n("depth"), r.s("axis")), true);

  if (t == "hole_pattern") {
    auto [u, w] = orth(r.s("axis"));
    std::vector<Prim> out;
    for (int i = 0; i < static_cast<int>(r.n("n1")); ++i)
      for (int j = 0; j < static_cast<int>(r.n("n2")); ++j) {
        gp_Pnt c(r.n("x"), r.n("y"), r.n("z"));
        c.Translate(u * (i * r.n("d1")) + w * (j * r.n("d2")));
        out.push_back({cylinder(c, r.n("diameter") / 2, r.n("depth"), r.s("axis")), true});
      }
    return out;
  }

  if (t == "rib") {
    gp_Vec a(axis_dir(r.s("leg_a_dir"))), b(axis_dir(r.s("leg_b_dir")));
    if (std::fabs(a.Dot(b)) > 1e-9) throw std::runtime_error("rib legs must be perpendicular");
    gp_Vec n = a.Crossed(b);
    gp_Pnt o(r.n("x"), r.n("y"), r.n("z"));
    gp_Pnt pa = o.Translated(a * r.n("leg_a")), pb = o.Translated(b * r.n("leg_b"));
    return one(prism_polygon({o, pa, pb}, n * r.n("thickness")), false);
  }

  if (t == "pocket") {
    double rad = r.n("corner_radius");
    if (rad < 0) throw std::runtime_error("pocket corner_radius must not be negative");
    TopoDS_Shape s = box(r.n("x"), r.n("y"), r.n("z_top") - r.n("depth"), r.n("length"), r.n("width"), r.n("depth") + 1e-3);
    if (rad > 0) {
      if (rad >= std::min(r.n("length"), r.n("width")) / 2)
        throw std::runtime_error("pocket corner_radius must be less than half the pocket's smaller side");
      BRepFilletAPI_MakeFillet mk(s);
      gp_Pnt m;
      for (auto& e : edges_of(s))
        if (parallel_line(e, gp_Dir(0, 0, 1), m)) mk.Add(rad, e);
      mk.Build();
      if (!mk.IsDone()) throw std::runtime_error("pocket corner rounding failed");
      s = mk.Shape();
    }
    return one(s, true);
  }

  if (t == "sweep_path") {
    const auto& p = r.pts.at("points");
    std::vector<gp_Pnt> pts;
    for (auto& q : p) pts.emplace_back(q[0], q[1], q.size() > 2 ? q[2] : 0.0);
    BRepBuilderAPI_MakePolygon poly;
    for (auto& q : pts) poly.Add(q);
    TopoDS_Wire path = poly.Wire();
    gp_Vec d0(pts[0], pts[1]);
    if (d0.Magnitude() < 1e-9) throw std::runtime_error("sweep_path has coincident first points");
    gp_Circ circ(gp_Ax2(pts[0], gp_Dir(d0)), r.n("radius"));
    TopoDS_Wire prof = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circ).Edge());
    BRepOffsetAPI_MakePipeShell ps(path);
    ps.SetTransitionMode(BRepBuilderAPI_RoundCorner);
    ps.Add(prof);
    ps.Build();
    if (!ps.IsDone()) throw std::runtime_error("sweep failed");
    ps.MakeSolid();
    return one(ps.Shape(), cut_op());
  }

  if (t == "fin_array") {
    std::vector<Prim> out;
    for (int i = 0; i < static_cast<int>(r.n("count")); ++i)
      out.push_back({box(r.n("x"), r.n("y") + i * r.n("pitch"), r.n("z"), r.n("length"), r.n("fin_thickness"), r.n("height")), false});
    return out;
  }

  if (t == "linear_pattern" || t == "circular_pattern") {
    const auto& src = earlier.at(r.s("target"));
    std::vector<Prim> out;
    int n = static_cast<int>(r.n("count"));
    double step = 0;
    if (t == "circular_pattern") {
      double ang = r.n("angle");
      step = std::fabs(ang - 360) < 1e-9 ? ang / n : ang / std::max(n - 1, 1);
    }
    for (int k = 1; k < n; ++k) {
      gp_Trsf tr;
      if (t == "linear_pattern")
        tr.SetTranslation(gp_Vec(k * r.n("dx"), k * r.n("dy"), k * r.n("dz")));
      else
        tr.SetRotation(gp_Ax1(gp_Pnt(r.n("cx"), r.n("cy"), r.n("cz")), axis_dir(r.s("axis"))), k * step * std::numbers::pi / 180);
      for (auto& p : src) out.push_back({BRepBuilderAPI_Transform(p.shape, tr, true).Shape(), p.cut});
    }
    return out;
  }
  throw std::runtime_error("no primitive builder for " + t);
}

TopoDS_Shape apply_modifier(const TopoDS_Shape& part, const Resolved& r) {
  const std::string& t = r.type->name;
  if (t == "fillet" || t == "chamfer") {
    auto edges = edges_in(part, r.s("edge_axis"), r.vec.at("region"));
    if (edges.empty()) throw std::runtime_error(t + " selects no edges");
    if (t == "fillet") {
      BRepFilletAPI_MakeFillet mk(part);
      for (auto& e : edges) mk.Add(r.n("radius"), e);
      mk.Build();
      if (!mk.IsDone()) throw std::runtime_error("fillet failed");
      return mk.Shape();
    }
    BRepFilletAPI_MakeChamfer mk(part);
    for (auto& e : edges) mk.Add(r.n("length"), e);
    mk.Build();
    if (!mk.IsDone()) throw std::runtime_error("chamfer failed");
    return mk.Shape();
  }
  if (t == "shell") {
    gp_Dir n = axis_dir(r.s("open_side"));
    std::vector<TopoDS_Face> cand;
    double best = -1e300;
    for (auto& f : faces_of(part)) {
      if (BRepAdaptor_Surface(f).GetType() != GeomAbs_Plane) continue;
      if (normal(f).Dot(n) < 1 - 1e-6) continue;
      double c = gp_Vec(centre(f).XYZ()).Dot(gp_Vec(n));
      cand.push_back(f);
      best = std::max(best, c);
    }
    TopTools_ListOfShape open;
    for (auto& f : cand)
      if (std::fabs(gp_Vec(centre(f).XYZ()).Dot(gp_Vec(n)) - best) < 1e-6) open.Append(f);
    if (open.IsEmpty()) throw std::runtime_error("shell finds no face on the open side");
    BRepOffsetAPI_MakeThickSolid mk;
    mk.MakeThickSolidByJoin(part, open, -r.n("thickness"), 1e-4, BRepOffset_Skin, false, false, GeomAbs_Arc);
    mk.Build();
    if (!mk.IsDone()) throw std::runtime_error("shell failed");
    return mk.Shape();
  }
  throw std::runtime_error("no modifier for " + t);
}

}  // namespace structura
