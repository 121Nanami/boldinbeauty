# Elite Bundle dressing: pipeline prepared (2026-10-07, cloud session, branch `claude/busy-ptolemy-ag5mi7`)

**Status: prepared and tested headless; no map has been dressed yet.** Dressing needs the packs inside the Unreal project on the
owner's Mac. This cloud session cannot run Unreal and cannot move the packs (see "Why not from here").

## Owner request
Replace the greybox in all levels with real assets from the Elite Bundle ("maximum quality"). Later in the session the owner
connected Google Drive: folder **`Elite_Bundle`** (owner `admin@example.com`). Layouts stay as they are; only the
look changes.

## What the bundle is (read from Drive, 2026-10-07; folder names and a few listings, not every file)
Three vendors, **22 Unreal-native packs** (`.uasset` / `.umap` in the usual `Meshes / Materials / Textures / Maps / Blueprint`
layout, so no FBX import is needed; individual meshes 100-450 KB, so thousands of files in total):

| Vendor | Packs |
| --- | --- |
| Scansfactory | Cemetery, UrbanDistrict_Gate, RuinedCrypt, UrbanProps_Vol1, UrbanProps_Vol2 |
| Dekogon | MedievalVOL9, MedievalKitchen, Victorian, Stone_Decor, Foliage_Sets, Restaurant, Dockhouse, Anemoia, Boneyard, AncientMountainTemple |
| Meshingun | Gothic_Interior, MedievalProps_V02, GothicCemetery, FeudalJapanInteriorProps_Vol1, FeudalJapanExteriorProps_Vol1, EgyptCity, FoodScans |

Inspected in detail: `UrbanProps_Vol1` (Environment, Demo, BaseMaterials), `MedievalVOL9` (Meshes, Materials, Textures, Maps; meshes
are `SM_*`), `FeudalJapanExteriorProps_Vol1` (Blueprint, Map, Environment). Per-pack file counts and the engine version the packs
were saved with are **OPEN** until the library script runs.

### Fit against the owner's chapter briefs (the important finding)
The bundle is **medieval / gothic / Victorian / feudal Japan / Egypt / urban**. Most chapter briefs are sci-fi or surreal, so only
some maps have a brief these packs fit. Dressing a map whose brief names a different world would be inventing design, so the
default (`Tools/elite_map_packs.json`, PROPOSED, editable) is:

| Map | Packs allowed | Reason |
| --- | --- | --- |
| L_NeoTokyo | UrbanProps_Vol1/2, UrbanDistrict_Gate | street props for a modern city (the buildings stay the sci-fi Fab kits) |
| Ch02 Ruins | UrbanProps, RuinedCrypt, Boneyard, Cemetery, Stone_Decor, Foliage_Sets | rubble, wrecks, dead foliage |
| Ch03 Docks | Dockhouse, UrbanProps | props only: the brief says the lighthouse is the only structure |
| Ch06 Moon Kingdom | Foliage_Sets, Stone_Decor, AncientMountainTemple | everything covered in flora |
| Ch08 Intersection | UrbanProps | abandoned vehicles, debris |
| Ch12 Paradise | Foliage_Sets, UrbanProps | neo-tropical city |
| Ch13 Secretive Soul | FeudalJapan* (interior + exterior), Foliage_Sets, Stone_Decor, AncientMountainTemple | small Japanese countryside town (the clearest match) |
| Ark, Ch04, Ch05, Ch07, Ch09, Ch10, Ch11 | none | no brief fits; Ch10 is a pastel drawing look and Ch11 is "pure grey, no texture" (never dressed) |

Packs allowed on no map (MedievalVOL9, MedievalKitchen, Victorian, Restaurant, Anemoia, Gothic_Interior, MedievalProps_V02,
GothicCemetery, EgyptCity, FoodScans) are **OPEN: owner decision** where, if anywhere, they belong. I did not assign them.
The first pass dresses **props**; structures, ground and walls stay greybox because no pack here is a sci-fi building or
facade set (the Fab kits already cover those, untextured). Textured materials for those are OPEN (see risks).

## Why not from here
- Drive returns each file to me as base64 in the conversation (a 300 KB mesh is roughly 100K tokens), so the thousands of files
  cannot be pulled into this container, and there is no Unreal here anyway.
- The packs must be copied from the Mac's `Elite_Bundle` (or synced from Drive) into the Unreal project, then built there.
- The only connected Mac session is the owner's mid-task "game trailer design" session; I did not redirect it.

## What was built (all tested without Unreal)
| File | Role |
| --- | --- |
| `Tools/elite_library.py` | Editor script: finds every pack folder under `/Game`, lists each static mesh with its real size -> `Saved/EliteLibrary.json`. |
| `Tools/elite_autodress.py` | Reads that library plus `elite_map_packs.json`; for each map and each greybox prop (32 kinds) picks meshes by name and size, writes `Tools/elite_dress.json`, prints what could and could not be dressed. |
| `Tools/elite_map_packs.json` | Which packs may dress which map (PROPOSED, table above). |
| `Tools/greybox_dress.py` + hooks in `greybox_common.py`, `greybox_props.py` | The build reads the mapping and places the real mesh instead of the whole greybox prop; also supports material and kit-building-material swaps. Inert with no mapping; a mesh missing from the project keeps the greybox; Ch11 and the traversal/brief materials are never dressed. |
| `Tools/elite_scan.py` | Optional read-only catalogue of a local folder (for archives or non-Unreal content). |
| `.gitignore` | Every `Content/*` folder except `BIB` and `Weather` is ignored: `*.uasset` is LFS-tracked repo-wide and the packs are licensed, so they must never be staged. |
| `Tests/test_elite_tools.py` | 20 checks, all pass. One real bug found and fixed on the way: names like `SM_Bench02a` were not split at the letter-to-digit boundary, so no rule would have matched them. |

## Steps on the Mac
1. **Copy the packs** from `Elite_Bundle/<Vendor>/<Pack>/` to `Demo/BoldInBeautyDemo/Content/<Pack>/`, folder unchanged (assets
   refer to each other by that path). If a pack folder holds a further `Content/` layer, copy what is inside it. Open the project once
   so Unreal registers them; note any "saved with a newer version" errors.
2. `UnrealEditor <uproject> -ExecCmds="py <abs>/Tools/elite_library.py"` -> check `Saved/EliteLibrary.log` ends `STATUS OK`.
3. `python3 Tools/elite_autodress.py` -> `Tools/elite_dress.json` and the coverage table.
4. `Tools/build_greybox.sh` (or `BIB_GREYBOX_ONLY=ch13`), then `verify_chapters.py` / `verify_greybox.py` screenshots; compare with
   the greybox and tune `RULES` in `elite_autodress.py` or the pack list.

## Known risks (not yet measured)
- **Texture stretch** on scaled engine cubes if materials are applied to blocks; the first pass avoids it by swapping whole props only.
- **Collision:** imported prop meshes may have none; props the player climbs need it.
- **Cost:** Dekogon / Scansfactory meshes are likely high-poly; 100s of instances per map need Nanite or LODs. Not measured.
- **Anime look:** `ABIBAnimeStyleVolume` inks and bands the final image; judge photoreal scans on screen.
- **Licence:** check each pack's terms before any pack leaves the owner's machine (that is why they are git-ignored).

## Cost
No credit used beyond this session's own tokens. Drive listings cost about 80K tokens in total.

## Handoff record
Device: cloud container (Linux), root `/home/user/BoldInBeauty-Project`. Base: `main` @ `55a353a` merged with `claude/chapter-maps`
@ `a622cd5`. Mac freshness: OPEN. Scope: shared (Tools, Tests, ignore rule). Hashes (sha256, first 16) are in the PR description.
**Not run:** anything inside Unreal (`elite_library.py`, the hooks in `greybox_common.py`, `build_greybox.sh`). Proposed MAIN entry:
none until the Mac steps produce verified facts about the packs.
