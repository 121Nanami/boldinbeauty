// Boldinbeauty demo — the angels' tactical brain (owner request 2026-10-13, goal 2: "every Angel AI should fight at or above a
// skilled player's level while staying fair. They should play smart and must not cheat"; owner: "make the angels overall a bit
// harder to face"). Engine-independent and tested (Tests/angel_brain_tests.cpp); the Unreal layer is BIBGame/BIBEnemy.cpp.
//
// Layers (each angel thinks every frame; nothing here reads a key press):
//   * Perception with a reaction delay: the commander is recorded every frame (FPlayerView); an angel decides on the snapshot
//     from ReactionSeconds ago (FPlayerTimeline), only when it sees the commander (line of sight, range) or remembers where it was.
//   * Habits (FHabits): the commander's last N actions - dodge side, favourite opener, basic-attack spam, dodging before a strike
//     lands - on top of the shared FMind (BIBAngelMind.h), which still learns the long-term counters.
//   * The crowd (FCrowd): attack tokens taken in turns (an angel that just attacked waits), flankers spread on a ring around the
//     commander (behind and to the sides once the angels adapt), ranged shots staggered, punish windows may borrow a token.
//   * Think(): defence (sidestep / backstep / guard a telegraphed heavy, skill-scaled, never certain), self-preservation (retreat
//     and regroup when hurt or close to BREAK), punishing (whiffed recoveries, heals, ultimate and long-cast wind-ups), spacing
//     (hold the role's range, back off when outranged, close in on a recovering commander), then the best attack by score -
//     distance, angle, cooldowns, the commander's state, habits, variety - with feints and varied combo enders.
// Fairness: decisions use the delayed snapshot only; every attack keeps a telegraph of at least MinTelegraph; ranged shots fly
// and can be sidestepped; the assassin's lunge is telegraphed. Difficulty knobs per tier are data: Config/DefaultGame.ini
// [BIB.AngelAI] (AiConfigKeys lists them). Every number is PROPOSED.
#pragma once

#include "BIBAngelMind.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace BIB
{
	namespace Angel
	{
		// ------------------------------------------------------------------ small 2D math (cm, top-down)
		struct FV2
		{
			double X = 0.0;
			double Y = 0.0;
		};
		inline FV2 operator+(const FV2& A, const FV2& B) { return {A.X + B.X, A.Y + B.Y}; }
		inline FV2 operator-(const FV2& A, const FV2& B) { return {A.X - B.X, A.Y - B.Y}; }
		inline FV2 operator*(const FV2& A, double S) { return {A.X * S, A.Y * S}; }
		double Length(const FV2& A);
		double Dot(const FV2& A, const FV2& B);
		FV2 Normal(const FV2& A);             // unit vector (zero stays zero)
		FV2 Perp(const FV2& A);               // rotated 90 degrees to the left
		FV2 Rotate(const FV2& A, double Radians);

		// ------------------------------------------------------------------ difficulty knobs
		enum class ETier : int
		{
			Regular = 0,   // angel, heavy, ranged, mage, assassin
			Elite,         // mini boss
			Boss,          // chapter boss
			Count
		};
		constexpr int TierCount = static_cast<int>(ETier::Count);

		struct FKnobs
		{
			double ReactionSeconds = 0.3;    // how old the snapshot an angel decides on is (chapter 1, before Difficulty)
			double Aggression = 0.5;         // 0..1: token priority, approach speed, attack cadence, combo length
			double DodgeChance = 0.25;       // chance to evade a telegraphed heavy aimed at it (scaled by skill)
			double BlockChance = 0.25;       // chance to guard light swings (melee roles)
			double AdaptRate = 1.0;          // multiplies the mind's learning speed and how much habits steer choices
			double PunishChance = 0.5;       // chance to jump on an opening (whiff, heal, ultimate / long-cast wind-up)
			double FeintChance = 0.25;       // base chance to feint or delay an ender
			double TurnRateDeg = 360.0;      // degrees per second the angel can turn
			double MinTelegraph = 0.35;      // no attack warns for less than this (seconds; fairness - never scaled down)
			double DodgeCooldown = 2.5;      // seconds between evasions
			double RetreatHealth = 0.25;     // health share under which it retreats / regroups / changes tactics
			double SightRange = 3000.0;      // cm
			double MemorySeconds = 4.0;      // how long it keeps hunting the last known position after losing sight
			double AttackIntervalMult = 1.0; // x the archetype's attack interval
		};

		struct FAiTuning
		{
			FKnobs Tier[TierCount];
			double Difficulty = 1.15;          // owner 2026-10-13 "a bit harder": reaction / interval divided, chances multiplied
			double ReactionPerChapter = 0.008; // reaction gets this much faster per chapter after the first ...
			double ReactionFloor = 0.15;       // ... never below this (fairness)
			double ExtraAttackers = 1.0;       // melee tokens on top of BIBAngelMind.h MaxAttackers
			double TokenTurnSeconds = 0.9;     // an angel waits this long after its attack before taking a token again (/ aggression)
			double FlankFrontCone = 60.0;      // degrees either side of the commander's facing that adapted flankers avoid
			double HoldExtra = 230.0;          // cm beyond its reach that a waiting melee angel holds
			double OutrangedMargin = 70.0;     // cm outside the commander's reach that a waiting angel backs off to
			double MaxShooters = 2.0;          // ranged / mage angels firing at once
			double ShotStagger = 0.35;         // seconds between two shots starting
			double ShotSpeed = 2600.0;         // cm/s: ranged shots fly and can be sidestepped
			double ShotLaneHalfWidth = 75.0;   // cm either side of the shot's line
			double ShotLead = 0.6;             // share of the commander's (delayed) velocity a shot leads by
			double DodgeSideOffset = 150.0;    // cm a shot / blast is offset toward the side the commander usually dodges to
			double SidestepDistance = 330.0;   // cm
			double RegroupSeconds = 2.5;
			double RegroupCooldown = 9.0;
			double StrainCaution = 0.75;       // Strain share at which it plays safe
			double FeintExtraDelay = 0.35;     // seconds a feint holds after its first flash
			double PunishReach = 520.0;        // cm beyond its reach a melee angel will dash in to punish
			double HabitWindow = 12.0;         // actions remembered
		};

		FAiTuning& AiTuning();
		void ResetAiTuning();

		struct FAiConfigKey
		{
			std::string Name;   // "Regular.ReactionSeconds", "Difficulty", "ShotSpeed" ...
			double* Value;
		};
		std::vector<FAiConfigKey> AiConfigKeys(FAiTuning& Target);
		const char* TierName(ETier Tier);
		ETier TierFor(ERole Role);

		/** The knobs an angel of this tier fights with in this chapter (Difficulty and the chapter applied). */
		FKnobs EffectiveKnobs(ETier Tier, int Chapter);

		// ------------------------------------------------------------------ what an angel can perceive of the commander
		struct FPlayerView
		{
			double Time = -100.0;            // world time of the snapshot
			FV2 Pos;
			FV2 Vel;
			FV2 Facing{1.0, 0.0};
			double HealthFraction = 1.0;
			bool bAttacking = false;         // winding up or swinging anything
			bool bHeavyWindup = false;       // winding up a heavy (finisher, ability, Special, ultimate) - telegraphed
			bool bRecovery = false;          // in the recovery after an attack
			bool bWhiffed = false;           // ... and it hit nothing
			bool bLongCast = false;          // a long cast / charge (Expel, Close enough, court, ultimate beat)
			bool bDodging = false;
			bool bBlocking = false;
			bool bAirborne = false;
			double ThreatReach = 300.0;      // cm the commander's current attack reaches
			double ActionStartedAt = -100.0; // when the current attack / cast began (one reaction per action)
			double SecondsSinceSwitch = 100.0;
			int Operator = 0;
		};

		class FPlayerTimeline
		{
		public:
			void Record(const FPlayerView& View);
			/** The newest snapshot no newer than Time (the oldest kept one if Time is older than all). */
			FPlayerView At(double Time) const;
			bool Empty() const { return Views.empty(); }
			/** True when the health rose by more than 2% within the last Window seconds before Time (a heal). */
			bool Healing(double Time, double Window = 0.6) const;
			void Clear() { Views.clear(); }

		private:
			std::vector<FPlayerView> Views;  // oldest first, ~2.5 s kept
		};

		// ------------------------------------------------------------------ habits over the last N actions
		enum class EAction : int
		{
			Basic = 0,
			Heavy,
			Cast,
			Dodge,
			Block,
			Jump,
			Switch,
			Count
		};
		enum class ESide : int
		{
			Left = 0,
			Right,
			Back,
			Forward,
			Count
		};

		class FHabits
		{
		public:
			/** Derives the commander's actions from two consecutive snapshots (what anyone watching would see). */
			void Observe(const FPlayerView& Before, const FPlayerView& After);
			/** An angel started a telegraph that lands at HitAt (for "dodges too early"). */
			void NoteTelegraph(double StartedAt, double HitAt);
			void Push(EAction Action, ESide Side = ESide::Count, double Time = 0.0);
			void Reset();

			int Count(EAction Action) const;
			/** The side the commander dodges to most (relative to the angel that threatened them, else to their facing), and how sure (0..1). */
			ESide FavouriteDodge(double& Confidence) const;
			/** The action that most often starts an exchange (the first after 2 s of calm), and how sure. */
			EAction FavouriteOpener(double& Confidence) const;
			/** 0..1: how much of the window is basic attacks in a row. */
			double SpamLevel() const;
			/** 0..1: share of dodges that came before the strike they were dodging was due (bait with delays). */
			double EarlyDodgeRate() const;
			int Size() const { return static_cast<int>(History.size()); }

		private:
			struct FEntry
			{
				EAction Action;
				ESide Side;
				double Time;
				bool bOpener;
			};
			std::vector<FEntry> History;
			struct FTelegraph
			{
				double StartedAt;
				double HitAt;
			};
			std::vector<FTelegraph> Telegraphs;
			int EarlyDodges = 0;
			int TimedDodges = 0;
			double LastActionAt = -100.0;
		};

		// ------------------------------------------------------------------ the crowd: tokens, rings, ranged turns
		struct FCrowdAgent
		{
			int Id = 0;
			ERole Role = ERole::Melee;
			FV2 Pos;
			double Reach = 150.0;
			bool bCommitted = false;   // winding up / swinging now
			bool bSees = true;
		};

		struct FCrowdSlot
		{
			bool bToken = false;       // may attack now
			bool bMayShoot = false;    // ranged / mage: may start a shot now
			FV2 HoldPoint;             // where to wait
			bool bFlanker = false;     // holding the side / back of the commander
		};

		class FCrowd
		{
		public:
			/** Re-plans tokens and positions for this frame. */
			void Update(double Now, const FPlayerView& Commander, const std::vector<FCrowdAgent>& Agents, double Adaptation, double Aggression);
			const FCrowdSlot& Slot(int Id) const;
			/** An angel finished an attack: its token goes back and it waits its turn. */
			void Release(int Id, double Now, double Aggression);
			/** A punish may borrow one token over the cap. */
			bool BorrowToken(int Id, double Now);
			void NoteShot(int Id, double Now);
			void Reset();
			int TokensHeld() const;

		private:
			struct FMemory
			{
				bool bToken = false;
				double FreeAt = 0.0;
				double LastAttackAt = -100.0;
				double TokenSince = -100.0;
				bool bBorrowed = false;
				double LastSeen = 0.0;
			};
			std::map<int, FMemory> Memory;
			std::map<int, FCrowdSlot> Slots;
			FCrowdSlot Empty;
			double LastShotAt = -100.0;
			int Shooting = 0;
		};

		// ------------------------------------------------------------------ one angel's decision
		enum class EIntent : int
		{
			Hold = 0,     // wait at Goal (facing the commander)
			Approach,     // move to Goal
			Strafe,       // circle at range (StrafeSign)
			Retreat,      // move to Goal fast (regroup)
			Reposition,   // ranged: find a line of sight (StrafeSign) toward Goal
			Sidestep,     // quick evasion along Goal (a direction)
			Backstep,     // quick evasion away
			Guard,        // raise the guard
			Attack,       // start Move
			Count
		};

		enum class EEnder : int
		{
			Quick = 0,    // short windup (still >= MinTelegraph)
			Delayed,      // held, catches early dodges
			Sweep,        // wide, catches a sidestep
			Unblockable,  // rose flash, ignores block and parry
			Count
		};

		enum class EBehavior : int
		{
			Spacing = 0,
			Punish,
			Defense,
			Feint,
			Adapt,
			Coordination,
			SelfPreservation,
			Kit,
			Count
		};
		constexpr int BehaviorCount = static_cast<int>(EBehavior::Count);
		const char* BehaviorName(EBehavior Behavior);
		const char* IntentName(EIntent Intent);

		struct FAction
		{
			EIntent Intent = EIntent::Hold;
			FV2 Goal;                       // a point (Hold / Approach / Retreat / Reposition) or a direction (Sidestep / Backstep)
			double StrafeSign = 1.0;
			double Speed = 1.0;             // x walking speed
			EMove Move = EMove::Strike;
			double Windup = 0.5;            // seconds (already >= MinTelegraph)
			double DamageMult = 1.0;
			bool bUnblockable = false;
			double FeintDelay = 0.0;        // a feint: the real strike comes this long after the first flash
			int FollowUps = 0;              // combo strikes after the first
			EEnder Enders[3] = {EEnder::Quick, EEnder::Quick, EEnder::Quick};
			FV2 AimOffset;                  // ranged / mage: offset toward the predicted dodge side
			bool bBorrowToken = false;      // a punish took a token over the cap
			bool bSuperArmor = false;       // charge / slam: light hits do not interrupt it
			EBehavior Tags[4] = {EBehavior::Count, EBehavior::Count, EBehavior::Count, EBehavior::Count};
			void Tag(EBehavior Behavior);
			bool Has(EBehavior Behavior) const;
		};

		// Small deterministic random source (tests reproduce exactly; the game seeds one per angel).
		struct FRandom
		{
			uint32_t State = 0x9E3779B9u;
			double Next();   // 0..1
		};

		struct FBrainState
		{
			double MoveReadyAt[static_cast<int>(EMove::Count)] = {};
			EMove Recent[3] = {EMove::Count, EMove::Count, EMove::Count};
			double DodgeReadyAt = 0.0;
			double HandledThreat = -100.0;   // ActionStartedAt of the last threat it rolled against
			double HandledOpening = -100.0;  // ActionStartedAt of the last opening it rolled against
			double RetreatUntil = -1.0;
			FV2 RetreatGoal;
			double RegroupReadyAt = 0.0;
			double AttackReadyAt = 0.0;       // the archetype's interval between attacks
			double LastSeenAt = -100.0;
			FV2 LastKnown;
			double PunishUntil = -1.0;        // dashing in on an opening until then
			double StrafeSign = 1.0;
			double NextStrafeFlip = 0.0;
			double HandledHealAt = -100.0;    // snapshot time of the last heal it rolled against
			EIntent LastIntent = EIntent::Count;   // stats count a continuous intent once, when it starts
			FRandom Random;
		};

		struct FThink
		{
			int Id = 0;                      // crowd id
			double Now = 0.0;
			double Interval = 1.6;           // the archetype's seconds between attacks
			ERole Role = ERole::Melee;
			ETier Tier = ETier::Regular;
			FKnobs Knobs;
			FV2 Pos;
			FV2 Facing{1.0, 0.0};
			double Reach = 150.0;            // its basic attack reach (cm)
			double KeepMin = 0.0;            // ranged / mage band
			double KeepMax = 0.0;
			double BaseWindup = 0.5;         // the archetype's telegraph
			double HealthFraction = 1.0;
			double StrainFraction = 0.0;
			bool bPhaseTwo = false;
			bool bSees = true;               // line of sight to the commander right now
			bool bLineBlockedByAlly = false; // ranged: an ally stands in the line
			FV2 AllyCentre;                  // where its group is (regroup point)
			int NearbyAllies = 0;
			FPlayerView View;                // the delayed snapshot (ReactionSeconds old)
			FV2 CommanderNow;                // where the commander is now (only used while it is seen)
			bool bHealing = false;           // FPlayerTimeline::Healing on the delayed snapshot
		};

		/** One decision. Mind / Habits / Crowd are the shared knowledge; State is this angel's memory. */
		FAction Think(const FThink& In, const FMind& Mind, const FHabits& Habits, FCrowd& Crowd, FBrainState& State);

		/** Seconds before an attack of this move may be chosen again. */
		double MoveCooldown(EMove Move);

		// ------------------------------------------------------------------ what the angels did (Tools/verify_angels.py reads it)
		struct FAiStats
		{
			int Behaviors[7][BehaviorCount] = {};   // [role][behaviour]
			int Moves[7][static_cast<int>(EMove::Count)] = {};
			int Evasions = 0;
			int Punishes = 0;
		};
		FAiStats& AiStats();
		void ResetAiStats();
		void CountAction(ERole Role, const FAction& Action);
	}
}
