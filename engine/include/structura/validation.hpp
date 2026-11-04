// The standing validation suite (F2): cases with analytical or NAFEMS references, run through the real engine path
// (geometry, gmsh, CalculiX). Used by the tests, by `structura validate` and by the trust panel's suite status.
#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace structura {

struct ValidationRow {
  std::string id, name, quantity, unit, reference_source;
  double reference = 0, value = 0, error_pct = 0, tolerance_pct = 0;
  bool pass = false;
  size_t dof = 0;
  double mesh_size = 0, seconds = 0;
};

std::vector<std::string> validation_cases();
ValidationRow run_validation_case(const std::string& id, const std::string& workdir, int threads = 4);
nlohmann::json to_json(const std::vector<ValidationRow>& rows);
std::string to_markdown(const std::vector<ValidationRow>& rows, const std::string& machine);

}  // namespace structura
