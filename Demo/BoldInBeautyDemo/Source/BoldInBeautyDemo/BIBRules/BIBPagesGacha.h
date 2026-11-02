// Boldinbeauty demo — hub pages added on 2026-10-07 (owner request: classic gacha rebalance, inventory with more currencies and
// Outer World boosts, the element system, simple events): Lobby > Inventory, Lobby > Events (Rookie week, weekly operation,
// event shop, Overtime weekend, Element Surge), Lobby > Elements, Revolution > The Ark > Ark shop, and the element / Attunement
// lines on the operator, team and campaign pages. BIBPages.cpp calls in here first. Engine-independent.
#pragma once

#include <string>

#include "BIBPages.h"

namespace BIB
{
	namespace Pages
	{
		namespace Gacha
		{
			// Extra Lobby root tiles (Inventory, Events badge, Rookie week).
			void LobbyTiles(FProfile& Profile, FPage& Page);
			// Builds the page when it is one of the new ones (returns false otherwise).
			bool BuildLobbyPage(FProfile& Profile, const FUiState& State, FPage& Page);
			bool BuildRevolutionPage(FProfile& Profile, const FUiState& State, FPage& Page);
			// Runs the action when it belongs to the new pages (ids start with "gx:"; returns false otherwise).
			bool Perform(FProfile& Profile, FUiState& State, const std::string& Id, FPerform& Out);
			// Parent of a new page; bFound is false when the page is not a new one.
			std::string ParentPage(const std::string& Page, bool& bFound);
			// Diamond cost of a "gx:" action (Settings > Gameplay confirmation), 0 = none.
			long long DiamondCost(const FProfile& Profile, const std::string& Id);

			// Lines and tiles added to existing pages.
			std::string ElementTag(int RosterIndex);                       // "[Fire]"
			void OperatorElement(const FProfile& Profile, int RosterIndex, FPage& Page);   // element line + Attune tile
			std::string StageElementLine(const FProfile& Profile, int StageId);           // campaign / tower: enemy element and counter
			std::string ChargeLine(const FProfile& Profile);
			int ClaimableEvents(const FProfile& Profile);
		}
	}
}
