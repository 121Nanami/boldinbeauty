# The 13th frame, the bb-Showreel motion pack in game, iOS build — Claude Code (Opus 5.5) — 2026-10-09

- Status: complete with checks (MACOS-LOCAL); scripted PIE, not played by a person; Windows OPEN.
- Lead / worker: this session (ai-agent-bb-d6), standalone; owner is the lead. Branch `claude/chapter-maps` after `99f83eb`.
- Device / root: owner's Mac, worktree `BoldInBeauty-Project/worktrees/ai-agent-bb`, UE 5.8.3 (`UE_5.8`, iOS component now installed).
- Host / model: Claude Code, Opus 5.5; Claude Pro entitlement as configured.

## Owner request (2026-10-09, chat, verbatim)
"So few notes to mimic the 12 fps illusion can you try some trick to kind of force a 13th frame to make actions more vivid on things
like movement turns move usage etc also i just downloaded ios to do the mobile build <local-home>/Downloads/motion-studio/videos/bb-Showreel
Here is a file filled with stylised transitions ui etc to use in game Just note they are all animated, to be extra stylised Make sure
to use them all"

## What was built
| Area | Result | Where |
| --- | --- | --- |
| The 13th frame | The anime look holds the picture at 12 fps. Action moments now insert one extra fresh frame immediately, between two steps, without moving the 12 fps clock: hits (0.6x attack and up), every cast, dodges, parries, switches, jumps, sharp turns (60 degrees in 0.15 s), starting to run, hard landings, and every enemy telegraph (so a tell is never hidden by a held frame). Heavy moments (ultimates, perfect dodge, parry, stun, chain attack, guard break, big hits, big falls) draw that frame as an anime impact frame: two-tone ink with rose lines and radial speed lines, for exactly one step. Max one accent per 1/30 s; impact frames off with Settings > reduce motion or `bib.ImpactFrames 0`. | `BIBAnimeStyle.*` (RequestAccent, ImpactFrame on a dynamic instance), `BIBPlayerFeel.cpp`, `BIBFx.cpp`, `Tools/build_assets.py` (shader) |
| Motion pack export | The pack's own source (HyperFrames + GSAP) copied to `Art/MotionPack/source`; `game.js` blanks the sample values ("800 / 4,800", "21 HITS", "FLOOR 42", sample mail and quests) so the game draws the real ones. `Tools/motion_export.py` renders all 49 UI pieces + 5 transitions at 1920x1080, **12 fps** (the game's anime rate), keeps the frames of the clips the game plays, packs 172 atlas pages (2048x2048, mips for phones); `motion_import.py` imports them; `motion_movies.sh` transcodes the ads, trailer, sheets, transitions showcase and master reel. All generated content is git-ignored (`import_motion_pack.sh` rebuilds it, like the Elite bundle). | `Art/MotionPack`, `Tools/motion_*`, `Tools/import_motion_pack.sh` |
| Motion player | Full-screen layer above everything (z 60): 12 fps flipbooks from the atlases, the pack's 1920x1080 layout fitted to any screen, or anchored to a point (target lock on the angel, crit number over the hit, button presses under the cursor, mobile flashes on the touch buttons); real values drawn in the blanked places with the brand fonts. | `BIBGame/BIBMotion.*` |
| Every piece has a moment | Menu (transition 03 + nav bar), station (transition 02 + page header with the real name; banner select at the factory), personal page / page change (transition 01), Back / Close / any action (secondary / primary button presses), Settings (toggles), travel (loading loop), Ark toasts (notice toasts with the real messages), purchase (shop card, real price), rewards (reward popup with real amounts, resource floaters with the real wallet), level up (real level), Team page (real battle power), summon (ticket consume with the real count or the modal, then charge, then the best rarity's reveal, x10 card backs), quests / mail / season pass / campaign map (real chapter and stage), arena rank, tower floor (real floor), boss rush round (real round), arcade (Night Shift, Rhythm), stage start (transition 04, 05 on a chapter boss level; skill buttons or the touch cluster), boss intro (real chapter), mini-boss alert, combo milestones (10, 25, 50, 100 ...; real count), big hits (critical number, real value), ultimate charged, switching (squad switcher), block / parry / perfect dodge (defense prompts), low HP (HP-bar warning), new target (target lock, real distance), VICTORY (real rewards) / DEFEAT; mobile: the three joysticks are the touch stick's art (ring in the Ark, tactical in fights, hex while climbing / swimming), the action cluster at fight start, switch / dodge / block flashes, the full mobile layout as a one-time controls tutorial. | `BIBGame/BIBMotionEvents.*` |
| Videos | 16 screens in the Ark: each ad over the place it advertises (government broadcast at the Government Building, ASCEND pills at the Premium store, sponsor auction at the arena, the Merchant at the Trade Centre, the broken board in the Back Alley, Media's apartments at the library), the download trailer over the factory, the master reel in the arcade, the 7 UI sheets and the transitions showcase over the other stations; ads have quiet spatial sound. The download trailer is also the startup movie of packaged builds (skippable). | `BIBGame/BIBVideoScreen.*`, `BIBGameMode.cpp` (SpawnScreens), `M_BIBScreen`, `Config/DefaultGame.ini` |
| iOS | With the iOS component installed the game target **compiles and links for iOS** (arm64 executable, 2026-10-09 20:10). Packaging stops at code signing: Xcode has no Apple account (0 signing identities). Automatic signing is set; `Tools/package_ios.sh` builds, cooks and packages once `BIB_IOS_TEAM` is given. Phones drop one mip level of the motion atlases (`Config/DefaultDeviceProfiles.ini`). | `Config/DefaultEngine.ini`, `Tools/package_ios.sh` |

## Checks (MACOS-LOCAL, 2026-10-09)
- Editor build 0 warnings; iOS target compile + link OK (signing blocked, see below).
- `verify_combat.py` PASS incl. "30 accents in the fight"; impact frame screenshot via `bib.Debug.ImpactHold`.
- `verify_motion.py` PASS: 54/54 pieces available, 16/16 screens playing, menu / station / summon (x10: charge + reveal) / campaign map /
  close / stage transition / VICTORY above the results / target lock / combo / boss intro by game events; all 54 pieces play.
- Regression on the final build: `verify_campaign` PASS, `verify_kits` PASS (125 OK, 0 flagged, 2 skipped), `verify_ark` PASS,
  `verify_anime_style` PASS, `verify_combat` PASS. `verify_accounts` was not run (it needs the local account server).

## Blockers and owner decisions
| Status | Item |
| --- | --- |
| OPEN (owner) | iOS signing: Xcode > Settings > Accounts > add your Apple ID (free personal team works for your own iPhone), then `BIB_IOS_TEAM=<Team ID> Demo/BoldInBeautyDemo/Tools/package_ios.sh`. The bundle id `com.boldinbeauty.demo` is a placeholder. |
| OPEN (owner) | Android: Android Studio + SDK / NDK are not installed on this Mac. |
| warning | Size: 172 atlas pages of 2048x2048 are about 1 GB as compressed textures with mips on desktop (about a quarter on phones, ~250 MB). They load only while a piece plays and are released after 20 s, but the package grows by that much. If that is too big: smaller cells or fewer clip frames in `Tools/motion_export.py` (PIECES / limit) and re-run `import_motion_pack.sh`. The first play of each piece in the editor builds its texture once (a short hitch). |
| info | Ad copy, prices and slogans in the ads are the pack's placeholders (the pack's own README asks the owner to approve them). |
