#!/usr/bin/env python3
"""Catalogues the Elite Bundle so the greybox maps can be dressed from facts, not guesses (owner request 2026-10-07).

Read-only and standard-library only: it walks the folder, counts files by type and category, and writes names and sizes. It
never opens, copies, imports or executes an asset (only the small JSON of .uproject / .uplugin files is parsed, for the engine
version). Run it on the Mac, where the bundle is:

    python3 Tools/elite_scan.py                                  # default root below
    python3 Tools/elite_scan.py "/path/to/Elite_Bundle" --out reports/demo/elite-bundle

Default root: $BIB_ELITE_ROOT, else ~/Documents/BoldInBeauty-Project/Elite_Bundle. Output (names only, safe to commit; the
assets themselves stay out of Git, their licence decides whether they may be shared):

    catalogue.md    what is in it, per pack: formats, size, categories, engine version, archives still to unpack
    catalogue.json  the same as data, plus up to 400 sample mesh / material / texture paths per pack for the mapping

The first level of folders is a "pack". If the bundle is a single wrapper folder, pass the inner folder instead.
"""

import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict

MESH = {".fbx", ".obj", ".gltf", ".glb", ".dae", ".usd", ".usda", ".usdc", ".usdz", ".abc", ".blend", ".ma", ".mb", ".3ds", ".max"}
TEXTURE = {".png", ".tga", ".jpg", ".jpeg", ".exr", ".hdr", ".tif", ".tiff", ".psd", ".bmp", ".dds", ".webp"}
AUDIO = {".wav", ".ogg", ".mp3", ".flac", ".aif", ".aiff"}
VIDEO = {".mp4", ".mov", ".webm", ".avi"}
ARCHIVE = {".zip", ".rar", ".7z", ".tar", ".gz", ".tgz", ".unitypackage"}
UNREAL = {".uasset", ".umap", ".uproject", ".uplugin", ".ubulk", ".uexp", ".upk"}
DOC = {".pdf", ".txt", ".md", ".rtf", ".url", ".html"}
SAMPLE_CAP = 400

# Category by keyword in the lower-cased path (first match per file wins; order matters).
CATEGORIES = [
    ("character", r"character|anime|humanoid|npc|skeletal|\bskin\b|\bhair\b|\bface\b|outfit|armou?r"),
    ("weapon", r"weapon|\bgun\b|\bsword\b|\brifle\b|\bpistol\b"),
    ("vehicle", r"vehicle|\bcar\b|\btruck\b|\bboat\b|\bship\b|\bbike\b|\bbus\b|\btrain\b|\bdrone\b|spaceship"),
    ("building", r"building|house|skyscraper|tower|facade|\bcity\b|\burban\b|architect|\bshop\b|apartment|temple|shrine|castle|ruin"),
    ("interior", r"interior|furniture|\broom\b|office|\bdesk\b|\bchair\b|\bsofa\b|kitchen|bedroom"),
    ("foliage", r"foliage|vegetation|\btree|\bplant|\bgrass|bush|flower|\bpalm|\bleaf|\bleaves|\bvine|moss|bamboo|jungle|forest"),
    ("terrain", r"terrain|landscape|\brock|cliff|boulder|\bsand\b|desert|mountain|\bground\b|\bsoil\b|\bmud\b|pebble|gravel"),
    ("water", r"water|ocean|\bsea\b|\bwave|river|\blake\b|\bpond"),
    ("sky", r"\bsky|cloud|skybox|hdri|atmosphere|\bmoon\b|\bstars?\b|\bsun\b"),
    ("fx", r"niagara|particle|\bfx\b|\bvfx\b|effect|explosion|smoke|\bfire\b|\brain\b|lightning|sparks?"),
    ("props", r"\bprops?\b|crate|barrel|\bbin\b|lamp|bench|sign|fence|barrier|debris|rubble|clutter|pipe|cable|container"),
    ("scifi", r"sci-?fi|cyber|futur|neon|hologram|robot|\bmech|factory|industrial|reactor"),
    ("material", r"material|shader|\bmi_|\bm_|decal|\bpbr\b"),
    ("ui", r"\bui\b|\bhud\b|widget|icon|font"),
    ("audio", r"audio|sound|music|\bsfx\b|\bsfx_"),
    ("level", r"\bmaps?\b|\blevels?\b|\bdemo\b|showcase|example"),
]
CATEGORY_RES = [(name, re.compile(rx)) for name, rx in CATEGORIES]


def classify(rel_path):
    low = rel_path.lower().replace("\\", "/")
    for name, rx in CATEGORY_RES:
        if rx.search(low):
            return name
    return "uncategorised"


def kind_of(ext):
    for name, group in (("mesh", MESH), ("texture", TEXTURE), ("audio", AUDIO), ("video", VIDEO), ("archive", ARCHIVE),
                        ("unreal", UNREAL), ("doc", DOC)):
        if ext in group:
            return name
    return "other"


def _engine_hint(path):
    """EngineAssociation / EngineVersion from a small .uproject / .uplugin, else None. Parsed as data only."""
    try:
        if os.path.getsize(path) > 256 * 1024:
            return None
        with open(path, "r", encoding="utf-8-sig") as handle:
            data = json.load(handle)
        return str(data.get("EngineAssociation") or data.get("EngineVersion") or "") or None
    except (OSError, ValueError):
        return None


def new_pack(name):
    return {"name": name, "files": 0, "bytes": 0, "kinds": Counter(), "exts": Counter(), "categories": Counter(),
            "engine": [], "has_content_dir": False, "licence_files": [], "archives": [], "samples": defaultdict(list),
            "unreal_assets": 0, "skipped": 0}


def scan(root):
    """Walks `root`; returns {"root", "packs": [...], "loose": pack for files directly in root, "errors": n}."""
    root = os.path.abspath(os.path.expanduser(root))
    if not os.path.isdir(root):
        raise SystemExit("not a folder: %s" % root)
    packs = {}
    errors = [0]

    def onerror(_):
        errors[0] += 1

    for current, dirs, files in os.walk(root, onerror=onerror, followlinks=False):
        dirs.sort()
        rel_dir = os.path.relpath(current, root)
        parts = [] if rel_dir == "." else rel_dir.replace("\\", "/").split("/")
        pack_name = parts[0] if parts else "(loose files in root)"
        pack = packs.setdefault(pack_name, new_pack(pack_name))
        if "Content" in dirs and parts:
            pack["has_content_dir"] = True
        for name in sorted(files):
            if name.startswith(".") or name in ("Thumbs.db", "Desktop.ini"):
                continue
            full = os.path.join(current, name)
            try:
                size = os.lstat(full).st_size
            except OSError:
                pack["skipped"] += 1
                continue
            ext = os.path.splitext(name)[1].lower()
            kind = kind_of(ext)
            rel = "/".join(parts[1:] + [name]) if parts else name
            pack["files"] += 1
            pack["bytes"] += size
            pack["kinds"][kind] += 1
            pack["exts"][ext or "(none)"] += 1
            if kind == "unreal":
                pack["unreal_assets"] += 1
            if ext in (".uproject", ".uplugin"):
                hint = _engine_hint(full)
                if hint and hint not in pack["engine"]:
                    pack["engine"].append(hint)
            if kind == "archive":
                pack["archives"].append({"path": rel, "bytes": size})
            if re.search(r"licen[cs]e|eula|terms|readme|credits", name, re.I) and kind == "doc":
                pack["licence_files"].append(rel)
            if kind in ("mesh", "unreal", "texture", "audio"):
                category = classify(rel)
                pack["categories"][category] += 1
                bucket = "mesh" if kind == "mesh" else kind
                if kind == "unreal":
                    low = name.lower()
                    # Unreal naming convention: SM_ static mesh, SK_ skeletal, M_/MI_ material, T_ texture, NS_ Niagara, BP_ blueprint
                    bucket = ("mesh" if low.startswith(("sm_", "sk_")) else "material" if low.startswith(("m_", "mi_", "mf_"))
                              else "texture" if low.startswith("t_") else "fx" if low.startswith(("ns_", "p_")) else
                              "blueprint" if low.startswith("bp_") else "map" if ext == ".umap" else "unreal_other")
                if len(pack["samples"][bucket]) < SAMPLE_CAP:
                    pack["samples"][bucket].append({"path": rel, "category": category, "bytes": size})
    result = [packs[k] for k in sorted(packs)]
    for pack in result:
        pack["kinds"] = dict(pack["kinds"])
        pack["exts"] = dict(pack["exts"].most_common(25))
        pack["categories"] = dict(pack["categories"].most_common())
        pack["samples"] = dict(pack["samples"])
    return {"root": root, "packs": result, "errors": errors[0]}


def human(n):
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if n < 1024 or unit == "TB":
            return "%.0f %s" % (n, unit) if unit == "B" else "%.1f %s" % (n, unit)
        n /= 1024.0


def verdict(pack):
    """One line on how the pack can be brought into the project."""
    kinds = pack["kinds"]
    if pack["unreal_assets"] and (pack["has_content_dir"] or kinds.get("unreal", 0) > 5):
        return "Unreal-native (.uasset): copy its Content folder into the project (Migrate), then reference /Game/ paths"
    if kinds.get("mesh"):
        return "raw meshes (%s): import through Tools/greybox_import.py-style tasks, textures and materials rebuilt" % ", ".join(
            e for e in pack["exts"] if e in MESH)
    if kinds.get("archive") and not (kinds.get("mesh") or pack["unreal_assets"]):
        return "archives only: unpack first, then scan again"
    if kinds.get("texture"):
        return "textures only: material source (needs world-aligned or tiled materials built from them)"
    return "no importable 3D or Unreal content found"


def render_markdown(data):
    total_files = sum(p["files"] for p in data["packs"])
    total_bytes = sum(p["bytes"] for p in data["packs"])
    lines = ["# Elite Bundle catalogue", "",
             "Scanned folder: `%s` (%d packs, %d files, %s, %d unreadable folders). Names and sizes only; nothing was copied or opened."
             % (data["root"], len(data["packs"]), total_files, human(total_bytes), data["errors"]), "",
             "| Pack | Files | Size | Meshes | Textures | Unreal assets | Archives | Engine | How to bring it in |",
             "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |"]
    for p in data["packs"]:
        k = p["kinds"]
        lines.append("| %s | %d | %s | %d | %d | %d | %d | %s | %s |" % (
            p["name"], p["files"], human(p["bytes"]), k.get("mesh", 0), k.get("texture", 0), p["unreal_assets"],
            k.get("archive", 0), ", ".join(p["engine"]) or "-", verdict(p)))
    for p in data["packs"]:
        lines += ["", "## " + p["name"], "",
                  "- %d files, %s. Types: %s." % (p["files"], human(p["bytes"]), ", ".join("%s %d" % kv for kv in sorted(p["kinds"].items()))),
                  "- Extensions: %s." % ", ".join("%s %d" % kv for kv in p["exts"].items()),
                  "- Categories (meshes, Unreal assets, textures, audio): %s." % (", ".join("%s %d" % kv for kv in p["categories"].items()) or "none"),
                  "- Engine version hints: %s." % (", ".join(p["engine"]) or "none found"),
                  "- Licence / readme files: %s." % (", ".join(p["licence_files"][:6]) or "none found")]
        if p["archives"]:
            lines.append("- Archives to unpack first: %s." % ", ".join("`%s` (%s)" % (a["path"], human(a["bytes"])) for a in p["archives"][:8]))
        for bucket in ("mesh", "material", "texture"):
            sample = p["samples"].get(bucket, [])[:6]
            if sample:
                lines.append("- First %s: %s." % (bucket + "es" if bucket == "mesh" else bucket + "s", ", ".join("`%s`" % s["path"] for s in sample)))
    return "\n".join(lines) + "\n"


def main(argv=None):
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("root", nargs="?", default=os.environ.get(
        "BIB_ELITE_ROOT", "~/Documents/BoldInBeauty-Project/Elite_Bundle"))
    parser.add_argument("--out", default=os.path.normpath(os.path.join(here, "..", "..", "..", "reports", "demo", "elite-bundle")))
    args = parser.parse_args(argv)
    data = scan(args.root)
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "catalogue.json"), "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=1, sort_keys=True)
    markdown = render_markdown(data)
    with open(os.path.join(args.out, "catalogue.md"), "w", encoding="utf-8") as handle:
        handle.write(markdown)
    print(markdown[:3000])
    print("written: %s" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
