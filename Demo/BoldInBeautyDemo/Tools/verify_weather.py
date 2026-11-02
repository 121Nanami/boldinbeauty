"""Checks Neo Tokyo under the levels and the day / night + weather system (BIBGame/BIBWeather.h, Tools/build_weather.py).

    UnrealEditor <BoldInBeautyDemo.uproject> -BIBDev -ExecCmds="py <abs path>/Tools/verify_weather.py"

1. Assets: MPC_BIBWeather, the sky / rain / particle / decal materials and the five sounds exist.
2. PIE boots into the Ark (the hub since 2026-10-07; underground, so no weather there). Campaign > Neo Tokyo opens L_NeoTokyo with the game's own mode and character.
3. In Neo Tokyo: the weather system is on and owns the lighting; the clock runs at 1 game hour per real minute; every
   phase and weather (clear dawn / noon / dusk / night, light / medium / heavy rain, thunderstorm, light / medium / heavy
   wind) is set and photographed; splashes, wind streaks, lightning, wetness build-up and puddles that stay are checked;
   the overhead rain-shelter capture agrees with a physics trace onto a roof.
4. Pause > Abandon returns to the Ark and switches the weather off; a code-built stage (Chapter 2-1) and the Chapter 1
   stage map switch it on.
Screenshots: Saved/Weather/<case>.png. Log: Saved/VerifyWeather.log (ends with RESULT PASS or RESULT FAIL).
Environment: BIB_VERIFY_KEEP_OPEN=1 keeps the editor open; BIB_WEATHER_SHOTS_ONLY=1 skips the checks after Neo Tokyo.
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
OUT = os.path.join(SAVED, "Weather")
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

HUB = "/Game/BIB/Maps/L_Hub"
W = unreal.BIBWeatherSystem
RAIN = unreal.BIBRain
WIND = unreal.BIBWind


def log(text):
    LOG.append(text)
    unreal.log("[verify_weather] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return UES.get_game_world()


def controller():
    w = world()
    return unreal.GameplayStatics.get_player_controller(w, 0) if w else None


def weather():
    w = world()
    return W.find_active(w) if w else None


def report():
    system = weather()
    if not system:
        return {}
    out = {}
    for part in str(system.get_report()).split():
        if "=" in part:
            key, value = part.split("=", 1)
            try:
                out[key] = float(value)
            except ValueError:
                out[key] = value
    return out


def map_name():
    w = world()
    return w.get_outermost().get_name() if w else ""


def hub():
    return bib_hub.hub()


def stage_title():
    pc = controller()
    return str(unreal.BIBBlueprintLibrary.get_hud_info(pc).get_editor_property("stage_title")) if pc else ""


def wait_until(cond, timeout, what):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.2
    check(False, "timed out waiting for " + what)
    return False


def console(cmd):
    unreal.SystemLibrary.execute_console_command(world(), cmd)


def look(pitch, yaw=None):
    pc = controller()
    rot = pc.get_control_rotation()
    pc.set_control_rotation(unreal.Rotator(pitch=pitch, yaw=rot.yaw if yaw is None else yaw, roll=0.0))


def screenshot(name, settle=2.5):
    yield settle
    path = os.path.join(OUT, name + ".png")
    console("HighResShot 1600x900 filename=%s" % path)
    log("screenshot %s  [%s]" % (path, W.get_clock_text()))
    yield 1.2


def trace_down(x, y, top_z):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, top_z), unreal.Vector(x, y, top_z - 60000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()   # (blocking_hit, initial_overlap, time, distance, location, impact_point, ...)
    return t[5] if t[0] else None


def assets():
    for path in ["MPC_BIBWeather", "M_BIBSky", "M_BIBRain", "M_BIBSplash", "M_BIBWindStreak", "M_BIBLightning",
                 "M_BIBWetDarken", "M_BIBWetGloss", "M_BIBPuddle", "Audio/S_RainLoop", "Audio/S_WindLoop",
                 "Audio/S_Thunder1", "Audio/S_Thunder2", "Audio/S_Thunder3"]:
        check(unreal.EditorAssetLibrary.does_asset_exist("/Game/Weather/" + path), "asset /Game/Weather/" + path)
    sky = unreal.load_asset("/Game/Weather/M_BIBSky")
    if sky:
        check(sky.get_editor_property("is_sky"), "M_BIBSky is a sky material")


def set_case(hour, rain, thunder, wind, wet=None, puddles=None):
    W.set_hour(hour)
    W.set_weather(rain, thunder, wind, True)
    if wet is not None:
        W.set_surface_water(wet, puddles if puddles is not None else 0.0)


def neo_tokyo():
    yield from wait_until(lambda: hub() is not None, 90, "the Ark")
    yield 2.0
    system = W.find_active(world())
    check(system is None, "the Ark (underground): no weather system enabled")
    hub().click_action("tab:5")
    yield 0.5
    try:
        ids = [str(hub().get_grid_id(i)) for i in range(hub().get_grid_count())]
    except Exception:
        ids = []
    if ids:
        check("camp:map:NeoTokyo" in ids, "the Campaign tab lists Neo Tokyo (%d tiles)" % len(ids))
    hub().click_action("camp:map:NeoTokyo")
    ok = yield from wait_until(lambda: "L_NeoTokyo" in map_name() and controller() is not None and controller().get_controlled_pawn() is not None, 120, "L_NeoTokyo to load")
    if not ok:
        return
    yield from wait_until(lambda: "Neo Tokyo" in stage_title(), 30, "free roam to start")
    check("Neo Tokyo" in stage_title(), "HUD shows the free roam title (%s)" % stage_title())
    pawn = controller().get_controlled_pawn()
    check(pawn.get_class().get_name().startswith("BP_BIBPlayerCharacter") or pawn.get_class().get_name().startswith("BIBPlayerCharacter"),
          "the operator character walks Neo Tokyo (%s)" % pawn.get_class().get_name())
    mode = unreal.GameplayStatics.get_game_mode(world())
    check(mode is not None and "BIBGameMode" in mode.get_class().get_name(), "Neo Tokyo runs the BIB game mode (%s)" % (mode.get_class().get_name() if mode else None))
    yield 8.0   # shaders, Lumen and the sky light capture settle
    r = report()
    check(r.get("enabled") == 1, "weather system enabled in Neo Tokyo")
    check(r.get("hiddenLevelLighting", 0) >= 3, "the map's own moon / sky light / atmosphere / fog are hidden (%s)" % r.get("hiddenLevelLighting"))
    check(r.get("assets") == 1 and r.get("skyMaterial") == 1, "weather assets loaded and the sky dome has its material")
    check(r.get("rainyMap") == 1, "Neo Tokyo is recognised as a rainy map")

    if os.environ.get("BIB_WEATHER_SWEEP"):
        # Look tuning: exposure / light sweeps (BIB_WEATHER_SWEEP="hour:offset:scale,...").
        W.set_time_scale(0.0)
        for item in os.environ["BIB_WEATHER_SWEEP"].split(","):
            hour, offset, scale = item.split(":")
            set_case(float(hour), RAIN.NONE, False, WIND.CALM, 0.0, 0.0)
            console("bib.Weather.ExposureOffset " + offset)
            console("bib.Weather.LightScale " + scale)
            look(-6.0)
            yield from screenshot("sweep_h%s_e%s_l%s" % (hour, offset, scale), 3.0)
        return

    # Clock: 1 real minute = 1 game hour.
    W.set_time_scale(1.0)
    W.set_hour(10.0)
    yield 0.2
    start_hour, start = W.get_hour(), time.monotonic()
    yield 6.0
    rate = (W.get_hour() - start_hour) / ((time.monotonic() - start) / 60.0)
    check(abs(rate - 1.0) < 0.12, "clock runs at 1 game hour per real minute (%.3f h/min)" % rate)
    W.set_time_scale(0.0)

    os.makedirs(OUT, exist_ok=True)
    N, LIGHT_R, MED_R, HEAVY_R = RAIN.NONE, RAIN.LIGHT, RAIN.MEDIUM, RAIN.HEAVY
    CALM, LIGHT_W, MED_W, HEAVY_W = WIND.CALM, WIND.LIGHT, WIND.MEDIUM, WIND.HEAVY

    # Phases, clear sky, dry streets.
    lux = {}
    for name, hour in (("dawn", 6.3), ("noon", 12.0), ("dusk", 17.8), ("night", 23.0)):
        set_case(hour, N, False, CALM, 0.0, 0.0)
        look(-6.0)
        yield from screenshot("clear_%s_street" % name, 4.0)
        look(28.0)
        yield from screenshot("clear_%s_sky" % name, 1.5)
        lux[name] = report().get("keyLux", 0)
        log("%s: %s" % (name, W.get_clock_text()))
    check(lux["noon"] > 5.0 and lux["noon"] > lux["dawn"] > lux["night"] * 0.5 and lux["night"] < 1.0,
          "key light: bright noon, softer dawn, dim moonlight (%s)" % lux)
    check(report().get("phase") == "Night", "23:00 is Night")
    W.set_hour(12.0)
    yield 0.5
    check(report().get("keyPitch", 0) < -20.0, "the noon sun shines down from high in the sky (pitch %.1f)" % report().get("keyPitch", 0))

    # Wetness builds up in rain; splashes appear.
    set_case(14.0, HEAVY_R, False, MED_W, 0.0, 0.0)
    look(-6.0)
    spawned0 = report().get("splashesSpawned", 0)
    yield 10.0
    r = report()
    check(r.get("wetness", 0) > 0.15, "ten seconds of heavy rain wet the streets (wetness %.2f)" % r.get("wetness", 0))
    check(r.get("splashesSpawned", 0) - spawned0 > 500, "rain splashes land (%d)" % (r.get("splashesSpawned", 0) - spawned0))
    check(r.get("rainAudio") == 1, "rain audio plays")
    check(r.get("shelterValid") == 1, "the rain-shelter capture ran")

    # Shelter capture vs physics: a rooftop near the player shelters the street under it; the open crossing does not.
    pawn_loc = controller().get_controlled_pawn().get_actor_location()
    system = weather()
    open_top = system.sample_shelter_top(unreal.Vector(pawn_loc.x, pawn_loc.y, pawn_loc.z))
    ground = trace_down(pawn_loc.x, pawn_loc.y, pawn_loc.z + 40000.0)
    if ground is not None:
        check(open_top < pawn_loc.z + 300.0, "open street: nothing shelters the player (top %.0f, player %.0f, trace %.0f)" % (open_top, pawn_loc.z, ground.z))
    found = False
    for dx in range(-5000, 5001, 1000):
        for dy in range(-5000, 5001, 1000):
            hit = trace_down(pawn_loc.x + dx, pawn_loc.y + dy, pawn_loc.z + 40000.0)
            if hit is not None and hit.z > pawn_loc.z + 1500.0:
                top = system.sample_shelter_top(unreal.Vector(hit.x, hit.y, pawn_loc.z))
                check(abs(top - hit.z) < 250.0, "a roof at (%.0f, %.0f) shelters the street below: capture %.0f vs trace %.0f" % (hit.x, hit.y, top, hit.z))
                found = True
                break
        if found:
            break
    check(found, "found a rooftop near the start for the shelter check")

    # Rain cases.
    for name, hour, rain, wind in (("light_rain_morning", 9.0, LIGHT_R, LIGHT_W), ("rain_afternoon", 15.0, MED_R, MED_W),
                                   ("heavy_rain_noon", 12.5, HEAVY_R, MED_W), ("heavy_rain_night", 21.5, HEAVY_R, LIGHT_W)):
        set_case(hour, rain, False, wind, 1.0, 0.85)
        look(-8.0)
        yield from screenshot(name, 4.0)
    look(-30.0)
    yield from screenshot("puddles_closeup", 1.0)

    # Wind: anime streaks, clear windy day.
    set_case(11.0, N, False, HEAVY_W, 0.0, 0.0)
    look(-4.0)
    yield 6.0
    check(report().get("streaks", 0) >= 6, "heavy wind draws wind streaks (%d)" % report().get("streaks", 0))
    yield from screenshot("heavy_wind_day", 0.5)
    set_case(11.0, N, False, LIGHT_W, 0.0, 0.0)
    yield 6.0
    light_streaks = report().get("streaks", 0)
    check(light_streaks < 8, "light wind draws few streaks (%d)" % light_streaks)
    set_case(11.0, N, False, MED_W, 0.0, 0.0)
    yield 3.0

    # Thunderstorm: bolts, flash, thunder.
    set_case(22.5, HEAVY_R, True, HEAVY_W, 1.0, 1.0)
    look(14.0)
    yield 4.0
    strikes0 = report().get("strikes", 0)
    system.strike_lightning()
    yield 0.05
    r = report()
    check(r.get("bolts", 0) >= 1 and r.get("flash", 0) > 0.1, "a strike draws a bolt and flashes (bolts %d, flash %.2f)" % (r.get("bolts", 0), r.get("flash", 0)))
    console("HighResShot 1600x900 filename=%s" % os.path.join(OUT, "storm_strike.png"))
    yield 1.5
    yield 20.0
    check(report().get("strikes", 0) - strikes0 >= 2, "the storm strikes by itself (%d)" % (report().get("strikes", 0) - strikes0))
    look(-8.0)
    yield from screenshot("storm_night_street", 0.5)

    # Puddles stay after the rain stops; surfaces dry over game hours.
    W.set_surface_water(1.0, 0.9)
    W.set_weather(N, False, CALM, True)
    W.set_time_scale(60.0)   # one game hour per second
    t0, h0 = time.monotonic(), W.get_hour()
    yield 1.0
    r = report()
    hours = (W.get_hour() - h0) % 24.0
    check(r.get("puddles", 0) > 0.9 - 0.25 * hours, "%.1f game hours after the rain the puddles are still there (%.2f)" % (hours, r.get("puddles", 0)))
    yield 8.0
    r = report()
    check(r.get("wetness", 1) < 0.9 and r.get("puddles", 1) < 0.85, "they dry out over the following hours (wetness %.2f, puddles %.2f)" % (r.get("wetness", 1), r.get("puddles", 1)))
    W.set_time_scale(1.0)
    W.set_auto_weather(True)

    # Leave: back to the hub, weather off.
    unreal.BIBBlueprintLibrary.abandon_stage(world())
    ok = yield from wait_until(lambda: bib_hub.in_ark(), 90, "the Ark after leaving Neo Tokyo")
    if ok:
        yield 2.0
        check(W.find_active(world()) is None, "back in the Ark the weather is off")


def code_stage():
    controller().bib_set_campaign_progress(8)
    for action in ("tab:5", "camp:ch:2", "camp:start:9"):
        hub().click_action(action)
        yield 0.3
    ok = yield from wait_until(lambda: "Chapter 2 - Level 1" in stage_title(), 60, "Chapter 2-1 to start")
    if not ok:
        return
    yield 4.0
    r = report()
    check(r.get("enabled") == 1 and r.get("hiddenLevelLighting", 0) >= 1, "code-built stage: weather on, hub lighting hidden (%s)" % r.get("hiddenLevelLighting"))
    set_case(17.6, RAIN.LIGHT, False, WIND.MEDIUM, 0.6, 0.3)
    yield from screenshot("chapter2_dusk_light_rain", 4.0)
    unreal.BIBBlueprintLibrary.abandon_stage(world())
    yield 1.0
    pc = controller()
    unreal.BIBBlueprintLibrary.return_to_hub(world())
    ok = yield from wait_until(lambda: bib_hub.in_ark(), 90, "the Ark after the stage")
    yield 2.0
    check(ok and W.find_active(world()) is None, "after the stage, back in the Ark, the weather is off")


def stage_map():
    LES.load_level("/Game/BIB/Maps/L_Chapter1_Stage1")
    yield 2.0
    LES.editor_request_begin_play()
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE in L_Chapter1_Stage1")
    yield 6.0
    check(report().get("enabled") == 1, "L_Chapter1_Stage1: weather on")
    set_case(8.0, RAIN.NONE, False, WIND.LIGHT, 0.0, 0.0)
    yield from screenshot("chapter1_stage1_morning", 4.0)
    LES.editor_request_end_play()
    yield 3.0


def flow():
    os.makedirs(OUT, exist_ok=True)
    assets()
    LES.load_level(HUB)
    yield 2.0
    LES.editor_request_begin_play()
    yield from wait_until(lambda: world() is not None and controller() is not None, 60, "PIE (boots into the Ark)")
    yield 4.0
    yield from neo_tokyo()
    if os.environ.get("BIB_WEATHER_SHOTS_ONLY") == "1":
        return
    if hub():
        yield from code_stage()
    LES.editor_request_end_play()
    yield 3.0
    yield from stage_map()


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
        with open(os.path.join(SAVED, "VerifyWeather.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        if world():
            LES.editor_request_end_play()
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
