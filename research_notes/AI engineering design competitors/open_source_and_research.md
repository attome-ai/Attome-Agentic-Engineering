# Open-Source Tools and Academic Research for an LLM Agent that Designs CAD, Simulates It and Optimizes It (as of Nov 2026)

Research notes for the "Attome Agentic Engineering / StructuraAI" competitor matrix and technical plan (AIDTC 2027). Researched 2026-11-03.

**Verification legend.** Entries marked **[verified]** were checked this session against a fetched primary page or a search result that quoted one. Entries marked **[from memory, verify]** come from the researcher's background knowledge. They are flagged as such, include their arXiv IDs, and must be checked before anyone quotes a number publicly. Scores (0-100) are the researcher's suggestions, using the rubric from the brief.

---

## 1. LLM CAD-generation research: what exists and what success rates are reported

### Takeaway
Text-to-CAD is now a crowded research field. Code-output approaches (CadQuery or build123d) are clearly better than DeepCAD-style command sequences. Benchmarks still show that frontier LLMs produce invalid CadQuery code 11% of the time on simple shapes and 54-90% of the time on complex ones (sweeps, lofts, shells). None of these generation papers check physical performance. Only the 2026 "physics-in-the-loop" papers (Section 2) do. Executability alone is a misleading metric.

### Cited Findings

**Text2CAD-Bench (arXiv 2605.18430, May 2026; ICML-style template, venue unconfirmed) [verified]**
- 600 human-curated examples in four levels: L1 = 200 primitives, L2 = 200, L3 = 100 sweeps/lofts/shells, and L4 = 100 real-world parts (about 40% industrial/mechanical). The output is CadQuery code. Metrics are Chamfer Distance (CD), IoU, Invalidity Rate (IR), and a VLM judge for L4. — [Text2CAD-Bench](https://arxiv.org/html/2605.18430v1)
- L1 geometric prompt: GPT-5.2 scored CD 44.31, IR 11.1%, IoU 0.59, the best on L1-L2. Claude-4.5-Sonnet had the best CD on L3 (70.13) but an IR of about 70%. For most models, IR rises to roughly 70-90% at L3. — [Text2CAD-Bench](https://arxiv.org/html/2605.18430v1)
- On L1, the domain-specific models produce poor geometry despite low IR. Text2CAD: CD 219.57, IR 11%, IoU 0.08. Text-to-CadQuery: CD 227.46, IR 44%, IoU 0.07. CADFusion: CD 229.62, IR 60%, IoU 0.03. — [Text2CAD-Bench](https://arxiv.org/html/2605.18430v1)
- CadQuery code beats DeepCAD-style command sequences. On L4, executability, geometric similarity and feature quality are largely independent, and no model leads on all three. The authors warn about survivorship bias when IR is high. — [Text2CAD-Bench](https://arxiv.org/html/2605.18430v1)

**CADCodeVerify + CADPrompt (Alrashedy et al., ICLR 2025, arXiv 2410.05340) [verified]**
- CADPrompt is a benchmark of 200 natural-language prompts with expert CadQuery scripts. CADCodeVerify is a VLM loop that renders the object, asks itself verification questions and refines the code. With GPT-4 it reduced point-cloud distance by 7.30% and raised the success/compile rate by about 5% (5.0% or 5.5%, depending on the version). On the Hard split, Refine-1 adds about 9% to the compile rate. — [ICLR 2025 proceedings](https://proceedings.iclr.cc/paper_files/paper/2025/hash/81a934cd364e18ea6fdeaf57a93c17d4-Abstract-Conference.html); [arXiv](https://arxiv.org/html/2410.05340)

**CAD-Recode (Rukhovich et al., ICCV 2025) [verified venue; metrics not retrieved]**
- Translates a point cloud into CadQuery Python, using about 1M procedurally generated sketch-extrude scripts and a Qwen2-based LLM. The repo says it was "state-of-the-art in three CAD reconstruction benchmarks" (DeepCAD, Fusion360, CC3D) as of December 2024. The repo has 278 stars. — [GitHub filaPro/cad-recode](https://github.com/filaPro/cad-recode)
- A 2026 paper says CAD-Recode shares a Qwen2-VL-2B backbone with its successors cadrille and CADEvolve. — [Zero-to-CAD arXiv 2604.24479](https://arxiv.org/pdf/2604.24479)
- Note: it is reverse engineering (input is a point cloud, not natural-language intent), so it is only an indirect competitor.

**CAD-Llama (arXiv 2505.04481; CVPR 2025 per the brief, venue not confirmed in this session) [verified numbers]**
- On text-to-CAD, CAD-Llama-INS scored 84.72 accuracy, versus 69.91 for Text2CAD and 70.36 for CAD-Translator. Its CD was 10.53 versus 20.64 for Text2CAD. — [CAD-Llama arXiv](https://arxiv.org/html/2505.04481v2)

**CADmium (arXiv 2507.09792, 2025) [verified qualitatively]**
- Fine-tunes code LLMs (Qwen2.5-Coder up to 14B) for text-to-sequential-CAD and evaluates on human-written CADPrompt. Text2CAD has the lowest invalidity ratio. CADmium-14B is better on primitive F1 (line, arc, circle, extrusion) and on CD. — [CADmium arXiv](https://arxiv.org/pdf/2507.09792)

**Text2CAD (Khan et al., NeurIPS 2024, arXiv 2409.17106) [from memory, verify]**
- Generates DeepCAD-style sequences from template-generated multi-level prompts (about 170K models with about 660K annotations). Later benchmarks show low IR but poor geometric fidelity on human prompts (see Text2CAD-Bench above). — [Text2CAD-Bench, which describes it](https://arxiv.org/html/2605.18430v1)

**Self-Improving CAD Generation Agents with FEA Feedback (Son, Park et al., SNU, arXiv 2605.17448v2, May 2026) [verified]**
- Also listed here because it is the closest academic analogue to StructuraAI (see Section 2). It cites CAD-Recode, CADRille, Text-to-CadQuery, CAD-Llama, CADCodeVerify (Alrashedy), CADDesigner, Text2CAD, EvoCAD, SEEK-CAD, CAD-Assistant, CADSmith and CADmium as prior art. — [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)

**Other 2026 benchmarks found (not fetched)**
- BenchCAD, "A Comprehensive, Industry-Standard Benchmark for Programmatic CAD". — [arXiv 2605.10865](https://arxiv.org/pdf/2605.10865)
- CadBench, a multimodal benchmark for CAD program generation. It reports CAD-Recode IoU/CD, but the split is unclear. — [arXiv 2605.10873](https://arxiv.org/html/2605.10873v1)
- Zero-to-CAD, agentic synthesis of CAD programs at million scale. — [arXiv 2604.24479](https://arxiv.org/pdf/2604.24479)
- textcad-bench, a unified benchmark repo covering 7 models. — [GitHub](https://github.com/prashantkul366/textcad-bench)
- CADGenBench public leaderboard (June 2026): adding build123d-mcp raised one model's score from 0.360 to 0.457 and validity from 88% to 100%. This is self-reported by the build123d-mcp project. — [build123d-mcp](https://github.com/pzfreo/build123d-mcp)

**From memory, not re-verified this session (include only with a "verify" caveat)**
- **Query2CAD** (arXiv 2406.00144, 2024): self-refining LLM that generates FreeCAD macros, with a BLIP2 captioning/VQA feedback loop.
- **BlenderLLM** (arXiv 2412.14203, 2024): fine-tuned LLM that writes Blender Python, plus the CADBench benchmark. Output is meshes, not B-rep.
- **CAD-Assistant** (Mallis et al., arXiv 2412.13810; ICCV 2025): training-free VLLM tool-augmented agent on FreeCAD.
- **LLM4CAD** (Li, Ahmed et al., ASME IDETC 2023/JCISE 2024): multimodal LLMs generating 3D CAD for five mechanical part categories.
- **CadVLM** (Wu et al., ECCV 2024, arXiv 2409.17457): vision-language model for sketch-level parametric CAD (autocomplete and autoconstraint).
- The same search found an ACL 2025 long paper, "Automated CAD Modeling Sequence Generation from Text…". — [ACL Anthology](https://aclanthology.org/2025.acl-long.1054.pdf)

### Suggested scores (0-100): CAD-generation research systems
Columns: NL intent / geometry / structural FEA / thermal / CFD / auto-opt / multi-obj-Pareto / LLM autonomy / closed loop / verification-trust / 3D result viz / cost-access

| System | NL | Geom | FEA | Therm | CFD | Opt | Pareto | Agent | Closed | Verif | 3Dviz | Cost |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Text2CAD (NeurIPS'24) | 65 | 45 | 0 | 0 | 0 | 0 | 0 | 15 | 0 | 10 | 20 | 70 |
| CADCodeVerify/CADPrompt (ICLR'25) | 70 | 60 | 0 | 0 | 0 | 0 | 0 | 45 | 45 | 40 | 20 | 70 |
| CAD-Recode (ICCV'25) | 5 | 70 | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 20 | 20 | 75 |
| CAD-Llama (2025) | 65 | 55 | 0 | 0 | 0 | 0 | 0 | 15 | 0 | 15 | 15 | 60 |
| CADmium (2025) | 65 | 55 | 0 | 0 | 0 | 0 | 0 | 15 | 0 | 15 | 15 | 70 |
| Query2CAD (2024, verify) | 60 | 45 | 0 | 0 | 0 | 0 | 0 | 40 | 40 | 30 | 20 | 70 |
| BlenderLLM (2024, verify) | 60 | 40 (mesh) | 0 | 0 | 0 | 0 | 0 | 20 | 10 | 15 | 30 | 70 |
| CAD-Assistant (2025, verify) | 65 | 60 | 0 | 0 | 0 | 0 | 0 | 65 | 50 | 40 | 30 | 60 |
| LLM4CAD (2024, verify) | 60 | 40 | 0 | 0 | 0 | 0 | 0 | 20 | 10 | 15 | 15 | 60 |
| CadVLM (ECCV'24, verify) | 20 | 40 (2D sketch) | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 15 | 10 | 60 |
| Text2CAD-Bench (benchmark) | n/a, benchmark only; evaluates NL→CadQuery validity and geometry |

### Inferences
- CadQuery or build123d code output, a render-and-check visual loop and an execution-repair loop are now table stakes. StructuraAI should not claim novelty for NL→CAD itself.
- With L3-style features (sweeps, lofts, shells) failing about 70-90% of the time, the agent should prefer simple extrude, revolve, fillet and pattern parameterizations. The LLM should choose parameters for templates or a constrained grammar rather than writing free-form geometry. This also makes the optimizer's job easier.
- Report three metrics separately: execution validity, geometric/requirement satisfaction and physical validity. The benchmarks show they diverge.

### Gaps
- Original-paper metrics for CAD-Recode (IR/CD/IoU), Text2CAD, Query2CAD, BlenderLLM, CAD-Assistant, LLM4CAD and CadVLM were not retrieved this session.
- The CAD-Llama CVPR 2025 venue was not confirmed.

---

## 2. LLM agents driving simulation (FEA/CFD), topology optimization, and design-simulate-optimize loops

### Takeaway
The closest prior art to StructuraAI appeared in May 2026. Two arXiv papers close the NL → CadQuery → gmsh → FEA → refine loop, and one of them uses exactly the planned stack (CadQuery + gmsh + CalculiX). Both show that FEA feedback substantially improves outcomes. Both also show that first-attempt requirement satisfaction is low (about 20-33%) and that strict all-requirement passes are rare. On the CFD side, OpenFOAM agents reach about 88-96% "runs successfully" on FoamBench. A September 2026 study shows a generic coding harness (Claude Opus 4.6) matches or beats the specialized multi-agent systems. Numerically correct FEA results remain hard: about 33% on the FEABench Gold split.

### Cited Findings

**Physics-in-the-Loop (Berger et al., TU Dresden / MAN / DFKI, arXiv 2605.19717, May 2026) [verified]**
- LangGraph multi-agent setup (Planner, CAD Engineer, Geometry Reviewer, Structural Reviewer). The pipeline is CadQuery → gmsh (HXT, tet4) → torch-fem, a PyTorch FEA solver. Physics tools are deterministic and the agents cannot modify them. The goal is a safety factor of 2-5 at minimum volume. The benchmark covers 20 load cases scaled into 500 configurations, with up to 10 iterations. — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)
- Execution / meshing / FEA success rates: Opus 4.5 96.4/88.2/81.3%, Sonnet 4.5 97.5/87.6/84.5%, Gemini 3 Flash 97.0/84.4/85.1%, Gemini 3 Pro 97.5/76.3/86.8%. — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)
- With FEA feedback, 59.0% of designs hit the target safety-factor range, versus 22.2% without it (p = 0.0008). Opus reached 72.7% versus 14.3%. A Planner agent cut the mean iterations from 10.77 to 4.44. — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)
- Cost was about $0.023 (Gemini Flash) to $0.204 (Opus) per iteration, at about 28.7 ± 36.9 s per iteration. A PyTorch topology optimization baseline converged in 18.6 s but produces voxels rather than editable CAD. — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)
- Failures: design-space violations and disconnected parts, attributed to weak load-path spatial reasoning. Sonnet produced over-complex, hard-to-mesh geometry. All models over-engineered, with safety factors near 5. The code is promised under MIT/CC at camera-ready. — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)

**Self-Improving CAD Generation Agents with FEA Feedback (Son, Park et al., arXiv 2605.17448v2, May 2026) [verified]**
- Toolchain: CadQuery → STEP → gmsh → **CalculiX**, with ParaView rendering 21 views. Agents run in the Codex and Claude Code harnesses. The new benchmark Hephaestus-CCX has 50 engineering briefs with executable requirement checkers, released with CalculiX evaluation kits. — [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)
- Mean requirement pass before and after one FEA retry: GPT-5.5 xhigh single-part 25.9→39.4%, multi-part 15.0→36.1%; Opus-4.7 max 27.5→29.1% and 10.7→18.6%. Across all 400 first attempts in the main sweep, the strict pass count was 0. — [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)
- Test-time scaling to 10 attempts (GPT-5.5 high) raised the mean pass rate from 38.8% to 60.5%, with 9/50 strict passes, at about 68 minutes per item. — [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)
- Failures: over-detailed geometry broke meshing or created stress concentrations; slender members failed buckling and deflection; parts exceeded mass limits; checker-contract gaps; mass-property errors invisible in renders. — [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)

**Generic harness vs. specialized CAE agents (Shi & Zhang, arXiv 2609.03718, Sep 2026) [verified]**
- With matched information and repair budget, a "Direct Baseline" coding agent scored 96.4% on FoamBench (110) with Claude Opus 4.6, 92.5% with GPT-5.5, 89.3% with DeepSeek V4 and 77.2% with Qwen3.5-Plus. The specialized Foam-Agent scored 88.2%. On FEABench Gold (15 cases), the best result was 33.3% (5/15) for both the specialized system and the harness. MCP-SIM scored 91.7% specialized versus 100% for the harness. — [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)
- Repair budget is decisive. With no repair (R≤1), FoamBench success was 0.9%. It rose to 71.8% at R≤2, 90.0% at R≤5 and 96.4% at R≤10. — [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)
- Solver tutorials and docs helped most: FoamBench went from 80.9% with no tutorial to 96.4% when reading the tutorial was mandatory. A scripted reflection prompt added nothing. — [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)
- Only FoamBench and FEABench check numerical results; the other suites count convergence or an LLM-judge verdict as success. Execution-based and field-level scores can differ by about 20 points. — [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)
- MechAgents (Ni & Buehler, 2023) and OpenFOAMGPT (Feng et al.) are cited, but the paper reports no numbers for them. FoamBench comes from CFDLLMBench (Somasekharan et al., 2025). — [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)

**Foam-Agent (arXiv 2505.04997, 2025; v2 arXiv 2509.18178) [verified]**
- Multi-agent OpenFOAM automation (meshing, configuration, HPC submission, ParaView post-processing). It reached 88.2% on a 110-task benchmark with Claude 3.5 Sonnet, versus 55.5% for MetaOpenFOAM. With GPT-4o the figures were 59.1% versus 17.3%. — [Foam-Agent](https://arxiv.org/html/2505.04997); [Foam-Agent 2.0](https://arxiv.org/html/2509.18178v1)

**FEABench (Mudur et al., Google, 2025; arXiv 2504.06260 from memory) [numbers verified via secondary source]**
- The benchmark has LLM agents solve FEA problems through the COMSOL Multiphysics API. The best agent strategy produced executable API calls 88% of the time across heat transfer, structural mechanics and electromagnetics. Numerically correct answers on the Gold split are only about 33%. — [PDE-Agents arXiv 2606.07850 (citing FEABench)](https://arxiv.org/pdf/2606.07850); [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)

**MechAgents (Ni & Buehler, MIT; arXiv 2311.08166; Extreme Mechanics Letters 2024, from memory) [qualitative only]**
- Multi-agent FEniCS system ("2 agents, 7 roles") with text input, field-plot output and a human user proxy. No benchmark numbers were found. — [PDE-Agents comparison table, arXiv 2606.07850](https://arxiv.org/pdf/2606.07850)

**Other simulation agents found**
- PDE-Agents (arXiv 2606.07850, 2026): LLM-orchestrated multi-agent FE simulation with knowledge-graph-augmented reasoning. — [arXiv](https://arxiv.org/pdf/2606.07850)
- A constrained natural-language interface for multiphysics FEniCS (arXiv 2606.10928). — [arXiv](https://arxiv.org/pdf/2606.10928)
- LLM agent for Fire Dynamics Simulator (arXiv 2412.17146). — [arXiv](https://arxiv.org/pdf/2412.17146)
- MetaOpenFOAM (arXiv 2407.21320) and OpenFOAMGPT (arXiv 2501.06327) are from memory. Their FoamBench-comparable figures appear above via Foam-Agent.

**LLM + topology optimization**
- TO-Master (arXiv 2607.01812, 2026): conversational LLM agent driving JAX-FEM topology optimization. Examples include brackets and a 2D heat-conduction heat-sink case. — [arXiv](https://arxiv.org/html/2607.01812v1)
- TopOptAgents / Self-Refining TO (arXiv 2605.23273): Scientist, Validator and Planner agents formulate, check and code topology optimization problems. The authors say 3D TO remains open. — [arXiv](https://arxiv.org/html/2605.23273)
- LLM-DSTO (MDPI J. Manuf. Mater. Process. 10(4):117, 2026): LLMs propose symbolic layouts, reaching about 87.5% of SIMP stiffness on a cantilever and about 94.3% of SIMP thermal performance on a heat sink. — [MDPI](https://www.mdpi.com/2504-4494/10/4/117)
- "From text to design" (Proc. Design Society, about 2025): function-calling agents build design spaces for topology optimization on the FCRC bracket benchmark. Geometry-generation success was low. — [Cambridge Core](https://www.cambridge.org/core/journals/proceedings-of-the-design-society/article/from-text-to-design-a-framework-to-leverage-llm-agents-for-automated-cad-generation/5BD8D63CFCED28BDD7A01313162FFBE7)

### Suggested scores (0-100): simulation, optimization and closed-loop systems
| System | NL | Geom | FEA | Therm | CFD | Opt | Pareto | Agent | Closed | Verif | 3Dviz | Cost |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Physics-in-the-Loop (2026) | 70 | 65 | 70 | 0 | 0 | 55 (LLM-driven SF/volume) | 10 | 75 | 75 | 70 | 35 (paper figures) | 65 |
| Self-Improving CAD+FEA / Hephaestus-CCX (2026) | 75 | 65 | 70 (CalculiX) | 0 | 0 | 40 | 0 | 80 | 70 | 70 (typed checkers) | 35 | 55 (68 min/item at 10 attempts) |
| Foam-Agent (2025) | 70 | 25 (blockMesh/snappy) | 0 | 15 | 75 | 0 | 0 | 75 | 65 | 45 | 40 (ParaView) | 70 |
| MetaOpenFOAM (2024, verify) | 65 | 15 | 0 | 10 | 55 | 0 | 0 | 65 | 50 | 30 | 30 | 70 |
| OpenFOAMGPT (2025, verify) | 65 | 15 | 0 | 10 | 55 | 0 | 0 | 55 | 45 | 30 | 25 | 65 |
| FEABench (2025, benchmark) | 60 | 30 | 60 (COMSOL) | 55 | 0 | 0 | 0 | 60 | 50 | 55 (numeric check) | 20 | 20 (COMSOL licence) |
| MechAgents (2023/24) | 60 | 10 | 50 (FEniCS) | 10 | 0 | 0 | 0 | 60 | 45 | 30 | 30 | 75 |
| Generic harness baseline (2026) | 70 | 20 | 40 | 30 | 75 | 0 | 0 | 80 | 70 | 45 | 25 | 70 |
| TO-Master (2026) | 65 | 30 (density field) | 60 | 35 | 0 | 70 | 10 | 65 | 60 | 45 | 35 | 75 |
| TopOptAgents (2026) | 60 | 25 | 55 | 10 | 0 | 65 | 0 | 60 | 55 | 50 | 25 | 75 |
| LLM-DSTO (2026) | 40 | 30 | 55 | 50 | 0 | 55 | 0 | 40 | 40 | 40 | 25 | 75 |

### Inferences
- **Differentiation for StructuraAI.** Neither 2026 closed-loop paper offers (a) systematic numeric multi-objective optimization producing a Pareto front (pymoo/Optuna), (b) thermal analysis on the same CAD, or (c) an interactive browser UI with field results and optimization history. Both are research pipelines with static figures. Those three things are StructuraAI's credible gap.
- **Architecture lessons.**
  - Use deterministic, agent-immutable physics tools.
  - Have the LLM parameterize and an optimizer search, rather than letting the LLM iterate free-form; the papers show LLMs over-engineer, with safety factors near 5.
  - Use a Planner step.
  - Give a repair budget of at least 5.
  - Inject solver docs and templates into context.
  - Report execution, meshing, FEA and requirement-pass rates separately.
  - Treat meshing as a failure stage in its own right: 76-88% meshing success after 96-97% execution success.
- Hephaestus-CCX, if public, is a ready-made CalculiX-based evaluation set that StructuraAI could report against.

### Gaps
- Code-release status and URLs for Physics-in-the-Loop and Hephaestus-CCX (Appendix H was truncated) were not confirmed.
- No LLM-agent heat-sink paper with CFD (conjugate heat transfer) plus optimization was found. Only LLM-DSTO (conduction-based topology optimization) and the TO-Master heat-conduction example turned up.
- No numbers for MechAgents, OpenFOAMGPT or MetaOpenFOAM were taken from the original papers.

---

## 3. Open-source projects: CAD MCP servers, FreeCAD FEM, topology optimization libraries, surrogates

### Takeaway
For build123d there is a usable, headless, Apache-2.0 MCP server (pzfreo/build123d-mcp). FreeCAD MCP (neka-nat) is the most popular at about 2,900 stars and can reach CalculiX through FreeCAD FEM, but it needs a running FreeCAD GUI. No maintained CadQuery-specific MCP server was found. Autodesk Fusion (May 2026) and Onshape (August 2026, early access) now have official MCP servers. For topology optimization, the active open Python options are PyTopo3D, pyMOTO, jax-fem and Scikit-Topt.

### Cited Findings

**CAD MCP servers (comparison guide dated 2026-10-16, published by a vendor, BuildCAD AI, so weigh its rankings accordingly)**

| Server | Repo/page | Stars | License | Headless | Capabilities |
|---|---|---|---|---|---|
| FreeCAD MCP (neka-nat) | github.com/neka-nat/freecad-mcp | about 2,900 | MIT | No (FreeCAD must be open; XML-RPC to an addon) | FreeCAD 1.0/1.1, screenshots, CalculiX FEM |
| build123d-mcp (pzfreo) | github.com/pzfreo/build123d-mcp | n/s | Apache-2.0 | Yes | STEP/STL/DXF/SVG, previews, topology, validity gate, printability |
| mcp-build123d (Casys-AI) | github.com/Casys-AI/mcp-build123d | n/s | n/s | Yes | Deno/TS; mass properties; STEP/STL/GLTF |
| Fusion MCP (Autodesk, official) | help.autodesk.com | n/a | with Fusion | No | Built in since May 2026; Autodesk says it is unreliable for complex models and assemblies |
| Onshape FeatureScript MCP (official) | onshape.com | n/a | n/s | Yes (cloud) | Early access since August 2026 |
| onshape-mcp / jarvis-onshape-mcp | github.com/hedless/onshape-mcp | n/s | n/s | Yes (REST API keys) | STEP/glTF export, interference checks |
| OpenSCAD MCP | github.com/RobertCoop/openscad-mcp | n/s | MIT | needs OpenSCAD | Meshes only |
| Blender MCP (ahujasid) | github.com/ahujasid/blender-mcp | highest | MIT | No | Meshes, renders |
| SolidworksMCP-TS, autocad-mcp | github.com/vespo92/SolidworksMCP-TS; github.com/puran-water/autocad-mcp | n/s | n/s | Windows/desktop | — |

Sources: [buildcad.ai CAD MCP comparison (Oct 2026)](https://buildcad.ai/guides/cad-mcp-servers); [neka-nat/freecad-mcp](https://github.com/neka-nat/freecad-mcp); [pzfreo/build123d-mcp](https://github.com/pzfreo/build123d-mcp); [Casys-AI/mcp-build123d](https://github.com/Casys-AI/mcp-build123d)

- theosib/FreeCAD-MCP-Server (LGPL-2.1+) connects Claude Code to a live FreeCAD instance. — [GitHub](https://github.com/theosib/FreeCAD-MCP-Server)
- A drawing2cad-mcp issue proposes CadQuery as a headless kernel because freecad-mcp 0.1.27 routes everything through XML-RPC to the GUI addon. — [GitHub issue](https://github.com/hackhog/drawing2cad/issues/7)
- agentcad is a CLI and MCP server for AI agents, tagged cadquery and build123d. — [GitHub topic build123d](https://github.com/topics/build123d)
- A build123d/CadQuery FreeCAD workbench exists. — [HN thread](https://news.ycombinator.com/item?id=47087171)

**Open topology-optimization libraries**
- **PyTopo3D**: 3D SIMP in Python with CUDA via CuPy (arXiv 2504.05604). — [GitHub](https://github.com/jihoonkim888/PyTopo3D)
- **pyMOTO**: modular topology optimization framework with OC and MMA optimizers. — [GitHub](https://github.com/aatmdelissen/pyMOTO)
- **jax-fem** (deepmodeling): differentiable FEM in JAX for topology optimization, updated around October 2026 per the GitHub topic listing. — [GitHub topics](https://github.com/topics/topology-optimization)
- **Scikit-Topt** (JOSS). — [ResearchGate](https://www.researchgate.net/publication/399103193_Scikit-Topt_A_Python_Library_for_Algorithm_Development_in_Topology_Optimization)
- **TopoKit**: pre-alpha, with 2D/3D SIMP and OC/MMA, but no CAD import/export yet. — [PyPI](https://pypi.org/project/topokit/)
- **zfergus/topopt**: early-stage, last updated July 2022, so likely stale. — [GitHub](https://github.com/zfergus/topopt)
- DTU TopOpt group's Python code collection. — [DTU](https://www.topopt.mek.dtu.dk/apps-and-software/topology-optimization-codes-written-in-python)
- DL4TO (3D deep-learning topology optimization): repository and status not confirmed.

### Inferences
- **MCP recommendation.** Build a custom MCP server around build123d or CadQuery plus the Gmsh/CalculiX pipeline, using pzfreo/build123d-mcp as reference or base (Apache-2.0 allows reuse). FreeCAD MCP is the most visible open "competitor", but its GUI dependence makes it a poor fit for a web service.
- The official Fusion and Onshape MCP servers mean commercial CAD vendors are entering the same space. They cover CAD only, not optimization.
- Topology optimization output (density fields) does not convert easily to editable B-rep. The Physics-in-the-Loop paper makes the same point. For a solo developer, parametric size and shape optimization on CadQuery parameters is the right first step, with topology optimization (PyTopo3D or pyMOTO) as an optional "concept" visualization.

### Gaps
- No data was found on NVIDIA PhysicsNeMo (formerly Modulus; Apache-2.0, from memory) or other open surrogate libraries in this session. The FreeCAD FEM workbench's AI addons were also not covered.
- Star counts and last-commit dates for most MCP repos were not stated.

---

## 4. Toolchain assessment (CadQuery/build123d, Gmsh, CalculiX, alternatives, OpenFOAM, pymoo/Optuna, browser visualization)

### Takeaway
The planned stack is the same one the strongest 2026 academic closed-loop paper uses (CadQuery → gmsh → CalculiX), so it is validated. The main risks are:
- **Meshing robustness**: 76-88% meshing success in Physics-in-the-Loop.
- **Getting CalculiX onto Windows**: no official binary; use conda-forge.
- **Parsing .frd results**.
- **Unit consistency**: CalculiX is unitless.

Browser display of per-node fields is best done by converting to surface meshes with per-vertex scalars, either glTF/three.js vertex colors or vtk.js/trame.

### Cited Findings

**CadQuery vs. build123d**
- build123d is derived from parts of CadQuery but extensively refactored into an independent framework. Both run on OCP/OpenCASCADE. It is Apache-2.0, and its docs call it "the most supported Code-CAD framework" (a self-claim). — [build123d GitHub](https://github.com/gumyr/build123d); [build123d docs: external tools](https://build123d.readthedocs.io/en/latest/external.html)
- The OCP viewer (ocp-vscode) supports both libraries. — [HN](https://hn.jakechampion.name/item/47772725)
- Benchmarks and agent papers use CadQuery as the output language (Text2CAD-Bench, CADPrompt, CAD-Recode, Physics-in-the-Loop, Hephaestus-CCX), so the LLM prior is stronger for CadQuery. — [Text2CAD-Bench](https://arxiv.org/html/2605.18430v1); [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)
- From memory, verify: CadQuery is Apache-2.0, and both libraries pip-install with OCP wheels on Windows.

**Gmsh + CalculiX**
- Both 2026 closed-loop papers mesh CadQuery output with gmsh. Physics-in-the-Loop uses the HXT algorithm with linear tet4 elements and reports meshing as the largest drop-off stage (for example, Gemini 3 Pro went from 97.5% execution to 76.3% meshing). — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)
- Hephaestus-CCX uses CalculiX with Abaqus-style input decks and ships CalculiX evaluation kits. — [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)
- **Windows availability.**
  - The official site (dhondt.de) does not host Windows builds; it points to bConverged, SourceForge calculixforwin and GitHub. — [dhondt.de](https://www.dhondt.de/)
  - conda-forge has win-64 packages for calculix 2.22 and 2.23. — [anaconda.org conda-forge calculix](https://anaconda.org/channels/conda-forge/packages/calculix/files)
  - A third-party GitHub release offers ccx/cgx 2.23 for Windows 11; verify checksums before using it. — [mkhan138 release](https://github.com/mkhan138/calculix-fea-examples/releases/tag/v1.0.0)
  - The official repository is github.com/Dhondtguido/CalculiX. — [CalculiX discourse](https://calculix.discourse.group/t/installation-of-ccx-2-23-fun/3833)
- **From memory, verify against the CalculiX manual (ccx_2.2x.pdf).**
  - Analysis types: \*STATIC (linear and nonlinear), \*HEAT TRANSFER (steady and transient), \*COUPLED TEMPERATURE-DISPLACEMENT, \*FREQUENCY (modal) and \*BUCKLE.
  - Element types:
    - C3D10, the quadratic tet, is the recommended solid tet. Gmsh "order 2" tets need node-ordering remapping for CalculiX/Abaqus; for 10-node tets the last two mid-side nodes are swapped.
    - Avoid C3D4 (linear tets) for stress, because they are overly stiff. Physics-in-the-Loop used tet4, which is a known accuracy weakness.
    - Use the DC3D10 element for heat transfer.
  - Results go to the .frd ASCII/binary file. Element stresses are extrapolated to nodes there; integration-point values go to .dat.
  - Units are user-consistent (for example mm-N-MPa-tonne). Mixing units is a classic error, as is forgetting density for modal and mass calculations.
  - Python tooling: ccx2paraview converts .frd to .vtu, and pygccx is a Python wrapper. Gmsh can also write .inp files directly. None of these were verified this session.

**Alternatives (from memory, flag as unverified this session)**
- **FEniCSx (dolfinx)**: very capable and differentiable-adjacent, but Windows support is limited (conda or WSL/Docker) and the learning curve is steeper.
- **scikit-fem**: pure Python, pip-installs on Windows; good for simple linear elasticity and heat.
- **Elmer**: GPL, has Windows builds, strong for multiphysics and thermal.
- **code_aster**: very capable, but Linux-centric (Salome-Meca).
- **torch-fem**: PyTorch FEA used by Physics-in-the-Loop; differentiable and pip-installable. — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)
- **jax-fem**: differentiable, used by TO-Master. — [arXiv 2607.01812](https://arxiv.org/html/2607.01812v1)

**OpenFOAM for later CFD**
- Agent automation is well studied: 88-96% FoamBench execution success. — [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)
- Solver docs and tutorials in context matter most (+15.5 points). — [arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)
- From memory: native Windows support for OpenFOAM is weak, so WSL2 or Docker is the practical route.

**pymoo vs. Optuna (from memory, verify via docs)**
- pymoo (Blank & Deb, IEEE Access 2020; Apache-2.0) provides NSGA-II, NSGA-III, MOEA/D, SMS-EMOA and R-NSGA-II, plus constraint handling and decision-making utilities such as pseudo-weights and hypervolume. It is a natural fit for batch evaluation of geometry parameters.
- Optuna (MIT) supports multi-objective studies (NSGAIISampler, NSGAIIISampler, TPE multi-objective, and BoTorch for Bayesian MO), `study.best_trials` for the Pareto set, built-in `plot_pareto_front` (Plotly) and an optimization-history plot. Its ask-and-tell interface, persistent RDB storage and dashboard are convenient for a long-running agent loop.
- Recommendation: use Optuna for sample-efficient Bayesian or TPE search when each FEA run is expensive (tens of seconds) and for its storage, history and dashboard. Use pymoo when the agent needs constrained NSGA-II or III over a population or hypervolume metrics. The two can coexist.

**Browser visualization of FEA fields**
- PyVista `Plotter.export_gltf` writes glTF but only supports PolyData. Unstructured volume meshes are converted to surfaces, so take the outer surface with `extract_surface()` and carry point scalars or colors. — [PyVista docs](https://docs.pyvista.org/api/plotting/_autosummary/pyvista.Plotter.export_gltf.html)
- EasyFEA exports simulation fields (displacement, von Mises) to glTF for a web gallery, which is a working precedent: `GLTF.Save_simu(simu, ["uy","Svm"], folder)`. — [EasyFEA docs](https://easyfea.readthedocs.io/en/stable/howto/postprocess.html)
- PyVista also has an exporter to vtk.js scene files. — [PyVista export.py](https://github.com/florian-wagner/pyvista/blob/floors/pyvista/export.py)
- From memory: trame (Kitware) gives a Python-driven web UI with VTK and ParaView rendering. Three.js with a BufferGeometry `color` attribute or a custom shader plus a lookup-table texture allows interactive colormap and range changes and deformation scaling with a displacement attribute. That approach is better than baking colors into glTF.

### Inferences
- **Recommended data path:**
  1. Run CalculiX and get the .frd file.
  2. Parse it with a custom parser or ccx2paraview → meshio/PyVista.
  3. Extract the surface.
  4. Export positions, displacement vectors and scalar arrays (von Mises, temperature) as a compact binary (glTF with custom attributes, or JSON/typed arrays).
  5. In Three.js, apply a shader-based colormap with a user-adjustable range and a deformation-scale slider.
  6. Plot the Pareto front and optimization history in Plotly.
- **Mitigations for the known pitfalls:**
  - Defeature small fillets and holes before meshing.
  - Use gmsh mesh-size fields and fall back across algorithms (HXT → Delaunay → Frontal), retrying with a coarser mesh.
  - Check that the solid is watertight and is a single connected body before meshing.
  - Run a mesh-convergence sanity check on the final design.
  - Standardize on mm-N-MPa-tonne and record units in metadata.
  - Validate the CalculiX setup against an analytic cantilever beam.

### Gaps
- Release cadence and maturity of CadQuery and build123d in 2026, pygccx and ccx2paraview status, and the CalculiX manual's element list were not verified this session.
- No 2026-specific source compared trame with vtk.js for FEA.

---

## 5. Visualization patterns used by the best tools

### Takeaway
The research systems show results as static figures. Galleries, convergence plots and failure-type distributions in Physics-in-the-Loop, and ParaView multi-view renders in Hephaestus. Interactive, explainable visualization is therefore a differentiation opportunity rather than prior art. The best-evidenced patterns are listed below.

### Cited Findings
- Physics-in-the-Loop uses four figure types:
  - a gallery of designs per load case and model;
  - iteration-by-iteration refinement snapshots;
  - safety factor and volume convergence across iterations, with the target band shaded;
  - a failure-type distribution, plus a side-by-side comparison against topology optimization output.
  — [arXiv 2605.19717](https://arxiv.org/html/2605.19717)
- Hephaestus-CCX uses 21 (or 7) calibrated ParaView views as feedback to the agent. Seven views sometimes matched 21. Some errors, such as mass and areal density, are invisible in renders, so numeric requirement tables must accompany images. — [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)
- CAD MCP servers emphasize multi-view snapshots, sections, measurements, clearance and validity checks returned to the agent (smlibpy, jarvis-onshape and build123d-mcp). — [buildcad.ai guide](https://buildcad.ai/guides/cad-mcp-servers)
- CADCodeVerify renders the object and answers verification questions about it. Visual self-checks are a recognized trust mechanism. — [arXiv 2410.05340](https://arxiv.org/html/2410.05340)
- Optuna's built-in Plotly visualizations (Pareto front, optimization history, parameter importance) are the de facto open-source standard (from memory; see docs).

### Inferences
- **Recommended StructuraAI views:**
  1. A 3D viewer with a contour legend, an adjustable range and a deformation-scale slider, overlaid on undeformed ghost geometry.
  2. A probe showing maximum stress or temperature with a marker at its location.
  3. A requirement checklist (pass/fail, with margin and safety factor) next to the 3D view, which is the "trust panel".
  4. An optimization-history plot with the feasible band shaded.
  5. An interactive Pareto front where clicking a point loads that design.
  6. A baseline-vs-optimized split view with a delta table.
  7. An agent-trace timeline showing each generate → mesh → solve → check step and the repairs.
  8. Downloadable STEP and report files.
- **Credibility.** Show the mesh statistics and a validation case (cantilever versus beam theory) to answer "why trust this?"

### Gaps
- No usability study comparing visualization patterns for AI-driven simulation results was found.
