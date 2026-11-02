"""Plays the account flow in PIE against a local development account server: the sign-in screen, sign-up by e-mailed code,
the new-account tutorial (Luna only, empty squad, forced Kuu summon, level 1, Plack for free), the save file, sign-out,
log-in again with the progress restored, the 3-day banner countdown, and the owner/admin account.

Start the server first (development mode: nothing is stored, e-mails are printed to its log):

    cd Demo/BoldInBeautyDemo/Server && npm install && ALLOW_MEMORY_STORE=1 MAIL_PROVIDER=console PORT=18090 node src/index.js > /tmp/bib-server.log

then run the editor WITHOUT -BIBDev / -BIBGuest (the sign-in screen is what is tested):

    BIB_SERVER_LOG=/tmp/bib-server.log UnrealEditor <BoldInBeautyDemo.uproject> -BIBApi=http://127.0.0.1:18090 -ExecCmds="py <this file>"

Delete Saved/BIB first (a remembered session or save from an earlier run would skip the sign-in screen), and wait for the server's
45 s resend cool-down if you repeat a run quickly (the script already waits where it needs to). Set BIB_SCREENSHOTS=1 (plus
-EnablePlugins=SlateInspectorToolset) for two screenshots in Saved/Screenshots.

Writes Saved/VerifyAccounts.log (last line RESULT PASS|FAIL). It drives the page model in the Ark (bib_hub.py; the 2D lobby WBP_Hub was deleted on 2026-10-07), like
verify_hub.py, and the sign-in flow through the Python-callable functions of UBIBAccountSubsystem.
"""

import json
import os
import re
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
SERVER_LOG = os.environ.get("BIB_SERVER_LOG", "")
STAMP = str(int(time.time()))
SHOTS = os.environ.get("BIB_SCREENSHOTS") == "1"
ADMIN_EMAIL = "admin@example.com"


def log(text):
    LOG.append(text)
    unreal.log("[verify_accounts] " + text)


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


def map_name():
    return unreal.GameplayStatics.get_current_level_name(world(), True)


def screen():
    return unreal.BIBBlueprintLibrary.get_current_screen(controller())


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


def actors(class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor) if a.get_class().get_name() == class_name]


def _find_image_bytes(value):
    """Base64 PNG/JPEG data anywhere in a toolset JSON result."""
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
    """With BIB_SCREENSHOTS=1 (and -EnablePlugins=SlateInspectorToolset): the editor window to Saved/Screenshots/accounts_<name>."""
    if not SHOTS:
        return
    yield 0.6
    try:
        result = unreal.ToolsetRegistry.execute_tool("SlateInspectorToolset.SlateInspectorToolset", "Screenshot", json.dumps({"Ref": ""}))
        data = None if result.error else _find_image_bytes(json.loads(result.value))
        if not data:
            log("screenshot %s unavailable: %s" % (name, (result.error or result.value)[:200]))
            return
        path = os.path.join(SAVED, "Screenshots", "accounts_" + name + (".png" if data[:4] == b"\x89PNG" else ".jpg"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as handle:
            handle.write(data)
        log("screenshot: %s (%d bytes)" % (path, len(data)))
    except Exception as error:
        log("screenshot %s failed: %s" % (name, error))


def alive_enemies():
    return [e for e in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.load_class(None, "/Script/BoldInBeautyDemo.BIBEnemy")) if e.is_alive()]


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


def accounts():
    # unreal.SubsystemBlueprintLibrary is not exposed to Python in UE 5.8: find the live instance through the object iterator.
    current = world()
    if current is None:
        return None
    game_instance = unreal.GameplayStatics.get_game_instance(current)
    for subsystem in unreal.ObjectIterator(unreal.BIBAccountSubsystem):
        if not subsystem.get_name().startswith("Default__") and subsystem.get_outer() == game_instance:
            return subsystem
    return None


def stage():
    return str(accounts().get_stage_name())


def owned():
    return sorted(str(r.get_editor_property("name")) for r in unreal.BIBBlueprintLibrary.get_roster(controller()) if r.get_editor_property("owned"))


def level_of(name):
    for r in unreal.BIBBlueprintLibrary.get_roster(controller()):
        if str(r.get_editor_property("name")) == name:
            return r.get_editor_property("level")
    return 0


def gold():
    return unreal.BIBBlueprintLibrary.get_currency(controller(), unreal.BIBCurrency.GOLD)


def code_for(email):
    """The newest 6-digit code the development server logged for this address (its console mailer prints every e-mail)."""
    if not SERVER_LOG or not os.path.exists(SERVER_LOG):
        return None
    with open(SERVER_LOG, encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    found = re.findall(r'"to":"%s".*?verification code is (\d{6})' % re.escape(email), text)
    return found[-1] if found else None


def save_file(user_id):
    return os.path.join(SAVED, "BIB", "Saves", user_id + ".json")


def read_json(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


CODE_REQUESTED_AT = {}
RESEND_COOLDOWN_SECONDS = 45.0  # Server/src/config.js resendCooldownSeconds: a second code for the same e-mail is refused sooner


def sign_up(username, email):
    acct = accounts()
    acct.begin_signup(username, email)
    CODE_REQUESTED_AT[email] = time.monotonic()
    yield from wait_until(lambda: stage() == "AwaitingCode", 20, "the code step after sign-up (%s)" % acct.get_message_text())
    yield from wait_until(lambda: code_for(email) is not None, 10, "the e-mailed code in the server log")
    acct.enter_code(code_for(email))
    yield from wait_until(lambda: stage() == "SignedIn" and hub() is not None, 30, "signed in and the hub on screen (%s)" % acct.get_message_text())
    yield 0.5


def log_in(email):
    acct = accounts()
    waited = RESEND_COOLDOWN_SECONDS + 3.0 - (time.monotonic() - CODE_REQUESTED_AT.get(email, 0.0))
    if waited > 0.0 and email in CODE_REQUESTED_AT:
        log("waiting %.0f s for the server's resend cooldown before logging in again" % waited)
        yield waited
    before = code_for(email)
    acct.begin_login(email)
    CODE_REQUESTED_AT[email] = time.monotonic()
    yield 0.5
    log("log-in requested for %s: stage %s, message '%s'" % (email, stage(), acct.get_message_text()))
    yield from wait_until(lambda: stage() == "AwaitingCode", 20, "the code step after log-in")
    if stage() != "AwaitingCode":
        log("log-in stuck: stage %s, message '%s'" % (stage(), acct.get_message_text()))
    yield from wait_until(lambda: code_for(email) is not None and code_for(email) != before or (before is None and code_for(email) is not None), 10, "a new e-mailed code")
    acct.enter_code(code_for(email))
    yield from wait_until(lambda: stage() == "SignedIn" and hub() is not None, 30, "logged in and the hub on screen")
    yield 0.5


def sign_out():
    act("tab:2")
    act("go:settings")
    act("acct:signout")
    yield from wait_until(lambda: screen() == unreal.BIBScreen.SIGN_IN and stage() == "SignedOut", 20, "the sign-in screen after signing out")


def play_level_one():
    """Level 1 of Chapter 1, the way verify_hub.py plays level 8: talk, then clear every section with BIBDamageEnemies."""
    pc = controller()
    act("camp:start:1")
    yield from wait_until(lambda: map_name().startswith("L_Chapter1_Stage1") and character() is not None, 60, "travel to level 1")
    yield 2.0
    pc = controller()  # the map travel replaced the player controller
    npc = actors("BP_BIBStoryNPC_C")
    if npc:
        loc = npc[0].get_actor_location()
        teleport(loc.x - 300, loc.y)
        yield 0.5
        for _ in range(6):
            yield from press("Talk")
            yield 0.3
    sections = sorted(actors("BP_BIBSection_C"), key=lambda a: a.get_editor_property("section_index"))
    check(len(sections) >= 2, "level 1 has sections (%d)" % len(sections))
    for section in sections:
        index = section.get_editor_property("section_index")
        door = section.get_editor_property("entry_door")
        yield from wait_until(lambda: door is None or door.is_open(), 15, "level 1 door %d" % (index + 1))
        target = section.get_actor_location()
        teleport(target.x, target.y)
        yield from wait_until(lambda: len(alive_enemies()) > 0, 10, "level 1 section %d spawn" % (index + 1))
        yield 0.5
        for _ in range(10):
            if not alive_enemies():
                break
            pc.bib_damage_enemies(10000000.0)
            yield 0.5
    yield from wait_until(lambda: screen() == unreal.BIBScreen.RESULTS, 20, "level 1 results")
    lines = [str(l) for l in pc.get_editor_property("result_lines")]
    check(pc.get_editor_property("result_success"), "level 1 cleared (%s)" % lines)
    check(any("TUTORIAL COMPLETE" in l and "Plack" in l for l in lines), "the results announce the tutorial reward (%s)" % lines)
    results = widgets("WBP_Results")
    if results:
        results[0].get_editor_property("Continue").on_clicked.broadcast()
    else:
        unreal.BIBBlueprintLibrary.return_to_hub(pc)
    yield from wait_until(lambda: bib_hub.in_ark(), 60, "back in the Ark after level 1")
    yield 1.0


def flow():
    yield from wait_until(lambda: world() is not None and controller() is not None and accounts() is not None, 60, "PIE")
    yield 2.0
    if not SERVER_LOG:
        check(False, "BIB_SERVER_LOG is set (see the top of this file)")
        return

    # ---- the sign-in screen comes first
    check(screen() == unreal.BIBScreen.SIGN_IN, "the sign-in screen is shown first (%s)" % screen())
    check(stage() == "SignedOut", "not signed in at launch (%s)" % stage())
    yield from shot("signin")
    accounts().begin_login("not-an-email")
    check(stage() == "SignedOut" and "e-mail" in str(accounts().get_message_text()), "an invalid e-mail is refused on the client (%s)" % accounts().get_message_text())
    accounts().begin_signup("", "x@example.com")
    check("username" in str(accounts().get_message_text()).lower(), "an empty username is refused on the client")

    # ---- create an account
    email = "tester%s@example.com" % STAMP
    username = "Tester%s" % STAMP[-5:]
    yield from sign_up(username, email)
    yield from shot("tutorial_step1")
    acct = accounts()
    check(str(acct.get_username()) == username and str(acct.get_email()) == email and len(str(acct.get_user_id())) >= 8, "signed in: username, e-mail and user id (%s / %s / %s)" % (acct.get_username(), acct.get_email(), acct.get_user_id()))
    check(not acct.is_admin(), "an ordinary account is not an admin")
    user_id = str(acct.get_user_id())
    check(owned() == ["Luna"], "a new account owns only Luna (%s)" % owned())
    check(list(unreal.BIBBlueprintLibrary.get_team(controller())) == [-1, -1, -1], "the squad is empty (%s)" % list(unreal.BIBBlueprintLibrary.get_team(controller())))
    check(os.path.exists(save_file(user_id)), "a save file was written for the new account (%s)" % save_file(user_id))

    # ---- the tutorial: everything but the next step is refused
    act("go:profile")
    check("tutorial" in status().lower(), "step 1 refuses other actions (%s)" % status())
    act("tab:6")
    check(int(hub().get_current_tab()) != 6, "step 1 keeps the Summon tab locked")
    act("tab:0")
    act("op:sel:5")  # Luna
    gold_before = gold()
    act("op:lvl")
    # the tutorial's first level-up is free (a new account owns only 300 Diamonds, owner rule 2026-10-06)
    check(level_of("Luna") == 2 and gold() <= gold_before, "step 3: Luna levelled up, the tutorial's first level-up is free (level %d)" % level_of("Luna"))
    act("tab:1")
    act("team:cycle:2")
    check(list(unreal.BIBBlueprintLibrary.get_team(controller()))[0] == 5, "step 5: tapping an empty slot added Luna and she leads (%s)" % list(unreal.BIBBlueprintLibrary.get_team(controller())))
    act("tab:6")
    act("sum:go:1:0")
    check("tutorial" in status().lower(), "step 7: only the tutorial summon is allowed (%s)" % status())
    act("sum:tut")
    yield 0.3
    check("Kuu" in owned() and len(owned()) == 2, "the forced summon gave Kuu (%s)" % owned())
    check(list(unreal.BIBBlueprintLibrary.get_team(controller()))[:2] == [5, 7], "Kuu joined the squad behind Luna (%s)" % list(unreal.BIBBlueprintLibrary.get_team(controller())))
    act("tab:5")
    act("camp:ch:1")
    yield from play_level_one()
    check(owned() == ["Kuu", "Luna", "Plack"], "level 1 unlocked Plack for free (%s)" % owned())

    # ---- everything is open now; the banner countdown is shown
    act("tab:6")
    act("sum:banner:1")
    check(int(hub().get_current_tab()) == 6, "the Summon tab opens after the tutorial")
    banners = unreal.BIBBlueprintLibrary.get_banners(controller())
    check(len(banners) == 4, "four banners (%d)" % len(banners))

    # ---- the save: on disk, with the tutorial finished
    time.sleep(0.2)
    saved = read_json(save_file(user_id))
    check(saved["userId"] == user_id and saved["profile"]["tutorial"]["step"] == "complete", "the save file holds this account's finished tutorial")
    check(set(saved["profile"]["operators"].keys()) == {"Luna", "Kuu", "Plack"}, "the save file holds exactly Luna, Kuu and Plack")
    check("admin" not in json.dumps(saved).lower(), "the save file carries no admin flag")

    # ---- sign out, sign in again: the progress is restored
    yield from sign_out()
    check(owned() == ["Luna"], "signed out: the profile behind the screen is blank (%s)" % owned())
    yield 1.0
    yield from log_in(email)
    check(owned() == ["Kuu", "Luna", "Plack"], "logged in again: the operators are back (%s)" % owned())
    check(level_of("Luna") == 2, "logged in again: Luna is still level 2")
    check(str(accounts().get_user_id()) == user_id, "logged in again as the same user id")

    # ---- a wrong code is refused and a second account cannot take the e-mail
    yield from sign_out()
    yield 1.0
    accounts().begin_signup("Another%s" % STAMP[-4:], email)
    yield from wait_until(lambda: stage() == "SignedOut" and "already exists" in str(accounts().get_message_text()), 15, "'account exists' for a taken e-mail (%s)" % accounts().get_message_text())

    # ---- the owner / admin account
    admin_email = ADMIN_EMAIL
    accounts().begin_signup("Owner", admin_email)
    yield from wait_until(lambda: stage() == "AwaitingCode", 20, "the code step for the owner e-mail")
    accounts().enter_code("000000")
    yield from wait_until(lambda: stage() != "Busy" and ("Wrong" in str(accounts().get_message_text()) or stage() != "AwaitingCode"), 15, "a wrong code is refused")
    check(stage() == "AwaitingCode" and "Wrong" in str(accounts().get_message_text()), "a wrong code keeps the code step open (%s)" % accounts().get_message_text())
    yield from wait_until(lambda: code_for(admin_email) is not None, 10, "the owner's e-mailed code")
    accounts().enter_code(code_for(admin_email))
    yield from wait_until(lambda: stage() == "SignedIn" and hub() is not None, 30, "the owner signed in")
    yield 0.5
    check(accounts().is_admin(), "the owner e-mail is flagged admin by the server")
    check(len(owned()) == 20, "the admin owns all 20 operators (%d)" % len(owned()))
    check(gold() == 999999999, "the admin has unlimited Gold (%d)" % gold())
    act("tab:2")
    act("go:profile")
    check("Profile" in str(hub().get_page_title()), "the admin is not held by the tutorial")
    act("tab:5")
    act("camp:ch:1")
    act("camp:start:8")
    yield from wait_until(lambda: map_name().startswith("L_Chapter1_Stage8"), 60, "the admin can start level 8 straight away")
    unreal.BIBBlueprintLibrary.abandon_stage(controller())
    yield 1.0
    results = widgets("WBP_Results")
    if results:
        results[0].get_editor_property("Continue").on_clicked.broadcast()
    yield from wait_until(lambda: bib_hub.in_ark(), 60, "back in the Ark")
    # the admin has every chapter open: the last level of Chapter 13 starts at once (it is built in code in L_Hub, then the game travels back to the Ark)
    act("tab:5")
    act("camp:ch:13")
    ids = [str(hub().get_grid_id(i)) for i in range(hub().get_grid_count())]
    check(str(hub().get_page_title()) == "Chapter 13" and "camp:start:104" in ids, "the admin can open Chapter 13 (%s)" % hub().get_page_title())
    act("camp:start:104")
    yield from wait_until(lambda: "Chapter 13 - Level 8" in str(unreal.BIBBlueprintLibrary.get_hud_info(controller()).get_editor_property("stage_title")), 60, "the admin can start Chapter 13 level 8 straight away")
    unreal.BIBBlueprintLibrary.abandon_stage(controller())
    yield 1.0
    results = widgets("WBP_Results")
    if results:
        results[0].get_editor_property("Continue").on_clicked.broadcast()
    yield from wait_until(lambda: bib_hub.in_ark(), 60, "back in the Ark after Chapter 13")
    yield from sign_out()


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
        with open(os.path.join(SAVED, "VerifyAccounts.log"), "w") as handle:
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
