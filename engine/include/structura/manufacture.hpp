// Manufacturing rules and cost (task H1): FDM printing and 3-axis CNC milling checks on the B-rep, a cost model per
// process, and process advice. Every rule, rate and price comes from <repo>/data/processes.json, where each value
// carries its source; the reports echo the inputs they used.
#pragma once
#include <array>
#include <string>

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace structura {

// <repo>/data/processes.json, or `path`; values are read as {"value": ..., "source": ...}.
nlohmann::json load_processes(const std::string& path = "");

// Support volume estimate (mm^3) for printing along `dir`: every triangle facing down more steeply than max_deg from
// vertical is supported by a column down to the bed. An upper bound: columns that would land on the part are counted
// to the bed.
double support_volume(const TopoDS_Shape& shape, const std::array<double, 3>& dir, double max_deg);

// Area (mm^2) of the part's surface that no tool can reach from any of the approach directions (a 3-axis tool along a
// is blocked if a ray from the surface point towards +a hits the part, or if the surface faces away from the tool).
// Estimated by area-weighted sampling of the exact surfaces.
double undercut_area(const TopoDS_Shape& shape, const std::vector<std::array<double, 3>>& approach, int samples = 3000,
                     unsigned seed = 1);

// Cost of making the part by `process` ("fdm" or "cnc") from the stated rates; fast enough for every evaluation (no
// rule sampling). `detail` (optional) receives time, volumes and the cost split.
double process_cost(const TopoDS_Shape& shape, const std::string& material, const std::array<double, 3>& print_dir,
                    const nlohmann::json& processes, const std::string& process, nlohmann::json* detail = nullptr);

// FDM: build volume, minimum wall, overhang, support, time and cost. `material` is a data/materials.json id.
nlohmann::json fdm_report(const TopoDS_Shape& shape, const std::string& material, const std::array<double, 3>& print_dir,
                          const nlohmann::json& processes);

// CNC 3-axis: stock, removed volume, undercuts, internal corner radii against the tool, time and cost.
nlohmann::json cnc_report(const TopoDS_Shape& shape, const std::string& material, const nlohmann::json& processes);

// Both reports plus advice: which process fits, which rule is closest to its limit, and the cost by each process.
nlohmann::json manufacture_report(const TopoDS_Shape& shape, const std::string& material, const std::array<double, 3>& print_dir,
                                  const nlohmann::json& processes);

}  // namespace structura
