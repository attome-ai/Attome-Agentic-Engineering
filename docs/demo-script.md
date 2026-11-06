# Demo script: three-minute video and live demo

For the proposal video (P5-t3) and the 3 February live demo. Every number on screen comes from a stored run; the
commands are tested. Rehearse with `python scripts/demo.py live` (timings in `benchmarks/demo_rehearsals.json`).

## Before recording

- `python scripts/demo.py preflight` must pass every line.
- Open in Chrome tabs, in this order: the bracket report `runs/p2_demo_delivered/report.html`, the same with `#split`, the heat
  sink report `runs/heatsink_30w_fan60_2400/report.html`, the workbench (`python -m structura.app`).
- Claude Code open in the repository folder (the MCP server loads from `.mcp.json`).
- Close notifications; screen at 1920x1080; browser zoom 100%.

## The three minutes

| Time | Show | Say |
|---|---|---|
| 0:00-0:15 | Title, then the hand-made bracket in the report (Baseline) | "Designing a part that is light and provably safe usually takes CAD, simulation and optimization, by hand. This one was designed by hand: it looks fine." |
| 0:15-0:35 | Claude Code: type the brief "A wall shelf bracket that holds 20 kg, as light as possible, printed in PETG." Show the spec card it returns | "I describe the part. The agent turns it into a spec I can check: the load at its design factor, a safety factor of 2 on printed strength, every value with its source. Nothing runs until I accept it." |
| 0:35-1:05 | Workbench: Optimize this brief, points streaming onto the plot | "Every point is a real finite-element solve, about four seconds each, of a design the agent composed from a typed grammar. Teal ones meet every requirement." (Speed this part up 4x in the edit.) |
| 1:05-1:30 | Report, `#split`: hand design and recommended design side by side, Von Mises | "Same colour scale. The hand design is about four times too weak: safety factor 0.48. The recommended design meets the brief: 184 grams, safety factor 2.05." |
| 1:30-1:50 | Trust panel: grade A, error bars; Pareto plot with "grade A" | "Why trust it? Lighter designs looked fine on a uniform mesh, but refining around the peak stress showed they fail, so this one is judged on the refined value. Every requirement has an error bar, equilibrium and energy balance check out, and the solver passes seven textbook benchmarks. Grade A means every requirement is proven, not just met." |
| 1:50-2:05 | ▶ load animation; parallel coordinates panel | "The animation shows how it bends; this chart shows every design tried: plate thickness is what makes it safe." |
| 2:05-2:25 | Heat sink report: temperature field, 103.7 g to 44.1 g | "Same pipeline, different physics: a heat sink for a 30 watt module with a small fan. More than half lighter, and the 75 degree limit is what stops it going lighter. Grade A." |
| 2:25-2:45 | Making the recommended design panel; then the printed parts under load [when the physical test is done] | "It tells me how to make it: printing fits, about 11 hours and 22 dollars at my stated rates. And here are both parts under load." |
| 2:45-3:00 | README results table | "Open tools, no solver licences, every number traceable to a stored run. Attome Agentic Engineering." |

## The demo on 3 February: the rendered video (D13)

Build it with `python scripts/make_video.py` -> `runs/video/attome_demo.mp4` (2 min 34 s, 1920 x 1080; frames from the
stored reports, a draft synthetic voice). To use your own voice, record one WAV per scene into `runs/video/voice/` with
the scene names in `SCENES` (for example `03_compare.wav`) and run it again. Scenes: title; brief to spec; hand design
against recommended in the safety colouring; the 1,600-design search; the trust record; the replay; how to make it;
the 30 W heat sink; the 100 kg refusal; the results card. The physical test is not in it yet: add a scene when the
printed parts have been loaded.

Play it from the local file (no network needed). Keep a copy on a USB stick. `scripts/demo.py live` is no longer part of
the demo; it runs the 20 kg brief in about 11 minutes and ends at grade B on the plain family (the grade A design is the
filleted geometry, verified offline).

### If something fails

- The video does not play: open `runs/p2_demo_delivered/report.html` and walk through the same panels by hand.
- No internet (the agent runs in the cloud): show the workbench and the reports (all offline), and play the video for
  the agent part.
- A judge asks "what if it can't be done?": run the 100 kg brief; the feasibility check answers in about 10 s and
  says the strongest design carries about 38 kg.
- A judge asks about trust: `structura reproduce --dir runs/delivered_trust2/t1204` re-solves the fine uniform mesh bit for bit. It does not re-run the local refinement (about 30 minutes), so do not say it does.

Answers to likely questions: `docs/judge-qa.md`.
