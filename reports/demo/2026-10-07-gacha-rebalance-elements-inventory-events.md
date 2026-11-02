# Demo: classic gacha rebalance, element system, inventory and boosts, movesets, events and three new arcade games — 2026-10-07

**Owner request (2026-10-07, verbatim summary):** "(1) rebalance the game for more classic gacha game standards like NIKKE or ZZZ;
(2) start fleshing out the character movesets to be more fun and unique yet still balanced; (3) add a few more simple events and
arcade mini games that fit the BB style; (4) flesh out the inventory with more currencies and 3, 6, 12 and 24 hour currency boosts
which correlate with the AFK rewards from the Outer World; also implement the element system that works a bit like NIKKE's system,
with 8 types: dark, nature, water, fire, magic, and 3 other types you can make up. You can add extra inventory stuff and currencies
but they must have practical use and fit the balancing."

- **Executors:** Claude Code, Opus 5.5 (`claude-opus-5-5`), owner's Mac (MACOS-LOCAL), Claude Pro. One lead session (rebalance,
  elements, inventory, events, integration, Unreal build, PIE) and two worker sessions in separate git worktrees: movesets
  (branch `claude/movesets`, c579bb6) and arcade mini-games (`claude/minigames`, dc7665f), both merged into the lead branch.
- **Branch:** `claude/gacha-elements-events`, based on `claude/arcade-gangs-arena` @ `8ae5f03`. Worktree
  `BoldInBeauty-Project/worktrees/ai-agent-bb`. Not merged to `main`, not pushed at the time of writing.
- **Status labels:** everything numeric is **PLACEHOLDER**; every new design element (names, element assignments, event designs,
  kit changes) is **PROPOSED**. The owner's earlier explicit numbers were kept: 300 starting Diamonds and nothing else, Chapter 1
  level 1 = 800 Gold / 250 Bytes / 2 hearts per clear and 50 Diamonds on the first clear, 10 Regular tickets for Chapter 1's end and
  1 per later chapter, the summon rates, pity (100 / 2,000), 50/50, Mileage (250 / 2,500), 300 Diamonds a pull, 2,700 a ten-pull.

## 1. Classic gacha rebalance (assistant's reading of "like NIKKE or ZZZ")
The summon rules are the owner's (MAIN §2) and were **not** changed. What changed is the economy around them:

**Pull income of a free player per 30-day month after the campaign** (`Tests/gacha_tests --budget`; Daily-banner tickets excluded,
they have no pity or Mileage like NIKKE's social recruit):

| Source | Before (2026-10-06) | After |
|---|---|---|
| Daily quests | 2.0 | 8.0 (80 Diamonds a day) |
| Weekly quests | 10.2 | 11.4 (200 Diamonds, 1 Regular, 1 Special a week) |
| 7-day login | 10.0 | 10.0 (smaller Diamonds, boosts / Charge / cores on the other days) |
| Season pass (free) | 14.2 | 8.3 (5 Regular, 2 Special, 400 Diamonds + cores, boosts, Charge) |
| Event shop (new) | - | 10.0 (1 Special, 1 Regular, 100 Diamonds a week from Event tokens) |
| Ark shop (new) | - | 3.0 (1 Special, 2 Regular a month from Ark medals) |
| Arena | 1.6 | 4.1 (10 Diamonds a win, Ark medals) |
| **Total** | **~38 (only ~14 usable on featured banners)** | **~55 (~34 usable on featured banners)** |

With the owner's 2% / pity-100 / 50-50 rules a featured 5-star costs about 65 pulls on average, so a free player now lands one
every one to two months with the 250-Mileage spark as a backstop — the NIKKE / ZZZ rhythm. `gacha_tests` keeps the total in 40-60.
**Paid side (simulated, no real payment):** Diamond packs rescaled to about $2.80 a pull at the top ($0.99 = 90 ... $99.99 =
10,800); new **Monthly supply** ($4.99: 300 Diamonds now + 100 a day by mail for 30 days, the best value as in both references).

**Charge (stamina, ZZZ-style) and Quick clear (sweep):** first clears stay free; replaying a cleared level for its Gold / Bytes /
Angel hearts costs 10 / 15 / 20 Charge (chapters 1-4 / 5-9 / 10-13). Charge refills 1 per 6 minutes up to 240 (real clock), from
Charge packs (+60, may exceed the cap) and up to 3 Diamond refills a day (50 / 100 / 150 Diamonds for +60). A cleared level can be
**quick-cleared** x1 or x5 from the chapter page: same rewards, no fight. `Config::UseCharge = false` turns Charge off.

**Pacing** (`Tests/playthrough.cpp`, 10 runs per style; the bot now spends Charge, quick-clears, claims events and uses boosts):

| Style | Before (days to finish 13 chapters) | After |
|---|---|---|
| casual 45 min/day | 25 | 24 |
| regular 120 min/day | 9 | 23 |
| hardcore 300 min/day (3 Diamond refills a day) | 4 | 14 (p90 37: upgrade-star walls on bad summon luck) |
| saver (keeps Diamonds) | 9 | 23 (p90 66) |

Progress is now gated by the daily Charge like ZZZ instead of by play time: every daily style finishes in about 3-4 weeks and
nobody can no-life it in 4 days. The campaign model asks for about 5 quick clears per level and 12 before a chapter boss
(`Tests/campaign_model.h ReplaysNormal / ReplaysChapterEnd`, was 1 / 2.5); `BIBCampaignData.inc` and `docs/campaign.md` regenerated.

**Outer World = the AFK system:** runs on the real clock (was 1 real minute = 1 world hour for demo testing; the tester panel
still skips hours), production +1% per campaign level cleared (NIKKE outpost-style), 12 h storage unchanged.

## 2. Movesets (worker session; all PROPOSED, no owner-written ability or passive touched)
Luna, Judge and Narrator (owner kits) and every SOURCE ability of Alchemist, Arita and Adapt are unchanged. The 14 assistant-
designed kits got a signature loop each, e.g. **Plack** Momentum (combos / Rush / parry build it, Spinning Kick spends it), **Kuu**
Grit (soaks hits during Bulwark, Ground Pound spends it, 3rd M1 hit stuns), **Media** Data meter and the Buffer Overflow passive,
**Death** Souls that Tithe spends and the Reaper's Due passive, **Stargazer** spends all starlight on Falling Star (banking is a
choice), **Myrcia** tidal soak/counter, **Shiraishi** Heat spend, **Fujita** bleed bursts, **Ishii** a Shock / Burn / Wet bolt cycle.
New generic step option `BonusStatus` / `BonusVs` (extra damage against an enemy with a status), rules code only.
**Balance:** `kit_balance.cpp` now prints an *effective power* (damage x survival, 5-star median = 1.00) with rarity bands
3* 0.55-0.75, 4* 0.75-0.90, 5* 0.90-1.10, 6* 1.10-1.30. Before: 15 of 17 data kits outside their band; after: only Alchemist
(0.66; her owner mechanics — the 75 s infection explosion, Ring leader illusions — are not modelled). Full tables and per-ability
text: `docs/operator-kits.md`. Plack and Kuu rose to their band (with-ultimate ST dps 1,910 -> 3,695 and 801 -> 1,799), so the
campaign model's reference team dps was updated (the enemy tables are relative and did not move).

## 3. Events and arcade mini-games
**Events** (`BIBRules/BIBEvents.*`, Lobby > Events; the old placeholder event list is gone):
- **Rookie week** — a new account's first 7 days, 3 beginner missions unlock a day: 20 Regular, 5 Special tickets, 2,000 Diamonds
  plus boosts, cores, Ark medals (classic beginner missions; older accounts do not get it).
- **Weekly operation** — alternates **Angel Hunt** (combat missions) and **Arcade Fever** (arcade missions, +2 Arcade tokens a day):
  8 missions, 1,400 Event tokens a week, an **event shop** with weekly limits (~2,000 tokens of goods, so the player chooses).
- **Overtime weekend** — days 6-7 of every week: the Outer World produces twice as fast and boosts pay 50% more.
- **Element Surge** — one element a week (rotating): clears with an operator of that element in the squad pay 3 Element cores
  (6 on a first clear) and its Attunement costs 30% fewer cores.
- **Ark shop** — Ark medals (30 per arena win, 10 per loss, new tower floors, arena season reward) buy a Special ticket, Regular
  tickets, cores, 24 h boosts, Charge packs and gear, monthly limits.

**Arcade mini-games** (worker session, `BIBRules/BIBMiniGames.*`, drawn in `BIBArcadeOverlay`), 1 Arcade token a play, Easy /
Medium / Hard, best score and stars saved, rewards on the Rhythm scale (loss 300 Gold; win 1,500 / 3,000 / 5,000 Gold and Bytes,
hearts at 3+ stars, Diamonds at 5 stars):
- **Halo Fall** — angel shooter: formations of angels descend firing light bolts, a golden Seraph crosses for bonus points.
- **Neon Memory** — memory match with the operators' portraits (4x3, 4x4, 6x4).
- **Firewall Breach** — timing hack: stop the cursor inside a shrinking port, Perfect / Good grading, integrity points.
Bot win rates Easy / Medium / Hard: Halo Fall 100 / 66 / 24%, Neon Memory 100 / 98 / 50%, Firewall Breach 100 / 83 / 7%.

## 4. Inventory, currencies and boosts (`BIBRules/BIBInventory.*`, Lobby > Inventory)
- **Outer World boosts** — Gold, Bytes and Angel-heart boosts of **3, 6, 12 and 24 hours** (12 items): using one pays that many
  hours of the Outer World's current production at once (NIKKE / AFK "credit cases"), so they grow with the buildings and the
  campaign; a floor rate (150 Gold, 50 Bytes, 0.5 hearts an hour) keeps early boosts useful. Sources: dailies, weeklies, login,
  season pass, Rookie week, event and Ark shops. "Use all boosts" button.
- **New currencies, each with one clear job:** **Charge** (campaign replays), **Element cores** (Attunement), **Event tokens**
  (event shop), **Ark medals** (Ark shop). **Charge pack** item (+60 Charge). The inventory page lists every currency with what
  it is for; the top bar can show any of the 14 currencies.

## 5. Element system (`BIBRules/BIBElement.*`)
- **Eight elements:** the owner's Fire, Water, Nature, Dark, Magic plus three PROPOSED to fit the setting: **Volt** (Neo Tokyo's
  machines and speed), **Halo** (the angels' light), **Glitch** (corrupted data, the 49% robot / Bytes side of the operators).
- **NIKKE-style "code weakness":** two rings of four — Fire > Nature > Volt > Water > Fire and Halo > Dark > Glitch > Magic > Halo.
  An advantaged hit deals **+15%** (NIKKE: +10%); no penalty otherwise, so elements reward a good team, never lock one out.
- **Attunement** (Operator tab): levels 0-10 per operator, +3% advantage damage a level (+45% in all at 10), 10 x level Element
  cores + 2,500 x level Gold (550 cores for a full one), +2% battle power a level.
- **Operators (PROPOSED from their kits' themes):** Fire Shiraishi, Plack, Gambler; Water Myrcia, Blood, Weather; Nature Arita, Kuu,
  Alchemist; Volt Fujita, Narrator; Dark Luna, Death; Halo Judge, Stargazer; Magic Ishii, Jester, Timekeeper; Glitch Media, Adapt.
- **Enemies:** Chapter 1 none (tutorial); chapters 2-9 one element each (Nature, Glitch, Water, Fire, Volt, Halo, Dark, Magic, so the
  starting trio meets enemies it counters early), chapters 10-13 alternate two elements by section; Ark Tower floors from 5 rotate
  every 5 floors; Boss Rush rotates per round. In Unreal the enemy label shows `[Fire] Brute ...`, an advantaged hit gets the bonus
  and the big damage number. The arena AI fight applies the same advantage. The campaign chapter page names the enemy element,
  the counter element and how many of your squad have the advantage.

## Verification (MACOS-LOCAL, 2026-10-07, UE 5.8.3, Apple clang)
- `Tests/run_core_tests.sh`: 103 core, 12,714 meta, 2,454 kit, 4,450 account, 4,063 campaign, 4,380 tester, 440 arcade / arena /
  live-ops / online, 59 mini-game and **230 new gacha / element / inventory / event checks, 0 failures**; playthrough bot: no
  soft-locks. The campaign tables were regenerated and the generator scripts (`gen_campaign_data.py`, `gen_tags.py`,
  `gen_kit_docs.py`) were fixed: their source lists had been incomplete since the arcade iteration, so they no longer linked.
- Editor build `BoldInBeautyDemoEditor Mac Development -NoHotReloadFromIDE`: **Succeeded, 0 errors, 0 warnings** (one compile
  error fixed on the way). 20 new icons imported with `Tools/import_art.py` (`STATUS OK: 20 of 20`).
- PIE: see below.

## PIE results (MACOS-LOCAL, 2026-10-07, editor build of this branch)
- `verify_gacha.py -BIBDev` (new): **PASS 16/16** — lobby tiles; inventory (13 items, tester +5 items, a 24 h Gold boost paid
  +3,600, Charge 240); element guide (8); events hub, weekly operation (8 missions), event shop (10); Ark shop (8); Luna's
  Attunement tile; Chapter 1 quick clear spent 10 Charge and paid; **Boss Rush round 1 boss carries an element and a Water squad's
  1,000-damage hit landed for 1,150 (+15%) in real combat.**
- `verify_hub.py -BIBDev`: **PASS 74/74**. `verify_tester.py -BIBGuest`: **PASS 32/32**.
- `verify_arcade.py -BIBDev`: **PASS 46/46** after one script fix (the dev profile spent its 5 daily Arcade tokens before the three
  new games; the script now tops up through the tester panel). Halo Fall, Neon Memory and Firewall Breach each cost a token,
  cleared, saved a record and paid; Boss Rush rounds 1 -> 2 still pass.
- `verify_kits.py -BIBDev` (all 20 operators on live enemies): **155 OK, 19 flagged (RESULT FAIL).** Every ability of the 17 data
  kits passed on its own cast. 18 flags are harness artefacts: the tested operator went down or swapped out, Luna stepped in and
  cleared the room, and the script then cast her "form 2" abilities with 0 enemies left (or with the Judge already down). One flag
  is the owner's Narrator kit: her basic attack showed no effect once with 10 enemies in range (not touched in this iteration;
  whether it predates it is OPEN — the last full sweep, 919400f, was 126 OK / 0 flagged). Follow-up: make the sweep respawn
  enemies / restore the tested operator before a "form 2" pass, then re-check Narrator's basic.

## Owner decisions (2026-10-07, after review)
Approved by the owner: Charge (stamina) exists; Volt, Halo, Glitch and every operator's element; the 3-4 week campaign pace; the
kit changes; the event and arcade game designs. These are no longer PROPOSED. Amounts, prices and rates remain PLACEHOLDER.

## Not verified / OPEN (owner decisions)
- **Nobody has played any of it.** How Charge pacing, the elements and the new kits feel is the owner's call.
- Owner decisions: the three new element names and every operator's element; whether stamina (Charge) belongs in Boldinbeauty at
  all (`Config::UseCharge`); the pull budget (~55 a month) and every reward / price; the 3-4 week campaign pace; the event designs
  and names; the kit changes; Alchemist below her band; Luna (owner kit) still out-damages every 5-6 star on one target; the
  mini-game designs and names.
- The account server does not know the new reward fields (admin mail cannot send Element cores, items etc. yet); arena defence
  lineups from the server carry no Attunement (treated as 0).
- Windows: OPEN.
