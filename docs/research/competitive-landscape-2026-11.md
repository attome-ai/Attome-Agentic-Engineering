# StructuraAI Can Win on Integration, Not Invention

As of November 2026, nobody sells the full loop StructuraAI wants to demo: a natural-language brief, turned into generated parametric CAD, checked with real FEA and thermal analysis on that same geometry, improved by multi-objective optimization, and explained through interactive field and Pareto visualization, all self-serve and cheap. The pieces exist, but different companies own them. SimScale's Engineering AI agent leads on natural-language-to-simulation and, since October 2026, runs Bayesian optimization on an existing Onshape model. Zoo and Adam lead text-to-CAD. Siemens/Altair and Ansys lead solvers, surrogates and Pareto optimization, but only for experts with enterprise budgets. Synera leads enterprise multi-agent orchestration of other vendors' tools. The biggest threat to any novelty claim is two May 2026 academic papers that already close the NL → CadQuery → gmsh → FEA → refine loop. One of them uses exactly the planned CadQuery + gmsh + CalculiX stack. StructuraAI therefore cannot claim novelty for text-to-CAD, LLM-driven simulation setup, or an LLM+FEA feedback loop. What it can credibly own for the January 10, 2027 proposal and the February 3, 2027 live demo is narrower: (1) numeric multi-objective optimization that produces a Pareto front, (2) structural plus thermal analysis on one parametric model, (3) a trust panel that shows verification evidence, and (4) visualization a non-expert can read, all on an open, license-safe stack. The engineering verdict is just as clear: wrap CalculiX, Gmsh, build123d/CadQuery and Optuna/pymoo, and spend the solo-developer budget on the thin orchestration, verification and visualization layer rather than on a new solver.

## Five camps each own one slice of the loop

The landscape splits into five camps. None of them ships the complete loop.

**Incumbent CAD suites** have mature generative design behind manual setup. Fusion's cloud generative design produces many outcomes per study, with manufacturing-method constraints and an outcome explorer. Its AI layer is still a text-to-command Assistant plus a local MCP server, and Autodesk's roadmap only says it is "working toward" neural CAD that turns prompts into editable geometry ([Fusion Roadmap 2026](https://www.autodesk.com/products/fusion-360/blog/fusion-roadmap-2026/); [Engineering.com](https://www.engineering.com/is-autodesks-neural-cad-worth-getting-excited-about/)). PTC Creo is the incumbent closest to natural-language optimization. Its reported "Automate" tier takes a plain-language objective and proposes modified geometry in a human-reviewed sandbox, but it is alpha, and the detail comes from a competitor's blog rather than PTC ([getleo.ai on Creo AI](https://www.getleo.ai/blog/creo-built-in-ai)). nTop leads implicit, lattice and field-driven geometry and has headless automation, but no evidence of any LLM feature turned up ([nTop support](https://support.ntop.com/hc/en-us/articles/26062971882131)).

**Incumbent simulation vendors** own solver credibility. Ansys SimAI is a physics-agnostic surrogate that needs 30–100 customer simulations and reports a per-prediction confidence level. Ansys's first agentic features, Mesh Agent and the Discovery Validation Agent, were only in preview in 2026 R1 ([Ansys SimAI](https://ansys.synopsys.com/products/ai/simai); [Synopsys PR](https://www.prnewswire.com/news-releases/synopsys-launches-ansys-2026-r1-to-re-engineer-engineering-with-joint-solutions-and-ai-powered-products-302711215.html)). Siemens now absorbs Altair. It has PhysicsAI embedded across Simcenter, a new PhysicsAI Generate that produces concepts from targets and historical data, and HEEDS for multi-objective optimization. Its copilots, however, are help tools, and no source shows an autonomous solve agent ([Siemens PR](https://www.prnewswire.com/news-releases/siemens-accelerates-engineering-simulation-with-a-unified-ai-powered-simcenter-portfolio-302835381.html)).

**SimScale is the most dangerous single competitor.** Its Engineering AI agent was opened to more than 900,000 community users in August 2026 and is free on every plan ([SimScale press](https://www.simscale.com/press/engineering-ai-agent-open-to-community/); [SimScale Autumn 2026](https://www.simscale.com/blog/whats-new-simscale-autumn-2026/)). Since September 2026 it has also run inside Onshape. From one prompt it selects the physics, prepares the CAD, meshes it, sets boundary conditions, solves and explains the results across FEA, CFD, thermal and EM ([SimScale Onshape launch](https://www.simscale.com/press/simscale-launches-engineering-ai-agent-for-onshape/)). The two research notes disagree on whether the agent optimizes. The autumn release notes show no optimization. The newer October 30, 2026 blog demonstrates a 16-design Bayesian optimization over three rounds, using Onshape configuration parameters as the design space, and says "everything in the demo is available today" ([SimScale text-to-CAD blog](https://www.simscale.com/blog/text-to-cad/)). This report follows the newer source. The SimScale agent does not generate geometry from scratch, and no Pareto front has been shown.

**Text-to-CAD startups** generate geometry but do no physics. Zoo's Zookeeper writes parametric KCL code on Zoo's own B-rep kernel. It checks itself with multi-view snapshots and mass properties, does well on simple parts and fails on complex ones. It has no FEA ([Zoo research](https://zoo.dev/research/zookeeper); [Leo AI comparison](https://www.getleo.ai/blog/text-to-cad-tools-comparison-guide), a competitor's source with factual errors). Adam is a feature-tree-aware copilot for Onshape and Fusion, with a REST API, an MCP server and the open-source CADAM. It markets a "CAD to FEA loop" that drives the customer's own Ansys, Abaqus or COMSOL licenses, but the official page text could not confirm that the loop has shipped ([adam.new](https://adam.new/); [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools)).

**Physics-AI startups** are surrogate- and enterprise-centric. PhysicsX raised $300M at a $2.4B valuation. Its LGM-Aero model was trained on more than 25M meshes, but there are no published accuracy benchmarks and no agent evidence ([SiliconANGLE](https://siliconangle.com/2026/06/08/physicsx-reels-300m-speed-hardware-design-ai/); [PhysicsX newsroom](https://www.physicsx.ai/newsroom/introducing-lgm-aero-genai-for-aero-engineering-and-airplane-showcase-application-for-aerostructures)). Neural Concept announced an "AI Design Copilot" for "CAD-ready models from high-level design ideas" at CES 2026, with limited access, and no later confirmation of general availability was found ([ePartTrade](https://epartrade.com/industry-news/ai-design-copilot-introduces-physicsaware-cadready-automation-for-engineers-a77e02fc)). Synera ($40M Series B) orchestrates agents across 75–80+ CAD/CAE tools. Its NASA "Text to Bipod Flexures" supervisor coordinates stress, modal and thermal agents, but Synera ships no solver of its own and sells only to enterprises ([Synera](https://www.synera.ai/news/agentic-ai-for-engineering-in-aerospace-and-defense); [SiliconANGLE](https://siliconangle.com/2026/04/14/german-startup-synera-lands-40-million-automate-engineering-workflows-ai-agents/)).

The May 2026 papers are the closest prior art of all. **Physics-in-the-Loop** (Berger et al.) runs CadQuery → gmsh → torch-fem with Planner, CAD and reviewer agents. With FEA feedback, 59.0% of designs reached the target safety-factor band, against 22.2% without it ([arXiv 2605.19717](https://arxiv.org/html/2605.19717)). **Self-Improving CAD Generation Agents with FEA Feedback** (Son, Park et al.) runs CadQuery → gmsh → **CalculiX** inside the Claude Code and Codex harnesses, scored against the 50-brief Hephaestus-CCX benchmark with executable requirement checkers ([arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)).

## The feature matrix: no column is strong on both NL intent and Pareto optimization

The scale runs from 0 (absent) through 1–30 (stub), 31–60 (partial) and 61–80 (solid) to 81–100 (leading). Scores reflect *shipped* capability as of November 2026. Unverified vendor claims are discounted. These are analyst judgments built from the cited evidence; no independent benchmark exists for any of them. **†** marks a low-confidence score: it is based on pre-2026 background knowledge that was not re-verified, on competitor or marketing sources only, or on public documentation too thin to support it. Four rows (requirements formalization, editable parametric output, reports/collaboration, openness) were added for this report and were not in the researchers' original matrices, so treat them with extra caution.

Columns: **FUS** Autodesk Fusion · **nTop** · **SIE** Siemens Simcenter + Altair (Inspire, HyperWorks, HEEDS) · **CREO** PTC Creo · **ANS** Ansys/Synopsys · **SIMS** SimScale agent (+Onshape) · **NC** Neural Concept · **PX** PhysicsX · **SYN** Synera · **ZOO** Zoo Design Studio/Zookeeper · **ADAM** Adam · **PIL** Physics-in-the-Loop (arXiv 2605.19717) · **HEPH** Self-Improving CAD+FEA / Hephaestus-CCX (arXiv 2605.17448)

| Capability | FUS | nTop | SIE | CREO | ANS | SIMS | NC | PX | SYN | ZOO | ADAM | PIL | HEPH |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ***Intent / requirements*** | | | | | | | | | | | | | |
| Natural-language intent input | 30 | 0 | 30 | 35† | 40 | 75 | 35† | 10 | 50† | 80 | 75 | 70 | 75 |
| Requirements formalized and machine-checked | 50† | 40† | 55† | 45† | 50 | 60 | 30† | 30† | 55 | 20 | 25† | 65 | 80 |
| ***Geometry generation*** | | | | | | | | | | | | | |
| Text-to-CAD from scratch | 15 | 0 | 10 | 20† | 10† | 5 | 35† | 5 | 40† | 75 | 60 | 65 | 65 |
| Physics-driven generative / topology geometry | 80 | 90 | 85 | 75 | 55 | 0 | 55 | 60 | 45† | 0 | 0 | 10 | 0 |
| Editable parametric output (B-rep / feature tree / code) | 60† | 35 | 65† | 80† | 50† | 40† | 45† | 30† | 60† | 80 | 75 | 65 | 65 |
| ***Simulation physics*** | | | | | | | | | | | | | |
| Structural FEA | 75† | 65 | 90 | 75† | 95 | 75 | 50† | 55† | 60 | 0 | 30† | 60 | 70 |
| Thermal | 60† | 50† | 85 | 65 | 90 | 75 | 50† | 50† | 50† | 0 | 15† | 0 | 0 |
| CFD | 30† | 45 | 90 | 45† | 95 | 75 | 75 | 80 | 35† | 0 | 5 | 0 | 0 |
| AI surrogate prediction | 15 | 30† | 85 | 20 | 85 | 60 | 85 | 90 | 15 | 0 | 0 | 0 | 0 |
| ***Optimization*** | | | | | | | | | | | | | |
| Automatic optimization | 75 | 75 | 90 | 70 | 85 | 55 | 65 | 65 | 60 | 0 | 10 | 50 | 40 |
| Multi-objective / Pareto | 60† | 55† | 85† | 45† | 85 | 20† | 55† | 50† | 40† | 0 | 0 | 10 | 0 |
| ***AI / agent autonomy*** | | | | | | | | | | | | | |
| LLM agent drives the tools | 35 | 5 | 25 | 35† | 40 | 75 | 35† | 10 | 75 | 65 | 65 | 75 | 80 |
| Closed design → simulate → iterate loop | 25 | 45 | 30 | 25† | 25 | 50 | 40† | 45 | 65† | 35 | 30† | 75 | 70 |
| ***Verification / trust*** | | | | | | | | | | | | | |
| Verification and trust reporting (UQ, mesh studies, checks) | 55† | 45 | 70 | 50† | 80 | 65 | 30† | 30 | 55 | 30 | 25 | 70 | 70 |
| ***Visualization*** | | | | | | | | | | | | | |
| 3D viewer with result fields | 75† | 70† | 85 | 70† | 85 | 75 | 60† | 60† | 35† | 40 | 35 | 30 | 40 |
| Design comparison / history / Pareto views | 75† | 50† | 75 | 50† | 75 | 45 | 55† | 45† | 35† | 25 | 20 | 35 | 25 |
| ***Manufacturing / export*** | | | | | | | | | | | | | |
| Manufacturability constraints / DFM | 80† | 75 | 80 | 70† | 55† | 10 | 25† | 15 | 55 | 15 | 35† | 10 | 15 |
| Export and interoperability | 70 | 55 | 75 | 70† | 85 | 60 | 60† | 40† | 85 | 70 | 60 | 50† | 50† |
| ***Collaboration / reporting*** | | | | | | | | | | | | | |
| Reports, audit trail, collaboration | 55† | 30† | 60† | 50† | 65 | 75 | 40† | 35† | 65 | 30† | 50† | 25 | 30 |
| API / MCP / automation | 75 | 75 | 80 | 65† | 85 | 75 | 50† | 50† | 70 | 65 | 70 | 25† | 40† |
| ***Openness / cost*** | | | | | | | | | | | | | |
| Cost and accessibility | 75 | 30 | 30 | 40† | 30 | 80 | 15 | 35 | 15 | 80 | 70 | 70 | 55 |
| Openness (source, self-host, bring-your-own LLM) | 15 | 10 | 10 | 5 | 15 | 10 | 5 | 10 | 25 | 25† | 50 | 60† | 60† |

### Why each column scores the way it does

**Fusion.** Generative design and manufacturing score high because of multi-outcome cloud studies with additive, milling, cutting and die-casting constraints. Reviewers note that the output "often requires significant manual cleanup and re-modeling" ([cognitivefuture.ai](https://cognitivefuture.ai/ai-cad-software-pricing/)). Natural-language intent and closed-loop autonomy stay low because the Assistant is text-to-command, neural CAD is unreleased, and no source shows the Assistant setting up a simulation or generative study ([Fusion Roadmap 2026](https://www.autodesk.com/products/fusion-360/blog/fusion-roadmap-2026/); [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools)). The cost score reflects an $85/month subscription plus a reported ~$1,600/year generative extension, with free personal and education tiers ([DE247](https://www.digitalengineering247.com/article/autodesk-modifies-generative-design-extension-pricing/news)). The physics, visualization and outcome-explorer scores rest on pre-2026 background knowledge (†).

**nTop.** It leads implicit geometry, and its topology-optimization block was improved in 5.0 and 5.18 ([nTop 5.0 meshing](https://support.ntop.com/hc/en-us/articles/30049593128211); [nTop 5.18](https://support.ntop.com/hc/en-us/articles/39473214985107-nTop-5-18-What-s-New)). Its CFD depth comes from acquiring cloudfluid ([3DPrint.com](https://3dprint.com/316451/ntop-acquires-cloudfluid-further-integrates-cfd-simulation/)). Conjugate heat transfer is reported only in a forum post (†). No LLM feature was found, and B-rep export of implicit bodies is a known weakness.

**Siemens/Altair.** PhysicsAI trains and predicts inside Simcenter Inspire 2026.1 ([Siemens blog](https://blogs.sw.siemens.com/simcenter/whats-new-in-simcenter-inspire-2026-1/)). HyperWorks 2026 adds GPU reduced-order models and side-by-side comparisons ([DE247](https://www.digitalengineering247.com/article/altair-hyperworks-2026-new-ai-hpc-and-multiphysics-capabilities)). HEEDS supplies multi-objective optimization ([DE247](https://www.digitalengineering247.com/article/siemens-adds-intelligence-based-design-to-nx-release/simulate)). Agent autonomy stays low: the claim of a Simcenter agent inside Industrial Copilot comes from a single unverified third-party blog ([Dr. Machine](https://drmachine.tech/en/news/siemens-industrial-copilot-nine-agents)).

**Creo.** Creo 12 added thermal objectives to generative design ([PTC](https://www.ptc.com/news/2025/ptc-launches-creo-12)), and GTO runs inside the parametric history tree ([TriStar](https://www.tristar.com/blog/creo-ai/)). Every score for the AI tiers is † because the Advise/Assist/Automate detail comes only from Leo AI, a competitor.

**Ansys.** It has the strongest verification credentials of any vendor: SimAI's per-prediction confidence plus autogenerated model evaluation reports, and reference-grade solvers. Agent autonomy stays at 40 because Mesh Agent is "available for exploratory use" and the Validation Agent is still "advancing through early customer evaluations" ([Synopsys PR](https://www.prnewswire.com/news-releases/synopsys-launches-ansys-2026-r1-to-re-engineer-engineering-with-joint-solutions-and-ai-powered-products-302711215.html)). Cost improves slightly thanks to the free Ansys Student/Discovery tier ([Ansys Student](https://www.ansys.com/en-gb/academic/students/ansys-student)).

**SimScale.** Natural-language intent, agent autonomy, reporting and cost are all strong. The agent runs autonomously and asynchronously, supports parallel conversations, exposes an API and MCP, compiles mesh-independence studies, and writes a PDF report of objective, design space, assumptions and results. It is free on every plan ([SimScale Autumn 2026](https://www.simscale.com/blog/whats-new-simscale-autumn-2026/); [SimScale text-to-CAD blog](https://www.simscale.com/blog/text-to-cad/)). Geometry generation is 0 because the agent only cleans up CAD. Pareto is low (†) because only single-objective Bayesian rounds were demonstrated. Community projects are public, which limits proprietary use.

**Neural Concept and PhysicsX.** Surrogates lead for both. Natural-language input, verification, viewer and API scores are mostly † because their public pages do not document these capabilities, and PhysicsX publishes no accuracy benchmarks ([SiliconANGLE](https://siliconangle.com/2026/06/08/physicsx-reels-300m-speed-hardware-design-ai/); [neuralconcept.com](https://www.neuralconcept.com/)).

**Synera.** Agent autonomy and interop are high because of shipped, logged, on-prem, bring-your-own-LLM multi-agent workflows across 80+ integrations ([synera.ai](https://www.synera.ai/)). Its physics depends on the customer's licensed tools, and the NASA figure of "100+ variants per hour" is a vendor claim (†).

**Zoo and Adam.** Text-to-CAD and editable output are high and every physics row is low. Adam's FEA score of 30† reflects a claim that could not be confirmed on its own page ([adam.new](https://adam.new/)). Zoo's openness score is † because its source-availability was not researched.

**The two research papers.** Both score high on closed-loop autonomy and verification because their physics tools are deterministic and agent-immutable, and HEPH adds executable requirement checkers. They score low on visualization (static paper figures and ParaView renders), on Pareto (neither runs a numeric multi-objective optimizer) and on thermal (structural only) ([arXiv 2605.19717](https://arxiv.org/html/2605.19717); [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)). PIL's FEA score is held at 60 because it used linear tet4 elements, which are known to be overly stiff for stress (a background claim; verify against the CalculiX manual). The openness scores are † because PIL's code is only "promised" under MIT/CC at camera-ready, and HEPH's release URL was not confirmed.

Read across the rows, the gaps line up with what StructuraAI wants to show. No column scores 60 or more on both natural-language intent and multi-objective/Pareto. The vendors that lead Pareto (SIE, ANS) score 30–40 on natural-language intent, and the natural-language leaders (SIMS, ZOO, ADAM, the papers) score 0–20 on Pareto. The comparison/history views and the trust-reporting rows are only "solid" for expensive expert tools. No product is under 75 on cost and still above 60 on structural FEA and natural-language intent, except SimScale.

## White space a solo developer can own, and what must not be claimed

Four gaps are genuinely open and small enough for one developer to build credibly by February 3, 2027.

**First, NL-to-Pareto.** A user's brief becomes an explicit, checkable spec. The spec drives a numeric multi-objective search (mass vs. stiffness/safety factor vs. peak temperature), and the result is an interactive Pareto front where clicking a point loads that design's geometry and stress or temperature fields. Neither 2026 closed-loop paper does systematic numeric multi-objective optimization, thermal analysis on the same CAD, or interactive browser results ([arXiv 2605.19717](https://arxiv.org/html/2605.19717); [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)). SimScale's shipped optimization is Bayesian over Onshape configurations, with no Pareto front shown ([SimScale blog](https://www.simscale.com/blog/text-to-cad/)).

**Second, structural plus thermal on one parametric model.** The papers are structural-only. The solver-driven thermal tools, ToffeeX and Diabatix ColdStream, run real thermal-fluid optimization but take form- or setup-driven input with no natural language, and access is through a demo request or "Try Now" rather than a self-serve plan ([toffeex.com](https://www.toffeex.com/); [diabatix.com](https://www.diabatix.com/coldstream)).

**Third, a visible trust layer.** Public verification and uncertainty evidence is weak for almost every physics-AI vendor ([physics-AI notes, PhysicsX coverage](https://siliconangle.com/2026/06/08/physicsx-reels-300m-speed-hardware-design-ai/)). The benchmarks show that execution validity, geometric fidelity and physical validity diverge ([Text2CAD-Bench](https://arxiv.org/html/2605.18430v1)), and execution-based and field-level scores can differ by about 20 points ([arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)). A trust panel would show requirement pass/fail with margins, mesh statistics, a mesh-convergence check, a reaction-force equilibrium check and a standing validation case (a cantilever against beam theory, or NAFEMS LE10 against its −5.38 MPa reference ([FeenoX LE10](https://www.seamplex.com/feenox/tests/nafems/le10/))). That answers "why should I trust this?" more directly than any competitor's public material does.

**Fourth, accessibility and openness.** The physics-AI segment is overwhelmingly enterprise and demo-gated, with no public pricing, and consolidation into Autodesk, CoreWeave, Mistral and Cadence is underway ([Mistral](https://mistral.ai/news/accelerate-ai-native-industry/); [Autodesk NavPack](https://www.autodesk.com/products/navpack)). A laptop-runnable, open-solver product that needs no customer training data and no commercial solver licenses is a real positioning wedge against surrogate vendors, which need 30–100 or more customer simulations ([Ansys SimAI](https://ansys.synopsys.com/products/ai/simai)), and against Adam and Synera, which delegate the physics to licenses the customer must already own.

The prior-art list is long, and the proposal must not claim any of the following as novel:

- **Natural-language-to-parametric CAD:** Zoo, Adam/CADAM, Henqo, and research from Text2CAD to CAD-Llama.
- **Render-and-check visual self-verification:** CADCodeVerify at ICLR 2025, and Zoo's snapshot review ([arXiv 2410.05340](https://arxiv.org/html/2410.05340); [Zoo research](https://zoo.dev/research/zookeeper)).
- **Natural-language-to-simulation setup and results interpretation:** SimScale.
- **An LLM+FEA closed refinement loop on CadQuery + gmsh + CalculiX:** Son et al.; Berger et al. with torch-fem.
- **Planner/reviewer multi-agent architectures:** Berger et al., and Synera's NASA supervisor.
- **CAD MCP servers:** build123d-mcp, FreeCAD MCP, and the official Fusion and Onshape servers ([buildcad.ai](https://buildcad.ai/guides/cad-mcp-servers)).
- **LLM-driven topology optimization:** TO-Master and TopOptAgents ([arXiv 2607.01812](https://arxiv.org/html/2607.01812v1); [arXiv 2605.23273](https://arxiv.org/html/2605.23273)).
- **Agent-written simulation reports:** SimScale.
- **Physics surrogates:** everyone, built on NVIDIA PhysicsNeMo architectures ([Rescale](https://rescale.com/release-notes/introducing-the-ai-physics-operating-system/)).

The honest framing is that StructuraAI *integrates and makes legible*: it is the first self-serve, open-stack product combining these known techniques with multi-objective structural plus thermal optimization and explainable visualization. Even that claim should be phrased as "we found no shipped product that…" rather than "first ever". The window is also short. SimScale (adding geometry editing or Pareto search to an agent that already optimizes) and Siemens (PhysicsAI Generate + HEEDS + a copilot) are the most likely incumbents to close it.

## Adopt real solvers and build only the thin engine layer

**Verdict: do not build a general FEA engine.** Spend the effort on a thin, verifiable orchestration layer. By the researcher's estimate, a linear tet10 elasticity plus steady-heat kernel in NumPy/SciPy takes about 1–2 weeks, and a verification suite about another week. Modal, buckling, contact, plasticity and production-quality thermo-mechanical coupling would take months each. That is exactly where CalculiX already wins, and the demo is roughly 13 weeks away. The parts worth building are what make the product defensible:

- a solver-independent DesignSpec/ResultSchema;
- CAD → mesh → input-deck adapters with physical-group tagging for boundary conditions;
- a content-hash cache from parameters to mesh to results;
- a verification harness that runs LE1/LE10/cantilever in CI and cross-checks CalculiX against scikit-fem;
- per-run sanity checks for equilibrium, units, mesh quality and convergence;
- schema-validated MCP tools (generate_part, mesh, solve_static, solve_thermal, evaluate_design);
- skin-mesh field export.

A small scikit-fem kernel is still worth keeping as a fast path and cross-check. It also gives a defensible "our own verified kernel" story without staking the demo on it.

The stack below favors open-source, modifiable tools with real physics and efficiency. Licenses marked ‡ come from the researchers' background knowledge and were not re-fetched; check each repository's LICENSE file before shipping.

| Layer | Adopt | License | Why / caveat |
|---|---|---|---|
| Agent ↔ CAD interface | Custom MCP server modelled on **build123d-mcp** | Apache-2.0 ([GitHub](https://github.com/pzfreo/build123d-mcp)) | Headless and sandboxed. The author self-reports that it raised a CADGenBench score from 0.360 to 0.457 and validity from 88% to 100%; this has not been independently replicated. FreeCAD MCP (MIT, about 2,900 stars) needs a running GUI, so it is a poor fit for a web service ([buildcad.ai](https://buildcad.ai/guides/cad-mcp-servers)). |
| CAD kernel / language | **CadQuery** as the LLM output language, with **build123d** for MCP tooling; both on OCCT/OCP | Apache-2.0‡; OCCT LGPL-2.1 + exception‡ | The benchmarks and both 2026 loop papers use CadQuery, so LLM priors are stronger there ([Text2CAD-Bench](https://arxiv.org/html/2605.18430v1)). Both libraries produce the same OCP shapes. |
| Implicit / lattice (phase 2) | PicoGK | Apache-2.0 (verified only via aggregators) | C#, with no first-class Python; optional. |
| Meshing | **Gmsh** in a separate process; **Netgen** as the commercial-safe alternative; **fTetWild** as a fallback for dirty geometry; **meshio** for conversion | GPL-2+ (verified, [gmsh.info](https://gmsh.info/LICENSE.txt)); LGPL-2.1‡; MPL-2.0‡; MIT‡ | Avoid TetGen (AGPL-3‡) in anything hosted. Meshing is the biggest failure stage. |
| Structural and thermal FEA | **CalculiX ccx** as a subprocess; **scikit-fem** as cross-check and fast path | GPL-2‡; BSD-3‡ | CalculiX covers static, nonlinear, modal, buckling, steady/transient heat and coupled thermo-mechanical analysis, using Abaqus-like decks that LLMs write well. Windows builds come from conda-forge 2.22/2.23 ([anaconda.org](https://anaconda.org/channels/conda-forge/packages/calculix/files)). NGSolve (LGPL‡) is the alternative if GPL becomes a problem. JAX-FEM is GPL-3 with commercial use "contact author", so it is research-only ([GitHub](https://github.com/deepmodeling/jax-fem)). torch-fem (MIT‡) is a permissive differentiable option to watch. |
| CFD (later) | **OpenFOAM** via WSL2/Docker; **SU2** for adjoint shape optimization; **XLB** for GPU lattice-Boltzmann | GPL-3‡; LGPL-2.1‡; Apache-2.0 ([GitHub](https://github.com/Autodesk/XLB)) | **Exclude FluidX3D**: its license forbids commercial use, military use and AI training ([LICENSE](https://github.com/ProjectPhysX/FluidX3D/blob/master/LICENSE.md)). Avoid Palabos (AGPL‡). |
| Topology optimization (concept view only) | Home-grown SIMP or PyTopo3D; DL4TO + dl4to4ocp; scikit-image marching cubes | MIT (unverified from LICENSE); Apache-2.0; MIT; BSD‡ | Density-to-editable-B-rep is unsolved, so deliver an STL plus a CalculiX re-verification ([arXiv 2504.05604](https://arxiv.org/abs/2504.05604v1); [DL4TO](https://github.com/dl4to/dl4to)). |
| Optimization | **Optuna** (storage, history, Pareto plots, ask-and-tell) plus **pymoo** (constrained NSGA-II/III, hypervolume); BoTorch/Ax for expensive runs under about 200 evaluations; SMT for surrogates | MIT‡; Apache-2.0‡; MIT‡; BSD-3‡ | Optuna's SQLite storage gives the design-history panel for free. |
| Visualization | **Three.js** with a shader colormap over skin-mesh scalars; **vtk.js** as the quicker FEA-native alternative; **PyVista** for surface extraction and glTF export; **Plotly** for Pareto and history plots | MIT‡; BSD-3‡; MIT‡; MIT‡ | PyVista's `export_gltf` handles PolyData only, so run `extract_surface()` first ([PyVista docs](https://docs.pyvista.org/api/plotting/_autosummary/pyvista.Plotter.export_gltf.html)). trame (Apache-2.0‡) is fastest to build but ties the UI to a Python session. |

The license implications are manageable for the competition and for hosted SaaS. GPL has no network clause, so running Gmsh and CalculiX server-side triggers no source-disclosure duty. Calling them as separate executables with file I/O is generally treated as "mere aggregation", which leaves the orchestration code proprietary. Distributing ccx or Gmsh binaries in a downloadable product requires offering their source, and importing the `gmsh` Python module into your own distributed process arguably makes the combined work GPL. The researchers flagged all of this as interpretation, not legal advice ([gmsh.info LICENSE](https://gmsh.info/LICENSE.txt)). AGPL tools (TetGen, Palabos) are the ones to avoid in anything hosted, and FluidX3D is unusable outright.

Efficiency comes from strategy more than from the solver. No controlled benchmark compares CalculiX, FEniCSx and scikit-fem at 50k–500k degrees of freedom on a laptop. FeenoX solved the LE10 fine mesh (about 616k DOF) in 1 min 34 s ([FeenoX LE10](https://www.seamplex.com/feenox/tests/nafems/le10/)). Reports on CalculiX's PaStiX vs. PARDISO speed conflict ([CalculiX forum](https://calculix.discourse.group/t/pastix-vs-pardiso-performance/177)). The researchers estimate about 1 s at 50k DOF and 10–60 s at 500k with a multithreaded direct solver. The workable recipe is:

- optimize on coarse linear meshes of about 20–50k DOF, then verify Pareto candidates on fine quadratic meshes;
- run several single-threaded ccx processes in parallel;
- cache results by parameter hash;
- reject cheaply on mass or beam-theory estimates before solving;
- fit a GP surrogate only after about 50–100 true evaluations.

Write the CalculiX `.inp` deck directly from Gmsh's arrays. FeenoX found that converting a Gmsh mesh to CalculiX format took more than five minutes on its fine mesh ([FeenoX LE10](https://www.seamplex.com/feenox/tests/nafems/le10/)). Before writing the proposal, run LE10 at three mesh densities on the actual demo laptop, so the runtime claims are measured rather than estimated.

## Product design: the LLM picks parameters, the optimizer searches, the solver decides

The evidence points firmly toward **parametric templates first, free-form CAD code second**. Frontier LLMs produce invalid CadQuery 11% of the time on primitive shapes but roughly 70–90% of the time on sweeps, lofts and shells ([Text2CAD-Bench](https://arxiv.org/html/2605.18430v1)). Both closed-loop papers saw over-detailed geometry break meshing or create stress concentrations, and in Physics-in-the-Loop every model over-engineered, landing near a safety factor of 5 against a 2–5 target ([arXiv 2605.19717](https://arxiv.org/html/2605.19717); [arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)). The right split of roles follows from that. The LLM interprets the brief, writes a typed spec (loads, supports, materials, objectives, limits with units), and either selects a vetted template (bracket, flexure, heat sink, enclosure wall, and similar families) or writes a constrained extrude/revolve/fillet/pattern script. It then names the free parameters and bounds. Optuna/pymoo, not the LLM, searches those parameters, because the optimizer does not over-engineer and does produce a real Pareto front. Free-form generation stays as the fallback for parts with no template, behind a validity gate (watertight, single connected solid) and a render-and-check step.

**Repair loops must be budgeted generously and kept out of the physics.** In a matched study, FoamBench success rose from 0.9% with no repair to 71.8% with 2 attempts, 90.0% with 5 and 96.4% with 10. Injecting solver tutorials and docs added about 15 points, while a scripted reflection prompt added nothing ([arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)). Physics-in-the-Loop shows meshing is its own failure stage: execution succeeded 96–97.5% of the time, but meshing only 76–88% ([arXiv 2605.19717](https://arxiv.org/html/2605.19717)). The pipeline therefore needs automatic defeaturing, Gmsh algorithm fallbacks (HXT → Delaunay → Frontal), coarsening retries, and fTetWild as a last resort. Solver tools should be deterministic and agent-immutable, so the agent can change geometry but never the physics that judges it. Standardize units (mm-N-MPa-tonne) and record them in metadata, because CalculiX is unitless. Prefer quadratic C3D10 elements for stress and DC3D10 for heat, but note that Gmsh's second-order node ordering needs remapping for CalculiX; this is background knowledge, so verify it against the manual.

**Model choice should vary by step and be benchmarked on your own tasks rather than assumed.** In Physics-in-the-Loop, models were nearly indistinguishable on execution success (96.4–97.5%). They differed on outcome: with FEA feedback, Opus reached the target band 72.7% of the time, versus 59.0% averaged across models. Cost per iteration ranged from about $0.023 (Gemini 3 Flash) to $0.204 (Opus), and a Planner agent cut mean iterations from 10.77 to 4.44 ([arXiv 2605.19717](https://arxiv.org/html/2605.19717)). On Hephaestus-CCX, GPT-5.5 xhigh gained more from one FEA retry (25.9% → 39.4% single-part) than Opus-4.7 max (27.5% → 29.1%). Across all 400 first attempts, zero passed every requirement, and 10 attempts took about 68 minutes per item ([arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)). Claude Opus 4.6 led the generic-harness CFD study at 96.4% ([arXiv 2609.03718](https://arxiv.org/html/2609.03718v1)). The practical pattern is a strong model (Claude Opus-class) for planning, spec extraction and first geometry authoring; a cheaper, faster model for repair iterations, log parsing and report drafting; and no LLM at all inside the numeric optimization loop. Results should be reported as separate execution, meshing, FEA and requirement-pass rates, ideally against Hephaestus-CCX if it is public. That is a credibility asset the judges can check.

**Visualization is where the product should feel unlike any competitor.** Research systems show only static galleries and convergence plots, and the vendors keep their viewers behind sales demos. The recommended views are:

- a 3D viewer with a shader colormap, an adjustable range, a deformation-scale slider and undeformed ghost geometry;
- a probe that marks peak stress or temperature;
- the trust panel beside it;
- an optimization-history plot with the feasible band shaded;
- a clickable Pareto front;
- a baseline-vs-optimized split view with a delta table;
- an agent-trace timeline of each generate → mesh → solve → check step and every repair;
- downloadable STEP and report files.

Numeric requirement tables must sit beside the images, because some failures (mass, areal density) are invisible in renders ([arXiv 2605.17448](https://arxiv.org/html/2605.17448v2)). For a live February demo, the content-hash cache doubles as insurance: precomputed runs of the scripted brief guarantee the demo completes even if a live mesh or solve fails. That is a planning inference, not something the sources tested.

## Conclusion

The competitive question has changed shape. In 2025, "AI that designs and simulates" read as a moonshot. By late 2026 every individual step is either shipped by a well-funded vendor or published with success rates, and the frontier has moved to **reliability, legibility and access**. The data says the hard problems are now meshing failures, over-engineered first attempts, near-zero strict requirement passes, and the gap between "it ran" and "it is right". Those are exactly the problems a disciplined solo developer can attack with templates, deterministic tools, an optimizer and a verification harness, and well-funded incumbents have not packaged answers to them for non-experts.

For the AIDTC proposal, the winning framing is not "the first AI that designs products". It is "a verified, explainable, open design-optimization loop": measured success rates on a public benchmark, a standing validation case, Pareto trade-offs a judge can click, and structural plus thermal results on one model. All of it runs on Apache, MIT and GPL-subprocess tools with no solver licenses or training data required. That claim survives scrutiny against SimScale, Synera and the May 2026 papers. Grander claims do not.

## Sources

**Text-to-CAD and CAD copilots**
- Zoo Zookeeper research — https://zoo.dev/research/zookeeper ; Zoo FAQ — https://zoo.dev/docs/faq
- Adam — https://adam.new/ ; TechCrunch on Adam — https://techcrunch.com/2025/10/31/yc-alum-adam-raises-4-1m-to-turn-viral-text-to-3d-tool-into-ai-copilot/ ; Show HN — https://news.ycombinator.com/item?id=47977694
- llms.blog AI CAD tools roundup (third party) — https://www.llms.blog/posts/best-ai-cad-tools
- Leo AI text-to-CAD comparison (competitor source, contains errors) — https://www.getleo.ai/blog/text-to-cad-tools-comparison-guide
- Backflip coverage — https://www.voxelmatters.com/backflip-ai-launches-cad-copilot-that-turns-3d-scans-into-editable-models/
- Spectral Labs SGS-1 — https://www.spectrallabs.ai/research/SGS-1
- SimScale "text-to-CAD" blog (Oct 30, 2026) — https://www.simscale.com/blog/text-to-cad/

**CAD incumbents**
- Fusion Roadmap 2026 — https://www.autodesk.com/products/fusion-360/blog/fusion-roadmap-2026/
- Engineering.com on neural CAD — https://www.engineering.com/is-autodesks-neural-cad-worth-getting-excited-about/
- DE247 generative design pricing — https://www.digitalengineering247.com/article/autodesk-modifies-generative-design-extension-pricing/news
- cognitivefuture.ai AI CAD pricing — https://cognitivefuture.ai/ai-cad-software-pricing/
- nTop 5.0 kernel — https://support.ntop.com/hc/en-us/articles/26062971882131 ; nTop 5.18 — https://support.ntop.com/hc/en-us/articles/39473214985107-nTop-5-18-What-s-New ; cloudfluid — https://3dprint.com/316451/ntop-acquires-cloudfluid-further-integrates-cfd-simulation/
- HyperWorks 2026 — https://www.digitalengineering247.com/article/altair-hyperworks-2026-new-ai-hpc-and-multiphysics-capabilities ; Simcenter Inspire 2026.1 — https://blogs.sw.siemens.com/simcenter/whats-new-in-simcenter-inspire-2026-1/
- PTC Creo 12 — https://www.ptc.com/news/2025/ptc-launches-creo-12 ; TriStar Creo AI — https://www.tristar.com/blog/creo-ai/ ; Leo on Creo AI (competitor source) — https://www.getleo.ai/blog/creo-built-in-ai
- Onshape AI Advisor — https://onshape.com/en/blog/ai-advisor-guide-cad-pdm
- SOLIDWORKS LEO/AURA — https://live.solidworks.com/demand/solidworks-ai-explained-leo-aura-solidworks-2026-solidworks-labs-demo?page=1

**Simulation incumbents**
- Ansys 2026 R1 — https://www.prnewswire.com/news-releases/synopsys-launches-ansys-2026-r1-to-re-engineer-engineering-with-joint-solutions-and-ai-powered-products-302711215.html ; SimAI — https://ansys.synopsys.com/products/ai/simai ; Ansys Student — https://www.ansys.com/en-gb/academic/students/ansys-student
- Siemens Simcenter summer 2026 — https://www.prnewswire.com/news-releases/siemens-accelerates-engineering-simulation-with-a-unified-ai-powered-simcenter-portfolio-302835381.html ; HEEDS in NX — https://www.digitalengineering247.com/article/siemens-adds-intelligence-based-design-to-nx-release/simulate ; Industrial Copilot agents (unverified blog) — https://drmachine.tech/en/news/siemens-industrial-copilot-nine-agents
- Dassault virtual companions — https://www.3ds.com/newsroom/press-releases/dassault-systemes-expands-3dexperience-ai-native-agentic-platform-new-virtual-companion-skills-co-engineer-humans
- SimScale agent to community — https://www.simscale.com/press/engineering-ai-agent-open-to-community/ ; Onshape launch — https://www.simscale.com/press/simscale-launches-engineering-ai-agent-for-onshape/ ; Autumn 2026 — https://www.simscale.com/blog/whats-new-simscale-autumn-2026/ ; pricing — https://simscale.com/pricing/
- Luminary research — https://luminary.ai/research/ ; Rescale AI Physics — https://rescale.com/release-notes/introducing-the-ai-physics-operating-system/ ; COMSOL 6.4 — https://comsol.com/release/6.4

**Physics-AI and agentic startups**
- Neural Concept — https://www.neuralconcept.com/ ; Series C — https://siliconangle.com/2025/12/18/ai-aided-design-software-startup-neural-concept-raises-100m-accelerate-product-engineering/ ; AI Design Copilot — https://epartrade.com/industry-news/ai-design-copilot-introduces-physicsaware-cadready-automation-for-engineers-a77e02fc
- PhysicsX Series C — https://siliconangle.com/2026/06/08/physicsx-reels-300m-speed-hardware-design-ai/ ; LGM-Aero — https://www.physicsx.ai/newsroom/introducing-lgm-aero-genai-for-aero-engineering-and-airplane-showcase-application-for-aerostructures
- Synera — https://www.synera.ai/ ; NASA case — https://www.synera.ai/news/agentic-ai-for-engineering-in-aerospace-and-defense ; Series B — https://siliconangle.com/2026/04/14/german-startup-synera-lands-40-million-automate-engineering-workflows-ai-agents/
- ToffeeX — https://www.toffeex.com/ ; Diabatix ColdStream — https://www.diabatix.com/coldstream ; Cognitive Design Systems — https://www.cognitive-design-systems.com/
- Mistral acquires Emmi AI — https://mistral.ai/news/accelerate-ai-native-industry/ ; Autodesk NavPack — https://www.autodesk.com/products/navpack

**Research**
- Physics-in-the-Loop (Berger et al., May 2026) — https://arxiv.org/html/2605.19717
- Self-Improving CAD Generation Agents with FEA Feedback / Hephaestus-CCX (Son, Park et al., May 2026) — https://arxiv.org/html/2605.17448v2
- Text2CAD-Bench — https://arxiv.org/html/2605.18430v1
- CADCodeVerify / CADPrompt — https://arxiv.org/html/2410.05340
- Generic harness vs specialized CAE agents — https://arxiv.org/html/2609.03718v1
- Foam-Agent — https://arxiv.org/html/2505.04997
- TO-Master — https://arxiv.org/html/2607.01812v1 ; TopOptAgents — https://arxiv.org/html/2605.23273
- PDE-Agents (citing FEABench) — https://arxiv.org/pdf/2606.07850

**Open-source toolchain and licenses**
- build123d-mcp — https://github.com/pzfreo/build123d-mcp ; CAD MCP comparison (vendor-published) — https://buildcad.ai/guides/cad-mcp-servers ; FreeCAD MCP — https://github.com/neka-nat/freecad-mcp
- build123d — https://github.com/gumyr/build123d
- Gmsh license — https://gmsh.info/LICENSE.txt
- CalculiX conda-forge Windows builds — https://anaconda.org/channels/conda-forge/packages/calculix/files ; CalculiX site — https://www.dhondt.de/ ; PaStiX vs PARDISO — https://calculix.discourse.group/t/pastix-vs-pardiso-performance/177
- JAX-FEM — https://github.com/deepmodeling/jax-fem ; NVIDIA Warp — https://github.com/Nvidia/warp ; scikit-fem — https://github.com/kinnala/scikit-fem
- FeenoX NAFEMS LE10 — https://www.seamplex.com/feenox/tests/nafems/le10/
- FluidX3D license — https://github.com/ProjectPhysX/FluidX3D/blob/master/LICENSE.md ; XLB — https://github.com/Autodesk/XLB
- PyTopo3D — https://arxiv.org/abs/2504.05604v1 ; DL4TO — https://github.com/dl4to/dl4to
- PyVista glTF export — https://docs.pyvista.org/api/plotting/_autosummary/pyvista.Plotter.export_gltf.html ; EasyFEA glTF — https://easyfea.readthedocs.io/en/stable/howto/postprocess.html
