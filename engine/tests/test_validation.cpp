// F2-T3 and F2-T5: the validation suite against analytical and NAFEMS references.
#include <doctest/doctest.h>

#include <filesystem>

#include "structura/validation.hpp"

using namespace structura;
namespace fs = std::filesystem;

namespace {
void check_case(const std::string& id) {
  ValidationRow r = run_validation_case(id, (fs::temp_directory_path() / "structura_validation").string());
  MESSAGE(r.name << ": " << r.quantity << " = " << r.value << " " << r.unit << " vs " << r.reference << " (" << r.error_pct
                 << "%, tolerance " << r.tolerance_pct << "%), DOF " << r.dof << ", " << r.seconds << " s");
  CHECK(r.pass);
}
}  // namespace

TEST_CASE("F2-T3: cantilever static deflection") { check_case("cantilever_static"); }
TEST_CASE("F2-T3: NAFEMS LE10") { check_case("nafems_le10"); }
TEST_CASE("F2-T3: thermal slab") { check_case("thermal_slab"); }
TEST_CASE("F2-T5: NAFEMS LE1") { check_case("nafems_le1"); }
TEST_CASE("F2-T5: Euler column buckling") { check_case("euler_column"); }
TEST_CASE("F2-T5: cantilever first natural frequency") { check_case("cantilever_modal"); }
TEST_CASE("F2-T5: thermal fin tip temperature") { check_case("thermal_fin"); }
