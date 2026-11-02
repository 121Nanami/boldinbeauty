# Goal 2 (queued) — Angel AI rework brief

- Status: QUEUED. Start after goal 1 (combat feel rework, branch `claude/happy-babbage-gmmcty`) is finished.
- Recorded: 2026-10-13, owner message in the Claude Code session that ran goal 1 ("to save limits store this as goal 2 after this").
- Lead: owner (standalone Claude Code session). Same working pattern as goal 1: Step 0 discovery, STOP for owner approval, then work.
- The brief below is the owner's text, kept verbatim.

---

<context>
Project: BoldInBeauty in <local-home>/Documents/BoldInBeauty-Project. Player combat was just reworked (hitstop, dodge-counter window, stagger meter, cancel windows).
Problem: enemy AI, especially every Angel enemy, is predictable and passive. Goal: every Angel AI should fight at or above a skilled player's level while staying fair. They should play smart and must not cheat.
</context>

<step_0_discovery>
1. Find all enemy AI: behavior trees, state machines, blackboards, perception, and attack selection. List every enemy type, Angels first.
2. For each type, describe its current decision logic in 2-3 lines and name its specific failures, for example "walks into attacks, never dodges, attacks one at a time in a queue".
STOP after this step and wait for my approval.
</step_0_discovery>

<target_behaviors>
Every Angel enemy MUST have these. Other enemies get the subset that fits their role.
- Spacing: holds its ideal range, backs off when outranged, and closes in when the player is recovering.
- Punishes mistakes: attacks during the player's whiffed recovery, heals, and ultimate wind-ups.
- Defense: dodges, blocks, or sidesteps telegraphed player heavies, with a skill-scaled chance. It must not react perfectly every time.
- Feints and mixups: delayed attacks, varied combo enders, and attack timings chosen to bait early dodges.
- Adapts: tracks the player's habits (dodge direction, favorite opener, spam) over the last N actions and counters them.
- Coordination: a shared attack-token system so groups flank and alternate pressure instead of queuing one by one. Ranged enemies reposition for line of sight.
- Self-preservation: retreats, regroups, or changes tactics when low on health or near stagger break.
- Uses its whole kit: picks attacks based on distance, angle, cooldowns, and player state, never purely at random.
</target_behaviors>

<fairness_rules>
- No reading inputs before they happen. Reactions use real perception plus a reaction delay (tunable, around 150-300 ms by tier).
- Every attack keeps a readable telegraph that the player can learn and dodge.
- Expose difficulty knobs per enemy tier in data: reaction time, aggression, dodge chance, adaptation rate.
</fairness_rules>

<constraints>
- Change only AI, perception, and enemy ability code or data. Leave the player ability code from the previous pass alone.
- Stop and ask before adding a dependency or plugin, deleting files, or replacing the AI framework entirely.
</constraints>

<verification>
- Builds with zero new errors.
- For each Angel type, run a test-map fight and log which target behaviors fired. Also confirm the enemy can still be beaten with clean play.
</verification>

<output_format>
✅ per enemy type: behaviors added, files touched.
At the end: a table of enemy type × target behaviors (yes/no) and the default difficulty knob values.
</output_format>

---

## Starting notes from goal 1's discovery (for the next session)
- Angel AI lives in `Demo/BoldInBeautyDemo/Source/BoldInBeautyDemo/BIBGame/BIBEnemy.cpp` (per-actor behaviour, telegraphs, attack tokens) and
  `BIBRules/BIBAngelMind.*` (shared mind that learns the commander's habits; engine-free, `Tests/angel_tests.cpp`).
  Archetype numbers: `BIBRules/BIBCampaign.cpp`; flinch / poise / Strain (was "Daze") meter: `BIBRules/BIBStagger.*`.
- There are no behaviour trees, blackboards or AI perception components; the AI is hand-written C++ ticks.
- Unreal cannot be built in the cloud container; engine-free rules compile with g++ (`Tests/run_core_tests.sh`), the editor build
  and PIE scripts (`Tools/verify_*.py`) run on the owner's Mac.
