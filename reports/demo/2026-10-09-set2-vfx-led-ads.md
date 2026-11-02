# Set 2 VFX / UI / transitions in game, ads on the LED pillars, music -60%, perf pass — Claude Code (Opus 5.5) — 2026-10-09

- Status: complete with checks (MACOS-LOCAL); scripted PIE, not played by a person; Windows OPEN.
- Lead / worker: this session (ai-agent-bb-d6), standalone; owner is the lead. Branch `claude/chapter-maps` after `053ed4b`.
- Host / model: Claude Code, Opus 5.5; Claude Pro entitlement as configured.

## Owner requests (2026-10-09, chat, verbatim)
- "The ads slogan is kinda irrelevant? just place them on the LED signs as ads, + i got <local-home>/Documents/BoldInBeauty-UI-Transitions-VFX.md all the vfx ui and transitions in this files done as these cool motion graphics in this folder .../bb-Showreel ... push a new version not build but version for playtesting by me, Make sure to make this all work in game replace the ui transitions vfx etc with these files"
- "make sure they game runs well and isn't a buggy lag storm on release"; "the music is way too loud at its baseline lower them all by like 60%"; renders finished during the session ("all the Stuff in the vfx ui and transistion folder is done").

## What was built
| Area | Result | Where |
| --- | --- | --- |
| Motion pack Set 2 | 240 pieces now (54 Set 1 + 180 VFX of sheets 09-18 + 6 new UI pieces), exported from the pack's source at 12 fps, 419 atlas pages (298 MB on disk). | `Tools/motion_export.py`, `Art/MotionPack/source`, `Tools/import_motion_pack.sh` |
| World VFX | VFX pieces follow the character they belong to (projected to the screen each frame, scaled so the pack's 300 px body = the actor's height, mirrored when the caster faces left). | `BIBMotion.*` (Follow / BodyCm / bFlip) |
| Abilities | Every operator's M1 / moves / special / ultimate plays its own sheet 12-18 piece on cast (Data kits' M1 on the first hit of the string; Narrator's Diano form uses D1-D3). Passives on switch-in; Shiraishi's shield bubble and Media's packet loss always on; Luna's idle motif when idle. | `BIBPlayerGAS.cpp`, `BIBMotionEvents.cpp` |
| Shared combat (sheet 09) | dash attack, block flare, parry spark, perfect dodge afterimage, dodge smear, switch in / out, jump dust, hard landing, climb grip dust, swim splash, stun halo, chain attack drop-in, flinch pop, poise shimmer (boss winding up), door unlock glow. | `BIBPlayerCharacter.cpp`, `BIBPlayerFeel.cpp`, `BIBEnemy.cpp`, `BIBGameMode.cpp` |
| Statuses (sheet 10) | Each angel's statuses run as loops on it (synced 10x/s): burn, bleed, poison, shock, wet, root, weaken, vulnerable, doom, infection, mutation, immunity, illusion, fear / taunt (Jester), freeze / time-frozen (Timekeeper), slow / chill (Weather, Ishii) / time-slowed, mark -> seeded (Stargazer) / soul-link (Arita) / taunt. | `BIBMotionEvents::SyncEnemy` |
| Angels (sheet 11) | guard stance, feint flicker, blink out/in, mirror tint (Nova / Volley / dash strike), assassin dash streak, boss phase 2 burst, death dissolve on every kill. | `BIBEnemy.cpp` |
| Alternates | Narrator and Judge alternates are not in the demo's kits, so their pieces play at the nearest moment: past-life (switch-in), suppression (parry), burning-out (75% of explode speed), brain-damage (explosion), life-of-serenity (Diano ends), ultimate parry punch (block in Diano); Judge forgiveness (switch out), ignite (becomes Judge), sentencing / condemned (contempt), verdict markers on Suspect / Guilty angels. | as above |
| New UI | story dialogue panel (real speaker and line; replaces the old box), objective tracker (section n / m, cleared), gauges on switch-in of resource operators, Judge mode wheel on every court change / contempt, Global Exchange panel opener. | `BIBMotionEvents.cpp`, `BIBPlayerController.cpp`, `BIBArkUI.cpp` |
| Ads on the LED signs | The six ads moved off the stations onto the Ark's 40 LED ad pillars (HoloAd placeholders hidden), two-sided, cycling. Screens with the same video share one player (6 decoders for 40 pillars); one per ad carries quiet sound. | `BIBVideoScreen.*`, `BIBGameMode::SpawnScreens` |
| Set 2 videos | The Set 2 master reel (arena) and the 11 Set 2 sheet videos on station screens; 50 Ark screens in all. | `Tools/motion_movies.sh`, `BIBGameMode.cpp` |
| Music -60% | All pack audio (ads, reels, startup trailer) re-encoded at 0.4x volume; arcade rhythm music baseline x0.4 (the Settings slider still scales it). | `Tools/motion_movies.sh`, `BIBArcadeOverlay.cpp` |

## Checks (MACOS-LOCAL, 2026-10-09)
- Editor build clean (0 warnings).
- `verify_motion.py` PASS: 240/240 pieces, 50/50 screens playing, 40 ad screens on the pillars; in a real fight 12 Set 2 pieces fired from game events (dodge smear, perfect dodge, Luna M1 / Move 1, idle motif, stun halo, flinch, poise, feint, mirror, door glow, death dissolve); every piece plays through the console hook.
- `verify_perf.py` (new) PASS, editor PIE on this Mac: Ark among the ads avg 8.4 ms (120 fps), p99 9.8 ms, 0 hitches; fights avg 8.4-8.5 ms, p99 <= 11.5 ms; one 102 ms hitch in the boss section (first load of a piece's atlas).
- `verify_combat.py` PASS; `verify_kits.py` PASS (125 OK, 0 flagged, 2 skipped).

## Notes / OPEN
| Status | Item |
| --- | --- |
| info | This is a playtest version on the branch (not a packaged build). The generated motion content and movies are git-ignored: play in this worktree, or on another checkout run `Demo/BoldInBeautyDemo/Tools/import_motion_pack.sh` then `Tools/build_assets.sh`. |
| warning | Size: 419 atlas pages (298 MB source; ~2 GB as desktop GPU textures if all were loaded; only playing pieces are loaded and are released after 20 s). Shrink via the fx cell limit in `Tools/motion_export.py` if needed. |
| OPEN (owner) | iOS signing (Apple team) and Android SDK, unchanged. |
| OPEN (owner) | Narrator / Judge alternates are mapped to stand-in moments until the kits have them. Ad copy is still the pack's placeholder text. |
