// Boldinbeauty demo — Daze, stun and combo rules (owner request 2026-10-09, Notion "Dev notes": make the gameplay feel
// "way way more like Zenless Zone Zero", with stuns / stun locks, attacks that flow together and bosses that are not
// "3 minutes of walking in a circle spamming LMB"). Engine-independent; the Unreal layer is BIBGame/BIBEnemy.cpp and
// BIBGame/BIBPlayerFeel.cpp.
//
// In game the meter is called STRAIN and a full meter BREAKS the enemy; the switch-in strike on a broken enemy is the RELAY
// STRIKE (owner 2026-10-13: own names, not the reference game's). Identifiers below keep the older "Daze" / "chain" names.
// How a hit feels (hit stop, reaction tiers, camera, rumble) is BIBHitFeel.h; TryFlinch below is the pre-2026-10-13 flinch
// rule, kept for the rules tests and replaced in game by BIBHitFeel.h ResolveReaction.
//
// How it works:
//   * Every enemy has a Daze meter. Hits fill it: a hit adds Daze from how hard it hit relative to the operator's attack
//     (so every kit fills it without per-ability tables), plus fixed bonuses for combo finishers, dodge counters, parry
//     counters and chain attacks.
//   * A full meter STUNS the enemy: it cannot act, takes StunDamageMult damage, and the player is offered a CHAIN ATTACK
//     (switch operator during the chain window: the incoming operator enters with a big strike).
//   * Without hits the meter drains after a short delay. After a stun the meter is empty and briefly immune.
//   * Hit reactions ("stun locks"): light enemies flinch on every hit (their attack windup is cancelled), brutes flinch on
//     heavy hits, mini bosses and bosses only when stunned. Too many flinches in a short time grant a short poise window
//     so a crowd cannot be locked forever by one button (the angels' Guard counter, BIBAngelMind.h, punishes spam further).
// Every number here is PLACEHOLDER.
#pragma once

namespace BIB
{
	namespace Stagger
	{
		enum class EWeight : int
		{
			Light = 0,   // grunt, ranged, mage, assassin: flinch on every hit
			Medium,      // brute: flinch on heavy hits
			Heavy        // mini boss, boss: never flinch, only stun
		};

		enum class EBonus : int
		{
			None = 0,
			ComboFinisher,
			DodgeCounter,
			ParryCounter,
			ChainAttack,
			SwitchStrike
		};

		// ------------------------------------------------------------------ tuning (PLACEHOLDER)
		constexpr double DazeMaxLight = 100.0;
		constexpr double DazeMaxBrute = 170.0;
		constexpr double DazeMaxMiniBoss = 420.0;
		constexpr double DazeMaxBoss = 750.0;
		constexpr double DazePerAttack = 9.0;          // a hit for exactly the operator's attack
		constexpr double DazeExponent = 0.7;           // big hits fill more, but sub-linearly
		constexpr double DazeMinPerHit = 1.5;
		constexpr double DazeMaxPerHit = 70.0;
		constexpr double DrainDelay = 3.5;             // seconds without a hit before the meter drains
		constexpr double DrainPerSecond = 0.06;        // share of the maximum per second
		constexpr double StunSecondsLight = 3.0;
		constexpr double StunSecondsBrute = 3.5;
		constexpr double StunSecondsBoss = 6.0;        // mini boss and boss
		constexpr double StunDamageMult = 1.6;         // damage taken while stunned
		constexpr double ImmuneAfterStun = 2.0;        // no Daze for this long after a stun ends
		constexpr double ChainWindow = 2.5;            // seconds to switch for a chain attack after a stun begins
		constexpr double FlinchSeconds = 0.32;
		constexpr double HeavyHitRatio = 2.5;          // a hit this many times the attack is "heavy" (brutes flinch)
		constexpr int FlinchesBeforePoise = 5;         // within PoiseWindow seconds ...
		constexpr double PoiseWindow = 2.5;
		constexpr double PoiseSeconds = 1.6;           // ... then no flinching for this long

		double BonusDaze(EBonus Bonus);
		double DazeMax(EWeight Weight, bool bBoss);
		double StunSeconds(EWeight Weight, bool bBoss);

		/** Daze added by a hit that dealt Dealt damage from an operator whose attack is Attack. */
		double DazeForHit(double Dealt, double Attack, EBonus Bonus);

		struct FMeter
		{
			double Daze = 0.0;
			double Max = DazeMaxLight;
			double LastHitAt = -100.0;
			double StunnedUntil = -1.0;
			double ImmuneUntil = -1.0;
			double ChainUntil = -1.0;
			int StunCount = 0;
			// hit reactions
			double FlinchUntil = -1.0;
			double PoiseUntil = -1.0;
			int FlinchesInWindow = 0;
			double FlinchWindowStart = -100.0;
			double BossInterruptAt = -100.0;   // bosses: last interrupting reaction (BIBHitFeel.h ResolveReaction)

			void Reset(double InMax);
			/** Adds Daze; returns true when this hit stunned the enemy (StunFor seconds from Now). */
			bool Add(double Points, double Now, double StunFor);
			void Tick(double Now, double DeltaSeconds);
			bool Stunned(double Now) const { return Now < StunnedUntil; }
			bool ChainOpen(double Now) const { return Now < ChainUntil; }
			double Fraction(double Now) const;
			/** Returns true when the hit makes the enemy flinch (cancel its windup). Ratio = Dealt / operator attack. */
			bool TryFlinch(EWeight Weight, double Ratio, double Now);
			bool Flinching(double Now) const { return Now < FlinchUntil; }
		};

		/** Damage multiplier for a hit on an enemy in this state. */
		double DamageTakenMult(const FMeter& Meter, double Now);

		// ------------------------------------------------------------------ basic-attack combo string
		// Pressing attack again inside the window advances the string; the last step is the finisher (more Daze, knockback).
		constexpr int ComboLength = 4;
		constexpr double ComboWindow = 0.9;             // seconds after a hit to continue the string
		double ComboStepMult(int Step);               // 0-based; the average over a full string is ~1.06 (PLACEHOLDER)

		struct FCombo
		{
			int Step = -1;
			double LastAt = -100.0;
			/** Returns the 0-based step of this hit. Window: seconds after the previous hit to continue the string. */
			int Advance(double Now, double Window = ComboWindow);
			void Reset() { Step = -1; LastAt = -100.0; }
			bool IsFinisher() const { return Step == ComboLength - 1; }
		};
	}
}
