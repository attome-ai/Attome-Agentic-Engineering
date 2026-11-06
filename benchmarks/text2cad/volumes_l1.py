"""Closed-form volumes (mm^3) from the dimensions each L1 prompt states, for the prompts where the formula is simple.

This checks that each answer builds what it was meant to build (a wrong depth, a cut that misses, a feature on the
wrong side changes the volume). It cannot catch a misreading of the prompt, because the same reading sets both: that
needs the benchmark's reference shapes.
"""
from math import pi, sqrt

V = {
    "L1_2": pi * (20**2 - 12.5**2) * 120,
    "L1_3": 50**3 - pi * 10**2 * 50,
    "L1_4": pi * 40**2 * 20 + pi * 20**2 * 30,
    "L1_5": 60 * 60 * 10 + pi * 15**2 * 50,
    "L1_6": pi * (50**2 - 25**2) * 5,
    "L1_7": 2 * 100 * 20 * 20 - 20 * 20 * 20,
    "L1_8": 80 * 80 * 40 - 2 / 3 * pi * 20**3,
    "L1_10": 100 * 50 * 30 - 20 * 15 * 100,
    "L1_11": pi * 30**2 * 40 - pi * 5**2 * 40,
    "L1_12": pi * 45**2 * 15 - 30 * 30 * 15,
    "L1_14": pi * 25**2 * 20 + pi * 25**2 * 40 / 3,
    "L1_15": 50**3 - 15**3 / 6,
    "L1_16": 120 * 40 * 10 - 2 * pi * 5**2 * 10,
    "L1_17": 60 * 20 * 30 - pi * 20**2 / 2 * 20,
    "L1_18": pi * 15**2 * 60 + 4 / 3 * pi * 15**3,
    "L1_19": pi * (40**2 - 30**2) * 20 + pi * (20**2 - 10**2) * 20,
    "L1_20": (30 + 5) / 2 * 80 * 40,
    "L1_21": pi * 25**2 * 80 - pi * (25**2 - 20**2) * 10,
    "L1_22": 30 / 3 * (60**2 + 40**2 + 60 * 40) - pi * 10**2 * 30,
    "L1_24": 10 * 40 * 60 + 50 * 40 * 10 - 10 * 40 * 10,
    "L1_25": pi * 20**2 * 40 - pi * 10**2 * 10 - pi * 5**2 * 30,
    "L1_26": 4 / 3 * pi * 30**3 - pi * 15**2 * (3 * 30 - 15) / 3,
    "L1_27": 20 * 20 * 30 + 100 * 20 * 10,
    "L1_29": pi * (30**2 + 20**2 + 10**2) * 10,
    "L1_32": 80 * 50 * 30 - 2 * 10**3 / 6,
    "L1_33": pi * (30**2 - 20**2) / 2 * 100,
    "L1_34": 60 * 20 * 100 + 30 * 20 * 100,
    "L1_35": pi * 20**2 * 40 - pi * 15**2 * 15 / 3,
    "L1_36": pi * (40**2 - 20**2) * 20,
    "L1_38": 60 * 30 / 2 * 80,
    "L1_39": 60 * 40 * 10 + pi * 20**2 / 2 * 10,
    "L1_40": pi * (15**2 * 20 * 2 + 7.5**2 * 20),
    "L1_41": pi * 50**2 * 10 - 4 * pi * 5**2 * 10,
    "L1_43": 100 * 50 * 30 - 30 * 20 * 100,
    "L1_44": 40**3 - pi * 10**2 * 30 - pi * 15**2 * 10,
    "L1_45": 2 / 3 * pi * (50**3 - 40**3),
    "L1_46": 30 * 40 / 2 * 60,
    "L1_48": 40 * 40 * 20 + pi * 10**2 * 20,
    "L1_56": 45 / 3 * (80**2 + 50**2 + 80 * 50) - 30 * 30 * 15,  # less the 1 mm chamfer at the cavity rim
}
TOL = {"L1_56": 0.002}  # the chamfer removes about 0.1% of the volume
assert sqrt  # kept for formulas added later
