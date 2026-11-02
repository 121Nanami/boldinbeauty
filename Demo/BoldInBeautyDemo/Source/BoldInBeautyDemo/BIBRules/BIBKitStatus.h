// Boldinbeauty demo — status effects on enemies (engine-independent).
//
// FStatusBook is the one place that decides how statuses stack, tick, expire and modify damage and movement. The
// Unreal enemy owns a book and mirrors it into gameplay tags on its AbilitySystemComponent (BIBGAS/); the unit
// tests drive the same book through a fake world.
#pragma once

#include "BIBKitTypes.h"

namespace BIB
{
	namespace Kits
	{
		struct FStatusEvent
		{
			enum class EType : int
			{
				DotDamage,       // Amount = damage dealt by a damage-over-time tick
				Expired,         // the status ran out (Doom is only a mark: expiry never kills)
				ShockDischarge   // Shock reached its stack cap: Amount = burst damage
			};
			EType Type = EType::DotDamage;
			EStatus Status = EStatus::None;
			double Amount = 0.0;
			int Source = -1;
			int Stacks = 0;
		};

		constexpr double ShockDischargeDamage = 450.0;   // PROPOSED: per stack when Shock reaches its cap
		constexpr double WetShockBonus = 0.5;            // PROPOSED: Wet enemies take +50% from Shock discharge
		constexpr double MarkDamageTaken = 0.15;         // PROPOSED: Mark = +15% damage taken
		constexpr double FreezeDamageTaken = 0.20;       // PROPOSED: frozen enemies take +20%
		constexpr double MutatedDamageTaken = 0.20;      // SOURCE (Alchemist): mutation lowers Defence by 20%
		constexpr double MutatedDamageDealt = 0.20;      // SOURCE (Alchemist): mutation lowers damage by 20%

		class FStatusBook
		{
		public:
			// Adds Stacks (capped by the rule), refreshes the duration and keeps the strongest Power.
			// Returns false when the application was ignored (e.g. Infection while Immune).
			bool Apply(EStatus Status, double Seconds, double Power, int Stacks, int Source);
			void Tick(double Dt, std::vector<FStatusEvent>& Events);

			int Stacks(EStatus Status) const;
			bool Has(EStatus Status) const { return Stacks(Status) > 0; }
			double Power(EStatus Status) const;
			double Remaining(EStatus Status) const;
			int Source(EStatus Status) const;
			void Clear(EStatus Status);
			void ClearAll();
			void RemoveStacks(EStatus Status, int Count);
			int TotalStacks() const;

			bool Disabled() const;           // Stun or Freeze: cannot move or attack
			bool Feared() const { return Has(EStatus::Fear); }
			bool Illusory() const { return Has(EStatus::Illusion); }
			double MoveMult() const;         // 0 = cannot move
			double DamageTakenMult() const;  // multiplies damage the enemy receives
			double DamageDealtMult() const;  // multiplies damage the enemy deals

			// For the HUD / label: "Burn x3, Slow".
			std::string Describe() const;

		private:
			struct FEntry
			{
				EStatus Id = EStatus::None;
				double Remaining = 0.0;
				double Power = 0.0;
				int Stacks = 0;
				double Timer = 0.0;
				int Source = -1;
			};
			std::vector<FEntry> Entries;
			FEntry* Find(EStatus Status);
			const FEntry* Find(EStatus Status) const;
		};
	}
}
