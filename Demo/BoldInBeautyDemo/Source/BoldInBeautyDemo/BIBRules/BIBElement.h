// Boldinbeauty demo — the element system (owner request 2026-10-07: "an element system that works a bit like Nikke's, with
// 8 types: dark, nature, water, fire, magic and 3 other types you can make up"). Engine-independent.
//
// How it works (NIKKE-style "code weakness"):
//   * Every operator has one element; enemies of a campaign chapter, an Ark Tower floor or a Boss Rush round have one too.
//   * Each element is strong against exactly one other element and weak to exactly one (two rings of four, below).
//   * Hitting an enemy your element is strong against deals +AdvantageBonus damage, plus the operator's Attunement bonus.
//     There is no penalty for a disadvantaged hit (as in NIKKE): elements reward a good team pick, they never lock a team out.
//   * Attunement (levels 0-10 per operator) raises that advantage bonus; it costs Element cores and Gold.
//
// The owner named Fire, Water, Nature, Dark and Magic. Volt, Halo and Glitch were suggested by the assistant (Neo Tokyo's machines,
// the angels' light, the 49% robot / Bytes side of the operators) and APPROVED by the owner on 2026-10-07, together with every
// operator's element. The chapter elements and all numbers remain PLACEHOLDER.
#pragma once

#include <string>

namespace BIB
{
	class FProfile;
	struct FActionResult;

	enum class EElement : int
	{
		None = -1,
		Fire = 0,
		Water,
		Nature,
		Volt,     // PROPOSED: electricity, Neo Tokyo's machines and speed
		Dark,
		Halo,     // PROPOSED: light, the angels' own element
		Magic,
		Glitch,   // PROPOSED: corrupted data, the robot / Bytes side of the operators
		Count
	};
	constexpr int ElementCount = static_cast<int>(EElement::Count);

	namespace Element
	{
		// ------------------------------------------------------------------ tuning (PLACEHOLDER)
		constexpr double AdvantageBonus = 0.15;          // +15% damage on an advantaged hit (NIKKE's code weakness is +10%)
		constexpr int MaxAttunement = 10;
		constexpr double AttunementBonusPerLevel = 0.03; // +3% advantage damage per Attunement level (level 10: +45% in all)
		constexpr int AttunementCoresStep = 10;          // level n -> n+1 costs 10 x (n+1) Element cores ...
		constexpr int AttunementGoldStep = 2500;         // ... and 2,500 x (n+1) Gold
		constexpr double AttunementPowerPerLevel = 0.02; // battle power +2% per Attunement level

		// ------------------------------------------------------------------ data
		const char* Name(EElement E);            // "Fire", "-" for None
		const char* Icon(EElement E);            // "el_fire" (Art/Icons/el_fire.png)
		const char* Blurb(EElement E);           // one line of flavour
		void Colour(EElement E, float Out[3]);   // linear RGB 0..1 for UI tints and enemy labels
		EElement FromIndex(int Index);           // clamps; -1 -> None

		// The rings: Fire > Nature > Volt > Water > Fire, and Halo > Dark > Glitch > Magic > Halo.
		EElement StrongAgainst(EElement E);      // the element E deals bonus damage to
		EElement WeakTo(EElement E);             // the element that deals bonus damage to E
		bool HasAdvantage(EElement Attacker, EElement Defender);

		// Elements of operators (by roster index) and of what the player fights.
		EElement OfOperator(int RosterIndex);
		EElement OfChapter(int Chapter);                 // Chapter 1 (the tutorial chapter) has none
		EElement OfStage(int StageId, int Section = 0);  // campaign, tower floor, boss rush round; sub-stories have none
		// The element that counters a stage (what the player should bring); None when the stage has no element.
		EElement CounterOf(int StageId);

		// ------------------------------------------------------------------ attunement
		int Attunement(const FProfile& Profile, int RosterIndex);
		int AttunementCores(int TargetLevel);    // cost to reach TargetLevel (1..MaxAttunement)
		int AttunementGold(int TargetLevel);
		// Element Surge (BIBEvents.h) discounts the Element cores of the week's element.
		int AttunementCoresFor(const FProfile& Profile, int RosterIndex, int TargetLevel);
		FActionResult TryAttune(FProfile& Profile, int RosterIndex);

		// Damage multiplier of a hit by this operator on an enemy of that element (1.0 when there is no advantage).
		double DamageMultiplier(const FProfile& Profile, int RosterIndex, EElement Target);
		double AdvantageBonusOf(const FProfile& Profile, int RosterIndex);   // AdvantageBonus + attunement

		// "Fire - strong against Nature, weak to Water"
		std::string Describe(EElement E);
	}
}
