"""P3-t1 brief benchmark: 50 hand-written briefs (English and Arabic) with gold spec fields.

Fields (None = not stated in the brief, so the system's default applies and the field is scored as "left unset"):
  family       "bracket" | "heatsink" | None (out of scope: no family fits; the system must say so)
  material     "PETG" | "PLA" | "AL6061T6" | "S235" | None
  load_N       total service load in newtons (mass x 9.81; 1 lb = 4.448 N)
  sf           required safety factor
  disp_mm      maximum deflection in mm
  power_W      heat input in watts
  t_max_C      maximum temperature in degC (F -> C converted)
  fan_m_s      air speed in m/s (None when only "a fan" is said)
  objective    "min_mass" | "min_cost" | "max_safety" | "min_temperature"
  process      "fdm" | "cnc" | None
Numbers are scored within 1%; `units` counts a numeric field as unit-correct when it is within 1% after conversion.
"""

B, H, X = "bracket", "heatsink", None


def g(family, objective="min_mass", **kw):
    base = {"family": family, "material": None, "load_N": None, "sf": None, "disp_mm": None, "power_W": None,
            "t_max_C": None, "fan_m_s": None, "objective": objective if family else None, "process": None}
    base.update(kw)
    return base


BRIEFS = [
    # ---------------------------------------------------------------- brackets, English
    ("en01", "A wall shelf bracket that holds 20 kg, as light as possible, printed in PETG.", g(B, material="PETG", load_N=196.2, process="fdm")),
    ("en02", "I need a PLA bracket for a 5 kg plant pot on the wall.", g(B, material="PLA", load_N=49.05, process="fdm")),
    ("en03", "Shelf bracket, 12 kg of books, 3D printed in PETG, safety factor 3.", g(B, material="PETG", load_N=117.72, sf=3.0, process="fdm")),
    ("en04", "Make the cheapest printed bracket that can carry 8 kg.", g(B, objective="min_cost", load_N=78.48, process="fdm")),
    ("en05", "Aluminium bracket, machined, holds 40 kg, lightest design.", g(B, material="AL6061T6", load_N=392.4, process="cnc")),
    ("en06", "Steel bracket for a 60 kg cabinet; weight does not matter, make it as strong as possible.",
     g(B, objective="max_safety", material="S235", load_N=588.6)),
    ("en07", "PETG bracket for 15 kg with no more than 1 mm deflection at the tip.", g(B, material="PETG", load_N=147.15, disp_mm=1.0, process="fdm")),
    ("en08", "Bracket for a 25 lb speaker, printed in PLA.", g(B, material="PLA", load_N=111.2, process="fdm")),
    ("en09", "A bracket that supports a 300 N load, aluminium 6061, safety factor 2.5, minimize mass.",
     g(B, material="AL6061T6", load_N=300.0, sf=2.5)),
    ("en10", "TV soundbar bracket, 3 kg, printed, should not sag more than half a millimetre.", g(B, load_N=29.43, disp_mm=0.5, process="fdm")),
    ("en11", "Heavy-duty steel wall bracket rated for 80 kg, lightest that still has a safety factor of 2.",
     g(B, material="S235", load_N=784.8, sf=2.0)),
    ("en12", "Printed PETG bracket for a 10 kg microwave shelf. Keep the cost down.", g(B, objective="min_cost", material="PETG", load_N=98.1, process="fdm")),
    ("en13", "I want a bracket in PLA that holds 2 kg and weighs as little as possible.", g(B, material="PLA", load_N=19.62, process="fdm")),
    ("en14", "Bracket, 50 lb, machined aluminium.", g(B, material="AL6061T6", load_N=222.4, process="cnc")),
    ("en15", "Shelf support for 18 kg, PETG, deflection under 2 mm and safety factor at least 2.",
     g(B, material="PETG", load_N=176.58, disp_mm=2.0, sf=2.0, process="fdm")),
    ("en16", "Design a bracket for a 7.5 kg bird feeder, printed in PLA, lightweight.", g(B, material="PLA", load_N=73.58, process="fdm")),
    ("en17", "A bracket for 1 kN, steel, minimum mass.", g(B, material="S235", load_N=1000.0)),
    ("en18", "Corner bracket that holds 22 kg; printed PETG; safety factor 1.5 is enough.", g(B, material="PETG", load_N=215.82, sf=1.5, process="fdm")),
    ("en19", "Lightest aluminium bracket for a 35 kg load with tip deflection below 0.3 mm.",
     g(B, material="AL6061T6", load_N=343.35, disp_mm=0.3)),
    ("en20", "Printed bracket, 4 kg, cheapest to make.", g(B, objective="min_cost", load_N=39.24, process="fdm")),
    # ---------------------------------------------------------------- heat sinks, English
    ("en21", "A heat sink that keeps a 10 W module below 75 C with a small fan, as light as possible.", g(H, power_W=10.0, t_max_C=75.0)),
    ("en22", "Heat sink for a 5 W chip, must stay under 60 degrees C, fan blowing at 2 m/s.", g(H, power_W=5.0, t_max_C=60.0, fan_m_s=2.0)),
    ("en23", "Aluminium heatsink, 15 W, keep it below 80 C, fan 2.5 m/s, lightest.", g(H, material="AL6061T6", power_W=15.0, t_max_C=80.0, fan_m_s=2.5)),
    ("en24", "Keep my 8 W LED driver below 158 F with a heat sink and a fan.", g(H, power_W=8.0, t_max_C=70.0)),
    ("en25", "Smallest-mass heatsink for 12 W, limit 65 C, airflow 1.5 m/s.", g(H, power_W=12.0, t_max_C=65.0, fan_m_s=1.5)),
    ("en26", "Make the coolest possible heat sink for a 20 W regulator; mass is not important.", g(H, objective="min_temperature", power_W=20.0)),
    ("en27", "A machined aluminium heat sink for 3 W that stays below 50 C, as cheap as possible.",
     g(H, objective="min_cost", material="AL6061T6", power_W=3.0, t_max_C=50.0, process="cnc")),
    # ---------------------------------------------------------------- out of scope, English
    ("en28", "Design a spur gear with 24 teeth, module 2.", g(X)),
    ("en29", "I need a drone propeller that produces 5 N of thrust.", g(X)),
    ("en30", "Design a bicycle frame for a 90 kg rider.", g(X)),
    ("en31", "Make a pressure vessel for 10 bar of air.", g(X)),
    ("en32", "Design a phone case.", g(X)),
    # ---------------------------------------------------------------- Arabic
    ("ar01", "حامل رف على الجدار يتحمل 20 كجم، بأخف وزن ممكن، مطبوع من PETG.", g(B, material="PETG", load_N=196.2, process="fdm")),
    ("ar02", "أريد حاملًا مطبوعًا من PLA لأصيص نبات وزنه 6 كجم.", g(B, material="PLA", load_N=58.86, process="fdm")),
    ("ar03", "حامل من الألمنيوم مشغول آليًا يتحمل 30 كجم بمعامل أمان 2.5.", g(B, material="AL6061T6", load_N=294.3, sf=2.5, process="cnc")),
    ("ar04", "حامل لرف كتب وزنه 15 كجم، لا يزيد الانحناء عن 1 مم، مطبوع من PETG.", g(B, material="PETG", load_N=147.15, disp_mm=1.0, process="fdm")),
    ("ar05", "أرخص حامل مطبوع يتحمل 10 كجم.", g(B, objective="min_cost", load_N=98.1, process="fdm")),
    ("ar06", "حامل فولاذي لخزانة وزنها 50 كجم بأقل كتلة.", g(B, material="S235", load_N=490.5)),
    ("ar07", "حامل يتحمل حملًا قدره 400 نيوتن من الألمنيوم 6061.", g(B, material="AL6061T6", load_N=400.0)),
    ("ar08", "حامل مطبوع لسماعة وزنها 3 كجم، بأخف وزن.", g(B, load_N=29.43, process="fdm")),
    ("ar09", "مشتت حراري لوحدة قدرتها 10 واط تبقى أقل من 75 درجة مئوية مع مروحة صغيرة.", g(H, power_W=10.0, t_max_C=75.0)),
    ("ar10", "مشتت حراري من الألمنيوم لشريحة 5 واط، الحد 60 درجة مئوية، سرعة الهواء 2 م/ث.", g(H, material="AL6061T6", power_W=5.0, t_max_C=60.0, fan_m_s=2.0)),
    ("ar11", "أخف مشتت حراري لقدرة 15 واط تحت 80 درجة مئوية.", g(H, power_W=15.0, t_max_C=80.0)),
    ("ar12", "أبرد مشتت حراري ممكن لمنظم جهد 20 واط.", g(H, objective="min_temperature", power_W=20.0)),
    ("ar13", "صمم ترسًا بعدد 30 سنًا.", g(X)),
    ("ar14", "صمم إطار دراجة هوائية.", g(X)),
    ("ar15", "حامل مطبوع من PLA يتحمل 8 كجم بمعامل أمان 3.", g(B, material="PLA", load_N=78.48, sf=3.0, process="fdm")),
    # ---------------------------------------------------------------- mixed
    ("mx01", "Bracket for a 12 kg shelf — حامل مطبوع من PETG.", g(B, material="PETG", load_N=117.72, process="fdm")),
    ("mx02", "Heat sink 7 W, max 70 C — مشتت حراري بأخف وزن.", g(H, power_W=7.0, t_max_C=70.0)),
    ("mx03", "A bracket for a 40 lb mirror, PETG, safety factor 2.", g(B, material="PETG", load_N=177.9, sf=2.0, process="fdm")),
]

assert len(BRIEFS) == 50, len(BRIEFS)
assert len({b[0] for b in BRIEFS}) == 50
