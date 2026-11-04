// H2: delivery formats beyond STEP and STL. 3MF (printing, with the print direction applied), GLB (web and AR) and a
// dimensioned three-view drawing as DXF and PDF, with hidden lines removed. No external writer: the formats are small.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>

#include "structura/design.hpp"
#include "structura/export.hpp"

namespace fs = std::filesystem;

namespace structura {

namespace {

struct TriMesh {
  std::vector<std::array<float, 3>> pos;
  std::vector<std::array<uint32_t, 3>> tri;
};

TriMesh triangulate(const TopoDS_Shape& shape, double tolerance) {
  BRepMesh_IncrementalMesh mesher(shape, tolerance, false, 0.2, true);
  TriMesh out;
  for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
    TopoDS_Face face = TopoDS::Face(ex.Current());
    TopLoc_Location loc;
    Handle(Poly_Triangulation) pt = BRep_Tool::Triangulation(face, loc);
    if (pt.IsNull()) throw std::runtime_error("a face has no triangulation");
    const uint32_t base = static_cast<uint32_t>(out.pos.size());
    for (int i = 1; i <= pt->NbNodes(); ++i) {
      gp_Pnt p = pt->Node(i).Transformed(loc.Transformation());
      out.pos.push_back({float(p.X()), float(p.Y()), float(p.Z())});
    }
    const bool flip = face.Orientation() == TopAbs_REVERSED;
    for (int i = 1; i <= pt->NbTriangles(); ++i) {
      int a, b, c;
      pt->Triangle(i).Get(a, b, c);
      if (flip) std::swap(b, c);
      out.tri.push_back({base + a - 1, base + b - 1, base + c - 1});
    }
  }
  if (out.tri.empty()) throw std::runtime_error("nothing to export: the shape has no triangles");
  return out;
}

uint32_t crc32(const std::string& data) {
  uint32_t crc = 0xFFFFFFFFu;
  for (unsigned char ch : data) {
    crc ^= ch;
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

void put16(std::string& s, uint16_t v) { s.append(reinterpret_cast<const char*>(&v), 2); }
void put32(std::string& s, uint32_t v) { s.append(reinterpret_cast<const char*>(&v), 4); }

// A zip archive with stored (uncompressed) entries, which every 3MF reader accepts.
std::string zip_stored(const std::vector<std::pair<std::string, std::string>>& files) {
  std::string out, central;
  for (auto& [name, data] : files) {
    const uint32_t offset = static_cast<uint32_t>(out.size()), crc = crc32(data), size = static_cast<uint32_t>(data.size());
    std::string head;
    put16(head, 20);  // version needed
    put16(head, 0);   // flags
    put16(head, 0);   // method: stored
    put16(head, 0);   // time
    put16(head, 0x21);  // date: 1980-01-01
    put32(head, crc);
    put32(head, size);
    put32(head, size);
    put16(head, static_cast<uint16_t>(name.size()));
    put16(head, 0);
    put32(out, 0x04034b50u);
    out += head + name + data;
    put32(central, 0x02014b50u);
    put16(central, 20);
    central += head;
    put16(central, 0);  // comment
    put16(central, 0);  // disk
    put16(central, 0);  // internal attributes
    put32(central, 0);  // external attributes
    put32(central, offset);
    central += name;
  }
  const uint32_t start = static_cast<uint32_t>(out.size()), length = static_cast<uint32_t>(central.size());
  out += central;
  put32(out, 0x06054b50u);
  put16(out, 0);
  put16(out, 0);
  put16(out, static_cast<uint16_t>(files.size()));
  put16(out, static_cast<uint16_t>(files.size()));
  put32(out, length);
  put32(out, start);
  put16(out, 0);
  return out;
}

// Rotation taking `dir` to +z (Rodrigues), for the 3MF build transform.
std::array<std::array<double, 3>, 3> rotation_to_z(std::array<double, 3> d) {
  const double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (n <= 0) throw std::runtime_error("print direction is zero");
  for (auto& v : d) v /= n;
  if (d[2] < -1 + 1e-12) return {{{1, 0, 0}, {0, -1, 0}, {0, 0, -1}}};
  const double vx = d[1], vy = -d[0], k = 1 / (1 + d[2]);  // v = d x z
  return {{{1 - k * vy * vy, k * vx * vy, vy}, {k * vx * vy, 1 - k * vx * vx, -vx}, {-vy, vx, d[2]}}};
}

struct Segment {
  double x0, y0, x1, y1;
  bool hidden;
};

// Hidden-line projection along `view`: visible and hidden sharp and outline edges as 2D segments.
std::vector<Segment> project(const TopoDS_Shape& shape, const gp_Ax2& view) {
  Handle(HLRBRep_Algo) algo = new HLRBRep_Algo();
  algo->Add(shape);
  algo->Projector(HLRAlgo_Projector(view));
  algo->Update();
  algo->Hide();
  HLRBRep_HLRToShape hlr(algo);
  std::vector<Segment> out;
  auto add = [&](TopoDS_Shape s, bool hidden) {
    if (s.IsNull()) return;
    BRepLib::BuildCurves3d(s);
    for (TopExp_Explorer ex(s, TopAbs_EDGE); ex.More(); ex.Next()) {
      BRepAdaptor_Curve c(TopoDS::Edge(ex.Current()));
      GCPnts_TangentialDeflection pts(c, 0.1, 0.05);
      for (int i = 1; i < pts.NbPoints(); ++i) {
        gp_Pnt a = pts.Value(i), b = pts.Value(i + 1);
        out.push_back({a.X(), a.Y(), b.X(), b.Y(), hidden});
      }
    }
  };
  add(hlr.VCompound(), false);
  add(hlr.OutLineVCompound(), false);
  add(hlr.Rg1LineVCompound(), false);
  add(hlr.HCompound(), true);
  add(hlr.OutLineHCompound(), true);
  return out;
}

std::string fmt(double v, int digits = 3) {
  std::ostringstream o;
  o.setf(std::ios::fixed);
  o.precision(digits);
  o << v;
  return o.str();
}

}  // namespace

void write_3mf(const TopoDS_Shape& shape, const std::string& path, const std::array<double, 3>& print_direction, double tolerance) {
  TriMesh m = triangulate(shape, tolerance);
  auto R = rotation_to_z(print_direction);
  // rest the rotated part on the build plate
  double zmin = 1e300;
  for (auto& p : m.pos) zmin = std::min(zmin, R[2][0] * p[0] + R[2][1] * p[1] + R[2][2] * p[2]);
  std::ostringstream model;
  model.precision(8);
  model << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
        << "<resources><object id=\"1\" type=\"model\"><mesh><vertices>\n";
  for (auto& p : m.pos) model << "<vertex x=\"" << p[0] << "\" y=\"" << p[1] << "\" z=\"" << p[2] << "\"/>\n";
  model << "</vertices><triangles>\n";
  for (auto& t : m.tri) model << "<triangle v1=\"" << t[0] << "\" v2=\"" << t[1] << "\" v3=\"" << t[2] << "\"/>\n";
  // 3MF transform: row vectors, "m00 m01 m02 m10 m11 m12 m20 m21 m22 m30 m31 m32", so the rows are the columns of R
  model << "</triangles></mesh></object></resources>\n<build><item objectid=\"1\" transform=\"" << R[0][0] << " " << R[1][0] << " " << R[2][0] << " "
        << R[0][1] << " " << R[1][1] << " " << R[2][1] << " " << R[0][2] << " " << R[1][2] << " " << R[2][2] << " 0 0 " << -zmin << "\"/></build>\n</model>\n";
  const std::string types =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
      "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
      "<Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/></Types>\n";
  const std::string rels =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
      "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/></Relationships>\n";
  std::ofstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("cannot write " + path);
  f << zip_stored({{"[Content_Types].xml", types}, {"_rels/.rels", rels}, {"3D/3dmodel.model", model.str()}});
}

void write_glb(const TopoDS_Shape& shape, const std::string& path, double tolerance) {
  TriMesh m = triangulate(shape, tolerance);
  std::array<float, 3> lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
  std::string bin;
  bin.reserve(m.pos.size() * 12 + m.tri.size() * 12);
  for (auto& t : m.tri) bin.append(reinterpret_cast<const char*>(t.data()), 12);
  const size_t index_bytes = bin.size();
  for (auto p : m.pos) {
    for (int i = 0; i < 3; ++i) {
      p[i] *= 0.001f;  // glTF is in metres; the engine is in millimetres
      lo[i] = std::min(lo[i], p[i]);
      hi[i] = std::max(hi[i], p[i]);
    }
    bin.append(reinterpret_cast<const char*>(p.data()), 12);
  }
  nlohmann::json g = {
      {"asset", {{"version", "2.0"}, {"generator", "Attome AE engine"}}},
      {"scene", 0},
      {"scenes", {{{"nodes", {0}}}}},
      {"nodes", {{{"mesh", 0}, {"name", "part"}}}},
      {"meshes", {{{"primitives", {{{"attributes", {{"POSITION", 1}}}, {"indices", 0}, {"mode", 4}}}}}}},
      {"accessors",
       {{{"bufferView", 0}, {"componentType", 5125}, {"count", m.tri.size() * 3}, {"type", "SCALAR"}},
        {{"bufferView", 1}, {"componentType", 5126}, {"count", m.pos.size()}, {"type", "VEC3"}, {"min", lo}, {"max", hi}}}},
      {"bufferViews",
       {{{"buffer", 0}, {"byteOffset", 0}, {"byteLength", index_bytes}, {"target", 34963}},
        {{"buffer", 0}, {"byteOffset", index_bytes}, {"byteLength", m.pos.size() * 12}, {"target", 34962}}}},
      {"buffers", {{{"byteLength", bin.size()}}}}};
  std::string js = g.dump();
  while (js.size() % 4) js += ' ';
  while (bin.size() % 4) bin += '\0';
  std::string out;
  put32(out, 0x46546C67u);  // "glTF"
  put32(out, 2);
  put32(out, static_cast<uint32_t>(12 + 8 + js.size() + 8 + bin.size()));
  put32(out, static_cast<uint32_t>(js.size()));
  put32(out, 0x4E4F534Au);  // "JSON"
  out += js;
  put32(out, static_cast<uint32_t>(bin.size()));
  put32(out, 0x004E4942u);  // "BIN\0"
  out += bin;
  std::ofstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("cannot write " + path);
  f << out;
}

nlohmann::json write_drawing(const TopoDS_Shape& shape, const std::string& title, const std::map<std::string, std::string>& labels, const std::string& dxf_path,
                             const std::string& pdf_path) {
  Bnd_Box box;
  BRepBndLib::AddOptimal(shape, box, false, false);
  double x0, y0, z0, x1, y1, z1;
  box.Get(x0, y0, z0, x1, y1, z1);
  const double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
  // first-angle free layout on a sheet in mm of model space: front (x-z), top (x-y) below it, side (y-z) to its right
  const gp_Pnt o(0, 0, 0);
  auto front = project(shape, gp_Ax2(o, gp_Dir(0, -1, 0), gp_Dir(1, 0, 0)));  // looking along +y: u = x, v = z
  auto top = project(shape, gp_Ax2(o, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)));     // looking down: u = x, v = y
  auto side = project(shape, gp_Ax2(o, gp_Dir(1, 0, 0), gp_Dir(0, 1, 0)));    // looking along -x: u = y, v = z
  const double gap = 0.25 * std::max({dx, dy, dz}) + 15;
  struct Placed {
    const std::vector<Segment>* segs;
    double ox, oy;
  };
  const std::vector<Placed> views = {
      {&front, -x0, gap + dy - z0},
      {&top, -x0, -y0},
      {&side, dx + gap - y0, gap + dy - z0},
  };
  struct Line {
    double x0, y0, x1, y1;
    bool hidden;
  };
  struct Text {
    double x, y, h;
    std::string s;
  };
  std::vector<Line> lines;
  std::vector<Text> texts;
  for (auto& v : views)
    for (auto& s : *v.segs) lines.push_back({s.x0 + v.ox, s.y0 + v.oy, s.x1 + v.ox, s.y1 + v.oy, s.hidden});
  // overall dimensions, measured on the solid
  const double h = std::max(3.0, 0.03 * std::max({dx, dy, dz}));
  auto dim = [&](double xa, double ya, double xb, double yb, double value, bool horizontal) {
    const double off = 2.2 * h;
    if (horizontal) {
      lines.push_back({xa, ya - off, xb, yb - off, false});
      lines.push_back({xa, ya, xa, ya - off - h / 2, false});
      lines.push_back({xb, yb, xb, yb - off - h / 2, false});
      texts.push_back({(xa + xb) / 2 - 1.5 * h, ya - off + 0.4 * h, h, fmt(value, 2)});
    } else {
      lines.push_back({xa - off, ya, xb - off, yb, false});
      lines.push_back({xa, ya, xa - off - h / 2, ya, false});
      lines.push_back({xb, yb, xb - off - h / 2, yb, false});
      texts.push_back({xa - off - 3.6 * h, (ya + yb) / 2, h, fmt(value, 2)});
    }
  };
  const double fy = gap + dy;
  dim(0, 0, dx, 0, dx, true);            // length, under the top view
  dim(0, 0, 0, dy, dy, false);           // width, beside the top view
  dim(0, fy, 0, fy + dz, dz, false);     // height, beside the front view
  double ty = -6.5 * h;
  texts.push_back({0, fy + dz + 2 * h, 1.3 * h, title});
  texts.push_back({0, ty, h, "All dimensions in mm. Solid lines visible, grey lines hidden. Free parameters of this design:"});
  for (auto& [k, v] : labels) {
    ty -= 1.5 * h;
    texts.push_back({0, ty, h, k + " = " + v});
  }
  // DXF R12: lines and text on two layers
  {
    std::ofstream f(dxf_path);
    if (!f) throw std::runtime_error("cannot write " + dxf_path);
    f << "0\nSECTION\n2\nENTITIES\n";
    for (auto& l : lines)
      f << "0\nLINE\n8\n" << (l.hidden ? "HIDDEN" : "VISIBLE") << "\n10\n" << l.x0 << "\n20\n" << l.y0 << "\n30\n0\n11\n" << l.x1 << "\n21\n" << l.y1 << "\n31\n0\n";
    for (auto& t : texts) f << "0\nTEXT\n8\nNOTES\n10\n" << t.x << "\n20\n" << t.y << "\n30\n0\n40\n" << t.h << "\n1\n" << t.s << "\n";
    f << "0\nENDSEC\n0\nEOF\n";
  }
  // PDF 1.4, one A3 landscape page, vector lines, scaled to fit
  {
    double lo_x = 1e300, lo_y = 1e300, hi_x = -1e300, hi_y = -1e300;
    for (auto& l : lines) {
      lo_x = std::min({lo_x, l.x0, l.x1});
      hi_x = std::max({hi_x, l.x0, l.x1});
      lo_y = std::min({lo_y, l.y0, l.y1});
      hi_y = std::max({hi_y, l.y0, l.y1});
    }
    for (auto& t : texts) {
      lo_x = std::min(lo_x, t.x);
      hi_x = std::max(hi_x, t.x + 0.55 * t.h * static_cast<double>(t.s.size()));
      lo_y = std::min(lo_y, t.y);
      hi_y = std::max(hi_y, t.y + t.h);
    }
    const double W = 1190.55, H = 841.89, M = 28;  // A3 in points
    const double scale = std::min((W - 2 * M) / (hi_x - lo_x), (H - 2 * M) / (hi_y - lo_y));
    auto X = [&](double v) { return M + (v - lo_x) * scale; };
    auto Y = [&](double v) { return M + (v - lo_y) * scale; };
    std::ostringstream c;
    c.precision(6);
    c << "0.6 w\n0.6 0.6 0.6 RG\n";
    for (auto& l : lines)
      if (l.hidden) c << X(l.x0) << " " << Y(l.y0) << " m " << X(l.x1) << " " << Y(l.y1) << " l S\n";
    c << "0 0 0 RG\n";
    for (auto& l : lines)
      if (!l.hidden) c << X(l.x0) << " " << Y(l.y0) << " m " << X(l.x1) << " " << Y(l.y1) << " l S\n";
    for (auto& t : texts) {
      std::string s;
      for (char ch : t.s) {
        if (ch == '(' || ch == ')' || ch == '\\') s += '\\';
        s += ch;
      }
      c << "BT /F1 " << t.h * scale << " Tf " << X(t.x) << " " << Y(t.y) << " Td (" << s << ") Tj ET\n";
    }
    const std::string content = c.str();
    std::vector<std::string> objs = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + fmt(W, 2) + " " + fmt(H, 2) + "] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
        "<< /Length " + std::to_string(content.size()) + " >>\nstream\n" + content + "\nendstream",
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"};
    std::string pdf = "%PDF-1.4\n";
    std::vector<size_t> at;
    for (size_t i = 0; i < objs.size(); ++i) {
      at.push_back(pdf.size());
      pdf += std::to_string(i + 1) + " 0 obj\n" + objs[i] + "\nendobj\n";
    }
    const size_t xref = pdf.size();
    pdf += "xref\n0 " + std::to_string(objs.size() + 1) + "\n0000000000 65535 f \n";
    for (size_t a : at) {
      char row[32];
      std::snprintf(row, sizeof row, "%010zu 00000 n \n", a);
      pdf += row;
    }
    pdf += "trailer\n<< /Size " + std::to_string(objs.size() + 1) + " /Root 1 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
    std::ofstream f(pdf_path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + pdf_path);
    f << pdf;
  }
  size_t visible = 0;
  for (auto& l : lines) visible += !l.hidden;
  return {{"visible_segments", visible}, {"hidden_segments", lines.size() - visible}, {"overall_mm", {dx, dy, dz}}, {"dxf", dxf_path}, {"pdf", pdf_path}};
}

nlohmann::json export_all(const Design& d, const CompiledPart& part, const std::string& dir, const std::array<double, 3>& print_direction) {
  fs::create_directories(dir);
  const fs::path o(dir);
  auto files = export_bundle(part.shape, dir);
  write_3mf(part.shape, (o / "part.3mf").string(), print_direction);
  write_glb(part.shape, (o / "part.glb").string());
  d.save(dir);  // graph.json and params.json: the editable source of the design
  std::map<std::string, std::string> labels;
  for (auto& [k, v] : d.params) {
    auto it = std::find_if(d.graph.parameters.begin(), d.graph.parameters.end(), [&](auto& p) { return p.first == k; });
    labels[k] = fmt(v, 3) + (it != d.graph.parameters.end() && it->second.unit != "1" ? " " + it->second.unit : "");
  }
  nlohmann::json drawing = write_drawing(part.shape, d.graph.name + " " + d.hash().substr(0, 12), labels, (o / "drawing.dxf").string(), (o / "drawing.pdf").string());
  files.push_back((o / "part.3mf").string());
  files.push_back((o / "part.glb").string());
  files.push_back((o / "graph.json").string());
  files.push_back((o / "params.json").string());
  files.push_back((o / "drawing.dxf").string());
  files.push_back((o / "drawing.pdf").string());
  nlohmann::json listing = nlohmann::json::array();
  for (auto& f : files) listing.push_back({{"file", fs::path(f).filename().string()}, {"bytes", fs::file_size(f)}});
  return {{"hash", d.hash()}, {"folder", fs::absolute(dir).string()}, {"files", listing}, {"drawing", drawing}, {"print_direction", print_direction}};
}

}  // namespace structura
