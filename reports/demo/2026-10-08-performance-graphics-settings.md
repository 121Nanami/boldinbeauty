# Performance pass and graphics settings — Claude Code (Opus 5.5) — 2026-10-08

- Status: complete with checks (MACOS-LOCAL); not played by a person; Windows OPEN.
- Accountable lead; worker; owned paths: this session (standalone, lead = owner); `Demo/BoldInBeautyDemo` source, tests, tools, README; this report.
- Device; actual root: owner's Mac (Apple M5 Pro, 16-core GPU, 24 GB, macOS 26.7), worktree `BoldInBeauty-Project/worktrees/ai-agent-bb`, branch `claude/chapter-maps` on `a622cd5`; UE 5.8.3.
- Base manifest; counterpart: Windows replica and freshness OPEN (no `sync/devices` profile on this Mac).
- Concurrency note: another session's editor (`BoldInBeauty-Project-worktrees/busy-ptolemy`) ran on the same Mac during some measurements; it shares the GPU, so the numbers are conservative.

## Owner request (2026-10-08, verbatim)
"add customisations that allow to lower graphics and other stuff that causes lag and 2. Start optimising the game to run better as currently it runs at 3 fps on a 4000 dollar pc"

## What was found
1. **Exactly 3 fps = the editor's background throttle.** `UEditorEngine::GetMaxTickRate` returns 3.0 whenever
   `ShouldThrottleCPUUsage()` is true: the editor app is not in the foreground and Editor Preferences > Performance >
   "Use Less CPU when in Background" is on. It is on in this Mac's engine-wide
   `~/Library/Application Support/Epic/UnrealEngine/5.8/Saved/Config/MacEditor/EditorSettings.ini`
   (`bThrottleCPUWhenNotForeground=True`), which overrides a project `DefaultEditorSettings.ini` (tried, no effect, removed).
   Reproduced: `verify_anime_style` with the editor unfocused ran every scene at **3 fps** (33 frames in 11 s); MAIN's previous
   entry also recorded "background editor ran at 3 fps". Whether this is the owner's exact case is an inference (no owner play
   log exists on this Mac; the owner may also mean another PC: OPEN).
2. **The 12 fps anime look rendered all 60 frames.** `ABIBAnimeStyleVolume` held a copy of the finished frame and pasted it over
   the 48 frames in between, but those 48 were still rendered in full (Lumen, shadows, post, anime material) and thrown away.
3. Weather: three 120 m DBuffer decals drawn even when nothing is wet; the splash / streak procedural mesh rebuilt every frame
   even when empty; no way to lower any of it.
4. Settings had a single Quality preset; render scale used `SetResolutionScaleNormalized`, which since UE 5.3 maps 0..1 onto
   0..100 % (50 % in the menu became "default", 70 % became 40 %); graphics never applied in PIE.

## Changes
- `BoldInBeautyDemo.cpp` (+ `UnrealEd` dependency in editor builds): registers `GEditor->ShouldDisableCPUThrottlingDelegates`
  so a running play session is never throttled; idle editor behaviour unchanged. `bib.Editor.FullSpeedPlayInBackground 0`
  restores the preference.
- `BIBAnimeStyle.cpp`: held frames are **not rendered** — `SetupViewFamily` clears the family's `Rendering` show flag (the scene
  render builder then skips the renderer: no culling, shadows, Lumen, base pass, post) and `BeginRenderViewFamily` enqueues an RDG
  copy of the kept image onto the viewport; HUD / Slate draw over it as before. Fresh frame on a new step, after a gap, on a
  resize or before the first capture. `bib.Gfx.SkipHeldFrames 0` = old path. Test hook `GetSkippedFrames()`.
  `bib.Gfx.AnimeLook 0|1` (Settings > Anime look) lets the game mode turn the volume off.
- `BIBWeather.*`: `bib.Gfx.WeatherDetail` 0 Off (sky, lighting, clock, sound, lightning flash), 1 Low (rain sheets; no splashes
  / traces, no wet / puddle decals; shelter capture every 6 s), 2 Full (as before). On Full the decals switch off while
  everything is dry; the particle mesh is not rebuilt while empty.
- `BIBRules` (`BIBMeta.h/.cpp`, `BIBPagesExtra.cpp`, `BIBSave.cpp`): `FSettings::GraphicsDetail[10]` (-1 = follow preset),
  `WeatherDetail`, `bVolumetricFog`, `bAnimeLook`, `bDynamicResolution`, `ResolutionScaleMin` 30. Settings > Graphics tiles:
  Performance mode, Auto-detect, Quality (resets groups), render scale -/+, dynamic resolution, frame-rate limit, v-sync,
  window mode, show frame rate, ten group tiles (view distance, shadows, lighting / GI, reflections, post-processing, textures,
  effects, anti-aliasing, foliage, shading), weather effects, volumetric fog, anime look. Saved in the profile JSON.
- `BIBGameInstance.cpp`: `ApplyUserSettings` maps all of it to `UGameUserSettings` (packaged / `-game`) or straight to
  `Scalability::SetQualityLevels` in PIE (only once a graphics option differs from the defaults or was just changed, so the
  editor otherwise keeps its own scalability); render scale is now `SetResolutionScaleValueEx(percent)`; `AutoDetectGraphics()`
  runs `Scalability::BenchmarkQualityLevels()`. `BIBPlayerController.cpp` routes `set:autodetect` and flags graphics actions.
- `BIBGameMode.cpp`: `-BIBFreeRoam` test hook (any `/Game/Greybox/Maps/` map opened with `?game=` walks in free roam, so
  chapter maps can be profiled); free-roam message names the map.
- Tools: new `Tools/perf_capture.py` (PIE scenes ark / stage1 / stage2 / neotokyo / chNN; frame times, CSV game / render / GPU,
  anime fresh / held / skipped counts; `BIB_PERF_*` environment); `verify_anime_style.py` now also checks that held frames are
  skipped and that a plain viewport `shot` between steps is a full picture.

All look numbers are untouched; the defaults reproduce the previous image (Quality High, Weather Full, fog on, anime look on).
The new option names, Performance-mode values (Low, 70 %, weather Low, no fog) and the 30 % render-scale floor are PROPOSED.

## Measurements (MACOS-LOCAL, PIE, editor unfocused, 12 s per scene, `Saved/Perf/*.log`)
Median frame time; PIE is capped at 120 fps by the editor. Neo Tokyo / chapter maps in a night thunderstorm with heavy wind.

| Scene | Old path (`SkipHeldFrames 0`) | New default | Performance mode |
| --- | --- | --- | --- |
| Ark (hub) | 17.4 ms, 58 fps, GPU 17.1 | 8.4 ms, 119 fps (cap), GPU 0.9 | 8.4 ms, 120 fps |
| Chapter 2-1 (code-built, combat stage) | 14.9 ms, 67 fps | 8.5 ms, 117 fps | 8.4 ms, 119 fps |
| Neo Tokyo storm | 21.1 ms, 47 fps, GPU 21.6 | 8.4 ms, 119 fps, GPU 2.1 | 8.4 ms, 119 fps |
| Chapter 8 Intersection storm | 17.5 ms, 57 fps | 8.4 ms, 120 fps | 8.4 ms, 120 fps |
| Chapter 12 Paradise storm | 19.9 ms, 50 fps | 8.4 ms, 119 fps | 8.4 ms, 120 fps |

Every held frame was skipped (e.g. Neo Tokyo 1,282 of 1,282); 12.0-12.5 fresh frames a second. The CSV "GPU" medians are per
frame, so they mostly show skipped frames; a fresh frame still costs what it did (the old column), so the saving is about 5x
GPU work per second. With the background throttle (before the module fix) the same verify ran at 3 fps in every scene.
Standalone `-game` (1920x1080 and native full screen) was already 60 fps capped before the change (Ark GPU 8-10 ms) and
Neo Tokyo storm reaches ~120 fps after it.

## Checks
- `Tests/run_core_tests.sh`: all 0 failures (12,915 meta checks incl. the new graphics-settings checks; save round trip).
- Editor build `BoldInBeautyDemoEditor Mac Development`: Succeeded, 0 warnings.
- `verify_anime_style.py`: RESULT PASS, 62 OK (unfocused editor, 78-117 fps; held frames skipped; viewport shots of the Ark,
  Chapter 1-1 and Neo Tokyo are full anime-look pictures).
- `perf_capture.py`: three runs above; Performance mode logged "graphics applied to this editor session (quality 0, render scale 70%)".
- `verify_weather.py` (regression for the decal / particle changes): RESULT PASS 45/45 (the editor process exited with code 1 at shutdown; the log's result is PASS).

## Warnings, issues and lead questions

| Status | Severity | Finding / question | Evidence and impact | Proposed next action | Responsible |
| --- | --- | --- | --- | --- | --- |
| OPEN | warning | Which machine / mode showed 3 fps (this Mac's editor, a packaged build, or another PC)? | No owner play log on this Mac; 3 fps reproduced only as the editor background throttle | Owner: play once (Settings > Graphics > Show frame rate); if still slow, run `perf_capture.py` on that machine | owner |
| OPEN | info | Owner look review of Performance mode / Anime look off | defaults unchanged | owner decides the preset values | owner |
| OPEN | info | 13-16k separate static mesh actors per greybox map | render thread fine on M5 Pro; PIE start and weaker CPUs pay for it | batch into instanced meshes in the build scripts if a slower PC needs it | next session |
| OPEN | info | PIE scalability set by the Settings page stays for that editor session | editor viewport uses it until the editor restarts | acceptable for testing | — |
| OPEN | info | Windows | not built or measured | — | Windows session |

## Proposed BoldInBeauty_MAIN update
Section 11, new entry "Unreal demo — performance pass and graphics settings — 2026-10-08" (integrated with this report; see
`sync/handoff-state.json`).

## Handoff
Uncommitted on `claude/chapter-maps` (the owner did not ask for a commit). Remaining: owner play-through on the machine that
showed 3 fps; instancing the greybox maps if needed; Windows.
