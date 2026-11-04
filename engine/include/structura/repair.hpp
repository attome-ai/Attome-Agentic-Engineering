// Safe automatic repair of a candidate that failed the gate (B4-T2).
// Removes faces below the minimum feature size (tiny fillets, slivers) with OCCT defeaturing, merges same-domain faces,
// and heals small gaps. Every operation is logged. A repair that changes volume by more than max_volume_change or
// touches a frozen face is rejected. The repaired shape is gated again by the caller (repair_and_gate does both).
#pragma once
#include <string>
#include <vector>

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include "structura/gate.hpp"

namespace structura {

struct RepairOptions {
  double min_feature = 0.3;          // mm: faces with an edge shorter than this (or area below its square) are removed
  double max_volume_change = 0.02;   // relative
  std::vector<TopoDS_Face> frozen;   // interface faces that must not change
};

struct RepairResult {
  bool changed = false;
  bool accepted = false;
  TopoDS_Shape shape;                 // repaired shape when accepted, otherwise the input
  nlohmann::json log = nlohmann::json::array();
  std::string reason;                 // why a repair was rejected, if it was
};

RepairResult repair(const TopoDS_Shape& shape, const RepairOptions& opt);

struct RepairGateResult {
  GateResult before, after;
  RepairResult repair;
  bool ok = false;
  TopoDS_Shape shape;
  nlohmann::json to_json() const;
};
RepairGateResult repair_and_gate(const TopoDS_Shape& shape, const GateLimits& lim, const RepairOptions& opt);

}  // namespace structura
