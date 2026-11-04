// B3-T2 (C++): the bracket family builds from its graph at default and corner parameters, with named faces.
#include <doctest/doctest.h>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/occ.hpp"

using namespace structura;

namespace {
void check(const CompiledPart& part) {
  CHECK(part.solid_count() == 1);
  CHECK(is_valid(part.shape));
  auto f = part.named_faces();
  CHECK(f.at("mount_holes").size() >= 2);
  for (auto n : {"load_face", "top_face", "wall_face"}) CHECK_MESSAGE(!f.at(n).empty(), n);
}
}  // namespace

TEST_CASE("B3-T2: bracket default build") {
  DesignGraph g = load_family("bracket");
  CompiledPart part = compile_graph(g);
  check(part);
  double v = volume(part.shape);
  CHECK(v > 60000);
  CHECK(v < 80000);
}

TEST_CASE("B3-T2: bracket builds at all 32 parameter corners") {
  DesignGraph g = load_family("bracket");
  std::vector<std::string> names;
  for (auto& [k, _] : g.parameters) names.push_back(k);
  for (size_t c = 0; c < (size_t(1) << names.size()); ++c) {
    Values v;
    for (size_t i = 0; i < names.size(); ++i) v[names[i]] = (c >> i) & 1 ? g.param(names[i])->high : g.param(names[i])->low;
    CAPTURE(c);
    check(compile_graph(g, v));
  }
}

TEST_CASE("B3-T2: bracket parameter errors name the parameter") {
  DesignGraph g = load_family("bracket");
  CHECK_THROWS_WITH_AS(compile_graph(g, {{"thickness", 40}}), doctest::Contains("thickness"), GraphError);
  CHECK_THROWS_WITH_AS(compile_graph(g, {{"rib_count", 1.5}}), doctest::Contains("rib_count"), GraphError);
}
