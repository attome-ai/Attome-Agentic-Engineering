// C3: a face the user clicked. A click becomes a geometric signature (surface type, direction, where the face sits inside the
// part's bounding box, its share of the area), so it finds the same face again after the parameters change, or says it cannot.
#include <algorithm>
#include <cmath>
#include <limits>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Cylinder.hxx>

#include "structura/compile.hpp"
#include "structura/occ.hpp"

namespace structura {
namespace {

// A picked face further than this from every signature is not the same face any more.
constexpr double kMaxScore = 0.5;  // the load face of the bracket scores 0.35 when its plate goes from the default to 12 mm
constexpr double kDirectionCos = 0.996;  // 5 degrees
// Two faces this close to equally good are a coin toss; a load must not land on one by chance.
constexpr double kAmbiguousGap = 0.02;

struct Sig {
  std::string surface;
  gp_Dir dir{0, 0, 1};
  std::array<double, 3> where{0, 0, 0};
  double area_fraction = 0;
};

Sig signature(const TopoDS_Face& f, const std::vector<double>& box, double total_area) {
  Sig s;
  BRepAdaptor_Surface su(f);
  if (su.GetType() == GeomAbs_Plane) {
    s.surface = "plane";
    s.dir = normal(f);
  } else if (su.GetType() == GeomAbs_Cylinder) {
    s.surface = "cylinder";
    s.dir = su.Cylinder().Axis().Direction();
  } else {
    s.surface = "other";
    s.dir = normal(f);
  }
  gp_Pnt c = centre(f);
  const double xyz[3] = {c.X(), c.Y(), c.Z()};
  for (int i = 0; i < 3; ++i) {
    const double span = box[i + 3] - box[i];
    s.where[i] = span > 1e-9 ? (xyz[i] - box[i]) / span : 0.5;
  }
  s.area_fraction = total_area > 0 ? area(f) / total_area : 0;
  return s;
}

std::string base64(const void* data, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const auto* p = static_cast<const unsigned char*>(data);
  std::string out;
  out.reserve((n + 2) / 3 * 4);
  for (size_t i = 0; i < n; i += 3) {
    const unsigned v = p[i] << 16 | (i + 1 < n ? p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
    out += T[v >> 18 & 63];
    out += T[v >> 12 & 63];
    out += i + 1 < n ? T[v >> 6 & 63] : '=';
    out += i + 2 < n ? T[v & 63] : '=';
  }
  return out;
}

double total_face_area(const std::vector<TopoDS_Face>& faces) {
  double t = 0;
  for (auto& f : faces) t += area(f);
  return t;
}

double score(const Sig& a, const Sig& b) {
  double d = 0;
  for (int i = 0; i < 3; ++i) d += (a.where[i] - b.where[i]) * (a.where[i] - b.where[i]);
  return std::sqrt(d) + 0.5 * std::fabs(std::log(std::max(a.area_fraction, 1e-12) / std::max(b.area_fraction, 1e-12)));
}

}  // namespace

std::vector<TopoDS_Face> select_picked(const TopoDS_Shape& s, const std::string& name, const ojson& sig) {
  Sig want;
  want.surface = sig.at("surface").get<std::string>();
  auto d = sig.at("direction").get<std::array<double, 3>>();
  want.dir = gp_Dir(d[0], d[1], d[2]);
  want.where = sig.at("where").get<std::array<double, 3>>();
  want.area_fraction = sig.at("area_fraction").get<double>();
  auto faces = faces_of(s);
  auto box = bbox(s);
  const double total = total_face_area(faces);
  const TopoDS_Face* best = nullptr;
  double b1 = std::numeric_limits<double>::infinity(), b2 = b1;
  for (auto& f : faces) {
    Sig got = signature(f, box, total);
    if (got.surface != want.surface) continue;
    const double dot = got.dir.Dot(want.dir);
    if ((want.surface == "cylinder" ? std::fabs(dot) : dot) < kDirectionCos) continue;  // a cylinder's axis has no sign
    const double x = score(got, want);
    if (x < b1) {
      b2 = b1;
      b1 = x;
      best = &f;
    } else if (x < b2) {
      b2 = x;
    }
  }
  if (!best || b1 > kMaxScore)
    throw CompileError("face " + name, "the picked face is gone from this design (nearest match scores " + std::to_string(b1) + ", limit " + std::to_string(kMaxScore) + ")");
  if (b2 - b1 < kAmbiguousGap)
    throw CompileError("face " + name, "the picked face is ambiguous in this design: two faces match it about equally");
  return {*best};
}

nlohmann::json tessellate(const TopoDS_Shape& shape, double deflection) {
  BRepMesh_IncrementalMesh(shape, deflection, false, 0.5);
  std::vector<float> pos;
  std::vector<uint32_t> idx;
  for (auto& f : faces_of(shape)) {
    TopLoc_Location loc;
    Handle(Poly_Triangulation) t = BRep_Tool::Triangulation(f, loc);
    if (t.IsNull()) continue;
    const uint32_t base = static_cast<uint32_t>(pos.size() / 3);
    for (int i = 1; i <= t->NbNodes(); ++i) {
      gp_Pnt p = t->Node(i).Transformed(loc.Transformation());
      pos.push_back(static_cast<float>(p.X()));
      pos.push_back(static_cast<float>(p.Y()));
      pos.push_back(static_cast<float>(p.Z()));
    }
    for (int i = 1; i <= t->NbTriangles(); ++i) {
      int a, b, c;
      t->Triangle(i).Get(a, b, c);
      for (int n : {a, b, c}) idx.push_back(base + n - 1);
    }
  }
  return {{"positions", base64(pos.data(), pos.size() * sizeof(float))},
          {"indices", base64(idx.data(), idx.size() * sizeof(uint32_t))},
          {"triangles", idx.size() / 3}};
}

nlohmann::json pick_face(const CompiledPart& part, const std::array<double, 3>& point) {
  auto faces = faces_of(part.shape);
  if (faces.empty()) throw std::runtime_error("the part has no faces");
  TopoDS_Vertex v = BRepBuilderAPI_MakeVertex(gp_Pnt(point[0], point[1], point[2]));
  int best = -1;
  double dist = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < faces.size(); ++i) {
    BRepExtrema_DistShapeShape e(v, faces[i]);
    if (e.IsDone() && e.Value() < dist) {
      dist = e.Value();
      best = static_cast<int>(i);
    }
  }
  if (best < 0) throw std::runtime_error("no face could be measured from the point");
  const TopoDS_Face& f = faces[best];
  auto box = bbox(part.shape);
  Sig s = signature(f, box, total_face_area(faces));
  nlohmann::json named = nlohmann::json::array();
  for (auto& [name, list] : part.named_faces())
    for (auto& g : list)
      if (g.IsSame(f)) named.push_back(name);
  // the face's own triangles, so the viewer can light up exactly what was picked
  BRepMesh_IncrementalMesh(part.shape, 0.2, false, 0.35);
  TopLoc_Location loc;
  nlohmann::json tris = nlohmann::json::array();
  Handle(Poly_Triangulation) t = BRep_Tool::Triangulation(f, loc);
  if (!t.IsNull()) {
    for (int i = 1; i <= t->NbTriangles(); ++i) {
      int a, b, c;
      t->Triangle(i).Get(a, b, c);
      for (int n : {a, b, c}) {
        gp_Pnt p = t->Node(n).Transformed(loc.Transformation());
        tris.push_back(p.X());
        tris.push_back(p.Y());
        tris.push_back(p.Z());
      }
    }
  }
  gp_Pnt c = centre(f);
  return {{"face_id", best},
          {"distance_mm", dist},
          {"area_mm2", area(f)},
          {"centre", {c.X(), c.Y(), c.Z()}},
          {"already_named", named},
          {"triangles", tris},
          {"selector",
           {{"kind", "picked"},
            {"signature",
             {{"surface", s.surface},
              {"direction", {s.dir.X(), s.dir.Y(), s.dir.Z()}},
              {"where", s.where},
              {"area_fraction", s.area_fraction}}}}}};
}

}  // namespace structura
