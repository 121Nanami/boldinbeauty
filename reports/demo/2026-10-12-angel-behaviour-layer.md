# Angel behaviour layer: player model, squad board, steering (2026-10-12)

Branch `bb-ai-work`, owner's Mac (macOS, UE 5.8). Executed by Claude Code (Sonnet 5.5) on the Claude Pro subscription. Cross-device
freshness (Windows) is OPEN: `sync/devices/` does not exist in this worktree and `sync/handoff-state.json` was absent, so no MAIN
write was reserved; the proposed MAIN text is at the end.

## What was found first
The brief described `ABIBEnemy` as a straight-line chaser with no tokens, separation or adaptation. That was true before 2026-10-09; the
branch already had the shared angel mind (`BIBRules/BIBAngelMind.*`: counters, feints, blink, mage lead scaled by adaptation), a token
cap (`MaxAttackers`), `Separation()`, strafing and flanking. The layer below is therefore additive: the mind still decides WHICH
counter-move is known; the new layer decides HOW the crowd behaves (timing, positions, who presses, walls). No archetype or tuning value was
touched (`BIBCampaign.cpp`, `BIBTuning.h`, `BIBAngelMind.*` are unchanged).

## Files
New, pure rules (no Unreal): `BIBRules/BIBPlayerModel.{h,cpp}`, `BIBSquad.{h,cpp}`, `BIBSteer.{h,cpp}`.
New, glue: `BIBGame/BIBTactics.{h,cpp}` (world subsystem + whisker sweeps), `BIBGame/BIBTacticsLibrary.{h,cpp}` (crowd measurements).
Changed: `BIBGame/BIBEnemy.{h,cpp}`, `Tests/run_core_tests.sh` (one added suite).
Tests / tools: `Tests/tactics_tests.cpp` (77 checks), `Tools/verify_tactics.py` (PIE crowd playtest).
Switch: console `bib.Tactics 0` turns the layer off (falls back to the 2026-10-09 behaviour); 1 = on (default).

## Behaviour table (kind -> adaptation)
| Kind | Adaptation (all bounded by `Strength() <= 0.6`) | Telegraph |
| --- | --- | --- |
| Grunt / Brute | token from the squad board (max 3 committed, rotates after 2.5 s unused, nearest + longest-waiting first); pressure closes in aiming ahead of a kiter (<= 350 cm); waiters walk to a spread slot, 1-2 flankers to the commander's back; random pause (<= 0.5 s) added between swings against a rhythmic dodger; some strikes become delayed/doubled feints; give way during a channel (<= 3 s) | unchanged windup + flash (a feint's flash is the same flash) |
| Assassin | as melee, plus arcs in from the side while far (straightens near); dash only when the commander is backing away, aimed at where they are heading | new 0.25 s flash before the dash; the strike after it keeps its windup |
| Ranged | walks to a support slot (spread bearings), strafes more against a standing/blocking commander, backs off and holds fire during a channel, jitter between shots, some shots delayed | existing aim line + flash |
| Mage | lead as before (still scaled by the mind) or the model's, plus sometimes a cut-off placement on the escape side; offset clamped to 0.9 x blast radius so a standing commander must still leave the circle | blast circle shown where aimed for the whole windup |
| MiniBoss / Boss | not counted for tokens; keep the mind's chains and the slam; no jitter or give-way | unchanged |
Fairness: strength ramps over 45 s of fighting, falls by up to 80% while the commander is losing health, and one attacker token is taken
away when recent damage exceeds 0.35 of a health bar. Nothing is unblockable or unavoidable that was not already.

## Verified (tool output)
- `Tests/run_core_tests.sh`: all suites 0 failures (core 103, meta 13036, kits 2460, account 4450, campaign 3964, tester 4380, arcade 443,
  mini-game 59, gacha 230, ark 777, tactics 77, angel 151, playthrough no soft-locks).
- `UnrealBuildTool BoldInBeautyDemoEditor Mac Development`: Succeeded, no warnings in the new/changed files.
- `Tools/verify_campaign.py` in PIE: RESULT PASS (127 OK, 0 FAIL).
- `Tools/verify_tactics.py` in PIE, stage 25 (11 angels), single runs each, standing still 10 s then backing away 6 s:

| | layer off | layer on (run 1) | layer on, staggered casts (run 2) |
| --- | --- | --- | --- |
| max melee committed (standing / kiting) | 2 / 1 | 2 / 1 | 2 / 1 |
| frames with more than 3 melee committed | 0 | 0 | 0 |
| mean committed (standing) | 0.28 | 0.15 | 0.30 |
| closest pair of angels (cm, standing / kiting) | 124 / 80 | 182 / 156 | 216 / 76 |
| mean distance of all angels while kiting (cm) | 1333 | 1181 | 1508 |
Single runs with random decisions and a 10 s / 6 s window: they show the token cap holds, nothing stacked (0% of pairs) and the crowd still
reaches and clears, but they do NOT show a measurable difference in how close the crowd stays to a kiter; that needs many seeds.

## Could not verify / OPEN
- `Tools/verify_combat.py` has one FAIL, identical with the layer off (`bib.Tactics 0`): "13th frame: 0 accents". `RequestAccent` returns
  early because the dream-filter rework made the legacy look (and its visual step) default-off; the handoff already lists
  `verify_anime_style.py` as to update. Unrelated to the AI; the other 11 checks pass, including "the angels learned the Guard counter".
- "Ultimate or channel active" is read from `BIB.State.Casting` held > 0.8 s and not dodging; the player exposes no dedicated ultimate flag
  and `BIBPlayer*` files were out of scope. A real flag would be more precise.
- Dodge rhythm only needs the Dodging tag; a human-feel playtest (rhythm dodging, real kiting, ultimates) has not been done, only the
  scripted stand-still / back-away run. No navmesh: whisker sweeps against WorldStatic/WorldDynamic only (doors may need a channel check).
- Windows build and counterpart freshness: OPEN.
- Another Claude session was building and running an editor in the same worktree during this work; one verify_tactics run failed from that
  interference and passed when re-run alone.
- `meta` check count differed between two runs of the suite (13022 vs 13036) although none of the new files are linked into it; likely a
  clock-dependent check, not investigated.

## Proposed MAIN update (not applied: no reservation, concurrent session)
"Angel behaviour layer (2026-10-12): `BIBRules/BIBPlayerModel|BIBSquad|BIBSteer` + `BIBGame/BIBTactics|BIBTacticsLibrary`; `bib.Tactics 0`
disables; max 3 melee tokens; strength cap 0.6, PLACEHOLDER numbers; see reports/demo/2026-10-12-angel-behaviour-layer.md."
