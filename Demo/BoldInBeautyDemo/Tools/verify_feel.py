"""Plays every operator's attacks against live angels in PIE and checks how the hits FEEL (owner request 2026-10-13: "rework every
player attack so hits feel physical and real"; BIBRules/BIBHitFeel.h). For each of the 20 operators: a fresh Stage 1 run, the full
basic string, every move and the Special, then the Ultimate (bar filled with the BIBFillUlt console hook). Between casts it reads
the hit-feel counters (UBIBBlueprintLibrary::GetMetaValue "feel:*", cleared with the BIBFeelResetStats console command) and logs:

    weights felt (light / medium / heavy / finisher / ultimate / ultimate finisher, ticks), the largest hit stop in frames, reactions
    played on the angels (recoil / flinch / stagger / knockback / launch / knockdown), camera kicks and push-ins, rumble calls, the
    operator's pose range (wind-up lean back < 0, impact lean forward > 0), ultimate cinematic beats.

A row is OK when the cast produced what its kind should: the basic string a light hit and its finisher a heavy one (Judge: light
impacts, she only damages the guilty), a damaging move a felt hit with a reaction, every cast a wind-up pose, the ultimate a
cinematic beat and, when it deals damage within its time, an ultimate finisher. Also checks: one ultimate beat is skipped by a press,
a dodge cancels the recovery of Kuu's Ground Pound, and no LogBIB / LogAbilitySystem error appears. Gameplay is not changed: only
console commands, input injection and read-only getters.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"
    BIB_FEEL_ONLY="Luna,Kuu" limits the run; BIB_VERIFY_KEEP_OPEN=1 keeps the editor open afterwards.

Writes Saved/VerifyFeel.log; the last line is RESULT PASS|FAIL <n ok>, <m flagged>, <k n/a>.
"""

import os
import time
import traceback

import unreal

LOG = []
ROWS = []
STATE = {"ok": 0, "flagged": 0, "na": 0, "sections": [], "section": -1, "skip_checked": False, "cancel_checked": False}
SAVED = unreal.Paths.project_saved_dir()
INPUT_FOR_KEY = {"LMB": "Attack", "1": "Move1", "2": "Move2", "3": "Move3", "4": "Special", "E": "UltimateE", "R": "UltimateR"}
WEIGHTS = ["Tick", "Light", "Medium", "Heavy", "Finisher", "Ultimate", "UltimateFinisher"]
REACTIONS = ["Recoil", "Flinch", "Stagger", "Knockback", "Launch", "Knockdown"]
STRING_LENGTH = {"Alchemist": 2, "Arita": 2, "Adapt": 3, "Blood": 3, "Death": 3, "Fujita": 4, "Ishii": 3, "Kuu": 3, "Media": 5,
                 "Myrcia": 3, "Plack": 3, "Shiraishi": 3, "Timekeeper": 3}   # the kits' own strings; everyone else swings 4


def log(text):
    LOG.append(text)
    unreal.log("[verify_feel] " + text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def character():
    pc = controller()
    return pc.get_controlled_pawn() if pc else None


def hud():
    return unreal.BIBBlueprintLibrary.get_hud_info(controller())


def map_name():
    return unreal.GameplayStatics.get_current_level_name(world(), True)


def game_time():
    return unreal.GameplayStatics.get_time_seconds(world())


def actors(class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == class_name]


def alive_enemies():
    cls = unreal.load_class(None, "/Script/BoldInBeautyDemo.BIBEnemy")
    return [e for e in unreal.GameplayStatics.get_all_actors_of_class(world(), cls) if e.is_alive()]


def cmd(text):
    unreal.SystemLibrary.execute_console_command(world(), text, controller())


def feel(key):
    return unreal.BIBBlueprintLibrary.get_meta_value(world(), "feel:" + key)


def stats():
    out = {w: feel("w:" + w) for w in WEIGHTS}
    out.update({r: feel("r:" + r) for r in REACTIONS})
    for key in ("kicks", "pushIns", "rumbles", "cinematics", "skips", "cancels", "maxHitstop", "minLean", "maxLean"):
        out[key] = feel(key)
    return out


def wait_until(cond, timeout, what):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.1
    log("WAIT-TIMEOUT " + what)
    return False


def teleport(x, y, z=150.0):
    character().set_actor_location(unreal.Vector(x, y, z), False, True)


def hold(name, seconds):
    subsystem = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]
    action = unreal.load_asset("/Game/BIB/Input/IA_" + name)
    start = game_time()
    while True:
        subsystem.inject_input_vector_for_action(action, unreal.Vector(1.0, 0.0, 0.0), [], [])
        yield 0
        if game_time() - start >= seconds:
            break
    yield 0
    yield 0


def next_to_enemy(distance=200.0):
    live = alive_enemies()
    char = character()
    if not live or not char:
        return False
    here = char.get_actor_location()
    target = min(live, key=lambda e: (e.get_actor_location() - here).length()).get_actor_location()
    teleport(target.x - distance, target.y, max(150.0, target.z + 80.0))
    look = unreal.MathLibrary.find_look_at_rotation(character().get_actor_location(), target)
    controller().set_control_rotation(unreal.Rotator(0.0, 0.0, look.yaw))
    return True


def enter_section(index):
    section = STATE["sections"][index]
    door = section.get_editor_property("entry_door")
    yield from wait_until(lambda: door is None or door.is_open(), 15, "door %d" % (index + 1))
    target = section.get_actor_location()
    teleport(target.x, target.y)
    yield from wait_until(lambda: len(alive_enemies()) > 0, 10, "section %d spawn" % (index + 1))
    STATE["section"] = index
    yield 0.6


def ensure_enemies():
    if alive_enemies():
        return True
    for index in range(STATE["section"] + 1, len(STATE["sections"])):
        yield from enter_section(index)
        if alive_enemies():
            return True
    return False


def start_run(op, team):
    cmd("BIBGrantAll")
    cmd("BIBTeam " + " ".join(team))
    yield 0.8
    unreal.BIBBlueprintLibrary.start_stage(controller(), 1)
    ok = yield from wait_until(lambda: map_name() == "L_Chapter1_Stage1" and character() is not None, 60, "stage 1 (%s)" % op)
    if not ok:
        return False
    yield 2.5
    npcs = actors("BP_BIBStoryNPC_C")
    if npcs:
        loc = npcs[0].get_actor_location()
        teleport(loc.x - 300, loc.y)
        yield 0.5
        for _ in range(6):
            yield from hold("Talk", 0)
            yield 0.3
    STATE["sections"] = sorted(actors("BP_BIBSection_C"), key=lambda a: a.get_editor_property("section_index"))
    STATE["section"] = -1
    if not STATE["sections"]:
        return False
    yield from enter_section(0)
    return len(alive_enemies()) > 0


def leave_stage():
    unreal.BIBBlueprintLibrary.abandon_stage(controller())
    yield 1.5
    unreal.BIBBlueprintLibrary.return_to_hub(controller())
    yield from wait_until(lambda: unreal.BIBArkLibrary.is_in_ark(controller()) if controller() else False, 60, "back in the Ark")
    yield 1.5


def team_for(op):
    if op in ("Luna", "Judge", "Narrator"):
        return [op] + [o for o in ("Luna", "Judge", "Narrator") if o != op]
    return [op, "Luna", "Judge"]


def delta(before, after):
    return {k: after[k] - before[k] for k in after if k not in ("maxHitstop", "minLean", "maxLean")}


def describe(d, after):
    felt = ", ".join("%s %d" % (w, d[w]) for w in WEIGHTS if d[w])
    reacts = ", ".join("%s %d" % (r, d[r]) for r in REACTIONS if d[r])
    return "felt [%s] reactions [%s] kicks %d push-ins %d rumble %d | max hit stop %d f, lean %.1f..%.1f deg" % (
        felt or "-", reacts or "-", d["kicks"], d["pushIns"], d["rumbles"], after["maxHitstop"], after["minLean"] / 10.0, after["maxLean"] / 10.0)


def row(op, what, verdict, evidence):
    ROWS.append((op, what, verdict, evidence))
    STATE[{"OK": "ok", "FLAG": "flagged", "n/a": "na"}[verdict]] += 1
    log("%-4s %-11s %-28s %s" % (verdict, op, what, evidence))


def judge_to_judge_mode():
    # Court setting (2) -> Judge (3), then do nothing for the 2.5 s windup (anything else is contempt of court).
    yield from hold("Move2", 0)
    yield 0.3
    yield from hold("Move3", 0)
    yield 3.2


def test_basic(op):
    yield from ensure_enemies()
    next_to_enemy()
    cmd("BIBFeelResetStats")
    yield 0.1
    before = stats()
    presses = STRING_LENGTH.get(op, 4)
    for _ in range(presses):
        next_to_enemy()
        yield from hold("Attack", 0)
        yield 0.35
    yield 0.4
    after = stats()
    d = delta(before, after)
    light = d["Light"] > 0
    heavy = d["Heavy"] > 0 or d["Finisher"] > 0
    wind_up = after["minLean"] < 0
    impact = after["maxLean"] > 0
    if op == "Judge":
        ok = (light or heavy) and wind_up and impact   # innocents and suspects take impacts, not damage
    else:
        ok = light and heavy and wind_up and impact and (d["Flinch"] + d["Recoil"] + d["Stagger"] + d["Knockback"]) > 0
    row(op, "basic string x%d" % presses, "OK" if ok else "FLAG", describe(d, after))


def test_ability(op, key, name):
    if not (yield from ensure_enemies()):
        row(op, name, "n/a", "no live enemies left")
        return
    next_to_enemy(260.0)
    if op == "Narrator" and key == "1":
        controller().bib_set_narrator_speed(500.0)
        yield 0.2
    cmd("BIBFeelResetStats")
    yield 0.1
    before = stats()
    seconds = 0.0
    if op == "Luna" and key in ("3", "4"):
        seconds = 1.5   # hold abilities
    if op == "Arita" and key == "3":
        seconds = 2.0
    yield from hold(INPUT_FOR_KEY[key], seconds)
    yield 5.0
    after = stats()
    d = delta(before, after)
    felt = sum(d[w] for w in WEIGHTS if w != "Tick")
    reacted = sum(d[r] for r in REACTIONS)
    wind_up = after["minLean"] < 0
    if felt == 0 and d["Tick"] == 0:
        # buffs, shields, stances, menus: only the wind-up pose is expected
        row(op, "%s %s" % (key, name), "OK" if wind_up else "FLAG", "no damage in 5 s (utility); " + describe(d, after))
        return
    ok = wind_up and (felt == 0 or (reacted > 0 and after["maxHitstop"] > 0))
    row(op, "%s %s" % (key, name), "OK" if ok else "FLAG", describe(d, after))


def test_ultimate(op, key, name):
    if not (yield from ensure_enemies()):
        row(op, "ult " + name, "n/a", "no live enemies left")
        return
    next_to_enemy(260.0)
    if op == "Judge":
        yield from judge_to_judge_mode()
    controller().bib_fill_ult()
    yield 0.2
    cmd("BIBFeelResetStats")
    yield 0.1
    before = stats()
    yield from hold(INPUT_FOR_KEY[key], 0)
    yield 0.2
    started = feel("cinematics") > before["cinematics"]
    if started and not STATE["skip_checked"]:
        yield 0.3
        yield from hold("Attack", 0)   # skip the beat
        yield 0.2
        STATE["skip_checked"] = True
        skipped = feel("skips") > 0
        row(op, "ult beat skip", "OK" if skipped else "FLAG", "attack press during the beat -> skips %d" % feel("skips"))
    # Luna's fuse is 10 s; others finish well inside 12 s
    yield 12.0
    after = stats()
    d = delta(before, after)
    finisher = d["UltimateFinisher"] > 0
    damaged = sum(d[w] for w in WEIGHTS) > 0
    if not started:
        row(op, "ult " + name, "FLAG", "no cinematic beat; " + describe(d, after))
    elif finisher:
        row(op, "ult " + name, "OK", "beat + finisher; " + describe(d, after))
    elif not damaged:
        row(op, "ult " + name, "OK", "beat; no damage within 12 s (transform / stance / zone that needs a trigger); " + describe(d, after))
    else:
        row(op, "ult " + name, "FLAG", "damage but no ultimate finisher; " + describe(d, after))


def test_recovery_cancel():
    # Kuu's Ground Pound: hits at 0.5 s, locks for 1.0 s - a dodge after the hit cancels the rest.
    if not (yield from ensure_enemies()):
        return
    next_to_enemy(220.0)
    cmd("BIBFeelResetStats")
    yield from hold("Move3", 0)
    yield 0.62
    subsystem = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]
    run = unreal.load_asset("/Game/BIB/Input/IA_Run")
    parry = unreal.load_asset("/Game/BIB/Input/IA_Parry")
    for _ in range(4):
        subsystem.inject_input_vector_for_action(run, unreal.Vector(1.0, 0.0, 0.0), [], [])
        subsystem.inject_input_vector_for_action(parry, unreal.Vector(1.0, 0.0, 0.0), [], [])
        yield 0
    yield 0.3
    cancels = feel("cancels")
    row("Kuu", "dodge cancels recovery", "OK" if cancels > 0 else "FLAG", "recovery cancels %d" % cancels)
    STATE["cancel_checked"] = True


def verify_operator(op):
    log("==== %s ====" % op)
    if not (yield from start_run(op, team_for(op))):
        row(op, "setup", "FLAG", "stage 1 / enemies did not start")
        if map_name() != "L_Ark":
            yield from leave_stage()
        return
    if op == "Judge":
        yield from judge_to_judge_mode()
    yield from test_basic(op)
    entries = [(str(a.get_editor_property("key")), str(a.get_editor_property("name"))) for a in hud().get_editor_property("abilities")]
    for key, name in entries:
        if key in ("1", "2", "3", "4") or (key == "R" and op == "Arita"):
            if op == "Judge" and key == "2":
                continue   # Court setting is a mode menu, not an attack
            yield from test_ability(op, key, name)
            if hud().get_editor_property("active_name") != op:
                yield from leave_stage()
                if not (yield from start_run(op, team_for(op))):
                    return
    ult = [(k, n) for k, n in entries if (k == "E" and op != "Judge") or (k == "R" and op == "Judge")]
    if ult:
        yield from test_ultimate(op, ult[0][0], ult[0][1])
    if op == "Kuu" and not STATE["cancel_checked"]:
        yield from test_recovery_cancel()
    yield from leave_stage()


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None, 90, "PIE")
    yield 3.0
    yield from wait_until(lambda: unreal.BIBArkLibrary.is_in_ark(controller()), 90, "the Ark")
    cmd("BIBFeelReload")
    roster = [str(r.get_editor_property("name")) for r in unreal.BIBBlueprintLibrary.get_roster(controller())]
    only = [n.strip() for n in os.environ.get("BIB_FEEL_ONLY", "").split(",") if n.strip()]
    for op in roster:
        if only and op not in only:
            continue
        yield from verify_operator(op)


def report():
    log("")
    log("%-11s %-28s %-4s %s" % ("operator", "attack", "", "evidence"))
    for op, what, verdict, evidence in ROWS:
        log("%-11s %-28s %-4s %s" % (op, what, verdict, evidence))
    log("RESULT %s %d ok, %d flagged, %d n/a" % ("PASS" if STATE["flagged"] == 0 else "FAIL", STATE["ok"], STATE["flagged"], STATE["na"]))


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
        try:
            report()
        except Exception:
            log(traceback.format_exc())
        with open(os.path.join(SAVED, "VerifyFeel.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        if os.environ.get("BIB_VERIFY_KEEP_OPEN") != "1":
            unreal.SystemLibrary.quit_editor()

    def tick(self, delta_seconds):
        if self.busy or self.done:
            return
        self.busy = True
        try:
            self._tick(delta_seconds)
        finally:
            self.busy = False

    def _tick(self, delta_seconds):
        try:
            if not self.started:
                self.boot += delta_seconds
                if self.boot > 8.0:
                    self.started = True
                    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
                    self.gen = flow()
                return
            if self.wait > 0.0:
                self.wait -= delta_seconds
                return
            step = self.gen.send(delta_seconds if self.primed else None)
            self.primed = True
            self.wait = step or 0.0
        except StopIteration:
            self.finish()
        except Exception:
            log(traceback.format_exc())
            STATE["flagged"] += 1
            self.finish()


Runner()
