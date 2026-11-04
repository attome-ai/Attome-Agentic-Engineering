// Meshing with gmsh run as a subprocess (.geo script in, .inp mesh out) and a fixed fallback ladder:
// Delaunay, HXT (single-threaded), Frontal at the requested size, then Delaunay at 1.5 x size. Named faces become physical groups;
// each group's meshed area is checked against the faces it should contain, so a selection that picked up the wrong
// faces is caught instead of silently loading the wrong surface.
#pragma once
#include <array>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace structura {

// ---------------------------------------------------------------- .inp reading
struct InpBlock {
  std::string type, elset;
  std::vector<std::vector<int>> rows;  // [eid, n1, n2, ...]
};
struct InpData {
  std::map<int, std::array<double, 3>> nodes;
  std::vector<InpBlock> blocks;
  std::map<std::string, std::vector<int>> nsets;  // names upper-cased (gmsh writes them so)
};
InpData parse_inp(const std::string& text);

// ---------------------------------------------------------------- meshing
struct MeshError : std::runtime_error {
  nlohmann::json attempts;
  MeshError(const std::string& m, nlohmann::json a) : std::runtime_error(m), attempts(std::move(a)) {}
};

struct MeshOptions {
  double size = 4.0;           // mm, maximum element size
  int order = 2;               // 2 = C3D10
  double timeout_s = 120;
  int threads = 1;
  double area_tol = 0.02;      // relative tolerance of the named-group area check
  double size_min_factor = 0.2;
  int curvature_points = 12;   // elements per 2*pi of curvature
  std::string size_map;        // C4: gmsh .pos scalar view giving the element size everywhere (adaptive refinement)
  double local_size = 0;       // C4: element size on and near the named faces (0 = none), growing to `size` over 4 x local_size
};

struct Mesh {
  std::map<int, std::array<double, 3>> nodes;
  std::vector<std::vector<int>> tets;                         // [eid, n1..n10], CalculiX C3D10 node order
  std::map<std::string, std::vector<std::vector<int>>> surfaces;  // group -> [eid, n1..n6] boundary triangles
  std::map<std::string, std::vector<int>> nsets;              // group -> node ids
  nlohmann::json report;
  size_t dof() const { return 3 * nodes.size(); }
};

Mesh mesh_shape(const TopoDS_Shape& shape, const std::map<std::string, std::vector<TopoDS_Face>>& named,
                const std::string& workdir, const MeshOptions& opt = {});

double tet_quality(const std::map<int, std::array<double, 3>>& nodes, const std::vector<int>& tet);
double tri_area(const std::map<int, std::array<double, 3>>& nodes, const std::vector<int>& tri);

}  // namespace structura
