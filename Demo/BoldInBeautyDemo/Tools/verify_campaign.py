"""Plays the 13-chapter campaign in PIE: the Campaign tab (13 chapters, locks), then real stages from every part of the
campaign — Chapter 2 level 1, a chapter boss level, the first levels that bring in mages and assassins, a late boss level and
the very last stage — comparing what each section actually spawns with the C++ plan (sections, enemy counts per kind), the
mini boss at the end of every level (in the section before the boss on a chapter's last level), the chapter boss, the enemy
health scale, and that the new enemy kinds (ranged, mage, assassin) really fight. Writes Saved/VerifyCampaign.log.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -EnablePlugins=SlateInspectorToolset -ExecCmds="py <this file>"

-BIBDev is the developer profile (every operator, no sign-in, nothing saved). It is not an admin, so the script unlocks stages
with the BIBSetCampaignProgress test hook. To play the same stages by hand use the admin account (every stage open) or the guest
account (a base profile that has to play through).

Environment: BIB_CAMPAIGN_STAGES="9,16,25,..." overrides the list of stages; BIB_VERIFY_KEEP_OPEN=1 keeps the editor open.
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

# stage ids (chapter-level counted through the campaign): 2-1, 2-8 (boss), 3-1 (first mages), 4-1 (first assassins),
# 5-8 (boss with every type), 13-8 (the last stage)
DEFAULT_STAGES = [9, 16, 17, 25, 40, 104]
KINDS = ["GRUNT", "BRUTE", "RANGED", "MAGE", "ASSASSIN", "MINIBOSS", "BOSS"]   # same order as GetStageSectionCounts


def log(text):
    LOG.append(text)
    unreal.log("[verify_campaign] " + text)


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


def hud():
    return unreal.BIBBlueprintLibrary.get_hud_info(controller())


def map_name():
    return unreal.GameplayStatics.get_current_level_name(world(), True)


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


def tile_ids():
    h = hub()
    return [str(h.get_grid_id(i)) for i in range(h.get_grid_count())] if h else []


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


def actors_named(class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() in (class_name, class_name + "_C")]


def enemies():
    return list(unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.load_class(None, "/Script/BoldInBeautyDemo.BIBEnemy")))


def alive_enemies():
    return [e for e in enemies() if e.is_alive()]


def kind_of(enemy):
    return str(enemy.get_editor_property("kind").name).upper().replace("_", "")


def teleport(x, y, z=150.0):
    character().set_actor_location(unreal.Vector(x, y, z), False, True)


def press(name, frames=1):
    subsystem = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith("Default__")][-1]
    action = unreal.load_asset("/Game/BIB/Input/IA_" + name)
    for _ in range(frames):
        subsystem.inject_input_vector_for_action(action, unreal.Vector(1.0, 0.0, 0.0), [], [])
        yield 0
    yield 0
    yield 0


def team_health():
    return sum(float(m.get_editor_property("hp_fraction")) for m in hud().get_editor_property("team") if m.get_editor_property("alive"))


def planned(stage, section):
    return [int(x) for x in unreal.BIBBlueprintLibrary.get_stage_section_counts(stage, section)]


def pages():
    """The Campaign tab: 13 chapters, locks, the chapter page."""
    act("tab:5")
    yield 0.3
    check("campaign" in title().lower(), "Campaign tab opens (%s)" % title())
    ids = tile_ids()
    chapters = [i for i in ids if i.startswith("camp:ch:")]
    check(len(chapters) == 13, "the Campaign tab lists 13 chapters (%d)" % len(chapters))
    controller().bib_set_campaign_progress(0)
    act("camp:ch:2")
    check("locked" in status().lower() and "campaign" in title().lower(), "Chapter 2 is locked at the start (%s)" % status())
    act("camp:ch:13")
    check("locked" in status().lower(), "Chapter 13 is locked at the start (%s)" % status())
    act("camp:ch:1")
    check(title() == "Chapter 1" and "camp:start:8" in tile_ids() and "camp:start:9" not in tile_ids(), "Chapter 1 lists levels 1-8 (%s)" % title())
    act("back")
    controller().bib_set_campaign_progress(8)
    act("camp:ch:2")
    check(title() == "Chapter 2" and "camp:start:9" in tile_ids() and "camp:start:16" in tile_ids(), "Chapter 2 opens once the Chapter 1 boss is cleared (%s)" % title())
    act("back")
    controller().bib_set_campaign_progress(0)
    act("tab:0")


def play_stage(stage):
    chapter, level = (stage - 1) // 8 + 1, (stage - 1) % 8 + 1
    final = level == 8
    name = "Chapter %d - Level %d" % (chapter, level)
    sections_planned = unreal.BIBBlueprintLibrary.get_stage_section_count(stage)
    scales = unreal.BIBBlueprintLibrary.get_stage_scales(stage)
    check(2 <= sections_planned <= 5, "%s plan has %d sections" % (name, sections_planned))
    log("%s: %d sections planned, enemy scales health x%.2f damage x%.2f" % (name, sections_planned, scales.x, scales.y))
    controller().bib_set_campaign_progress(stage - 1)
    act("tab:5")
    act("camp:ch:%d" % chapter)
    act("camp:start:%d" % stage)
    ok = yield from wait_until(lambda: name in str(hud().get_editor_property("stage_title")), 60, "%s to start" % name)
    if not ok:
        return
    yield 1.5
    pc = controller()
    sections = sorted(actors_named("BIBSection"), key=lambda a: a.get_editor_property("section_index"))
    check(len(sections) == sections_planned, "%s built %d sections (plan %d)" % (name, len(sections), sections_planned))
    npcs = actors_named("BIBStoryNPC")
    check(len(npcs) >= 1, "%s has its story contact" % name)
    if npcs:
        loc = npcs[0].get_actor_location()
        teleport(loc.x - 300, loc.y)
        yield 0.5
        for _ in range(8):
            yield from press("Talk")
            yield 0.25
    minis_at, boss_at = [], []
    totals = []
    for section in sections:
        index = section.get_editor_property("section_index")
        door = section.get_editor_property("entry_door")
        yield from wait_until(lambda: door is None or door.is_open(), 20, "%s door %d" % (name, index + 1))
        target = section.get_actor_location()
        teleport(target.x, target.y)
        yield from wait_until(lambda: len(alive_enemies()) > 0, 15, "%s section %d to spawn" % (name, index + 1))
        yield 0.5
        spawned = alive_enemies()
        seen = {k: 0 for k in KINDS}
        for enemy in spawned:
            seen[kind_of(enemy)] = seen.get(kind_of(enemy), 0) + 1
        got = [seen[k] for k in KINDS]
        want = planned(stage, index)
        total = sum(got)
        totals.append(total)
        check(got == want, "%s section %d spawned %s (plan %s, kinds %s)" % (name, index + 1, got, want, KINDS))
        check(10 <= total <= 30, "%s section %d has %d enemies (10-30)" % (name, index + 1, total))
        if seen["MINIBOSS"]:
            minis_at.append(index)
        if seen["BOSS"]:
            boss_at.append(index)
        if index == 0 and chapter >= 2:
            yield from fight_smoke(name, stage, spawned)
        for _ in range(12):
            if not alive_enemies():
                break
            pc.bib_damage_enemies(1.0e9)
            yield 0.5
        check(not alive_enemies(), "%s section %d cleared with BIBDamageEnemies" % (name, index + 1))
    check(len(minis_at) == 1, "%s has exactly one mini-boss (sections %s)" % (name, [i + 1 for i in minis_at]))
    if final:
        check(boss_at == [len(sections) - 1] and minis_at == [len(sections) - 2], "%s: the mini-boss closes section %d, the boss section %d (mini %s, boss %s)" % (
            name, len(sections) - 1, len(sections), [i + 1 for i in minis_at], [i + 1 for i in boss_at]))
    else:
        check(not boss_at and minis_at == [len(sections) - 1], "%s: no boss, the mini-boss ends the last section (mini %s, boss %s)" % (name, [i + 1 for i in minis_at], [i + 1 for i in boss_at]))
    yield from wait_until(lambda: unreal.BIBBlueprintLibrary.get_current_screen(pc) == unreal.BIBScreen.RESULTS, 20, "%s results" % name)
    lines = [str(l) for l in pc.get_editor_property("result_lines")]
    check(bool(pc.get_editor_property("result_success")), "%s cleared (%s)" % (name, lines[:3]))
    if stage < 104:
        want = "Chapter %d unlocked" % (chapter + 1) if final else "unlocked"
        check(any(want in l for l in lines), "%s results announce the unlock ('%s')" % (name, want))
    else:
        check(any("campaign is complete" in l for l in lines), "the last stage says the campaign is complete")
    check(any("Gold" in l for l in lines), "%s results list the rewards" % name)
    unreal.BIBBlueprintLibrary.return_to_hub(pc)
    yield from wait_until(lambda: hub() is not None, 30, "the hub after %s" % name)
    yield 0.5


def fight_smoke(name, stage, spawned):
    """Lets the first section's enemies fight a stationary team until the team has lost some health (at most 25 s): the enemies,
    the new kinds included, must really attack. Then reports where the ranged, mage and assassin enemies stand. The caller
    kills everything afterwards, so the team is never exposed for long."""
    kinds = {}
    for enemy in spawned:
        kinds.setdefault(kind_of(enemy), []).append(enemy)
    log("%s section 1 kinds: %s" % (name, {k: len(v) for k, v in kinds.items()}))
    before = team_health()
    waited = 0.0
    while waited < 25.0 and team_health() > before - 0.03:
        yield 0.5
        waited += 0.5
    after = team_health()
    check(after < before - 0.01, "%s: the enemies hurt the team within %.0f s (team health %.2f -> %.2f)" % (name, waited, before, after))
    char = character()
    if not char:
        return

    def distance(enemy):
        d = enemy.get_actor_location() - char.get_actor_location()
        return (d.x ** 2 + d.y ** 2) ** 0.5

    for kind in ("RANGED", "MAGE", "ASSASSIN"):
        alive = [e for e in kinds.get(kind, []) if e.is_alive()]
        if alive:
            ds = sorted(distance(e) for e in alive)
            log("%s: %s distances after %.0f s (cm): nearest %.0f, median %.0f, farthest %.0f" % (name, kind, waited, ds[0], ds[len(ds) // 2], ds[-1]))


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE")
    yield 2.0
    yield from wait_until(lambda: hub() is not None, 90, "the Ark")
    check("L_Ark" in map_name(), "PIE starts in the Ark (the hub since 2026-10-07; map %s)" % map_name())
    if not hub():
        return
    check(unreal.BIBBlueprintLibrary.get_stage_count() == 104, "the campaign has 104 stages (%d)" % unreal.BIBBlueprintLibrary.get_stage_count())
    s1, s8, s9, s104 = [unreal.BIBBlueprintLibrary.get_stage_scales(s) for s in (1, 8, 9, 104)]
    # Since the 2026-10-06 rebalance Chapter 1 is solved by the balance model like chapters 2-13 (MAIN: 1.00x-2.27x).
    check(abs(s1.x - 1.0) < 1e-6 and s1.x < s8.x < s9.x * 2 and s8.x < s104.x, "enemy scale starts at 1.00 and rises (1-1 %.2f, 1-8 %.2f, 2-1 %.2f, 13-8 %.2f)" % (s1.x, s8.x, s9.x, s104.x))
    check(s9.x >= s8.x * 0.95 and s104.x > 2.0 * s9.x * 0.9 and s104.y > s104.x, "enemies keep growing to the last stage (x%.2f at 9, x%.2f at 104)" % (s9.x, s104.x))
    yield from pages()
    stages = [int(s) for s in os.environ.get("BIB_CAMPAIGN_STAGES", "").split(",") if s.strip()] or DEFAULT_STAGES
    for stage in stages:
        yield from play_stage(stage)


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
        with open(os.path.join(SAVED, "VerifyCampaign.log"), "w") as handle:
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
