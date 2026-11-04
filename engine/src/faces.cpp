#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepTools.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Vec.hxx>

#include "structura/occ.hpp"

namespace structura {

std::vector<TopoDS_Face> faces_of(const TopoDS_Shape& s) {
  TopTools_IndexedMapOfShape m;
  TopExp::MapShapes(s, TopAbs_FACE, m);
  std::vector<TopoDS_Face> out;
  for (int i = 1; i <= m.Extent(); ++i) out.push_back(TopoDS::Face(m(i)));
  return out;
}

std::vector<TopoDS_Edge> edges_of(const TopoDS_Shape& s) {
  TopTools_IndexedMapOfShape m;
  TopExp::MapShapes(s, TopAbs_EDGE, m);
  std::vector<TopoDS_Edge> out;
  for (int i = 1; i <= m.Extent(); ++i) out.push_back(TopoDS::Edge(m(i)));
  return out;
}

std::vector<TopoDS_Shape> solids_of(const TopoDS_Shape& s) {
  TopTools_IndexedMapOfShape m;
  TopExp::MapShapes(s, TopAbs_SOLID, m);
  std::vector<TopoDS_Shape> out;
  for (int i = 1; i <= m.Extent(); ++i) out.push_back(m(i));
  return out;
}

double volume(const TopoDS_Shape& s) {
  GProp_GProps p;
  BRepGProp::VolumeProperties(s, p);
  return p.Mass();
}

double area(const TopoDS_Shape& s) {
  GProp_GProps p;
  BRepGProp::SurfaceProperties(s, p);
  return p.Mass();
}

gp_Pnt centre(const TopoDS_Face& f) {
  GProp_GProps p;
  BRepGProp::SurfaceProperties(f, p);
  return p.CentreOfMass();
}

gp_Dir normal(const TopoDS_Face& f) {
  double u0, u1, v0, v1;
  BRepTools::UVBounds(f, u0, u1, v0, v1);
  BRepGProp_Face prop(f);
  gp_Pnt p;
  gp_Vec n;
  prop.Normal((u0 + u1) / 2, (v0 + v1) / 2, p, n);
  if (n.Magnitude() < 1e-12) throw std::runtime_error("degenerate face normal");
  return gp_Dir(n);
}

bool is_valid(const TopoDS_Shape& s) {
  BRepCheck_Analyzer a(s);
  return a.IsValid();
}

std::vector<double> bbox(const TopoDS_Shape& s, bool optimal) {
  Bnd_Box b;
  if (optimal)
    BRepBndLib::AddOptimal(s, b, false, false);
  else
    BRepBndLib::Add(s, b);
  double x0, y0, z0, x1, y1, z1;
  b.Get(x0, y0, z0, x1, y1, z1);
  return {x0, y0, z0, x1, y1, z1};
}

double edge_length(const TopoDS_Edge& e) {
  BRepAdaptor_Curve c(e);
  return GCPnts_AbscissaPoint::Length(c);
}

std::string occ_message() {
  try {
    throw;
  } catch (const Standard_Failure& f) {
    const char* m = f.GetMessageString();
    return std::string(f.DynamicType()->Name()) + (m && *m ? std::string(": ") + m : std::string());
  } catch (const std::exception& e) {
    return e.what();
  } catch (...) {
    return "unknown error";
  }
}

}  // namespace structura
