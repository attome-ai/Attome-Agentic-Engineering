#include "structura/graph.hpp"

#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

namespace structura {
namespace {

struct KindBounds {
  double lo, hi;
  const char* unit;
};
const std::map<std::string, KindBounds> kKinds = {
    {"length", {0.05, 5000.0, "mm"}},
    {"pos", {-5000.0, 5000.0, "mm"}},
    {"count", {1, 500, "1"}},
    {"angle", {0.0, 360.0, "deg"}},
};

std::string fmt(double v) {
  std::ostringstream o;
  o << v;
  return o.str();
}

ParamSpec P(std::string name, std::string kind, std::string doc, std::optional<ojson> def = std::nullopt) {
  return {std::move(name), std::move(kind), std::move(doc), std::move(def)};
}

std::map<std::string, FeatureType> build_registry() {
  const ojson OPADD = "add";
  std::vector<FeatureType> v = {
      {"plate", "Rectangular block from its minimum corner.",
       {P("x", "pos", "min corner x"), P("y", "pos", "min corner y"), P("z", "pos", "min corner z"),
        P("length", "length", "size along x"), P("width", "length", "size along y"),
        P("thickness", "length", "size along z"), P("op", "op", "add or cut", OPADD)}},
      {"extrude_sketch", "Closed polygon in an XY plane at height z, extruded along +z.",
       {P("points", "points", "polygon corners [[x, y], ...]"), P("z", "pos", "sketch plane height"),
        P("height", "length", "extrusion height"), P("op", "op", "add or cut", OPADD)}},
      {"boss", "Cylinder added to the part.",
       {P("x", "pos", "base centre x"), P("y", "pos", "base centre y"), P("z", "pos", "base centre z"),
        P("diameter", "length", "diameter"), P("height", "length", "height"), P("axis", "axis", "direction", "z")}},
      {"hole", "Cylindrical hole cut from the part.",
       {P("x", "pos", "entry centre x"), P("y", "pos", "entry centre y"), P("z", "pos", "entry centre z"),
        P("diameter", "length", "diameter"), P("depth", "length", "depth"), P("axis", "axis", "drilling direction", "-z")}},
      {"hole_pattern", "Grid of holes: n1 along the first and n2 along the second axis orthogonal to the drilling axis.",
       {P("x", "pos", "first hole x"), P("y", "pos", "first hole y"), P("z", "pos", "first hole z"),
        P("diameter", "length", "diameter"), P("depth", "length", "depth"), P("axis", "axis", "drilling direction", "-z"),
        P("n1", "count", "holes along the first direction", 1), P("d1", "pos", "spacing along the first direction", 0.0),
        P("n2", "count", "holes along the second direction", 1), P("d2", "pos", "spacing along the second direction", 0.0)}},
      {"rib", "Triangular gusset with its right-angle corner at (x, y, z), legs along two perpendicular axes, extruded by thickness along their cross product. tip_cut > 0 truncates both acute tips square to their legs (blunt ends avoid knife-edge stress singularities and print better).",
       {P("x", "pos", "corner x"), P("y", "pos", "corner y"), P("z", "pos", "corner z"),
        P("leg_a", "length", "first leg length"), P("leg_a_dir", "axis", "first leg direction"),
        P("leg_b", "length", "second leg length"), P("leg_b_dir", "axis", "second leg direction"),
        P("thickness", "length", "rib thickness"), P("tip_cut", "pos", "length cut off each acute tip, along its leg (0 = sharp)", 0.0)}},
      {"pocket", "Rectangular pocket cut downward from z_top, with optional rounded vertical corners.",
       {P("x", "pos", "min corner x"), P("y", "pos", "min corner y"), P("z_top", "pos", "top of the pocket"),
        P("length", "length", "size along x"), P("width", "length", "size along y"), P("depth", "length", "depth"),
        P("corner_radius", "pos", "vertical corner radius (0 for sharp)", 0.0)}},
      {"fillet", "Rounds straight edges parallel to edge_axis whose midpoints lie inside region; an optional second set (edge_axis_2, region_2) is rounded in the same operation, which OCCT needs when the two sets meet.",
       {P("radius", "length", "fillet radius"), P("edge_axis", "axis", "edge direction"),
        P("region", "bbox", "[xmin, ymin, zmin, xmax, ymax, zmax]"),
        P("edge_axis_2", "axis_opt", "second edge direction or none", "none"),
        P("region_2", "bbox_opt", "second region or [] for none", ojson::array())}, true},
      {"chamfer", "Chamfers straight edges parallel to edge_axis whose midpoints lie inside region.",
       {P("length", "length", "chamfer length"), P("edge_axis", "axis", "edge direction"),
        P("region", "bbox", "[xmin, ymin, zmin, xmax, ymax, zmax]")}, true},
      {"shell", "Hollows the part to a wall thickness, opening the outermost planar face(s) facing open_side.",
       {P("thickness", "length", "wall thickness"), P("open_side", "axis", "direction of the open face")}, true},
      {"linear_pattern", "Repeats the primitives of feature `target` count times, each offset by (dx, dy, dz).",
       {P("target", "ref", "id of an earlier feature"), P("count", "count", "total number including the original"),
        P("dx", "pos", "offset x", 0.0), P("dy", "pos", "offset y", 0.0), P("dz", "pos", "offset z", 0.0)}, false, true},
      {"circular_pattern", "Repeats the primitives of feature `target` count times around an axis through (cx, cy, cz).",
       {P("target", "ref", "id of an earlier feature"), P("count", "count", "total number including the original"),
        P("cx", "pos", "axis point x", 0.0), P("cy", "pos", "axis point y", 0.0), P("cz", "pos", "axis point z", 0.0),
        P("axis", "axis", "rotation axis", "z"), P("angle", "angle", "total angle; 360 spaces copies evenly", 360.0)}, false, true},
      {"import_step", "Imports the solid of an uploaded STEP file (A3); sha256 pins the file content so a changed file never reuses "
                      "results. Later features may add to or cut from it.",
       {P("file", "file", "STEP file path, absolute or relative to the repository"), P("sha256", "hash", "SHA-256 of the file (hex)")}},
      {"sweep_path", "Circular profile swept along a polyline path.",
       {P("points", "points", "path points [[x, y, z], ...]"), P("radius", "length", "profile radius"),
        P("op", "op", "add or cut", OPADD)}},
      {"fin_array", "Row of rectangular fins along +y from (x, y, z): each fin is length (x) by fin_thickness (y) by height (z).",
       {P("x", "pos", "first fin min x"), P("y", "pos", "first fin min y"), P("z", "pos", "fin base z"),
        P("count", "count", "number of fins"), P("pitch", "length", "fin spacing along y"),
        P("length", "length", "fin length along x"), P("fin_thickness", "length", "fin thickness"),
        P("height", "length", "fin height")}},
  };
  std::map<std::string, FeatureType> m;
  for (auto& f : v) m[f.name] = f;
  return m;
}

double check_kind(const std::string& kind, double v, const std::string& where) {
  const auto& b = kKinds.at(kind);
  if (v < b.lo || v > b.hi)
    throw GraphError(where + " = " + fmt(v) + " outside physical bounds [" + fmt(b.lo) + ", " + fmt(b.hi) + "] " + b.unit);
  if (kind == "count") {
    if (std::fabs(v - std::round(v)) > 1e-9) throw GraphError(where + " must be a whole number, got " + fmt(v));
    v = std::round(v);
  }
  return v;
}

}  // namespace

const std::map<std::string, FeatureType>& feature_types() {
  static const auto reg = build_registry();
  return reg;
}

bool is_axis(const std::string& a) {
  static const std::set<std::string> ax = {"x", "y", "z", "-x", "-y", "-z"};
  return ax.count(a) > 0;
}

const ParamSpec* FeatureType::find(const std::string& p) const {
  for (auto& s : params)
    if (s.name == p) return &s;
  return nullptr;
}

double Resolved::n(const std::string& k) const {
  auto it = num.find(k);
  if (it == num.end()) throw GraphError("internal: missing numeric value " + k);
  return it->second;
}
const std::string& Resolved::s(const std::string& k) const {
  auto it = str.find(k);
  if (it == str.end()) throw GraphError("internal: missing text value " + k);
  return it->second;
}

double eval_value(const ojson& raw, const Values& v) {
  if (raw.is_boolean()) throw ExprError("booleans are not numeric values");
  if (raw.is_number()) return raw.get<double>();
  if (raw.is_string()) return evaluate(raw.get<std::string>(), v);
  throw ExprError("value " + raw.dump() + " is neither a number nor an expression");
}

// ---------------------------------------------------------------- io
DesignGraph DesignGraph::from_json(const ojson& j) {
  DesignGraph g;
  try {
    g.name = j.at("name").get<std::string>();
    g.version = j.value("version", std::string("1"));
    g.doc = j.value("doc", std::string());
    if (j.contains("parameters")) {
      for (auto& [k, p] : j.at("parameters").items()) {
        Param q;
        q.unit = p.value("unit", std::string("mm"));
        q.low = p.at("low").get<double>();
        q.high = p.at("high").get<double>();
        q.def = p.at("default").get<double>();
        q.integer = p.value("integer", false);
        q.doc = p.value("doc", std::string());
        if (q.def < q.low || q.def > q.high)
          throw GraphError("parameter '" + k + "': default " + fmt(q.def) + " outside [" + fmt(q.low) + ", " + fmt(q.high) + "]");
        g.parameters.emplace_back(k, q);
      }
    }
    static const std::regex idre("^[a-z][a-z0-9_]*$");
    for (auto& f : j.at("features")) {
      Feature x;
      x.id = f.at("id").get<std::string>();
      if (!std::regex_match(x.id, idre)) throw GraphError("feature id '" + x.id + "' must match ^[a-z][a-z0-9_]*$");
      x.type = f.at("type").get<std::string>();
      x.doc = f.value("doc", std::string());
      x.params = f.value("params", ojson::object());
      g.features.push_back(std::move(x));
    }
    if (j.contains("faces")) {
      for (auto& [k, s] : j.at("faces").items()) {
        FaceSelector fs;
        fs.kind = s.at("kind").get<std::string>();
        if (fs.kind != "plane" && fs.kind != "cylinder" && fs.kind != "picked") throw GraphError("face '" + k + "': kind must be plane, cylinder or picked");
        fs.normal = s.value("normal", std::string());
        fs.axis = s.value("axis", std::string());
        fs.at = s.value("at", ojson());
        fs.radius = s.value("radius", ojson());
        fs.signature = s.value("signature", ojson());
        if (s.contains("region") && !s.at("region").is_null()) fs.region = s.at("region").get<std::vector<ojson>>();
        fs.doc = s.value("doc", std::string());
        g.faces.emplace_back(k, fs);
      }
    }
    if (j.contains("preserve")) {
      g.preserve = j.at("preserve").get<std::vector<std::vector<double>>>();
      for (auto& b : g.preserve)
        if (b.size() != 7) throw GraphError("each preserve entry needs [x0, y0, z0, x1, y1, z1, volume_mm3]");
    }
    if (j.contains("design_space") && !j.at("design_space").is_null())
      g.design_space = j.at("design_space").get<std::vector<ojson>>();
  } catch (const GraphError&) {
    throw;
  } catch (const std::exception& e) {
    throw GraphError(std::string("invalid design graph JSON: ") + e.what());
  }
  return g;
}

DesignGraph DesignGraph::load(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw GraphError("cannot open " + path);
  return from_json(ojson::parse(in));
}

ojson DesignGraph::to_json() const {
  ojson j;
  j["name"] = name;
  j["version"] = version;
  j["doc"] = doc;
  ojson ps = ojson::object();
  for (auto& [k, p] : parameters)
    ps[k] = {{"unit", p.unit}, {"low", p.low}, {"high", p.high}, {"default", p.def}, {"integer", p.integer}, {"doc", p.doc}};
  j["parameters"] = ps;
  ojson fs = ojson::array();
  for (auto& f : features) fs.push_back({{"id", f.id}, {"type", f.type}, {"params", f.params}, {"doc", f.doc}});
  j["features"] = fs;
  ojson fa = ojson::object();
  for (auto& [k, s] : faces) {
    ojson x = {{"kind", s.kind}, {"doc", s.doc}};
    if (!s.normal.empty()) x["normal"] = s.normal;
    if (!s.axis.empty()) x["axis"] = s.axis;
    if (!s.at.is_null()) x["at"] = s.at;
    if (!s.radius.is_null()) x["radius"] = s.radius;
    if (!s.signature.is_null()) x["signature"] = s.signature;
    if (s.region) x["region"] = *s.region;
    fa[k] = x;
  }
  j["faces"] = fa;
  j["design_space"] = design_space ? ojson(*design_space) : ojson();
  if (!preserve.empty()) j["preserve"] = preserve;  // only when present: existing families keep their canonical JSON and hashes
  return j;
}

std::string DesignGraph::canonical_json() const {
  nlohmann::json sorted = nlohmann::json::parse(to_json().dump());  // std::map keys -> sorted
  return sorted.dump();
}

// ---------------------------------------------------------------- values and validation
const Param* DesignGraph::param(const std::string& k) const {
  for (auto& [n, p] : parameters)
    if (n == k) return &p;
  return nullptr;
}

Values DesignGraph::values(const Values& overrides) const {
  Values v;
  for (auto& [k, p] : parameters) v[k] = p.def;
  for (auto& [k, x] : overrides) {
    if (!param(k)) throw GraphError("unknown parameter '" + k + "'");
    v[k] = x;
  }
  for (auto& [k, x] : v) {
    const Param* p = param(k);
    if (x < p->low - 1e-9 || x > p->high + 1e-9)
      throw GraphError("parameter '" + k + "' = " + fmt(x) + " outside [" + fmt(p->low) + ", " + fmt(p->high) + "] " + p->unit);
    if (p->integer) {
      if (std::fabs(x - std::round(x)) > 1e-9) throw GraphError("parameter '" + k + "' must be an integer, got " + fmt(x));
      x = std::round(x);
    }
  }
  return v;
}

std::vector<Resolved> DesignGraph::resolve(const Values& overrides) const {
  Values vals = values(overrides);
  std::map<std::string, std::string> seen;
  std::vector<Resolved> out;
  if (features.empty()) throw GraphError("graph has no features");
  const auto& reg = feature_types();
  for (size_t i = 0; i < features.size(); ++i) {
    const Feature& f = features[i];
    std::string where = "feature '" + f.id + "' (" + f.type + ")";
    if (seen.count(f.id)) throw GraphError(where + ": duplicate id");
    auto it = reg.find(f.type);
    if (it == reg.end()) {
      std::string known;
      for (auto& [k, _] : reg) known += (known.empty() ? "" : ", ") + k;
      throw GraphError(where + ": unknown feature type; known types: " + known);
    }
    const FeatureType& ft = it->second;
    for (auto& [k, _] : f.params.items())
      if (!ft.find(k)) throw GraphError(where + ": unknown parameter '" + k + "'");
    Resolved r;
    r.feature = &f;
    r.type = &ft;
    for (auto& spec : ft.params) {
      std::string w = where + "." + spec.name;
      ojson raw = f.params.contains(spec.name) ? f.params.at(spec.name) : (spec.def ? *spec.def : ojson());
      if (raw.is_null()) throw GraphError(where + ": missing parameter '" + spec.name + "'");
      try {
        if (kKinds.count(spec.kind)) {
          r.num[spec.name] = check_kind(spec.kind, eval_value(raw, vals), w);
        } else if (spec.kind == "axis") {
          if (!raw.is_string() || !is_axis(raw.get<std::string>()))
            throw GraphError(w + " = " + raw.dump() + " is not one of -x, -y, -z, x, y, z");
          r.str[spec.name] = raw.get<std::string>();
        } else if (spec.kind == "axis_opt") {
          if (!raw.is_string() || (raw != "none" && !is_axis(raw.get<std::string>())))
            throw GraphError(w + " = " + raw.dump() + " is not none or one of -x, -y, -z, x, y, z");
          r.str[spec.name] = raw.get<std::string>();
        } else if (spec.kind == "bbox_opt") {
          if (!raw.is_array() || (!raw.empty() && raw.size() != 6)) throw GraphError(w + " needs 6 values or []");
          std::vector<double> q;
          for (auto& c : raw) q.push_back(check_kind("pos", eval_value(c, vals), w));
          r.vec[spec.name] = q;
        } else if (spec.kind == "file") {
          if (!raw.is_string() || raw.get<std::string>().empty()) throw GraphError(w + " must be a file path");
          r.str[spec.name] = raw.get<std::string>();
        } else if (spec.kind == "hash") {
          static const std::regex hex("^[0-9a-f]{64}$");
          if (!raw.is_string() || !std::regex_match(raw.get<std::string>(), hex)) throw GraphError(w + " must be 64 lowercase hex digits");
          r.str[spec.name] = raw.get<std::string>();
        } else if (spec.kind == "op") {
          if (!raw.is_string() || (raw != "add" && raw != "cut")) throw GraphError(w + " must be 'add' or 'cut'");
          r.str[spec.name] = raw.get<std::string>();
        } else if (spec.kind == "ref") {
          std::string t = raw.is_string() ? raw.get<std::string>() : raw.dump();
          if (!seen.count(t)) throw GraphError(w + " refers to '" + t + "', which is not an earlier feature");
          const FeatureType& tt = reg.at(seen.at(t));
          if (tt.modifies || tt.pattern) throw GraphError(w + " refers to '" + t + "', which has no primitives to repeat");
          r.str[spec.name] = t;
        } else if (spec.kind == "points") {
          if (!raw.is_array() || raw.size() < 2) throw GraphError(w + " needs at least two points");
          std::vector<std::vector<double>> pts;
          for (auto& p : raw) {
            if (!p.is_array() || p.size() < 2 || p.size() > 3) throw GraphError(w + ": each point needs 2 or 3 coordinates");
            std::vector<double> q;
            for (auto& c : p) q.push_back(check_kind("pos", eval_value(c, vals), w));
            pts.push_back(q);
          }
          r.pts[spec.name] = pts;
        } else if (spec.kind == "vec" || spec.kind == "bbox") {
          size_t n = spec.kind == "vec" ? 3 : 6;
          if (!raw.is_array() || raw.size() != n) throw GraphError(w + " needs " + std::to_string(n) + " values");
          std::vector<double> q;
          for (auto& c : raw) q.push_back(check_kind("pos", eval_value(c, vals), w));
          r.vec[spec.name] = q;
        }
      } catch (const ExprError& e) {
        throw GraphError(w + ": " + e.what());
      }
    }
    bool cuts = (r.str.count("op") && r.str.at("op") == "cut") || f.type == "hole" || f.type == "hole_pattern" || f.type == "pocket";
    if (i == 0 && (ft.modifies || ft.pattern || cuts)) throw GraphError(where + ": the first feature must add material");
    seen[f.id] = f.type;
    out.push_back(std::move(r));
  }
  for (auto& [name, sel] : faces) {
    std::string w = "face '" + name + "'";
    try {
      if (sel.kind == "plane") {
        if (!is_axis(sel.normal)) throw GraphError(w + ": a plane selector needs a normal");
        if (!sel.at.is_null()) eval_value(sel.at, vals);  // no "at": every planar face with this normal
      } else if (sel.kind == "picked") {
        for (const char* k : {"surface", "direction", "where", "area_fraction"})
          if (!sel.signature.is_object() || !sel.signature.contains(k)) throw GraphError(w + ": a picked selector needs signature." + k);
      } else {
        if (!is_axis(sel.axis) || sel.radius.is_null()) throw GraphError(w + ": a cylinder selector needs axis and radius");
        eval_value(sel.radius, vals);
      }
      if (sel.region) {
        if (sel.region->size() != 6) throw GraphError(w + ": region needs 6 values");
        for (auto& c : *sel.region) eval_value(c, vals);
      }
    } catch (const ExprError& e) {
      throw GraphError(w + ": " + e.what());
    }
  }
  return out;
}

std::set<std::string> DesignGraph::referenced_names() const {
  std::set<std::string> out;
  std::function<void(const ojson&)> walk = [&](const ojson& x) {
    if (x.is_string()) {
      try {
        for (auto& n : names_in(x.get<std::string>())) out.insert(n);
      } catch (const ExprError&) {
      }
    } else if (x.is_array() || x.is_object()) {
      for (auto& y : x) walk(y);
    }
  };
  for (auto& f : features) walk(f.params);
  for (auto& [_, s] : faces) {
    walk(s.at);
    walk(s.radius);
    if (s.region) walk(ojson(*s.region));
  }
  if (design_space) walk(ojson(*design_space));
  return out;
}

std::optional<std::vector<double>> DesignGraph::design_space_values(const Values& v) const {
  if (!design_space) return std::nullopt;
  std::vector<double> out;
  for (auto& c : *design_space) out.push_back(eval_value(c, v));
  return out;
}

}  // namespace structura
