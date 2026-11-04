// Trust record for a delivered design (F1). Written only by the engine; no agent tool can write it.
//   - requirement margins on the fine mesh, with the governing load case
//   - numerical error bars: the design is solved on three mesh levels and the Grid Convergence Index (Roache) gives an
//     error estimate for each governing value; a requirement whose margin is smaller than its error is "not proven"
//   - independent checks: reaction equilibrium, and strain energy against half the external work (Clapeyron)
//   - assumptions in plain words, the validation-suite status, a confidence grade with its written rule
//   - an evidence bundle with a content hash and a reproduce command
#pragma once
#include <string>

#include <nlohmann/json.hpp>

#include "structura/spec.hpp"

namespace structura {

struct TrustOptions {
  double fine_size = 0;  // 0 = half the spec's mesh size
  double ratio = 1.5;    // mesh size ratio between levels
  int threads = 4;
  std::string workdir;   // trust.json and the evidence bundle go here
  std::string spec_path; // recorded so the bundle can be reproduced
};

struct Gci {
  double fine = 0, medium = 0, coarse = 0;
  double h1 = 0, h2 = 0, h3 = 0;  // representative element sizes (mm)
  double order = 0, gci = 0;      // observed order p, GCI of the fine value (relative)
  bool monotone = false;
  nlohmann::json to_json() const;
};
Gci grid_convergence(double f1, double f2, double f3, double h1, double h2, double h3);

nlohmann::json build_trust(const DesignSpec& spec, const Values& free_params, const TrustOptions& o);
nlohmann::json reproduce(const std::string& dir, double tolerance = 1e-3);

}  // namespace structura
