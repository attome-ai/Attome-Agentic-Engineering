// One evaluation of a candidate against a DesignSpec, and the feasibility check run before any optimization.
//   evaluate_design: params -> build -> gate (with safe repair) -> mesh -> every load case at the high end of its
//   factor range -> metrics -> requirement checks with margins and the governing case.
//   check_feasibility (A2-T3): evaluates the most-material and least-material corners of the free parameters; if even
//   the strongest design fails a strength/stiffness requirement, or the lightest still breaks the mass limit, the spec
//   is infeasible and the report names the requirement and the best achievable value.
#pragma once
#include <string>

#include <nlohmann/json.hpp>

#include "structura/spec.hpp"

namespace structura {

struct EvalOptions {
  double mesh_size = 0;   // 0 = the spec's mesh size
  int threads = 1;
  std::string workdir;    // where meshes and solver files go
  bool keep_files = false;
  bool save_bundle = false;  // write design, STEP/STL, results and VTU to workdir
  bool screen = false;       // in-process in-house solver instead of CalculiX: for screening only, never reported as final
};

nlohmann::json evaluate_design(const DesignSpec& spec, const Values& free_params, const EvalOptions& o);
nlohmann::json check_feasibility(const DesignSpec& spec, const EvalOptions& o);

// Real-physics animation data (dynamic response): the first load case applied suddenly at its nominal value (factor 1),
// solved over time. Writes into `out`: part.step, mesh.vtu (static result), frames.bin (float32 per frame per node:
// ux uy uz von_mises, nodes in node_ids order) and dynamic.json (times, frequencies, damping, static and peak values,
// dynamic amplification, markers). Duration and time step come from the first frequency unless given.
struct DynamicOptions {
  double mesh_size = 0;
  int threads = 4;
  double damping_ratio = 0.03;  // typical for polymers and bolted metal joints; stated in the output as an assumption
  double periods = 4;           // simulated time in periods of the lowest computed mode
  int steps_per_period = 30;    // per period of the highest computed mode
  int store_every = 2;
  bool carried_mass = true;     // the load is a mass whose inertia moves with the part (a crate), not a massless force
};
nlohmann::json dynamic_bundle(const DesignSpec& spec, const Values& free_params, const std::string& out, const DynamicOptions& o);

}  // namespace structura
