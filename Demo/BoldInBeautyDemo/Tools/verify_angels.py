"""Fights every angel type in PIE and logs what its tactical brain did (owner request 2026-10-13, goal 2: "every Angel AI should fight
at or above a skilled player's level while staying fair"; BIBRules/BIBAngelBrain.h, BIBGame/BIBEnemyBrain.cpp). For each type
(Grunt = Angel, Brute = Heavy, Ranged, Mage, Assassin, MiniBoss, Boss) two fights on Chapter 1 Level 1:

  1. BEHAVIOURS - a group of that type (bosses with two grunt escorts) against a bot that plays with habits the angels can read:
     basic-attack spam, Specials (telegraphed heavies), early dodges always to the left, short blocks, jumps, whiffs at range.
     Halfway the angels are brought low (BIBDamageEnemies) to provoke regrouping. Logs the brain's counters per behaviour
     (UBIBBlueprintLibrary::GetMetaValue "angel:b:<Role>:<Behavior>", "angel:m:<Role>:<Move>"; cleared with BIBAngelResetStats).
  2. CLEAN PLAY - one angel (bosses pre-damaged to ~8%: their health pools are tuned for a full team) against a bot that only
     reads what is on screen: it dodges (Shift + F, i-frames) when a telegraphed strike is about to land ("angel:threatMs", the
     time to the next strike - every strike flashes first) and attacks otherwise. OK when the angel dies within the time limit.

Gameplay is not changed: console commands (BIBSpawnAngel, BIBDamageEnemies, BIBAngels), input injection and read-only getters.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"
    BIB_ANGELS_ONLY="Ranged,Boss" limits the run; BIB_VERIFY_KEEP_OPEN=1 keeps the editor open afterwards.

Writes Saved/VerifyAngels.log: a type x behaviour table and the clean-play results; the last line is RESULT PASS|FAIL.
"""

import math
import os
import time
import traceback

import unreal

LOG = []
STATE = {"fails": [], "rows": [], "sections": []}
SAVED = unreal.Paths.project_saved_dir()
LIB = unreal.BIBBlueprintLibrary
BEHAVIORS = ["Spacing", "Punish", "Defense", "Feint", "Adapt", "Coordination", "SelfPreservation", "Kit"]
MOVES = ["Strike", "Guard", "Feint", "Unblockable", "Blink", "Nova", "Volley", "Dash strike", "Anti-air", "Flank", "Combo", "Sweep",
         "Charge", "Lunge"]
# kind (BIBSpawnAngel), role (stat rows), group size, escorts, clean-play time limit (s), pre-damage for the clean fight
TYPES = [
    ("Grunt", "Angel", 3, 0, 90.0, 1.0),
    ("Brute", "Heavy", 2, 0, 120.0, 1.0),
    ("Ranged", "Ranged", 3, 0, 90.0, 1.0),
    ("Mage", "Mage", 2, 0, 90.0, 1.0),
    ("Assassin", "Assassin", 2, 0, 90.0, 1.0),
    ("MiniBoss", "MiniBoss", 1, 2, 120.0, 0.08),
    ("Boss", "Boss", 1, 2, 150.0, 0.08),
]
TEAM = ["Kuu", "Blood", "Shiraishi"]   # data kits: the attack repeats while held
REACH = 260.0


def log(text):
    LOG.append(text)
    unreal.log("[verify_angels] " + text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def character():
    pc = controller()
    return pc.get_controlled_pawn() if pc else None


def map_name():
    return unreal.GameplayStatics.get_current_level_name(world(), True)


def game_time():
    return unreal.GameplayStatics.get_time_seconds(world())


def cmd(text):
    unreal.SystemLibrary.execute_console_command(world(), text, controller())


def angel(key):
    return LIB.get_meta_value(world(), "angel:" + key)


def hud():
    return LIB.get_hud_info(controller())


def enemies():
    cls = unreal.load_class(None, "/Script/BoldInBeautyDemo.BIBEnemy")
    return [e for e in unreal.GameplayStatics.get_all_actors_of_class(world(), cls) if e.is_alive()]


def team_hp():
    h = hud()
    return float(h.get_editor_property("hp")) if h else 0.0


_ACTIONS = {}


def inject(*names):
    """Presses the named input actions together for one frame (call once per frame)."""
    subsystem = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]
    for name in names:
        if name not in _ACTIONS:
            _ACTIONS[name] = unreal.load_asset("/Game/BIB/Input/IA_" + name)
        subsystem.inject_input_vector_for_action(_ACTIONS[name], unreal.Vector(1.0, 0.0, 0.0), [], [])


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


def face(target):
    char = character()
    d = target.get_actor_location() - char.get_actor_location()
    controller().set_control_rotation(unreal.Rotator(roll=0.0, pitch=-10.0, yaw=math.degrees(math.atan2(d.y, d.x))))


def face_away(target):
    """A real whiff: swing at empty air (escorts crowding the commander would otherwise eat the swing, and a hit is no opening)."""
    char = character()
    d = target.get_actor_location() - char.get_actor_location()
    controller().set_control_rotation(unreal.Rotator(roll=0.0, pitch=-10.0, yaw=math.degrees(math.atan2(-d.y, -d.x))))


def nearest(live):
    here = character().get_actor_location()
    return min(live, key=lambda e: (e.get_actor_location() - here).length())


def distance_to(target):
    a = character().get_actor_location()
    b = target.get_actor_location()
    return math.hypot(a.x - b.x, a.y - b.y)


def start_run():
    cmd("BIBGrantAll")
    cmd("BIBTeam " + " ".join(TEAM))
    yield 0.8
    LIB.start_stage(controller(), 1)
    ok = yield from wait_until(lambda: map_name() == "L_Chapter1_Stage1" and character() is not None, 60, "stage 1")
    if not ok:
        return False
    yield 2.5
    npcs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BP_BIBStoryNPC_C"]
    if npcs:
        loc = npcs[0].get_actor_location()
        character().set_actor_location(unreal.Vector(loc.x - 300, loc.y, loc.z), False, True)
        yield 0.5
        for _ in range(6):
            inject("Talk")
            yield 0.3
    sections = sorted([a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == "BP_BIBSection_C"],
                      key=lambda a: a.get_editor_property("section_index"))
    if not sections:
        return False
    door = sections[0].get_editor_property("entry_door")
    yield from wait_until(lambda: door is None or door.is_open(), 15, "door 1")
    c = sections[0].get_actor_location()
    character().set_actor_location(unreal.Vector(c.x, c.y, 150.0), False, True)
    yield from wait_until(lambda: len(enemies()) > 0, 10, "section 1 spawn")
    yield 0.5
    # Clear the stage's own angels: the test spawns its own.
    for _ in range(12):
        if not enemies():
            break
        controller().bib_damage_enemies(1.0e9)
        yield 0.3
    return True


def leave_stage():
    LIB.abandon_stage(controller())
    yield 1.5
    LIB.return_to_hub(controller())
    yield from wait_until(lambda: unreal.BIBArkLibrary.is_in_ark(controller()) if controller() else False, 60, "back in the Ark")
    yield 1.5


def bring_low(fraction):
    """BIBDamageEnemies in small steps until every angel is at or under fraction of its health."""
    for _ in range(400):
        live = enemies()
        if not live or all(e.get_health_fraction() <= fraction for e in live):
            return
        controller().bib_damage_enemies(150.0 if max(e.get_health_fraction() for e in live) < 0.5 else 2000.0)
        yield 0.05


def fight(seconds, habits, low_at=None, respawn=None):
    """The bot. habits=True plays readable habits; False plays clean (dodge only what is telegraphed, attack otherwise).
    respawn: when the group is beaten before the time is up, call it and keep fighting (the behaviour phase needs the whole
    window: a full-power team kills a group in a few seconds, too soon for punishes, adaptation or regrouping to show)."""
    start = game_time()
    dodge_ready = 0.0
    special_at = start + 2.0
    jump_at = start + 4.0
    block_until = -1.0
    block_at = start + 6.0
    whiff_until = -1.0
    lowered = False
    hp_start = team_hp()
    hits = 0
    last_hp = hp_start
    while game_time() - start < seconds:
        now = game_time()
        live = enemies()
        if not live:
            if respawn is None:
                return True, now - start, hits
            respawn()
            STATE["respawns"] = STATE.get("respawns", 0) + 1
            yield 1.0
            continue
        if low_at is not None and not lowered and now - start >= low_at:
            lowered = True
            yield from bring_low(0.12)   # under every tier's RetreatHealth (0.25 / 0.2 / 0.15): regrouping / caution can show
            continue
        target = nearest(live)
        face(target)
        dist = distance_to(target)
        threat = angel("threatMs")
        hp = team_hp()
        if hp < last_hp - 0.5:
            hits += 1
        last_hp = hp
        if habits:
            # Early dodges, always to the left; spam; Specials; short blocks; jumps; whiffs from out of reach.
            if 0 <= threat <= 650 and now >= dodge_ready:
                inject("Run", "Parry", "MoveLeft")
                dodge_ready = now + 0.7
            elif now < block_until:
                inject("Block")
            elif now >= block_at:
                block_until, block_at = now + 1.0, now + 7.0
            elif now >= jump_at:
                inject("Jump")
                jump_at = now + 5.0
            elif now >= special_at and dist < 600:
                inject("Special")
                special_at = now + 2.0   # telegraphed heavies: what the angels dodge (ranged ones only when one reaches them)
            elif now < whiff_until:
                face_away(target)
                inject("Attack")
            elif dist > REACH + 450 and int(now) % 6 == 0:
                whiff_until = now + 0.6   # swing at nothing (a whiff the angels may punish)
            elif REACH + 80 < dist <= REACH + 450 and int(now) % 3 == 0:
                whiff_until = now + 0.6   # a whiff just out of reach: inside the angels' punish reach (Reach + PunishReach)
            elif dist > REACH:
                inject("Run", "MoveForward")
            elif int(now) % 4 == 1:
                whiff_until = now + 0.5   # a whiff up close (turned away): the opening a boss in melee range punishes
            else:
                inject("Attack")
        else:
            if 0 <= threat <= 220 and now >= dodge_ready:
                inject("Run", "Parry", "MoveLeft")
                dodge_ready = now + 0.45
            elif dist > REACH:
                inject("Run", "MoveForward")
            elif threat < 0 or threat > 450:
                inject("Attack")
        yield 0
    return False, seconds, hits


def counters(role):
    out = {b: angel("b:%s:%s" % (role, b)) for b in BEHAVIORS}
    moves = {m: angel("m:%s:%s" % (role, m)) for m in MOVES}
    return out, {m: n for m, n in moves.items() if n > 0}


def verify_type(kind, role, count, escorts, limit, start_fraction):
    # ---- 1. behaviours
    ok = yield from start_run()
    if not ok:
        STATE["fails"].append("%s: stage did not start" % kind)
        return
    cmd("BIBAngels 0.6")   # a mind that has learned something (counters show up in the kit)
    cmd("BIBAngelResetStats")
    def spawn_group():
        cmd("BIBSpawnAngel %s %d" % (kind, count))
        if escorts:
            cmd("BIBSpawnAngel Grunt %d" % escorts)

    spawn_group()
    yield 1.0
    STATE["respawns"] = 0
    # Ranged angels, mages, mini bosses and bosses show their rarer behaviours (a punish, a coordinated cast) a couple of times a minute: a
    # longer window keeps the check from failing on an unlucky 40 s.
    seconds = 70.0 if role in ("Ranged", "Mage", "MiniBoss", "Boss") else 40.0
    yield from fight(seconds, True, low_at=seconds * 0.6, respawn=spawn_group)
    log("%-9s behaviour phase: %.0f s of fighting, group respawned %d times" % (role, seconds, STATE["respawns"]))
    fired, moves = counters(role)
    missing = [b for b in BEHAVIORS if fired[b] <= 0]
    STATE["rows"].append((role, fired, moves))
    log("%-9s behaviours %s" % (role, " ".join("%s=%d" % (b, fired[b]) for b in BEHAVIORS)))
    log("%-9s moves      %s" % (role, " ".join("%s=%d" % (m, n) for m, n in moves.items()) or "none"))
    if missing:
        STATE["fails"].append("%s: behaviours not seen: %s" % (role, ", ".join(missing)))
    cmd("BIBAngelStats")
    for _ in range(12):
        if not enemies():
            break
        controller().bib_damage_enemies(1.0e9)
        yield 0.3
    yield from leave_stage()

    # ---- 2. clean play
    ok = yield from start_run()
    if not ok:
        STATE["fails"].append("%s: stage did not start (clean play)" % kind)
        return
    cmd("BIBAngels 0.0")
    cmd("BIBSpawnAngel %s 1" % kind)
    yield 1.0
    if start_fraction < 1.0:
        yield from bring_low(start_fraction)
    won, seconds, hits = yield from fight(limit, False)
    log("%-9s clean play: %s in %.1f s, %d hits taken" % (role, "WON" if won else "NOT WON", seconds, hits))
    STATE["rows"][-1] = STATE["rows"][-1] + ((won, seconds, hits),)
    if not won:
        STATE["fails"].append("%s: clean play did not beat it in %.0f s" % (role, limit))
    yield from leave_stage()


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None, 90, "PIE")
    yield 3.0
    yield from wait_until(lambda: unreal.BIBArkLibrary.is_in_ark(controller()), 90, "the Ark")
    cmd("BIBAngelReload")
    only = [n.strip().lower() for n in os.environ.get("BIB_ANGELS_ONLY", "").split(",") if n.strip()]
    for kind, role, count, escorts, limit, start_fraction in TYPES:
        if only and kind.lower() not in only and role.lower() not in only:
            continue
        yield from verify_type(kind, role, count, escorts, limit, start_fraction)


def report():
    log("")
    log("%-9s %s | clean play" % ("type", " ".join("%-6s" % b[:6] for b in BEHAVIORS)))
    for row in STATE["rows"]:
        role, fired = row[0], row[1]
        clean = row[3] if len(row) > 3 else None
        cells = " ".join("%-6s" % ("yes" if fired[b] > 0 else "NO") for b in BEHAVIORS)
        tail = "n/a" if clean is None else ("won %.0f s, %d hits" % (clean[1], clean[2]) if clean[0] else "NOT WON")
        log("%-9s %s | %s" % (role, cells, tail))
    for fail in STATE["fails"]:
        log("FAIL " + fail)
    log("RESULT %s %d types, %d problems" % ("PASS" if not STATE["fails"] else "FAIL", len(STATE["rows"]), len(STATE["fails"])))


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
        with open(os.path.join(SAVED, "VerifyAngels.log"), "w") as handle:
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
            STATE["fails"].append("exception")
            self.finish()


Runner()
