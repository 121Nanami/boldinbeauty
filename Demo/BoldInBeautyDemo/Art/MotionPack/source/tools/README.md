# bb-Showreel: BoldInBeauty in-game motion pack

Everything here is original, code-built motion graphics in the trailer's UI look. There are no AI-generated images and no character art. The music and sound effects are synthesized in code with numpy. Every video file is 3840x2160 at 30 fps. Audio is mixed to about -27 LUFS, following the owner's earlier request for quieter audio.

## What's in the folder

| Path | What it is |
|---|---|
| `BoldInBeauty_bb-Showreel_Master_4K.mp4` | The full reel, about 10 minutes: intro, 6 ads, transitions, 7 UI sheets, trailer, outro |
| `01_In-Game_Ads_4K/` | 6 in-game ads (mp4 with audio) |
| `02_Page_Transitions_4K/` | 5 official page transitions as **transparent ProRes 4444 .mov** files, plus a showcase video |
| `03_UI_Pack_Sheets_4K/` | 7 sheet videos that play each UI piece for 5 s, with labels |
| `UI_Pack_4K/` | **The separate file for later use.** All 49 UI pieces as transparent ProRes 4444 .mov files, one folder per sheet, each with a PNG contact sheet |
| `04_Download_Trailer_4K/` | The 2:00 download trailer |
| `src/`, `shared/`, `tools/` | Editable source (HyperFrames HTML + GSAP). See "Editing" below |

## The 6 ads
1. **Government broadcast**: obvious propaganda for the operator programme. It cuts from "WE PROFIT." to a glitch-corrected "WE PROTECT." (32 s)
2. **ASCEND™ pills**: a "legitimate" infomercial for getting healthy and buff in 1 month, at $25,000 a month (32 s)
3. **Sponsor slots 2010**: next season's operator-uniform sponsorship map, with a live auction and SOLD stamps (32 s)
4. **The Merchant**: a deliberately cheap ad for cheap gear and loot. Choppy 10 fps motion, WordArt and star wipes; prices are in Gold (24 s)
5. **Broken black board**: a dead billboard that glitches violently, flashing pieces of the other ads. It loops seamlessly (20 s)
6. **Media's private apartments**: a luxury ad with a wireframe tower and a penthouse floor plan, "by invitation only" (32 s)

## Page transitions (simple → full BB)
| # | Name | Length | Screen fully covered at (swap pages here) |
|---|---|---|---|
| 01 | Simple Tech | 0.80 s | 0.40 s |
| 02 | Cyber Slab | 1.00 s | 0.45 s |
| 03 | Stylised Tech | 1.20 s | 0.60 s |
| 04 | BB Signal | 1.35 s | 0.55 s |
| 05 | Full BB | 1.60 s | 0.75 s |

The game spec (MAIN, 2026-10-06) says page transitions are 0.3 s. These are showcase-length masters: play them at 2–5x speed in engine, or tell me a target length and I'll re-time them.

## UI pack (49 pieces, 5 s each)
- **01 Navigation & Core**: hub nav bar, primary button states, secondary/back/close, settings toggles and sliders, loading spinner, modal dialog, notice toasts, page header
- **02 Economy & Rewards**: currency bar, reward popup, shop purchase, level up, resource floaters
- **03 Summon**: banner select, hold-to-summon, rarity reveals (3★ Rare, 4★ Epic, 5★ Legendary, 6★ Secret), x10 results, ticket consume
- **04 Combat HUD**: HP bar, squad switcher, skill buttons with cooldowns, ultimate ready, damage numbers, boss intro, mini-boss alert, combo counter, block/parry/dodge, victory, defeat, target lock
- **05 Progression**: campaign node map, season pass, quest claim, the Ark floor counter, arena rank up, mail inbox, battle power
- **06 Arcade**: Night Shift power and doors, Rhythm 5 lanes, Boss Rush rounds
- **07 Mobile Controls**: joystick A (rose ring), joystick B (hex 8-way), joystick C (tactical with trail), touch action cluster, switch/dodge/block buttons, full mobile HUD layout

## Download trailer
2:00 at 128 BPM (64 bars) in a hyper-stylised sticker and comic-panel style. It ends on the same ring it starts with, so it loops while the game downloads. Every line comes from the claim ledger in `../bib-trailer/claims.md`. It carries the CONCEPT / IN DEVELOPMENT tag and has no character images.

## For the owner to approve
- **Ad copy is new in-world text.** Slogans, product names ("ASCEND"), prices, auction amounts, the merchant's items and the apartment amenities are all placeholders written for these ads. None of it is canon until the owner approves it. No government, district or faction names were invented.
- **Media is named on screen** in ad 6, as requested. No character image is shown.
- **Orientation:** the brief said "4k 2160 3840". The ads are landscape **3840x2160**. If the in-game billboards are portrait (2160x3840), they can be re-laid out.
- Ad 4 uses Impact and Comic Sans (system fonts) on purpose, for the cheap look.

## Editing
Each piece is `src/<id>.html`. Build and render a piece with `tools/render.sh <id>`, then add audio with `python3 tools/finish.py <id>`. Rebuild the UI pack sources with `python3 tools/gen_ui.py` and the reel with `python3 tools/assemble.py`. The music comes from `tools/audio.py`; run it with the project's `.venv` Python.
