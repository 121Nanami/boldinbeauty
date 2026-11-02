"""Checks the chapter maps 2-13 built by greybox_chapters.py and takes editor-viewport screenshots.

    UnrealEditor <BoldInBeautyDemo.uproject> -ExecCmds="py <abs path>/Tools/verify_chapters.py"

Per map: required pieces by tag (four sections with one boss section, section gates, climb / jump-up / swim pieces, the
chapter's own must-haves), every water volume has a brush and a size, the anime look in
every map but Chapter 11, one PlayerStart above solid ground. In PIE: the free-fly pawn spawns, a test Character dropped
into the map's water switches to swimming, and the movers move. Screenshots: Saved/Greybox/<map>_{start,overview}.png.
BIB_CHAPTERS="3,11" limits the run. Writes Saved/VerifyChapters.log (RESULT PASS / RESULT FAIL) and quits the editor.
"""

import os
import traceback

import unreal

LOG, FAILS = [], []
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Greybox")
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAPS = "/Game/Greybox/Maps/"

COMMON = [("Section", 4, 4), ("BossSection", 1, 1), ("SectionGate", 3, None)]
# Map -> extra (tag, min, max) checks. Swim / climb / jump-up / mover per the owner's "jump up to climb, swim, move".
CHECKS = {
    "L_Ch02_Ruins": [("SurfaceHatch", 16, 16), ("Ruin", 15, None), ("ToppledTower", 1, 1), ("Swim", 1, None), ("Climbable", 20, None),
                     ("JumpUp", 20, None)],
    "L_Ch03_Docks": [("Lighthouse", 1, 1), ("LighthouseStair", 80, None), ("LighthouseBeam", 1, 1), ("Wave", 20, None), ("Pier", 10, None),
                     ("Swim", 1, None), ("JumpUp", 10, None), ("Mover", 20, None)],
    "L_Ch04_HangingCity": [("Chain", 400, None), ("ChainAnchor", 1, 1), ("SwingBridge", 8, None), ("Lift", 4, 4), ("Climbable", 400, None),
                           ("Mover", 12, None)],
    "L_Ch05_ArcTech": [("Cube", 500, None), ("Conveyor", 4, None), ("Press", 10, 10), ("RobotArm", 8, None), ("Catwalk", 2, 2),
                       ("Swim", 3, None), ("JumpUp", 10, None), ("Mover", 100, None)],
    "L_Ch06_MoonKingdom": [("Moon", 1, 1), ("Vine", 20, None), ("Swim", 1, None), ("JumpUp", 10, None), ("Climbable", 20, None)],
    "L_Ch07_TOMBS": [("Desk", 100, None), ("AngelSecret", 5, 5), ("SecretDoor", 5, 5), ("Atrium", 40, None), ("Climbable", 2, None)],
    "L_Ch08_Intersection": [("AngelPlaceholder", 100, None), ("RainPlaceholder", 1, 1), ("RainGrate", 20, None), ("Swim", 4, None),
                            ("JumpUp", 40, None), ("Mover", 40, None)],
    "L_Ch09_RemnantRemains": [("DataCentre", 1, None), ("Screen", 1, 1), ("RainPlaceholder", 1, 1), ("WindPlaceholder", 1, 1),
                              ("Swim", 4, None), ("Cable", 8, None), ("Climbable", 8, None)],
    "L_Ch10_Hallucinations": [("MorseMessage", 4, 4), ("ApparitionSpawn", 20, None), ("HorrorApparitionPlaceholder", 1, None),
                              ("Swim", 1, None), ("JumpUp", 50, None), ("Mover", 5, None)],
    "L_Ch11_ReturnToNowhere": [("NowhereFragment", 10, None), ("NowherePath", 10, 10), ("NowhereExit", 1, 1), ("NoPostProcess", 1, 1),
                               ("WhiteNoisePlaceholder", 1, 1)],
    "L_Ch12_Paradise": [("RooftopGarden", 3, None), ("Waterfall", 3, None), ("RainPlaceholder", 1, 1), ("Swim", 5, None),
                        ("JumpUp", 20, None), ("Climbable", 20, None)],
    "L_Ch13_SecretiveSoul": [("Monolith", 1, 1), ("RedGlyph", 40, None), ("AngelGodVoice", 1, 1), ("Torii", 3, 3), ("Shrine", 1, None),
                             ("RainPlaceholder", 1, 1), ("Swim", 5, None), ("JumpUp", 10, None)],
}
NO_COMBAT = {"L_Ch11_ReturnToNowhere"}
NO_ANIME = {"L_Ch11_ReturnToNowhere"}
HIDE_FOR_OVERVIEW = ("HeightFog", "HeavyFog", "RockCeiling", "RockWall", "MegaWall", "MegaRoof", "TunnelRoof", "CrossRoof", "Roof", "DataCentreMass",
                     "DataCentreTier")


def log(text):
    LOG.append(text)
    unreal.log("[verify_chapters] " + text)


def check(ok, text):
    log(("PASS " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def tagged(actors, tag):
    name = unreal.Name(tag)
    return [a for a in actors if name in a.tags]


def structural(name):
    actors = EAS.get_all_level_actors()
    check(len(actors) >= 8000, "%s: %d actors (owner detail floor 8,000)" % (name, len(actors)))
    checks = ([] if name in NO_COMBAT else COMMON) + CHECKS[name]
    for tag, lo, hi in checks:
        n = len(tagged(actors, tag))
        check(n >= lo and (hi is None or n <= hi), "%s: tag %s = %d (want %d..%s)" % (name, tag, n, lo, hi))
    if name in NO_COMBAT:
        check(not tagged(actors, "Section"), "%s: no sections, no bosses" % name)
    styles = [a for a in actors if hasattr(unreal, "BIBAnimeStyleVolume") and isinstance(a, unreal.BIBAnimeStyleVolume)]
    want = 0 if name in NO_ANIME else 1
    check(len(styles) == want, "%s: %d anime style volume(s) (want %d)" % (name, len(styles), want))
    for vol in tagged(actors, "WaterVolume"):
        origin, extent = vol.get_actor_bounds(False)
        ok = vol.get_editor_property("water_volume") and extent.x > 100 and extent.z > 10   # no brush = zero bounds
        if not ok:
            check(False, "%s: water volume %s has a brush (extent %.0f %.0f %.0f)" % (name, vol.get_actor_label(), extent.x,
                                                                                            extent.y, extent.z))
    water = tagged(actors, "WaterVolume")
    if water:
        check(True, "%s: %d water volumes checked" % (name, len(water)))
    empty = [a.get_actor_label() for a in actors if isinstance(a, unreal.StaticMeshActor) and not a.static_mesh_component.static_mesh]
    check(not empty, "%s: every mesh actor has a mesh (%d without)" % (name, len(empty)))
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    check(len(starts) == 1, "%s: one PlayerStart" % name)
    if starts:
        loc = starts[0].get_actor_location()
        hit = unreal.SystemLibrary.line_trace_single(UES.get_editor_world(), loc, loc - unreal.Vector(0, 0, 2000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        check(hit is not None and hit.to_tuple()[0], "%s: solid ground under the PlayerStart" % name)
    return actors


def views(actors):
    start = [a for a in actors if isinstance(a, unreal.PlayerStart)][0]
    loc, yaw = start.get_actor_location(), start.get_actor_rotation().yaw
    marks = tagged(actors, "Section") or tagged(actors, "NowherePath") or [start]
    xs = [a.get_actor_location().x for a in marks] + [loc.x]
    ys = [a.get_actor_location().y for a in marks] + [loc.y]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    span = max(max(xs) - min(xs), max(ys) - min(ys), 20000)
    import math
    back = (loc.x - 1500 * math.cos(math.radians(yaw)), loc.y - 1500 * math.sin(math.radians(yaw)), loc.z + 600)
    shots = [("start", back, (-8, yaw, 0)), ("overview", (cx, cy - span * 0.9, span * 0.7), (-35, 90, 0))]
    # Close look at the boss section's dressing (second detail pass, 2026-10-07): from its south-west, above head height.
    boss = tagged(actors, "BossSection")
    if boss:
        b = boss[0].get_actor_location()
        shots.append(("section", (b.x - 3200, b.y - 3200, b.z + 1500), (-22, 45, 0)))
    return shots


class Runner:
    def __init__(self):
        try:   # the waits count frames; a background editor otherwise drops to a few frames a second
            unreal.get_default_object(unreal.EditorPerformanceSettings).set_editor_property("throttle_cpu_when_not_foreground", False)
        except Exception as error:
            log("note: background throttle left on (%s)" % error)
        self.steps = self.script()
        self.wait = 0
        self.busy = False
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, _dt):
        if self.busy:   # saving a map ticks Slate from inside a step
            return
        if self.wait > 0:
            self.wait -= 1
            return
        try:
            self.busy = True
            self.wait = next(self.steps)
            self.busy = False
        except StopIteration:
            self.finish()
        except Exception:
            log(traceback.format_exc())
            FAILS.append("exception")
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        log("RESULT " + ("PASS" if not FAILS else "FAIL (%d)" % len(FAILS)))
        with open(os.path.join(unreal.Paths.project_saved_dir(), "VerifyChapters.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        unreal.SystemLibrary.quit_editor()

    def script(self):
        os.makedirs(OUT, exist_ok=True)
        only = [int(x) for x in os.environ.get("BIB_CHAPTERS", "").split(",") if x.strip()]
        for name in CHECKS:
            if only and int(name[4:6]) not in only:
                continue
            check(LES.load_level(MAPS + name), "load " + name)
            yield 30
            actors = structural(name)
            water = tagged(actors, "WaterVolume")
            movers = tagged(actors, "Mover")[:5]
            before = [m.get_actor_transform() for m in movers]
            probe, vol = None, None
            if water:
                # A test Character placed just below the surface of the biggest water volume; PIE copies it into play.
                vol = max(water, key=lambda v: v.get_actor_bounds(False)[1].x * v.get_actor_bounds(False)[1].y)
                origin, extent = vol.get_actor_bounds(False)
                spot = unreal.Vector(origin.x, origin.y, origin.z + extent.z - min(150.0, extent.z * 0.5))
                probe = EAS.spawn_actor_from_class(unreal.Character, spot)
                probe.set_editor_property("tags", [unreal.Name("SwimProbe")])
                probe.character_movement.set_editor_property("run_physics_with_no_controller", True)
            LES.editor_request_begin_play()
            yield 240
            game = UES.get_game_world()
            pawn = unreal.GameplayStatics.get_player_pawn(game, 0) if game else None
            check(pawn is not None, "%s: PIE spawned a pawn (%s)" % (name, pawn.get_class().get_name() if pawn else None))
            if probe and game:
                found = unreal.GameplayStatics.get_all_actors_with_tag(game, "SwimProbe")
                mode = found[0].character_movement.movement_mode if found else None
                check(mode == unreal.MovementMode.MOVE_SWIMMING, "%s: a character in %s swims (%s)" % (name, vol.get_actor_label(), mode))
            if movers and game:
                live = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_with_tag(game, "Mover")}
                moved = 0
                for m, t0 in zip(movers, before):
                    a = live.get(m.get_actor_label())
                    if not a:
                        continue
                    d = a.get_actor_location() - t0.translation
                    r0, r1 = t0.rotation.rotator(), a.get_actor_rotation()
                    turned = max(abs(r1.yaw - r0.yaw), abs(r1.pitch - r0.pitch), abs(r1.roll - r0.roll))
                    if d.length() > 5.0 or turned > 1.0:
                        moved += 1
                check(moved > 0, "%s: movers move in play (%d of %d sampled)" % (name, moved, len(movers)))
            LES.editor_request_end_play()
            yield 90
            if probe:
                EAS.destroy_actor(probe)
                LES.save_current_level()   # generated map: back to its built content, no unsaved-changes prompt
            first = True
            EAS.select_nothing()
            for view, loc, rot in views(actors):
                if view == "overview":
                    for a in EAS.get_all_level_actors():
                        if a.get_actor_label().startswith(HIDE_FOR_OVERVIEW):
                            a.set_is_temporarily_hidden_in_editor(True)
                UES.set_level_viewport_camera_info(unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
                yield 300 if first else 150
                first = False
                shot = os.path.join(OUT, "%s_%s.png" % (name, view))
                if os.path.exists(shot):
                    os.remove(shot)
                unreal.SystemLibrary.execute_console_command(UES.get_editor_world(), "HighResShot 1600x900 filename=%s" % shot)
                for _ in range(400):   # the shot is written on a later frame; do not move the camera before it exists
                    if os.path.exists(shot):
                        break
                    yield 1
                log("screenshot " + shot + ("" if os.path.exists(shot) else " (NOT WRITTEN)"))
                yield 10


Runner()
