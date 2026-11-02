// Boldinbeauty demo — the hub UI as data. Seven tabs (Operator, Team, Lobby, Shop, Revolution, Campaign, Summon)
// are described as pages: a title, some text lines, a grid of tiles and a bar of buttons. The Unreal widget
// (the Ark panels, BIBArkUI; the 2D lobby UBIBHubWidget was deleted on 2026-10-07) and the tests both read this model, so all menu behaviour is unit-tested without the engine.
// Engine-independent; every price, reward and name behind it is a PLACEHOLDER (see BIBMetaOps.h).
#pragma once

#include <string>
#include <vector>

#include "BIBArena.h"
#include "BIBCore.h"
#include "BIBOnline.h"

namespace BIB
{
	namespace Pages
	{
		enum ETab : int
		{
			TabOperator = 0,
			TabTeam,
			TabLobby,
			TabShop,
			TabRevolution,
			TabCampaign,
			TabSummon,
			TabCount
		};

		constexpr int MaxGrid = 24;
		constexpr int MaxBar = 6;

		const char* TabName(int Tab);
		const char* TabIcon(int Tab);

		struct FUiState
		{
			int Tab = TabLobby;
			std::string Page;           // "" = the tab's root page
			std::string Status;         // last action message
			int SelOperator = 5;        // roster index
			int SelSlot = 0;            // equipment slot (ESlot)
			int SelItem = -1;           // equipment id
			int SelBanner = 0;
			int SelSubStory = 0;
			int SelChapter = 1;         // campaign chapter shown on the chapter page (1..13)
			int TradeFrom = 0;          // index into Meta::TradeCurrencies()
			int TradeTo = 1;
			int TradeAmount = 0;        // index into the amount presets
			int PassPage = 0;           // season pass page, 10 tiers each
			int QuestTab = 0;           // Quests page: Meta::EQuestList (Daily, Weekly, Story, Milestones)
			int SettingsTab = 0;        // Settings page: Audio, Graphics, Controls, Interface, Account
			int ListOffset = 0;         // paging for long grids
			int TesterStep = 3;         // Tester panel: index into Tester::StepAmount (1,000)
			int TesterMode = 0;         // Tester panel: Tester::EMode (add / remove / set)
			int TesterGear = 3;         // Tester panel: gear bonus preset index (4 %)

			// Owner request 2026-10-06 (arcade, arena, friends, gangs, mail, notices, preferences): BIBPagesExtra.cpp
			Online::FCache Net;         // what the account server last said (friends, gang, notices, arena)
			std::string InputText;      // text typed into the last prompt (FPerform::Prompt), or the key captured
			int SelNotice = -1;
			int SelFriend = -1;         // index into Net.Friends
			int SelMember = -1;         // index into Net.Gang.MemberList
			int KeyGroup = 0;           // key bindings page: 0 battle, 1 Night Shift, 2 Rhythm
			bool bArenaDefence = false; // arena team page edits the defence team
			int ArenaSlot = 0;
			int NightDiff = 0;
			int RhythmDiff = 0;
			std::string ConfirmId;      // the Diamond-spending action waiting for a second tap (Settings > Gameplay)
			Arena::FBattle LastBattle;  // the arena fight the overlay replays
			Arena::FLineup LastAttack;
			Arena::FLineup LastDefence;

			// last summon result (shown instead of the banner list until dismissed)
			std::vector<int> SummonOps;
			std::vector<int> SummonStars;
			std::vector<char> SummonNew;
			int SummonCount = 0;
			int SummonPay = 0;
		};

		struct FPageAction
		{
			std::string Id;       // opaque; pass to Perform()
			std::string Label;
			std::string Sub;      // small second line (cost, state)
			std::string Icon;     // icon id (Art/Icons/<id>.png) or "portrait:<name>" for an operator head
			bool bEnabled = true;
			bool bHighlight = false;
			bool bBadge = false;  // "something to claim"
		};

		struct FPage
		{
			int Tab = TabLobby;
			std::string Title;
			std::string Subtitle;
			std::vector<std::string> Lines;
			std::vector<FPageAction> Grid;  // up to MaxGrid tiles
			std::vector<FPageAction> Bar;   // up to MaxBar buttons
			std::string Portrait;           // operator name (lower case) for the large portrait, "" = none
			std::string PortraitStars;      // lower third, e.g. "5* Legendary"
			std::string PortraitName;       // lower third, e.g. "Judge"
			bool bCanBack = false;
			bool bFullScreen = false;       // a sub-page: shown full screen (bottom tabs hidden unless the tutorial points at one)
			std::string Status;
			// Tutorial (BIBTutorial.h): the instruction, the bottom tab to point at, and the tabs that are locked for now.
			std::string Coach;
			int NavHighlight = -1;
			bool NavLocked[TabCount] = {};
		};

		// What the engine layer opens on top of the hub (BIBGame/BIBArcadeOverlay).
		enum class EOverlay : int
		{
			None = 0,
			NightShift,   // A = difficulty
			Rhythm,       // A = song, B = difficulty
			ArenaBattle,  // replays FUiState::LastBattle
			HaloFall,     // the 2026-10-07 mini-games (BIBMiniGames.h): A = difficulty
			NeonMemory,
			FirewallBreach,
			ArkRally,      // 2026-10-15 (BIBRally.h): A = difficulty, B = 1 for the player's own track (free, no reward)
			UltimateNight   // Ultimate Custom Night (BIBUltimateNight.h, 2026-10-14): A = mode (Easy, Medium, Hard, Angel)
		};

		struct FPrompt
		{
			std::string Key;      // the answer comes back as Perform("in:<Key>") with FUiState::InputText set
			std::string Title;
			std::string Hint;
			std::string Initial;
			int MaxLength = 60;
		};

		struct FPerform
		{
			std::string Message;
			bool bOk = true;
			bool bStartStage = false;
			int StageId = 0;
			std::string FreeRoamMap;  // not empty: open this level map for free roam ("NeoTokyo"; the engine layer travels)
			bool bSignOut = false;   // the player asked to sign out (the engine layer handles it)
			std::vector<Online::FCall> Calls;   // account-server requests to send (UBIBOnlineSubsystem)
			FPrompt Prompt;                     // Key not empty: ask the player for a line of text
			int CaptureKey = -1;                // >= 0: wait for the next key press and bind it (FSettings::KeyBinds index)
			EOverlay Overlay = EOverlay::None;
			int OverlayA = 0;
			int OverlayB = 0;
		};

		struct FResourceView
		{
			std::string Icon;
			std::string Label;
			long long Amount = 0;
		};

		FPage Build(FProfile& Profile, const FUiState& State);
		FPerform Perform(FProfile& Profile, FUiState& State, const std::string& ActionId);

		std::vector<FResourceView> Resources(const FProfile& Profile);   // the six top-bar currencies the player chose
		const char* CurrencyIcon(ECurrency Currency);
		int UnreadNotices(const FProfile& Profile, const FUiState& State);
		std::string CommanderLine(const FProfile& Profile);
		std::string Duration(long long Seconds);   // "2d 13h", "5h 12m", "3m 05s"
		std::string LowerName(int RosterIndex);
		int BannerShowcase(const FProfile& Profile, int BannerIndex);
	}
}
