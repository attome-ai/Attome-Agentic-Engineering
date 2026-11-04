// C1, C4: bolted supports and adaptive refinement against a stress concentration with a known answer.
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>

#include "structura/adaptive.hpp"
#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/fea.hpp"

using namespace structura;
namespace fs = std::filesystem;

namespace {
const Material kSteel = [] {
  Material m;
  m.id = "test";
  m.E = 210000;
  m.nu = 0.3;
  m.density = 7.85e-9;
  m.yield = 235;
  return m;
}();

// 120 x 40 x 4 mm plate with a central 10 mm hole, pulled along x.
CompiledPart plate_with_hole() {
  return compile_graph(DesignGraph::from_json(ojson::parse(R"({
    "name": "plate_hole",
    "features": [{"id": "p", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": 120, "width": 40, "thickness": 4}},
                 {"id": "h", "type": "hole", "params": {"x": 60, "y": 20, "z": -0.5, "diameter": 10, "depth": 5, "axis": "z"}}],
    "faces": {"xmin": {"kind": "plane", "normal": "-x", "at": 0}, "xmax": {"kind": "plane", "normal": "x", "at": 120}}})")));
}

// A block held through two bolt holes, loaded at its far end.
CompiledPart bolted_block() {
  return compile_graph(DesignGraph::from_json(ojson::parse(R"({
    "name": "bolted_block",
    "features": [{"id": "p", "type": "plate", "params": {"x": 0, "y": 0, "z": 0, "length": 80, "width": 40, "thickness": 8}},
                 {"id": "h", "type": "hole_pattern", "params": {"x": 10, "y": 10, "z": -0.5, "diameter": 6.6, "depth": 9, "axis": "z", "n1": 2, "d1": 20, "n2": 1, "d2": 0}}],
    "faces": {"bolts": {"kind": "cylinder", "axis": "z", "radius": 3.3}, "tip": {"kind": "plane", "normal": "x", "at": 80},
              "seat": {"kind": "plane", "normal": "-x", "at": 0}}})")));
}
}  // namespace

TEST_CASE("C4-T2: adaptive refinement converges on a known stress concentration") {
  CompiledPart p = plate_with_hole();
  LoadCase lc;
  lc.name = "tension";
  lc.supports = {{"xmin", "123"}};
  const double force = 1600;  // N, 10 MPa gross on 40 x 4 mm
  lc.loads = {{"xmax", "force", {force, 0, 0}, 0}};
  fs::path dir = fs::temp_directory_path() / "structura_adaptive";
  fs::remove_all(dir);
  AdaptiveOptions o;
  o.start_size = 5;
  o.tolerance = 0.03;
  SolveOptions so;
  so.threads = 4;
  so.exclude_near_bc_mm = 10;
  AdaptiveResult r = solve_adaptive(p.shape, p.named_faces(), kSteel, lc, dir.string(), o, so);
  // Peterson, finite-width plate with a central hole, d/W = 0.25: Kt on the net section = 3 - 3.14 r + 3.667 r^2 - 1.527 r^3
  const double dw = 0.25, kt = 3 - 3.14 * dw + 3.667 * dw * dw - 1.527 * dw * dw * dw;
  const double nominal = force / ((40.0 - 10.0) * 4.0), expected = kt * nominal;
  MESSAGE("levels " << r.levels.size() << ", peak " << r.levels.back().peak_MPa << " MPa against " << expected << " MPa, elements "
                    << r.levels.front().elements << " -> " << r.levels.back().elements);
  CHECK(r.converged);
  CHECK(r.levels.size() >= 2);
  CHECK(r.levels.back().peak_MPa == doctest::Approx(expected).epsilon(0.05));
  CHECK(r.result.equilibrium_error < 0.005);
}

TEST_CASE("C1-T3: bolted supports hold the part through the holes without rigid faces") {
  CompiledPart p = bolted_block();
  fs::path dir = fs::temp_directory_path() / "structura_bolted";
  fs::remove_all(dir);
  MeshOptions mo;
  mo.size = 3;
  Mesh m = mesh_shape(p.shape, p.named_faces(), (dir / "mesh").string(), mo);
  LoadCase rigid, bolted;
  rigid.supports = {{"bolts", "123"}};
  bolted.supports = {{"bolts", "123", "bolted"}};
  rigid.loads = bolted.loads = {{"tip", "force", {0, 0, -200}, 0}};
  // two bolts alone hold only the mean position of each bore: the part can still rotate about the line between them
  SetupReport loose = check_setup(m, bolted);
  CHECK_FALSE(loose.ok);
  CHECK(loose.restrained == 5);
  // the end of the part bearing on a stop prevents that rotation
  bolted.supports.push_back({"seat", "3"});
  SetupReport setup = check_setup(m, bolted);
  CHECK(setup.ok);
  SolveOptions so;
  so.threads = 4;
  Result a = solve_static(m, kSteel, rigid, (dir / "rigid").string(), so);
  Result b = solve_static(m, kSteel, bolted, (dir / "bolted").string(), so);
  MESSAGE("raw peak rigid " << a.max_von_mises_raw_MPa << " MPa, bolted " << b.max_von_mises_raw_MPa << " MPa; tip " << a.max_disp_mm << " vs " << b.max_disp_mm << " mm");
  CHECK(b.equilibrium_error < 0.005);       // the reactions at the bolts balance the load
  CHECK(b.max_disp_mm >= a.max_disp_mm);    // the bores can now deform, so the part is no stiffer than with rigid bores
  CHECK(b.max_disp_mm < 3 * a.max_disp_mm);  // and it is still held
  // the deck ties each bore to its reference node in all three directions
  std::string deck = write_deck(m, kSteel, bolted);
  size_t n = 0;
  for (size_t at = deck.find("*EQUATION"); at != std::string::npos; at = deck.find("*EQUATION", at + 1)) ++n;
  CHECK(n == 6);  // three directions at each of two bolts
}
