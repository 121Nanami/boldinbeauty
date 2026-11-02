"""Checks the greybox maps built by build_greybox.py and takes editor-viewport screenshots.

    UnrealEditor <BoldInBeautyDemo.uproject> -ExecCmds="py <this file>"

For each map: structural checks by actor tag (the owner's required pieces), a PIE start that must spawn the free-fly pawn
on the PlayerStart above solid ground, then screenshots from fixed cameras to Saved/Greybox/<map>_<view>.png.
Writes Saved/VerifyGreybox.log (ends with RESULT PASS or RESULT FAIL) and quits the editor.
"""

import os
import traceback

import unreal

LOG, FAILS = [], []
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Greybox")
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# (tag, minimum, maximum or None)
CHECKS = {
    "/Game/Greybox/Maps/L_NeoTokyo": [
        ("Building", 100, None), ("Climbable", 1000, None), ("ClimbRoute", 500, None), ("RooftopBridge", 30, None),
        ("NeonSignPlaceholder", 150, None), ("BillboardPlaceholder", 5, None), ("HoloAdPlaceholder", 4, 4), ("RainPlaceholder", 1, 1),
        ("CarPlaceholder", 10, None),
    ],
    "/Game/Greybox/Maps/L_Ark": [
        ("Catwalk", 3, 3),   # the two exit catwalks + the secret catwalk
        ("Government", 1, 1), ("Armoury", 1, 1), ("Resistance", 1, 1), ("ResistanceMark", 1, 1), ("ToSurface", 16, None),
        ("UnderworldLadder", 8, 8), ("TradeStall", 5, None), ("Mansion", 30, None), ("FakeTree", 100, None), ("FakeGrass", 1, 1),
        ("FakeSky", 1, 1), ("SecretStaircase", 500, 500), ("TownBuilding", 40, None), ("AreaMarker", 5, 5),
    ],
}
VIEWS = {
    "/Game/Greybox/Maps/L_NeoTokyo": [
        ("crossing", (-2500, -2500, 350), (8, 45, 0)),
        ("street", (-16400, -28000, 250), (3, 90, 0)),
        ("rooftops", (-26000, -26000, 9000), (-18, 45, 0)),
        ("overview", (0, -60000, 40000), (-35, 90, 0)),
    ],
    "/Game/Greybox/Maps/L_Ark": [
        ("rollcall", (-6000, -6000, 900), (5, 45, 0)),
        ("rooms", (-8200, 0, 700), (-5, 160, 0)),
        ("catwalks", (4000, 0, 300), (-3, 0, 0)),
        ("townsquare", (36000, -15000, 4000), (-10, 35, 0)),
        ("underworld", (35000, -18000, -3600), (-5, 35, 0)),
        ("civilian", (77000, -18000, 3500), (-12, 40, 0)),
        ("secret", (128000, -32000, 9000), (10, 60, 0)),
        ("overview", (50000, -90000, 50000), (-30, 90, 0)),
    ],
}


def log(text):
    LOG.append(text)
    unreal.log("[verify_greybox] " + text)


def check(ok, text):
    log(("PASS " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def tag_count(actors, tag):
    name = unreal.Name(tag)
    return sum(1 for a in actors if name in a.tags)


# Kit buildings must stand on their ground: (tag, ground z).
GROUNDED = {
    "/Game/Greybox/Maps/L_NeoTokyo": [("Building", 15.0)],
    "/Game/Greybox/Maps/L_Ark": [("TownBuilding", 0.0), ("Mansion", 6.0), ("Government", 200.0), ("Armoury", 0.0), ("Resistance", 0.0)],
}
# Hidden in the editor only for overview shots (cavern rock, fog, rain volume).
OVERVIEW_HIDE = ("RockCeiling", "RockWall", "HeavyFog", "HeightFog", "RAIN_VOLUME")


def grounded(path):
    actors = EAS.get_all_level_actors()
    for tag, ground in GROUNDED[path]:
        name = unreal.Name(tag)
        bad = []
        for a in actors:
            if name in a.tags and isinstance(a, unreal.StaticMeshActor):
                origin, extent = a.get_actor_bounds(False)
                if abs(origin.z - extent.z - ground) > 30:
                    bad.append("%s %.0f" % (a.get_actor_label(), origin.z - extent.z))
        check(not bad, "%s: every %s stands on z=%.0f (%d off: %s)" % (path.rsplit("/", 1)[1], tag, ground, len(bad), ", ".join(bad[:5])))


def structural(path):
    actors = EAS.get_all_level_actors()
    floor = 8000 if path.endswith("L_NeoTokyo") else 1000   # owner 2026-10-07: Neo Tokyo at least 8,000 pieces
    check(len(actors) >= floor, "%s: %d actors (want %d+)" % (path, len(actors), floor))
    for tag, lo, hi in CHECKS[path]:
        n = tag_count(actors, tag)
        check(n >= lo and (hi is None or n <= hi), "%s: tag %s = %d (want %d..%s)" % (path.rsplit("/", 1)[1], tag, n, lo, hi))
    empty = [a.get_actor_label() for a in actors if isinstance(a, unreal.StaticMeshActor) and not a.static_mesh_component.static_mesh]
    check(not empty, "every mesh actor has a mesh (%d without)" % len(empty))
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    check(len(starts) == 1, "one PlayerStart")
    mode = UES.get_editor_world().get_world_settings().get_editor_property("default_game_mode")
    check(mode is not None and mode.get_name() == "GameModeBase", "game mode override GameModeBase (%s)" % (mode.get_name() if mode else None))
    if starts:
        loc = starts[0].get_actor_location()
        hit = unreal.SystemLibrary.line_trace_single(UES.get_editor_world(), loc, loc - unreal.Vector(0, 0, 2000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        check(hit is not None and hit.to_tuple()[0], "solid ground under the PlayerStart")


class Runner:
    def __init__(self):
        self.steps = self.script()
        self.wait = 0
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, _dt):
        if self.wait > 0:
            self.wait -= 1
            return
        try:
            self.wait = next(self.steps)
        except StopIteration:
            self.finish()
        except Exception:
            log(traceback.format_exc())
            FAILS.append("exception")
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log("RESULT " + ("PASS" if not FAILS else "FAIL (%d)" % len(FAILS)))
        with open(os.path.join(unreal.Paths.project_saved_dir(), "VerifyGreybox.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        unreal.SystemLibrary.quit_editor()

    def script(self):
        os.makedirs(OUT, exist_ok=True)
        for path in CHECKS:
            check(LES.load_level(path), "load " + path)
            yield 30
            structural(path)
            grounded(path)
            # PIE: the engine's DefaultPawn must spawn on the PlayerStart.
            LES.editor_request_begin_play()
            yield 240
            game = UES.get_game_world()
            pawn = unreal.GameplayStatics.get_player_pawn(game, 0) if game else None
            check(pawn is not None, "PIE spawned a pawn (%s)" % (pawn.get_class().get_name() if pawn else None))
            if pawn:
                loc = pawn.get_actor_location()
                check(abs(loc.x) < 50000 * 4 and loc.z > -1000, "pawn at %.0f %.0f %.0f" % (loc.x, loc.y, loc.z))
            LES.editor_request_end_play()
            yield 120
            first = True
            for name, loc, rot in VIEWS[path]:
                if name == "overview":
                    for a in EAS.get_all_level_actors():
                        if a.get_actor_label().startswith(OVERVIEW_HIDE):
                            a.set_is_temporarily_hidden_in_editor(True)
                UES.set_level_viewport_camera_info(unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
                yield 900 if first else 240   # first view waits for shaders and Lumen
                first = False
                shot = os.path.join(OUT, "%s_%s.png" % (path.rsplit("/", 1)[1], name))
                unreal.SystemLibrary.execute_console_command(UES.get_editor_world(), "HighResShot 1600x900 filename=%s" % shot)
                log("screenshot " + shot)
                yield 90


Runner()
