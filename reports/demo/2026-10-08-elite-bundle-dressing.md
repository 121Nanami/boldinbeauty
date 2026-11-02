# Elite_Bundle dressing of the 13 chapter maps and the Ark — Claude Code (Opus 5.5) — 2026-10-08

- Status: complete with checks (MACOS-LOCAL); not walked by a person; Windows OPEN.
- Accountable lead; worker; owned paths: this session (standalone, lead = owner); `Demo/BoldInBeautyDemo/Tools/greybox_bundle.py`,
  `Tools/import_elite_bundle.sh`, hooks in `greybox_chapters.py` / `greybox_neotokyo.py` / `greybox_ark.py`, `.gitignore`, demo
  README, `docs/chapter-maps.md`, this report, MAIN / HANDOFF entries.
- Device; actual root: owner's Mac, worktree `BoldInBeauty-Project/worktrees/ai-agent-bb`, branch `claude/chapter-maps` on
  `db86411`; UE 5.8.3.
- Base manifest; counterpart: Windows replica and freshness OPEN (no `sync/devices` profile on this Mac).

## Owner request (2026-10-08, verbatim)
"Here's a bundle with 100s of basically random assets to fill up the 13 main chapter levels And arc with those assets thats all
Its all up to you to do this"

## The bundle
`<local-home>/Documents/BoldInBeauty-Project/Elite_Bundle` (2.1 GB, 1,047 assets, UE 5.4, package root
`/Game/Elite_Bundle/...`). 376 static meshes; many folders hold only materials, textures, blueprints or maps (Feudal Japan,
Egypt, Boneyard, Foliage Sets, Stone Decor, Restaurant, Gothic Interior, Ancient Mountain Temple ...). 167 mesh material slots are
empty because the bundle does not contain those materials (most of the Dockhouse, Anemoia, Victorian and Scansfactory meshes).

## What was built
- `Tools/import_elite_bundle.sh`: copies the bundle to `Content/Elite_Bundle` (git-ignored: store content, 2 GB; GitHub LFS
  would also cost money). Run once per machine.
- `Tools/greybox_bundle.py`, called last in every map build (`Tools/build_greybox.sh`), so rebuilds keep it:
  - catalogue: 299 meshes used (kitchen 118, dock 66, manor 33, gothic 30, cemetery 21, crypt 14, anemoia 10, urban 7), 77 left
    out (glass planes, roof / frame pieces, backdrops, whole rooms, tiny tools); 92 meshes get a palette colour in their empty
    slots (wood, metal, stone, flora ...), so no engine checkerboard shows.
  - placement: vignettes (one medium / large piece with 2-5 small pieces around it), clutter, set pieces just outside the section
    rings, pieces along the routes between sections and across the map; every piece on a traced floor and only where an overlap
    box touches nothing that blocks (no props in walls, cover, jump-up steps or water); the middle 30 % of every combat section
    and every Ark station ring kept clear; small pieces without collision, 60 m draw distance, no shadow.
  - themes per chapter (see `docs/chapter-maps.md`, "Elite_Bundle dressing"); Chapter 10 pastel (grave scans kept realistic);
    Chapter 11 flat grey, each leg dressed with the props of the map it echoes; the Ark per area (Merchant Row food, library
    books, factory / training storage, gang hall, town square, Underworld, civilian flora).
- Without the bundle in Content the build logs a warning and the maps come out as before.

## Result (build log, `Saved/BuildGreybox.log`, STATUS OK, 217 s)
| Map | Bundle pieces | Different meshes |
| --- | ---: | ---: |
| Neo Tokyo | 2,233 | 143 |
| the Ark | 2,967 | 230 |
| 2 Ruins | 1,910 | 117 |
| 3 Docks | 1,378 | 105 |
| 4 Hanging City | 1,305 | 191 |
| 5 A.R.C TECH | 1,191 | 68 |
| 6 Moon Kingdom | 2,334 | 191 |
| 7 T.O.M.B.S | 1,211 | 88 |
| 8 Intersection | 916 | 74 |
| 9 Remnant Remains | 1,498 | 67 |
| 10 Hallucinations | 1,159 | 232 |
| 11 Return to Nowhere | 610 | — |
| 12 Paradise | 1,744 | 189 |
| 13 A Secretive Soul | 1,621 | 173 |

## Checks
- `Tools/build_greybox.sh` STATUS OK (all 14 maps), no overlap / trace fallback warnings.
- PIE screenshots of two bundle vignettes per map (scratch script, `Saved/BundleShots`, not committed): props stand on the floor,
  themed as intended, anime look applied; Chapter 10 pastel, Chapter 11 grey.
- `Tools/verify_chapters.py` RESULT PASS (218 checks, 0 failures: sections, gates, swim volumes switching a character to swimming,
  movers, PlayerStart on solid ground, every mesh actor has a mesh). `Tools/verify_ark.py` RESULT PASS (133 OK: 17 stations,
  teleports, saved position).
- Run note: an unfocused editor throttles to 3 fps and viewport screenshots time out; launch verify scripts with
  `-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False`.
- Not run: frame-rate capture with the extra pieces, a person walking the maps, Windows.

## Assistant's reading (PROPOSED, OPEN for the owner)
Which pack goes in which chapter, every count, placement rule and fallback colour; pastel recolour in Chapter 10; grey props in
Chapter 11 (the owner's brief says "nothing in it" but also "a mashup of the 10 past maps": read as grey echoes, easy to remove
with `THEMES[11]`). Not done: the bundle's blueprints, maps, sounds and particle effects; frame cost of the extra ~1-3k actors per
map (not measured; instancing remains the next step if a slower PC needs it).

## Playtest build (owner: "Push this build as the latest as i want to do a playtest")
owner asked to push this build as the latest for a playtest: `main` on GitHub fast-forwarded to it; standalone app `~/Documents/BoldInBeauty-Builds/2026-10-08-playtest/BoldInBeautyDemo-Mac/BoldInBeautyDemo.app` (packaged, runs; Docks, Moon Kingdom, Ruins load in about 0.2-0.7 s). Packaged-only fixes: `M_GB` flagged for Nanite (palette colours on bundle meshes showed the default material); bundle materials resaved in 5.8; the Scansfactory `m_BaseMaterial_01_01` (shipped without its textures, cannot compile) is treated as missing and gets palette colours (99 meshes on fallback colours).
