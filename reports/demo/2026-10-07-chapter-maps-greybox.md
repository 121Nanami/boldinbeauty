# Chapter maps 2-13 greybox and the detail pass (Neo Tokyo included): 2026-10-07

**Device:** MACOS-LOCAL (owner's Mac), UE 5.8.3, worktree `worktrees/ai-agent-bb`, branch `claude/chapter-maps` (based on
`claude/anime-style-build` @ `d140278`). **Executor:** Claude Code, Opus 5.5 (Claude subscription). **Status:** built and
machine-checked, **not walked by a person**. Counterpart (Windows) freshness: OPEN.

## Owner requests (2026-10-07)
1. Each chapter has a unique map; Chapter 1 is Neo Tokyo. Follow the BB style. Make the maps unique, vibrant and dynamic to play:
   areas to jump up to and climb, places to swim, ways to move around levels and between sections. Maps 2-13 come from the
   owner's ideas (recorded word for word as FINAL in `HANDOFF.md`). Update the handoff with the ideas first, then greybox each level.
2. Follow-up: make the greyboxes more fleshed out and higher detail; they looked low quality next to Neo Tokyo.
3. Follow-up: "don't cheap out": about 8-12k minimum per map, and bring the Neo Tokyo greybox to 8-12k minimum.

## What was built
- `HANDOFF.md`: new section "Chapter maps 1-13: owner level ideas, FINAL" with the owner's brief for every map; in-game texts verbatim.
- `docs/chapter-maps.md`: brief + greybox reading for each map, the shared traversal colour code, the detail table, OPEN items.
- `Source/.../BIBGame/BIBGreyboxMover.*`: new actor; a static mesh that moves back and forth, spins or swings (swinging bridges,
  lifts, waves, conveyor cubes, presses, robot arms, drones, forklifts, gantries, drifting steps, boats, the lighthouse beam).
- `Tools/greybox_chapters.py`: the twelve layouts (four combat sections each with gates and a boss section; Chapter 11 has
  none). `greybox_common.py`: helpers `mover`, `water` (a water PhysicsVolume plus surface), `jump_steps`, `climb_wall`, `section`,
  `rain_placeholder`, `lighting`, a kit registry / keep-out for prop placement, `force_material`, a `save(anime=False)` option, 33
  new palette colours, and a build warning below 8,000 pieces.
- `Tools/greybox_props.py` (multi-part props) and `Tools/greybox_detail.py` (detail pass per map, Neo Tokyo included).
- `Tools/build_greybox.py/.sh`: `BIB_GREYBOX_ONLY=neotokyo|ark|chapters|ch03,ch11`. `Tools/verify_chapters.py`: new check.
  `verify_greybox.py`: Neo Tokyo must have at least 8,000 pieces.

| Map | Pieces | Map | Pieces |
| --- | --- | --- | --- |
| L_NeoTokyo | 15,687 (was 5,119) | L_Ch08_Intersection | 9,576 |
| L_Ch02_Ruins | 9,244 | L_Ch09_RemnantRemains | 9,228 |
| L_Ch03_Docks | 8,688 | L_Ch10_Hallucinations | 8,675 |
| L_Ch04_HangingCity | 8,967 | L_Ch11_ReturnToNowhere | 9,943 |
| L_Ch05_ArcTech | 8,934 | L_Ch12_Paradise | 9,190 |
| L_Ch06_MoonKingdom | 8,982 | L_Ch13_SecretiveSoul | 9,042 |
| L_Ch07_TOMBS | 9,920 | | |

Rebuilding Neo Tokyo plus the twelve chapter maps takes about 90 s (`BIB_GREYBOX_ONLY=neotokyo,chapters`). `L_Ark` was not
rebuilt this round.

## Verification (MACOS-LOCAL, 2026-10-07)
- Editor build (`BoldInBeautyDemoEditor Mac Development`, `-NoHotReloadFromIDE`): Succeeded, 0 warnings.
- `build_greybox.sh` (neotokyo + chapters): STATUS OK, every map at or above 8,000 pieces, no warnings.
- `verify_chapters.py` (PIE): **not completed on the final (detailed) maps**; the last full run was interrupted. Earlier runs on
  the pre-detail maps passed every check for chapters 2-6 (tags, sections, water volumes, PlayerStart on ground, pawn spawn),
  including "a character in the water swims" (Ruins, Docks) and "movers move in play" (Hanging City). Run it again:
  `UnrealEditor <uproject> -ExecCmds="py <abs>/Tools/verify_chapters.py"` (about 6 min per map at this detail level).
- Found on the way: the screenshots run through the editor viewport lag behind when the editor renders slowly (about 3 fps with
  this much geometry and Lumen). The script now waits until each file exists. Do not rely on screenshots taken by older runs.

## Decisions the assistant made (PROPOSED, all reversible)
All layouts, sizes, counts, section placement, traversal placement and prop choices (see `docs/chapter-maps.md`). Four sections
per map with the boss in section 4 (MAIN section 7). The data centre was chosen for Chapter 9 (the owner left it to the
assistant). Chapter 11 is a 1.4 km walk (about 5.2 min at the demo's 450 cm/s), with no anime volume and a neutral
post-process volume; its detail pass is all one flat grey. The Moon Kingdom's palace dome and civic spire are a reading of "all
the structures you'd expect".

## OPEN (owner)
A walking / climbing / swimming character for the greybox maps; how the maps join the campaign stages; whether Chapter 13
has a boss; the Chapter 9 Morse `-.--.` (reads "("); the Chapter 13 "wree"; real rain / wind / wave FX, white noise and the
voice line; the Chapter 10 pastel look; Chapter 11's density against "nothing in it" (kept to grey props, no life, no events).
