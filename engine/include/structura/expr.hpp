// Safe arithmetic expressions over named design parameters.
// A value is a number or a string such as "thickness * 2 + 5" or "max(3, w / 4)". Allowed: numbers, declared
// parameter names, + - * / // % ** (power), unary +/-, parentheses and the functions
// min max abs round int sqrt sin cos tan (trigonometry in degrees). Anything else is rejected.
#pragma once
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace structura {

struct ExprError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

using Values = std::map<std::string, double>;

double evaluate(const std::string& expr, const Values& params);
std::set<std::string> names_in(const std::string& expr);

}  // namespace structura
