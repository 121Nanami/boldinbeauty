"""Builds the greybox maps under /Game/Greybox from the two Fab kits (owner request 2026-10-07):

    /Game/Greybox/Maps/L_NeoTokyo   Neo Tokyo: many buildings and streets, heavy rain and fog, climbing routes
    /Game/Greybox/Maps/L_Ark        the arc: operators' dome, town square + Underworld, civilian area, secret dome
    /Game/Greybox/Maps/L_Ch02_..L_Ch13_   one map per chapter 2-13 (greybox_chapters.py; owner level ideas 2026-10-07)

Separate from build_assets.py (which wipes /Game/BIB): nothing here touches the demo's maps or game mode. Kit meshes are
imported once into /Game/Greybox/Kits (greybox_import.py); materials and maps are rebuilt every run. Both maps override
the game mode with the engine's GameModeBase, so Play spawns a free-flying DefaultPawn (WASD + mouse, E/Q up/down).

    Tools/build_greybox.sh [path/to/UE_5.8]

Writes Saved/BuildGreybox.log (ends with STATUS OK or STATUS FAILED).
"""

import os
import sys
import time
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import greybox_ark       # noqa: E402
import greybox_chapters  # noqa: E402
import greybox_common    # noqa: E402
import greybox_import    # noqa: E402
import greybox_neotokyo  # noqa: E402
import greybox_story     # noqa: E402

LOG = []


def log(*parts):
    line = " ".join(str(p) for p in parts)
    LOG.append(line)
    unreal.log("[build_greybox] " + line)


def main():
    # "neotokyo", "ark", "chapters" (all of 2-13) or a list such as "ch03,ch11" to rebuild only those maps
    only = os.environ.get("BIB_GREYBOX_ONLY", "")
    parts = [p.strip().lower() for p in only.split(",") if p.strip()]
    chapters = [int(p[2:]) for p in parts if p.startswith("ch") and p[2:].isdigit()]
    started = time.time()
    greybox_import.import_kits(log)
    materials = greybox_common.build_materials()
    log("materials: %d instances" % len(materials))
    if not parts or "neotokyo" in parts:
        greybox_neotokyo.build(materials, log)
    if not parts or "ark" in parts:
        greybox_ark.build(materials, log)
    if not parts or "chapters" in parts or [c for c in chapters if c < 14]:
        greybox_chapters.build(materials, log, only=[c for c in chapters if c < 14] or None)
    if not parts or "story" in parts or [c for c in chapters if c >= 14]:
        greybox_story.build(materials, log, only=[c for c in chapters if c >= 14] or None)   # chapters 14-40 (2026-10-11)
    unreal.EditorAssetLibrary.save_directory(greybox_common.GREYBOX, only_if_is_dirty=True, recursive=True)
    log("DONE in %.0f s" % (time.time() - started))


def run():
    status = "FAILED"
    try:
        main()
        status = "OK"
    except Exception:
        log(traceback.format_exc())
    log("STATUS " + status)
    with open(os.path.join(unreal.Paths.project_saved_dir(), "BuildGreybox.log"), "w") as handle:
        handle.write("\n".join(LOG) + "\n")


run()
