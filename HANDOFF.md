# Boldinbeauty — handoff for the next AI (2026-10-15, updated with Ark Rally, after goal 2: the Angel AI rework and the combat feel rework)

**Read first:** `CLAUDE.md`, `AGENTS.md`, `BoldInBeauty_MAIN` (sections 0, 11, 12), `sync/README.md`, then this file. MAIN is the shared database; the newest owner instruction wins. The owner is the only developer and creative authority: never invent or change lore, characters, combat, progression or levels. Unknowns stay `OPEN`/`DEFERRED`; demo stand-in numbers are `PLACEHOLDER`.

## Angel behaviour layer (2026-10-12, branch `bb-ai-work`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-12-angel-behaviour-layer.md`.** Player model, squad board (max 3 melee tokens, roles, slots)
and whisker steering for the angels, on top of the existing angel mind (`BIBRules/BIBPlayerModel|BIBSquad|BIBSteer`, `BIBGame/BIBTactics*`);
`bib.Tactics 0` turns it off. Checks: core tests, build, `verify_campaign`, new `Tools/verify_tactics.py` PASS; `verify_combat` has one
pre-existing FAIL (13th-frame accents, same with the layer off). **Next:** human playtest, a real ultimate flag from the player.

## Locked maps built, the Ark is the default level (2026-10-11, branch `bb-ai-work`, owner's Mac)
Owner asked for it. `BIB_GREYBOX_ONLY=neotokyo,ch03,ch11,ch13 BIB_BUILD_LOCKED=1 Tools/build_greybox.sh` built `L_NeoTokyo`, `L_Ch03_Docks`,
`L_Ch11_ReturnToNowhere` (no post-processing volume, by design) and `L_Ch13_SecretiveSoul` into this worktree's git-ignored `Content/Greybox`
(the owner-lock guard in `greybox_chapters.py` / `greybox_story.py` is unchanged: it only stops builds without `BIB_BUILD_LOCKED`).
- **Ark default:** `Config/DefaultEngine.ini` `EditorStartupMap` and `GameDefaultMap` = `/Game/Greybox/Maps/L_Ark`. The Ark map now carries
  `BP_BIBGameMode` itself (`greybox_ark.py`), because the game starts there instead of being travelled to from `L_Hub`. `verify_ark_start.py` PASS.
- **Caveat:** `L_Ark` is generated and git-ignored; on a fresh checkout run `BIB_GREYBOX_ONLY=ark Tools/build_greybox.sh` first or the game has no default map.

## Asset inventory of the 40 chapters (2026-10-11, branch `bb-ai-work`, owner's Mac)
Newest work with the dream filter below. **Read `docs/Assets/Inventory/README.md`** and the MAIN entry "Asset inventory".
- Cataloguing only (nothing created or imported): arc + 40 chapters read in full; 40 chapter files confirmed. Per-chapter ledgers in
  `ledgers/`, deduplicated master catalog in `master/`, 2673 assets / 5558 line items, 946 `[inferred]`; all 1,285 Tripo rows accounted for. Regenerate with
  `build_ledger.py --all` then `--master` after editing `src/`.
- **Owner decisions** in `AMBIGUITIES.md`: chapters 1-13 have two scripts that disagree (newer `ChNN.md` followed, older tagged `[alt]`);
  cube size, 2008/2009 voice date, Judge's surname, card design, the 933 Tripo-only `[inferred]` rows; Nowhere must be filterless.
- **One-file version for any AI: `docs/Assets/ALL_ASSETS.md`** (all 2,673 assets with variations, chapters and a Tripo prompt or production brief each, in the updated dream-core art style; rebuild with `docs/Assets/Inventory/build_single_file.py`).
- MAIN and HANDOFF were backed up to `docs/Assets/Inventory/backups/` before editing. `docs/Story` and `docs/Assets` are still untracked.
- **Next:** owner picks canon scripts; then trim / confirm the `[inferred]` rows; then art production planning.

## Dream filter rework (2026-10-11, branch `bb-ai-work`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-11-dream-filter-rework.md`.**
- Old anime look replaced by native post-process settings only (desaturated, cool white point, lavender shadows, vignette 0.2,
  static fringe 0.08); no ink, bands, boil or 12 fps stepping. **Backup:** `bLegacyAnimeLook = true` on `ABIBAnimeStyleVolume`
  restores the old filter (`MI_BIBAnimeStyle` untouched). Before/after shots in `reports/demo/screenshots/2026-10-11_dream_*`.
- **Part 2 (AAA grade):** `DreamLookVersion` 2 (default) adds filmic tone curve, local exposure, soft bloom, split-toned grade, narrow
  exposure range; `DreamLookVersion = 1` is the backup. Old -> new table and 10 shots in the report.
- **Next:** owner look review (reference Omori); update `verify_anime_style.py`; re-measure performance.

## What exists
A playable **Unreal Engine 5.8 demo** (not the owner's real game) in `Demo/BoldInBeautyDemo`. Branch `claude/great-newton-wp7hgt` ([PR 1](https://github.com/121Nanami/BoldInBeauty-Project/pull/1)) holds the hub iteration; the newer work (Gameplay Ability System, all 20 operator kits) is on `claude/exciting-johnson-jy21xk`, based on it. Neither is merged to `main`. Never touch the owner's nested `Unreal Projects/BoldInBeauty` project or the `BoldInBeauty`/`BoldInBeauty-Game` repos.

- **Rules core** (`Source/.../BIBRules`): pure C++, no Unreal. Summoning, pity, levels, equipment, quests, mail, season pass, shops, Outer World, arcade, tower, arena, stage plans. `BIBPages.cpp` describes the seven-tab hub as data. Tests: `Tests/run_core_tests.sh` (g++/clang, 100+ and 7,600 checks).
- **Game layer** (`BIBGame`, `BIBGAS`): **all 20 operators have full kits** (a normal account now starts with only Luna; the admin account and the `-BIBDev` developer profile own all 20). Luna, Judge and Narrator are hand-written (owner files); Alchemist, Arita and Adapt follow the owner's Notion pages; the other 14 were designed by the assistant from the owner's playstyle notes and are **PROPOSED** (owner decides). The kit data and rules are pure C++ (`BIBRules/BIBKit*`, 2,413 checks, balance simulator); the Unreal layer runs them on the **Gameplay Ability System** (abilities, attributes, GameplayEffects for damage / healing / cooldowns, 534 generated gameplay tags). Read `docs/gas-architecture.md` and `docs/operator-kits.md`. `UBIBHubWidget` draws pages; `BIBUI` is a text-only Slate fallback.
- **Content** (`Content/BIB`, Git LFS) is generated entirely by `Tools/build_assets.sh`; hand edits are lost. Icons and portraits are plain PNGs in `Art/`, imported as `T_<name>`; replace a file and rebuild.
- **Campaign (2026-10-05):** all **13 chapters x 8 levels = 104 stages** are playable (read the section below). Chapter 1 is unchanged (boss on level 8 unlocks Plack); chapters 2–13 are generated from standard enemy archetypes and balanced to the economy.

## Flow / sync / impact pass (2026-10-10, branch `claude/chapter-maps`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-10-flow-sync-impact-pass.md`.** UI / VFX / transitions now share the world's 12 fps step
clock (`BIBMotion` start phase + per-step projection of followed VFX), every FX and hit pops an accent frame, hit stop is >= one
step (attacker 50-110 ms, angel 60-150 ms), atlas pages prefetch async. Checks: build, `verify_combat`, `verify_motion`, `verify_perf` PASS.
**Next:** owner play-through; per-piece impact timing for ability VFX.
## Goal 2: Angel AI rework (2026-10-14, branch `claude/happy-babbage-gmmcty`, on top of the combat feel rework)
Newest work. **Read `reports/demo/2026-10-14-angel-ai.md`** and the MAIN entry "goal 2: Angel AI rework".
- Brain (engine-free, `BIBRules/BIBAngelBrain.*`, 4,600 checks): every angel decides on the commander as seen ReactionSeconds ago
  (261 / 209 / 174 ms by tier in chapter 1, faster by chapter, floor 150 ms), with line of sight, memory and call-outs; habits over the
  last 12 actions; attack tokens / flank ring / ranged turns; defence, self-preservation, punishes, utility-scored kit moves, feints
  and habit-read combo enders. Every attack telegraphs >= MinTelegraph (0.35 / 0.30 s); ranged shots fly and can be sidestepped.
- Unreal: `UBIBAngelDirector` (world subsystem), `BIBGame/BIBEnemyBrain.cpp`, read-only `ABIBPlayerCharacter::GetAngelView()`
  (`BIBAngelView.cpp`). Knobs: `Config/DefaultGame.ini [BIB.AngelAI]` (`Difficulty` = 1.15 for the owner's "a bit harder");
  console `BIBAngel`, `BIBAngelReload`, `BIBAngelStats`, `BIBSpawnAngel <Kind> [N]`.
- **Not built yet:** editor build on the Mac, then `Tools/verify_angels.py` (new), `verify_combat.py`, `verify_feel.py`.

## Combat feel rework: every player attack (2026-10-13, branch `claude/happy-babbage-gmmcty` = `claude/chapter-maps` + this pass)
**Read `reports/demo/2026-10-13-combat-feel-rework.md`** and the MAIN entry "combat feel rework".
- Every hit has a weight (`BIBRules/BIBHitFeel.*`): hit stop on operator and angel together (light 3 .. ultimate finisher 14
  frames), reactions per enemy weight (flinch / stagger / knockback / launch / knockdown; bosses recoil, interrupted at most every
  6 s), directional camera kick + push-in, rumble, sound hooks, procedural wind-up / impact / recovery poses, 0.9 s skippable
  ultimate beats. Tune in `Config/DefaultGame.ini [BIB.CombatFeel]`; `BIBFeelReload` re-reads it while playing.
- Kit runtime reports hit origins (`FHitInfo`); data kits keep their own basic strings; dodge cancels recoveries; buffer 0.18 s;
  gamepad layout (`ABIBPlayerController::PadKey`). Names: Strain / BROKEN / Relay Strike / Phantom Counter.
- Luna rebalanced (owner allowed it): power 1.69 -> 0.84. Judge / Narrator unchanged (OPEN).
- **Not built yet:** run the editor build, then `Tools/verify_feel.py` (new), `verify_combat.py`, `verify_kits.py`,
  `verify_campaign.py` on the Mac. Core tests pass here (972 new feel checks).
- Goal 2 (the Angel AI rework, brief `reports/demo/goal-2-angel-ai-brief.md`) is done: see the section above.

## Follow-up 2: Set 2 VFX / UI / transitions in game, ads on the LED pillars, music -60% (2026-10-09, branch `claude/chapter-maps`)
Newest work. **Read `reports/demo/2026-10-09-set2-vfx-led-ads.md`.** 240 motion pieces; ability / status / angel / shared VFX follow
their character (`BIBMotion` Follow, `BIBMotionEvents` Ability / Shared / SyncEnemy); the six ads run on the Ark's 40 LED ad pillars
(shared players); pack audio and arcade music at 40%. Checks: `verify_motion`, `verify_perf` (new), `verify_combat`, `verify_kits` PASS.

## Follow-up: the 13th frame, the bb-Showreel motion pack in game, iOS (2026-10-09, branch `claude/chapter-maps`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-09-13th-frame-motion-pack.md`.**
- 13th frame: action moments insert an extra fresh frame between the 12 fps steps; heavy moments draw it as a two-tone impact frame
  (`ABIBAnimeStyleVolume::RequestAccent`, `ImpactFrame` in the shader, `bib.ImpactFrames`, off with reduce motion).
- Motion pack: all 54 animated pieces of `~/Downloads/motion-studio/videos/bb-Showreel` play in game, each at its own moment
  (`BIBGame/BIBMotion.*`, `BIBMotionEvents.*`); 16 Ark video screens (ads over their stations, trailer, reels; `BIBVideoScreen.*`);
  the trailer is the startup movie. Generated content is git-ignored: on a new machine run `Tools/import_motion_pack.sh`
  (needs npx hyperframes, ffmpeg, numpy - the motion-studio `.venv`), then `Tools/build_assets.sh` as usual. Check: `Tools/verify_motion.py`.
- iOS: the game compiles and links for iOS; signing needs the owner's Apple team (`BIB_IOS_TEAM=<id> Tools/package_ios.sh`). Android SDK missing.
- Size warning: ~1 GB of motion atlases on desktop (~250 MB on phones); see the report for how to shrink.

## Dev notes pass: Zenless-style combat, adaptive angels, BB UI, dreamcore, Ticket road, traversal, mobile (2026-10-09, branch `claude/chapter-maps`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-09-dev-notes-zzz-pass.md`** and the MAIN entry "Dev notes pass". Owner's Notion "Dev notes"
page + chat: make it feel and look like Zenless Zone Zero (Anime Zero for the attack loop), full creative control incl. movesets.
- Combat feel: `BIBGame/BIBPlayerFeel.cpp` (buffer, strings, cancels, dash attack, perfect dodge, chain attack, hit stop) on
  `BIBRules/BIBStagger.*` (Daze / stun / flinch / combo, tested). FX: `BIBGame/BIBFx.*` + `M_BIBFx`.
- Angels: `BIBRules/BIBAngelMind.*` (shared adaptive mind, tested) driving `BIBEnemy.cpp`; console `BIBAngels 0..1`.
- UI: battle HUD rebuilt in `BIBArkUI.cpp` (Zenless layout, trailer style, fonts `Content/BIBFonts` via `BIBFonts.h`).
- Look: dreamcore layer in `Tools/build_assets.py` (M_BIBAnimeStyle `Dream*`; rebuild with `Tools/build_assets.sh`).
- Tickets: Ticket road (`BIBEvents`, Lobby > Events), bot proves ~17.6 months to all 6 Secrets. Inventory cap 1,500 (save bug).
- Traversal: lower jump, climb, swim, hard landings. Mobile: `BIBGame/BIBTouch.*`, `-BIBTouch`, iOS / Android ini.
- Checks: `Tools/verify_combat.py` (new, PASS), `Tools/ui_shots.py` (screenshots in `Saved/Combat`), campaign / kits / Ark PASS.
- **Blocked / next:** iOS needs the UE iOS component + Apple signing; Android needs SDK / NDK; owner play-through (feel is
  tuned by scripts, not by a person); optional: re-theme map materials, redesign PROPOSED kits, 12 fps stepping vs input feel.

## Balancing, playtesting and bug fixing (2026-10-09, branch `claude/chapter-maps`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-09-balancing-playtest-bugfix.md`** and the MAIN entry "balancing, playtesting and bug fixing".
- Owner: "hours and hours of balancing playtesting and bug fixing". Every number changed is PLACEHOLDER; no owner rule touched.
- Campaign: the bot's long tails were a bot bug (fixed); model plans stars for an unlucky player; boss levels trimmed to their threat
  target; Chapter 1 eases in (7 replays) and day-1 login Charge is 120, so Chapter 1 is done on day 1; regular player 21 days.
- Economy holes closed: Ark Tower floors ramp to campaign hardness; Trade Centre received tickets x25 / arcade tokens x30; Boss Rush
  Diamonds + tickets only on a new best round. Tests added for each.
- Bugs: `verify_kits.py` harness (now 124 OK / 0 flagged); UE 5.8 TEDS editor crash on travelling into the Ark -> `TEDS.Enable=0` in
  `Config/DefaultEngine.ini` (editor-only). Scripted runs: never pass `-log` on the Mac (the editor hangs on exit).
- **Follow-up (same day):** Chapter 11 is now the owner's walk (one entry, `L_Ch11_ReturnToNowhere`, exit clears 81-88; `verify_walk.py`);
  Alchemist: switch-out no longer wipes her infection and benched kits tick it (the infect-mutate-swap play works), her own faster
  infection profile, Transmutation x1.3, HP 5200 / attack 1300 -> power 1.00; Gambler / Media attack nudged; armour upgrades 500 Bytes/step.
- **Next:** owner play-through; OPEN: drops make the Armoury irrelevant after Chapter 2 (owner call).

## Ark Rally: a clean-room Big Bang Racing-style arcade racer (2026-10-15, branch `claude/funny-keller-yka6n1`, cloud container)
Newest work. **Read `reports/demo/2026-10-15-ark-rally.md`** and the MAIN entry "Ark Rally".
- Owner asked for a copyright-safe "same working and feeling" version of Traplight's Big Bang Racing as a Boldinbeauty arcade game.
  Built clean-room (no app download / decompile / assets): `BIBRules/BIBRally.*` physics trial bike, track codes (player-made tracks),
  three built-in tracks, three rival ghosts, hub page `arcade-rally`, Slate overlay, icon. Everything PLACEHOLDER / PROPOSED.
- Checks: `Tests/run_core_tests.sh` all suites 0 failures. **Not compiled in Unreal, not played.** **Next (owner's Mac):** build the
  editor target, fix any compile error in `BIBArcadeOverlay.cpp`, draw and import the icon (`python3 Tools/make_icons.py --only rally`,
  `Tools/import_art.py`; the PNG could not be pushed from the cloud), run
  `verify_arcade.py -BIBDev`, then play a few races and tune the feel (numbers at the top of `BIBRally.cpp`).
- **Follow-up (2026-10-15, owner feedback):** flips fill a nitro meter (Space / Shift / NITRO button); a second vehicle, the buggy
  (slower, steady, hard to flip, very hard to crash; `RallyVehicleSpec()`); a far less punishing dirt bike (crash only when really
  upside down, balancing wheelies, landing assist, respawn on the last safe ground with speed); Night Shift removed from the arcade
  (code kept for Ultimate Custom Night). Same caveat: not compiled in Unreal or played.

## Elite_Bundle dressing: chapter maps and the Ark (2026-10-08, branch `claude/chapter-maps`, owner's Mac)
**Read `reports/demo/2026-10-08-elite-bundle-dressing.md`** and the MAIN entry "Elite_Bundle dressing".
- Owner: fill the 13 chapter maps and the Ark with the `Elite_Bundle` assets. On a new machine run
  `Tools/import_elite_bundle.sh` once (copies it to `Content/Elite_Bundle`, git-ignored), then `Tools/build_greybox.sh`.
- `Tools/greybox_bundle.py` (last pass of every map build) places 600-3,000 themed bundle pieces per map on real floors, never in
  walls / cover / water, fight centres and station rings clear; missing bundle materials get palette colours. Themes PROPOSED.
- Playtest app: `~/Documents/BoldInBeauty-Builds/2026-10-08-playtest/BoldInBeautyDemo-Mac/BoldInBeautyDemo.app`; GitHub `main` = this build.
- Checks: build OK, `verify_chapters` 218 PASS, `verify_ark` 133 OK. **Next:** owner walks a few maps; measure frame cost
  (`perf_capture.py`); instance or thin out if a slower PC needs it.

## Performance pass and graphics settings (2026-10-08, branch `claude/chapter-maps`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-08-performance-graphics-settings.md`** and the MAIN entry "performance pass and graphics settings".
- **3 fps** = the editor's "Use Less CPU when in Background" throttle (exactly 3 fps when the editor is not the focused app);
  the game module now switches it off while a play session runs. Whether that was the owner's case is OPEN (ask which machine).
- **Held anime frames are not rendered** (12 renders a second instead of 60; `bib.Gfx.SkipHeldFrames 0` = old). PIE: Ark 58 ->
  119 fps, Neo Tokyo storm 47 -> 119 (editor cap), every held frame skipped, `verify_anime_style` PASS 62.
- **Settings > Graphics:** Performance mode, Auto-detect, Quality, render scale 30-100 %, dynamic resolution, ten per-group
  levels, weather effects Off / Low / Full, volumetric fog, anime look; saved per profile. All option values PROPOSED.
- **Profiling:** `Tools/perf_capture.py` (+ `-BIBFreeRoam` to walk any greybox map). **Next:** owner plays once with
  Settings > Graphics > Show frame rate on the machine that was slow; instance the greybox maps if a weaker PC needs it.

## The Ark is the hub, saved position, no white glare, second detail pass (2026-10-07/08, branch `claude/chapter-maps`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-07-ark-hub-saved-position-look-detail.md`** and the MAIN entry "the Ark is the hub".
- **The 2D main screen / lobby is deleted** (`UBIBHubWidget`, `WBP_Hub`, the Slate hub, `-BIBClassicHub`). Play boots `L_Hub`, which
  travels straight to `L_Ark`; the sign-in screen sits over the living Ark (circling camera). In the Ark only a three-line menu
  button shows: a flow-chart menu (M / Esc / P) whose leaves teleport to real places (factory = summon, armoury, task room =
  campaign, Merchant Row stalls, arcade, training grounds ...) or open a personal page; F / T at a station's ring opens it; battles
  show only the fight HUD. This integrates the interrupted `claude/ark-world-ui` work (worktree `worktrees/ai-agent-ark`, left as is).
- **Saved position:** `FMeta::ArkPos/ArkYaw` (`arkPosition` in the save); kept while walking, written on leaving; every Ark load
  arrives at an explicit station trip, else the saved spot, else the plaza. Survives a restart (`verify_ark_save.py`, two runs).
- **Look:** highlights soft-clipped with their colour kept (`WhiteKnee` / `WhiteCeiling` / `BrightSaturation` in `MI_BIBAnimeStyle`),
  weaker bloom, wet streets a sheen not a sky mirror, smaller overcast exposure lift: 0.0% white pixels in all nine weather shots (was
  up to 11%). Object boil from world position (`ObjectShake`), form and contact shading; camera shake stays off.
- **Greybox:** `Tools/greybox_detail2.py` dresses every combat section (cover, jump-up + climb platforms, props, lights), routes
  between sections (markers, lookout towers), Neo Tokyo (fire escapes, balconies, neon, street props) and the Ark; Chapter 11 empty.
- **Verify scripts:** old hub scripts now use `Tools/bib_hub.py` in the Ark; `verify_hub/game/flow.py` removed. Results in the report
  (all PASS except one flaky campaign spawn count, rerun PASS). `build_assets.sh` no longer fakes success from a worktree.
- **Next:** owner play-through and look review (all placements and numbers PROPOSED); `verify_accounts` / `verify_kits` re-run;
  performance pass; join chapter maps to stages; merge the branch chain when the owner is happy.

## Chapter maps 1-13: owner level ideas, FINAL (2026-10-07, branch `claude/chapter-maps`, owner's Mac)
**Owner decision, final:** each chapter has its own unique map; Chapter 1 is Neo Tokyo. Follow the BB style (the anime look,
`ABIBAnimeStyleVolume`). Make every map unique and vibrant, dynamic to play: areas the player can **jump up to and climb**,
places to **swim**, and ways to **move around levels and between sections**. Start with greyboxing only. The owner's ideas
below are the creative brief and are final; the greybox layouts built from them are the assistant's reading (PROPOSED). Detail
and layout notes: `docs/chapter-maps.md`. In-game texts are kept exactly as the owner wrote them.
1. **Neo Tokyo** - the existing `L_NeoTokyo` (many buildings and streets, heavy rain and fog, climbing buildings).
2. **The Ruins** - based on the Outer World, on top of the arc.
3. **Docks** - a semi-ruined area near the sea, waves from the sea crashing, heavy fog; the only structure is a lighthouse.
4. **The Hanging City** - a city hanging above an endless void, seemingly held up by a single massive chain that ascends into
   the sky; the top cannot be seen.
5. **A.R.C TECH INCORPORATED** - a massive factory, fully functional, seemingly with no damage, running fully automatically
   with no visible workers at all; it seems to be making tiny cubes that glow insanely bright yellow.
6. **The Moon Kingdom** - seemingly a second arc, built out like an actual liveable arc with all the structures you would
   expect, yet not a single living creature, everything covered in flora, and a massive Moon on the inside that casts a deep
   darkness over the entire "Arc".
7. **The Old Man's Battings Site, or T.O.M.B.S** - a massive structure that seems larger than any man-made structure; inside it
   just looks like a regular office, yet it holds many "Angel"-based secrets.
8. **Intersection** - a simple tunnel drenched in rain and seemingly flooding, brimming with angels everywhere, as it is the
   main transport path the angels take from their assumed headquarters.
9. **Remnant Remains** - an empty desert with violent winds and constant downpour; somehow in the middle is a massive data
   centre / electrical cyber structure (assistant's choice: a data centre) hosting a small screen that states (verbatim):
   `Angel//V1 Success Line down Operator//V! Faillllllllllllllllll ...---... ////// .- -.--. . .-.. ... / .-- .. .-.. .-.. / -.- .. .-.. .-.. / ..- ... / .- .-.. .-..`
10. **Hallucinations** - the entire chapter is a hallucination: a nonsensical dreamscape with a pastel-colour drawing look,
    juxtaposed by hyper-realistic terrifying things randomly appearing, and Morse code messages telling a small story:
    "The angels bring mercy for all Not those stupid pure ones you call angels but TRUE ANGELS THE ONE The rest are all filth
    compared to its existence"
11. **Return to Nowhere** - a place simply known as nowhere: a mashup of all 10 past maps seemingly pasted together. Its own
    theme: pure grey, no post-processing, nothing in it, no texture, just pure grey and white noise. No levels, no bosses,
    nothing: you just walk through the single level (about 5 minutes long) that is the map and walk out; nothing happens, but
    it should feel horrifying the whole time.
12. **Paradise** - a beautiful neo-tropical city, not ruined, bright vibrant colours, constant good vibes; quite heavy rainfall.
13. **A Secretive Soul** - a small Japanese-looking town like one in the countryside, consistent low to medium rain; in the
    middle is a monolith, pure black with red hieroglyphics in an undecipherable language; when you get close the voice of the
    angel god says (verbatim): "They wree purity, Impure sin, Thats all"

**Built (2026-10-07, MACOS-LOCAL):** `/Game/Greybox/Maps/L_Ch02_Ruins` ... `L_Ch13_SecretiveSoul` from `Tools/greybox_chapters.py`
(`build_greybox.sh`, `BIB_GREYBOX_ONLY=chapters`), each with four sections (boss in 4; none in Chapter 11), climb (orange), jump-up
(teal), swim (water volumes) and moving pieces (`ABIBGreyboxMover`, new C++ actor). After the owner's follow-up ("more fleshed
out", "8-12k minimum", Neo Tokyo too), a detail pass (`greybox_detail.py` + `greybox_props.py`) brings every map to
8,675-9,943 pieces and Neo Tokyo to 15,687. Check: `Tools/verify_chapters.py`. Report: `reports/demo/2026-10-07-chapter-maps-greybox.md`.
Not walked by a person; Play is still the free-fly pawn.

## Day / night + weather, Neo Tokyo under the levels (2026-10-07, branch `claude/weather-tod`, owner's Mac)
Newest work; on `claude/anime-style-build` @ `d140278`. **Read `reports/demo/2026-10-07-weather-day-night-neotokyo.md`** and the MAIN
entry "day / night + weather system".
- **Campaign tab > Neo Tokyo:** free roam of `L_NeoTokyo` with the demo game mode and the team (needs `Tools/build_greybox.sh` on the machine).
- **`ABIBWeatherSystem`** (`BIBGame/BIBWeather.*`) in every 3D level: 24-minute day (1 min = 1 game hour), dawn / day / dusk / night,
  stylised sky (stars, moon, Milky Way, cel clouds), vivid grade; light / medium / heavy rain, thunderstorms, light / medium / heavy
  wind; rain stops under roofs; splashes, wet / glossy surfaces and clothes, drips, puddles that last for hours; synthesised audio.
  Assets: `Tools/build_weather.sh` -> `/Game/Weather`. Console `bib.Weather.Hour/Rain/Thunder/Wind/Auto/TimeScale/Strike/Clock/Report`.
- **Verified (Mac):** build 0 / 0, core tests green, `verify_weather.py` 45/45, `verify_hub` 74/74. Not played by a person; frame cost not measured.
- **Next:** owner look review (all numbers PROPOSED; palette in `BIBWeather.cpp`, shaders in `build_weather.py`); performance pass when asked.

## Anime style post-process volume (2026-10-07, branch `claude/greybox-maps`, owner's Mac)
Newest work. **Read `reports/demo/2026-10-07-anime-style-volume.md`** and the MAIN entry "anime style post-process volume".
- `ABIBAnimeStyleVolume` (`BIBGame/BIBAnimeStyle.*`) + `MI_BIBAnimeStyle` (from `build_assets.py`): thick ink lines, cel bands, cool
  shadows; the finished 3D image is held and refreshed **12 times a second** while the game runs at 60. VFX, world text and damage
  numbers are included; HUD / menus stay smooth. In every gameplay map (Chapter 1 stages, Neo Tokyo, Ark), not in `L_Hub`; the game
  mode turns it on for code-built stages in `L_Hub` and off on the menus. `bib.VisualStepRate <n>` overrides (0 = smooth).
- **Verified (Mac):** build 0 / 0, `verify_anime_style.py` PASS (12.0 fresh frames/s at 60 fps), `verify_hub` 74/74. Not played by a person.
- **Update (branch `claude/anime-style-build`):** heavier look (3 hard bands, hatching, lavender shadows, coloured ink, grain, gentle
  line boil); owner asked to drop the shake: `FrameJitter` 0. **Mac build:** `Tools/package_mac.sh` -> `~/Documents/BoldInBeauty-Builds/2026-10-07-anime-style/`
  (hub, Neo Tokyo, Ark start; unsigned). Do not use `BuildCookRun -archive` on 5.8.3 (broken .app, see the report).
- **Next:** owner look review; tune `MI_BIBAnimeStyle` parameters to taste (all numbers PROPOSED).

## Greybox maps: Neo Tokyo and the Ark (2026-10-07, branch `claude/greybox-maps`, owner's Mac)
Newest work; based on `claude/gacha-elements-events`. **Read `reports/demo/2026-10-07-greybox-neotokyo-ark.md`** and the MAIN entry
"greybox maps". Two maps from the owner's Fab kits (SCI-FI BUILDINGS CITY KIT 1 + 2): `/Game/Greybox/Maps/L_NeoTokyo` and
`/Game/Greybox/Maps/L_Ark` (operators' dome, catwalks over the hole, town square + Underworld, civilian area, secret dome).
- **Rebuild:** `Demo/BoldInBeautyDemo/Tools/build_greybox.sh` (imports the kits from the Fab vault cache once, then builds both
  maps in about 30 s). `Content/Greybox` is git-ignored (230 MB); every map is regenerated, so edit the scripts, not the maps.
- **Check:** `UnrealEditor <uproject> -ExecCmds="py <abs path>/Tools/verify_greybox.py"` -> `Saved/VerifyGreybox.log`, screenshots in
  `Saved/Greybox/`. RESULT PASS on 2026-10-07. Not walked by a person; Play gives a free-fly pawn (no climbing character yet).
- **Next:** owner walk-through; owner decisions in the report (climbing character, rain FX, connections, LFS, sublevels).

## Classic gacha rebalance, elements, inventory and boosts, movesets, events, mini-games (2026-10-07, branch `claude/gacha-elements-events`, owner's Mac)
Newest work; based on `claude/arcade-gangs-arena` and merges `claude/movesets` + `claude/minigames`. **Read
`reports/demo/2026-10-07-gacha-rebalance-elements-inventory-events.md`** and the MAIN entry "eighth iteration".
- **Rebalance:** owner summon rules untouched; free income ~55 pulls a month (~34 for featured banners); **Charge** stamina gates
  campaign replays (first clears free) with **Quick clear** sweeps, so every daily play style needs ~3-4 weeks for 13 chapters
  (`Config::UseCharge`); Outer World on the real clock with a campaign bonus; Monthly supply (simulated).
- **Elements** (`BIBRules/BIBElement.*`): Fire, Water, Nature, Dark, Magic + PROPOSED Volt, Halo, Glitch; NIKKE-style +15% on
  advantage, Attunement 0-10; enemy element at spawn in Unreal (`ABIBEnemy::SetElement`, label `[Fire] ...`).
- **Inventory** (`BIBInventory.*`, Lobby > Inventory): 3/6/12/24 h Gold / Bytes / Angel-heart boosts that pay Outer World hours;
  Charge, Element cores, Event tokens, Ark medals; Charge packs.
- **Events** (`BIBEvents.*`, Lobby > Events): Rookie week, weekly operation (Angel Hunt / Arcade Fever) + event shop, Overtime
  weekend, Element Surge, Ark shop. **Arcade:** Halo Fall, Neon Memory, Firewall Breach (`BIBMiniGames.*`).
- **Movesets:** the 14 assistant-designed kits reworked (all PROPOSED) and balanced to rarity bands (`docs/operator-kits.md`).
- **Verified (Mac, 2026-10-07):** core tests all green (230 new + 59 mini-game checks), editor build 0 / 0, PIE `verify_gacha`
  16/16, `verify_hub` 74/74, `verify_arcade` 46/46, `verify_tester` 32/32; `verify_kits` 155 OK / 19 flagged (sweep artefacts
  plus one Narrator basic-attack flag, see the report). **Not played by a person.**
- **Owner approved (2026-10-07):** Charge stays; Volt / Halo / Glitch and all element assignments; the 3-4 week pace; the kit
  changes; the event and arcade game designs. Amounts and prices remain PLACEHOLDER.
- **Next:** owner play-through; fix the kit-sweep harness (respawn enemies before "form 2" casts) and re-check Narrator's basic
  attack; then merge the branch chain (rebalance -> arcade -> gacha) when she is happy.

## Arcade, the Ark, Outer World, gangs, friends, notices, live ops (2026-10-07, branch `claude/arcade-gangs-arena`, owner's Mac)
Newest work; based on `claude/rebalance-quests-ui`. **Read `reports/demo/2026-10-07-arcade-ark-gangs-liveops.md`** and the MAIN entry "seventh iteration".
- **Arcade** (Lobby > Arcade): Night Shift (the owner's FNAF-style night, `BIBRules/BIBArcade.*`, drawn by `BIBGame/BIBArcadeOverlay.*`), Boss Rush (real 3D combat, boss after boss), Rhythm (5-lane Guitar Hero style). **The Ark:** endless tower with Operator (3-4★) and Secrets (5-6★) modes, never resets, saves the floor you fell on; semi-online arena (attack / defence teams, AI vs AI replay, trophies, Bronze..Platinum 1-3 + Champions, monthly season drop). Battle power on operators. Outer World built with Gold + Bytes. Friends, gangs (30, Gang coins, shop), admin mail to one / all, notices, returning players, monthly season pass (~15 days), top-bar currency choice, key bindings, damage numbers, camera shake.
- **Verified (Mac, 2026-10-07):** core tests all green (440 new checks), server 118/118, editor build 0 / 0, PIE `verify_hub` 74/74, `verify_arcade` 30/30, `verify_tester` 32/32. **Not played by a person; online pages not tried against the live server** (restart the Mac server from this branch's `Server/` to get the new routes; `render-deploy` lacks them).
- **Next:** owner play-through of the three arcade games, the tower and the arena; owner decisions on every PLACEHOLDER number in the report; merge the branch chain (rebalance -> arcade) when she is happy.

## Rebalance, quests, full-screen pages, settings (2026-10-06, branch `claude/rebalance-quests-ui`, owner's Mac)
Based on `claude/optimistic-tesla-g2pv4m`. **Read `reports/demo/2026-10-06-rebalance-quests-fullscreen.md`.** Owner numbers: new
accounts start with 300 Diamonds only; level 1 pays 800 Gold / 250 Bytes / 2 hearts per clear and 50 Diamonds on the first clear;
chapter-end quests pay 10 Regular tickets (Chapter 1) then 1; a quest every 2-3 levels (20-50 Diamonds or loot); daily + weekly
quests; heavier per-level growth (+6%, enemies follow via `Tests/campaign_model.h`, Chapter 1 included); full-screen sub-pages;
0.3 s page transition; Settings with five working sections. Verified on the Mac: all core tests, editor build, `verify_hub` 72/72.
Everything except the owner's numbers is PLACEHOLDER. `verify_tester` 32/32 (guest). Not re-run on this branch: `verify_accounts`, `verify_campaign`.

## Tester build, whole-game playthroughs (2026-10-05, branch `claude/optimistic-tesla-g2pv4m`, cloud session)
Based on `claude/zealous-brahmagupta-24nfg9` (everything above is in it). **Read `reports/demo/2026-10-05-tester-panel-playthroughs-windows.md`** (its Windows part is out of scope: the owner chose to skip the Windows build).
- **Tester panel:** every player (guest, signed in, admin, `-BIBDev`) gets **Lobby > Tester panel** (`BIBRules/BIBTester.*`, pages in `BIBPages.cpp`, switch `Config::TesterMode`): every currency (add / remove / set, wallet presets), operator unlocks / levels / stars / duplicates / gates, campaign progress, tower, sub-stories, gear, XP, day, Outer World, summon pity, five presets, **Skip tutorial** on every tutorial page. The admin account keeps its unlimited wallet, every operator and every level. 4,189 headless checks (`Tests/tester_tests.cpp`). `Tools/verify_tester.py` (run with `-BIBGuest`) is the editor-side walk and **has never run**.
- **Playthrough bot:** `Tests/playthrough.cpp` (also in `run_core_tests.sh`; `--stages` prints the grind per stage). 300 runs: every run finishes the 104 levels, no soft-lock. A regular 120 min/day player needs about 9 days. Found and fixed: Angel hearts were paid 6x too richly and the Trade Centre made the surplus worth millions of Gold; now `5 + level` per clear (PLACEHOLDER). The 14 PROPOSED kits have big relative outliers (report): owner decides.
- **Next, in order:** (1) Done 2026-10-05 on the Mac: editor build clean, `verify_hub.py` 53/53, `verify_tester.py` 32/32, `verify_accounts.py` 35 OK (see the report). (2) Owner decisions listed in the report (hearts rates, kit outliers, Kimi's work, whether TesterMode stays on for the published build, hosting).

## Accounts, saving, tutorial (2026-10-04, branch `claude/festive-thompson-6e11vf`)
Sign-in screen first (create account / log in by e-mailed code, or a local guest); saves per account in `Saved/BIB/Saves/<user id>.json` plus a cloud copy; new accounts own only Luna and play a 9-step tutorial (Kuu forced summon, level 1, Plack free); the Featured banners rotate every 3 days (UTC; pools are PLACEHOLDER); the owner e-mail is the server-flagged admin (all operators, unlimited currencies, all levels). **The verify scripts need `-BIBDev`** (the old developer profile). The account server is `Demo/BoldInBeautyDemo/Server` (Node + MongoDB; its README has the setup). Read `docs/accounts-and-saving.md` (design, launch options, OPEN decisions) and `reports/demo/2026-10-04-accounts-saves-tutorial.md`. Mac-verified: build clean, `verify_hub` 53/53 (`-BIBDev`), `verify_accounts` 34 OK.

**Account server status (2026-10-05, owner's Mac, SNAPSHOT):** the owner reports that **the server works** and that she finished the guided setup. What an AI could check (Mac helper session, metadata only): `GET http://127.0.0.1:8080/health` returns `{"ok":true}`; one `node --env-file=.env src/index.js` (pid 6087, started 2026-10-05 11:38 AEST from an interactive zsh, so by hand, not by the setup script) listens on 8080 with its working directory in the Mac worktree `<local-home>/Documents/BIB-macfix-wt/Demo/BoldInBeautyDemo/Server`; `npm ci && npm test` there pass 84/84 at fc6ed58 (Node 26.10.0, Atlas CLI 1.56.0). **Caution, do not over-read this:** `Server/.env` there is a symlink to `<local-home>/.config/boldinbeauty/mongodb.env` (link made 2026-10-04 21:51 AEST, before this session's setup run), and `Server/.env.bak` does not exist. `npm run setup` copies an existing `.env` to `.env.bak` in its last step, so that step did not run in this folder: the running server most likely uses an older env file, not one written by the new guided setup (an inference, not a fact). `/health` never touches the database, so nothing an AI saw proves that Atlas answers, that codes are e-mailed (the older file may use `MAIL_PROVIDER=console`, which prints codes in the server window) or that the admin sign-up (user id 10000001, `isAdmin`) worked; those rest on the owner's report. No AI has read `.env`, the target file or the Terminal (the helper sessions' safety checks block it and it must stay that way; never ask for the connection string or App password). The mode of the target file is unknown (the `755` the helper first reported was the symlink's own mode and means nothing); the owner can check with `ls -l ~/.config/boldinbeauty/mongodb.env` and run `chmod 600` on it. Two Google accounts are in play: the Atlas/MongoDB login is `redacted@example.com`; the sender mailbox, the admin e-mail and the in-game sign-up are `admin@example.com`. Start the server another day with `cd Demo/BoldInBeautyDemo/Server && npm run start:env` (keep the window open; the game reaches it at `http://127.0.0.1:8080` unless `-BIBApi=<url>` or `ApiBaseUrl` says otherwise). The owner has a private step-by-step page for the setup. **Open:** confirm with the owner which env file the server uses and whether codes arrive by e-mail, and re-run `npm run setup` if she wants the guided result (it is safe to repeat; nothing is written until its last step); an optional public host (today the server runs only while a Mac window is open)

## 13-chapter campaign (2026-10-05, branch `claude/zealous-brahmagupta-24nfg9`, based on `claude/festive-thompson-6e11vf`)
The owner asked for everything in this file plus levels for the other 12 chapters: standard enemies (basic melee, ranged, mage = area damage, brute = tank, assassin = high damage / close range / low health, mini boss, boss), **a mini boss at the end of every level, and in a chapter's last level the mini boss in the section before the boss**, balanced to the currency, levels and operators of that point of the game and a bit harder so some grinding is needed. Testing hint: the admin Gmail `admin@example.com` has everything maxed and every stage open; the guest-account button is a base account with no login.
- **Read `docs/campaign.md`** (generated: archetype table, the reference-player assumptions, every level's sections, mix, recommended team level, enemy scales, threat vs target) and `BoldInBeauty_MAIN` ("Unreal demo — 13-chapter campaign ... fifth iteration"). Code: `BIBRules/BIBCampaign.*` (archetypes, plan generator, rewards), `BIBRules/BIBCampaignData.inc` (GENERATED tables), `Tests/campaign_model.h` (balance model, not shipped), `Tools/gen_campaign_data.py`, `Tests/campaign_tests.cpp` (4,047 checks, also fails when the tables are stale), `Tools/verify_campaign.py` (PIE). Unreal: `BIBEnemy` (Ranged / Mage / Assassin), `BIBGameMode`, `BIBLevelActors`, the Campaign page in `BIBPages.cpp`.
- **Change a number?** edit the constant (reference player in `Tests/campaign_model.h`, rewards and archetypes in `BIBCampaign.*`), `python3 Tools/gen_campaign_data.py`, `Tests/run_core_tests.sh`, then the Mac jobs. Everything numeric is PLACEHOLDER.
- **Decisions the assistant made (reversible, OPEN):** 8 levels per chapter; one monotone reward curve for all stages (**Chapter 1's old 5,000 x level Gold fell**, it had made Chapter 1 the best farm); Diamonds and the ticket **first clear only**; the new enemies' numbers, behaviours and labels; chapter mix themes; only the Chapter 1 boss gives an operator. Chapter 1's plans and enemy numbers are untouched (level 8 keeps mini boss and boss in one section).
- **Never play-tested.** Unit tests, the balance model and the Mac compile + PIE runs (2026-10-05: editor build clean, `verify_hub` 53/53, `verify_campaign` 128/128 and 65/65) show the structure works; whether fights feel right is the owner's call. Stage ids are 1..104 (stage id = cleared-levels count), `BIBSetCampaignProgress <n>` takes 0..104, verify scripts still need `-BIBDev`.
- **Fixed on the way:** `Tools/gen_tags.py` and `gen_kit_docs.py` had been broken since the accounts iteration (missing `BIBTutorial`).

## Best next step (about $20 of AI budget left; chosen 2026-10-05)
**Put the demo in other people's hands: a hosted account server plus a standalone macOS build, after one owner play-through.** (Prepared 2026-10-05: `docs/hosting.md` and a Render blueprint `render.yaml`; nothing is deployed because every host step needs the owner's logins; see the packaging status in MAIN / the report.) Why this one: the owner-specified work is built and checked (rules core, hub, 20 kits, accounts, saves, tutorial, rotation), the account server now works on the owner's Mac, and the only things that stop anyone else from playing are that the server lives in a Terminal window on her Mac and that the game is only run from the Unreal editor. Everything else open needs an owner decision first (rotation pools, Plack twice, demo tools, starting wallet, kit numbers, chapters 2-13), so spending credit on it would mean inventing design, which is not allowed. Rough size, an estimate and not measured: a few dollars of AI time for the whole path, which leaves most of the budget in reserve; hosting must stay on free tiers unless the owner approves a price.
1. **Owner play-through (no cost, about 30 minutes).** Sign up with the admin e-mail, check username / e-mail / user id and the admin extras; sign out and back in (code by e-mail); make a second account with another e-mail and play the 9-step tutorial, quitting half-way to see it resume; summon, level up, clear 1-1 and check Plack; close and reopen the game to check the save. Write what feels wrong into a short list. Only a person can do this; the Mac `verify_accounts.py` covers the mechanics (34 checks) but not the feel.
2. **Host the server** (Atlas free tier stays; host free tier). The `Dockerfile` and `Server/README.md` ("a host") already cover it. Decisions for the owner: which host (Render, Fly.io, Railway or Cloud Run) and whether a card is needed; Atlas > Network Access must allow the host (most free hosts have no fixed address, so this means `0.0.0.0/0`, which is a trade-off for the owner to accept; it is less risky here because the database user can touch only the `boldinbeauty` database and has a random password); set `TRUST_PROXY_HOPS=1` behind the host's proxy so the per-address rate limits see the player and not the proxy; copy the variables from `.env.example`, never the `.env` file itself; check `https://<host>/health`. The game warns in the log when the address is `http://` and not local.
3. **Standalone macOS build** (Mac helper session, UE 5.8.3, `-NoHotReloadFromIDE`): set `[BoldInBeauty] ApiBaseUrl=https://<host>` in `Config/DefaultGame.ini` (or ship with `-BIBApi=`), cook and package (the full cook already passes: 637 packages, 0 errors), run the packaged app once through sign-up, tutorial 1-1 and a restart. Code-sign or notarize only if the owner wants to hand the app to others; unsigned apps need a right-click Open.
Not worth the credit now: tuning kit numbers, new operators, or in-app purchases (they stay simulated), because each waits on the owner. (Chapters 2-13 were designed on 2026-10-05 at the owner's explicit request: see the campaign section above; the owner still decides the real numbers, level counts, names and story.)

## Quick start
`Tests/run_core_tests.sh` runs anywhere. On the Mac: `Tools/build_assets.sh`, then `verify_assets.py`, `verify_ark.py` (the hub since 2026-10-08), `verify_gacha.py` and `verify_weather.py` via `UnrealEditor <uproject> -ExecCmds="py <script>"`. The demo `README.md` has exact commands, controls and the asset table. Console test hooks: `BIBFillUlt`, `BIBDamageEnemies`, `BIBSetNarratorSpeed`, `BIBSetCampaignProgress`, `BIBTeam <A> <B> <C>` (hub), `BIBGrantAll`, `BIBUltVariant <n>`, `BIBTags`, `BIBCheckTags`. After changing kit data: `Tests/run_core_tests.sh`, `python3 Tools/gen_tags.py`, `python3 Tools/gen_kit_docs.py`.

## Owner decisions
Move layout follows MAIN. Ultimate charge comes from damage dealt. Featured banners: Judge 5★, Narrator 6★. Tab switches operators; ultimate keys E (R for Judge). Narrator walks faster with speed and is pushed forward at (speed − base); base is read as her starting speed (25, Diano 125), unconfirmed. In-app purchases are **simulated**; never add real payment.

## Verified on the owner's Mac (UE 5.8, 2026-10-04)
Editor build clean; `verify_assets.py` OK; `verify_hub.py` 53/53 in PIE (all tabs, 18 sub-pages, level lock, level 8 boss and Plack); `verify_game.py` passes standalone; **0 missing textures**; full cook (637 packages, 0 errors) includes all art. The 20 real portraits come from the owner's xlsx (`make_portraits.py --xlsx`). Ten screenshots: `reports/demo/screenshots/2026-10-04_hub_*`.

**GAS layer (2026-10-04, Mac, UE 5.8.3):** compiled on the first pass (0 errors, 0 warnings); PIE `verify_hub.py` 53/53, every gameplay tag registered, abilities granted, cooldowns tick as GameplayEffects, no crash on Luna, Judge, Narrator or Blood/Death/Gambler key presses. Sweep of all 20 operators against live enemies (`Tools/verify_kits.py`, 919400f): **PASS 126 ok, 0 flagged**, no error / ensure / crash lines, no ability that does nothing. Self-buff ultimates and passives only passed on a message (buffs are now mirrored as `BIB.Buff.*` tags and verified on the Mac); Adapt's AK lands all 10 shots in 1 s but each did about 1,050-1,150 in play because his drifting damage stat sat at its placeholder floor of 0.5 (rules test: 2000 each at 1.0); the drift bounds (0.5-2.5) are the owner's to set. Mac Job 3 (8480860): buff tags verified, Narrator swap was a test artefact. Run it with `UnrealEditor <uproject> -ExecCmds="py <path>/Tools/verify_kits.py"` (`BIB_KITS_ONLY="Luna,Arita"` limits it, about 20-25 min for all).

## Open
**Owner questions on the kits:** see `BoldInBeauty_MAIN` ("Unreal demo — Gameplay Ability System and 20 operator kits"): still open: the first infection goo tick (read as 50 s), the soil puddle's per-enemy roll, Adapt's passives and M1, every number of the 14 PROPOSED kits. Answered 2026-10-04: 3-star Ultimates kept, Arita's Full takeover is a takeover, Adapt owns "Why do i bother" (Adapt's Ultimate), Ring leader is a Special, Transmutation 40 s. Balance is a relative simulation only; the only tuning is one pass on Stargazer and Weather (scaled down about 25%, now near Adapt in the simulator). Notion: designs are in GDD_Abilities / GDD_Skills / GDD_Tags and an index page; the per-operator inline databases could not be written by the connector. Grid icons render ~44 px, not 64. The portrait's lower third covers the feet. Narrator drifts after about a second at base speed (+25 speed per second): confirm intended. `verify_flow.py` is legacy (ability, ultimate and tint checks unported). `Demo/BoldInBeautyDemo/Build/` is an untracked cook leftover. All prices, rewards and rates are placeholders; portraits are the owner's drafts, pending approval.

## Working rules
- Commit trailers: `Co-Authored-By: Claude <model> <noreply@example.com>` plus the session's `Claude-Session:` URL. Push only to the feature branch; never force-push.
- The cloud container cannot run Unreal. Do compile and editor work through the owner's Mac Remote Control session: send bounded tasks, forbid prompting tools, read its final reply. Build with `-NoHotReloadFromIDE`.
- One writer per file set; follow `sync/README.md` for MAIN edits; record hashes in `reports/demo`.
