// Boldinbeauty demo — status effects on enemies (see BIBKitStatus.h).
#include "BIBKitStatus.h"

#include <algorithm>
#include <cstdio>

namespace BIB
{
	namespace Kits
	{
		const char* SlotName(ESlotId Slot)
		{
			switch (Slot)
			{
			case ESlotId::Basic: return "M1";
			case ESlotId::Move1: return "Move1";
			case ESlotId::Move2: return "Move2";
			case ESlotId::Move3: return "Move3";
			case ESlotId::Special: return "Special";
			case ESlotId::Ultimate: return "Ultimate";
			default: return "?";
			}
		}

		const char* SlotKeyLabel(ESlotId Slot)
		{
			switch (Slot)
			{
			case ESlotId::Basic: return "LMB";
			case ESlotId::Move1: return "1";
			case ESlotId::Move2: return "2";
			case ESlotId::Move3: return "3";
			case ESlotId::Special: return "4";
			case ESlotId::Ultimate: return "E";
			default: return "?";
			}
		}

		const FStatusRule& StatusRule(EStatus Status)
		{
			//                          name          tag                        dot    tick  max  seconds
			static const FStatusRule Rules[StatusCount] = {
				{"None",       "BIB.Status.None",       false, 0.0, 1, 0.0},
				{"Stun",       "BIB.Status.Stun",       false, 0.0, 1, 1.0},
				{"Slow",       "BIB.Status.Slow",       false, 0.0, 1, 3.0},
				{"Root",       "BIB.Status.Root",       false, 0.0, 1, 2.0},
				{"Burn",       "BIB.Status.Burn",       true,  1.0, 6, 5.0},
				{"Bleed",      "BIB.Status.Bleed",      true,  1.0, 8, 6.0},
				{"Poison",     "BIB.Status.Poison",     true,  1.0, 6, 8.0},
				{"Freeze",     "BIB.Status.Freeze",     false, 0.0, 1, 2.0},
				{"Shock",      "BIB.Status.Shock",      false, 0.0, 5, 6.0},
				{"Wet",        "BIB.Status.Wet",        false, 0.0, 1, 8.0},
				{"Mark",       "BIB.Status.Mark",       false, 0.0, 1, 8.0},
				{"Vulnerable", "BIB.Status.Vulnerable", false, 0.0, 3, 6.0},
				{"Weaken",     "BIB.Status.Weaken",     false, 0.0, 1, 6.0},
				{"Fear",       "BIB.Status.Fear",       false, 0.0, 1, 3.0},
				{"Doom",       "BIB.Status.Doom",       false, 0.0, 1, 6.0},
				{"Infection",  "BIB.Status.Infection",  false, 0.0, 1, 100000.0},
				{"Mutated",    "BIB.Status.Mutated",    false, 0.0, 1, 100000.0},
				{"Immune",     "BIB.Status.Immune",     false, 0.0, 1, 100000.0},
				{"Illusion",   "BIB.Status.Illusion",   false, 0.0, 1, 5.0},
			};
			const int Index = static_cast<int>(Status);
			return Rules[(Index >= 0 && Index < StatusCount) ? Index : 0];
		}

		const char* BuffTag(EBuff Buff)
		{
			static const char* Tags[BuffCount] = {
				"BIB.Buff.DamageDealt", "BIB.Buff.DamageTaken", "BIB.Buff.MoveSpeed", "BIB.Buff.AttackSpeed",
				"BIB.Buff.CooldownFlow", "BIB.Buff.LifeSteal", "BIB.Buff.Reflect", "BIB.Buff.Evade", "BIB.Buff.Regen",
				"BIB.Buff.Immovable", "BIB.Buff.StatusResist", "BIB.Buff.EvadeChance"};
			const int Index = static_cast<int>(Buff);
			return Tags[(Index >= 0 && Index < BuffCount) ? Index : 0];
		}

		FStatusBook::FEntry* FStatusBook::Find(EStatus Status)
		{
			for (FEntry& Entry : Entries)
			{
				if (Entry.Id == Status)
				{
					return &Entry;
				}
			}
			return nullptr;
		}

		const FStatusBook::FEntry* FStatusBook::Find(EStatus Status) const
		{
			for (const FEntry& Entry : Entries)
			{
				if (Entry.Id == Status)
				{
					return &Entry;
				}
			}
			return nullptr;
		}

		bool FStatusBook::Apply(EStatus Status, double Seconds, double Power, int Stacks, int Source)
		{
			if (Status == EStatus::None || Stacks <= 0)
			{
				return false;
			}
			// Immunity (Alchemist): cannot contract Infection again.
			if (Status == EStatus::Infection && Has(EStatus::Immune))
			{
				return false;
			}
			const FStatusRule& Rule = StatusRule(Status);
			const double Duration = Seconds > 0.0 ? Seconds : Rule.DefaultSeconds;
			if (FEntry* Existing = Find(Status))
			{
				Existing->Stacks = std::min(Rule.MaxStacks, Existing->Stacks + Stacks);
				Existing->Remaining = std::max(Existing->Remaining, Duration);
				Existing->Power = std::max(Existing->Power, Power);
				Existing->Source = Source;
				return true;
			}
			FEntry Entry;
			Entry.Id = Status;
			Entry.Remaining = Duration;
			Entry.Power = Power;
			Entry.Stacks = std::min(Rule.MaxStacks, Stacks);
			Entry.Source = Source;
			Entries.push_back(Entry);
			return true;
		}

		void FStatusBook::Tick(double Dt, std::vector<FStatusEvent>& Events)
		{
			for (size_t Index = 0; Index < Entries.size();)
			{
				FEntry& Entry = Entries[Index];
				const FStatusRule& Rule = StatusRule(Entry.Id);
				Entry.Remaining -= Dt;
				if (Rule.bDamageOverTime && Rule.TickInterval > 0.0)
				{
					Entry.Timer += Dt;
					while (Entry.Timer >= Rule.TickInterval)
					{
						Entry.Timer -= Rule.TickInterval;
						FStatusEvent Event;
						Event.Type = FStatusEvent::EType::DotDamage;
						Event.Status = Entry.Id;
						Event.Amount = Entry.Power * static_cast<double>(Entry.Stacks);
						Event.Source = Entry.Source;
						Event.Stacks = Entry.Stacks;
						Events.push_back(Event);
					}
				}
				if (Entry.Id == EStatus::Shock && Entry.Stacks >= Rule.MaxStacks)
				{
					FStatusEvent Event;
					Event.Type = FStatusEvent::EType::ShockDischarge;
					Event.Status = Entry.Id;
					Event.Amount = ShockDischargeDamage * static_cast<double>(Entry.Stacks) * (Has(EStatus::Wet) ? 1.0 + WetShockBonus : 1.0);
					Event.Source = Entry.Source;
					Event.Stacks = Entry.Stacks;
					Events.push_back(Event);
					Entry.Remaining = 0.0;
				}
				if (Entry.Remaining <= 0.0)
				{
					FStatusEvent Event;
					Event.Type = FStatusEvent::EType::Expired;
					Event.Status = Entry.Id;
					Event.Source = Entry.Source;
					Event.Stacks = Entry.Stacks;
					Events.push_back(Event);
					Entries.erase(Entries.begin() + static_cast<std::ptrdiff_t>(Index));
					continue;
				}
				++Index;
			}
		}

		int FStatusBook::Stacks(EStatus Status) const
		{
			const FEntry* Entry = Find(Status);
			return Entry ? Entry->Stacks : 0;
		}

		double FStatusBook::Power(EStatus Status) const
		{
			const FEntry* Entry = Find(Status);
			return Entry ? Entry->Power : 0.0;
		}

		double FStatusBook::Remaining(EStatus Status) const
		{
			const FEntry* Entry = Find(Status);
			return Entry ? std::max(0.0, Entry->Remaining) : 0.0;
		}

		int FStatusBook::Source(EStatus Status) const
		{
			const FEntry* Entry = Find(Status);
			return Entry ? Entry->Source : -1;
		}

		void FStatusBook::Clear(EStatus Status)
		{
			Entries.erase(std::remove_if(Entries.begin(), Entries.end(), [Status](const FEntry& Entry) { return Entry.Id == Status; }), Entries.end());
		}

		void FStatusBook::ClearAll()
		{
			Entries.clear();
		}

		void FStatusBook::RemoveStacks(EStatus Status, int Count)
		{
			if (FEntry* Entry = Find(Status))
			{
				Entry->Stacks -= Count;
				if (Entry->Stacks <= 0)
				{
					Clear(Status);
				}
			}
		}

		int FStatusBook::TotalStacks() const
		{
			int Total = 0;
			for (const FEntry& Entry : Entries)
			{
				Total += Entry.Stacks;
			}
			return Total;
		}

		bool FStatusBook::Disabled() const
		{
			return Has(EStatus::Stun) || Has(EStatus::Freeze);
		}

		double FStatusBook::MoveMult() const
		{
			if (Disabled() || Has(EStatus::Root))
			{
				return 0.0;
			}
			double Mult = 1.0;
			if (const FEntry* Slow = Find(EStatus::Slow))
			{
				Mult = std::max(0.05, 1.0 - Slow->Power);
			}
			return Mult;
		}

		double FStatusBook::DamageTakenMult() const
		{
			double Bonus = 0.0;
			if (Has(EStatus::Mark))
			{
				Bonus += MarkDamageTaken;
			}
			if (Has(EStatus::Freeze))
			{
				Bonus += FreezeDamageTaken;
			}
			if (Has(EStatus::Mutated))
			{
				Bonus += MutatedDamageTaken;
			}
			if (const FEntry* Vulnerable = Find(EStatus::Vulnerable))
			{
				Bonus += Vulnerable->Power * static_cast<double>(Vulnerable->Stacks);
			}
			return 1.0 + Bonus;
		}

		double FStatusBook::DamageDealtMult() const
		{
			double Reduction = 0.0;
			if (const FEntry* Weaken = Find(EStatus::Weaken))
			{
				Reduction += Weaken->Power > 0.0 ? Weaken->Power : 0.25;
			}
			if (Has(EStatus::Mutated))
			{
				Reduction += MutatedDamageDealt;
			}
			return std::max(0.1, 1.0 - Reduction);
		}

		std::string FStatusBook::Describe() const
		{
			std::string Text;
			for (const FEntry& Entry : Entries)
			{
				if (!Text.empty())
				{
					Text += ", ";
				}
				Text += StatusRule(Entry.Id).Name;
				if (Entry.Stacks > 1)
				{
					char Buffer[16];
					std::snprintf(Buffer, sizeof(Buffer), " x%d", Entry.Stacks);
					Text += Buffer;
				}
			}
			return Text;
		}
	}
}
