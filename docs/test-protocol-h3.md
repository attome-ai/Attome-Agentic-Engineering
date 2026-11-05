> **Predictions updated 2026-11-05 for the delivered design (184.4 g).** Before printing, the owner should review decision D10, which sets this design's grade A.

# H3: Physical test protocol, 20 kg PETG bracket

Draft 2026-11-04. Hardware, printing and testing are the owner's; this document fixes what is measured, how, and
what it is compared against, so the result is evidence and not an anecdote.

## Purpose

Show that the ranking and the order of magnitude predicted by the engine hold on real printed parts:

1. The recommended design is much stiffer and stronger than the hand baseline (relative comparison, the main claim).
2. Measured stiffness is within a stated band of the prediction (absolute comparison, secondary: printed parts are
   anisotropic and the PETG data is a datasheet value, flagged `verify` in `data/materials.json`).
3. The measured stiffness calibrates the PETG print knock-down (H3 calibration loop).

## Specimens

| Design | Parameters | Predicted mass | Source |
|---|---|---|---|
| Baseline | thickness 6, plate_width 50, rib_count 1, rib_thickness 4, fillet_radius 3 | 99.2 g | `benchmarks/h3_predictions.json` |
| Recommended (trial 1204) | thickness 11.92, plate_width 38.25, rib_count 3, rib_thickness 5.42, fillet_radius 3.94 | 184.4 g | `benchmarks/delivered_trust_1600.json` |

Both are on the delivered family, `bracket_delivered`, with 0.5 mm fillets on the rib flanks.

STL files: `baseline/design/part.stl` and `trial_1204/design/part.stl` under `runs/p2_demo_1600/opt/report_bundles_e2f3a47f50e1_3mm/`. `runs/` is not in git; regenerate with `structura export --family bracket_delivered --set ... --out DIR` from the parameters above (give the full-precision values from the JSON files).

**Three prints of each design**, all with identical settings:

- PETG from one spool, one printer, nozzle 0.4 mm, layer 0.2 mm.
- **100% infill** (the knock-down factors assume it), 4 perimeters.
- Orientation: wall plate flat on the bed, build direction +Z of the model (`print_direction` [0, 0, 1] in the spec).
- Record: printer, slicer and version, temperatures, print time, and the **weighed mass** of every part.

Weighed mass against predicted mass is the first comparison and needs no rig.

## Rig (bill of materials)

Prices are indicative; buy locally.

| Item | Purpose | Qty | Indicative cost |
|---|---|---|---|
| Load cell, 50 kg, bar type | force | 1 | 5 BHD |
| HX711 24-bit amplifier board | load-cell readout | 1 | 1 BHD |
| ESP32 development board | sampling, USB serial | 1 | 4 BHD |
| Digital dial indicator with data port, 0.01 mm, 25 mm travel (or a 50 mm linear potentiometer) | displacement | 1 | 15–25 BHD |
| Steel angle or thick plywood test wall, M5 bolts, washers | mounting like a real wall | 1 set | 5 BHD |
| Screw jack or lever with weights, hook and strap to the arm end | loading | 1 | 5–10 BHD |
| Calibration weights (or water bottles weighed on a kitchen scale): 1, 2, 5, 10 kg | load-cell calibration | 1 set | – |
| Safety glasses, a guard in front of the part | PETG fails suddenly | 1 | – |

Calibrate the load cell with at least four known weights before every session; record the fit (slope, intercept,
R²).

## Set-up

- Bolt the bracket through its two mounting holes to the test wall, wall face flat against it, as in the model
  (bolts fixed, wall face supported normal to the wall).
- Apply the load **downward at the end face of the arm**, through a strap 10–15 mm wide centred on the face (the model
  spreads the load over that face).
- Place the dial indicator **under the end of the arm**, on the load line. Zero it with the strap hanging at no load.

## Procedure (each specimen)

1. **Preload** 10 N, then unload and re-zero (seats the bolts).
2. **Stiffness steps:** load in steps of 20 N up to 100 N for the baseline and to 200 N for the recommended design,
   holding each step 10 s, recording force and displacement at the end of each hold. Unload. Repeat once.
3. **Proof load:** load to the service load of 196.2 N (20 kg) and hold 60 s; record the displacement at the start
   and end of the hold (creep) and any visible damage.
   - The baseline is predicted to yield locally *before* 196 N (see below): stop when its force stops rising or
     a crack appears, and record that load as its failure load.
4. **To failure** (recommended design only, optional on one specimen): increase in 50 N steps until the force drops;
   record the peak force and photograph the fracture (location and whether it ran along layers).

Log everything at 10 Hz or faster through the ESP32 (`force_N, disp_mm, t_s` CSV per specimen), plus photos.

## Predictions to compare against

From the stored runs. The service case is solved at the top of its factor range, 1.5 ×
196.2 N = **294.3 N**, with the PETG print knock-down applied (strength × 0.5, modulus × 0.85). The model is linear, so
stiffness = force / displacement and the load at first local yield = safety factor × 294.3 N.

| Quantity | Baseline | Recommended | Ratio |
|---|---|---|---|
| Displacement at the arm end (load line) at 294.3 N | 2.350 mm | 0.512 mm | 4.6× |
| Stiffness at the arm end | **125.3 N/mm** | **574.8 N/mm** | **4.6×** |
| Displacement at 196.2 N (proof load, linear) | 1.57 mm | 0.34 mm | |
| Safety factor at 294.3 N | 0.48 (uniform mesh) | 2.05 (settled peak) | at least 4.3× |
| Load at first local yield (knocked-down strength) | at most ≈ 142 N (14 kg) | ≈ 604 N (62 kg) | at least 4.3× |

Arm-end values: `benchmarks/h3_predictions.json`, from `python benchmarks/gauge_point.py` on those two bundles. The displacement is the
force-weighted mean of the loaded nodes (the displacement that does the work of the load); half of force times it
equals the solver's strain energy (345.74 and 75.34 N mm), which checks the extraction. The baseline has no local-refinement record, so its stress is a uniform-mesh value and its true peak can only be higher: its yield load is an upper bound.

Notes on these numbers:

- "First local yield" is the onset of yield at the most stressed point, not fracture; printed parts usually break
  later, along a layer. The failure load is expected **above** this value; the knock-down is deliberately conservative.
- Absolute stiffness depends on the real modulus of the printed PETG; the **ratio** does not. The ratio is the main
  claim.

## Pass criteria (decided before testing)

| Claim | Pass if |
|---|---|
| Ranking | Every recommended specimen is stiffer and carries more load than every baseline specimen |
| Stiffness ratio | Measured mean ratio within ±30% of the predicted 4.6× |
| Absolute stiffness | Measured within ±30% of prediction for each design (else calibrate, below) |
| Service load | Recommended design carries 196.2 N for 60 s with no visible damage and deflection below 3 mm (R2) |
| Mass | Weighed mass within ±10% of prediction |

Report the mean and the spread (min–max) of the three specimens; do not drop specimens.

## Calibration loop

If absolute stiffness is off, the effective modulus knock-down for PETG at this orientation and infill is

`modulus_knockdown_new = 0.85 × (measured stiffness / predicted stiffness)`,

taken as the mean over both designs. Record it in `data/materials.json` under `PETG.print_knockdown` with this test
as its source (date, printer, settings, file of the raw data), re-run the 20 kg study, and report whether the
recommendation changes. Strength knock-down is calibrated only if failures were observed on at least three specimens.

## Outputs

- `data/tests/h3/<specimen>.csv` raw logs, `data/tests/h3/specimens.json` (masses, settings, failure loads, notes).
- Photos in `docs/screenshots/h3/`.
- A force–displacement plot of measured against predicted, for the proposal (section 5.2) and the report.
