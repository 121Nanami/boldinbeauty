# UI / UX flow audit and fixes — Claude Code (Sonnet 5.5) — 2026-10-12

- Status: complete with checks (build, core tests, `verify_ark`, `verify_ui_flow`, `verify_perf` PASS; `verify_combat` 1 pre-existing FAIL, see section 6).
- Lead / owned paths: standalone session, no lead reachable; owned `BIBUI/BIBUI.cpp`, `BIBGame/BIBArkUI.cpp`, `BIBGame/BIBArcadeOverlay.cpp` (key aliasing only), `BIBGame/BIBPlayerController.{h,cpp}` (screen keys, last counter only).
- Device / root: macOS, `<local-home>/Documents/BoldInBeauty-Project/BoldInBeauty-Project/worktrees/bb-ai-work`, branch `bb-ai-work`, UE 5.8 (engine at `.../BoldInBeauty-Project/UE_5.8`).
- Scope: SHARED code, MACOS-LOCAL verification. Windows counterpart OPEN.
- Not available: `sync/devices/*` profiles and `sync/handoff-state.json` do not exist in this checkout, so no base manifest / writer reservation was possible. MAIN was **not** edited (concurrent writers are active in this tree; proposal below).
- **Concurrent work in the same tree:** during this session other uncommitted edits appeared in `BIBEnemy.*`, `BIBFx.*`, `BIBKitLuna.cpp`, `BIBPlayerCharacter.*`, `BIBPlayerFeel.cpp`, `BIBPlayerGAS.cpp`, `BIBMeta.h`, `BIBSave.cpp`, `BIBPagesExtra.cpp` (and a new `BIBTactics`). They are not mine and were not touched; the build and tests below include them. `BIBSave.cpp` / `BIBMeta.h` changed (not by me) — whoever owns that should confirm it is not a save-format change.

## 1. Audit — flow: sign-in → Ark → campaign → combat → results

Confirmed from code (`file:line` at the time of reading):

| # | Finding | Where | Status |
|---|---|---|---|
| A1 | Sign-in: nothing had keyboard focus; the player had to click the e-mail box, then click the code box after "E-mail me a code". | `BIBUI.cpp` BuildSignIn, `BIBPlayerController.cpp` RebuildMenu | **Fixed** |
| A2 | Sign-in code step: Esc did nothing (only the Back button). | same | **Fixed** (Esc / B = Back on the code step) |
| A3 | Pause screen (UMG `WBP_Pause`, Slate fallback in `BuildPause`): P/Esc could not resume (UI-only input mode); no keyboard or gamepad way out at all. | controller + `BuildPause` | **Fixed** (second pass, tested) |
| A4 | In a stage only `P` paused. Esc and a gamepad Start did nothing. | `BIBPlayerController.cpp` PlayerTick | **Fixed** |
| A5 | Results "Continue" was mouse-only (dead end for keyboard / gamepad). | `WBP_Results` / `BuildResults` | **Fixed** (second pass; not run) |
| A6 | Ark menu / panel: Esc closed everything but Gamepad Start did not; Start did not open the Ark menu from the world. | `SBIBArkScreen`, PlayerTick | **Fixed** |
| A7 | Ark menu / panel: keyboard / d-pad navigation moved focus invisibly (cards only drew a hover tint) and focus started on the root, so arrows did nothing. | `Card`, `SBIBArkScreen` | **Fixed** |
| A8 | Refusals while a menu or panel was open were invisible: `ArkToast` is drawn only by the Ark HUD, which is hidden behind the menu (failed teleport, "tutorial asks for something else first", locked page). A silent click. | `BuildArkHud` vs menu | **Fixed** (toast strip inside menu and panel) |
| A9 | Disabled cards (dimmed to 42 %, swallowed clicks) never said why. The reason is the card's own second line. | `Card` | **Fixed** (55 % opacity; click / Enter shows `<label>: <second line>`) |
| A10 | Every card click rebuilt the menu and replayed the fade / slide of **all** columns (flicker, felt slow). | `BuildMenuScreen` | **Fixed** (only a new column animates) |
| A11 | Station panel always reopened on its first counter tab (Roll Call etc.). | `ArkOpenStation` | **Fixed** (session-only memory; not while the tutorial walks a route) |
| A12 | Arcade overlays / prompt took keyboard only; Esc worked, gamepad B / A / d-pad did nothing (dead end on a pad). | `BIBArcadeOverlay.cpp` | **Fixed** (aliases B/Start → Escape, A → Enter, d-pad → arrows; key capture stays raw) |
| A13 | Combat HUD: ready skills were only a 4 px strip; HP numbers were 12 pt dim grey; bench HP bars 4 px. | `BuildBattleHud` | **Fixed** (full slab outline in the operator colour when ready; HP numbers 14 pt, white with shadow; bench bars 6 px) |
| A14 | Offline / guest: sign-in screen offers it; "Please wait..." has a 15 s HTTP timeout (`BIBAccount.cpp:120`), so no soft-lock. | — | OK, no change |
| A15 | First-time player after the tutorial: Ark menu flow, `TutorialPath`, highlights and 777 Ark checks all behave; no blocker found by reading or by `verify_ark`. | — | OK |

## 2. Proposals (not done — owner decision, scope or risk)

| # | Proposal | Why not done |
|---|---|---|
| P1 | **Gamepad gameplay**: `IMC_BIB_Default` (Tools/build_assets.py `BUTTONS`) and `DefaultKey` map keyboard / mouse only; there is no gamepad binding for move, look, attack, block, parry, dodge, moves, ultimates, switch, talk. Menus are now reachable on a pad, combat is not. | A control layout is an owner decision (MAIN §27: follow the owner's specified controller behaviour). Needs an IMC rebuild (`build_assets.sh`) and a layout. |
| P2 | Pause "Abandon stage" is one press with no confirm. Default highlight is Resume, so Enter alone never abandons. | A confirm is an extra click the owner may not want. |
| P3 | Enemy telegraph on the HUD (an "incoming" edge indicator while an enemy is winding up / unblockable). Today the telegraph is the enemy's body tint only (gold, rose = unblockable). | Needs enemy → HUD data; enemy presentation is a separate task. |
| P4 | Transition durations (panel 0.2 s fade + 0.25 s slide, column 0.2 s) left as they are; they are tied to the 12 fps motion pass. | Presentation timing is the owner's to judge. |
| P5 | `ShowResults` is also used for "The Ark is not built on this machine": Continue → ReturnToHub → same message again. Harmless loop on a machine without `L_Ark`; a quit / build hint would help. | Wording / flow. |
| P6 | Results screen: focus ring is a brighter button, not a bordered ring (the old Slate buttons have no focus brush). | Cosmetic. |

## 3. Gameplay feel (movement / input only)

Nothing changed. Input buffering already exists and covers what was asked:
`BIBPlayerFeel.cpp` — `BufferSeconds = 0.25`, `IsBufferable`: Attack, Parry (parry / dodge share the key; dodge = Shift+F), Move 1-3, Special, Ultimate E / R, Switch, Jump are recorded 0.25 s early and fired as soon as they can; cleared while the Judge menu / Exchange panel is open. `ParryWindow = 0.3` (`BIBTuning.h`) and `PerfectDodgeWindow = 0.2` are tuning values: **no coyote-time forgiveness added** (it would alter the effective documented window).
Proposal P7 (owner): if parry / dodge still feel strict, a grace on *pressing* slightly before the window (buffer for parry during an enemy windup) is the safe lever; changing the window length is a tuning change.

## 4. Changes

| Path | Change | HEAD SHA-256 (first 16) | After SHA-256 (first 16) | Scope |
|---|---|---|---|---|
| `Source/BoldInBeautyDemo/BIBUI/BIBUI.cpp` | `SBIBKeyScreen` (focus, Up/Down, Enter/Space/A, Esc/B/Start); `MakeKeyButton`; Results / Pause / Sign-in wrapped; sign-in focus follows the stage | aca6602fc5a80cdb | 3e85727741b71cee | SHARED |
| `.../BIBGame/BIBArkUI.cpp` | `Card` focus ring + disabled reason + 55 % opacity; `FFocusSink`; `SBIBArkScreen` focus hand-off + Start; `ToastStrip` in menu / panel; column animation only when new; HUD readability | 7ee6df5b50628e67 | fc961e92cf6dad44 | SHARED |
| `.../BIBGame/BIBArcadeOverlay.cpp` | `OverlayKeyName` gamepad aliases | a462117fecd3fc34 | cd2e3f2e2e858bb3 | SHARED |
| `.../BIBGame/BIBPlayerController.cpp` | Esc / Start pause in a stage, Start opens Ark menu, `MenuColumnsShown` reset, remembered counter | 87e7c6168c7c174a | 2e96f9a63b4566e5 | SHARED (after hash includes another session's CombatFeel hunk) |
| `.../BIBGame/BIBArkLibrary.{h,cpp}` | test hooks PressUiKey / GetFocusedWidgetType / GetFocusedWidgetPath | n/a | n/a | SHARED |
| `Tools/verify_ui_flow.py` | new check | null | n/a | SHARED |
| `.../BIBGame/BIBPlayerController.h` | `MenuColumnsShown`, `LastCounter` (session-only), `ArkOpenStation` default `-1` | bfec72ac29903565 | 9c56852e72da24ed | SHARED |

No economy, gacha, reward, tuning, save-format or enemy / ability file was touched. No file deleted, no dependency added.

## 5. Verification (tool output)

- Build: `Build.sh BoldInBeautyDemoEditor Mac Development` → `Result: Succeeded`, 25 actions, 0 errors, 0 `warning:` lines (`/private/tmp/claude-501/build4.log`).
- `Tests/run_core_tests.sh`: exit 0, all suites 0 failures (103 / 13,036 meta / 2,460 kit / 4,450 account / 3,964 campaign / 4,380 tester / 443 arcade / 59 mini-game / 230 gacha / 777 Ark / 77 tactics / 151 angel; "No soft-locks"). Baseline before my edits was also all 0 failures.
- `Tools/verify_ark.py` (closest hub check; there is no `verify_hub.*`), PIE, `-BIBDev`: `RESULT PASS`, 133 OK, 0 FAIL (`Saved/VerifyArk.log`) — menu columns, teleports, counters, summon, stages 1 and 9 and return, saved position.

## 6. Playtest addendum (same day, second pass)

New: `Tools/verify_ui_flow.py` (PIE, real Slate key path via the test hooks `BIBArkLibrary.PressUiKey / GetFocusedWidgetType / GetFocusedWidgetPath`). Run: `UnrealEditor BoldInBeautyDemo.uproject -BIBDev -ExecCmds="py $PWD/Tools/verify_ui_flow.py"` (`Saved/VerifyUiFlow.log`); with `-BIBNoAutoLogin` it checks the sign-in focus instead.

**What the playtest found and fixed**
- The first pass was wrong about *where* Pause / Results live: in the real game they are the UMG `WBP_Pause` / `WBP_Results` (built by `Tools/build_assets.py`); the Slate versions in `BIBUI.cpp` are only the no-asset fallback. In UI-only input mode the controller sees no keys, so the pause screen's own hint "P / Resume continues the stage." did not work either. The first run showed focus staying on the viewport and every pause key failing.
- Fix: `ABIBPlayerController` registers a Slate input pre-processor while Pause / Results is up (`UpdateScreenKeys`, `HandleScreenKey`, `ContinueFromResults`). Pause: P (or the rebound pause key), Esc, pad B / Start, Enter / Space / A resume. Results: Enter / Space / A, Esc / B / Start continue. Works for the UMG and the Slate screens.
- Optimisation (battle HUD, runs every frame): the six skill slabs copied a struct with three strings on every lambda call and built + hashed a `Key+Name` string per slab per frame for the cooldown peak. Now by reference and an array indexed by slot; the curtain still resets when the ability changes (operator switch).

**Results (`Saved/*.log`, editor PIE, this build)**
- `verify_ui_flow`: RESULT PASS, 22 OK. Covers pad Start / M / P open the Ark menu, focus lands on a card (SButton), Backspace and pad B go up a column, Start closes menu and panel, Roll Call panel focus on a button, Start / P pause a stage, Enter / B / Start / P resume, Down + Enter never abandons. Sign-in (`-BIBNoAutoLogin`): the e-mail box (`SEditableText`) has focus without a click, PASS.
- `verify_perf`: RESULT PASS; Ark among the LED ads and fight sections 1 and 5 at avg 16.7 ms (60 fps), p99 17.9 ms, max 38.6 ms, 0 hitches over 100 ms. (No before-number was captured for the HUD change, so the micro-optimisation is not separately measured; the run is vsync-bound.)
- `verify_combat`: 1 FAIL, `action moments insert extra fresh frames (the 13th frame): 0 accents`. Not from this pass: `BIBAnimeStyleVolume::RequestAccent` returns early when `GetVisualStep() < 0`, which is what the 2026-10-11 dream filter does by design (HANDOFF "update verify_anime_style.py" is the open item for the same reason). Every other combat check passed. Needs the owner's call: keep the accent frames under the dream filter, or retire the check.
- Core tests were green on the previous build; the rules core is untouched since.

**Still not verified**
- Esc itself (the editor ends Play on Esc; test with a packaged build, Start / P are covered).
- Results screen Enter / Esc (no script finishes a stage to reach it; same pre-processor code path as Pause).
- Disabled-card reason toast and focus ring colour (visual; no screenshot taken).
- Real gamepad hardware, Windows.

## Proposed BoldInBeauty_MAIN update (pending lead / writer)

Target: §11, after the 2026-10-11 entries. *"UI / UX flow pass, 2026-10-12 (SHARED artifact, WORKING; MACOS-LOCAL verification): keyboard / gamepad reach every screen — Results, Pause and Sign-in take Up/Down, Enter / Space / A, Esc / B / Start and focus themselves; Esc or Start pauses in a stage and opens the Ark menu; Ark cards show a focus ring, a disabled card says why on click, refusals show inside menus / panels; only a new menu column animates; station panels reopen on the last counter (session only). Gamepad has no gameplay bindings (OPEN, owner decides layout). Report: `reports/demo/2026-10-12-ui-ux-flow-pass.md`."*

## Handoff

Branch `bb-ai-work`, nothing committed (the tree holds other sessions' uncommitted work; committing was not requested). Next for the owner: decide P1 (gamepad layout) and play-test the five unverified paths above.
