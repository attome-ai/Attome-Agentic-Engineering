# Up the factory ladder: plans for levels 2 to 5 (P8)

Draft 2026-11-04 for the owner's review and sign-off (exit check P8-x0: each level has its own signed feature plan
before work starts). Level 1 is today's system: one part, designed, simulated, optimized, verified and explained.
Each level reuses everything below it; nothing is thrown away. Every level keeps the same rules: the engine computes,
the agent composes and explains; every number comes from a stored run with its error bars; licences stay clean.

## Level 2: assemblies (fasteners, contact, assembly order, assembly animation)

**Goal.** "Design a shelf unit for 40 kg" returns several parts that fit, a bill of materials with fasteners, a
verified simulation of the whole assembly, the order to put it together and an animation of the assembly.

| # | Feature | Approach | Proof it works |
|---|---|---|---|
| L2.1 | Assembly in the grammar | Parts placed by mates (face to face, pin in hole, bolted joint) on named faces; each part keeps its own family and parameters | A bracket + board + 2 bolts + 2 wall plugs assembly builds and gates |
| L2.2 | Fasteners | Standard bolts, nuts, screws and plugs (ISO sizes) as library parts with preload and shear/tension capacity; hole sizes checked against them | Bolt capacity check against hand calculation (VDI 2230 simplified) |
| L2.3 | Contact | CalculiX surface-to-surface contact (penalty) between parts, friction coefficient as a stated input; replaces today's "wall can pull" simplification | Hertz contact validation case; a bracket that lifts off the wall where it should |
| L2.4 | Assembly simulation | Static, modal and the time integration already built, on the whole assembly with contact | The shelf unit's deflection against a hand estimate; mesh convergence per part |
| L2.5 | Assembly order | Assembly by disassembly: find a part that can slide out without colliding (exact B-rep collision sweeps along candidate directions), remove it, repeat; reversed, this is a valid order. If none exists, report which parts lock each other | Every generated order replayed with collision checks; a deliberately locked design is reported, not animated |
| L2.6 | Assembly animation | In the desktop scene and the report: exploded view, then each part travelling in along its insertion direction, fasteners turning in; the same no-interpenetration check as today on every frame | Frame check: no overlap above 0.01 mm |
| L2.7 | Instructions | Step list with part and fastener counts and tools, generated from the order (like a furniture manual), English and Arabic | A reader assembles the printed demo unit from the instructions alone |
| L2.8 | Physical motion (optional) | Rigid-body physics (MuJoCo, Apache-2.0) for parts that move: hinges, drawers, a weight falling onto a shelf, with contact | A dropped weight's rebound against the energy balance |

**Order of work.** L2.1 and L2.2, then L2.5 and L2.6 (they need only geometry, and they are the most visible),
then L2.3 and L2.4 (contact is the hard physics), then L2.7, then L2.8.

**Risks.** Contact makes solves slower and can fail to converge: start with bonded contact, then frictionless, then
friction; keep the gate's no-overlap rule for assemblies. Assembly-order search grows with part count: limit to about
20 parts at level 2 and use sub-assemblies above that.

## Level 3: products with several kinds of physics, including air flow (CFD)

**Goal.** Enclosures, heat sinks with real air flow, fans and ducts: thermal and structural results driven by a
simulated flow instead of a correlation.

- CFD with OpenFOAM or SU2 (GPL, run as separate programs), steady laminar and RANS first; conjugate heat transfer
  for heat sinks; validation against the fin-channel correlation and published heat-sink data.
- Coupled runs: flow -> temperature -> thermal stress, on one parametric model.
- Surrogate models (C6) become worth it here, because a CFD evaluation costs minutes, not seconds; the GPU becomes
  useful for them.
- Proof: a heat sink measured in a small wind tunnel or with a fan and thermocouples against the prediction.

## Level 4: designing the manufacturing process

**Goal.** For each part, how to make it: printer settings or CNC toolpaths, time, cost and the process that wins.

- FDM slicing through the owner's installed slicer CLI (never bundled; slicers are AGPL) for exact time and material.
- CNC: toolpaths (roughing, finishing) for 3-axis parts, cycle time from the paths, fixturing between setups.
- Process selection across printing, machining, sheet metal and moulding with cost against volume.
- Proof: a printed and a machined part made from the generated instructions, time and cost against the estimate.

## Level 5: the factory

**Goal.** "Make 500 of these a month" returns a factory layout: machines, stations, buffers, people, throughput,
energy and cost, simulated before anything is bought.

- Discrete-event simulation of the production line (open source, e.g. SimPy, MIT) fed by level 4's process times.
- Layout optimization (station placement, material flow) with the same optimizer and trust rules.
- Energy and cost per part; bottleneck analysis; what-if on demand.
- Proof: a small real workshop's measured throughput against the simulation.

## Sign-off

| Level | Owner sign-off | Date |
|---|---|---|
| 2 Assemblies | | |
| 3 Multiphysics and CFD | | |
| 4 Manufacturing process | | |
| 5 Factory | | |
