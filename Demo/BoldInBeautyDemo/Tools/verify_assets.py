"""Reads back /Game/BIB after build_assets.py and checks the hooks. Writes Saved/VerifyAssets.log."""
import os
import unreal

OUT = []
FAIL = []


def check(ok, text):
    OUT.append(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAIL.append(text)


def cdo(path):
    bp = unreal.load_asset(path)
    return unreal.get_default_object(bp.generated_class())


pc = cdo("/Game/BIB/Blueprints/BP_BIBPlayerController")
actions = pc.get_editor_property("button_actions")
check(len(actions) == 18, "controller Button Actions = %d" % len(actions))
check(pc.get_editor_property("default_mapping_context") is not None, "controller Default Mapping Context set")
check(pc.get_editor_property("look_action") is not None, "controller Look Action set")
check(pc.get_editor_property("hud_widget_class") is not None, "controller Hud Widget Class set")
screens = pc.get_editor_property("screen_widget_classes")
check(len(screens) == 2, "controller Screen Widget Classes = %d (%s): results, pause" % (len(screens), ", ".join(sorted(str(k) for k in screens.keys()))))
# The 2D lobby (WBP_Hub / UBIBHubWidget) was deleted on 2026-10-07 (owner request: the Ark is the hub).
check(not unreal.EditorAssetLibrary.does_asset_exist("/Game/BIB/UI/WBP_Hub"), "WBP_Hub (the deleted 2D lobby) is not built any more")

# UI art
icons = ["gold", "bytes", "hearts", "diamond", "ticket_regular", "ticket_special", "nav_operator", "nav_team", "nav_lobby", "nav_shop",
         "nav_revolution", "nav_campaign", "nav_summon", "quest", "mail", "settings", "profile", "back", "stage", "lock"]
check(all(unreal.EditorAssetLibrary.does_asset_exist("/Game/BIB/UI/Icons/T_" + n) for n in icons), "UI icon textures exist (%d checked)" % len(icons))
names = ["media", "arita", "adapt", "alchemist", "shiraishi", "myrcia", "ishii", "death", "luna", "kuu", "stargazer", "narrator", "gambler",
         "fujita", "timekeeper", "weather", "plack", "jester", "judge", "blood"]
check(all(unreal.EditorAssetLibrary.does_asset_exist("/Game/BIB/UI/Portraits/T_" + n) and
          unreal.EditorAssetLibrary.does_asset_exist("/Game/BIB/UI/Portraits/T_%s_head" % n) for n in names), "20 operator portraits (full + head) exist")

imc = unreal.load_asset("/Game/BIB/Input/IMC_BIB_Default")
mappings = imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
check(len(mappings) == 19, "IMC mappings = %d: %s" % (len(mappings), ", ".join(
    "%s=%s" % (m.get_editor_property("action").get_name(), m.get_editor_property("key").get_editor_property("key_name")) for m in mappings)))

gm = cdo("/Game/BIB/Blueprints/BP_BIBGameMode")
check(gm.get_editor_property("default_pawn_class").get_name() == "BP_BIBPlayerCharacter_C", "game mode Default Pawn")
check(gm.get_editor_property("player_controller_class").get_name() == "BP_BIBPlayerController_C", "game mode Player Controller")
check(len(gm.get_editor_property("enemy_classes")) == 4, "game mode Enemy Classes = 4")
check("L_Hub" in str(gm.get_editor_property("hub_map")), "game mode Hub Map = %s" % gm.get_editor_property("hub_map"))
stages = gm.get_editor_property("stage_maps")
check(len(stages) == 8 and all(("Stage%d" % (i + 1)) in str(stages[i]) for i in range(8)), "game mode Stage Maps = %d (levels 1-8 in order)" % len(stages))
for kind in ("Grunt", "Brute", "MiniBoss", "Boss"):
    check(cdo("/Game/BIB/Blueprints/BP_Enemy_" + kind).get_editor_property("kind").name.replace("_", "").lower() == kind.lower(),
          "BP_Enemy_%s Kind = %s" % (kind, cdo("/Game/BIB/Blueprints/BP_Enemy_" + kind).get_editor_property("kind")))

tint = unreal.load_asset("/Game/BIB/Materials/M_BIBTint")
params = [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(tint)] if tint else []
check(tint is not None and "Color" in params, "M_BIBTint exists with vector parameter Color (%s)" % params)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# sections per level from BIB::StagePlan: Chapter 1 has 8 levels; level 8 ends with the mini-boss and the boss
SECTIONS = {1: 2, 2: 2, 3: 3, 4: 3, 5: 3, 6: 4, 7: 4, 8: 5}
for stage in range(1, 9):
    levels.load_level("/Game/BIB/Maps/L_Chapter1_Stage%d" % stage)
    actors = actors_sub.get_all_level_actors()
    sections = [a for a in actors if a.get_class().get_name() == "BP_BIBSection_C"]
    npcs = [a for a in actors if a.get_class().get_name() == "BP_BIBStoryNPC_C"]
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    check(len(sections) == SECTIONS[stage] and len(npcs) == 1 and len(starts) == 1, "stage %d: %d sections, %d NPC, %d PlayerStart" % (stage, len(sections), len(npcs), len(starts)))
    bosses = sum(s.get_editor_property("bosses") for s in sections)
    check(bosses == (1 if stage == 8 else 0), "stage %d: %d chapter boss(es)" % (stage, bosses))
    check(len(npcs[0].get_editor_property("dialogue_lines")) == 0, "stage %d: NPC DialogueLines empty (placeholder)" % stage)
    for section in sorted(sections, key=lambda s: s.get_editor_property("section_index")):
        door = section.get_editor_property("entry_door")
        extent = section.get_editor_property("bounds").get_unscaled_box_extent()
        loc = section.get_actor_location()
        check(door is not None and section.get_editor_property("stage_number") == stage,
              "stage %d section %d: door=%s grunts=%d brutes=%d mini=%d boss=%d extent=(%.0f,%.0f,%.0f) bottom z=%.0f" % (
                  stage, section.get_editor_property("section_index"), door.get_actor_label() if door else None,
                  section.get_editor_property("grunts"), section.get_editor_property("brutes"),
                  section.get_editor_property("mini_bosses"), section.get_editor_property("bosses"),
                  extent.x, extent.y, extent.z, loc.z - extent.z))
levels.load_level("/Game/BIB/Maps/L_Hub")
names = sorted(a.get_class().get_name() for a in actors_sub.get_all_level_actors())
for wanted in ("DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog", "PlayerStart"):
    check(wanted in names, "L_Hub has " + wanted)

OUT.append("RESULT " + ("OK" if not FAIL else "FAIL (%d)" % len(FAIL)))
with open(os.path.join(unreal.Paths.project_saved_dir(), "VerifyAssets.log"), "w") as handle:
    handle.write("\n".join(OUT) + "\n")
