// Boldinbeauty demo — the hub pages added on 2026-10-06 (owner request): arcade (Night Shift, Boss Rush, Rhythm), the Ark
// arena and endless tower, friends, gangs and the gang shop, notices, the admin mail / notice desk, the welcome-back event,
// and the reworked Settings (gameplay, accessibility, key bindings, lobby customisation). BIBPages.cpp calls in here first.
#pragma once

#include <string>
#include <vector>

#include "BIBPages.h"

namespace BIB
{
	namespace Pages
	{
		namespace Extra
		{
			// The Lobby root's tiles.
			void LobbyRoot(FProfile& Profile, const FUiState& State, FPage& Page);
			// Builds the page when it is one of the new ones (returns false otherwise).
			bool BuildLobbyPage(FProfile& Profile, const FUiState& State, FPage& Page);
			bool BuildRevolutionPage(FProfile& Profile, const FUiState& State, FPage& Page);
			// Runs the action when it belongs to the new pages (returns false otherwise; Out is untouched then).
			bool Perform(FProfile& Profile, FUiState& State, const std::string& Id, FPerform& Out);
			// Parent of a new page ("" = the tab root); bFound is false when the page is not a new one.
			std::string ParentPage(const std::string& Page, bool& bFound);
			// Settings > Gameplay "confirm Diamond spending": how many Diamonds the action would spend (0 = none).
			long long DiamondCost(const FProfile& Profile, const FUiState& State, const std::string& Id);
			// The requests the hub sends when a signed-in session starts (profile, defence, mail, notices, gang, friends, arena).
			std::vector<Online::FCall> SessionCalls(const FProfile& Profile);
		}
	}
}
