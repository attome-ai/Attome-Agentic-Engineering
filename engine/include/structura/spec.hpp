// DesignSpec (A2): the typed, machine-checked contract a study runs from.
// Units are fixed by quantity name (mm, N, MPa, g, Hz). Requirements compile into executable checks that the trust
// panel evaluates; the LLM fills a spec but cannot change materials, checks or thresholds once a study runs.
#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "structura/fea.hpp"
#include "structura/graph.hpp"

namespace structura {

struct SpecError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Quantities a requirement or objective can name, with their units and direction of "better".
struct Quantity {
  std::string unit;
  bool higher_is_better;
  std::string doc;
};
const std::map<std::string, Quantity>& quantities();

struct Requirement {
  std::string id;        // R1, R2, ... (never renumbered between spec versions)
  std::string quantity;  // see quantities()
  std::string op;        // <= or >=
  double value = 0;
  std::string source;    // brief sentence, default, or derivation it came from
};

struct Objective {
  std::string quantity;
  std::string sense;  // min or max
};

struct SpecLoadCase {
  LoadCase lc;
  std::array<double, 2> factor_range{1.0, 1.0};  // loads are evaluated at the high end (worst case)
};

struct Check {
  std::string id, quantity, op, unit, load_case;
  double value = 0, limit = 0, margin = 0;  // margin > 0 means passing with room, relative to the limit
  bool passed = false;
  nlohmann::json to_json() const;
};

struct Metrics {  // per load case and overall
  std::map<std::string, std::map<std::string, double>> by_case;  // case -> quantity -> value
  std::map<std::string, double> overall;                         // governing (worst) value per quantity
  std::map<std::string, std::string> governing_case;             // quantity -> case that governs
};

struct DesignSpec {
  std::string id, brief, family;
  int version = 1;
  Values fixed_params;
  std::map<std::string, std::array<double, 2>> free_params;  // name -> [low, high] within the family's bounds
  std::string material = "PETG";
  bool apply_print_knockdown = true;
  std::array<double, 3> print_direction{0, 0, 1};
  std::vector<SpecLoadCase> load_cases;
  std::vector<ThermalCase> thermal_cases;  // steady heat transfer (C2); at least one structural or thermal case
  std::vector<Objective> objectives;
  std::vector<Requirement> requirements;
  double mesh_size = 4.0;
  double stress_exclude_near_bc_mm = 3.0;  // Saint-Venant exclusion for the strength check (stress_evaluation in the spec)
  nlohmann::json raw;  // as loaded, for versioning and diffs

  static DesignSpec from_json(const nlohmann::json& j);  // validates against the family, materials and quantities
  static DesignSpec load(const std::string& path);
  std::string hash() const;
  Material effective_material() const;  // with the print knock-down applied when requested
  std::vector<Check> check(const Metrics& m) const;
};

// Metric values for one solved load case (structural) given the effective material.
std::map<std::string, double> structural_metrics(const Result& r, const Material& eff);

}  // namespace structura
