// Headless whole-game playthroughs (owner request 2026-10-05: "go throughout the whole game many times for testing and
// balancing"). A bot plays the real rules (FProfile, Meta::ClearStage, summons, levelling, Angel-heart gates, equipment,
// quests, mail, the Outer World) from a fresh guest account to the end of the 13-chapter campaign, many times, with different
// amounts of play time per day and different summon luck. It reports pacing, grind walls and soft-locks.
//
// It does NOT fight: whether the team beats a stage is decided the way the campaign tables do it (the team must reach the
// stage's recommended level, BIB::Campaign::RecommendedLevel). Minutes per run, the bot's play time per day and its spending
// habits are PLACEHOLDER assumptions of this file, not game design. What it checks is the economy as the shipped code runs it:
// that no stage is unreachable, that the level cap and Angel hearts do not stop the player, and where the grind is.
//
//   playthrough               table of pacing for every play style (also fails on a soft-lock or a broken invariant)
//   playthrough --stages      adds per-stage replay counts for the "regular" style
//   playthrough --seeds N     number of runs per style (default 60)
//   playthrough --secret-days N  days the bot keeps playing after the campaign to collect every Secret (default 1100, 0 = off)
//   playthrough --no-road     the same without the Ticket road (2026-10-09), to measure the older free income
//
// After the campaign (owner request 2026-10-09: "a consistent mean for users to get the maybe 1000s of tickets needed to
// obtain secrets it can take users a year or 2 to get them all but make it possible") the bot keeps playing on the real UTC
// clock (featured banners rotate every 3 days, seasons and the arena reset monthly) as a Secret hunter: it does its daily
// quests, 5 arena fights, the events and the Ticket road, and spends Special tickets, Diamonds and Mileage only while a
// Secret it does not own is featured (Regular tickets go to the Standard banner, whose 2,000-roll pity never rotates away).
// It reports the free pulls a day and the day the sixth Secret arrives. Arcade, Ark Tower and gang rewards are NOT counted
// (skill / server rewards), so the income it reports is a floor.
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArena.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCampaign.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBEvents.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBInventory.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBLiveOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBTester.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
	using namespace BIB;

	struct FStyle
	{
		const char* Name;
		int MinutesPerDay;     // play time
		bool bDailyExtras;     // login bonus, mail, quests, season pass, Outer World, daily ticket
		bool bSpendDiamonds;   // summons with Diamonds when it can afford a x10
		int ChargeRefills;     // Diamond Charge refills a day (2026-10-07: replays cost Charge)
		int PostDaysPerWeek;   // days a week it logs in after the campaign (2026-10-09 Secret hunt)
	};

	const FStyle Styles[] = {
		{"casual    (45 min/day, extras)", 45, true, true, 0, 7},
		{"regular   (120 min/day, extras)", 120, true, true, 0, 7},
		{"hardcore  (300 min/day, extras, 3 refills)", 300, true, true, 3, 7},
		{"no extras (120 min/day, stages only)", 120, false, true, 0, 7},
		{"saver     (120 min/day, keeps Diamonds)", 120, true, false, 0, 7},
		{"weekender (regular; after: 3 days a week)", 120, true, true, 0, 3},
	};
	constexpr int RegularStyle = 1;

	struct FOptions
	{
		int SecretDays = 1100;   // post-campaign days simulated (0 = no Secret hunt)
		bool bRoad = true;
	};
	FOptions GOptions;

	// Summon value of a wallet: tickets + Diamonds / 300 (Daily tickets excluded: no pity, no Mileage).
	double PullValue(const FProfile& Profile)
	{
		return Profile.Get(ECurrency::RegularTicket) + Profile.Get(ECurrency::SpecialTicket) +
			static_cast<double>(Profile.Get(ECurrency::Diamonds)) / Config::DiamondsPerRoll;
	}

	int SecretCount()
	{
		int Count = 0;
		for (const FRosterEntry& Def : Roster())
		{
			Count += Def.Stars == 6 ? 1 : 0;
		}
		return Count;
	}

	int SecretsOwned(const FProfile& Profile)
	{
		int Count = 0;
		for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
		{
			Count += Roster()[static_cast<size_t>(Index)].Stars == 6 && Profile.Owns(Index) ? 1 : 0;
		}
		return Count;
	}

	// PLACEHOLDER: how long one run of a stage takes (travel, fights, talking).
	double RunMinutes(int StageId)
	{
		if (Campaign::IsWalkStage(StageId))
		{
			return 5.0;   // Return to Nowhere: a five-minute walk
		}
		int Enemies = 0;
		for (const FSectionPlanData& Section : StagePlan(StageId))
		{
			Enemies += Section.Total();
		}
		return 1.5 + 0.10 * Enemies;
	}

	struct FRun
	{
		bool bFinished = false;
		int Days = 0;
		int Runs = 0;
		int FirstClears = 0;
		int Replays = 0;
		double Minutes = 0.0;
		int StuckAt = 0;                 // stage the bot could not reach
		std::string StuckWhy;
		int Summons = 0;
		int OperatorsOwned = 0;
		int StarsTotal = 0;
		int FiveStarOrBetter = 0;
		int LevelBlockedByHearts = 0;    // days the next level was blocked only by Angel hearts
		int LevelBlockedByCap = 0;       // days a team member sat at the level cap while below what the stage wants
		long long LeftGold = 0;
		long long LeftBytes = 0;
		long long LeftHearts = 0;
		long long LeftDiamonds = 0;
		int ChapterDay[CampaignChapters + 1] = {};
		int ChargeDays = 0;              // days the bot stopped farming because it ran out of Charge
		std::vector<int> ReplaysAtStage;
		long long Earned[3] = {0, 0, 0};     // Gold, Bytes, Angel hearts from everything except summons
		long long Spent[3] = {0, 0, 0};      // spent on levels, gates and Outer World
		double FinalTeamLevel = 0.0;
		int FinalNeedLevel = 0;
		int TeamCapAtEnd = 0;
		// Secret hunt (2026-10-09)
		double CampaignPulls = 0.0;      // summon value earned during the campaign (tickets + Diamonds / 300)
		double PostPullsOld = 0.0;       // after the campaign, without the Ticket road
		double PostPullsRoad = 0.0;      // the Ticket road's tickets
		int PostDays = 0;                // calendar days simulated after the campaign
		int PostPlayDays = 0;            // of which the bot logged in
		int SecretsAtCampaignEnd = 0;
		int SecretDay = 0;               // account day the last Secret arrived (0 = not within the simulated days)
		int SecretsAtEnd = 0;
		int SecretsFromMileage = 0;
		int PullsMade = 0;               // non-Daily summon results over the whole account
		double SpentPulls = 0.0;         // summon value spent (to measure income)
	};

	double TeamAverage(const FProfile& Profile)
	{
		double Sum = 0.0;
		int Count = 0;
		for (int Member : Profile.Team)
		{
			if (Member >= 0)
			{
				Sum += Profile.Operators[static_cast<size_t>(Member)].Level;
				++Count;
			}
		}
		return Count > 0 ? Sum / Count : 0.0;
	}

	// Levels the squad evenly with everything it can pay, then buys upgrade stars from duplicate units where a star is the
	// thing standing in the way (the level cap). A member stuck at its cap or an Angel-heart gate is set aside and the others
	// keep levelling, like a player would (2026-10-09: the bot used to stop the whole squad, which turned one unlucky
	// duplicate roll into weeks of waiting). Returns the reason the last level-up failed ("" when it never failed).
	std::string LevelSquad(FProfile& Profile, FRun& Run, bool& bBlockedByHearts, bool& bBlockedByCap)
	{
		std::string Reason;
		std::vector<int> SetAside;
		auto IsSetAside = [&SetAside](int Member) { return std::find(SetAside.begin(), SetAside.end(), Member) != SetAside.end(); };
		for (int Guard = 0; Guard < 4000; ++Guard)
		{
			int Lowest = -1;
			for (int Member : Profile.Team)
			{
				if (Member >= 0 && !IsSetAside(Member) &&
					(Lowest < 0 || Profile.Operators[static_cast<size_t>(Member)].Level < Profile.Operators[static_cast<size_t>(Lowest)].Level))
				{
					Lowest = Member;
				}
			}
			if (Lowest < 0)
			{
				break;
			}
			FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(Lowest)];
			if (Owned.Level >= Profile.LevelCap(Lowest))
			{
				if (!Profile.TryUpgradeStar(Lowest).bOk)
				{
					bBlockedByCap = true;
					Reason = "cap";
					SetAside.push_back(Lowest);
				}
				continue;
			}
			if (Profile.NeedsHeartGate(Lowest))
			{
				if (!Profile.TryPayHeartGate(Lowest).bOk)
				{
					bBlockedByHearts = true;
					Reason = "hearts";
					SetAside.push_back(Lowest);
					continue;
				}
				continue;
			}
			const FActionResult Result = Profile.TryLevelUp(Lowest, false);
			if (!Result.bOk)
			{
				Reason = "currency";
				break;
			}
		}
		(void)Run;
		return Reason;
	}

	void SummonPolicy(FProfile& Profile, FRun& Run, const FStyle& Style)
	{
		auto Pull = [&](int Banner, int Count, EPay Pay)
		{
			const double Before = PullValue(Profile);
			const FSummonOutcome Outcome = Profile.Summon(Banner, Count, Pay);
			if (Outcome.bOk)
			{
				Run.Summons += static_cast<int>(Outcome.Results.size());
				Run.PullsMade += Banner == 3 ? 0 : static_cast<int>(Outcome.Results.size());
				Run.SpentPulls += Before - PullValue(Profile);
			}
			return Outcome.bOk;
		};
		// Daily ticket (one a day), then tickets, then Diamonds in tens.
		if (Profile.Get(ECurrency::DailyTicket) > 0)
		{
			Pull(3, 1, EPay::Ticket);
		}
		while (Profile.Get(ECurrency::RegularTicket) >= 10 && Pull(0, 10, EPay::Ticket)) {}
		while (Profile.Get(ECurrency::RegularTicket) >= 1 && Pull(0, 1, EPay::Ticket)) {}
		while (Profile.Get(ECurrency::SpecialTicket) >= 1 && Pull(1 + (Run.Summons % 2), 1, EPay::Ticket)) {}
		if (Style.bSpendDiamonds)
		{
			while (Profile.Get(ECurrency::Diamonds) >= Config::TenPullPaidRolls * Config::DiamondsPerRoll && Pull(1 + (Run.Summons / 10 % 2), 10, EPay::Diamonds)) {}
		}
		// Mileage: a featured 5-star at 250.
		for (int Banner = 1; Banner <= 2; ++Banner)
		{
			if (Profile.Get(ECurrency::Mileage) >= Config::MileageFeatured5Cost || Profile.Get(ECurrency::Mileage) >= Config::MileageFeatured6Cost)
			{
				Profile.RedeemMileage(Banner);
			}
		}
	}

	void DailyExtras(FProfile& Profile)
	{
		Meta::ClaimLogin(Profile);
		Meta::ClaimAllMail(Profile);
		Meta::ClaimAllQuests(Profile);   // daily, weekly, story and milestones
		Meta::ClaimAllSeason(Profile);
		// The Outer World runs on the real clock (2026-10-07) and stores 12 hours: the bot collects twice a day.
		for (int Visit = 0; Visit < 2; ++Visit)
		{
			Meta::AdvanceWorld(Profile, 12.0);
			Meta::CollectWorld(Profile);
		}
		// Events (2026-10-07): claim the missions, shop for pulls first, use every boost at once.
		for (int Index = 0; Index < RookieMissionCount; ++Index)
		{
			Events::ClaimRookie(Profile, Index);
		}
		for (int Index = 0; Index < EventMissionCount; ++Index)
		{
			Events::ClaimMission(Profile, Index);
		}
		for (int Index = 0; Index < EventShopCount; ++Index)
		{
			while (Events::BuyEventShop(Profile, Index).bOk) {}
		}
		for (int Index = 0; Index < ArkShopCount; ++Index)
		{
			while (Events::BuyArkShop(Profile, Index).bOk) {}
		}
		for (int Index = 0; Index < static_cast<int>(EItem::ChargePack); ++Index)
		{
			const EItem Item = static_cast<EItem>(Index);
			if (Inventory::Count(Profile, Item) > 0)
			{
				Inventory::Use(Profile, Item, Inventory::Count(Profile, Item));
			}
		}
		// Spare Gold and Bytes go to the cheapest Outer World upgrade, as long as the squad is not waiting for them.
		for (int Round = 0; Round < BuildingCount; ++Round)
		{
			int Best = -1;
			long long BestCost = 0;
			for (int Index = 0; Index < BuildingCount; ++Index)
			{
				const FCost Cost = Meta::BuildingNextCost(Profile, Index);
				const long long Total = static_cast<long long>(Cost.Gold) + Cost.Bytes;
				if ((Best < 0 || Total < BestCost) && !(Profile.Meta.Buildings[Index].bBuilt && Profile.Meta.Buildings[Index].Level >= MaxBuildingLevel))
				{
					Best = Index;
					BestCost = Total;
				}
			}
			if (Best < 0 || Profile.Get(ECurrency::Gold) < BestCost * 4)
			{
				break;
			}
			if (!Meta::BuildOrUpgrade(Profile, Best).bOk)
			{
				break;
			}
		}
	}

	// ---------------------------------------------------------------- after the campaign: the Secret hunt (2026-10-09)
	// The five daily arena fights against generated rivals (the guest path of Revolution > Arena).
	void ArenaFights(FProfile& Profile)
	{
		FArenaState& A = Profile.Meta.Arena;
		if (!Arena::TeamIsValid(Profile, A.Attack))
		{
			Arena::AutoFill(Profile, false);
		}
		for (int Guard = 0; Guard < Arena::DailyAttempts && Profile.Meta.ArenaAttempts > 0 && Arena::TeamIsValid(Profile, A.Attack); ++Guard)
		{
			const uint32_t Seed = static_cast<uint32_t>(Profile.NowUnix) + static_cast<uint32_t>(Profile.Meta.ArenaWins * 31 + Profile.Meta.ArenaLosses * 17 + A.Trophies);
			const Arena::FLineup Theirs = Arena::MakeRival(Profile, Seed);
			const Arena::FLineup Mine = Arena::MakeLineup(Profile, A.Attack, Profile.Meta.Name);
			const Arena::FBattle Battle = Arena::Simulate(Mine, Theirs, Seed * 2654435761u);
			const int Delta = Battle.bAttackerWon ? Arena::TrophyDelta(A.Trophies, Theirs.Trophies) : -Arena::TrophyDelta(Theirs.Trophies, A.Trophies);
			bool bFloor = A.bChampionFloor;
			const int Trophies = Arena::ApplyTrophies(A.Trophies, Delta, bFloor);
			A.bChampionFloor = bFloor;
			Arena::RecordFight(Profile, Theirs, Battle.bAttackerWon, Trophies, Trophies - A.Trophies);
		}
		Arena::ClaimSeasonReward(Profile);
	}

	// A day of a player who does the dailies after the campaign. Quick clears of the last level use real Charge; the other
	// daily-quest actions (summon, level-ups, an equipment change, three arcade games) are reported to the quests the way the
	// hub pages report them - their own rewards (arcade prizes, levels) are not simulated, so the income is a floor.
	void PostGameDay(FProfile& Profile, const FStyle& Style)
	{
		if (!Style.bDailyExtras)
		{
			return;
		}
		if (Meta::CanStartStage(Profile, TotalCampaignLevels).bOk)
		{
			Meta::QuickClear(Profile, TotalCampaignLevels, 3);
		}
		Meta::NotifyQuest(Profile, Meta::EQuestEvent::Summon);   // the Daily-banner pull below
		Meta::NotifyQuest(Profile, Meta::EQuestEvent::LevelUp, 3);
		Meta::NotifyQuest(Profile, Meta::EQuestEvent::EquipChange);
		Meta::NotifyQuest(Profile, Meta::EQuestEvent::ArcadePlay, 3);
		ArenaFights(Profile);
		DailyExtras(Profile);   // login, mail, quests, season pass, Outer World, events, Ark shop, boosts
		Meta::ClaimAllQuests(Profile);
		if (GOptions.bRoad)
		{
			Events::ClaimRoad(Profile);
		}
	}

	// Spends like a Secret hunter: Regular tickets on the Standard banner (persistent pity); Mileage, Special tickets and
	// (unless a saver) Diamonds only while the featured Secret is one the player does not own.
	void HuntPolicy(FProfile& Profile, FRun& Run, const FStyle& Style)
	{
		auto Pull = [&](int Banner, int Count, EPay Pay)
		{
			const double Before = PullValue(Profile);
			const FSummonOutcome Outcome = Profile.Summon(Banner, Count, Pay);
			if (Outcome.bOk)
			{
				Run.Summons += static_cast<int>(Outcome.Results.size());
				Run.PullsMade += Banner == 3 ? 0 : static_cast<int>(Outcome.Results.size());
				Run.SpentPulls += Before - PullValue(Profile);
			}
			return Outcome.bOk;
		};
		if (Profile.Get(ECurrency::DailyTicket) > 0)
		{
			Pull(3, 1, EPay::Ticket);
		}
		while (Profile.Get(ECurrency::RegularTicket) >= 10 && Pull(0, 10, EPay::Ticket)) {}
		while (Profile.Get(ECurrency::RegularTicket) >= 1 && Pull(0, 1, EPay::Ticket)) {}
		const int Banner = Profile.FindBanner("featured-6");
		if (Banner < 0)
		{
			return;
		}
		auto Wanted = [&]() { return !Profile.Owns(Profile.Banners[static_cast<size_t>(Banner)].FeaturedOperator); };
		if (Wanted() && Profile.Get(ECurrency::Mileage) >= Config::MileageFeatured6Cost && Profile.RedeemMileage(Banner).bOk)
		{
			++Run.SecretsFromMileage;
		}
		while (Wanted() && Profile.Get(ECurrency::SpecialTicket) >= 10 && Pull(Banner, 10, EPay::Ticket)) {}
		while (Wanted() && Profile.Get(ECurrency::SpecialTicket) >= 1 && Pull(Banner, 1, EPay::Ticket)) {}
		if (Style.bSpendDiamonds)
		{
			while (Wanted() && Profile.Get(ECurrency::Diamonds) >= Config::TenPullPaidRolls * Config::DiamondsPerRoll && Pull(Banner, 10, EPay::Diamonds)) {}
		}
		if (Wanted() && Profile.Get(ECurrency::Mileage) >= Config::MileageFeatured6Cost && Profile.RedeemMileage(Banner).bOk)
		{
			++Run.SecretsFromMileage;
		}
	}

	void SecretHunt(FProfile& Profile, FRun& Run, const FStyle& Style)
	{
		const int Total = SecretCount();
		Run.SecretsAtCampaignEnd = SecretsOwned(Profile);
		if (Run.SecretsAtCampaignEnd >= Total)
		{
			Run.SecretDay = Run.Days;
			Run.SecretsAtEnd = Total;
			return;
		}
		// The real clock from here: noon UTC, starting at the rotation anchor (2026-10-04), one day per step.
		int64_t Now = Config::RotationAnchorUnix + 12 * 3600;
		Profile.SetTime(Now);
		LiveOps::UpdateClock(Profile, Now);
		for (int Day = 1; Day <= GOptions.SecretDays; ++Day)
		{
			Now += 86400;
			Profile.SetTime(Now);
			LiveOps::UpdateClock(Profile, Now);   // the daily reset (Profile.Day + 1), seasons, Charge
			++Run.PostDays;
			if ((Day - 1) % 7 >= Style.PostDaysPerWeek)
			{
				continue;   // a day off: the Daily ticket expires, the Outer World keeps producing
			}
			++Run.PostPlayDays;
			const double Before = PullValue(Profile);
			const int RoadBefore = Profile.Meta.Events.RoadTickets;
			PostGameDay(Profile, Style);
			const double Earned = PullValue(Profile) - Before;
			const int Road = Profile.Meta.Events.RoadTickets - RoadBefore;
			Run.PostPullsRoad += Road;
			Run.PostPullsOld += Earned - Road;
			HuntPolicy(Profile, Run, Style);
			if (SecretsOwned(Profile) >= Total)
			{
				Run.SecretDay = Run.Days + Day;
				break;
			}
		}
		Run.SecretsAtEnd = SecretsOwned(Profile);
	}

	FRun Play(const FStyle& Style, unsigned Seed, bool bRecordStages)
	{
		FRun Run;
		FProfile Profile(Seed);
		FAccountInfo Guest;
		Guest.bGuest = true;
		Profile.Account = Guest;
		Profile.ResetForAccount();
		Tester::SkipTutorial(Profile); // the tutorial's own end state: Luna, Kuu, Plack, level 1 cleared
		Run.ReplaysAtStage.assign(TotalCampaignLevels + 1, 0);
		const double StartPulls = PullValue(Profile);
		int IdleDays = 0;
		for (int Day = 1; Day <= 3000 && Profile.StagesCleared < TotalCampaignLevels; ++Day)
		{
			if (Day > 1)
			{
				Profile.AdvanceDay();
				Inventory::RegenCharge(Profile, 86400);   // a day of Charge regeneration (the game uses the real clock)
			}
			int RefillsLeft = Style.ChargeRefills;
			Run.Days = Day;
			if (Style.bDailyExtras)
			{
				DailyExtras(Profile);
			}
			SummonPolicy(Profile, Run, Style);
			Tester::EquipBestOnTeam(Profile);
			bool bHearts = false;
			bool bCap = false;
			double Budget = Style.MinutesPerDay;
			auto Wallet = [&Profile](long long Out[3])
			{
				Out[0] = Profile.Get(ECurrency::Gold);
				Out[1] = Profile.Get(ECurrency::Bytes);
				Out[2] = Profile.Get(ECurrency::AngelHearts);
			};
			const int StartCleared = Profile.StagesCleared;
			for (int Guard = 0; Guard < 200; ++Guard)
			{
				long long Before[3];
				long long After[3];
				Wallet(Before);
				const std::string Reason = LevelSquad(Profile, Run, bHearts, bCap);
				(void)Reason;
				Wallet(After);
				for (int Kind = 0; Kind < 3; ++Kind)
				{
					Run.Spent[Kind] += Before[Kind] - After[Kind];
				}
				const int Next = Profile.StagesCleared + 1;
				if (Next > TotalCampaignLevels)
				{
					break;
				}
				const bool bReady = TeamAverage(Profile) >= Campaign::RecommendedLevel(Next);
				const int Stage = bReady ? Next : std::max(1, Profile.StagesCleared); // not ready: farm the newest cleared stage
				// Replays cost Charge (2026-10-07): Charge packs first, then the style's Diamond refills, else the day's farming ends.
				if (!bReady && !Meta::CanStartStage(Profile, Stage).bOk)
				{
					if (Inventory::Count(Profile, EItem::ChargePack) > 0)
					{
						Inventory::Use(Profile, EItem::ChargePack);
						continue;
					}
					if (RefillsLeft > 0 && Inventory::RefillCharge(Profile).bOk)
					{
						--RefillsLeft;
						continue;
					}
					++Run.ChargeDays;
					break;
				}
				// Replays are quick clears (2026-10-07: a sweep for Charge takes a few seconds), first clears are played.
				const double Cost = bReady ? RunMinutes(Stage) : 0.2;
				if (Budget < Cost)
				{
					break;
				}
				Budget -= Cost;
				Run.Minutes += Cost;
				++Run.Runs;
				if (bReady)
				{
					++Run.FirstClears;
				}
				else
				{
					++Run.Replays;
					++Run.ReplaysAtStage[static_cast<size_t>(Next)];
				}
				Wallet(Before);
				if (bReady)
				{
					Meta::ClearStage(Profile, Stage);
				}
				else
				{
					Meta::QuickClear(Profile, Stage, 1);
				}
				// The chapter quests (tickets, Diamonds, loot every 2-3 levels) are part of the campaign: every style claims them.
				for (int Quest = 0; Quest < StoryQuestCount; ++Quest)
				{
					Meta::ClaimQuest(Profile, Meta::EQuestList::Story, Quest);
				}
				Wallet(After);
				for (int Kind = 0; Kind < 3; ++Kind)
				{
					Run.Earned[Kind] += After[Kind] - Before[Kind];
				}
				Tester::EquipBestOnTeam(Profile);
				const int Chapter = ChapterOfStage(Profile.StagesCleared);
				if (Chapter >= 1 && Profile.StagesCleared == ChapterFirstStage(Chapter) + CampaignLevels - 1 && Run.ChapterDay[Chapter] == 0)
				{
					Run.ChapterDay[Chapter] = Day;
				}
			}
			Run.LevelBlockedByHearts += bHearts ? 1 : 0;
			Run.LevelBlockedByCap += bCap ? 1 : 0;
			IdleDays = Profile.StagesCleared == StartCleared && Run.Runs > 0 ? IdleDays : 0;
			// a day with no first clear is a grind day; a long stretch of them with nothing left to buy is a soft-lock
			if (Profile.StagesCleared == StartCleared)
			{
				++IdleDays;
			}
			else
			{
				IdleDays = 0;
			}
			if (IdleDays >= 120)
			{
				Run.StuckAt = Profile.StagesCleared + 1;
				Run.StuckWhy = bCap ? "level cap (needs upgrade stars)" : (bHearts ? "Angel hearts" : "currency");
				break;
			}
		}
		Run.bFinished = Profile.StagesCleared >= TotalCampaignLevels;
		Run.CampaignPulls = PullValue(Profile) - StartPulls + Run.SpentPulls;
		Run.FinalTeamLevel = TeamAverage(Profile);
		Run.FinalNeedLevel = Campaign::RecommendedLevel(TotalCampaignLevels);
		for (int Member : Profile.Team)
		{
			Run.TeamCapAtEnd = std::max(Run.TeamCapAtEnd, Member >= 0 ? Profile.LevelCap(Member) : 0);
		}
		for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
		{
			if (Profile.Owns(Index))
			{
				++Run.OperatorsOwned;
				Run.StarsTotal += Profile.Operators[static_cast<size_t>(Index)].UpgradeStars;
				Run.FiveStarOrBetter += Roster()[static_cast<size_t>(Index)].Stars >= 5 ? 1 : 0;
			}
		}
		Run.LeftGold = Profile.Get(ECurrency::Gold);
		Run.LeftBytes = Profile.Get(ECurrency::Bytes);
		Run.LeftHearts = Profile.Get(ECurrency::AngelHearts);
		Run.LeftDiamonds = Profile.Get(ECurrency::Diamonds);
		if (Run.bFinished && GOptions.SecretDays > 0)
		{
			SecretHunt(Profile, Run, Style);
		}
		// the invariant that matters for a tester or a real player: whatever happened, the save must load back unchanged
		const std::string Text = Save::Serialize(Profile, 1800000000);
		FProfile Loaded(1u);
		Loaded.Account = Profile.Account;
		std::string Error;
		if (!Save::Deserialize(Text, Loaded, Error) || Save::Serialize(Loaded, 1800000000) != Text)
		{
			std::printf("FAIL: seed %u, %s: the final profile does not survive a save (%s)\n", Seed, Style.Name, Error.c_str());
			Run.StuckWhy = "save round trip failed";
			Run.bFinished = false;
		}
		(void)bRecordStages;
		return Run;
	}

	double Median(std::vector<double> Values)
	{
		if (Values.empty())
		{
			return 0.0;
		}
		std::sort(Values.begin(), Values.end());
		return Values[Values.size() / 2];
	}

	double Percentile(std::vector<double> Values, double P)
	{
		if (Values.empty())
		{
			return 0.0;
		}
		std::sort(Values.begin(), Values.end());
		return Values[std::min(Values.size() - 1, static_cast<size_t>(P * static_cast<double>(Values.size())))];
	}
}

int main(int Argc, char** Argv)
{
	int Seeds = 60;
	bool bStages = false;
	for (int Index = 1; Index < Argc; ++Index)
	{
		if (std::strcmp(Argv[Index], "--stages") == 0)
		{
			bStages = true;
		}
		else if (std::strcmp(Argv[Index], "--seeds") == 0 && Index + 1 < Argc)
		{
			Seeds = std::max(1, std::atoi(Argv[++Index]));
		}
		else if (std::strcmp(Argv[Index], "--secret-days") == 0 && Index + 1 < Argc)
		{
			GOptions.SecretDays = std::max(0, std::atoi(Argv[++Index]));
		}
		else if (std::strcmp(Argv[Index], "--no-road") == 0)
		{
			GOptions.bRoad = false;
		}
	}
	int Failures = 0;
	std::printf("Full-game playthroughs: %d runs per style, guest account, 13 chapters x 8 levels = %d levels\n", Seeds, TotalCampaignLevels);
	std::printf("(run time per stage and play time per day are PLACEHOLDER assumptions of this tool)\n\n");
	std::printf("%-42s %6s %8s %8s %8s %7s %6s %5s %5s\n", "style", "done", "days p10", "days p50", "days p90", "grind%", "ops", "5*+", "stars");
	std::vector<FRun> Regular;
	std::vector<std::vector<FRun>> AllRuns;
	for (const FStyle& Style : Styles)
	{
		std::vector<FRun> Runs;
		for (int Seed = 0; Seed < Seeds; ++Seed)
		{
			Runs.push_back(Play(Style, 1000u + static_cast<unsigned>(Seed) * 7919u, bStages));
		}
		int Done = 0;
		std::vector<double> Days;
		std::vector<double> GrindShare;
		std::vector<double> Ops;
		std::vector<double> Fives;
		std::vector<double> Stars;
		for (const FRun& Run : Runs)
		{
			Done += Run.bFinished ? 1 : 0;
			Days.push_back(Run.Days);
			GrindShare.push_back(Run.Runs > 0 ? 100.0 * Run.Replays / Run.Runs : 0.0);
			Ops.push_back(Run.OperatorsOwned);
			Fives.push_back(Run.FiveStarOrBetter);
			Stars.push_back(Run.StarsTotal);
			if (!Run.bFinished)
			{
				++Failures;
				std::printf("  SOFT-LOCK or failure: %s stuck at stage %d (%s)\n", Style.Name, Run.StuckAt, Run.StuckWhy.c_str());
			}
		}
		std::printf("%-42s %3d/%-2d %8.0f %8.0f %8.0f %6.0f%% %6.1f %5.1f %5.1f\n", Style.Name, Done, Seeds, Percentile(Days, 0.10), Median(Days),
			Percentile(Days, 0.90), Median(GrindShare), Median(Ops), Median(Fives), Median(Stars));
		if (&Style == &Styles[RegularStyle])
		{
			Regular = Runs;
		}
		AllRuns.push_back(Runs);
	}
	if (!Regular.empty())
	{
		std::printf("\nWhen a regular player (120 min/day) finishes each chapter (median day):\n  ");
		for (int Chapter = 1; Chapter <= CampaignChapters; ++Chapter)
		{
			std::vector<double> Day;
			for (const FRun& Run : Regular)
			{
				Day.push_back(Run.ChapterDay[Chapter]);
			}
			std::printf("Ch%-2d d%-4.0f", Chapter, Median(Day));
			if (Chapter % 7 == 0)
			{
				std::printf("\n  ");
			}
		}
		std::vector<double> Hearts;
		std::vector<double> Cap;
		std::vector<double> LeftGold;
		std::vector<double> LeftBytes;
		std::vector<double> LeftHearts;
		std::vector<double> LeftDiamonds;
		for (const FRun& Run : Regular)
		{
			Hearts.push_back(Run.LevelBlockedByHearts);
			Cap.push_back(Run.LevelBlockedByCap);
			LeftGold.push_back(static_cast<double>(Run.LeftGold));
			LeftBytes.push_back(static_cast<double>(Run.LeftBytes));
			LeftHearts.push_back(static_cast<double>(Run.LeftHearts));
			LeftDiamonds.push_back(static_cast<double>(Run.LeftDiamonds));
		}
		std::vector<double> Earn[3];
		std::vector<double> Spend[3];
		std::vector<double> TeamLevel;
		for (const FRun& Run : Regular)
		{
			for (int Kind = 0; Kind < 3; ++Kind)
			{
				Earn[Kind].push_back(static_cast<double>(Run.Earned[Kind]));
				Spend[Kind].push_back(static_cast<double>(Run.Spent[Kind]));
			}
			TeamLevel.push_back(Run.FinalTeamLevel);
		}
		const char* Kinds[3] = {"Gold", "Bytes", "Angel hearts"};
		std::printf("\n\nRegular player, whole game (median): earned from stages and daily extras / spent on levels, gates and the Outer World\n");
		for (int Kind = 0; Kind < 3; ++Kind)
		{
			std::printf("  %-13s earned %12.0f   spent %12.0f   (%.0f%% spent)\n", Kinds[Kind], Median(Earn[Kind]), Median(Spend[Kind]), 100.0 * Median(Spend[Kind]) / std::max(1.0, Median(Earn[Kind])));
		}
		std::printf("  team level at the end %.0f (the last stage wants %d, cap %d)\n", Median(TeamLevel), Regular[0].FinalNeedLevel, Regular[0].TeamCapAtEnd);
		std::printf("\nRegular player, median at the end: days blocked by Angel hearts %.0f, days at the level cap %.0f\n", Median(Hearts), Median(Cap));
		std::printf("  wallet left: Gold %.0f, Bytes %.0f, Angel hearts %.0f, Diamonds %.0f\n", Median(LeftGold), Median(LeftBytes), Median(LeftHearts), Median(LeftDiamonds));
		if (bStages)
		{
			std::printf("\nReplays needed per stage (median over %d regular runs); only stages that needed grinding:\n", Seeds);
			for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
			{
				std::vector<double> Counts;
				for (const FRun& Run : Regular)
				{
					Counts.push_back(Run.ReplaysAtStage[static_cast<size_t>(Stage)]);
				}
				if (Median(Counts) > 0.0)
				{
					std::printf("  %-12s need Lv %3d  replays %4.0f (p90 %4.0f)\n", StageName(Stage).c_str(), Campaign::RecommendedLevel(Stage), Median(Counts), Percentile(Counts, 0.9));
				}
			}
		}
	}
	// ------------------------------------------------------------ the Secret hunt (2026-10-09)
	int SecretFailures = 0;
	std::string SecretSummary;
	if (GOptions.SecretDays > 0)
	{
		std::printf("\nSecret hunt after the campaign (%s; %d Secrets; up to %d more days; Mileage %d, pity %d, featured Secret rotates every %d days):\n",
			GOptions.bRoad ? "with the Ticket road" : "WITHOUT the Ticket road", SecretCount(), GOptions.SecretDays, Config::MileageFeatured6Cost, Config::Pity6Rolls,
			Config::RotationDays);
		std::printf("%-42s %7s %13s %13s %8s %8s %8s %7s %7s\n", "style", "all 6", "camp. pull/d", "post pull/d", "road/d", "days p10", "days p50", "p90", "pulls");
		for (size_t StyleIndex = 0; StyleIndex < AllRuns.size(); ++StyleIndex)
		{
			const std::vector<FRun>& Runs = AllRuns[StyleIndex];
			int Done = 0;
			std::vector<double> Days;
			std::vector<double> CampaignRate;
			std::vector<double> OldRate;
			std::vector<double> RoadRate;
			std::vector<double> Pulls;
			for (const FRun& Run : Runs)
			{
				Done += Run.SecretDay > 0 ? 1 : 0;
				// not done within the simulated days: counted as "after the end" so the percentiles stay honest
				Days.push_back(Run.SecretDay > 0 ? Run.SecretDay : 1e9);
				CampaignRate.push_back(Run.Days > 0 ? Run.CampaignPulls / Run.Days : 0.0);
				OldRate.push_back(Run.PostDays > 0 ? Run.PostPullsOld / Run.PostDays : 0.0);
				RoadRate.push_back(Run.PostDays > 0 ? Run.PostPullsRoad / Run.PostDays : 0.0);
				Pulls.push_back(Run.PullsMade);
			}
			auto DayText = [](double Value)
			{
				char Buffer[32];
				std::snprintf(Buffer, sizeof(Buffer), Value >= 1e8 ? ">end" : "%.0f", Value);
				return std::string(Buffer);
			};
			std::printf("%-42s %3d/%-3d %13.1f %13.1f %8.1f %8s %8s %7s %7.0f\n", Styles[StyleIndex].Name, Done, static_cast<int>(Runs.size()), Median(CampaignRate),
				Median(OldRate), Median(RoadRate), DayText(Percentile(Days, 0.10)).c_str(), DayText(Median(Days)).c_str(), DayText(Percentile(Days, 0.90)).c_str(), Median(Pulls));
			if (static_cast<int>(StyleIndex) == RegularStyle)
			{
				const double MedianDays = Median(Days);
				char Buffer[160];
				std::snprintf(Buffer, sizeof(Buffer), "Regular player owns all %d Secrets on day %s (median, %.1f months)%s", SecretCount(), DayText(MedianDays).c_str(),
					MedianDays / 30.4, GOptions.bRoad ? "" : " without the Ticket road");
				SecretSummary = Buffer;
				// Owner request 2026-10-09: "it can take users a year or 2 to get them all but make it possible".
				if (GOptions.bRoad && (MedianDays < 365.0 || MedianDays > 730.0))
				{
					++SecretFailures;
					std::printf("  FAIL: a regular player should own every Secret in 1-2 years (365-730 days), median %s\n", DayText(MedianDays).c_str());
				}
			}
		}
		std::printf("  (pull/d = tickets + Diamonds / 300 earned per calendar day, Daily tickets excluded; arcade, Ark Tower and gangs not counted)\n");
	}
	std::printf("\n%s\n", Failures == 0 ? "No soft-locks: every run finished the campaign." : "SOFT-LOCKS FOUND (see above)");
	if (!SecretSummary.empty())
	{
		std::printf("%s%s\n", SecretSummary.c_str(), SecretFailures == 0 ? "" : " - OUT OF THE 1-2 YEAR TARGET");
	}
	return Failures == 0 && SecretFailures == 0 ? 0 : 1;
}
