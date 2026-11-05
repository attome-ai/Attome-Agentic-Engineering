#include "attome/fasteners.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

namespace attome {
namespace {

Bnd_Box bbox(const TopoDS_Shape& s) {
  Bnd_Box b;
  BRepBndLib::AddOptimal(s, b, false, false);
  return b;
}

gp_Pnt centroid(const TopoDS_Shape& s) {
  GProp_GProps g;
  BRepGProp::VolumeProperties(s, g);
  return g.CentreOfMass();
}

const nlohmann::json* describe(const nlohmann::json& description, const std::string& name) {
  for (auto& q : description.at("parts"))
    if (q.value("name", "") == name) return &q;
  return nullptr;
}

}  // namespace

Thread iso_thread(double d) {
  // ISO 261 coarse pitch, ISO 898-1 tensile stress area As = pi/4 ((d2 + d3) / 2)^2
  static const std::map<int, Thread> table = {
      {3, {3, 0.5, 5.03}},    {4, {4, 0.7, 8.78}},    {5, {5, 0.8, 14.2}},    {6, {6, 1.0, 20.1}},    {8, {8, 1.25, 36.6}},
      {10, {10, 1.5, 58.0}},  {12, {12, 1.75, 84.3}}, {14, {14, 2.0, 115.0}}, {16, {16, 2.0, 157.0}}, {20, {20, 2.5, 245.0}},
      {22, {22, 2.5, 303.0}}, {24, {24, 3.0, 353.0}}, {27, {27, 3.0, 459.0}}, {30, {30, 3.5, 561.0}}};
  auto it = table.find(static_cast<int>(std::lround(d)));
  if (it == table.end() || std::abs(d - it->first) > 1e-6)
    throw std::runtime_error("no ISO coarse thread for a " + std::to_string(d) + " mm bolt (M3 to M30 are tabulated)");
  return it->second;
}

Grade bolt_grade(const std::string& name) {
  // EN 1993-1-8 Table 3.1 (fyb, fub) and Table 3.4 (alpha_v)
  static const std::map<std::string, Grade> table = {
      {"4.6", {"4.6", 400, 240, 0.6}}, {"4.8", {"4.8", 400, 320, 0.5}}, {"5.6", {"5.6", 500, 300, 0.6}},
      {"5.8", {"5.8", 500, 400, 0.5}}, {"6.8", {"6.8", 600, 480, 0.5}}, {"8.8", {"8.8", 800, 640, 0.6}},
      {"10.9", {"10.9", 1000, 900, 0.5}}};
  auto it = table.find(name);
  if (it == table.end()) throw std::runtime_error("bolt grade " + name + " is not covered by EN 1993-1-8 (4.6 to 10.9)");
  return it->second;
}

Resistance en1993_resistance(const Thread& t, const Grade& g, double gamma_M2) {
  return {0.9 * g.fub * t.As / gamma_M2, g.alpha_v * g.fub * t.As / gamma_M2};
}

double interaction(double Fv_Ed, double Ft_Ed, const Resistance& r) { return Fv_Ed / r.Fv_Rd + Ft_Ed / (1.4 * r.Ft_Rd); }

nlohmann::json wall_bolt_check(const std::vector<Part>& parts, const nlohmann::json& description) {
  using nlohmann::json;
  const double g = 9.81;
  std::vector<size_t> bolts;
  for (size_t i = 0; i < parts.size(); ++i)
    if (parts[i].kind == "bolt") bolts.push_back(i);
  if (bolts.empty()) return {{"evaluated", false}, {"reason", "no bolts in the assembly"}};

  // the bolts' axis: all horizontal and parallel, or this simple statics does not apply
  const json* d0 = describe(description, parts[bolts[0]].name);
  gp_Vec axis(d0->at("axis")[0].get<double>(), d0->at("axis")[1].get<double>(), d0->at("axis")[2].get<double>());
  axis.Normalize();
  for (size_t b : bolts) {
    const json* d = describe(description, parts[b].name);
    gp_Vec a(d->at("axis")[0].get<double>(), d->at("axis")[1].get<double>(), d->at("axis")[2].get<double>());
    if (a.Normalized().Dot(axis) < 1 - 1e-6)
      return {{"evaluated", false}, {"reason", "the bolts are not parallel: the wall-plate statics used here does not apply"}};
  }
  if (std::abs(axis.Z()) > 1e-6)
    return {{"evaluated", false}, {"reason", "the bolts are not horizontal: the wall-plate statics used here does not apply"}};

  // the wall (fixed part the bolts enter) and the held part (the moving, non-fastener part the bolts pass through)
  Bnd_Box bb = bbox(parts[bolts[0]].shape);
  const Part* wall = nullptr;
  const Part* held = nullptr;
  for (auto& p : parts) {
    if (p.kind == "bolt" || bbox(p.shape).IsOut(bb)) continue;
    const json* d = describe(description, p.name);
    bool fastener = d && d->value("fastener", false);
    if (p.fixed && !wall) wall = &p;
    else if (!p.fixed && !fastener && !held) held = &p;
  }
  if (!wall || !held) return {{"evaluated", false}, {"reason", "could not find the wall and the part the bolts hold"}};

  // load: weight of every movable part with a stated mass, at its centroid
  double W = 0, moment_arm_sum = 0;
  json loads = json::array();
  const gp_Vec out = -axis;  // out of the wall
  double x0, y0, z0, x1, y1, z1;
  bbox(wall->shape).Get(x0, y0, z0, x1, y1, z1);
  // the wall face: the farthest point of the wall's box along `out` (the box's support function)
  const double face = std::max(out.X() * x0, out.X() * x1) + std::max(out.Y() * y0, out.Y() * y1);
  for (auto& p : parts) {
    const json* d = describe(description, p.name);
    if (p.fixed || !d || !d->contains("mass_kg")) continue;
    double m = d->at("mass_kg").get<double>();
    gp_Pnt c = centroid(p.shape);
    double e = c.X() * out.X() + c.Y() * out.Y() - face;
    W += m * g;
    moment_arm_sum += m * g * e;
    loads.push_back({{"part", p.name}, {"mass_kg", m}, {"lever_mm", e}});
  }
  if (W <= 0) return {{"evaluated", false}, {"reason", "no part states a mass (\"mass_kg\"): there is no load to check"}};
  const double M = moment_arm_sum;  // N mm
  const double e = M / W;

  bbox(held->shape).Get(x0, y0, z0, x1, y1, z1);
  const double zp = z0;  // the plate pivots about its lowest edge, pressing on the wall there
  double sum_sq = 0;
  for (size_t b : bolts) {
    double z = centroid(parts[b].shape).Z() - zp;
    sum_sq += z * z;
  }
  if (sum_sq <= 0) return {{"evaluated", false}, {"reason", "the bolts sit at the pivot edge: no lever to resist the moment"}};

  json rows = json::array();
  bool all_ok = true, any_plug_unchecked = false;
  double worst = 0;
  for (size_t b : bolts) {
    const json* d = describe(description, parts[b].name);
    const double dia = d->at("diameter").get<double>();
    const bool stated = d->contains("grade");
    const std::string gname = d->value("grade", "4.6");
    Thread t = iso_thread(dia);
    Grade gr = bolt_grade(gname);
    Resistance r = en1993_resistance(t, gr);
    gp_Pnt c = centroid(parts[b].shape);
    const double T = M * (c.Z() - zp) / sum_sq;
    const double V = W / bolts.size();
    const double ratio = interaction(V, std::max(T, 0.0), r);
    worst = std::max(worst, ratio);
    json row = {{"bolt", parts[b].name}, {"size", "M" + std::to_string(static_cast<int>(dia))}, {"grade", gname},
                {"grade_stated", stated}, {"tension_N", T}, {"shear_N", V}, {"Ft_Rd_N", r.Ft_Rd}, {"Fv_Rd_N", r.Fv_Rd},
                {"interaction", ratio}, {"steel_ok", ratio <= 1.0}};
    all_ok = all_ok && ratio <= 1.0;
    // the plug on this bolt's axis, if any, and its datasheet tension load
    for (auto& p : parts) {
      const json* pd = describe(description, p.name);
      if (!pd || p.kind == "bolt" || !pd->value("fastener", false) || !pd->contains("axis")) continue;
      gp_Pnt pc = centroid(p.shape);
      gp_Vec off(c, pc);
      gp_Vec perp = off - axis * off.Dot(axis);
      if (perp.Magnitude() > 0.5) continue;
      row["plug"] = p.name;
      if (pd->contains("allowed_tension_kN")) {
        double allowed = pd->at("allowed_tension_kN").get<double>() * 1000.0;
        row["plug_allowed_N"] = allowed;
        row["plug_ratio"] = T / allowed;
        row["plug_ok"] = T <= allowed;
        all_ok = all_ok && T <= allowed;
      } else {
        row["plug_ok"] = nullptr;
        any_plug_unchecked = true;
      }
    }
    rows.push_back(row);
  }
  json result = {{"evaluated", true},
                 {"method", "EN 1993-1-8 Table 3.4 bolt resistances (gM2 = 1.25); wall plate pivoting about its lowest edge"},
                 {"load_N", W}, {"lever_mm", e}, {"moment_Nmm", M}, {"pivot_z_mm", zp}, {"held_part", held->name},
                 {"wall", wall->name}, {"loads", loads}, {"bolts", rows}, {"worst_interaction", worst},
                 {"verdict", !all_ok ? "fail" : (any_plug_unchecked ? "incomplete" : "pass")}};
  if (any_plug_unchecked)
    result["note"] = "wall plug pull-out is not checked: give the plug's datasheet tension load as \"allowed_tension_kN\" "
                     "(it depends on the plug and the wall material, and is never assumed)";
  return result;
}

}  // namespace attome
