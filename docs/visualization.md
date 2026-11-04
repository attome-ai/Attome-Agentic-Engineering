# Visualization: first release (P4)

`python -m structura.report.build --spec SPEC --study STUDY_DIR [--trust DIR] [--baseline k=v ...] --out report.html`
writes one self-contained HTML file (Three.js 0.149.0 embedded, MIT) that opens offline in any browser. Every number
comes from a stored engine run. Screenshots: `docs/screenshots/report_p2_demo_*.png` (P2 demo, 20 kg bracket).

| Feature | Built | Still to do |
|---|---|---|
| G1 3D viewer with fields | Von Mises, displacement and safety view on the skin mesh (engine `write_skin`), viridis colormap with legend in real units, hover probe with value, location and local safety factor, own orbit controls, light/dark | Contour bands and isolines, peak labels pinned in 3D, load and support symbols, agent annotations in 3D, measured 200k-triangle load time |
| G2 Deformation, modes, clip, probe | Deformation slider with undeformed ghost, probe | Load-up animation, mode-shape animation, clip plane, pinned probes |
| G3 History | Safety factor per evaluation with the required band, lightest-feasible-so-far line, failed evaluations marked | Live streaming, shape morph replay, parallel coordinates, Sobol bar chart, thumbnail gallery |
| G4 Pareto explorer | Every evaluation, the fine-mesh Pareto designs, recommended ring, baseline square; click loads the design in 3D and in the comparison | Error bars and grades per point, drag-to-steer, 3-objective view |
| G5 Baseline vs optimized | Toggle between selected and baseline in the same camera, comparison table with changes and pass/fail | Two synchronized viewers, translucent overlay, test photos and curves |
| G6 Agent timeline | Steps with outcome and detail from the study log | Live streaming, time travel, replay |
| I1 Report | Self-contained interactive HTML with spec card, trust panel, assumptions | PDF, Arabic, run history browser, hash-chained audit log |
