# Demo: whole-game rebalance, quest system, full-screen pages, page transition, working settings — 2026-10-06

**Owner request (2026-10-06, verbatim summary):** (1) rebalance the whole game with a much heavier health and damage increase,
enemies scaling with it; (2) on start give 300 Diamonds and no other currency; a stage clear gives 50 Diamonds the first time only,
800 Gold, 250 Bytes and 2 Angel hearts for the first level, balance future levels and boss levels from that; (3) a quest system: a
Regular ticket at the end of each chapter (10 for Chapter 1), an extra quest every 2-3 levels worth about 20-50 Diamonds or Gold /
Bytes / Angel hearts, plus daily and weekly quests; (4) a 0.3 s transition between pages, every page full screen like the battle
pass (quests too), and Settings with relevant preferences that work. "Interpret as you will."

- **Executor:** Claude Code, Opus 5.5 (`claude-opus-5-5`), on the owner's Mac (MACOS-LOCAL), Claude Pro subscription.
- **Branch:** `claude/rebalance-quests-ui`, based on `claude/optimistic-tesla-g2pv4m` @ `6e9472e` (the tester build).
- **Worktree:** `<local-home>/Documents/BIB-rebalance-wt` (new; the owner's other worktrees were not touched).
- Everything numeric that the owner did not state is **PLACEHOLDER** and reversible.

## What changed

### 1. Heavier growth, enemies scaled to match
- `Config::StatPercentPerLevel` **1% → 6%** per operator level (health and damage): level 100 is now 6.94x level 1 (was 1.99x).
- **Chapter 1 is now solved by the balance model like chapters 2-13** (it used to keep its fixed 1 + 0.15/level curve). Enemy health
  follows the damage a reference team deals at the stage's recommended level, enemy damage follows its health, both relative to stage
  1 (base numbers against a fresh team), times a hardness ramp (1.0x at 1-1 to 1.5x at the Chapter 1 boss, then 1.5x) that reproduces
  how much tougher the old Chapter 1 boss was than its player. Result: Chapter 1 runs 1.00x → 2.27x (old: 1.00x → 2.05x); the last
  boss is about 12x (old about 4x) because the operators are now much stronger. Full table: `docs/campaign.md` (generated).
- Level costs: Gold unchanged (100 x level); **Bytes 40 → 30 x level** so the cost ratio follows the 800 : 250 reward ratio.
- Ark Tower and sub-stories keep their own placeholder curves (unchanged).

### 2. Starting wallet and stage rewards (owner numbers)
- New account: **300 Diamonds, nothing else** (`Config::StartingDiamonds`); the welcome mail no longer carries Gold or Diamonds; no
  starting tickets, tokens or Daily ticket (the Daily ticket arrives with the next day as before). The developer profile (`-BIBDev`)
  keeps its test wallet; the Tester panel's "Starting wallet" preset now means 300 Diamonds.
- The tutorial's "level Luna up once" step is **free** (a new account has no Gold); the coach text and the tile say so.
- Stage reward (`BIBCampaign.cpp StageReward`), every clear: stage 1 **800 Gold, 250 Bytes, 2 Angel hearts**; Gold and Bytes grow by
  12% of that per stage, hearts by 6% (stage 103: 10,590 Gold, 3,310 Bytes, 14 hearts). **First clear only: 50 Diamonds** in Chapter
  1, +10 per chapter (170 in Chapter 13). **Boss levels** (each chapter's level 8) pay 2x Gold / Bytes / hearts and 3x Diamonds.
- Stage clears no longer pay a ticket; tickets come from the story quests.
- Side economies rescaled to the new income (all PLACEHOLDER): daily supply mail 3,000/1,000 → 500 Gold / 150 Bytes; login bonus;
  season pass tracks; Armoury prices halved; Back Alley hoards cut (100,000 Gold for 80 Diamonds was worth a hundred clears; now 5,000);
  Outer World buildings half price and 40% output; arcade payouts; Ark Tower and sub-story first-clear rewards; starter bundle Gold.

### 3. Quest system (`BIBMetaOps.h`, four lists)
- **Story** (one-time, 39 quests = 3 per chapter): after **level 3** of every chapter 20-50 Diamonds (20 in Chapter 1 rising to 50 in
  Chapter 13); after **level 6** Gold + Bytes (odd chapters) or Angel hearts + Gold (even chapters) worth 2-3 clears; **chapter end**
  (boss cleared): **10 Regular tickets for Chapter 1, 1 for every later chapter** (22 in all). A quest every 3, 3, then 2 levels.
- **Daily** (8, reset every day): claim the login bonus, clear 3 levels, summon once, level up 3 times, change equipment, play the
  arcade 3 times, collect from the Outer World, and a bonus for completing 5 other dailies (20 Diamonds).
- **Weekly** (7, reset every 7 days, `FMeta::WeekIndex`): clear 15 levels, summon 10 times (a Regular ticket), level up 20 times,
  buy 3 things, fight 5 times in the Ark, collect 5 times, complete 25 dailies (80 Diamonds + a Regular ticket).
- **Milestones** (6, one-time): the two campaign milestones moved into Story; new ones: own 5 operators, raise an operator to level 20.
- Saved per profile (`weekly`, `weekIndex`, `story` in the save JSON; older saves load with empty lists). Claim all repeats until
  the daily bonus is taken too.

### 4. Full-screen pages, page transition, Settings
- **Full screen:** every page below a tab's root (`FPage::bFullScreen` = it has a Back button) hides the bottom tabs, like the
  Season pass, and menu pages drop the side portrait, so the page fills the screen. Operator pages and summon results keep their
  portrait (the operator is the content). The tutorial still shows the tabs when it points at one.
- **Transition:** any change of tab, page, quest tab or settings section fades in and slides up the middle of the screen over
  **0.3 s** (ease-out; `Config::PageTransitionSeconds`, `UBIBHubWidget::StartTransition`). Settings > Interface turns it off.
- **Quests page:** a full-screen board with tab buttons Daily / Weekly / Story / Milestones (each shows how many are ready) and Claim
  all; ready quests are highlighted; Story lists open quests, claimable first.
- **Settings page** (Preferences folded in; the old `go:prefs` link opens Settings > Interface), five sections, all saved:
  - Audio: master volume (applied to the audio device's primary volume), effects, music, voice (stored: the demo has no music/voice yet).
  - Graphics: quality (engine scalability), window mode, V-sync, frame-rate limit (30/60/120/unlimited), render scale 50-100%, show
    frame rate (`stat fps`). Applied through `UGameUserSettings` in the packaged / `-game` build; **the editor's PIE window only
    logs them**, so they do not rewrite the editor's own settings.
  - Controls: mouse sensitivity, invert vertical look, field of view 70-110, controls line in battle (applied to the operator live).
  - Interface: page transitions, lobby operator, commander name, restore defaults.
  - Account: sign out / leave guest mode, demo tools.
- No widget asset changed: `WBP_Hub` already names `NavBar` and `Middle`; the C++ drives them. `Tools/build_assets.sh` need not run.

## Verification (MACOS-LOCAL, 2026-10-06)
- `Tests/run_core_tests.sh` (Apple clang): **103 core, 13,008 hub/meta, 2,419 kit, 4,429 account, 4,061 campaign, 4,189 tester
  checks, 0 failures**; playthrough bot 30 runs x 5 styles, **no soft-locks**. New checks: the four quest lists, story rewards (10 then
  1 tickets, 20-50 Diamonds), weekly reset on day 8, the daily bonus, quest and settings pages, full-screen flags, settings clamps,
  save round-trip of the new settings and quests, the 300-Diamond start, the owner's stage-1 numbers.
- Playthrough (regular player, 120 min/day): finishes the 104 levels in **10 days** (was 9); team level 94 at the end (last stage
  asks 93); Gold 100% and Bytes 99% spent; Angel hearts 74% spent, never blocking; casual 25 days, hardcore 4.
- Editor build `BoldInBeautyDemoEditor Mac Development -NoHotReloadFromIDE` (UE 5.8): **Succeeded, 0 errors, 0 warnings** (137 s;
  10 s incremental after adding the `IsNavBarShown` test hook).
- `verify_hub.py -BIBDev` in PIE: see "PIE result" below.

## PIE result
`UnrealEditor BoldInBeautyDemo.uproject -BIBDev -EnablePlugins=SlateInspectorToolset -ExecCmds="py Tools/verify_hub.py"`:
**RESULT PASS, 72 OK / 0 FAIL** (was 53; 19 new checks), no error or ensure lines in the editor log. New checks: the lobby root
shows the tabs; opening Quests starts the transition and it ends within 0.45 s; Quests is full screen (no tabs, no side portrait);
the four quest tabs list their quests; Back brings the tabs back; Settings is full screen with options in all five sections; field
of view changes; with transitions off a section change does not animate; shop sub-pages are full screen. The Chapter 1 boss result
lines show the new rewards (2,940 Gold, 920 Bytes, 6 hearts, 150 Diamonds, chapter-quest pointer).
The first run stopped on a test-script error (Python cannot call `GetWidgetFromName`); fixed with the `IsNavBarShown` hook.
Screenshot: `reports/demo/screenshots/2026-10-06_hub_quests_fullscreen.png` (from the first run; on the second run the
SlateInspectorToolset capture returned empty data for every shot, a capture-tool issue: the checks themselves passed).

`verify_tester.py -BIBGuest` in PIE (fresh guest save, so a 300-Diamond account): **RESULT PASS, 32/32**.
`verify_accounts.py` (needs the account server) and `verify_campaign.py` were not re-run on this branch.

## OPEN (owner decisions)
- Every number except the owner's (300 Diamonds; 800 / 250 / 2 / 50 for level 1; 10 and 1 tickets) is PLACEHOLDER: the +6% per
  level, the 12% / 6% reward growth, boss multipliers, daily/weekly rewards, the side-economy rescale.
- Stage clears no longer give a ticket and chapter bosses no longer give a Special ticket (the quests replace them). Say if both
  should stay.
- Fights were not play-tested; the balance is the model's. The owner's play-through decides how the new curve feels.
- Window mode / graphics apply only outside the editor; a packaged build is needed to see them.
