"""Imports the motion pack's atlas pages (Saved/MotionPack/<id>/page_NN.png, from Tools/motion_export.py) as UI textures
/Game/BIBMotion/<id>/T_<id>_NN (no mips, never streamed, UI group, compressed with alpha). Run by Tools/import_motion_pack.sh:
    UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file>
Writes Saved/MotionImport.log (last line STATUS OK | STATUS FAIL).
"""
import glob
import os
import traceback

import unreal

DEMO = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(DEMO, "Saved", "MotionPack")
ROOT = "/Game/BIBMotion"
LOG = []


def log(text):
    LOG.append(text)
    unreal.log("[motion_import] " + text)


def main():
    if unreal.EditorAssetLibrary.does_directory_exist(ROOT):
        unreal.EditorAssetLibrary.delete_directory(ROOT)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    count = 0
    for folder in sorted(glob.glob(os.path.join(SRC, "*"))):
        pid = os.path.basename(folder)
        tasks = []
        for page in sorted(glob.glob(os.path.join(folder, "page_*.png"))):
            n = int(os.path.basename(page)[5:7])
            task = unreal.AssetImportTask()
            task.filename = page
            task.destination_path = "%s/%s" % (ROOT, pid)
            task.destination_name = "T_%s_%02d" % (pid.replace("-", "_"), n)
            task.automated = True
            task.replace_existing = True
            task.save = False
            tasks.append(task)
        tools.import_asset_tasks(tasks)
        for task in tasks:
            for path in task.imported_object_paths:
                tex = unreal.load_asset(path)
                if not isinstance(tex, unreal.Texture2D):
                    continue
                # mips let phones drop a level (Config/DefaultDeviceProfiles.ini); the UI draws the top level on desktop
                tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
                tex.set_editor_property("never_stream", True)
                tex.set_editor_property("srgb", True)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
                unreal.EditorAssetLibrary.save_loaded_asset(tex)
                count += 1
        log("%s: %d page(s)" % (pid, len(tasks)))
    log("imported %d atlas pages" % count)
    return count > 0


try:
    ok = main()
    LOG.append("STATUS " + ("OK" if ok else "FAIL"))
except Exception:
    LOG.append(traceback.format_exc())
    LOG.append("STATUS FAIL")
with open(os.path.join(DEMO, "Saved", "MotionImport.log"), "w") as handle:
    handle.write("\n".join(LOG) + "\n")
