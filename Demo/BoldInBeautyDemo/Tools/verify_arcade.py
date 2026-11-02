"""Plays the 2026-10-06 features in PIE through the real hub (UBIBHubWidget.ClickAction) and the Slate overlays: Night Shift
(removed from the arcade 2026-10-15: checks its tile is gone), Rhythm (an autoplayed song), the text prompt (commander name), key capture
(rebinding Jump), the arena (teams, a practice rival, the fight replay), Boss Rush in real combat (round 1 -> round 2, rewards,
damage numbers, the run summary) and the endless tower's "fell on floor" record; the mini-games Halo Fall, Neon Memory, Firewall
Breach and Ark Rally (2026-10-15: a race on Medium, then the player's own track on the buggy). Writes Saved/VerifyArcade.log.

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <this file>"
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
    unreal.log("[verify_arcade] " + text)


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


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None and hub() is not None, 60, "PIE and the Ark")
    yield 2.0

    # ---- lobby: the new tiles
    act("tab:2")
    ids = [str(hub().get_grid_id(i)) for i in range(hub().get_grid_count())]
    check(all(x in ids for x in ("go:friends", "go:gang", "go:arcade", "go:notices")), "lobby shows Friends, Gang, Arcade and Notices (%s)" % ids)
    act("go:friends")
    check("Friends" in title(), "the friends page opens offline (%s)" % title())
    act("back")
    act("go:gang")
    check("Gang" in title(), "the gang page opens offline (%s)" % title())
    act("back")

    # ---- the arcade: Night Shift left it on 2026-10-15 (owner request); its rules live on in Ultimate Custom Night
    act("go:arcade")
    ids = [str(hub().get_grid_id(i)) for i in range(hub().get_grid_count())]
    check("arc:night" not in ids and "arc:rhythm" in ids, "the arcade has no Night Shift tile (%s)" % ids)

    # ---- Rhythm: an autoplayed song
    act("arc:rhythm")
    act("arc:rdiff:1")
    act("arc:song:0")
    yield from wait_until(lambda: LIB.is_arcade_overlay_open(), 5, "the Rhythm overlay")
    check(overlay().startswith("rhythm song=0"), "Rhythm opens (%s)" % overlay())
    cmd("rhythm:autoplay")
    cmd("advance:80")
    check("passed=1" in overlay(), "an autoplayed song passes (%s)" % overlay())
    cmd("close")
    yield from wait_until(lambda: not LIB.is_arcade_overlay_open(), 5, "the overlay closes")
    yield 0.3
    check(meta("rhythmBest") > 0 and "Song cleared" in status(), "the score is recorded (%d; %s)" % (meta("rhythmBest"), status()))
    act("back")

    # ---- the 2026-10-07 mini-games: Halo Fall, Neon Memory, Firewall Breach
    # The dev profile's 5 daily Arcade tokens are spent by now: top up through the tester panel (adds 1,000 by default).
    act("tab:2")
    act("go:tester-cur")
    act("tst:cur:8")
    check(meta("arcadeTokens") >= 10, "tester: Arcade tokens topped up for the rest of the run (%d)" % meta("arcadeTokens"))
    act("tab:2")
    act("go:arcade")
    ids = [str(hub().get_grid_id(i)) for i in range(hub().get_grid_count())]
    check(all(x in ids for x in ("arc:halo", "arc:memory", "arc:breach", "arc:rally")), "the arcade lists the four mini-games (%s)" % ids)
    games = (
        ("halo", "Halo Fall", "halo:win", 0),
        ("memory", "Neon Memory", "memory:solve", 1),
        ("breach", "Firewall Breach", "breach:autoplay", 2),
        ("rally", "Ark Rally", "rally:autoplay", 3),
    )
    for key, name, command, index in games:
        tokens = meta("arcadeTokens")
        act("arc:" + key)
        check(name in title(), "the %s page opens (%s)" % (name, title()))
        act("arc:%sgo:1" % key)
        yield from wait_until(lambda: LIB.is_arcade_overlay_open(), 5, "the %s overlay" % name)
        check(overlay().startswith(key + " Medium") and meta("arcadeTokens") == tokens - 1, "%s opens on Medium for one token (%s)" % (name, overlay()))
        if key == "halo":
            cmd("key:SpaceBar")
            cmd("key:D")
            cmd("advance:0.5")
            cmd("keyup:D")
            cmd("keyup:SpaceBar")
            check("x=0.5" not in overlay(), "the move key moves the turret (%s)" % overlay())
        if key == "rally":
            cmd("key:Up")
            cmd("advance:1.5")
            cmd("keyup:Up")
            check(float(overlay().split("x=")[1].split("/")[0]) > 6.0, "the gas key drives the bike (%s)" % overlay())
        if key == "memory":
            cmd("advance:3")
            check("preview=0" in overlay(), "the preview ends (%s)" % overlay())
        cmd(command)
        cmd({"breach": "advance:40", "rally": "advance:70"}.get(key, "advance:2"))
        check("won=1" in overlay(), "%s is cleared (%s)" % (name, overlay()))
        yield from wait_until(lambda: "result=" in overlay() and not overlay().endswith("result="), 10, "the %s result" % name)
        cmd("close")
        yield from wait_until(lambda: not LIB.is_arcade_overlay_open(), 5, "the overlay closes")
        yield 0.3
        check(meta("miniBest%d" % index) > 0 and meta("miniWins%d" % index) >= 1 and "cleared" in status(),
              "%s records the score and pays (best %d; %s)" % (name, meta("miniBest%d" % index), status()))
        act("back")

    # ---- Ark Rally: write a track, ride it for free (no reward)
    act("arc:rally")
    act("arc:rallyedit")
    yield 0.3
    check(overlay() == "prompt", "editing the track opens the text prompt (%s)" % overlay())
    cmd("prompt:text:F10 C3 H16,2 K F12")
    yield 0.5
    check("Track saved" in status() and "3 coins" in status(), "the typed track is saved (%s)" % status())
    act("arc:rallycar")
    check("Buggy" in status(), "the vehicle tile picks the buggy (%s)" % status())
    tokens = meta("arcadeTokens")
    act("arc:rallytry")
    yield from wait_until(lambda: LIB.is_arcade_overlay_open(), 5, "the custom track overlay")
    check(overlay().startswith("rally Custom") and "vehicle=buggy" in overlay() and meta("arcadeTokens") == tokens,
          "your own track opens for free, on the buggy (%s)" % overlay())
    cmd("rally:autoplay")
    cmd("advance:20")
    check("won=1" in overlay() and "coins=3" in overlay(), "the custom track is finished with its coins (%s)" % overlay())
    yield from wait_until(lambda: "result=" in overlay() and not overlay().endswith("result="), 10, "the custom track result")
    cmd("close")
    yield from wait_until(lambda: not LIB.is_arcade_overlay_open(), 5, "the overlay closes")
    yield 0.3
    check("no reward" in status(), "a custom track pays nothing (%s)" % status())
    act("arc:rallycar")
    check("Dirt bike" in status(), "back to the dirt bike (%s)" % status())
    # ---- Ultimate Custom Night (2026-10-14): locked before Chapter 13, the one-time 3,000 Diamonds, a loss
    cleared = meta("cleared")
    if meta("ucnUnlocked") == 0:
        act("tst:camp:none")
        act("tab:2")
        act("go:arcade")
        act("arc:ucn")
        check("Arcade" in title() and "Chapter 13" in status(), "Ultimate Custom Night is locked before Chapter 13 (%s; %s)" % (title(), status()))
    else:
        log("note: this dev save already latched the Ultimate Custom Night unlock; the locked check is covered by Tests/ucn_tests.cpp")
    act("tst:camp:all")
    act("tab:2")
    act("go:arcade")
    ids = [str(hub().get_grid_id(i)) for i in range(hub().get_grid_count())]
    check("arc:ucn" in ids, "the arcade lists Ultimate Custom Night (%s)" % ids)
    act("arc:ucn")
    check("Ultimate Custom Night" in title(), "Chapter 13 cleared: the mode page opens (%s)" % title())
    claimed_before = meta("ucnRewardClaimed")
    diamonds = meta("diamondsRaw")
    tokens = meta("arcadeTokens")
    act("arc:ucngo:0")
    yield from wait_until(lambda: LIB.is_arcade_overlay_open(), 5, "the Ultimate Custom Night overlay")
    check(overlay().startswith("ucn Easy") and meta("arcadeTokens") == tokens - 1, "Easy opens for one token (%s)" % overlay())
    cmd("key:A")
    yield 0.1
    check("left=closed" in overlay(), "the left door key closes the left door (%s)" % overlay())
    cmd("key:A")
    cmd("key:W")
    yield 0.1
    check("window=closed" in overlay(), "W shuts the front window (%s)" % overlay())
    cmd("key:W")
    cmd("key:SpaceBar")
    cmd("key:Four")
    yield 0.1
    check("left=open" in overlay() and "window=open" in overlay() and "camera=up" in overlay() and "cam=3" in overlay(), "camera up, CAM 4 picked (%s)" % overlay())
    cmd("ucn:autoplay")
    cmd("advance:330")
    check("won=1" in overlay(), "the perfect bot survives Easy in the real overlay (%s)" % overlay())
    yield from wait_until(lambda: "result=" in overlay() and not overlay().endswith("result="), 10, "the night's result")
    cmd("close")
    yield from wait_until(lambda: not LIB.is_arcade_overlay_open(), 5, "the overlay closes")
    yield 0.3
    paid = meta("diamondsRaw") - diamonds
    if claimed_before == 0:
        check(meta("ucnRewardClaimed") == 1 and paid == 3000 and "First win reward" in status(), "the first win pays 3,000 Diamonds once (+%d; %s)" % (paid, status()))
    else:
        check(paid == 0, "this dev save already claimed the reward: nothing paid (+%d)" % paid)
    check(meta("ucnWins0") >= 1, "the win is recorded (%d)" % meta("ucnWins0"))
    # A second win in another mode pays nothing more.
    act("arc:ucngo:1")
    yield from wait_until(lambda: LIB.is_arcade_overlay_open(), 5, "the Medium overlay")
    diamonds = meta("diamondsRaw")
    cmd("ucn:autoplay")
    cmd("advance:330")
    check("won=1" in overlay(), "the perfect bot survives Medium (%s)" % overlay())
    yield from wait_until(lambda: "result=" in overlay() and not overlay().endswith("result="), 10, "the night's result")
    cmd("close")
    yield from wait_until(lambda: not LIB.is_arcade_overlay_open(), 5, "the overlay closes")
    yield 0.3
    check(meta("diamondsRaw") == diamonds and meta("ucnRewardClaimed") == 1, "a second win never pays again (%d -> %d)" % (diamonds, meta("diamondsRaw")))
    # A loss: nobody at the controls.
    act("arc:ucngo:0")
    yield from wait_until(lambda: LIB.is_arcade_overlay_open(), 5, "the overlay (loss)")
    cmd("advance:150")
    check("over=1" in overlay() and "won=0" in overlay(), "an unattended night is lost (%s)" % overlay())
    yield from wait_until(lambda: "result=" in overlay() and not overlay().endswith("result="), 10, "the loss result")
    cmd("close")
    yield from wait_until(lambda: not LIB.is_arcade_overlay_open(), 5, "the overlay closes")
    yield 0.3
    check("did not survive" in status(), "the loss is reported (%s)" % status())
    act("tst:camp:%d" % cleared)
    act("back")
    act("back")

    # ---- prompt: rename the commander
    act("go:settings")
    act("set:tab:4")
    act("pref:name")
    yield 0.3
    check(overlay() == "prompt", "renaming opens the text prompt (%s)" % overlay())
    cmd("prompt:text:Captain Verify")
    yield 0.5
    check(not LIB.is_arcade_overlay_open() and "Captain Verify" in status(), "the typed name is applied (%s)" % status())

    # ---- key capture: rebind Jump
    act("set:tab:2")
    act("go:keys")
    act("keys:bind:4")
    yield 0.3
    check(overlay() == "prompt", "binding opens key capture (%s)" % overlay())
    cmd("prompt:key:LeftControl")
    yield 0.5
    check("Jump" in status() and "Ctrl" in status(), "Jump is rebound (%s)" % status())
    act("keys:reset")
    act("back")
    act("back")

    # ---- arena: teams, a practice rival (no server in -BIBDev), the replay
    act("tab:4")
    act("go:ark")
    act("go:arena")
    check("Arena" in title(), "arena page (%s)" % title())
    act("arena:edit:0")
    act("arena:auto")
    act("back")
    act("arena:find")
    check("rival" in status().lower(), "a practice rival is found (%s)" % status())
    act("arena:fight")
    yield from wait_until(lambda: LIB.is_arcade_overlay_open(), 5, "the arena replay")
    check(overlay().startswith("arena"), "the fight is replayed (%s)" % overlay())
    cmd("key:SpaceBar")
    yield 0.2
    check("over=1" in overlay(), "Space skips to the result (%s)" % overlay())
    cmd("close")
    yield from wait_until(lambda: not LIB.is_arcade_overlay_open(), 5, "the overlay closes")
    check(meta("arenaFights") == 1, "one arena fight recorded (%d, trophies %d)" % (meta("arenaFights"), meta("trophies")))
    act("back")

    # ---- endless tower: a fall is saved
    act("go:tower")
    act("tower:start:0")
    yield from wait_until(lambda: "Ark Tower" in str(hud().get_editor_property("stage_title")), 30, "tower floor 1")
    LIB.abandon_stage(controller())
    yield 1.0
    check(meta("towerLost0") == 1, "the tower saves the floor you fell on (%d)" % meta("towerLost0"))
    yield from back_to_hub()

    # ---- Boss Rush in real combat
    act("tab:2")
    act("go:arcade")
    act("arc:boss")
    yield from wait_until(lambda: "Boss Rush" in str(hud().get_editor_property("stage_title")), 30, "Boss Rush round 1")
    yield 1.0
    check(meta("bossRound") == 1, "Boss Rush starts on round 1 (%d)" % meta("bossRound"))
    pc = controller()
    for _ in range(30):
        pc.bib_damage_enemies(1.0e9)
        yield 0.2
        if meta("bossBest") >= 1:
            break
    check(meta("bossBest") == 1, "clearing round 1 pays and records it (best %d)" % meta("bossBest"))
    check(meta("popups") > 0, "damage numbers were recorded for the hits (%d)" % meta("popups"))
    yield from wait_until(lambda: meta("bossRound") == 2, 10, "round 2")
    check(meta("bossRound") == 2, "the next boss arrives in the same arena (round %d)" % meta("bossRound"))
    LIB.abandon_stage(pc)
    yield 1.0
    lines = [str(x) for x in pc.get_editor_property("result_lines")]
    check(any("Boss Rush over" in x for x in lines), "the run summary (%s)" % lines)
    yield from back_to_hub()


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
        with open(os.path.join(SAVED, "VerifyArcade.log"), "w") as handle:
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
