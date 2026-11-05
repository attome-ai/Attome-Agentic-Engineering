// attome_assembly plan --assembly ASSEMBLY.json --out DIR
// Loads the parts, refuses an assembly whose parts overlap, plans the assembly order and writes DIR/plan.json and one
// STL per part (in place) for the viewer. Prints the plan.
#include <filesystem>
#include <fstream>
#include <iostream>

#include "attome/assembly.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  try {
    std::string cmd = argc > 1 ? argv[1] : "", file, out;
    for (int i = 2; i + 1 < argc; i += 2) {
      std::string k = argv[i];
      if (k == "--assembly") file = argv[i + 1];
      if (k == "--out") out = argv[i + 1];
    }
    if (cmd != "plan" || file.empty() || out.empty()) {
      std::cerr << "usage: attome_assembly plan --assembly ASSEMBLY.json --out DIR\n";
      return 2;
    }
    std::ifstream in(file);
    if (!in) throw std::runtime_error("cannot open " + file);
    nlohmann::json j = nlohmann::json::parse(in);
    auto parts = attome::load_assembly(j, fs::path(file).parent_path().string());
    nlohmann::json result = {{"assembly", j.value("name", fs::path(file).stem().string())}, {"parts", parts.size()}};
    nlohmann::json ov = attome::overlaps(parts);
    result["overlaps"] = ov;
    if (!ov.empty()) {
      result["ok"] = false;
      result["message"] = "parts overlap in the assembled position: fix the placement before planning";
    } else {
      nlohmann::json p = attome::plan(parts);
      result.update(p);
      if (result.value("ok", false)) {  // never hand the viewer a plan the replay check rejects
        result["replay_check"] = attome::replay(parts, result["steps"]);
        if (!result["replay_check"].value("ok", false)) {
          result["ok"] = false;
          result["message"] = "the replay check found parts passing through each other: the plan is not shown";
        }
      }
    }
    nlohmann::json desc = nlohmann::json::array();
    for (auto& q : j.at("parts")) desc.push_back({{"name", q.at("name")}, {"kind", q.at("kind")}, {"fastener", q.value("fastener", q.at("kind") == "bolt" || q.at("kind") == "plug")},
                                                   {"fixed", q.value("fixed", false)}, {"diameter", q.value("diameter", 0.0)},
                                                   {"length", q.value("length", 0.0)},
                                                   {"color", q.value("color", "")}});
    result["part_list"] = desc;
    attome::write_parts(parts, out);
    std::ofstream(fs::path(out) / "plan.json") << result.dump(2);
    std::cout << result.dump(2) << "\n";
    return result.value("ok", false) ? 0 : 1;
  } catch (const std::exception& e) {
    std::cout << nlohmann::json({{"ok", false}, {"error", e.what()}}).dump(2) << "\n";
    return 1;
  }
}
