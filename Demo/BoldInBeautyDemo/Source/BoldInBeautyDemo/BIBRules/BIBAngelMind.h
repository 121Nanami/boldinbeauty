// Boldinbeauty demo — the angels' adaptive mind (owner request 2026-10-09, Notion "Dev notes": "enemies adapt in real time to
// counter your playstyle and there's no set kit for angels they just adapt to your playstyle and start using moves against
// you"; angels "are meant to be incredibly smart beings"). Engine-independent; the Unreal layer is BIBGame/BIBEnemy.cpp.
//
// How it works:
//   * One shared mind (the angels learn together) watches the commander: basic-attack spam, ability and area use, ranged poking,
//     dodging, parrying, blocking, kiting, jumping and switching. Each is a decaying rate (events per minute).
//   * Every counter-move has a "pressure" from the traits it answers. Pressure teaches the move (Expertise 0..1); moves the
//     player stops provoking are slowly forgotten. A move counts as known from KnownAt, and grows stronger up to 1.
//   * Angels have no fixed kit beyond their basic role (melee / brute / ranged / mage / assassin / mini boss / boss). Each
//     decision picks from what the mind knows, weighted by expertise, role and the situation (distance, the player in the air,
//     blocking, attacking...). Copied moves ("Mirror") are the player's own habits turned against them.
//   * Mini bosses and bosses learn faster; the mind persists across the stages of one play session (decaying between them).
// Every number here is PLACEHOLDER.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BIB
{
	namespace Angel
	{
		enum class ETrait : int
		{
			MeleeSpam = 0,   // basic attacks
			AbilityUse,      // moves, specials, ultimates
			AreaUse,         // hits that caught 3+ angels at once
			RangedPoke,      // damage dealt from far away
			Dodging,
			Parrying,
			Blocking,
			Kiting,          // backing away from approaching angels
			Jumping,
			Switching,
			Count
		};
		constexpr int TraitCount = static_cast<int>(ETrait::Count);

		enum class ECounter : int
		{
			Guard = 0,       // vs basic-attack spam: raise a guard, absorb front hits, riposte
			Feint,           // vs dodging / parrying: delayed or doubled strikes
			Unblockable,     // vs blocking / parrying: rose-flashing strikes that ignore block and parry
			Blink,           // vs kiting / poking: gap-closing dash behind the commander
			Scatter,         // vs area attacks: spread out, never bunch up
			MirrorNova,      // copied area burst
			MirrorVolley,    // copied ranged volley
			MirrorDash,      // copied dash strike
			AntiAir,         // vs jumping: shots that punish airtime
			Ambush,          // vs switching: focus the operator who just came in
			Count
		};
		constexpr int CounterCount = static_cast<int>(ECounter::Count);

		enum class ERole : int
		{
			Melee = 0,
			Brute,
			Ranged,
			Mage,
			Assassin,
			MiniBoss,
			Boss
		};

		enum class EMove : int
		{
			Strike = 0,      // the role's basic attack (melee hit, shot or blast)
			Guard,
			Feint,
			Unblockable,
			Blink,
			Nova,
			Volley,
			DashStrike,
			AntiAir,
			Flank,           // reposition to the commander's side / back instead of attacking now
			// 2026-10-13 (goal 2, BIBAngelBrain.h): every role's own kit
			Combo,           // 2-3 strikes with a varied ender (quick / delayed / sweep / unblockable)
			Sweep,           // wide, slower strike that catches a sidestep
			Charge,          // brute: telegraphed armoured rush
			Lunge,           // assassin: telegraphed gap-closing strike (replaces the old unwarned dash)
			Count
		};

		// ------------------------------------------------------------------ tuning (PLACEHOLDER)
		constexpr double RateHalfLife = 25.0;       // seconds: how fast a habit is forgotten
		constexpr double LearnPerSecond = 0.03;     // expertise per second at pressure 1
		constexpr double ForgetPerSecond = 0.004;
		constexpr double KnownAt = 0.3;
		constexpr double BossLearnMult = 1.8;
		constexpr double StageCarryOver = 0.6;      // expertise kept when a new stage starts

		const char* TraitName(ETrait Trait);
		const char* CounterName(ECounter Counter);
		const char* CounterBlurb(ECounter Counter);
		const char* MoveName(EMove Move);

		class FMind
		{
		public:
			/** The commander did something (Weight 1 = one event). */
			void Observe(ETrait Trait, double Weight = 1.0);
			/** Advance learning; bBossPresent speeds it up (the boss is the cleverest angel in the room). */
			void Tick(double DeltaSeconds, bool bBossPresent);
			/** Learning speed multiplier (the tier's AdaptRate, BIBAngelBrain.h; set by the angels, 1 = the base rate). */
			void SetLearnMult(double Mult) { LearnMult = Mult > 0.0 ? Mult : 1.0; }
			double GetLearnMult() const { return LearnMult; }
			/** A new stage: rates reset, expertise partly kept. */
			void NewStage();
			void Reset();

			double Rate(ETrait Trait) const { return Rates[static_cast<int>(Trait)]; }
			double Pressure(ECounter Counter) const;
			double Expertise(ECounter Counter) const { return Skill[static_cast<int>(Counter)]; }
			bool Knows(ECounter Counter) const { return Expertise(Counter) >= KnownAt; }
			int KnownCount() const;
			/** Overall adaptation 0..1 (mean of the three best counters). */
			double Adaptation() const;
			/** Counters learned since the last call (for the "ANGELS ADAPTED" banner). */
			std::vector<ECounter> TakeNewlyLearned();
			/** "Guard 64% · Blink 31%": what the angels know, strongest first. */
			std::string Describe() const;
			void SetExpertise(ECounter Counter, double Value);   // tests / console

		private:
			double Rates[TraitCount] = {};
			double Skill[CounterCount] = {};
			bool Announced[CounterCount] = {};
			std::vector<ECounter> Fresh;
			double LearnMult = 1.0;
		};

		struct FSituation
		{
			double Distance = 0.0;            // cm to the commander
			double AttackRange = 150.0;       // cm, this angel's basic reach
			bool bPlayerAirborne = false;
			bool bPlayerBlocking = false;
			bool bPlayerJustSwitched = false; // within the last 2 s
			bool bPlayerAttacking = false;    // attacked in the last 0.6 s
			bool bFacingPlayer = true;        // the commander is looking at this angel
			int NearbyAllies = 0;             // other angels within 300 cm
			double HealthFraction = 1.0;
		};

		struct FDecision
		{
			EMove Move = EMove::Strike;
			double WindupMult = 1.0;          // feints and heavy moves telegraph differently
			bool bUnblockable = false;
			bool bSpread = false;             // move away from nearby allies first (Scatter)
		};

		/** Roll is 0..1 (a random number from the caller, so tests are deterministic). */
		FDecision Decide(const FMind& Mind, ERole Role, const FSituation& Situation, double Roll);

		/** How many angels may attack at once (the rest circle and flank). Grows with the mind's adaptation. */
		int MaxAttackers(int AliveAngels, double Adaptation);
	}
}
