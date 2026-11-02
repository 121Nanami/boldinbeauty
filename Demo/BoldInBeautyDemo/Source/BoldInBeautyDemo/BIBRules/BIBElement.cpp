// Boldinbeauty demo — the element system (see BIBElement.h).
#include "BIBElement.h"

#include "BIBCore.h"
#include "BIBEvents.h"
#include "BIBMeta.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cstring>

namespace BIB
{
	namespace Element
	{
		namespace
		{
			struct FElementDef
			{
				const char* Name;
				const char* Icon;
				const char* Blurb;
				float Colour[3];
			};

			const FElementDef& Def(EElement E)
			{
				static const FElementDef Defs[ElementCount] = {
					{"Fire", "el_fire", "Burns through Nature.", {1.00f, 0.38f, 0.16f}},
					{"Water", "el_water", "Douses Fire.", {0.20f, 0.55f, 1.00f}},
					{"Nature", "el_nature", "Grounds Volt.", {0.35f, 0.85f, 0.30f}},
					{"Volt", "el_volt", "Electrifies Water.", {1.00f, 0.88f, 0.20f}},
					{"Dark", "el_dark", "Blacks out Glitch.", {0.55f, 0.30f, 0.80f}},
					{"Halo", "el_halo", "Angel light that purges Dark.", {1.00f, 0.96f, 0.75f}},
					{"Magic", "el_magic", "Binds Halo.", {0.90f, 0.35f, 0.85f}},
					{"Glitch", "el_glitch", "Crashes Magic.", {0.20f, 1.00f, 0.80f}},
				};
				static const FElementDef NoneDef = {"-", "el_none", "No element.", {0.7f, 0.7f, 0.7f}};
				const int Index = static_cast<int>(E);
				return Index >= 0 && Index < ElementCount ? Defs[Index] : NoneDef;
			}

			// PROPOSED element of every operator, picked from their kits' themes (the owner decides):
			//   Fire   Shiraishi 3*, Plack 4*, Gambler 5*        (fire fist / Overdrive / hot streaks)
			//   Water  Myrcia 3*, Blood 5*, Weather 5*            (tide-like guard / Crimson Sea / rain and storms)
			//   Nature Arita 3*, Kuu 4*, Alchemist 5*             (flowers / earth-heavy defender / infection and soil)
			//   Volt   Fujita 3*, Narrator 6*                     (flicker speed / speed and explosions)
			//   Dark   Luna 4*, Death 6*                          (black hole / the reaper)
			//   Halo   Judge 5*, Stargazer 5*                     (judgement / starlight)
			//   Magic  Ishii 3*, Jester 6*, Timekeeper 6*         (arcane mage / stage magic / time magic)
			//   Glitch Media 6*, Adapt 6*                         (cyber teleports / glitch steps)
			struct FOperatorElement
			{
				const char* Name;
				EElement Element;
			};
			constexpr FOperatorElement OperatorElements[] = {
				{"Shiraishi", EElement::Fire}, {"Plack", EElement::Fire}, {"Gambler", EElement::Fire},
				{"Myrcia", EElement::Water}, {"Blood", EElement::Water}, {"Weather", EElement::Water},
				{"Arita", EElement::Nature}, {"Kuu", EElement::Nature}, {"Alchemist", EElement::Nature},
				{"Fujita", EElement::Volt}, {"Narrator", EElement::Volt},
				{"Luna", EElement::Dark}, {"Death", EElement::Dark},
				{"Judge", EElement::Halo}, {"Stargazer", EElement::Halo},
				{"Ishii", EElement::Magic}, {"Jester", EElement::Magic}, {"Timekeeper", EElement::Magic},
				{"Media", EElement::Glitch}, {"Adapt", EElement::Glitch},
			};
		}

		const char* Name(EElement E) { return Def(E).Name; }
		const char* Icon(EElement E) { return Def(E).Icon; }
		const char* Blurb(EElement E) { return Def(E).Blurb; }

		void Colour(EElement E, float Out[3])
		{
			const FElementDef& D = Def(E);
			Out[0] = D.Colour[0];
			Out[1] = D.Colour[1];
			Out[2] = D.Colour[2];
		}

		EElement FromIndex(int Index)
		{
			return Index < 0 || Index >= ElementCount ? EElement::None : static_cast<EElement>(Index);
		}

		EElement StrongAgainst(EElement E)
		{
			switch (E)
			{
			case EElement::Fire: return EElement::Nature;
			case EElement::Nature: return EElement::Volt;
			case EElement::Volt: return EElement::Water;
			case EElement::Water: return EElement::Fire;
			case EElement::Halo: return EElement::Dark;
			case EElement::Dark: return EElement::Glitch;
			case EElement::Glitch: return EElement::Magic;
			case EElement::Magic: return EElement::Halo;
			default: return EElement::None;
			}
		}

		EElement WeakTo(EElement E)
		{
			for (int Index = 0; Index < ElementCount; ++Index)
			{
				if (StrongAgainst(static_cast<EElement>(Index)) == E && E != EElement::None)
				{
					return static_cast<EElement>(Index);
				}
			}
			return EElement::None;
		}

		bool HasAdvantage(EElement Attacker, EElement Defender)
		{
			return Attacker != EElement::None && Defender != EElement::None && StrongAgainst(Attacker) == Defender;
		}

		EElement OfOperator(int RosterIndex)
		{
			if (RosterIndex < 0 || RosterIndex >= static_cast<int>(Roster().size()))
			{
				return EElement::None;
			}
			const char* OperatorName = Roster()[static_cast<size_t>(RosterIndex)].Name;
			for (const FOperatorElement& Entry : OperatorElements)
			{
				if (std::strcmp(Entry.Name, OperatorName) == 0)
				{
					return Entry.Element;
				}
			}
			return EElement::None;
		}

		EElement OfChapter(int Chapter)
		{
			// PLACEHOLDER: Chapter 1 teaches the basics without elements; chapters 2-9 introduce one element each, in ring order
			// so a new player meets the elements their starting team (Luna Dark, Plack Fire, Kuu Nature) counters early.
			static const EElement Order[] = {EElement::Nature, EElement::Glitch, EElement::Water, EElement::Fire, EElement::Volt,
				EElement::Halo, EElement::Dark, EElement::Magic, EElement::Glitch, EElement::Volt, EElement::Magic, EElement::Halo};
			if (Chapter < 2 || Chapter > CampaignChapters)
			{
				return EElement::None;
			}
			return Order[Chapter - 2];
		}

		EElement OfStage(int StageId, int Section)
		{
			if (!IsValidStage(StageId))
			{
				return EElement::None;
			}
			const FStageRef Ref = DecodeStage(StageId);
			switch (Ref.Kind)
			{
			case EStageKind::Campaign:
			{
				const EElement Main = OfChapter(Ref.A);
				// Chapters 10-13 mix two elements: odd sections bring the element the main one is strong against, so one
				// team cannot counter every section (PLACEHOLDER).
				if (Ref.A >= 10 && Main != EElement::None && Section % 2 == 1)
				{
					return StrongAgainst(Main);
				}
				return Main;
			}
			case EStageKind::Tower:
				// Floors 1-4 have no element; then every 5 floors the element moves on around both rings (PLACEHOLDER).
				return Ref.B < 5 ? EElement::None : static_cast<EElement>((Ref.B / 5 - 1) % ElementCount);
			case EStageKind::BossRush:
				return static_cast<EElement>((std::max(1, Ref.B) - 1) % ElementCount);
			case EStageKind::SubStory:
				return EElement::None;
			}
			return EElement::None;
		}

		EElement CounterOf(int StageId)
		{
			return WeakTo(OfStage(StageId, 0));
		}

		// ------------------------------------------------------------------ attunement
		int Attunement(const FProfile& Profile, int RosterIndex)
		{
			if (RosterIndex < 0 || RosterIndex >= static_cast<int>(Profile.Operators.size()))
			{
				return 0;
			}
			return std::clamp(Profile.Operators[static_cast<size_t>(RosterIndex)].Attunement, 0, MaxAttunement);
		}

		int AttunementCores(int TargetLevel)
		{
			return AttunementCoresStep * std::clamp(TargetLevel, 1, MaxAttunement);
		}

		int AttunementGold(int TargetLevel)
		{
			return AttunementGoldStep * std::clamp(TargetLevel, 1, MaxAttunement);
		}

		int AttunementCoresFor(const FProfile& Profile, int RosterIndex, int TargetLevel)
		{
			const int Cores = AttunementCores(TargetLevel);
			if (Events::SurgeActive(Profile) && Events::SurgeElement(Profile) == OfOperator(RosterIndex))
			{
				return Cores - Cores * Events::SurgeDiscountPercent / 100;
			}
			return Cores;
		}

		FActionResult TryAttune(FProfile& Profile, int RosterIndex)
		{
			FActionResult Result;
			if (RosterIndex < 0 || RosterIndex >= static_cast<int>(Profile.Operators.size()) || !Profile.Owns(RosterIndex))
			{
				Result.Message = "You do not own that operator.";
				return Result;
			}
			FOwnedOperator& Op = Profile.Operators[static_cast<size_t>(RosterIndex)];
			if (Op.Attunement >= MaxAttunement)
			{
				Result.Message = "Attunement is at the maximum (" + std::to_string(MaxAttunement) + ").";
				return Result;
			}
			const int Target = Op.Attunement + 1;
			const int Cores = AttunementCoresFor(Profile, RosterIndex, Target);
			const int Gold = AttunementGold(Target);
			if (Profile.Get(ECurrency::ElementCore) < Cores || Profile.Get(ECurrency::Gold) < Gold)
			{
				Result.Message = "Attunement " + std::to_string(Target) + " needs " + Meta::Num(Cores) + " Element cores and " + Meta::Num(Gold) + " Gold.";
				return Result;
			}
			Profile.Spend(ECurrency::ElementCore, Cores);
			Profile.Spend(ECurrency::Gold, Gold);
			Op.Attunement = Target;
			Events::Notify(Profile, Events::EStat::Attune);
			Result.bOk = true;
			Result.Message = std::string(Roster()[static_cast<size_t>(RosterIndex)].Name) + " reached " + Name(OfOperator(RosterIndex)) +
				" Attunement " + std::to_string(Target) + ": advantage damage +" + std::to_string(static_cast<int>(AdvantageBonusOf(Profile, RosterIndex) * 100.0 + 0.5)) + "%.";
			return Result;
		}

		double AdvantageBonusOf(const FProfile& Profile, int RosterIndex)
		{
			return AdvantageBonus + AttunementBonusPerLevel * Attunement(Profile, RosterIndex);
		}

		double DamageMultiplier(const FProfile& Profile, int RosterIndex, EElement Target)
		{
			return HasAdvantage(OfOperator(RosterIndex), Target) ? 1.0 + AdvantageBonusOf(Profile, RosterIndex) : 1.0;
		}

		std::string Describe(EElement E)
		{
			if (E == EElement::None)
			{
				return "No element";
			}
			return std::string(Name(E)) + " - strong against " + Name(StrongAgainst(E)) + ", weak to " + Name(WeakTo(E));
		}
	}
}
