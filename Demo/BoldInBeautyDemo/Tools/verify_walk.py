"""Plays Chapter 11, Return to Nowhere, in PIE (owner level idea, final: "No levels, no bosses, nothing: you just walk through the
single level (about 5 minutes long) that is the map and walk out"). With the campaign at 10-8 cleared the Chapter 11 page shows one
walk; starting it opens L_Ch11_ReturnToNowhere with the team on foot, no enemies, no anime look and no weather (pure grey); walking
partway does nothing; reaching the exit frame clears the whole chapter (stage 88), saves and returns to the Ark.
    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"
Needs Tools/build_greybox.sh to have built the chapter maps. Writes Saved/VerifyWalk.log (last line RESULT PASS|FAIL).
"""
import os
import traceback
import unreal

LOG = []
FAILS = []
SAVED = unreal.Paths.project_saved_dir()
LIB = unreal.BIBBlueprintLibrary


def log(text):
    LOG.append(text)
    unreal.log("[verify_walk] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    return worlds[0] if worlds else None


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def character():
    pc = controller()
    return pc.get_controlled_pawn() if pc else None


def map_name():
    return unreal.GameplayStatics.get_current_level_name(world(), True) if world() else ""


def actors(class_path):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.load_class(None, class_path)))


def tagged(tag):
    return list(unreal.GameplayStatics.get_all_actors_with_tag(world(), tag))


def wait_until(cond, timeout, what):
    waited = 0.0
    while waited < timeout:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.25
        waited += 0.25
    check(False, "timed out waiting for " + what)
    return False


def flow():
    yield from wait_until(lambda: controller() is not None and unreal.BIBArkLibrary.is_in_ark(controller()), 120, "the Ark")
    yield 2.0
    pc = controller()
    pc.bib_set_campaign_progress(80)
    yield 0.3
    check(LIB.get_meta_value(pc, "cleared") == 80, "campaign at 10-8 cleared (%d)" % LIB.get_meta_value(pc, "cleared"))
    check(LIB.get_stage_section_count(81) == 0 and LIB.get_stage_section_count(88) == 0, "Chapter 11 has no sections")
    check("Return to Nowhere" in str(LIB.get_stage_display_name(81)), "stage 81 is called Return to Nowhere (%s)" % LIB.get_stage_display_name(81))
    # (a bool UFUNCTION with an out parameter returns None to Python when it returns false)
    check(LIB.start_stage(pc, 85) is None, "a later Chapter 11 stage cannot be started on its own")
    check(LIB.start_stage(pc, 81) is not None, "the walk starts")
    arrived = yield from wait_until(lambda: map_name() == "L_Ch11_ReturnToNowhere" and character() is not None, 90, "L_Ch11_ReturnToNowhere")
    if not arrived:
        return
    yield 3.0
    check(len(actors("/Script/BoldInBeautyDemo.BIBEnemy")) == 0, "no enemies")
    looks = [v for v in actors("/Script/BoldInBeautyDemo.BIBAnimeStyleVolume") if v.is_style_enabled()]
    check(not looks, "no anime look (pure grey, no post-processing)")
    weather = [w for w in actors("/Script/BoldInBeautyDemo.BIBWeatherSystem") if w.is_weather_enabled()]
    check(not weather, "no weather")
    exits = tagged("NowhereExit")
    check(len(exits) == 1, "the map has its exit marker (%d)" % len(exits))
    if not exits:
        return
    start = character().get_actor_location()
    exit_at = exits[0].get_actor_location()
    log("walk: start %s exit %s (%.0f m straight line)" % (start, exit_at, start.distance(exit_at) / 100.0))
    mid = unreal.Vector((start.x + exit_at.x) / 2, (start.y + exit_at.y) / 2, start.z + 200)
    character().set_actor_location(mid, False, True)
    yield 2.0
    check(map_name() == "L_Ch11_ReturnToNowhere" and LIB.get_meta_value(controller(), "cleared") == 80, "half way nothing happens")
    character().set_actor_location(unreal.Vector(exit_at.x - 300, exit_at.y, exit_at.z + 200), False, True)
    back = yield from wait_until(lambda: controller() is not None and unreal.BIBArkLibrary.is_in_ark(controller()), 60, "back in the Ark after walking out")
    check(back, "walking out returns to the Ark")
    yield 1.0
    pc = controller()
    check(LIB.get_meta_value(pc, "cleared") == 88, "the walk cleared Chapter 11 (%d)" % LIB.get_meta_value(pc, "cleared"))


class Runner:
    def __init__(self):
        self.gen = None
        self.wait = 0.0
        self.started = False
        self.primed = False
        self.busy = False
        self.done = False
        self.boot = 0.0
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def finish(self):
        self.done = True
        unreal.unregister_slate_post_tick_callback(self.handle)
        log("RESULT " + ("PASS" if not FAILS else "FAIL (%d): %s" % (len(FAILS), FAILS)))
        with open(os.path.join(SAVED, "VerifyWalk.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        if os.environ.get("BIB_VERIFY_KEEP_OPEN") != "1":
            unreal.SystemLibrary.quit_editor()

    def tick(self, delta):
        if self.busy or self.done:
            return
        self.busy = True
        try:
            self._tick(delta)
        finally:
            self.busy = False

    def _tick(self, delta):
        try:
            if not self.started:
                self.boot += delta
                if self.boot > 8.0:
                    self.started = True
                    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
                    self.gen = flow()
                return
            if self.wait > 0.0:
                self.wait -= delta
                return
            step = self.gen.send(delta if self.primed else None)
            self.primed = True
            self.wait = step or 0.0
        except StopIteration:
            self.finish()
        except Exception:
            log(traceback.format_exc())
            FAILS.append("exception")
            self.finish()


RUNNER = Runner()
