# Assembly planner (Level 2, docs/factory-ladder.md): a standalone C++ tool on OpenCASCADE, built separately from the
# engine so it never touches the engine's sources or build folder.
cmake_minimum_required(VERSION 3.24)
project(attome_assembly CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8 /EHsc /W3 /permissive- /bigobj)
endif()

find_package(OpenCASCADE REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED)
find_package(doctest REQUIRED)

add_library(assembly_core STATIC src/assembly.cpp src/fasteners.cpp)
target_include_directories(assembly_core PUBLIC include ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(assembly_core PUBLIC
  TKDESTEP TKDESTL TKXSBase TKBool TKBO TKPrim TKFillet TKMesh TKTopAlgo TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath TKernel
  nlohmann_json::nlohmann_json)

add_executable(attome_assembly src/main.cpp)
target_link_libraries(attome_assembly PRIVATE assembly_core)

add_executable(assembly_tests tests/test_assembly.cpp)
target_link_libraries(assembly_tests PRIVATE assembly_core doctest::doctest)

add_executable(fastener_tests tests/test_fasteners.cpp)
target_link_libraries(fastener_tests PRIVATE assembly_core doctest::doctest)
