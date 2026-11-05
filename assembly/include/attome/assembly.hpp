// Assembly planning (Level 2): parts placed in space, the order to put them together, and each part's insertion path.
//
// Method: assembly by disassembly. For a part P and a direction d, P can be taken out along d if, sliding P along d,
// it never runs into another part still in place. This is tested on the exact B-rep: points sampled on P's surface
// (and on the other parts' surfaces, sliding the opposite way) are cast as rays along the direction; a ray that enters
// another part's material blocks the move. Parts that only touch along the sliding direction do not block it. The
// part with the fewest options is not forced: any free part is removed, fasteners first; repeating gives a removal
// order, and reversed it is an assembly order with insertion direction -d. If no part can move, the parts that lock
// each other are reported instead of an order.
#pragma once
#include <array>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>
#include <nlohmann/json.hpp>

namespace attome {

struct Part {
  std::string name;
  std::string kind;   // "step" (file), "box", "boxes" (fused), "cylinder", "tube", "bolt", "plug"
  bool fastener = false;
  bool fixed = false;  // the surroundings (a wall): never moved, not an assembly step
  TopoDS_Shape shape; // placed in the assembly frame (mm)
};

// {"parts": [{"name", "kind", ...size fields..., "at": [x, y, z], "axis": [x, y, z] (cylinders, tubes, bolts, plugs),
//              "holes": [{"at", "axis", "diameter", "depth"}] (optional, any kind), "fixed": true (optional),
//              "rotate": {"axis": [x, y, z], "deg": a}, "translate": [x, y, z] (optional), "file": "part.step" (kind step)}]}
std::vector<Part> load_assembly(const nlohmann::json& j, const std::string& base_dir);

// Parts whose material overlaps (volume of intersection above tol_mm3): an assembly must not have any.
nlohmann::json overlaps(const std::vector<Part>& parts, double tol_mm3 = 1e-3);

// True if `p` can slide along unit direction `d` without entering any part in `others`.
bool can_slide(const Part& p, const std::vector<const Part*>& others, const std::array<double, 3>& d, int samples = 600);

// The plan: {"ok", "steps": [{"part", "insert_direction", "travel_mm"}] (assembly order), "locked": [...]}.
nlohmann::json plan(const std::vector<Part>& parts);

// Replays a plan's steps with exact boolean intersections at frames+1 positions along each insertion path (a check
// independent of the planner's ray casting): {"ok", "max_overlap_mm3", "failures": [...], ...}.
nlohmann::json replay(const std::vector<Part>& parts, const nlohmann::json& steps, int frames = 24, double tol_mm3 = 1e-3);

// Writes one STL per part (placed) into dir, for the viewer.
void write_parts(const std::vector<Part>& parts, const std::string& dir);

}  // namespace attome
