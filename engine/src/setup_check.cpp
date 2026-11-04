// C3: checks on a load case before anything is solved. A wrong setup gives plausible but wrong results, so it is refused
// here with the reason: the supports must stop all six rigid-body motions, and every load must act on a real face that
// is not fully held by a support.
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string>

#include <Eigen/Dense>

#include "structura/fea.hpp"

namespace structura {

namespace {
const char* const MOTION[6] = {"move along x", "move along y", "move along z", "rotate about x", "rotate about y", "rotate about z"};
}

nlohmann::json SetupReport::to_json() const {
  return {{"ok", ok}, {"rigid_body_modes_restrained", restrained}, {"problems", problems}};
}

SetupReport check_setup(const Mesh& m, const LoadCase& lc) {
  SetupReport rep;
  // Rigid motion of a point at r from the centroid: u = t + w x r. Each held degree of freedom gives one row of A in
  // A [t, w] = 0; the supports stop every rigid motion only if A has rank 6.
  std::array<double, 3> c{0, 0, 0}, lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
  for (auto& [id, p] : m.nodes)
    for (int i = 0; i < 3; ++i) {
      c[i] += p[i] / static_cast<double>(m.nodes.size());
      lo[i] = std::min(lo[i], p[i]);
      hi[i] = std::max(hi[i], p[i]);
    }
  const double L = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1e-9});
  Eigen::Matrix<double, 6, 6> AtA = Eigen::Matrix<double, 6, 6>::Zero();
  std::set<int> fully_held;
  for (auto& sp : lc.supports) {
    auto it = m.nsets.find(sp.group);
    if (it == m.nsets.end() || it->second.empty()) {
      rep.problems.push_back("support on face group '" + sp.group + "', which does not exist on this mesh");
      continue;
    }
    if (sp.kind == "bolted") {
      for (auto& tris : connected_faces(m, sp.group)) {
        std::array<double, 3> b{0, 0, 0};
        double count = 0;
        for (auto& t : tris)
          for (size_t k = 1; k < t.size(); ++k) {
            auto& q = m.nodes.at(t[k]);
            for (int i = 0; i < 3; ++i) b[i] += q[i];
            count += 1;
          }
        const double rx = (b[0] / count - c[0]) / L, ry = (b[1] / count - c[1]) / L, rz = (b[2] / count - c[2]) / L;
        Eigen::Matrix<double, 6, 3> rows;
        rows << 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, -rz, ry, rz, 0, -rx, -ry, rx, 0;
        AtA.noalias() += rows * rows.transpose();
      }
      continue;
    }
    for (int n : it->second) {
      auto& p = m.nodes.at(n);
      const double rx = (p[0] - c[0]) / L, ry = (p[1] - c[1]) / L, rz = (p[2] - c[2]) / L;
      for (char d : sp.dofs) {
        Eigen::Matrix<double, 6, 1> row;
        if (d == '1') row << 1, 0, 0, 0, rz, -ry;
        else if (d == '2') row << 0, 1, 0, -rz, 0, rx;
        else if (d == '3') row << 0, 0, 1, ry, -rx, 0;
        else continue;
        AtA.noalias() += row * row.transpose();
      }
      if (sp.dofs.find('1') != std::string::npos && sp.dofs.find('2') != std::string::npos && sp.dofs.find('3') != std::string::npos)
        fully_held.insert(n);
    }
  }
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 6, 6>> eig(AtA);
  const double top = eig.eigenvalues().maxCoeff();
  rep.restrained = 0;
  for (int i = 0; i < 6; ++i) {
    if (top > 0 && eig.eigenvalues()(i) > 1e-9 * top) {
      ++rep.restrained;
    } else {
      Eigen::Index k = 0;
      eig.eigenvectors().col(i).cwiseAbs().maxCoeff(&k);
      rep.problems.push_back(std::string("the part is free to ") + MOTION[k] + ": the supports do not stop this rigid-body motion");
    }
  }
  if (lc.loads.empty()) rep.problems.push_back("load case '" + lc.name + "' has no loads");
  for (auto& l : lc.loads) {
    auto s = m.surfaces.find(l.group);
    if (s == m.surfaces.end() || s->second.empty()) {
      rep.problems.push_back("load on face group '" + l.group + "', which does not exist on this mesh");
      continue;
    }
    const double size = l.kind == "pressure" ? std::abs(l.pressure) : std::sqrt(l.force[0] * l.force[0] + l.force[1] * l.force[1] + l.force[2] * l.force[2]);
    if (size <= 0) rep.problems.push_back("load on '" + l.group + "' is zero");
    auto ns = m.nsets.find(l.group);
    if (ns != m.nsets.end() && !ns->second.empty() &&
        std::all_of(ns->second.begin(), ns->second.end(), [&](int n) { return fully_held.count(n) > 0; }))
      rep.problems.push_back("load on '" + l.group + "' acts on a fully held face, so it goes straight into the support");
  }
  rep.ok = rep.problems.empty();
  return rep;
}

}  // namespace structura
