"""UI screenshots for review (2026-10-09): the Ark, the menu, station panels and a battle; run with -BIBTouch for the mobile
controls. Saved/Combat/ui_*.png and Saved/UiShots.log.

Old docstring of verify_combat follows: Plays the Zenless-style combat and traversal pass in PIE (owner request 2026-10-09, Notion "Dev notes"): lower jumps, the
4-hit basic string and its combo counter, Daze filling to a stun, the chain attack on switch, the angels learning the Guard
counter from basic-attack spam (and guarding), the boss bar, wall climbing and swimming; takes HUD screenshots on the way.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"

Writes Saved/VerifyCombat.log (last line RESULT PASS|FAIL) and Saved/Combat/*.png (screenshots with the HUD).
"""

import glob
import math
import os
import shutil
import sys
import time
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bib_hub  # noqa: E402

LOG = []
FAILS = []
SAVED = unreal.Paths.project_saved_dir()
SHOTS = os.path.join(SAVED, "Combat")
LIB = unreal.BIBBlueprintLibrary


def log(text):
    LOG.append(text)
    unreal.log("[verify_combat] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def character():
    pc = controller()
    return pc.get_controlled_pawn() if pc else None


def feel(key):
    return LIB.get_meta_value(world(), "feel:" + key)


def hud():
    return LIB.get_hud_info(controller())


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


def enemies():
    return [e for e in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.load_class(None, "/Script/BoldInBeautyDemo.BIBEnemy")) if e.is_alive()]


def press(name, frames=1):
    subsystem = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]
    action = unreal.load_asset("/Game/BIB/Input/IA_" + name)
    for _ in range(frames):
        subsystem.inject_input_vector_for_action(action, unreal.Vector(1.0, 0.0, 0.0), [], [])
        yield 0
    yield 0
    yield 0


def face(target):
    char = character()
    d = target - char.get_actor_location()
    controller().set_control_rotation(unreal.Rotator(roll=0.0, pitch=-10.0, yaw=math.degrees(math.atan2(d.y, d.x))))


def step_to(enemy, distance=170.0):
    char = character()
    e = enemy.get_actor_location()
    c = char.get_actor_location()
    dx, dy = c.x - e.x, c.y - e.y
    n = max(1.0, math.hypot(dx, dy))
    char.set_actor_location(unreal.Vector(e.x + dx / n * distance, e.y + dy / n * distance, e.z + 10.0), False, True)
    face(e)


def shot(name):
    """Screenshot with the HUD (the 'shot showui' console command) copied to Saved/Combat/<name>.png."""
    os.makedirs(SHOTS, exist_ok=True)
    folder = os.path.join(SAVED, "Screenshots")
    before = set(glob.glob(os.path.join(folder, "**", "*.png"), recursive=True))
    unreal.SystemLibrary.execute_console_command(world(), "shot showui")
    end = time.monotonic() + 10.0
    while time.monotonic() < end:
        now = set(glob.glob(os.path.join(folder, "**", "*.png"), recursive=True)) - before
        if now:
            yield 0.5
            newest = max(now, key=os.path.getmtime)
            shutil.copy(newest, os.path.join(SHOTS, name + ".png"))
            log("screenshot %s.png" % name)
            return
        yield 0.1
    log("screenshot %s NOT written" % name)


def start_stage(stage):
    chapter = (stage - 1) // 8 + 1
    controller().bib_set_campaign_progress(stage - 1)
    hub = bib_hub.hub()
    hub.click_action("tab:5")
    hub.click_action("camp:ch:%d" % chapter)
    hub.click_action("camp:start:%d" % stage)
    yield from wait_until(lambda: "Chapter" in str(hud().get_editor_property("stage_title")), 60, "stage")
    yield 2.0


ARK = unreal.BIBArkLibrary


def flow():
    touch = "-BIBTouch" in unreal.SystemLibrary.get_command_line()
    tag = "touch_" if touch else ""
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE")
    yield 2.0
    yield from wait_until(lambda: bib_hub.hub() is not None, 90, "the Ark")
    yield 2.0
    loc = ARK.get_station_location(world(), "factory")
    if loc and character():
        character().set_actor_location(unreal.Vector(loc.x - 1400, loc.y - 600, loc.z + 120), False, True)
        face(unreal.Vector(loc.x, loc.y, loc.z + 300))
        yield 1.0
    yield from shot("ui_%sark_walk" % tag)
    if not touch:
        ARK.open_menu(world())
        yield 0.6
        for node in ("ops",):
            ARK.menu_click(world(), node)
            yield 0.6
        yield from shot("ui_menu")
        ARK.close_screens(world())
        yield 0.4
        for station in ("factory", "rollcall", "taskroom", "armoury"):
            ARK.open_station(world(), station, 0)
            yield 1.0
            yield from shot("ui_station_" + station)
            ARK.close_screens(world())
            yield 0.4
    yield from start_stage(9)
    npcs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BIBStoryNPC"]
    if npcs:
        loc = npcs[0].get_actor_location()
        character().set_actor_location(unreal.Vector(loc.x - 300, loc.y, 150.0), False, True)
        yield 0.5
        for _ in range(8):
            yield from press("Talk")
            yield 0.25
    yield 2.0
    yield from shot("ui_%sbattle" % tag)
    check(True, "screenshots taken")


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
        with open(os.path.join(SAVED, "UiShots.log"), "w") as handle:
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


Runner()
