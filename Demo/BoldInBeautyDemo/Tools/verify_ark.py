"""Plays the Ark hub in PIE (owner request 2026-10-07: the 2D lobby became the Ark). Boots from the editor's map with the
developer profile, follows the travel into L_Ark, then: the walking HUD shows only the menu button; the flow-chart menu opens
column by column (Menu / The Ark / Town Square / ...); every place teleports the player onto its ring and opens its counters
with F; a personal page opens straight from the menu; a summon runs in the factory; Chapter 1 level 1 (an editor map) and
Chapter 2 level 1 (built in code in the old hub map) start from the task room with only the battle HUD on screen, and both
come back to the Ark at the task room. Writes Saved/VerifyArk.log.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev [-EnablePlugins=SlateInspectorToolset] -ExecCmds="py <this file>"

BIB_SCREENSHOTS=1 (with the SlateInspectorToolset plugin) saves Saved/Screenshots/ark_<name>.png.
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


def flow():
    pc = controller()
    # ------------------------------------------------------------------ boot: the hub map hands over to the Ark
    ok = yield from wait_until(lambda: in_ark(), 90)
    check(ok, "the game travels into the Ark after the profile is ready (map: %s)" % map_name())
    if not ok:
        return
    yield 1.0
    pc = controller()
    check("L_Ark" in map_name(), "the hub is L_Ark")
    stations = sorted(str(s) for s in ARK.get_station_ids(pc))
    log("stations: %s" % ", ".join(stations))
    expected = ["alley", "arcade", "arena", "arkshop", "armoury", "eventshop", "factory", "gangshop", "ganghall", "government",
                "library", "premium", "resistance", "rollcall", "taskroom", "tower", "trade"]
    check(stations == sorted(expected), "all 17 stations stand in the Ark (%d)" % len(stations))
    check(ARK.is_ark_hud_shown(pc) and not ARK.is_battle_hud_shown(pc), "walking: only the Ark HUD (menu button), no battle HUD")
    check(screen() == "None", "walking: no menu screen open (%s)" % screen())
    here = character().get_actor_location()
    check(dist2d(here, unreal.Vector(43500, 0, 0)) < 1500, "arrived on the plaza in front of the government building (%.0f, %.0f)" % (here.x, here.y))
    check(str(ARK.get_ark_arrival(pc)) == "plaza", "a profile with no saved position starts on the plaza (%s)" % ARK.get_ark_arrival(pc))
    yield from shot("plaza")

    # ------------------------------------------------------------------ the flow-chart menu
    ARK.open_menu(pc)
    yield 0.3
    check(screen() == "ArkMenu", "M opens the menu (%s)" % screen())
    top = [str(x) for x in ARK.get_menu_children(pc, "menu")]
    check(top == ["the-ark", "shops", "ops", "missions", "me", "system"], "first column: %s" % top)
    ARK.menu_click(pc, "the-ark")
    yield 0.2
    ARK.menu_click(pc, "town")
    yield 0.3
    check(str(ARK.get_menu_path(pc)) == "menu / the-ark / town", "clicks open columns: %s" % ARK.get_menu_path(pc))
    yield from shot("menu_town")
    ARK.menu_click(pc, "merchant-row")
    yield 0.3
    check(str(ARK.get_menu_path(pc)) == "menu / the-ark / town / merchant-row", "a fourth column: %s" % ARK.get_menu_path(pc))
    yield from shot("menu_merchants")
    ARK.menu_click(pc, "town")
    yield 0.2
    check(str(ARK.get_menu_path(pc)) == "menu / the-ark / town", "clicking a crumb steps back: %s" % ARK.get_menu_path(pc))
    ARK.close_screens(pc)
    yield 0.2

    # ------------------------------------------------------------------ every place: teleport, stand on the ring, F opens it
    places = [n for n in (str(x) for x in ARK.get_menu_node_ids()) if n.startswith("ark:")]
    shots = {"factory", "armoury", "arcade", "trade", "rollcall", "alley"}
    for node in places:
        station = node[4:]
        before = ARK.get_teleport_count(pc)
        ARK.open_menu(pc)
        yield 0.1
        ARK.menu_click(pc, node)
        yield 1.2   # land and settle
        anchor = ARK.get_station_location(pc, station)
        body = character().get_actor_location()
        check(ARK.get_teleport_count(pc) == before + 1 and screen() == "None", "%s: the menu teleports there" % station)
        check(dist2d(body, anchor) < 400 and abs(body.z - anchor.z) < 250, "%s: standing on the ring (%.0f cm away, dz %.0f)" % (station, dist2d(body, anchor), body.z - anchor.z))
        check(str(ARK.get_station_in_reach(pc)) == station, "%s: the F prompt is for this station (%s)" % (station, ARK.get_station_in_reach(pc)))
        if station in shots:
            yield from shot("at_" + station)
        check(ARK.interact(pc), "%s: F opens it" % station)
        yield 0.3
        title = str(ARK.get_panel_title(pc))
        check(screen() == "ArkPanel" and str(ARK.get_panel_station(pc)) == station and title != "", "%s: panel shows '%s'" % (station, title))
        if station in shots:
            yield from shot("panel_" + station)
        yield from back_to_walking()
        check(screen() == "None", "%s: Back closes the panel" % station)

    # ------------------------------------------------------------------ a personal page opens straight from the menu
    ARK.open_menu(pc)
    yield 0.1
    ARK.menu_click(pc, "me")
    yield 0.2
    ARK.menu_click(pc, "me:mail")
    yield 0.3
    check(screen() == "ArkPanel" and str(ARK.get_panel_title(pc)) == "Mail" and str(ARK.get_panel_station(pc)) == "", "Commander > Mail opens without moving")
    yield from shot("panel_mail")
    yield from back_to_walking()

    # ------------------------------------------------------------------ counters and the factory
    check(ARK.open_station(pc, "rollcall", 1) and str(ARK.get_panel_title(pc)) != "", "Roll Call > Squad opens the team page (%s)" % ARK.get_panel_title(pc))
    yield 0.2
    check(ARK.open_station(pc, "armoury", 1), "Armoury > Equip opens")
    yield 0.2
    yield from back_to_walking()
    ARK.open_menu(pc)
    yield 0.1
    ARK.menu_click(pc, "op:factory")
    yield 1.0
    ARK.interact(pc)
    yield 0.3
    ARK.panel_action(pc, "sum:go:1:1")
    yield 0.4
    check(str(ARK.get_panel_title(pc)) == "Summon results", "a summon in the factory shows its results (%s; %s)" % (ARK.get_panel_title(pc), ARK.get_status_line(pc)))
    yield from shot("factory_summon")
    yield from back_to_walking()

    # ------------------------------------------------------------------ battles: from the task room, only the battle HUD
    pc.bib_set_campaign_progress(8)
    for stage, chapter, label in ((1, 1, "editor map"), (9, 2, "built in code")):
        ARK.open_menu(pc)
        yield 0.1
        ARK.menu_click(pc, "ms:campaign")
        yield 1.0
        check(str(ARK.get_station_in_reach(pc)) == "taskroom", "Missions > Campaign teleports to the task room")
        ARK.interact(pc)
        yield 0.2
        ARK.panel_action(pc, "camp:ch:%d" % chapter)
        yield 0.2
        ARK.panel_action(pc, "camp:start:%d" % stage)
        ok = yield from wait_until(lambda: ARK.is_battle_hud_shown(controller()), 60)
        pc = controller()
        check(ok, "stage %d (%s) starts with the battle HUD (map %s)" % (stage, label, map_name()))
        check(not ARK.is_ark_hud_shown(pc), "stage %d: no menu button, no Ark HUD in battle" % stage)
        info = LIB.get_hud_info(pc)
        check(info.valid and len(info.abilities) > 0 and str(info.objective) != "", "stage %d: battle HUD has the objective and %d abilities" % (stage, len(info.abilities)))
        yield 1.5
        yield from shot("battle_stage%d" % stage)
        LIB.abandon_stage(pc)
        yield 0.5
        LIB.return_to_hub(controller())
        ok = yield from wait_until(lambda: in_ark(), 60)
        yield 1.0
        pc = controller()
        check(ok and str(ARK.get_station_in_reach(pc)) == "taskroom", "after stage %d the player is back in the Ark at the task room (%s)" % (stage, ARK.get_station_in_reach(pc)))

    # ------------------------------------------------------------------ the saved position (owner 2026-10-07)
    ARK.open_menu(pc)
    yield 0.1
    ARK.menu_click(pc, "ark:arcade")
    yield 1.2
    body = character()
    start = body.get_actor_location()
    body.set_actor_location(unreal.Vector(start.x + 700, start.y + 350, start.z + 40), False, True)   # step off the ring
    yield 1.5
    ARK.save_ark_position(pc, False)
    saved = ARK.get_saved_ark_position(pc)
    check(saved is not None, "walking keeps a position in the profile")
    if saved:
        spot = saved[0]
        log("saved position: %.0f, %.0f, %.0f yaw %.0f" % (spot.x, spot.y, spot.z, saved[1]))
        LIB.start_stage(pc, 9)
        ok = yield from wait_until(lambda: ARK.is_battle_hud_shown(controller()), 60)
        check(ok, "a stage starts from there (Chapter 2-1)")
        LIB.abandon_stage(controller())
        yield 0.5
        LIB.return_to_hub(controller())
        ok = yield from wait_until(lambda: in_ark(), 60)
        yield 1.0
        pc = controller()
        here = character().get_actor_location()
        check(ok and str(ARK.get_ark_arrival(pc)) == "saved", "back in the Ark at the saved position (%s)" % ARK.get_ark_arrival(pc))
        check(dist2d(here, spot) < 120, "standing where the player left (%.0f cm away)" % dist2d(here, spot))
        yield from shot("back_at_saved_position")

    # ------------------------------------------------------------------ free roam leaf
    ARK.open_menu(pc)
    yield 0.1
    ARK.menu_click(pc, "missions")
    yield 0.2
    check(str(ARK.get_menu_node_kind(pc, "ms:neotokyo")) == "Action", "Missions > Neo Tokyo is a run-action leaf")
    ARK.close_screens(pc)
    yield 0.2


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
        with open(os.path.join(SAVED, "VerifyArk.log"), "w") as handle:
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
