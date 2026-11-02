# Dev notes pass: Zenless-style combat, adaptive angels, BB UI, dreamcore, tickets, traversal, mobile — Claude Code (Opus 5.5) — 2026-10-09

- Status: complete with checks (MACOS-LOCAL); scripted PIE playtests and headless bots, not played by a person; Windows OPEN.
- Accountable lead; worker; owned paths: this session (ai-agent-bb-d6, standalone, lead = owner). One worker subagent (ticket
  income, BIBRules / Tests only) stopped at the session rate limit; the lead finished and fixed its work. Peer session ai-agent-bb-79
  (balancing) finished and handed over the worktree / editor before any build (its last commit 83e6c77).
- Device; actual root: owner's Mac, worktree `BoldInBeauty-Project/worktrees/ai-agent-bb`, branch `claude/chapter-maps`, base `83e6c77`;
  UE 5.8.3 at the documentation root (`UE_5.8`).
- Host/model: Claude Code, Opus 5.5 (`claude-opus-5-5`); Claude Pro entitlement as configured; the worker hit "session limit, resets
  2:20pm (Australia/Brisbane)".
- Base manifest; counterpart: Windows replica and freshness OPEN. MAIN replicas still diverge (worldwide documentation-root MAIN vs this
  branch's MAIN, see the 2026-10-09 balancing report); only this branch's MAIN was updated.

## Owner request (Notion "Dev notes", 2026-10-09, summarised; read in full from the page)
Make the UI style and gameplay feel "way way more like Zenless Zone Zero"; moves feel unresponsive and lag behind inputs; no stuns /
stun locks; attacks weak vs enemy health, bosses are "3 minutes of walk in a circle and spam lmb with no threat"; enemies (mage and
archer included) are too simple - angels should "adapt in real time to counter your playstyle", "no set kit for angels"; use the
motion-studio folder's graphics / trailer; make building themes and especially the shader "a bit more dream core looking" while
keeping the slightly terrifying theme; a consistent way to get the 1000s of tickets Secrets need ("a year or 2 ... but make it
possible"); jump too high; swim, climb, fall missing; "mainly please fix up the ui" (layout and feel, like ZZZ / Arknights); then a
mobile version with a joystick. Follow-up in chat: reference the Roblox game Anime Zero for the attack loop; "you have full creative
control this go ... change all moves prestablished movesets balancing etc all your choice".

## What was built
| Area | Result | Where |
| --- | --- | --- |
| Responsiveness | 0.25 s input buffer for attack / parry / dodge / moves / special / ult / switch / jump (consumed on success, quiet retries); basic-attack recovery cancels into dodge and every ability; soft lock-on | `BIBGame/BIBPlayerFeel.cpp`, `BIBPlayerCharacter.cpp`, `BIBPlayerGAS.cpp` |
| Attack loop (ZZZ / Anime Zero) | every operator's basic attack is a 4-hit string (x0.85/0.95/1.0/1.45, finisher knocks back and adds Daze); Luna's string pulls with the black hole and the finisher pushes with the white hole; sprint + attack = dash attack; hand-written kits ~2x their old basic DPS (Luna's old 2 s / 1 s punch cooldown was the "lag") | `BIBRules/BIBStagger.*`, `BIBTuning.h` |
| Stuns | Daze meter on every angel (hit size relative to the operator's attack + bonuses); full = STUNNED 3-6 s, x1.6 damage, CHAIN ATTACK on Tab (next operator drops in for 4x attack, area); flinch on hits ("stun locks"; brutes on heavy hits, bosses never) with poise after 5 flinches in 2.5 s | `BIBStagger`, `BIBEnemy.cpp` |
| Counters | perfect dodge (hit in the first 0.2 s of a dodge) -> slow motion + dodge counter; parry -> riposte with big Daze; switch-in strike | `BIBPlayerFeel.cpp` |
| Hit feel | hit stop on angel (and lighter on the operator), white flash, sparks in the operator colour, -12 degree slash streaks, shock rings, camera jolts, combo counter, Numeral damage numbers that punch in; data-kit ability cues (17 operators) were wireframe debug spheres (invisible in shipping) -> real glowing bursts / rings / beams / pillars per cue type | `BIBGame/BIBFx.*`, `BIBKitWorld.cpp`, `BIBArcadeOverlay.cpp`, `M_BIBFx` |
| Angels | one shared mind learns the commander's habits (decaying rates) and teaches counters: Guard Counter (vs LMB spam: front hits guarded, riposte; abilities / heavy hits break it), Feint, Unblockable (rose flash, ignores block and parry), Blink, Scatter, Mirror Nova / Volley / Dash (your moves copied), Anti-Air, Ambush; bosses learn 1.8x faster; 60% carried to the next stage. Crowds: attack tokens (2-4 commit, the rest circle / flank), separation, strafing archers that blink away from rushers, mages that lead their aim, boss combo chains and a faster phase 2; gold / rose telegraph flashes; health + Daze bars over each angel | `BIBRules/BIBAngelMind.*`, `BIBEnemy.cpp` |
| UI | battle HUD rebuilt in the Zenless layout and the trailer's style guide: squad top left (portrait, leaning slabs), boss bar + Daze top centre, ANGEL MIND line, objective top right, combo counter right, centre banners (PERFECT DODGE, STUNNED / TAB CHAIN ATTACK, GUARD BREAK, ANGELS ADAPTED), skill slabs with falling cooldown curtains bottom right; palette Neo Black / Midnight Steel / Bold Rose / Electric Violet / Angel Cyan; League Gothic, JetBrains Mono, Archivo Black (OFL) for every menu heading too | `BIBArkUI.cpp`, `BIBFonts.h`, `Content/BIBFonts` |
| Dreamcore | new layer in the anime post-process: pastel lavender haze with distance (tints, never bleaches), soft glow, colour fringe to the corners, slow "breathing" of the image edges, rare memory-glitch bands, violet-black deep shadows, rose-black vignette, grain; `DreamStrength 0` turns it off | `Tools/build_assets.py` (M_BIBAnimeStyle, 13 + 4 `Dream*` parameters) |
| Tickets for Secrets | Ticket road (opens after the campaign): daily stipend 5 Special tickets after 5 daily quests, weekly 10 Regular for 5 stipend days, every 20 stipend days 20 Special; time-gated, not farmable; page in Lobby > Events and a line on the Daily quests. Bot: a regular player owns all 6 Secrets on day ~534 (17.6 months median) | `BIBRules/BIBEvents.*`, `BIBPagesGacha.cpp`, `BIBSave.cpp`, `Tests/playthrough.cpp` |
| Bug found by the long bot | saves keep at most 2,000 equipment items; 1-2 years of drops exceed it, so a long-time player's save silently lost gear. Fix: inventory cap 1,500 (a new drop replaces the weakest spare, unequipped, un-upgraded item) | `BIBCore.cpp`, `BIBConfig.h` |
| Traversal | jump apex 360 cm (Luna; "All base except jump is doubled" was coded as doubling the jump) / ~90 cm floaty -> ~85 cm snappy (gravity x1.9, JumpZ 560, kit jump mult capped 1.25); wall climbing (W into a tall wall, W/S/A/D, Space jumps off, F lets go, mantle at the top); swim controls (float to the surface, Space up, Ctrl / C dive, Shift faster); hard landings (camera jolt, 0.3 s, damage from very high); Judge cannot climb or swim (Judge.md) | `BIBPlayerFeel.cpp` |
| Mobile | touch layer: floating joystick (left 45%, rim = run), drag to look, Zenless-style button arc (ATTACK, DODGE, PARRY, JUMP, BLOCK, 1-3, SP, ULT with charge fill, SWITCH, TALK, pause; ENTER + JUMP in the Ark) with live cooldowns; desktop skill slabs step aside; landscape iOS / Android settings; UE's default touch sticks off | `BIBGame/BIBTouch.*`, `BIBPlayerController.*`, `Config/*.ini` |

All numbers PLACEHOLDER. Owner rules in MAIN §2/§3 (rates, pity, Mileage, Daily ticket) untouched; Judge's mode rules, Narrator's
speed rules and every hand-written kit's moves kept; only basic attacks, jump and the systems above changed (full creative control
granted in chat).

## Checks (MACOS-LOCAL, 2026-10-09)
- `Tests/run_core_tests.sh`: all 0 failures (103 core, 13,022 meta, 2,460 kit, 4,450 account, 3,964 campaign, 4,380 tester, 443 arcade,
  59 mini-game, 230 gacha, 777 Ark, **151 angel / Daze**); bot: no soft-locks; Secrets day 534 median for a regular player.
- Editor build `BoldInBeautyDemoEditor Mac Development`: Succeeded, 0 warnings.
- PIE: `verify_combat.py` (new) **PASS**: jump apex 84 cm; combos to 16-17 hits; Daze to 100% -> stun -> chain attack (Luna -> Judge);
  boss takes damage from the string; the angels learned Guard Counter (100%), 5 counters known, adaptation 91% over one boss stage;
  climb up a wall and jump off. Regression with the new AI / feel: `verify_campaign.py` PASS (127), `verify_kits.py` PASS (124 OK,
  0 flagged, 3 skipped), `verify_ark.py` PASS; `Tools/ui_shots.py` (desktop and `-BIBTouch`) PASS. Dreamcore tuned down once after a
  screenshot showed the haze bleaching floors to near-white (now tints only, white ceiling re-applied).
- Screenshots: `Demo/BoldInBeautyDemo/Saved/Combat/*.png` (battle HUD, stun / chain banner, boss, angels adapted, climb, Ark menu,
  station panels, mobile layout with `-BIBTouch`).
- iOS compile: **blocked** - "Missing files required to build IOS targets. Enable IOS as an optional download component in the Epic
  Games Launcher." Android: no SDK / NDK on this Mac.

## Warnings, issues and decisions for the owner
| Status | Severity | Finding / question | Evidence | Next action |
| --- | --- | --- | --- | --- |
| OPEN | blocker (mobile build) | iOS needs the UE 5.8 iOS component; Android needs Android Studio + SDK / NDK; both need the owner's signing accounts (bundle ids are placeholders `com.boldinbeauty.demo`) | UBT log above | owner installs the components; then `RunUAT BuildCookRun -platform=IOS` |
| OPEN | info | the 12 fps "held frames" anime look (owner 2026-10-07) adds up to 83 ms of visual delay; it can make inputs *look* late even with the buffer | `BIBAnimeStyle.h` | owner: keep, or try Settings > Graphics with stepping off |
| OPEN | info | swimming controls are in, but the campaign's code-built stages have no water; swim is proven only by verify_chapters' test character in the chapter maps | `verify_combat.py` log | play a chapter map (free roam) |
| OPEN | info | "building themes" got the dreamcore post-process everywhere; the maps' own materials / props were not re-themed (a map rebuild with new palettes is a separate pass) | - | owner's call |
| OPEN | info | the 14 PROPOSED kits keep their abilities; their basic attacks now flow as strings and every cue has a real effect, but no ability was redesigned | - | owner's call |
| PROPOSED | info | Ticket road amounts and the inventory cap are assistant choices | bot | owner's call |
