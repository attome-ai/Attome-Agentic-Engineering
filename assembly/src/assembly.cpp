#include "attome/assembly.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <random>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>

namespace fs = std::filesystem;

namespace attome {
namespace {

using V3 = std::array<double, 3>;

gp_Dir dir_of(const nlohmann::json& a) { return gp_Dir(a.at(0).get<double>(), a.at(1).get<double>(), a.at(2).get<double>()); }
gp_Pnt pnt_of(const nlohmann::json& a) { return gp_Pnt(a.at(0).get<double>(), a.at(1).get<double>(), a.at(2).get<double>()); }

double volume(const TopoDS_Shape& s) {
  GProp_GProps g;
  BRepGProp::VolumeProperties(s, g);
  return g.Mass();
}

Bnd_Box box_of(const TopoDS_Shape& s) {
  Bnd_Box b;
  BRepBndLib::AddOptimal(s, b, false, false);
  return b;
}

// a hexagon of `af` across flats in the plane through `c` normal to `n`, extruded `h` along n
TopoDS_Shape hex_prism(const gp_Pnt& c, const gp_Dir& n, double af, double h) {
  gp_Ax2 ax(c, n);
  gp_Vec x(ax.XDirection()), y(ax.YDirection());
  double r = af / std::sqrt(3.0);  // circumradius
  BRepBuilderAPI_MakePolygon poly;
  for (int k = 0; k < 6; ++k) {
    double a = k * 3.14159265358979 / 3;
    poly.Add(c.Translated(x * (r * std::cos(a)) + y * (r * std::sin(a))));
  }
  poly.Close();
  TopoDS_Face f = BRepBuilderAPI_MakeFace(poly.Wire());
  return BRepPrimAPI_MakePrism(f, gp_Vec(n) * h).Shape();
}

TopoDS_Shape transform(const TopoDS_Shape& s, const nlohmann::json& j) {
  gp_Trsf t;
  if (j.contains("rotate")) {
    gp_Trsf r;
    r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), dir_of(j["rotate"].at("axis"))), j["rotate"].at("deg").get<double>() * 3.14159265358979 / 180);
    t = r;
  }
  if (j.contains("translate")) {
    gp_Trsf m;
    m.SetTranslation(gp_Vec(pnt_of(j["translate"]).XYZ()));
    t = m * t;
  }
  return BRepBuilderAPI_Transform(s, t, true).Shape();
}

struct Sample {
  gp_Pnt p;
  gp_Vec n;  // outward normal
};

// area-weighted points on the exact surfaces, deterministic
std::vector<Sample> surface_samples(const TopoDS_Shape& s, int n, unsigned seed) {
  std::vector<TopoDS_Face> faces;
  std::vector<double> w;
  for (TopExp_Explorer e(s, TopAbs_FACE); e.More(); e.Next()) {
    TopoDS_Face f = TopoDS::Face(e.Current());
    GProp_GProps g;
    BRepGProp::SurfaceProperties(f, g);
    if (g.Mass() > 1e-9) {
      faces.push_back(f);
      w.push_back(g.Mass());
    }
  }
  std::vector<Sample> out;
  if (faces.empty()) return out;
  std::mt19937 rng(seed);
  std::discrete_distribution<size_t> pick(w.begin(), w.end());
  // every face gets at least a few points, so small faces (a bolt head's underside) are never missed
  std::vector<size_t> order;
  for (size_t i = 0; i < faces.size(); ++i)
    for (int k = 0; k < 3; ++k) order.push_back(i);
  for (int k = 0; k < n; ++k) order.push_back(pick(rng));
  for (size_t fi : order) {
    const TopoDS_Face& f = faces[fi];
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    std::uniform_real_distribution<double> du(u0, u1), dv(v0, v1);
    BRepGProp_Face prop(f);
    for (int t = 0; t < 60; ++t) {
      double u = du(rng), v = dv(rng);
      if (BRepClass_FaceClassifier(f, gp_Pnt2d(u, v), 1e-7).State() != TopAbs_IN) continue;
      gp_Pnt p;
      gp_Vec nn;
      prop.Normal(u, v, p, nn);
      if (nn.Magnitude() < 1e-12) break;
      out.push_back({p, nn.Normalized()});
      break;
    }
  }
  return out;
}

// Does a ray from `p` along `d` enter `target`'s material (first surface met is entered, not left)?
bool ray_enters(const gp_Pnt& p0, const gp_Vec& n, const gp_Vec& d, const TopoDS_Shape& target) {
  // start a hair inside the moving part's own material (along -n), and a hair behind along -d: a face in contact is
  // still met head-on when moving into it, but a contact plane is never grazed when sliding along it
  gp_Pnt start = p0.Translated(n * -1e-3).Translated(d * -1e-3);
  BRepIntCurveSurface_Inter it;
  it.Init(target, gp_Lin(start, gp_Dir(d)), 1e-6);
  double best = 1e300;
  bool entering = false;
  for (; it.More(); it.Next()) {
    double w = it.W();
    if (w < 0 || w >= best) continue;
    if (it.State() == TopAbs_ON) continue;  // on a face's boundary edge: a graze, not a crossing
    BRepGProp_Face prop(it.Face());
    gp_Pnt q;
    gp_Vec nn;
    prop.Normal(it.U(), it.V(), q, nn);
    if (nn.Magnitude() < 1e-12) continue;
    best = w;
    entering = nn.Normalized().Dot(d) < -1e-3;  // moving against the surface's outward normal: into the material
  }
  return entering;
}

bool boxes_meet_along(const Bnd_Box& moving, const Bnd_Box& other, const gp_Vec& d) {
  // the moving part's box swept far along d against the other's box
  Bnd_Box swept = moving;
  double x0, y0, z0, x1, y1, z1;
  moving.Get(x0, y0, z0, x1, y1, z1);
  for (double s : {1e4}) {
    swept.Add(gp_Pnt(x0 + d.X() * s, y0 + d.Y() * s, z0 + d.Z() * s));
    swept.Add(gp_Pnt(x1 + d.X() * s, y1 + d.Y() * s, z1 + d.Z() * s));
  }
  swept.Enlarge(0.01);
  return !swept.IsOut(other);
}

// Contact points of p with the others: samples whose outward normal, a hair away, meets another part's surface.
std::vector<gp_Vec> contact_normals(const Part& p, const std::vector<const Part*>& others) {
  std::vector<gp_Vec> out;
  for (auto& s : surface_samples(p.shape, 600, 7)) {
    for (auto* q : others) {
      BRepIntCurveSurface_Inter it;
      it.Init(q->shape, gp_Lin(s.p.Translated(s.n * -1e-3), gp_Dir(s.n)), 1e-6);
      bool touching = false;
      for (; it.More(); it.Next()) {
        // the line is infinite both ways: only hits just ahead count (the start is 1e-3 behind the surface)
        if (it.W() < 0 || it.W() >= 0.05 || it.State() == TopAbs_ON) continue;
        BRepGProp_Face prop(it.Face());
        gp_Pnt q;
        gp_Vec m;
        prop.Normal(it.U(), it.V(), q, m);
        if (m.Magnitude() > 1e-12 && m.Normalized().Dot(s.n) < -0.9) { touching = true; break; }  // faces press on each other
      }
      if (touching) { out.push_back(s.n); break; }
    }
  }
  return out;
}

}  // namespace

std::vector<Part> load_assembly(const nlohmann::json& j, const std::string& base_dir) {
  std::vector<Part> parts;
  for (auto& q : j.at("parts")) {
    Part p;
    p.name = q.at("name");
    p.kind = q.at("kind");
    p.fastener = q.value("fastener", p.kind == "bolt" || p.kind == "plug");
    p.fixed = q.value("fixed", false);
    if (p.kind == "box") {
      gp_Pnt a = pnt_of(q.at("at"));
      auto sz = q.at("size");
      p.shape = BRepPrimAPI_MakeBox(a, sz[0].get<double>(), sz[1].get<double>(), sz[2].get<double>()).Shape();
    } else if (p.kind == "boxes") {
      for (auto& bx : q.at("boxes")) {
        auto sz = bx.at("size");
        TopoDS_Shape b = BRepPrimAPI_MakeBox(pnt_of(bx.at("at")), sz[0].get<double>(), sz[1].get<double>(), sz[2].get<double>()).Shape();
        p.shape = p.shape.IsNull() ? b : BRepAlgoAPI_Fuse(p.shape, b).Shape();
      }
    } else if (p.kind == "tube" || (p.kind == "plug" && q.contains("inner_diameter"))) {
      gp_Ax2 ax(pnt_of(q.at("at")), dir_of(q.at("axis")));
      double len = q.at("length");
      TopoDS_Shape outer = BRepPrimAPI_MakeCylinder(ax, q.at("diameter").get<double>() / 2, len).Shape();
      TopoDS_Shape inner = BRepPrimAPI_MakeCylinder(ax, q.at("inner_diameter").get<double>() / 2, len).Shape();
      p.shape = BRepAlgoAPI_Cut(outer, inner).Shape();
    } else if (p.kind == "cylinder" || p.kind == "plug") {
      gp_Ax2 ax(pnt_of(q.at("at")), dir_of(q.at("axis")));
      p.shape = BRepPrimAPI_MakeCylinder(ax, q.at("diameter").get<double>() / 2, q.at("length").get<double>()).Shape();
    } else if (p.kind == "bolt") {
      // ISO 4017-like hex bolt: "at" is the centre of the head's underside, "axis" points along the shank
      gp_Pnt c = pnt_of(q.at("at"));
      gp_Dir ax = dir_of(q.at("axis"));
      double d = q.at("diameter"), len = q.at("length");
      double af = q.value("head_across_flats", 1.6 * d + 0.4), hh = q.value("head_height", 0.7 * d);
      TopoDS_Shape shank = BRepPrimAPI_MakeCylinder(gp_Ax2(c, ax), d / 2, len).Shape();
      TopoDS_Shape head = hex_prism(c, ax.Reversed(), af, hh);
      p.shape = BRepAlgoAPI_Fuse(shank, head).Shape();
    } else if (p.kind == "step") {
      STEPControl_Reader r;
      fs::path f = fs::path(q.at("file").get<std::string>());
      if (f.is_relative()) f = fs::path(base_dir) / f;
      if (r.ReadFile(f.string().c_str()) != IFSelect_RetDone) throw std::runtime_error("cannot read " + f.string());
      r.TransferRoots();
      p.shape = r.OneShape();
    } else {
      throw std::runtime_error("part '" + p.name + "': unknown kind '" + p.kind + "'");
    }
    for (auto& h : q.value("holes", nlohmann::json::array())) {  // drilled holes, in the part's own frame
      gp_Ax2 ax(pnt_of(h.at("at")), dir_of(h.at("axis")));
      p.shape = BRepAlgoAPI_Cut(p.shape, BRepPrimAPI_MakeCylinder(ax, h.at("diameter").get<double>() / 2, h.at("depth").get<double>()).Shape()).Shape();
    }
    if (q.contains("rotate") || q.contains("translate")) p.shape = transform(p.shape, q);
    parts.push_back(std::move(p));
  }
  return parts;
}

nlohmann::json overlaps(const std::vector<Part>& parts, double tol) {
  nlohmann::json out = nlohmann::json::array();
  for (size_t i = 0; i < parts.size(); ++i)
    for (size_t k = i + 1; k < parts.size(); ++k) {
      if (box_of(parts[i].shape).IsOut(box_of(parts[k].shape))) continue;
      BRepAlgoAPI_Common c(parts[i].shape, parts[k].shape);
      double v = c.IsDone() ? volume(c.Shape()) : 0;
      if (v > tol) out.push_back({{"a", parts[i].name}, {"b", parts[k].name}, {"volume_mm3", v}});
    }
  return out;
}

bool can_slide(const Part& p, const std::vector<const Part*>& others, const V3& dd, int samples) {
  gp_Vec d(dd[0], dd[1], dd[2]);
  d.Normalize();
  Bnd_Box bp = box_of(p.shape);
  std::vector<const Part*> nearby;  // ("near" is a macro in the Windows headers)
  for (auto* q : others)
    if (boxes_meet_along(bp, box_of(q->shape), d)) nearby.push_back(q);
  if (nearby.empty()) return true;
  auto mine = surface_samples(p.shape, samples, 7);
  for (auto* q : nearby) {
    for (auto& s : mine)  // P's surface moving along d into Q
      if (ray_enters(s.p, s.n, d, q->shape)) return false;
    for (auto& s : surface_samples(q->shape, samples, 11))  // Q's surface, seen from P, moving along -d into P
      if (ray_enters(s.p, s.n, -d, p.shape)) return false;
  }
  return true;
}

nlohmann::json plan(const std::vector<Part>& parts) {
  std::vector<size_t> remaining;
  for (size_t i = 0; i < parts.size(); ++i) remaining.push_back(i);
  auto movable = [&](size_t i) { return !parts[i].fixed; };
  auto top_z = [&](size_t i) {
    double x0, y0, z0, x1, y1, z1;
    box_of(parts[i].shape).Get(x0, y0, z0, x1, y1, z1);
    return z1;
  };
  const V3 axes[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
  nlohmann::json removal = nlohmann::json::array();
  while (std::any_of(remaining.begin(), remaining.end(), movable)) {
    // take parts off from the top down (the crate before the shelf, the shelf before the bracket): reversed, the
    // assembly builds up from the fixed surroundings like a real installation
    std::vector<size_t> order;
    for (size_t i : remaining)
      if (movable(i)) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return top_z(a) > top_z(b) + 1e-6; });
    bool moved = false;
    for (size_t i : order) {
      std::vector<const Part*> others;
      for (size_t k : remaining)
        if (k != i) others.push_back(&parts[k]);
      // among the free directions, take the one that opens the most contact (moving straight off the face it sits
      // on, as a fitter would), not merely any direction that happens to be free
      std::vector<V3> free_dirs;
      for (auto& d : axes)
        if (can_slide(parts[i], others, d)) free_dirs.push_back(d);
      if (!free_dirs.empty()) {
        auto contacts = contact_normals(parts[i], others);
        // gravity: a part resting on something (a contact facing down) is lowered into place, so lifting it is preferred
        bool rests = std::any_of(contacts.begin(), contacts.end(), [](const gp_Vec& c) { return c.Z() < -0.9; });
        if (std::getenv("ASSEMBLY_DEBUG")) {
          int cx[6] = {0, 0, 0, 0, 0, 0};
          for (auto& c : contacts) {
            if (c.X() > 0.9) cx[0]++; if (c.X() < -0.9) cx[1]++; if (c.Y() > 0.9) cx[2]++; if (c.Y() < -0.9) cx[3]++;
            if (c.Z() > 0.9) cx[4]++; if (c.Z() < -0.9) cx[5]++;
          }
          std::cerr << parts[i].name << ": contacts +x " << cx[0] << " -x " << cx[1] << " +y " << cx[2] << " -y " << cx[3] << " +z " << cx[4]
                    << " -z " << cx[5] << " | free:";
          for (auto& d : free_dirs) std::cerr << " (" << d[0] << "," << d[1] << "," << d[2] << ")";
          std::cerr << " | rests " << rests << "\n";
        }
        std::stable_sort(free_dirs.begin(), free_dirs.end(), [&](const V3& a, const V3& b) {
          auto score = [&](const V3& d) {
            int n = 0;
            for (auto& c : contacts) n += (c.X() * d[0] + c.Y() * d[1] + c.Z() * d[2]) < -0.5;
            return n;
          };
          return score(a) > score(b);
        });
        if (rests) {
          auto up = std::find(free_dirs.begin(), free_dirs.end(), V3{0, 0, 1});
          if (up != free_dirs.end()) std::rotate(free_dirs.begin(), up, up + 1);
        }
      }
      for (auto& d : free_dirs) {
        Bnd_Box b = box_of(parts[i].shape);
        double x0, y0, z0, x1, y1, z1;
        b.Get(x0, y0, z0, x1, y1, z1);
        double extent = std::abs(d[0]) * (x1 - x0) + std::abs(d[1]) * (y1 - y0) + std::abs(d[2]) * (z1 - z0);
        removal.push_back({{"part", parts[i].name}, {"remove_direction", d}, {"insert_direction", V3{-d[0], -d[1], -d[2]}},
                           {"travel_mm", extent + 40.0}});
        remaining.erase(std::find(remaining.begin(), remaining.end(), i));
        moved = true;
        break;
      }
      if (moved) break;
    }
    if (!moved) {
      nlohmann::json locked = nlohmann::json::array();
      for (size_t i : remaining)
        if (movable(i)) locked.push_back(parts[i].name);
      return {{"ok", false}, {"locked", locked},
              {"message", "no remaining part can slide out along any axis: these parts lock each other, so they cannot be "
                          "assembled in this configuration (change a joint or split a part)"}};
    }
  }
  std::reverse(removal.begin(), removal.end());
  nlohmann::json steps = nlohmann::json::array();
  int k = 0;
  for (auto& r : removal) {
    r["step"] = ++k;
    r.erase("remove_direction");
    steps.push_back(r);
  }
  return {{"ok", true}, {"steps", steps}};
}

void write_parts(const std::vector<Part>& parts, const std::string& dir) {
  fs::create_directories(dir);
  for (auto& p : parts) {
    BRepMesh_IncrementalMesh mesh(p.shape, 0.05, false, 0.3, true);
    StlAPI_Writer w;
    w.ASCIIMode() = false;
    w.Write(p.shape, (fs::path(dir) / (p.name + ".stl")).string().c_str());
  }
}

}  // namespace attome
