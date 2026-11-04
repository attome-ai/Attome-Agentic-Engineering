# AI Text-to-CAD, CAD Copilots and AI Product-Design Assistants (state as of Nov 2026)

Research date: 2026-11-03. About 20 search/fetch calls. Many sources are vendor or competitor marketing; reliability is flagged inline. The scores at the end are suggested by the researcher and are judgments, not sourced facts.

## Q1. What each product ships today (inputs, outputs, quality, simulation, autonomy, visualization, integrations, pricing/funding, limitations)

### Takeaway
No pure text-to-CAD tool (Zoo, Backflip, Spectral SGS-1, Henqo, Vizcom, Meshy/Tripo) ships built-in FEA, thermal or CFD verification, or physics-driven optimization. The products closest to "describe it, then generate, simulate and iterate" are **Adam** (it claims a CAD-to-FEA loop that drives external solvers; this is marketing and I could not verify it), the **SimScale Engineering AI agent inside Onshape** (shipped: natural-language simulation setup across FEA/CFD/thermal/EM, plus Bayesian parametric optimization on an existing parametric model), and **Synera** (enterprise multi-agent orchestration of CAD and simulation tools, with NASA as a reference customer). None of them generates geometry from scratch and verifies it in one self-contained, consumer-accessible loop.

### Cited Findings

**Zoo (formerly KittyCAD): Zoo Design Studio, Zookeeper agent, KCL, Text-to-CAD API**
- Zookeeper is the conversational CAD agent built into Zoo Design Studio. Zoo announced it on Feb 5 2026 and shipped it with Design Studio v1.1 in Jan 2026. It replaces the earlier Text-to-CAD product, which launched in Dec 2023 — [Zoo research page](https://zoo.dev/research/zookeeper); [Zoo blog](https://zoo.dev/blog/announcing-zookeeper)
- Output is parametric KCL code (Zoo's CAD language) run on Zoo's own B-rep geometry engine. Users can mix this with point-and-click editing — [Zoo research](https://zoo.dev/research/zookeeper); [Zoo FAQ](https://zoo.dev/docs/faq)
- Self-verification: Zookeeper runs the model to catch errors, reviews the geometry through multi-view snapshots, searches the docs, and computes mass, volume, surface area and centre of mass. The announcement mentions no FEA or simulation. Zoo calls the agent "early" — [Zoo research](https://zoo.dev/research/zookeeper)
- Inputs: text prompts and existing projects/selections. In Feb 2026, image attachments were planned and STEP/STL reverse engineering was "in training." The March 2026 update added analysis of uploaded files and STEP-to-commented-KCL conversion. The May 2026 sketch-mode update added sketch blocks and a geometric constraint solver — [Zoo research](https://zoo.dev/research/zookeeper); [What's new Mar 2026](https://zoo.dev/blog/whats-new-mar-2026) (search-summary level)
- Known limitation: sketches inside custom KCL functions cannot be edited with the GUI sketch tools — [Zoo blog via search](https://zoo.dev/blog/announcing-zookeeper)
- API/CLI: there is a `zoo ml text-to-cad` CLI command group and a separate API pricing page — [Zoo docs](https://zoo.dev/docs/developer-tools/cli/manual/zoo_ml_text-to-cad). I found no Zoo MCP server.
- Pricing (third-party check, Oct 29 2026): Free $0 for 20 min/month of Zookeeper time; Plus $20/mo (~400 min); Pro $99/mo; Team $399/user/mo; extra usage $0.0083/s. Zoo trains on Free-plan data with no opt-out — [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools). The Zoo FAQ confirms 20 free minutes and Fast/Thoughtful modes on paid plans — [Zoo FAQ](https://zoo.dev/docs/faq)
- Quality: it does well on single simple parts (brackets, flanges, housings) and fails on complex parts (enclosure walls intersect at odd angles; snap-fits don't work) — [Leo AI comparison](https://www.getleo.ai/blog/text-to-cad-tools-comparison-guide) (a competitor's blog that contains factual errors: it calls Zoo "mesh-based STL," which contradicts Zoo's B-rep/KCL docs). Another comparison: "Good simple parts, failed in testing complex parts" — [search summary of mesh-vs-CAD reviews](https://getleo.ai/blog/best-engineering-tools-for-converting-sketches-into-3d-cad)

**Backflip AI**
- Founded by the Markforged founders Greg Mark and David Benhaim. Raised a $30M Series A co-led by NEA and a16z (dated Dec 2024 in profiles). I found no 2026 round — [Yahoo Finance](https://finance.yahoo.com/technology/ai/articles/backflip-ai-converts-2d-engineering-130000356.html); [Extruct](https://www.extruct.ai/hub/backflip-ai/)
- Text and sketch input produce **meshes**. Parametric output comes only from scans and drawings. In Aug 2026 Backflip shipped scan/mesh-to-parametric CAD (feature tree plus sketch constraints) as an Autodesk Fusion add-in. On Sep 17 2026 it launched 2D-drawing-to-parametric-3D. Plugins for more CAD packages are "planned later in 2026" — [VoxelMatters](https://www.voxelmatters.com/backflip-ai-launches-cad-copilot-that-turns-3d-scans-into-editable-models/); [The Decoder](https://the-decoder.com/backflip-ai-turns-3d-scans-into-editable-cad-models-in-minutes-instead-of-hours/); [Yahoo Finance](https://finance.yahoo.com/technology/ai/articles/backflip-ai-converts-2d-engineering-130000356.html)
- Pricing: 50 free credits; Builder $20/mo (100 credits); Pro $50/mo (300); Business $380/mo (2,500). Fast Mode costs 10 credits per job and Thinking Mode 50. No simulation mentioned — [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools)

**Adam (adam.new; YC W25; formerly marketed as AdamCAD)**
- Raised a $4.1M seed led by TQ Ventures after its viral text-to-3D app. It has pivoted to an AI copilot — [TechCrunch](https://techcrunch.com/2025/10/31/yc-alum-adam-raises-4-1m-to-turn-viral-text-to-3d-tool-into-ai-copilot/)
- The copilot runs inside Onshape (via FeatureScript) and Fusion, in beta per Show HN. It reads parts, understands the existing feature tree and edits it agentically, keeping everything parametric — [Show HN](https://news.ycombinator.com/item?id=47977694); [Product Hunt](https://www.producthunt.com/products/adam-cad-copilot). There is also a Rhino/Fusion "Connector" plugin — [adam.new](https://adam.new/)
- CADAM is Adam's open-source text-to-CAD tool. It turns text or a reference image into parametric OpenSCAD — [DevelopersDigest](https://www.developersdigest.tech/blog/adam-ai-cad-yc-w25-open-source-text-to-cad)
- Shipped developer surface: a REST API (/v1, OpenAPI spec), an authenticated **MCP server** for Adam projects and tasks, and OAuth — [adam.new](https://adam.new/)
- **Simulation claim (marketing, unverified):** Adam positions itself as an "AI mechanical engineer" with a "CAD to FEA loop that connects simulation software to CAD, runs analysis, and carries results back into the design" — [search summary of adam.new](https://adam.new/). A third-party roundup says it supports Ansys, Abaqus, COMSOL and Simcenter, runs DFM checks against company documents, edits parts after ECOs using simulation and PLM failure data, works via Slack/Teams/email, and has SOC 2 Type I — [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools). When I fetched adam.new directly, the text I got contained no FEA wording, so I could not confirm whether this is shipped or preview.
- Pricing: Free (Lite model, daily quota); Pro $40/mo; Max $200/mo; Teams/Enterprise separate — [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools)
- Quality: in the Leo test a gearbox housing failed to compile because of bad Booleans between non-intersecting bodies — [Leo AI comparison](https://www.getleo.ai/blog/text-to-cad-tools-comparison-guide) (competitor source; it wrongly calls Adam "Datagrok")

**Leo AI (getleo.ai)**
- Positioned as an engineering copilot built on a proprietary "Large Mechanical Model" trained on CAD, specs and engineering data. Raised $9.7M seed in Sep 2025 (lead: Flint Capital). Preqin reports an undisclosed angel round in Feb 2026, not corroborated elsewhere. Leo claims 96% correct answers vs 46% for generic AI, and names Scania and HP as customers — [Yahoo Finance](https://finance.yahoo.com/news/startup-built-ai-copilot-mechanical-130002739.html); [Preqin](https://www.preqin.com/data/profile/asset/leo-ai/765259)
- Core shipped value is engineering Q&A with cited sources, calculations (including "stress analysis" hand calcs) and natural-language/geometry part search across PDM/PLM (SolidWorks PDM, Vault, Windchill, Teamcenter, Arena). It is **not an FEA tool** — [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools); [Leo blog](https://www.getleo.ai/blog/text-to-cad-tools-comparison-guide)
- Contradiction: Leo's own blog says Leo "does not generate new geometry from text," while its FAQ (per llms.blog) says it can generate parametric parts and assemblies from text, images and drawings. Pricing is enterprise-only, annual per user, with no free trial — [Leo blog](https://www.getleo.ai/blog/text-to-cad-tools-comparison-guide); [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools)

**Vizcom**
- Sketch-to-render for industrial design. Raised a $20M Series A led by Index Ventures in Mar 2024; I found no 2026 round — [Vizcom blog](https://vizcom.ai/blog/vizcom-raises-series-a-funding); [CB Insights](https://www.cbinsights.com/company/vizcom/financials)
- 2D-to-3D turns images or sketches into **triangulated meshes** (.glb, .obj, .stl, .usdz). Detailed editing must happen in external tools. There is no parametric CAD and no simulation — [Vizcom docs](https://docs.vizcom.com/2D-to-3D)

**Spectral Labs (SGS-1)**
- SGS-1 (announced Sep 15 2025) generates B-rep parts as STEP from an image or 3D mesh, and has a Fusion 360 plug-in demo. SGS-2 is in development — [Spectral Labs](https://www.spectrallabs.ai/research/SGS-1); [X announcement](https://x.com/spectral_hq/status/1967642998646411418?lang=en)
- In Leo's 10-prompt test, a flat plate was perfect and simple STEPs opened cleanly in SolidWorks/Fusion. A multi-pocket machined housing was unusable (fillets lost tangency, planar faces warped). The reviewer called it a "promising research prototype" — [Leo AI comparison](https://www.getleo.ai/blog/text-to-cad-tools-comparison-guide)

**Henqo** (exists; small, self-described)
- Turns plain English into editable parametric B-rep solids on build123d/OpenCASCADE, with STEP/STL export. It checks manufacturing constraints (wall thickness, hole clearances, feature sizes), produces engineering drawings with PMI and instant manufacturing quotes, and lets users click faces or edges to iterate. Pay-once pricing. No independent reviews — [BetaList](https://betalist.com/startups/henqo); [AlternativeTo](https://alternativeto.net/software/henqo/about)

**Autodesk Fusion: Autodesk Assistant / Neural CAD**
- Text-to-Command (natural-language Fusion operations) ships but is immature. Neural CAD (text to editable B-rep) was announced but is **not publicly available** as of Apr 2026, and Autodesk has given no timeframe. The Assistant is being linked to Fusion through MCP — [Fusion Roadmap 2026](https://www.autodesk.com/products/fusion-360/blog/fusion-roadmap-2026/); [Engineering.com](https://www.engineering.com/is-autodesks-neural-cad-worth-getting-excited-about/); [DEVELOP3D](https://develop3d.com/cad/autodesk-unleashes-neural-cad-3d-generative-ai-foundation-models/)
- The Assistant has "Show me / With me / For me" modes and confirms changes before applying them. It cannot add sketch dimensions or constraints, works in cm (inch inputs can misplace geometry) and has limited assembly support. Flex Tokens for agentic tasks start Oct 13 2026. Fusion costs $85/mo or $680/yr — [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools). Fusion has its own simulation and generative design products, but these sources do not tie them to the Assistant.

**Shapr3D AI**: AI features are limited to generative renders and a help chatbot. I found no AI geometry generation — [Shapr3D support](https://support.shapr3d.com/hc/en-us/articles/20520096011164-Shapr3D-AI-User-Guides-and-Data-Use-Overview); [Shapr3D forum](https://discourse.shapr3d.com/t/feature-request-integrating-claude-ai-for-actual-3d-model-generation-not-just-renders/40767)

**Onshape AI Advisor**: a documentation help assistant only. It does not generate designs — [llms.blog](https://www.llms.blog/posts/best-ai-cad-tools)

**SimScale Engineering AI agent (simulation vendor; closest shipped analogue to the target loop)**
- Runs inside Onshape as an App Store app. From plain language it sets up, runs and interprets FEA, CFD, thermal and EM simulations on SimScale's cloud. It assigns materials, fills gaps in assumptions (and asks the user to confirm the simulation budget), and produces a PDF report documenting objective, design space, assumptions and results. It can run parametric optimization: the demo ran a 16-design Bayesian optimization over 3 rounds, using Onshape configuration parameters as the design space. Variable tables are not yet supported. The article says "everything in the demo is available today." Physics AI surrogates and headless/API workflows are described as future headroom — [SimScale blog, Oct 30 2026](https://www.simscale.com/blog/text-to-cad/)
- The Onshape FeatureScript MCP server (Onshape Labs, Aug 13 2026) lets a coding LLM write reusable parametric features — [SimScale blog](https://www.simscale.com/blog/text-to-cad/)

**Synera (Bremen; enterprise agentic engineering)**
- Raised a $40M Series B led by Revaia. Synera orchestrates specialized agents across CAD, meshing, manufacturing simulation and structural analysis (on NVIDIA NemoClaw, May 2026) and connects to 75+ tools (Altair, Autodesk, Hexagon, PTC, Siemens) — [TheNextWeb](https://thenextweb.com/news/synera-40m-series-b-agentic-ai-engineering); [BusinessWire](https://www.businesswire.com/news/home/20260531683430/en/Synera-Advances-AI-Agents-for-Design-and-Engineering-Simulation-with-NVIDIA)
- NASA Goddard's "Text to Bipod Flexures": a supervisor agent coordinates CAD and simulation agents for stress, modal and thermal analysis. Synera claims 100+ variants per hour, versus 4 variants in 2 days by 2 engineers. These are vendor claims — [Synera](https://www.synera.ai/news/agentic-ai-for-engineering-in-aerospace-and-defense)

**Camfer (YC S24)**: an AI CAD tool that plugs into SolidWorks and generates parametric designs from natural language and images. I found no FEA details — [search result summary](https://github.com/topics/text-to-cad?o=desc&s=updated) (weak source; verify)

**Meshy / Tripo (mesh-only generators)**
- They output triangle meshes suited to organic shapes. In All3DP tests, Meshy's STL had 4 non-manifold edges and Tripo's 24 (not printable), and generated threads were unusable. They lack dimensional precision. The recommended workflow is to rebuild in CAD — [All3DP](https://all3dp.com/1/do-free-text-to-3d-model-generators-really-work-we-tested-3-of-them); [Tripo blog](https://www.tripo3d.ai/blog/explore/ai-3d-model-generator-cad-like-precision-limitations)

**Academic: Text2CAD (NeurIPS 2024), FeaGPT**
- Text2CAD generates sequential parametric CAD from beginner-to-expert prompts. It is research, not a commercial product — [NeurIPS](https://proceedings.neurips.cc/paper_files/paper/2024/hash/0e5b96f97c1813bb75f6c28532c2ecc7-Abstract.html)
- FeaGPT runs an end-to-end Geometry-Mesh-Simulation-Analysis pipeline (FreeCAD + CalculiX) from natural-language specifications. It is academic, but it is the clearest public example of text-to-geometry-plus-FEA — [arXiv](https://arxiv.org/html/2510.21993v1)

### Inferences
- **Whitespace for the client:** among the generation-first tools (Zoo, Backflip, SGS-1, Henqo, Vizcom, Meshy/Tripo), none has native physics verification. The simulation-first tools (SimScale agent, Synera) need an existing parametric CAD model or enterprise setup. Adam is the main startup claiming both sides, and it delegates solving to the customer's commercial solvers (Ansys/Abaqus/COMSOL). A self-contained "NL → generated parametric CAD → built-in FEA/thermal → optimization → rich visualization" product with accessible pricing appears unoccupied, with SimScale+Onshape as the strongest practical substitute.
- Geometry reliability across all generators degrades sharply with complexity: multi-feature housings, fillets, snap-fits and Booleans fail. A simulation-verified loop has to handle invalid or non-watertight geometry gracefully.
- Code-as-CAD (KCL, OpenSCAD, CadQuery, build123d, FeatureScript) is the dominant route to editable output. Zoo's snapshot-based visual self-check is the most explicit shipped self-correction mechanism among the text-to-CAD tools.

### Gaps
- I could not verify whether Adam's CAD-to-FEA loop is generally available or demo/enterprise-only. Its official page text did not include it.
- Zoo's official pricing page, API pricing and any MCP server were not confirmed. The prices above come from a third party.
- CADAgent, Ponder, Trace, Kittl, "Plasticity AI" and "Fusion Assistant" as separate AI-CAD products: no evidence found. "Plasticity AI" results were an unrelated NLP company. Plasticity (the NURBS modeller) showed no AI features in my results. Kittl is, to my knowledge, a graphic-design tool, not CAD (not checked during this session). "Fusion Assistant" corresponds to Autodesk Assistant in Fusion. I found nothing on Ponder, Trace or a product called CADAgent in AI CAD, so treat them as unverified or non-existent.
- Spectral Labs' 2026 status (SGS-2 release, funding, pricing) is unknown.
- I found no independent, non-vendor benchmark of text-to-CAD accuracy. The main hands-on comparison is by competitor Leo AI and contains factual errors.
- I found no HN/Reddit threads with substantive user complaints in the time available, beyond the Show HN posts.

## Q2. Suggested 0-100 scores per capability row (researcher judgment)

### Takeaway
Scores reflect shipped capability. Vendor claims I could not verify are discounted (Adam's FEA loop, Synera's NASA numbers). SimScale is included as the reference "simulation agent" comparator.

### Cited Findings
Rows: NL = NL intent input; Geo = geometry generation; FEA = structural FEA; Th = thermal; CFD; Opt = automatic optimization; Par = multi-objective/Pareto; Sur = AI surrogate; Agt = LLM/agent autonomy; CL = closed-loop autonomy; Ver = verification/trust reporting; 3DV = 3D result visualization; Cmp = design comparison/history visualization; Mfg = manufacturability; Exp = export/interop; API = API/automation; Cost = cost/accessibility. Evidence for each score is cited in Q1.

| Product | NL | Geo | FEA | Th | CFD | Opt | Par | Sur | Agt | CL | Ver | 3DV | Cmp | Mfg | Exp | API | Cost |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Zoo Design Studio / Zookeeper | 80 | 70 | 0 | 0 | 0 | 0 | 0 | 0 | 65 | 35 | 30 | 65 | 25 | 15 | 70 | 65 | 80 |
| Adam (copilot + CADAM) | 75 | 60 | 35 | 15 | 5 | 10 | 0 | 0 | 65 | 30 | 25 | 40 | 20 | 35 | 60 | 70 | 70 |
| Backflip AI | 30 | 60 | 0 | 0 | 0 | 0 | 0 | 0 | 25 | 5 | 10 | 45 | 10 | 10 | 60 | 15 | 70 |
| Leo AI | 75 | 20 | 10 | 5 | 0 | 0 | 0 | 0 | 40 | 5 | 50 | 15 | 10 | 40 | 30 | 20 | 20 |
| Vizcom | 45 | 40 | 0 | 0 | 0 | 0 | 0 | 0 | 15 | 0 | 0 | 70 | 45 | 0 | 35 | 10 | 65 |
| Spectral Labs SGS-1 | 25 | 50 | 0 | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 15 | 0 | 15 | 60 | 20 | 40 |
| Henqo | 70 | 55 | 0 | 0 | 0 | 0 | 0 | 0 | 30 | 15 | 20 | 50 | 15 | 50 | 60 | 10 | 60 |
| Autodesk Fusion Assistant (Neural CAD unreleased) | 55 | 25 | 0* | 0* | 0* | 0 | 0 | 0 | 40 | 10 | 25 | 75 | 30 | 30 | 85 | 40 | 55 |
| Shapr3D AI | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 70 | 20 | 0 | 75 | 5 | 60 |
| Meshy / Tripo (mesh-only) | 70 | 35 | 0 | 0 | 0 | 0 | 0 | 0 | 15 | 0 | 0 | 65 | 25 | 5 | 40 | 60 | 85 |
| SimScale AI agent (+Onshape) | 65 | 10 | 80 | 75 | 75 | 60 | 25 | 25 | 65 | 55 | 65 | 80 | 50 | 15 | 60 | 40 | 45 |
| Synera (enterprise agents) | 60 | 50 | 70 | 55 | 30 | 60 | 35 | 20 | 75 | 60 | 50 | 50 | 40 | 50 | 75 | 70 | 10 |

*Fusion has separate Simulation and Generative Design products, but they are not driven by the AI Assistant in any evidence found. Scored 0 for the AI path.

One-line justifications (key scores):
- **Zoo:** the strongest shipped text-to-parametric-B-rep tool, with an agent that self-checks via snapshots and mass properties. It has no physics. It has an API/CLI and a free tier, but complex parts fail.
- **Adam:** a feature-tree-aware copilot in Onshape/Fusion with MCP and API. FEA is scored "partial" only because the CAD-to-FEA loop with external solvers is claimed but unverified, and it requires the customer's solver licences.
- **Backflip:** a leader in drawing/scan-to-parametric conversion. Text gives only a mesh. No physics.
- **Leo:** strong engineering Q&A, hand calcs and PLM search. It has little geometry generation and no FEA. Enterprise-only pricing.
- **Vizcom:** good for concept renders and visualization, with mesh-only 3D. Not engineering.
- **SGS-1:** image/mesh to STEP, research-grade, and degrades on complex parts.
- **Henqo:** a small, unreviewed text-to-B-rep tool with built-in DFM rules and quoting, which makes its manufacturability score notable.
- **Fusion Assistant:** a command copilot inside a full CAD and export ecosystem. Neural CAD generation is unreleased.
- **Shapr3D:** AI renders and a help chatbot only.
- **Meshy/Tripo:** fast, cheap meshes with frequent non-manifold errors. No engineering use.
- **SimScale agent:** the shipped NL-to-multiphysics-simulation agent with Bayesian optimization and PDF reports. It does not generate geometry and depends on an Onshape parametric model.
- **Synera:** the most complete multi-agent CAD-plus-simulation orchestration, but enterprise-only. Its NASA metrics are vendor claims.

### Inferences
- For the client's matrix, the main threats are SimScale, which has the simulation and agent depth, and Adam and Synera, which have the agentic CAD-plus-solver loop. Zoo leads on geometry generation.
- The rows where the client could be "leading" (verification/trust reporting, Pareto/multi-objective, comparison and history visualization, accessible built-in physics) are weak or absent across every product researched.

### Gaps
- The scores are subjective. They are not hands-on validated, and no independent benchmark exists.
- Synera's and SimScale's Pareto and surrogate capabilities were only lightly researched, because they belong to an adjacent scope. Other researchers may have better data.
