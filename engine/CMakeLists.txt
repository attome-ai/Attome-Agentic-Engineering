cmake_minimum_required(VERSION 3.24)
project(structura_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W3 /permissive- /utf-8 /EHsc /bigobj)
  add_compile_definitions(_USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

find_package(OpenCASCADE REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED)
find_package(doctest REQUIRED)
find_package(Eigen3 REQUIRED CONFIG)

set(OCCT_TOOLKITS TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep TKGeomAlgo TKTopAlgo TKPrim TKBO TKBool
    TKFillet TKOffset TKShHealing TKMesh TKXSBase TKDESTEP TKDESTL TKDE TKXCAF TKLCAF TKCAF TKCDF TKV3d TKService)

add_library(structura STATIC
  src/expr.cpp
  src/graph.cpp
  src/features.cpp
  src/compile.cpp
  src/faces.cpp
  src/sha256.cpp
  src/design.cpp
  src/gate.cpp
  src/process.cpp
  src/inp.cpp
  src/mesh.cpp
  src/step_io.cpp
  src/fea.cpp
  src/export.cpp
  src/analyses.cpp
  src/validation.cpp
  src/fem.cpp
  src/repair.cpp
)
set_target_properties(structura PROPERTIES OUTPUT_NAME structura_core)
target_include_directories(structura PUBLIC include ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(structura PUBLIC ${OCCT_TOOLKITS} nlohmann_json::nlohmann_json Eigen3::Eigen)
target_compile_definitions(structura PUBLIC STRUCTURA_ROOT="${CMAKE_SOURCE_DIR}/..")

add_executable(structura_cli src/cli/main.cpp)
set_target_properties(structura_cli PROPERTIES OUTPUT_NAME structura)
target_link_libraries(structura_cli PRIVATE structura)

add_executable(engine_tests
  tests/main.cpp
  tests/test_expr.cpp
  tests/test_grammar.cpp
  tests/test_bracket.cpp
  tests/test_design.cpp
  tests/test_gate.cpp
  tests/test_mesh.cpp
  tests/test_fea.cpp
  tests/test_export.cpp
  tests/test_validation.cpp
  tests/test_golden.cpp
  tests/test_crosscheck.cpp
  tests/test_repair.cpp
)
target_link_libraries(engine_tests PRIVATE structura doctest::doctest)

enable_testing()
add_test(NAME engine_tests COMMAND engine_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..)
