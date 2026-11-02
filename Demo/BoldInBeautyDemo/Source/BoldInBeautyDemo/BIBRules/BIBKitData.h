// Boldinbeauty demo — the 17 data-driven operator kits (internal; use FindKit / AllKits from BIBKitTypes.h).
#pragma once

#include "BIBKitTypes.h"

namespace BIB
{
	namespace Kits
	{
		// Owner-designed kits (Notion, 2026-10-04). Gaps in the written kits are filled with PROPOSED entries.
		FKitDef MakeAlchemist();
		FKitDef MakeArita();
		FKitDef MakeAdapt();

		// Designed from the owner's playstyle notes (2026-10-04); every number is PROPOSED.
		FKitDef MakeBlood();
		FKitDef MakeDeath();
		FKitDef MakeFujita();
		FKitDef MakeGambler();
		FKitDef MakeIshii();
		FKitDef MakeJester();
		FKitDef MakeKuu();
		FKitDef MakeMedia();
		FKitDef MakeMyrcia();
		FKitDef MakePlack();
		FKitDef MakeShiraishi();
		FKitDef MakeStargazer();
		FKitDef MakeTimekeeper();
		FKitDef MakeWeather();

		// Gameplay tags for the three bespoke kits (their logic is written in Unreal; the vocabulary lives here so that
		// the tag list, the docs and the tests stay in one place).
		const std::vector<std::string>& BespokeKitTags();
	}
}
