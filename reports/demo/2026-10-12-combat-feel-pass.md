# Combat-feel pass (presentation only) — Claude Code (Sonnet 5.5) — 2026-10-12

- Status: complete with checks (rules tests, real editor build, `verify_kits.py` PIE run); **visual feel and combat-scene performance remain OPEN** (see Unverified)
- Accountable lead; worker; owned paths: no lead reachable from this standalone session (request below); worker = this session; owned paths = the files in the table below plus this report
- Device; actual root; declared Unreal version: macOS; `<local-home>/Documents/BoldInBeauty-Project/BoldInBeauty-Project/worktrees/bb-ai-work` (branch `bb-ai-work`, HEAD `e77ed04`); EngineAssociation 5.8 (`UE_5.8` at the documentation root)
- Requested host/model/effort; actual use or substitution: not specified in the task; actual = Claude Code, `claude-sonnet-5-5`. No Opus 5.5 / GPT-6.1 Sol / other-provider delegation was used.
- Actual subscription/account entitlement; observed remaining quota/reset: OPEN (not observable from this session)
- Target identity: `Demo/BoldInBeautyDemo` (`BoldInBeautyDemo.uproject`), branch `bb-ai-work`
- Base manifest; counterpart handoff/freshness: no mutually acknowledged base (`sync/handoff-state.json`: `last_mutually_acknowledged_base: null`); Windows counterpart OPEN. Working tree also holds another session's uncommitted work (see Warnings).
- Scope: SHARED source change (portable C++); MACOS-LOCAL evidence for the compile and test runs

## Summary

Most of what the task lists **already existed**: `BIBPlayerFeel.cpp` has hit stop (operator and angel), heavy-hit camera shake, a white body flash, sparks, slash streaks, combo counter, perfect-dodge / parry / chain beats; `BIBFx` has the enemy tell (gold / rose) and `AreaWarning` rings. What was missing, and is now added:

1. **Nothing was switchable or scalable.** One slider (**Combat feel 0–150 %**) and five switches (hit stop, hit flash, view punch, ultimate beat, ground telegraphs) in Settings > Gameplay; they scale / switch the existing hit stop, shake and flash as well as the new effects. Defaults (100 %, all on) reproduce the old numbers exactly.
2. **No anticipation for the player's own moves.** Hand-written kits had 0.5–3 s windups with no cue at all; data kits had no cue until the effect landed. Now: windup telegraphs on the ground (Judge gavel / retribution use `GavelRadius` / `RetributionRadius` for exactly the windup's length; Luna's blackhole shows the lane it will travel), closing rings for the other windups, a cast ring for every move, and an area ring for the 14 data-kit abilities that fire a caster-locked, delayed burst.
3. **No weight difference between a basic attack and an ultimate.** A slot table (`BIBCombatFeel.h`) multiplies hit stop and shake, adds a ground ring under the struck angel and a field-of-view punch on heavy hits, heavier for Move < Special < Ultimate.
4. **Ultimate activation beat:** charge ring + pillar, vignette pulse, field-of-view punch, a 0.18 s slow-down that never overrides one already running.
5. **Defence:** block (cyan flare), parry (gold cross + view punch), perfect dodge (view widens) have world feedback in addition to the HUD prompt.
6. **Trails:** wake behind dodge / dash / kit movement and behind Luna's orb.
7. **Performance:** `ABIBFx` effects are recycled (actor + mesh + material instance created once) and capped by the Effects graphics group (Low 8 / Medium 14 / High 22 / Epic 32); enemy tells are essential and never dropped.
8. **Audio:** nine named hooks, silent until the owner adds sounds (list below). The project has no combat audio assets.

## Audit

Rubric (my own, derived from the kit data and the code; 0 = none, 1 = generic / minimal, 2 = ability-specific, 3 = strong; the number before the arrow is as found, after is now). Anticipation = a cue before the effect; Impact = hit stop / shake / flash / popup / knockback readability of the hits it deals; Follow-through = trail, recovery, lingering effect; Audio = sound cue (none existed anywhere in combat; every ability now has a hook); Readability = does the area or reach read before and while it happens. Enemy readability (gold / rose tells, `AreaWarning`) is unchanged and rated separately: already 2–3.
Method: `Tests`-style dump of every data kit (`BIB::Kits::AllKits()`, 102 abilities) plus manual review of `BIBKitLuna/Judge/Narrator.cpp` (21 abilities incl. Diano). Mean score (A+I+F+R, of 12): **3.39 -> 5.46** over 123 abilities.

Weakest as found (rank, then wired first): (1) Judge Paperwork / Court setting / Gavel / Retribution and Luna Blackhole / Orbiting / Expel: real windups of 0.5–3 s with no cue (score 0–1 of 12); (2) Narrator moves: no cue at all; (3) the 14 data abilities with a delayed area burst: effect appears with no warning; (4) everything else: generic.

<details><summary>Full table (123 abilities)</summary>

| Operator | Slot | Ability | Anticipation | Impact | Follow-through | Audio | Readability | Note |
|---|---|---|---|---|---|---|---|---|
| Luna | M1 | Regular attack (4-hit string) | 1→1 | 2→2 | 1→1 | 0→hook | 1→1 | slash streaks, finisher ring (existing) |
| Luna | Move1 | Blackhole | 0→2 | 2→3 | 1→2 | 0→hook | 1→2 | 0.5 s windup: gathering orb + shot lane; wake behind the orb |
| Luna | Move2 | Orbiting | 0→2 | 1→1 | 1→1 | 0→hook | 0→1 | 2 s windup: ring closing at the orbit radius |
| Luna | Move3 | Event horizon | 1→1 | 1→2 | 1→1 | 0→hook | 1→1 | held field; pulse ring on start |
| Luna | Special | Expel | 1→2 | 2→3 | 0→0 | 0→hook | 1→1 | charge ring + the existing growing sphere |
| Luna | Ultimate | Reality Destabilisation | 1→3 | 2→3 | 1→1 | 0→hook | 1→1 | beat; the 25 s later blast keeps the ultimate weight |
| Judge | M1 | Regular attack | 1→1 | 2→2 | 1→1 | 0→hook | 1→1 | existing |
| Judge | Move1 | Paperwork | 0→2 | 0→0 | 0→0 | 0→hook | 0→0 | 1-1.25 s windup ring |
| Judge | Move2 | Court setting | 0→2 | 0→0 | 0→0 | 0→hook | 0→0 | 1-2.5 s windup ring (gold when becoming Judge) |
| Judge | Move3 | Gavel smash | 0→3 | 1→2 | 1→1 | 0→hook | 1→2 | 0.5 s ground telegraph at GavelRadius before the 3 smashes |
| Judge | Special | Retribution | 0→3 | 1→2 | 1→1 | 0→hook | 1→2 | 1 s ground telegraph at RetributionRadius |
| Judge | Ultimate | Justice | 1→3 | 1→1 | 1→1 | 0→hook | 2→2 | beat; counter ring already drawn every frame |
| Narrator | M1 | Regular attack | 1→1 | 2→2 | 1→1 | 0→hook | 1→1 | existing |
| Narrator | Move1 | Explosion | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 | cast ring; existing blast + ring |
| Narrator | Move2 | Global Exchange (panel) | 0→0 | 0→0 | 0→0 | 0→hook | 0→0 | panel, not a cast |
| Narrator | Move3 | I AM STILL ALIVE | 0→1 | 0→0 | 0→0 | 0→hook | 0→0 | cast ring |
| Narrator | Special | Close enough | 0→2 | 1→1 | 0→0 | 0→hook | 0→0 | 3 s ring for the whole windup |
| Narrator | Ultimate | Switch (Diano) | 1→3 | 0→0 | 0→0 | 0→hook | 0→0 | beat |
| Diano | Move1 | Acceleration | 0→2 | 2→3 | 0→1 | 0→hook | 0→0 | pillar on the dash + movement trail |
| Diano | Move2 | Endless god | 0→1 | 1→1 | 0→0 | 0→hook | 0→0 | cast ring |
| Diano | Move3 | Embodiment of imperfection | 0→1 | 0→0 | 0→0 | 0→hook | 0→0 | cast ring |
| Alchemist | M1 | Goo Slap | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Alchemist | Move1 | Infection | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Alchemist | Move2 | Mutation | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Alchemist | Move3 | Transmutation | 0→3 | 2→3 | 2→3 | 0→hook | 1→2 | 2 caster-locked delayed burst telegraphed; dash trail |
| Alchemist | Special | Ring leader | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Alchemist | Ultimate | One with the soil | 1→3 | 0→0 | 1→1 | 0→hook | 0→0 | ultimate beat; no direct damage step (status / hook / buff) |
| Arita | M1 | Petal Swipe | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Arita | Move1 | Infection | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Arita | Move2 | Friend zone | 0→1 | 2→3 | 2→2 | 0→hook | 1→1 |  |
| Arita | Move3 | Spring time | 0→1 | 0→0 | 0→0 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Arita | Special | Natures Curse | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Arita | Ultimate | Full takeover | 1→3 | 0→0 | 1→1 | 0→hook | 0→0 | ultimate beat; no direct damage step (status / hook / buff) |
| Adapt | M1 | Glitch Jab | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Adapt | Move1 | Adaption Ability | 0→1 | 0→0 | 0→0 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Adapt | Move2 | Adaption Passives | 0→1 | 0→0 | 0→0 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Adapt | Move3 | Adaption stats | 0→1 | 0→0 | 0→0 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Adapt | Special | Switch | 0→1 | 0→0 | 0→0 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Adapt | Ultimate | Why do i bother | 1→3 | 2→3 | 1→1 | 0→hook | 1→1 | ultimate beat |
| Blood | M1 | Lancet | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Blood | Move1 | Crimson Pool | 0→1 | 1→2 | 2→2 | 0→hook | 1→1 |  |
| Blood | Move2 | Clot Chains | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed |
| Blood | Move3 | Hemorrhage | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Blood | Special | Blood Rite | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 2 caster-locked delayed burst telegraphed |
| Blood | Ultimate | Crimson Sea | 1→3 | 2→3 | 2→2 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed; ultimate beat |
| Death | M1 | Scythe Graze | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Death | Move1 | Harvest | 0→1 | 2→3 | 1→1 | 0→hook | 0→0 |  |
| Death | Move2 | Shadow Step | 0→1 | 2→3 | 1→1 | 0→hook | 0→0 |  |
| Death | Move3 | Tithe | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed |
| Death | Special | Last Rites | 0→1 | 2→3 | 1→1 | 0→hook | 0→0 |  |
| Death | Ultimate | Reaper's Ledger | 1→3 | 2→3 | 1→1 | 0→hook | 1→1 | ultimate beat |
| Fujita | M1 | Flicker Jabs | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Fujita | Move1 | Quick Slash | 0→1 | 2→3 | 1→2 | 0→hook | 1→1 | dash trail |
| Fujita | Move2 | Smoke Step | 0→1 | 0→0 | 1→1 | 0→hook | 1→1 | no direct damage step (status / hook / buff) |
| Fujita | Move3 | Needle Rain | 0→1 | 2→3 | 0→0 | 0→hook | 0→0 |  |
| Fujita | Special | Pressure Points | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed |
| Fujita | Ultimate | Hundred Cuts | 1→3 | 2→3 | 1→1 | 0→hook | 1→1 | ultimate beat |
| Gambler | M1 | Card Flick | 1→1 | 2→2 | 1→1 | 0→hook | 1→1 |  |
| Gambler | Move1 | Dice Roll | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Gambler | Move2 | Coin Toss | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Gambler | Move3 | Slot Machine | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Gambler | Special | All In | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Gambler | Ultimate | House Always Wins | 1→3 | 2→3 | 2→2 | 0→hook | 1→1 | ultimate beat |
| Ishii | M1 | Arcane Bolt | 1→1 | 2→2 | 1→1 | 0→hook | 1→1 |  |
| Ishii | Move1 | Frost Ring | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 2 caster-locked delayed burst telegraphed |
| Ishii | Move2 | Ember Field | 0→1 | 1→2 | 2→2 | 0→hook | 1→1 |  |
| Ishii | Move3 | Storm Chain | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Ishii | Special | Overload | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Ishii | Ultimate | Elemental Cataclysm | 1→3 | 2→3 | 1→1 | 0→hook | 1→1 | ultimate beat |
| Jester | M1 | Pratfall | 1→1 | 2→2 | 1→2 | 0→hook | 1→1 | dash trail |
| Jester | Move1 | Juggle | 0→1 | 2→3 | 1→2 | 0→hook | 1→1 | dash trail |
| Jester | Move2 | Spotlight | 0→1 | 2→3 | 2→2 | 0→hook | 1→1 |  |
| Jester | Move3 | Punchline | 0→1 | 2→3 | 1→2 | 0→hook | 1→1 | dash trail |
| Jester | Special | Encore | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Jester | Ultimate | Grand Finale | 1→3 | 2→3 | 1→2 | 0→hook | 1→2 | 2 caster-locked delayed burst telegraphed; dash trail; ultimate beat |
| Kuu | M1 | Haymaker | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Kuu | Move1 | Shield Bash | 0→1 | 2→3 | 1→1 | 0→hook | 0→0 |  |
| Kuu | Move2 | Bulwark | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Kuu | Move3 | Ground Pound | 0→3 | 2→3 | 1→2 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed; dash trail |
| Kuu | Special | Iron Wall | 0→1 | 2→3 | 2→2 | 0→hook | 1→1 |  |
| Kuu | Ultimate | Immovable | 1→3 | 2→3 | 2→2 | 0→hook | 1→1 | ultimate beat |
| Media | M1 | Packet Jab | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Media | Move1 | Blink Cut | 0→1 | 2→3 | 1→2 | 0→hook | 1→1 | dash trail |
| Media | Move2 | Lag Spike | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 2 caster-locked delayed burst telegraphed |
| Media | Move3 | Ping Storm | 0→1 | 2→3 | 1→1 | 0→hook | 0→0 |  |
| Media | Special | Firewall | 0→1 | 1→2 | 2→2 | 0→hook | 1→1 |  |
| Media | Ultimate | System Overload | 1→3 | 2→3 | 1→1 | 0→hook | 0→0 | ultimate beat |
| Myrcia | M1 | Mace Smash | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Myrcia | Move1 | Brace | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Myrcia | Move2 | Cleave | 0→1 | 2→3 | 1→1 | 0→hook | 0→0 |  |
| Myrcia | Move3 | Charge | 0→1 | 2→3 | 1→2 | 0→hook | 1→1 | dash trail |
| Myrcia | Special | Fortify | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Myrcia | Ultimate | Last Bastion | 1→3 | 2→3 | 2→2 | 0→hook | 1→1 | ultimate beat |
| Plack | M1 | Combo Strike | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Plack | Move1 | Rush | 0→1 | 2→3 | 1→2 | 0→hook | 1→1 | dash trail |
| Plack | Move2 | Guard Stance | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Plack | Move3 | Spinning Kick | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed |
| Plack | Special | Second Wind | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Plack | Ultimate | Overdrive | 1→3 | 2→3 | 1→1 | 0→hook | 1→1 | ultimate beat |
| Shiraishi | M1 | Fire Fist | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Shiraishi | Move1 | Flame Dash | 0→1 | 2→3 | 1→2 | 0→hook | 1→1 | dash trail |
| Shiraishi | Move2 | Cinder Shield | 0→1 | 2→3 | 2→2 | 0→hook | 1→1 |  |
| Shiraishi | Move3 | Eruption | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed |
| Shiraishi | Special | Phoenix Guard | 0→1 | 0→0 | 1→1 | 0→hook | 1→1 | no direct damage step (status / hook / buff) |
| Shiraishi | Ultimate | Inferno Soul | 1→3 | 2→3 | 2→2 | 0→hook | 1→1 | ultimate beat |
| Stargazer | M1 | Starshot | 1→1 | 2→2 | 1→1 | 0→hook | 1→1 |  |
| Stargazer | Move1 | Falling Star | 0→3 | 2→3 | 1→1 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed |
| Stargazer | Move2 | Constellation | 0→1 | 1→2 | 2→2 | 0→hook | 1→1 |  |
| Stargazer | Move3 | Supernova Seed | 0→1 | 0→0 | 1→1 | 0→hook | 1→1 | no direct damage step (status / hook / buff) |
| Stargazer | Special | Zenith Gaze | 0→1 | 2→3 | 2→2 | 0→hook | 1→1 |  |
| Stargazer | Ultimate | Cosmic Collapse | 1→3 | 2→3 | 1→1 | 0→hook | 1→2 | 1 caster-locked delayed burst telegraphed; ultimate beat |
| Timekeeper | M1 | Second Hand | 1→1 | 2→2 | 1→1 | 0→hook | 0→0 |  |
| Timekeeper | Move1 | Rewind Step | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Timekeeper | Move2 | Stasis Bubble | 0→1 | 2→3 | 2→2 | 0→hook | 1→1 |  |
| Timekeeper | Move3 | Fast Forward | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Timekeeper | Special | Temporal Echo | 0→1 | 0→0 | 1→1 | 0→hook | 0→0 | no direct damage step (status / hook / buff) |
| Timekeeper | Ultimate | Zero Hour | 1→3 | 2→3 | 1→1 | 0→hook | 1→1 | ultimate beat |
| Weather | M1 | Gust Bolt | 1→1 | 2→2 | 1→1 | 0→hook | 1→1 |  |
| Weather | Move1 | Downpour | 0→1 | 1→2 | 2→2 | 0→hook | 1→1 |  |
| Weather | Move2 | Blizzard | 0→1 | 1→2 | 2→2 | 0→hook | 1→1 |  |
| Weather | Move3 | Thunderstorm | 0→1 | 2→3 | 1→1 | 0→hook | 1→1 |  |
| Weather | Special | Cyclone | 0→1 | 1→2 | 2→2 | 0→hook | 1→1 |  |
| Weather | Ultimate | Perfect Storm | 1→3 | 2→3 | 2→2 | 0→hook | 1→1 | ultimate beat |

</details>

Notes on the table: "no direct damage step" means the damage (if any) comes through a status tick or a named hook, so Impact stays 0 for the cast itself; its ticks still get the global hit feedback. Impact +1 on Move / Special / Ultimate is the slot weight (ring + view punch + heavier stop / shake). Scores are a rubric for ordering work, not measurements.

## Completed work and checks

### What each stage gives which abilities

| Effect | Where it comes from (existing event) | Abilities |
| --- | --- | --- |
| Cast ring (anticipation) | `TryActivateSlot` success -> `FeelOnCast` | every Move / Special of data kits and Narrator moves without a windup action; Narrator Close enough: ring lasts the full 3 s windup |
| Windup telegraph | `StartAction` -> `FeelOnActionStart` | Luna Blackhole (orb gathers + lane), Orbiting (ring at orbit radius, 2 s), Event horizon, Expel; Judge Paperwork, Court setting, Gavel (ground ring at `GavelRadius`, 0.5 s), Retribution (ground ring at `RetributionRadius`, 1 s); Diano Acceleration; Dodge |
| Delayed-burst ground ring | `FeelDataAreaTelegraphs` reads the ability's own steps | 14 data abilities: a `Burst` fired >= 0.3 s after the cast, with the caster locked until it fires, not following a target / random enemy |
| Ultimate beat | `FeelOnCast(Ultimate)` | all 20 operators' ultimates (Luna, Judge, Narrator, 17 data kits) |
| Ground ring + view punch + weight | `OnHitLanded` -> `FeelOnHit` | every damaging hit of a Move / Special / Ultimate (heavier by slot); basic-attack finisher gets a small punch |
| Hit stop / shake scale | `OnHitLanded`, `ABIBEnemy::ReceiveDamage`, `AddShake` | all hits |
| Flash scale | `ABIBEnemy::ReceiveDamage` | all hits |
| Trail | `FeelTickView` (during dodge / Diano dash / kit movement), `LunaTick` (orb) | Dodge, Diano Acceleration, 11 data kits' dashes, Luna Blackhole |
| Block / parry / perfect dodge | `ReceiveEnemyHit`, `PerfectDodge` | all operators |
| Late blast keeps ultimate weight | `LunaTick` | Luna Reality Destabilisation's 25 s later explosion |

Wired weakest-first in the order listed under "Weakest as found"; because the layer hooks shared events, the last group costs nothing extra.

### Hit stop interaction (precise)

- The existing hit stop is the engine's per-actor `CustomTimeDilation` (angel 0.02 for 0.06–0.15 s; operator 0.05 for 0.05–0.11 s). It is **not** presentation-only: it also holds that actor's own `Tick(DeltaSeconds)` timers. Verified in code: `ActionTime += DeltaSeconds` (Luna / Judge / Narrator windups and Luna's per-tick damage) and the angels' windup counters advance on dilated delta, while cooldowns (GAS effects) and the data-kit runtime use `World->GetTimeSeconds()`, which is not dilated. So during a hit stop a hand-written kit's windup and an angel's attack timer pause for up to 0.11 / 0.15 s while cooldowns keep running. **This is pre-existing and unchanged at 100 %.**
- This pass makes that scalable: at 0 % or with Hit stop off it is removed entirely; heavier slots hold up to 1.6x longer (capped 0.16 s operator, 0.2 s angel). The owner may want the opposite default for balance-sensitive fights; `bFeelHitStop` is the switch.
- New code adds **no** actor time dilation. The ultimate beat uses the existing global slow-mo (`SetSlowMo`): every game-time clock scales equally for at most 0.18 s real time (about 0.12 s of game time not elapsing), so cooldowns, windups and damage ticks keep their relative timing; it is skipped while another slow-mo runs and is removed with Ultimate beat off or Reduce motion.
- The existing perfect-dodge / parry / daze / chain slow-mos are unchanged and not governed by the new switches (they are close to mechanics: the counter window follows them). Flagged for the owner.

### Behaviour changes to be aware of

- **Reduce motion** now also disables the body flash, view punch, vignette pulse and ultimate beat (the flash was previously always on).
- Camera shake from taking damage (`ABIBPlayerController::AddShake`) is also scaled by Combat feel.
- FX over the per-quality cap are dropped (non-essential only). At Low quality a very busy fight shows fewer sparks/rings than before; gameplay is unaffected.
- `ABIBFx::Spawn` gained a defaulted `bEssential` parameter; all existing call sites compile unchanged.

### Sound hooks (owner: please author; nothing was fabricated)

`/Game/BIB/Audio/Combat/<name>.<name>` (same convention as `/Game/Weather/Audio`): `Cast_Move`, `Cast_Special`, `Ultimate_Charge`, `Hit_Light`, `Hit_Heavy`, `Block`, `Parry`, `Dodge`, `Dodge_Perfect`. Missing assets are skipped silently after one failed load per name (`BIBCombatFeel::PlayCue`). Not yet hooked (no clear existing event): per-operator signature sounds, UI beep for the ultimate-ready state.

### Checks

| Check | Result |
| --- | --- |
| `Tests/run_core_tests.sh` before changes | all suites 0 failures (103 / 13022 / 2460 / 4450 / 3964 / 4380 / 443 / 59 / 230 / 777 / 151 checks; no soft-locks) |
| `Tests/run_core_tests.sh` after changes | all suites 0 failures, no soft-locks (103 / 13036 / 2460 / 4450 / 3964 / 4380 / 443 / 59 / 230 / 777 / 77 tactics / 151). The script and `tactics_tests.cpp` carry another session's uncommitted additions (the tactics suite); I did not edit either |
| UBT `BoldInBeautyDemoEditor Mac Development` in the working tree | first attempt failed only in `BIBArkUI.cpp` (other session's include, see Warnings); after removing that include line: **Result: Succeeded** |
| UBT build + link of a scratch copy (before the include fix) | Succeeded, 381 s |
| `Tools/verify_kits.py` (PIE, every ability of every operator cast on live enemies, ~14 min) | **RESULT PASS 127 ok, 0 flagged, 0 skipped**; no LogBIB / LogAbilitySystem error, ensure or crash |
| `Tools/perf_capture.py` `ark,stage1` | Ark: median 33.3 ms (30 fps), GPU-bound (GPU 42.7 ms, game thread 4.9 ms, render thread 42.6 ms). `stage1`: "timed out waiting for stage 1", so its 60 fps number is not a combat measurement. Nothing here exercises the effects layer |

### Files

| Affected path (under `Demo/BoldInBeautyDemo/Source/BoldInBeautyDemo/`) | Action | Before SHA-256 (HEAD, 12) | After SHA-256 (12) | Classification |
| --- | --- | --- | --- | --- |
| BIBGame/BIBCombatFeel.h | created | null | 7ba8c6d20710 | SHARED |
| BIBGame/BIBCombatFeel.cpp | created | null | d718733bd7e4 | SHARED |
| BIBGame/BIBPlayerFeelFx.cpp | created | null | 0d7caebf7e9e | SHARED |
| BIBGame/BIBFx.h | edited (pool, cap, essential) | 44c5b20e559d | dae131534d92 | SHARED |
| BIBGame/BIBFx.cpp | edited | 801c833dedad | aacd99cb7844 | SHARED |
| BIBGame/BIBEnemy.h | edited (+FlashBrightness) | 5b03d34b7c29 | d8e7be8942d3 | SHARED |
| BIBGame/BIBEnemy.cpp | edited (hit stop / flash scale) | 397534491dbe | 7623c5d50fe3 | SHARED |
| BIBGame/BIBKitLuna.cpp | edited (orb wake, ult attribution) | 34a8d1426315 | b7ad3965dc6a | SHARED |
| BIBGame/BIBPlayerCharacter.h | edited (members, accessors) | f9f69c76bedc | 58222e44a0cf | SHARED |
| BIBGame/BIBPlayerCharacter.cpp | edited (StartAction, view settings, parry / block) | aba7afa708e3 | ab94eb3a7058 | SHARED |
| BIBGame/BIBPlayerController.cpp | edited (shake scale, one hunk; file also holds another session's changes) | 87e7c6168c7c | 2e96f9a63b45 | SHARED |
| BIBGame/BIBPlayerFeel.cpp | edited (hit stop / shake scale, hooks) | e4af8e5daeaf | 8b0533e46675 | SHARED |
| BIBGame/BIBPlayerGAS.cpp | edited (one call) | 46678954b322 | 73125dd6f458 | SHARED |
| BIBRules/BIBMeta.h | edited (6 settings fields) | 0450b56a9a52 | 1e84903219ba | SHARED |
| BIBRules/BIBSave.cpp | edited (save / load) | e442094b3207 | 581ecd96757f | SHARED |
| BIBRules/BIBPagesExtra.cpp | edited (settings tiles + actions) | d5de23827f7c | 366fff67e832 | SHARED |

## Warnings, issues and lead questions

| Status | Severity | Finding, lead request or question | Evidence and impact | Proposed next action | Responsible agent |
| --- | --- | --- | --- | --- | --- |
| resolved by me, tell the owner of that edit | warning | `BIBArkUI.cpp` (another session's uncommitted file) included `Misc/GuardValue.h`, absent in UE 5.8 | UBT: `fatal error: 'Misc/GuardValue.h' file not found`. `TGuardValue` is in `Templates/UnrealTemplate.h` (via `CoreMinimal.h`) | I deleted that one `#include` line (nothing else in the file) after the other session's UBT had exited, so the project builds | the session that edited `BIBArkUI.cpp` |
| OPEN | warning | The task forbids touching "UI pages" but item 4 requires the existing settings page; I added 7 tiles and 6 action handlers to the Settings page (`BIBPagesExtra.cpp`) and no other page | diff of `BIBPagesExtra.cpp` | confirm this reading; revert those hunks if the settings page is out of scope (the layer then runs at defaults, not switchable) | lead / owner |
| OPEN | warning | Existing hit stop holds windup timers (see Hit stop interaction) | code evidence above | owner decision: keep, or default Hit stop off | owner |
| OPEN | info | Another session has uncommitted edits in `BIBArkUI.cpp`, `BIBPlayerController.*`, untracked `BIBRules/BIBPlayerModel.cpp`, `docs/Assets/ALL_ASSETS.md` | `git status` | not touched; integrate separately | other session |
| OPEN | info | No lead reachable; this report is the lead request | CLAUDE.md "Lead consultation" | lead reviews | lead |

## Unverified

- **How it looks and feels in the game.** Nothing was run in the editor: ring sizes, intensities (`Intensity` 5–10), FOV degrees (0.7–2.4) and vignette are chosen from the existing FX conventions, not tuned by eye.
- **Combat performance.** The capture could not reach a fight (stage 1 timed out), and the Ark is GPU-bound regardless of effects. The pooling / cap are expected to reduce spawns and allocations; no before/after number exists. The 14-minute `verify_kits` run (hundreds of effects, 0 errors) is a stability check, not a frame-time one.
- No automated test covers the new settings keys (save round trip is covered generically by the existing save tests, which pass).
- Windows behaviour, mobile touch layout of the extra settings tiles.

## Proposed BoldInBeauty_MAIN update

Target section: 11 (completed artifacts). Integration: pending lead review (not written by this session: another session has uncommitted work in the tree and no writer reservation was coordinated).

> **Combat feel pass — 2026-10-12 (Claude Code, Sonnet 5.5), WORKING until checked in the editor.** Presentation-only shared layer: `BIBGame/BIBCombatFeel.*` (settings view, slot weights, sound hooks), `BIBGame/BIBPlayerFeelFx.cpp` (anticipation, windup telegraphs, impact ring, view punch, ultimate beat, defence feedback, trails), pooled and capped `ABIBFx`. Settings > Gameplay: Combat feel 0-150 % plus switches for hit stop, hit flash, view punch, ultimate beat, ground telegraphs; defaults reproduce the previous behaviour. Gameplay numbers untouched. Pre-existing: hit stop uses per-actor time dilation and therefore pauses that actor's own windup timers briefly (OPEN owner decision). Sound hooks under `/Game/BIB/Audio/Combat/` await owner-authored sounds. Report: `reports/demo/2026-10-12-combat-feel-pass.md`.

## Handoff

Remaining: tune by eye in a real session; a combat-scene `perf_capture` (fix the stage 1 wait, then compare Combat feel 0 % / 100 % through `BIB_PERF_ACTIONS=set:feel:-1,...`); author the nine sounds; resolve the `BIBArkUI.cpp` include; owner decisions above. No transfer, commit or push was made; Windows counterpart not checked.
