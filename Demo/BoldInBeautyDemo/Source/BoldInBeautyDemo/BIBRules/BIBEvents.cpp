// Boldinbeauty demo — simple recurring events and the Ark shop (see BIBEvents.h). Every amount is PLACEHOLDER.
#include "BIBEvents.h"

#include "BIBCore.h"
#include "BIBInventory.h"
#include "BIBLiveOps.h"
#include "BIBMetaOps.h"

#include <algorithm>

namespace BIB
{
	namespace Events
	{
		namespace
		{
			FActionResult Ok(const std::string& Message)
			{
				FActionResult Result;
				Result.bOk = true;
				Result.Message = Message;
				return Result;
			}

			FActionResult Fail(const std::string& Message)
			{
				FActionResult Result;
				Result.Message = Message;
				return Result;
			}

			constexpr int Item(EItem I) { return static_cast<int>(I); }

			struct FMissionDef
			{
				const char* Title;
				EStat Stat;
				int Target;
				FReward Reward;
			};

			// Rookie week: 3 missions a day, cumulative counters since the account started. In total 20 Regular tickets,
			// 5 Special tickets and 2,000 Diamonds plus boosts, Element cores and Ark medals (classic beginner missions).
			const FMissionDef RookieDefs[RookieMissionCount] = {
				{"Clear 2 campaign levels", EStat::StageClear, 2, {.RegularTickets = 2}},
				{"Summon once", EStat::Summon, 1, {.RegularTickets = 2}},
				{"Level up operators 3 times", EStat::LevelUp, 3, {.Diamonds = 200}},
				{"Clear 5 campaign levels", EStat::StageClear, 5, {.RegularTickets = 2, .Item = Item(EItem::GoldBoost6h), .ItemAmount = 1}},
				{"Claim 2 login bonuses", EStat::LoginBonus, 2, {.Diamonds = 200}},
				{"Complete 5 daily quests", EStat::DailyDone, 5, {.SpecialTickets = 1}},
				{"Summon 10 times", EStat::Summon, 10, {.RegularTickets = 3}},
				{"Level up operators 15 times", EStat::LevelUp, 15, {.Diamonds = 300}},
				{"Collect from the Outer World", EStat::WorldCollect, 1, {.Charge = 60, .Item = Item(EItem::HeartsBoost6h), .ItemAmount = 1}},
				{"Clear 10 campaign levels", EStat::StageClear, 10, {.RegularTickets = 2, .ElementCores = 30}},
				{"Play the arcade twice", EStat::ArcadePlay, 2, {.Diamonds = 100, .ArcadeTokens = 5}},
				{"Complete 15 daily quests", EStat::DailyDone, 15, {.SpecialTickets = 1}},
				{"Fight 3 times in the Ark", EStat::ArkPlay, 3, {.RegularTickets = 2, .ArkMedals = 200}},
				{"Summon 20 times", EStat::Summon, 20, {.RegularTickets = 2}},
				{"Claim 5 login bonuses", EStat::LoginBonus, 5, {.Diamonds = 300}},
				{"Clear 16 campaign levels", EStat::StageClear, 16, {.SpecialTickets = 1, .Item = Item(EItem::BytesBoost12h), .ItemAmount = 1}},
				{"Level up operators 40 times", EStat::LevelUp, 40, {.Diamonds = 300}},
				{"Attune an operator's element", EStat::Attune, 1, {.RegularTickets = 2, .ElementCores = 40}},
				{"Claim 7 login bonuses", EStat::LoginBonus, 7, {.Diamonds = 300, .SpecialTickets = 2}},
				{"Complete 30 daily quests", EStat::DailyDone, 30, {.RegularTickets = 3}},
				{"Clear 20 campaign levels", EStat::StageClear, 20, {.Diamonds = 300, .Item = Item(EItem::GoldBoost24h), .ItemAmount = 1}},
			};

			// Weekly operation missions per theme (Event tokens; 1,400 a week in total).
			const FMissionDef HuntDefs[EventMissionCount] = {
				{"Clear 10 campaign levels", EStat::StageClear, 10, {.EventTokens = 150}},
				{"Clear 25 campaign levels", EStat::StageClear, 25, {.EventTokens = 250}},
				{"Fight 10 times in the Ark", EStat::ArkPlay, 10, {.EventTokens = 200}},
				{"Summon 20 times", EStat::Summon, 20, {.EventTokens = 200}},
				{"Level up operators 30 times", EStat::LevelUp, 30, {.EventTokens = 150}},
				{"Complete 20 daily quests", EStat::DailyDone, 20, {.EventTokens = 250}},
				{"Use 5 Outer World boosts", EStat::BoostUse, 5, {.EventTokens = 100}},
				{"Claim 5 login bonuses", EStat::LoginBonus, 5, {.EventTokens = 100}},
			};
			const FMissionDef FeverDefs[EventMissionCount] = {
				{"Play the arcade 5 times", EStat::ArcadePlay, 5, {.EventTokens = 150}},
				{"Play the arcade 15 times", EStat::ArcadePlay, 15, {.EventTokens = 250}},
				{"Fight 10 times in the Ark", EStat::ArkPlay, 10, {.EventTokens = 200}},
				{"Clear 10 campaign levels", EStat::StageClear, 10, {.EventTokens = 200}},
				{"Level up operators 30 times", EStat::LevelUp, 30, {.EventTokens = 150}},
				{"Complete 20 daily quests", EStat::DailyDone, 20, {.EventTokens = 250}},
				{"Use 5 Outer World boosts", EStat::BoostUse, 5, {.EventTokens = 100}},
				{"Claim 5 login bonuses", EStat::LoginBonus, 5, {.EventTokens = 100}},
			};

			const FMissionDef& WeeklyDef(ETheme ThemeId, int Index)
			{
				const int Clamped = std::clamp(Index, 0, EventMissionCount - 1);
				return ThemeId == ETheme::ArcadeFever ? FeverDefs[Clamped] : HuntDefs[Clamped];
			}

			int ArkShopKeyOf(const FProfile& Profile)
			{
				return Profile.NowUnix > 0 ? LiveOps::SeasonKeyOf(Profile.NowUnix) : 1 + (std::max(1, Profile.Day) - 1) / 30;
			}

			void RefreshArkShop(FProfile& Profile)
			{
				FEventState& E = Profile.Meta.Events;
				const int Key = ArkShopKeyOf(Profile);
				if (E.ArkShopKey != Key)
				{
					E.ArkShopKey = Key;
					std::fill(std::begin(E.ArkShopBought), std::end(E.ArkShopBought), 0);
				}
			}
		}

		// ===================================================================== calendar
		int WeekIndex(const FProfile& Profile) { return (std::max(1, Profile.Day) - 1) / 7; }
		int DayOfWeek(const FProfile& Profile) { return (std::max(1, Profile.Day) - 1) % 7 + 1; }
		int DaysLeftInWeek(const FProfile& Profile) { return 8 - DayOfWeek(Profile); }

		void Refresh(FProfile& Profile)
		{
			FEventState& E = Profile.Meta.Events;
			const int Week = WeekIndex(Profile);
			if (E.WeekKey != Week)
			{
				E.WeekKey = Week;
				std::fill(std::begin(E.Counters), std::end(E.Counters), 0);
				std::fill(std::begin(E.MissionClaimed), std::end(E.MissionClaimed), false);
				std::fill(std::begin(E.ShopBought), std::end(E.ShopBought), 0);
			}
			RefreshArkShop(Profile);
		}

		void Notify(FProfile& Profile, EStat Stat, int Amount)
		{
			if (Amount <= 0 || Stat == EStat::Count)
			{
				return;
			}
			Refresh(Profile);
			FEventState& E = Profile.Meta.Events;
			const int Index = static_cast<int>(Stat);
			E.Counters[Index] = std::min(1000000, E.Counters[Index] + Amount);
			if (RookieActive(Profile))
			{
				E.RookieCounters[Index] = std::min(1000000, E.RookieCounters[Index] + Amount);
			}
		}

		// ===================================================================== Rookie week
		void StartRookie(FProfile& Profile)
		{
			FEventState& E = Profile.Meta.Events;
			E.RookieStartDay = std::max(1, Profile.Day);
			std::fill(std::begin(E.RookieCounters), std::end(E.RookieCounters), 0);
			std::fill(std::begin(E.RookieClaimed), std::end(E.RookieClaimed), false);
		}

		int RookieDay(const FProfile& Profile)
		{
			const int Start = Profile.Meta.Events.RookieStartDay;
			if (Start <= 0)
			{
				return 0;
			}
			const int DayNumber = Profile.Day - Start + 1;
			return DayNumber >= 1 && DayNumber <= RookieDays ? DayNumber : 0;
		}

		bool RookieActive(const FProfile& Profile)
		{
			return RookieDay(Profile) > 0;
		}

		FMission RookieMission(const FProfile& Profile, int Index)
		{
			const int Clamped = std::clamp(Index, 0, RookieMissionCount - 1);
			const FMissionDef& Def = RookieDefs[Clamped];
			const FEventState& E = Profile.Meta.Events;
			FMission M;
			M.Title = Def.Title;
			M.Stat = Def.Stat;
			M.Target = Def.Target;
			M.Reward = Def.Reward;
			M.UnlockDay = Clamped / 3 + 1;
			M.bLocked = RookieDay(Profile) < M.UnlockDay;
			M.Progress = std::min(Def.Target, E.RookieCounters[static_cast<int>(Def.Stat)]);
			M.bClaimed = E.RookieClaimed[Clamped];
			M.bComplete = M.Progress >= Def.Target;
			return M;
		}

		FActionResult ClaimRookie(FProfile& Profile, int Index)
		{
			if (Index < 0 || Index >= RookieMissionCount)
			{
				return Fail("No such mission.");
			}
			if (!RookieActive(Profile))
			{
				return Fail("The Rookie week is over.");
			}
			const FMission M = RookieMission(Profile, Index);
			if (M.bLocked)
			{
				return Fail("This mission unlocks on day " + std::to_string(M.UnlockDay) + ".");
			}
			if (M.bClaimed)
			{
				return Fail("Already claimed.");
			}
			if (!M.bComplete)
			{
				return Fail(M.Title + ": " + std::to_string(M.Progress) + " / " + std::to_string(M.Target) + ".");
			}
			Profile.Meta.Events.RookieClaimed[Index] = true;
			return Ok("Rookie week: " + M.Title + " - " + Meta::ApplyReward(Profile, M.Reward));
		}

		FReward RookieTotal()
		{
			FReward Total;
			for (const FMissionDef& Def : RookieDefs)
			{
				Total.Diamonds += Def.Reward.Diamonds;
				Total.RegularTickets += Def.Reward.RegularTickets;
				Total.SpecialTickets += Def.Reward.SpecialTickets;
				Total.ElementCores += Def.Reward.ElementCores;
				Total.ArkMedals += Def.Reward.ArkMedals;
				Total.ArcadeTokens += Def.Reward.ArcadeTokens;
				Total.Charge += Def.Reward.Charge;
			}
			return Total;
		}

		// ===================================================================== weekly operation
		ETheme Theme(const FProfile& Profile)
		{
			return WeekIndex(Profile) % 2 == 0 ? ETheme::AngelHunt : ETheme::ArcadeFever;
		}

		const char* ThemeName(ETheme ThemeId)
		{
			return ThemeId == ETheme::ArcadeFever ? "Arcade Fever" : "Angel Hunt";
		}

		std::string ThemeBlurb(ETheme ThemeId)
		{
			return ThemeId == ETheme::ArcadeFever
				? "Neo Tokyo's arcades stay open all night: arcade missions, and +" + std::to_string(ArcadeFeverTokens) + " Arcade tokens every day."
				: "Angels were sighted above the Ark: clear levels and fight in the Ark for Event tokens.";
		}

		FMission Mission(const FProfile& Profile, int Index)
		{
			const int Clamped = std::clamp(Index, 0, EventMissionCount - 1);
			const FMissionDef& Def = WeeklyDef(Theme(Profile), Clamped);
			const FEventState& E = Profile.Meta.Events;
			const bool bCurrent = E.WeekKey == WeekIndex(Profile);
			FMission M;
			M.Title = Def.Title;
			M.Stat = Def.Stat;
			M.Target = Def.Target;
			M.Reward = Def.Reward;
			M.Progress = bCurrent ? std::min(Def.Target, E.Counters[static_cast<int>(Def.Stat)]) : 0;
			M.bClaimed = bCurrent && E.MissionClaimed[Clamped];
			M.bComplete = M.Progress >= Def.Target;
			return M;
		}

		FActionResult ClaimMission(FProfile& Profile, int Index)
		{
			if (Index < 0 || Index >= EventMissionCount)
			{
				return Fail("No such mission.");
			}
			Refresh(Profile);
			const FMission M = Mission(Profile, Index);
			if (M.bClaimed)
			{
				return Fail("Already claimed this week.");
			}
			if (!M.bComplete)
			{
				return Fail(M.Title + ": " + std::to_string(M.Progress) + " / " + std::to_string(M.Target) + ".");
			}
			Profile.Meta.Events.MissionClaimed[Index] = true;
			return Ok(std::string(ThemeName(Theme(Profile))) + ": " + M.Title + " - " + Meta::ApplyReward(Profile, M.Reward));
		}

		int TokensPerWeek()
		{
			int Tokens = 0;
			for (const FMissionDef& Def : HuntDefs)
			{
				Tokens += Def.Reward.EventTokens;
			}
			return Tokens;
		}

		const FShopItem& EventShopItem(int Index)
		{
			// Weekly limits. Buying everything costs ~2,000 tokens against 1,400 earned: the player picks (classic event shop).
			static const FShopItem Items[EventShopCount] = {
				{"Special ticket", "ticket_special", 300, 1, {.SpecialTickets = 1}},
				{"Regular ticket", "ticket_regular", 150, 1, {.RegularTickets = 1}},
				{"Element cores x20", "element_core", 100, 3, {.ElementCores = 20}},
				{"Gold boost (12 h)", "boost_gold", 60, 2, {.Item = Item(EItem::GoldBoost12h), .ItemAmount = 1}},
				{"Bytes boost (12 h)", "boost_bytes", 60, 2, {.Item = Item(EItem::BytesBoost12h), .ItemAmount = 1}},
				{"Angel-heart boost (12 h)", "boost_hearts", 80, 1, {.Item = Item(EItem::HeartsBoost12h), .ItemAmount = 1}},
				{"Charge pack", "charge", 50, 3, {.Item = Item(EItem::ChargePack), .ItemAmount = 1}},
				{"Arcade tokens x5", "arcade_token", 40, 2, {.ArcadeTokens = 5}},
				{"Diamonds x100", "diamond", 120, 1, {.Diamonds = 100}},
				{"Random equipment", "equipment", 80, 2, {.Equipment = 1}},
			};
			return Items[std::clamp(Index, 0, EventShopCount - 1)];
		}

		int EventShopLeft(const FProfile& Profile, int Index)
		{
			const FEventState& E = Profile.Meta.Events;
			const int Clamped = std::clamp(Index, 0, EventShopCount - 1);
			const int Bought = E.WeekKey == WeekIndex(Profile) ? E.ShopBought[Clamped] : 0;
			return std::max(0, EventShopItem(Clamped).Limit - Bought);
		}

		FActionResult BuyEventShop(FProfile& Profile, int Index)
		{
			if (Index < 0 || Index >= EventShopCount)
			{
				return Fail("No such item.");
			}
			Refresh(Profile);
			const FShopItem& Shop = EventShopItem(Index);
			if (EventShopLeft(Profile, Index) <= 0)
			{
				return Fail(Shop.Name + " is sold out this week.");
			}
			if (!Profile.Spend(ECurrency::EventToken, Shop.Price))
			{
				return Fail("Need " + Meta::Num(Shop.Price) + " Event tokens.");
			}
			++Profile.Meta.Events.ShopBought[Index];
			Meta::NotifyQuest(Profile, Meta::EQuestEvent::ShopBuy);
			return Ok("Event shop: " + Meta::ApplyReward(Profile, Shop.Reward));
		}

		// ===================================================================== Overtime weekend
		bool OvertimeActive(const FProfile& Profile)
		{
			return DayOfWeek(Profile) >= 6;
		}

		double WorldMultiplier(const FProfile& Profile)
		{
			return OvertimeActive(Profile) ? OvertimeWorldMultiplier : 1.0;
		}

		double BoostMultiplier(const FProfile& Profile)
		{
			return OvertimeActive(Profile) ? OvertimeBoostMultiplier : 1.0;
		}

		// ===================================================================== Element Surge
		bool SurgeActive(const FProfile& Profile)
		{
			(void)Profile;
			return true;   // every week has a Surge element
		}

		EElement SurgeElement(const FProfile& Profile)
		{
			// Walks both rings: Fire, Water, Nature, Volt, Dark, Halo, Magic, Glitch, then again.
			return static_cast<EElement>(WeekIndex(Profile) % ElementCount);
		}

		int SurgeCoresFor(const FProfile& Profile, int StageId, bool bFirstClear)
		{
			if (!SurgeActive(Profile) || DecodeStage(StageId).Kind != EStageKind::Campaign)
			{
				return 0;
			}
			const EElement Surge = SurgeElement(Profile);
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				if (Profile.Team[Slot] >= 0 && Element::OfOperator(Profile.Team[Slot]) == Surge)
				{
					return bFirstClear ? SurgeCoresFirstClear : SurgeCoresPerClear;
				}
			}
			return 0;
		}

		// ===================================================================== Ark shop
		const FShopItem& ArkShopItem(int Index)
		{
			// Monthly limits (they reset with the arena season).
			static const FShopItem Items[ArkShopCount] = {
				{"Special ticket", "ticket_special", 1200, 1, {.SpecialTickets = 1}},
				{"Regular ticket", "ticket_regular", 600, 2, {.RegularTickets = 1}},
				{"Element cores x20", "element_core", 150, 5, {.ElementCores = 20}},
				{"Gold boost (24 h)", "boost_gold", 200, 2, {.Item = Item(EItem::GoldBoost24h), .ItemAmount = 1}},
				{"Bytes boost (24 h)", "boost_bytes", 200, 2, {.Item = Item(EItem::BytesBoost24h), .ItemAmount = 1}},
				{"Angel-heart boost (12 h)", "boost_hearts", 250, 2, {.Item = Item(EItem::HeartsBoost12h), .ItemAmount = 1}},
				{"Charge pack", "charge", 100, 4, {.Item = Item(EItem::ChargePack), .ItemAmount = 1}},
				{"Random equipment", "equipment", 150, 3, {.Equipment = 1}},
			};
			return Items[std::clamp(Index, 0, ArkShopCount - 1)];
		}

		int ArkShopLeft(const FProfile& Profile, int Index)
		{
			const FEventState& E = Profile.Meta.Events;
			const int Clamped = std::clamp(Index, 0, ArkShopCount - 1);
			const int Bought = E.ArkShopKey == ArkShopKeyOf(Profile) ? E.ArkShopBought[Clamped] : 0;
			return std::max(0, ArkShopItem(Clamped).Limit - Bought);
		}

		FActionResult BuyArkShop(FProfile& Profile, int Index)
		{
			if (Index < 0 || Index >= ArkShopCount)
			{
				return Fail("No such item.");
			}
			RefreshArkShop(Profile);
			const FShopItem& Shop = ArkShopItem(Index);
			if (ArkShopLeft(Profile, Index) <= 0)
			{
				return Fail(Shop.Name + " is sold out this month.");
			}
			if (!Profile.Spend(ECurrency::ArkMedal, Shop.Price))
			{
				return Fail("Need " + Meta::Num(Shop.Price) + " Ark medals.");
			}
			++Profile.Meta.Events.ArkShopBought[Index];
			Meta::NotifyQuest(Profile, Meta::EQuestEvent::ShopBuy);
			return Ok("Ark shop: " + Meta::ApplyReward(Profile, Shop.Reward));
		}

		int ArkMedalsForArena(bool bWon)
		{
			return bWon ? 30 : 10;
		}

		int ArkMedalsForFloor(int Floor)
		{
			return 10 + std::max(0, Floor) / 5 * 2;
		}

		int ActiveCount(const FProfile& Profile)
		{
			return 2 + (RookieActive(Profile) ? 1 : 0) + (OvertimeActive(Profile) ? 1 : 0);   // weekly operation, Surge, ...
		}

		// ===================================================================== Ticket road (2026-10-09, PLACEHOLDER amounts)
		bool RoadOpen(const FProfile& Profile)
		{
			return Profile.StagesCleared >= TotalCampaignLevels;
		}

		FReward RoadDailyReward()
		{
			FReward Reward;
			Reward.SpecialTickets = RoadDailySpecial;
			return Reward;
		}

		FReward RoadWeeklyReward()
		{
			FReward Reward;
			Reward.RegularTickets = RoadWeeklyRegular;
			return Reward;
		}

		FReward RoadMilestoneReward()
		{
			FReward Reward;
			Reward.SpecialTickets = RoadMilestoneSpecial;
			return Reward;
		}

		double RoadTicketsPer30Days()
		{
			return 30.0 * RoadDailySpecial + 30.0 / 7.0 * RoadWeeklyRegular + 30.0 / RoadMilestoneDays * RoadMilestoneSpecial;
		}

		FRoad Road(const FProfile& Profile)
		{
			const FEventState& E = Profile.Meta.Events;
			FRoad R;
			R.bOpen = RoadOpen(Profile);
			for (size_t Index = 0; Index < Profile.Meta.Daily.size(); ++Index)
			{
				R.DailyQuestsDone += Profile.Meta.Daily[Index].bClaimed ? 1 : 0;
			}
			R.bDailyClaimed = E.RoadLastDay == Profile.Day;
			R.bDailyReady = R.bOpen && !R.bDailyClaimed && R.DailyQuestsDone >= RoadDailyQuests;
			const bool bSameWeek = E.RoadWeekKey == WeekIndex(Profile);
			R.WeekDays = bSameWeek ? E.RoadWeekDays : 0;
			R.bWeeklyClaimed = bSameWeek && E.bRoadWeekClaimed;
			R.bWeeklyReady = R.bOpen && !R.bWeeklyClaimed && R.WeekDays >= RoadWeeklyDays;
			R.TotalDays = E.RoadDays;
			R.MilestonesClaimed = E.RoadMilestones;
			R.NextMilestoneDay = (E.RoadMilestones + 1) * RoadMilestoneDays;
			R.bMilestoneReady = R.bOpen && R.TotalDays >= R.NextMilestoneDay;
			R.TicketsEarned = E.RoadTickets;
			return R;
		}

		int RoadClaimable(const FProfile& Profile)
		{
			const FRoad R = Road(Profile);
			return (R.bDailyReady ? 1 : 0) + (R.bWeeklyReady ? 1 : 0) + (R.bMilestoneReady ? 1 : 0);
		}

		FActionResult ClaimRoad(FProfile& Profile)
		{
			FRoad R = Road(Profile);
			if (!R.bOpen)
			{
				return Fail("The Ticket road opens when the campaign is finished (Chapter 13 boss).");
			}
			FEventState& E = Profile.Meta.Events;
			std::string Text;
			int Tickets = 0;
			auto Pay = [&](const char* What, const FReward& Reward)
			{
				Text += (Text.empty() ? "" : "; ") + std::string(What) + ": " + Meta::ApplyReward(Profile, Reward);
				Tickets += Reward.RegularTickets + Reward.SpecialTickets;
			};
			if (R.bDailyReady)
			{
				if (E.RoadWeekKey != WeekIndex(Profile))
				{
					E.RoadWeekKey = WeekIndex(Profile);
					E.RoadWeekDays = 0;
					E.bRoadWeekClaimed = false;
				}
				E.RoadLastDay = Profile.Day;
				++E.RoadWeekDays;
				E.RoadDays = std::min(E.RoadDays + 1, 10000000);
				Pay("Daily stipend", RoadDailyReward());
				R = Road(Profile);
			}
			if (R.bWeeklyReady)
			{
				E.bRoadWeekClaimed = true;
				Pay("Weekly bonus", RoadWeeklyReward());
			}
			// One milestone per stipend day at most, so an old save with a long road cannot pay a backlog in one go.
			if (R.bMilestoneReady)
			{
				++E.RoadMilestones;
				Pay(("Road milestone " + std::to_string(E.RoadMilestones)).c_str(), RoadMilestoneReward());
			}
			if (Tickets == 0)
			{
				if (R.bDailyClaimed)
				{
					return Fail("Today's stipend is taken - come back tomorrow.");
				}
				return Fail("Claim " + std::to_string(RoadDailyQuests) + " daily quests first (" + std::to_string(R.DailyQuestsDone) + " today).");
			}
			E.RoadTickets = std::min(E.RoadTickets + Tickets, 2000000000);
			return Ok("Ticket road - " + Text);
		}
	}
}
