# Combat feel rework: every player attack — Claude Code (Opus 5.5) — 2026-10-13

- Status: complete with checks (cloud container); the Unreal build and the in-editor test run are **OPEN until run on the owner's Mac**
  (no Unreal Engine in the cloud container; the engine-independent rules compile and pass here).
- Accountable lead; worker; owned paths: this session (standalone, lead = owner); no subagents. Owned paths: `Demo/BoldInBeautyDemo/
  Source/BoldInBeautyDemo/{BIBGame,BIBRules}` (combat, kits, feel, input), `Config/DefaultGame.ini` ([BIB.CombatFeel] only),
  `Tests/`, `Tools/verify_feel.py`, `Tools/verify_combat.py`, this report, MAIN / HANDOFF entries.
- Device; actual root; declared Unreal version: Claude Code cloud container (Linux), `/home/user/BoldInBeauty-Project`, branch
  `claude/happy-babbage-gmmcty` fast-forwarded to `claude/chapter-maps` @ `da063f1` (newest build, 2026-10-09; owner approved);
  `.uproject` EngineAssociation 5.8 (owner's Mac: UE 5.8.3). No `sync/devices` profile for this device.
- Requested host/model/effort; actual: Claude Code, Opus 5.5 (`claude-opus-5-5`); Claude Pro entitlement as configured; quota not
  observable here.
- Base manifest; counterpart: Windows replica and freshness OPEN; MAIN replica of this branch updated only. Hashes:
  `reports/demo/combat-feel-manifest-2026-10-13.json`.

## Owner request (2026-10-13, summarised)
Rework every player attack (basic strings, specials, ultimates, every character) so hits feel physical and real, matching how
Zenless Zone Zero's combat FEELS without copying its names, UI, characters, assets or moves. Targets: hit stop scaled by weight
(light 2-4 frames, heavy 6-10, ultimate finisher 12+, attacker AND target frozen); clear anticipation / impact / recovery with the
impact on the frame the hit connects; reactions scaled to the hit (flinch, stagger, knockback, launch, knockdown; enemies never
ignore a heavy hit); directional camera shake, push-in on heavies, a framed camera moment on ultimates; layered spark / sound /
rumble; ~150-200 ms input buffer, recovery -> dodge cancels, a rhythm in each string; a rewarding dodge with slow motion and a
counter (own name and visuals); a stagger / break meter with a punish window; ultimates with a short skippable cinematic beat, a big
screen-space impact and invulnerability. Change only combat / ability / animation timing / VFX-SFX hookup / camera code or data;
existing assets only, no placeholder art; tuning in data or config. Owner answers to the Step 0 questions: build from the newest
branch; testing on the Mac; procedural poses on the existing meshes are fine ("we won't have anims for a while"); the assistant picks
the names, "Perfect Dodge" stays; add gamepad bindings for rumble; force a drawn frame on hits; tuning in config. Mid-task: "You are
allowed to alter hand written kits as honestly they are violently unbalanced like luna's." Queued as goal 2: the Angel AI brief
(`reports/demo/goal-2-angel-ai-brief.md`).

## What changed (all numbers PROPOSED; every feel number is in `Config/DefaultGame.ini [BIB.CombatFeel]`)
| Area | Before (2026-10-09 build) | After |
| --- | --- | --- |
| Hit weight | none: everything scaled from damage / attack | every hit has a weight - tick, light, medium, heavy, finisher, ultimate, ultimate finisher - from where it came from (basic string step, move, Special, ultimate, counter; the kit runtime now reports its hits' slot, tick and final hit: `BIBRules/BIBKitRuntime.h FHitInfo`) and its size (`BIBRules/BIBHitFeel.h`) |
| Hit stop | target 2-7 frames from damage size (bosses x0.6); operator only on hits >= 0.6x attack and only slowed to 15%; ticks froze too | **both frozen** for: light 3, medium 5, heavy 8, finisher 10, ultimate 6, ultimate finisher **14** frames (60 fps); ticks 0; once per frame at the heaviest weight; cap 0.3 s |
| On the 12 fps look | a short hit stop could fall between drawn frames | every felt hit forces a fresh drawn frame; heavy+ draw the two-tone impact frame (reduce motion: no flashes) |
| Reactions | grunts flinch (0.32 s), brutes only on hits >= 2.5x attack, bosses never | weight x target table: grunt flinch / stagger / knockback / launch / knockdown; brute recoil / flinch / stagger / knockback / knockdown; boss recoil, a flinch on heavies and a stagger on finishers at most every 6 s (stays threatening); light / medium spam builds poise, heavies always land; the body tilts away from the hit (knockdown lies down and gets up) |
| Camera | random pitch / yaw jitter on hits >= 2.5x attack | a directional kick on every felt hit (snaps along the hit, punches down, overshoots, settles; 0.6 deg light .. 4 deg ultimate finisher), a push-in on medium+ (12 / 30 / 45 / 70 cm), the ultimate beat frames the operator from the front and orbits |
| Rumble | none (no gamepad bindings) | per weight 0.15 .. 1.0 intensity; gamepad layout added (below) |
| Sound | none in combat | layered hook per weight + ultimate beat + perfect dodge (`Sound.*` keys); **no combat sound assets exist** (see Missing assets) |
| Poses (no animations exist) | none: the cylinder never moved | wind-up (lean back + squash) until the hit frame, impact lunge on the hit (held by the hit stop), recovery back to neutral; every basic step, move, Special and ultimate; from the kit timelines for data kits |
| Input buffer | 0.25 s | **0.18 s** |
| Cancels | basic attack recovery only | + a dodge cancels the recovery of every cast after its last hit (data kits: `FRuntime::InRecovery / CancelRecovery`; Luna's Blackhole, Judge's Gavel smash and Retribution) |
| String rhythm | data kits ran two strings at once (their own 1-5 hits and an engine 4-step with x0.85-1.45 on top), so the "finisher" landed on random hits | a kit with its own string keeps it, its last hit is the finisher (heavy); single-hit kits and the hand-written kits swing the engine 4-step string; finishers wind up longer (data: +0.05-0.09 s; engine string hits at 0.08 / 0.08 / 0.10 / 0.22 s, steps 0.26 / 0.26 / 0.30 / 0.52 s) |
| Perfect Dodge | slow motion 0.25x for 0.45 s, "DODGE COUNTER" (3x) | same feel numbers, now config; the counter is the **Phantom Counter**, a heavy hit; rumble + sound hook |
| Break meter | "Daze" -> "STUNNED" x1.6 damage, "CHAIN ATTACK" on Tab | **STRAIN** -> **BROKEN** (punish window x1.6 damage, 3-6 s) -> **RELAY STRIKE** on Tab (finisher weight); Strain bonus per weight (heavy +6, finisher +10, ultimate finisher +20) |
| Ultimates | none; Luna's "cutscene" was a 5 s invisible timer | every ultimate opens with a **0.9 s cinematic beat** (world at 3% speed, camera frames the operator, name banner, impact frame, rumble; LMB / ult / dodge / jump skips after 0.15 s; the operator cannot be hit, +0.35 s after); the ultimate's last hit is the ultimate finisher (14 frames, knockdown, 0.3 s slow motion, floor ring, critical number); an ultimate's own damage never recharges it (data kits already; now Luna, Judge, Narrator too) |
| Gamepad | none | left stick move, right stick look, X attack, A jump, B dodge (sprint + parry), LB parry, Y Special, RB switch, LT block, RT move 1, D-pad left / right moves 2 / 3, D-pad up ultimate, D-pad down R ultimate, L3 sprint, View talk, Menu pause |
| Tuning | constants in headers | `Config/DefaultGame.ini [BIB.CombatFeel]` (152 values + sound paths), read at start; console `BIBFeelReload` (re-reads the file while playing), `BIBFeel <Key> <Value>`, `BIBFeel` (lists), `BIBFeelResetStats` |

## Per character
✅ **Luna**: rebalanced (owner allowed; power 1.69 -> 0.84, 4-star band 0.75-0.90 on the `Tests/kit_balance.cpp` yardstick): string
factor 0.30 -> 0.45; Blackhole orbit ticks 1250 -> 500, explosion 2800 -> 2200, lock 4.5 s -> 0.9 s (orb flies on by itself, dodge
cancels after the shot); Reality Destabilisation: cinematic beat + 0.6 s instead of a 5 s timer, 10 s fuse (was 25), flips every
2.5 s, 9,000 within 16 m (was 55,000 to the whole map), charges at half rate, cannot overlap, its damage no longer refills her bar.
Weights: string light / heavy finisher; Blackhole shot medium, orbit ticks, explosion heavy; Orbiting / Event horizon ticks, white-hole
nuke heavy; Expel heavy; ultimate finisher. Poses for every move. Files: `BIBGame/BIBTuning.h`, `BIBGame/BIBKitLuna.cpp`,
`BIBGame/BIBPlayerCharacter.cpp`, `Tests/kit_balance.cpp`.

✅ **Judge**: hits on innocents / suspects (no damage by her rules) now land as light impacts; Paperwork stuns land as medium
impacts; Gavel smash: medium / medium / heavy impacts on everything in the circle, a sentence kill is heavy, dodge-cancel after the
third smash; Retribution: finisher on the guilty, heavy impacts on the rest, dodge-cancel after the strike; Justice: beat, every
sentence is an ultimate finisher, its kills no longer recharge it. Numbers unchanged (see OPEN). Files: `BIBGame/BIBKitJudge.cpp`,
`BIBGame/BIBPlayerCharacter.cpp`.

✅ **Narrator / Diano**: string light / heavy; Explosion heavy; Close enough: 3 s wind-up pose, executes as Special finisher; Switch:
beat; Diano's block counter (10,000) is a counter hit; Acceleration medium; the speed self-explosion is a finisher. Numbers
unchanged. Files: `BIBGame/BIBKitNarrator.cpp`, `BIBGame/BIBPlayerCharacter.cpp`.

✅ **The 17 data kits** (Alchemist, Arita, Adapt, Blood, Death, Fujita, Gambler, Ishii, Jester, Kuu, Media, Myrcia, Plack, Shiraishi,
Stargazer, Timekeeper, Weather): every hit carries its slot / tick / final flag from the kit runtime, so each gets its weight,
reaction, camera, rumble and pose from its own timeline; basic strings use the kit's own string (finisher = last hit); finishers wind
up longer for Alchemist (0.15 -> 0.22 s), Arita (0.12 -> 0.18), Adapt (0.10 -> 0.18), Blood (0.10 -> 0.18), Death (0.15 -> 0.20),
Fujita (0.05 -> 0.14), Ishii (0.10 -> 0.18), Media (0.05 -> 0.10), Shiraishi (0.10 -> 0.18), Timekeeper (0.08 -> 0.16); Kuu, Myrcia,
Plack already had heavy wind-ups; Gambler, Jester, Stargazer and Weather (single-hit basics) swing the engine string. Recovery
cancels for every locked cast. All 17 stay in their rarity band (Media 1.11 -> 1.10 after the timing change). Files:
`BIBRules/BIBKitData1-3.cpp`, `BIBRules/BIBKitRuntime.h/.cpp`, `BIBRules/BIBKitSteps.cpp`, `BIBRules/BIBKitHooks.cpp`,
`BIBRules/BIBKitTypes.h` (`FeelWeight` override field), `BIBGame/BIBKitWorld.h/.cpp`, `BIBGame/BIBPlayerGAS.cpp`.

Shared (every operator): `BIBRules/BIBHitFeel.h/.cpp` (new), `BIBRules/BIBStagger.h/.cpp`, `BIBGame/BIBFeelConfig.h/.cpp` (new),
`BIBGame/BIBPlayerFeel.cpp`, `BIBGame/BIBPlayerCharacter.h/.cpp`, `BIBGame/BIBPlayerGAS.cpp`, `BIBGame/BIBPlayerController.h/.cpp`,
`BIBGame/BIBEnemy.h/.cpp` (reactions only - the Angel AI is goal 2), `BIBGame/BIBBlueprintLibrary.cpp` (test counters),
`BIBGame/BIBArkUI.cpp` / `BIBMotionEvents.cpp` (the renamed words only), `Config/DefaultGame.ini`, `Tests/feel_tests.cpp` (new),
`Tests/run_core_tests.sh`, `Tools/verify_feel.py` (new), `Tools/verify_combat.py` (wording).

## Before / after: every attack
Frames at 60 fps. "Hit stop" before = the angel's freeze / the operator's (slowed, not frozen; "none" under 0.6x attack); after =
both frozen. Reaction columns are grunt / brute / boss; `*` = a boss is interrupted at most every 6 s, otherwise it recoils. "dodge-cancel
after" = the cast's recovery from that second can be cancelled with a dodge. Hand-written kits first (their damage numbers are in
the text above), then the 17 data kits (generated from the kit data before and after: `BIBRules/BIBKitData*.cpp`).

#### Shared actions
| Attack | Before | After |
| --- | --- | --- |
| Dash attack (sprint + LMB, 1.4x) | target 3-4 f / operator slowed; knockback 420; shake 0.9 | medium: 5 f both, stagger / flinch / recoil, kick 1.0 deg, push-in 12 cm, lunge pose |
| Phantom Counter (after a Perfect Dodge, 3x) | "dodge counter": 5 f / slowed; knockback 500; shake 2.2 | heavy: 8 f both, knockback / stagger / flinch*, kick 1.8 deg, push-in 30 cm, Strain +28 +6 |
| Parry riposte (2x) | 4 f / slowed | heavy: 8 f both, knockback / stagger / flinch* |
| Switch-in strike (1.5x) | 4 f / slowed | medium: 5 f both |
| Relay Strike (was chain attack, 4x + 2x splash) | 6 f / slowed, slow motion 0.2 | finisher on the broken angel (10 f, launch / knockback / stagger*), heavy splash; slow motion kept |
| Perfect Dodge | slow 0.25x for 0.45 s, window 0.2 s | same (config: `PerfectDodge*`), rumble 0.35, sound hook |

#### Luna (attack 783)
| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction before -> after | After: weight, camera |
|---|---|---|---|---|---|
| LMB string #1-2 (200 / 223 -> **299 / 335**) | 0.09 -> **0.08** | step 0.27 -> **0.26** | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | light, kick 0.6 deg |
| LMB string #3 (235 -> **352**) | 0.09 -> **0.10** | step 0.27 -> **0.30** | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | light, kick 0.6 deg |
| LMB finisher #4 (341 -> **512**) | 0.20 -> **0.22** | step 0.50 -> **0.52** | 3 f / none -> **8 f both** | knockback 650 / - / - -> **knockback / stagger / flinch*** | heavy, kick 1.8 deg, push-in 30 cm |
| 1 Blackhole shot (1250) | 0.5 + flight | **4.5 -> 0.9 / after 0.55** | 4 f / 2 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | medium |
| 1 Blackhole orbit ticks (1250 -> **500**/s) | 1 s ticks | - | 4 f / 2 f each -> **0 f (tick)** | flinch -> **-** | tick: spark only |
| 1 Blackhole explosion (2800 -> **2200**) | orb's end | - | 5 f / 3 f + shake -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | heavy, kick 1.8 deg, push-in 30 cm |
| 2 Orbiting contact ticks (149/s) | 2.0 wind-up | - | 2 f each -> **0 f** | flinch -> **-** | tick |
| 3 Event horizon pull (250/s) | hold | - | 2 f every frame (stutter) -> **0 f** | flinch every frame -> **-** | tick |
| 3 White-hole nuke (2000) | on contact | - | 5 f / 3 f -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | heavy |
| 4 Expel (750 + 250/s, x1.5-2) | release | - | 6-7 f / 3-4 f -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | heavy (Special finisher) |
| E Reality Destabilisation (55,000 to the map -> **9,000 within 16 m**) | 5 s timer + 25 s fuse -> **0.9 s beat + 0.6 s + 10 s fuse** | invulnerable 5 s -> **beat + 0.35 s** | 7 f / 4 f -> **14 f both** | flinch / flinch / - -> **knockdown / knockdown / stagger*** | ultimate finisher, kick 4 deg, push-in 70 cm, slow-mo |

#### Judge (attack 200)
| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction before -> after | After: weight, camera |
|---|---|---|---|---|---|
| LMB string #1-3 (128 / 143 / 150) | 0.09 -> **0.08 / 0.08 / 0.10** | 0.27 -> **0.26 / 0.26 / 0.30** | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | light |
| LMB finisher #4 (218) | 0.20 -> **0.22** | 0.50 -> **0.52** | 3 f / 2 f -> **8 f both** | knockback 650 -> **knockback / stagger / flinch*** | heavy |
| LMB on an innocent / suspect (no damage) | - | - | nothing -> **3 f both** | nothing -> **flinch / recoil / recoil** | light impact |
| 1 Paperwork (Judge mode: stuns the guilty 3 s) | 1.25 | 1.25 | nothing -> **5 f both** | stun only -> **stun + stagger / flinch / recoil** | medium impact, wind-up pose |
| 3 Gavel smash (3 smashes, stun 0.5 s each) | 0.5 / 1.0 / 1.5 | 2.25 / **after 1.55** | nothing (kills by size) -> **5 / 5 / 8 f** | stun -> **+ stagger ... / knockback ...** | medium, medium, heavy; kill heavy |
| 4 Retribution (kills the guilty, stuns the rest) | 1.0 | 3.0 / **after 1.05** | kills by size -> **10 f (kill) / 8 f (stun)** | stun -> **launch ... / knockback ...** | finisher / heavy |
| R Justice (stance; sentences on hit) | stance | - | kills by size -> **beat + 14 f** | - -> **knockdown / knockdown / stagger*** | ultimate finisher |

#### Narrator / Diano (attack 272)
| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction before -> after | After: weight, camera |
|---|---|---|---|---|---|
| LMB string #1-3 (231 / 258 / 272) | 0.09 -> **0.08 / 0.08 / 0.10** | 0.27 -> **0.26 / 0.26 / 0.30** | 4 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | light |
| LMB finisher #4 (394) | 0.20 -> **0.22** | 0.50 -> **0.52** | 4 f / 2 f -> **8 f both** | knockback 650 -> **knockback / stagger / flinch*** | heavy |
| 1 Explosion (1250, 100 speed) | instant | - | 7 f / 4 f -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | heavy, lunge pose |
| 4 Close enough (executes < 20%, boss < 5%) | 3.0 | - | by size -> **8-10 f** | - -> **knockback-launch / ...** | Special finisher, 3 s wind-up pose |
| E Switch -> Diano | 0.5 transform | invulnerable 0.5 | - -> **beat** | - | beat |
| Diano block counter (10,000) | on a hit | - | 7 f -> **10 f** | - -> **launch / knockback / stagger*** | counter (finisher by size) |
| Diano 1 Acceleration (499) | dash | - | 4 f / 2 f -> **5 f both** | flinch -> **stagger / flinch / recoil** | medium |
| Speed self-explosion (12,500 / 30,000) | at the limit | - | 7 f -> **10 f** | flinch -> **launch / knockback / stagger*** | finisher |


#### Alchemist (attack 1300)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Goo Slap #1 | 0.15 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Goo Slap #2 | 0.15 -> **0.22** | 0.00 | 4 f / 3 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Infection | no hit (utility) | 3.00 | - | - | wind-up gesture pose |
| Move2 Mutation | kit mechanic | 2.00 | by size -> **by size, both** | by size | kit mechanic damage: weight from its slot and size |
| Move3 Transmutation | 0.00 | 9.00 | 4 f / 3 f slowed -> **5 f (ticks 0 f) both** | flinch / - / - -> **stagger / flinch / recoil (ticks: -)** | Medium + ticks, kick 1.0 deg, push-in 12 cm |
| Special Ring leader | no hit (utility) | 2.00 | - | - | wind-up gesture pose |
| Ultimate One with the soil | no hit (utility) | 0.00 | - | - | wind-up gesture pose |

#### Arita (attack 240)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Petal Swipe #1 | 0.12 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Petal Swipe #2 | 0.12 -> **0.18** | 0.00 | 4 f / 2 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Infection | no hit (utility) | 3.00 | - | - | wind-up gesture pose |
| Move2 Friend zone | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Move3 Spring time | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Special Natures Curse | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Ultimate Full takeover | no hit (utility) | 0.00 | - | - | wind-up gesture pose |

#### Adapt (attack 640)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Glitch Jab #1 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Glitch Jab #2 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Glitch Jab #3 | 0.10 -> **0.18** | 0.00 | 4 f / 3 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Adaption Ability | kit mechanic | 0.40 | by size -> **by size, both** | by size | kit mechanic damage: weight from its slot and size |
| Move2 Adaption Passives | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Move3 Adaption stats | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Special Switch | no hit (utility) | 1.00 | - | - | wind-up gesture pose |
| Ultimate Why do i bother | 0.00 | 1.00 / after 0.95 | 5 f / 4 f slowed -> **14 f both** | flinch / flinch / - -> **knockdown / knockdown / stagger*** | UltimateFinisher, kick 4.0 deg, push-in 70 cm |

#### Blood (attack 420)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Lancet #1 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Lancet #2 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Lancet #3 | 0.10 -> **0.18** | 0.00 | 3 f / 2 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Crimson Pool | 0.60 | 0.60 | 2 f / none -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Move2 Clot Chains | 0.50 | 0.80 / after 0.55 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Move3 Hemorrhage | 0.40 | 0.80 / after 0.45 | 3 f / 2 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Special Blood Rite | 1.00 | 1.20 / after 1.05 | 6 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Ultimate Crimson Sea | 0.80 | 0.80 | 7 f / 4 f slowed -> **6 f (ticks 0, last 14 f) both** | flinch / flinch / - -> **stagger / flinch / recoil (last: knockdown / knockdown / stagger*)** | Ultimate + ticks -> last: UltimateFinisher, kick 1.4 deg; last hit 4.0 deg, push-in 70 cm, slow-mo |

#### Death (attack 950)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Scythe Graze #1 | 0.12 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Scythe Graze #2 | 0.12 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Scythe Graze #3 | 0.15 -> **0.20** | 0.00 | 3 f / 2 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Harvest | 0.50 | 0.70 / after 0.57 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Move2 Shadow Step | 0.20 | 0.60 / after 0.27 | 5 f / 3 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move3 Tithe | 0.50 | 0.60 / after 0.55 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Special Last Rites | 2.00 | 2.00 | - -> **8 f both** | - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Ultimate Reaper's Ledger | 3.50 | 4.00 / after 3.55 | - -> **14 f both** | - -> **knockdown / knockdown / stagger*** | UltimateFinisher, kick 4.0 deg, push-in 70 cm |

#### Fujita (attack 220)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Flicker Jabs #1 | 0.05 | 0.00 | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Flicker Jabs #2 | 0.05 | 0.00 | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Flicker Jabs #3 | 0.05 | 0.00 | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Flicker Jabs #4 | 0.05 -> **0.14** | 0.00 | 3 f / none -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Quick Slash | 0.00 | 0.30 / after 0.05 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Move2 Smoke Step | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Move3 Needle Rain | 0.30 | 0.50 / after 0.35 | 3 f / 2 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Special Pressure Points | 0.60 | 0.60 | 3 f / 2 f slowed -> **8 f (ticks 0 f) both** | flinch / - / - -> **knockback / stagger / flinch* (ticks: -)** | Heavy + ticks, kick 1.8 deg, push-in 30 cm |
| Ultimate Hundred Cuts | 0.06 | 3.20 | 4 f / 3 f slowed -> **0 f (last: 14 f) both** | flinch / - / - -> **- (last: knockdown / knockdown / stagger*)** | Tick -> last: UltimateFinisher, last hit: kick 4.0 deg, push-in 70 cm, slow-mo |

#### Gambler (attack 440)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Card Flick | 0.05 | 0.00 | 4 f / 3 f slowed -> **3 f (4th: 8 f) both** | flinch / - / - -> **flinch / recoil / recoil (4th: knockback / stagger / flinch*)** | Light (4th: Heavy), kick 0.6 deg (4th 1.8, push-in 30 cm) |
| Move1 Dice Roll | 0.50 | 0.50 | 7 f / 4 f slowed -> **10 f both** | flinch / flinch / - -> **launch / knockback / stagger*** | Finisher, kick 2.6 deg, push-in 45 cm |
| Move2 Coin Toss | no hit (utility) | 0.40 | - | - | wind-up gesture pose |
| Move3 Slot Machine | 1.20 | 1.20 | 7 f / 4 f slowed -> **10 f both** | flinch / flinch / - -> **launch / knockback / stagger*** | Finisher, kick 2.6 deg, push-in 45 cm |
| Special All In | 1.00 | 1.00 | 7 f / 4 f slowed -> **10 f both** | flinch / flinch / - -> **launch / knockback / stagger*** | Finisher, kick 2.6 deg, push-in 45 cm |
| Ultimate House Always Wins | 1.00 | 1.00 | 6 f / 4 f slowed -> **6 f (ticks 0, last 14 f) both** | flinch / flinch / - -> **stagger / flinch / recoil (last: knockdown / knockdown / stagger*)** | Ultimate + ticks -> last: UltimateFinisher, kick 1.4 deg; last hit 4.0 deg, push-in 70 cm, slow-mo |

#### Ishii (attack 330)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Arcane Bolt #1 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Arcane Bolt #2 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Arcane Bolt #3 | 0.10 -> **0.18** | 0.00 | 3 f / 2 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Frost Ring | 0.50 | 0.60 / after 0.55 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Move2 Ember Field | 0.50 | 0.60 | 3 f / 2 f slowed -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Move3 Storm Chain | 0.60 | 0.80 / after 0.65 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Special Overload | 1.00 | 1.00 | 3 f / 2 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Ultimate Elemental Cataclysm | 0.50 | 6.00 | 6 f / 4 f slowed -> **0 f (last: 14 f) both** | flinch / flinch / - -> **- (last: knockdown / knockdown / stagger*)** | Tick -> last: UltimateFinisher, last hit: kick 4.0 deg, push-in 70 cm, slow-mo |

#### Jester (attack 560)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Pratfall | 0.00 | 0.00 | 3 f / 2 f slowed -> **3 f (4th: 8 f) both** | flinch / - / - -> **flinch / recoil / recoil (4th: knockback / stagger / flinch*)** | Light (4th: Heavy), kick 0.6 deg (4th 1.8, push-in 30 cm) |
| Move1 Juggle | 0.20 | 0.50 | 5 f / 3 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move2 Spotlight | 0.40 | 0.60 / after 0.45 | 7 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move3 Punchline | 0.50 | 0.80 / after 0.55 | 7 f / 4 f slowed -> **10 f both** | flinch / flinch / - -> **launch / knockback / stagger*** | Finisher, kick 2.6 deg, push-in 45 cm |
| Special Encore | kit mechanic | 0.40 | by size -> **by size, both** | by size | kit mechanic damage: weight from its slot and size |
| Ultimate Grand Finale | 0.50 | 8.00 | 7 f / 4 f slowed -> **14 f both** | flinch / flinch / - -> **knockdown / knockdown / stagger*** | UltimateFinisher, kick 4.0 deg, push-in 70 cm |

#### Kuu (attack 500)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Haymaker #1 | 0.25 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Haymaker #2 | 0.30 | 0.00 | 4 f / 3 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Haymaker #3 | 0.35 | 0.00 | 4 f / 3 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Shield Bash | 0.40 | 0.60 / after 0.45 | 6 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move2 Bulwark | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Move3 Ground Pound | 0.50 | 1.00 / after 0.55 | 6 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Special Iron Wall | 0.00 | 0.00 | 7 f / 4 f slowed -> **10 f (ticks 0 f) both** | flinch / flinch / - -> **launch / knockback / stagger* (ticks: -)** | Finisher + ticks, kick 2.6 deg, push-in 45 cm |
| Ultimate Immovable | 8.00 | 0.00 | 7 f / 4 f slowed -> **14 f both** | flinch / flinch / - -> **knockdown / knockdown / stagger*** | UltimateFinisher, kick 4.0 deg, push-in 70 cm |

#### Media (attack 335)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Packet Jab #1 | 0.05 | 0.00 | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Packet Jab #2 | 0.05 | 0.00 | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Packet Jab #3 | 0.05 | 0.00 | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Packet Jab #4 | 0.05 | 0.00 | 3 f / none -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Packet Jab #5 | 0.05 -> **0.10** | 0.00 | 3 f / 2 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Blink Cut | 0.00 | 0.20 / after 0.05 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Move2 Lag Spike | 0.30 | 0.30 | 4 f / 2 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Move3 Ping Storm | 0.04 | 1.50 | 3 f / 2 f slowed -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Special Firewall | 0.50 | 0.60 | 3 f / 2 f slowed -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Ultimate System Overload | 0.03 | 6.00 | 4 f / 3 f slowed -> **0 f (last: 14 f) both** | flinch / - / - -> **- (last: knockdown / knockdown / stagger*)** | Tick -> last: UltimateFinisher, last hit: kick 4.0 deg, push-in 70 cm, slow-mo |

#### Myrcia (attack 430)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Mace Smash #1 | 0.20 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Mace Smash #2 | 0.25 | 0.00 | 4 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Mace Smash #3 | 0.30 | 0.00 | 4 f / 3 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Brace | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Move2 Cleave | 0.35 | 0.50 / after 0.40 | 5 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move3 Charge | 0.00 | 0.50 / after 0.05 | 6 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Special Fortify | 0.00 | 0.00 | 4 f / 3 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Ultimate Last Bastion | 6.00 | 0.00 | 7 f / 4 f slowed -> **14 f both** | flinch / flinch / - -> **knockdown / knockdown / stagger*** | UltimateFinisher, kick 4.0 deg, push-in 70 cm |

#### Plack (attack 540)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Combo Strike #1 | 0.12 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Combo Strike #2 | 0.12 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Combo Strike #3 | 0.18 | 0.00 | 4 f / 3 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Rush | 0.00 | 0.25 / after 0.05 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Move2 Guard Stance | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Move3 Spinning Kick | 0.30 | 0.60 / after 0.35 | 4 f / 3 f slowed -> **5 f both** | flinch / - / - -> **stagger / flinch / recoil** | Medium, kick 1.0 deg, push-in 12 cm |
| Special Second Wind | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Ultimate Overdrive | 10.00 | 0.00 | 7 f / 4 f slowed -> **14 f both** | flinch / flinch / - -> **knockdown / knockdown / stagger*** | UltimateFinisher, kick 4.0 deg, push-in 70 cm |

#### Shiraishi (attack 140)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Fire Fist #1 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Fire Fist #2 | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Fire Fist #3 | 0.10 -> **0.18** | 0.00 | 4 f / 3 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Flame Dash | 0.00 | 0.25 / after 0.05 | 6 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move2 Cinder Shield | 0.00 | 0.00 | 7 f / 4 f slowed -> **10 f (ticks 0 f) both** | flinch / flinch / - -> **launch / knockback / stagger* (ticks: -)** | Finisher + ticks, kick 2.6 deg, push-in 45 cm |
| Move3 Eruption | 0.60 | 0.70 / after 0.65 | 7 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Special Phoenix Guard | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Ultimate Inferno Soul | 0.00 | 0.00 | 7 f / 4 f slowed -> **6 f (ticks 0, last 14 f) both** | flinch / flinch / - -> **stagger / flinch / recoil (last: knockdown / knockdown / stagger*)** | Ultimate + ticks -> last: UltimateFinisher, kick 1.4 deg; last hit 4.0 deg, push-in 70 cm, slow-mo |

#### Stargazer (attack 440)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Starshot | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f (4th: 8 f) both** | flinch / - / - -> **flinch / recoil / recoil (4th: knockback / stagger / flinch*)** | Light (4th: Heavy), kick 0.6 deg (4th 1.8, push-in 30 cm) |
| Move1 Falling Star | 1.00 | 1.00 | 6 f / 4 f slowed -> **8 f both** | flinch / flinch / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move2 Constellation | 0.50 | 0.50 | 3 f / 2 f slowed -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Move3 Supernova Seed | 0.40 | 0.60 / after 0.45 | 7 f / 4 f slowed -> **10 f both** | flinch / flinch / - -> **launch / knockback / stagger*** | Finisher, kick 2.6 deg, push-in 45 cm |
| Special Zenith Gaze | 0.80 | 3.20 | 4 f / 2 f slowed -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Ultimate Cosmic Collapse | 0.50 | 5.00 | 7 f / 4 f slowed -> **6 f (ticks 0, last 14 f) both** | flinch / flinch / - -> **stagger / flinch / recoil (last: knockdown / knockdown / stagger*)** | Ultimate + ticks -> last: UltimateFinisher, kick 1.4 deg; last hit 4.0 deg, push-in 70 cm, slow-mo |

#### Timekeeper (attack 280)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Second Hand #1 | 0.08 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Second Hand #2 | 0.08 | 0.00 | 3 f / 2 f slowed -> **3 f both** | flinch / - / - -> **flinch / recoil / recoil** | Light, kick 0.6 deg |
| M1 Second Hand #3 | 0.08 -> **0.16** | 0.00 | 3 f / 2 f slowed -> **8 f both** | flinch / - / - -> **knockback / stagger / flinch*** | Heavy, kick 1.8 deg, push-in 30 cm |
| Move1 Rewind Step | no hit (utility) | 0.20 | - | - | wind-up gesture pose |
| Move2 Stasis Bubble | 0.40 | 0.40 | 2 f / none -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Move3 Fast Forward | no hit (utility) | 0.00 | - | - | wind-up gesture pose |
| Special Temporal Echo | kit mechanic | 0.00 | by size -> **by size, both** | by size | kit mechanic damage: weight from its slot and size |
| Ultimate Zero Hour | kit mechanic | 0.80 | by size -> **by size, both** | by size | kit mechanic damage: weight from its slot and size |

#### Weather (attack 330)

| Attack | Hit at (s) before -> after | Lock (s) / dodge-cancel | Hit stop target / operator before -> after | Reaction grunt / brute / boss before -> after | After: weight, camera |
|---|---|---|---|---|---|
| M1 Gust Bolt | 0.10 | 0.00 | 3 f / 2 f slowed -> **3 f (4th: 8 f) both** | flinch / - / - -> **flinch / recoil / recoil (4th: knockback / stagger / flinch*)** | Light (4th: Heavy), kick 0.6 deg (4th 1.8, push-in 30 cm) |
| Move1 Downpour | 0.50 | 0.60 | 2 f / none -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Move2 Blizzard | 0.50 | 0.60 | 2 f / none -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Move3 Thunderstorm | 0.50 | 0.80 | 4 f / 3 f slowed -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Special Cyclone | 0.60 | 0.80 | 3 f / none -> **0 f both** | flinch / - / - -> **- / - / -** | Tick, spark only |
| Ultimate Perfect Storm | 1.00 | 1.00 | 4 f / 3 f slowed -> **0 f (last: 14 f) both** | flinch / - / - -> **- (last: knockdown / knockdown / stagger*)** | Tick -> last: UltimateFinisher, last hit: kick 4.0 deg, push-in 70 cm, slow-mo |

## Verification
| Check | Where | Result |
| --- | --- | --- |
| Engine-independent rules, strict flags (`-Wall -Wextra -Wshadow -Wconversion -Werror`, no exceptions / RTTI) | cloud, `Tests/run_core_tests.sh` | **PASS, 0 failures**: 103 core, 12,990 meta, 2,460 kit, 4,450 account, 3,964 campaign, 4,380 tester, 443 arcade, 59 mini-game, 230 gacha, 777 Ark, 151 angel, **972 feel (new)**; playthrough bot: no soft-locks |
| Feel tests (`Tests/feel_tests.cpp`) | cloud | weights, the owner's frame targets (light 2-4, heavy 6-10, ultimate finisher 12+), every enemy weight reacts to heavy+, poise vs light spam, boss interrupt cooldown, broken enemies only recoil, poses (wind-up < 0, impact > 0, back to neutral), every `[BIB.CombatFeel]` key present in `DefaultGame.ini` with the code's default and no stray keys, the kit runtime's hit origins for all 17 kits (basic finisher = last hit of the kit's string; **13 of 13** ultimates that deal damage directly have exactly one finisher moment, last), recovery cancel (Kuu's Ground Pound cancellable after its hit; Myrcia's Cleave not before its hit) |
| Balance (`Tests/kit_balance.cpp`, `docs/operator-kits.md` regenerated) | cloud | 0 of 17 data kits out of band; Luna (analytic, new) 0.84 in the 4-star band (was 1.69) |
| Unreal build (`BoldInBeautyDemoEditor Mac Development`) | **owner's Mac: OPEN** | not possible here (no engine in the container). The Unreal-side code was reviewed line by line for UE 5.8 API use, unity-build name clashes, shadowing (C4456 / C4458) and unused code; risk remains until the first build |
| Per-operator test-map fight (`Tools/verify_feel.py`, new) | **owner's Mac: OPEN** | for each of the 20 operators against live angels on Stage 1: the full basic string, every move and the Special, the ultimate (bar filled by `BIBFillUlt`); logs per cast the weights felt, max hit stop, reactions, kicks / push-ins / rumble, pose range, ultimate beat and finisher; also a beat skip and a dodge cancelling Kuu's recovery. Writes `Saved/VerifyFeel.log` (`RESULT PASS|FAIL`) |
| Regression | **owner's Mac: OPEN** | `Tools/verify_combat.py` (wording updated), `Tools/verify_kits.py`, `Tools/verify_campaign.py` |

Commands on the Mac (from the documentation root; as for the earlier passes, no `-log`):
```
"<UE_5.8>/Engine/Build/BatchFiles/Mac/Build.sh" BoldInBeautyDemoEditor Mac Development -Project="$PWD/Demo/BoldInBeautyDemo/BoldInBeautyDemo.uproject" -WaitMutex -NoHotReloadFromIDE
"<UE_5.8>/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/Demo/BoldInBeautyDemo/BoldInBeautyDemo.uproject" -BIBDev -ExecCmds="py Tools/verify_feel.py"
(then verify_combat.py, verify_kits.py, verify_campaign.py the same way)
```
Expected in `verify_feel.py` (described test; not yet run): every basic string row OK with light hits and a heavy finisher (Judge:
light impacts until someone is guilty); damaging moves felt with a reaction and a hit stop > 0; utility moves (buffs, shields,
stances, Judge's Court setting is skipped) show only the wind-up pose; every ultimate starts a beat; ultimates whose damage lands
within 12 s show an ultimate finisher (Alchemist's puddle, Arita's takeover, Timekeeper's Zero Hour and Narrator's Switch are
expected to report "beat; no damage" because their damage needs a trigger or comes later).

## Missing assets (not created: no placeholder art)
| Asset | Used by | Hook |
| --- | --- | --- |
| Combat hit sounds, 3 layers per weight (body thump, crack / slice, sweetener) for tick, light, medium, heavy, finisher, ultimate, ultimate finisher | every hit | `Sound.<Weight>` in `[BIB.CombatFeel]` (comma-separated asset paths), `Sound.Volume` |
| Ultimate beat sting, Perfect Dodge whoosh | beats, dodges | `Sound.UltCinematic`, `Sound.PerfectDodge` |
| Operator animation: wind-up / impact / recovery per attack, dodge, parry, hit reactions | all 20 operators | the procedural poses stand in (`StartPose` timings are the clip timings to match) |
| Angel animation: recoil, flinch, stagger, knockback, launch, knockdown + get-up | every angel | the tilt stands in (`[BIB.CombatFeel] Reaction.*.Seconds` = clip lengths) |
| Per-weight hit-spark VFX (Niagara) | every hit | code-built glowing shapes (`BIBFx`, `M_BIBFx`) sized by `Weight.*.SparkScale`; GameplayCue tags stay free for real VFX |
| Ultimate camera sequences (per operator) | 20 ultimates | procedural framing (`UltCamera*`) |
| Gamepad button prompts in the HUD | HUD | the HUD still shows keyboard keys |
| Force-feedback curves (optional) | rumble | native dynamic force feedback is used, no asset needed |

## Warnings, issues and decisions for the owner
| Status | Severity | Finding / question | Evidence and impact | Proposed next action |
| --- | --- | --- | --- | --- |
| OPEN | blocker (verification) | Unreal build and the in-editor tests have not run | no engine in the cloud container | run the commands above on the Mac; send the logs back if anything fails |
| PROPOSED | decision | Luna rebalance (string 0.45, Blackhole 500 / 2200 and 0.9 s lock, Destabilisation 9,000 within 16 m after 10 s, half-rate charge, no overlap, no self-recharge) | analytic power 1.69 -> 0.84 | keep, or tell me which part to restore |
| OPEN | decision | Judge and Narrator numbers unchanged | the balance yardstick cannot value Judge's verdict kills or Narrator's speed game (basic-only analytic: Judge ~476 dps, Narrator ~1,175 dps vs a 5-star median offence ~3,700) | owner: say if they feel weak in play; a proper simulation of both is a separate task |
| PROPOSED | info | data kits with their own string no longer get the engine string's x0.85-1.45 on top | about -6% basic damage for 13 data kits; the balance simulator never included it; all in band | none |
| PROPOSED | info | gamepad layout | `ABIBPlayerController::PadKey` | owner's call; Settings > Controls only rebinds keyboard keys |
| OPEN | info | the 12 fps held-frame look | every felt hit forces a drawn frame, so a 3-frame hit stop is visible; owner play-through decides if it reads well | `[BIB.CombatFeel]` tuning live with `BIBFeelReload` |
| OPEN | info | Angel AI is untouched except reactions | goal 2 brief queued | `reports/demo/goal-2-angel-ai-brief.md` |
