#include "structura/expr.hpp"

#include <cctype>
#include <cmath>
#include <numbers>
#include <vector>

namespace structura {
namespace {

enum class Tok { Num, Name, Op, LPar, RPar, Comma, End };

struct Token {
  Tok kind;
  std::string text;
  double num = 0;
};

std::vector<Token> lex(const std::string& s) {
  std::vector<Token> out;
  size_t i = 0;
  while (i < s.size()) {
    char c = s[i];
    if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
    if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
      size_t j = i;
      while (j < s.size() && (std::isdigit(static_cast<unsigned char>(s[j])) || s[j] == '.')) ++j;
      if (j < s.size() && (s[j] == 'e' || s[j] == 'E')) {
        size_t k = j + 1;
        if (k < s.size() && (s[k] == '+' || s[k] == '-')) ++k;
        if (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) {
          j = k;
          while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
        }
      }
      std::string t = s.substr(i, j - i);
      try {
        size_t used = 0;
        double v = std::stod(t, &used);
        if (used != t.size()) throw ExprError("bad number");
        out.push_back({Tok::Num, t, v});
      } catch (...) {
        throw ExprError("cannot parse number '" + t + "' in expression '" + s + "'");
      }
      i = j;
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      size_t j = i;
      while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) ++j;
      out.push_back({Tok::Name, s.substr(i, j - i)});
      i = j;
      continue;
    }
    if (c == '(') { out.push_back({Tok::LPar, "("}); ++i; continue; }
    if (c == ')') { out.push_back({Tok::RPar, ")"}); ++i; continue; }
    if (c == ',') { out.push_back({Tok::Comma, ","}); ++i; continue; }
    if (c == '*' && i + 1 < s.size() && s[i + 1] == '*') { out.push_back({Tok::Op, "**"}); i += 2; continue; }
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') { out.push_back({Tok::Op, "//"}); i += 2; continue; }
    if (std::string("+-*/%").find(c) != std::string::npos) { out.push_back({Tok::Op, std::string(1, c)}); ++i; continue; }
    throw ExprError("expression '" + s + "' uses a forbidden character '" + std::string(1, c) + "'");
  }
  out.push_back({Tok::End, ""});
  return out;
}

const std::set<std::string> kFunctions = {"min", "max", "abs", "round", "int", "sqrt", "sin", "cos", "tan"};

// Recursive descent: expr := term (('+'|'-') term)*; term := unary (('*'|'/'|'//'|'%') unary)*;
// unary := ('+'|'-') unary | power; power := atom ('**' unary)?; atom := num | name | call | '(' expr ')'.
struct Parser {
  const std::vector<Token>& t;
  const std::string& src;
  const Values* params;           // null when only collecting names
  std::set<std::string>* names;   // null when evaluating
  size_t p = 0;

  const Token& peek() const { return t[p]; }
  const Token& next() { return t[p++]; }
  [[noreturn]] void fail(const std::string& m) const { throw ExprError("expression '" + src + "': " + m); }

  double expr() {
    double v = term();
    while (peek().kind == Tok::Op && (peek().text == "+" || peek().text == "-")) {
      std::string op = next().text;
      double r = term();
      v = op == "+" ? v + r : v - r;
    }
    return v;
  }
  double term() {
    double v = unary();
    while (peek().kind == Tok::Op && (peek().text == "*" || peek().text == "/" || peek().text == "//" || peek().text == "%")) {
      std::string op = next().text;
      double r = unary();
      if (op == "*") v *= r;
      else {
        if (params && r == 0) fail("divides by zero");
        if (op == "/") v = names ? 0 : v / r;
        else if (op == "//") v = names ? 0 : std::floor(v / r);
        else v = names ? 0 : v - std::floor(v / r) * r;  // Python modulo semantics
      }
    }
    return v;
  }
  double unary() {
    if (peek().kind == Tok::Op && (peek().text == "+" || peek().text == "-")) {
      std::string op = next().text;
      double v = unary();
      return op == "-" ? -v : v;
    }
    return power();
  }
  double power() {
    double b = atom();
    if (peek().kind == Tok::Op && peek().text == "**") {
      next();
      double e = unary();
      return names ? 0 : std::pow(b, e);
    }
    return b;
  }
  double atom() {
    const Token& k = next();
    if (k.kind == Tok::Num) return k.num;
    if (k.kind == Tok::LPar) {
      double v = expr();
      if (next().kind != Tok::RPar) fail("missing ')'");
      return v;
    }
    if (k.kind == Tok::Name) {
      if (peek().kind == Tok::LPar) {
        if (!kFunctions.count(k.text)) fail("calls an unknown function '" + k.text + "'");
        next();
        std::vector<double> args;
        if (peek().kind != Tok::RPar) {
          args.push_back(expr());
          while (peek().kind == Tok::Comma) { next(); args.push_back(expr()); }
        }
        if (next().kind != Tok::RPar) fail("missing ')' after arguments of " + k.text);
        return call(k.text, args);
      }
      if (kFunctions.count(k.text)) fail("function '" + k.text + "' used without arguments");
      if (names) { names->insert(k.text); return 1.0; }
      auto it = params->find(k.text);
      if (it == params->end()) fail("refers to unknown parameter '" + k.text + "'");
      return it->second;
    }
    fail("unexpected '" + k.text + "'");
  }
  double call(const std::string& f, const std::vector<double>& a) {
    auto need = [&](size_t n) { if (a.size() != n) fail(f + " takes " + std::to_string(n) + " argument(s)"); };
    if (names) return 1.0;
    if (f == "min" || f == "max") {
      if (a.empty()) fail(f + " needs arguments");
      double v = a[0];
      for (double x : a) v = f == "min" ? std::min(v, x) : std::max(v, x);
      return v;
    }
    need(1);
    double x = a[0];
    if (f == "abs") return std::fabs(x);
    if (f == "round") {  // Python round half to even
      double r = std::nearbyint(x);
      return r;
    }
    if (f == "int") return std::trunc(x);
    if (f == "sqrt") { if (x < 0) fail("sqrt of a negative number"); return std::sqrt(x); }
    double rad = x * std::numbers::pi / 180.0;
    if (f == "sin") return std::sin(rad);
    if (f == "cos") return std::cos(rad);
    return std::tan(rad);
  }
};

}  // namespace

double evaluate(const std::string& expr, const Values& params) {
  auto toks = lex(expr);
  Parser ps{toks, expr, &params, nullptr};
  double v = ps.expr();
  if (ps.peek().kind != Tok::End) throw ExprError("expression '" + expr + "': unexpected '" + ps.peek().text + "'");
  if (!std::isfinite(v)) throw ExprError("expression '" + expr + "' is not a finite number");
  return v;
}

std::set<std::string> names_in(const std::string& expr) {
  std::set<std::string> out;
  auto toks = lex(expr);
  Parser ps{toks, expr, nullptr, &out};
  ps.expr();
  if (ps.peek().kind != Tok::End) throw ExprError("expression '" + expr + "': unexpected '" + ps.peek().text + "'");
  return out;
}

}  // namespace structura
