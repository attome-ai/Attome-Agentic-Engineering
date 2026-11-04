#include "structura/compile.hpp"

#include <cmath>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Vec.hxx>

#include "structura/occ.hpp"

namespace structura {
namespace {

gp_Dir dir_of(const std::string& a) {
  if (a == "x") return gp_Dir(1, 0, 0);
  if (a == "y") return gp_Dir(0, 1, 0);
  if (a == "z") return gp_Dir(0, 0, 1);
  if (a == "-x") return gp_Dir(-1, 0, 0);
  if (a == "-y") return gp_Dir(0, -1, 0);
  return gp_Dir(0, 0, -1);
}

TopoDS_Shape clean(const TopoDS_Shape& s) {
  ShapeUpgrade_UnifySameDomain u(s, true, true, true);
  u.Build();
  return u.Shape();
}

TopoDS_Shape boolean(const TopoDS_Shape* part, const Prim& p, const std::string& fid) {
  if (!part) {
    if (p.cut) throw CompileError(fid, "cannot cut before any material exists");
    return p.shape;
  }
  try {
    TopoDS_Shape out;
    if (p.cut) {
      BRepAlgoAPI_Cut op(*part, p.shape);
      if (!op.IsDone()) throw std::runtime_error("cut not done");
      out = op.Shape();
    } else {
      BRepAlgoAPI_Fuse op(*part, p.shape);
      if (!op.IsDone()) throw std::runtime_error("fuse not done");
      out = op.Shape();
    }
    return clean(out);
  } catch (...) {
    throw CompileError(fid, std::string("boolean ") + (p.cut ? "cut" : "add") + " failed: " + occ_message());
  }
}

}  // namespace

size_t CompiledPart::solid_count() const { return solids_of(shape).size(); }

std::map<std::string, std::vector<TopoDS_Face>> CompiledPart::named_faces() const {
  std::map<std::string, std::vector<TopoDS_Face>> out;
  for (auto& [name, sel] : graph.faces) out[name] = select_faces(shape, sel, values);
  return out;
}

std::optional<std::vector<double>> CompiledPart::design_space() const { return graph.design_space_values(values); }

CompiledPart compile_graph(const DesignGraph& g, const Values& overrides) {
  auto resolved = g.resolve(overrides);
  CompiledPart cp;
  cp.values = g.values(overrides);
  cp.graph = g;
  bool have = false;
  TopoDS_Shape part;
  for (auto& r : resolved) {
    const std::string& fid = r.feature->id;
    std::vector<Prim> prims;
    try {
      if (r.type->modifies) {
        if (!have) throw CompileError(fid, "needs an existing part");
        part = apply_modifier(part, r);
        cp.prims[fid] = {};
        continue;
      }
      prims = build_primitives(r, cp.prims);
    } catch (const CompileError&) {
      throw;
    } catch (...) {
      throw CompileError(fid, occ_message());
    }
    cp.prims[fid] = prims;
    for (auto& p : prims) {
      part = boolean(have ? &part : nullptr, p, fid);
      have = true;
    }
  }
  if (!have) throw CompileError(g.features.back().id, "graph produced no geometry");
  auto sol = solids_of(part);
  if (sol.size() == 1) part = sol[0];
  cp.shape = part;
  return cp;
}

std::vector<TopoDS_Face> select_faces(const TopoDS_Shape& s, const FaceSelector& sel, const Values& v, double tol) {
  std::optional<std::vector<double>> region;
  if (sel.region) {
    std::vector<double> r;
    for (auto& c : *sel.region) r.push_back(eval_value(c, v));
    region = r;
  }
  auto inside = [&](const gp_Pnt& c) {
    if (!region) return true;
    auto& r = *region;
    return r[0] - tol <= c.X() && c.X() <= r[3] + tol && r[1] - tol <= c.Y() && c.Y() <= r[4] + tol &&
           r[2] - tol <= c.Z() && c.Z() <= r[5] + tol;
  };
  std::vector<TopoDS_Face> out;
  if (sel.kind == "plane") {
    gp_Dir n = dir_of(sel.normal);
    double at = eval_value(sel.at, v);
    double sign = sel.normal[0] == '-' ? -1 : 1;
    for (auto& f : faces_of(s)) {
      if (BRepAdaptor_Surface(f).GetType() != GeomAbs_Plane) continue;
      if (normal(f).Dot(n) < 1 - 1e-6) continue;
      gp_Pnt c = centre(f);
      double coord = gp_Vec(c.XYZ()).Dot(gp_Vec(n)) * sign;
      if (std::fabs(coord - at) <= tol * std::max(1.0, std::fabs(at)) && inside(c)) out.push_back(f);
    }
  } else {
    gp_Dir a = dir_of(sel.axis);
    double rad = eval_value(sel.radius, v);
    for (auto& f : faces_of(s)) {
      BRepAdaptor_Surface su(f);
      if (su.GetType() != GeomAbs_Cylinder) continue;
      gp_Cylinder cyl = su.Cylinder();
      if (std::fabs(std::fabs(cyl.Axis().Direction().Dot(a)) - 1) > 1e-6) continue;
      if (std::fabs(cyl.Radius() - rad) > tol * std::max(1.0, rad)) continue;
      if (inside(centre(f))) out.push_back(f);
    }
  }
  return out;
}

}  // namespace structura
