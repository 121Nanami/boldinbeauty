// Boldinbeauty demo — the angels' squad board (behaviour layer, 2026-10-12). Engine-independent; BIBGame/BIBTactics.* feeds it
// the live angels once per frame and BIBGame/BIBEnemy.cpp reads each angel's assignment.
//
//   * Attack tokens: at most Capacity melee angels may be committed (telegraphing or swinging) at the same time; Capacity is
//     the caller's number (BIBAngelMind MaxAttackers, eased by the player model) clamped to TokenCap. A token that is not
//     used within TokenHoldSeconds rotates to whoever has waited longest, so one angel stuck behind a wall cannot hold it.
//     Bosses always attack and are not counted.
//   * Roles: Pressure (holds a token and closes in), Flank (the nearest angels without a token, who circle to the
//     commander's back), Wait (the rest, who circle at the edge of reach) and Support (ranged and mage angels, who keep
//     their distance). Each angel without a token also gets an angular slot around the commander so the crowd spreads
//     out instead of stacking on one point.
// Every number here is PLACEHOLDER.
#pragma once

#include <vector>

namespace BIB
{
	namespace Tactics
	{
		constexpr int TokenCap = 3;                    // "about 2-3 at once"
		constexpr double TokenHoldSeconds = 2.5;       // an unused token rotates after this
		constexpr double WaitBonusCmPerSecond = 120.0; // waiting is worth this much distance when tokens are handed out

		enum class EClass : int
		{
			Melee = 0,
			Ranged,   // ranged and mage angels
			Boss      // mini boss and boss: always attack, are not counted
		};

		enum class EDuty : int
		{
			Pressure = 0,
			Flank,
			Support,
			Wait
		};

		struct FAgent
		{
			int Id = 0;
			EClass Class = EClass::Melee;
			double X = 0.0;
			double Y = 0.0;
			bool bCommitted = false;   // telegraphing or swinging now
		};

		struct FSquadInput
		{
			double Now = 0.0;
			double TargetX = 0.0;      // the commander
			double TargetY = 0.0;
			double TargetFacing = 0.0; // radians, the way the commander looks
			int Capacity = 2;          // simultaneous melee attackers wanted (clamped to TokenCap)
			int Flankers = 1;          // how many waiting melee angels circle to the back
		};

		double WrapAngle(double Radians);

		class FSquad
		{
		public:
			void Reset() { Entries.clear(); }
			void Update(const FSquadInput& Input, const std::vector<FAgent>& Agents);

			bool Knows(int Id) const { return Find(Id) != nullptr; }
			bool HasToken(int Id) const;
			EDuty DutyOf(int Id) const;
			/** Radians: the bearing from the commander this angel should stand at (Flank, Wait and Support angels). */
			double SlotAngle(int Id) const;
			int TokensHeld() const;                 // melee angels that hold a token (committed or not)
			int CommittedMelee() const;

		private:
			struct FEntry
			{
				int Id = 0;
				EClass Class = EClass::Melee;
				bool bToken = false;
				bool bCommitted = false;
				double TokenSince = 0.0;
				double WaitSince = 0.0;
				double Distance = 0.0;
				double Bearing = 0.0;
				double Slot = 0.0;
				EDuty Duty = EDuty::Wait;
			};
			std::vector<FEntry> Entries;
			const FEntry* Find(int Id) const;
			void AssignSlots(const FSquadInput& Input, EDuty Duty, double Spacing, bool bAroundBack);
		};
	}
}
