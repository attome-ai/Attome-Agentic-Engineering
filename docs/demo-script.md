> **Numbers below predate decision D10** and must be re-recorded once the bracket search converges (234 g, 34.4 g and the 14.7 h print are stale). See `docs/benchmarks.md`.

# Demo script: three-minute video and live demo

For the proposal video (P5-t3) and the 3 February live demo. Every number on screen comes from a stored run; the
commands are tested. Rehearse with `python scripts/demo.py live` (timings in `benchmarks/demo_rehearsals.json`).

## Before recording

- `python scripts/demo.py preflight` must pass every line.
- Open in Chrome tabs, in this order: the bracket report `runs/p2_demo/report.html`, the same with `#split`, the heat
  sink report `runs/heatsink_fan_demo_300/report.html`, the workbench (`python -m structura.app`).
- Claude Code open in the repository folder (the MCP server loads from `.mcp.json`).
- Close notifications; screen at 1920x1080; browser zoom 100%.

## The three minutes

| Time | Show | Say |
|---|---|---|
| 0:00-0:15 | Title, then the hand-made bracket in the report (Baseline) | "Designing a part that is light and provably safe usually takes CAD, simulation and optimization, by hand. This one was designed by hand: it looks fine." |
| 0:15-0:35 | Claude Code: type the brief "A wall shelf bracket that holds 20 kg, as light as possible, printed in PETG." Show the spec card it returns | "I describe the part. The agent turns it into a spec I can check: the load at its design factor, a safety factor of 2 on printed strength, every value with its source. Nothing runs until I accept it." |
| 0:35-1:05 | Workbench: Optimize this brief, points streaming onto the plot | "Every point is a real finite-element solve, about four seconds each, of a design the agent composed from a typed grammar. Teal ones meet every requirement." (Speed this part up 4x in the edit.) |
| 1:05-1:30 | Report, `#split`: hand design and recommended design side by side, Von Mises | "Same colour scale. The hand design is five times too weak: safety factor 0.37. The recommended design meets the brief: 234 grams, safety factor 2.06." |
| 1:30-1:50 | Trust panel: grade A, error bars; Pareto plot with "grade A" | "Why trust it? Three mesh levels give an error bar on every requirement; equilibrium and energy balance check out; the solver passes seven textbook benchmarks. Grade A means every requirement is proven, not just met." |
| 1:50-2:05 | ▶ load animation; parallel coordinates panel | "The animation shows how it bends; this chart shows every design tried: plate thickness is what makes it safe." |
| 2:05-2:25 | Heat sink report: temperature field, 103.7 g to 34.4 g | "Same pipeline, different physics: a heat sink for a 10 watt module. Two thirds lighter, still below 75 degrees, also grade A." |
| 2:25-2:45 | Making the recommended design panel; then the printed parts under load [when the physical test is done] | "It tells me how to make it: printing fits, 14.7 hours, about 30 dollars at my stated rates. And here are both parts under load." |
| 2:45-3:00 | README results table | "Open tools, no solver licences, every number traceable to a stored run. Attome Agentic Engineering." |

## Live demo (3 February), if something fails

- Optimization too slow or a solve fails: `scripts/demo.py live` prints the fallback; open the precomputed report.
- No internet (the agent runs in the cloud): show the workbench and the reports (all offline), and play the video for
  the agent part.
- A judge asks "what if it can't be done?": run the 100 kg brief; the feasibility check answers in about 10 s and
  says the strongest design carries about 38 kg.
- A judge asks about trust: `structura reproduce --dir runs/p2_demo/trust` reproduces the result bit for bit.

Answers to likely questions: `docs/judge-qa.md`.
