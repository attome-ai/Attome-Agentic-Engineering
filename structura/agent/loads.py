"""Use case to loads (A1): turn a situation ("holds 20 kg", "people lean on it") into forces with the working shown.

The agent picks the case; it cannot invent a factor. Every factor here has a source. Units: kg, N, m/s^2.
"""
from __future__ import annotations

G = 9.80665  # m/s^2, standard gravity (CGPM 1901)

# name -> (factor, what it covers, source)
FACTORS = {
    "static": (1.0, "steady load, no amplification", "definition"),
    "variable_action": (1.5, "partial factor for variable actions (people, stored goods)",
                        "EN 1990:2002 Annex A1, Table A1.2(B): gamma_Q = 1.5"),
    "permanent_action": (1.35, "partial factor for permanent actions (self-weight, fixed equipment)",
                         "EN 1990:2002 Annex A1, Table A1.2(B): gamma_G = 1.35"),
    "sudden_application": (2.0, "load placed suddenly (dropped onto the part from rest at zero height)",
                           "energy method: a suddenly applied load doubles the static deflection and stress"),
}


def derive(mass_kg: float, case: str = "variable_action", supports: int = 1) -> dict:
    """Force per supporting part for a mass, with the factor, its source and the arithmetic written out."""
    if mass_kg <= 0:
        raise ValueError("mass must be positive")
    if case not in FACTORS:
        raise ValueError(f"unknown load case {case!r}; known: {', '.join(FACTORS)}")
    if supports < 1:
        raise ValueError("at least one supporting part")
    f, what, src = FACTORS[case]
    weight = mass_kg * G
    per_part = weight / supports
    return {
        "nominal_N": round(per_part, 3), "factor": f, "design_N": round(per_part * f, 3), "case": case, "covers": what, "source": src,
        "working": (f"{mass_kg:g} kg x {G} m/s^2 = {weight:.1f} N"
                    + (f", shared by {supports} parts = {per_part:.1f} N each" if supports > 1 else "")
                    + f"; x {f:g} ({case}) = {per_part * f:.1f} N"),
        "spec_hint": {"force": [0, 0, -round(per_part, 3)], "factor_range": [1.0, f]},
    }
