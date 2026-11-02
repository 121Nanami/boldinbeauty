"""The start of the game (owner request 2026-10-07: the boring main screen is gone): PIE boots into the Ark at once, the sign-in
screen sits over the living Ark while a camera circles the government spire, and after "Continue as guest" the player
spawns in the Ark (at the saved position when the guest save has one).

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBNoAutoLogin -ExecCmds="py <abs path>/Tools/verify_ark_start.py"

-BIBNoAutoLogin skips the saved sign-in, so the sign-in screen always shows and nothing touches a real account (the guest
save is local). Screenshots: Saved/ArkStart/<case>.png. Writes Saved/VerifyArkStart.log (last line RESULT PASS|FAIL).
"""

import math
import os
import traceback

import unreal

LOG, FAILS = [], []
SAVED = unreal.Paths.project_saved_dir()
OUT = os.path.join(SAVED, "ArkStart")
ARK = unreal.BIBArkLibrary


def log(text):
    LOG.append(text)
    unreal.log("[verify_ark_start] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def map_name():
    return str(unreal.GameplayStatics.get_current_level_name(world(), True)) if world() else ""


def screen():
    pc = controller()
    return str(ARK.get_screen_name(pc)) if pc else ""


def camera():
    pc = controller()
    return pc.player_camera_manager.get_camera_location() if pc else None


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


def shot(name):
    path = os.path.join(OUT, name + ".png")
    unreal.SystemLibrary.execute_console_command(world(), "HighResShot 1600x900 filename=%s" % path)
    yield 1.5
    log("screenshot " + path)


def flow():
    os.makedirs(OUT, exist_ok=True)
    ok = yield from wait_until(lambda: "L_Ark" in map_name() and screen() == "SignIn", 120)
    check(ok, "the game boots straight into the Ark with the sign-in screen over it (map %s, screen %s)" % (map_name(), screen()))
    if not ok:
        return
    yield 4.0
    a = camera()
    yield 2.0
    b = camera()
    moved = math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2) if a and b else 0.0
    check(moved > 50, "behind the sign-in a camera circles the Ark (moved %.0f cm in 2 s)" % moved)
    check(not ARK.is_ark_hud_shown(controller()), "no menu button while signing in")
    yield from shot("signin_over_ark")
    accounts = [x for x in unreal.ObjectIterator(unreal.BIBAccountSubsystem) if not x.get_name().startswith("Default__")]
    check(bool(accounts), "the account subsystem exists")
    if not accounts:
        return
    accounts[-1].begin_guest()   # the sign-in screen's "Continue as guest"
    ok = yield from wait_until(lambda: ARK.is_in_ark(controller()) and screen() == "None", 60)
    check(ok, "Continue as guest: the player walks the Ark (screen %s)" % screen())
    yield 2.0
    pc = controller()
    arrival = str(ARK.get_ark_arrival(pc))
    saved = ARK.get_saved_ark_position(pc)
    check(arrival == ("saved" if saved else "plaza"), "arrival: %s (the guest save %s a position)" % (arrival, "has" if saved else "has no"))
    check(ARK.is_ark_hud_shown(pc), "the menu button is on screen")
    view = pc.get_view_target()
    check(view is not None and view == pc.get_controlled_pawn(), "the camera is back on the player")
    yield from shot("spawned_in_ark")


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
        with open(os.path.join(SAVED, "VerifyArkStart.log"), "w") as handle:
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
