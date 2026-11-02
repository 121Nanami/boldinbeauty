"""Frame-time capture in PIE (owner request 2026-10-08: "it runs at 3 fps", optimise and add graphics options).

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBGuest -BIBFreeRoam -ExecCmds="py <abs path>/Tools/perf_capture.py"

Walks a list of scenes in Play-in-Editor (the way the game is played from the editor) and records, per scene, the frame time
the editor sees (Slate ticks) plus a CSV profile (game / render thread and GPU time, `csvprofile start|stop`):
  ark          - the Ark (the hub) after the guest sign-in
  neotokyo     - L_NeoTokyo free roam (Campaign > Neo Tokyo) at night in a thunderstorm with heavy wind
  stage1       - Chapter 1 Level 1 (its own map, L_Chapter1_Stage1) with its enemies
  stage2       - Chapter 2 Level 1 (code-built in L_Hub) with its enemies
  ch<NN>       - a chapter greybox map in free roam (needs -BIBFreeRoam), e.g. BIB_PERF_SCENES="ark,neotokyo,ch08,ch12"
Environment: BIB_PERF_SCENES (default "ark,neotokyo,ch08"), BIB_PERF_SECONDS (default 12), BIB_PERF_CMDS (console commands
run once PIE is up, ";"-separated, e.g. "r.ScreenPercentage 50"), BIB_PERF_ACTIONS (Settings page actions clicked in the
Ark first, ","-separated, e.g. "set:perf" or "set:weather,set:volfog"; they are saved to the profile, so back the save up),
BIB_PERF_TAG (name in the log), BIB_VERIFY_KEEP_OPEN=1.
Writes Saved/Perf/<tag>.log: one line per scene with the median / 90th percentile frame time and the CSV stats.
"""

import csv
import glob
import os
import statistics
import sys
import time
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bib_hub  # noqa: E402

LOG = []
SAVED = unreal.Paths.project_saved_dir()
OUT = os.path.join(SAVED, "Perf")
CSV_DIR = os.path.join(SAVED, "Profiling", "CSV")
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
HUB = "/Game/BIB/Maps/L_Hub"
W = unreal.BIBWeatherSystem
TAG = os.environ.get("BIB_PERF_TAG", "perf")
SECONDS = float(os.environ.get("BIB_PERF_SECONDS", "12"))
SCENES = [s.strip() for s in os.environ.get("BIB_PERF_SCENES", "ark,neotokyo,ch08").split(",") if s.strip()]
CHAPTERS = {
    "ch02": "L_Ch02_Ruins", "ch03": "L_Ch03_Docks", "ch04": "L_Ch04_HangingCity", "ch05": "L_Ch05_ArcTech",
    "ch06": "L_Ch06_MoonKingdom", "ch07": "L_Ch07_TOMBS", "ch08": "L_Ch08_Intersection", "ch09": "L_Ch09_RemnantRemains",
    "ch10": "L_Ch10_Hallucinations", "ch11": "L_Ch11_ReturnToNowhere", "ch12": "L_Ch12_Paradise", "ch13": "L_Ch13_SecretiveSoul",
}
DELTAS = []


def log(text):
    LOG.append(text)
    unreal.log("[perf_capture] " + text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def map_name():
    return str(unreal.GameplayStatics.get_current_level_name(world(), True)) if world() else ""


def console(cmd):
    unreal.SystemLibrary.execute_console_command(world(), cmd)


def wait_until(cond, timeout, what):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.2
    log("timed out waiting for " + what)
    return False


def summarise_csv(path):
    """Median game / render / GPU time of a CSV profile (ms)."""
    csv.field_size_limit(10 ** 9)
    with open(path) as handle:
        rows = list(csv.reader(handle))
    head = rows[0]
    out = {}
    for name in ("FrameTime", "GameThreadTime", "RenderThreadTime", "RHIThreadTime", "GPUTime"):
        if name not in head:
            continue
        index = head.index(name)
        values = []
        for row in rows[1:]:
            if len(row) == len(head):
                try:
                    values.append(float(row[index]))
                except ValueError:
                    pass
        if values:
            out[name] = statistics.median(values)
    return out


def measure(name):
    """Frame times over SECONDS plus a CSV profile of the same window."""
    before = set(glob.glob(os.path.join(CSV_DIR, "*.csv")))
    try:
        unreal.BIBAnimeStyleVolume.reset_frame_stats()
    except Exception:
        pass
    console("csvprofile start")
    DELTAS.clear()
    yield SECONDS
    samples = sorted(DELTAS)
    console("csvprofile stop")
    # The profiler writes its file on a worker thread: wait for the new one and for its size to settle.
    path, size = None, -1
    end = time.monotonic() + 20.0
    while time.monotonic() < end:
        fresh = sorted(set(glob.glob(os.path.join(CSV_DIR, "*.csv"))) - before, key=os.path.getmtime)
        if fresh:
            current = os.path.getsize(fresh[-1])
            if fresh[-1] == path and current == size and current > 0:
                break
            path, size = fresh[-1], current
        yield 0.5
    stats = {}
    if path:
        try:
            stats = summarise_csv(path)
        except Exception as error:
            log("csv %s unreadable (%s)" % (path, error))
    if samples:
        med = statistics.median(samples) * 1000.0
        p90 = samples[int(len(samples) * 0.9)] * 1000.0
        line = "%-10s frames %4d  median %7.2f ms (%5.1f fps)  p90 %7.2f ms" % (name, len(samples), med, 1000.0 / med, p90)
    else:
        line = "%-10s no frames" % name
    if stats:
        line += "  | csv " + "  ".join("%s %.2f" % (k.replace("Time", ""), v) for k, v in stats.items())
    try:
        fresh_frames, held = unreal.BIBAnimeStyleVolume.get_frame_stats()
        line += "  | anime fresh %d held %d skipped %d" % (fresh_frames, held, unreal.BIBAnimeStyleVolume.get_skipped_frames())
        unreal.BIBAnimeStyleVolume.reset_frame_stats()
    except Exception:
        pass
    log(line)


def back_to_ark():
    if bib_hub.hub() is not None:
        return
    unreal.BIBBlueprintLibrary.abandon_stage(world())
    yield 1.0
    unreal.BIBBlueprintLibrary.return_to_hub(world())
    yield from wait_until(lambda: bib_hub.hub() is not None, 120, "the Ark")
    yield 3.0


def stage(chapter, start):
    yield from back_to_ark()
    controller().bib_set_campaign_progress(start - 1)
    for action in ("tab:5", "camp:ch:%d" % chapter, "camp:start:%d" % start):
        bib_hub.hub().click_action(action)
        yield 0.3
    yield from wait_until(lambda: bib_hub.hub() is None and controller() is not None and controller().get_controlled_pawn() is not None, 120, "stage %d" % start)
    yield 8.0


def storm():
    W.set_hour(21.0)
    W.set_weather(unreal.BIBRain.HEAVY, True, unreal.BIBWind.HEAVY, True)
    W.set_surface_water(1.0, 1.0)


def flow():
    os.makedirs(OUT, exist_ok=True)
    log("tag %s, scenes %s, %.0f s each" % (TAG, ",".join(SCENES), SECONDS))
    LES.load_level(HUB)
    yield 2.0
    LES.editor_request_begin_play()
    yield from wait_until(lambda: bib_hub.hub() is not None, 120, "the Ark")
    for action in [a.strip() for a in os.environ.get("BIB_PERF_ACTIONS", "").split(",") if a.strip()]:
        bib_hub.hub().click_action(action)
        log("settings action: " + action)
        yield 0.3
    for cmd in [c.strip() for c in os.environ.get("BIB_PERF_CMDS", "").split(";") if c.strip()]:
        console(cmd)
        log("console: " + cmd)
    yield 6.0   # shaders, Lumen and the sky capture settle
    for scene in SCENES:
        if scene == "ark":
            if bib_hub.hub() is None:
                console("open %s?game=/Script/BoldInBeautyDemo.BIBGameMode" % "/Game/Greybox/Maps/L_Ark")
                yield from wait_until(lambda: bib_hub.hub() is not None, 120, "the Ark")
                yield 6.0
            yield from measure("ark")
        elif scene in ("stage1", "stage2"):
            yield from stage(1, 1) if scene == "stage1" else stage(2, 9)
            yield from measure(scene)
        elif scene == "neotokyo" or scene in CHAPTERS:
            target = "L_NeoTokyo" if scene == "neotokyo" else CHAPTERS[scene]
            if scene == "neotokyo":
                yield from back_to_ark()
            if scene == "neotokyo" and bib_hub.hub() is not None:
                bib_hub.hub().click_action("camp:map:NeoTokyo")   # the player's way in: Campaign > Neo Tokyo
            else:
                console("open /Game/Greybox/Maps/%s?game=/Script/BoldInBeautyDemo.BIBGameMode" % target)
            ok = yield from wait_until(lambda: target in map_name() and controller() is not None and controller().get_controlled_pawn() is not None,
                                       120, target)
            if not ok:
                continue
            yield 6.0
            storm()
            yield 6.0
            yield from measure(scene)
        else:
            log("unknown scene " + scene)


class Runner:
    def __init__(self):
        self.gen = None
        self.wait = 0.0
        self.started = False
        self.primed = False
        self.busy = False
        self.done = False
        self.boot = 0.0
        self.quit_in = 0.0
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def finish(self):
        self.done = True
        with open(os.path.join(OUT, TAG + ".log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        if world():
            LES.editor_request_end_play()
        self.quit_in = 3.0   # quit once PIE has really ended (quitting during end-play could hang the editor)

    def quit_tick(self, delta):
        self.quit_in -= delta
        if self.quit_in > 0.0 or world():
            return
        unreal.unregister_slate_post_tick_callback(self.handle)
        if os.environ.get("BIB_VERIFY_KEEP_OPEN") != "1":
            unreal.SystemLibrary.quit_editor()

    def tick(self, delta):
        DELTAS.append(delta)
        if self.done:
            self.quit_tick(delta)
            return
        if self.busy:
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
            self.finish()


RUNNER = Runner()
