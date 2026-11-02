// Boldinbeauty demo — live operations driven by the real clock (owner request 2026-10-06). Engine-independent.
//
//   * Seasons are UTC calendar months: the season pass (it lasts one month; a player who finishes every daily and weekly
//     quest reaches tier 50 in about 15 days), the arena season and the gang season.
//   * Daily reset follows the UTC day (the demo's "Next day" tool still works on top of it).
//   * Returning players: away 14 days or more -> a 7-day welcome-back event with a 3-reward check-in every day and six
//     "Returned quests" worth 5 Regular tickets, 600 Diamonds and some loot in total.
//   * The Outer World keeps producing while the game is closed (up to its storage limit).
#pragma once

#include <cstdint>
#include <string>

#include "BIBMeta.h"

namespace BIB
{
	class FProfile;
	struct FActionResult;

	namespace LiveOps
	{
		// ------------------------------------------------------------------- calendar
		int UtcDayIndex(int64_t UnixSeconds);              // days since 1970-01-01
		int SeasonKeyOf(int64_t UnixSeconds);              // year * 12 + month - 1 (UTC)
		std::string SeasonLabel(int SeasonKey);            // "October 2026"
		std::string SeasonCode(int SeasonKey);             // "2026-10" (the server's season key)
		int SeasonKeyFromCode(const std::string& Code);    // 0 when not a "YYYY-MM"
		int64_t SeasonStartUnix(int SeasonKey);
		int64_t SeasonEndUnix(int SeasonKey);              // start of the next month
		int DaysLeftInSeason(int64_t UnixSeconds);

		// Called every second by the game (and once after loading): applies day changes, season changes, the returning-player
		// check and offline Outer World production. Returns lines worth telling the player (empty most of the time).
		std::vector<std::string> UpdateClock(FProfile& Profile, int64_t NowUnix);

		// ------------------------------------------------------------------- season pass
		int SeasonPassDaysLeft(const FProfile& Profile);

		// ------------------------------------------------------------------- returning players
		bool ReturnActive(const FProfile& Profile);
		int ReturnDay(const FProfile& Profile);            // 1..7 while active, 0 otherwise
		int64_t ReturnSecondsLeft(const FProfile& Profile);
		FReward ReturnCheckInReward(int Day);              // three rewards per day (PLACEHOLDER amounts)
		bool ReturnCheckInClaimed(const FProfile& Profile, int Day);
		FActionResult ClaimReturnCheckIn(FProfile& Profile, int Day);
		int ClaimableReturn(const FProfile& Profile);      // check-in days + quests ready

		struct FReturnQuest
		{
			std::string Title;
			int Target = 1;
			int Progress = 0;
			bool bClaimed = false;
			bool bComplete = false;
			FReward Reward;
		};
		FReturnQuest ReturnQuest(const FProfile& Profile, int Index);   // 0..ReturnQuestCount-1
		FActionResult ClaimReturnQuest(FProfile& Profile, int Index);
		FReward ReturnQuestTotal();                        // the six rewards added up: 5 Regular tickets, 600 Diamonds, loot
		// Starts the event now (the tester panel and the tests use it; UpdateClock starts it after 14 days away).
		void StartReturn(FProfile& Profile, int64_t NowUnix);
	}
}
