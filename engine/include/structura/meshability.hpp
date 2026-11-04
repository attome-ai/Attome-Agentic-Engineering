// Mesh-ability predictor (B4-T3): cheap geometric features of a gated shape -> probability that gmsh meshes it on the
// first attempt. Trained by logistic regression on sweep data; until a sweep has seen failures it predicts
// "meshable" for everything and says so in its model notes.
#pragma once
#include <array>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace structura {

struct MeshFeatures {
  double log_min_face_area = 0;  // log10 mm^2
  double face_count = 0;
  double log_min_edge = 0;       // log10 mm
  double thickness_ratio = 0;    // min wall / bounding-box diagonal
  std::array<double, 4> vec() const { return {log_min_face_area, face_count, log_min_edge, thickness_ratio}; }
  nlohmann::json to_json() const;
};
MeshFeatures mesh_features(const TopoDS_Shape& shape, double min_wall_mm);

struct MeshabilityModel {
  std::array<double, 4> w{0, 0, 0, 0}, mean{0, 0, 0, 0}, scale{1, 1, 1, 1};
  double b = 10;  // large bias: "always meshable" until trained on failures
  bool trained = false;
  std::string note = "untrained: no meshing failures observed yet, every shape predicted meshable";
  double precision = 1, recall = 1;  // for the failure class, on the training sweep
  double probability(const MeshFeatures& f) const;  // probability of meshing
  nlohmann::json to_json() const;
  static MeshabilityModel from_json(const nlohmann::json& j);
};

// Fit on samples (features, meshed?) by logistic regression; stays untrained when one class is missing.
MeshabilityModel fit_meshability(const std::vector<MeshFeatures>& x, const std::vector<int>& meshed);

}  // namespace structura
