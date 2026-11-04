// A3: does an uploaded part match a part family? Search the family's parameters for the design closest to the part, using
// cheap measures (bounding box, volume, surface area), then judge the best candidate by real overlap: intersection over
// union of the two solids. Only a high overlap counts as a fit, because then the family's parameters can stand in for the
// uploaded geometry and the normal optimizer can redesign it. The part is compared where it lies; it is not re-oriented.
#include <algorithm>
#include <cmath>
#include <random>

#include <BRepAlgoAPI_Common.hxx>

#include "structura/design.hpp"
#include "structura/inspect.hpp"
#include "structura/occ.hpp"

namespace structura {

namespace {
struct Measures {
  std::vector<double> box;
  double vol = 0, area = 0;
};
Measures measure(const TopoDS_Shape& s) { return {bbox(s), volume(s), area(s)}; }

double mismatch(const Measures& a, const Measures& b) {
  double e = 0;
  const double size = std::max({a.box[3] - a.box[0], a.box[4] - a.box[1], a.box[5] - a.box[2], 1e-9});
  for (int i = 0; i < 6; ++i) e += std::abs(a.box[i] - b.box[i]) / size;
  return e + std::abs(a.vol - b.vol) / std::max(a.vol, 1e-9) + std::abs(a.area - b.area) / std::max(a.area, 1e-9);
}
}  // namespace

nlohmann::json fit_family(const TopoDS_Shape& part, const std::string& family, const FitOptions& o) {
  DesignGraph g = load_family(family);
  const Measures target = measure(part);
  std::mt19937 rng(o.seed);
  auto score = [&](const Values& v) {
    try {
      return mismatch(target, measure(compile_graph(g, v).shape));
    } catch (const std::exception&) {
      return 1e30;  // this combination does not build
    }
  };
  Values best;
  double best_score = 1e30;
  for (int i = 0; i < o.samples; ++i) {
    Values v;
    for (auto& [k, p] : g.parameters) {
      if (p.integer) v[k] = std::uniform_int_distribution<int>(int(p.low), int(p.high))(rng);
      else v[k] = std::uniform_real_distribution<double>(p.low, p.high)(rng);
    }
    const double s = score(v);
    if (s < best_score) {
      best_score = s;
      best = v;
    }
  }
  if (best.empty()) throw std::runtime_error("no design of family '" + family + "' could be built");
  double fraction = 0.25;  // coordinate search from the best sample, step shrinking
  for (int round = 0; round < o.rounds; ++round, fraction *= 0.5) {
    for (auto& [k, p] : g.parameters) {
      const double step = p.integer ? 1.0 : (p.high - p.low) * fraction;
      for (double dir : {-1.0, 1.0}) {
        for (int tries = 0; tries < 6; ++tries) {
          Values v = best;
          v[k] = std::clamp(best[k] + dir * step, p.low, p.high);
          if (v[k] == best[k]) break;
          const double s = score(v);
          if (s >= best_score) break;
          best_score = s;
          best = v;
        }
      }
    }
  }
  // Cheap measures cannot separate some parameters (plate thickness against rib thickness give the same volume), so the
  // last stage searches on real overlap.
  auto overlap_of = [&](const Values& v) {
    try {
      CompiledPart fitted = compile_graph(g, v);
      BRepAlgoAPI_Common common(part, fitted.shape);
      if (!common.IsDone()) return 0.0;
      const double overlap = volume(common.Shape()), union_vol = volume(part) + volume(fitted.shape) - overlap;
      return union_vol > 0 ? overlap / union_vol : 0.0;
    } catch (const std::exception&) {
      return 0.0;
    }
  };
  double iou = overlap_of(best);
  fraction = 0.15;
  for (int round = 0; round < o.rounds && iou < 0.999; ++round, fraction *= 0.6) {
    for (auto& [k, p] : g.parameters) {
      const double step = p.integer ? 1.0 : (p.high - p.low) * fraction;
      for (double dir : {-1.0, 1.0}) {
        for (int tries = 0; tries < 8; ++tries) {
          Values v = best;
          v[k] = std::clamp(best[k] + dir * step, p.low, p.high);
          if (v[k] == best[k]) break;
          const double s = overlap_of(v);
          if (s <= iou) break;
          iou = s;
          best = v;
        }
      }
    }
  }
  const bool fits = iou >= o.min_iou;
  return {{"family", family}, {"fits", fits}, {"iou", iou}, {"min_iou", o.min_iou}, {"params", best}, {"cheap_mismatch", best_score},
          {"meaning", fits ? "the family reproduces this part closely: optimize these parameters, and check that the frozen interfaces are still in place"
                           : "the family does not reproduce this part: redesign it by operations on the uploaded solid, or analyse it as it is"}};
}

}  // namespace structura
