# Gameplay Ability System in the demo

Status: WORKING, 2026-10-04. Compiles on UE 5.8.3 (Mac, 0 errors, 0 warnings) and passes the first PIE runs; the full per-ability
verification (`Tools/verify_kits.py`, Mac, 919400f) passed: 126 casts OK, 0 flagged. Windows is OPEN.

## What "translated to GAS" means here (honest summary)

The demo uses the real Gameplay Ability System for everything GAS is good at, and keeps a pure C++ rules layer for the parts that must
be unit-testable without the editor. It is a hybrid on purpose:

| Concern | Where it lives |
| --- | --- |
| Every operator slot (regular attack, Move 1-3, Special, Ultimate, Dodge, Parry) | a `UGameplayAbility` (`UBIBAbility_KitSlot`), granted per team member; activation goes through `UAbilitySystemComponent::TryActivateAbility` |
| Health, ultimate charge, shield, the two kit resources | attributes of `UBIBAttributeSet` on the operator's component |
| Damage and healing | instant GameplayEffects (`GE_BIB_Damage`, `GE_BIB_Heal`, SetByCaller magnitudes) writing the `IncomingDamage` / `IncomingHealing` meta attributes; `PostGameplayEffectExecute` turns them into Health changes |
| Cooldowns | GameplayEffects granting `BIB.Cooldown.<Operator>.<Slot>` for the cooldown's duration (re-applied with the remaining time when something reduces it) |
| Invulnerability (dodge, kit i-frames), parry window | timed GameplayEffects granting `BIB.State.Invulnerable` / `BIB.State.Parrying` |
| Combat and kit state | loose gameplay tags mirrored every tick (`BIB.State.*`), e.g. `BIB.State.Judge.Lawyer`, `BIB.State.Luna.Blackhole`, `BIB.State.Kit.Curse` |
| Enemy health and statuses | the enemy has its own component and the same attribute set; operator-applied statuses are mirrored as `BIB.Status.*` tags |
| Visual effects | `GameplayCue.BIB.*` tags are executed on the operator's component at the effect position; the demo also draws coloured placeholder shapes, so Niagara / animation can be attached to the same tags later |
| Kit rules that are data: what a move does, timelines, zones, projectiles, passives, statuses, hooks | `BIBRules/` (`BIB::Kits::FRuntime`), pure C++, 2,400+ g++ checks and a balance simulator; the Unreal layer implements `IWorld` (`FBIBKitWorld`) on top of GAS |

Things GAS does **not** do here, deliberately: the rules layer decides stacking, DoT ticks, buff arithmetic and reaction logic (so they can be
tested and balanced offline); GAS applies the results. Nothing is replicated (single player demo). The Luna, Judge and Narrator kits keep their
hand-written state machines, but their activation, cooldowns, health, ultimate, damage and state tags run through GAS. The attribute set mirrors the
rules layer's modifiers (`DamageDealtMult`, `DamageTakenMult`, `MoveSpeedMult`, `AttackSpeedMult`, resources, shield) so tools can read them.

## Files

| File | Role |
| --- | --- |
| `Source/BoldInBeautyDemo/BIBGAS/BIBGameplayTags.*` | cached tag lookups, `BIB.Cooldown.<Operator>.<Slot>` / `BIB.Ability.<Operator>.<Slot>` builders, `LogMissingTags()` |
| `BIBGAS/BIBAttributeSet.*` | Health, MaxHealth, Ultimate, Shield, ResourceA/B, mirrored modifiers, IncomingDamage / IncomingHealing |
| `BIBGAS/BIBAbilitySystemComponent.*` | `ApplyDamage`, `ApplyHealing`, `StartCooldown`, `GetCooldownRemaining`, `ReduceCooldown`, `GrantTimedTag`, `SyncMirroredTags`, `ExecuteBIBCue`, `OnDamageTaken` |
| `BIBGAS/BIBGameplayEffects.*` | the GameplayEffects, built once in code (no assets) |
| `BIBGAS/BIBGameplayAbility.*` | `UBIBAbility_KitSlot`: one class for every slot; `InputID` packs member and slot; grants `BIB.Ability.<Operator>.<Slot>` while active |
| `BIBGame/BIBPlayerGAS.cpp` | component set-up, attribute-backed health / ultimate, cooldown helpers, ability granting and activation, mirrored tags |
| `BIBGame/BIBPlayerData.cpp` | the 17 data kits: input, hold / toggle, hit handling, HUD |
| `BIBGame/BIBKitWorld.*` | `IWorld` adapter: damage, healing, status, impulses, cooldowns, cues, movement |
| `BIBGame/BIBEnemy.*` | enemy component, status book, fear / root / slow / stun / illusion behaviour |
| `Config/DefaultGameplayTags.ini` | 534 generated tags (`Tools/gen_tags.py`) |

## Tags

The tag list is generated from the code, never typed by hand: `python3 Tools/gen_tags.py` rebuilds `Config/DefaultGameplayTags.ini` from
`BIB::Kits::AllGameplayTags()` (abilities, cooldowns, passives, statuses, buffs, cues, states and the bespoke Luna / Judge / Narrator names)
and keeps tags the owner added in the editor. `Tests/kit_tests.cpp` checks that every operator has `BIB.Ability.<Operator>.<Slot>` and
`BIB.Cooldown.<Operator>.<Slot>` for all 8 slots (Basic, Move1-3, Special, Ultimate, Dodge, Parry), and that every tag the Unreal layer looks up exists.
In the editor, console `BIBCheckTags` prints how many expected tags the project does not register (0 on the Mac run).

Namespaces: `BIB.Kit.*`, `BIB.Ability.*`, `BIB.Cooldown.*`, `BIB.Passive.*`, `BIB.State.*`, `BIB.Status.*`, `BIB.Buff.*`, `BIB.Type.*`, `BIB.Slot.*`,
`BIB.Damage.*`, `BIB.Event.*`, `BIB.Data.*` (SetByCaller names), `BIB.Team.*`, `GameplayCue.BIB.*`.

## Test and balance hooks (console in PIE)

`BIBGrantAll`, `BIBTeam <A> <B> <C>` (hub; names from the roster), `BIBFillUlt`, `BIBUltVariant <n>` (alternative ultimate; the mechanism is kept but no operator has one since the owner's 2026-10-04 answer, in game R would cycle it),
`BIBTags` (active operator's gameplay tags), `BIBCheckTags`, `BIBDamageEnemies <n>`, `BIBSetNarratorSpeed <n>`, `BIBSetCampaignProgress <n>`.
All 20 operators are owned with `-BIBDev` (the developer profile the verify scripts use) and by the admin account; a normal account starts with only Luna (see `docs/accounts-and-saving.md`).

## Adding or changing an ability

1. Edit the kit data in `BIBRules/BIBKitData*.cpp` (steps, cooldown, tags, source label PROPOSED / SOURCE / PLACEHOLDER).
2. `Tests/run_core_tests.sh` (and `--balance <Operator>` for the per-ability breakdown), then `python3 Tools/gen_tags.py` and `python3 Tools/gen_kit_docs.py`.
3. Build with `-NoHotReloadFromIDE`. No Unreal code changes are needed for a data ability; a new named mechanic needs a hook in `BIBKitHooks.cpp`.

## Known limits and OPEN items

- GameplayCue notifies are not authored; cues fire into the cue manager (no handler) and the demo draws debug shapes (Development builds only). On startup the
  engine logs one warning that no `GameplayCueNotifyPaths` are set (harmless; set it in `Config/DefaultGame.ini` when real cue assets exist).
- Doom (Death) is a mark only; expiry no longer kills (removed the old `DoomDeath` event so a free mark could never become a free kill).
- Enemies apply no statuses to operators, so Shiraishi's status resistance and Judge's immunity are exercised by rules tests only.
- Per-operator inline Notion databases could not be written through the connector (title property rejected); the designs are in the global GDD_Abilities, GDD_Skills
  and GDD_Tags databases plus an index page.
- Windows never built; the PIE sweep of all 20 operators passed (126 OK, 0 flagged) but a cast only has to change something observable, so self-buff ultimates and passives passed on a message; active timed buffs are mirrored as `BIB.Buff.*` tags since 8480860 (Mac verified: Plack, Myrcia, Kuu).
