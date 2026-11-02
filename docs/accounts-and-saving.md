# Accounts, saving, the tutorial and the 3-day banner (Unreal demo)

Owner request, 2026-10-04: a save system (summoned characters, currencies, unlocked things such as characters and levels); a
3-day timer that cycles the featured characters on the banner; a basic tutorial (Luna, upgrade, an empty squad that takes only
Luna, a forced summon of Kuu, play 1-1, then Plack unlocked for free); sign-up and log-in by e-mail verification showing the
username, e-mail and user id; and the owner e-mail `admin@example.com` as the flagged admin account (every
character, infinite items, every level). New accounts start with only Luna.

Everything below is **WORKING** (an implementation of that request) unless marked OPEN. Numbers, names and pools are PLACEHOLDERS
until the owner decides. No game design was added: the tutorial, the rotation pools and the admin entitlements are exactly the
list the owner gave, with the gaps listed under "Decisions I made / OPEN".

## Where things live

| Part | Files |
| --- | --- |
| JSON helper | `BIBRules/BIBJson.h/.cpp` (strict parser, depth limit; no Unreal) |
| Save format | `BIBRules/BIBSave.h/.cpp`: `Serialize` / `Deserialize` / `Peek`. All-or-nothing, sanitising, operators and banners saved by **name** |
| Profile additions | `BIBRules/BIBCore.h/.cpp`: `FAccountInfo`, `StartNewPlayer`, `StartAdmin`, `ApplyAccount`, `ResetForAccount`, unlimited currencies, squad rule, `SetTime` / `ApplyRotation` |
| Tutorial | `BIBRules/BIBTutorial.h/.cpp` + the hooks in `BIBPages.cpp` (`Build` decorates, `Perform` gates) |
| Account client | `BIBGame/BIBAccount.h/.cpp` (`UBIBAccountSubsystem`, HTTP) |
| Loading, autosave, cloud sync | `BIBGame/BIBGameInstance.h/.cpp` |
| Sign-in screen | `BIBUI/BIBUI.cpp` `BuildSignIn` (code-built Slate; `EBIBScreen::SignIn`) |
| Account server | `Demo/BoldInBeautyDemo/Server` (see its README) |
| Tests | `Tests/account_tests.cpp` (rules, 4,425 checks), `Server/test` (84 / 127 tests), `Tools/verify_accounts.py` (PIE) |

## The save

- **What is saved:** all Gold / Diamonds / Bytes / Angel hearts / tickets / Mileage / arcade tokens; every owned operator with level,
  upgrade stars, duplicate units and Angel-heart gates; equipment and who wears it; the squad; each banner's pity counters and the
  3-day rotation period; the tutorial step and whether the tutorial summon was used; the day, Chapter 1 levels cleared; and the whole
  meta-game (mail, quests, login streak, season pass claims, Outer World buildings and stored output, arcade, tower floors,
  arena, sub-stories, settings, name).
- **Not saved:** the account (the server decides it), the unlimited-currency flag, the random generator state, the hub's open tab.
- **Where:** `Saved/BIB/Saves/<user id>.json` (a guest uses `guest.json`), written to a temporary file first, the previous copy kept as
  `.bak`, then moved into place. A signed-in player's save is also uploaded to the account server.
- **When:** after every hub action (summon, level-up, purchase...), when a stage is cleared, every 10 s while playing (the Outer World
  keeps producing), when signing out and when the game closes. The cloud copy follows at most every 15 s and when signing out.
- **Which copy wins:** on sign-in the newer of the local and cloud save (by `savedAt`) is loaded; a tie goes to the local file. A save
  written for another account is never loaded. A broken file falls back to its `.bak`, then to the other copy, then to a new game.
- **Several devices:** last writer wins. If the cloud copy is newer the game says so; the next change made on this device is newer
  still and replaces it. (OPEN: the owner may prefer to be asked.)

## New accounts and the tutorial

A new account owns only Luna (MAIN section 5: Luna is granted automatically) with an **empty squad**, the normal placeholder
wallet, and the tutorial pending. While a step is active, the hub allows only that step's action (everything else is greyed out and
refused with the instruction), the next tab is gold, and the instruction is the first line of every page:

1. Open the Operator tab. 2. Tap Luna. 3. Level Luna up once (Gold + Bytes). 4. Open the Team tab. 5. Tap an empty slot: only Luna
can be added (the squad is tidied so she leads). 6. Open the Summon tab. 7. The forced summon, **Kuu** (the existing tutorial
summon, MAIN section 5); Kuu joins the squad. 8. Open the Campaign tab. 9. Open Chapter 1 and play level 1; clearing it unlocks
**Plack** for free, who joins the squad. The step is saved, so quitting half-way resumes there.

The admin account and the developer profile have no tutorial.

## The 3-day featured banner

The two Featured (Special) banners rotate their featured operator every 3 days, computed from the UTC clock (`Config::RotationAnchorUnix`
= 2026-10-04 00:00 UTC), so it needs no server, is the same for everyone and survives save/load. Period 0 shows the owner's launch
picks, Judge (5*) and Narrator (6*); the Summon page shows the countdown. When the featured operator changes, that banner's pity
resets (WORKING: MAIN section 2 says 5-star pity resets each banner). Mileage redeems whoever is featured now.

## The admin account

The server flags the owner e-mail (`ADMIN_EMAILS`); the game then gives that account: all 20 operators, every currency reading
999,999,999 and never decreasing (also duplicate units and Mileage, so upgrade stars, heart gates, equipment upgrades and summons
are free), six pieces of equipment per slot, all Chapter 1 levels and sub-stories open in any order, no tutorial, and the rules accept any tower floor (the Tower page still offers the next one).
The flag is never read from a save file and never given to a guest or to a session that could not reach the server.
The test hooks that change the profile (`BIBTeam`, `BIBGrantAll`, `BIBSetCampaignProgress`) work only for the admin or with `-BIBDev`.

## Launch options

| Option | Effect |
| --- | --- |
| *(none)* | Sign-in screen first. |
| `-BIBGuest` | Skips the sign-in screen: a local guest, saved to `guest.json`. |
| `-BIBDev` | The old developer profile: every operator, Luna / Judge / Narrator, no sign-in, nothing saved. **Use this for `verify_hub.py`, `verify_kits.py`, `verify_game.py`.** |
| `-BIBApi=<url>` | Account server address (else `[BoldInBeauty] ApiBaseUrl` in `DefaultGame.ini`). |

## Decisions I made (reversible) and OPEN items for the owner

1. **Rotation pools (OPEN, PLACEHOLDER).** MAIN says eligible pools are OPEN. The 5-star slot rotates Judge, Alchemist, Blood,
   Stargazer, Gambler, Weather; the 6-star slot Narrator, Jester, Media, Timekeeper, Death, Adapt (each rarity in roster order,
   starting with your pick). Change `Config::RotationPool5/6`.
2. **Squad size.** MAIN says bring 3 operators. A new account cannot, so the squad must be *as full as the collection allows* (three,
   or all of them while the player owns fewer than three). Kuu and Plack join the squad automatically during the tutorial.
3. **Plack twice.** MAIN section 5 says Plack comes from the Chapter 1 boss. The tutorial now gives him after level 1; the boss still
   "grants" him, so a player who already has him gets the normal duplicate bonus (+20 units). Say if the boss should stop granting him.
4. **Demo tools** (Settings: grant currency, next day, +XP...) are still open to every account. Say if they should be admin-only.
5. **Starting wallet** (owner decision 2026-10-06): a new account starts with **300 Diamonds and no other currency** (the welcome
   mail carries no attachment; the tutorial's one level-up is free). The developer profile (`-BIBDev`) keeps its old test wallet.
6. **Admin: unlimited** is a display value (999,999,999) rather than a true infinity.
7. **Windows** is not built or tested (as before).
