// In-house linear elasticity on quadratic tetrahedra (Eigen), independent of CalculiX (F2-T4, D3-T3).
// Same element family as CalculiX C3D10 with a 4-point Gauss rule (exact for the quadratic-tet stiffness), assembled
// and solved in process. It cross-checks the CalculiX path (deck writing, node order, result parsing) and serves as
// the fast screening solver for the optimizer. Every reported result still comes from CalculiX.
#pragma once
#include <array>
#include <map>
#include <string>

#include "structura/fea.hpp"
#include "structura/mesh.hpp"

namespace structura {

struct FemResult {
  std::map<int, std::array<double, 3>> disp;  // node -> mm
  double max_disp_mm = 0;
  size_t dof = 0, free_dof = 0;
  double assemble_s = 0, solve_s = 0;
};

FemResult fem_static(const Mesh& m, const Material& mat, const LoadCase& lc);

}  // namespace structura
