#!/usr/bin/env python3
"""Generate data/gamedata/hardware.json (catalog step 4a, classic scenario).

Descriptive only: the 21 original hardware programs per side.  Usage (from
the repository root):
    python3 tools/gen_hardware_catalog.py > data/gamedata/hardware.json
"""
import json
import sys

SIDES = ["US", "SV"]

IDS = {
    "US": {
        ("probe", 0): "EXPLORER", ("probe", 1): "RANGER", ("probe", 2): "SURVEYOR",
        ("rocket", 0): "ATLAS", ("rocket", 1): "TITAN", ("rocket", 2): "SATURN",
        ("rocket", 3): "NOVA", ("rocket", 4): "BOOSTERS",
        ("manned", 0): "MERCURY", ("manned", 1): "GEMINI", ("manned", 2): "APOLLO",
        ("manned", 3): "XMS2", ("manned", 4): "JUPITER", ("manned", 5): "EAGLE",
        ("manned", 6): "CRICKET",
    },
    "SV": {
        ("probe", 0): "SPUTNIK", ("probe", 1): "COSMOS", ("probe", 2): "LUNA",
        ("rocket", 0): "R7", ("rocket", 1): "PROTON", ("rocket", 2): "N1",
        ("rocket", 3): "UR700", ("rocket", 4): "BOOSTERS",
        ("manned", 0): "VOSTOK", ("manned", 1): "VOSKHOD", ("manned", 2): "SOYUZ",
        ("manned", 3): "LAPOT", ("manned", 4): "LK700", ("manned", 5): "LKM",
        ("manned", 6): "LK",
    },
}
MISC_IDS = ["KICKER_A", "KICKER_B", "KICKER_C", "EVA_SUITS", "DOCKING", "PHOTO_RECON"]

CATS = ["probe", "rocket", "manned", "misc"]
SRC_KEY = {"probe": "Probe", "rocket": "Rocket", "manned": "Manned", "misc": "Misc"}
CAT_LETTER = {"probe": "P", "rocket": "R", "manned": "C", "misc": "M"}
COUNTS = {"probe": 3, "rocket": 5, "manned": 7, "misc": 6}
HELP_BASE = {"probe": 201, "rocket": 207, "manned": 217, "misc": 231}

ROLES = {
    "probe": ["orbital_probe", "interplanetary_probe", "lunar_probe"],
    "rocket": ["launcher"] * 4 + ["booster"],
    "manned": ["capsule", "capsule", "capsule", "minishuttle", "capsule",
               "lander", "lander"],
    "misc": ["kicker", "kicker", "kicker", "eva", "docking_module", "photo_recon"],
}
CREW = [1, 2, 3, 3, 4, 2, 1]
PRESTIGE_FIRST = ["OnePerson", "TwoPerson", "ThreePerson", "Minishuttle", "FourPerson"]

# VabSprite enum values in vab.cpp
VAB_SPRITE = {
    "probe": [9, 10, 11],
    "rocket": [0, 1, 2, 3, None],
    "manned": [13, 14, 15, 16, 17, 18, 19],
    "misc": [20, 21, 22, None, 12, None],
}
VAB_SPRITE_BOOSTERS = [4, 5, 6, None, None]

# Capsule limits from crew.cpp HardRequest() (unmanned) / SecondHard() (manned).
# Future[pad].Duration is the level index 0..5 (A..F); None = no limit.
LIMITS = {
    0: dict(dur_m=2, dur_u=2, days_m=2, days_u=1),
    1: dict(dur_m=5, dur_u=5, days_m=5, days_u=5),
    2: dict(dur_m=None, dur_u=None, days_m=None, days_u=None),
    3: dict(dur_m=4, dur_u=3, days_m=None, days_u=None),
    4: dict(dur_m=None, dur_u=None, days_m=None, days_u=None),
}


def type_label(side, cat, idx):
    """Text appended to the program name in ShowUnit (rdplex.cpp)."""
    if cat == "probe":
        return "SATELLITE" if idx in (0, 1) else "PROBE"
    if cat == "rocket":
        if idx <= 3:
            return "ROCKET"
        return "STRAP-ON" if side == "US" else "BOOSTER STAGE"
    if cat == "manned":
        return ["CAPSULE", "CAPSULE", "CAPSULE", "MINISHUTTLE", "SPACECRAFT",
                "MODULE", "MODULE"][idx]
    return ["BOOSTER", "BOOSTER", "BOOSTER", "", "MODULE", ""][idx]


def main():
    urast = json.load(open("data/gamedata/urast.json"))["value0"]["P"]
    out = []
    for p, side in enumerate(SIDES):
        for cat in CATS:
            for idx in range(COUNTS[cat]):
                src = urast["value%d" % p][SRC_KEY[cat]]["value%d" % idx]
                hid = side + "." + (MISC_IDS[idx] if cat == "misc"
                                    else IDS[side][(cat, idx)])
                letter = CAT_LETTER[cat]
                e = {
                    "id": hid,
                    "side": side,
                    "category": cat,
                    "role": ROLES[cat][idx],
                    "name": src["Name"],
                    "type_label": type_label(side, cat, idx),
                }
                if side == "SV" and cat == "rocket" and idx == 4:
                    e["label_replaces_name"] = True
                e["crew"] = CREW[idx] if cat == "manned" else 0
                e["unit_weight"] = src["UnitWeight"]
                e["max_payload"] = src["MaxPay"]
                e["duration_days"] = src["Duration"]
                if cat == "manned" and idx <= 4:
                    lim = LIMITS[idx]
                    e["duration_level"] = ("ABCDEF"[lim["dur_m"]]
                                           if lim["dur_m"] is not None else None)
                    e["limits"] = {
                        "max_future_duration_manned": lim["dur_m"],
                        "max_future_duration_unmanned": lim["dur_u"],
                        "max_plan_days_manned": lim["days_m"],
                        "max_plan_days_unmanned": lim["days_u"],
                    }
                reach = None
                if cat == "probe":
                    reach = ["orbital", "interplanetary", "translunar"][idx]
                elif cat == "manned" and idx <= 4:
                    reach = "orbital" if idx == 0 else "translunar"
                if reach:
                    e["reach"] = reach
                e["can_dock"] = ((cat == "manned" and idx in (1, 2, 3))
                                 or (cat == "misc" and idx == 4))
                e["direct_ascent"] = (cat == "manned" and idx == 4)
                e["accepts_boosters"] = (cat == "rocket" and idx in (0, 1, 2))
                if cat == "manned" and idx in (1, 2, 3):
                    lm2 = side + "." + IDS[side][("manned", 5)]
                    lm1 = side + "." + IDS[side][("manned", 6)]
                    e["compatible_landers"] = [lm1] if idx == 1 else [lm2, lm1]
                    e["lander_needs_docking_module"] = idx in (1, 3)
                    e["compatible_kickers"] = [side + ".KICKER_A", side + ".KICKER_B"]
                if cat == "manned" and idx <= 4:
                    e["prestige_first"] = PRESTIGE_FIRST[idx]
                e["anim_id"] = letter + str(idx)
                img = {
                    "rd": "images/rd/rdfull.but.%d.png" % (p * 32 + CATS.index(cat) * 8 + idx),
                    "liftoff": "images/liftoff.abz.%s%s%d.png" % (side, letter, idx),
                }
                vs = VAB_SPRITE[cat][idx]
                if vs is not None:
                    img["vab_sprite"] = vs
                if cat == "rocket" and VAB_SPRITE_BOOSTERS[idx] is not None:
                    img["vab_sprite_boosters"] = VAB_SPRITE_BOOSTERS[idx]
                if cat == "manned" and idx <= 4:
                    img["aprog"] = "images/aprog/aprog.%d.%d.png" % (p, idx + 1)
                    img["patch_base"] = 50 * p + 10 * idx
                    img["mhist"] = 7 + p * 8 + idx
                e["images"] = img
                # rdplex.cpp formats "i%d03" into a char[5], which truncates to "i" + 3 digits
                e["help_id"] = "i%d" % (HELP_BASE[cat] + idx * 2 + p)
                e["legacy"] = {"cat": CATS.index(cat), "idx": idx}
                out.append(e)
    doc = {
        "version": 1,
        "scenario": "classic",
        "_comment": ("Descriptive catalog of the 21 original programs per side "
                     "(stage 4a). Numerical stats stay in urast.json/hist.json."),
        "hardware": out,
    }
    json.dump(doc, sys.stdout, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
