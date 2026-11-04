// Compile a DesignGraph into an OpenCASCADE solid and resolve its named faces.
// Failures are raised as CompileError naming the feature that caused them, never swallowed.
#pragma once
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include "structura/graph.hpp"

namespace structura {

struct CompileError : std::runtime_error {
  std::string feature_id;
  CompileError(const std::string& fid, const std::string& msg)
      : std::runtime_error("feature '" + fid + "': " + msg), feature_id(fid) {}
};

struct Prim {
  TopoDS_Shape shape;
  bool cut = false;
};

struct CompiledPart {
  TopoDS_Shape shape;
  Values values;
  DesignGraph graph;
  std::map<std::string, std::vector<Prim>> prims;

  size_t solid_count() const;
  std::map<std::string, std::vector<TopoDS_Face>> named_faces() const;
  std::optional<std::vector<double>> design_space() const;
};

CompiledPart compile_graph(const DesignGraph& g, const Values& overrides = {});
std::vector<TopoDS_Face> select_faces(const TopoDS_Shape& s, const FaceSelector& sel, const Values& v, double tol = 1e-4);

// Feature builders (features.cpp).
std::vector<Prim> build_primitives(const Resolved& r, const std::map<std::string, std::vector<Prim>>& earlier);
TopoDS_Shape apply_modifier(const TopoDS_Shape& part, const Resolved& r);

}  // namespace structura
