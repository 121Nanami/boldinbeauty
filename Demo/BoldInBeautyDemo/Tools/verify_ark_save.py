"""The game starts in the Ark where the player last stood (owner request 2026-10-07), across a restart. Two editor runs with a
local guest (a real save file, unlike -BIBDev):

    BIB_ARK_SAVE_PHASE=1 UnrealEditor <BoldInBeautyDemo.uproject> -BIBGuest -ExecCmds="py <abs path>/Tools/verify_ark_save.py"
    BIB_ARK_SAVE_PHASE=2 UnrealEditor <BoldInBeautyDemo.uproject> -BIBGuest -ExecCmds="py <abs path>/Tools/verify_ark_save.py"

Phase 1: the game boots into the Ark (no 2D main screen), the player teleports to the arcade, walks off the ring, and the
position is written to the guest save; the spot is noted in Saved/ArkSaveSpot.txt. Phase 2: a fresh editor and PIE start the
guest in the Ark at that spot (arrival "saved"). Writes Saved/VerifyArkSave<phase>.log (last line RESULT PASS|FAIL).
"""

import math
import os
import traceback

import unreal

LOG, FAILS = [], []
SAVED = unreal.Paths.project_saved_dir()
ARK = unreal.BIBArkLibrary
PHASE = os.environ.get("BIB_ARK_SAVE_PHASE", "1")
SPOT_FILE = os.path.join(SAVED, "ArkSaveSpot.txt")


def log(text):
    LOG.append(text)
    unreal.log("[verify_ark_save] " + text)


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


def map_name():
    return str(unreal.GameplayStatics.get_current_level_name(world(), True)) if world() else ""


def wait_until(cond, timeout):
    waited = 0.0
    while waited < timeout:
        try:
            if cond():
                return True
        except Exception:
            pass
        waited += 0.2
        yield 0.2
    return False


def flow():
    ok = yield from wait_until(lambda: ARK.is_in_ark(controller()), 120)
    check(ok, "the guest starts in the Ark, no 2D main screen (map %s, screen %s)" % (map_name(), ARK.get_screen_name(controller()) if controller() else "-"))
    if not ok:
        return
    yield 2.0
    pc = controller()
    arrival = str(ARK.get_ark_arrival(pc))
    if PHASE == "1":
        ARK.open_menu(pc)
        yield 0.2
        ARK.menu_click(pc, "ark:arcade")
        yield 1.5
        body = character()
        at = body.get_actor_location()
        body.set_actor_location(unreal.Vector(at.x + 650, at.y - 400, at.z + 40), False, True)
        yield 2.0
        ARK.save_ark_position(pc, True)   # what the game does on leaving; the 10 s autosave writes the same
        saved = ARK.get_saved_ark_position(pc)
        check(saved is not None, "the position is in the profile")
        if saved:
            spot = saved[0]
            with open(SPOT_FILE, "w") as handle:
                handle.write("%f %f %f %f\n" % (spot.x, spot.y, spot.z, saved[1]))
            log("saved spot: %.0f %.0f %.0f yaw %.0f (arrival this run: %s)" % (spot.x, spot.y, spot.z, saved[1], arrival))
        yield 1.0
    else:
        if not os.path.exists(SPOT_FILE):
            check(False, "phase 1 left %s" % SPOT_FILE)
            return
        x, y, z, yaw = [float(v) for v in open(SPOT_FILE).read().split()]
        here = character().get_actor_location()
        feet = here.z - character().capsule_component.get_scaled_capsule_half_height()
        dist = math.hypot(here.x - x, here.y - y)
        check(arrival == "saved", "after a restart the guest arrives at the saved position (%s)" % arrival)
        check(dist < 120 and abs(feet - z) < 150, "standing where they left (%.0f cm away, height %.0f vs %.0f)" % (dist, feet, z))
        rot = character().get_actor_rotation().yaw
        check(abs(((rot - yaw) + 180) % 360 - 180) < 5, "facing the same way (%.0f vs %.0f)" % (rot, yaw))


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
        with open(os.path.join(SAVED, "VerifyArkSave%s.log" % PHASE), "w") as handle:
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
