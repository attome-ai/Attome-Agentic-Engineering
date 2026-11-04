#include <set>
// B3-T1 (C++): every grammar feature compiles at the low and high corner of its bounds; invalid graphs fail before build.
#include <doctest/doctest.h>

#include <string>

#include "structura/compile.hpp"
#include "structura/occ.hpp"

using namespace structura;

namespace {

const char* BASE = R"({"id": "base", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": 100, "width": 100, "thickness": 20}})";

struct Fixture {
  const char* params;    // {"name": [low, high, integer]}
  const char* features;  // JSON array of features after the base plate
};

const std::map<std::string, Fixture> FIX = {
    {"plate", {R"({"t": [2, 30, false]})", R"([{"id": "f", "type": "plate", "params": {"x": 0, "y": 0, "z": 20, "length": 40, "width": 40, "thickness": "t"}}])"}},
    {"extrude_sketch", {R"({"h": [1, 40, false]})", R"([{"id": "f", "type": "extrude_sketch", "params": {"points": [[10, 10], [60, 10], [35, 50]], "z": 20, "height": "h"}}])"}},
    {"boss", {R"({"d": [4, 40, false], "h": [1, 30, false]})", R"([{"id": "f", "type": "boss", "params": {"x": 50, "y": 50, "z": 20, "diameter": "d", "height": "h"}}])"}},
    {"hole", {R"({"d": [1, 30, false]})", R"([{"id": "f", "type": "hole", "params": {"x": 50, "y": 50, "z": 20, "diameter": "d", "depth": 20}}])"}},
    {"hole_pattern", {R"({"n": [1, 4, true], "d": [2, 10, false]})", R"([{"id": "f", "type": "hole_pattern", "params": {"x": 15, "y": 15, "z": 20, "diameter": "d", "depth": 20, "n1": "n", "d1": 20, "n2": 2, "d2": 30}}])"}},
    {"rib", {R"({"t": [1, 20, false], "a": [5, 60, false]})", R"([{"id": "wall", "type": "plate", "params": {"x": 0, "y": 0, "z": 20, "length": 10, "width": 100, "thickness": 60}},
      {"id": "f", "type": "rib", "params": {"x": 10, "y": 40, "z": 20, "leg_a": "a", "leg_a_dir": "x", "leg_b": "a", "leg_b_dir": "z", "thickness": "t"}}])"}},
    {"pocket", {R"({"r": [0, 9, false], "dep": [1, 15, false]})", R"([{"id": "f", "type": "pocket", "params": {"x": 20, "y": 20, "z_top": 20, "length": 40, "width": 20, "depth": "dep", "corner_radius": "r"}}])"}},
    {"fillet", {R"({"r": [0.5, 8, false]})", R"([{"id": "f", "type": "fillet", "params": {"radius": "r", "edge_axis": "y", "region": [-1, -1, 19, 101, 101, 21]}}])"}},
    {"chamfer", {R"({"c": [0.5, 8, false]})", R"([{"id": "f", "type": "chamfer", "params": {"length": "c", "edge_axis": "x", "region": [-1, -1, 19, 101, 101, 21]}}])"}},
    {"shell", {R"({"t": [1, 8, false]})", R"([{"id": "f", "type": "shell", "params": {"thickness": "t", "open_side": "z"}}])"}},
    {"linear_pattern", {R"({"n": [1, 5, true]})", R"([{"id": "h", "type": "hole", "params": {"x": 10, "y": 50, "z": 20, "diameter": 6, "depth": 20}},
      {"id": "f", "type": "linear_pattern", "params": {"target": "h", "count": "n", "dx": 18}}])"}},
    {"circular_pattern", {R"({"n": [1, 8, true]})", R"([{"id": "h", "type": "hole", "params": {"x": 80, "y": 50, "z": 20, "diameter": 6, "depth": 20}},
      {"id": "f", "type": "circular_pattern", "params": {"target": "h", "count": "n", "cx": 50, "cy": 50}}])"}},
    {"sweep_path", {R"({"r": [1, 6, false]})", R"([{"id": "f", "type": "sweep_path", "params": {"points": [[10, 10, "20 + r * 0.5"], [90, 10, "20 + r * 0.5"], [90, 90, "20 + r * 0.5"]], "radius": "r"}}])"}},
    {"fin_array", {R"({"n": [1, 10, true], "h": [2, 50, false]})", R"([{"id": "f", "type": "fin_array", "params": {"x": 0, "y": 2, "z": 20, "count": "n", "pitch": 10, "length": 100, "fin_thickness": 2, "height": "h"}}])"}},
};

DesignGraph graph_for(const std::string& type) {
  const auto& f = FIX.at(type);
  ojson params = ojson::object();
  const ojson spec = ojson::parse(f.params);  // keep alive: items() of a temporary dangles
  for (auto& [k, v] : spec.items())
    params[k] = {{"low", v[0]}, {"high", v[1]}, {"default", v[0]}, {"integer", v[2]}};
  ojson feats = ojson::array();
  feats.push_back(ojson::parse(BASE));
  for (auto& x : ojson::parse(f.features)) feats.push_back(x);
  return DesignGraph::from_json({{"name", "fixture_" + type}, {"parameters", params}, {"features", feats}});
}

}  // namespace

TEST_CASE("B3-T1: every feature type has a fixture") {
  // import_step has no numeric parameters to put at bound corners and needs a real file: covered by A3-T2 (test_inspect.cpp)
  const std::set<std::string> covered_elsewhere = {"import_step"};
  for (auto& [name, _] : feature_types())
    if (!covered_elsewhere.count(name)) CHECK_MESSAGE(FIX.count(name), name);
  CHECK(FIX.size() + covered_elsewhere.size() == feature_types().size());
}

TEST_CASE("B3-T1: every feature compiles at its bound corners") {
  for (auto& [type, _] : FIX) {
    DesignGraph g = graph_for(type);
    REQUIRE_MESSAGE(!g.parameters.empty(), g.to_json().dump());
    std::vector<std::string> names;
    for (auto& [k, __] : g.parameters) names.push_back(k);
    size_t corners = size_t(1) << names.size();
    for (size_t c = 0; c < corners; ++c) {
      Values v;
      for (size_t i = 0; i < names.size(); ++i) {
        const Param* p = g.param(names[i]);
        v[names[i]] = (c >> i) & 1 ? p->high : p->low;
      }
      CAPTURE(type);
      CAPTURE(c);
      CompiledPart part = compile_graph(g, v);
      CHECK(part.solid_count() == 1);
      CHECK(is_valid(part.shape));
      CHECK(volume(part.shape) > 0);
    }
  }
}

TEST_CASE("B3-T1: invalid graphs are rejected before any geometry is built") {
  SUBCASE("out of bounds names the parameter") {
    DesignGraph g = graph_for("boss");
    CHECK_THROWS_WITH_AS(g.resolve({{"d", 100}}), doctest::Contains("'d'"), GraphError);
  }
  SUBCASE("missing parent") {
    auto g = DesignGraph::from_json(ojson::parse(std::string(R"({"name": "bad", "features": [)") + BASE +
                                                 R"(, {"id": "p", "type": "linear_pattern", "params": {"target": "nope", "count": 2, "dx": 5}}]})"));
    CHECK_THROWS_WITH_AS(g.resolve(), doctest::Contains("not an earlier feature"), GraphError);
  }
  SUBCASE("unknown parameter in expression") {
    auto g = DesignGraph::from_json(ojson::parse(R"({"name": "bad", "features": [{"id": "base", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": "L", "width": 10, "thickness": 2}}]})"));
    CHECK_THROWS_WITH_AS(g.resolve(), doctest::Contains("unknown parameter 'L'"), GraphError);
  }
  SUBCASE("code injection") {
    auto g = DesignGraph::from_json(ojson::parse(R"J({"name": "bad", "features": [{"id": "base", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": "__import__('os')", "width": 10, "thickness": 2}}]})J"));
    CHECK_THROWS_AS(g.resolve(), GraphError);
  }
  SUBCASE("first feature must add") {
    auto g = DesignGraph::from_json(ojson::parse(R"({"name": "bad", "features": [{"id": "h", "type": "hole", "params": {"x": 0, "y": 0, "z": 0, "diameter": 5, "depth": 5}}]})"));
    CHECK_THROWS_WITH_AS(g.resolve(), doctest::Contains("must add material"), GraphError);
  }
  SUBCASE("unknown feature type") {
    auto g = DesignGraph::from_json(ojson::parse(R"({"name": "bad", "features": [{"id": "a", "type": "teleport", "params": {}}]})"));
    CHECK_THROWS_WITH_AS(g.resolve(), doctest::Contains("unknown feature type"), GraphError);
  }
}
