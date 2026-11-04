// A Design: a family graph plus parameter values, with a stable content hash.
// The hash covers the canonical graph JSON and the parameters rounded to 6 significant digits.
#pragma once
#include <string>

#include <TopoDS_Shape.hxx>

#include "structura/compile.hpp"
#include "structura/graph.hpp"

namespace structura {

std::string sha256_hex(const std::string& msg);
double round6(double v);

struct Design {
  DesignGraph graph;
  Values params;
  std::string created_at;

  Design(DesignGraph g, const Values& overrides = {}, std::string created = "");
  std::string hash() const;
  CompiledPart build() const;
  void save(const std::string& folder) const;  // graph.json, params.json
  static Design load(const std::string& folder);
};

std::string utc_now();

void write_step(const TopoDS_Shape& s, const std::string& path);
TopoDS_Shape read_step(const std::string& path);

std::string families_dir();                       // <repo>/families
DesignGraph load_family(const std::string& name);  // also checks every declared parameter is used

}  // namespace structura
