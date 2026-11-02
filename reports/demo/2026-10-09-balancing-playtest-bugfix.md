# Balancing, playtesting and bug fixing — Claude Code (Opus 5.5) — 2026-10-09

- Status: complete with checks (MACOS-LOCAL); headless bots and scripted PIE playtests, not played by a person; Windows OPEN.
- Accountable lead; worker; owned paths: this session (standalone, lead = owner); `Demo/BoldInBeautyDemo` rules, tests, tools, config;
  `docs/campaign.md` (generated); this report; HANDOFF / MAIN entries.
- Device; actual root: owner's Mac, worktree `BoldInBeauty-Project/worktrees/ai-agent-bb`, branch `claude/chapter-maps`, base `e6fc5b3`;
  UE 5.8.3 at the documentation root (`UE_5.8`).
- Host/model: Claude Code, Opus 5.5 (`claude-opus-5-5`); Claude Pro entitlement as configured; quota not measured in this runtime.
- Base manifest; counterpart: Windows replica and freshness OPEN. **MAIN replicas diverge (OPEN):** the worldwide
  `<local-home>/Documents/BoldInBeauty-Project/BoldInBeauty_MAIN` (234,548 bytes, 2026-10-09 09:29, art-pack entries) lacks the demo
  entries of 2026-10-07/08 that this branch's copy (220,898 bytes) has, and the branch copy lacks the art entries. Not reconciled here.

## Owner request (2026-10-09, verbatim)
"Check most recent Handoff and Boldinbeauty_MAIN should be 35kb and 229kb respectively i just want you to do hours and hours of
balancing playtesting and bug fixing thats all"

Read: `HANDOFF.md` (35,382 bytes) and the worldwide MAIN (234,548 bytes = 229 KiB), sections 0-3, 11, 12.

## How it was playtested
- **Headless whole-game bot** (`Tests/playthrough.cpp`): 5 play styles x 60 seeds through all 104 stages on the real rules.
- **Balance model** (`Tests/campaign_model.h` -> `Tools/gen_campaign_data.py` -> `BIBCampaignData.inc`, `docs/campaign.md`).
- **Kit simulator** (`Tests/kit_balance.cpp`) and focused economy probes (tower stash, Trade Centre loops, arcade payouts).
- **PIE playtests in UE 5.8.3** (editor rebuilt after every rules change): campaign, kits (all 20 operators on live enemies), Ark hub,
  gacha, arcade, tester panel (guest), Ark saved position (two runs), 13 chapter maps, weather, anime look.

## Findings and fixes (all numbers PLACEHOLDER; no owner rule changed)
| # | Finding (evidence) | Fix | Result |
|---|---|---|---|
| 1 | Bot bug: when the lowest squad member hit its level cap without duplicates the bot stopped levelling the whole squad; that made p90 34-46 days and a 28-replay spike at 10-8 | `LevelSquad` sets a capped / heart-gated member aside and keeps levelling the others | p90: hardcore 34 -> 13, no extras 38 -> 24, saver 46 -> 22 days; 10-8 28 -> 12 replays |
| 2 | Balance model counted only campaign summons (71) and planned stars for the average duplicate luck | stars planned for a 20th-percentile player (Poisson quantile) plus ~0.35 free pulls per stage | unlucky players are no longer asked for levels their cap cannot reach; end-game ceiling is 140 (Luna, Plack and Kuu are the only 4-stars; owner star rules unchanged) |
| 3 | 8 of 12 boss levels overshot their threat target even at the smallest layout (3-8 +24%, 7-8 +17%, 12-8 +13%, 11-8 +14%) | enemy damage trimmed to the target for any level >5% over (health unchanged) + test | every level within +5% of target |
| 4 | Chapter 1 (onboarding) needed 32 replays, more than Chapter 2 (26); 1-2 asked for Lv 7 straight after the tutorial; Chapter 1 done on day 3 | Chapter 1 eases in like Chapter 2; the tutorial stays the scale anchor but is not farmed; scales never below base | 7 replays; 1-2 asks for Lv 3; Chapter 1 boss still x2.05 health (old value) |
| 5 | New accounts have 0 Charge (owner rule: 300 Diamonds only) and regain 1 per 6 min; a first session stalled in Chapter 1 | day-1 login Charge 30 -> 120 | Chapter 1 finished on day 1 |
| 6 | Ark Tower floors had no hardness ramp: floors at the player's campaign difficulty paid ~3x the campaign's first-clear Gold (2.3M vs 0.68M) with no Charge | floors ramp to the campaign's 1.5x hardness over 8 floors | stash ~1.1-1.5x campaign first-clear Gold |
| 7 | Trade Centre: Gold -> summon tickets at the Diamond-derived rate; post-campaign daily sweeps bought ~27 pulls/day (~800/month vs ~55 free). MAIN: the reverse Diamond exchange "is not established" | receiving a ticket costs x25 (giving one is unchanged) + test | ~1 pull a day from post-game farming |
| 8 | Trade Centre sold arcade tokens for 588 Gold; a hard Night Shift win pays ~15,000 Gold-equivalent incl. 50 Diamonds, repeatable: a Charge-free farm | receiving a token costs x30 (~17,600 Gold) + test | buying a token never pays; tokens still refill to 5 a day |
| 9 | Boss Rush paid its Diamonds and Regular ticket every run: 5 free tokens to round 10 = ~5 tickets + 200 Diamonds a day | Diamonds/tickets only on a new best round (Gold/Bytes/hearts every run) + test | — |
| 10 | Kit sweep (`verify_kits.py`) flagged 19 casts that were harness artefacts (empty room, tested operator swapped out) | casts with no live enemy / wrong operator are SKIPPED and retried on a fresh stage; form 2 only for the tested operator | PIE 124 OK, 0 flagged, 2 skipped (was 155 OK / 19 flagged); Narrator's basic attack works in both forms |
| 11 | Editor crash in PIE about 1 run in 7 (2 of ~14), first frame after travelling into the Ark: `Assertion failed: Err == 0` (`PThreadsSharedMutex.h`), stack entirely in UE 5.8's experimental editor data storage (TEDS / MassEntity) | `Config/DefaultEngine.ini` `[ConsoleVariables] TEDS.Enable=0` (editor-only; the packaged game has no TEDS) | 0 crashes in the 12 runs after |
| — | Tried and reverted: free Chapter 1 replays. The PIE gacha run showed a free 1-8 quick clear pays 2,940 Gold (more than 2-1), an unlimited farm | replaced by fix 5 | — |

Reached by the bot after all fixes (60 seeds each): casual 23 days, regular 21, hardcore 13, no extras 24, saver 21-22; every run
finishes; regular player finishes Chapter 1 on day 1, Chapter 13 on day 21 (owner-approved pace: 3-4 weeks).

Checked and left alone: arena (5 fights a day, enforced); season pass (50 x 100 XP, ~340 XP a day from quests = ~15 days of a monthly
season); Night Shift / Rhythm Diamonds per token (skill rewards, ~25 pulls a month at most); daily extras vs stage income (stages ~3.0M
Gold, extras ~55k net); event / Ark / gang shops (ticket sales capped per rotation); equipment (an item reaches 10% with upgrades, so
the model's +20% damage / +20% defence is reachable; note: a full upgrade costs only 1,500 Bytes against ~900k earned, so gear
upgrades are not a real choice - OPEN, owner's call).

## Checks (MACOS-LOCAL, 2026-10-09)
- `Tests/run_core_tests.sh`: all 0 failures (103 core, 12,918 meta, 2,454 kit, 4,450 account, 4,162 campaign, 4,380 tester, 443 arcade,
  59 mini-game, 232 gacha, 777 Ark); bot: no soft-locks.
- Editor build `BoldInBeautyDemoEditor Mac Development`: Succeeded after each change, 0 warnings.
- Account server `npm test`: 118/118.
- PIE (this session, before the TEDS change): campaign PASS 127, Ark PASS 133, gacha PASS, arcade PASS (one earlier run crashed in
  TEDS), tester PASS 31, Ark save 1+2 PASS, chapters PASS, weather PASS 45, anime style PASS 62, kits PASS 124 / 0 flagged.
- **Final regression, final build, TEDS off (11 editor runs + accounts): all PASS, 0 crashes** - campaign, arcade, gacha, Ark, weather,
  anime style, tester (guest), Ark save 1 + 2, chapters, kits 123 OK / 0 flagged / 3 skipped (Narrator's form-2 casts after she fell),
  accounts 35/35 against a fresh local in-memory server (`verify_accounts.py`: its step-3 check expected Gold to be spent, but the
  tutorial's first level-up is free since the owner's 300-Diamonds rule; check corrected - not a game bug). Before the TEDS change
  the engine crash hit 2 of ~14 runs.

## Warnings, issues and owner questions
| Status | Severity | Finding / question | Evidence and impact | Proposed next action | Responsible |
|---|---|---|---|---|---|
| OPEN | info | Alchemist (owner kit, 5-star) simulates at power 0.66, below the 5-star band 0.90-1.10 | `kit_balance` table; the only kit out of band | owner decides whether to tune the owner's numbers | owner |
| OPEN | info | Chapter 11 "Return to Nowhere": the owner's brief says no levels, no bosses; the campaign still has 8 levels with a boss in Chapter 11 | `docs/chapter-maps.md` vs `BIBCampaign` | owner decides when maps are joined to stages | owner |
| OPEN | info | End-game ceiling: without a third star on each of Luna/Plack/Kuu the team average stops at 140; stages 12-5..13-8 recommend 140 and get harder through enemy mix only | owner star rules (20n units, +20 cap) and only three 4-stars | owner decides (more 4-stars, shared duplicate units are OPEN in MAIN) | owner |
| OPEN | info | Elite_Bundle meshes reference materials the bundle does not ship (LoadErrors at map load) | palette override already applied by `greybox_bundle.py`; log noise only | none needed | — |
| OPEN | info | Editor hung on exit only when launched with `-log` (macOS console window, `FMacConsoleOutputDevice::DestroyConsole`) | sampled stack; harness flag, not the game | do not pass `-log` to scripted runs | — |
| OPEN | warning | MAIN replicas diverge (see header) | sizes / grep | reconcile per `sync/README.md` | lead |

## Proposed BoldInBeauty_MAIN update
Target: section 11, after "Elite_Bundle dressing". Integration: see the HANDOFF; added to this branch's MAIN and the worldwide MAIN.

### Unreal demo — balancing, playtesting and bug fixing — 2026-10-09
**SHARED artifact, WORKING; MACOS-LOCAL verification.** Branch `claude/chapter-maps` (on `e6fc5b3`). Owner request: "hours and hours of
balancing playtesting and bug fixing". Fixes (all numbers PLACEHOLDER, no owner rule changed): playthrough bot keeps levelling the squad
when one member is capped (the long tails were the bot); balance model plans stars for a 20th-percentile-luck player plus daily income;
boss levels trimmed to their threat target; Chapter 1 eases in (7 replays, done on day 1) with day-1 login Charge 120; Ark Tower floors
ramp to the campaign's 1.5x hardness; Trade Centre prices received tickets x25 and arcade tokens x30 (Gold -> pulls / arcade farms);
Boss Rush Diamonds and tickets only on a new best round; kit sweep skips harness artefacts (124 OK, 0 flagged); TEDS off in the
editor (UE 5.8 crash on travelling into the Ark). Bot: regular player 21 days, all styles 13-24 days, no soft-locks. Report:
`reports/demo/2026-10-09-balancing-playtest-bugfix.md`. OPEN for the owner: Alchemist below its band; Chapter 11 has levels; end-game
ceiling at 140.


## Follow-up (owner, 2026-10-09, verbatim): "fix up level 11 and make alchemists kit a bit better maybe she's meant to be a character you start the infection muatate then instantly swap out but anyway balance her and you can rebalamce armour costs"

**Chapter 11 - Return to Nowhere is now the owner's walk.** The owner's final brief: no levels, no bosses, nothing - a ~5 minute walk.
The campaign had 8 generated combat levels there. Now: one entry ("Return to Nowhere") on the chapter page; starting it opens
`L_Ch11_ReturnToNowhere` with the team on foot, no enemies, no anime look, no weather, no HUD lines; reaching the map's `NowhereExit`
frame clears the whole chapter (stage ids 81-88 kept, so saves and numbering are unchanged), saves and returns to the Ark. No rewards,
no Charge, nothing to quick clear; it can be walked again. Chapter 11's story quests (Lobby) keep their loot. Balance model and bot
skip the walk; side effect: Chapters 12-13 now ramp Lv 121 -> 140 with grind instead of sitting at 140 (bot: Chapter 11 done the same
day as Chapter 10, campaign still 21 days). Code: `BIBCampaign` (`WalkChapter`, `IsWalkStage`, `CurveReward`), `BIBMetaOps`
(clear / start / quick clear), `BIBGameMode` (`StartStage` -> free roam "Nowhere", exit check, `bPlainWorld`), `BIBPages`.
PIE: `Tools/verify_walk.py` PASS (no enemies, no look, no weather, half way nothing happens, the exit clears to 88 and returns to the Ark).

**Alchemist.** Bug found: switching out called `ClearBattlefield()`, which wiped her infection list, and the game only ticked the
active operator's kit - so "infect, mutate, swap out" did nothing (enemies kept the green tag but took no damage). Fixed: switch-out
keeps infections and benched operators tick them (`FRuntime::TickBackground`; her soil puddle still waits for her). PIE: infect, swap to
Luna, walk away - the infected grunt dies (2,000 -> 0). Balance (PLACEHOLDER): her own infection profile (goo 1,000 every 5 s from 10 s,
explosion 3,500 at 40 s; Arita keeps the owner's 45 / 75 s via the per-kit `FInfectionTuning`), Transmutation owner numbers x1.3
(866/s, 1,950, 3 x 1,100, 2,275), placeholder HP 4,600 -> 5,200 and attack 800 -> 1,300. Simulator power 0.66 -> 1.00 (in band for any
attack 1,275-1,400; her result is noisy below that). The simulator gained a "setup and swap" scenario (Infection + Mutation, then
benched, re-entering when Infection is ready): 107k damage in 90 s with ~10 s on the field; on a crowd her infection kills every grunt
while she is away. Moving her shifted the 5-star median, so Gambler (400 -> 440) and Media (310 -> 335) attack were nudged back into band:
all 17 kits in band.

**Armour costs.** Upgrades 100 -> 500 Bytes per step (7,500 to +5 instead of 1,500 against ~930k Bytes a campaign pays). With linear
level growth a +1% upgrade is worth about half a team level late game; a full squad (12 items) now costs ~10% of the campaign's Bytes.
Armoury (Gold) prices kept: its starter tier is for Chapters 1-2, where it still beats the 1-5% drops. Back Alley prime gear (8-10%
for 400-600 Diamonds) kept as the premium tier. OPEN: drops (1-5% every clear, ~470 items by the end) make the Armoury irrelevant after
Chapter 2 - fixing that needs new stock or fewer drops, an owner call.

Checks: core suites all 0 failures (3,964 campaign incl. the walk, 2,460 kit incl. Alchemist profile + switch-out); 0 kits out of band;
editor build clean. **Final PIE regression on the follow-up build (12 scripts): all PASS, 0 crashes** - campaign, arcade, gacha, Ark, weather, anime style, tester, Ark save 1 + 2, chapters, walk, kits 124 OK / 0 flagged / 3 skipped (Narrator's form-2 casts after she fell), including the Alchemist infect-and-swap check (infected grunt 2,000 -> 0 while Luna is active).

## Change manifest (SHA-256, first 16 hex; before = `e6fc5b3`)
| Path (under `Demo/BoldInBeautyDemo/` unless noted) | Before | After | Scope |
|---|---|---|---|
| `Source/BoldInBeautyDemo/BIBRules/BIBArcade.cpp` | 857c4fa44ab60e75 | babe92102d07bd78 | shared |
| `Source/BoldInBeautyDemo/BIBRules/BIBCampaignData.inc` (generated) | 7b07d5ee138f454e | 58c2b37f78a60f35 | shared |
| `Source/BoldInBeautyDemo/BIBRules/BIBMeta.cpp` | 7d20ae252bcbd41f | 2c2b8d3ecfab86f4 | shared |
| `Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.cpp` | 5610691715484b61 | bc1008fe6cb0a21e | shared |
| `Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h` | 1ab9a844f8278b5c | e28dc39f785c011f | shared |
| `Tests/arcade_tests.cpp` | 2c3998aead4e6951 | 4c67af4e1e82109d | shared |
| `Tests/campaign_model.h` | b145b99bd96ae14a | 9c476cddb812b378 | shared |
| `Tests/campaign_tests.cpp` | f7eaf43e6634286e | fd4d174e49dc09f2 | shared |
| `Tests/meta_tests.cpp` | c2e9bdfb75e25784 | 4a3d0341c7bf50b9 | shared |
| `Tests/playthrough.cpp` | aa44cc8ba8134cb8 | 897849898512772e | shared |
| `Tools/verify_kits.py` | c971a0e84789a906 | ebad07a1f3ba8572 | shared |
| `Config/DefaultEngine.ini` | 1720b94785db8525 | 6ef630e48b8ce64e | shared (editor setting) |
| `docs/campaign.md` (doc root, generated) | 96912e8dcc334ca1 | fb2bcd47464b0f9e | shared |
| worldwide `BoldInBeauty_MAIN` (`<local-home>/Documents/BoldInBeauty-Project`) | 9ca04feaa55e66e5 | 3be2693f4a04641b | shared; reserved and released in `sync/handoff-state.json` |
| this branch's `BoldInBeauty_MAIN` | see git | see git | shared replica (diverged, OPEN) |

## Handoff
Commits on `claude/chapter-maps` (local; push only if the owner asks). Remaining: owner play-through; owner decisions above.
