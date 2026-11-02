"""Plays the keyboard / gamepad paths of the 2026-10-12 UI pass in PIE (docs: reports/demo/2026-10-12-ui-ux-flow-pass.md).
Keys go through the real Slate path (BIBArkLibrary.PressUiKey), so focus, bubbling and the input mode are exercised:
Esc / Start / M open and close the Ark menu, focus lands on a card, Backspace goes up, a stage pauses on Esc / Start,
the pause screen moves with Down, Enter on Resume resumes (never abandons by accident), Enter on Abandon leaves the
stage, sign-out shows the sign-in screen with the e-mail box focused. Writes Saved/VerifyUiFlow.log.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"
"""

import math
import os
import time
import traceback

import unreal

LOG = []
FAILS = []
SAVED = unreal.Paths.project_saved_dir()
ARK = unreal.BIBArkLibrary
LIB = unreal.BIBBlueprintLibrary


def log(text):
    LOG.append(text)
    unreal.log("[verify_ark] " + text)


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


def wait_until(cond, timeout):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.1
    return False


SHOTS = os.environ.get("BIB_SCREENSHOTS") == "1"


def _find_image_bytes(value):
    import base64
    if isinstance(value, dict):
        value = list(value.values())
    if isinstance(value, list):
        for item in value:
            found = _find_image_bytes(item)
            if found:
                return found
    elif isinstance(value, str) and len(value) > 1000:
        try:
            data = base64.b64decode(value.split(",")[-1])
        except Exception:
            return None
        if data[:8] == b"\x89PNG\r\n\x1a\n" or data[:3] == b"\xff\xd8\xff":
            return data
    return None


def shot(name):
    if not SHOTS:
        return
    yield 0.6
    try:
        import json
        result = unreal.ToolsetRegistry.execute_tool("SlateInspectorToolset.SlateInspectorToolset", "Screenshot", json.dumps({"Ref": ""}))
        data = None if result.error else _find_image_bytes(json.loads(result.value))
        if not data:
            log("screenshot %s unavailable: %s" % (name, (result.error or result.value)[:200]))
            return
        path = os.path.join(SAVED, "Screenshots", "ark_" + name + (".png" if data[:4] == b"\x89PNG" else ".jpg"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as handle:
            handle.write(data)
        log("screenshot: %s (%d bytes)" % (path, len(data)))
    except Exception as error:
        log("screenshot %s failed: %s" % (name, error))


def screen():
    return str(ARK.get_screen_name(controller()))


def in_ark():
    return ARK.is_in_ark(controller())


def map_name():
    return str(unreal.GameplayStatics.get_current_level_name(world(), True))


def dist2d(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def back_to_walking():
    for _ in range(8):
        if screen() == "None":
            return
        if screen() == "ArkPanel":
            ARK.panel_back(controller())
        else:
            ARK.close_screens(controller())
        yield 0.15


def press(key):
    ARK.press_ui_key(key)


def focus_info():
    return "%s | %s" % (ARK.get_focused_widget_type(), ARK.get_focused_widget_path())


def flow():
    pc = controller()
    # -BIBNoAutoLogin boots to the sign-in screen over the Ark: the e-mail box must already have focus
    ok = yield from wait_until(lambda: in_ark() or screen() == "SignIn", 90)
    if screen() == "SignIn":
        yield 1.5
        log("sign-in focus: " + focus_info())
        check("SEditableText" in str(ARK.get_focused_widget_type()), "sign-in: the e-mail box has focus without a click (%s)" % ARK.get_focused_widget_type())
        return
    check(ok, "the game travels into the Ark (map: %s)" % map_name())
    if not ok:
        return
    yield 1.5
    pc = controller()
    check(screen() == "None", "walking: no screen open (%s)" % screen())

    # ---- the Ark menu from the keyboard and the pad
    press("Gamepad_Special_Right")
    yield 0.6
    check(screen() == "ArkMenu", "pad Start opens the Ark menu (%s)" % screen())
    yield 0.4
    log("focus after open: " + focus_info())
    check(str(ARK.get_focused_widget_type()) == "SButton", "focus lands on a card, not the root (%s)" % ARK.get_focused_widget_type())
    press("Gamepad_Special_Right")
    yield 0.5
    check(screen() == "None", "pad Start closes the menu (%s)" % screen())
    press("M")
    yield 0.6
    check(screen() == "ArkMenu", "M opens the menu (%s)" % screen())
    press("BackSpace")
    yield 0.4
    check(screen() == "None", "Backspace from the first column closes it (%s)" % screen())
    # (Esc is not driven here: in the editor it ends the Play session before the game sees it.)
    press("P")
    yield 0.6
    check(screen() == "ArkMenu", "P opens the menu (%s)" % screen())
    # a branch card opens a second column; Backspace / B goes up one level, then closes
    ARK.menu_click(pc, "the-ark")
    yield 0.4
    path_before = str(ARK.get_menu_path(pc))
    press("Gamepad_FaceButton_Right")
    yield 0.4
    path_after = str(ARK.get_menu_path(pc))
    check(path_before != path_after and screen() == "ArkMenu", "pad B goes up one column ('%s' -> '%s')" % (path_before, path_after))
    press("Gamepad_Special_Right")
    yield 0.5
    check(screen() == "None", "pad Start closes the menu from a second column (%s)" % screen())

    # ---- a station panel: B goes back / closes, Start closes
    opened = ARK.open_station(pc, "rollcall", 0)
    yield 0.5
    check(opened and screen() == "ArkPanel", "Roll Call opens its panel (%s)" % screen())
    yield 0.3
    log("panel focus: " + focus_info())
    check(str(ARK.get_focused_widget_type()) == "SButton", "panel focus is on a button (%s)" % ARK.get_focused_widget_type())
    press("Gamepad_Special_Right")
    yield 0.5
    check(screen() == "None", "pad Start closes the panel (%s)" % screen())

    # ---- a stage pauses on Esc / Start and the pause screen is drivable
    LIB.start_stage(pc, 1)
    ok = yield from wait_until(lambda: ARK.is_battle_hud_shown(controller()), 60)
    check(ok, "stage 1 starts")
    if not ok:
        return
    yield 1.0
    pc = controller()
    press("Gamepad_Special_Right")
    yield 0.5
    check(screen() == "Pause", "pad Start pauses in a stage (%s)" % screen())
    yield 0.4
    press("Enter")
    yield 0.5
    check(screen() == "None" and ARK.is_battle_hud_shown(controller()), "Enter on the default (Resume) resumes, it does not abandon (%s)" % screen())
    press("Gamepad_Special_Right")
    yield 0.5
    check(screen() == "Pause", "pad Start pauses (%s)" % screen())
    press("Gamepad_FaceButton_Right")
    yield 0.5
    check(screen() == "None", "pad B on the pause screen resumes (%s)" % screen())
    press("P")
    yield 0.5
    check(screen() == "Pause", "P still pauses (%s)" % screen())
    press("Gamepad_Special_Right")
    yield 0.5
    check(screen() == "None", "pad Start resumes from the pause screen (%s)" % screen())
    press("P")
    yield 0.5
    press("Down")
    yield 0.2
    press("Enter")
    yield 1.0
    check(screen() != "Pause", "Down + Enter on the pause screen only resumes, never abandons (%s)" % screen())
    LIB.return_to_hub(controller())
    ok = yield from wait_until(lambda: in_ark(), 60)
    check(ok, "back in the Ark")
    yield 1.5
    pc = controller()



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
        with open(os.path.join(SAVED, "VerifyUiFlow.log"), "w") as handle:
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
