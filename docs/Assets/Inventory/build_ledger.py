#!/usr/bin/env python3
"""Asset inventory ledger builder (documentation tooling, no game assets are created).

Usage: build_ledger.py src/Ch07.txt      -> writes ledgers/Ch07.md and updates registry.json
       build_ledger.py --master          -> writes master/<CAT>.md, master/INDEX.md and counts.json

Source line format (one asset per line, '#' comments, blank lines ignored):
    CAT|Name|Subcategory|Where first seen in this chapter|P|Description|Var1;Var2;?Var3~reason
CAT is the ID prefix (ENV ARC PRP FLO FAU CRE CHR NPC VEH MAT VFX WTH LGT AUD UI DOC). P = H hero / S standard / B background.
A variation starting with '?' is [inferred] (text after '~' is the one-line reason). A description ending in
'[inferred: reason]' marks an inferred asset. An asset with the same CAT+name in an earlier chapter keeps its ID; this chapter's
variations are merged into it (the master catalog lists every chapter and every variation once).
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REG = os.path.join(HERE, "registry.json")
PRI = {"H": "hero", "S": "standard", "B": "background"}


def load():
    if os.path.exists(REG):
        return json.load(open(REG))
    return {"assets": {}, "next": {}}


def norm(name):
    return re.sub(r"[^a-z0-9]+", " ", name.lower()).strip()


def chapter_label(path):
    base = os.path.basename(path)[:-4]
    return base


def build(src):
    reg = load()
    ch = chapter_label(src)
    rows, n_new, n_rec, n_var, n_inf = [], 0, 0, 0, 0
    for ln, raw in enumerate(open(src, encoding="utf-8"), 1):
        raw = raw.rstrip("\n")
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        parts = raw.split("|")
        if len(parts) != 7:
            raise SystemExit("%s:%d: expected 7 fields, got %d: %s" % (src, ln, len(parts), raw[:100]))
        cat, name, sub, where, pri, desc, var = [p.strip() for p in parts]
        key = cat + ":" + norm(name)
        a = reg["assets"].get(key)
        new = a is None
        if new:
            reg["next"][cat] = reg["next"].get(cat, 0) + 1
            a = {"id": "%s-%04d" % (cat, reg["next"][cat]), "name": name, "cat": cat, "sub": sub, "first": ch,
                 "where": where, "pri": pri, "desc": desc, "chapters": [], "vars": [], "inferred": "[inferred" in desc,
                 "nvar": 0}
            reg["assets"][key] = a
            n_new += 1
        else:
            n_rec += 1
            order = "HSB"
            if order.index(pri) < order.index(a["pri"]):
                a["pri"] = pri
            a.setdefault("notes", [])
            if desc and desc not in a["notes"] and desc != a["desc"]:
                a["notes"].append("%s: %s" % (ch, desc))
        if ch not in a["chapters"]:
            a["chapters"].append(ch)
        vrows = []
        for v in [x.strip() for x in var.split(";") if x.strip()]:
            inferred = v.startswith("?")
            v = v.lstrip("?").strip()
            reason = ""
            if "~" in v:
                v, reason = [t.strip() for t in v.split("~", 1)]
            if any(e["text"] == v for e in a["vars"]):
                continue
            a["nvar"] += 1
            vid = "%s.v%02d" % (a["id"], a["nvar"])
            e = {"id": vid, "text": v, "chapter": ch, "inferred": inferred, "reason": reason}
            a["vars"].append(e)
            vrows.append(e)
            n_var += 1
            n_inf += 1 if inferred else 0
        if new and a["inferred"]:
            n_inf += 1
        rows.append((a, new, vrows, pri, desc))
    json.dump(reg, open(REG, "w"), indent=1, ensure_ascii=False)
    out = os.path.join(HERE, "ledgers", ch + ".md")
    with open(out, "w", encoding="utf-8") as f:
        f.write("# Inventory ledger - %s\n\nSource: `docs/Story` (see `docs/Assets/Inventory/README.md`). Status of every entry: not started.\n"
                "Entries marked *recurs* keep the ID of their first chapter; only this chapter's variations are listed.\n\n" % ch)
        f.write("| ID | Name | Category / sub | First seen here | Priority | Status | Description |\n| --- | --- | --- | --- | --- | --- | --- |\n")
        for a, new, vrows, pri, desc in rows:
            tag = "" if new else " *(recurs; first %s)*" % a["first"]
            f.write("| %s | %s%s | %s / %s | %s | %s | not started | %s |\n" % (
                a["id"], a["name"], tag, a["cat"], a["sub"], a["where"], PRI[pri], desc))
            for e in vrows:
                mark = " [inferred: %s]" % e["reason"] if e["inferred"] else ""
                f.write("| %s | %s - %s%s | %s / %s (variation) | %s | %s | not started | |\n" % (
                    e["id"], a["name"], e["text"], mark, a["cat"], a["sub"], a["where"], PRI[a["pri"]]))
    print("%s: %d new assets, %d recurring, %d new variation lines, %d inferred; registry %d assets" % (
        ch, n_new, n_rec, n_var, n_inf, len(reg["assets"])))
    return n_new, n_rec, n_var


def master():
    reg = load()
    bycat = {}
    for a in reg["assets"].values():
        bycat.setdefault(a["cat"], []).append(a)
    os.makedirs(os.path.join(HERE, "master"), exist_ok=True)
    counts = {}
    for cat, lst in sorted(bycat.items()):
        lst.sort(key=lambda a: a["id"])
        nv = sum(len(a["vars"]) for a in lst)
        ninf = sum(1 for a in lst if a["inferred"]) + sum(1 for a in lst for v in a["vars"] if v["inferred"])
        counts[cat] = {"assets": len(lst), "variations": nv, "line_items": len(lst) + nv, "inferred": ninf}
        with open(os.path.join(HERE, "master", cat + ".md"), "w", encoding="utf-8") as f:
            f.write("# Master catalog - %s (%d assets, %d variations)\n\nOne ID per unique asset; every chapter listed; variations kept.\n\n" % (cat, len(lst), nv))
            f.write("| ID | Name | Sub | Priority | First | Chapters | Description | Status |\n| --- | --- | --- | --- | --- | --- | --- | --- |\n")
            for a in lst:
                chs = ", ".join(c.replace("Ch", "") for c in a["chapters"])
                notes = (" Also: " + " / ".join(a.get("notes", []))) if a.get("notes") else ""
                f.write("| %s | %s | %s | %s | %s (%s) | %s | %s%s | not started |\n" % (
                    a["id"], a["name"], a["sub"], PRI[a["pri"]], a["first"], a["where"], chs, a["desc"], notes))
                for v in a["vars"]:
                    mark = " [inferred: %s]" % v["reason"] if v["inferred"] else ""
                    f.write("| %s | %s - %s%s | %s | %s | %s | %s | variation | not started |\n" % (
                        v["id"], a["name"], v["text"], mark, a["sub"], PRI[a["pri"]], v["chapter"], v["chapter"].replace("Ch", "")))
    json.dump(counts, open(os.path.join(HERE, "master", "counts.json"), "w"), indent=1)
    tot = {k: sum(c[k] for c in counts.values()) for k in ("assets", "variations", "line_items", "inferred")}
    with open(os.path.join(HERE, "master", "INDEX.md"), "w", encoding="utf-8") as f:
        f.write("# Master catalog index\n\n| Category | Unique assets | Variations | Line items | [inferred] |\n| --- | --- | --- | --- | --- |\n")
        for cat, c in counts.items():
            f.write("| %s | %d | %d | %d | %d |\n" % (cat, c["assets"], c["variations"], c["line_items"], c["inferred"]))
        f.write("| **Total** | %(assets)d | %(variations)d | %(line_items)d | %(inferred)d |\n" % tot)
    print(json.dumps(counts), tot)


def rebuild_all():
    """Rebuild every ledger and the registry from src/*.txt in chapter order (idempotent)."""
    if os.path.exists(REG):
        os.remove(REG)
    import glob
    for p in sorted(glob.glob(os.path.join(HERE, "src", "Ch*.txt"))):
        build(p)


if __name__ == "__main__":
    if sys.argv[1] == "--all":
        rebuild_all()
    elif sys.argv[1] == "--master":
        master()
    else:
        build(sys.argv[1])
