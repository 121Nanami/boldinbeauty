# The Ark is the hub (real-time UI, 2D main screen deleted, saved position), no white glare + object shake / shading, second greybox detail pass: 2026-10-07/08

**Device:** MACOS-LOCAL (owner's Mac), UE 5.8.3, worktree `worktrees/ai-agent-bb`, branch `claude/chapter-maps` (on `596e6ed`).
**Executor:** Claude Code, Opus 5.5 (Claude subscription). **Status:** built and machine-checked, **not played by a person**.
Counterpart (Windows) freshness: OPEN.

## Owner requests
1. **Earlier session (2026-10-07, interrupted, uncommitted in `worktrees/ai-agent-ark`, branch `claude/ark-world-ui`), verbatim:**
   "Your job is to transfer the UI From a 2d intro screen to a highend working interface In game Transfer most of the static things
   into a simple thing one of those 3 lined menu's in the top right no other ui on base then when you click that does a flow chart
   like Menu/The-Resistence/Arc not actually one but an example and every time you click one it opens another except a few things,
   First of all in battles make sure only relevant UI Is there, 2. Make it so all the merchants have dedicated stalls and if you
   click on it in the menu it teloports you there same rules with the arcade as well If these structures don't exist yet make them
   with grey boxing also this is all in the arc the lobby doesn't exist anymore Also blockout on the one side that doesn't have a
   thing on it a big factory the operator generator factory which is where you summon from again greybox this And apply this logic
   for the rest of stuff like a armourary for the armour equipment etc [...] mainly convert the boring old ui to vibrant ui with
   actual 3d presences in the arc"
2. **This session:** "Continue worimg on gyey boxing all 13 chapter + the arc with more details, Setting up the real time ui to work
   and replace and delete that boring main screen and spawn the player in the arc where there last position was saved, And all
   other tasks + Can you fix up the Postprocessor to be have none of that Extreme white light the occurs and keep it like the other
   part with colors also add a bit more shaking and shading to the objects not the camera At the end off this update the
   BoldInBeauty_MAIN and Handoff"

## What was done
**Ark hub (integrated from the interrupted session).** Its working tree was applied here as a patch (sha256
`d4a60ddc5d3d77df0768413e20c5c8658321272e17143e9ca1d8a1cde9092705`) plus 11 new files; the `ai-agent-ark` worktree itself was not
touched. `BIBRules/BIBArk.*` (stations and the flow-chart menu as data), `BIBGame/BIBArkUI.*` (menu button, flow-chart columns, glass
station panels over the 3D view, arrival banner, toasts, a battle HUD that shows only the fight), `BIBArkStation.*` (hologram sign,
ring, beacon, light), `BIBArkLibrary.*` (test hooks), `Tools/greybox_ark_hub.py` (Operator Generator Factory on the empty north side
of the operators' platform, Merchant Row with five stalls, arcade hall, training grounds, library, gang hall, Back Alley stall,
17 station anchors), `Tests/ark_tests.cpp`, `Tools/verify_ark.py`.

**Deleted the boring main screen.** `UBIBHubWidget` (`BIBHubWidget.h/.cpp`), `WBP_Hub` (no longer built), the Slate hub and its
unused Summon / Operators / Equipment / Team / Merchant / Demo-tools screens, and `-BIBClassicHub`. `EBIBScreen::MainMenu` stays as an
enum slot only (Blueprint data compatibility); `SetScreen(MainMenu)` goes to the Ark. `L_Hub` is now only the boot map (it travels to
the Ark at once, before sign-in) and the map code-built stages are built in. The sign-in screen sits over the living Ark while a
camera circles the government spire. Results > Continue, Pause > Abandon and the Blueprint `ReturnToHub` all end in the Ark. With no
`L_Ark` on the machine the game says to run `Tools/build_greybox.sh`. Level maps opened directly in the editor stay where they are.

**Saved position.** `FMeta::bArkPosValid / ArkPos / ArkYaw` (`BIBRules/BIBMeta.h`), saved as `arkPosition` in the save JSON
(`BIBSave.cpp`; older saves have none and start on the plaza; non-finite or > 20 km values are dropped). `ABIBGameMode` copies the
player's feet position into the profile every 0.5 s while walking on the ground (the 10 s autosave writes it) and writes the save on
leaving for a stage or free roam and before sign-out. Arrival on every Ark load: an explicit trip to a station, else the saved spot
(only if a floor trace finds ground there), else the plaza. Test hooks `GetArkArrival`, `GetSavedArkPosition`, `SaveArkPosition`.

**Post-process: no extreme white light, keep the colours** (`Tools/build_assets.py` HLSL; `BIBAnimeStyle.cpp`; `BIBWeather.cpp`;
`build_weather.py`; `greybox_common.py`). Cause: wet streets were near-mirrors (decal roughness 0.08, road 0.08) reflecting a bright
sky, the highlight tier pushed bright pixels towards white (x1.08), bloom 0.8 from 1.0, and overcast added +0.9 EV. Now: luminance
above `WhiteKnee` 0.6 rolls off towards `WhiteCeiling` 0.84 with the hue kept, bleached pixels get saturation back
(`BrightSaturation` 0.6), no channel above 0.94; the highlight tier multiplies by its warm tint; bloom 0.25 from 2.5; overcast +0.45
EV; local-exposure highlight contrast 0.7; wet-gloss decal roughness 0.24, greybox WetRoad 0.2, Sidewalk 0.32 (puddles stay mirrors).
**More shake and shading on the objects, not the camera:** the drawing of each surface boils with noise fixed to its world position
(`ObjectShake` 1.4 px at 1080p, `ObjectBoilScale` 140 cm, re-drawn 8 times a second, fading with distance); the sky, the camera and
the HUD do not move (`FrameJitter` stays 0). Form shading (`FormShade` 0.45) darkens and cools surfaces turning away from the view;
contact shading (`ContactShade` 0.55) darkens creases and the ground under objects from the line pass's depth samples. 42 material
parameters (was 33).

**Greybox, more details** (`Tools/greybox_detail2.py`, see `docs/chapter-maps.md`). Sections went from about 29 pieces to about 200:
cover walls, pillars, 2-3 raised platforms (teal jump-up, orange climb face, railings), themed props, ground tiles and lights; routes
between sections get floor markers and a two-tier lookout tower; Neo Tokyo gets 66 fire escapes, 50 vertical neon signs, balconies and
about 200 pavement props; the Ark's town square, operators' platform, Underworld and civilian boulevard are furnished. Chapter 11 is
left empty (owner: "nothing in it"). Pieces are placed on the real floor by editor line traces.

| Map | Actors | Map | Actors |
| --- | --- | --- | --- |
| L_NeoTokyo | 18,191 (was 15,687) | L_Ch08_Intersection | 10,327 |
| L_Ark | 13,348 (+5,593) | L_Ch09_RemnantRemains | 9,626 |
| L_Ch02_Ruins | 10,114 | L_Ch10_Hallucinations | 9,387 |
| L_Ch03_Docks | 9,395 | L_Ch11_ReturnToNowhere | 9,943 (unchanged) |
| L_Ch04_HangingCity | 9,772 | L_Ch12_Paradise | 9,884 |
| L_Ch05_ArcTech | 9,849 | L_Ch13_SecretiveSoul | 9,596 |
| L_Ch06_MoonKingdom | 9,605 | L_Ch07_TOMBS | 10,632 |

**Verify scripts.** The older PIE scripts drove `WBP_Hub`; they now drive the same page model in the Ark through `Tools/bib_hub.py`
(`UBIBArkLibrary::GetPageGridIds / GetPageBarIds / GetPageTab`). `verify_hub.py`, `verify_game.py` and the legacy `verify_flow.py`
tested only the deleted lobby and were removed. New: `verify_ark_save.py` (two editor runs), `verify_ark_start.py` (sign-in over the
Ark, `-BIBNoAutoLogin`), `shoot_sections.py` (screenshots). **Fixed on the way:** `Tools/build_assets.sh` run from a worktree deleted
`Content/BIB`, did not find the engine and printed the previous run's log (a false STATUS OK); it now finds the engine, fails loudly
and clears the old log. `verify_tester.py` also removes `guest.json.bak` (the loader falls back to it). Two stale expectations from
earlier iterations were corrected: `verify_campaign` (Chapter 1 enemy scale 2.05 since the 2026-10-06 rebalance) and
`verify_chapters` (the first detail pass added 20 tagged feeder conveyors).

## Verification (MACOS-LOCAL, 2026-10-07/08, UE 5.8.3)
- Editor build (`BoldInBeautyDemoEditor`, `-NoHotReloadFromIDE`): Succeeded, 0 errors / 0 warnings (four builds).
- `Tests/run_core_tests.sh`: 103 core, 12,873 meta, 2,454 kit, 4,450 account, 4,066 campaign, 4,380 tester, 440 arcade / arena /
  live-ops / online, 59 mini-game, 230 gacha and **777 Ark** checks (9 new for the saved position), 0 failures; no soft-locks.
- `build_assets.sh` STATUS OK (42 anime parameters, 2 UMG screens, no `WBP_Hub`); `build_weather.sh` OK; `build_greybox.sh` OK (counts
  above). `verify_assets.py` 74 OK, 0 FAIL.
- PIE: `verify_ark` **133/133 PASS** (boot into the Ark, plaza arrival, 17 stations, menu, every teleport + F, panels, factory summon,
  battles with only the battle HUD, back at the task room, saved position after a stage: arrival "saved"); `verify_ark_save` phases
  1 and 2 **PASS** (after a fresh editor start the guest stands 0 cm from the saved spot, same facing); `verify_ark_start` **8/8 PASS**
  (`-BIBNoAutoLogin`: boots into `L_Ark` with the sign-in screen, the intro camera moves 2,220 cm in 2 s, no menu button, Continue
  as guest -> walking in the Ark at the saved spot, camera on the player); `verify_anime_style` 47 OK PASS; `verify_weather` 45 OK
  PASS (incl. rain audio and `L_Chapter1_Stage1` weather on after the boot-rule fix); `verify_gacha` 16 PASS; `verify_arcade` 46 PASS;
  `verify_tester` 31 PASS (`-BIBGuest`); `verify_campaign` 125 OK, one flaky miss of 2 assassins in 13-8 section 3 (two reruns of
  13-8 PASS, 34/34 each; spawn code untouched); `verify_chapters` 217 PASS, 1 stale FAIL (conveyor count from the first detail pass; check corrected, Chapter 5 rerun 21/21 PASS).
- **White glare, measured** on the same nine weather screenshots (old 2026-10-07 shots vs now, 400x225, pixels with all channels
  >= 235 / >= 215, mean saturation): clear noon 11.2% / 13.3% -> 0.0% / 0.0%, saturation 0.21 -> 0.29; clear dawn 11.3% / 22.6% ->
  0 / 0 (0.13 -> 0.24); heavy wind day 10.9% / 13.1% -> 0 / 0 (0.23 -> 0.31); heavy rain noon 3.6% / 14.3% -> 0 / 0 (0.15 -> 0.25);
  light rain morning, heavy rain night, storm night, puddles close-up and dusk sky also 0.0% white with higher saturation.
- Screenshots looked at: Neo Tokyo noon / heavy rain (blue-lavender street with cyan and pink neon reflections instead of white),
  the Ark plaza, the Ruins boss section (cover, platforms, wrecks, lavender shadows). `shoot_sections.py` wrote only the Ruins shot
  before quitting; `verify_chapters`' editor-viewport shots were unreliable this run (editor at 3 fps in the background).
- **Not measured:** the 12 fresh frames a second (the background editor ran at 3 fps; the frame-step code is unchanged and was
  measured at 12.0/s on 2026-10-07); frame cost of the new detail. Not played by a person.

## Assistant's reading (PROPOSED, OPEN for the owner)
Which page lives in which building and where the new buildings stand; the menu tree and keys (M / Esc / P menu, F or T at a ring,
hold Alt for the cursor); the intro camera; every look number above; every detail-pass placement and the chapter prop themes.

## OPEN
- Owner walk / play-through and look review (the Ark menus, the saved position, the new shading and shake).
- Performance: the detail pass adds 400-5,600 actors per map; frame cost not measured. The 12-steps-a-second presentation was not
  re-measured this round because the background editor ran at 3 fps (the frame-step code is unchanged).
- `verify_accounts.py` (needs the dev account server) and `verify_kits.py` (about 25 min) were ported but not re-run.
- Joining the chapter maps to the campaign stages; a climbing / swimming character for them.
- The documentation-root and repository-root `BoldInBeauty_MAIN` replicas differ from this branch's copy (not reconciled). Windows.
