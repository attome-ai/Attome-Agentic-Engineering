// F2-T5: golden results. The demo designs' key numbers are stored; a change that moves any of them by more than
// 0.5% fails until it is explained and the golden file is updated with a written reason in the commit message.
// To regenerate after an explained change: set STRUCTURA_UPDATE_GOLDEN=1 and run this test once.
#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/fea.hpp"

using namespace structura;
namespace fs = std::filesystem;

TEST_CASE("F2-T5: golden result for the default bracket under 100 kg") {
  CompiledPart p = compile_graph(load_family("bracket"));
  fs::path dir = fs::temp_directory_path() / "structura_golden_bracket";
  fs::remove_all(dir);
  MeshOptions mo;
  mo.size = 4;
  Mesh m = mesh_shape(p.shape, p.named_faces(), (dir / "mesh").string(), mo);
  LoadCase lc = LoadCase::from_json(nlohmann::json::parse(
      R"({"name": "service", "supports": [{"group": "mount_holes"}, {"group": "wall_face", "dofs": "1"}],
          "loads": [{"group": "load_face", "force": [0, 0, -981]}]})"));
  Result r = solve_static(m, load_material("PETG"), lc, (dir / "run").string(), {4, 600});
  nlohmann::json now = {{"mass_g", r.mass_t * 1e6}, {"max_disp_mm", r.max_disp_mm}, {"max_von_mises_MPa", r.max_von_mises_MPa},
                        {"dof", m.dof()}};
  fs::path golden = fs::path(STRUCTURA_ROOT) / "engine" / "tests" / "golden" / "bracket_default.json";
  const char* upd = std::getenv("STRUCTURA_UPDATE_GOLDEN");
  if (upd && std::string(upd) == "1") {
    fs::create_directories(golden.parent_path());
    std::ofstream(golden) << now.dump(2);
    MESSAGE("golden file written: " << golden.string());
  }
  REQUIRE_MESSAGE(fs::exists(golden), "no golden file; run once with STRUCTURA_UPDATE_GOLDEN=1");
  std::ifstream in(golden);
  auto ref = nlohmann::json::parse(in);
  for (auto key : {"mass_g", "max_disp_mm", "max_von_mises_MPa"}) {
    double a = ref.at(key), b = now.at(key);
    double rel = std::fabs(b - a) / std::fabs(a);
    CAPTURE(key);
    CAPTURE(a);
    CAPTURE(b);
    CHECK(rel <= 0.005);
  }
}
