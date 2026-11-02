"""Imports the Tripo team-plan GLB exports as static meshes (with their PBR textures/materials).

  Props       BB_<slug>__<id8>.glb   -> /Game/BB/Assets/SM_BB_<slug>_<id8>
  Characters  BB_<Name>.glb (8k)      -> /Game/BB/Characters/<Name>/SM_BBChar_<Name>   (whole-body reference mesh, not rigged)

Source folder: BIB_TRIPO_DIR (default <documentation root>/art/tripo-assets/2026-10-13-team/glb).
Filters: BIB_TRIPO_ONLY=props|characters, BIB_TRIPO_MATCH=<substring list, comma separated>.
Writes Saved/ImportTripoTeam.log and Saved/ImportTripoTeam.json (asset path, source file, bounds in cm per asset).

    UnrealEditor-Cmd <BoldInBeautyDemo.uproject> -run=pythonscript -script=<this file> -unattended -nullrhi -stdout
"""

import glob
import json
import os
import re

import unreal

ASSETS = "/Game/BB/Assets"
CHARS = "/Game/BB/Characters"
LOG = []
RESULT = []
PROP_RE = re.compile(r"^BB_(?P<slug>.+)__(?P<id>[0-9a-f]{8})\.glb$")


def log(text):
    LOG.append(text)
    unreal.log("[import_tripo_team] " + text)


def source_dir():
    env = os.environ.get("BIB_TRIPO_DIR")
    if env:
        return env
    here = os.path.abspath(unreal.Paths.project_dir())
    probe = here
    for _ in range(8):
        candidate = os.path.join(probe, "art", "tripo-assets", "2026-10-13-team", "glb")
        if os.path.isdir(candidate):
            return candidate
        probe = os.path.dirname(probe)
    return os.path.expanduser("~/Documents/BoldInBeauty-Project/art/tripo-assets/2026-10-13-team/glb")


def plan(folder):
    only = os.environ.get("BIB_TRIPO_ONLY", "")
    match = [m.strip().lower() for m in os.environ.get("BIB_TRIPO_MATCH", "").split(",") if m.strip()]
    jobs = []
    for path in sorted(glob.glob(os.path.join(folder, "BB_*.glb"))):
        base = os.path.basename(path)
        m = PROP_RE.match(base)
        if m:
            kind, name = "props", "SM_BB_%s_%s" % (m.group("slug"), m.group("id"))
            dest = ASSETS
        else:
            stem = base[3:-4]
            kind, name = "characters", "SM_BBChar_%s" % stem
            dest = "%s/%s" % (CHARS, stem)
        if only and only != kind:
            continue
        if match and not any(t in base.lower() for t in match):
            continue
        jobs.append((path, dest, name, kind))
    return jobs


def make_task(path, dest, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    return task


def main():
    folder = source_dir()
    jobs = plan(folder)
    log("source %s, %d files" % (folder, len(jobs)))
    ok = 0
    for path, dest, name, kind in jobs:
        task = make_task(path, dest, name)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        paths = [str(p) for p in task.get_editor_property("imported_object_paths")]
        mesh = None
        for p in paths:
            asset = unreal.load_asset(p)
            if isinstance(asset, unreal.StaticMesh):
                mesh = asset
                break
        if mesh is None:
            log("FAILED %s -> %s (objects: %s)" % (os.path.basename(path), dest, paths))
            continue
        box = mesh.get_bounding_box()
        size = [round(box.max.x - box.min.x, 1), round(box.max.y - box.min.y, 1), round(box.max.z - box.min.z, 1)]
        RESULT.append({"kind": kind, "source": os.path.basename(path), "asset": mesh.get_path_name(), "size_cm": size,
                       "objects": len(paths)})
        ok += 1
        log("imported %s %s size_cm=%s" % (kind, mesh.get_path_name(), size))
    log("STATUS %s: %d of %d" % ("OK" if ok == len(jobs) and ok > 0 else "FAILED", ok, len(jobs)))


try:
    main()
except Exception as error:  # keep the log even on failure
    log("STATUS FAILED: %s" % error)
saved = unreal.Paths.project_saved_dir()
with open(os.path.join(saved, "ImportTripoTeam.log"), "w") as handle:
    handle.write("\n".join(LOG) + "\n")
with open(os.path.join(saved, "ImportTripoTeam.json"), "w") as handle:
    json.dump(RESULT, handle, indent=1)
