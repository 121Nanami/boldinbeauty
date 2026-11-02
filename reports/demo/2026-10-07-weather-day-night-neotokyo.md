# Day / night + weather system, Neo Tokyo under the levels — 2026-10-07

- Status: complete with machine checks; **not played by a person**; all look numbers PROPOSED.
- Lead / worker: Claude Code, Opus 5.5 (Claude subscription), session `ai-agent-bb-b7`; one concurrent peer (`ai-agent-bb-49`,
  anime shader + Mac build) coordinated by message. File ownership agreed: this task never edits `build_assets.py`,
  `BIBAnimeStyle.*` or `Content/BIB`.
- Device: MACOS-LOCAL (owner's Mac), UE 5.8.3, worktree `worktrees/ai-agent-weather`, branch `claude/weather-tod` on top of
  `claude/anime-style-build` @ `d140278` (which is on `claude/greybox-maps`). Windows counterpart: OPEN (not checked).

## Owner request (2026-10-07, summarised)
1. Add the Neo Tokyo level under the levels.
2. In every playable level that is not a 2D arcade game: a weather system on a 24-minute day (1 minute game time = 1 hour),
   with simple dawn, day, dusk and night. Mostly lighting work: very vibrant by day within the anime post-process; a dynamic,
   good-looking dark sky at night. Weather events: light / medium / heavy rain, thunder, light / medium / heavy wind.
   Arknights-level quality inside the heavily stylised BB look. Water on character clothes, water trails that stay and
   reflect light, rain on small things like grass and the floor, as realistic as possible (optimise later).

## What was built
**Neo Tokyo under the levels.** The Campaign tab lists a **Neo Tokyo** tile after the 13 chapters (`camp:map:NeoTokyo`,
`BIBPages.cpp`). It opens `/Game/Greybox/Maps/L_NeoTokyo` with the demo's own game mode (`OpenLevel ... ?game=`), so the
player's team walks the greybox city with the normal character, camera and HUD ("Neo Tokyo - free roam"). There is no story,
there are no enemies, and Pause > Abandon stage goes back to the menu (`ABIBGameMode::StartFreeRoam`, `EBIBFlow::Roam`). The
greybox map files are untouched.

**`ABIBWeatherSystem`** (`BIBGame/BIBWeather.h/.cpp`): spawned by `ABIBGameMode` wherever the anime look is on (stage maps,
Neo Tokyo, code-built stages incl. tower / arena / Boss Rush). It is off on the hub menus and in the 2D arcade overlays.
- **Clock:** one session-wide clock, 60 real s = 1 game hour, which keeps running across levels and menus. Phases: dawn
  04:30-07:00, day, dusk 17:20-19:50, night. The sun rises in the east at 06:00, reaches 55° at noon and sets at 18:00; the moon
  runs opposite.
- **Lighting:** it hides the level's own sun / sky light / atmosphere / fog / clouds (and restores them when it switches off)
  and drives one key light (sun or moon), a real-time sky light, height + volumetric fog (maps built with heavy fog, like Neo
  Tokyo, stay foggy), and a time-of-day grade (unbound post-process at priority 5, under the anime volume): manual exposure,
  saturation, contrast, white balance and local exposure to keep shadows readable. An 11-key stylised palette covers sky,
  light, fog, grade, stars and clouds. Exposure was calibrated with sweeps in Neo Tokyo.
- **Sky (`M_BIBSky`, IsSky):** an anime gradient with a bright horizon band, a dawn / dusk glow wrapping toward the sun, the sun
  disc and corona, and a big stylised moon with maria and halo. At night: two layers of twinkling stars, a Milky Way, and a
  shooting star every few seconds; the star field turns with the hour. Cel-shaded clouds have crisp edges, three tones and
  silver linings, drift with the wind, close up into an overcast sheet in rain, and are lit from inside by lightning.
- **Weather:** a scheduler changes the weather every 2-5 game hours. Weights are PROPOSED; maps tagged `RainPlaceholder`
  (Neo Tokyo) rain far more often and start in heavy rain. Transitions take ~25 s. The rain levels are 0.3 / 0.62 / 1.0; a
  thunderstorm is heavy rain plus lightning and at least medium wind; wind can be calm, light, medium or heavy.
- **Rain:** three camera-centred rain layers (lit translucent, world-sized streaks, slanted into the wind) that stop under
  roofs. An overhead orthographic depth capture (160 m, 512²) gives the first rain-blocking surface. Splashes come from line
  traces from the sky, so they land on roofs, awnings and characters, not under cover.
- **Water on surfaces:** three DBuffer decals around the camera (120 m) on every surface, characters' clothes included:
  - wet darkening and gloss with porous variation;
  - drips running down walls and clothes;
  - puddles only on flat, top-most surfaces, with rain ripples and wind waves (world-space normals), which mirror neon at night.
  Wetness builds in ~40 s of heavy rain and dries over ~3 game hours. Puddles grow over ~2.5 game hours and stay ~6-10 game
  hours after the rain. Sheltered spots stay dry (walls test the street in front of them).
- **Wind:** anime speed lines that drift with the wind (more and faster when stronger), rain slant, cloud speed, and fog
  thinning.
- **Thunder:** jagged branching bolts, mostly in front of the camera, 0.5-3 km away, with a three-pulse flash. Each pulse is
  held long enough to survive the 12 fps frame hold. A flash light and the clouds light up, and thunder follows at the speed of
  sound.
- **Audio:** rain and wind loops plus three thunder sounds, synthesised by `build_weather.py` (no third-party audio). Rain is
  muffled under cover.
- **Readout:** a small clock / weather line in the bottom-right corner (`bib.Weather.Clock 0` hides it; PROPOSED).
- **Assets:** `Tools/build_weather.sh` builds `/Game/Weather` (MPC_BIBWeather, M_BIBSky, M_BIBRain, M_BIBSplash,
  M_BIBWindStreak, M_BIBLightning, M_BIBWetDarken, M_BIBWetGloss, M_BIBPuddle, five sounds). It never touches `/Game/BIB`
  or `/Game/Greybox`. Plugin `ProceduralMeshComponent` is enabled (engine plugin).
- **Console:** `bib.Weather.Hour/Rain/Thunder/Wind/Auto/TimeScale/Strike/Clock/Report`, plus look tuning with
  `bib.Weather.ExposureOffset` and `bib.Weather.LightScale`.

## Verification (MACOS-LOCAL, 2026-10-07, UE 5.8.3, after rebasing onto `d140278`)
- Editor build: Succeeded, 0 errors, 0 warnings. `Tests/run_core_tests.sh`: every suite 0 failures (new checks: Neo Tokyo tile
  and action; the campaign root now has 14 tiles).
- `Tools/build_weather.sh`: STATUS OK. No material fails to compile in PIE.
- `Tools/verify_weather.py` (PIE, `-BIBDev`): **RESULT PASS, 45/45**. It checks:
  - the assets exist, the hub menus have no weather, Campaign lists Neo Tokyo, and the tile opens `L_NeoTokyo` with
    `BP_BIBGameMode_C` and `BP_BIBPlayerCharacter_C`;
  - the weather is on, the 4 level-lighting components are hidden, and the map is recognised as rainy;
  - the clock runs at 1.000 game h per real min, and the key light reads noon 10 / dawn 3.5 / dusk 2.7 / moon 0.44 lux, with
    the noon sun at -55°;
  - heavy rain wets the streets within 10 s and ~5,000 splashes land; rain audio plays;
  - the shelter capture agrees with physics (roof 5,907 vs trace 5,899 cm; the open crossing is unsheltered);
  - heavy wind draws 16 streaks and light wind 3; a strike draws a bolt and flash, and the storm strikes by itself;
  - puddles survive 3 game hours after the rain and then dry; leaving goes back to the hub with the weather off;
  - a code-built Chapter 2-1 turns the weather on and back off, and `L_Chapter1_Stage1` has it on.
- `verify_hub.py` regression: **RESULT PASS (74/74)**.
- Screenshots: `reports/demo/screenshots/2026-10-07_weather_*.png` (dawn, noon, dusk sky, night sky, light / heavy rain, rainy
  night, puddle close-up, heavy wind, storm, Chapter 2 dusk rain, Chapter 1 morning).

## Not done / OPEN
- Nobody has played it. Frame-time cost was not measured; the owner asked to optimise later. Known costs: volumetric fog,
  Lumen reflections on wet surfaces, three full-screen decals, three rain layers, a scene capture every ~2 s or 8 m, and up to
  64 traces per frame for splashes.
- Small stage rooms (Chapter 1) are dim when the low morning / evening sun is behind their walls.
  `bib.Weather.ExposureOffset` and the palette in `BIBWeather.cpp` tune it.
- Wetness streaks and puddle noise are world-space, so on a moving character the drip pattern slides slightly (hidden mostly
  by the 12 fps hold). There are no wet footprints, no per-material wetness (the decals treat every surface alike) and no
  foliage sway; there is no foliage asset yet.
- The Ark is not opened through the game mode (it is not in the levels list); under its cavern roof the system would
  correctly keep rain out but the sun would be blocked.
- Windows: OPEN.

## Decisions the assistant made (PROPOSED, reversible)
These are all numbers and looks: phase times, sun path, palette colours, exposure, light levels, rain / wind levels, the
scheduler's weights and durations, how fast surfaces wet and dry, puddle size, splash / streak counts, bolt distance and
frequency, the synthesised sounds, the corner readout, Neo Tokyo starting in heavy rain, and free roam having no enemies or
story. No lore, character or gameplay rule was changed.

## Proposed BoldInBeauty_MAIN update
Integrated into section 11 as "Unreal demo — day / night + weather system, Neo Tokyo under the levels — 2026-10-07"
(hashes in `sync/handoff-state.json`, amendment `2026-10-07_weather`).
