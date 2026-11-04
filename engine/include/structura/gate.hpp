// Geometry validity gate, run on every candidate before meshing.
// Hard checks (any failure rejects the shape with a named reason): valid B-rep, exactly one solid, positive volume,
// inside the design space, outside keep-out boxes, minimum wall thickness, minimum feature (edge) size, minimum hole
// diameter. Print overhang is measured and reported; it rejects only when a support-free part is required.
// gate() never repairs anything.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace structura {

struct GateLimits {
  std::optional<std::vector<double>> design_space;
  std::vector<std::vector<double>> keep_out;
  double min_wall = 1.2;           // mm, three FDM perimeters at 0.4 mm
  double min_feature = 0.3;        // mm, shortest allowed edge
  double min_hole_diameter = 2.0;  // mm
  std::array<double, 3> print_direction{0, 0, 1};
  double max_overhang_deg = 45.0;  // from vertical
  bool require_support_free = false;
  int wall_samples = 200;
  unsigned seed = 0;
  static GateLimits from_json(const nlohmann::json& j);
};

struct GateResult {
  bool ok = false;
  std::vector<std::string> reasons, warnings;
  double volume_mm3 = 0;
  size_t solids = 0;
  std::optional<double> min_wall_mm, min_edge_mm, overhang_area_mm2;
  nlohmann::json to_json() const;
};

GateResult gate(const TopoDS_Shape& shape, const GateLimits& limits = {});
std::optional<double> wall_thickness(const TopoDS_Shape& shape, int samples, unsigned seed);
double overhang_area(const TopoDS_Shape& shape, const std::array<double, 3>& dir, double max_deg);

}  // namespace structura
