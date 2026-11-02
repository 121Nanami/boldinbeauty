# Tripo mesh credit budget plan (draft, 2026-10-12)

Credits now: 87,590 (89,930 at start; 2,340 spent on 36 meshes). Owner floor: 6,000. Usable: 81,590.

## What the catalogue really needs
- ALL_ASSETS.md has 2,673 entries, but only **2,232 are mesh types** (ENV ARC PRP CHR NPC CRE FAU FLO VEH). The other 441 (VFX, WTH, LGT, AUD, UI, DOC) are not Tripo meshes. So the "2.7k assets" figure is really 2,232, and 87,590 / 2,232 = 39 credits each if everything were meshed.
- 310 of those are ENV (locations). 199 are marked "environment set piece". They are built in Unreal from ARC/PRP modules, so they should not be single meshes. Plan: skip ENV. Mesh set = 1,922.
- By priority (catalogue field), excluding ENV: hero 295, standard 515, background 1,112.
- 167 CHR, 29 NPC, 164 CRE and 6 FAU entries are marked rigged (rig: Y). Rigging is a separate credit cost that is NOT in this plan (price not checked).

## Prices read from the generator (Multi-View to 3D, no spend to read)
| Setting | Credits |
| --- | --- |
| H3.1 Best Quality + 8K texture (current default, used for the 36 meshes) | 65 |
| H3.1 Best Quality, 8K texture off | 55 |
| H3.0 Fast & Balanced, 8K texture off | 40 |
| H2.5 Legacy | not read (UI did not register) |
| Geometry-only / Smart Mesh / Generate in Parts | not read |

## Proposed tiers (hero quality is not reduced)
| Tier | Count | Setting | Each | Total |
| --- | --- | --- | --- | --- |
| Hero (all categories) | 295 | H3.1 + 8K | 65 | 19,175 |
| Standard characters and creatures | 142 | H3.1, no 8K | 55 | 7,810 |
| Standard props and architecture | 373 | H3.0, no 8K | 40 | 14,920 |
| Background architecture kit (about half of 686; the rest are kit variants reused by instancing) | 343 | H3.0, no 8K | 40 | 13,720 |
| Background other (props, crowd NPCs, creatures, small categories) | 426 | H3.0, no 8K | 40 | 17,040 |
| **Total** | **1,579** | | **avg 46** | **72,665** |

Left after: 14,925, of which 8,925 is above the 6,000 floor. That 8,925 has to cover retries (rejected images, bad results, about 5 to 10%) AND all rigging. It is tight, so rigging may need to be limited to hero characters.

## Can the average reach 35?
Not at these prices with full coverage. The average is 46. To reach 35 across 1,579 meshes the background tier (1,112 before dedup) would have to cost about 25 each, which needs a cheaper setting (H2.5 Legacy, geometry-only, or lower detail) that I have not been able to price yet. Cutting the mesh count (more kit reuse, skipping low-value background props) is the other lever.

## Suggested process
1. Pilot 10 standard + 10 background meshes on H3.0 (800 credits) and check quality against the hero meshes before committing.
2. Read the remaining prices (H2.5, geometry-only) and re-run this table.
3. Mesh hero first, then standard, then background, stopping at a fixed credit checkpoint (for example 40,000 left) to review.
4. Keep a 5 to 10% reserve for retries and decide the rigging budget separately.

Assumptions to confirm with the owner: skip ENV set pieces, share kit meshes for ARC background, rigging only for hero characters.

## Cutting assets (owner's second lever) - added 2026-10-12
Analysis of ALL_ASSETS.md (non-ENV mesh entries = 1,922):
- **942 are inferred-only** (not in the story text; they came from the earlier Tripo list). Mostly ARC background kit pieces (664), PRP background (133), creature variants (88), NPC (28), CHR (10), VEH (10). This is open decision 6 in the catalogue.
- **40 more are name duplicates** of story-backed entries (same object, different variant label).
- ENV (310) skipped (built from modules).
- Candidate ID lists: TRIPO_CUT_CANDIDATES.md (nothing was deleted from the catalogue).

| Scenario | Meshes | Credits | Avg | Left above 6,000 floor |
| --- | --- | --- | --- | --- |
| Everything non-ENV, tiered prices (no cuts) | 1,922 | 86,430 | 45 | negative (over budget) |
| **A**: story-backed only, de-duplicated (hero 65, standard chars/creatures 55, other standard and background 40) | 940 | 46,625 | 50 | 34,965 |
| **A+**: same cut, quality-first (hero and standard 65, background 55) | 940 | 59,420 | 63 | 22,170 |
| **B**: A plus the first 200 inferred ARC kit pieces | 1,140 | 54,625 | 48 | 26,965 |

Leftover has to cover retries (5 to 10%) and rigging (about 366 entries are rigged; price not checked).

## DECISION: Plan C (owner delegated the choice, 2026-10-12)
Owner constraints: rigging is done in Blender with Rigify (no Tripo rigging cost); the game needs 41 completely unique maps; keep the whole game near 1,000 assets; hero quality not reduced.

Findings: the catalogue's 41 map tags (Neo Tokyo, Docks, Hanging City, Ruins, A.R.C TECH, Moon Kingdom, ... Charter) each carry about 14 architecture kit pieces, almost all inferred-only. 40 of the 41 maps have fewer than 2 story-backed architecture pieces. Cutting every inferred piece would leave those maps bare, so each gets 3 signature pieces.

| Part | Meshes | Setting | Each | Credits |
| --- | --- | --- | --- | --- |
| Hero (all categories) | 280 | H3.1 + 8K | 65 | 18,200 |
| Standard | 492 | H3.1, no 8K | 55 | 27,060 |
| Background (168 story-backed + 120 map-signature kit pieces) | 288 | H3.0, no 8K | 40 | 11,520 |
| **Total** | **1,060** | | | **56,780** |

Trim about 60 more background entries during review to land at about 1,000 meshes and about 54,400 credits. ENV (310 locations) are not meshed; maps are assembled from these pieces. Already meshed (36) count toward the hero/standard totals.
Left after: about 31,000 to 33,000 credits (about 25,000 to 27,000 above the 6,000 floor) for retries, regenerating weak results and upgrading any tier that looks soft in the pilot.
ID lists: TRIPO_PLAN_C_IDS.md. Nothing in ALL_ASSETS.md was changed.
