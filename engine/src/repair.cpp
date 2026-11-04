#include "structura/repair.hpp"

#include <cmath>

#include <BRepAlgoAPI_Defeaturing.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include "structura/occ.hpp"

namespace structura {
namespace {

// A face is a removable small feature when its effective width (2 x area / perimeter) is below the minimum feature
// size: this catches tiny fillets and slivers but not a large face that merely shares a short edge with one.
bool is_small(const TopoDS_Face& f, double min_feature) {
  double a = area(f), p = 0;
  for (auto& e : edges_of(f)) p += edge_length(e);
  if (p <= 0) return false;
  return a < min_feature * min_feature || 2 * a / p < min_feature;
}

// A frozen face survives if some face of the result has the same area and centre (within tolerance).
bool frozen_intact(const TopoDS_Shape& out, const std::vector<TopoDS_Face>& frozen) {
  auto faces = faces_of(out);
  for (auto& f : frozen) {
    double a = area(f);
    gp_Pnt c = centre(f);
    bool found = false;
    for (auto& g : faces)
      if (std::fabs(area(g) - a) <= 1e-6 * std::max(1.0, a) && centre(g).Distance(c) <= 1e-5) { found = true; break; }
    if (!found) return false;
  }
  return true;
}

}  // namespace

RepairResult repair(const TopoDS_Shape& shape, const RepairOptions& opt) {
  RepairResult r;
  r.shape = shape;
  const double v0 = volume(shape);
  TopoDS_Shape cur = shape;

  // 1. remove faces below the minimum feature size, never frozen ones
  TopTools_ListOfShape remove;
  int nsmall = 0;
  for (auto& f : faces_of(cur)) {
    if (!is_small(f, opt.min_feature)) continue;
    bool fr = false;
    for (auto& z : opt.frozen)
      if (f.IsSame(z)) fr = true;
    if (fr) continue;
    remove.Append(f);
    ++nsmall;
  }
  if (nsmall > 0) {
    try {
      BRepAlgoAPI_Defeaturing df;
      df.SetShape(cur);
      df.AddFacesToRemove(remove);
      df.SetRunParallel(false);
      df.SetToFillHistory(false);
      df.Build();
      if (df.IsDone() && !df.HasErrors()) {
        TopoDS_Shape s = df.Shape();
        r.log.push_back({{"operation", "defeature"}, {"faces_affected", nsmall}, {"volume_change_pct", 100 * (volume(s) - v0) / v0}});
        cur = s;
        r.changed = true;
      } else {
        r.log.push_back({{"operation", "defeature"}, {"faces_affected", nsmall}, {"failed", true}});
      }
    } catch (...) {
      r.log.push_back({{"operation", "defeature"}, {"faces_affected", nsmall}, {"failed", true}, {"error", occ_message()}});
    }
  }

  // 2. merge faces lying on the same surface (removes slivers left by booleans)
  {
    size_t before = faces_of(cur).size();
    ShapeUpgrade_UnifySameDomain u(cur, true, true, true);
    u.Build();
    TopoDS_Shape s = u.Shape();
    size_t after = faces_of(s).size();
    if (after < before) {
      r.log.push_back({{"operation", "merge_same_domain"}, {"faces_affected", before - after}, {"volume_change_pct", 100 * (volume(s) - v0) / v0}});
      cur = s;
      r.changed = true;
    }
  }

  // 3. heal small gaps and tolerances
  if (!is_valid(cur)) {
    ShapeFix_Shape fix(cur);
    fix.SetPrecision(1e-6);
    fix.SetMaxTolerance(opt.min_feature / 10);
    fix.Perform();
    TopoDS_Shape s = fix.Shape();
    r.log.push_back({{"operation", "heal"}, {"valid_after", is_valid(s)}, {"volume_change_pct", 100 * (volume(s) - v0) / v0}});
    cur = s;
    r.changed = true;
  }

  if (!r.changed) {
    r.accepted = true;
    return r;
  }
  double dv = std::fabs(volume(cur) - v0) / v0;
  if (dv > opt.max_volume_change) {
    r.reason = "repair changed the volume by " + std::to_string(100 * dv) + "% (limit " + std::to_string(100 * opt.max_volume_change) + "%)";
    return r;
  }
  if (!frozen_intact(cur, opt.frozen)) {
    r.reason = "repair touched a frozen interface face";
    return r;
  }
  r.accepted = true;
  r.shape = cur;
  return r;
}

RepairGateResult repair_and_gate(const TopoDS_Shape& shape, const GateLimits& lim, const RepairOptions& opt) {
  RepairGateResult out;
  out.before = gate(shape, lim);
  out.shape = shape;
  if (out.before.ok) {
    out.ok = true;
    out.after = out.before;
    return out;
  }
  out.repair = repair(shape, opt);
  if (!out.repair.accepted || !out.repair.changed) {
    out.after = out.before;
    return out;
  }
  out.after = gate(out.repair.shape, lim);
  out.ok = out.after.ok;
  if (out.ok) out.shape = out.repair.shape;
  return out;
}

nlohmann::json RepairGateResult::to_json() const {
  return {{"ok", ok}, {"gate_before", before.to_json()}, {"gate_after", after.to_json()},
          {"repair", {{"changed", repair.changed}, {"accepted", repair.accepted}, {"reason", repair.reason}, {"log", repair.log}}}};
}

}  // namespace structura
