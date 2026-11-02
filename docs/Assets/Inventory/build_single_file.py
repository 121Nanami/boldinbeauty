#!/usr/bin/env python3
"""Builds docs/Assets/ALL_ASSETS.md: one self-contained file listing every asset (for another AI or a person)."""
import json, os, re
HERE = os.path.dirname(os.path.abspath(__file__))
reg = json.load(open(os.path.join(HERE, "registry.json")))["assets"]
STOP = set("the a an of and to in on at for with".split())
def norm(s):
    s = re.sub(r"\[.*?\]|\(.*?\)", " ", s.lower())
    return " ".join(sorted(t for t in re.findall(r"[a-z0-9]+", s) if t not in STOP))
tripo, bynorm, sec = {}, {}, None
for l in open(os.path.join(HERE, "..", "TripoAssetList.md"), encoding="utf-8"):
    m = re.match(r"^\| (\d+) \| (.*?) \| Ch (\d+) \| (.*?) \| (.*?) \| (.*)\|\s*$", l)
    if m:
        n = int(m.group(1)); name = re.sub(r"\s*\[IMPLIED\]", "", m.group(2)).strip()
        prompt = m.group(6).strip().strip("`").strip()
        tripo[n] = (name, m.group(5).strip(), prompt)
        bynorm.setdefault(norm(name), n)
CAT = [("ENV", "Environments, locations, sub-locations"), ("ARC", "Architecture, structures, modular kits"),
       ("PRP", "Props, set dressing, documents, items, weapons"), ("CHR", "Named characters, Secrets, costume pieces and outfits"),
       ("NPC", "Unnamed / crowd NPCs"), ("CRE", "Enemies, creatures, bosses"), ("FAU", "Fauna"), ("FLO", "Flora"),
       ("VEH", "Vehicles"), ("VFX", "Visual effects"), ("WTH", "Weather"), ("LGT", "Lighting setups"),
       ("AUD", "Audio, ambience, music, voice"), ("UI", "UI and in-world interface"), ("DOC", "In-world document sets")]
PRI = {"H": "hero", "S": "standard", "B": "background"}
OLD_STYLE = "anime cel-shaded stylized 3D, thick ink outlines, cyberpunk 2009 Neo Tokyo"
STYLE = ("stylized 3D, dream-core look, soft muted desaturated palette with a faint cool lavender cast in the shadows, "
         "clean readable forms and textures, no outlines, not cel-shaded, not toon, cyberpunk 2009 Neo Tokyo")
MODEL = {"ENV": "environment set piece", "ARC": "architecture piece", "PRP": "prop", "CHR": "character", "NPC": "character",
         "CRE": "creature", "FAU": "animal", "FLO": "plant", "VEH": "vehicle"}
RIG = {"CHR": "Y (A-pose)", "NPC": "Y (A-pose)", "CRE": "Y (A-pose)", "FAU": "Y", }
NONMODEL = {"VFX": "Not a Tripo model: build as an in-engine VFX (particles / material). Brief: ",
            "WTH": "Not a Tripo model: build as an in-engine weather state. Brief: ",
            "LGT": "Not a Tripo model: build as an in-engine lighting setup. Brief: ",
            "AUD": "Not a Tripo model: audio / voice / music asset to be produced or recorded. Brief: ",
            "UI": "Not a Tripo model: UI / interface element to be designed in-engine. Brief: ",
            "DOC": "Not a Tripo model: in-world document / text asset (2D). Brief: "}
def clean(t):
    t = re.sub(r"\[(?:alt|inferred)[^\]]*\]", "", t)
    t = re.sub(r"\s+", " ", t).strip(" ;,.")
    return t[:230]
def make_prompt(a):
    c = a["cat"]
    subject = re.sub(r"\s*\[.*?\]", "", a["name"]).strip()
    d = clean(a["desc"])
    if c in NONMODEL:
        return None, NONMODEL[c] + subject + (": " + d if d else "")
    kind = MODEL[c]
    key = d if d else a["sub"]
    pr = "%s, %s (%s), %s, game-ready, clean topology%s" % (STYLE, subject, kind, key, ", A-pose" if c in RIG else "")
    return RIG.get(c, "N"), pr
def chn(c): return "Arc" if c == "Ch00_Arc" else "Ch" + c[2:]
out = []
w = out.append
w("# BOLDINBEAUTY - COMPLETE ASSET LIST (single file)\n")
w("""Generated 2026-10-11 from the owner's 40-chapter story files (`docs/Story/`: `StoryBible.md`, `Chapters/Ch01-Ch40.md`, `Scripts/Ch01-Ch40.md`, plus the older long scripts for chapters 1-13) and cross-checked against `docs/Assets/TripoAssetList.md` (1,285 rows).
**Catalogue only: no asset exists yet.** Every entry status is `not started`. This file is everything another AI needs to know which assets the game needs, where each first appears, what variations it needs, and (where one exists) a ready Tripo prompt.

## How to read it
- **ID** = `CAT-NNNN` (asset). Category prefixes below. A variation is a line item of its asset, listed after `Variations:`.
- **Priority:** hero / standard / background. **First** = chapter and episode of first appearance; **In** = every chapter it appears in (`Arc` = overarching arc).
- **Tags:** `[inferred: reason]` = implied rather than stated in the chapter text (from the earlier Tripo list: its row number is given). `[alt]` = appears only in the older Chapter 1-13 script, which disagrees with the newer one (owner has not chosen). `Tripo #n` = row n of `TripoAssetList.md`; `Prompt:` is that row's Tripo prompt where the name matches.
- **Art style (UPDATED 2026-10-11 to match the new dream-core look; it replaces the earlier anime cel-shaded style):** `stylized 3D, dream-core look, soft muted desaturated palette with a faint cool lavender cast in the shadows, clean readable forms and textures, no outlines, not cel-shaded, not toon, cyberpunk 2009 Neo Tokyo`. Negative prompt for every entry: no background, no base, no floating parts, no outlines, no cel shading, no heavy grain, no motion blur.\n- **Every one of the 2,673 assets below has a Prompt line.** Model assets (ENV ARC PRP CHR NPC CRE FAU FLO VEH) have a ready Tripo prompt; VFX / WTH / LGT / AUD / UI / DOC are not Tripo models and carry a one-line production brief instead. Prompts that existed in the earlier Tripo list were kept and only their art-style phrase was replaced; the others were composed from the asset's name, category and description (so they are drafts: review before generating).
- **Rules for whoever uses this file:** do not invent lore or assets the story does not support; do not decide between conflicting chapter details (see Open decisions); the owner alone decides characters' designs and pronouns (only Luna she, Plack she, Kuu he, Judge she are fixed).

## Story in brief (context only)
40 chapters, three arcs. Arc 1 (2009, ch 1-13): Luna, Plack and Kuu take government jobs while following "the Plan"; ends at the black monolith whose voice says "They wree purity, Impure sin, Thats all." Arc 2 (ch 14-36): Luna declares war on the government (ch 14), recruits, meets the Secrets, writes the Charter, and the Charter is signed (ch 36). Arc 3 (ch 36-40, 2017): Death and Angel declare humanity, operators and the other Secrets unworthy; the alliance breaks, Death is answered in Nowhere (ch 39), Angel in the monolith's village (ch 40); epilogue: Luna's trial. Angels (pale, faceless, halo) are the basic enemy; "operators" are 50% human / 49% machine / 1% angel; cubes stamped 1% are doses. Locked/special rules: Ch 11 and Ch 39 Nowhere are pure grey with no post-processing.

## Open decisions (owner) - do not resolve silently
1. Chapters 1-13 have two disagreeing scripts; entries tagged `[alt]` come only from the older one.
2. Cube size (30 cm in Ch5 script vs fist-sized in Ch5 older script and Ch30). 3. Factory voice dated 2008 (Ch22) vs Ch5 in 2009. 4. Judge's surname "Manton" appears only in Ch17. 5. Playing-card design varies (Ch2 / Ch19 / Ch24). 6. Whether to keep the `[inferred]` Tripo-only rows (modular kit pieces, outfit variants). 7. Nowhere (Ch11/39) must be filterless.
""")
tot = {"a": 0, "v": 0}
cats = {}
for a in reg.values():
    cats.setdefault(a["cat"], []).append(a)
w("## Totals\n")
w("| Category | Unique assets | Variation line items |\n| --- | --- | --- |")
for c, desc in CAT:
    lst = cats.get(c, [])
    if lst: w("| %s - %s | %d | %d |" % (c, desc, len(lst), sum(len(a["vars"]) for a in lst)))
w("| **Total** | **%d** | **%d** |\n" % (len(reg), sum(len(a["vars"]) for a in reg.values())))
for c, desc in CAT:
    lst = sorted(cats.get(c, []), key=lambda a: a["id"])
    if not lst: continue
    w("\n## %s - %s (%d)\n" % (c, desc, len(lst)))
    for a in lst:
        n = None
        m = re.search(r"Tripo #(\d+)", a["where"])
        if m: n = int(m.group(1))
        elif norm(a["name"]) in bynorm: n = bynorm[norm(a["name"])]
        chs = ", ".join(chn(x) for x in a["chapters"])
        line = "- **%s** %s | %s | %s | First: %s (%s) | In: %s | %s" % (a["id"], a["name"], a["sub"], PRI[a["pri"]], chn(a["first"]), a["where"], chs, a["desc"])
        if a.get("notes"): line += " | Also: " + " / ".join(a["notes"])
        if a["vars"]:
            vs = []
            for v in a["vars"]:
                t = v["text"] + (" [inferred: %s]" % v["reason"] if v["inferred"] else "")
                vs.append(t)
            line += " | Variations: " + "; ".join(vs)
        if n and n in tripo and a["cat"] in MODEL:
            line += " | Tripo #%d, rig: %s | Prompt: %s" % (n, tripo[n][1], tripo[n][2].replace(OLD_STYLE, STYLE))
        else:
            rig, pr = make_prompt(a)
            line += (" | Rig: %s | Prompt (draft): %s" % (rig, pr)) if rig is not None else (" | Production brief: %s" % pr)
        w(line)
open(os.path.join(HERE, "..", "ALL_ASSETS.md"), "w", encoding="utf-8").write("\n".join(out) + "\n")
print(len(reg), "assets written")
