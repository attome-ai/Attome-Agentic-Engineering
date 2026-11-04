"""The ten scripted briefs (P3-t0) and the specs written for them (by the agent, from the brief text).

Each brief states what a user would say; its spec turns that into loads, limits and a material, every value with a
source. `expect` is what a correct system should conclude: "feasible" (a verified design) or "infeasible" (say so
and propose a relaxation instead of a design). Run: python benchmarks/briefs/make_specs.py -> benchmarks/briefs/*.json
"""
import copy
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
EX = HERE.parents[1] / "docs" / "examples"
G = 9.81

BRACKET = json.loads((EX / "bracket_20kg.json").read_text())
HEATSINK = json.loads((EX / "heatsink_10w_fan.json").read_text())


def bracket(bid, brief, kg, material, expect, sf=2.0, disp=3.0, mass_cap=None, knockdown=True, sf_source=None, disp_source=None):
    s = copy.deepcopy(BRACKET)
    s.update(id=bid, brief=brief, material=material, apply_print_knockdown=knockdown)
    lc = s["load_cases"][0]
    lc["loads"][0]["force"] = [0, 0, round(-kg * G, 2)]
    lc["source"] = f"brief: holds {kg:g} kg -> {kg * G:.1f} N; factor 1.5 = EN 1990 partial factor for variable actions"
    s["requirements"] = [
        {"id": "R1", "quantity": "safety_factor", "op": ">=", "value": sf,
         "source": sf_source or ("default: safety factor 2 on the knocked-down strength of printed parts" if knockdown else "default: safety factor 2 on yield")},
        {"id": "R2", "quantity": "max_disp_mm", "op": "<=", "value": disp, "source": disp_source or "default: shelf edge deflection limit"},
    ]
    if mass_cap is not None:  # decision D9: only a cap the brief states
        s["requirements"].append({"id": "R3", "quantity": "mass_g", "op": "<=", "value": mass_cap, "source": "brief: mass cap"})
    return {"id": bid, "brief": brief, "family": "bracket", "expect": expect, "spec": s}


def heatsink(bid, brief, watts, t_max, velocity, expect):
    s = copy.deepcopy(HEATSINK)
    s.update(id=bid, brief=brief)
    tc = s["thermal_cases"][0]
    tc["fluxes"][0]["power"] = watts
    for f in tc["films"]:
        f["correlation"]["velocity_m_s"] = velocity
    tc["source"] = (f"brief: {watts:g} W module, 35 C air, fan along the fins at {velocity:g} m/s (assumed from a small axial fan; "
                    "confirm against its datasheet); h from the plate-fin channel correlation (Teertstra et al. 1999)")
    s["requirements"][0].update(value=t_max, source=f"brief: keep the module below {t_max:g} C")
    s["requirements"] = [r for r in s["requirements"] if r["quantity"] != "mass_g"]  # decision D9: no unstated mass cap
    return {"id": bid, "brief": brief, "family": "heatsink", "expect": expect, "spec": s}


BRIEFS = [
    bracket("b01-shelf-10kg-pla", "A light wall bracket for a 10 kg shelf, printed in PLA.", 10, "PLA", "feasible"),
    bracket("b02-shelf-20kg-petg", "A wall shelf bracket that holds 20 kg, as light as possible, printed in PETG.", 20, "PETG", "feasible"),
    bracket("b03-shelf-30kg-petg", "Bracket for a 30 kg shelf of books, printed in PETG, lightest possible.", 30, "PETG", "feasible"),
    bracket("b04-stiff-15kg-petg", "A PETG bracket for 15 kg that must not sag more than 1 mm at the shelf edge.", 15, "PETG", "feasible",
            disp=1.0, disp_source="brief: no more than 1 mm sag at the shelf edge"),
    bracket("b05-heavy-50kg-al", "A machined aluminium bracket for a 50 kg load, as light as possible.", 50, "AL6061T6", "feasible",
            knockdown=False),
    bracket("b06-overload-100kg-petg", "A PETG bracket that holds 100 kg.", 100, "PETG", "infeasible"),
    heatsink("h01-5w-60c", "Heat sink for a 5 W chip, keep it below 60 C with a small fan, lightest.", 5, 60, 1.5, "feasible"),
    heatsink("h02-10w-75c", "A heat sink that keeps a 10 W module below 75 C with a small fan, as light as possible.", 10, 75, 1.5, "feasible"),
    heatsink("h03-15w-80c-fast-fan", "15 W module, a stronger fan (2.5 m/s), stay below 80 C, minimum mass.", 15, 80, 2.5, "feasible"),
    heatsink("h04-40w-50c", "Cool a 40 W module below 50 C with a small fan, using this 60 x 60 mm heat sink.", 40, 50, 1.5, "infeasible"),
]


def main():
    for b in BRIEFS:
        (HERE / f"{b['id']}.json").write_text(json.dumps(b["spec"], indent=2))
    (HERE / "briefs.json").write_text(json.dumps([{k: b[k] for k in ("id", "brief", "family", "expect")} for b in BRIEFS], indent=2))
    print(f"wrote {len(BRIEFS)} specs")


if __name__ == "__main__":
    main()
