# Dream filter rework (2026-10-11)
Executor: Claude Code, Sonnet 5.5, owner's Mac (MACOS-LOCAL), branch `bb-ai-work`. Owner brief: `~/Downloads/filter_rework_prompt.md`; reference "Omori". The old filter hid detail and shook. All numbers PROPOSED.

## Audit (before any change)
Filter = `ABIBAnimeStyleVolume` (`BIBGame/BIBAnimeStyle.*`) + `M_BIBAnimeStyle`/`MI_BIBAnimeStyle` (`Tools/build_assets.py`) + 12 fps frame holder.
- **Lost detail:** 4 px ink lines from depth, normals and colour; 3 hard cel bands (`CelStrength 1`); hatching 0.55; shadow tint 0.9; form/contact shade 0.45/0.55; saturation 1.3; channel ceiling 0.94; object boil resampling the image from displaced UVs.
- **Shaking:** `ObjectShake 1.4 px` (world-position noise re-seeded 8 x/s, constant), `BoilAmount 0.5` line wobble (8 x/s), 12 fps frame holding with the camera held. `FrameJitter` was already 0. Camera shake `BIBPlayerController.cpp` is hit-triggered only (not constant).

## Change
| Setting | Old | New |
|---|---|---|
| Style material (ink, bands, hatch, boil) | `MI_BIBAnimeStyle` blended | none (native settings only) |
| `bStepVisuals` / frame holder | true, 12 fps | false, extension not created |
| Vignette | 0.3 | 0.2 (static) |
| Scene fringe | 0 | 0.08 (static, edge only) |
| Film grain | 0 | 0 (UE grain re-seeds per frame = flicker) |
| Bloom | 0.25 / thr 2.5 | unchanged |
| Saturation | 1.3 (material) | 0.85 |
| Contrast | cel bands | 0.97 |
| White temp | default | 5300 K |
| Shadow gain | lavender 0.5/0.42/0.95 tier | 0.95/0.94/1.0 |
| Midtone gain | - | 0.99/0.985/1.0 |
No time input anywhere in the new settings; nothing animates.

## Assets / files
- Modified: `BIBAnimeStyle.h/.cpp` (new `bLegacyAnimeLook`, default false = dream; `ApplyLookSettings`; stale old-material blendables stripped at runtime), `Tools/capture_filter_shots.py` (`BIB_LEGACY=1` for "before"; falls back to existing maps).
- **Backup of the old filter:** `bLegacyAnimeLook = true` on a volume restores the old look exactly. `M_BIBAnimeStyle`, `MI_BIBAnimeStyle`, `build_assets.py` HLSL untouched. Nothing deleted, no level saved, no plugin or project/render setting changed.
- Editor `UnrealEditor` pid 66698 (other pane, MCP) was closed with the owner's permission before building.

## Verification (MACOS-LOCAL, UE 5.8.3)
Editor build Succeeded. `capture_filter_shots.py` RESULT PASS for both runs. Identical camera positions (PlayerStart + offsets) on `L_Ch02_Ruins` and `L_Chapter1_Stage1`. `L_NeoTokyo` / `L_Ark` do not exist in this worktree's generated `Content/Greybox`. Shots: `reports/demo/screenshots/2026-10-11_dream_{before,after}_*.png`.
Seen: outlines, banding, hatching and grain gone; props, tiles, text ("Story contact") readable; cool grey-lavender haze, no blown white. Not played by a person; motion absence is by construction (no animated input), not measured by video.

## Uncertainty / OPEN
- Mood tuning is by eye against "Omori"; the Ruins scene is warm sand so the cast is subtle there. All numbers adjustable in `ApplyLookSettings`.
- Hit-triggered camera shake (`bCameraShake`, saved user setting) left as is; say if default off.
- `Tools/verify_anime_style.py` still expects stepping and ink; it fails against the new default until it sets `legacy_anime_look`.
- Held-frame performance saving is gone; re-run `perf_capture.py`.
- Windows not checked.

---
# Part 2 - AAA grade (dream filter v2), 2026-10-11
Owner brief: `~/Downloads/aaa_filter_and_asset_inventory_prompt.md` Job 1. Same rules (dream-core, no toon, no animated screen-space
motion, minimal obscuring). Raises craft, not intensity. `DreamLookVersion` on `ABIBAnimeStyleVolume`: **2 = AAA (default)**,
**1 = the first dream filter (backup, kept in `ApplyDreamV1`)**, `bLegacyAnimeLook` = the original anime look. Nothing deleted.

| Setting | v1 (old) | v2 (new) | Why |
|---|---|---|---|
| Bloom method / size scale | default | SOG / 5.0 | wide, faint glow instead of a tight halo |
| Bloom intensity / threshold | 0.25 / 2.5 | 0.22 / 1.4 | only real highlights glow; tints cool and falling off (0.6 -> 0.2) |
| Film slope / toe / shoulder | engine default | 0.86 / 0.50 / 0.30 | long soft shoulder: filmic highlight roll-off, no clip |
| Film black clip / white clip | default | 0 / 0.04 | no crushed blacks |
| Blue correction / expand gamut | default | 0.4 / 0.8 | blue lights stay blue, hue-preserving gamut |
| Local exposure (highlight / shadow contrast, detail) | off | 0.85 / 0.9 / 1.25 | shadow and highlight detail without global contrast |
| Auto exposure min / max / speed up, down | default | 0.6 / 2.2 / 1.2 / 1.2 | one consistent exposure in every lighting condition, no pumping |
| Ambient occlusion intensity / radius | default | 0.3 / 120 | faint contact depth, static |
| White temp | 5300 | 5600 | tone mapping handles the cool, grade stays subtle |
| Saturation / contrast | 0.85 / 0.97 | 0.88 / 0.98 | |
| Shadows: saturation, gain, offset | gain 0.95/0.94/1.0 | 0.9, gain 0.95/0.955/1.0, offset +0.004/+0.005/+0.010 | lavender, lifted a hair so nothing crushes |
| Highlights: saturation, gain | none | 0.85, 1.0/0.99/0.965 | pale cream, not white |
| Midtone gain | 0.99/0.985/1.0 | 0.99/0.99/1.0 | |
| Vignette / fringe | 0.2 / 0.08 | 0.18 / 0.06 | lighter, still static |
| Film grain, motion blur, lens flare | 0 | 0 | unchanged |
Banding: Unreal's tonemapper dithers its output by default (`r.Tonemapper.GrainQuantization`, a project render setting, left untouched);
film grain stays 0 because the engine's grain re-seeds every frame.

**Verification (MACOS-LOCAL, UE 5.8.3):** build Succeeded; `capture_filter_shots.py` RESULT PASS for v1 and v2 on five maps with
different lighting (Ch02 Ruins sand, Ch1 Stage 1 blue dusk, Ch10 pastel, Ch23 red sky, Ch33 white room), identical camera
positions. Shots `reports/demo/screenshots/2026-10-11_aaa_{v1,v2}_*.png`. Seen: white-room windows and lamps keep their shape
(roll-off instead of clip), red sky keeps depth in the shadows and a soft glow on the lamps, nothing crushed or blown, no
motion by construction. Judged by eye only; not played by a person; Windows OPEN. Re-capture with
`BIB_DREAMV1=1 BIB_MAPS=/Game/... Tools/capture_filter_shots.py`.
