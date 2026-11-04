# CAD Incumbents: Generative Design, Topology Optimization and AI (state as of Nov 2026)

Scope: Autodesk Fusion, nTop, Altair/Siemens (Inspire -> Simcenter Inspire, HyperWorks, physicsAI), PTC Creo, PTC Onshape, Dassault SOLIDWORKS / 3DEXPERIENCE. Research date 2026-11-03. Some vendor pages (autodesk.com blog) returned HTTP 403 and could not be read directly. Several 2026 claims come only from secondary sources, including blogs by Leo AI (getleo.ai), which is itself an AI-for-CAD competitor. Each such claim is flagged.

Legend used below: **[SHIPPED]** confirmed GA; **[BETA/LABS]** preview or early access; **[ANNOUNCED]** roadmap or research only; **[BG]** background knowledge from before 2026, not re-verified in this session (treat as likely-but-verify).

---

## Q1. Autodesk Fusion: what is shipped vs. announced?

### Takeaway
Fusion has the most mature cloud generative design among the incumbents: multi-outcome topology generation, manufacturing-method constraints and an outcome explorer, sold as a paid extension or per study. Its AI layer in 2026 is the Autodesk Assistant (text-to-command, GA around mid-2026 per secondary sources) plus a local Fusion MCP server. True text-to-editable-CAD ("neural CAD") and Project Bernini are still announced or research, not shipped.

### Cited Findings
- Autodesk's 2026 Fusion roadmap says it is "working toward" neural CAD experiences that turn natural-language prompts into editable design geometry. This is a roadmap item. — [Fusion Roadmap 2026](https://www.autodesk.com/products/fusion-360/blog/fusion-roadmap-2026/) (page 403 to fetcher; content via search summary)
- DE247 (2025) reported that neural CAD "will be commercially available at some point in the future" in Fusion and Forma, and would be the first text-to-CAD in a major CAD suite. — [DE247: Autodesk doubles down on AI](https://www.digitalengineering247.com/article/autodesk-doubles-down-on-ai)
- Autodesk Assistant was reported active in Fusion, Inventor, Moldflow and Vault (Engineering.com summary, April 2026). It offers text-to-command, manufacturing support and onboarding in Fusion. — [Autodesk Assistant enters a new era](https://www.autodesk.com/products/fusion-360/blog/autodesk-assistant-enters-a-new-era/); [Pulse summary: Fusion MCP servers](https://www.pulse.bot/ai/news/autodesk-announces-fusion-mcp-servers-and-more-ai-updates-7c41cbec-4130-4345-a31e-929e65cfacec/)
- A Fusion MCP server runs locally and enables AI-driven command automation (external LLM agents can drive Fusion commands). AI rendering gives 20 free images per month to Fusion subscribers. — [Pulse summary](https://www.pulse.bot/ai/news/autodesk-announces-fusion-mcp-servers-and-more-ai-updates-7c41cbec-4130-4345-a31e-929e65cfacec/)
- UNVERIFIED: a Japanese blog (Oct 2026) says Assistant moved from Tech Preview to GA in July 2026, with monthly token limits on Fusion actions. — [issoh.co.jp](https://www.issoh.co.jp/md/tech/8616)
- AU 2026 class descriptions say the Fusion AI Assistant "can translate text into CAD". These are session abstracts, not release notes. — [AU 2026 class](https://www.autodesk.com/autodesk-university/class/Beyond-Automation-AI-Collaboration-with-Autodesk-Assistant-2026)
- Project Bernini (generative 3D shapes from text, image or point cloud) is still described as research and "not commercially available". An executive said Bernini output "can then be converted into Fusion". — [Autodesk Research: Project Bernini](https://www.research.autodesk.com/projects/project-bernini/); [Autodesk News](https://adsknews.autodesk.com/en/news/research-project-bernini/); [Jon Peddie: Bernini grows legs](https://www.jonpeddie.com/news/bernini-grows-legs/)
- Generative design pricing: a flat option of about $200/month or $1,600/year for the extension has been reported. Pay-as-you-go has been reported at about 33 cloud credits per study at roughly $1 per credit. Earlier pricing needed 125 credits (25 to solve, 100 to download). Confirm against the official page. — [DE247: Autodesk modifies generative design extension pricing](https://www.digitalengineering247.com/article/autodesk-modifies-generative-design-extension-pricing/news); [DEVELOP3D: Fusion 360 changes pricing](https://develop3d.com/cad/fusion-360-changes-pricing-access-for-generative-design-tools/); [cognitivefuture.ai pricing guide (Sep 2026)](https://cognitivefuture.ai/ai-cad-software-pricing/)
- Reviewers note that generative output "often requires significant manual cleanup and re-modeling" before production. — [cognitivefuture.ai](https://cognitivefuture.ai/ai-cad-software-pricing/)
- [BG] Generative design workflow (not re-verified this session):
  - Inputs: preserve, obstacle and starting geometry; loads and constraints across multiple load cases; multiple materials; objectives (minimize mass or maximize stiffness) with safety-factor or displacement limits; a manufacturing method (unrestricted/additive, 2.5-axis, 3-axis and 5-axis milling, 2-axis cutting, die casting).
  - Solving: cloud solve, which yields many outcomes.
  - Outcome explorer: thumbnails, filter, a scatter plot of (for example) mass vs. max von Mises stress or factor of safety, side-by-side comparison, and iteration animation.
  - Output: mesh plus convertible T-spline/B-rep (and, for milling or 2.5-axis, more prismatic B-rep features).
  - Fusion Simulation also covers static stress, modal, thermal, thermal stress, buckling, nonlinear, event simulation, and electronics cooling/CFD-lite.
  - API: Python/C++ Fusion API.
  - Access: free for education and personal use (limited), plus a startup program.

### Inferences
- Fusion is the closest incumbent analogue to "describe -> optimized prototype -> simulated", but the chain is not closed. Generative studies are set up manually: an LLM or the Assistant does not author loads, constraints or objectives from natural language, as far as any source shows. Text-to-geometry is not GA.
- The MCP server is strategically important. It lets third-party agents (including an Attome-like agent) drive Fusion, so Fusion could become a backend instead of a competitor.

### Gaps
- Could not read autodesk.com blog pages (403), so the exact GA status, token limits and capabilities of Assistant in Fusion as of Nov 2026 are not confirmed from primary sources.
- No primary source on whether the Assistant can set up generative design or simulation studies.
- Current official generative design extension price not confirmed. "AutoConstrain" naming not verified: Autodesk's AI sketch auto-constrain exists in [BG] but has no 2026 citation here.

---

## Q2. nTop (nTopology)

### Takeaway
nTop is the leader in implicit, field-driven geometry, including lattices, TPMS and field-driven thickness. It has built-in topology optimization, FE simulation and parameter optimization, and an expanding thermal/CFD capability after it acquired cloudfluid. There is no evidence of a shipped LLM assistant or text-to-design feature. Its automation story is programmatic (nTop Automate/CLI, nTop Core SDK), not conversational.

### Cited Findings
- nTop 5.0 introduced a new implicit modeling kernel, and all later releases build on it. nTop said better CAD-body export and meshing were still in development at the time. — [nTop 5.0 New Implicit Modeling Kernel](https://support.ntop.com/hc/en-us/articles/26062971882131); [nTop docs](https://docs.ntop.com/help-articles/whats-new/backwards-compatibility-notices/ntop-5-0-new-implicit-modeling-kernel)
- nTop Core is a developer library that lets partners work directly with implicit bodies without meshing. — [nTop support](https://support.ntop.com/hc/en-us/articles/26062971882131)
- The nTop 5.0 meshing update changed simulation and topology optimization workflows. The topology optimization block converges to results similar to 4.26 with fewer iterations. — [nTop 5.0 Meshing updates for Simulation & Optimization](https://support.ntop.com/hc/en-us/articles/30049593128211)
- nTop 5.18 improved the topology optimization block, letting it run without an optimization constraint list. — [nTop 5.18 What's New](https://support.ntop.com/hc/en-us/articles/39473214985107-nTop-5-18-What-s-New)
- The "What's New" index lists releases up to 5.54 (cadence of roughly frequent minor releases). — [nTop What's New](https://support.ntop.com/hc/en-us/sections/360011659053-What-s-New)
- UNVERIFIED (forum post): nTop 5.38.3 (Jan 2026) added conjugate heat transfer analysis, a Streamline block, and robustness updates to meshing and parameter optimization. — [forum post](https://forums.srcds.com/viewtopic/305566)
- nTop acquired cloudfluid to deepen CFD simulation, including lattice Boltzmann methods. — [3DPrint.com](https://3dprint.com/316451/ntop-acquires-cloudfluid-further-integrates-cfd-simulation/)
- nTop Automate is a programmatic environment for running nTop workflows from custom code. — [SOTA2 listing](https://www.sota2.com/products/ntopology-inc-ntop-automate)
- Pricing is not published (contact sales). A Capterra listing shows a "starting $1,500/user/year", which is unverified and probably outdated. — [G2 nTop pricing](https://www.g2.com/products/ntop/pricing); [Capterra](https://www.capterra.com/p/240777/nTopology/)
- Siemens PLM Components 2026 conference: an nTop Technical Fellow presented on implicit modeling, where geometry is defined by continuous functions rather than B-rep vertices, edges and faces. — [CIMdata commentary June 2026](https://www.cimdata.com/en/resources/complimentary-reports-research/commentaries/item/30193-siemens-plm-components-2026-commentary)
- [BG] Further capabilities (not re-verified this session):
  - Visual node/block programming, where notebooks are parametric and re-executable.
  - Field-driven design, where simulation results drive lattice or thickness.
  - Built-in linear static and modal FE.
  - Topology optimization with manufacturing constraints (overhang, symmetry, min size).
  - Design-of-experiments and parameter optimization.
  - Exports: STL/3MF/mesh, CLI/slices for AM, and STEP/Parasolid (B-rep export of implicits is limited or approximate).
  - Free nTop for education/academic and startup programs.
  - An NVIDIA collaboration on GPU-accelerated implicits and ML surrogates for real-time design exploration.

### Inferences
- nTop is the strongest "geometry generation + DfAM" incumbent, and has real closed-loop optimization through scripted notebooks plus parameter optimization. It lacks NL intent and LLM agents. Its output is natively implicit/mesh, so editable B-rep handoff to mainstream CAD is a known weakness.

### Gaps
- No primary source found for any 2026 nTop AI or LLM feature. I found no evidence it exists; treat it as absent unless shown otherwise.
- Contents of nTop 5.4x–5.54 not reviewed; exact thermal/CFD GA status is unverified.

---

## Q3. Altair (now Siemens): Inspire / Simcenter Inspire, HyperWorks 2026, physicsAI, DesignAI

### Takeaway
This is the strongest incumbent on simulation-driven design plus ML surrogates. Inspire gives topology, topography and gauge optimization with OptiStruct and SimSolid solvers. physicsAI (geometric deep learning) now trains and predicts directly inside Simcenter Inspire 2026.1, and HyperWorks 2026 adds GPU ROMs and browser-deployed physics-AI models. There is no evidence of an LLM agent that sets up and runs optimization from natural language.

### Cited Findings
- HyperWorks 2026 was released Dec 2025 as Altair joined Siemens Digital Industries Software. Features:
  - Geometric deep learning and generative algorithms for near-real-time predictions.
  - GPU-accelerated reduced-order modeling.
  - Physics-based AI models deployable in secure browser-based environments, "up to 1,000x faster than traditional solver simulations" (vendor claim).
  - Implicit modeling, direct surface editing, and side-by-side design comparisons.
  - Unified multiphysics: thermal-fluid and EM-structural coupling.
  - Expanded Python/API support, no-code workflow tools, and cloud integration.
  
  — [DE247 HyperWorks 2026](https://www.digitalengineering247.com/article/altair-hyperworks-2026-new-ai-hpc-and-multiphysics-capabilities); [DEVELOP3D](https://develop3d.com/simulation/hyperworks-ai-update-delivers-simulation-at-scale/); [Siemens news](https://news.siemens.com/de-ch/altair-hyperworks-2026/)
- FlightStream (HyperWorks) is cited by JetZero as a faster alternative to high-fidelity CFD for early design. — [DE247](https://www.digitalengineering247.com/article/altair-hyperworks-2026-new-ai-hpc-and-multiphysics-capabilities)
- Rebrand: with the Simcenter June 2026 release, Siemens brought Altair products into the Simcenter family, and Inspire became Simcenter Inspire. — [Siemens Simcenter Inspire](https://www.siemens.com/en-us/products/simcenter/mechanical-simulation/inspire/)
- Simcenter Inspire 2026.1:
  - Users can train PhysicsAI models from existing simulation data and predict results inside Inspire. Training data can come from Simcenter OptiStruct and SimSolid structural analyses, and from Cast, Mold, Form and Extrude workflows.
  - It adds a unified multi-profile UI.
  
  — [What's new in Simcenter Inspire 2026.1](https://blogs.sw.siemens.com/simcenter/whats-new-in-simcenter-inspire-2026-1/)
- Inspire supports topology, topography and gauge optimization, and its structural solvers are SimSolid and OptiStruct. — [Altair Inspire help: structure](https://help.altair.com/inspire/en_us/topics/inspire/structure/structure_st_c.htm)
- Inspire includes implicit modeling. — [Inspire implicit overview](https://help.altair.com/inspire/en_us/topics/implicit/implicit_overview_r.htm)
- No source found for a product or feature named "DesignAI", or for agentic/LLM features in HyperWorks 2026. — (search: [arcweb HyperWorks 2026](https://www.arcweb.com/blog/altair-hyperworks-2026-design-simulation-scale-ai))
- [BG] Further capabilities (not re-verified this session):
  - Inspire topology optimization takes design and non-design spaces, multiple load cases, and objectives (max stiffness, min mass) with stress, displacement and frequency constraints.
  - Shape controls cover symmetry, cyclic repetition, draw direction (single/split for casting), extrusion, overhang (AM), and min/max thickness.
  - PolyNURBS can wrap results into editable smooth B-rep. Lattice output and Inspire Print3D/Cast/Mold/Form/Extrude manufacturing simulation are also available.
  - HyperStudy covers DOE, multi-objective optimization (GA, Pareto) and response surfaces.
  - Licensing uses Altair Units (token-based), with free student editions and a startup program.

### Inferences
- Altair/Siemens best match the "physics verification + surrogate" half of the target platform. Topology optimization, multi-load-case FEA, manufacturing simulation, ML surrogates trained on the customer's own data, and Pareto exploration via HyperStudy all exist. The pieces are spread across tools and are expert-driven, with no NL intent and no autonomous loop.

### Gaps
- No confirmation of any LLM copilot in Inspire or HyperWorks as of Nov 2026. Siemens has Industrial Copilot in NX/Teamcenter, but its integration with Simcenter Inspire is unverified.
- Inspire 2026 release notes PDF could not be parsed.
- Post-acquisition pricing and licensing (Altair Units vs. Siemens) unverified.

---

## Q4. PTC Creo (Creo 12 / Creo 13 / Creo+)

### Takeaway
Creo has mature, shipped topology optimization: GTO runs local and in the history tree, and GDX is cloud generative with multiple outcomes. Creo 12 added thermal physics to generative design. Creo 13 introduced a three-tier AI layer: Advise (GA chat on documentation), Assist (beta, model-aware edits) and Automate (alpha, natural-language objective -> proposed geometry with human review). The tier details come mainly from a competitor's blog.

### Cited Findings
- Creo 12 (June 2025) added thermal physics to generative design (optimizing for heat dissipation alongside weight and stiffness) among 250+ enhancements. — [PTC: Creo 12 launch](https://www.ptc.com/news/2025/ptc-launches-creo-12); [Engineering.com](https://www.engineering.com/ptc-launches-creo-12-with-new-design-simulation-tools/)
- Creo has a Generative Topology Optimization (GTO) extension and a cloud-based Generative Design Extension (GDX). — [PTC generative design page](https://www.ptc.com/en/technologies/cad/generative-design); [PTC Creo 13 help: Perform Generative Design Topology Optimization](https://support.ptc.com/help/creo/creo_pma/r13/usascii//generative_design/perform_topology_optmization.html)
- GTO runs inside the parametric history tree; GDX is cloud-based; Creo Simulate topology optimization is the local legacy path. — [TriStar: Creo AI](https://www.tristar.com/blog/creo-ai/); [getleo.ai GDX guide](https://www.getleo.ai/blog/generative-design-creo-gdx)
- Creo AI Assistant was introduced in beta with Creo+ 13.0 and gives in-environment troubleshooting for specific error messages. — [TriStar (PTC reseller), Mar 2026](https://www.tristar.com/blog/creo-ai/)
- Creo 13 AI tiers, per Leo AI (a competitor's blog, dated Sep 2026, so lower confidence):
  - Advise [SHIPPED]: chat over documentation that finds commands and diagnoses regeneration failures. It makes no design decisions.
  - Assist [BETA]: reads the active model's geometry, reports dimensions and radii, and applies parametric changes across related features. Broader availability is expected "late autumn 2026".
  - Automate [ALPHA]: takes a plain-language objective (for example a target flow ratio or acoustic constraint) and proposes modified geometry in a sandbox with a before/after comparison that requires human review. Release is "targeted before end of 2026".
  - All tiers are limited to the open model and Creo documentation, with no PDM/ERP access, and PTC has published no accuracy figures.
  
  — [getleo.ai: What PTC's built-in AI does inside Creo](https://www.getleo.ai/blog/creo-built-in-ai)
- Creo+ 13.0 claims 15% faster regeneration and up to 10x faster interference checks. — [TriStar](https://www.tristar.com/blog/creo-ai/)
- [BG] Further capabilities (not re-verified this session):
  - GDX (from Frustum) generates outcomes with manufacturing constraints: additive, casting/draw, and milling.
  - Outputs are converted to smooth B-rep/surface geometry in-model.
  - Creo Simulation Live (Ansys-powered, GPU real-time) covers structural, modal, thermal and fluid. Creo Ansys Simulation is the full solver.
  - API: Creo Toolkit / J-Link / Web.Link, Creo+ SaaS.
  - Free Creo student edition. GDX and GTO are paid extensions.

### Inferences
- If Automate ships as described, Creo would be the first incumbent with an NL-objective -> geometry-change loop inside parametric CAD. It is human-gated by design and alpha-stage. Today, Creo's physics-based generative design is strong but manual-setup.

### Gaps
- No PTC primary source found for the Advise/Assist/Automate tiers or their Nov 2026 status.
- Whether Automate uses physics simulation to verify proposals is unknown.
- GDX pricing unknown.

---

## Q5. Onshape (PTC)

### Takeaway
Onshape's shipped AI is AI Advisor, a documentation-grounded help chatbot that cannot see the user's model. In 2026, Onshape Labs added a FeatureScript MCP Server that lets external LLMs generate custom FeatureScript features. Onshape has no generative design or topology optimization. Its native simulation (linear static/modal, [BG]) is basic.

### Cited Findings
- AI Advisor was embedded in the Onshape workspace in Oct 2025, giving step-by-step recommendations, troubleshooting and best practices. — [DE247](https://www.digitalengineering247.com/article/onshape-ai-advisor-now-embedded-directly-in-the-design-environment); [Onshape blog](https://www.onshape.com/en/blog/ai-advisor-artificial-intelligence-cad-engineering-software); [DEVELOP3D](https://develop3d.com/cad/onshape-release-ai-advisor-for-real-time-guidance/)
- AI Advisor is grounded in Onshape documentation and the Learning Center and "does not have access to your data". Admins can toggle it, and it is off by default for Enterprise. It is not available on Onshape Government. — [Onshape AI Advisor guide](https://onshape.com/en/blog/ai-advisor-guide-cad-pdm)
- AI Advisor is built on Amazon Bedrock and answers questions about Onshape, not about the user's model. — [getleo.ai Onshape built-in AI](https://www.getleo.ai/blog/onshape-built-in-ai) (competitor blog)
- Onshape Labs (early access) opened July 2026. A FeatureScript MCP Server became generally available within Labs in Aug 2026, turning natural-language instructions into reusable custom features (code). PTC says "AI-generated outputs and automation still require engineering review." — [getleo.ai](https://www.getleo.ai/blog/onshape-built-in-ai); [Onshape forum: recent improvements](https://forum.onshape.com/categories/recent-improvements)
- PTC has said it is "exploring" automated geometry creation and intelligent design optimization. This is not shipped. — [arcweb](https://www.arcweb.com/node/161778); [Nasdaq/Zacks](https://www.nasdaq.com/articles/ptc-boosts-cad-innovation-latest-onshape-ai-advisor-launch)
- Onshape release notes mention a simulation release alongside AI Advisor (configurable variable studios + simulation). — [Onshape what's new](https://www.onshape.com/es/resource-center/what-is-new/ai-advisor-configurable-variable-studios-simulation)
- [BG] Further capabilities (not re-verified this session):
  - Onshape Simulation, included in Professional/Enterprise, covers linear static and modal analysis in the assembly context, using a meshless or real-time solver.
  - A free plan exists for hobbyists (public docs) and education. Standard is about $1,500/yr and Professional about $2,500/yr.
  - The REST API and FeatureScript are strong.

### Inferences
- Onshape is weak on generative and physics but strong on cloud, API, collaboration and versioning. The FeatureScript MCP route means LLMs can already generate parametric features in Onshape, a plausible substrate for third-party agents.

### Gaps
- No confirmation of Onshape thermal, buckling or CFD. No generative design found.
- Onshape Labs feature list beyond the MCP server is unverified.

---

## Q6. Dassault SOLIDWORKS / 3DEXPERIENCE

### Takeaway
SOLIDWORKS 2026 ships AI "virtual companions" AURA (conversational, can answer about and act on the model, such as show/hide or changing materials) and LEO (AI drawing creation), but they are in SOLIDWORKS Labs (Beta). Generative/topology work lives in SOLIDWORKS Simulation (topology study) and 3DEXPERIENCE/CATIA roles. Text-to-3D and image-to-3D in AURA are roadmap items.

### Cited Findings
- LEO and AURA are available in SOLIDWORKS 2026 FD01 via the new SOLIDWORKS Labs (Beta) tab. — [SOLIDWORKS Live: AI explained LEO/AURA](https://live.solidworks.com/demand/solidworks-ai-explained-leo-aura-solidworks-2026-solidworks-labs-demo?page=1); [SOLIDWORKS 2026 FD01 session](https://live.solidworks.com/demand/solidworks-2026-fd01-ai-powered-3d-cad-software-leo-aura-performance-upgrades?page=15)
- AURA learns company processes, specs and regulations and answers natural-language questions. It can summarize and translate, answer questions about a model, and act on it (show/hide parts, change materials). Future AI-driven 3D model generation and change requests were presented as roadmap items. — [Monoist (3DEXPERIENCE World Japan 2025)](https://monoist.itmedia.co.jp/mn/spv/2512/02/news020_2.html); [Monoist (3DEXPERIENCE World 2025)](https://monoist.itmedia.co.jp/mn/articles/2502/27/news126_2.html)
- LEO powers "Drawing Creation (Beta)" to automate initial drawing setup. — [GoEngineer: SOLIDWORKS 2026 in the classroom](https://www.goengineer.com/en-us/blog/solidworks-2026-in-the-classroom-how-ai-driven-design-frees-up-your-office-hours)
- xDesign showed generative "stacked dimension" functionality that auto-constrains sketches at 3DEXPERIENCE World 2025. An "Aura – 3D Model from Picture" concept appears roadmap-only. — [ohmycad](https://ohmycad.com/en/?p=61577); [seacadtech](https://vn.seacadtech.com/blog/horizon-of-ai-in-solidworks-and-3dexperience/)
- Sources conflict on AURA's status: "available since January in SOLIDWORKS Design 2026" vs. Labs (Beta). — [ohmycad](https://ohmycad.com/en/?p=61577) vs. [SOLIDWORKS Live](https://live.solidworks.com/upcoming/solidworks-ai-explained-leo-aura-solidworks-2026-solidworks-labs-demo)
- [BG] Further capabilities (not re-verified this session):
  - SOLIDWORKS Simulation Professional/Premium includes a Topology Study with stiffness/mass objectives and manufacturing controls (preserved regions, symmetry, de-mold direction, thickness). Output is a mesh to be re-modeled.
  - 3DEXPERIENCE has the "Function Driven Generative Designer" (CATIA/xGenerative) role, which produces subdivision-surface editable output, plus SIMULIA structural, thermal and CFD roles.
  - "Fabrikator" was not verified in any source.
  - Free and low-cost options: SOLIDWORKS for Makers (~$48/yr), student editions, and a startup program.

### Inferences
- Dassault is ahead in "LLM acts on the model" (AURA show/hide and material changes), but it is beta. Generative design is split across expensive platform roles, with no NL -> optimized-design loop.

### Gaps
- No 2026 primary source on functional generative design in xDesign or 3DEXPERIENCE. "Fabrikator" was not found.
- AURA GA status conflicting.

---

## Q7. Suggested competitor-matrix scores (0-100)

Scale: 0 absent, 1-30 stub, 31-60 partial, 61-80 solid, 81-100 leading. Scores are my judgment from the findings above. [BG] items influenced the scores and should be verified.

| Capability | Fusion | nTop | Altair/Siemens Inspire+HyperWorks | Creo | Onshape | SOLIDWORKS/3DX |
|---|---|---|---|---|---|---|
| NL intent input | 30 | 0 | 5 | 35 | 15 | 30 |
| Geometry generation | 80 | 90 | 80 | 75 | 15 | 55 |
| Structural FEA | 75 | 65 | 90 | 75 | 40 | 80 |
| Thermal | 60 | 50 | 75 | 65 | 5 | 65 |
| CFD | 30 | 45 | 70 | 45 | 0 | 55 |
| Automatic optimization | 75 | 75 | 85 | 70 | 0 | 55 |
| Multi-objective / Pareto | 60 | 55 | 80 | 45 | 0 | 40 |
| AI surrogate | 15 | 30 | 85 | 20 | 0 | 15 |
| LLM / agent autonomy | 35 | 5 | 10 | 35 | 25 | 35 |
| Closed-loop autonomy | 25 | 45 | 40 | 25 | 0 | 15 |
| Verification / trust reporting | 55 | 45 | 70 | 50 | 25 | 55 |
| 3D result visualization | 75 | 70 | 85 | 70 | 40 | 75 |
| Design comparison / history viz | 75 | 50 | 70 | 50 | 55 | 45 |
| Manufacturability | 80 | 75 | 85 | 70 | 10 | 55 |
| Export / interop | 70 | 55 | 70 | 70 | 70 | 70 |
| API / automation | 75 | 75 | 80 | 65 | 80 | 60 |
| Cost / accessibility | 75 | 30 | 45 | 40 | 85 | 50 |

One-line justifications:
- **Fusion**
  - NL 30: Assistant does text-to-command; text-to-CAD (neural CAD) is announced, not GA.
  - GeoGen 80: multi-outcome cloud generative design with B-rep conversion [BG].
  - FEA 75: static, modal, buckling and nonlinear in Fusion Simulation [BG].
  - Thermal 60: thermal and thermal-stress [BG].
  - CFD 30: electronics cooling only [BG].
  - AutoOpt 75: cloud generative design produces many outcomes per study.
  - Pareto 60: the outcome explorer scatter plot filters trade-offs but has no formal Pareto optimizer [BG].
  - Surrogate 15: no confirmed ML surrogate in Fusion.
  - LLM 35: Assistant plus local MCP server can drive commands.
  - Closed-loop 25: study setup is manual.
  - Trust 55: safety factor and stress per outcome [BG].
  - 3D viz 75 [BG].
  - Compare 75: the outcome explorer [BG].
  - Mfg 80: additive, 2.5/3/5-axis milling, 2-axis cutting and die casting [BG].
  - Export 70.
  - API 75: Python/C++ API plus MCP.
  - Cost 75: cheap subscription, a roughly $1,600/yr extension, and free education and personal tiers.
- **nTop**
  - NL 0: no LLM found.
  - GeoGen 90: the industry-leading implicit/lattice/field-driven engine.
  - FEA 65: built-in FE and topology optimization.
  - Thermal 50: conjugate heat transfer reported (unverified).
  - CFD 45: cloudfluid LBM acquisition.
  - AutoOpt 75: parameter optimization and topology optimization in re-executable notebooks.
  - Pareto 55: DOE/parameter sweeps [BG].
  - Surrogate 30: NVIDIA/ML work [BG, unverified].
  - LLM 5: none.
  - Closed-loop 45: scripted notebooks (Automate) can run full design-sim-optimize loops headless.
  - Trust 45.
  - 3D viz 70: field visualization.
  - Compare 50.
  - Mfg 75: AM-centric (overhang, build prep).
  - Export 55: implicit-to-B-rep export is weak.
  - API 75: Automate, Core SDK, CLI.
  - Cost 30: unpublished enterprise pricing; education program [BG].
- **Altair/Siemens**
  - NL 5: no LLM found.
  - GeoGen 80: topology, topography and gauge optimization plus PolyNURBS and implicit.
  - FEA 90: OptiStruct and SimSolid.
  - Thermal 75: multiphysics thermal-fluid coupling.
  - CFD 70: AcuSolve, FlightStream and others in HyperWorks.
  - AutoOpt 85: OptiStruct optimization.
  - Pareto 80: HyperStudy multi-objective [BG].
  - Surrogate 85: physicsAI trains and predicts in Inspire 2026.1, plus GPU ROM and browser deployment.
  - LLM 10: none found.
  - Closed-loop 40: HyperStudy and Python can automate DOE/optimization but are expert-configured.
  - Trust 70: solver-grade results and reports.
  - 3D viz 85.
  - Compare 70: side-by-side comparisons in HyperWorks 2026.
  - Mfg 85: casting draw, extrusion, AM overhang, and Cast/Mold/Form/Extrude simulation.
  - Export 70.
  - API 80: Python/API and no-code workflows.
  - Cost 45: unit-based licensing, student editions [BG].
- **Creo**
  - NL 35: Advise is GA; Automate's NL-objective -> geometry is alpha.
  - GeoGen 75: GTO in-tree plus cloud GDX.
  - FEA 75: Simulate, Simulation Live and Ansys [BG].
  - Thermal 65: thermal in generative design (Creo 12).
  - CFD 45: Creo Flow Analysis and Simulation Live fluids [BG].
  - AutoOpt 70.
  - Pareto 45: GDX gives multiple outcomes [BG].
  - Surrogate 20: GPU real-time is not an ML surrogate.
  - LLM 35: Assist (beta) edits parameters.
  - Closed-loop 25: Automate is sandboxed with human review and alpha.
  - Trust 50: before/after review.
  - 3D viz 70.
  - Compare 50.
  - Mfg 70: AM, casting and milling constraints [BG].
  - Export 70.
  - API 65: Toolkit/J-Link [BG].
  - Cost 40: expensive extensions; free student edition [BG].
- **Onshape**
  - NL 15: Advisor is documentation Q&A only; the MCP server writes FeatureScript.
  - GeoGen 15: only LLM-written FeatureScript features.
  - FEA 40: linear static/modal [BG].
  - Thermal 5 and CFD 0: none found.
  - AutoOpt and Pareto 0.
  - Surrogate 0.
  - LLM 25: FeatureScript MCP in Labs.
  - Closed-loop 0.
  - Trust 25.
  - 3D viz 40.
  - Compare 55: versioning, branching and compare [BG].
  - Mfg 10.
  - Export 70.
  - API 80: REST and FeatureScript.
  - Cost 85: free plan and education [BG].
- **SOLIDWORKS/3DX**
  - NL 30: AURA NL chat (beta).
  - GeoGen 55: Simulation topology study gives mesh output; 3DX generative roles cost extra [BG].
  - FEA 80: Simulation and SIMULIA.
  - Thermal 65.
  - CFD 55: Flow Simulation [BG].
  - AutoOpt 55.
  - Pareto 40.
  - Surrogate 15.
  - LLM 35: AURA can act on the model (show/hide, materials) in beta.
  - Closed-loop 15.
  - Trust 55.
  - 3D viz 75.
  - Compare 45.
  - Mfg 55: topology-study manufacturing controls [BG].
  - Export 70.
  - API 60: SW API/VBA/C# [BG].
  - Cost 50: Makers and student editions are cheap; Premium and 3DX roles are expensive [BG].

### Inferences
- No incumbent ships the full chain: NL intent -> automatic setup of loads, constraints and objectives -> geometry generation -> physics verification -> autonomous iteration -> Pareto/report.
- Each vendor leads on a different piece:
  - Generative and manufacturing UX: Fusion.
  - Implicit geometry: nTop.
  - Solver, surrogate and multi-objective: Altair/Siemens.
  - Closest announced NL-objective -> geometry change: Creo Automate (alpha, human-gated).
  - LLM acting on the model: SOLIDWORKS AURA (beta).
- Fusion's MCP server and Onshape's FeatureScript MCP make incumbents potential tool backends for agentic platforms, as well as competitors.

### Gaps
- Most [BG] items need primary-source re-verification before publication.
- Late-2026 events (AU 2026 in particular) may have announced neural CAD or Assistant expansions that no source here confirms.
- Pricing figures for generative extensions (Fusion, Creo GDX, nTop) are either secondary or undisclosed.
