#include "structura/meshability.hpp"

#include <cmath>

#include "structura/occ.hpp"

namespace structura {

nlohmann::json MeshFeatures::to_json() const {
  return {{"log_min_face_area", log_min_face_area}, {"face_count", face_count}, {"log_min_edge", log_min_edge},
          {"thickness_ratio", thickness_ratio}};
}

MeshFeatures mesh_features(const TopoDS_Shape& shape, double min_wall_mm) {
  MeshFeatures f;
  double amin = 1e300, emin = 1e300;
  auto faces = faces_of(shape);
  for (auto& x : faces) amin = std::min(amin, area(x));
  for (auto& e : edges_of(shape)) emin = std::min(emin, edge_length(e));
  auto b = bbox(shape);
  double diag = std::sqrt((b[3] - b[0]) * (b[3] - b[0]) + (b[4] - b[1]) * (b[4] - b[1]) + (b[5] - b[2]) * (b[5] - b[2]));
  f.log_min_face_area = std::log10(std::max(amin, 1e-12));
  f.face_count = static_cast<double>(faces.size());
  f.log_min_edge = std::log10(std::max(emin, 1e-12));
  f.thickness_ratio = diag > 0 ? min_wall_mm / diag : 0;
  return f;
}

double MeshabilityModel::probability(const MeshFeatures& f) const {
  auto v = f.vec();
  double z = b;
  for (int i = 0; i < 4; ++i) z += w[i] * (v[i] - mean[i]) / scale[i];
  return 1 / (1 + std::exp(-z));
}

nlohmann::json MeshabilityModel::to_json() const {
  return {{"w", w}, {"b", b}, {"mean", mean}, {"scale", scale}, {"trained", trained}, {"note", note},
          {"precision_failure", precision}, {"recall_failure", recall}};
}

MeshabilityModel MeshabilityModel::from_json(const nlohmann::json& j) {
  MeshabilityModel m;
  m.w = j.at("w");
  m.b = j.at("b");
  m.mean = j.at("mean");
  m.scale = j.at("scale");
  m.trained = j.at("trained");
  m.note = j.value("note", std::string());
  m.precision = j.value("precision_failure", 1.0);
  m.recall = j.value("recall_failure", 1.0);
  return m;
}

MeshabilityModel fit_meshability(const std::vector<MeshFeatures>& x, const std::vector<int>& y) {
  MeshabilityModel m;
  size_t n = x.size(), pos = 0;
  for (int v : y) pos += v;
  if (n == 0 || pos == 0 || pos == n) {
    m.note = pos == n ? "untrained: all " + std::to_string(n) + " sweep designs meshed, every shape predicted meshable"
                      : "untrained: no successful meshes to learn from";
    return m;
  }
  for (int i = 0; i < 4; ++i) {
    double s = 0, s2 = 0;
    for (auto& f : x) { double v = f.vec()[i]; s += v; s2 += v * v; }
    m.mean[i] = s / n;
    double var = s2 / n - m.mean[i] * m.mean[i];
    m.scale[i] = var > 1e-12 ? std::sqrt(var) : 1.0;
  }
  m.b = 0;
  m.w = {0, 0, 0, 0};
  const double lr = 0.1, l2 = 1e-3;
  for (int it = 0; it < 4000; ++it) {  // batch gradient descent on the log-loss with a small L2 penalty
    std::array<double, 4> gw{0, 0, 0, 0};
    double gb = 0;
    for (size_t k = 0; k < n; ++k) {
      double e = m.probability(x[k]) - y[k];
      auto v = x[k].vec();
      for (int i = 0; i < 4; ++i) gw[i] += e * (v[i] - m.mean[i]) / m.scale[i];
      gb += e;
    }
    for (int i = 0; i < 4; ++i) m.w[i] -= lr * (gw[i] / n + l2 * m.w[i]);
    m.b -= lr * gb / n;
  }
  size_t tp = 0, fp = 0, fn = 0;  // positive class here = predicted failure
  for (size_t k = 0; k < n; ++k) {
    bool pred_fail = m.probability(x[k]) < 0.5, fail = y[k] == 0;
    tp += pred_fail && fail;
    fp += pred_fail && !fail;
    fn += !pred_fail && fail;
  }
  m.precision = tp + fp ? double(tp) / (tp + fp) : 0;
  m.recall = tp + fn ? double(tp) / (tp + fn) : 0;
  m.trained = true;
  m.note = "logistic regression on " + std::to_string(n) + " sweep designs (" + std::to_string(n - pos) + " meshing failures)";
  return m;
}

}  // namespace structura
