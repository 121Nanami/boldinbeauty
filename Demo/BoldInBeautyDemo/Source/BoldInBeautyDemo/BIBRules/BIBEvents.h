// Boldinbeauty demo — simple recurring events and the Ark shop (owner request 2026-10-07: "add a few more simple events ...
// that fit the bb style"). Engine-independent. Names, missions, prices and amounts are PROPOSED / PLACEHOLDER.
//
// The event calendar follows the game day (Profile.Day, which follows the UTC day and the demo's "Next day" tool), in 7-day
// weeks like the weekly quests:
//   * Rookie week     - the first 7 days of a new account: 3 missions unlock each day (classic gacha beginner missions).
//   * Weekly operation - every week; the theme alternates: "Angel Hunt" (combat missions) and "Arcade Fever" (arcade missions
//                        and +2 daily Arcade tokens). Missions pay Event tokens for the event shop (weekly limits).
//   * Overtime weekend - days 6-7 of every week: the Outer World produces twice as fast and boosts pay 50% more.
//   * Element Surge    - every week one element (rotating): campaign clears with an operator of that element in the squad
//                        pay Element cores, and that element's Attunement costs 30% fewer cores.
//   * Ark shop         - Ark medals (arena fights, tower floors) buy Element cores, boosts, Charge and a ticket; monthly limits.
#pragma once

#include <string>

#include "BIBElement.h"
#include "BIBMeta.h"

namespace BIB
{
	class FProfile;
	struct FActionResult;

	namespace Events
	{
		enum class EStat : int
		{
			StageClear = 0,
			Summon,
			LevelUp,
			ArcadePlay,
			WorldCollect,
			ArkPlay,
			DailyDone,
			BoostUse,
			Attune,
			LoginBonus,
			Count
		};
		static_assert(static_cast<int>(EStat::Count) == EventStatCount, "BIBMeta.h EventStatCount must match EStat");

		// Counts an action for the weekly operation and the Rookie week (Meta::NotifyQuest forwards its events here).
		void Notify(FProfile& Profile, EStat Stat, int Amount = 1);
		// Starts a new week's counters when the week changed (called by Notify, the pages and the daily reset).
		void Refresh(FProfile& Profile);

		int WeekIndex(const FProfile& Profile);      // (Day - 1) / 7
		int DayOfWeek(const FProfile& Profile);      // 1..7
		int DaysLeftInWeek(const FProfile& Profile); // 1..7

		// ------------------------------------------------------------------ Rookie week
		constexpr int RookieDays = 7;
		void StartRookie(FProfile& Profile);         // new accounts (FProfile::StartNewPlayer)
		bool RookieActive(const FProfile& Profile);
		int RookieDay(const FProfile& Profile);      // 1..7 while active, 0 otherwise

		struct FMission
		{
			std::string Title;
			EStat Stat = EStat::StageClear;
			int Target = 1;
			int Progress = 0;
			bool bClaimed = false;
			bool bComplete = false;
			bool bLocked = false;    // Rookie week: the day has not come yet
			int UnlockDay = 1;
			FReward Reward;
		};
		FMission RookieMission(const FProfile& Profile, int Index);   // 0..RookieMissionCount-1 (3 per day)
		FActionResult ClaimRookie(FProfile& Profile, int Index);
		FReward RookieTotal();                                       // everything the Rookie week pays

		// ------------------------------------------------------------------ Weekly operation
		enum class ETheme : int
		{
			AngelHunt = 0,
			ArcadeFever
		};
		ETheme Theme(const FProfile& Profile);
		const char* ThemeName(ETheme Theme);
		std::string ThemeBlurb(ETheme Theme);
		FMission Mission(const FProfile& Profile, int Index);        // 0..EventMissionCount-1
		FActionResult ClaimMission(FProfile& Profile, int Index);
		int TokensPerWeek();                                         // every mission's tokens added up
		constexpr int ArcadeFeverTokens = 2;                         // extra daily Arcade tokens during Arcade Fever

		struct FShopItem
		{
			std::string Name;
			std::string Icon;
			int Price = 0;
			int Limit = 1;       // per week (event shop) / per month (Ark shop)
			FReward Reward;
		};
		const FShopItem& EventShopItem(int Index);                   // 0..EventShopCount-1
		int EventShopLeft(const FProfile& Profile, int Index);
		FActionResult BuyEventShop(FProfile& Profile, int Index);

		// ------------------------------------------------------------------ Overtime weekend
		bool OvertimeActive(const FProfile& Profile);
		constexpr double OvertimeWorldMultiplier = 2.0;
		constexpr double OvertimeBoostMultiplier = 1.5;
		double WorldMultiplier(const FProfile& Profile);             // 2.0 on Overtime days, 1.0 otherwise
		double BoostMultiplier(const FProfile& Profile);

		// ------------------------------------------------------------------ Element Surge
		bool SurgeActive(const FProfile& Profile);
		EElement SurgeElement(const FProfile& Profile);
		constexpr int SurgeDiscountPercent = 30;
		constexpr int SurgeCoresPerClear = 3;
		constexpr int SurgeCoresFirstClear = 6;
		// Element cores a campaign clear pays during the Surge (0 when no squad member has the element).
		int SurgeCoresFor(const FProfile& Profile, int StageId, bool bFirstClear);

		// ------------------------------------------------------------------ Ark shop and Ark medals
		const FShopItem& ArkShopItem(int Index);                     // 0..ArkShopCount-1
		int ArkShopLeft(const FProfile& Profile, int Index);
		FActionResult BuyArkShop(FProfile& Profile, int Index);
		int ArkMedalsForArena(bool bWon);                            // 30 for a win, 10 for a loss
		int ArkMedalsForFloor(int Floor);                            // first clear of a tower floor

		// Everything currently running, for the Events page: title + one line each.
		int ActiveCount(const FProfile& Profile);

		// ------------------------------------------------------------------ Ticket road (owner request 2026-10-09)
		// "a consistent mean for users to get the maybe 1000s of tickets needed to obtain secrets it can take users a year or 2 to
		// get them all but make it possible". A long, steady, time-gated ticket income that opens when the campaign is finished
		// (the campaign's balance tables are solved without it). Nothing here changes rates, pity, Mileage, prices or the Daily
		// ticket (MAIN §2); it only pays Special and Regular tickets on a daily / weekly / every-20-days rhythm:
		//   * daily stipend  - once a game day, after claiming RoadDailyQuests daily quests: RoadDailySpecial Special tickets;
		//   * weekly bonus   - RoadWeeklyDays stipends inside one 7-day week: RoadWeeklyRegular Regular tickets (Standard banner,
		//                      whose 2,000-roll pity never rotates away);
		//   * road milestone - every RoadMilestoneDays stipends (endless): RoadMilestoneSpecial Special tickets.
		// It cannot be farmed: every part is counted in stipend days and one stipend exists per game day. All amounts PLACEHOLDER
		// (balanced so a daily free player owns all six Secrets in about 18 months: Tests/playthrough.cpp, report
		// reports/demo/2026-10-09-secret-ticket-income.md).
		constexpr int RoadDailyQuests = 5;        // daily quests claimed today (of 8; the daily bonus quest counts)
		constexpr int RoadDailySpecial = 5;
		constexpr int RoadWeeklyDays = 5;
		constexpr int RoadWeeklyRegular = 10;
		constexpr int RoadMilestoneDays = 20;
		constexpr int RoadMilestoneSpecial = 20;

		struct FRoad
		{
			bool bOpen = false;              // the campaign is finished (Chapter 13 boss cleared)
			int DailyQuestsDone = 0;         // daily quests claimed today
			bool bDailyClaimed = false;      // today's stipend taken
			bool bDailyReady = false;        // open, enough daily quests, not taken
			int WeekDays = 0;                // stipends this week
			bool bWeeklyClaimed = false;
			bool bWeeklyReady = false;
			int TotalDays = 0;               // stipends ever (the road's length)
			int MilestonesClaimed = 0;
			int NextMilestoneDay = RoadMilestoneDays;   // TotalDays the next milestone needs
			bool bMilestoneReady = false;
			int TicketsEarned = 0;           // every ticket the road has paid
		};
		bool RoadOpen(const FProfile& Profile);
		FRoad Road(const FProfile& Profile);
		int RoadClaimable(const FProfile& Profile);           // parts ready to claim (0..3)
		FReward RoadDailyReward();
		FReward RoadWeeklyReward();
		FReward RoadMilestoneReward();
		// Claims every part that is ready (stipend first, so it can complete the weekly bonus and a milestone the same day).
		FActionResult ClaimRoad(FProfile& Profile);
		// Tickets the road pays in 30 days to a player who claims every day (for the page): 150 + 42.9 + 30 = ~222.9.
		double RoadTicketsPer30Days();
	}
}
