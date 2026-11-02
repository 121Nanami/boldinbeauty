"""Captures fixed-camera screenshots for the post-process filter before/after comparison (dream filter, 2026-10-11).

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <abs path>/Tools/capture_filter_shots.py" \
        -ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False

Opens L_NeoTokyo and L_Ark in PIE, places the player pawn at the map's PlayerStart and takes two fixed shots per map
(yaw 0 at eye height, and a raised three-quarter view). Identical positions for every run, so the same capture before and
after a filter change is comparable. Output folder: Saved/<BIB_SHOT_OUT or FilterShots>/<map>_<shot>.png.
Set BIB_LEGACY=1 for the "before" shots (old anime look). Log: Saved/CaptureFilterShots.log (ends with RESULT PASS or RESULT FAIL).
"""

import os
import time
import traceback

import unreal

LOG, FAILS = [], []
SAVED = unreal.Paths.project_saved_dir()
OUT = os.path.join(SAVED, os.environ.get("BIB_SHOT_OUT", "FilterShots"))
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

MAPS = [("/Game/Greybox/Maps/L_NeoTokyo", "neotokyo"), ("/Game/Greybox/Maps/L_Ark", "ark")]
if os.environ.get("BIB_MAPS"):   # comma list of /Game paths; the name is the asset name
    MAPS = [(m, m.split("/")[-1].lower()) for m in os.environ["BIB_MAPS"].split(",")]
# Worktrees whose generated Greybox content lacks those two maps: use what exists (the shots stay comparable run to run).
MAPS = (MAPS if os.environ.get("BIB_MAPS") else [m for m in MAPS if unreal.EditorAssetLibrary.does_asset_exist(m[0])]) or [
    ("/Game/Greybox/Maps/L_Ch02_Ruins", "ch02ruins"), ("/Game/BIB/Maps/L_Chapter1_Stage1", "ch1stage1")]

# (label, offset from PlayerStart, pitch, yaw)
SHOTS = [
    ("street", (0.0, 0.0, 160.0), -5.0, 0.0),
    ("vista", (900.0, 900.0, 650.0), -28.0, 45.0),
]


def log(text):
    LOG.append(text)
    unreal.log("[capture_filter_shots] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return UES.get_game_world()


def wait_until(cond, timeout, what):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.1
    check(False, "timed out waiting for " + what)
    return False


def pie(path):
    LES.load_level(path)
    yield 2.0
    if os.environ.get("BIB_DREAMV1") == "1":   # "before" run for the AAA grade: the first dream filter
        for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
            if isinstance(actor, unreal.BIBAnimeStyleVolume):
                actor.set_editor_property("dream_look_version", 1)
    if os.environ.get("BIB_LEGACY") == "1":   # "before" run: the old anime look (in memory only, the level is not saved)
        n = 0
        for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
            if isinstance(actor, unreal.BIBAnimeStyleVolume):
                actor.set_editor_property("legacy_anime_look", True)
                actor.set_editor_property("step_visuals", False)   # held frames cannot be photographed fairly
                n += 1
        log("legacy look set on %d volume(s)" % n)
    LES.editor_request_begin_play()
    ok = yield from wait_until(lambda: world() is not None
                               and unreal.GameplayStatics.get_player_controller(world(), 0) is not None
                               and unreal.GameplayStatics.get_player_controller(world(), 0).get_controlled_pawn() is not None,
                               60, "PIE in " + path)
    yield 6.0   # shaders and Lumen settle
    return ok


def capture(path, name):
    if not (yield from pie(path)):
        yield from end_pie()
        return
    w = world()
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    starts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart)
    base = starts[0].get_actor_location() if starts else pawn.get_actor_location()
    check(len(starts) > 0, "%s: PlayerStart found (%d)" % (name, len(starts)))
    for label, offset, pitch, yaw in SHOTS:
        pawn.set_actor_location(unreal.Vector(base.x + offset[0], base.y + offset[1], base.z + offset[2]), False, True)
        pc.set_control_rotation(unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0))
        yield 2.0
        target = os.path.join(OUT, "%s_%s.png" % (name, label))
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1600x900 filename=%s" % target)
        yield 1.5
        check(os.path.isfile(target) and os.path.getsize(target) > 30000, "%s_%s: screenshot written (%s)" % (name, label, target))
    yield from end_pie()


def end_pie():
    LES.editor_request_end_play()
    yield 3.0


def flow():
    os.makedirs(OUT, exist_ok=True)
    log("output: " + OUT)
    for path, name in MAPS:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            check(False, "map exists: " + path)
            continue
        yield from capture(path, name)


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
        with open(os.path.join(SAVED, "CaptureFilterShots.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        if world():
            LES.editor_request_end_play()
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
                    self.gen = flow()
                return
            if self.wait > 0.0:
                self.wait -= delta
                return
            step = self.gen.send(delta if self.primed else None)
            self.primed = True
            self.wait = step if isinstance(step, (int, float)) else 0.0
        except StopIteration:
            self.finish()
        except Exception:
            log(traceback.format_exc())
            self.finish()


RUNNER = Runner()
