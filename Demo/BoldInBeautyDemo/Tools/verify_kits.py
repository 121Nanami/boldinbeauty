"""Data-driven PIE check of EVERY ability of every roster operator. Each operator gets a fresh Stage 1 run (team: the operator plus
Luna and Judge; Luna, Judge and Narrator use Luna/Judge/Narrator in their own order), then every HUD ability (basic, 1, 2, 3, Special,
Ultimate, plus the alternative ultimate and the Narrator's second form) is cast once on live enemies through the same Enhanced Input
injection a player's keys use. Before and after each cast it records operator HP and ultimate, the enemies' HP sum and count, the
per-slot cooldowns, the operator's and the enemies' GameplayTags and the HUD message feed. A cast is OK when any of these changed:
cooldown started, enemy HP/count dropped, an enemy got a BIB.Status.* tag, the operator got a BIB.State.* tag (or any BIB tag), operator
HP/ultimate changed, a message appeared. Otherwise it is FLAGGED, as is any LogBIB/LogAbilitySystem error, ensure or crash line.
Does not change gameplay: only console commands, input injection and the read-only getters on UBIBBlueprintLibrary.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"
    BIB_KITS_ONLY="Luna,Arita"  limits the run to those operators; BIB_VERIFY_KEEP_OPEN=1 keeps the editor open afterwards.

Writes Saved/VerifyKits.log; the last line is `RESULT PASS|FAIL n ok, m flagged, k skipped`. A cast with no live enemy or with the tested
operator swapped out is SKIPPED (not judged) and retried once on a fresh stage.
"""

import glob
import os
import re
import time
import traceback

import unreal

LOG = []
FAILS = []
SAVED = unreal.Paths.project_saved_dir()
HOLD_SECONDS = 1.5
BASIC_HOLD_SECONDS = 0.6
MAX_CAST_SECONDS = 12.0
INPUT_FOR_KEY = {"LMB": "Attack", "1": "Move1", "2": "Move2", "3": "Move3", "4": "Special", "E": "UltimateE", "R": "UltimateR"}
SLOT_FOR_KEY = {"LMB": "Basic", "1": "Move1", "2": "Move2", "3": "Move3", "4": "Special", "E": "Ultimate", "R": "R"}
ERROR_PATTERN = re.compile(r"((LogBIB\w*|LogAbilitySystem|LogGameplayEffects|LogGameplayTags|LogGameplayAbilities)\S*: (Error|Warning)|"
                           r"Ensure condition failed|Assertion failed|Fatal error|LogPython: Error)")
ROWS = []          # (operator, slot, name, verdict, evidence)
OP_TAGS_SEEN = set()
ENEMY_TAGS_SEEN = set()
ERROR_LINES = []
STATE = {"ok": 0, "flagged": 0, "skipped": [], "section": -1, "sections": [], "log_offset": 0, "log_path": None}


def log(text):
    LOG.append(text)
    unreal.log("[verify_kits] " + text)


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


def enemies():
    return list(unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.load_class(None, "/Script/BoldInBeautyDemo.BIBEnemy")))


def alive_enemies():
    return [e for e in enemies() if e.is_alive()]


def cmd(text):
    unreal.SystemLibrary.execute_console_command(world(), text, controller())


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


def input_subsystem():
    return [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]


def hold(name, seconds):
    """Injects the input every frame for `seconds` of game time (a 1-frame press when seconds is 0)."""
    subsystem = input_subsystem()
    action = unreal.load_asset("/Game/BIB/Input/IA_" + name)
    start = game_time()
    while True:
        subsystem.inject_input_vector_for_action(action, unreal.Vector(1.0, 0.0, 0.0), [], [])
        yield 0
        if game_time() - start >= seconds:
            break
    yield 0
    yield 0


# ----------------------------------------------------------------------------------------------------------------- log scanning

def find_log():
    candidates = glob.glob(os.path.join(unreal.Paths.project_log_dir(), "*.log")) + glob.glob(os.path.expanduser("~/Library/Logs/Unreal Engine/*/BoldInBeautyDemo.log"))
    candidates = [c for c in candidates if os.path.basename(c).startswith("BoldInBeautyDemo") and "backup" not in c.lower()]
    return max(candidates, key=os.path.getmtime) if candidates else None


def new_error_lines():
    path = STATE["log_path"]
    if not path:
        return []
    found = []
    try:
        with open(path, "rb") as handle:
            handle.seek(STATE["log_offset"])
            data = handle.read()
            STATE["log_offset"] += len(data)
        for line in data.decode("utf-8", "ignore").splitlines():
            if "[verify_kits]" in line or "Cmd:" in line:
                continue
            if ERROR_PATTERN.search(line):
                found.append(line.strip()[:260])
    except Exception as error:
        log("log scan failed: %s" % error)
    return found


# ----------------------------------------------------------------------------------------------------------------- snapshots

def split_tags(text):
    return set(t.strip() for t in str(text).replace("\n", ",").split(",") if t.strip())


def snapshot():
    h = hud()
    pc = controller()
    live = alive_enemies()
    enemy_tags = set()
    for e in live:
        enemy_tags |= split_tags(unreal.BIBBlueprintLibrary.get_enemy_tags(e))
    return {
        "hp": float(h.get_editor_property("hp")),
        "ult": float(h.get_editor_property("ult")),
        "name": str(h.get_editor_property("active_name")),
        "enemy_hp": sum(float(unreal.BIBBlueprintLibrary.get_enemy_hp(e)) for e in live),
        "enemies": len(live),
        "cooldowns": {str(a.get_editor_property("key")): float(a.get_editor_property("cooldown_remaining")) for a in h.get_editor_property("abilities")},
        "op_tags": split_tags(unreal.BIBBlueprintLibrary.get_operator_tags(pc)),
        "enemy_tags": enemy_tags,
        "messages": [str(m) for m in h.get_editor_property("messages")],
        "action": str(h.get_editor_property("action_line")),
        "menu": bool(h.get_editor_property("judge_mode_menu")),
    }


def classify(before, after, seen_op, seen_enemy, key, new_errors, extra=""):
    reasons = []
    cd_before = before["cooldowns"].get(key, 0.0)
    cd_after = after["cooldowns"].get(key, 0.0)
    if cd_after > cd_before + 0.05:
        reasons.append("cooldown %.1f->%.1f" % (cd_before, cd_after))
    if after["enemy_hp"] < before["enemy_hp"] - 0.5:
        reasons.append("enemy HP %.0f->%.0f" % (before["enemy_hp"], after["enemy_hp"]))
    if after["enemies"] < before["enemies"]:
        reasons.append("enemies %d->%d" % (before["enemies"], after["enemies"]))
    status = sorted(t for t in (seen_enemy - before["enemy_tags"]) if t.startswith("BIB.Status."))
    if status:
        reasons.append("enemy status " + ",".join(status))
    state = sorted(t for t in (seen_op - before["op_tags"]) if t.startswith("BIB.State.") or t.startswith("BIB.Buff."))
    if state:
        reasons.append("operator tags +" + ",".join(state))
    if abs(after["hp"] - before["hp"]) > 0.5:
        reasons.append("operator HP %.0f->%.0f" % (before["hp"], after["hp"]))
    fresh = [m for m in after["messages"] if m not in before["messages"]]
    if fresh:
        reasons.append("msg '%s'" % fresh[0][:60])
    if extra:
        reasons.append(extra)
    return reasons


def record(operator, slot, name, reasons, before, after, new_errors):
    if reasons:
        STATE["ok"] += 1
        verdict = "OK"
        evidence = "; ".join(reasons)[:170]
    else:
        STATE["flagged"] += 1
        verdict = "FLAG"
        evidence = "no effect: HP %.0f/%.0f enemies %d/%d enemyHP %.0f/%.0f action '%s'" % (
            before["hp"], after["hp"], before["enemies"], after["enemies"], before["enemy_hp"], after["enemy_hp"], after["action"][:40])
    for line in new_errors:
        STATE["flagged"] += 1
        ERROR_LINES.append("%s %s: %s" % (operator, slot, line))
        ROWS.append((operator, slot, name, "FLAG", "log: " + line[:150]))
    ROWS.append((operator, slot, name, verdict, evidence))
    log("CAST %-11s %-9s %-34s %-4s %s" % (operator, slot, name[:34], verdict, evidence))


# ----------------------------------------------------------------------------------------------------------------- stage helpers

def _dist2(a, b):
    return (a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2


def teleport_next_to_enemies():
    live = alive_enemies()
    char = character()
    if not live or not char:
        return
    here = char.get_actor_location()
    nearest = min(live, key=lambda e: _dist2(e.get_actor_location(), here))
    target = nearest.get_actor_location()
    offset = unreal.Vector(-350.0, 0.0, 0.0)
    teleport(target.x + offset.x, target.y + offset.y, max(150.0, target.z + 80.0))
    look = unreal.MathLibrary.find_look_at_rotation(char.get_actor_location(), target)
    controller().set_control_rotation(unreal.Rotator(0, look.pitch * 0.0, look.yaw))


def enter_section(index):
    sections = STATE["sections"]
    section = sections[index]
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


def start_operator_run(op, team):
    pc = controller()
    cmd("BIBGrantAll")
    cmd("BIBTeam " + " ".join(team))
    yield 0.8
    unreal.BIBBlueprintLibrary.start_stage(pc, 1)
    ok = yield from wait_until(lambda: map_name() == "L_Chapter1_Stage1" and character() is not None, 60, "stage 1 travel (%s)" % op)
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
        log("no sections found for " + op)
        return False
    yield from enter_section(0)
    yield 0.5
    return len(alive_enemies()) > 0


def leave_stage():
    pc = controller()
    unreal.BIBBlueprintLibrary.abandon_stage(pc)
    yield 1.5
    unreal.BIBBlueprintLibrary.return_to_hub(controller())
    yield from wait_until(lambda: unreal.BIBArkLibrary.is_in_ark(controller()) if controller() else False, 60, "back in the Ark (the hub)")
    yield 1.5


# ----------------------------------------------------------------------------------------------------------------- casting

def cast(op, entry, label_suffix="", menu_choice=None):
    """Casts one HUD ability entry and records the effect. Returns the after-snapshot."""
    key = str(entry.get_editor_property("key"))
    name = str(entry.get_editor_property("name"))
    if name.startswith("Ult"):
        slot = "Ultimate"
    elif name.startswith("Special"):
        slot = "Special"
    elif name.startswith("Reg attack"):
        slot = "Basic"
    else:
        slot = SLOT_FOR_KEY.get(key, key)
    slot += label_suffix
    if key not in INPUT_FOR_KEY:
        log("skip %s %s (unknown key %s)" % (op, name, key))
        return None
    has_enemies = yield from ensure_enemies()
    active = str(hud().get_editor_property("active_name"))
    if not has_enemies or active != op:
        # 2026-10-09: the sweep used to cast anyway and FLAG the result, so a cleared room or a tested operator who went down
        # (Luna swapped in) showed up as broken abilities. Those casts prove nothing either way: SKIP and retry on a fresh stage.
        why = "no live enemies left" if not has_enemies else "tested operator not active (%s is)" % active
        STATE["skipped"].append((op, slot, name, why))
        ROWS.append((op, slot, name, "SKIP", why))
        log("CAST %-11s %-9s %-34s SKIP %s" % (op, slot, name[:34], why))
        return None
    teleport_next_to_enemies()
    yield 0.3
    if name.startswith("Ult"):
        controller().bib_fill_ult()
        yield 0.2
    new_error_lines()  # drop anything logged before this cast
    before = snapshot()
    seen_op, seen_enemy = set(before["op_tags"]), set(before["enemy_tags"])
    input_name = INPUT_FOR_KEY[key]
    if "(hold)" in name or key == "LMB":
        yield from hold(input_name, HOLD_SECONDS if "(hold)" in name else BASIC_HOLD_SECONDS)
    else:
        yield from hold(input_name, 0)
    start = time.monotonic()
    quiet = 0
    toggled = False
    if menu_choice:
        # Judge's court setting opens a menu for 4 s; pick the option while it is open (Move3 = Judge mode).
        opened = yield from wait_until(lambda: snapshot()["menu"], 3, "court menu open")
        if opened:
            yield 0.2
            yield from hold(menu_choice, 0)
            log("Judge court menu: picked %s" % menu_choice)
    while time.monotonic() - start < MAX_CAST_SECONDS:
        yield 0.1
        current = snapshot()
        seen_op |= current["op_tags"]
        seen_enemy |= current["enemy_tags"]
        quiet = quiet + 1 if current["action"] == "" else 0
        if "(toggle)" in name and not toggled and time.monotonic() - start > 2.0:
            yield from hold(input_name, 0)
            toggled = True
        if quiet >= 6:
            break
    yield 0.4
    after = snapshot()
    seen_op |= after["op_tags"]
    seen_enemy |= after["enemy_tags"]
    OP_TAGS_SEEN.update(t for t in seen_op if t.startswith("BIB."))
    ENEMY_TAGS_SEEN.update(t for t in seen_enemy if t.startswith("BIB."))
    errors = new_error_lines()
    extra = "menu opened" if (after["menu"] and not before["menu"]) else ""
    record(op, slot, name, classify(before, after, seen_op, seen_enemy, key, errors, extra), before, after, errors)
    buffs = sorted(t for t in seen_op if t.startswith("BIB.Buff."))
    if buffs:
        yield from watch_buffs(op, slot, name, buffs)
    return after


def watch_buffs(op, slot, name, buffs):
    """Logs when the operator's BIB.Buff.* tags (timed buffs) appear and disappear after a cast (max 45 s)."""
    start = time.monotonic()
    last = None
    while time.monotonic() - start < 45.0:
        current = sorted(t for t in split_tags(unreal.BIBBlueprintLibrary.get_operator_tags(controller())) if t.startswith("BIB.Buff."))
        if current != last:
            log("BUFFS %s %s %s t=%.1fs: %s" % (op, slot, name, time.monotonic() - start, ", ".join(current) or "(none)"))
            last = current
        if not current:
            break
        yield 0.25
    else:
        log("BUFFS %s %s %s still active after 45 s: %s" % (op, slot, name, ", ".join(last or [])))


def run_ability_list(op, suffix=""):
    entries = [a for a in hud().get_editor_property("abilities") if not str(a.get_editor_property("name")).startswith("Change ultimate")]
    names = [str(a.get_editor_property("name")) for a in entries]
    log("%s%s abilities: %s" % (op, suffix, names))
    if op == "Alchemist":
        # Ring leader needs a living enemy with Infection, so cast it straight after Infection (before Transmutation kills the group).
        order = {"LMB": 0, "1": 1, "4": 2, "2": 3, "3": 4, "E": 5}
        entries = sorted(entries, key=lambda a: order.get(str(a.get_editor_property("key")), 9))
    for entry in entries:
        key = str(entry.get_editor_property("key"))
        if op == "Narrator":
            if key == "1" and not suffix:
                controller().bib_set_narrator_speed(500.0)
                yield 0.3
            elif suffix and key in ("1", "3"):
                controller().bib_set_narrator_speed(1500.0)
                yield 0.3
        choice = "Move3" if (op == "Judge" and key == "2") else None
        yield from cast(op, entry, suffix, menu_choice=choice)
        if choice:
            yield 0.3
    return [str(a.get_editor_property("name")) for a in hud().get_editor_property("abilities")]


def verify_operator(op, team):
    log("==== %s (team %s) ====" % (op, " ".join(team)))
    ready = yield from start_operator_run(op, team)
    if not ready:
        STATE["flagged"] += 1
        ROWS.append((op, "setup", "-", "FLAG", "stage 1 / enemies did not start"))
        log("FLAG %s setup failed" % op)
        if map_name() != "L_Ark":
            yield from leave_stage()
        return
    first = [str(a.get_editor_property("name")) for a in hud().get_editor_property("abilities")]
    after_names = yield from run_ability_list(op)
    if hud().get_editor_property("active_name") != op:
        log("%s: active operator is now %s" % (op, hud().get_editor_property("active_name")))
    if hud().get_editor_property("active_name") != op:
        log("%s: not active any more, so a changed ability list is not a second form; no form 2 pass" % op)
    elif after_names and [n for n in after_names if not n.startswith("Change")] != [n for n in first if not n.startswith("Change")]:
        yield from run_ability_list(op, " (form 2)")
    retry = [r for r in STATE["skipped"] if r[0] == op and "(form 2)" not in r[1] and "(retry)" not in r[1]]
    if retry:
        # a fresh Stage 1 for the casts that had no live enemies or no tested operator (only the base form: a second form would
        # first have to be reached again)
        names = set(r[2] for r in retry)
        for r in retry:
            STATE["skipped"].remove(r)
        log("%s: retrying %d skipped cast(s) on a fresh stage: %s" % (op, len(names), sorted(names)))
        yield from leave_stage()
        ready = yield from start_operator_run(op, team_for(op))
        if ready:
            for entry in [a for a in hud().get_editor_property("abilities") if str(a.get_editor_property("name")) in names]:
                key = str(entry.get_editor_property("key"))
                if op == "Narrator" and key == "1":
                    controller().bib_set_narrator_speed(500.0)
                    yield 0.3
                choice = "Move3" if (op == "Judge" and key == "2") else None
                yield from cast(op, entry, " (retry)", menu_choice=choice)
    if op == "Alchemist":
        yield from alchemist_swap_check()
    # alternative ultimate (BIBUltVariant 1) for operators that have the "Change ultimate" entry
    if any(str(a.get_editor_property("name")).startswith("Change ultimate") for a in hud().get_editor_property("abilities")):
        controller().bib_ult_variant(1)
        yield 0.5
        entries = [a for a in hud().get_editor_property("abilities") if str(a.get_editor_property("key")) == "E"]
        if entries:
            yield from cast(op, entries[0], " (alt)")
    yield from leave_stage()


def alchemist_swap_check():
    """Owner (2026-10-09): Alchemist infects, mutates and swaps out - her infection must keep working while she is benched."""
    yield from leave_stage()
    ready = yield from start_operator_run("Alchemist", team_for("Alchemist"))
    if not ready:
        ROWS.append(("Alchemist", "swap", "infect and swap", "SKIP", "stage did not start"))
        return
    teleport_next_to_enemies()
    yield 0.3
    yield from hold("Move1", 0)
    yield 3.5
    infected = [e for e in alive_enemies() if "BIB.Status.Infection" in split_tags(unreal.BIBBlueprintLibrary.get_enemy_tags(e))]
    yield from hold("SwitchOperator", 0)
    yield 1.0
    active = str(hud().get_editor_property("active_name"))
    hp_before = sum(float(unreal.BIBBlueprintLibrary.get_enemy_hp(e)) for e in infected if e.is_alive())
    teleport(-20000, -20000)   # the new operator walks away: nothing but the infection can hurt them
    yield 22.0
    hp_after = sum(float(unreal.BIBBlueprintLibrary.get_enemy_hp(e)) for e in infected if e.is_alive())
    dead = len([e for e in infected if not e.is_alive()])
    ok = bool(infected) and active != "Alchemist" and (hp_after < hp_before - 1.0 or dead > 0)
    evidence = "infected %d, active %s, infected HP %.0f -> %.0f, %d died" % (len(infected), active, hp_before, hp_after, dead)
    STATE["ok" if ok else "flagged"] += 1
    ROWS.append(("Alchemist", "swap", "infect and swap out", "OK" if ok else "FLAG", evidence))
    log("CAST %-11s %-9s %-34s %-4s %s" % ("Alchemist", "swap", "infect and swap out", "OK" if ok else "FLAG", evidence))


def team_for(op):
    if op == "Luna":
        return ["Luna", "Judge", "Narrator"]
    if op == "Judge":
        return ["Judge", "Luna", "Narrator"]
    if op == "Narrator":
        return ["Narrator", "Luna", "Judge"]
    return [op, "Luna", "Judge"]


def flow():
    ok = yield from wait_until(lambda: world() is not None and controller() is not None, 90, "PIE")
    yield 3.0
    STATE["log_path"] = find_log()
    if STATE["log_path"]:
        STATE["log_offset"] = os.path.getsize(STATE["log_path"])
    log("engine log: %s" % STATE["log_path"])
    yield from wait_until(lambda: unreal.BIBArkLibrary.is_in_ark(controller()), 90, "the Ark (the hub)")
    if map_name() != "L_Ark":
        log("not in the Ark: " + map_name())
    cmd("BIBCheckTags")
    yield 0.5
    for line in new_error_lines():
        STATE["flagged"] += 1
        ERROR_LINES.append("startup: " + line)
        ROWS.append(("-", "startup", "BIBCheckTags", "FLAG", line[:150]))
    roster = [str(r.get_editor_property("name")) for r in unreal.BIBBlueprintLibrary.get_roster(controller())]
    only = [n.strip() for n in os.environ.get("BIB_KITS_ONLY", "").split(",") if n.strip()]
    log("roster: %s" % roster)
    for op in roster:
        if only and op not in only:
            continue
        yield from verify_operator(op, team_for(op))


def report():
    log("")
    log("TABLE operator | slot | ability | verdict | evidence")
    for operator, slot, name, verdict, evidence in ROWS:
        log("TABLE %s | %s | %s | %s | %s" % (operator, slot, name, verdict, evidence))
    flagged = [r for r in ROWS if r[3] == "FLAG"]
    log("")
    log("FLAGGED (%d):" % len(flagged))
    for operator, slot, name, verdict, evidence in flagged:
        log("  %s | %s | %s | %s" % (operator, slot, name, evidence))
    log("SKIPPED, harness could not judge (%d):" % len(STATE["skipped"]))
    for operator, slot, name, why in STATE["skipped"]:
        log("  %s | %s | %s | %s" % (operator, slot, name, why))
    log("LogBIB/LogAbilitySystem error/ensure lines (%d):" % len(ERROR_LINES))
    for line in ERROR_LINES:
        log("  " + line)
    log("Distinct BIB.* tags seen on operators (%d): %s" % (len(OP_TAGS_SEEN), ", ".join(sorted(OP_TAGS_SEEN))))
    log("Distinct BIB.* tags seen on enemies (%d): %s" % (len(ENEMY_TAGS_SEEN), ", ".join(sorted(ENEMY_TAGS_SEEN))))
    log("RESULT %s %d ok, %d flagged, %d skipped" % ("PASS" if STATE["flagged"] == 0 and not FAILS else "FAIL", STATE["ok"], STATE["flagged"], len(STATE["skipped"])))


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
        with open(os.path.join(SAVED, "VerifyKits.log"), "w") as handle:
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
