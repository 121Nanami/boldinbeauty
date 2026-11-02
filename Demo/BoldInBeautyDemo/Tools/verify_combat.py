"""Plays the Zenless-style combat and traversal pass in PIE (owner request 2026-10-09, Notion "Dev notes"): lower jumps, the
4-hit basic string and its combo counter, Daze filling to a stun, the chain attack on switch, the angels learning the Guard
counter from basic-attack spam (and guarding), the boss bar, wall climbing and swimming; takes HUD screenshots on the way.
Names in game since 2026-10-13: the Daze meter is STRAIN, a stun is a BREAK, the chain attack is the RELAY STRIKE (keys below keep
the old names). Per-operator hit feel (hit stop, reactions, camera, ultimate beat) is checked by Tools/verify_feel.py.

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


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE")
    yield 2.0
    yield from wait_until(lambda: bib_hub.hub() is not None, 90, "the Ark")
    # ---- jump (owner: "jump height feels way to high")
    jump_z, gravity = feel("jumpZ"), feel("gravity")
    apex = (jump_z ** 2) / (2.0 * 980.0 * gravity / 100.0) if gravity > 0 else 9999
    check(apex < 110.0, "jump apex ~%.0f cm (JumpZ %d, gravity x%.2f; was ~90 cm floaty and 360 cm for Luna)" % (apex, jump_z, gravity / 100.0))
    yield from shot("ark")

    # ---- Chapter 2 boss level: combos, Daze, stun, chain, the angels learning
    ok = yield from start_stage(16)
    if not ok:
        return
    all_sections = sections()
    npcs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BIBStoryNPC"]
    if npcs:
        loc = npcs[0].get_actor_location()
        character().set_actor_location(unreal.Vector(loc.x - 300, loc.y, 150.0), False, True)
        yield 0.5
        for _ in range(8):
            yield from press("Talk")
            yield 0.25
    yield from enter_section(all_sections[0])
    controller().bib_angels(0.0)
    yield from shot("battle_start")
    unreal.BIBAnimeStyleVolume.reset_frame_stats()
    unreal.BIBAnimeStyleVolume.request_accent_frame(world(), True)
    unreal.SystemLibrary.execute_console_command(world(), "bib.Debug.ImpactHold 1")
    yield 0.3
    yield from shot("impact_frame")
    unreal.SystemLibrary.execute_console_command(world(), "bib.Debug.ImpactHold 0")
    combo_peak, daze_peak, stunned_seen, chain_seen = 0, 0, False, False
    switched = False
    start_name = str(hud().get_editor_property("active_name"))
    guard_seen = 0
    end = time.monotonic() + 40.0
    while time.monotonic() < end:
        alive = enemies()
        if not alive:
            break
        char = character()
        target = min(alive, key=lambda e: (e.get_actor_location() - char.get_actor_location()).length())
        if (target.get_actor_location() - char.get_actor_location()).length() > 260.0:
            step_to(target)
        else:
            face(target.get_actor_location())
        yield from press("Attack")
        yield 0.12
        combo_peak = max(combo_peak, feel("combo"))
        daze_peak = max(daze_peak, feel("maxDaze"))
        guard_seen = max(guard_seen, feel("guarding"))
        if feel("stunned") > 0:
            stunned_seen = True
        if feel("chain") == 1 and not switched:
            chain_seen = True
            yield from shot("stun_chain")
            yield from press("SwitchOperator")
            yield 0.3
            switched = str(hud().get_editor_property("active_name")) != start_name
        if switched and stunned_seen and feel("guard") >= 30 and guard_seen:
            break
        # keep the team alive while it stands in the crowd
        if float(hud().get_editor_property("hp_fraction")) < 0.35:
            controller().bib_damage_enemies(1.0e9)
    accents = unreal.BIBAnimeStyleVolume.get_accent_frames()
    check(accents >= 10, "action moments insert extra fresh frames (the 13th frame): %d accents in the fight" % accents)
    check(combo_peak >= 4, "basic attacks chain into a combo counter (peak %d hits)" % combo_peak)
    check(daze_peak > 0 or stunned_seen, "hits fill the angels' Strain (peak %d%%)" % daze_peak)
    check(stunned_seen, "a full Strain meter breaks an angel")
    check(chain_seen, "a break opens the relay strike (Tab)")
    check(switched, "Tab during the relay window brings the next operator in (%s -> %s)" % (start_name, hud().get_editor_property("active_name")))
    yield from clear_section()

    # ---- the boss section: boss bar on screen
    for section in all_sections[1:]:
        yield from enter_section(section)
        if any(str(e.get_editor_property("kind").name).upper() == "BOSS" for e in enemies()):
            boss = [e for e in enemies() if str(e.get_editor_property("kind").name).upper() == "BOSS"][0]
            step_to(boss, 600.0)
            yield 1.0
            yield from shot("boss")
            hits = 0
            for _ in range(60):
                if not boss.is_alive():
                    break
                step_to(boss, 260.0)
                yield from press("Attack")
                yield 0.15
                hits += 1
                if hits == 20:
                    yield from shot("angels_adapted")
                if float(hud().get_editor_property("hp_fraction")) < 0.35:
                    controller().bib_fill_ult()
            check(boss.get_health_fraction() < 1.0, "the boss takes damage from the string (health %.3f)" % boss.get_health_fraction())
            yield from shot("boss_fight")
        yield from clear_section()

    # basic-attack spam through the stage teaches the angels to guard (judged after the boss fight: Judge, who came in on the
    # chain, cannot basic-attack in Witness mode, so the spam pauses for a while)
    guard = feel("guard")
    check(guard >= 30, "the angels learned the Guard counter from basic-attack spam (expertise %d%%, %d counters known)" % (guard, feel("known")))
    log("angels guarding at once (peak, section 1): %d; adaptation %d%%" % (guard_seen, feel("adapt")))

    # ---- climbing: walk into a tall wall
    yield from wait_until(lambda: LIB.get_current_screen(controller()) == unreal.BIBScreen.RESULTS, 20, "results")
    unreal.BIBBlueprintLibrary.return_to_hub(controller())
    yield from wait_until(lambda: bib_hub.hub() is not None, 30, "the Ark again")
    ok = yield from start_stage(17)
    if ok:
        blocks = [b for b in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.load_class(None, "/Script/BoldInBeautyDemo.BIBBlock"))]
        walls = []
        for b in blocks:
            origin, extent = b.get_actor_bounds(False)
            if extent.z > 200.0 and min(extent.x, extent.y) > 20.0 and max(extent.x, extent.y) > 300.0:
                walls.append((b, origin, extent))
        climbed = False
        for b, origin, extent in walls[:6]:
            char = character()
            # stand 100 cm off the wall's long face, facing it
            if extent.x >= extent.y:
                pos = unreal.Vector(origin.x, origin.y - extent.y - 90.0, origin.z - extent.z + 110.0)
                face_at = unreal.Vector(origin.x, origin.y, pos.z)
            else:
                pos = unreal.Vector(origin.x - extent.x - 90.0, origin.y, origin.z - extent.z + 110.0)
                face_at = unreal.Vector(origin.x, origin.y, pos.z)
            char.set_actor_location(pos, False, True)
            face(face_at)
            yield 0.3
            for _ in range(40):
                yield from press("MoveForward")
                if feel("climbing") == 1:
                    climbed = True
                    break
            if climbed:
                z0 = character().get_actor_location().z
                for _ in range(30):
                    yield from press("MoveForward")
                check(character().get_actor_location().z > z0 + 30.0, "climbing moves up the wall (%.0f -> %.0f)" % (z0, character().get_actor_location().z))
                yield from shot("climb")
                yield from press("Jump")
                yield 0.5
                check(feel("climbing") == 0, "Space jumps off the wall")
                break
        check(climbed, "walking into a tall wall starts a climb (%d walls tried)" % min(6, len(walls)))
        # swimming: any water volume in the map
        vols = [v for v in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.PhysicsVolume) if v.get_editor_property("water_volume")]
        if vols:
            origin, extent = vols[0].get_actor_bounds(False)
            character().set_actor_location(unreal.Vector(origin.x, origin.y, origin.z + extent.z - 60.0), False, True)
            yield 1.5
            check(feel("swimming") == 1, "deep water is swum in (%s)" % vols[0].get_actor_label() if hasattr(vols[0], "get_actor_label") else "water")
            yield from shot("swim")
        else:
            log("no water volume in this stage's map (swim not checked)")


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
        with open(os.path.join(SAVED, "VerifyCombat.log"), "w") as handle:
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
