// structura: the Attome AE engine command line. Every command prints one JSON object to stdout.
// On failure it prints {"error": <kind>, "message": ...} and exits with code 2.
//
//   structura describe                                   grammar: feature types and their parameters
//   structura families                                   available part families
//   structura build  (--family NAME | --graph FILE) [--params FILE] [--set k=v ...] [--out DIR]
//   structura gate   (--family NAME | --graph FILE) [--params FILE] [--set k=v ...] [--limits FILE]
//   structura mesh   (--family NAME | --graph FILE) [--params FILE] [--set k=v ...] --out DIR [--size MM] [--order 1|2]
//   structura solve  --job FILE [--out DIR]     build, gate, mesh and solve every load case of a job
//   structura validate [--case ID] [--out report.md] [--json file] [--machine TEXT]   standing validation suite
//   structura sweep  (--family NAME | --graph FILE) [--n 200] [--seed 1] [--out FILE]   random designs through the gate
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/export.hpp"
#include "structura/fea.hpp"
#include "structura/gate.hpp"
#include "structura/mesh.hpp"
#include "structura/occ.hpp"
#include "structura/validation.hpp"

namespace fs = std::filesystem;
using namespace structura;
using json = nlohmann::json;

namespace {

struct Args {
  std::string cmd;
  std::map<std::string, std::string> opt;
  std::vector<std::string> sets;
};

Args parse(int argc, char** argv) {
  Args a;
  if (argc < 2) throw std::runtime_error("usage: structura <describe|families|build|gate|mesh> [options]");
  a.cmd = argv[1];
  for (int i = 2; i < argc; ++i) {
    std::string k = argv[i];
    if (k.rfind("--", 0) != 0) throw std::runtime_error("unexpected argument " + k);
    if (i + 1 >= argc) throw std::runtime_error("missing value for " + k);
    std::string v = argv[++i];
    if (k == "--set") a.sets.push_back(v);
    else a.opt[k.substr(2)] = v;
  }
  return a;
}

json read_json(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  return json::parse(in);
}

DesignGraph graph_from(const Args& a) {
  if (a.opt.count("family")) return load_family(a.opt.at("family"));
  if (a.opt.count("graph")) return DesignGraph::load(a.opt.at("graph"));
  throw std::runtime_error("give --family NAME or --graph FILE");
}

Values params_from(const Args& a) {
  Values v;
  if (a.opt.count("params")) {
    json p = read_json(a.opt.at("params"));
    if (p.contains("params")) p = p["params"];
    for (auto& [k, x] : p.items()) v[k] = x.get<double>();
  }
  for (auto& s : a.sets) {
    auto eq = s.find('=');
    if (eq == std::string::npos) throw std::runtime_error("--set needs name=value, got " + s);
    v[s.substr(0, eq)] = std::stod(s.substr(eq + 1));
  }
  return v;
}

json face_info(const std::map<std::string, std::vector<TopoDS_Face>>& named) {
  json out = json::object();
  for (auto& [name, faces] : named) {
    json arr = json::array();
    for (auto& f : faces) {
      gp_Pnt c = centre(f);
      arr.push_back({{"area_mm2", area(f)}, {"centre", {c.X(), c.Y(), c.Z()}}, {"bbox", bbox(f)}});
    }
    out[name] = arr;
  }
  return out;
}

json describe() {
  json out = json::object();
  for (auto& [name, ft] : feature_types()) {
    json ps = json::array();
    for (auto& p : ft.params) {
      json x = {{"name", p.name}, {"kind", p.kind}, {"doc", p.doc}};
      if (p.def) x["default"] = json::parse(p.def->dump());
      ps.push_back(x);
    }
    out[name] = {{"doc", ft.doc}, {"params", ps}, {"modifies", ft.modifies}, {"pattern", ft.pattern}};
  }
  return {{"features", out},
          {"kinds", {{"length", "mm, 0.05 to 5000"}, {"pos", "mm, -5000 to 5000"}, {"count", "whole number, 1 to 500"},
                     {"angle", "deg, 0 to 360"}, {"axis", "x y z -x -y -z"}, {"op", "add or cut"}}},
          {"expressions", "numbers or strings over declared parameters: + - * / // % ** min max abs round int sqrt sin cos tan"}};
}

int run(const Args& a) {
  if (a.cmd == "describe") {
    std::cout << describe().dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "validate") {  // run the standing validation suite; optionally write the markdown report
    std::vector<ValidationRow> rows;
    std::string work = a.opt.count("work") ? a.opt.at("work") : (fs::temp_directory_path() / "structura_validate").string();
    int threads = a.opt.count("threads") ? std::stoi(a.opt.at("threads")) : 4;
    for (auto& id : validation_cases()) {
      if (a.opt.count("case") && a.opt.at("case") != id) continue;
      rows.push_back(run_validation_case(id, work, threads));
    }
    if (a.opt.count("out"))
      std::ofstream(a.opt.at("out")) << to_markdown(rows, a.opt.count("machine") ? a.opt.at("machine") : std::string(""));
    size_t passed = 0;
    for (auto& r : rows) passed += r.pass;
    json out = {{"passed", passed}, {"cases", rows.size()}, {"date", utc_now()}, {"rows", to_json(rows)}};
    if (a.opt.count("json")) std::ofstream(a.opt.at("json")) << out.dump(2);
    std::cout << out.dump(2) << "\n";
    return passed == rows.size() ? 0 : 1;
  }
  if (a.cmd == "families") {
    json out = json::array();
    for (auto& e : fs::directory_iterator(families_dir()))
      if (e.path().extension() == ".json") {
        DesignGraph g = DesignGraph::load(e.path().string());
        json ps = json::object();
        for (auto& [k, p] : g.parameters)
          ps[k] = {{"unit", p.unit}, {"low", p.low}, {"high", p.high}, {"default", p.def}, {"integer", p.integer}, {"doc", p.doc}};
        json faces = json::array();
        for (auto& [k, _] : g.faces) faces.push_back(k);
        out.push_back({{"name", g.name}, {"version", g.version}, {"doc", g.doc}, {"parameters", ps}, {"faces", faces}});
      }
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "solve") {  // one evaluation: build -> gate -> mesh -> solve each load case
    json job = read_json(a.opt.at("job"));
    std::string out = a.opt.count("out") ? a.opt.at("out") : job.value("out", std::string("runs/job"));
    DesignGraph g = job.contains("graph") ? DesignGraph::from_json(ojson::parse(job["graph"].dump())) : load_family(job.at("family"));
    Values v;
    const json jp = job.value("params", json::object());  // keep alive: items() of a temporary dangles
    for (auto& [k, x] : jp.items()) v[k] = x.get<double>();
    Design d(g, v);
    CompiledPart part = d.build();
    d.save((fs::path(out) / "design").string());
    export_bundle(part.shape, (fs::path(out) / "design").string());
    GateLimits lim = job.contains("gate") ? GateLimits::from_json(job["gate"]) : GateLimits{};
    if (!lim.design_space) lim.design_space = part.design_space();
    GateResult gr = gate(part.shape, lim);
    json res = {{"hash", d.hash()}, {"params", d.params}, {"gate", gr.to_json()}, {"folder", fs::absolute(out).string()}};
    if (!gr.ok) {
      res["status"] = "gate_fail";
      std::ofstream(fs::path(out) / "evaluation.json") << res.dump(2);
      std::cout << res.dump(2) << "\n";
      return 0;
    }
    MeshOptions mo;
    json jm = job.value("mesh", json::object());
    mo.size = jm.value("size", 4.0);
    mo.order = jm.value("order", 2);
    mo.threads = jm.value("threads", 1);
    Mesh m = mesh_shape(part.shape, part.named_faces(), (fs::path(out) / "mesh").string(), mo);
    res["mesh"] = m.report;
    Material mat = load_material(job.value("material", std::string("PETG")));
    SolveOptions so;
    so.threads = job.value("solver_threads", 1);
    json cases = json::object();
    json lcs = job.contains("load_cases") ? job["load_cases"] : json::array({job.at("load_case")});
    for (auto& jl : lcs) {
      LoadCase lc = LoadCase::from_json(jl);
      Result r = solve_static(m, mat, lc, (fs::path(out) / ("solve_" + lc.name)).string(), so);
      r.save((fs::path(out) / ("result_" + lc.name)).string());
      write_vtu(m, r, (fs::path(out) / ("result_" + lc.name) / "result.vtu").string());
      cases[lc.name] = r.summary();
    }
    res["material"] = {{"id", mat.id}, {"E", mat.E}, {"nu", mat.nu}, {"density", mat.density}, {"yield", mat.yield}, {"source", mat.source}, {"verify", mat.verify}};
    res["results"] = cases;
    res["status"] = "ok";
    std::ofstream(fs::path(out) / "evaluation.json") << res.dump(2);
    std::cout << res.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "sweep") {  // random in-bounds designs through build and gate
    DesignGraph g = graph_from(a);
    int n = a.opt.count("n") ? std::stoi(a.opt.at("n")) : 200;
    unsigned seed = a.opt.count("seed") ? static_cast<unsigned>(std::stoul(a.opt.at("seed"))) : 1u;
    std::mt19937 rng(seed);
    json designs = json::array(), reasons = json::object();
    int passed = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
      Values v;
      for (auto& [k, p] : g.parameters) {
        if (p.integer) v[k] = std::uniform_int_distribution<int>(int(p.low), int(p.high))(rng);
        else v[k] = std::uniform_real_distribution<double>(p.low, p.high)(rng);
      }
      json row = {{"params", v}};
      try {
        CompiledPart part = compile_graph(g, v);
        GateLimits lim;
        lim.design_space = part.design_space();
        GateResult r = gate(part.shape, lim);
        passed += r.ok;
        row["ok"] = r.ok;
        row["reasons"] = r.reasons;
        row["min_wall_mm"] = r.min_wall_mm ? json(*r.min_wall_mm) : json();
        for (auto& x : r.reasons) {
          std::string key = x.substr(0, x.find(" of ") == std::string::npos ? 60 : x.find(" of "));
          reasons[key] = reasons.value(key, 0) + 1;
        }
      } catch (const std::exception& e) {
        row["ok"] = false;
        row["reasons"] = {e.what()};
        reasons["compile"] = reasons.value("compile", 0) + 1;
      }
      designs.push_back(row);
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    json out = {{"family", g.name}, {"n", n}, {"seed", seed}, {"passed", passed}, {"pass_rate", double(passed) / n},
                {"seconds", std::round(secs * 10) / 10}, {"reasons", reasons}, {"designs", designs}};
    if (a.opt.count("out")) std::ofstream(a.opt.at("out")) << out.dump(1);
    out.erase("designs");
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  DesignGraph g = graph_from(a);
  Design d(g, params_from(a));
  CompiledPart part = d.build();
  if (a.cmd == "build") {
    auto named = part.named_faces();
    json out = {{"hash", d.hash()}, {"template", g.name}, {"params", d.params}, {"solids", part.solid_count()},
                {"volume_mm3", volume(part.shape)}, {"bbox", bbox(part.shape)}, {"valid", is_valid(part.shape)},
                {"named_faces", face_info(named)}};
    if (a.opt.count("out")) {
      std::string dir = a.opt.at("out");
      d.save(dir);
      write_step(part.shape, (fs::path(dir) / "part.step").string());
      std::ofstream(fs::path(dir) / "build.json") << out.dump(2);
      out["folder"] = fs::absolute(dir).string();
    }
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "gate") {
    GateLimits lim = a.opt.count("limits") ? GateLimits::from_json(read_json(a.opt.at("limits"))) : GateLimits{};
    if (!lim.design_space) lim.design_space = part.design_space();
    json out = gate(part.shape, lim).to_json();
    out["hash"] = d.hash();
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "mesh") {
    if (!a.opt.count("out")) throw std::runtime_error("mesh needs --out DIR");
    MeshOptions o;
    if (a.opt.count("size")) o.size = std::stod(a.opt.at("size"));
    if (a.opt.count("order")) o.order = std::stoi(a.opt.at("order"));
    if (a.opt.count("threads")) o.threads = std::stoi(a.opt.at("threads"));
    Mesh m = mesh_shape(part.shape, part.named_faces(), a.opt.at("out"), o);
    json out = m.report;
    out["hash"] = d.hash();
    std::ofstream(fs::path(a.opt.at("out")) / "mesh_report.json") << out.dump(2);
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  throw std::runtime_error("unknown command " + a.cmd);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(parse(argc, argv));
  } catch (const GraphError& e) {
    std::cout << json{{"error", "graph"}, {"message", e.what()}}.dump(2) << "\n";
  } catch (const CompileError& e) {
    std::cout << json{{"error", "compile"}, {"feature", e.feature_id}, {"message", e.what()}}.dump(2) << "\n";
  } catch (const MeshError& e) {
    std::cout << json{{"error", "mesh"}, {"message", e.what()}, {"attempts", e.attempts}}.dump(2) << "\n";
  } catch (...) {
    std::cout << json{{"error", "engine"}, {"message", occ_message()}}.dump(2) << "\n";
  }
  return 2;
}
