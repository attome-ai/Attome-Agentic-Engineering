// C1, C4: adaptive mesh refinement driven by a recovered-stress error estimate, until the peak stress converges.
#pragma once
#include <map>
#include <string>
#include <vector>

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

#include "structura/fea.hpp"

namespace structura {

struct AdaptiveOptions {
  double start_size = 6;    // mm, first mesh and the largest element allowed
  double min_size = 0.5;    // mm, smallest element allowed
  double eta_target = 0.15;  // an element is refined when stress varies across it by more than this fraction of the peak
  double tolerance = 0.05;  // stop when the peak stress changes by less than this between levels
  int max_levels = 5;
};

struct AdaptiveLevel {
  int level = 0;
  size_t elements = 0, dof = 0, elements_refined = 0;
  double peak_MPa = 0, max_disp_mm = 0, seconds = 0, change = -1;
};

struct AdaptiveResult {
  bool converged = false;
  std::vector<AdaptiveLevel> levels;
  Mesh mesh;      // the last level
  Result result;  // the last level
  nlohmann::json to_json() const;
};

AdaptiveResult solve_adaptive(const TopoDS_Shape& shape, const std::map<std::string, std::vector<TopoDS_Face>>& named, const Material& mat,
                              const LoadCase& lc, const std::string& workdir, const AdaptiveOptions& o = {}, const SolveOptions& so = {});

}  // namespace structura
