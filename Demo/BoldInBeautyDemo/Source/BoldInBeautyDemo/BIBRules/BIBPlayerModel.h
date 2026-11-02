// Boldinbeauty demo — the angels' rolling model of the commander (behaviour layer, 2026-10-12). Engine-independent; the Unreal
// glue is BIBGame/BIBTactics.* (it samples the player every frame) and BIBGame/BIBEnemy.cpp (it acts on the advice).
//
// BIBAngelMind.h decides WHICH counter-move an angel knows (Guard, Feint, Blink ...). This model decides HOW the crowd behaves
// around the commander: when swings are timed, who closes the gap, who backs off, where an area attack is aimed. It tracks
//   * dodge / block / parry frequency (edges of the player's state, decaying) and whether dodges come on a rhythm,
//   * kiting (backing away from the nearest angel) versus standing still,
//   * the typical engagement distance,
//   * recent damage taken (a decaying sum of health fractions),
//   * whether an ultimate or channel is running (a cast held longer than a basic attack).
// Advice is bounded: Strength() ramps up slowly while a fight is observed, is capped at StrengthCap, and falls while the
// commander is taking damage, so adaptation stays fair. It changes what an angel does, never the warning before it.
// Every number here is PLACEHOLDER (behaviour tuning only; hit points, damage, speed, interval and windup stay in the archetype table).
#pragma once

namespace BIB
{
	namespace Tactics
	{
		constexpr double HabitHalfLife = 20.0;       // seconds: how fast a dodge / block / parry habit is forgotten
		constexpr double DamageHalfLife = 6.0;       // seconds: "recent" damage
		constexpr double StrengthCap = 0.6;          // adaptation never exceeds this
		constexpr double StrafeBiasMax = StrengthCap; // StrafeBias is S * (0.5 .. 1), so it tops out at the cap
		constexpr double StrengthRampSeconds = 45.0; // seconds of observed fighting until the cap
		constexpr double ChannelMinSeconds = 0.8;    // a cast held this long counts as an ultimate / channel, not a basic attack
		constexpr double BackOffMaxSeconds = 3.0;    // angels give way for at most this long per channel
		constexpr double RetreatSpeedCmS = 120.0;    // moving away from the nearest angel faster than this = kiting
		constexpr double StillSpeedCmS = 40.0;       // slower than this = standing still
		constexpr double EngageRangeCm = 2500.0;     // an angel farther away than this does not count as the fight
		constexpr int DodgeHistory = 6;

		struct FPlayerSample
		{
			double Dt = 0.0;                 // seconds since the last sample
			double Speed = 0.0;              // horizontal cm/s
			double RetreatSpeed = 0.0;       // cm/s moving away from the nearest angel (negative = closing in)
			double NearestDistance = -1.0;   // cm to the nearest angel; < 0 = nobody alive
			bool bCasting = false;           // BIB.State.Casting
			bool bDodging = false;           // BIB.State.Dodging
			bool bBlocking = false;
			bool bParrying = false;
			double HealthFraction = 1.0;     // active operator, 0..1
			bool bDiscontinuity = false;     // the operator was switched this sample: a health jump is not damage
		};

		// What the crowd should do differently. Every field is 0..1 unless noted and already scaled by Strength().
		struct FAdvice
		{
			double Strength = 0.0;        // overall adaptation, 0..StrengthCap
			double TimingJitterS = 0.0;   // seconds added at random to the pause between swings (breaks a dodge rhythm)
			double FeintBias = 0.0;       // chance that an honest strike is turned into a delayed feint (still telegraphed)
			double Intercept = 0.0;       // 0..1: aim melee approach at where a retreating commander is going
			double GapClose = 0.0;        // 0..1: more of the crowd presses instead of waiting while the commander kites
			double MageLead = 0.0;        // 0..1: how far ahead of a moving commander an area attack is aimed
			double MageCutOff = 0.0;      // chance an area attack is placed on the commander's escape side
			double StrafeBias = 0.0;      // 0..1: ranged angels reposition more often
			double BackOff = 0.0;         // 0..1: uncommitted angels give way (an ultimate / channel is running)
			int TokenDelta = 0;           // 0 or -1: fewer simultaneous melee attackers while the commander is hurt
			bool bChannel = false;
			bool bKiting = false;
		};

		class FPlayerModel
		{
		public:
			void Reset();
			void Sample(const FPlayerSample& Sample);

			double Clock() const { return ClockSeconds; }
			double ObservedSeconds() const { return Observed; }
			double DodgePerMinute() const;
			double BlockPerMinute() const;
			double ParryPerMinute() const;
			/** 0..1: how regular the dodge intervals are (needs three or more recent dodges). */
			double DodgeRhythm() const;
			double KiteFraction() const { return Kite; }
			double StillFraction() const { return Still; }
			/** cm, < 0 until an angel has been seen within EngageRangeCm. */
			double EngagementDistance() const { return Engagement; }
			/** Sum of recent health-fraction losses (1.0 = a whole health bar in the last few seconds). */
			double RecentDamage() const { return Damage; }
			bool ChannelActive() const { return CastSeconds >= ChannelMinSeconds; }
			double ChannelSeconds() const { return CastSeconds; }
			/** 0..StrengthCap. */
			double Strength() const;
			FAdvice Advise() const;

		private:
			double ClockSeconds = 0.0;
			double Observed = 0.0;       // seconds with at least one angel alive
			double DodgeCount = 0.0;     // decayed event counters
			double BlockCount = 0.0;
			double ParryCount = 0.0;
			double DodgeTimes[DodgeHistory] = {};
			int DodgeStored = 0;
			double Kite = 0.0;
			double Still = 0.0;
			double Engagement = -1.0;
			double Damage = 0.0;
			double CastSeconds = 0.0;
			double LastHealth = 1.0;
			bool bWasDodging = false;
			bool bWasBlocking = false;
			bool bWasParrying = false;
			bool bHaveHealth = false;
		};
	}
}
