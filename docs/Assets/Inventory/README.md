# Asset inventory (catalogue only, 2026-10-11)

Complete list of assets the 40-chapter story implies. **Nothing was created or imported; this is documentation.**
Status of every entry: `not started`. All content is PROPOSED story material (see `docs/Story/README.md`); the owner decides lore.

## Sources read in full
- Arc: `docs/Story/StoryBible.md` (ledger `Ch00_Arc`).
- 40 chapters: `docs/Story/Chapters/Ch01.md` ... `Ch40.md` (40 files confirmed, none missing or duplicated) and their performable
  scripts `docs/Story/Scripts/Ch01.md` ... `Ch40.md`.
- For Chapters 1-13 a second, older and longer script exists per chapter (`Scripts/Ch01_NeoTokyo.md` ... `Ch13_SecretiveSoul.md`,
  written 10:15 the same day; the short `ChNN.md` scripts are newer, 10:36, and agree with `Chapters/ChNN.md`). Both were read.
  Assets found only in the older script are tagged `[alt]` and are listed in `AMBIGUITIES.md` for the owner to decide.
- Cross-check: `docs/Assets/TripoAssetList.md` (1,285 rows). All 1,285 rows are accounted for: 350 match a chapter-derived asset by name; 933 were added as `[inferred]` entries
  with their Tripo row number (mostly per-chapter modular kit pieces and outfit variants; 112 are near-matches kept as their own
  entry with the likely twin named); 2 merged into an identical-name entry.

## ID scheme (new; no existing project scheme was changed)
`CAT-NNNN` for an asset, `CAT-NNNN.vNN` for each variation line item (a variation is its own line item).
| Prefix | Meaning | | Prefix | Meaning |
| --- | --- | --- | --- | --- |
| ENV | environments, locations, sub-locations | | CRE | enemies, creatures, bosses |
| ARC | architecture, structures, kits | | FAU | animals / fauna |
| PRP | props, set dressing, documents, items | | FLO | flora |
| CHR | named characters, Secrets, costume pieces | | VEH | vehicles |
| NPC | unnamed / crowd NPCs | | VFX | visual effects |
| WTH | weather | | LGT | lighting setups |
| AUD | audio, ambience, music, voice | | UI | UI and in-world interface |
| DOC | in-world document sets | | | |
An asset that recurs keeps the ID of its first chapter; each chapter ledger lists it again as *recurs* with that chapter's variations.

## Layout
| Path | What |
| --- | --- |
| `src/ChNN.txt` | Hand-written source per chapter (one asset per line: `CAT\|Name\|Subcategory\|First seen\|P\|Description\|Var;Var;?Inferred~reason`) |
| `ledgers/ChNN.md` | Per-chapter ledger (generated): ID, name, category/subcategory, chapter location, description, priority (hero / standard / background), status, one row per variation |
| `master/<CAT>.md` | Deduplicated master catalog by category: one ID per unique asset, every chapter listed, every variation kept |
| `master/INDEX.md`, `master/counts.json`, `master/per-chapter-counts.md` | Counts |
| `registry.json` | ID registry (generated) |
| `build_ledger.py`, `crosscheck_tripo.py` | Tooling: `build_ledger.py --all` then `--master` regenerates everything from `src/` |
| `AMBIGUITIES.md` | Conflicts and open questions for the owner |
| `backups/` | MAIN and HANDOFF as they were before this inventory was added |

## Totals (master catalog)
See `master/INDEX.md`. 2673 unique assets, 2885 variation line items, 5558 line items in total, 946 marked `[inferred]`.
Marks: `[alt]` = only in the older Chapter 1-13 script; `[inferred: reason]` = not stated in the chapter text.
