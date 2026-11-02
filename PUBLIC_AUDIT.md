# Public snapshot audit

This file describes the public snapshot of Boldinbeauty prepared for `https://github.com/121Nanami/boldinbeauty`. It was built in a scratch directory. Nothing was pushed and no pull request was opened.

The archive is a `git archive` of one commit. It has no history.

## Where this was made

- Device: Linux cloud workspace, not the Mac or the Windows replica.
- Project root inspected: the documentation checkout at `/workspace`.
- `sync/devices/` is not in this checkout (it is gitignored). Cross-device freshness is OPEN.
- No file in the source tree was changed for this export. Git LFS objects were materialized in the working tree only long enough to copy them, then the working tree was restored.

## Scrub

Text and Unreal asset metadata were rewritten before the archive was packed.

- macOS `/Users/...` paths were removed. In text they are `<local-home>`. Inside `.uasset` files the same paths were rewritten to `/home/public-user` with the same byte length, so the asset string table stays the same size.
- The local macOS account name was removed, including the nickname taken from that account.
- Personal names from the export request were removed from credits and author mentions. The set-piece name "Cliff stair" is a prop, and it is unchanged.
- One personal Google account address was replaced with `redacted@example.com`.
- The project admin mailbox was replaced with `admin@example.com` everywhere it granted admin, including tests. Look-alike addresses in the server tests now use `example.com` as well.
- Remote-session identifiers (`session_...`) were replaced with `session-redacted`.
- No `.env`, private key, or live API token was in the tree. `Server/.env.example` still has empty placeholders. Server tests use fake MongoDB URIs and fake app passwords on purpose.
- A scraped GitHub HTML page that had been saved as `OFL-MPLUSRounded1c.txt` was replaced with the SIL Open Font License text for M PLUS Rounded 1c. It contained no account name.

After the rewrite, a scan of the snapshot found no remaining `/Users/` paths, no remaining account name, no remaining personal mailbox, and no `session_` identifiers.

## Licensed packs that are not in this snapshot

These were already absent from git (the demo gitignore keeps them out). They are purchased marketplace libraries and are not redistributed. The build scripts still tell you how to point them at a local copy you own.

### Fab

- SCI-FI BUILDINGS CITY KIT
- SCI-FI BUILDINGS CITY KIT V.2

`Tools/build_greybox.sh` imports these from the Fab vault cache into `Content/Greybox` (about 230 MB when built). That folder is not in this snapshot.

### Elite Bundle

About 2 GB of Unreal packs used only as local dressing. Not in git. Vendors and packs named in the project notes:

| Vendor | Packs |
| --- | --- |
| Scansfactory | Cemetery, UrbanDistrict_Gate, RuinedCrypt, UrbanProps_Vol1, UrbanProps_Vol2 |
| Dekogon | MedievalVOL9, MedievalKitchen, Victorian, Stone_Decor, Foliage_Sets, Restaurant, Dockhouse, Anemoia, Boneyard, AncientMountainTemple |
| Meshingun | Gothic_Interior, MedievalProps_V02, GothicCemetery, FeudalJapanInteriorProps_Vol1, FeudalJapanExteriorProps_Vol1, EgyptCity, FoodScans |

## Other files left out

No file in the source checkout was over 50 MB. The archive limit is 100 MB, so the heavier screenshots were left out. Their real Git LFS sizes add up to 45.03 MB across 29 files. Forty-three smaller screenshots are included.

| File | Real size |
| --- | --- |
| `2026-10-04_accounts_signin.png` | 3.94 MB |
| `2026-10-11_dream_before_ch1stage1_street.png` | 2.12 MB |
| `2026-10-11_dream_before_ch1stage1_vista.png` | 1.98 MB |
| `2026-10-11_dream_before_ch02ruins_street.png` | 1.95 MB |
| `2026-10-11_dream_before_ch02ruins_vista.png` | 1.94 MB |
| `2026-10-11_aaa_v1_l_chapter1_stage1_vista.png` | 1.70 MB |
| `2026-10-11_dream_after_ch1stage1_vista.png` | 1.70 MB |
| `2026-10-11_aaa_v2_l_chapter1_stage1_vista.png` | 1.59 MB |
| `2026-10-11_aaa_v1_l_chapter1_stage1_street.png` | 1.57 MB |
| `2026-10-11_dream_after_ch1stage1_street.png` | 1.56 MB |
| `2026-10-11_aaa_v1_l_ch10_hallucinations_vista.png` | 1.56 MB |
| `2026-10-11_aaa_v1_l_ch10_hallucinations_street.png` | 1.55 MB |
| `2026-10-11_aaa_v2_l_chapter1_stage1_street.png` | 1.53 MB |
| `2026-10-11_aaa_v2_l_ch10_hallucinations_vista.png` | 1.53 MB |
| `2026-10-11_aaa_v2_l_ch10_hallucinations_street.png` | 1.47 MB |
| `2026-10-11_aaa_v2_l_ch23_redsky_street.png` | 1.36 MB |
| `2026-10-11_aaa_v2_l_ch23_redsky_vista.png` | 1.33 MB |
| `2026-10-11_aaa_v1_l_ch23_redsky_vista.png` | 1.32 MB |
| `2026-10-11_aaa_v2_l_ch02_ruins_vista.png` | 1.27 MB |
| `2026-10-11_aaa_v2_l_ch02_ruins_street.png` | 1.26 MB |
| `2026-10-11_dream_after_ch02ruins_vista.png` | 1.25 MB |
| `2026-10-11_aaa_v1_l_ch02_ruins_vista.png` | 1.25 MB |
| `2026-10-11_aaa_v2_l_ch33_whiteroom_vista.png` | 1.25 MB |
| `2026-10-11_aaa_v1_l_ch33_whiteroom_vista.png` | 1.23 MB |
| `2026-10-11_aaa_v1_l_ch23_redsky_street.png` | 1.19 MB |
| `2026-10-11_dream_after_ch02ruins_street.png` | 1.17 MB |
| `2026-10-11_aaa_v1_l_ch02_ruins_street.png` | 1.17 MB |
| `2026-10-11_aaa_v1_l_ch33_whiteroom_street.png` | 1.16 MB |
| `2026-10-11_aaa_v2_l_ch33_whiteroom_street.png` | 1.16 MB |

Proprietary font binaries, not redistributable:

- `Demo/BoldInBeautyDemo/Art/MotionPack/source/shared/fonts/Impact.ttf`
- `Demo/BoldInBeautyDemo/Art/MotionPack/source/shared/fonts/ComicSansBold.ttf`
- `Demo/BoldInBeautyDemo/Art/MotionPack/source-dream/shared/fonts/Impact.ttf`
- `Demo/BoldInBeautyDemo/Art/MotionPack/source-dream/shared/fonts/ComicSansBold.ttf`

The motion-pack ads still name Impact and Comic Sans as system fonts. The snapshot does not ship the font files. The `@font-face` rules that pointed at those files were removed.

## Kept third-party material

- SIL Open Font License fonts: League Gothic, JetBrains Mono, Archivo Black, Fraunces, M PLUS Rounded 1c. License files travel with the fonts.
- GSAP 3.14.2 under the GreenSock standard license (`https://gsap.com/standard-license`). The copyright line in `gsap.min.js` is unchanged.
- Weather audio is synthesized by `Tools/build_weather.py`. It is not a purchased library.

## Checks

- Account server tests, in-memory store: 118 passed, 0 failed (`npm test` in `Demo/BoldInBeautyDemo/Server`).
- Included screenshots and operator portraits are real PNG and JPEG files, not Git LFS pointers.
- This public tree stores those binaries directly. `.gitattributes` in the snapshot does not mark them as Git LFS.
