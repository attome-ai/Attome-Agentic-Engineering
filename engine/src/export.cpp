#include "structura/export.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

namespace {
std::string b64(const void* data, size_t n) {
  static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const unsigned char* p = static_cast<const unsigned char*>(data);
  std::string out;
  out.reserve((n + 2) / 3 * 4);
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = p[i] << 16 | (i + 1 < n ? p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
    out += t[(v >> 18) & 63];
    out += t[(v >> 12) & 63];
    out += i + 1 < n ? t[(v >> 6) & 63] : '=';
    out += i + 2 < n ? t[v & 63] : '=';
  }
  return out;
}
}  // namespace

void write_skin(const Mesh& m, const Result& r, const std::string& path, double yield_MPa) {
  // boundary faces: tet faces (by sorted corners) that belong to exactly one element; keep the 6 nodes in order
  static const int F[4][6] = {{1, 2, 3, 5, 6, 7}, {1, 4, 2, 8, 9, 5}, {2, 4, 3, 9, 10, 6}, {3, 4, 1, 10, 8, 7}};
  std::map<std::array<int, 3>, std::pair<int, std::array<int, 6>>> faces;  // key -> (count, nodes)
  for (auto& t : m.tets) {
    if (t.size() < 11) continue;
    for (auto& f : F) {
      std::array<int, 6> n;
      for (int k = 0; k < 6; ++k) n[k] = t[f[k]];
      std::array<int, 3> key = {n[0], n[1], n[2]};
      std::sort(key.begin(), key.end());
      auto it = faces.find(key);
      if (it == faces.end()) faces[key] = {1, n};
      else it->second.first++;
    }
  }
  std::map<int, uint32_t> index;
  std::vector<float> pos, disp, vm;
  std::vector<uint32_t> idx;
  auto vid = [&](int node) {
    auto it = index.find(node);
    if (it != index.end()) return it->second;
    uint32_t i = static_cast<uint32_t>(index.size());
    index[node] = i;
    auto& c = m.nodes.at(node);
    pos.insert(pos.end(), {float(c[0]), float(c[1]), float(c[2])});
    auto du = r.disp.find(node);
    std::array<double, 3> u = du == r.disp.end() ? std::array<double, 3>{0, 0, 0} : du->second;
    disp.insert(disp.end(), {float(u[0]), float(u[1]), float(u[2])});
    auto s = r.von_mises.find(node);
    vm.push_back(float(s == r.von_mises.end() ? 0.0 : s->second));
    return i;
  };
  for (auto& [_, cf] : faces) {
    if (cf.first != 1) continue;
    auto& n = cf.second;  // corners a b c, mids ab bc ca; outward orientation follows the tet face order
    uint32_t a = vid(n[0]), b = vid(n[1]), c = vid(n[2]), ab = vid(n[3]), bc = vid(n[4]), ca = vid(n[5]);
    // tet face orders above are inward for CalculiX numbering; reverse to make normals point out of the part
    for (auto tri : {std::array<uint32_t, 3>{a, ca, ab}, {ab, bc, b}, {ca, c, bc}, {ab, ca, bc}}) idx.insert(idx.end(), tri.begin(), tri.end());
  }
  float vmax = 0, umax = 0;
  for (float v : vm) vmax = std::max(vmax, v);
  for (size_t i = 0; i < disp.size(); i += 3) umax = std::max(umax, std::sqrt(disp[i] * disp[i] + disp[i + 1] * disp[i + 1] + disp[i + 2] * disp[i + 2]));
  nlohmann::json j = {{"format", "attome-skin-1"}, {"units", {{"length", "mm"}, {"stress", "MPa"}}},
                      {"vertices", index.size()}, {"triangles", idx.size() / 3}, {"max_von_mises_MPa", vmax}, {"max_disp_mm", umax},
                      {"yield_MPa", yield_MPa},
                      {"positions", b64(pos.data(), pos.size() * 4)}, {"disp", b64(disp.data(), disp.size() * 4)},
                      {"von_mises", b64(vm.data(), vm.size() * 4)}, {"indices", b64(idx.data(), idx.size() * 4)}};
  std::ofstream(path) << j.dump();
}

std::vector<std::string> export_bundle(const TopoDS_Shape& shape, const std::string& dir) {
  fs::create_directories(dir);
  std::string step = (fs::path(dir) / "part.step").string(), stl = (fs::path(dir) / "part.stl").string();
  write_step(shape, step);
  write_stl(shape, stl);
  return {step, stl};
}

}  // namespace structura
