"""Lists every static mesh of the Elite Bundle packs that sit in the project, with its real size (run inside the editor).

    UnrealEditor <uproject> -ExecCmds="py <abs path>/Tools/elite_library.py"

Packs are the top-level /Game folders other than the project's own (BIB, Weather, Greybox, ...): copy each pack folder from the
bundle to Content/<PackName> unchanged (a pack's assets refer to each other by that path; Content/* except BIB and Weather is
git-ignored, so nothing here is committed). Writes Saved/EliteLibrary.json, the input of Tools/elite_autodress.py:

    {"version": 1, "packs": {"MedievalVOL9": {"meshes": 212, "materials": 95}}, "meshes": [{"path", "pack", "name", "size": [x, y, z]}]}

Environment: BIB_ELITE_PACKS="Dockhouse,Boneyard" limits the scan to those folders. Log: Saved/EliteLibrary.log (ends STATUS OK / FAILED).
"""

import json
import os
import time
import traceback

import unreal

OWN = {"BIB", "Weather", "Greybox", "Developers", "Collections", "__ExternalActors__", "__ExternalObjects__"}
LOG = []


def log(*parts):
    line = " ".join(str(p) for p in parts)
    LOG.append(line)
    unreal.log("[elite_library] " + line)


def pack_names():
    only = [p.strip() for p in os.environ.get("BIB_ELITE_PACKS", "").split(",") if p.strip()]
    names = []
    for entry in unreal.EditorAssetLibrary.list_assets("/Game", recursive=False, include_folder=True):
        name = entry.rstrip("/").split("/")[-1]
        if entry.endswith("/") and name not in OWN and (not only or name in only):
            names.append(name)
    return sorted(names)


def class_name(data):
    path = getattr(data, "asset_class_path", None)
    return str(path.asset_name) if path is not None else str(getattr(data, "asset_class", ""))


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    packs, meshes, started = {}, [], time.time()
    names = pack_names()
    if not names:
        raise RuntimeError("no pack folders under /Game: copy the bundle's pack folders to Content/<PackName> first")
    for pack in names:
        counts = {"meshes": 0, "materials": 0, "skipped": 0}
        for data in registry.get_assets_by_path("/Game/" + pack, recursive=True):
            kind = class_name(data)
            if kind in ("MaterialInstanceConstant", "Material"):
                counts["materials"] += 1
                continue
            if kind != "StaticMesh":
                continue
            package = str(data.package_name)
            mesh = unreal.load_asset(package)
            if not isinstance(mesh, unreal.StaticMesh):
                counts["skipped"] += 1
                continue
            box = mesh.get_bounding_box()
            meshes.append({"path": package, "pack": pack, "name": str(data.asset_name),
                           "size": [round(box.max.x - box.min.x, 1), round(box.max.y - box.min.y, 1), round(box.max.z - box.min.z, 1)]})
            counts["meshes"] += 1
        packs[pack] = counts
        log("pack %s: %d meshes, %d materials, %d unreadable (%.0f s)" % (pack, counts["meshes"], counts["materials"],
                                                                         counts["skipped"], time.time() - started))
    out = os.path.join(unreal.Paths.project_saved_dir(), "EliteLibrary.json")
    with open(out, "w") as handle:
        json.dump({"version": 1, "packs": packs, "meshes": meshes}, handle, indent=1, sort_keys=True)
    log("wrote %s: %d meshes in %d packs" % (out, len(meshes), len(packs)))


def run():
    status = "FAILED"
    try:
        main()
        status = "OK"
    except Exception:
        log(traceback.format_exc())
    log("STATUS " + status)
    with open(os.path.join(unreal.Paths.project_saved_dir(), "EliteLibrary.log"), "w") as handle:
        handle.write("\n".join(LOG) + "\n")


run()
