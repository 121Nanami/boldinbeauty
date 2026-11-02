# Boldinbeauty — gameplay & UI demo (Unreal Engine 5.8)

A playable demo of Boldinbeauty's core loop. Function over looks, as requested.

The project is a C++ backend that editor assets plug into: Blueprints, UMG widgets, Enhanced Input and `.umap` stages.
Anything without an asset falls back to a code-built version: a blockout level from basic shapes, Slate UI and
default keys. So it always runs, even before the assets exist.

## Open and run (Mac, UE 5.8 + Xcode)

1. Open `Demo/BoldInBeautyDemo/BoldInBeautyDemo.uproject` (double-click, or open it in Rider).
2. When asked to rebuild the `BoldInBeautyDemo` module, choose **Yes**. Rider: build `BoldInBeautyDemoEditor`.
3. The editor opens `L_Hub` (the boot map). Press **Play**: the game goes straight into **the Ark** (`L_Ark`, built by
   `Tools/build_greybox.sh`); the sign-in screen sits over the living Ark, and after signing in you spawn where you last stood.
   The old 2D main screen / lobby was deleted on 2026-10-07 (owner request). Without a built `L_Ark` the game says how to build it.
   - Play in the editor: click the viewport to capture the mouse. **Shift+F1** releases it. **Esc** stops Play (editor default).

## Controls

| Key | Action |
| --- | --- |
| WASD / mouse | move / look |
| Shift | run (Judge, Narrator and Kuu-style heavies cannot sprint; each operator's speed comes from its kit) |
| Space | jump (Luna jumps double; Narrator needs 100 speed) |
| LMB | regular attack |
| RMB (hold) | block |
| F | parry · **Shift+F** dodge (MAIN: hold dash + parry) |
| 1 / 2 / 3 | Move 1 / Move 2 / Move 3 |
| 4 | Special |
| E (all operators except Judge) · R (Judge) | Ultimate (keys as written in the operator files). R is also **Arita's Special** (owner file). No operator currently has an alternative ultimate (the `BIBUltVariant` mechanism is kept for future ones) |
| Tab | switch operator |
| T | talk to the story contact / continue dialogue |
| P | pause (Resume / Abandon) |
| M / Esc / P (in the Ark) | the flow-chart menu (three-line button, top right): Menu > The Ark > Town Square > ... |
| F or T (in a station's ring) | open that place (factory = summon, armoury = equipment, task room = campaign, stalls = shops ...) |
| hold Alt (in the Ark) | show the mouse cursor to click the menu button |
| LMB repeatedly | the 4-hit basic string (the 4th hit is the finisher); while sprinting: dash attack (2026-10-09) |
| Tab while an angel is STUNNED | chain attack: the next operator drops in on the stunned angel (gold banner) |
| LMB right after a perfect dodge | dodge counter (violet banner) |
| W into a tall wall | climb (W/S/A/D on the wall, Space jumps off, F lets go, the top mantles over); Judge cannot climb |
| in deep water | swim: Space up, Ctrl (or C) dive, Shift faster; Judge cannot swim |
| `-BIBTouch` | mobile controls on a desktop (mouse = one finger); always on for iOS / Android builds |

## Combat feel, angels, look and mobile (owner Dev notes, 2026-10-09)

Zenless-Zone-Zero-style combat: inputs are buffered for 0.25 s, the basic attack is a 4-hit string that cancels into dodge and
abilities, hits stop the world for a few hundredths of a second, and every angel has a **Daze** meter: fill it and the angel is
**STUNNED** (x1.6 damage) and Tab brings the next operator in with a **chain attack**. Perfect dodges slow time and open a dodge
counter; parries riposte. The **angels adapt**: one shared mind learns your habits (basic-attack spam, dodging, blocking, kiting,
area attacks, jumping, switching) and answers with counters (Guard Counter, Feint, Unblockable, Blink, Scatter, Mirror Nova /
Volley / Dash, Anti-Air, Ambush) - shown in the "ANGEL MIND" line under the boss bar. Console: `BIBAngels 1` (fully adapted) /
`BIBAngels 0`. The HUD and menus use the BoldInBeauty trailer style (League Gothic, JetBrains Mono, Archivo Black; rose, violet,
cyan, -12 degree cuts). The anime post-process gained a **dreamcore** layer (MI_BIBAnimeStyle `Dream*` parameters,
`DreamStrength 0` = off). Mobile: floating joystick + button arc (`BIBGame/BIBTouch.cpp`). Check: `Tools/verify_combat.py`.

## What is in the demo

**The Ark is the hub** (owner request 2026-10-07). The 2D lobby is gone: you walk the Ark, and the only thing on screen is a
three-line menu button. The menu is a flow chart (every click opens the next column); its leaves either **teleport** you to a
place in the Ark or open a personal page (mail, profile, settings ...) in a glass panel over the 3D view. Every place is a real
greybox building with a glowing ring: the **Operator Generator Factory** (summon), Roll Call (team), the Armoury (equipment),
the unmarked Task Room (campaign), Merchant Row (one stall per shop), the arcade hall, the training grounds (tower, arena), the
library, the gang hall, the government building, the resistance house and the Back Alley stall in the Underworld. In battles only
what the fight needs is on screen. Your **position in the Ark is saved** (every few seconds while you walk, and when you leave for
a stage, free roam or sign out): the game, a stage's end and a restart all put you back where you stood (`BIBRules/BIBArk.*`,
`BIBGame/BIBArkUI.*`, `BIBArkStation.*`, `Tools/greybox_ark_hub.py`). The pages behind the panels are the same page model as
before (below):
- **Operator:** all 20 operators with portraits. Level up (Gold + Bytes, or supplement with Diamonds), Angel-heart gates every
  20 levels, duplicate upgrade stars, and a per-slot equipment list (equip, unequip, upgrade with Bytes; the selection is kept).
- **Team:** operator order (change a slot, swap, rotate) and team options that apply at the start of each stage. Narrator's
  Global Exchange can be preset; Luna and Judge have no configurable mechanics yet.
- **Lobby:** Quests (a full-screen board with Daily, Weekly, Story and Milestone tabs: story quests after levels 3 and 6 of every
  chapter pay 20-50 Diamonds or Gold / Bytes / Angel hearts, and every chapter's end pays Regular tickets, ten for Chapter 1), Mail,
  Notices, Events (7-day login bonus), Arcade (Boss Rush, Rhythm, Halo Fall, Neon Memory, Firewall Breach, Ultimate Custom Night and Ark Rally,
  a side-view racer: dirt bike or buggy, flips fill a nitro meter, rival ghosts, a track code editor), the 50-tier Season pass (free and paid tracks),
  Profile and Settings (Audio, Graphics, Controls, Interface, Account and the demo tools; the options apply in the engine).
- **Full-screen pages:** every page below a tab's root fills the screen like the Season pass (the bottom tabs make room; Back
  returns) and every page change fades and slides in over 0.3 s (Settings > Interface can turn it off).
- **Shop:** the Armoury (armour for Gold and Bytes), the Trade Centre (exchange almost anything for anything, 15% fee, Diamonds
  can be given but not received; receiving summon tickets costs 25x and arcade tokens 30x their base value, 2026-10-09), the Back Alley (high-end goods for Diamonds, locked by Commander level) and the Premium
  store (in-app purchases, **simulated: no real payment is ever taken**).
- **Revolution:** the Outer World (school, police station, fire station, hospital, gym, market, workshop, shelter produce
  Gold, Bytes and Angel hearts over time; 1 minute of play = 1 world hour) and the Ark (Endless Tower with Operator mode for
  3-4* operators and Secrets mode for 5-6* operators, a basic arena that simulates PvP, and four short sub-stories).
- **Campaign:** 13 chapters of 8 levels (104 stages), played in order; a chapter opens when the previous chapter's boss (level 8) is
  cleared. Every level shows the team level it is built for ("Lv"), so you know when to replay earlier levels for Gold, Bytes and
  Angel hearts. The admin account has every level open; a guest or new account plays through. Details: `docs/campaign.md`.
- **Summon:** pick a banner; the featured operator's portrait is shown with rarity and name in the lower third. Standard,
  Featured (Judge 5*), Featured (Narrator 6*) and Daily banners follow MAIN section 2 exactly (rates, pity, 50/50, Mileage,
  Daily rules). Results show each pull as a portrait tile.
- All 20 operators have full kits (M1, three moves, Special, Ultimate; passives for 5-6*). Since 2026-10-04 a normal account starts with only Luna; the admin account and `-BIBDev` own all 20. Luna, Judge
  and Narrator are the owner's hand-written kits; Alchemist, Arita and Adapt follow the owner's Notion pages; the other 14 were designed from the
  owner's playstyle notes (PROPOSED). Full tables: `docs/operator-kits.md`; how it works on the Gameplay Ability System: `docs/gas-architecture.md`.
- Everything marked PLACEHOLDER (prices, rewards, rates, names, minigame payouts) is a stand-in, not a design decision.

**Accounts, saving and the tutorial** (owner request 2026-10-04; design and open decisions: `docs/accounts-and-saving.md`)
- **Sign-in screen first:** *Create account* (username + e-mail) or *Log in* (e-mail), each confirmed by a 6-digit code the account
  server e-mails; or *Play offline as a guest*. The Profile page shows the username, e-mail and user id. The account server is in
  `Server/` (Node + MongoDB; its README says how to run and deploy it); the game reads its address from `[BoldInBeauty] ApiBaseUrl` in
  `Config/DefaultGame.ini` or `-BIBApi=<url>`.
- **Saving:** everything (currencies, operators with levels and stars, equipment, squad, pity, quests, mail, season pass, Outer World,
  progress...) is saved per account to `Saved/BIB/Saves/<user id>.json` after every action, when a stage is cleared, every 10 s and on
  exit, and synced to the account; the newer of the local and cloud copy loads at sign-in.
- **New accounts own only Luna** with an empty squad and play a 9-step tutorial: level Luna up, tap an empty slot (only Luna can be
  added), a forced first summon (Kuu), play Chapter 1 level 1, and Plack is unlocked for free.
- **Featured banners rotate every 3 days** (UTC clock; Judge 5* and Narrator 6* first); the Summon page shows the countdown.
- **Owner/admin account:** the e-mail `admin@example.com` is flagged admin by the server and gets every operator,
  unlimited currencies and every level.
- **Tester build (2026-10-05):** every player, guest or signed in, is a tester and gets **Lobby > Tester panel** (`BIBRules/BIBTester.*`, switch:
  `Config::TesterMode`). It sets any currency (add / remove / set with 1 to 100,000,000 per tap, four wallet presets), operator unlocks, levels
  (above 100 it adds upgrade stars), stars, duplicate units and Angel-heart gates, campaign progress (any chapter or level, tower, sub-stories),
  gear (add, upgrade, equip best, clear), Commander and Season XP, the day, Outer World, summon pity and 50/50, and five whole-profile presets
  (new player, early, middle, late, everything unlocked). **Skip tutorial** is on every tutorial page. The admin account keeps its unlimited
  wallet, every operator and every level; the panel does not reduce it. Tested headless in `Tests/tester_tests.cpp`, in the editor by
  `Tools/verify_tester.py` (run with `-BIBGuest`).
- **Launch options:** none = sign-in screen (over the Ark); `-BIBGuest` = local guest; `-BIBDev` = the old developer profile (all
  operators, no sign-in, nothing saved) that most verify scripts need; `-BIBNoAutoLogin` = ignore the saved sign-in (tests);
  `-BIBClassicHud` = the old UMG battle HUD.

**Stages (MAIN §7)**
- Every chapter has 8 levels. Each: talk to the story contact (T), then fight through 2-5 sections of 10-30 enemies; a section's
  door opens only after every enemy is dead; **a mini-boss closes every level** and the **chapter boss closes level 8** (in chapters
  2-13 the mini-boss holds the section before the boss; Chapter 1's boss shares its last section with the mini-boss, as before).
  The Chapter 1 boss unlocks Plack. Rewards are paid into the profile: Gold, Bytes and Angel hearts on every clear (that is the
  grind), Diamonds on the first clear (owner, 2026-10-06: level 1 pays 800 Gold, 250 Bytes, 2 Angel hearts and 50 Diamonds; the
  rest of the curve and the boss levels are balanced from there, see `docs/campaign.md`). Tickets come from the story quests.
- **Enemy types** (BIBRules/BIBCampaign.cpp): basic (melee), ranged (keeps its distance, shoots after a telegraph), mage (marks the
  ground under you and blasts it: step out of the circle), brute (slow tank), assassin (fast, high damage, dashes in, low health),
  mini-boss, chapter boss (adds a telegraphed slam). Chapter 1 has basic and brutes; ranged join in chapter 2, mages in 3,
  assassins in 4; from chapter 5 every chapter leans on one or two types.
- **Balance:** enemy health and damage grow stage by stage with the economy of a reference player (levels, upgrade stars, summons
  from the rewards) and every stage asks for a bit more than that player has, so about one replay per level and two or three
  before a boss. `docs/campaign.md` has every number; `python3 Tools/gen_campaign_data.py` regenerates the tables
  (`BIBRules/BIBCampaignData.inc`) after a change; `Tests/campaign_tests.cpp` fails when they are out of date.
- Tower floors and sub-story levels use the same flow in a code-built arena. Chapters 2-13 are also code-built (the editor maps
  `L_Chapter1_Stage1..8` exist for Chapter 1 only).

**Operators** (owner mapping for the demo: file Ability 1–3 → Move 1–3, Ability 4 → Special, Ultimate unchanged)
- **Luna:** Blackhole, Orbiting (black/white holes), Event horizon (hold), Expel (hold, 4 sizes), and Reality Destabilisation (gravity flips every 3 s, then 55,000 to everything).
- **Judge:** Innocent / Suspect / Guilty enemies; Witness / Lawyer / Judge modes; Paperwork, Court setting, Gavel smash, Retribution and Justice.
  - Restraining orders and the Lawyer passive are included.
  - Enemies that hit Judge 10 times, or deal 10 % of her health, are condemned.
  - Switching operators resets every enemy to Innocent.
- **Narrator / Diano:** speed is the resource. You explode at 1,000 speed (5,000 as Diano).
  - Narrator moves: Explosion, Global Exchange (stat panel), I AM STILL ALIVE (parry or random mimic) and Close enough.
  - The Switch ultimate turns you into Diano: Acceleration (chains), Endless god (hold) and Embodiment of imperfection.
- Shared: block, parry, dodge, switching, and death passes control to the next operator in line.

**Not in the demo (yet):**
- operator-file alternates and extra specials (Judge: Forgiveness, Genecide, Sentencing, Ignite; Narrator: the alt/special cards, pills)
- Diano's The TRUE STRONGEST and THE SOUL ONE (they need Ult² and secret mini-bosses)
- climb and swim
- signature weapons
- enemies that apply statuses to operators (Shiraishi's status resistance is only rules-tested)

## Editor assets

Asset layout under `Content/BIB/`. All of it is generated by `Tools/build_assets.py`, so it can be rebuilt after a
C++ change instead of being edited by hand:

```sh
Demo/BoldInBeautyDemo/Tools/build_assets.sh [path/to/UE_5.8]   # deletes Content/BIB, rebuilds it (~1 min), prints Saved/BuildAssets.log
UnrealEditor-Cmd Demo/BoldInBeautyDemo/BoldInBeautyDemo.uproject -run=pythonscript -script=$PWD/Demo/BoldInBeautyDemo/Tools/verify_assets.py -nullrhi   # Saved/VerifyAssets.log
UnrealEditor Demo/BoldInBeautyDemo/BoldInBeautyDemo.uproject -BIBDev -EnablePlugins=SlateInspectorToolset -ExecCmds="py $PWD/Demo/BoldInBeautyDemo/Tools/verify_ark.py"   # PIE: the Ark hub, Saved/VerifyArk.log (BIB_SCREENSHOTS=1: Saved/Screenshots/ark_*)
BIB_ARK_SAVE_PHASE=1 UnrealEditor ... -BIBGuest -ExecCmds="py $PWD/Demo/BoldInBeautyDemo/Tools/verify_ark_save.py"   # then BIB_ARK_SAVE_PHASE=2: the saved position across a restart
UnrealEditor ... -BIBNoAutoLogin -ExecCmds="py $PWD/Demo/BoldInBeautyDemo/Tools/verify_ark_start.py"   # boot into the Ark, sign-in over it, guest spawn
```

`Tools/verify_accounts.py` (PIE; needs the dev account server running, commands at the top of the file): the sign-in screen, sign-up by e-mailed code, the Luna-only start and the whole tutorial, the save file, sign-out and log-in with the progress restored, the owner/admin account. Run it WITHOUT `-BIBDev`. `Server/` has its own `npm test`.
`Tools/verify_kits.py` (PIE, about 20-25 min): every ability of all 20 operators is cast once on live enemies through the Enhanced Input path; it records HP, ultimate, cooldowns, gameplay tags and messages before and after and flags errors, ensures and casts that change nothing (`BIB_KITS_ONLY="Luna,Arita"` limits it; writes `Saved/VerifyKits.log`, last line `RESULT PASS|FAIL n ok, m flagged`). It uses the read-only `GetOperatorTags` / `GetEnemyTags` / `GetEnemyHP` getters of `UBIBBlueprintLibrary`.

`verify_campaign.py` (PIE, needs `-BIBDev`): the Campaign tab (13 chapters, locks), then real stages 2-1, 2-8 (boss), 3-1, 4-1, 5-8 and
13-8 (`BIB_CAMPAIGN_STAGES="9,16,..."` picks others): every section's spawn is compared with the C++ plan, the mini-boss and boss placement
is checked, and a stationary team must lose health to the first section's enemies (writes `Saved/VerifyCampaign.log`).

`verify_ark.py` (PIE, `-BIBDev`): boot into the Ark, all 17 stations, the flow-chart menu, every teleport + F, panels, a summon in
the factory, two battles (only the battle HUD), back at the task room, the saved position after a stage. The older PIE scripts
(`verify_gacha`, `verify_arcade`, `verify_tester`, `verify_campaign`, `verify_accounts`, `verify_weather`, `verify_anime_style`,
`verify_kits`) drive the same pages through `Tools/bib_hub.py` in the Ark. `verify_hub.py`, `verify_game.py` and the legacy
`verify_flow.py` tested the deleted 2D lobby and were removed with it (2026-10-07). A Mac cook includes every `/Game/BIB`
texture through `DirectoriesToAlwaysCook`.
The hub's rules are also tested without the engine: `Tests/run_core_tests.sh` (13,000+ hub checks incl. a walk over every page
and every button, 4,000+ campaign checks: the plans of all 104 stages, unlocks, rewards, saves, the balance tables, 4,000+ Tester panel
checks, and a **playthrough bot**: `Tests/playthrough.cpp` plays 13 chapters x 8 levels with the real rules from a fresh guest account,
many times and with five play styles, and fails on a soft-lock; `--stages` prints the grind per stage).
**Windows build:** `Tools/Package-Windows.bat` on a Windows PC (see `docs/windows-build.md`); Unreal cannot build the Windows target from a Mac.

**UI art.** Icons (`Art/Icons/*.png`, made by `Tools/make_icons.py`) and portraits (`Art/Portraits/<name>.png` full body and
`<name>_head.png` head, lower-case operator names) are plain files imported as `/Game/BIB/UI/Icons|Portraits/T_<file>`. To swap
art, replace a PNG with the same name and re-run `build_assets.sh`. The portraits are cut from the owner's workbook with
`Tools/make_portraits.py --xlsx "BoldInBeauty — 20 character designs.xlsx"` (each picture is named by the row it is anchored
to; checked by eye). `--placeholders` draws stand-in
portraits; `--sheets <dir>` cuts the front view of the owner's four-view character sheets into the real ones (check them
by eye; `--panels` fixes a sheet whose front view is not the first column).

**Without the new assets.** The hub also works from C++ alone: when the controller's main-menu widget is not a hub widget
(older assets), the code-built Slate hub is shown with the same pages (text only, no icons or portraits).

The script uses the engine's experimental EditorToolset (Blueprint graph DSL) and UMGToolSet plugins, enabled in the
`.uproject` together with PythonScriptPlugin and EditorScriptingUtilities. Edits made by hand in the editor are lost
on the next rebuild; change the script instead (or stop using it). Stage walls are cubes plus hidden cube blockers up
to the 2200 cm ceiling (not BlockingVolume brushes), matching the code-built blockout.

| Asset | Parent / type | Hooks it uses |
| --- | --- | --- |
| `Input/IA_*` (18 digital actions + `IA_Look` Axis2D), `Input/IMC_BIB_Default` | Input Action / Mapping Context | `BP_BIBPlayerController` → **Default Mapping Context**, **Button Actions** (one per `EBIBInput`), **Look Action** |
| `Blueprints/BP_BIBGameMode` | `BIBGameMode` | **Hub Map**, **Stage Maps** (index 0 = level 1 ... index 7 = level 8; must have 8 entries, otherwise levels are built in code), **Enemy Classes** per kind, Default Pawn / Player Controller classes |
| `Blueprints/BP_BIBGameInstance` | `BIBGameInstance` | project setting Game Instance Class |
| `Blueprints/BP_BIBPlayerController` | `BIBPlayerController` | input assets above; **Hud Widget Class**; **Screen Widget Classes** (one per `EBIBScreen`) |
| `Blueprints/BP_BIBPlayerCharacter` | `BIBPlayerCharacter` | Body/Head/Camera components, **Tint By Operator**, event **On Active Operator Changed** |
| `Blueprints/BP_Enemy_Grunt` / `_Brute` / `_MiniBoss` / `_Boss` | `BIBEnemy` | **Kind**, **Use Tuning Defaults** or custom stats, events **On Enemy Died** / **On Attack Telegraph** |
| `Blueprints/BP_BIBDoor`, `BP_BIBSection`, `BP_BIBStoryNPC` | `BIBDoor` / `BIBSection` / `BIBStoryNPC` | placed in stage maps |
| `UI/WBP_HUD`, `UI/WBP_Results`, `UI/WBP_Pause` | `BIBUserWidget` | call the **BIB** Blueprint library (`Get Hud Info`, `Start Stage`, …) |
| `UI/Icons/T_*`, `UI/Portraits/T_*` | Texture2D | imported from `Art/` |
| `Materials/M_BIBTint` | Material | vector parameter `Color` → BaseColor; every tinted actor uses it through `BIBVisuals` (BasicShapeMaterial if missing) |
| `Maps/L_Hub`, `Maps/L_Chapter1_Stage1` ... `Stage8` | Level | `L_Hub` is the boot map (it hands over to the Ark) and the place code-built stages are built in; it has a Player Start and lights. Stage maps add a Story NPC and `BIBSection` actors (StageNumber, SectionIndex, enemy counts, Entry Door, optional Spawn Points), plus the matching `BIBDoor`s |

Test hooks (console in PIE, or Blueprint/Python on the player controller): `BIBFillUlt` fills the active operator's
ultimate; `BIBDamageEnemies <amount>` damages every live enemy through the normal damage path (ult charge included);
`BIBSetNarratorSpeed <n>` sets Narrator's speed; `BIBTeam <A> <B> <C>` (hub) sets the team by operator name, e.g. `BIBTeam Blood Death Gambler`,
then start a stage; `BIBGrantAll`; `BIBUltVariant <n>` (alternative ultimate; no operator has one at the moment); `BIBTags` prints the active operator's gameplay tags;
`BIBSetCampaignProgress <n>` marks campaign stages 1..n (0-104) as cleared so a test can jump ahead (stage id = level counted through all
chapters: 16 = Chapter 2 done); `BIBCheckTags` reports gameplay tags the project is missing. After changing kit data run `python3 Tools/gen_tags.py` (tags) and
`python3 Tools/gen_kit_docs.py` (docs/operator-kits.md).

**Narrator speed.** Her speed raises her walking speed (450 + (Speed - base) x 1.5 cm/s). Once Speed is above the base speed
(the starting speed: 25, Diano 125) she is also pushed forward along her facing at (Speed - base) x 1.5 cm/s; movement input
steers and brakes. Speed also rises by 25 every second she is not hit, so with no input she starts to drift about a second
after reaching the base speed (measured: 13 cm/s in the first 0.7 s at speed 25; 614 cm/s at speed 425). The cm/s per speed point and the cap are PLACEHOLDERS (`BIBTuning.h`).
When the editor is already open on the same
engine, build from the command line with `-NoHotReloadFromIDE` so the editor loads the new module.

Stage flow from a map: **Start Stage** opens the stage map. The game mode begins the stage, the team talks to the
Story NPC, the first section's Entry Door opens, and entering a section's bounds seals it and spawns its enemies.
Clearing it opens the next door. Results → **Continue** travels back to **Hub Map**.

The fallback rules are per asset:
- A screen with no widget class uses the Slate menu.
- A missing input asset uses the default key.
- A stage with no map, or a map with no sections, uses the code-built blockout.

## Greybox maps (Neo Tokyo, the Ark)
`Tools/build_greybox.sh` imports the owner's two Fab kits (SCI-FI BUILDINGS CITY KIT 1 + 2, from the Fab vault cache or
`BIB_KIT_ROOT`) into `/Game/Greybox/Kits` and builds `/Game/Greybox/Maps/L_NeoTokyo` and `/Game/Greybox/Maps/L_Ark`
(`Saved/BuildGreybox.log`). Open a map and press Play: a free-fly pawn (WASD + mouse, E/Q). Orange = climbable, yellow =
ladders, neon = sign placeholders; editor labels are hidden in game. `Content/Greybox` is generated and git-ignored.
Check: `UnrealEditor BoldInBeautyDemo.uproject -ExecCmds="py $PWD/Tools/verify_greybox.py"` (`Saved/VerifyGreybox.log`,
screenshots in `Saved/Greybox`). Details: `reports/demo/2026-10-07-greybox-neotokyo-ark.md`.

**Chapter maps 2-13** (owner level ideas, 2026-10-07): the same script also builds `/Game/Greybox/Maps/L_Ch02_Ruins` ...
`L_Ch13_SecretiveSoul` (`Tools/greybox_chapters.py`; `BIB_GREYBOX_ONLY=chapters` or `ch03,ch11`). Teal = jump up onto, blue =
swim (water volumes), white rings / frames = combat sections and gates, moving blocks = `ABIBGreyboxMover`. Chapter 11 has no
anime look and no combat. Check: `UnrealEditor BoldInBeautyDemo.uproject -ExecCmds="py $PWD/Tools/verify_chapters.py"`
(`Saved/VerifyChapters.log`). Briefs and layouts: `docs/chapter-maps.md` at the documentation root.
**Second detail pass** (`Tools/greybox_detail2.py`, owner 2026-10-07 "more details"): every combat section of chapters 2-10, 12, 13
gets cover, raised platforms (teal jump-up steps, orange climb face), themed props, ground tiles and lights; routes between
sections get floor markers and a lookout tower; Neo Tokyo gets fire escapes, balconies, vertical neon and street props; the Ark's
town, operators' platform, Underworld and civilian boulevard are furnished. Pieces sit on the real floor (editor line traces).
**Elite_Bundle dressing** (`Tools/greybox_bundle.py`, owner 2026-10-08 "fill up the 13 main chapter levels and arc with those
assets"): run `Tools/import_elite_bundle.sh` once (copies the owner's `Elite_Bundle` from the documentation root, or a path /
`BIB_ELITE_BUNDLE`, to `Content/Elite_Bundle`, about 2 GB, git-ignored), then `Tools/build_greybox.sh`. The last pass of every map
places themed vignettes and clutter from the bundle's ~300 usable meshes (docks gear in Docks, graves and dead trees in The Ruins,
books and cabinets in T.O.M.B.S, market food at Merchant Row ...), on real floors, never overlapping anything that blocks, with the
middle of each fight left open. Chapter 10 is recoloured pastel (grave scans stay real), Chapter 11 is all flat grey. Meshes whose
materials are missing from the bundle get a palette colour. Without the bundle the maps build as before (a warning in the log).

## Anime style (12 fps visuals, thick lines)
Every gameplay map holds one `BIBAnimeStyleVolume` (`BIBGame/BIBAnimeStyle.h`), the Ark included; `L_Hub` (the boot map) has
none, and the game mode spawns one there only while a code-built stage runs (chapters 2-13, tower, arena, Boss Rush).
**Owner follow-up 2026-10-07:** no extreme white light and keep the colours: highlights are soft-clipped below `WhiteCeiling`
(0.84) with their hue kept and bleached areas re-saturated (`WhiteKnee`, `ChannelCeiling`, `BrightSaturation`), bloom is weak
(0.25 from 2.5), wet streets are a sheen (roughness 0.24 decal, 0.2 road) instead of a sky mirror, and overcast exposure lifts
less. More shake and shading on the **objects, not the camera**: each surface's drawing boils with noise fixed to its world
position (`ObjectShake` 1.4 px, `ObjectBoilScale` 140 cm, 8 times a second) while the camera, sky and HUD stay still; form
shading (`FormShade`) darkens surfaces turning away from the view and contact shading (`ContactShade`) darkens creases and the
ground under things. The volume blends
`MI_BIBAnimeStyle` (ink lines, cel bands, cool shadows; built by `build_assets.py`, tune its parameters) and holds the finished
3D image so it updates 12 times a second while the game runs at full rate (since 2026-10-08 the frames in between are not
rendered at all, see "Performance and graphics settings"). HUD and menus stay smooth; damage numbers step with
the world. `bib.VisualStepRate <n>` overrides the rate (0 = smooth, -1 = the volume's `VisualStepRate`).
Check: `UnrealEditor BoldInBeautyDemo.uproject -BIBDev -ExecCmds="py $PWD/Tools/verify_anime_style.py"` (`Saved/VerifyAnimeStyle.log`,
screenshots in `Saved/AnimeStyle`). Details: `reports/demo/2026-10-07-anime-style-volume.md`.

**Standalone Mac build:** `Tools/package_mac.sh [out dir]` (cooks every map incl. the greybox maps, stages, copies the whole
staged folder; start `<out>/BoldInBeautyDemo-Mac/BoldInBeautyDemo.app`).

## Neo Tokyo under the levels (free roam)
Campaign tab > **Neo Tokyo** (after the 13 chapters) opens `L_NeoTokyo` with the demo's own game mode (`?game=`), so the team
walks the greybox city with the normal character and camera; no story, sections or enemies. Pause > Abandon stage returns to the
Ark (also from the Ark menu: Missions > Neo Tokyo). The map must be built on the machine (`Tools/build_greybox.sh`); otherwise the tile says so.

## Day / night and weather
`ABIBWeatherSystem` (`BIBGame/BIBWeather.h`) runs in every playable 3D level (the same places as the anime look: stage maps, Neo
Tokyo, code-built stages; not the menus or the 2D arcade games). One clock for the whole session: **24 real minutes = 1 day**
(1 minute = 1 game hour), dawn ~04:30-07:00, day, dusk ~17:20-19:50, night. While on it replaces the level's sun / sky light /
atmosphere / fog with its own (restored when it switches off): sun by day and moon by night, real-time sky light, height +
volumetric fog, the stylised sky `M_BIBSky` (gradient, sun, moon, twinkling stars, Milky Way, shooting stars, cel clouds) and a
time-of-day grade. Weather changes by itself every 2-5 game hours (Neo Tokyo rains far more often): light / medium / heavy rain,
thunderstorm (lightning bolts, flashes, delayed thunder), calm / light / medium / heavy wind (anime wind lines, rain slant, cloud
speed). Rain stops under roofs (an overhead depth capture), splashes on whatever it hits, and wets everything incl. characters'
clothes: darker, glossy surfaces, drips running down walls, puddles with ripples that keep reflecting for hours after the rain.
Assets: `Tools/build_weather.sh` builds `/Game/Weather` (MPC, materials, sounds synthesised into `Art/Weather/*.wav`).
Console: `bib.Weather.Hour 18.5`, `bib.Weather.Rain 0-3`, `bib.Weather.Thunder 0|1`, `bib.Weather.Wind 0-3`,
`bib.Weather.Auto 1`, `bib.Weather.TimeScale 60`, `bib.Weather.Strike`, `bib.Weather.Clock 0|1` (the corner readout),
`bib.Weather.Report`; look tuning `bib.Weather.ExposureOffset`, `bib.Weather.LightScale`.
Check: `UnrealEditor BoldInBeautyDemo.uproject -BIBDev -ExecCmds="py $PWD/Tools/verify_weather.py"` (`Saved/VerifyWeather.log`,
screenshots in `Saved/Weather`). Details: `reports/demo/2026-10-07-weather-day-night-neotokyo.md`.

## Performance and graphics settings (2026-10-08)
Owner report: "it runs at 3 fps". **Exactly 3 fps is the editor's background throttle:** with Editor Preferences > Performance >
"Use Less CPU when in Background" on, Unreal caps the editor and Play in Editor at 3 fps (`UEditorEngine::GetMaxTickRate`)
whenever another app has focus (it is on in this Mac's engine-wide `EditorSettings.ini`, which overrides any project file).
The game module now asks the editor not to throttle while a play session runs (`BoldInBeautyDemo.cpp`,
`GEditor->ShouldDisableCPUThrottlingDelegates`; `bib.Editor.FullSpeedPlayInBackground 0` gives the preference back); an idle
editor still saves CPU in the background.
- **Held frames are no longer rendered.** The anime look shows a new image 12 times a second; the other 48 frames used to be
  rendered in full and then covered with the kept image. Now they skip the 3D scene completely (`SetupViewFamily` turns the
  family's Rendering show flag off) and the kept image goes straight to the viewport, so the GPU draws the world 12 times a
  second. Game, animation, physics, input and HUD still run every frame. `bib.Gfx.SkipHeldFrames 0` restores the old behaviour.
- **Weather** only draws what is visible: the three wet / puddle decals switch off while everything is dry, and the splash /
  streak mesh is not rebuilt while empty.
- **Settings > Graphics** (saved with the profile, applied at once): Performance mode (one tap: Low, 70% render scale, light
  weather, no volumetric fog), Auto-detect (the engine benchmark), Quality preset, render scale 30-100% (now applied as the real
  percentage), dynamic resolution, frame-rate limit, v-sync, window mode, frame-rate counter, then each engine group on its own
  (view distance, shadows, lighting / Lumen GI, reflections, post-processing, textures, effects, anti-aliasing, foliage,
  shading: follow the preset or Low..Epic), **Weather effects** (Off = sky, light and sound only; Low = rain sheets without
  splashes, wet decals or puddles; Full), **Volumetric fog**, **Anime look** (off = plain image, no 12 fps hold).
  Packaged / `-game`: through `UGameUserSettings`. PIE: applied to the editor session only once a graphics option differs from
  the defaults, so the editor keeps its own scalability otherwise. Console: `bib.Gfx.WeatherDetail 0-2`, `bib.Gfx.AnimeLook 0|1`.
- **Measure:** `UnrealEditor BoldInBeautyDemo.uproject -BIBDev -BIBFreeRoam -ExecCmds="py $PWD/Tools/perf_capture.py"` with
  `BIB_PERF_SCENES="ark,stage2,stage1,neotokyo,ch08"` (frame times + CSV profile per scene, `Saved/Perf/<tag>.log`);
  `-BIBFreeRoam` lets `<map>?game=/Script/BoldInBeautyDemo.BIBGameMode` walk any greybox map, chapter maps included.
  Details and numbers: `reports/demo/2026-10-08-performance-graphics-settings.md`.

## Placeholders

MAIN marks many values OPEN or DEFERRED: level costs, armour %, enemy stats, ult charge rate, cooldowns the files
don't give, merchant prices and rewards. The demo needs *some* number for each of these. Every such number is labelled
`PLACEHOLDER` in one of two files:
- `Source/BoldInBeautyDemo/BIBRules/BIBConfig.h` — economy and progression
- `Source/BoldInBeautyDemo/BIBGame/BIBTuning.h` — combat

Change them there. Numbers labelled `SOURCE` come from MAIN or the operator files. Story dialogue is a placeholder
for the owner to write.

## Code layout

- `BIBRules/` — engine-independent rules: summoning, pity, Mileage, currencies, progression, equipment and team.
  Unit-tested by `Tests/run_core_tests.sh` (103 core + 13,000+ hub + 2,419 operator-kit + 4,000+ tester checks, including 2-million-roll rate checks).
- `BIBGame/`
  - `BIBPlayerCharacter` plus `BIBKitLuna` / `BIBKitJudge` / `BIBKitNarrator` — the three hand-written kits; `BIBPlayerGAS` (Gameplay Ability
    System plumbing), `BIBPlayerData` (the 17 data kits) and `BIBKitWorld` (the `IWorld` adapter the data kits run on)
  - `BIBPlayerController` + `BIBInput` — Enhanced Input state and UI hosting (UMG or Slate)
  - `BIBBlueprintLibrary` — Blueprint/UMG API and `BIBUserWidget`
  - `BIBLevelActors` — `BIBSection`, `BIBDoor`, `BIBStoryNPC` for stage maps
  - `BIBEnemy` — enemies
  - `BIBGameMode` — stage flow and level building
  - `BIBGameInstance` — the profile
  - `BIBBlock` — coloured basic shapes
  - `BIBEnvironment` — lights
- `BIBGAS/` — attribute set, ability system component, code-built GameplayEffects, the one kit-slot GameplayAbility and the tag lookups.
- `BIBRules/BIBKit*.{h,cpp}` — the operator kit data model, runtime, status book and the 17 kit tables (pure C++; `Tests/kit_tests.cpp`, `Tests/kit_balance.cpp`).
- `BIBUI/BIBUI.cpp` — fallback Slate HUD and menus.

Ability effects use debug-draw circles and spheres, which show in editor and Development builds but not in Shipping builds.
