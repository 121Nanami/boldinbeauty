"""Walks the 2026-10-07 features in PIE through the real hub (UBIBHubWidget.ClickAction): Lobby > Inventory (tester items, a
boost paying Gold, Charge), Elements, Events (weekly operation, event shop), the Ark shop, the operator page's Attunement
tile, the campaign's quick clear spending Charge, and the element system in real combat: a Water squad hits Boss Rush round 1
(a Fire boss) for +15%. Writes Saved/VerifyGacha.log.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"   (needs the editor build of 2026-10-07)
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
LIB = unreal.BIBBlueprintLibrary


def log(text):
    LOG.append(text)
    unreal.log("[verify_gacha] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def hud():
    return LIB.get_hud_info(controller())


def map_name():
    return unreal.GameplayStatics.get_current_level_name(world(), True)


def widgets(name):
    w = world()
    return [x for x in unreal.ObjectIterator(unreal.UserWidget)
            if x.get_class().get_name() == name + "_C" and not x.get_name().startswith("Default__") and x.get_world() == w and x.is_in_viewport()]


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


def meta(key):
    return LIB.get_meta_value(controller(), key)


def overlay():
    return str(LIB.get_arcade_overlay_state())


def cmd(command):
    return LIB.arcade_overlay_command(command)


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


def back_to_hub():
    results = widgets("WBP_Results")
    if results:
        results[0].get_editor_property("Continue").on_clicked.broadcast()
    else:
        LIB.return_to_hub(controller())
    yield from wait_until(lambda: bib_hub.in_ark(), 60, "back in the Ark")
    yield 1.0


def ids():
    h = hub()
    return [str(h.get_grid_id(i)) for i in range(h.get_grid_count())] if h else []


def console(text):
    unreal.SystemLibrary.execute_console_command(world(), text, controller())


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None and hub() is not None, 60, "PIE and the Ark")
    yield 2.0

    # ---- lobby tiles
    act("tab:2")
    check(all(x in ids() for x in ("go:inventory", "go:elements", "go:events")), "lobby shows Inventory, Elements and Events (%s)" % ids())

    # ---- inventory
    act("go:inventory")
    check("Inventory" in title() and hub().get_grid_count() == 13, "the inventory lists 13 items (%s, %d)" % (title(), hub().get_grid_count()))
    act("gx:tst:items")
    check(meta("items") >= 65, "tester: +5 of every item (%d)" % meta("items"))
    gold = meta("gold")
    act("gx:use:3")
    check(meta("gold") > gold, "a 24 h Gold boost pays Gold (%d -> %d): %s" % (gold, meta("gold"), status()))
    act("gx:tst:charge")
    check(meta("charge") == 240, "tester: Charge 240 (%d)" % meta("charge"))

    # ---- elements and events
    act("back")
    act("go:elements")
    check("Elements" in title() and hub().get_grid_count() == 8, "the element guide shows 8 elements (%s)" % title())
    act("back")
    act("go:events")
    check("Events" in title() and "go:event-ops" in ids() and "go:event-shop" in ids(), "the events hub (%s, %s)" % (title(), ids()))
    act("go:event-ops")
    check(hub().get_grid_count() == 8, "the weekly operation has 8 missions (%d)" % hub().get_grid_count())
    act("back")
    act("go:event-shop")
    check("Event shop" in title() and hub().get_grid_count() == 10, "the event shop has 10 goods (%s)" % title())

    # ---- the Ark shop
    act("tab:4")
    act("go:ark")
    check("go:ark-shop" in ids(), "the Ark has a shop tile")
    act("go:ark-shop")
    check("Ark shop" in title() and hub().get_grid_count() == 8, "the Ark shop has 8 goods (%s)" % title())

    # ---- operator page: element and Attunement
    act("tab:0")
    act("op:sel:5")
    check("gx:attune" in ids(), "Luna's page has the Attunement tile (%s)" % ids())

    # ---- campaign: quick clear spends Charge
    controller().bib_set_campaign_progress(8)
    act("tab:5")
    act("camp:ch:1")
    bar = [str(hub().get_bar_id(i)) for i in range(hub().get_bar_count())]
    check("camp:sweep:8:1" in bar and "gx:inv" in bar, "the chapter page offers quick clears and the Charge button (%s)" % bar)
    charge = meta("charge")
    act("camp:sweep:8:1")
    check(meta("charge") == charge - 10, "quick clear of Chapter 1 level 8 costs 10 Charge (%d -> %d): %s" % (charge, meta("charge"), status()))

    # ---- the element system in real combat: Water squad vs Boss Rush round 1 (a Fire boss)
    console("BIBTeam Myrcia Blood Weather")
    yield 0.5
    act("tab:2")
    act("go:arcade")
    act("arc:boss")
    yield from wait_until(lambda: "Boss Rush" in str(hud().get_editor_property("stage_title")), 30, "Boss Rush round 1")
    yield 1.0
    check(meta("enemyElements") >= 1, "Boss Rush enemies carry an element (%d)" % meta("enemyElements"))
    pc = controller()
    pc.bib_damage_enemies(1000.0)
    yield 0.2
    check(meta("lastPopup") == 1150, "a Water hit on a Fire boss deals +15%% (1000 -> %d)" % meta("lastPopup"))
    LIB.abandon_stage(pc)
    yield 1.0
    yield from back_to_hub()
    console("BIBTeam Luna Judge Narrator")
    yield 0.5


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
        with open(os.path.join(SAVED, "VerifyGacha.log"), "w") as handle:
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
