// DesignGraph: an ordered list of typed grammar features over named, bounded design parameters.
// Validation (resolve) runs before any geometry is built and rejects unknown types or parameters, missing values,
// expressions over undeclared names, out-of-bounds values and references to features that are not earlier in the
// graph, with a message that names the feature and parameter.
#pragma once
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "structura/expr.hpp"

namespace structura {

using ojson = nlohmann::ordered_json;

struct GraphError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------- feature type registry
struct ParamSpec {
  std::string name;
  std::string kind;  // length | pos | count | angle | axis | op | points | vec | bbox | ref
  std::string doc;
  std::optional<ojson> def;  // empty = required
};

struct FeatureType {
  std::string name;
  std::string doc;
  std::vector<ParamSpec> params;
  bool modifies = false;  // fillet, chamfer, shell act on the current part
  bool pattern = false;   // linear_pattern, circular_pattern repeat an earlier feature's primitives
  const ParamSpec* find(const std::string& p) const;
};

const std::map<std::string, FeatureType>& feature_types();
bool is_axis(const std::string& a);

// ---------------------------------------------------------------- graph model
struct Param {
  std::string unit = "mm";
  double low = 0, high = 0, def = 0;
  bool integer = false;
  std::string doc;
};

struct Feature {
  std::string id, type, doc;
  ojson params = ojson::object();
};

struct FaceSelector {
  std::string kind;  // plane | cylinder | picked
  std::string normal, axis;
  ojson at, radius;
  ojson signature;  // picked: surface, direction, where (fractions of the bounding box), area_fraction (C3)
  std::optional<std::vector<ojson>> region;
  std::string doc;
};

// Evaluated values of one feature.
struct Resolved {
  const Feature* feature = nullptr;
  const FeatureType* type = nullptr;
  std::map<std::string, double> num;
  std::map<std::string, std::string> str;
  std::map<std::string, std::vector<std::vector<double>>> pts;
  std::map<std::string, std::vector<double>> vec;
  double n(const std::string& k) const;
  const std::string& s(const std::string& k) const;
};

struct DesignGraph {
  std::string name, version = "1", doc;
  std::vector<std::pair<std::string, Param>> parameters;
  std::vector<Feature> features;
  std::vector<std::pair<std::string, FaceSelector>> faces;
  std::optional<std::vector<ojson>> design_space;
  // frozen interfaces (A3): boxes [x0 y0 z0 x1 y1 z1 volume_mm3]; a design keeps at least that much material in each box
  std::vector<std::vector<double>> preserve;

  static DesignGraph from_json(const ojson& j);
  static DesignGraph load(const std::string& path);
  ojson to_json() const;
  std::string canonical_json() const;  // sorted keys, no whitespace

  const Param* param(const std::string& k) const;
  Values values(const Values& overrides = {}) const;                 // defaults + checked overrides
  std::vector<Resolved> resolve(const Values& overrides = {}) const;  // full validation
  std::set<std::string> referenced_names() const;
  std::optional<std::vector<double>> design_space_values(const Values& v) const;
};

double eval_value(const ojson& raw, const Values& v);  // number or expression

}  // namespace structura
