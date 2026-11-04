// structura: the Attome AE engine command line. Every command prints one JSON object to stdout.
// On failure it prints {"error": <kind>, "message": ...} and exits with code 2.
//
//   structura describe                                   grammar: feature types and their parameters
//   structura families                                   available part families
//   structura export --family F --set k=v --out DIR      STEP, STL, 3MF, GLB, drawing (DXF, PDF) and design source
//   structura adaptive --spec S --set k=v [--out DIR]    refine where stress varies until the peak converges
//   structura build  (--family NAME | --graph FILE) [--params FILE] [--set k=v ...] [--out DIR]
//   structura gate   (--family NAME | --graph FILE) [--params FILE] [--set k=v ...] [--limits FILE]
//   structura mesh   (--family NAME | --graph FILE) [--params FILE] [--set k=v ...] --out DIR [--size MM] [--order 1|2]
//   structura solve  --job FILE [--out DIR]     build, gate, mesh and solve every load case of a job
//   structura validate [--case ID] [--out report.md] [--json file] [--machine TEXT]   standing validation suite
//   structura meshsweep (--family NAME) [--n 500] [--seed 2] [--size 4] [--out FILE] [--model FILE]   gate, repair, mesh; fit mesh-ability
//   structura spec        --spec FILE                         validated spec summary
//   structura evaluate    --spec FILE --set k=v ... [--out DIR] [--bundle 1] [--mesh-size MM]
//   structura feasibility --spec FILE [--mesh-size MM]       can any design in the bounds meet the requirements?
//   structura trust       --spec FILE --set k=v ... [--out DIR] [--fine-size MM]   trust record with error bars and grade
//   structura reproduce   --dir DIR                           re-run a trust bundle and compare
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

#include "structura/adaptive.hpp"
#include "structura/compile.hpp"
#include "structura/design.hpp"
#include "structura/evaluate.hpp"
#include "structura/export.hpp"
#include "structura/fea.hpp"
#include "structura/gate.hpp"
#include "structura/inspect.hpp"
#include "structura/manufacture.hpp"
#include "structura/mesh.hpp"
#include "structura/meshability.hpp"
#include "structura/occ.hpp"
#include "structura/repair.hpp"
#include "structura/trust.hpp"
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
  if (a.cmd == "reproduce") {
    json r = reproduce(a.opt.at("dir"));
    std::cout << r.dump(2) << "\n";
    return r["match"].get<bool>() ? 0 : 1;
  }
  if (a.cmd == "trust") {
    DesignSpec spec = DesignSpec::load(a.opt.at("spec"));
    TrustOptions to;
    to.spec_path = fs::absolute(a.opt.at("spec")).string();
    if (a.opt.count("fine-size")) to.fine_size = std::stod(a.opt.at("fine-size"));
    if (a.opt.count("out")) to.workdir = a.opt.at("out");
    if (a.opt.count("threads")) to.threads = std::stoi(a.opt.at("threads"));
    std::cout << build_trust(spec, params_from(a), to).dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "dynamic") {  // real-physics animation data: sudden load, response over time
    DesignSpec spec = DesignSpec::load(a.opt.at("spec"));
    DynamicOptions o;
    if (a.opt.count("mesh-size")) o.mesh_size = std::stod(a.opt.at("mesh-size"));
    if (a.opt.count("threads")) o.threads = std::stoi(a.opt.at("threads"));
    if (a.opt.count("damping")) o.damping_ratio = std::stod(a.opt.at("damping"));
    if (a.opt.count("periods")) o.periods = std::stod(a.opt.at("periods"));
    if (a.opt.count("steps-per-period")) o.steps_per_period = std::stoi(a.opt.at("steps-per-period"));
    std::cout << dynamic_bundle(spec, params_from(a), a.opt.at("out"), o).dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "evaluate" || a.cmd == "feasibility" || a.cmd == "spec") {  // DesignSpec commands
    DesignSpec spec = DesignSpec::load(a.opt.at("spec"));
    EvalOptions o;
    if (a.opt.count("mesh-size")) o.mesh_size = std::stod(a.opt.at("mesh-size"));
    if (a.opt.count("threads")) o.threads = std::stoi(a.opt.at("threads"));
    if (a.opt.count("out")) o.workdir = a.opt.at("out");
    o.save_bundle = a.opt.count("bundle") && a.opt.at("bundle") == "1";
    o.keep_files = a.opt.count("keep") && a.opt.at("keep") == "1";
    o.screen = a.opt.count("screen") && a.opt.at("screen") == "1";
    json out;
    if (a.cmd == "spec") {
      json fp = json::object();
      for (auto& [k, b] : spec.free_params) fp[k] = b;
      json req = json::array();
      for (auto& r : spec.requirements)
        req.push_back({{"id", r.id}, {"quantity", r.quantity}, {"op", r.op}, {"value", r.value}, {"unit", quantities().at(r.quantity).unit}, {"source", r.source}});
      json obj = json::array();
      for (auto& ob : spec.objectives) obj.push_back({{"quantity", ob.quantity}, {"sense", ob.sense}});
      Material eff = spec.effective_material();
      out = {{"id", spec.id}, {"version", spec.version}, {"hash", spec.hash()}, {"family", spec.family}, {"free_params", fp},
             {"fixed_params", spec.fixed_params}, {"requirements", req}, {"objectives", obj},
             {"material", {{"id", eff.id}, {"name", eff.name}, {"yield_effective", eff.yield}, {"E", eff.E}}},
             {"load_cases", spec.raw["load_cases"]}, {"mesh_size", spec.mesh_size}};
    } else if (a.cmd == "feasibility") {
      out = check_feasibility(spec, o);
    } else {
      out = evaluate_design(spec, params_from(a), o);
    }
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "adaptive") {  // C1, C4: refine where the stress varies until the peak converges, for one design and load case
    DesignSpec spec = DesignSpec::load(a.opt.at("spec"));
    Values v = spec.fixed_params;
    for (auto& [k, x] : params_from(a)) v[k] = x;
    Design d(load_family(spec.family), v);
    CompiledPart part = d.build();
    if (spec.load_cases.empty()) throw std::runtime_error("adaptive refinement needs a structural load case");
    LoadCase lc = spec.load_cases.front().lc;
    const double f = spec.load_cases.front().factor_range[1];
    for (auto& l : lc.loads) {
      for (auto& c : l.force) c *= f;
      l.pressure *= f;
    }
    AdaptiveOptions o;
    if (a.opt.count("start")) o.start_size = std::stod(a.opt.at("start"));
    if (a.opt.count("min")) o.min_size = std::stod(a.opt.at("min"));
    if (a.opt.count("tolerance")) o.tolerance = std::stod(a.opt.at("tolerance"));
    SolveOptions so;
    so.threads = a.opt.count("threads") ? std::stoi(a.opt.at("threads")) : 4;
    so.exclude_near_bc_mm = spec.stress_exclude_near_bc_mm;
    std::string work = a.opt.count("out") ? a.opt.at("out") : (fs::temp_directory_path() / "structura_adaptive_cli").string();
    fs::remove_all(work);
    AdaptiveResult r = solve_adaptive(part.shape, part.named_faces(), spec.effective_material(), lc, work, o, so);
    json out = r.to_json();
    out["hash"] = d.hash();
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "meshsweep") {  // B4-T3: random designs through gate, repair and mesh; fit the mesh-ability model
    DesignGraph g = graph_from(a);
    int n = a.opt.count("n") ? std::stoi(a.opt.at("n")) : 500;
    unsigned seed = a.opt.count("seed") ? static_cast<unsigned>(std::stoul(a.opt.at("seed"))) : 2u;
    double size = a.opt.count("size") ? std::stod(a.opt.at("size")) : 4.0;
    std::string work = a.opt.count("work") ? a.opt.at("work") : (fs::temp_directory_path() / "structura_meshsweep").string();
    std::mt19937 rng(seed);
    std::vector<MeshFeatures> feats;
    std::vector<int> first_ok;
    int gated = 0, repaired = 0, meshed = 0, meshed_first = 0;
    json rows = json::array();
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
      Values v;
      for (auto& [k, p] : g.parameters)
        v[k] = p.integer ? double(std::uniform_int_distribution<int>(int(p.low), int(p.high))(rng))
                         : std::uniform_real_distribution<double>(p.low, p.high)(rng);
      json row = {{"params", v}};
      try {
        CompiledPart part = compile_graph(g, v);
        GateLimits lim;
        lim.design_space = part.design_space();
        lim.preserve = part.graph.preserve;
        RepairGateResult rg = repair_and_gate(part.shape, lim, RepairOptions{});
        row["gate_ok_before_repair"] = rg.before.ok;
        row["gate_ok"] = rg.ok;
        if (!rg.ok) { rows.push_back(row); continue; }
        ++gated;
        repaired += !rg.before.ok;
        MeshFeatures mf = mesh_features(rg.shape, rg.after.min_wall_mm.value_or(0));
        row["features"] = mf.to_json();
        MeshOptions mo;
        mo.size = size;
        mo.timeout_s = 60;
        bool ok = true;
        json att;
        try {
          Mesh m = mesh_shape(rg.shape, part.named_faces(), (fs::path(work) / std::to_string(i)).string(), mo);
          att = m.report["attempts"];
        } catch (const MeshError& e) {
          ok = false;
          att = e.attempts;
        }
        bool first = ok && att.size() == 1;
        meshed += ok;
        meshed_first += first;
        feats.push_back(mf);
        first_ok.push_back(first ? 1 : 0);
        row["meshed"] = ok;
        row["meshed_first_attempt"] = first;
        row["attempts"] = att.size();
        fs::remove_all(fs::path(work) / std::to_string(i));
      } catch (const std::exception& e) {
        row["error"] = e.what();
      }
      rows.push_back(row);
    }
    MeshabilityModel model = fit_meshability(feats, first_ok);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    json out = {{"family", g.name}, {"n", n}, {"seed", seed}, {"mesh_size", size}, {"gate_pass", gated},
                {"repaired_then_passed", repaired}, {"meshed", meshed}, {"meshed_first_attempt", meshed_first},
                {"gate_pass_to_mesh_success", gated ? double(meshed) / gated : 0.0},
                {"gate_pass_to_first_attempt_success", gated ? double(meshed_first) / gated : 0.0},
                {"model", model.to_json()}, {"seconds", std::round(secs)}};
    if (a.opt.count("model")) std::ofstream(a.opt.at("model")) << model.to_json().dump(2);
    if (a.opt.count("out")) {
      json full = out;
      full["designs"] = rows;
      std::ofstream(a.opt.at("out")) << full.dump(1);
    }
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "inspect") {  // A3: interfaces of an uploaded STEP part; --family-out writes a draft family that imports it
    if (!a.opt.count("step")) throw std::runtime_error("inspect needs --step FILE");
    std::string step = fs::absolute(a.opt.at("step")).string();
    TopoDS_Shape shape = read_step(step);
    json out = inspect_shape(shape);
    if (a.opt.count("family-out")) {
      std::string name = a.opt.count("name") ? a.opt.at("name") : fs::path(step).stem().string();
      json fam = draft_family(name, step, out, shape);
      fs::create_directories(fs::absolute(a.opt.at("family-out")).parent_path());
      std::ofstream(a.opt.at("family-out")) << fam.dump(2);
      out["family_file"] = fs::absolute(a.opt.at("family-out")).string();
      out["family"] = fam;
    }
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "families") {
    json out = json::array();
    for (auto& e : fs::recursive_directory_iterator(families_dir()))  // uploads/<name> included (A3)
      if (e.path().extension() == ".json") {
        DesignGraph g;
        try {
          g = DesignGraph::load(e.path().string());
        } catch (const std::exception&) {
          continue;  // an unloadable file (e.g. a rejected candidate) must not hide the valid families
        }
        std::string fam_name = fs::relative(e.path(), families_dir()).replace_extension().generic_string();
        json ps = json::object();
        for (auto& [k, p] : g.parameters)
          ps[k] = {{"unit", p.unit}, {"low", p.low}, {"high", p.high}, {"default", p.def}, {"integer", p.integer}, {"doc", p.doc}};
        json faces = json::array();
        for (auto& [k, _] : g.faces) faces.push_back(k);
        out.push_back({{"name", fam_name}, {"version", g.version}, {"doc", g.doc}, {"parameters", ps}, {"faces", faces}});
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
    if (lim.preserve.empty()) lim.preserve = part.graph.preserve;
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
        lim.preserve = part.graph.preserve;
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
  if (a.cmd == "export") {  // H2: every delivery format for one design
    if (!a.opt.count("out")) throw std::runtime_error("export needs --out FOLDER");
    std::array<double, 3> dir{0, 0, 1};
    if (a.opt.count("print-dir")) dir = json::parse(a.opt.at("print-dir")).get<std::array<double, 3>>();
    std::cout << export_all(d, part, a.opt.at("out"), dir).dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "build") {
    auto named = part.named_faces();
    json out = {{"hash", d.hash()}, {"template", g.name}, {"params", d.params}, {"solids", part.solid_count()},
                {"volume_mm3", volume(part.shape)}, {"bbox", bbox(part.shape)}, {"valid", is_valid(part.shape)},
                {"named_faces", face_info(named)}};
    if (a.opt.count("out")) {
      std::string dir = a.opt.at("out");
      d.save(dir);
      write_step(part.shape, (fs::path(dir) / "part.step").string());
      write_stl(part.shape, (fs::path(dir) / "part.stl").string());  // for renders and printing
      std::ofstream(fs::path(dir) / "build.json") << out.dump(2);
      out["folder"] = fs::absolute(dir).string();
    }
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "manufacture") {  // H1: FDM and CNC rules, cost and process advice for one design
    json P = load_processes(a.opt.count("processes") ? a.opt.at("processes") : "");
    std::string mat = a.opt.count("material") ? a.opt.at("material") : "PETG";
    std::array<double, 3> dir{0, 0, 1};
    if (a.opt.count("print-dir")) dir = json::parse(a.opt.at("print-dir")).get<std::array<double, 3>>();
    json out = manufacture_report(part.shape, mat, dir, P);
    out["hash"] = d.hash();
    out["params"] = d.params;
    std::cout << out.dump(2) << "\n";
    return 0;
  }
  if (a.cmd == "gate") {
    GateLimits lim = a.opt.count("limits") ? GateLimits::from_json(read_json(a.opt.at("limits"))) : GateLimits{};
    if (!lim.design_space) lim.design_space = part.design_space();
    if (lim.preserve.empty()) lim.preserve = part.graph.preserve;
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
  } catch (const SpecError& e) {
    std::cout << json{{"error", "spec"}, {"message", e.what()}}.dump(2) << "\n";
  } catch (const MeshError& e) {
    std::cout << json{{"error", "mesh"}, {"message", e.what()}, {"attempts", e.attempts}}.dump(2) << "\n";
  } catch (...) {
    std::cout << json{{"error", "engine"}, {"message", occ_message()}}.dump(2) << "\n";
  }
  return 2;
}
