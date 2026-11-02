"""Imports the two Fab kits (SCI-FI BUILDINGS CITY KIT 1 and 2) as untextured static meshes under /Game/Greybox/Kits.

Called by build_greybox.py. Source FBX files come from the Fab vault cache (override with BIB_KIT_ROOT). Meshes that are
already imported are kept, so a rebuild only re-imports what is missing. Writes Saved/GreyboxKitBounds.json (size of every
mesh in cm) so the layout code and a person can pick buildings by size.
"""

import json
import os
import re

import unreal

KITS_ROOT = "/Game/Greybox/Kits"
VAULT = os.environ.get("BIB_KIT_ROOT", "<local-home>/UnrealEngine/Launcher/VaultCache/FabLibrary")
KIT_DIRS = {
    "K1": "SCI-FI_BUILDINGS_CITY_KIT-427297da",
    "K2": "SCI-FI_BUILDINGS_CITY_KIT_V_2-5449f412",
}
CATEGORY = {
    "cityscape": "Cityscape", "construction": "Construction", "corporation": "Corporation", "factories": "Factory",
    "high-rise buildings": "HighRise", "skyscraper": "Skyscraper", "small cottage": "Cottage",
}


def _fbx_files(kit_dir):
    found = []
    for root, _, files in os.walk(os.path.join(VAULT, kit_dir)):
        for name in files:
            if name.lower().endswith(".fbx"):
                found.append(os.path.join(root, name))
    return sorted(found)


def asset_name(kit, path):
    folder = os.path.basename(os.path.dirname(path)).lower()
    stem = re.sub(r"[^0-9A-Za-z]+", "", os.path.splitext(os.path.basename(path))[0].replace("skyscraper", "").replace("Construction", ""))
    category = CATEGORY.get(folder)
    return ("SM_%s_%s_%s" % (kit, category, stem)) if category else ("SM_%s_%s" % (kit, stem)), (category or "Buildings")


def _task(source, dest_path, dest_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", dest_path)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = ui.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False)
    task.set_editor_property("options", ui)
    return task


def _complex_collision(mesh):
    """Buildings are climbed, so their collision follows the render mesh exactly."""
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)


def import_kits(log):
    bounds = {}
    for kit, kit_dir in KIT_DIRS.items():
        files = _fbx_files(kit_dir)
        if not files:
            raise RuntimeError("No FBX files for %s under %s (set BIB_KIT_ROOT)" % (kit, os.path.join(VAULT, kit_dir)))
        tasks, wanted = [], []
        for source in files:
            name, category = asset_name(kit, source)
            dest = "%s/%s/%s" % (KITS_ROOT, kit, category)
            wanted.append("%s/%s" % (dest, name))
            if not unreal.EditorAssetLibrary.does_asset_exist("%s/%s" % (dest, name)):
                tasks.append(_task(source, dest, name))
        if tasks:
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        imported = 0
        for path in wanted:
            mesh = unreal.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                log("kit: missing after import", path)
                continue
            _complex_collision(mesh)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
            box = mesh.get_bounding_box()
            lo, hi = box.min, box.max
            bounds[path] = {"min": [lo.x, lo.y, lo.z], "size": [hi.x - lo.x, hi.y - lo.y, hi.z - lo.z]}
            imported += 1
        log("kit %s: %d meshes (%d newly imported)" % (kit, imported, len(tasks)))
    out = os.path.join(unreal.Paths.project_saved_dir(), "GreyboxKitBounds.json")
    with open(out, "w") as handle:
        json.dump(bounds, handle, indent=1, sort_keys=True)
    return bounds
