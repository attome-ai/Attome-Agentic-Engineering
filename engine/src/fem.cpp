#include "structura/fem.hpp"

#include <chrono>
#include <cmath>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

namespace structura {
namespace {

using clk = std::chrono::steady_clock;

// Node order (CalculiX C3D10 / VTK quadratic tetra): corners 1-4, then mid-side (1,2) (2,3) (3,1) (1,4) (2,4) (3,4).
// Shape-function gradients with respect to the volume coordinates L1..L4.
void dN_dL(const double L[4], double g[10][4]) {
  for (int i = 0; i < 10; ++i)
    for (int k = 0; k < 4; ++k) g[i][k] = 0;
  for (int i = 0; i < 4; ++i) g[i][i] = 4 * L[i] - 1;
  const int edge[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {1, 3}, {2, 3}};
  for (int e = 0; e < 6; ++e) {
    int a = edge[e][0], b = edge[e][1];
    g[4 + e][a] = 4 * L[b];
    g[4 + e][b] = 4 * L[a];
  }
}

}  // namespace

FemResult fem_static(const Mesh& m, const Material& mat, const LoadCase& lc) {
  auto t0 = clk::now();
  FemResult out;
  std::unordered_map<int, int> idx;
  std::vector<int> ids;
  for (auto& [id, _] : m.nodes) {
    idx[id] = static_cast<int>(ids.size());
    ids.push_back(id);
  }
  const int ndof = 3 * static_cast<int>(ids.size());
  out.dof = ndof;

  // constitutive matrix, Voigt order xx yy zz xy yz zx with engineering shear strains
  const double E = mat.E, nu = mat.nu;
  const double lam = E * nu / ((1 + nu) * (1 - 2 * nu)), mu = E / (2 * (1 + nu));
  Eigen::Matrix<double, 6, 6> D = Eigen::Matrix<double, 6, 6>::Zero();
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) D(i, j) = lam + (i == j ? 2 * mu : 0);
  for (int i = 3; i < 6; ++i) D(i, i) = mu;

  // 4-point rule on the reference tetrahedron (degree 2)
  const double a = 0.5854101966249685, b = 0.1381966011250105, w = 1.0 / 24.0;
  const double gp[4][4] = {{a, b, b, b}, {b, a, b, b}, {b, b, a, b}, {b, b, b, a}};  // (L1, L2, L3, L4)

  // fixed dofs
  std::vector<char> fixed(ndof, 0);
  for (auto& s : lc.supports) {
    auto it = m.nsets.find(s.group);
    if (it == m.nsets.end() || it->second.empty()) throw std::runtime_error("support group '" + s.group + "' has no nodes");
    for (int n : it->second)
      for (char c : s.dofs) fixed[3 * idx.at(n) + (c - '1')] = 1;
  }
  std::vector<int> map_free(ndof, -1);
  int nfree = 0;
  for (int i = 0; i < ndof; ++i)
    if (!fixed[i]) map_free[i] = nfree++;
  out.free_dof = nfree;

  std::vector<Eigen::Triplet<double>> trip;
  trip.reserve(m.tets.size() * 900);
  Eigen::Matrix<double, 6, 30> B;
  Eigen::Matrix<double, 30, 30> Ke;
  for (auto& t : m.tets) {
    Eigen::Matrix<double, 10, 3> X;
    for (int i = 0; i < 10; ++i) {
      auto& c = m.nodes.at(t[1 + i]);
      X(i, 0) = c[0];
      X(i, 1) = c[1];
      X(i, 2) = c[2];
    }
    Ke.setZero();
    for (auto& q : gp) {
      double g[10][4];
      dN_dL(q, g);
      // derivatives with respect to xi, eta, zeta where L2 = xi, L3 = eta, L4 = zeta, L1 = 1 - xi - eta - zeta
      Eigen::Matrix<double, 10, 3> dNdxi;
      for (int i = 0; i < 10; ++i) {
        dNdxi(i, 0) = g[i][1] - g[i][0];
        dNdxi(i, 1) = g[i][2] - g[i][0];
        dNdxi(i, 2) = g[i][3] - g[i][0];
      }
      Eigen::Matrix3d J = dNdxi.transpose() * X;  // J(r, c) = d x_c / d xi_r
      double detJ = J.determinant();
      if (detJ <= 0) throw std::runtime_error("inverted or degenerate element " + std::to_string(t[0]));
      Eigen::Matrix<double, 10, 3> dNdx = (J.inverse() * dNdxi.transpose()).transpose();
      B.setZero();
      for (int i = 0; i < 10; ++i) {
        double nx = dNdx(i, 0), ny = dNdx(i, 1), nz = dNdx(i, 2);
        B(0, 3 * i) = nx;
        B(1, 3 * i + 1) = ny;
        B(2, 3 * i + 2) = nz;
        B(3, 3 * i) = ny;
        B(3, 3 * i + 1) = nx;
        B(4, 3 * i + 1) = nz;
        B(4, 3 * i + 2) = ny;
        B(5, 3 * i) = nz;
        B(5, 3 * i + 2) = nx;
      }
      Ke.noalias() += (B.transpose() * D * B) * (w * detJ);
    }
    int gdof[30];
    for (int i = 0; i < 10; ++i)
      for (int c = 0; c < 3; ++c) gdof[3 * i + c] = map_free[3 * idx.at(t[1 + i]) + c];
    for (int r = 0; r < 30; ++r) {
      if (gdof[r] < 0) continue;
      for (int c = 0; c < 30; ++c)
        if (gdof[c] >= 0) trip.emplace_back(gdof[r], gdof[c], Ke(r, c));
    }
  }
  Eigen::SparseMatrix<double> K(nfree, nfree);
  K.setFromTriplets(trip.begin(), trip.end());
  trip.clear();
  trip.shrink_to_fit();

  Eigen::VectorXd f = Eigen::VectorXd::Zero(nfree);
  for (auto& l : lc.loads) {
    if (l.kind != "force") throw std::runtime_error("the in-house solver supports force loads only");
    for (auto& [n, fv] : distribute_force(m, l.group, l.force))
      for (int c = 0; c < 3; ++c) {
        int k = map_free[3 * idx.at(n) + c];
        if (k >= 0) f[k] += fv[c];
      }
  }
  auto t1 = clk::now();
  Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
  solver.compute(K);
  if (solver.info() != Eigen::Success) throw std::runtime_error("factorization failed: the structure may be under-constrained");
  Eigen::VectorXd u = solver.solve(f);
  if (solver.info() != Eigen::Success) throw std::runtime_error("solve failed");
  auto t2 = clk::now();
  for (size_t i = 0; i < ids.size(); ++i) {
    std::array<double, 3> d{0, 0, 0};
    for (int c = 0; c < 3; ++c) {
      int k = map_free[3 * i + c];
      if (k >= 0) d[c] = u[k];
    }
    out.disp[ids[i]] = d;
    out.max_disp_mm = std::max(out.max_disp_mm, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
  }
  // Stress recovery as CalculiX does it for C3D10: sigma = D B u at the four integration points, extrapolated linearly to
  // the corners, mid-side nodes the mean of their two corners, the tensor averaged over the elements sharing a node, then
  // von Mises. Evaluating at the nodes directly spikes in distorted curved elements (a factor of two near small fillets).
  const double ga = 0.1381966011250105, gb = 0.5854101966249685;  // barycentric coordinates of the four-point rule
  const int kEdge[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {1, 3}, {2, 3}};
  std::unordered_map<int, std::pair<Eigen::Matrix<double, 6, 1>, int>> acc;
  for (auto& t : m.tets) {
    Eigen::Matrix<double, 10, 3> X;
    Eigen::Matrix<double, 30, 1> ue;
    for (int i = 0; i < 10; ++i) {
      auto& c = m.nodes.at(t[1 + i]);
      auto& d = out.disp.at(t[1 + i]);
      for (int k = 0; k < 3; ++k) {
        X(i, k) = c[k];
        ue(3 * i + k) = d[k];
      }
    }
    Eigen::Matrix<double, 6, 1> gp[4], sum = Eigen::Matrix<double, 6, 1>::Zero();
    for (int a = 0; a < 4; ++a) {
      double L[4] = {ga, ga, ga, ga};
      L[a] = gb;
      double g[10][4];
      dN_dL(L, g);
      Eigen::Matrix<double, 10, 3> dNdxi;
      for (int i = 0; i < 10; ++i) {
        dNdxi(i, 0) = g[i][1] - g[i][0];
        dNdxi(i, 1) = g[i][2] - g[i][0];
        dNdxi(i, 2) = g[i][3] - g[i][0];
      }
      Eigen::Matrix3d J = dNdxi.transpose() * X;
      Eigen::Matrix<double, 10, 3> dNdx = (J.inverse() * dNdxi.transpose()).transpose();
      B.setZero();
      for (int i = 0; i < 10; ++i) {
        double nx = dNdx(i, 0), ny = dNdx(i, 1), nz = dNdx(i, 2);
        B(0, 3 * i) = nx; B(1, 3 * i + 1) = ny; B(2, 3 * i + 2) = nz;
        B(3, 3 * i) = ny; B(3, 3 * i + 1) = nx;
        B(4, 3 * i + 1) = nz; B(4, 3 * i + 2) = ny;
        B(5, 3 * i) = nz; B(5, 3 * i + 2) = nx;
      }
      gp[a] = D * B * ue;  // xx yy zz xy yz zx
      sum += gp[a];
    }
    Eigen::Matrix<double, 6, 1> corner[4];
    for (int a = 0; a < 4; ++a) corner[a] = (gp[a] - ga * sum) / (gb - ga);  // inverse of the linear map corners -> points
    for (int a = 0; a < 10; ++a) {
      Eigen::Matrix<double, 6, 1> v = a < 4 ? corner[a] : Eigen::Matrix<double, 6, 1>(0.5 * (corner[kEdge[a - 4][0]] + corner[kEdge[a - 4][1]]));
      auto [it, added] = acc.try_emplace(t[1 + a], Eigen::Matrix<double, 6, 1>::Zero(), 0);
      it->second.first += v;
      it->second.second += 1;
    }
  }
  for (auto& [n, v] : acc) {
    const Eigen::Matrix<double, 6, 1> st = v.first / v.second;
    const double vm = std::sqrt(0.5 * ((st(0) - st(1)) * (st(0) - st(1)) + (st(1) - st(2)) * (st(1) - st(2)) + (st(2) - st(0)) * (st(2) - st(0))) +
                                3 * (st(3) * st(3) + st(4) * st(4) + st(5) * st(5)));
    out.von_mises[n] = vm;
    out.max_von_mises_MPa = std::max(out.max_von_mises_MPa, vm);
  }
  out.assemble_s = std::chrono::duration<double>(t1 - t0).count();
  out.solve_s = std::chrono::duration<double>(t2 - t1).count();
  return out;
}

}  // namespace structura
