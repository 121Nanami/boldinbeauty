// Boldinbeauty demo — the Ark as the hub (owner request 2026-10-07): the 2D lobby is gone. The player walks the Ark; the only
// thing on screen is a three-line menu button in the top right. The menu is a flow chart (Menu > The Ark > Town Square >
// Armoury ...): every click opens the next column, except the leaves, which either teleport the player to a place in the Ark
// (merchants, arcade, factory, armoury ...) or open a personal page straight away (mail, profile, settings ...).
//
// Every place ("station") is a real 3D structure in L_Ark (Tools/greybox_ark_hub.py builds it and tags an anchor with the
// station id). Walking up to it and pressing F (or T) opens its counters: the same engine-independent pages as before
// (BIBPages.h), so every rule, price and tutorial step is unchanged. This file is the station table and the menu tree as data,
// unit-tested in Tests/ark_tests.cpp. Engine-independent.
//
// Placement and grouping are the assistant's greybox reading (PROPOSED, OPEN for the owner): which page lives in which
// building, the Operator Generator Factory on the empty north side of the operators' platform, Merchant Row, the arcade hall,
// the training grounds and the Back Alley stall on the Underworld trade floor.
#pragma once

#include <string>
#include <vector>

#include "BIBCore.h"
#include "BIBPages.h"

namespace BIB
{
	namespace Ark
	{
		// One page a station opens. The panel switches between a station's counters with a tab strip.
		struct FCounter
		{
			std::string Label;
			int Tab = Pages::TabLobby;
			std::string Page;      // "" = the tab's root page
			std::string Icon;
		};

		struct FStation
		{
			std::string Id;        // also the tag on the anchor actor in L_Ark ("BIBStation" + Id)
			std::string Name;
			std::string Area;      // area node id in the menu ("operators", "town", "underworld")
			std::string Blurb;     // one line under the name
			std::string Icon;
			float Colour[3] = {1.f, 1.f, 1.f};   // the station's accent (hologram, ring, panel header)
			bool bMerchant = false;
			bool bUnmarked = false;  // no sign over it in the world (task room, resistance): the menu still knows it
			std::vector<FCounter> Counters;
		};

		const std::vector<FStation>& Stations();
		const FStation* FindStation(const std::string& Id);

		// Opens a counter the way the old bottom tab + tile did: Perform("tab:<n>") (so tutorial steps still count), then the page.
		// Pages whose id holds a ':' ("op:equip") cannot travel through "go:<page>", so they are set directly.
		Pages::FPerform OpenCounter(FProfile& Profile, Pages::FUiState& State, const FCounter& Counter);

		// The station that hosts a bottom tab's root page (tutorial: "open the Summon tab" -> walk to the factory).
		std::string StationForTab(int Tab);
		// The station whose counter shows this page, "" when the page is personal (menu only).
		std::string StationForPage(int Tab, const std::string& Page);

		// ------------------------------------------------------------------ the flow-chart menu
		enum class ENodeKind : int
		{
			Branch = 0,   // opens the next column
			Teleport,     // moves the player to a station (Station)
			Page,         // opens a page in the panel without moving (Tab, Page)
			Action        // runs one page action (Action), e.g. free roam in Neo Tokyo or sign out
		};

		struct FNode
		{
			std::string Id;
			std::string Label;
			std::string Sub;
			std::string Icon;
			ENodeKind Kind = ENodeKind::Branch;
			std::string Station;
			int Tab = Pages::TabLobby;
			std::string Page;
			std::string Action;
			bool bBadge = false;   // something to claim behind it
			bool bEnabled = true;
			bool bHighlight = false;   // the tutorial points here
			float Colour[3] = {0.55f, 0.75f, 1.f};
		};

		constexpr const char* RootId = "menu";

		// The node with this id (badges, counts and tutorial highlights filled in from the profile). Id "" or unknown = the root.
		FNode Node(FProfile& Profile, const Pages::FUiState& State, const std::string& Id);
		// The children of a branch node, in display order (empty for a leaf).
		std::vector<FNode> Children(FProfile& Profile, const Pages::FUiState& State, const std::string& Id);
		// "menu" -> "the-ark" -> "town" -> "armoury": the path from the root to a node (empty when the id is unknown).
		std::vector<std::string> PathTo(const std::string& Id);
		// Every node id in the tree (tests and the verify script walk it).
		std::vector<std::string> AllNodeIds();

		// Tutorial: the menu path the current step wants (empty when the tutorial is over or the step needs no menu).
		std::vector<std::string> TutorialPath(const FProfile& Profile);
		// One line for the HUD while the tutorial runs ("" when it is over).
		std::string TutorialLine(const FProfile& Profile);
	}
}
