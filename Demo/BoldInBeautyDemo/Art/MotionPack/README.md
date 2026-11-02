# bb-Showreel motion pack, game edition (2026-10-09)

The owner's motion pack (`~/Downloads/motion-studio/videos/bb-Showreel`: 49 animated UI pieces, 5 page transitions, 6 in-game ads,
the download trailer, the UI sheets and the master reel) in the game. Owner: "use them all".

- `source/` - the pack's editable HyperFrames source (src, shared, tools/build.py), copied unchanged. `game.js` is the game edition's
  only addition: sample values in the pieces ("800 / 4,800", "21 HITS", "FLOOR 42", sample mail and quests) are drawn transparent so
  the game draws the real value in the same place.
- `Tools/import_motion_pack.sh` builds everything (git-ignored output, like the Elite bundle):
  `Tools/motion_export.py` renders each piece at 1920x1080 / 12 fps in game mode (npx hyperframes), keeps the frames of the clips the
  game plays, packs 2048x2048 atlas pages and writes `Content/BIBMotion/motion.json`; `Tools/motion_import.py` imports the pages as
  `/Game/BIBMotion/<id>/T_<id>_NN`; `Tools/motion_movies.sh` transcodes the videos to `Content/Movies`.
- In game: `BIBGame/BIBMotion.*` plays pieces on a full-screen layer, `BIBGame/BIBMotionEvents.*` decides when (every piece has a
  moment), `BIBGame/BIBVideoScreen.*` puts the videos on screens in the Ark, the trailer is the startup movie. Check: `Tools/verify_motion.py`.
- The piece catalogue (clips per game event, value slots, anchors) is in `Tools/motion_export.py` (PIECES).

## BB Dream (dream-core rework, 2026-10-16)

GreyHat approved board v2 (pastel, soft, less tech, plus the admiration rule). The game now builds the UI pieces and page transitions from the BB Dream source:
- `source-dream/`: the BB Dream HyperFrames source (src, shared, tools, bundled OFL fonts; README inside). It has 55 UI pieces, 5 new page transitions (Milk Veil, Ribbons, Cloud Bloom, Sparkle Iris, Full Dream) and a `-linger` variant of every piece. `source/` stays; the VFX (sheets 09-18), ads and trailer still come from it.
- `Tools/motion_export.py`: UI and transitions build from `source-dream`, VFX from `source`. Everything renders at **13 fps**. The manifest starts fresh when its stored fps differs. The values the game draws use the dream text colours (plum on the pastel panels).
- `game.js`: hide rules match without case and read "×" as "x" (the dream skin writes sentence case and "×10").
- Re-run `Tools/import_motion_pack.sh` to rebuild `Content/BIBMotion` (generated, git-ignored).
- Rendered 4K / 1080p ProRes masters live outside git in `~/Downloads/motion-studio/videos/bb-Dream` (about 35 GB).

Not done yet:
- The value fonts are still the game's League Gothic / JetBrains Mono (`BIBFonts`). Fraunces and M PLUS Rounded 1c are in `source-dream/shared/fonts/dream` if they should be imported.
- Linger tracks (the admiration rule) are not in the `PIECES` catalogue, and there is no idle/snap-back hook in `BIBMotion` yet. See `source-dream/README.md`.
