// Export a design bundle: STEP and STL geometry, and result fields as VTU (ParaView) (task H2-T1).
#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>

#include <nlohmann/json.hpp>

#include "structura/design.hpp"
#include "structura/fea.hpp"
#include "structura/mesh.hpp"

namespace structura {

// STL for printing; the chord tolerance must stay below a tenth of the minimum wall.
void write_stl(const TopoDS_Shape& shape, const std::string& path, double tolerance = 0.05);

// Unstructured grid of quadratic tetrahedra (VTK type 24) with displacement (vector) and von Mises (scalar)
// point data. CalculiX C3D10 and VTK quadratic-tetra node orders are identical.
void write_vtu(const Mesh& m, const Result& r, const std::string& path);

// Skin mesh for the browser viewer (G1): the boundary of the tet mesh as triangles (each quadratic face split into
// four), with per-vertex displacement and von Mises. JSON with base64 little-endian arrays: positions and disp float32
// xyz, von_mises float32, indices uint32. Units mm and MPa.
void write_skin(const Mesh& m, const Result& r, const std::string& path, double yield_MPa = 0);

// Skin with one scalar field (e.g. temperature) and no displacement, same format; `name` becomes the field key.
void write_skin_scalar(const Mesh& m, const std::map<int, double>& field, const std::string& name, const std::string& unit,
                       const std::string& path);

// G2: the volume mesh's corner nodes and one nodal field, for slicing in the viewer. JSON with base64 little-endian
// arrays: positions float32 xyz, tets uint32 (4 corners, 0-based), scalar float32.
void write_volume(const Mesh& m, const std::map<int, double>& field, const std::string& name, const std::string& unit, const std::string& path);

// Writes part.step, part.stl and, per load case, result.vtu next to result.json; returns the files written.
std::vector<std::string> export_bundle(const TopoDS_Shape& shape, const std::string& dir);

// 3MF for printing, in mm, with the print direction turned to +z and the part resting on the plate.
void write_3mf(const TopoDS_Shape& shape, const std::string& path, const std::array<double, 3>& print_direction = {0, 0, 1}, double tolerance = 0.05);
// Binary glTF for the web and AR, in metres as glTF requires.
void write_glb(const TopoDS_Shape& shape, const std::string& path, double tolerance = 0.05);
// Three hidden-line views with overall dimensions and the labelled free parameters, as DXF and PDF.
nlohmann::json write_drawing(const TopoDS_Shape& shape, const std::string& title, const std::map<std::string, std::string>& labels, const std::string& dxf_path,
                             const std::string& pdf_path);
// Everything geometry-side for one design: STEP, STL, 3MF, GLB, graph and parameters, drawing. Returns a manifest.
nlohmann::json export_all(const Design& d, const CompiledPart& part, const std::string& dir, const std::array<double, 3>& print_direction = {0, 0, 1});

}  // namespace structura
