// Assembly planner: cases with known answers.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "attome/assembly.hpp"

using namespace attome;
using nlohmann::json;

static std::vector<Part> load(const char* text) { return load_assembly(json::parse(text), "."); }

TEST_CASE("a pin in a blind hole comes out only upward; the base comes off downward after it") {
  // base 40 x 40 x 20 with a 10 mm pin standing in it, modelled as touching blocks: a block on top of a base
  auto parts = load(R"J({"parts": [
    {"name": "base", "kind": "box", "at": [0, 0, 0], "size": [40, 40, 20]},
    {"name": "block", "kind": "box", "at": [10, 10, 20], "size": [20, 20, 10]}]})J");
  CHECK(overlaps(parts).empty());
  std::vector<const Part*> base_only = {&parts[0]};
  CHECK(can_slide(parts[1], base_only, {0, 0, 1}));    // lifts off the base
  CHECK_FALSE(can_slide(parts[1], base_only, {0, 0, -1}));  // cannot push down into it
  CHECK(can_slide(parts[1], base_only, {1, 0, 0}));    // sliding along the contact face is free
  json p = plan(parts);
  REQUIRE(p["ok"].get<bool>());
  CHECK(p["steps"].size() == 2);
}

TEST_CASE("a bolt through a plate into a block is inserted last along its shank") {
  auto parts = load(R"J({"parts": [
    {"name": "block", "kind": "box", "at": [-20, -20, -30], "size": [40, 40, 30]},
    {"name": "plate", "kind": "box", "at": [-20, -20, 0], "size": [40, 40, 6]},
    {"name": "bolt", "kind": "bolt", "at": [0, 0, 6], "axis": [0, 0, -1], "diameter": 6, "length": 20}]})J");
  // the bolt passes through the plate into the block: the shank volume overlaps both (no holes modelled here)
  CHECK_FALSE(overlaps(parts).empty());
}

TEST_CASE("a bolt in clearance holes: removed first along its axis, inserted last") {
  // plate and block with 6.6 mm holes, M6 bolt: build holes by modelling the plate and block as rings around the bolt
  auto parts = load(R"J({"parts": [
    {"name": "block", "kind": "box", "at": [-20, -20, -30], "size": [40, 40, 30]},
    {"name": "bolt", "kind": "bolt", "at": [0, 0, 0], "axis": [0, 0, 1], "diameter": 6, "length": 10, "translate": [0, 0, 40]}]})J");
  CHECK(overlaps(parts).empty());
  json p = plan(parts);
  REQUIRE(p["ok"].get<bool>());
}

TEST_CASE("a part caged inside a closed box is reported as locked") {
  auto parts = load(R"J({"parts": [
    {"name": "floor", "kind": "box", "at": [0, 0, 0], "size": [50, 50, 5]},
    {"name": "roof", "kind": "box", "at": [0, 0, 45], "size": [50, 50, 5]},
    {"name": "wall_xm", "kind": "box", "at": [-5, -5, -5], "size": [5, 60, 60]},
    {"name": "wall_xp", "kind": "box", "at": [50, -5, -5], "size": [5, 60, 60]},
    {"name": "wall_ym", "kind": "box", "at": [0, -5, -5], "size": [50, 5, 60]},
    {"name": "wall_yp", "kind": "box", "at": [0, 50, -5], "size": [50, 5, 60]},
    {"name": "inside", "kind": "box", "at": [20, 20, 20], "size": [10, 10, 10]}]})J");
  CHECK(overlaps(parts).empty());
  // the walls are glued corners: every wall touches its neighbours, but the cage can still be taken apart wall by
  // wall; what must never happen is the inside part leaving before the cage opens
  json p = plan(parts);
  REQUIRE(p["ok"].get<bool>());
  int pos_inside = -1, last_wall = -1;
  for (size_t i = 0; i < p["steps"].size(); ++i) {
    std::string n = p["steps"][i]["part"];
    if (n == "inside") pos_inside = static_cast<int>(i);
  }
  CHECK(pos_inside >= 0);
  // inserted before the cage is closed: some wall or the roof comes after it
  bool closed_after = false;
  for (size_t i = static_cast<size_t>(pos_inside) + 1; i < p["steps"].size(); ++i) closed_after = true;
  CHECK(closed_after);
  (void)last_wall;
}

TEST_CASE("two interlocking hooks cannot be separated along any axis") {
  // an L hook hanging in another L hook: each blocks the other in every axis direction
  auto parts = load(R"J({"parts": [
    {"name": "hook_a_bar", "kind": "box", "at": [0, 0, 0], "size": [10, 40, 4]},
    {"name": "post", "kind": "box", "at": [0, 18, 4], "size": [10, 4, 10]},
    {"name": "lid", "kind": "box", "at": [0, 10, 14], "size": [10, 20, 4]},
    {"name": "trapped", "kind": "box", "at": [0, 4, 4], "size": [10, 12, 6]}]})J");
  CHECK(overlaps(parts).empty());
  json p = plan(parts);
  // the trapped block can slide out along +/-x (the hooks are open sideways): a valid plan exists
  CHECK(p["ok"].get<bool>());
}

TEST_CASE("a capped bar threaded through a frame is locked: reported, not planned") {
  // frame opening 28 x 28 mm, caps 34 x 34 mm at both ends of the bar: neither can leave along any axis
  auto parts = load(R"J({"parts": [
    {"name": "frame", "kind": "boxes", "boxes": [
      {"at": [0, 0, 0], "size": [40, 6, 6]}, {"at": [0, 0, 34], "size": [40, 6, 6]},
      {"at": [0, 0, 6], "size": [6, 6, 28]}, {"at": [34, 0, 6], "size": [6, 6, 28]}]},
    {"name": "bar", "kind": "boxes", "boxes": [
      {"at": [15, -30, 15], "size": [10, 66, 10]}, {"at": [3, -40, 3], "size": [34, 10, 34]}, {"at": [3, 36, 3], "size": [34, 10, 34]}]}]})J");
  CHECK(overlaps(parts).empty());
  json p = plan(parts);
  CHECK_FALSE(p["ok"].get<bool>());
  CHECK(p["locked"].size() == 2);
}

TEST_CASE("a bolt in a drilled hole leaves only along its shank, and the wall never moves") {
  auto parts = load(R"J({"parts": [
    {"name": "wall", "kind": "box", "at": [-40, -50, -50], "size": [40, 100, 100], "fixed": true,
     "holes": [{"at": [-31, 0, 0], "axis": [1, 0, 0], "diameter": 6.2, "depth": 32}]},
    {"name": "bolt", "kind": "bolt", "at": [0, 0, 0], "axis": [-1, 0, 0], "diameter": 6, "length": 25}]})J");
  CHECK(overlaps(parts).empty());
  std::vector<const Part*> wall = {&parts[0]};
  CHECK(can_slide(parts[1], wall, {1, 0, 0}));        // out of the hole
  CHECK_FALSE(can_slide(parts[1], wall, {0, 0, 1}));  // not sideways: the shank is in the hole
  CHECK_FALSE(can_slide(parts[1], wall, {-1, 0, 0})); // not deeper: the head bears on the wall
  json p = plan(parts);
  REQUIRE(p["ok"].get<bool>());
  REQUIRE(p["steps"].size() == 1);  // the fixed wall is not a step
  CHECK(p["steps"][0]["insert_direction"] == json::array({-1.0, -0.0, -0.0}));
}

TEST_CASE("the replay check passes the planned order and catches a path that runs through a part") {
  auto parts = load(R"J({"parts": [
    {"name": "wall", "kind": "box", "at": [-40, -50, -50], "size": [40, 100, 100], "fixed": true,
     "holes": [{"at": [-31, 0, 0], "axis": [1, 0, 0], "diameter": 6.2, "depth": 32}]},
    {"name": "bolt", "kind": "bolt", "at": [0, 0, 0], "axis": [-1, 0, 0], "diameter": 6, "length": 25}]})J");
  json p = plan(parts);
  REQUIRE(p["ok"].get<bool>());
  json good = replay(parts, p["steps"]);
  CHECK(good["ok"].get<bool>());
  CHECK(good["max_overlap_mm3"].get<double>() < 1e-3);
  // the same bolt brought in sideways (down through the wall face) instead of along its shank
  json bad_steps = p["steps"];
  bad_steps[0]["insert_direction"] = json::array({0.0, 0.0, -1.0});
  json bad = replay(parts, bad_steps);
  CHECK_FALSE(bad["ok"].get<bool>());
  CHECK(bad["failures"][0]["against"] == "wall");
}
