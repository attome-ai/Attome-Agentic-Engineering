// Requirement traceability and spec versions (A2-T4).
// trace(): one row per requirement: id, the brief sentence or default it came from, the spec field, the checker,
// the value and margin, and the evidence file. diff(): what changed between two spec versions.
#pragma once
#include <string>

#include <nlohmann/json.hpp>

#include "structura/spec.hpp"

namespace structura {

nlohmann::json trace(const DesignSpec& spec, const nlohmann::json& evaluation, const std::string& evidence_file);
nlohmann::json spec_diff(const nlohmann::json& a, const nlohmann::json& b);  // list of {path, old, new}
std::string save_spec_version(const nlohmann::json& spec, const std::string& folder);  // writes spec.v<n>.json, returns path

}  // namespace structura
