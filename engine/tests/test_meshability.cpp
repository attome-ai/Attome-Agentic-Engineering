// B4-T3: the mesh-ability model learns a separable failure pattern and stays untrained without failures.
#include <doctest/doctest.h>

#include <random>

#include <BRepPrimAPI_MakeBox.hxx>

#include "structura/meshability.hpp"

using namespace structura;

TEST_CASE("B4-T3: features of a simple block") {
  MeshFeatures f = mesh_features(BRepPrimAPI_MakeBox(10, 20, 30).Shape(), 10);
  CHECK(f.face_count == 6);
  CHECK(f.log_min_face_area == doctest::Approx(std::log10(200.0)));
  CHECK(f.log_min_edge == doctest::Approx(1.0));
}

TEST_CASE("B4-T3: logistic model separates tiny-edge failures") {
  std::mt19937 rng(3);
  std::normal_distribution<double> nz(0, 0.1);
  std::vector<MeshFeatures> x;
  std::vector<int> y;
  for (int i = 0; i < 200; ++i) {
    MeshFeatures f;
    bool fail = i % 4 == 0;
    f.log_min_edge = (fail ? -2.0 : 0.0) + nz(rng);  // failures have edges near 0.01 mm
    f.log_min_face_area = 1 + nz(rng);
    f.face_count = 30 + nz(rng);
    f.thickness_ratio = 0.05 + nz(rng) * 0.01;
    x.push_back(f);
    y.push_back(fail ? 0 : 1);
  }
  MeshabilityModel m = fit_meshability(x, y);
  CHECK(m.trained);
  CHECK(m.recall > 0.9);
  CHECK(m.precision > 0.9);
  MeshabilityModel back = MeshabilityModel::from_json(m.to_json());
  CHECK(back.probability(x[0]) == doctest::Approx(m.probability(x[0])));
}

TEST_CASE("B4-T3: no failures means an untrained model that predicts meshable") {
  std::vector<MeshFeatures> x(10);
  std::vector<int> y(10, 1);
  MeshabilityModel m = fit_meshability(x, y);
  CHECK_FALSE(m.trained);
  CHECK(m.probability(MeshFeatures{}) > 0.99);
}
