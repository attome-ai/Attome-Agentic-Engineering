#include "structura/export.hpp"

#include <filesystem>
#include <fstream>
#include <map>

#include <BRepMesh_IncrementalMesh.hxx>
#include <StlAPI_Writer.hxx>

#include "structura/design.hpp"

namespace fs = std::filesystem;

namespace structura {

void write_stl(const TopoDS_Shape& shape, const std::string& path, double tolerance) {
  BRepMesh_IncrementalMesh mesher(shape, tolerance, false, 0.2, true);
  StlAPI_Writer w;
  w.ASCIIMode() = false;
  if (!w.Write(shape, path.c_str())) throw std::runtime_error("cannot write STL to " + path);
}

void write_vtu(const Mesh& m, const Result& r, const std::string& path) {
  std::map<int, int> index;  // node id -> 0-based point index
  int k = 0;
  for (auto& [id, _] : m.nodes) index[id] = k++;
  std::ofstream o(path);
  if (!o) throw std::runtime_error("cannot write " + path);
  o.precision(9);
  o << "<?xml version=\"1.0\"?>\n<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n"
    << "<UnstructuredGrid>\n<Piece NumberOfPoints=\"" << m.nodes.size() << "\" NumberOfCells=\"" << m.tets.size() << "\">\n"
    << "<PointData Scalars=\"von_mises_MPa\" Vectors=\"displacement_mm\">\n"
    << "<DataArray type=\"Float64\" Name=\"displacement_mm\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (auto& [id, _] : m.nodes) {
    auto it = r.disp.find(id);
    std::array<double, 3> u = it == r.disp.end() ? std::array<double, 3>{0, 0, 0} : it->second;
    o << u[0] << " " << u[1] << " " << u[2] << "\n";
  }
  o << "</DataArray>\n<DataArray type=\"Float64\" Name=\"von_mises_MPa\" format=\"ascii\">\n";
  for (auto& [id, _] : m.nodes) {
    auto it = r.von_mises.find(id);
    o << (it == r.von_mises.end() ? 0.0 : it->second) << "\n";
  }
  o << "</DataArray>\n</PointData>\n<Points>\n<DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (auto& [id, c] : m.nodes) o << c[0] << " " << c[1] << " " << c[2] << "\n";
  o << "</DataArray>\n</Points>\n<Cells>\n<DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">\n";
  for (auto& t : m.tets) {
    for (size_t i = 1; i < t.size(); ++i) o << index.at(t[i]) << (i + 1 < t.size() ? " " : "\n");
  }
  o << "</DataArray>\n<DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">\n";
  for (size_t i = 1; i <= m.tets.size(); ++i) o << i * 10 << "\n";
  o << "</DataArray>\n<DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">\n";
  for (size_t i = 0; i < m.tets.size(); ++i) o << 24 << "\n";
  o << "</DataArray>\n</Cells>\n</Piece>\n</UnstructuredGrid>\n</VTKFile>\n";
}

std::vector<std::string> export_bundle(const TopoDS_Shape& shape, const std::string& dir) {
  fs::create_directories(dir);
  std::string step = (fs::path(dir) / "part.step").string(), stl = (fs::path(dir) / "part.stl").string();
  write_step(shape, step);
  write_stl(shape, stl);
  return {step, stl};
}

}  // namespace structura
