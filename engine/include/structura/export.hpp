// Export a design bundle: STEP and STL geometry, and result fields as VTU (ParaView) (task H2-T1).
#pragma once
#include <string>

#include <TopoDS_Shape.hxx>

#include "structura/fea.hpp"
#include "structura/mesh.hpp"

namespace structura {

// STL for printing; the chord tolerance must stay below a tenth of the minimum wall.
void write_stl(const TopoDS_Shape& shape, const std::string& path, double tolerance = 0.05);

// Unstructured grid of quadratic tetrahedra (VTK type 24) with displacement (vector) and von Mises (scalar)
// point data. CalculiX C3D10 and VTK quadratic-tetra node orders are identical.
void write_vtu(const Mesh& m, const Result& r, const std::string& path);

// Writes part.step, part.stl and, per load case, result.vtu next to result.json; returns the files written.
std::vector<std::string> export_bundle(const TopoDS_Shape& shape, const std::string& dir);

}  // namespace structura
