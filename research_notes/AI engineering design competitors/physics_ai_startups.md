# Physics-AI, Surrogate, Generative/Computational Design and "Agentic Engineering" Startups (state as of Nov 2026)

Scope: Neural Concept, PhysicsX, Monolith AI (now CoreWeave), NAVASTO (now Autodesk NavPack), Synera, ToffeeX, Diabatix (ColdStream), Cognitive Design Systems, Hyperganic, BeyondMath, Emmi AI (now Mistral), Inductiva, Pasteur Labs, Cadence/Future Facilities. Also found: Luminary Cloud, Navier, JuliaHub (Dyad), LEAP 71 (Noyron), Vinci (formerly Vinci4D).

How to read this: "SHIPPED" means a product or feature a primary or press source describes as available. "ANNOUNCED" means a planned or launched-but-limited-access feature. "MARKETING" means an unverified claim, such as a speed-up or accuracy figure. Research budget was limited, so many fine-grained UX details (viewer, UQ, export formats) could not be verified from primary docs. Those are listed under Gaps rather than guessed.

---

## 1. Current capabilities per company (inputs, geometry generation, physics, solver vs surrogate, optimization, agents, verification, visualization, manufacturability, integrations)

### Takeaway
Most of the field falls into three groups:
- **Surrogate / foundation-model predictors:** Neural Concept, PhysicsX, NavPack, Luminary, BeyondMath, Emmi, Monolith. These predict performance fast from geometry or data, and mostly need customer simulation data.
- **Solver-driven generative design tools:** ToffeeX, Diabatix ColdStream, Cognitive Design. These run real physics optimization and output manufacturable geometry, but have no natural-language input or LLM agents.
- **Agent orchestration layers:** Synera, Navier, JuliaHub Dyad, Mistral+Emmi (planned). These drive existing CAD/CAE tools.

No vendor found publicly ships the full loop from natural-language product intent to generated CAD to real FEA/thermal verification to Pareto visualization in a self-serve product. Neural Concept's "AI Design Copilot" (CES 2026) and Mistral/Emmi's planned "agents for engineers" come closest in stated ambition.

### Cited Findings

#### Neural Concept (Lausanne, EPFL spin-off)
- Platform "embeds deep learning directly into CAD and physics-based simulation workflows"; AI "understands geometry, physics and design intent"; demo shows F1 aerodynamics predicted in 0.3 s (SHIPPED surrogate; speed is MARKETING) — [SiliconANGLE, Dec 2025](https://siliconangle.com/2025/12/18/ai-aided-design-software-startup-neural-concept-raises-100m-accelerate-product-engineering/)
- Generative CAD feature was planned for early 2026 — [SiliconANGLE](https://siliconangle.com/2025/12/18/ai-aided-design-software-startup-neural-concept-raises-100m-accelerate-product-engineering/); [Neural Concept Series C coverage, Barchart/BusinessWire](https://www.barchart.com/story/news/36693852/neural-concept-closes-100m-funding-round-led-by-growth-equity-at-goldman-sachs-alternatives-to-scale-ai-native-engineering)
- "AI Design Copilot" launched Jan 2026 (CES 2026). It is described as integrating "spatial reasoning, physics awareness and CAD-ready geometry generation", "instant generation of CAD-ready models from high-level design ideas", and integration with NVIDIA Omniverse. It launched as a feature within the platform, with "broader access planned later this quarter" (ANNOUNCED/limited access; no follow-up confirming general availability found) — [ePartTrade, Jan 12 2026](https://epartrade.com/industry-news/ai-design-copilot-introduces-physicsaware-cadready-automation-for-engineers-a77e02fc)
- The company website now positions the platform as a "modular environment for human-agent collaboration across the full product development cycle". It lists customers MAHLE (+15% energy efficiency in one design cycle), GM (model "trained on data from 11 vehicle programs"), JLR (aero) and Leonardo. Deployment partners are AWS, Microsoft and NVIDIA. Website pages fetched gave no detail on inputs, UQ, visualization or API — [neuralconcept.com](https://www.neuralconcept.com/)
- Training-data dependency: the GM example shows models trained on the customer's historical program data — [neuralconcept.com](https://www.neuralconcept.com/)

#### PhysicsX (London; ex-F1 engineers)
- Platform uses neural operators that reuse results from past PDE solves. Models cover aerodynamic shape optimization of car parts and multiphysics (for example, chip current to heat). Other components: a "Data Factory" that generates design variations from 260 staff-curated baseline designs, a "Simulation Workbench" to organize simulation data, and dozens of templates. Used in more than 6 verticals, including semiconductor, industrial machinery and automotive — [SiliconANGLE, Jun 8 2026](https://siliconangle.com/2026/06/08/physicsx-reels-300m-speed-hardware-design-ai/)
- LGM-Aero ("Large Geometry Model") was trained on more than 25M meshes (over 10B vertices) and tens of thousands of CFD and FEA simulations generated with Siemens tools. It is claimed to be zero-shot for "flying shapes", predicting lift, drag, stability and structural stress in under a second. The CEO claims "the same level of accuracy" as numerical simulation, but no benchmarks are published (MARKETING) — [PhysicsX newsroom](https://www.physicsx.ai/newsroom/introducing-lgm-aero-genai-for-aero-engineering-and-airplane-showcase-application-for-aerostructures)
- Ai.rplane is a free public demonstrator (airplane.physicsx.ai). It generates novel aircraft shapes and runs a "generate, predict, optimize" workflow driven by user "preferences". LGM-Aero is offered on the enterprise platform (SHIPPED Dec 2024) — [PhysicsX newsroom](https://www.physicsx.ai/newsroom/introducing-lgm-aero-genai-for-aero-engineering-and-airplane-showcase-application-for-aerostructures); [Siemens press release](https://news.siemens.com/en-us/siemens-physicsx/)
- Planned Ai.rplane features (ANNOUNCED): cargo packaging, commercial powertrain selection, control mechanisms — [Siemens press release](https://news.siemens.com/en-us/siemens-physicsx/)
- No public evidence of LLM agents in the PhysicsX product was found in a 2026 search — [search via Siemens/DE247 coverage](https://www.digitalengineering247.com/article/siemens-and-physicsx-collaborate-to-build-deep-physics-simulation/news)

#### Monolith AI (London; acquired by CoreWeave)
- Website states "Monolith has joined CoreWeave" — [monolithai.com](https://www.monolithai.com/)
- Products: a notebook-style no-code/low-code AI platform for domain experts, Anomaly Detector (test data validation; claims more than 90% of known errors found on sample data), Next Test Recommender (claims up to 70% fewer tests), root-cause analysis and system calibration. Inputs are mainly test-rig channel data; simulation data is mentioned only in a customer quote. Focus is test and validation, not geometry generation — [monolithai.com](https://www.monolithai.com/)
- CoreWeave plans to pair Monolith's simulation and test-driven ML with its AI cloud for manufacturers. Deal dates vary across aggregators (Oct 2025 announcement; completion reported Nov 2025) — [Dealroom/EIT news](https://eit.dealroom.co/news/feed/coreweave-acquires-monolith-ai-platform); [MergerLinks](https://app.mergerlinks.com/transactions/2025-10-06-monolith-ai-limited)

#### NAVASTO (Berlin; now Autodesk NavPack)
- navasto.de now 301-redirects to autodesk.com/products/navpack (fetched Nov 2026) — [autodesk.com/products/navpack](https://www.autodesk.com/products/navpack)
- Autodesk announced the acquisition at Autodesk University (Oct 2024). NAVASTO tech enables early aero (drag) prediction inside Alias for automotive exteriors. NavPack lets customers train custom physics-AI models on their own simulation datasets, with "predictive and generative" features, evaluating "hundreds of configurations in minutes". It runs on Windows and Linux under network-subscription licensing — [advancedmanufacturing.org](https://www.advancedmanufacturing.org/news-desk/autodesk-bets-big-on-ai-expects-manufacturers-to-do-the-same/article_26d3b88a-8d62-11ef-af0a-2fae2713cc2b.html); [Autodesk Design Studio blog, Nov 2025](https://blogs.autodesk.com/design-studio/2025/11/19/how-navpack-brings-engineering-intelligence-into-alias-the-design-studio-aif-2025/); [Autodesk NavPack](https://www.autodesk.com/products/navpack)
- NVIDIA PhysicsNeMo/Warp integration is reported only by a secondary aggregator (unverified) — [welcome.ai](https://welcome.ai/content/autodesk-and-nvidia-reshape-automotive-design-with-ai-integration.md)
- Earlier SimScale partnership integrated NAVASTO tools into SimScale — [CB Insights profile](https://www.cbinsights.com/company/navasto)

#### Synera (Bremen; "agentic engineering")
- Specialist AI agents run engineering processes end to end, with every step logged for review. Workflows are deterministic, auditable and run on-prem. Customers can "use the LLM of your choice". Agent types include:
  - Manufacturing agents: meshing, CAD defeaturing, fiber orientation, drop-test validation for injection molding.
  - Cost agents: catalog search, design and simulate parts, cost estimate and bill of processes.
  - Sales/RFQ agents.
  - Multi-agent supervisors: NASA uses a supervisor that coordinates optical, mechanical layout, harnessing, structural validation and reporting agents.
  - Applications: Automated FE Analysis, Design-to-Cost, AM, injection molding and casting — [synera.ai](https://www.synera.ai/)
- 80+ CAD/CAE/PLM/ERP integrations (some outlets say 75+), named as including Altair, Autodesk, Hexagon, PTC and Siemens — [synera.ai](https://www.synera.ai/); [The Next Web](https://thenextweb.com/news/synera-40m-series-b-agentic-ai-engineering); [SiliconANGLE Apr 2026](https://siliconangle.com/2026/04/14/german-startup-synera-lands-40-million-automate-engineering-workflows-ai-agents/)
- Claimed results (MARKETING/case study): at NASA, one engineer runs 100+ design variants in an hour; at IMS Gear, RFQ turnaround fell from weeks to about 10 min — [synera.ai](https://www.synera.ai/)
- Synera does not ship its own physics solver or surrogate. It orchestrates the customer's existing tools (inference from integration-centric description) — [SiliconANGLE](https://siliconangle.com/2026/04/14/german-startup-synera-lands-40-million-automate-engineering-workflows-ai-agents/)

#### ToffeeX (London; Imperial College spin-out)
- "Physics generative AI engine" in which "Navier–Stokes and heat transfer equations drive the geometry". It does topology optimization for thermal-fluid parts: cold plates, heat exchangers, heat sinks, manifolds, data-center cooling. The user defines domain, boundary conditions, goals and objectives. Exports are STEP and "production-ready" geometry, with AM (LPBF) referenced. Regional web app log-ins (app.us.toffeex.com) suggest a browser app. Structural analysis is not mentioned. Access is demo-only — [toffeex.com](https://www.toffeex.com/)
- Users and customers listed: Airbus, Rolls-Royce, EOS, Toyota, Hyundai, Westinghouse, Fraunhofer, Baker Hughes, Ricoh — [toffeex.com](https://www.toffeex.com/)

#### Diabatix ColdStream (Leuven)
- Cloud-based with no install and regional portals (EU/US/Asia). Physics: thermal and flow (temperature, thermal resistance, radiation, cooling capacity), with a 3D CFD solver plus 1D thermal network. Generative concept generation covers pins, fins and gyroids, plus topology optimization and exploration of "hundreds of designs". Manufacturing methods: 3D printing, die casting, CNC milling, sheet metal. A developer API is available. Inputs are imported geometry, thermal loads and objectives. Entry points are "Try Now" and "Book a demo", with no public prices — [diabatix.com/coldstream](https://www.diabatix.com/coldstream)
- Claimed +60% heat transfer efficiency on a heatsink case (MARKETING with linked paper) — [diabatix.com/coldstream](https://www.diabatix.com/coldstream)

#### Cognitive Design Systems (CDS; Cognitive Design platform)
- Implicit modeling engine with "AI-driven multi-objective generative optimization", parametric workflows, topology optimization and DoE. Requirements-driven exploration compares variants on cost, weight, CO2 and lead time. Integrated FEA, with the solver unnamed. Manufacturability covers AM, casting, machining, molding and forging. Exports are STEP, STL, IGES and native CAD, used with SolidWorks, CATIA, NX and Creo. Deployment is on-prem or private cloud. "Genesis" is not mentioned on the site — [cognitive-design-systems.com](https://www.cognitive-design-systems.com/)
- Customer logos: Valeo, Leonardo, Naval Group, DGA, French Air Force, Adidas, Toyota, Mitsubishi, MBDA, Nissan, Safran, Liebherr — [cognitive-design-systems.com](https://www.cognitive-design-systems.com/)

#### Hyperganic (Munich)
- Co-founders Lin Kayser (CEO) and Michael Gallo (CTO) stepped down in March 2023. The article noted that "adoption of the Hyperganic Core 3 remained slow" — [TCT Magazine, Mar 2023](https://www.tctmagazine.com/founders-lin-kayser-michael-gallo-depart-hyperganic/)
- No 2025–2026 product news was found (see Gaps) — [CB Insights](https://www.cbinsights.com/company/hyperganic)

#### LEAP 71 (Dubai; computational engineering model "Noyron"; added as notable)
- Noyron-generated rocket engines are designed algorithmically and 3D printed. The company says it fires a Noyron-generated engine about every 4 weeks. It is scaling to 200 kN aerospike and 2000 kN reference designs, and has a multi-year propulsion agreement with Aspire Space — [Zawya press release](https://www.zawya.com/en/press-release/companies-news/leap-71-scales-computational-rocket-engine-development-to-meganewton-class-thrust-rcqflo4e); [3DPrint.com](https://3dprint.com/322814/leap-71-tests-two-3d-printed-methane-rocket-engines-in-weeks/); [Ground News/Aspire](https://ground.news/article/aspire-space-to-develop-large-reusable-launch-vehicle-in-partnership-with-leap-71)

#### BeyondMath (Cambridge/London)
- "Generative physics model" that the company calls the world's largest foundational physics model, trained "directly on first-principles physics". It covers aerodynamics to thermal management and claims up to 1,000x speed-up, with "engineering-grade simulations in minutes" (MARKETING). Partners are NVIDIA and AWS, plus the Honeywell STRATA project — [tech.eu, Feb 2026](https://tech.eu/2026/02/25/beyondmath-secures-185m-to-expand-the-worlds-largest-foundational-physics-ai-model/); [Vestbee](https://vestbee.com/insights/articles/beyond-math-raises-18-5-m)

#### Emmi AI (Linz/Vienna; acquired by Mistral AI, May 2026)
- Builds surrogate models for CFD, thermal and material stress. Applications include injection molding, automotive safety testing and power-grid stabilization. Team of 30+ — [Mistral announcement](https://mistral.ai/news/accelerate-ai-native-industry/); [The Decoder](https://the-decoder.com/mistral-ai-acquires-viennese-physical-ai-startup-emmi-ai/); [JKU](https://www.jku.at/en/news-events/press-releases/detail/news/jku-spin-off-emmi-ai-sichert-sich-mit-15-millionen-euro-groesste-seed-finanzierungsrunde-in-oesterreich/)
- Mistral's plans (ANNOUNCED): extend its models to physics, enable AI agents to work with existing engineering tools, build "best-in-class agents for engineers", and offer an integrated platform for aerospace, auto and semiconductors — [Mistral](https://mistral.ai/news/accelerate-ai-native-industry/)

#### Inductiva (Lisbon)
- Python API and package for running open-source physical simulators in the cloud, massively parallel on hundreds of cores. Sign-up is self-serve via console.inductiva.ai with an API key. It runs real solvers rather than surrogates (it can generate training data) — [GitHub inductiva](https://github.com/inductiva/inductiva); [ECO Sapo](https://eco.sapo.pt/2023/12/18/inductiva-fecha-ronda-seed-de-225-milhoes-de-euros/)

#### Pasteur Labs (Brooklyn; PBC; "simulation intelligence")
- Acquired FOSAI (space autonomy) in Aug/Sep 2025. It planned a public platform launch later in 2025, with data-generation products and end-to-end physics-AI apps to follow (ANNOUNCED; no launch confirmation found). Partnered with Cornell SciAI Center — [Digital Engineering 24/7](https://www.digitalengineering247.com/article/pasteur-labs-buys-ai-startup-fosai-to-accelerate-adoption-of-ai-driven-physics-simulation-for-space-defense-applications/news); [IC News](https://intelligencecommunitynews.com/pasteur-labs-acquires-fosai/); [Cornell](https://news.cornell.edu/node/329090)

#### Cadence / Future Facilities
- Acquisition announced July 2022. The 6SigmaDCX line is now the "Cadence Reality Digital Twin Platform": a CFD-solver digital twin for data-center power, cooling and capacity, with Reality DC Design, a Reality DC Insight web portal, and a library of about 14,000 equipment models. It is a vertical data-center thermal and CFD tool, not a general product-design tool — [BusinessWire 2022](https://www.businesswire.com/news/home/20220711005319/en/Cadence-to-Acquire-Future-Facilities-A-Pioneer-in-Data-Center-Digital-Twins); [Verdantix](https://atlas.verdantix.com/cadence/cadence-reality-digital-twin-platform)

#### Other notable players found
- **Luminary Cloud:** Physics AI platform for CFD. Launched Luminary Private Cloud (on-prem, Feb 2026), adopted NVIDIA GeoTransolver (Mar 2026) and launched SHIFT-Crash, a full-vehicle crash-response prediction model (Apr 2026) — [search results summarizing Luminary 2026 news; primary: Luminary resources](https://www.luminarycloud.com/resources/luminary-cloud-secures-dollar72m-series-b-to-lead-the-physics-ai-era); [SiliconANGLE](https://siliconangle.com/2025/09/15/luminary-cloud-raises-72m-advance-ai-driven-physical-product-design/)
- **Navier (SF; YC):** "Agent-Driven Engineering" between CAD and validation. Agents use computer vision and spatial reasoning to read 3D geometry, align design intent with tests, run parallel testing and produce automated reports alongside existing CAD/sim tools — [Pulse2](https://pulse2.com/navier-5-6-million-seed/); [VCA Online](https://www.vcaonline.com/news/2025121505/navier-raises-5-6m-to-deploy-agent-driven-engineering-the-next-engineering-unlock-after-cad-and-simulation/)
- **JuliaHub:** Dyad 3.0 brings agentic AI to physics-based simulation and industrial digital twins (system-level modeling) — [Startup Fortune, Jun 2026](https://startupfortune.com/juliahub-raised-65-million-to-prove-that-ai-can-do-the-hard-work-of-physical-engineering/)
- **Vinci (formerly Vinci4D):** Palo Alto, founded 2023. No details verified — [Luminary/Vinci search; CB Insights](https://www.cbinsights.com/investor/luminary-cloud)

### Inferences
- Natural-language input is essentially absent as a shipped feature across the physics-AI vendors. Neural Concept's Copilot ("CAD-ready models from high-level design ideas") is the only one that suggests it, and its access status is unclear. Synera's agents are LLM-backed but are configured as deterministic workflows, not free-form "describe a product" requests.
- Surrogate vendors (Neural Concept, PhysicsX, NavPack, Luminary, Emmi, BeyondMath) require customer simulation datasets or their own synthetic data factories. That is a key differentiator against a platform that runs real FEA first and needs no training data.
- Solver-based generative tools (ToffeeX, ColdStream, CDS) are the closest functional analogue to "optimized geometry verified by physics", but they are domain-narrow (ToffeeX and ColdStream: thermal/fluid; CDS: structural/thermo-mechanical) and form- or setup-driven rather than intent-driven.
- Verification and trust reporting (UQ, solver re-check of AI predictions) is weakly evidenced publicly for every vendor. PhysicsX claims parity with numerical simulation but publishes no benchmarks. That leaves room for a product that shows explicit solver verification of every AI-proposed design.

### Gaps
- Could not verify per-vendor 3D viewer, field visualization, Pareto/history UI, UQ features or API docs for Neural Concept, PhysicsX, NavPack or BeyondMath. Their public pages do not describe them, and docs are gated.
- Neural Concept Copilot general-availability status after Q1 2026 is unconfirmed.
- Emmi AI's specific model names (for example "NeuralCFD" or AB-UPT) and their post-acquisition availability are not on Mistral's page.
- Hyperganic's current operating status in 2025–2026 is unknown; no recent sources were found.
- Monolith's 3D/geometry deep-learning features and its post-CoreWeave product roadmap are not described on the current site.
- Pasteur Labs' platform launch, ToffeeX structural capability and ColdStream export formats are unverified.

---

## 2. Target customers, business model, pricing/accessibility, funding, limitations

### Takeaway
The segment is overwhelmingly enterprise and demo-gated (no public pricing). The exceptions are PhysicsX's free Ai.rplane demonstrator, Inductiva's self-serve API, ColdStream's "Try Now" and Synera's education program. Capital is concentrating: PhysicsX ($300M Series C at $2.4B, Jun 2026), Neural Concept ($100M Series C, Dec 2025), Luminary ($72M Series B), JuliaHub ($65M), Synera ($40M Series B). Consolidation is underway: Monolith went to CoreWeave, NAVASTO to Autodesk, Emmi to Mistral and Future Facilities to Cadence.

### Cited Findings
- **Neural Concept:** $100M Series C (Dec 18 2025), led by Goldman Sachs Alternatives with Forestay, Alven, HTGF, D.E. Shaw and Aster. Follows a $27M Series B (2024). 50+ enterprise customers; revenue up more than 4x in 18 months; targets auto, aerospace, energy and advanced manufacturing — [SiliconANGLE](https://siliconangle.com/2025/12/18/ai-aided-design-software-startup-neural-concept-raises-100m-accelerate-product-engineering/); [tech.eu](https://tech.eu/2025/12/18/neural-concept-raises-100m-to-scale-ai-native-engineering/). An aggregator lists $79.6M, which conflicts with the $100M figure (likely a currency or tranche artifact) — [Signalbase](https://www.trysignalbase.com/news/funding/neural-concept-secures-796m)
- **PhysicsX:** $300M Series C (Jun 8 2026) led by Temasek, with NVIDIA, Applied Materials and Siemens; $2.4B valuation. Booked revenue tripled in a year (company claim); about 300 employees. Earlier $135M Series B (2025) — [SiliconANGLE](https://siliconangle.com/2026/06/08/physicsx-reels-300m-speed-hardware-design-ai/); [Design Development Today](https://www.designdevelopmenttoday.com/industries/iot/news/22944120/physicsx-raises-135m-series-b-to-usher-in-ainative-engineering-manufacturing). Ai.rplane is free; the enterprise platform is unpriced — [PhysicsX newsroom](https://www.physicsx.ai/newsroom/introducing-lgm-aero-genai-for-aero-engineering-and-airplane-showcase-application-for-aerostructures)
- **Synera:** $40M (€35M) Series B (Apr 14 2026) led by Revaia, with Capgemini (ISAI Cap Venture), UVC, BMW iVentures, Cherry and Spark. Total about $58.1M. ARR doubled in 2025, with 60% of new business from agentic AI. 60+ enterprise customers in 15 countries (BMW, Airbus, NASA, Hyundai, Volvo, L'Oréal, Miele, Stihl). On-prem deployment; a pricing page exists but has no figures; there is an Education Program — [BusinessWire/Synera press](https://synera.io/press/synera-raises-40m-series-b-to-scale-agentic-ai-engineering-for-global-manufacturers); [SiliconANGLE](https://siliconangle.com/2026/04/14/german-startup-synera-lands-40-million-automate-engineering-workflows-ai-agents/); [synera.ai](https://www.synera.ai/)
- **Monolith AI:** Acquired by CoreWeave (announced Oct 2025, terms undisclosed). Customers include BMW, Mercedes-Benz, Honda, Nissan, Siemens, BAE Systems, Michelin and Vertical Aerospace — [Dealroom/EIT](https://eit.dealroom.co/news/feed/coreweave-acquires-monolith-ai-platform); [monolithai.com](https://www.monolithai.com/)
- **NAVASTO / Autodesk NavPack:** Acquisition announced Oct 2024. Sold as an Autodesk network subscription (Windows/Linux) — [Mainsights](https://www.mainsights.io/ma-news/autodesk-strengthens-ai-capabilities-with-acquisition-of-ai-engineering-software-provider-navasto); [Autodesk NavPack](https://www.autodesk.com/products/navpack)
- **Emmi AI:** €15M seed (2025; Austria's largest seed) from three.vc, Speedinvest, Serena and Push.vc. Mistral signed a definitive agreement to acquire it (May 2026). Terms undisclosed; French Tech Journal reports more than €300M, while a trend.at report says mostly share swap and "several hundred million euros" (single-sourced) — [JKU](https://www.jku.at/en/news-events/press-releases/detail/news/jku-spin-off-emmi-ai-sichert-sich-mit-15-millionen-euro-groesste-seed-finanzierungsrunde-in-oesterreich/); [Mistral](https://mistral.ai/news/accelerate-ai-native-industry/); [French Tech Journal](https://www.frenchtechjournal.com/mistral-ai-buys-viennas-emmi-ai-in-eur300m-bid-to-own-europes-factory-floor/); [trend.at](https://www.trend.at/unternehmen/europaeischer-ki-champion-mistral-uebernimmt-linzer-emmi-ai)
- **BeyondMath:** $10M seed extension (Feb 2026) led by Cambridge Innovation Capital, bringing seed total to $18.5M (UP.Partners, Insight, InMotion). Customers are unnamed auto, aerospace and electronics OEMs — [tech.eu](https://tech.eu/2026/02/25/beyondmath-secures-185m-to-expand-the-worlds-largest-foundational-physics-ai-model/)
- **ToffeeX:** Funding data conflicts. One source says a Series A in Nov 2022 with about $5.8M total; Caplight and CB Insights say about $7M; Latka claims bootstrapped with $3.7M 2025 revenue and 34 staff — [VCBacked](https://www.vcbacked.co/company/toffeex); [CB Insights](https://www.cbinsights.com/company/toffeeam/financials); [Latka](https://getlatka.com/companies/toffeex.com)
- **Diabatix:** Undisclosed round with Saffelberg Investments to grow ColdStream (date and amount unknown) — [Digital Engineering 24/7](https://digitalengineering247.com/article/ai-scale-up-diabatix-to-boost-growth-in-generative-thermal-design)
- **Cognitive Design Systems:** €2M seed (Iron Hands Capital, DAA Capital Partners, SpaceFounders France), dated Nov 2023 per data providers. Strong French defense and aerospace customer base — [3DPrint.com](https://3dprint.com/305355/e2m-drives-3d-printing-analysis-software-from-european-startup/); [Caplight](https://www.caplight.com/company/cognitive-design-systems)
- **Hyperganic:** €6.4M round (HV Capital, Vsquared; date unclear) — [Munich Startup](https://www.munich-startup.de/en/69103/hyperganic-secures-64-million-euros/)
- **Inductiva:** €2.25M seed (Dec 2023) led by Iberis Capital. No 2025 round was found — [ECO Sapo](https://eco.sapo.pt/2023/12/18/inductiva-fecha-ronda-seed-de-225-milhoes-de-euros/); [El Referente](https://elreferente.es/inversiones/inductiva-logra-22m-ronda-inversion-semilla-liderada-piberis-capital/)
- **Luminary Cloud:** $72M Series B (Sep 2025) led by N47 with Sutter Hill and NVentures. Aggregator totals ($187M vs $244M) conflict — [Luminary](https://www.luminarycloud.com/resources/luminary-cloud-secures-dollar72m-series-b-to-lead-the-physics-ai-era); [ZoomInfo](https://www.zoominfo.com/financial/luminary-cloud-inc/538154176)
- **Navier:** $5.6M seed (Dec 2025) from GV, HCVC and YC — [Pulse2](https://pulse2.com/navier-5-6-million-seed/)
- **JuliaHub:** $65M Series B with Dyad 3.0 agentic launch (2026) — [pulse.bot](https://www.pulse.bot/ai/news/juliahub-raises-65m-series-b-and-launches-dyad-30-bringing-agentic-ai-to-industrial-digital-twins-a361d05b-b548-4909-9b44-dff116717e52/)
- **Limitations reported:** PhysicsX coverage notes no public accuracy or validation data and no detail on out-of-distribution handling — [SiliconANGLE](https://siliconangle.com/2026/06/08/physicsx-reels-300m-speed-hardware-design-ai/). Commentary questions PhysicsX's valuation — [newmarketpitch](https://newmarketpitch.com/blogs/news/digital-twin-physicsx-overvalued). Hyperganic Core adoption was "slow" — [TCT](https://www.tctmagazine.com/founders-lin-kayser-michael-gallo-depart-hyperganic/)

### Inferences
- The space has no self-serve, transparent-pricing, natural-language-first offering for SMB or prosumer engineers. Accessibility is a clear wedge for a new entrant.
- Strategic acquirers (Autodesk, CoreWeave, Mistral, Cadence, and Siemens as an investor in PhysicsX) are absorbing physics-AI capability into larger stacks. Expect incumbents' CAD and CAE suites to bundle surrogate and agent features.

### Gaps
- No public pricing for any enterprise vendor. Inductiva's per-compute pricing could not be retrieved.
- Diabatix and ToffeeX funding remain unreliable (aggregators conflict).

---

## 3. Suggested 0–100 capability scores (for the competitor matrix)

### Takeaway
On public evidence, the following lead on what they actually do. All scores are analyst judgments derived from the findings above, not vendor-provided figures.
- **Physics surrogates:** PhysicsX and Neural Concept.
- **Agent orchestration:** Synera.
- **Solver-verified generative geometry with manufacturability:** ToffeeX, ColdStream and CDS.

No vendor scores above 60 on both "NL intent input" and "closed-loop autonomy". Low or uncertain scores reflect missing public evidence and should be refreshed if demos or docs are obtained.

### Cited Findings
- Scores are derived from the cited capability findings in Section 1 and pricing/access findings in Section 2. See the sources there.

### Inferences
Score key: NL = NL intent input; Geo = geometry generation; FEA = structural FEA; Th = thermal; CFD; Opt = automatic optimization; MO = multi-objective/Pareto; Sur = AI surrogate; LLM = LLM/agent autonomy; CL = closed-loop autonomy; Ver = verification/trust reporting; 3D = 3D result visualization; Hist = design comparison/history visualization; Mfg = manufacturability; Exp = export/interop; API = API/automation; Acc = cost/accessibility.

| Company | NL | Geo | FEA | Th | CFD | Opt | MO | Sur | LLM | CL | Ver | 3D | Hist | Mfg | Exp | API | Acc |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Neural Concept | 35 | 55 | 50 | 50 | 75 | 65 | 55 | 85 | 35 | 40 | 30 | 60 | 55 | 25 | 60 | 50 | 15 |
| PhysicsX (platform + LGM/Ai.rplane) | 10 | 60 | 55 | 50 | 80 | 65 | 50 | 90 | 10 | 45 | 30 | 60 | 45 | 15 | 40 | 50 | 35 |
| Monolith (CoreWeave) | 5 | 5 | 15 | 20 | 20 | 50 | 40 | 70 | 5 | 30 | 45 | 25 | 50 | 5 | 30 | 45 | 15 |
| NAVASTO / Autodesk NavPack | 5 | 35 | 10 | 10 | 70 | 45 | 35 | 75 | 5 | 25 | 25 | 55 | 40 | 15 | 60 | 40 | 30 |
| Synera | 40 | 45 | 55 | 40 | 40 | 60 | 45 | 15 | 70 | 65 | 55 | 30 | 35 | 55 | 85 | 70 | 25 |
| ToffeeX | 0 | 80 | 10 | 80 | 75 | 75 | 40 | 10 | 0 | 50 | 50 | 55 | 35 | 60 | 60 | 15 | 20 |
| Diabatix ColdStream | 0 | 75 | 5 | 85 | 70 | 75 | 45 | 10 | 0 | 50 | 50 | 55 | 50 | 70 | 50 | 50 | 35 |
| Cognitive Design Systems | 10 | 80 | 70 | 35 | 5 | 70 | 70 | 20 | 5 | 50 | 40 | 50 | 55 | 85 | 75 | 20 | 20 |
| Hyperganic | 0 | 60 | 20 | 15 | 10 | 30 | 15 | 0 | 0 | 20 | 10 | 40 | 15 | 50 | 50 | 40 | 10 |
| LEAP 71 (Noyron) | 0 | 75 | 30 | 45 | 40 | 40 | 20 | 0 | 0 | 50 | 50 | 20 | 15 | 70 | 40 | 15 | 15 |
| BeyondMath | 0 | 5 | 20 | 50 | 65 | 20 | 10 | 80 | 0 | 15 | 20 | 35 | 15 | 0 | 20 | 30 | 10 |
| Emmi AI (Mistral) | 10 | 5 | 45 | 50 | 70 | 25 | 15 | 85 | 20 | 15 | 20 | 35 | 15 | 10 | 20 | 30 | 10 |
| Inductiva | 0 | 0 | 40 | 40 | 60 | 20 | 10 | 10 | 0 | 15 | 30 | 15 | 10 | 0 | 40 | 85 | 75 |
| Pasteur Labs | 0 | 0 | 20 | 20 | 30 | 20 | 15 | 50 | 20 | 15 | 30 | 15 | 10 | 0 | 15 | 20 | 10 |
| Cadence Reality DC (Future Facilities) | 0 | 15 | 0 | 80 | 80 | 30 | 20 | 30 | 10 | 20 | 50 | 75 | 50 | 10 | 50 | 40 | 15 |
| Luminary Cloud | 10 | 20 | 30 | 30 | 85 | 45 | 35 | 80 | 15 | 30 | 40 | 70 | 50 | 5 | 50 | 70 | 40 |
| Navier | 40 | 10 | 40 | 30 | 30 | 30 | 15 | 15 | 60 | 45 | 50 | 35 | 25 | 20 | 40 | 40 | 25 |

One-line justifications, grouped per company:

**Neural Concept**
- NL 35: Copilot "from high-level design ideas" was announced with limited access.
- Geo 55: generative CAD and Copilot exist, but GA is unconfirmed.
- Sur 85: core strength; trained on customer programs.
- Ver 30: no public UQ evidence.
- Acc 15: enterprise only.

**PhysicsX**
- Sur 90: LGM trained on 25M meshes plus Data Factory.
- Geo 60: Ai.rplane shape generation, but limited to its domain.
- LLM 10: no agent evidence.
- Acc 35: free Ai.rplane demo, but an enterprise core.
- Ver 30: accuracy parity claimed without benchmarks.

**Monolith**
- Opt 50 and Ver 45: Next Test Recommender and Anomaly Detector are test-focused.
- Geo 5: no geometry generation.

**NavPack**
- CFD 70 and Sur 75: drag prediction in Alias with custom-trained models.
- Exp 60: inside the Autodesk ecosystem.

**Synera**
- LLM 70 and CL 65: shipped multi-agent orchestration (NASA supervisor agent) with LLM of choice.
- Exp 85: 80+ tool integrations.
- Sur 15: no native surrogate; physics depends on third-party solvers.
- NL 40: conversational intent is not documented.

**ToffeeX**
- Geo 80 and Th 80: physics-driven thermal-fluid topology optimization.
- FEA 10: no structural evidence.
- Mfg 60: STEP output and AM focus.

**ColdStream**
- Th 85: CFD plus 1D thermal.
- Mfg 70: AM, die-cast, CNC and sheet-metal methods.
- API 50: developer API listed.
- Acc 35: "Try Now" plus cloud.

**CDS**
- Geo 80, MO 70 and Mfg 85: implicit modeling, multi-KPI, five manufacturing processes.
- FEA 70: integrated FEA, solver unnamed.
- Exp 75: STEP, IGES, STL and native CAD.

**Hyperganic**: low across the board because product status is uncertain after the 2023 founder exit.

**LEAP 71**: Geo 75 and Mfg 70 for proven computational engineering of AM rocket engines; this is an internal tool, not a sold platform.

**BeyondMath / Emmi**: surrogate-centric; Emmi's agents are only a plan at Mistral.

**Inductiva**
- API 85 and Acc 75: self-serve Python API running real open-source solvers.
- No design generation.

**Pasteur Labs**: mostly announced or research-stage.

**Cadence Reality DC**: Th 80, CFD 80 and 3D 75, but vertical to data centers.

**Luminary**
- CFD 85 and Sur 80: Physics AI, SHIFT-Crash, private cloud.
- API 70: Python SDK assumed; unverified, see Gaps.

**Navier**: LLM 60 for agents reading 3D CAD and automating validation and reports, at early stage.

### Gaps
- Scores for 3D, Hist, Ver and API are low-confidence for every vendor, because public pages do not document viewers, Pareto views or APIs. They should be validated via demos or docs.
- Luminary's API/SDK and Neural Concept's API were not verified in this research session.
- Scores are suggestions by the researcher, not vendor-validated.
