// C3: setup checks refuse wrong boundary conditions before any solve.
#include <doctest/doctest.h>

#include <filesystem>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/fea.hpp"

using namespace structura;
namespace fs = std::filesystem;

namespace {
Mesh beam() {
  CompiledPart p = compile_graph(DesignGraph::from_json(ojson::parse(R"({
    "name": "beam", "features": [{"id": "b", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": 60, "width": 10, "thickness": 10}}],
    "faces": {"root": {"kind": "plane", "normal": "-x", "at": 0}, "tip": {"kind": "plane", "normal": "x", "at": 60}}})")));
  fs::path dir = fs::temp_directory_path() / "structura_setup_check";
  fs::remove_all(dir);
  MeshOptions o;
  o.size = 6;
  return mesh_shape(p.shape, p.named_faces(), dir.string(), o);
}
LoadCase lc(std::vector<Support> s, std::vector<Load> l) {
  LoadCase c;
  c.supports = std::move(s);
  c.loads = std::move(l);
  return c;
}
const Load down{"tip", "force", {0, 0, -100}, 0};
}  // namespace

TEST_CASE("C3-T3: setup checks") {
  Mesh m = beam();
  SUBCASE("a fully fixed end passes") {
    SetupReport r = check_setup(m, lc({{"root", "123"}}, {down}));
    CHECK(r.ok);
    CHECK(r.restrained == 6);
  }
  SUBCASE("no support leaves all six motions free") {
    SetupReport r = check_setup(m, lc({}, {down}));
    CHECK_FALSE(r.ok);
    CHECK(r.restrained == 0);
  }
  SUBCASE("a face held only along its normal can slide and spin") {
    SetupReport r = check_setup(m, lc({{"root", "1"}}, {down}));
    CHECK_FALSE(r.ok);
    CHECK(r.restrained == 3);  // x, and rotations about y and z
    CHECK(r.problems.front().find("free to") != std::string::npos);
  }
  SUBCASE("a missing face is named") {
    SetupReport r = check_setup(m, lc({{"nowhere", "123"}, {"root", "123"}}, {{"also_nowhere", "force", {0, 0, -1}, 0}}));
    CHECK_FALSE(r.ok);
    CHECK(r.problems.size() == 2);
  }
  SUBCASE("a load on the fixed face is refused") {
    SetupReport r = check_setup(m, lc({{"root", "123"}}, {{"root", "force", {0, 0, -1}, 0}}));
    CHECK_FALSE(r.ok);
    CHECK(r.problems.front().find("fully held") != std::string::npos);
  }
  SUBCASE("a zero load is refused") {
    SetupReport r = check_setup(m, lc({{"root", "123"}}, {{"tip", "force", {0, 0, 0}, 0}}));
    CHECK_FALSE(r.ok);
  }
}
