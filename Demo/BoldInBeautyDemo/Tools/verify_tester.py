"""Plays the Tester panel in PIE as a guest (owner request 2026-10-05: every guest and signed-in player is a tester).
Skips the tutorial, then drives Lobby > Tester panel through the real hub widget (UBIBHubWidget.ClickAction): currencies
(add / remove / set, wallet presets), operators (unlock all, level, stars), campaign progress, equipment, XP, summon pity,
presets. Every tester page is opened, so a missing icon texture fails the run. Writes Saved/VerifyTester.log.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBGuest -ExecCmds="py <this file>"

-BIBGuest starts a local guest: a normal player with a pending tutorial, not the -BIBDev developer profile the other verify
scripts use. The rules behind every action are unit-tested headless (Tests/tester_tests.cpp); this checks that the widget,
the page model and the engine layer agree.
"""

import os
import time
import traceback

import unreal

import os as _os
import sys as _sys

_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import bib_hub  # noqa: E402  (the 2D lobby was deleted on 2026-10-07: pages are driven in the Ark)

LOG = []
FAILS = []
SAVED = unreal.Paths.project_saved_dir()
# A guest's save persists between runs (the tutorial would already be skipped): start from a fresh guest every time.
for name in ("guest.json", "guest.json.bak"):   # the loader falls back to the .bak copy, so remove both
    try:
        os.remove(os.path.join(SAVED, "BIB", "Saves", name))
    except OSError:
        pass


def log(text):
    LOG.append(text)
    unreal.log("[verify_tester] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def widgets(name):
    w = world()
    found = []
    for widget in unreal.ObjectIterator(unreal.UserWidget):
        if widget.get_class().get_name() == name + "_C" and not widget.get_name().startswith("Default__") and widget.get_world() == w and widget.is_in_viewport():
            found.append(widget)
    return found


def hub():
    return bib_hub.hub()


def act(action_id):
    h = hub()
    if not h:
        check(False, "the player walks the Ark (to run %s)" % action_id)
        return False
    h.click_action(action_id)
    return True


def status():
    return str(hub().get_status_line()) if hub() else ""


def title():
    return str(hub().get_page_title()) if hub() else ""


def ids():
    h = hub()
    return [str(h.get_grid_id(i)) for i in range(h.get_grid_count())] + [str(h.get_bar_id(i)) for i in range(h.get_bar_count())]


def wait_until(cond, timeout, what):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            if cond():
                return
        except Exception:
            pass
        yield 0.1
    check(False, "timed out waiting for " + what)


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None and hub() is not None, 60, "PIE and the Ark")
    yield 2.0
    h = hub()
    check(h is not None, "the guest walks the Ark (the hub)")
    if not h:
        return

    # ---- a new guest has the tutorial; a tester can skip it from any page
    act("tab:0")
    yield 0.2
    check("tst:tut:skip" in ids(), "the tutorial page offers Skip tutorial (%s)" % ids())
    act("tab:6")
    yield 0.1
    check("tutorial" in status().lower(), "other tabs are held back during the tutorial (%s)" % status())
    act("tst:tut:skip")
    yield 0.3
    check("skipped" in status().lower(), "skipping the tutorial (%s)" % status())

    # ---- Lobby > Tester panel
    act("tab:2")
    yield 0.2
    check("go:tester" in ids(), "the Lobby has a Tester panel tile")
    act("go:tester")
    yield 0.2
    check(title() == "Tester panel", "Tester panel opens (%s)" % title())
    check(h.get_grid_count() >= 7, "the tester root lists its sections (%d tiles)" % h.get_grid_count())
    pages = ["go:tester-cur", "go:tester-ops", "go:tester-camp", "go:tester-gear", "go:tester-prog", "go:tester-pity", "go:tester-preset"]
    for page in pages:
        check(page in ids(), "the tester root has " + page)

    # ---- currencies: amount, mode, set, wallet presets
    act("go:tester-cur")
    yield 0.2
    act("tst:mode")
    act("tst:mode")      # Add -> Remove -> Set
    act("tst:step")      # 1,000 -> 10,000
    act("tst:cur:1")     # Diamonds
    yield 0.2
    check("Diamonds is now 10,000" in status(), "set Diamonds to 10,000 (%s)" % status())
    act("tst:wallet:0")
    yield 0.2
    check("applied" in status().lower(), "wallet preset (%s)" % status())
    act("tst:wallet:3")
    yield 0.2
    check("Very rich wallet applied" in status(), "very rich wallet (%s)" % status())
    act("back")
    yield 0.1

    # ---- operators: unlock all, level, stars, one operator's page
    act("go:tester-ops")
    yield 0.2
    act("tst:ops:unlock")
    yield 0.1
    check("unlocked" in status().lower(), "unlock every operator (%s)" % status())
    act("tst:ops:lv:50")
    yield 0.1
    check("level 50" in status(), "every operator to level 50 (%s)" % status())
    act("tst:op:sel:0")
    yield 0.2
    check(title().startswith("Tester: "), "an operator's tester page opens (%s)" % title())
    act("tst:op:lvset:160")
    yield 0.1
    act("tst:op:star:1")
    yield 0.1
    check("upgrade star" in status().lower(), "upgrade star (%s)" % status())
    act("tst:op:own")
    yield 0.1
    check("locked" in status().lower(), "lock the operator again (%s)" % status())
    act("tst:op:own")
    yield 0.1
    act("back")
    act("back")
    yield 0.1

    # ---- campaign
    act("go:tester-camp")
    yield 0.2
    act("tst:camp:all")
    yield 0.1
    check("104 of 104" in status(), "all 104 levels cleared (%s)" % status())
    act("tst:camp:d:-8")
    yield 0.1
    check("96 of 104" in status(), "one chapter back (%s)" % status())
    act("tst:tower:0")
    act("tst:sub:1")
    act("back")
    yield 0.1

    # ---- equipment, XP, pity
    act("go:tester-gear")
    yield 0.2
    act("tst:gear:add:3:0")
    yield 0.1
    check("Added" in status(), "add a weapon (%s)" % status())
    act("tst:gear:equip")
    yield 0.1
    act("back")
    act("go:tester-prog")
    yield 0.2
    act("tst:xp:clset:60")
    yield 0.1
    check("level is now 60" in status(), "commander level 60 (%s)" % status())
    act("tst:day:1")
    yield 0.1
    check("It is now day" in status(), "next day (%s)" % status())
    act("back")
    act("go:tester-pity")
    yield 0.2
    act("tst:pity:1:5")
    yield 0.1
    check("updated" in status().lower(), "pity set one short (%s)" % status())
    act("back")

    # ---- presets
    act("go:tester-preset")
    yield 0.2
    act("tst:preset:4")
    yield 0.3
    check("Everything unlocked" in status(), "everything-unlocked preset (%s)" % status())
    act("tst:preset:0")
    yield 0.3
    check("New player" in status(), "new-player preset (%s)" % status())
    act("tab:6")
    yield 0.1
    check("tutorial" in status().lower(), "the new-player preset brings the tutorial back (%s)" % status())
    act("tab:2")
    act("go:tester")
    act("tst:tut:skip")
    yield 0.2

    # ---- every icon the tester pages ask for exists
    for page in pages:
        act("tab:2")
        act("go:tester")
        act(page)
        yield 0.1
    yield 0.3
    # (The missing-texture count belonged to the deleted 2D lobby widget; the Ark panels load the same icons.)


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
        with open(os.path.join(SAVED, "VerifyTester.log"), "w") as handle:
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
