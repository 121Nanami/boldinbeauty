# Story proposal and greybox: 40 chapters (2026-10-11)

Execution: Claude Code (Sonnet 5.5) on the owner's Mac, worktree `bb-ai-work`, UE 5.8. Owner request in-session: write the events
myself in a NIKKE-like style, Angel is a non-summonable male Secret, Luna marries in 2009, start the 8000 server, most locks may be
bypassed, make chapters 14-40 and the reworks. Base commit `e6fc5b3`; MAIN sha256 `930a8646...df08e9` before and after (not edited).

## What was produced
| Item | Path |
| --- | --- |
| Story bible | `docs/Story/StoryBible.md` |
| 40 chapter files (9 reworks, 27 new, 4 locked with authored events) | `docs/Story/Chapters/ChXX.md` |
| Greybox builder, data and verifier | `Demo/BoldInBeautyDemo/Tools/greybox_story.py`, `story_data.json`, `verify_story.py` |
| Hooks into the existing build | `build_greybox.py`, `greybox_chapters.py` (story overlay, locked-chapter skip, backups), `greybox_common.py` (`MarkBlue`) |
| Project file | `BoldInBeautyDemo.uproject`: `ModelContextProtocol` plugin enabled |
| Tripo list | `docs/Assets/TripoAssetList.md` |

## Evidence
- Editor module build: `Build.sh BoldInBeautyDemoEditor Mac Development` succeeded.
- `build_greybox.sh` (story + chapters): STATUS OK; `verify_story.py`: RESULT PASS for 36 maps.
- MCP: server listening on 127.0.0.1:8000 (`lsof`), initialize / tools/list / list_toolsets answered.
- Locked chapters 1, 3, 11, 13: no map built or edited (log: "chapter N is LOCKED").

## Proposed MAIN update (single writer, after owner approval of the story)
SHARED, WORKING: a 40-chapter story proposal exists in `docs/Story/` (NIKKE-style episodes; Arc 1 = 2009, Arc 2 = 2010-2016, Arc 3 =
2017). Angel is a male Secret, not summonable until a future update. Luna and Plack marry in 2009 (Ch 12). Operators are "born"
at about 15-18, so ages are loose. 36 story greybox maps exist (`greybox_story.py`). MACOS-LOCAL SNAPSHOT: Unreal MCP started with
`ModelContextProtocol.StartServer 8000`. Everything is PROPOSED until the owner approves it.

## Unresolved
- Owner approval or edits of the story; the Ch 8 shard reveal and the Ch 21 1% ceiling touch DEFERRED angel behaviour.
- Characterisation of roster operators used as guests (functions only); the wedding details; who proposes.
- Gameplay logic, dialogue, cutscenes and balance for chapters 14-40 are not built.
- Windows counterpart: not checked (no `sync/devices` profile, no handoff state). The Git commit is not made.

## Revision after the owner's feedback (same day)
The owner said the first levels were below their minimum quality and mostly too small, and that the story and asset list were too thin.
- **Story:** full NIKKE-style episode scripts for all 40 chapters (`docs/Story/Scripts/`, about 90,000 words).
- **Assets:** 1,285 (was 279), each with a Tripo prompt and a zone, driving labelled greybox stand-ins in every map.
- **Levels:** 27 new maps rebuilt as 1.0-1.3 km worlds (`greybox_world.py`, `world_heroes.py`, `world_plans.py`), 12,600 pieces each
  (9,100 sea map), six discovery areas each, hero set pieces, roads, districts and an edge. `verify_story.py`: RESULT PASS for 36 maps.
- **Not done / honest:** greybox only, generic dressing, no gameplay logic, no walking character, the 9 reworked maps not enlarged,
  nothing walked by a person. The earlier per-chapter builders (`greybox_levels*.py`) remain as the helper library and are otherwise
  superseded.
