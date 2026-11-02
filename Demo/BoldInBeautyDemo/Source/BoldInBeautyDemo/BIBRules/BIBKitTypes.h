// Boldinbeauty demo — operator kit data model (engine-independent, no Unreal includes).
//
// A kit is DATA: three standard moves, one Special and one Ultimate (MAIN §6), a regular attack ("M1"), 0-4 passives
// (5-6 star only) and up to two kit resources. The runtime in BIBKitRuntime.h interprets the data against an
// IWorld; the Unreal layer (BIBGAS/) implements IWorld on top of the Gameplay Ability System.
//
// Numbers are labelled like BIBTuning.h:
//   SOURCE      = written by the owner (Notion operator pages / MAIN / operator files)
//   PROPOSED    = designed by the assistant on the owner's request (2026-10-04) from the owner's playstyle notes
//   PLACEHOLDER = needed so the demo runs; replace when the owner decides
//
// Rules code must stay exception-free and RTTI-free (Unreal builds with both off).
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace BIB
{
	namespace Kits
	{
		// ------------------------------------------------------------------ geometry
		struct FVec
		{
			double X = 0.0;
			double Y = 0.0;
			double Z = 0.0;
		};

		inline FVec operator+(const FVec& A, const FVec& B) { return {A.X + B.X, A.Y + B.Y, A.Z + B.Z}; }
		inline FVec operator-(const FVec& A, const FVec& B) { return {A.X - B.X, A.Y - B.Y, A.Z - B.Z}; }
		inline FVec operator*(const FVec& A, double S) { return {A.X * S, A.Y * S, A.Z * S}; }
		inline double Dot2(const FVec& A, const FVec& B) { return A.X * B.X + A.Y * B.Y; }
		inline double Len2(const FVec& A) { return std::sqrt(A.X * A.X + A.Y * A.Y); }
		inline double Dist2(const FVec& A, const FVec& B) { return Len2(A - B); }
		inline FVec Norm2(const FVec& A)
		{
			const double L = Len2(A);
			return L > 1e-9 ? FVec{A.X / L, A.Y / L, 0.0} : FVec{1.0, 0.0, 0.0};
		}
		inline FVec Rotate2(const FVec& A, double Radians)
		{
			const double C = std::cos(Radians);
			const double S = std::sin(Radians);
			return {A.X * C - A.Y * S, A.X * S + A.Y * C, A.Z};
		}

		// ------------------------------------------------------------------ slots
		enum class ESlotId : int
		{
			Basic = 0, // regular attack (M1)
			Move1,
			Move2,
			Move3,
			Special,
			Ultimate,
			Count
		};
		constexpr int SlotCount = static_cast<int>(ESlotId::Count);
		const char* SlotName(ESlotId Slot);      // "M1", "Move1" ...
		const char* SlotKeyLabel(ESlotId Slot);  // "LMB", "1" ...

		// ------------------------------------------------------------------ statuses (on enemies)
		enum class EStatus : int
		{
			None = 0,
			Stun,
			Slow,
			Root,
			Burn,
			Bleed,
			Poison,
			Freeze,
			Shock,
			Wet,
			Mark,
			Vulnerable,
			Weaken,
			Fear,
			Doom,
			Infection,
			Mutated,
			Immune,
			Illusion,
			Count
		};
		constexpr int StatusCount = static_cast<int>(EStatus::Count);

		struct FStatusRule
		{
			const char* Name;
			const char* Tag;           // gameplay tag, e.g. "BIB.Status.Burn"
			bool bDamageOverTime;      // deals Power damage per stack every TickInterval
			double TickInterval;
			int MaxStacks;
			double DefaultSeconds;
		};
		const FStatusRule& StatusRule(EStatus Status);

		// ------------------------------------------------------------------ stat modifiers (on the caster)
		enum class EBuff : int
		{
			DamageDealt = 0,  // additive deviation: +0.25 = x1.25
			DamageTaken,      // additive deviation: -0.5 = takes half
			MoveSpeed,
			AttackSpeed,      // basic attack interval divisor (+0.5 = 50% faster)
			CooldownFlow,     // all cooldowns recover faster (+1.0 = twice as fast)
			LifeSteal,        // fraction of damage dealt returned as health
			Reflect,          // fraction of damage taken reflected to the attacker
			Evade,            // number of incoming hits negated (Magnitude = charges)
			Regen,            // health fraction of max per second
			Immovable,        // 1 = cannot be knocked back
			StatusResist,     // fraction chance of shrugging off a status entirely (0..1)
			EvadeChance,      // chance to negate an incoming hit
			Count
		};
		constexpr int BuffCount = static_cast<int>(EBuff::Count);
		const char* BuffTag(EBuff Buff);

		// ------------------------------------------------------------------ visual cues
		// Each cue is a GameplayCue tag ("GameplayCue.BIB.<Name>"): the demo draws coloured placeholder shapes and the
		// owner can attach Niagara / animation to the same tags later.
		enum ECue : int
		{
			CueNone = 0,
			CueSlash, CueSlam, CueBurst, CueNova, CueRing, CueBeam, CueDash, CueBlink, CueHeal, CueShield, CueBuff,
			CueDebuff, CueMark, CueExecute, CueZone, CueMeteor, CueLightning, CueExplosion, CueFlame, CueFrost,
			CueBlood, CuePoison, CueNature, CueTime, CueStar, CueUltimate, CueGlitch, CueSplash, CueWind,
			CueCount
		};

		// ------------------------------------------------------------------ timeline steps
		enum class EStep : int
		{
			Melee = 0,   // cone in front of the caster
			Burst,       // circle (centred on the caster, or ahead via Offset)
			Projectile,  // travelling circle(s)
			Zone,        // persistent circle that ticks
			Beam,        // rectangle in front, optionally channelled over Duration
			Dash,        // moves the caster; damages what it passes
			Teleport,    // blinks the caster (to an enemy, forward or behind)
			Buff,        // timed modifier on the caster
			Heal,        // Param = fraction of max health, Damage = flat
			Shield,      // Damage = amount, Duration = seconds
			Resource,    // Param = amount (+gain / -spend) on resource Stat
			Execute,     // kill what meets the condition (bosses take a fraction)
			Chain,       // hits the nearest Hits enemies within Radius
			Detonate,    // consumes statuses for damage
			SelfDamage,  // Param = fraction of max health (cost / sacrifice)
			Hook,        // named kit mechanic (see EHook)
			Cue          // visual only
		};

		// Named mechanics that do not fit the generic steps.
		enum class EHook : int
		{
			None = 0,
			// Alchemist / Arita (owner kits)
			InfectTouched,        // infects the nearest enemy in reach
			MutateNearest,        // syringe: the uninfected explode, the infected mutate
			RingLeader,           // every infected enemy gains Illusion
			SoilPuddle,           // Alchemist ultimate: puddle that rolls an effect on each enemy
			SpringTime,           // Arita: hold-to-grow flower field (uses the held seconds)
			NaturesCurse,         // Arita: toggle - damage split and soul link
			FullTakeover,         // Arita: giant flower creature
			// Adapt
			AdaptCastCopied,      // casts the selected adapted ability
			AdaptSwitch,          // selects the next adapted ability
			AdaptPassiveCopy,     // copies a teammate passive (or heals 25%)
			// Jester
			JesterEncore,         // replays the last three abilities
			// Timekeeper
			TimeRecord,           // starts recording damage dealt (echo)
			TimeRelease,          // replays the recorded damage Param times
			// Stargazer
			StarSeed,             // marks an enemy; it detonates later with bonus damage
			Count
		};

		// Targeting / behaviour flags for steps.
		enum EStepFlag : unsigned
		{
			FlagNone = 0,
			FlagAtTarget = 1u << 0,        // centre on the nearest enemy in front (within Range) instead of the caster
			FlagFollowCaster = 1u << 1,    // zone moves with the caster
			FlagRandomEnemy = 1u << 2,     // choose a random enemy each hit (deterministic through IWorld::Random01)
			FlagNearestEnemy = 1u << 3,    // choose the nearest enemy
			FlagMarkedOnly = 1u << 4,      // only affects enemies with Doom/Mark
			FlagBehindTarget = 1u << 5,    // teleport behind the target
			FlagIgnoreBoss = 1u << 6,      // bosses are unaffected
			FlagInvulnerable = 1u << 7,    // caster cannot be hit while the step runs
			FlagTrueDamage = 1u << 8,      // ignores damage-taken modifiers
			FlagAllEnemies = 1u << 9,      // everything alive (ultimates)
			FlagRespectBoss = 1u << 10,    // Execute: bosses take BossFraction of max health instead of dying
			FlagFromAimPoint = 1u << 11,   // origin is Range ahead of the caster (ground target)
			FlagHitTarget = 1u << 12,      // passives/reactions: act on the enemy that triggered them
			FlagStoreTarget = 1u << 13,    // remember the chosen enemy for a later FlagStoredTarget step
			FlagStoredTarget = 1u << 14,   // act on the remembered enemy
			FlagUseStored = 1u << 15,      // Damage += Param2 x the value stored by the ability (damage taken, recorded damage)
			FlagTurret = 1u << 16,         // zone tick: hit the nearest enemy within Radius instead of everything inside
			FlagHitAmount = 1u << 17,      // passives: Damage += Param2 x the damage that triggered the passive
			FlagUseResource = 1u << 18     // Damage += Param2 x kit resource Stat (Stargazer's starlight)
		};

		struct FStep
		{
			EStep Kind = EStep::Burst;
			double At = 0.0;          // seconds after the ability starts
			double Duration = 0.0;    // zones/beams/buffs/shields; 0 = instant
			double Interval = 0.0;    // repeat interval while Duration runs (0 = single hit at At)
			double Range = 0.0;       // melee reach / projectile range / beam length / dash distance
			double Radius = 0.0;      // circle radius / projectile radius / beam width / chain reach
			double Offset = 0.0;      // centre offset forward of the caster
			double ArcDot = -1.0;     // melee cone: minimum dot with facing (-1 = full circle)
			double Damage = 0.0;      // per hit, absolute (x caster damage multiplier)
			double AtkMult = 0.0;     // plus AtkMult x caster attack
			int Hits = 1;             // projectile count / chain targets / repeat count
			int Pierce = 1;           // projectile pierce (99 = all)
			double Speed = 0.0;       // projectile speed
			EStatus Status = EStatus::None;
			double StatusSeconds = 0.0;
			double StatusPower = 0.0; // DoT damage per tick / slow fraction / vulnerability fraction ...
			int StatusStacks = 1;
			double Knockback = 0.0;   // outward impulse (cm/s); negative = pull
			double Param = 0.0;       // step specific (heal fraction, buff magnitude, resource amount ...)
			double Param2 = 0.0;
			int Stat = 0;             // EBuff for Buff, resource index for Resource
			EHook HookId = EHook::None;
			int CueId = 0;            // visual cue (index into CueName)
			unsigned Flags = FlagNone;
			int Combo = -1;           // basic attack: only run when the combo index matches (-1 = always)
			double ExecuteBelow = 0.0;// Execute: kill when HP fraction is below this (1.0 = anything)
			double WetBonus = 0.0;    // extra damage fraction against Wet enemies (lightning)
			EStatus BonusStatus = EStatus::None; // PROPOSED 2026-10-07: pay-off status ...
			double BonusVs = 0.0;     // ... extra damage fraction against an enemy that carries it (Stun, Burn, Doom ...)
			double BossFraction = 0.15; // Execute vs bosses: fraction of max health instead

			// ---- fluent builders (keep the data tables readable)
			FStep& WithAt(double V) { At = V; return *this; }
			FStep& WithDuration(double Seconds, double Tick = 0.0) { Duration = Seconds; Interval = Tick; return *this; }
			FStep& WithRadius(double V) { Radius = V; return *this; }
			FStep& WithOffset(double V) { Offset = V; return *this; }
			FStep& WithDamage(double V, double Atk = 0.0) { Damage = V; AtkMult = Atk; return *this; }
			FStep& WithStatus(EStatus S, double Seconds, double Power = 0.0, int Stacks = 1)
			{
				Status = S;
				StatusSeconds = Seconds;
				StatusPower = Power;
				StatusStacks = Stacks;
				return *this;
			}
			FStep& WithKnock(double V) { Knockback = V; return *this; }
			FStep& WithFlags(unsigned V) { Flags |= V; return *this; }
			FStep& WithCue(int V) { CueId = V; return *this; }
			FStep& WithCombo(int V) { Combo = V; return *this; }
			FStep& WithHits(int V) { Hits = V; return *this; }
			FStep& WithParams(double A, double B = 0.0) { Param = A; Param2 = B; return *this; }
			FStep& WithPierce(int V) { Pierce = V; return *this; }
			FStep& WithWet(double V) { WetBonus = V; return *this; }
			FStep& WithBonusVs(EStatus S, double V) { BonusStatus = S; BonusVs = V; return *this; }
			FStep& WithExecute(double Below, double BossFrac = 0.15) { ExecuteBelow = Below; BossFraction = BossFrac; return *this; }
			FStep& WithRange(double V) { Range = V; return *this; }
			FStep& WithStat(int V) { Stat = V; return *this; }
		};

		// A weighted or indexed alternative inside an ability (Gambler outcomes, Jester acts, Weather fronts).
		enum class EBranchMode : int
		{
			None = 0,
			Weighted,    // chosen with IWorld::Random01 (and the owner's luck modifier)
			ByAct,       // Jester: the current act (deterministic rotation)
			ByFront      // Weather: the current weather front (rotates each cast)
		};

		struct FBranch
		{
			std::string Label;
			double Weight = 1.0;
			bool bGood = false;       // counts toward Gambler's luck pity
			std::vector<FStep> Steps;
		};

		// A reaction armed by an ability: runs Steps when the trigger fires inside the window.
		enum class EReaction : int
		{
			None = 0,
			OnDamaged,     // the caster is hit (before it lands when bNegate)
			OnShieldBreak  // the caster's shield was just depleted
		};

		struct FReaction
		{
			EReaction Trigger = EReaction::None;
			double Window = 0.0;      // seconds the reaction stays armed after the cast
			int MaxTriggers = 1;
			bool bNegate = false;     // the triggering hit does no damage
			std::vector<FStep> Steps;
		};

		// ------------------------------------------------------------------ abilities
		enum class ECast : int
		{
			Instant = 0,  // runs its timeline and finishes
			Hold,         // keeps running while the key is held (Arita's Spring time); release is reported to the runtime
			Toggle        // on/off (Arita's Natures Curse)
		};

		struct FAbilityDef
		{
			std::string Id;            // "Blood.Move1" -> tag BIB.Ability.Blood.Move1
			std::string Name;
			std::string Note;          // one-line description for the HUD / docs
			ESlotId Slot = ESlotId::Move1;
			ECast Cast = ECast::Instant;
			double Cooldown = 0.0;     // seconds (basic attacks: the attack interval)
			double LockSeconds = 0.0;  // movement locked after the start
			double TotalSeconds = 0.0; // timeline length (the ability stays active this long)
			int CostResource = -1;     // kit resource index (0/1) spent on cast, -1 = none
			double CostAmount = 0.0;
			double HealthCostFraction = 0.0; // fraction of max health paid (cannot kill)
			EStatus RequiresStatusOnEnemy = EStatus::None; // at least one enemy must have it
			int RequiresResource = -1;       // minimum resource needed (not spent)
			double RequiresAmount = 0.0;
			double CancelDamageFraction = 0.0; // cancelled by this fraction of max health lost (0 = never)
			int ComboLength = 1;       // basic attack combo length
			int FeelWeight = -1;       // hit feel weight forced for every hit (BIB::Feel::EWeight, BIBHitFeel.h), -1 = classify
			double ComboWindow = 1.2;  // seconds before the combo resets
			std::vector<FStep> Steps;
			EBranchMode BranchMode = EBranchMode::None;
			std::vector<FBranch> Branches;     // a Hook-less step with Kind==Cue and Param==-1 marks where branches run (see runtime)
			double BranchAt = 0.0;             // when the chosen branch starts
			FReaction Reaction;
			bool bStoresDamageTaken = false;   // FlagUseStored reads the damage taken while the ability ran
			bool bAutomatic = false;           // runs by itself (Adapt's Move 2/3); pressing the key does nothing
			double AutoInterval = 0.0;         // > 0: the timeline repeats on its own every AutoInterval seconds
			int Source = 0;            // 0 PLACEHOLDER, 1 SOURCE (owner), 2 PROPOSED (assistant)
			std::vector<std::string> Tags; // extra gameplay tags (e.g. "BIB.Type.Control")
		};

		// ------------------------------------------------------------------ passives
		enum class ETrigger : int
		{
			Always = 0,      // permanent stat modifier (Mods)
			OnHit,           // the owner dealt damage (any source)
			OnBasicHit,
			OnKill,
			OnDamaged,       // the owner took damage
			OnDodge,         // an Evade charge / dodge negated a hit
			OnParry,
			OnAbilityCast,   // any ability cast (Param = slot filter, -1 any)
			OnLowHealth,     // health fell below Threshold fraction
			Periodic,        // every Interval seconds
			OnStatusApplied, // the owner applied a status (Param = status filter)
			OnSwitchIn       // became the active operator
		};

		struct FPassiveDef
		{
			std::string Id;
			std::string Name;
			std::string Note;
			ETrigger Trigger = ETrigger::Always;
			double Chance = 1.0;
			double Interval = 0.0;     // Periodic interval / internal cooldown for the others
			double Threshold = 0.0;
			double Param = -1.0;       // trigger filter
			int EveryN = 1;            // fires every Nth trigger (OnAbilityCast ...)
			int CondResource = -1;     // only while this kit resource ...
			double CondMin = 0.0;      // ... is at least CondMin
			EStatus CondEnemyStatus = EStatus::None; // only while some enemy has this status
			double Mods[BuffCount] = {}; // permanent modifiers (Always)
			std::vector<FStep> Steps;  // executed at the owner's position when triggered
			int Source = 0;
		};

		// ------------------------------------------------------------------ kit
		struct FKitTraits
		{
			double DamageTakenMod = 0.0;    // innate additive deviation (-0.2 = takes 20% less)
			double StatusResist = 0.0;      // 0..1 chance to shrug off a status entirely (Shiraishi)
			bool bImmuneToBurn = false;
			bool bKnockbackImmune = false;
			double InitialShield = 0.0;     // shield pool at the start of a fight
			double ShieldRegenPerSecond = 0.0; // after ShieldRegenDelay without damage
			double ShieldRegenDelay = 4.0;
			double MaxShield = 0.0;
		};

		// How fast an operator's Infection pays off (the owner's Infection page: goo after 45 s, explode at 75 s - the read of the
		// first goo tick is still OPEN). Per kit since 2026-10-09 so Alchemist, the infection specialist, can be tuned without Arita.
		struct FInfectionTuning
		{
			double GooStartsAt = 45.0;
			double GooInterval = 5.0;
			double GooDamage = 500.0;
			double ExplodeAt = 75.0;
			double ExplodeDamage = 2000.0;
		};

		struct FKitDef
		{
			int KitId = 0;             // BIB::EKit value
			std::string Name;
			int Stars = 3;
			double BaseHP = 1.0;       // PLACEHOLDER / PROPOSED unless noted
			double BaseAttack = 1.0;
			double WalkMult = 1.0;
			double RunMult = 1.0;
			double JumpMult = 1.0;
			bool bCanSprint = true;
			double DodgeCooldown = 1.0;
			double DodgeDistanceMult = 1.0;
			double ParryWindow = 0.3;
			std::string ResourceName[2];
			double ResourceMax[2] = {0.0, 0.0};
			double ResourceStart[2] = {0.0, 0.0};
			double ResourceRegen[2] = {0.0, 0.0};   // per second (negative = decay)
			FKitTraits Traits;
			FAbilityDef Abilities[SlotCount];
			std::vector<FAbilityDef> UltimateVariants; // alternative ultimates; selection 0 = Abilities[Ultimate], i = UltimateVariants[i-1]
			std::vector<FPassiveDef> Passives;
			float Colour[3] = {0.6f, 0.6f, 0.6f};
			std::string Playstyle;     // one-line summary
			bool bSpecialOnR = false;  // owner wrote "R" for the Special (Arita)
			FInfectionTuning InfectionTuning;   // only kits with the Infection move use it
		};

		// ------------------------------------------------------------------ registry
		const FKitDef* FindKit(int KitId);              // nullptr when the kit has no data (Luna/Judge/Narrator are bespoke)
		const FKitDef* FindKitByName(const char* Name);
		const std::vector<const FKitDef*>& AllKits();   // the 17 data-driven kits
		const char* CueName(int CueId);
		int CueTotal();
		// Every gameplay tag the kits, statuses and buffs use (for registration and the generated tag list).
		std::vector<std::string> AllGameplayTags();

		// Enemy abilities Adapt can copy (PLACEHOLDER enemy attacks).
		struct FEnemyAbility
		{
			int Id;
			const char* Name;
			double Damage;      // per hit (scaled by the enemy's own damage)
			double Cooldown;
			double Radius;      // AoE radius (0 = single target melee)
		};
		const std::vector<FEnemyAbility>& EnemyAbilities();
	}
}
