"""Performance pass before an owner playtest (owner 2026-10-09: "make sure the game runs well and isn't a buggy lag storm"):
frame times in the Ark among the LED ad screens and in a full fight with abilities, dodges, switches and the Set 2 VFX. Editor PIE
numbers (slower than a packaged build). Saved/VerifyPerf.log, screenshots Saved/Combat/perf_*.png.
"""


import glob
import json
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
try:
    with open(os.path.join(unreal.Paths.project_content_dir(), "BIBMotion", "motion.json")) as _handle:
        MANIFEST = set(json.load(_handle)["pieces"])
except Exception:
    MANIFEST = set()
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


def press_many(names, frames=2):
    subsystem = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]
    actions = [unreal.load_asset("/Game/BIB/Input/IA_" + n) for n in names]
    for _ in range(frames):
        for action in actions:
            subsystem.inject_input_vector_for_action(action, unreal.Vector(1.0, 0.0, 0.0), [], [])
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


ARK = unreal.BIBArkLibrary


def played(piece):
    return LIB.get_meta_value(world(), "motion:played:" + piece) == 1



FRAMES = []


def measure(label, seconds, action=None):
    """Frame times (real seconds between editor ticks while PIE runs) over a stretch of play."""
    FRAMES.clear()
    MEASURING[0] = True
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if action:
            yield from action()
        else:
            yield 0
    MEASURING[0] = False
    ft = sorted(FRAMES)
    if not ft:
        check(False, label + ": no frames")
        return
    avg = sum(ft) / len(ft)
    p95 = ft[int(len(ft) * 0.95)]
    p99 = ft[int(len(ft) * 0.99)]
    hitches = sum(1 for f in ft if f > 0.1)
    log("%s: %d frames, avg %.1f ms (%.0f fps), p95 %.1f ms, p99 %.1f ms, max %.1f ms, hitches >100 ms: %d"
        % (label, len(ft), avg * 1000, 1.0 / avg, p95 * 1000, p99 * 1000, ft[-1] * 1000, hitches))
    RESULTS[label] = (avg, p95, hitches, len(ft))


MEASURING = [False]
RESULTS = {}


def start_stage(stage):
    chapter = (stage - 1) // 8 + 1
    controller().bib_set_campaign_progress(stage - 1)
    hub = bib_hub.hub()
    hub.click_action("tab:5")
    hub.click_action("camp:ch:%d" % chapter)
    hub.click_action("camp:start:%d" % stage)
    yield from wait_until(lambda: "Chapter" in str(hud().get_editor_property("stage_title")), 60, "stage")
    yield 2.0


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE")
    yield 2.0
    yield from wait_until(lambda: bib_hub.hub() is not None, 120, "the Ark")
    yield 4.0
    log("screens %d (ads %d), playing %d" % (LIB.get_meta_value(world(), "screens"), LIB.get_meta_value(world(), "screens:ads"),
                                             LIB.get_meta_value(world(), "screens:playing")))
    # The Ark: stand among the ad pillars and turn around (all screens in view at some point).
    pillars = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BIBVideoScreen"]
    if pillars:
        p = pillars[len(pillars) // 2].get_actor_location()
        character().set_actor_location(unreal.Vector(p.x + 900, p.y, 300.0), False, True)
        face(p)
        yield 1.0
        yield from shot("perf_ark_ads")

    def spin():
        r = controller().get_control_rotation()
        controller().set_control_rotation(unreal.Rotator(roll=0.0, pitch=-5.0, yaw=r.yaw + 2.0))
        yield 0

    yield from measure("Ark among the LED ads (turning)", 15.0, spin)
    yield from start_stage(16)
    npcs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BIBStoryNPC"]
    if npcs:
        loc = npcs[0].get_actor_location()
        character().set_actor_location(unreal.Vector(loc.x - 300, loc.y, 150.0), False, True)
        yield 0.5
        for _ in range(8):
            yield from press("Talk")
            yield 0.25
    secs = sorted([a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BIBSection"],
                  key=lambda a: a.get_editor_property("section_index"))
    for i, sec in enumerate(secs):
        door = sec.get_editor_property("entry_door")
        yield from wait_until(lambda: door is None or door.is_open(), 20, "door")
        c = sec.get_actor_location()
        character().set_actor_location(unreal.Vector(c.x, c.y, 150.0), False, True)
        yield from wait_until(lambda: len(enemies()) > 0, 15, "spawn")
        yield 0.5
        if i in (0, len(secs) - 1):
            state = {"n": 0}

            def fight():
                if not enemies():
                    yield 0
                    return
                char = character()
                target = min(enemies(), key=lambda e: (e.get_actor_location() - char.get_actor_location()).length())
                step_to(target)
                state["n"] += 1
                k = state["n"] % 12
                if k == 3:
                    yield from press_many(["Run", "Parry"])
                elif k in (5, 6, 7):
                    yield from press(["Move1", "Move2", "Move3"][k - 5])
                elif k == 9:
                    yield from press("Special")
                elif k == 11:
                    yield from press("SwitchOperator")
                else:
                    yield from press("Attack")
                yield 0.1

            yield from measure("fight, section %d (abilities, dodges, switches, VFX)" % (i + 1), 20.0, fight)
            if i == 0:
                yield from shot("perf_fight")
        for _ in range(12):
            if not enemies():
                break
            controller().bib_damage_enemies(1.0e9)
            yield 0.5
    yield from wait_until(lambda: LIB.get_current_screen(controller()) == unreal.BIBScreen.RESULTS, 20, "results")
    for label, (avg, p95, hitches, n) in RESULTS.items():
        check(avg < 1.0 / 30.0, "%s: average frame under 33 ms (%.1f ms)" % (label, avg * 1000))
        check(hitches <= max(3, n // 200), "%s: few hitches over 100 ms (%d)" % (label, hitches))


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
        with open(os.path.join(SAVED, "VerifyPerf.log"), "w") as handle:
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
        if MEASURING[0]:
            FRAMES.append(delta)
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
