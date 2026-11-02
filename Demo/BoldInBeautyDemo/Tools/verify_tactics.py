"""Plays a crowd stage in PIE and measures how the angels move (2026-10-12, behaviour layer): attack tokens, stacking and
distance while the commander stands still and then backs away. Writes Saved/VerifyTactics.log (last line RESULT PASS|FAIL).

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="[bib.Tactics 0,]py <this file>"

Environment: BIB_TACTICS_STAGE (default 25), BIB_TACTICS_LABEL (name written next to the numbers).
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
LIB = unreal.BIBBlueprintLibrary


def log(text):
    LOG.append(text)
    unreal.log("[verify_tactics] " + text)


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
    chapter, level = (stage - 1) // 8 + 1, (stage - 1) % 8 + 1
    name = "Chapter %d - Level %d" % (chapter, level)
    controller().bib_set_campaign_progress(stage - 1)
    hub = bib_hub.hub()
    hub.click_action("tab:5")
    hub.click_action("camp:ch:%d" % chapter)
    hub.click_action("camp:start:%d" % stage)
    ok = yield from wait_until(lambda: name in str(hud().get_editor_property("stage_title")), 60, "%s to start" % name)
    yield 1.5
    return ok


def sections():
    found = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BIBSection"]
    return sorted(found, key=lambda a: a.get_editor_property("section_index"))


def enter_section(section):
    door = section.get_editor_property("entry_door")
    yield from wait_until(lambda: door is None or door.is_open(), 20, "door %d" % (section.get_editor_property("section_index") + 1))
    c = section.get_actor_location()
    character().set_actor_location(unreal.Vector(c.x, c.y, 150.0), False, True)
    yield from wait_until(lambda: len(enemies()) > 0, 15, "section %d to spawn" % (section.get_editor_property("section_index") + 1))
    yield 0.6


def clear_section():
    for _ in range(12):
        if not enemies():
            break
        controller().bib_damage_enemies(1.0e9)
        yield 0.5


def parse(text):
    out = {}
    for pair in text.split():
        key, _, value = pair.partition("=")
        out[key] = float(value)
    return out


def phase(name, seconds, hold):
    LIB2 = unreal.BIBTacticsLibrary
    LIB2.reset_crowd_stats()
    end = time.monotonic() + seconds
    while time.monotonic() < end and enemies():
        if hold:
            subsystem = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]
            subsystem.inject_input_vector_for_action(unreal.load_asset("/Game/BIB/Input/IA_" + hold), unreal.Vector(1.0, 0.0, 0.0), [], [])
        LIB2.sample_crowd(world())
        yield 0
    stats = LIB2.get_crowd_stats()
    log("%s [%s]: %s" % (name, os.environ.get("BIB_TACTICS_LABEL", "layer"), stats))
    return parse(stats)


def flow():
    stage = int(os.environ.get("BIB_TACTICS_STAGE", "25"))
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE")
    yield 2.0
    yield from wait_until(lambda: bib_hub.hub() is not None, 90, "the Ark")
    ok = yield from start_stage(stage)
    check(ok, "stage %d started" % stage)
    if not ok:
        return
    npcs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BIBStoryNPC"]
    if npcs:
        loc = npcs[0].get_actor_location()
        character().set_actor_location(unreal.Vector(loc.x - 300, loc.y, 150.0), False, True)
        yield 0.5
        for _ in range(8):
            yield from press("Talk")
            yield 0.25
    sec = sections()
    check(len(sec) > 0, "the stage has sections")
    yield from enter_section(sec[0])
    check(len(enemies()) >= 6, "a crowd of %d angels spawned" % len(enemies()))
    still = yield from phase("standing still", 10.0, None)
    check(still.get("samples", 0) > 30, "samples taken while standing (%d)" % still.get("samples", 0))
    check(still.get("framesOverCap", 1) == 0, "never more than 3 melee angels committed at once (max %d)" % still.get("maxCommittedMelee", -1))
    check(still.get("stackedPairShare", 1.0) < 0.05, "angels do not stack on one point (%.2f%% of pairs)" % (100.0 * still.get("stackedPairShare", 1.0)))
    if enemies():
        kite = yield from phase("backing away", 6.0, "MoveBack")
        check(kite.get("framesOverCap", 1) == 0, "never more than 3 melee committed while kiting (max %d)" % kite.get("maxCommittedMelee", -1))
    yield from clear_section()
    check(len(enemies()) == 0, "the crowd can be cleared")


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
        with open(os.path.join(SAVED, "VerifyTactics.log"), "w") as handle:
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
