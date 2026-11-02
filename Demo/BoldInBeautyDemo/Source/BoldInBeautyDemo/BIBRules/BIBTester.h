// Boldinbeauty demo — tester tools (owner request 2026-10-05): every player of this build is a tester and can set currencies,
// operator ownership / levels / upgrade stars, campaign progress, gear, XP, the day, the tutorial and the summon pity counters.
// Pure rules on FProfile, so the Lobby > Tester panel (BIBPages.cpp) and the unit tests share them. Engine-independent.
//
// Nothing here is a game-design decision: the presets are shortcuts to states a tester wants to start from, not a statement of
// how far a player "should" be at any point. The admin account is unchanged by this file (its wallet stays unlimited).
#pragma once

#include <string>

#include "BIBCore.h"

namespace BIB
{
	namespace Tester
	{
		enum EMode : int
		{
			ModeAdd = 0,
			ModeRemove,
			ModeSet,
			ModeCount
		};

		constexpr int StepCount = 8;                  // amount presets: 1, 10, 100, 1,000, 10,000, 100,000, 1,000,000, 100,000,000
		constexpr int MaxAmount = 2000000000;         // the save system's ceiling for a stored amount (BIBSave.cpp)
		constexpr int MaxOperatorLevel = 200;         // above the base cap of 100 the upgrade stars are raised to allow it
		constexpr int GearPercentSteps = 5;           // gear bonus presets: 1..5 %
		constexpr int PresetCount = 5;
		constexpr int WalletPresetCount = 4;

		bool Enabled();                               // Config::TesterMode
		long long StepAmount(int StepIndex);
		const char* ModeName(int Mode);
		double GearPercent(int Index);

		// ------------------------------------------------------------ currencies
		// Adds, removes or sets one currency. Refused for an account whose wallet is unlimited (admin).
		FActionResult ApplyCurrency(FProfile& Profile, ECurrency Currency, int Mode, long long Amount);
		// 0 empty, 1 the starting wallet of a new profile, 2 rich, 3 very rich. Refused for an unlimited wallet.
		FActionResult WalletPreset(FProfile& Profile, int Preset);
		const char* WalletPresetName(int Preset);

		// ------------------------------------------------------------- operators
		FActionResult SetOwned(FProfile& Profile, int RosterIndex, bool bOwned);
		// Clamps to 1..MaxOperatorLevel. A level above the cap raises the upgrade stars so the cap allows it. Heart gates below
		// the level count as paid; a level on a multiple of 20 leaves that gate due, as in normal play.
		FActionResult SetLevel(FProfile& Profile, int RosterIndex, int Level);
		FActionResult SetStars(FProfile& Profile, int RosterIndex, int Stars);
		FActionResult SetDuplicates(FProfile& Profile, int RosterIndex, int Mode, long long Amount);
		FActionResult SetGatePaid(FProfile& Profile, int RosterIndex, bool bPaid);
		void UnlockAllOperators(FProfile& Profile);
		void LockAllOperators(FProfile& Profile);                       // keeps Luna; empties the squad slots that lose their operator
		void SetAllLevels(FProfile& Profile, int Level);                // owned operators only
		void SetAllStars(FProfile& Profile, int Stars);                 // owned operators only

		// -------------------------------------------------------------- campaign
		void SetCampaignProgress(FProfile& Profile, int LevelsCleared); // 0..104; does not grant the Chapter 1 boss reward
		void SetTowerBest(FProfile& Profile, int Mode, int Floor);
		void SetSubStories(FProfile& Profile, bool bAllCleared);

		// ------------------------------------------------------------------ gear
		FEquipment& GrantGear(FProfile& Profile, ESlot Slot, EBonus Bonus, double Percent);
		void SetAllGearUpgrade(FProfile& Profile, int Level);           // 0..Config::EquipMaxUpgrade
		void ClearGear(FProfile& Profile);
		// Puts the best unworn piece of each slot on every squad member.
		int EquipBestOnTeam(FProfile& Profile);

		// ----------------------------------------------------------- progression
		FActionResult SetCommanderXp(FProfile& Profile, int Mode, long long Amount);
		void SetCommanderLevel(FProfile& Profile, int Level);
		FActionResult SetSeasonXp(FProfile& Profile, int Mode, long long Amount);
		void SetSeasonTier(FProfile& Profile, int Tier);
		void SetArenaPoints(FProfile& Profile, int Points);
		void SetBuildings(FProfile& Profile, int Level);                // 0 = none, up to Meta::MaxBuildingLevel
		void SetPity(FProfile& Profile, int BannerIndex, int Since5, int Since6, bool bLostFiftyFifty);
		// Ends the tutorial the way finishing it would: Kuu summoned, Plack unlocked, squad Luna / Kuu / Plack, level 1 cleared.
		void SkipTutorial(FProfile& Profile);

		// --------------------------------------------------------------- presets
		// A preset starts from a fresh profile of the same kind (ResetForAccount), skips the tutorial and then sets everything
		// below. 0 new player (tutorial pending), 1 early, 2 middle, 3 late, 4 everything unlocked.
		void ApplyPreset(FProfile& Profile, int Preset);
		const char* PresetName(int Preset);
		const char* PresetNote(int Preset);
	}
}
