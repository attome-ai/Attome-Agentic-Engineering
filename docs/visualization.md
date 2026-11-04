# Visualization: first release (P4)

`python -m structura.report.build --spec SPEC --study STUDY_DIR [--trust DIR] [--baseline k=v ...] --out report.html`
writes one self-contained HTML file (Three.js 0.149.0 embedded, MIT) that opens offline in any browser. Every number
comes from a stored engine run. Screenshots: `docs/screenshots/report_p2_demo_*.png` (P2 demo, 20 kg bracket).

| Feature | Built | Still to do |
|---|---|---|
| G1 3D viewer with fields | Von Mises, displacement, safety and temperature on the skin mesh, viridis colormap with legend in real units, hover probe, own orbit controls, light/dark; load arrows, support boxes (fully held / one direction) and heat-input arrows from the engine's markers | Contour bands and isolines, peak labels pinned in 3D, agent annotations in 3D, measured 200k-triangle load time |
| G2 Deformation, modes, clip, probe | Deformation slider with undeformed ghost, probe | Load-up animation, mode-shape animation, pinned probes; clip plane only with volume data (the report carries the surface mesh, so a browser cut would show surface values inside; use the VTU in ParaView) |
| G3 History | Objective per evaluation with the requirement band, best-feasible-so-far line, failed evaluations marked; brushable parallel coordinates of every evaluation (parameters and results) | Live streaming, shape morph replay, Sobol bar chart, thumbnail gallery |
| G4 Pareto explorer | Every evaluation, the fine-mesh Pareto designs, recommended ring with its trust grade, baseline square, a bar per Pareto design showing how far it moved from the search mesh to the fine mesh; click loads the design in 3D and in the comparison | Grades for every Pareto design (needs a trust run each), drag-to-steer, 3-objective view |
| G5 Baseline vs optimized | Toggle between selected and baseline, and a side-by-side view (one camera, one colour scale over both, probe on either half, `report.html#split`); comparison table with changes and pass/fail | Translucent overlay, test photos and curves |
| G6 Agent timeline | Steps with outcome and detail from the study log | Live streaming, time travel, replay |
| I1 Report | Self-contained interactive HTML with spec card, trust panel, assumptions | PDF, Arabic, run history browser, hash-chained audit log |

## Hands-on workbench (`python -m structura.app`)

A local page (127.0.0.1:8765, Python standard library only) for using the engine by hand, with no AI in the loop:
pick a brief from `docs/examples`, move the parameter sliders, press Evaluate (or tick "evaluate when a slider is
released"). The real pipeline runs (build, gate, mesh, CalculiX; about 4 s for the bracket at 4 mm) and the page shows
the field in 3D with load and support markers, the metrics and every requirement with pass/fail. A gate or mesh
failure is shown with its reason. `#run` evaluates once on load. Screenshot: `docs/screenshots/workbench.png`.

Live optimization (G3 live streaming): "Optimize this brief" in the workbench starts a study in the background (one
at a time); every finished evaluation streams to the page within a second (`/api/progress`) and is plotted on the two
objectives with the requirement line: teal feasible, grey not, orange the best feasible design so far. Clicking a
point loads its parameters into the sliders and solves it for the 3D view. Reloading the page picks up a running or
finished study. Test: 40 evaluations of the 20 kg brief streamed and plotted (none feasible at that budget, as
expected: the brief needs about 200).
