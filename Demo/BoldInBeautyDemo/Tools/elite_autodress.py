#!/usr/bin/env python3
"""Writes Tools/elite_dress.json from the mesh library (pure Python, runs anywhere).

    python3 Tools/elite_autodress.py [Saved/EliteLibrary.json] [--out Tools/elite_dress.json] [--variants 6]

Reads the library made by Tools/elite_library.py (every static mesh of the packs with its size) and Tools/elite_map_packs.json
(which packs may dress which map, PROPOSED). For every map and every greybox prop (car, bench, barrel, tree, ... the functions
of greybox_props.py) it picks the pack meshes whose NAME says what the prop is and whose SIZE fits the greybox prop, then writes
the mapping that greybox_dress.py reads. A prop with no match stays greybox, so the printed coverage is the honest list of what
the bundle can and cannot dress. Nothing is guessed from outside the library; re-run after changing the packs or the rules.
"""

import argparse
import fnmatch
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# prop -> (name must match, name must not match, (min, max) height in cm, optional damaged-variant pattern)
RULES = {
    "bench": (r"\bbench\b", r"\bpews?\b|church", (30, 140), None),
    "bin": (r"\b(bin|trash|garbage|dustbin|dumpster|waste)\b", r"\b(coal|bread|flour|storage)\b", (50, 220), None),
    "crate": (r"\b(crate|crates|wooden box|cargo box)\b", r"\b(stack|pile|open)\b", (30, 160), None),
    "barrel": (r"\b(barrel|keg|cask)\b", r"\b(broken|pile|stack|rack)\b", (50, 170), r"\bbroken\b"),
    "lamp": (r"\b(lamp ?post|street ?lamp|street ?light|lamp pole)\b", r"\bbroken\b", (250, 1000), r"\bbroken\b"),
    "tree": (r"\btrees?\b", r"\b(stump|log|branch|sapling|fallen|trunk|root|dead|bare)\b", (350, 2600), r"\b(dead|bare|withered)\b"),
    "palm": (r"\bpalm\b", r"\b(dead|stump)\b", (350, 2000), None),
    "bush": (r"\b(bush|shrub|hedge)\b", r"\b(dead|dry)\b", (40, 320), None),
    "flower_patch": (r"\bflowers?\b", r"\b(pot|vase|bouquet|table)\b", (10, 160), None),
    "rock": (r"\b(rock|rocks|boulder)\b", r"\b(wall|stairs?|floor|bridge|tile|brick|pillar|column|statue|grave|tomb|well|door|gate)\b", (60, 700), None),
    "rubble_pile": (r"\b(rubble|debris)\b", None, (15, 320), None),
    "barrier": (r"\b(barrier|barricade|fence)\b", r"\b(gate|door|window|post only)\b", (60, 220), r"\bbroken\b"),
    "boat": (r"\b(boat|rowboat|canoe|ship|raft)\b", r"\b(paddle|oar|sail only)\b", (50, 900), r"\b(wreck|wrecked|broken|sunken)\b"),
    "stall": (r"\b(stall|market stand|market stall|cart)\b", r"\b(horse|wheel only)\b", (120, 420), None),
    "cabinet": (r"\b(cabinet|cupboard|dresser|wardrobe|bookcase)\b", r"\b(open door)\b", (90, 320), None),
    "plant": (r"\b(plant|houseplant|flowerpot)\b", r"\b(dead|dry)\b", (25, 220), None),
    "car": (r"\b(car|sedan|taxi|automobile)\b", r"\b(card|cart|carpet|carriage|wheel)\b", (110, 260), r"\b(wreck|wrecked|burnt|burned|broken)\b"),
    "truck": (r"\b(truck|lorry|van)\b", None, (150, 420), r"\b(wreck|wrecked|burnt|burned|broken)\b"),
    "traffic_light": (r"\btraffic ?lights?\b", None, (200, 700), None),
    "vending": (r"\bvending\b", None, (120, 260), None),
    "power_pole": (r"\b(power pole|utility pole|telegraph pole|electric pole)\b", None, (500, 1500), None),
    "water_tank": (r"\b(water tank|cistern)\b", None, (150, 900), None),
}
HEIGHT_FROM = {"lamp": "h", "power_pole": "h"}   # the greybox prop's own height argument sets the scale


def words(name):
    """'SM_BenchWood02a' -> 'bench wood 02a': camel case and underscores split, prefixes dropped, lower case."""
    name = re.sub(r"^(SM|SK|S|BP)_", "", name)
    name = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", name)
    name = re.sub(r"(?<=[A-Za-z])(?=\d)", " ", name)   # Bench02a -> Bench 02a
    return re.sub(r"[_\-\s]+", " ", name).strip().lower()


def matches(entry, include, exclude, height):
    text = words(entry["name"])
    if not re.search(include, text):
        return False
    if exclude and re.search(exclude, text):
        return False
    return height[0] <= entry["size"][2] <= height[1]


def spread(items, count):
    """`count` items evenly spread over the sorted list (deterministic variety)."""
    items = sorted(items, key=lambda e: e["path"])
    if len(items) <= count:
        return items
    return [items[int(i * len(items) / count)] for i in range(count)]


def build(library, map_packs, variants=6):
    mapping = {"version": 1, "generated_by": "Tools/elite_autodress.py", "maps": {}}
    coverage = {}
    for map_name, cfg in sorted(map_packs["maps"].items()):
        patterns = cfg["packs"]
        pool = [m for m in library["meshes"] if any(fnmatch.fnmatch(m["pack"], p) for p in patterns)]
        props, found = {}, {}
        for prop, (inc, exc, height, damaged) in RULES.items():
            normal = [m for m in pool if matches(m, inc, exc, height) and not (damaged and re.search(damaged, words(m["name"])))]
            broken = [m for m in pool if damaged and matches(m, inc, None, height) and re.search(damaged, words(m["name"]))]
            if not normal and not broken:
                continue
            rule = {}
            if normal:
                rule["meshes"] = [m["path"] for m in spread(normal, variants)]
            if broken:
                rule["damaged_meshes"] = [m["path"] for m in spread(broken, variants)]
            if prop in HEIGHT_FROM:
                rule["height_from"] = HEIGHT_FROM[prop]
            props[prop] = rule
            found[prop] = (len(normal), len(broken))
        if props:
            mapping["maps"][map_name] = {"props": props}
        coverage[map_name] = {"pool": len(pool), "props": found, "why": cfg.get("why", "")}
    return mapping, coverage


def report(coverage):
    lines = ["| Map | Pool | Props dressed (meshes found; damaged variants) | Note |", "| --- | ---: | --- | --- |"]
    for map_name, c in coverage.items():
        props = ", ".join("%s %d%s" % (p, n, "+%d" % d if d else "") for p, (n, d) in sorted(c["props"].items())) or "none: stays greybox"
        lines.append("| %s | %d | %s | %s |" % (map_name, c["pool"], props, c["why"]))
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("library", nargs="?", default=os.path.join(HERE, "..", "Saved", "EliteLibrary.json"))
    parser.add_argument("--out", default=os.path.join(HERE, "elite_dress.json"))
    parser.add_argument("--variants", type=int, default=6)
    args = parser.parse_args(argv)
    with open(args.library, encoding="utf-8") as handle:
        library = json.load(handle)
    with open(os.path.join(HERE, "elite_map_packs.json"), encoding="utf-8") as handle:
        map_packs = json.load(handle)
    mapping, coverage = build(library, map_packs, args.variants)
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(mapping, handle, indent=1, sort_keys=True)
    print(report(coverage))
    print("\nwrote %s (%d maps with at least one dressed prop)" % (args.out, len(mapping["maps"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
