#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

#include "structura/mesh.hpp"

namespace structura {
namespace {

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

std::string strip(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

size_t nodes_per(const std::string& t) {
  static const std::map<std::string, size_t> m = {{"C3D10", 10}, {"C3D4", 4}, {"CPS6", 6}, {"STRI65", 6}, {"CPS3", 3},
                                                  {"T3D3", 3}, {"T3D2", 2}, {"C3D20", 20}, {"C3D8", 8}};
  auto it = m.find(t);
  return it == m.end() ? 0 : it->second;
}

}  // namespace

InpData parse_inp(const std::string& text) {
  InpData d;
  enum { NONE, NODE, ELEM, NSET } mode = NONE;
  InpBlock* blk = nullptr;
  std::vector<int>* set = nullptr;
  size_t need = 0;
  static const std::regex type_re("TYPE=(\\w+)"), elset_re("ELSET=([\\w\\-.]+)"), nset_re("NSET=([\\w\\-.]+)");
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    std::string s = strip(line);
    if (s.empty() || s.rfind("**", 0) == 0) continue;
    if (s[0] == '*') {
      std::string key = upper(s);
      key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
      std::smatch m;
      if (key.rfind("*NODE", 0) == 0 && key.rfind("*NODEPRINT", 0) != 0 && key.rfind("*NODEFILE", 0) != 0 &&
          key.rfind("*NODEOUTPUT", 0) != 0) {
        mode = NODE;
      } else if (key.rfind("*ELEMENT", 0) == 0) {
        d.blocks.push_back({});
        blk = &d.blocks.back();
        if (std::regex_search(key, m, type_re)) blk->type = m[1];
        if (std::regex_search(key, m, elset_re)) blk->elset = m[1];
        need = nodes_per(blk->type) + 1;
        mode = ELEM;
      } else if (key.rfind("*NSET", 0) == 0 && std::regex_search(key, m, nset_re)) {
        set = &d.nsets[m[1]];
        mode = NSET;
      } else {
        mode = NONE;
      }
      continue;
    }
    std::vector<std::string> vals;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      tok = strip(tok);
      if (!tok.empty()) vals.push_back(tok);
    }
    if (vals.empty()) continue;
    if (mode == NODE) {
      d.nodes[std::stoi(vals[0])] = {std::stod(vals[1]), std::stod(vals[2]), vals.size() > 3 ? std::stod(vals[3]) : 0.0};
    } else if (mode == ELEM) {
      std::vector<int> ints;
      for (auto& v : vals) ints.push_back(std::stoi(v));
      auto& rows = blk->rows;
      if (!rows.empty() && need > 1 && rows.back().size() < need)
        rows.back().insert(rows.back().end(), ints.begin(), ints.end());
      else
        rows.push_back(ints);
    } else if (mode == NSET) {
      for (auto& v : vals) set->push_back(std::stoi(v));
    }
  }
  return d;
}

}  // namespace structura
