"""Checks the anime post-process volume (BIBGame/BIBAnimeStyle.h, material from build_assets.py).

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <abs path>/Tools/verify_anime_style.py"

1. Assets: M_BIBAnimeStyle (post process) and MI_BIBAnimeStyle exist.
2. Maps: exactly one BIBAnimeStyleVolume in every gameplay map (Chapter 1 stages, L_NeoTokyo, L_Ark), none in L_Hub.
3. PIE boots into the Ark (the hub since 2026-10-07: the 2D lobby was deleted): the Ark is stylised and stepped while walking
   and behind the flow-chart menu (the menu itself is smooth Slate on top); a code-built stage (Chapter 2-1, built in L_Hub)
   turns the style on and presents about 12 fresh frames a second while the game keeps rendering every frame; the stage map
   and Neo Tokyo do the same.
4. Held frames are not rendered (2026-10-08 optimisation, bib.Gfx.SkipHeldFrames): most held frames skip the 3D scene and
   the viewport still shows the kept image (a plain viewport `shot` taken between steps is a full picture, not black).
Screenshots: Saved/AnimeStyle/<case>.png. Writes Saved/VerifyAnimeStyle.log (ends with RESULT PASS or RESULT FAIL).
Environment: BIB_VERIFY_KEEP_OPEN=1 keeps the editor open.
"""

import os
import time
import traceback

import unreal

import sys as _sys

_sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bib_hub  # noqa: E402  (the 2D lobby was deleted on 2026-10-07: pages are driven in the Ark)

LOG, FAILS = [], []
SAVED = unreal.Paths.project_saved_dir()
OUT = os.path.join(SAVED, "AnimeStyle")
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

HUB = "/Game/BIB/Maps/L_Hub"
GAMEPLAY_MAPS = ["/Game/BIB/Maps/L_Chapter1_Stage%d" % i for i in range(1, 9)] + ["/Game/Greybox/Maps/L_NeoTokyo", "/Game/Greybox/Maps/L_Ark"]
STEP_RATE = 12.0


def log(text):
    LOG.append(text)
    unreal.log("[verify_anime_style] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return UES.get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def volumes(w):
    return list(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.BIBAnimeStyleVolume)) if w else []


def hub():
    return bib_hub.hub()


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


class Frames:
    """Counts editor ticks (one rendered game frame each) next to the volume's fresh / held counters."""
    count = 0


def measure(name, seconds=4.0, expect_style=True):
    unreal.BIBAnimeStyleVolume.reset_frame_stats()
    start_frames, start = Frames.count, time.monotonic()
    yield seconds
    elapsed = time.monotonic() - start
    frames = Frames.count - start_frames
    fresh, held = unreal.BIBAnimeStyleVolume.get_frame_stats()
    log("%s: %.1f s, %d game frames (%.0f fps), %d fresh (%.1f/s), %d held" % (name, elapsed, frames, frames / elapsed, fresh, fresh / elapsed, held))
    if not expect_style:
        check(fresh == 0 and held == 0, "%s: no held frames on the menus" % name)
        return
    fps = frames / elapsed
    want = min(STEP_RATE, fps)
    check(abs(fresh / elapsed - want) <= max(2.0, want * 0.2), "%s: about %.0f fresh frames a second (%.1f)" % (name, want, fresh / elapsed))
    if fps > STEP_RATE * 1.5:
        check(held > fresh, "%s: frames are held between steps (%d held, %d fresh)" % (name, held, fresh))
        skipped = unreal.BIBAnimeStyleVolume.get_skipped_frames()
        check(skipped >= held * 0.8, "%s: held frames are not rendered (%d of %d skipped)" % (name, skipped, held))


def viewport_shot(name):
    """`shot` reads the game viewport itself after a frame (held frames included), unlike HighResShot which renders anew."""
    folder = os.path.join(SAVED, "Screenshots")
    def pngs():
        found = []
        for root, _dirs, files in os.walk(folder):
            found += [os.path.join(root, f) for f in files if f.lower().endswith(".png")]
        return set(found)
    before = pngs()
    unreal.SystemLibrary.execute_console_command(world(), "shot")
    yield 2.0
    new = sorted(pngs() - before, key=os.path.getmtime)
    if not new:
        check(False, "%s: viewport shot written" % name)
        return
    import shutil
    target = os.path.join(OUT, name + "_viewport.png")
    shutil.copyfile(new[-1], target)
    size = os.path.getsize(target)
    check(size > 30000, "%s: the viewport shows a picture while frames are skipped (%d bytes, %s)" % (name, size, target))


def press(name, frames=1):
    subsystem = [x for x in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not x.get_name().startswith("Default__")][-1]
    action = unreal.load_asset("/Game/BIB/Input/IA_" + name)
    for _ in range(frames):
        subsystem.inject_input_vector_for_action(action, unreal.Vector(1.0, 0.0, 0.0), [], [])
        yield 0
    yield 0


def combat():
    """Walk into the first section and fight, for a screenshot with enemies, hits and damage numbers."""
    w = world()
    sections = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor) if a.get_class().get_name().startswith("BIBSection")]
    npcs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor) if a.get_class().get_name().startswith("BIBStoryNPC")]
    pawn = controller().get_controlled_pawn()
    if npcs and pawn:
        loc = npcs[0].get_actor_location()
        pawn.set_actor_location(unreal.Vector(loc.x - 300, loc.y, 150), False, True)
        yield 0.5
        for _ in range(8):
            yield from press("Talk")
            yield 0.25
    if not sections or not pawn:
        check(False, "combat: stage has sections and a player")
        return
    first = min(sections, key=lambda a: a.get_editor_property("section_index"))
    target = first.get_actor_location()
    pawn.set_actor_location(unreal.Vector(target.x - 600, target.y, 150), False, True)
    yield 3.0
    controller().set_control_rotation(unreal.Rotator(pitch=-15.0, yaw=0.0, roll=0.0))
    for _ in range(12):
        yield from press("Attack")
        yield 0.15


def screenshot(name):
    path = os.path.join(OUT, name + ".png")
    unreal.SystemLibrary.execute_console_command(world(), "HighResShot 1600x900 filename=%s" % path)
    log("screenshot " + path)
    yield 1.0


def assets():
    material = unreal.load_asset("/Game/BIB/Materials/M_BIBAnimeStyle")
    check(material is not None, "M_BIBAnimeStyle exists")
    if material:
        check(material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_POST_PROCESS, "M_BIBAnimeStyle is a post-process material")
        check(material.get_editor_property("blendable_location") == unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING, "M_BIBAnimeStyle runs after tonemapping")
    check(unreal.load_asset("/Game/BIB/Materials/MI_BIBAnimeStyle") is not None, "MI_BIBAnimeStyle exists")


def map_volumes():
    for path in [HUB] + GAMEPLAY_MAPS:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            check(False, "map exists: " + path)
            continue
        check(LES.load_level(path), "load " + path)
        yield 2.0
        found = [a for a in EAS.get_all_level_actors() if isinstance(a, unreal.BIBAnimeStyleVolume)]
        want = 0 if path == HUB else 1
        check(len(found) == want, "%s: %d anime style volume(s) (want %d)" % (path.rsplit("/", 1)[1], len(found), want))
        for v in found:
            check(v.get_editor_property("unbound") and v.is_style_enabled(), "%s: volume is unbound and enabled" % path.rsplit("/", 1)[1])


def pie(path):
    LES.load_level(path)
    yield 2.0
    LES.editor_request_begin_play()
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE in " + path)
    yield 6.0   # shaders and Lumen settle


def end_pie():
    LES.editor_request_end_play()
    yield 3.0


def flow():
    os.makedirs(OUT, exist_ok=True)
    assets()
    yield from map_volumes()

    # The Ark (the hub): stylised while walking and behind the menu; then a code-built stage (Chapter 2-1, stage id 9).
    yield from pie(HUB)
    yield from wait_until(lambda: hub() is not None, 90, "the Ark")
    yield 4.0
    on = [v for v in volumes(world()) if v.is_style_enabled()]
    check(len(on) == 1, "the Ark: one enabled anime volume (%d)" % len(on))
    yield from measure("the Ark (walking)")
    yield from screenshot("ark_plaza")
    yield from viewport_shot("ark_plaza")
    unreal.BIBArkLibrary.open_menu(controller())
    yield 0.6
    yield from measure("the Ark behind the menu")
    yield from screenshot("ark_menu")
    unreal.BIBArkLibrary.close_screens(controller())
    yield 0.3
    if hub():
        controller().bib_set_campaign_progress(8)
        for action in ("tab:5", "camp:ch:2", "camp:start:9"):
            hub().click_action(action)
            yield 0.3
        started = yield from wait_until(lambda: "Chapter 2 - Level 1" in str(unreal.BIBBlueprintLibrary.get_hud_info(controller()).get_editor_property("stage_title")), 60, "Chapter 2-1 to start")
        if started:
            yield 3.0
            on = [v for v in volumes(world()) if v.is_style_enabled()]
            check(len(on) == 1, "code-built stage: the game mode enabled one anime volume (%d)" % len(on))
            yield from measure("Chapter 2-1 (code-built, L_Hub)")
            yield from screenshot("chapter2_level1")
            yield from combat()
            yield from measure("Chapter 2-1 in combat", 3.0)
            yield from screenshot("chapter2_level1_combat")
            unreal.SystemLibrary.execute_console_command(world(), "bib.VisualStepRate 0")
            yield from measure("Chapter 2-1 with bib.VisualStepRate 0", 2.0, expect_style=False)
            unreal.SystemLibrary.execute_console_command(world(), "bib.VisualStepRate -1")
            yield 0.5
    yield from end_pie()

    for path, name in (("/Game/BIB/Maps/L_Chapter1_Stage1", "chapter1_stage1"), ("/Game/Greybox/Maps/L_NeoTokyo", "neotokyo")):
        yield from pie(path)
        on = [v for v in volumes(world()) if v.is_style_enabled()]
        check(len(on) == 1, "%s: anime volume enabled in PIE (%d)" % (name, len(on)))
        yield from measure(name)
        yield from screenshot(name)
        yield from viewport_shot(name)
        yield from end_pie()


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
        with open(os.path.join(SAVED, "VerifyAnimeStyle.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        if world():
            LES.editor_request_end_play()
        if os.environ.get("BIB_VERIFY_KEEP_OPEN") != "1":
            unreal.SystemLibrary.quit_editor()

    def tick(self, delta):
        Frames.count += 1
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
