// Boldinbeauty demo — operator kit runtime (engine-independent).
//
// FRuntime interprets an FKitDef against an IWorld: casting, costs, timelines, zones, projectiles, buffs, passives,
// reactions and the named hook mechanics. The Unreal layer owns one FRuntime per team member and implements IWorld on
// top of the Gameplay Ability System: cooldowns are GameplayEffects, health/shield/ultimate are attributes, damage and
// healing go through GameplayEffects, statuses and states are mirrored as gameplay tags. The unit tests implement
// IWorld with a fake battlefield, which is how every kit is checked without the editor.
#pragma once

#include "BIBKitStatus.h"
#include "BIBKitTypes.h"

#include <algorithm>
#include <map>
#include <set>

namespace BIB
{
	namespace Kits
	{
		struct FEnemyView
		{
			int Id = -1;
			FVec Pos;
			double Radius = 40.0;
			double HP = 1.0;
			double MaxHP = 1.0;
			bool bBoss = false;
		};

		enum EDamageFlag : unsigned
		{
			DmgNone = 0,
			DmgTrue = 1u << 0,     // ignores the enemy's damage-taken modifiers
			DmgDot = 1u << 1,      // damage over time (does not interrupt)
			DmgBasic = 1u << 2,    // from the regular attack
			DmgExecute = 1u << 3,
			DmgReflect = 1u << 4   // returned damage (does not feed the ultimate)
		};

		// Where a hit comes from, so the game can give it the right weight (BIBHitFeel.h: hit stop, reaction, camera ...).
		struct FHitInfo
		{
			ESlotId Slot = ESlotId::Count;   // Count = not from a cast (passive, status, reflect, a benched operator's effect)
			bool bTick = false;              // a zone / channel tick
			bool bFinal = false;             // the cast's last damaging hit; basics: the last hit of the kit's own string
			int ComboIndex = 0;              // basics: this hit's place in the kit's string ...
			int ComboLength = 1;             // ... and the string's length
			int FeelWeight = -1;             // FAbilityDef::FeelWeight (BIB::Feel::EWeight), -1 = classify
		};

		// Everything the runtime needs from the game. All distances are centimetres, times seconds.
		class IWorld
		{
		public:
			virtual ~IWorld() = default;

			virtual double Now() const = 0;
			virtual FVec CasterPos() const = 0;
			virtual FVec CasterAim() const = 0;                 // unit vector the caster faces / aims along
			virtual double CasterAttack() const = 0;            // regular attack damage (level and equipment included)
			virtual double CasterDamageMult() const = 0;        // level / equipment multiplier for ability damage
			virtual double CasterHP() const = 0;
			virtual double CasterMaxHP() const = 0;
			virtual void CasterMove(const FVec& To, double Seconds) = 0;   // Seconds == 0: teleport
			virtual void CasterInvulnerable(double Seconds) = 0;
			virtual void CasterHeal(double Amount) = 0;
			virtual void CasterSelfDamage(double Amount) = 0;
			virtual void CasterLock(double Seconds) = 0;        // movement locked
			virtual void CasterHealthScale(double Mult, bool bFill) = 0;

			virtual void GetEnemies(std::vector<FEnemyView>& Out) const = 0;
			virtual bool EnemyAlive(int Id) const = 0;
			virtual double DamageEnemy(int Id, double Amount, unsigned Flags) = 0;  // returns damage dealt
			/** DamageEnemy with the hit's origin (the game uses it for hit feel); worlds that do not care keep the default. */
			virtual double DamageEnemyHit(int Id, double Amount, unsigned Flags, const FHitInfo& Info)
			{
				(void)Info;
				return DamageEnemy(Id, Amount, Flags);
			}
			virtual void KillEnemy(int Id, bool bRespectBoss, double BossFraction) = 0;
			/** KillEnemy with the hit's origin (executes are hits too: hit stop, reaction, camera). */
			virtual void KillEnemyHit(int Id, bool bRespectBoss, double BossFraction, const FHitInfo& Info)
			{
				(void)Info;
				KillEnemy(Id, bRespectBoss, BossFraction);
			}
			virtual void Impulse(int Id, const FVec& Velocity) = 0;
			virtual bool ApplyStatus(int Id, EStatus Status, double Seconds, double Power, int Stacks) = 0;
			virtual int StatusStacks(int Id, EStatus Status) const = 0;
			virtual void ClearStatus(int Id, EStatus Status) = 0;
			virtual void RemoveStatusStacks(int Id, EStatus Status, int Count) = 0;

			virtual double CooldownRemaining(ESlotId Slot) const = 0;
			virtual void StartCooldown(ESlotId Slot, double Seconds) = 0;
			virtual void ReduceCooldown(ESlotId Slot, double Seconds) = 0;

			virtual void AddUltimate(double DamageDealt) = 0;
			virtual void Cue(int CueId, const FVec& Pos, double Radius, double Seconds) = 0;
			virtual void Message(const std::string& Text) = 0;
			virtual double Random01() = 0;
		};

		struct FCastResult
		{
			bool bOk = false;
			std::string Reason;
		};

		/** True for a step that deals damage when it runs (used for hit feel and for cancelling a cast's recovery). */
		bool StepDealsDamage(const FStep& Step);

		// Tunables for the owner's Alchemist / Arita infection rules (SOURCE unless marked).
		namespace Infection
		{
			constexpr double SpreadPeriod = 10.0;     // every 10 seconds ...
			constexpr double SpreadWindow = 5.0;      // ... a 5 second period starts
			// goo start / interval / damage and the explosion's time and damage are per kit: FKitDef::InfectionTuning (BIBKitTypes.h)
			constexpr double ExplodeRadius = 1000.0;  // PLACEHOLDER ("large area")
			constexpr double MutationDamageBonus = 1.5; // infection based attacks do 50% more damage
			constexpr double MutateExplosionDamage = 5000.0; // uninfected enemy hit by Mutation
			constexpr double MutateExplosionRadius = 350.0;  // PLACEHOLDER (WORKING: the explosion also hurts neighbours)
			constexpr double ContactPad = 45.0;       // PLACEHOLDER touching distance beyond the two radii
			constexpr double IllusionSeconds = 5.0;
		}

		class FRuntime
		{
		public:
			FRuntime(const FKitDef& InKit, IWorld& InWorld);
			// While the operator is swapped out: only the effects she left on enemies keep running (infection, mutation).
			void TickBackground(double Dt);
			~FRuntime();

			const FKitDef& Kit() const { return *KitPtr; }

			// ---- fight lifecycle
			void ResetForFight();
			void OnSwitchIn();
			void SetTeammatePassives(const std::vector<const FPassiveDef*>& Passives);

			// ---- casting
			FCastResult CanCast(ESlotId Slot) const;
			FCastResult Cast(ESlotId Slot);
			void Release(ESlotId Slot);
			const FAbilityDef& Ability(ESlotId Slot) const;  // honours the selected ultimate variant
			void SelectUltimateVariant(int Index);
			int UltimateVariant() const { return UltimateVariantIndex; }
			bool IsCasting() const;                    // a timeline with a movement lock is running
			bool IsCasting(ESlotId Slot) const;
			bool MovementLocked() const;
			/** True when every cast that locks the caster has dealt its last hit at least Grace seconds ago (its recovery). */
			bool InRecovery(double Grace) const;
			/** Ends the movement lock of casts in recovery (a dodge cancels the recovery); their remaining effects still run. */
			void CancelRecovery();
			/** The basic attack's place in the kit's own string after the latest cast, and the string's length. */
			int BasicComboIndex() const { return ComboIndex; }
			int BasicComboLength() const { return std::max(1, Ability(ESlotId::Basic).ComboLength); }
			void CancelAll();                          // interrupts timelines (zones and projectiles keep going)
			// zones, projectiles, buffs, states (stage end / switch out). A switch-out keeps the infections she left on enemies
			// (2026-10-09: Alchemist infects, mutates and swaps out; TickBackground keeps them running).
			void ClearBattlefield(bool bKeepInfection = false);
			void Tick(double Dt);

			// ---- callbacks from the host
			// Returns the damage that reaches health after evasion, reactions, modifiers, reflect and shield.
			double OnIncomingDamage(double Amount, int AttackerId, bool bMelee);
			void NotifyDodge();                        // the host dodged a hit (i-frames)
			void NotifyParry();
			void NotifyEnemyKilled(int EnemyId);   // dedupes kills the runtime already counted
			void NotifyEnemyAbilityHit(int EnemyAbilityId, double EnemyDamage);   // Adapt copies it
			// Damage the host dealt outside the runtime (e.g. debug hooks): feeds passives and echo.
			void NotifyDamageDealt(int EnemyId, double Amount, bool bBasic);

			// ---- state for the host (mirrored into attributes and tags)
			double Resource(int Index) const { return Res[Index]; }
			double ResourceMax(int Index) const { return Kit().ResourceMax[Index]; }
			double Shield() const { return ShieldAmount; }
			double Mod(EBuff Buff) const;              // total additive deviation (timed + passive + innate + adaptation)
			double DamageDealtMult() const;            // 1 + Mod(DamageDealt)
			double DamageTakenMult() const;
			double MoveSpeedMult() const;
			double AttackSpeedMult() const;
			double HealthScale() const { return HealthScaleValue; }
			bool Invulnerable() const;
			std::string KitLine() const;               // one-line HUD resource/state text
			std::string AbilityNote(ESlotId Slot) const; // extra HUD note for a slot ("needs 20 blood", "act: Melee")
			bool HasBuffActive(EBuff Buff) const;
			int ActiveZoneCount() const { return static_cast<int>(Zones.size()); }
			int ActiveProjectileCount() const { return static_cast<int>(Projectiles.size()); }
			bool HasState(const std::string& Name) const;   // "Curse", "Takeover", "Recording", "ZeroHour" ...
			std::vector<std::string> ActiveStateTags() const;

			// ---- inspection (tests and HUD)
			int ActIndex() const { return Act; }
			const char* ActName() const;
			const char* FrontName() const;
			int AdaptedCount() const { return static_cast<int>(Adapted.size()); }
			int InfectedCount() const { return static_cast<int>(Infected.size()); }
			bool IsInfected(int EnemyId) const { return Infected.count(EnemyId) != 0; }
			bool IsMutated(int EnemyId) const;
			double LastCastDamage() const { return LastDamage; }
			double TotalDamageDealt() const { return TotalDealt; }
			int LastGamblerLuck() const { return static_cast<int>(Luck * 100.0); }
			int KillsThisFight() const { return KillCount; }
			int AdaptedSelected() const { return AdaptSelected; }
			std::string AdaptedName(int Index) const;
			double AdaptStat(int Index) const { return AdaptStats[Index]; }
			void ForceRandom(double Value) { bForcedRandom = true; ForcedRandom = Value; }
			void ClearForcedRandom() { bForcedRandom = false; }

		private:
			struct FScheduled
			{
				FStep Step;
				double At = 0.0;        // absolute time
				double EndAt = 0.0;
				double NextTick = 0.0;
				bool bStarted = false;
				bool bDone = false;
			};

			struct FTimeline
			{
				const FAbilityDef* Def = nullptr;
				ESlotId Slot = ESlotId::Move1;
				double Start = 0.0;
				double Held = 0.0;
				bool bHolding = false;
				bool bReleased = false;
				bool bFree = false;             // Encore replays cost nothing
				double DamageTakenStart = 0.0;
				std::vector<FScheduled> Steps;
				double EndsAt = 0.0;
				double LockEndsAt = 0.0;        // the cast keeps the caster busy until then
				double LastHitAt = -1.0;        // absolute time of the last damaging step (-1 = the cast deals no damage)
			};

			struct FZone
			{
				FStep Step;
				FVec Centre;
				double EndsAt = 0.0;
				double NextTick = 0.0;
				bool bFollow = false;
				bool bUlt = false;
				FHitInfo Hit;                   // the cast that placed it
			};

			struct FProjectile
			{
				FStep Step;
				FVec Pos;
				FVec Dir;
				double Travelled = 0.0;
				int PierceLeft = 1;
				std::vector<int> Hit;
				bool bUlt = false;
				FHitInfo Source;                // the cast that fired it
			};

			struct FBuff
			{
				EBuff Stat = EBuff::DamageDealt;
				double Magnitude = 0.0;
				double ExpiresAt = 0.0;
			};

			struct FPassiveState
			{
				const FPassiveDef* Def = nullptr;
				double Timer = 0.0;
				double IcdUntil = 0.0;
				int Counter = 0;
				bool bBelowLow = false;
			};

			struct FReactionState
			{
				const FReaction* Def = nullptr;
				double ExpiresAt = 0.0;
				int TriggersLeft = 0;
			};

			struct FInfect
			{
				double Age = 0.0;
				double GooTimer = 0.0;
				bool bMutated = false;
			};

			struct FCopied
			{
				int EnemyAbilityId = 0;
				double Damage = 0.0;
				double Cooldown = 0.0;
				double Radius = 0.0;
				std::string Name;
			};

			struct FFlower
			{
				FVec Pos;
				int Type = 0;            // 0 thorns, 1 poison, 2 pheromone
				double EndsAt = 0.0;
				double NextTrigger = 0.0;
			};

			struct FEcho
			{
				int Id = -1;
				double Amount = 0.0;
			};

			struct FContext
			{
				int TriggerEnemy = -1;
				double Held = 0.0;
				double Amount = 0.0;
				bool bBasic = false;
				bool bUlt = false;     // running inside an ultimate: its damage does not recharge the ultimate
			};

			const FKitDef* KitPtr;
			IWorld* World;
			int UltimateVariantIndex = 0;

			double Res[2] = {0.0, 0.0};
			double ShieldAmount = 0.0;
			double TimedShield = 0.0;          // the part of ShieldAmount that expires at ShieldExpiresAt
			double ShieldExpiresAt = 0.0;
			double LastDamagedAt = -100.0;
			double HealthScaleValue = 1.0;
			double LockedUntil = 0.0;
			double InvulnerableUntil = 0.0;
			double LastDamage = 0.0;
			double TotalDealt = 0.0;
			double Luck = 0.0;
			int ComboIndex = 0;
			double LastBasicAt = -100.0;
			std::vector<FTimeline> Timelines;
			std::vector<FTimeline> PendingTimelines;   // started while the timelines are being ticked
			bool bInTimelineTick = false;
			int PassiveDepth = 0;
			double AutoTimer[SlotCount] = {};
			FVec LastCasterPos;
			bool bHasLastPos = false;
			double TakeoverHealthScale = 1.0;
			std::vector<FZone> Zones;
			std::vector<FProjectile> Projectiles;
			std::vector<FBuff> Buffs;
			std::vector<FPassiveState> Passives;
			std::vector<FPassiveDef> CopiedPassives;     // Adapt: stable storage for copied teammate passives
			std::vector<FReactionState> Reactions;
			std::vector<const FAbilityDef*> RecentCasts;
			std::vector<const FPassiveDef*> TeammatePassives;
			double DamageTakenWhileCasting = 0.0;
			int StoredTarget = -1;
			double StoredValue = 0.0;
			double HitAmount = 0.0;            // damage that triggered the passive being run (FlagHitAmount)
			bool bStoringTaken = false;
			bool bSuppressUlt = false;         // true while ultimate damage is being dealt
			FHitInfo CurrentHit;               // origin of the hits being dealt right now (set around each timeline step)
			std::set<int> CountedKills;
			int KillCount = 0;
			FStep SeedStep;
			FVec SeedLastPos;

			// kit state
			int Act = 0;                       // Jester: index into the act order (rotates per cast)
			int ActCasts = 0;
			int Front = 0;                     // Weather: rotates per cast
			bool bCurse = false;
			int CurseLink[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
			int CurseLinkCount = 0;
			double TakeoverUntil = 0.0;
			double AdaptStats[3] = {1.0, 1.0, 1.0};   // damage, speed, health
			double AdaptUse[3] = {0.0, 0.0, 0.0};
			std::vector<FCopied> Adapted;
			int AdaptSelected = 0;
			double AdaptPassiveTimer = 0.0;
			double UltimateActiveUntil = 0.0;
			bool bRecording = false;
			std::vector<FEcho> EchoLog;
			double ZeroHourUntil = 0.0;
			std::map<int, FInfect> Infected;
			double InfectionClock = 0.0;
			std::vector<FFlower> Flowers;
			double SoilUntil = 0.0;
			int SoilToggle = 0;
			std::map<int, double> SoilTouched;
			int SeedTarget = -1;
			double SeedDetonateAt = 0.0;
			double SeedStoredDamage = 0.0;
			bool bForcedRandom = false;
			double ForcedRandom = 0.5;
			double Rand();
			double Now() const { return World->Now(); }

			// ---- helpers
			void BeginTimeline(const FAbilityDef& Def, ESlotId Slot, double Held, bool bFree, double StartTime);
			int ChooseBranch(const FAbilityDef& Def);
			void ExecuteStep(const FStep& Step, FTimeline* Timeline, const FContext& Ctx);
			void RunStepsNow(const std::vector<FStep>& Steps, const FContext& Ctx);
			void TickTimelines(double Dt);
			void TickZones(double Dt);
			void TickProjectiles(double Dt);
			void TickBuffs(double Dt);
			void TickPassives(double Dt);
			void TickKitState(double Dt);
			void TickInfection(double Dt, bool bOnField = true);
			void TickFlowers(double Dt);
			void TickReactions();
			void FirePassives(ETrigger Trigger, const FContext& Ctx, int Filter = -1);
			bool PassiveConditionMet(const FPassiveDef& Def) const;

			void CollectEnemies(std::vector<FEnemyView>& Out) const;
			bool FindNearest(const std::vector<FEnemyView>& All, const FVec& From, double MaxDist, unsigned Flags, FEnemyView& Out) const;
			bool PassesFilter(const FEnemyView& Enemy, unsigned Flags) const;
			void ResolveTargets(const FStep& Step, const FContext& Ctx, std::vector<FEnemyView>& Targets, FVec& Centre, FVec& Facing);
			void HitEnemy(const FStep& Step, const FEnemyView& Enemy, const FVec& Centre, bool bBasic, double DamageScale = 1.0);
			double StepDamage(const FStep& Step, bool bBasic) const;
			void RecordDamage(int EnemyId, double Dealt, bool bBasic);
			void HandleKill(int EnemyId);
			bool HasPassive(const char* PassiveId) const;
			bool IsKit(const char* Name) const { return KitPtr->Name == Name; }

			void AddBuff(EBuff Stat, double Magnitude, double Seconds);
			void AddShield(double Amount, double Seconds);
			void SpendResource(int Index, double Amount);
			void GainResource(int Index, double Amount);
			void SelfCost(double Fraction);
			double CooldownFor(const FAbilityDef& Def) const;

			// hooks
			void RunHook(const FStep& Step, FTimeline* Timeline, const FContext& Ctx);
			void InfectEnemy(int Id);
			void MutateEnemy(int Id);
			void SpawnFlowers(double Held);
			void ToggleCurse();
			void StartTakeover(double Seconds);
			void EndTakeover();
			void ApplyHealthScale(bool bFill);
			void AdaptCast();
			void AdaptSwitchNext();
			void AdaptCopyPassive();
			void StartEncore();
			void ReleaseEcho(double Multiplier, double MinimumPerEnemy);
			void DetonateSeed();
			bool IsBasicBoostedByTakeover() const { return TakeoverUntil > Now(); }
		};
	}
}
