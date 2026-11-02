"""Imports UI art PNGs as textures without rebuilding the rest of /Game/BIB (Tools/build_assets.py rebuilds everything).
Same settings as build_assets.py import_art(): T_<file name> under /Game/BIB/UI/Icons or /Game/BIB/UI/Portraits, no mips,
editor-icon compression, UI texture group. Writes Saved/ImportArt.log.

    BIB_ART_ONLY="gang_coin,friends" UnrealEditor-Cmd <BoldInBeautyDemo.uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
    (without BIB_ART_ONLY every PNG under Art/Icons and Art/Portraits is (re)imported)
"""

import glob
import os

import unreal

UI = "/Game/BIB/UI"
LOG = []


def log(text):
    LOG.append(text)
    unreal.log("[import_art] " + text)


def main():
    only = [name.strip() for name in os.environ.get("BIB_ART_ONLY", "").split(",") if name.strip()]
    project = unreal.Paths.project_dir()
    tasks = []
    for source, destination in (("Art/Icons", UI + "/Icons"), ("Art/Portraits", UI + "/Portraits")):
        for path in sorted(glob.glob(os.path.join(project, source, "*.png"))):
            name = os.path.splitext(os.path.basename(path))[0]
            if only and name not in only:
                continue
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", path)
            task.set_editor_property("destination_path", destination)
            task.set_editor_property("destination_name", "T_" + name)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    count = 0
    for task in tasks:
        for asset_path in task.get_editor_property("imported_object_paths"):
            texture = unreal.load_asset(asset_path)
            if isinstance(texture, unreal.Texture2D):
                texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
                texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
                texture.set_editor_property("never_stream", True)
                unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
                count += 1
                log("imported " + asset_path)
    log("STATUS %s: %d of %d textures" % ("OK" if count == len(tasks) and count > 0 else "FAILED", count, len(tasks)))


try:
    main()
except Exception as error:
    log("STATUS FAILED: %s" % error)
with open(os.path.join(unreal.Paths.project_saved_dir(), "ImportArt.log"), "w") as handle:
    handle.write("\n".join(LOG) + "\n")
