// Boldinbeauty demo — operations on the meta-game state (BIBMeta.h). Engine-independent.
// All amounts, prices, rates and names are PLACEHOLDERS for the owner to replace.
#pragma once

#include <string>
#include <vector>

#include "BIBCore.h"

namespace BIB
{
	namespace Meta
	{
		// ----------------------------------------------------------------- text
		std::string Num(long long Value);                // 12,345
		std::string RewardText(const FReward& Reward);   // "Gold 2,000, Bytes 500"
		std::string ApplyReward(FProfile& Profile, const FReward& Reward); // grants, returns RewardText

		// ------------------------------------------------------------ commander
		int CommanderLevel(const FProfile& Profile);
		int XpPerLevel();
		void AddCommanderXp(FProfile& Profile, int Amount);

		// --------------------------------------------------------------- quests
		// Four lists (owner request 2026-10-06): Daily (reset every day), Weekly (reset every 7 days), Story (one-time chapter
		// quests: levels 3 and 6 of every chapter pay Diamonds or Gold / Bytes / Angel hearts, the chapter end pays Regular
		// tickets - ten for Chapter 1, one after that) and Milestones (one-time account goals).
		enum class EQuestEvent : int
		{
			StageClear = 0,
			Summon,
			LevelUp,
			EquipChange,
			ShopBuy,
			ArcadePlay,
			WorldCollect,
			LoginBonus,
			ArkPlay,      // an Ark Tower floor or an arena fight
			DailyDone,    // a daily quest was claimed
			Count
		};

		enum class EQuestList : int
		{
			Daily = 0,
			Weekly,
			Story,
			Milestone,
			Count
		};

		struct FQuestInfo
		{
			std::string Title;
			int Target = 1;
			int Progress = 0;
			bool bClaimed = false;
			bool bComplete = false;
			FReward Reward;
		};

		struct FStoryQuestDef
		{
			std::string Title;
			int Chapter = 0;
			int Level = 0;   // level inside the chapter
			int Stage = 0;   // campaign stage id that completes it
			FReward Reward;
		};

		FStoryQuestDef StoryQuestDef(int Index);   // 0..StoryQuestCount-1
		FQuestInfo StoryQuest(const FProfile& Profile, int Index);
		int QuestCount(EQuestList List);
		const char* QuestListName(EQuestList List);
		FQuestInfo Quest(const FProfile& Profile, EQuestList List, int Index);
		FQuestInfo DailyQuest(const FProfile& Profile, int Index);
		FQuestInfo WeeklyQuest(const FProfile& Profile, int Index);
		FQuestInfo MilestoneQuest(const FProfile& Profile, int Index);
		void NotifyQuest(FProfile& Profile, EQuestEvent Event, int Amount = 1);
		FActionResult ClaimQuest(FProfile& Profile, EQuestList List, int Index);
		FActionResult ClaimQuest(FProfile& Profile, bool bMilestone, int Index);   // Daily or Milestone (older callers)
		FActionResult ClaimAllQuests(FProfile& Profile);
		int ClaimableQuests(const FProfile& Profile);
		int ClaimableQuests(const FProfile& Profile, EQuestList List);
		int DaysUntilWeeklyReset(const FProfile& Profile);

		// ----------------------------------------------------------------- mail
		int UnclaimedMail(const FProfile& Profile);
		FActionResult ClaimMail(FProfile& Profile, int MailId);
		FActionResult ClaimAllMail(FProfile& Profile);
		int DeleteClaimedMail(FProfile& Profile);

		// ------------------------------------------------------ events / login
		bool CanClaimLogin(const FProfile& Profile);
		FReward LoginReward(int StreakDay); // 1..7
		FActionResult ClaimLogin(FProfile& Profile);
		int NoticeCount();
		std::string NoticeTitle(int Index);
		std::string NoticeBody(int Index);
		int EventCount();
		std::string EventTitle(int Index);
		std::string EventBody(int Index);

		// ---------------------------------------------------------- season pass
		int SeasonTier(const FProfile& Profile);
		int SeasonXpPerTier();
		FReward SeasonReward(int Tier, bool bPremiumTrack);
		FActionResult ClaimSeasonTier(FProfile& Profile, int Tier);
		FActionResult ClaimAllSeason(FProfile& Profile);
		int ClaimableSeasonTiers(const FProfile& Profile);

		// ------------------------------------------------------------- shops
		struct FShopGear
		{
			std::string Name;
			ESlot Slot = ESlot::Helmet;
			EBonus Bonus = EBonus::Defence;
			double Percent = 0.0;
			int Gold = 0;
			int Bytes = 0;
		};
		const std::vector<FShopGear>& ArmouryStock();
		FActionResult BuyArmoury(FProfile& Profile, int Index);

		struct FAlleyOffer
		{
			std::string Name;
			int Diamonds = 0;
			int LevelLock = 1; // Commander level
			FReward Reward;
			bool bGear = false;
			ESlot Slot = ESlot::Helmet;
			EBonus Bonus = EBonus::Defence;
			double Percent = 0.0;
		};
		const std::vector<FAlleyOffer>& AlleyStock();
		FActionResult BuyAlley(FProfile& Profile, int Index);

		// Trade Centre: exchange nearly anything for anything else. Diamonds can be given but not received.
		const std::vector<ECurrency>& TradeCurrencies();
		bool CanReceiveInTrade(ECurrency Currency);
		long long TradeCost(ECurrency From, ECurrency To, long long AmountTo); // 0 when the pair is not allowed
		int TradeFeePercent();
		int TradePremiumMarkup(ECurrency To);   // receiving a summon ticket / arcade token costs this many times its base value (giving one does not)
		FActionResult Trade(FProfile& Profile, ECurrency From, ECurrency To, long long AmountTo);

		// Premium store: SIMULATED in-app purchases. No real payment, SDK or account is involved.
		struct FIapOffer
		{
			std::string Name;
			int PriceCents = 0;
			FReward Reward;
			bool bGivesPass = false;
			bool bOncePerProfile = false;
			bool bMonthly = false;   // Monthly supply (BIBInventory.h): 300 Diamonds now, 100 a day by mail for 30 days
		};
		const std::vector<FIapOffer>& IapOffers();
		FActionResult SimulatePurchase(FProfile& Profile, int Index);

		// -------------------------------------------------------- Outer World
		struct FBuildingDef
		{
			const char* Name;
			const char* Icon;
			int CostGold;
			int CostBytes;
			double GoldPerHour;
			double BytesPerHour;
			double HeartsPerHour;
		};
		const FBuildingDef& BuildingDef(int Index);
		constexpr double BuildingStorageHours = 12.0;
		int BuildingBuiltCount(const FProfile& Profile);
		FCost BuildingNextCost(const FProfile& Profile, int Index); // build or upgrade
		FActionResult BuildOrUpgrade(FProfile& Profile, int Index);
		void AdvanceWorld(FProfile& Profile, double Hours);
		FActionResult CollectWorld(FProfile& Profile);
		double BuildingRate(const FProfile& Profile, int Index, int Resource); // Resource 0 Gold, 1 Bytes, 2 Hearts (campaign bonus included)
		constexpr double WorldPercentPerStage = 1.0;                // +1% Outer World production per campaign level cleared
		double WorldCampaignMultiplier(const FProfile& Profile);

		// ---------------------------------------------------- tower / arena / stories
		// Tower modes: 0 = Operator (3-4 star operators only), 1 = Secrets (5-6 star operators only).
		std::vector<int> TowerTeam(const FProfile& Profile, int Mode); // the squad's eligible members, then the strongest other eligible operators (max 3)
		int TowerNextFloor(const FProfile& Profile, int Mode);
		FActionResult CanStartStage(const FProfile& Profile, int StageId);
		void EffectiveTeam(const FProfile& Profile, int StageId, int OutTeam[3]);
		std::vector<std::string> ClearStage(FProfile& Profile, int StageId);
		std::vector<std::string> FailStage(FProfile& Profile, int StageId);   // records where a tower climb fell
		// Quick clear (classic gacha "sweep", 2026-10-07): a campaign level already cleared pays its replay rewards Times times
		// at once for the Charge, without playing it. Returns the lines of the last clear plus a summary.
		FActionResult QuickClear(FProfile& Profile, int StageId, int Times);
		FReward TowerFloorReward(int Mode, int Floor);

		double TeamPower(const FProfile& Profile);   // the squad's battle power (BIBArena.h)

		// ----------------------------------------------------------- profile
		int OperatorsOwned(const FProfile& Profile);
		const char* StatName(int Stat); // Narrator Global Exchange stat names
	}
}
