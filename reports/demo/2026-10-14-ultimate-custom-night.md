# Ultimate Custom Night — arcade mode (2026-10-14)

**SHARED artifact, WORKING.** Executor: Claude Code, Opus 5.5, owner's Mac (MACOS-LOCAL). Worktree `worktrees/bb-ai-work` (branch `bb-ai-work`, HEAD `645585b`, uncommitted). This continues a previous session's partial work in this worktree (rules, subjects and bots files existed but were untested). **Not play-tested by a person.** Windows counterpart OPEN.

## Owner follow-up (2026-10-14, second message) — implemented
- **Plack's sacrifice, every mode:** at 5 AM Plack sacrifices herself (she is gone) and summons **Death (019)**. Death is not active before that in any mode, Angel included. Her AI is the mode's: Easy 15, Medium 15, Hard 10, Angel 20. Roster table: Death row `{15, 15, 10, 20}` with `EArrival::PlackSacrifice`; she is the only subject that waits.
- **Angel (000)** is active from the start in Hard and Angel mode (owner confirmation; a unit test now locks this in).
- **Front window:** a third barrier (`SetWindow`, key **W**, a WINDOW button and the window drawn between the doors). **CAM 3 is the view outside it.** Luna's and Plack's straight lines now pass CAM 3, so you see them coming.
- **Diano** (odd hours of 015, like Plushtrap): each time *you* lower the camera, he attacks the left door (left-panned audio), the right door (right-panned) or the middle window (centred audio). You have **1 s**. Jester's forced camera moves never cue him.
- **Blood (010):** comes to the window every **20-30 s** and goes through **4 phases of 2-10 s** each (read as: each phase). Shut the window in phase 4 and she leaves; open at the end of phase 4 kills; **shutting in phases 1-3 kills**. The window shows her phase, and the WINDOW button warns "DON'T SHUT YET".
- **Weather (013):** takes the form of another operator (random, not himself) with a **neon yellow or light blue aura**, on CAM 3 and at the window. Shut the window. On purpose, there is no text cue for him: the aura plus a faint shimmer sound are the telegraph.
- **Stargazer (owner, third message):** after 1-7-9-5-3-6-4-8 he goes to CAM 2 or 3, then **smashes your front window and kills you**. A shut window does not stop him; only a flash during his route does. The overlay shows him through the cracked glass.
- **Owner's view of the human-model bot:** it is "quite a bit better than average". It uses exactly the requested parameters (0.35 ± 0.1 s, 5% missed inputs, 0.2 s between actions) and models no attention limits. Read its win rates as an upper bound, not as an average player's chances.
- **Scheduler:** no window threat opens while Blood holds the window open. Diano skips a side that cannot be shut. Weather waits out Blood's visit instead of giving up (bounded, because her visit is bounded).
- **Bugs found while re-verifying:**
  - The real Kuu and Adapt's copy of Kuu shared one "Kuu is at this door" slot, so one punch cleared the other's hold and the scheduler sent Luna to a door about to break. "Kuu at a door" is now read from Kuu's open duties.
  - The perfect bot didn't flash Stargazer on his current stop when it was the only one with charges left.

## What was built
- **Rules** (`BIBRules`, engine-free, headless-testable): `BIBUltimateNight.*` holds the world, the tools (2 doors and a camera system with a limited shock per camera), power, clock, **one roster table** (id, name, AI level per mode; 0 = not in that mode), the **central threat scheduler**, and the account side (Chapter 13 unlock, one-time 3,000 Diamonds). `BIBUcnSubjects.cpp` has **one behaviour class per subject (000-020)**. `BIBUcnBots.*` has the perfect, human-model and do-nothing bots and the suite runner.
- **Scheduler:** every lethal threat opens through `OpenDuty`. That call runs an earliest-deadline-first feasibility test over all unanswered lethal duties. Each duty is charged the reaction budget (default **0.35 s**, `FUcnTuning::ReactionBudget`, configurable) plus UI travel and click time. A threat that would make the set infeasible is **delayed**, never forced. It is skipped after `DeferCap`, so retries are bounded. Stargazer's flash window is exclusive. A door that cannot close is never the answer to a lethal threat.
- **AI formula** (one place, `MovementFor`): interval = 9 − 0.2·N s, chance = N/20. AI 10/15/20 reproduce Night Shift's Easy/Medium/Hard movement. It applies only to movement-based subjects. Stated cooldowns and kill timers are fixed.
- **UI:** `BIBArcadeOverlay` gains `EBIBOverlayKind::UltimateNight`, in the Night Shift Slate style: office with doors, desk and office tray; camera feed with infection, vaccine bottle and shock button; the 10-camera map; Jester's advert; jumpscare.
  - Sounds are all generated with `BIBSynth::Tone` plus the existing cues. Door threats are panned to their side.
  - Visual telegraphs for every lethal threat, in both views: edge pulses, the doorway occupant, office objects, and a centre line.
  - Judge's **FALSE ALARM** cue shows on the door buttons and next to the shock button before a misplay.
- **Pages:** the arcade page gets an "Ultimate Custom Night" tile (locked until Chapter 13, badged while the reward is unclaimed). It leads to an `arcade-ucn` mode page with 4 modes and `EOverlay::UltimateNight`.
- **Keys:** Night Shift door and camera bindings, digits for cameras, **F** shock, **V** vaccine, **X** close advert, **C** click the most urgent office object, Esc twice to quit.
- **Save:** `FArcadeRecords` adds `UcnPlays/UcnWins/UcnBestHour`, `bUcnUnlocked` (latched) and `bUcnRewardClaimed`, serialized in the account save. Older saves load with defaults.
- **Reward:** paid only on a win, only after the unlock, only if not yet claimed. The flag is set before paying, so the payment is idempotent.

## Files changed (`Demo/BoldInBeautyDemo/`, SHA-256 first 16 hex)
New:
- `Source/BoldInBeautyDemo/BIBRules/BIBUltimateNight.h` `bd0656179b7f22ba`
- `Source/BoldInBeautyDemo/BIBRules/BIBUltimateNight.cpp` `cd4dc5db9ee0c3b9`
- `Source/BoldInBeautyDemo/BIBRules/BIBUcnSubjects.cpp` `7f27f70a13207412`
- `Source/BoldInBeautyDemo/BIBRules/BIBUcnBots.h` `700124898d331040`
- `Source/BoldInBeautyDemo/BIBRules/BIBUcnBots.cpp` `8553f717de4842cd`
- `Tests/ucn_tests.cpp` `a6dd1594e17b3191`
- `Tests/ucn_beatability.cpp` `50f8cdf8efbede2f`
- `Tests/run_ucn_beatability.sh` `9e563090504dab7e`

Modified:
- `BIBRules/BIBMeta.h` `9cf7a006d29adecd`
- `BIBRules/BIBSave.cpp` `286277daf6ef3247`
- `BIBRules/BIBPages.h` `1b858de994600415`
- `BIBRules/BIBPagesExtra.cpp` `9271866b9c3060ac`
- `BIBGame/BIBArcadeOverlay.h` `f27ec2da9be477c7`
- `BIBGame/BIBArcadeOverlay.cpp` `dc2dc50ca4b3aa2b`
- `BIBGame/BIBPlayerController.cpp` `e82ab2c2bc0de5f3`
- `BIBGame/BIBBlueprintLibrary.cpp` `13b72a9701b62332`
- `Tests/run_core_tests.sh` `bb805a84cfb6ac05` (UCN rules added to the shared sources, since the arcade page now lists the mode)
- `Tools/verify_arcade.py` `1c032e35ff66b1f3`

Not touched: Night Shift rules (`BIBArcade.*`), economy, summon rates, other games, `Server/`. Nothing deleted, no dependency added. Untracked `Content/BB/` and `Tools/import_tripo_team.py` in this worktree belong to other work.

## Verification (MACOS-LOCAL, UE 5.8.3, real output)
- `Tests/run_core_tests.sh` (final tree): every existing suite has 0 failures: 103 core, 13,067 meta, 2,460 kit, 4,450 account, 3,964 campaign, 4,380 tester, 443 arcade, 59 mini-game, 230 gacha, 777 ark, 77 tactics, 151 angel; playthrough found no soft-locks. New: **780 UCN unit checks, 0 failures; 48 beatability checks, 0 failures.**
- Editor build `BoldInBeautyDemoEditor Mac Development -NoHotReloadFromIDE`: **Succeeded, 0 errors, 0 warnings** (154 s). After a one-line text fix ("12 AM" instead of "0 AM" in the loss line), the incremental build also Succeeded (6.6 s).
- After the follow-up: editor build Succeeded, 0 warnings (UBT recompiled the 5 UCN units). PIE `Tools/verify_arcade.py -BIBDev` **RESULT PASS, 60 OK / 0 FAIL**: it adds "W shuts the front window"; the loss line now reads "reached 12 AM".
- First PIE run `Tools/verify_arcade.py -BIBDev`: **RESULT PASS, 59 OK / 0 FAIL**, all old checks included. The UCN checks:
  - locked before Chapter 13
  - tile and page after the unlock
  - door and camera keys
  - the perfect bot survives Easy in the real overlay → **+3,000 Diamonds**
  - a Medium win → **+0** (no double pay)
  - an unattended Easy night is lost (Plack, about 47 s)

  This PIE run was on the build before the "12 AM" text fix.

### Win-rate table (200 seeds per mode, seeds 1..200)
| Mode | Perfect bot | Human-model bot | Do-nothing bot | Human target |
|---|---|---|---|---|
| Easy | 100.0% | 99.0% | 0.0% | ≥ 90% |
| Medium | 100.0% | 99.0% | 0.0% | ≥ 60% |
| Hard | 100.0% | 89.0% | 0.0% | ≥ 25% |
| Angel | 100.0% | 25.0% | 0.0% | ≥ 3% |

This is the final table after the owner's follow-up rules. Extra stress run: perfect bot **1,000/1,000 seeds in every mode**. The scheduler skipped 1 attack in 200 Angel nights and none in other modes.

- Human-model losses: Easy: Death 2. Medium: Death 2. Hard: Shiraishi 9, Stargazer 7, Death 3, Diano 3. Angel: Shiraishi 132, Stargazer 13, Media 3, Death 2.
- Fairness counters: 0 untelegraphed kills and 0 short-window kills, for every bot in every mode.
- Perfect bot's worst seed: power 17.8% left (Angel); shocks 0 left in Angel and 3 in Hard; cameras 2/10 working in Angel. So resources are sufficient, but the Angel shock margin is exactly zero on its worst seed.
- **Caveat:** the human-model bot models reaction time, missed inputs and the 0.2 s action gap, not attention across 21 subjects. Real players will do worse than these rates, especially in Hard.

### Bugs found and fixed during verification
- **Scheduler deadlock:** a pending Angel arrival made the scheduler refuse Stargazer's flash duty, but his route still ran, so he was unflashable (perfect bot 69-85% in Angel). Stargazer now takes precedence, and a route never starts without its duty.
- **Undefined behaviour:** "no deadline" windows (1e9 s) were converted to int milliseconds for telegraph events, which overflowed. They are now clamped.
- **Spec fidelity:** walkers used Night Shift's random room graph. The owner says "straight line across 5 cameras", so they now use fixed routes. The strike now falls on the next movement opportunity, not a 2 s placeholder.
- **Judge:** now treats Kuu at the doorway as "a character is there".
- **Hang risk:** Stargazer's wait was unbounded; it is now bounded by `DeferCap`.
- Alchemist's 30 s cadence no longer drifts.
- Bot fixes: the human remembers a Gambler camera only while she is still there; Stargazer flash gets a real per-stop reaction time.

## PLACEHOLDER numbers (all in `FUcnTuning` unless noted; none of the owner's numbers were changed)
| Name | Value | Note |
|---|---|---|
| WeatherEvery / WeatherWindow | 25-40 s / 5 s | owner gave no frequency or kill timer |
| bWindowUsesPower | true | a shut window costs a power bar |
| PowerDrainScale | **0.6** | tuned from 1.0: Shiraishi needs the camera up most of the night, and at 1.0 the perfect bot ran out of power in Medium+ |
| ShiraishiStepsToDoor | **12** | tuned from 10 (perfect bot 85% in Angel at 10); route = CAM 1→0 in order, then the right door |
| ShocksPerCamera | **5** | tuned from 4 (4 left the perfect bot with 0 margin); "very limited shock per camera" |
| ShockReserve | 2 | charges only Stargazer's flash may use |
| VaccineRespawn | 12 s | a new bottle this long after one is used |
| ActionSeconds / ClickSeconds | 0.25 / 0.20 s | UI travel per action / per extra click |
| DoorWindowMin | 2.0 s | minimum walker warning; actual = next movement opportunity (5-7 s) |
| DoorRepairSeconds | 20 s | a door Kuu broke |
| JudgeGrace | 0.6 s | a late honest action just after a threat is not a misplay |
| DeathRushWindow | 1.5 s | owner gave none |
| DeathOfficeEvery | 25 s | how often Death can appear in the office |
| MediaCharge | 6 | Night Shift's value |
| StargazerWarn / StargazerRush | 3.0 / 1.0 s | warning before the route; door rush after it |
| ShiraishiBurn | 4.0 s | "burns it, then kills you" |
| PowerOutDeath | 10 s | from power out to the dark |
| MiniWindow | 4.0 s | warning before a third mini camera |
| DeferCap / AdaptCycleCap / Judge copy | 12 / 60 / 20 s | |
| AI formula | 9 − 0.2·N s, N/20 | `MovementFor` |
| Night frame | 5 min, 50 s hours, 15 s grace, Night Shift drain/usage | reused |
| First appearances | Gambler grace+8 s; Narrator grace+4 s; Myrcia's bursts at 1, 2, 3 AM | |
| Walker routes | L: Stage-Dining-Storage-West Hall-West Corner; R: Stage-Dining-Kitchen-East Hall-East Corner | after leaving: back to Stage |
| Kuu after a punch | next move in 2 intervals | |
| Alchemist "middle" | CAM 6 | |
| Jester's advert | covers the monitor until closed | |
| Angel portrait | drawn halo with "000" | no texture yet |

## OPEN for the owner
1. **Weather:** how often he comes (PLACEHOLDER 25-40 s) and how long he waits at the window (PLACEHOLDER 5 s).
2. **Blood:** read as *each* of the 4 phases taking 2-10 s (not the whole visit). Shutting early = death (from "DON'T shut"). Confirm both.
3. **Window and power:** a shut window costs a power bar like a door (PLACEHOLDER).
4. **Judge and the window:** shutting the window with nobody at it and nobody on CAM 3 is a false alarm. Someone passing on CAM 3 (e.g. Luna) counts as "there".
5. **CAM 3 routes:** Luna's line is Stage-Dining-CAM 3-West Hall-West Corner and Plack's is Stage-Dining-CAM 3-East Hall-East Corner. The other cameras are a PLACEHOLDER.
6. **Shiraishi wording:** applied literally: camera on her = 45 s per step (stalled, not frozen); down = 10 s; default = 20 s. If "down and doesn't move at all" means frozen, say so. Her route length (12, PLACEHOLDER) and the door she reaches are also unset.
7. **Angel "does 2 simple attacks":** only one is described (office every other pull, 1.5 s).
8. **Arita "walks across the office … either door":** implemented as the door at the end of her chosen path.
9. **Fujita "mimic Kuu":** implemented as either door with the same 5 s timer (no punch).
10. **Alchemist:** her cameras are disabled permanently, like Ishii's. Is that intended?
11. **Gambler:** no stated penalty for ignoring her; none implemented.
12. **Death:** rush window 1.5 s and office frequency 25 s are placeholders.
13. **Difficulty:** human-model Hard is 89%, which may feel easier than "incredibly hard". Only placeholder fairness numbers were tuned.
14. A person's play-test, Windows build and counterpart freshness.

## Device / sync manifest
Device: macOS (owner's Mac). Root: `<local-home>/Documents/BoldInBeauty-Project/BoldInBeauty-Project/worktrees/bb-ai-work`. Base: git `645585b` plus a dirty tree (no mutually acknowledged manifest, so OPEN). Scope: shared game implementation. Windows: OPEN. MAIN was not edited (`BoldInBeauty_MAIN` sha256 `dae9c46b9d772dfd…`, unchanged).

## Proposed `BoldInBeauty_MAIN` update (for the accountable lead to integrate in §11)
> ### Unreal demo — Ultimate Custom Night arcade mode — 2026-10-14
> **SHARED artifact, WORKING.** Worktree `bb-ai-work` (uncommitted, on `645585b`). Executor: Claude Code, Opus 5.5, owner's Mac (MACOS-LOCAL).
> - **Owner follow-up (2026-10-14):**
>   - Plack's 5 AM sacrifice summons Death in every mode (at the mode's AI; Death inactive before it); Angel is active from the start in Hard and Angel mode.
>   - Front window: a third barrier; CAM 3 is outside it.
>   - Diano: like Plushtrap, on each camera-down attacks left / right / middle window, 1 s.
>   - Blood: every 20-30 s at the window, 4 phases of 2-10 s; shut only in the last phase.
>   - Weather: disguised as another operator with a neon yellow or light blue aura on CAM 3 at the window; shut it.
> - **Owner request recorded (2026-10-14):**
>   - The mode: Ultimate Custom Night, subjects 000-020, four modes: Easy AI 15 001-005; Medium AI 15 001-010; Hard AI 10 001-015 + 000; Angel AI 20 000-020.
>   - Tools: 2 doors, cameras, a very limited shock per camera.
>   - Unlock and reward: unlocked by the first Chapter 13 clear; one-time 3,000 Diamonds on the first win of any mode, once per account.
>   - Subjects: the owner's text for each subject is recorded in `reports/demo/2026-10-14-ultimate-custom-night.md`.
>   - Requirement: incredibly hard, never impossible.
> - **Implementation:**
>   - Code: `BIBRules/BIBUltimateNight.*` (one roster table, AI formula, central threat scheduler with a 0.35 s reaction budget, reward and save), `BIBUcnSubjects.cpp` (one module per subject), `BIBUcnBots.*`.
>   - UI: overlay `EBIBOverlayKind::UltimateNight`; arcade tile and `arcade-ucn` page.
>   - Tests: `Tests/ucn_tests.cpp`, `Tests/ucn_beatability.cpp`, `Tools/verify_arcade.py`.
> - **Assistant's interpretation (PLACEHOLDER, OPEN):**
>   - Tuned values: power drain ×0.6, Shiraishi route 12 cameras, 5 shocks per camera.
>   - Other gaps: walker routes, Death rush 1.5 s / office every 25 s, mini-camera warning 4 s, vaccine respawn 12 s, Judge grace 0.6 s.
>   - Unresolved: Weather's frequency and timer, the window's power cost, Blood's per-phase reading, Shiraishi's wording, Angel's second attack (full list in the report).
> - **Verification (MACOS-LOCAL, UE 5.8.3):**
>   - `run_core_tests.sh`: all suites 0 failures, plus 780 UCN unit checks and 48 beatability checks.
>   - 200 seeds per mode: perfect bot 100/100/100/100% (1,000/1,000 per mode in a stress run); human-model 99/99/89/25%; do-nothing 0% everywhere.
>   - Editor build Succeeded with 0 warnings; `verify_arcade.py -BIBDev` PASS 60/60.
>   - **Not play-tested by a person.** Windows OPEN.
