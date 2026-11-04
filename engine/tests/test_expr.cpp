#include <doctest/doctest.h>

#include "structura/expr.hpp"

using namespace structura;

TEST_CASE("B3-T1: expressions evaluate with Python semantics") {
  Values p{{"t", 6}, {"w", 50}, {"n", 3}};
  CHECK(evaluate("t * 2 + 5", p) == doctest::Approx(17));
  CHECK(evaluate("w / (n + 1) - t / 2", p) == doctest::Approx(9.5));
  CHECK(evaluate("max(3, w / 4)", p) == doctest::Approx(12.5));
  CHECK(evaluate("-t ** 2", p) == doctest::Approx(-36));
  CHECK(evaluate("2 ** 3 ** 2", p) == doctest::Approx(512));
  CHECK(evaluate("7 // 2", p) == doctest::Approx(3));
  CHECK(evaluate("-7 // 2", p) == doctest::Approx(-4));
  CHECK(evaluate("-7 % 3", p) == doctest::Approx(2));
  CHECK(evaluate("round(2.5) + round(3.5)", p) == doctest::Approx(6));
  CHECK(evaluate("cos(60)", p) == doctest::Approx(0.5));
  CHECK(evaluate("1e-3 * 2", p) == doctest::Approx(0.002));
}

TEST_CASE("B3-T1: forbidden constructs are rejected") {
  Values p{{"t", 6}};
  CHECK_THROWS_AS(evaluate("__import__('os')", p), ExprError);
  CHECK_THROWS_AS(evaluate("t.real", p), ExprError);
  CHECK_THROWS_AS(evaluate("open(1)", p), ExprError);
  CHECK_THROWS_AS(evaluate("L * 2", p), ExprError);
  CHECK_THROWS_AS(evaluate("t / 0", p), ExprError);
  CHECK_THROWS_AS(evaluate("t +", p), ExprError);
}

TEST_CASE("B3-T1: names are collected without evaluating") {
  auto n = names_in("max(thickness, w / 2) + 3");
  CHECK(n == std::set<std::string>{"thickness", "w"});
}
