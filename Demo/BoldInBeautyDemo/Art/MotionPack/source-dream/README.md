# BB Dream: the dream-core motion pack

This is the bb-style rework of the in-game motion pack: dream-core, pastel and soft, with less tech. GreyHat approved board v2 on 2026-10-16. The board is at https://claude.ai/artifact/Rdeysoab9W6FNSBzRWXHHx.

Everything is original code-built motion graphics. There are no AI-generated images, no character art, and no faces or eyes. Sweet pieces and transitions are 3840x2160; linger tracks are 1920x1080 (to fit disk space; they play as idle overlays). Everything is **13 fps**, the BB stepped rate.

## What's in the pack

| Path | What it is |
|---|---|
| `UI_Pack_Dream_4K/Sheet_NN_*/NN_nn_<id>.mov` | 55 UI pieces (the 49 from the first pack plus 6 from sheet 08), the **sweet** version. Transparent ProRes 4444, 4K, 5 s. Each folder has a PNG contact sheet. |
| `UI_Pack_Dream_4K/Sheet_NN_*/NN_nn_<id>-linger.mov` | The same 55 pieces as **linger** tracks. Transparent ProRes 4444, 1080p, 20 s (3.8 s entrance + 16.2 s creep). |
| `Showcase_Sheets_4K/` | 8 sheet showcase videos and the transitions showcase (mp4), for review. |
| `Page_Transitions_Dream_4K/Transition_0N.mov` | 5 page transitions: 01 Milk Veil, 02 Ribbons, 03 Cloud Bloom, 04 Sparkle Iris, 05 Full Dream. Transparent, same lengths and swap points as before (0.8 / 1.0 / 1.2 / 1.35 / 1.6 s, covered at 0.4 / 0.45 / 0.6 / 0.55 / 0.75 s). |
| `src/`, `shared/`, `tools/` | Editable source (HyperFrames HTML + GSAP). |

## The admiration rule (linger tracks)

GreyHat's rule is that the more you admire it, the more weirdly scary it becomes. A linger track plays the piece's entrance, holds it, and then drifts through four stages:

| Time held | Stage | What happens |
|---|---|---|
| 0–4 s | sweet | Pure pastel, gentle bob, motes drift up |
| 4–8 s | off | Letters slip, a soft shadow appears, colour fades a little, percentages keep counting past 100 |
| 8–12 s | wrong | The shadow detaches and lags, colour drains toward moss, approved lines rewrite themselves, motes gather into a ring |
| 12 s + | hollow | Ink thickens to Hollow Ink, the rose-black vignette closes, single hollow frames appear, then everything goes completely still |

The rewritten lines appear only on the three pieces GreyHat approved: `victory`, `ultimate-ready` and `notice-toast`. Every other piece creeps without new words. Any new in-world line needs her approval first.

## Hook for the game team (`BIBMotion`)

The pack does not change any game code. To use the linger tracks:

1. Play `<id>.mov` as today.
2. If the player gives **no input** (the linger file starts with the same 3.8 s entrance) for the hold time of that screen (results, menus, the Ark hub, summon, the idle HUD), cross-fade on one 13 fps step to `<id>-linger.mov` and play it from its hold point. The hold point is 3.8 s by default; per-piece values are listed in `tools/gen_ui.py` `HOLD`.
3. On **any input**, cut straight back to the last frame of the sweet piece. This is the snap-back GreyHat approved ("as if nothing happened").
4. Short-lived pieces (damage numbers, toasts and transitions) never switch to linger.

`Content/BIBMotion/motion.json` would need a `linger` path per piece. `Tools/import_motion_pack.sh` must also export at 13 fps instead of 12 (`bib.VisualStepRate`).

## Editing

- `shared/dream.js` holds the skin: neon to pastel by role (fill, line, text), cut corners to pills, cel bands, a thin plum ink edge, ALL CAPS to sentence case, and soft motion helpers. It loads before the kit, so the piece files in `shared/uip*.js` are almost untouched. The only hand edits are the summon-reveal sunburst and milk backdrop (`uip2.js`) and an internal variable rename in `ui.js`.
- `shared/linger.js` is the admiration layer. `shared/dtrans.js` holds the five dream transitions.
- To build and render: run `python3 tools/gen_ui.py`, then `tools/render.sh <id>` (4K at 13 fps). Run `tools/snap.sh <id> 1,2,3` for a quick contact sheet (set `PREVIEW_BG='#FBF6FF'` to preview over pastel).

Fonts are bundled in `shared/fonts/dream`: Fraunces (variable, SOFT and WONK axes) and M PLUS Rounded 1c. Both are OFL.
