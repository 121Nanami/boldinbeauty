# Demo: arcade (Night Shift, Boss Rush, Rhythm), the Ark (endless tower x2, arena), Outer World, gangs, friends, live ops — 2026-10-06/07

**Owner request (2026-10-06, summary):** three arcade games: (1) a FNAF-like night with Luna, Plack, Kuu, Media and Narrator
(Luna / Plack like Bonnie / Chica to the left / right door; Kuu goes to either door and winds up a punch for 5 s, breaks a closed
door, misses an open doorway; Media teleports to a doorway with panning audio, close it within 2 / 1.5 / 1 s or lose; Narrator like
Freddy, watching her camera pauses her, at the door she never leaves; 100% power, 2 doors, Easy 50% every 7 s, Medium 75% every
6 s, Hard 100% every 5 s); (2) Boss Rush, boss after boss until you lose, with rewards; (3) a simple Guitar Hero in the Boldinbeauty
look. Gangs of 30 with join requirements, gang rewards and gang currency at the end of a monthly season for a gang shop. Returning
players (14 days away): 7 days of 3 rewards plus Returned quests worth 5 tickets, 600 Diamonds and some loot. Season pass balanced to
about 15 days with all quests, lasting a month. The Outer World (build with resources, not Diamonds; passive Gold / Bytes / Angel
hearts). The Ark with: a semi-online arena (attack and defence teams, AI fights another player's lineup, trophies, Bronze-Silver-Gold-
Diamond-Platinum 1-3 and Champions, drop 3-4 ranks each monthly season, Champions cannot fall below Champions, matchmaking by
strength then rank) and the infinite tower with two modes (3-4 star and 5-6 star), never resets, saves where you fell. Battle power
next to operators, a friends menu, an admin toggle to mail one user or all users, no beginning mail, Nikke-style notices, lobby
customisation (which currencies show) and preferences such as key binds. "Most is up for your interpretation."

- **Executors:** Claude Code, Opus 5.5 (`claude-opus-5-5`), owner's Mac (MACOS-LOCAL), Claude Pro. Session 1 (2026-10-06) wrote the
  code; a subagent wrote the server routes. Session 2 (2026-10-07, this report) rebuilt, imported the icons, ran the PIE checks,
  wrote this report and committed.
- **Branch:** `claude/arcade-gangs-arena`, based on `claude/rebalance-quests-ui` @ `72dd923`. Worktree `<local-home>/Documents/BIB-arcade-wt`.
- Everything numeric the owner did not state is **PLACEHOLDER** and reversible.

## What exists now
### Arcade (Lobby > Arcade, 1 arcade token a play)
- **Night Shift** (`BIBRules/BIBArcade.*`, drawn by `BIBGame/BIBArcadeOverlay.*`): office with two doors and lights, 10 cameras with a
  map, power 100%, 12 AM-6 AM. The owner's rules as given; PLACEHOLDER: 50 s per hour (5-minute night), 15 s grace at the start,
  FNAF power drain (each door, light or camera adds usage), Kuu's broken door repairs in 15 s, Media teleports after 6 successful
  rolls, Narrator needs 2 successful rolls per room. Default keys: A / D doors, hold Q / E lights, Space camera, 1-0 pick a camera. The panning
  audio is generated (`BIBSynth.*`, no sound assets yet). A bot in `Tests/arcade_tests.cpp` plays hundreds of nights per mode:
  Easy almost always survivable, Hard rarely.
- **Boss Rush:** the real 3D combat; bosses come one after another in the same arena, the team heals 35% between rounds, every round
  pays when cleared, the best round is saved.
- **Rhythm:** a 5-lane note highway (Perfect / Great / Good windows 50 / 100 / 150 ms), score, combo, stars, best score saved.

### The Ark (Revolution tab)
- **Endless tower, two modes:** Operator mode (3-4 star operators) and Secrets mode (5-6 star). Never resets; first clear of a floor
  pays (Gold 400 + 80 x floor, Bytes 125 + 25 x floor, Secrets mode +25%, PLACEHOLDER); the floor you fell on is saved and shown.
  The team auto-fills from eligible operators.
- **Arena (semi-online):** set an Attack and a Defence team; Fight matches you against another player's defence (the server picks
  similar battle power first, then similar rank; guests and offline players fight a generated practice rival of similar strength);
  both sides are AI-controlled and the fight is replayed (Space skips). Trophies are zero-sum (10-50 per fight); 100 trophies a rank
  (PLACEHOLDER); ranks Bronze 1 ... Platinum 3, Champions in the owner's order; monthly season drop 3 ranks (4 from Diamond 1 up);
  Champions floor during the season; season reward by rank; 5 attempts a day (PLACEHOLDER).
- **Battle power** (`BIBArena.*`): one number per operator from health, attack and defence after level, stars and equipment,
  weighted by rarity (formula PLACEHOLDER); shown on operator and team screens and used for matchmaking.

### Outer World
Eight buildings built and upgraded (to level 10) with **Gold and Bytes only**; they produce Gold / Bytes / Angel hearts per hour,
store up to 12 hours and keep producing while the game is closed (`BIBLiveOps`).

### Online features (`BIBRules/BIBOnline.*`, `BIBGame/BIBOnlineSubsystem.*`, server `Server/src/social.js`, contract `docs/online-api.md`)
- **Friends:** search by user id or username, requests, accept / decline / remove (max 50).
- **Gangs:** up to 30 members, minimum level / battle power and open / closed settings, leader kick, donations (1,000 Gold = 10
  points), 5 gang milestones, monthly gang season rewards paid in **Gang coins**, a gang shop. Creating a gang costs 20,000 Gold.
- **Admin mail:** the admin account gets an Admin page with a toggle: one user (id or name) or all users. **No beginning mail** is
  sent any more.
- **Notices:** Nikke-style notice board (4 categories), opens at login when there are unread notices; the admin can post them.

### Live ops (`BIBRules/BIBLiveOps.*`), real UTC clock
- Seasons are calendar months (season pass, arena, gangs). **Season pass** now fills only from daily and weekly quests (240 a day +
  700 a week): tier 50 on day 15 with every quest done; it resets monthly.
- **Returning players:** away 14+ days -> 7-day event, 3 rewards a check-in day, six Returned quests totalling 5 Regular tickets,
  600 Diamonds and some loot.
- Daily reset follows the UTC day.

### Lobby customisation and preferences
- Top bar: six slots, each cycles through every currency (Gang coins included).
- Settings > **Key bindings**: rebind every battle, Night Shift and Rhythm key with key capture (a clash swaps the two keys;
  Rhythm lanes default D F J K L). New preferences: **damage numbers** and **camera shake** on heavy hits (both new in the battle
  code so the switches really work), text size 75-125% for the battle HUD and the arcade.
- Commander name by text prompt.

## Verification (MACOS-LOCAL, 2026-10-07, UE 5.8.3, Apple clang 21)
- `Tests/run_core_tests.sh`: **103 core, 10,612 meta, 2,419 kit, 4,439 account, 4,061 campaign, 4,272 tester, 440 arcade / arena /
  live-ops / online checks, 0 failures**; playthrough bot, no soft-locks.
- Server `npm test` (memory store): **118/118**. Session 1 also reports 190/190 against a local MongoDB (not re-run on 2026-10-07).
- Editor build `BoldInBeautyDemoEditor Mac Development -NoHotReloadFromIDE`: **Succeeded, 0 errors, 0 warnings** (incremental
  after the last game-mode edit).
- Icons: 9 new PNGs imported with `Tools/import_art.py` (`STATUS OK: 9 of 9`), the rest of `Content/BIB` untouched.
- PIE `verify_hub.py -BIBDev`: **RESULT PASS, 74 OK / 0 FAIL**, 0 missing textures after 7 tabs, 27 pages, 20 operators, 4 banners.
- PIE `verify_arcade.py -BIBDev` (new): **RESULT PASS, 30 OK / 0 FAIL**: lobby entries; friends and gang pages offline; Night Shift
  doors, camera, cameras, Media's window, a survived Easy night paid, Media getting in on Hard after 1 s; an autoplayed Rhythm song
  (57,200, 5 stars); text prompt; key capture rebinding Jump; arena practice rival, replay, skip, trophies; tower fell-on-floor
  record; Boss Rush round 1 cleared and paid, damage numbers, round 2 boss in the same arena, run summary.
- PIE `verify_tester.py -BIBGuest`: **RESULT PASS, 32/32**.

## Not verified / OPEN
- **Nobody has played these.** How Night Shift, Rhythm and Boss Rush feel is the owner's call; every placeholder number above too.
- The online features were not run against the live account server from inside the game (the pages were opened offline in PIE;
  the routes pass the server tests). The server on the owner's Mac must be restarted from this branch's `Server/` to get the new
  routes; a hosted copy (`render-deploy`) would need the same update.
- Arena fights are AI vs AI simulated in the rules core and replayed; server-side battle verification is OPEN (a player could
  report a fake result).
- Sounds are generated placeholders; no music for the Rhythm songs (the notes are a generated chart).
- Windows: OPEN.
