#include "BIBSquad.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Tactics
	{
		namespace
		{
			constexpr double Pi = 3.14159265358979323846;
		}

		double WrapAngle(double Radians)
		{
			while (Radians > Pi)
			{
				Radians -= 2.0 * Pi;
			}
			while (Radians <= -Pi)
			{
				Radians += 2.0 * Pi;
			}
			return Radians;
		}

		const FSquad::FEntry* FSquad::Find(int Id) const
		{
			for (const FEntry& Entry : Entries)
			{
				if (Entry.Id == Id)
				{
					return &Entry;
				}
			}
			return nullptr;
		}

		bool FSquad::HasToken(int Id) const
		{
			const FEntry* Entry = Find(Id);
			return Entry && (Entry->Class == EClass::Boss || Entry->bToken);
		}

		EDuty FSquad::DutyOf(int Id) const
		{
			const FEntry* Entry = Find(Id);
			return Entry ? Entry->Duty : EDuty::Wait;
		}

		double FSquad::SlotAngle(int Id) const
		{
			const FEntry* Entry = Find(Id);
			return Entry ? Entry->Slot : 0.0;
		}

		int FSquad::TokensHeld() const
		{
			int Count = 0;
			for (const FEntry& Entry : Entries)
			{
				Count += (Entry.Class == EClass::Melee && Entry.bToken) ? 1 : 0;
			}
			return Count;
		}

		int FSquad::CommittedMelee() const
		{
			int Count = 0;
			for (const FEntry& Entry : Entries)
			{
				Count += (Entry.Class == EClass::Melee && Entry.bCommitted) ? 1 : 0;
			}
			return Count;
		}

		void FSquad::Update(const FSquadInput& Input, const std::vector<FAgent>& Agents)
		{
			// Carry state over for angels that were here last frame; new angels start waiting.
			std::vector<FEntry> Next;
			Next.reserve(Agents.size());
			for (const FAgent& Agent : Agents)
			{
				FEntry Entry;
				if (const FEntry* Old = Find(Agent.Id))
				{
					Entry = *Old;
				}
				else
				{
					Entry.TokenSince = Input.Now;
					Entry.WaitSince = Input.Now;
				}
				Entry.Id = Agent.Id;
				Entry.Class = Agent.Class;
				Entry.bCommitted = Agent.bCommitted;
				const double Dx = Agent.X - Input.TargetX;
				const double Dy = Agent.Y - Input.TargetY;
				Entry.Distance = std::sqrt(Dx * Dx + Dy * Dy);
				Entry.Bearing = std::atan2(Dy, Dx);
				Next.push_back(Entry);
			}
			Entries.swap(Next);

			const int Capacity = std::max(1, std::min(Input.Capacity, TokenCap));

			// Release: unused tokens rotate; a committed angel keeps its token until the strike resolves.
			int Held = 0;
			for (FEntry& Entry : Entries)
			{
				if (Entry.Class != EClass::Melee)
				{
					Entry.bToken = false;
					continue;
				}
				if (Entry.bCommitted)
				{
					if (!Entry.bToken)
					{
						Entry.bToken = true;      // it committed on its own (illusion, guard riposte): count it
						Entry.TokenSince = Input.Now;
					}
				}
				else if (Entry.bToken && Input.Now - Entry.TokenSince > TokenHoldSeconds)
				{
					Entry.bToken = false;
					Entry.WaitSince = Input.Now;
				}
				Held += Entry.bToken ? 1 : 0;
			}
			// A smaller capacity (the commander is hurt): the farthest idle holders give theirs back first.
			while (Held > Capacity)
			{
				FEntry* Far = nullptr;
				for (FEntry& Entry : Entries)
				{
					if (Entry.Class == EClass::Melee && Entry.bToken && !Entry.bCommitted && (!Far || Entry.Distance > Far->Distance))
					{
						Far = &Entry;
					}
				}
				if (!Far)
				{
					break;   // everyone left is already committed: let those strikes finish
				}
				Far->bToken = false;
				Far->WaitSince = Input.Now;
				--Held;
			}
			// Grant: the closest angel, discounted by how long it has waited, takes each free token.
			while (Held < Capacity)
			{
				FEntry* Best = nullptr;
				double BestScore = 0.0;
				for (FEntry& Entry : Entries)
				{
					if (Entry.Class != EClass::Melee || Entry.bToken)
					{
						continue;
					}
					const double Score = Entry.Distance - WaitBonusCmPerSecond * (Input.Now - Entry.WaitSince);
					if (!Best || Score < BestScore || (Score == BestScore && Entry.Id < Best->Id))
					{
						Best = &Entry;
						BestScore = Score;
					}
				}
				if (!Best)
				{
					break;
				}
				Best->bToken = true;
				Best->TokenSince = Input.Now;
				++Held;
			}

			// Duties.
			std::vector<FEntry*> Idle;
			for (FEntry& Entry : Entries)
			{
				switch (Entry.Class)
				{
				case EClass::Boss:
					Entry.Duty = EDuty::Pressure;
					break;
				case EClass::Ranged:
					Entry.Duty = EDuty::Support;
					break;
				default:
					if (Entry.bToken)
					{
						Entry.Duty = EDuty::Pressure;
					}
					else
					{
						Entry.Duty = EDuty::Wait;
						Idle.push_back(&Entry);
					}
					break;
				}
			}
			std::sort(Idle.begin(), Idle.end(), [](const FEntry* A, const FEntry* B)
			{
				return A->Distance != B->Distance ? A->Distance < B->Distance : A->Id < B->Id;
			});
			const size_t Flankers = static_cast<size_t>(std::max(0, Input.Flankers));
			for (size_t Index = 0; Index < Idle.size() && Index < Flankers; ++Index)
			{
				Idle[Index]->Duty = EDuty::Flank;
			}

			AssignSlots(Input, EDuty::Flank, 0.6, true);
			AssignSlots(Input, EDuty::Wait, 2.0 * Pi, false);   // waiters share the whole ring
			AssignSlots(Input, EDuty::Support, 0.7, false);
		}

		// Spreads one duty's angels evenly around a base bearing, in the order they already stand, so nobody crosses.
		void FSquad::AssignSlots(const FSquadInput& Input, EDuty Duty, double Spacing, bool bAroundBack)
		{
			std::vector<FEntry*> Group;
			for (FEntry& Entry : Entries)
			{
				if (Entry.Duty == Duty)
				{
					Group.push_back(&Entry);
				}
			}
			if (Group.empty())
			{
				return;
			}
			double Base = Input.TargetFacing + Pi;   // the commander's back
			if (!bAroundBack)
			{
				// The crowd's own mean bearing: the group opens up where it already is.
				double Sx = 0.0;
				double Sy = 0.0;
				for (const FEntry* Entry : Group)
				{
					Sx += std::cos(Entry->Bearing);
					Sy += std::sin(Entry->Bearing);
				}
				Base = (Sx == 0.0 && Sy == 0.0) ? Group.front()->Bearing : std::atan2(Sy, Sx);
			}
			const double Count = static_cast<double>(Group.size());
			const double Step = std::min(Spacing, 2.0 * Pi / Count);
			std::sort(Group.begin(), Group.end(), [Base](const FEntry* A, const FEntry* B)
			{
				const double Ra = WrapAngle(A->Bearing - Base);
				const double Rb = WrapAngle(B->Bearing - Base);
				return Ra != Rb ? Ra < Rb : A->Id < B->Id;
			});
			for (size_t Index = 0; Index < Group.size(); ++Index)
			{
				Group[Index]->Slot = WrapAngle(Base + (static_cast<double>(Index) - (Count - 1.0) * 0.5) * Step);
			}
		}
	}
}
