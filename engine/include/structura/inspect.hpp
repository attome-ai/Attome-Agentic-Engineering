// Interface detection on an uploaded part (task A3): faces with types, sizes and axis-aligned selectors, holes (concave
// cylinders merged by axis and radius) and hole patterns, internal fillets, the envelope and the largest planar faces
// as mating candidates; and a draft part family that imports the file (import_step) with those faces named.
#pragma once
#include <string>

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace structura {

nlohmann::json inspect_shape(const TopoDS_Shape& shape);

// A family with one import_step feature (file content pinned by SHA-256), no parameters, and named faces for the
// axis-aligned mating candidates and holes found by inspect_shape.
nlohmann::json draft_family(const std::string& name, const std::string& step_path, const nlohmann::json& inspection);

}  // namespace structura
