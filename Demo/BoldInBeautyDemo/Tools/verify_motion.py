"""Plays the bb-Showreel motion pack in PIE (owner 2026-10-09: "use them all"): the pieces load, the Ark screens play the
ads (on the LED ad pillars) / trailer / reels, the menu, a station, a summon, pages and a fight play their pieces; Set 2 (sheets
09-18): the fight plays world VFX pinned to the operator and the angels (dodge smear, ability VFX, status loops, flinch, death
dissolve), the objective tracker and the story dialogue panel; every piece of the manifest plays through the console hook.
Saved/VerifyMotion.log, screenshots Saved/Combat/motion_*.png.

Old docstring of verify_combat: Plays the Zenless-style combat and traversal pass in PIE (owner request 2026-10-09, Notion "Dev notes"): lower jumps, the
4-hit basic string and its combo counter, Daze filling to a stun, the chain attack on switch, the angels learning the Guard
counter from basic-attack spam (and guarding), the boss bar, wall climbing and swimming; takes HUD screenshots on the way.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"

Writes Saved/VerifyCombat.log (last line RESULT PASS|FAIL) and Saved/Combat/*.png (screenshots with the HUD).
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


def start_stage(stage):
    chapter = (stage - 1) // 8 + 1
    controller().bib_set_campaign_progress(stage - 1)
    hub = bib_hub.hub()
    hub.click_action("tab:5")
    hub.click_action("camp:ch:%d" % chapter)
    hub.click_action("camp:start:%d" % stage)
    yield from wait_until(lambda: "Chapter" in str(hud().get_editor_property("stage_title")), 60, "stage")
    yield 0.25
    yield from shot("motion_stage_transition")
    yield 2.0


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE")
    yield 2.0
    yield from wait_until(lambda: bib_hub.hub() is not None, 120, "the Ark")
    pieces = LIB.get_meta_value(world(), "motion:pieces")
    check(pieces == len(MANIFEST) and pieces >= 240, "all %d motion pieces are available (%d)" % (len(MANIFEST), pieces))
    yield 3.0
    screens, playing = LIB.get_meta_value(world(), "screens"), LIB.get_meta_value(world(), "screens:playing")
    check(screens >= 14, "the Ark has its video screens (%d)" % screens)
    check(playing >= screens - 1 and playing > 0, "the screens play (%d of %d)" % (playing, screens))
    ads = LIB.get_meta_value(world(), "screens:ads")
    check(ads >= 12, "the six ads run on the LED ad pillars (%d screens)" % ads)
    loc = ARK.get_station_location(world(), "government")
    if loc and character():
        character().set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + 120), False, True)
        yaw = 0.0
        for st in ARK.get_station_ids(world()):
            pass
        yield 0.5
        pawn = character()
        fwd = pawn.get_actor_forward_vector()
        face(unreal.Vector(loc.x + fwd.x * 600, loc.y + fwd.y * 600, loc.z + 900))
        yield 1.5
        yield from shot("motion_ark_screen")
    ARK.open_menu(world())
    yield 0.2
    yield from shot("motion_menu_transition")
    yield 1.0
    check(played("tr3") and played("nav-bar"), "the menu opens with transition 03 and the nav bar")
    ARK.close_screens(world())
    yield 0.5
    ARK.open_station(world(), "factory", 0)
    yield 0.35
    yield from shot("motion_station_open")
    yield 1.0
    check(played("tr2") and played("page-header") and played("banner-select"), "a station opens with transition 02, the page header (and banner select at the factory)")
    actions = [str(a) for a in ARK.get_panel_actions(world())]
    log("factory actions: %s" % actions[:30])
    summon = [a for a in actions if a.startswith("sum:go:10")] + [a for a in actions if a.startswith("sum:go:")]
    if summon:
        ARK.panel_action(world(), summon[0])
        yield 2.0
        yield from shot("motion_summon")
        yield 3.0
        check(played("summon-charge") and any(played(p) for p in ("reveal-rare", "reveal-epic", "reveal-legendary", "reveal-secret")),
              "a summon plays the charge and a rarity reveal (%s)" % summon[0])
    ARK.panel_action(world(), "tab:5")
    yield 1.0
    check(played("campaign-map"), "the campaign page opens with the campaign map")
    ARK.close_screens(world())
    yield 0.6
    check(played("btn-secondary"), "closing plays the secondary-button press")
    yield from start_stage(16)
    check(played("tr4") or played("tr5"), "a stage starts with transition 04 / 05")
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
        yield 0.6
        if i == 0 or i == len(secs) - 1:
            end = time.monotonic() + 12.0
            n = 0
            while time.monotonic() < end and enemies():
                char = character()
                target = min(enemies(), key=lambda e: (e.get_actor_location() - char.get_actor_location()).length())
                step_to(target)
                n += 1
                if n % 9 == 3:
                    yield from press_many(["Run", "Parry"])
                elif n % 9 == 6:
                    yield from press("Move1")
                    yield 0.3
                    yield from shot("motion_fx_ability_%d" % i)
                else:
                    yield from press("Attack")
                yield 0.12
            yield from shot("motion_fight_%d" % i)
        for _ in range(12):
            if not enemies():
                break
            controller().bib_damage_enemies(1.0e9)
            yield 0.5
    yield from wait_until(lambda: LIB.get_current_screen(controller()) == unreal.BIBScreen.RESULTS, 20, "results")
    yield 0.6
    yield from shot("motion_victory")
    check(played("victory"), "a cleared stage plays VICTORY")
    check(played("target-lock") and played("combo-counter"), "a fight plays the target lock and the combo counter")
    check(played("boss-intro"), "the boss section plays the boss intro")
    fx_played = sorted(p for p in MANIFEST if p.startswith("fx") and played(p))
    log("Set 2 VFX played by the fight: %s" % fx_played)
    check(any(p.endswith("dodge-dash-smear") for p in fx_played), "a dodge plays the dodge smear on the operator")
    check(any(p.endswith("angel-death-dissolve") for p in fx_played), "a kill plays the angel death dissolve")
    check(any(p[:4] in ("fx12", "fx13", "fx14", "fx15", "fx16", "fx17", "fx18") for p in fx_played), "an ability plays its operator VFX")
    check(played("objective-tracker"), "a section start plays the objective tracker")
    old_ids = ["nav-bar", "btn-primary", "btn-secondary", "settings-controls", "loading", "modal", "notice-toast", "page-header", "currency-bar",
               "reward-popup", "shop-card", "level-up", "resource-gain", "banner-select", "summon-charge", "reveal-rare", "reveal-epic",
               "reveal-legendary", "reveal-secret", "summon-x10", "ticket-use", "hp-bar", "squad-switch", "skill-buttons", "ultimate-ready",
               "damage-numbers", "boss-intro", "miniboss-warning", "combo-counter", "defense-prompts", "victory", "defeat", "target-lock",
               "campaign-map", "season-pass", "quest-complete", "ark-floor", "arena-rank", "mail-inbox", "battle-power", "night-shift",
               "rhythm-lanes", "boss-rush", "joystick-ring", "joystick-hex", "joystick-tactical", "mobile-actions", "mobile-switch-dodge",
               "mobile-hud-layout", "tr1", "tr2", "tr3", "tr4", "tr5"]
    all_ids = sorted(MANIFEST)
    check(all(p in MANIFEST for p in old_ids), "the first 54 pieces are still in the manifest")
    done_by_events = [p for p in all_ids if played(p)]
    log("pieces played by game events in this run: %d of %d" % (len(done_by_events), len(all_ids)))
    # every piece must load and draw: play each one through the console hook, screenshot a few
    unreal.BIBBlueprintLibrary.return_to_hub(controller())
    yield from wait_until(lambda: bib_hub.hub() is not None, 60, "the Ark again")
    yield 1.0
    shots = {"fx12-07-ultimate-reality-destabilisation": 1.5, "fx09-04-perfect-dodge-afterimage": 0.5, "fx10-01-infection": 0.8,
             "story-dialogue": 1.0, "objective-tracker": 0.8, "judge-mode-wheel": 0.8, "gauges-a": 1.0, "global-exchange-panel": 1.0,
             "reveal-secret": 1.3, "boss-intro": 2.6, "ultimate-ready": 0.6, "mail-inbox": 2.0, "level-up": 1.0, "joystick-ring": 0.4,
             "notice-toast": 1.6, "tr5": 0.6, "quest-complete": 2.8, "victory": 1.5}
    bad = []
    for p in all_ids:
        ok = controller().bib_motion(p, "")
        if not ok:
            bad.append(p)
            continue
        if p in shots:
            yield shots[p]
            yield from shot("motion_piece_" + p)
        else:
            yield 0.12
    check(not bad, "every piece plays through the console hook (failed: %s)" % bad)
    done = [p for p in all_ids if played(p)]
    log("pieces played in this run: %d of %d: %s" % (len(done), len(all_ids), done))
    log("not reached in this run (need their moment): %s" % [p for p in all_ids if p not in done])


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
        with open(os.path.join(SAVED, "VerifyMotion.log"), "w") as handle:
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
