# Flow / sync / impact pass — Claude Code (Sonnet 5.5) — 2026-10-10

- Status: complete with checks (MACOS-LOCAL); scripted PIE, not played by a person; Windows OPEN.
- Lead / worker: this session (ai-agent-bb), standalone; owner is the lead. Branch `claude/chapter-maps` after `da063f1`.
- Host / model: Claude Code, Sonnet 5.5; Claude Pro entitlement as configured. Device MACOS-LOCAL, root `worktrees/ai-agent-bb`.

## Owner request (2026-10-10, chat)
"help fix the flow of gameplay animations ui vfx transitions ... the ui graphics vfx and transitions aren't syncing well with actual gameplay and the moves still aren't feeling that impactful when used".

## Diagnosis (root causes found in code)
| # | Finding | Effect |
| --- | --- | --- |
| 1 | The 3D scene is drawn at 12 steps/s (83 ms, held frames not rendered) but HUD, damage numbers, motion layer and followed VFX ran on the full frame rate. | UI reacted up to 83 ms before the world showed the same hit. |
| 2 | Motion pieces started at an arbitrary phase (`FPlatformTime`), so their 12 fps frames flipped between the world's steps. | Beating / stutter between UI-VFX and scene. |
| 3 | Followed VFX re-projected the live actor every frame while the scene showed a held image. | VFX slid ahead of the character they belong to. |
| 4 | Only hits with ratio >= 0.6 popped an accent frame; FX bursts (spark / slash / ring) did not. | Light hits and effects appeared up to a step late. |
| 5 | Hit stop was 25-70 ms (attacker) and 35-110 ms (angel), shorter than one 83 ms step; FX lived as short as 30 ms. | The freeze and the burst were often never drawn: weak impact. |
| 6 | Atlas pages loaded synchronously on first use / page change. | The 102 ms hitch in the boss section; late-starting VFX. |

## Changes (all numbers PLACEHOLDER)
- `BIBMotion.cpp`: a play starts on the last world-step boundary (frames flip with the scene); followed actors are projected once per
  world step and cached; first two atlas pages prefetched on `Play`, the next page streamed asynchronously while one plays
  (`FStreamableManager`), handles released with the 20 s unused rule.
- `BIBFx.cpp`: every effect spawn requests an accent (13th) frame (throttled 1/30 s in `RequestAccent`); minimum effect life 0.17 s (two steps).
- `BIBPlayerFeel.cpp`: accent on every landed hit (heavy = impact frame as before); attacker hit stop 50-110 ms at time dilation 0.05.
- `BIBEnemy.cpp`: angel hit stop 60-150 ms (boss x0.7), longer than the attacker's so the victim "crunches" last.
- Unchanged: owner rules, kits, damage numbers, reduce-motion behaviour (`bib.ImpactFrames`, settings).

## Checks (MACOS-LOCAL, 2026-10-10, UE 5.8 editor PIE)
- Editor build OK (4 files + link, 0 errors).
- `verify_combat.py` PASS; `verify_motion.py` PASS (240/240 pieces); `verify_perf.py` PASS: Ark among ads avg 8.5 ms, fights avg 8.3-8.4 ms,
  p99 <= 11.1 ms, 0 hitches > 100 ms (max 48.7 ms in the boss section; was 102 ms on 2026-10-09).

## OPEN / next
| Status | Item |
| --- | --- |
| OPEN (owner) | Feel is tuned by script, not by a person: play a few fights and say if hit stop is too long / short (`BIBPlayerFeel.cpp` line of `SelfHitStopUntilReal`, `BIBEnemy.cpp` `Stop`). |
| idea | Per-hit camera punch / FOV kick, attack anticipation frames, and an impact-aligned cue inside each ability piece need per-piece hit-time metadata (not in `motion.json` today). |
| OPEN | Windows counterpart not checked. |
