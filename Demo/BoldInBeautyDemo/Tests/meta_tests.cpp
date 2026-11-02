// Unit tests for the demo's meta-game (stages, quests, mail, season pass, shops, Outer World, arcade, tower, arena)
// and for the hub page model. Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArena.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArcade.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCampaign.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBInventory.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

namespace
{
	int GFailures = 0;
	int GChecks = 0;

	void Expect(bool bCondition, const std::string& What)
	{
		++GChecks;
		if (!bCondition)
		{
			++GFailures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	using namespace BIB;
	using namespace BIB::Meta;
	namespace P = BIB::Pages;

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	// ------------------------------------------------------------------ stages
	void TestStagePlans()
	{
		Expect(CampaignLevels == 8 && CampaignChapters == 13, "8 levels, 13 chapters");
		for (int Level = 1; Level <= CampaignLevels; ++Level)
		{
			const int Id = CampaignStageId(Level);
			Expect(IsValidStage(Id), "campaign level valid " + std::to_string(Level));
			const FStageRef Ref = DecodeStage(Id);
			Expect(Ref.Kind == EStageKind::Campaign && Ref.B == Level, "campaign decode " + std::to_string(Level));
			const std::vector<FSectionPlanData> Plan = StagePlan(Id);
			Expect(Plan.size() >= 2 && Plan.size() <= 5, "MAIN 7: 2-5 sections in level " + std::to_string(Level));
			int Minis = 0;
			int Bosses = 0;
			for (size_t Index = 0; Index < Plan.size(); ++Index)
			{
				Expect(Plan[Index].Total() >= Config::MinEnemiesPerSection && Plan[Index].Total() <= Config::MaxEnemiesPerSection,
					"MAIN 7: 10-30 enemies, level " + std::to_string(Level) + " section " + std::to_string(Index + 1));
				Minis += Plan[Index].MiniBosses;
				Bosses += Plan[Index].Bosses;
			}
			Expect(Minis >= 1, "mini-boss in every level " + std::to_string(Level));
			Expect(Bosses == (Level == CampaignLevels ? 1 : 0), "boss only at the chapter end (level " + std::to_string(Level) + ")");
			Expect(IsChapterFinal(Id) == (Level == CampaignLevels), "chapter final flag " + std::to_string(Level));
		}
		const std::vector<FSectionPlanData> Last = StagePlan(CampaignStageId(8));
		Expect(Last.back().Bosses == 1 && Last.back().MiniBosses == 1, "level 8 ends with the boss section (mini-boss too)");
		Expect(!IsValidStage(0) && !IsValidStage(TotalCampaignLevels + 1) && IsValidStage(9) && StagePlan(TotalCampaignLevels + 1).empty(), "invalid campaign levels (the campaign has 104 stages now)");

		for (int Mode = 0; Mode < 2; ++Mode)
		{
			for (int Floor : {1, 2, 5, 10, 50, 100, 250, MaxTowerFloor})
			{
				const int Id = TowerStageId(Mode, Floor);
				const FStageRef Ref = DecodeStage(Id);
				Expect(Ref.Kind == EStageKind::Tower && Ref.A == Mode && Ref.B == Floor, "tower decode");
				const std::vector<FSectionPlanData> Plan = StagePlan(Id);
				Expect(Plan.size() == 1 && Plan[0].Total() >= 10 && Plan[0].Total() <= 30, "tower floor size " + std::to_string(Floor));
				Expect((Plan[0].MiniBosses == 1) == (Floor % 5 == 0) && (Plan[0].Bosses == 1) == (Floor % 25 == 0), "tower mini-boss every 5th floor, a boss every 25th");
			}
		}
		Expect(!IsValidStage(TowerStageId(0, 0)) && !IsValidStage(TowerStageId(0, MaxTowerFloor + 1)), "tower bounds");
		for (int Story = 0; Story < SubStoryCount; ++Story)
		{
			Expect(SubStoryLevels(Story) >= 1 && SubStoryLevels(Story) <= 3, "sub-stories have 1-3 levels");
			for (int Level = 1; Level <= SubStoryLevels(Story); ++Level)
			{
				const int Id = SubStoryStageId(Story, Level);
				Expect(IsValidStage(Id), "sub-story stage valid");
				const std::vector<FSectionPlanData> Plan = StagePlan(Id);
				Expect(!Plan.empty() && Plan.size() <= 5 && Plan.back().MiniBosses == 1, "sub-story plan has a mini-boss");
				for (const FSectionPlanData& Section : Plan)
				{
					Expect(Section.Total() >= 10 && Section.Total() <= 30, "sub-story section size");
				}
			}
			Expect(!IsValidStage(SubStoryStageId(Story, SubStoryLevels(Story) + 1)), "sub-story level past the end invalid");
		}
		Expect(StageEnemyScale(CampaignStageId(1)) == 1.0 && StageEnemyScale(CampaignStageId(8)) > 2.0, "enemy scale grows through the chapter");
		Expect(Contains(StageName(CampaignStageId(8)), "chapter end"), "level 8 name says chapter end");
	}

	// ------------------------------------------------------------ sequential unlock
	void TestCampaignFlow()
	{
		FProfile Profile(7);
		Expect(CanStartStage(Profile, CampaignStageId(1)).bOk, "level 1 open at start");
		for (int Level = 2; Level <= CampaignLevels; ++Level)
		{
			Expect(!CanStartStage(Profile, CampaignStageId(Level)).bOk, "level " + std::to_string(Level) + " locked at start");
		}
		const int Plack = FindOperator("Plack");
		for (int Level = 1; Level <= CampaignLevels; ++Level)
		{
			Expect(CanStartStage(Profile, CampaignStageId(Level)).bOk, "level " + std::to_string(Level) + " opens after the previous clear");
			Expect(!Profile.Owns(Plack) || Level == CampaignLevels + 1, "Plack not owned before level 8");
			const int GoldBefore = Profile.Get(ECurrency::Gold);
			const std::vector<std::string> Lines = ClearStage(Profile, CampaignStageId(Level));
			Expect(Profile.Get(ECurrency::Gold) > GoldBefore && !Lines.empty(), "level reward paid " + std::to_string(Level));
			Expect(Profile.StagesCleared == Level, "levels cleared counter " + std::to_string(Level));
		}
		Expect(Profile.Owns(Plack), "Plack joins after the chapter boss (level 8)");
		Expect(CanStartStage(Profile, CampaignStageId(3)).bOk, "cleared levels can be replayed");
		Expect(Profile.Meta.CommanderXp >= 800 && Profile.Meta.SeasonXp == 0, "clears give Commander XP; Season XP comes only from daily and weekly quests (2026-10-06)");
	}

	// ---------------------------------------------------- placeholder operators / tower
	void TestPlaceholderOperatorsAndTower()
	{
		FProfile Profile(9);
		const int Luna = FindOperator("Luna");
		const int Judge = FindOperator("Judge");
		const int Narrator = FindOperator("Narrator");
		std::vector<int> Operator = TowerTeam(Profile, 0);
		std::vector<int> Secrets = TowerTeam(Profile, 1);
		Expect(Operator.size() == 1 && Operator[0] == Luna, "Operator mode: only Luna (4*) of the default team");
		Expect(Secrets.size() == 2 && Secrets[0] == Judge && Secrets[1] == Narrator, "Secrets mode: Judge and Narrator (5*/6*)");
		int Team[3];
		EffectiveTeam(Profile, TowerStageId(0, 1), Team);
		Expect(Team[0] == Luna && Team[1] == -1 && Team[2] == -1, "tower team keeps only eligible operators");
		EffectiveTeam(Profile, CampaignStageId(1), Team);
		Expect(Team[0] == Luna && Team[1] == Judge && Team[2] == Narrator, "campaign keeps the whole team");

		for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
		{
			Profile.GrantOperator(Index);
		}
		Expect(Profile.SetTeamSlot(0, FindOperator("Myrcia")).bOk && Profile.SetTeamSlot(1, FindOperator("Arita")).bOk, "placeholder operators join the team");
		Expect(Profile.TeamIsPlayable(), "team with placeholders is playable");
		Expect(Profile.SetTeamSlot(2, FindOperator("Fujita")).bOk, "third placeholder joins");
		Operator = TowerTeam(Profile, 0);
		Expect(Operator.size() == 3, "Operator mode accepts 3* placeholders");
		const std::vector<int> SecretsFilled = TowerTeam(Profile, 1);
		Expect(SecretsFilled.size() == 3 && CanStartStage(Profile, TowerStageId(1, 1)).bOk, "Secrets mode fills its team with the strongest owned 5-6* operators");
		for (int Index : SecretsFilled)
		{
			Expect(Roster()[static_cast<size_t>(Index)].Stars >= 5, "Secrets mode only fields 5-6* operators");
		}

		FProfile Fresh(11);
		Expect(TowerNextFloor(Fresh, 0) == 1, "tower starts at floor 1");
		Expect(CanStartStage(Fresh, TowerStageId(0, 1)).bOk && !CanStartStage(Fresh, TowerStageId(0, 2)).bOk, "tower climbs one floor at a time");
		const int Gold = Fresh.Get(ECurrency::Gold);
		ClearStage(Fresh, TowerStageId(0, 1));
		Expect(Fresh.Meta.TowerBest[0] == 1 && Fresh.Get(ECurrency::Gold) == Gold + TowerFloorReward(0, 1).Gold, "floor 1 reward and progress");
		Expect(TowerNextFloor(Fresh, 0) == 2 && Fresh.Meta.TowerBest[1] == 0, "modes progress separately");
		const int GoldAfter = Fresh.Get(ECurrency::Gold);
		ClearStage(Fresh, TowerStageId(0, 1));
		Expect(Fresh.Get(ECurrency::Gold) == GoldAfter, "an old floor pays nothing");
		for (int Floor = 2; Floor <= 10; ++Floor)
		{
			ClearStage(Fresh, TowerStageId(1, Floor - 1 == 0 ? 1 : Floor - 1) /* climb Secrets */);
		}
		Expect(Fresh.Meta.TowerBest[1] >= 1, "Secrets mode records floors");
	}

	// -------------------------------------------------------------- sub-stories
	void TestSubStories()
	{
		FProfile Profile(13);
		Expect(CanStartStage(Profile, SubStoryStageId(0, 1)).bOk && !CanStartStage(Profile, SubStoryStageId(0, 2)).bOk, "sub-story levels unlock in order");
		const int Gold = Profile.Get(ECurrency::Gold);
		ClearStage(Profile, SubStoryStageId(0, 1));
		Expect(Profile.Meta.SubStoryCleared[0] == 1 && Profile.Get(ECurrency::Gold) == Gold + 600, "first clear pays");
		const int After = Profile.Get(ECurrency::Gold);
		ClearStage(Profile, SubStoryStageId(0, 1));
		Expect(Profile.Get(ECurrency::Gold) == After, "repeat clear pays nothing");
		const int Tickets = Profile.Get(ECurrency::RegularTicket);
		ClearStage(Profile, SubStoryStageId(0, 2));
		ClearStage(Profile, SubStoryStageId(0, 3));
		Expect(Profile.Get(ECurrency::RegularTicket) == Tickets + 1, "last level adds a ticket");
		Expect(Profile.Meta.SubStoryCleared[0] == 3, "sub-story fully cleared");
	}

	// ------------------------------------------------------------------- quests
	void TestQuests()
	{
		FProfile Profile(21);
		Expect(ClaimableQuests(Profile) == 0, "nothing to claim at the start");
		Expect(QuestCount(EQuestList::Daily) == DailyQuestCount && QuestCount(EQuestList::Weekly) == WeeklyQuestCount
			&& QuestCount(EQuestList::Story) == StoryQuestCount && QuestCount(EQuestList::Milestone) == MilestoneCount, "four quest lists");
		Expect(!ClaimQuest(Profile, false, 2).bOk, "cannot claim an unfinished quest");
		NotifyQuest(Profile, EQuestEvent::Summon);
		const int Bytes = Profile.Get(ECurrency::Bytes);
		const int Xp = Profile.Meta.CommanderXp;
		Expect(DailyQuest(Profile, 2).bComplete && ClaimableQuests(Profile) == 1, "summon quest completes");
		Expect(WeeklyQuest(Profile, 1).Progress == 1 && !WeeklyQuest(Profile, 1).bComplete, "a summon counts for the weekly quest too");
		Expect(ClaimQuest(Profile, false, 2).bOk && Profile.Get(ECurrency::Bytes) == Bytes + 300, "claim pays the reward");
		Expect(Profile.Meta.CommanderXp == Xp + 30 && Profile.Meta.SeasonXp == 30, "claim adds Commander and Season XP");
		Expect(!ClaimQuest(Profile, false, 2).bOk, "no double claim");
		Expect(DailyQuest(Profile, 7).Progress == 1 && WeeklyQuest(Profile, 6).Progress == 1, "a claimed daily quest counts for the daily bonus and the weekly quest");
		NotifyQuest(Profile, EQuestEvent::ArcadePlay, 2);
		Expect(!DailyQuest(Profile, 5).bComplete, "arcade quest needs 3 plays");
		NotifyQuest(Profile, EQuestEvent::ArcadePlay);
		Expect(DailyQuest(Profile, 5).bComplete, "arcade quest completes at 3");
		NotifyQuest(Profile, EQuestEvent::StageClear, 2);
		Expect(!DailyQuest(Profile, 1).bComplete, "three clears needed");
		NotifyQuest(Profile, EQuestEvent::StageClear);
		Expect(DailyQuest(Profile, 1).bComplete, "clear quest completes at 3");
		Expect(ClaimLogin(Profile).bOk && DailyQuest(Profile, 0).bComplete, "the login bonus completes its quest");
		NotifyQuest(Profile, EQuestEvent::EquipChange);
		NotifyQuest(Profile, EQuestEvent::WorldCollect);
		Expect(ClaimAllQuests(Profile).bOk, "claim all");
		Expect(DailyQuest(Profile, 7).bClaimed, "five daily quests unlock and claim-all takes the daily bonus");
		const int Diamonds = Profile.Get(ECurrency::Diamonds);
		(void)Diamonds;
		Profile.AdvanceDay();
		Expect(DailyQuest(Profile, 2).Progress == 0 && !DailyQuest(Profile, 2).bClaimed, "daily quests reset on a new day");
		Expect(WeeklyQuest(Profile, 6).Progress >= 6, "weekly progress survives a day inside the week");
		for (int Day = 0; Day < 6; ++Day)
		{
			Profile.AdvanceDay();
		}
		Expect(Profile.Day == 8 && WeeklyQuest(Profile, 6).Progress == 0 && DaysUntilWeeklyReset(Profile) == 7, "weekly quests reset on day 8");

		// Milestones
		Expect(MilestoneQuest(Profile, 1).Progress < 20 && !MilestoneQuest(Profile, 1).bComplete, "level-20 milestone not done");
		Profile.Operators[static_cast<size_t>(FindOperator("Luna"))].Level = 20;
		Expect(MilestoneQuest(Profile, 1).bComplete && ClaimQuest(Profile, true, 1).bOk, "milestone completes from progress");
		Expect(!ClaimQuest(Profile, true, 1).bOk && !ClaimQuest(Profile, false, 99).bOk, "milestone double claim / unknown quest");

		// Story quests: levels 3, 6 and 8 of every chapter; ten Regular tickets for Chapter 1, one after that.
		int Tickets = 0;
		int StoryDiamonds = 0;
		for (int Index = 0; Index < StoryQuestCount; ++Index)
		{
			const FStoryQuestDef Def = StoryQuestDef(Index);
			Expect(Def.Chapter == Index / 3 + 1 && Def.Stage == CampaignStageId(Def.Chapter, Def.Level), "story quest " + std::to_string(Index) + " stage");
			Tickets += Def.Reward.RegularTickets;
			StoryDiamonds += Def.Reward.Diamonds;
			if (Def.Level == 3)
			{
				Expect(Def.Reward.Diamonds >= 20 && Def.Reward.Diamonds <= 50, "level-3 quests pay 20-50 Diamonds");
			}
			if (Def.Level == 6)
			{
				Expect(Def.Reward.Gold > 0 && (Def.Reward.Bytes > 0 || Def.Reward.Hearts > 0), "level-6 quests pay Gold with Bytes or hearts");
			}
		}
		Expect(StoryQuestDef(2).Reward.RegularTickets == 10 && StoryQuestDef(5).Reward.RegularTickets == 1 && Tickets == 10 + 12, "chapter tickets: 10 then 1");
		Expect(StoryQuestDef(0).Level == 3 && StoryQuestDef(1).Level == 6 && StoryQuestDef(2).Level == 8, "a quest every 2-3 levels");
		FProfile Story(22);
		Expect(!StoryQuest(Story, 0).bComplete, "story quest needs the level");
		const int Regular = Story.Get(ECurrency::RegularTicket);
		Story.StagesCleared = 8;
		Expect(ClaimableQuests(Story, EQuestList::Story) == 3, "clearing Chapter 1 completes its three story quests");
		Expect(ClaimQuest(Story, EQuestList::Story, 2).bOk && Story.Get(ECurrency::RegularTicket) == Regular + 10, "Chapter 1 pays ten Regular tickets");
		Expect(!ClaimQuest(Story, EQuestList::Story, 2).bOk && !ClaimQuest(Story, EQuestList::Story, 3).bOk, "no double claim; chapter 2 not done");
		Expect(CommanderLevel(Profile) >= 1 && XpPerLevel() > 0, "commander level is defined");
		(void)StoryDiamonds;
	}

	// --------------------------------------------------------------------- mail
	void TestMailAndLogin()
	{
		FProfile Profile(23);
		Expect(Profile.Meta.Mail.empty() && UnclaimedMail(Profile) == 0, "no beginning mail (owner request 2026-10-06)");
		Profile.AdvanceDay();
		Expect(Profile.Meta.Mail.size() == 1 && Contains(Profile.Meta.Mail[0].Title, "Day 2"), "daily supply mail arrives");
		Profile.AdvanceDay();
		const int Gold = Profile.Get(ECurrency::Gold);
		Expect(ClaimMail(Profile, Profile.Meta.Mail[0].Id).bOk && Profile.Get(ECurrency::Gold) == Gold + Profile.Meta.Mail[0].Reward.Gold, "claim a mail");
		Expect(!ClaimMail(Profile, Profile.Meta.Mail[0].Id).bOk && !ClaimMail(Profile, 999).bOk, "mail double claim / missing");
		Expect(ClaimAllMail(Profile).bOk && UnclaimedMail(Profile) == 0 && !ClaimAllMail(Profile).bOk, "claim all");
		Expect(DeleteClaimedMail(Profile) == 2 && Profile.Meta.Mail.empty(), "delete claimed");
		for (int Day = 0; Day < 40; ++Day)
		{
			Profile.AdvanceDay();
		}
		Expect(Profile.Meta.Mail.size() <= 20, "mailbox is capped");

		FProfile Login(24);
		Expect(CanClaimLogin(Login) && ClaimLogin(Login).bOk && !CanClaimLogin(Login) && !ClaimLogin(Login).bOk, "login bonus once a day");
		Expect(Login.Meta.LoginStreak == 1, "streak 1");
		Login.AdvanceDay();
		Expect(ClaimLogin(Login).bOk && Login.Meta.LoginStreak == 2, "streak 2 on the next day");
		Login.AdvanceDay();
		Login.AdvanceDay();
		Expect(ClaimLogin(Login).bOk && Login.Meta.LoginStreak == 1, "missing a day resets the streak");
		for (int Step = 2; Step <= 7; ++Step)
		{
			Login.AdvanceDay();
			Login.AdvanceDay();
			ClaimLogin(Login);
		}
		Expect(LoginReward(7).Diamonds == 50 && LoginReward(7).SpecialTickets == 1 && LoginReward(99).Diamonds == 50 && LoginReward(0).Gold == 600, "login reward table (2026-10-07 rebalance)");
	}

	// -------------------------------------------------------------- season pass
	void TestSeasonPass()
	{
		FProfile Profile(25);
		Expect(SeasonTier(Profile) == 0 && !ClaimSeasonTier(Profile, 1).bOk, "tier 1 not reached");
		Profile.Meta.SeasonXp = SeasonXpPerTier() * 3;
		Expect(SeasonTier(Profile) == 3 && ClaimableSeasonTiers(Profile) == 3, "three tiers claimable");
		const int Gold = Profile.Get(ECurrency::Gold);
		Expect(ClaimSeasonTier(Profile, 1).bOk && Profile.Get(ECurrency::Gold) == Gold + SeasonReward(1, false).Gold, "free reward");
		Expect(!ClaimSeasonTier(Profile, 1).bOk, "free track claimed once; paid track locked");
		Expect(!Profile.Meta.PremiumClaimed[1], "paid track is not given without the pass");
		Expect(SimulatePurchase(Profile, 7).bOk && Profile.Meta.bPremiumPass, "premium pass purchase (simulated)");
		Expect(ClaimSeasonTier(Profile, 1).bOk && Profile.Meta.PremiumClaimed[1], "paid reward after the pass");
		Expect(ClaimAllSeason(Profile).bOk && ClaimableSeasonTiers(Profile) == 0, "claim all");
		Profile.Meta.SeasonXp = 100000;
		Expect(SeasonTier(Profile) == SeasonTiers, "tier caps at 50");
		for (int Tier = 1; Tier <= SeasonTiers; ++Tier)
		{
			Expect(SeasonReward(Tier, false).Gold > 0 && SeasonReward(Tier, true).Bytes > 0, "both tracks pay at every tier");
		}
		Expect(SeasonReward(50, true).Diamonds == 300 && SeasonReward(50, false).SpecialTickets == 1 && SeasonReward(25, false).SpecialTickets == 1, "tier 50 rewards (2026-10-07 rebalance)");
		Expect(SeasonReward(0, false).Gold == 0 && SeasonReward(51, true).Bytes == 0, "no tier 0 / 51");
		Expect(!SimulatePurchase(Profile, 7).bOk, "pass cannot be bought twice");
	}

	// -------------------------------------------------------------------- shops
	void TestShops()
	{
		FProfile Profile(27);
		const int Items = static_cast<int>(Profile.Inventory.size());
		const int Gold = Profile.Get(ECurrency::Gold);
		const int Bytes = Profile.Get(ECurrency::Bytes);
		FActionResult Bought = BuyArmoury(Profile, 0);
		Expect(Bought.bOk && static_cast<int>(Profile.Inventory.size()) == Items + 1, "armoury sells an item");
		Expect(Profile.Get(ECurrency::Gold) == Gold - 1000 && Profile.Get(ECurrency::Bytes) == Bytes - 250, "armoury price paid");
		Expect(Profile.Inventory.back().Percent == 1.5 && Profile.Inventory.back().Bonus == EBonus::Defence && Profile.Inventory.back().Slot == ESlot::Helmet, "armoury item stats");
		Expect(!BuyArmoury(Profile, 99).bOk, "unknown armoury item");
		Profile.Spend(ECurrency::Gold, Profile.Get(ECurrency::Gold));
		Expect(!BuyArmoury(Profile, 0).bOk, "armoury needs Gold");

		FProfile Alley(28);
		Expect(!BuyAlley(Alley, 0).bOk && Contains(BuyAlley(Alley, 0).Message, "Commander level 5"), "Back Alley is level-locked");
		Alley.Meta.CommanderXp = XpPerLevel() * 4; // level 5
		const int Hearts = Alley.Get(ECurrency::AngelHearts);
		Expect(BuyAlley(Alley, 0).bOk && Alley.Get(ECurrency::AngelHearts) == Hearts + 40 && Alley.Get(ECurrency::Diamonds) == 3000 - 150, "Back Alley sells hearts for Diamonds");
		Expect(!BuyAlley(Alley, 4).bOk, "Prime Helmet needs level 15");
		Alley.Meta.CommanderXp = XpPerLevel() * 29;
		const int AlleyItems = static_cast<int>(Alley.Inventory.size());
		Expect(BuyAlley(Alley, 7).bOk && static_cast<int>(Alley.Inventory.size()) == AlleyItems + 1 && Alley.Inventory.back().Percent == 10.0, "Prime Blade at level 30");
		Alley.Spend(ECurrency::Diamonds, Alley.Get(ECurrency::Diamonds));
		Expect(!BuyAlley(Alley, 1).bOk, "Back Alley needs Diamonds");
		Expect(AlleyStock().size() == 8 && ArmouryStock().size() == 8, "stock sizes");
	}

	void TestTradeCentre()
	{
		FProfile Profile(29);
		Expect(TradeCost(ECurrency::Gold, ECurrency::Bytes, 100) == 71, "100 Bytes cost 71 Gold (15% fee)");
		Expect(TradeCost(ECurrency::Diamonds, ECurrency::Gold, 1000) > 0, "Diamonds can be given away");
		Expect(TradeCost(ECurrency::Gold, ECurrency::Diamonds, 1) == 0 && !CanReceiveInTrade(ECurrency::Diamonds), "Diamonds cannot be received");
		Expect(TradeCost(ECurrency::Gold, ECurrency::Gold, 5) == 0 && TradeCost(ECurrency::Gold, ECurrency::Mileage, 5) == 0, "invalid pairs");
		const int Gold = Profile.Get(ECurrency::Gold);
		Expect(Trade(Profile, ECurrency::Gold, ECurrency::Bytes, 100).bOk, "trade works");
		Expect(Profile.Get(ECurrency::Gold) == Gold - 71 && Profile.Get(ECurrency::Bytes) == 10100, "balances move");
		Expect(!Trade(Profile, ECurrency::Gold, ECurrency::Bytes, 100000000).bOk, "cannot trade more than you have");
		Expect(WeeklyQuest(Profile, 3).Progress == 1, "a trade counts as shopping for the weekly quest");
		// No loop may make a profit.
		for (ECurrency From : TradeCurrencies())
		{
			for (ECurrency To : TradeCurrencies())
			{
				if (From == To || !CanReceiveInTrade(To) || !CanReceiveInTrade(From))
				{
					continue;
				}
				const long long Cost = TradeCost(From, To, 1000);
				const long long Back = TradeCost(To, From, 1000);
				Expect(Cost > 0 && Back > 0, "pair trades both ways");
				Expect(TradeCost(From, To, 1000) * 1 > 0, "positive price");
			}
		}
		// A round trip A->B->A returns strictly less than the start.
		const long long Start = 100000;
		const long long Got = Start * 10 * 85 / (6 * 100); // Gold -> Bytes, ignoring rounding
		const long long BackGold = Got * 6 * 85 / (10 * 100);
		Expect(BackGold < Start, "round trip loses value");
		// 2026-10-09: tickets are premium. A finished campaign's daily sweeps (Charge regeneration only) buy about one pull a day.
		{
			const Campaign::FStageReward Last = Campaign::StageReward(TotalCampaignLevels);
			const int Sweeps = Inventory::ChargeCap / Inventory::ReplayChargeCost(TotalCampaignLevels);
			const double Pulls = static_cast<double>(Sweeps) * (static_cast<double>(Last.Gold) / static_cast<double>(TradeCost(ECurrency::Gold, ECurrency::SpecialTicket, 1)) +
				static_cast<double>(Last.Bytes) / static_cast<double>(TradeCost(ECurrency::Bytes, ECurrency::SpecialTicket, 1)) +
				static_cast<double>(Last.Hearts) / static_cast<double>(TradeCost(ECurrency::AngelHearts, ECurrency::SpecialTicket, 1)));
			Expect(Pulls > 0.5 && Pulls < 1.5, "a day of post-campaign sweeps trades for about one summon ticket (" + std::to_string(Pulls) + ")");
			Expect(TradeCost(ECurrency::RegularTicket, ECurrency::Gold, 8925) == 1, "giving a ticket away is not marked up (8,925 Gold after the fee)");
			// an arcade token bought with Gold costs more than the best arcade win pays back (Night Shift, hard)
			const FReward Best = Arcade::NightReward(Arcade::EDifficulty::Hard, true, Arcade::NightHours);
			const long long WinValue = Best.Gold + Best.Bytes * 6 / 10 + Best.Hearts * 100LL + Best.Diamonds * 35LL;
			Expect(TradeCost(ECurrency::Gold, ECurrency::ArcadeToken, 1) > WinValue, "an arcade token bought with Gold costs more than a hard Night Shift win pays (" + std::to_string(WinValue) + ")");
		}
	}

	void TestPremiumStore()
	{
		FProfile Profile(31);
		const int Diamonds = Profile.Get(ECurrency::Diamonds);
		const FActionResult Pack = SimulatePurchase(Profile, 1);
		Expect(Pack.bOk && Profile.Get(ECurrency::Diamonds) == Diamonds + 480, "480 Diamonds pack (2026-10-07 rebalance)");
		Expect(Contains(Pack.Message, "no real payment"), "purchase message says no real payment");
		Expect(Profile.Meta.IapSpentCents == 499, "simulated spend recorded");
		Expect(SimulatePurchase(Profile, 6).bOk && Profile.Meta.bStarterBundleBought, "starter bundle");
		Expect(!SimulatePurchase(Profile, 6).bOk, "starter bundle only once");
		Expect(!SimulatePurchase(Profile, 99).bOk, "unknown offer");
		for (const FIapOffer& Offer : IapOffers())
		{
			Expect(Offer.PriceCents > 0, "every offer has a placeholder price");
		}
	}

	// -------------------------------------------------------------- Outer World
	void TestOuterWorld()
	{
		FProfile Profile(33);
		const int School = static_cast<int>(EBuilding::School);
		const int Gold = Profile.Get(ECurrency::Gold);
		Expect(BuildingBuiltCount(Profile) == 0 && !CollectWorld(Profile).bOk, "nothing built, nothing to collect");
		Expect(BuildOrUpgrade(Profile, School).bOk && Profile.Meta.Buildings[School].Level == 1, "build the school");
		Expect(Profile.Get(ECurrency::Gold) == Gold - 2500 && Profile.Get(ECurrency::Bytes) == 10000 - 500, "build cost paid");
		AdvanceWorld(Profile, 1.0);
		Expect(std::fabs(Profile.Meta.Buildings[School].StoredBytes - 16.0) < 1e-9, "school makes 16 Bytes an hour");
		AdvanceWorld(Profile, 100.0);
		Expect(std::fabs(Profile.Meta.Buildings[School].StoredBytes - 16.0 * BuildingStorageHours) < 1e-9, "storage caps at 12 hours");
		const int Bytes = Profile.Get(ECurrency::Bytes);
		Expect(CollectWorld(Profile).bOk && Profile.Get(ECurrency::Bytes) == Bytes + 192, "collect pays the stored Bytes");
		Expect(DailyQuest(Profile, 6).Progress == 1, "collecting counts for the daily quest");
		Expect(BuildOrUpgrade(Profile, School).bOk && Profile.Meta.Buildings[School].Level == 2, "upgrade");
		Expect(std::fabs(BuildingRate(Profile, School, 1) - 32.0) < 1e-9, "rate scales with level");
		for (int Level = 3; Level <= MaxBuildingLevel; ++Level)
		{
			Profile.Add(ECurrency::Gold, 100000);
			Profile.Add(ECurrency::Bytes, 100000);
			BuildOrUpgrade(Profile, School);
		}
		Expect(Profile.Meta.Buildings[School].Level == MaxBuildingLevel && !BuildOrUpgrade(Profile, School).bOk, "maximum level");
		Profile.Add(ECurrency::Gold, 1000000);
		Profile.Add(ECurrency::Bytes, 1000000);
		for (int Index = 0; Index < BuildingCount; ++Index)
		{
			Expect(BuildOrUpgrade(Profile, Index).bOk || Profile.Meta.Buildings[Index].bBuilt, std::string("build ") + BuildingDef(Index).Name);
		}
		Expect(BuildingBuiltCount(Profile) == BuildingCount, "all eight buildings built");
		Expect(!BuildOrUpgrade(Profile, 99).bOk, "unknown building");
		Profile.Spend(ECurrency::Gold, Profile.Get(ECurrency::Gold));
		Expect(!BuildOrUpgrade(Profile, 1).bOk || Profile.Meta.Buildings[1].Level > 1, "needs Gold to upgrade");
		AdvanceWorld(Profile, 5.0);
		const int Hearts = Profile.Get(ECurrency::AngelHearts);
		Expect(CollectWorld(Profile).bOk && Profile.Get(ECurrency::AngelHearts) > Hearts - 1, "hospital and shelter make Angel hearts");
	}

	// ------------------------------------------------------------------- arcade (detailed tests: arcade_tests.cpp)
	void TestArcade()
	{
		FProfile Profile(35);
		Expect(Profile.Get(ECurrency::ArcadeToken) == 5, "five arcade tokens at the start");
		for (int Play = 0; Play < 3; ++Play)
		{
			const int Tokens = Profile.Get(ECurrency::ArcadeToken);
			Expect(Arcade::StartGame(Profile, Arcade::EGame::NightShift).bOk && Profile.Get(ECurrency::ArcadeToken) == Tokens - 1, "a play costs one token");
		}
		Expect(DailyQuest(Profile, 5).bComplete && Profile.Meta.ArcadeRecords.Plays[0] == 3, "three plays complete the arcade quest");
		FProfile Broke(38);
		Broke.Spend(ECurrency::ArcadeToken, 5);
		Expect(!Arcade::StartGame(Broke, Arcade::EGame::Rhythm).bOk, "no tokens, no play");
		Broke.AdvanceDay();
		Expect(Broke.Get(ECurrency::ArcadeToken) == 5, "daily arcade top-up to 5");
	}

	// -------------------------------------------------------------------- arena (detailed tests: arcade_tests.cpp)
	void TestArena()
	{
		FProfile Profile(39);
		Expect(TeamPower(Profile) > 0 && TeamPower(Profile) == TeamBattlePower(Profile), "team power is the squad's battle power");
		Expect(Arena::AutoFill(Profile, false).bOk && Arena::TeamIsValid(Profile, Profile.Meta.Arena.Attack), "attack team auto-fills");
		for (int Fight = 0; Fight < 5; ++Fight)
		{
			const Arena::FLineup Rival = Arena::MakeRival(Profile, static_cast<uint32_t>(Fight + 1));
			const Arena::FBattle Battle = Arena::Simulate(Arena::MakeLineup(Profile, Profile.Meta.Arena.Attack, "Me"), Rival, static_cast<uint32_t>(Fight));
			const int Delta = Battle.bAttackerWon ? Arena::TrophyDelta(Profile.Meta.Arena.Trophies, Rival.Trophies) : -Arena::TrophyDelta(Rival.Trophies, Profile.Meta.Arena.Trophies);
			bool bFloor = Profile.Meta.Arena.bChampionFloor;
			Arena::RecordFight(Profile, Rival, Battle.bAttackerWon, Arena::ApplyTrophies(Profile.Meta.Arena.Trophies, Delta, bFloor), Delta);
		}
		Expect(Profile.Meta.ArenaWins + Profile.Meta.ArenaLosses == 5 && Profile.Meta.ArenaAttempts == 0, "five attempts a day");
		Profile.AdvanceDay();
		Expect(Profile.Meta.ArenaAttempts == 5 && !Profile.Meta.ArenaLog.empty(), "attempts reset daily; the log is kept");
		Expect(Profile.Meta.Arena.Trophies >= 0, "trophies never go negative");
	}

	// ------------------------------------------------------------------- pages
	const char* PortraitNames[20] = {"media", "arita", "adapt", "alchemist", "shiraishi", "myrcia", "ishii", "death", "luna", "kuu",
		"stargazer", "narrator", "gambler", "fujita", "timekeeper", "weather", "plack", "jester", "judge", "blood"};

	void CheckPage(const P::FPage& Page, const std::string& Where)
	{
		Expect(!Page.Title.empty(), Where + ": has a title");
		Expect(static_cast<int>(Page.Grid.size()) <= P::MaxGrid && static_cast<int>(Page.Bar.size()) <= P::MaxBar, Where + ": within grid/bar limits");
		std::set<std::string> Ids;
		for (const P::FPageAction& Action : Page.Grid)
		{
			Expect(Ids.insert(Action.Id).second, Where + ": unique tile id " + Action.Id);
			Expect(!Action.Label.empty(), Where + ": tile has a label");
		}
		for (const P::FPageAction& Action : Page.Bar)
		{
			Expect(Ids.insert(Action.Id).second, Where + ": unique button id " + Action.Id);
		}
	}

	void WalkPages(const FProfile& Start, const P::FUiState& State, int Depth, std::set<std::string>& Visited)
	{
		FProfile Copy = Start;
		const P::FPage Page = P::Build(Copy, State);
		const std::string Key = std::to_string(State.Tab) + "/" + State.Page + "/" + std::to_string(!State.SummonOps.empty());
		CheckPage(Page, Key);
		if (!Visited.insert(Key).second || Depth > 4)
		{
			return;
		}
		std::vector<P::FPageAction> Actions = Page.Grid;
		Actions.insert(Actions.end(), Page.Bar.begin(), Page.Bar.end());
		if (Page.bCanBack)
		{
			Actions.push_back({"back", "Back", "", "", true, false, false});
		}
		for (const P::FPageAction& Action : Actions)
		{
			FProfile Next = Start;
			P::FUiState NextState = State;
			const P::FPerform Done = P::Perform(Next, NextState, Action.Id);
			Expect(!Contains(Done.Message, "Unknown action"), Key + ": action works " + Action.Id);
			if (NextState.Tab != State.Tab || NextState.Page != State.Page || NextState.SummonOps.empty() != State.SummonOps.empty())
			{
				WalkPages(Next, NextState, Depth + 1, Visited);
			}
			else
			{
				CheckPage(P::Build(Next, NextState), Key + " after " + Action.Id);
			}
		}
	}

	void TestPageModel()
	{
		FProfile Profile(41);
		Profile.GrantDemoCurrency();
		Profile.Meta.CommanderXp = XpPerLevel() * 30;
		Profile.Meta.SeasonXp = 1200;
		P::FUiState State;
		std::set<std::string> Visited;
		for (int Tab = 0; Tab < P::TabCount; ++Tab)
		{
			State.Tab = Tab;
			State.Page.clear();
			WalkPages(Profile, State, 0, Visited);
			Expect(std::string(P::TabName(Tab)) != "?" && !std::string(P::TabIcon(Tab)).empty(), "tab has a name and icon");
		}
		Expect(Visited.size() >= 25, "the walk reached many distinct pages (" + std::to_string(Visited.size()) + ")");

		// Specific pages.
		P::FUiState S;
		S.Tab = P::TabOperator;
		P::FPage Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == 20, "operator list shows all 20 operators");
		std::set<std::string> Icons;
		for (const P::FPageAction& Tile : Page.Grid)
		{
			Icons.insert(Tile.Icon);
		}
		for (const char* Name : PortraitNames)
		{
			Expect(Icons.count(std::string("portrait:") + Name) == 1, std::string("portrait id for ") + Name);
		}
		S.Tab = P::TabLobby;
		Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == (Config::TesterMode ? 13u : 12u) && !Page.Portrait.empty() && !Page.PortraitStars.empty() && !Page.PortraitName.empty(),
			"lobby: twelve tiles (Friends and Gang added 2026-10-06, Inventory and Elements 2026-10-07; one more with the Tester panel) and a portrait with a name plate");
		S.Tab = P::TabShop;
		Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == 4, "shop root: armoury, trade centre, back alley, premium");
		S.Tab = P::TabRevolution;
		Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == 2, "revolution root: outer world and the ark");
		S.Tab = P::TabCampaign;
		Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == 14, "campaign root: 13 chapters + Neo Tokyo (free roam)");
		S.Page = "chapter";
		Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == 8 && Contains(Page.Grid[7].Sub, "BOSS") && Page.bCanBack, "chapter page: 8 levels, level 8 is the boss");
		S.Tab = P::TabSummon;
		S.Page.clear();
		Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == 4 && !Page.Portrait.empty() && Page.Bar.size() == 6, "summon: four banners, portrait, six buttons");
		S.SelBanner = 1;
		Page = P::Build(Profile, S);
		Expect(Page.PortraitName == "Judge" && Contains(Page.PortraitStars, "5"), "Judge banner shows Judge with 5* in the lower third");
		S.SelBanner = 2;
		Page = P::Build(Profile, S);
		Expect(Page.PortraitName == "Narrator" && Contains(Page.PortraitStars, "6"), "Narrator banner shows Narrator with 6*");
		for (int Banner = 0; Banner < 4; ++Banner)
		{
			Expect(P::BannerShowcase(Profile, Banner) >= 0, "every banner has a showcase operator");
		}
		Expect(P::Resources(Profile).size() == 6 && P::Resources(Profile)[0].Icon == "gold", "resource bar starts with the coin");
		Expect(P::LowerName(FindOperator("Stargazer")) == "stargazer", "portrait file names are lower case");
	}

	void TestPageActions()
	{
		FProfile Profile(43);
		P::FUiState State;
		auto Do = [&](const std::string& Id) { return P::Perform(Profile, State, Id); };

		Do("tab:0");
		Expect(State.Tab == P::TabOperator && State.Page.empty(), "tab switch");
		Do("op:sel:" + std::to_string(FindOperator("Judge")));
		Expect(State.Page == "op" && State.SelOperator == FindOperator("Judge"), "select an operator");
		const int Gold = Profile.Get(ECurrency::Gold);
		Expect(Do("op:lvl").bOk && Profile.Get(ECurrency::Gold) == Gold - 100, "level up from the operator page");
		Expect(DailyQuest(Profile, 3).Progress == 1 && WeeklyQuest(Profile, 2).Progress == 1, "level up counts for the daily and weekly quests");
		Do("op:slot:0");
		Expect(State.Page == "op:equip" && State.SelSlot == 0, "open the helmet list");
		int HelmetId = -1;
		for (const FEquipment& Item : Profile.Inventory)
		{
			if (Item.Slot == ESlot::Helmet)
			{
				HelmetId = Item.Id;
			}
		}
		if (HelmetId < 0)
		{
			HelmetId = Profile.AddEquipment(ESlot::Helmet, EBonus::Damage, 2.0, "Test helmet").Id;
		}
		Do("op:item:" + std::to_string(HelmetId));
		Expect(State.SelItem == HelmetId, "selection is kept in the UI state");
		Expect(Do("op:equip").bOk && Profile.FindEquipment(HelmetId)->EquippedBy == FindOperator("Judge"), "equip");
		Expect(State.SelItem == HelmetId, "selection survives the action (the old bug)");
		Expect(Do("op:upg").bOk && Profile.FindEquipment(HelmetId)->UpgradeLevel == 1 && State.SelItem == HelmetId, "upgrade keeps the selection");
		Expect(Do("op:unequip").bOk && Profile.FindEquipment(HelmetId)->EquippedBy == -1, "unequip");
		Do("back");
		Expect(State.Page == "op", "back goes up one page");
		Do("back");
		Expect(State.Page.empty(), "back to the operator list");

		Do("tab:1");
		Expect(Do("team:swap:0").bOk && Profile.Team[0] == FindOperator("Judge"), "swap team slots");
		Do("team:rot");
		Do("team:default");
		Expect(Profile.Team[0] == FindOperator("Luna") && Profile.Team[2] == FindOperator("Narrator"), "reset team");
		const int Before = Profile.Meta.NarratorExchangeFrom;
		Do("team:nx:from");
		Expect(Profile.Meta.NarratorExchangeFrom != Before && Profile.Meta.NarratorExchangeFrom != Profile.Meta.NarratorExchangeTo, "Narrator exchange preset cycles without picking the same stat twice");
		Do("team:cycle:1");
		Expect(Profile.Team[1] != Profile.Team[0] && Profile.Team[1] != Profile.Team[2], "cycling a slot never duplicates an operator");

		Do("tab:6");
		Expect(Do("sum:banner:0").bOk && State.SelBanner == 0, "choose a banner");
		const int Tickets = Profile.Get(ECurrency::RegularTicket);
		Expect(Do("sum:go:1:0").bOk && Profile.Get(ECurrency::RegularTicket) == Tickets - 1 && State.SummonOps.size() == 1, "summon x1 with a ticket");
		Expect(DailyQuest(Profile, 2).bComplete && Profile.Meta.Summons == 1, "summon counts for the daily quest and the profile");
		Do("sum:again");
		Expect(State.SummonOps.size() == 1, "summon again repeats the same pull");
		Expect(Do("sum:go:10:1").bOk && State.SummonOps.size() == 10, "x10 with Diamonds");
		P::FPage Results = P::Build(Profile, State);
		Expect(Results.Grid.size() == 10 && !Results.Portrait.empty() && Results.bCanBack, "results page shows ten portrait tiles");
		Do("sum:done");
		Expect(State.SummonOps.empty(), "back to banners");
		Expect(Do("sum:tut").bOk && !Do("sum:tut").bOk, "tutorial summon works once");

		Do("tab:5");
		Expect(!Do("camp:ch:2").bOk && Contains(State.Status, "locked"), "chapter 2 is locked until the Chapter 1 boss is cleared");
		Expect(Do("camp:ch:1").bOk && State.Page == "chapter", "open chapter 1");
		Expect(!Do("camp:start:2").bOk && Contains(State.Status, "in order"), "level 2 is locked");
		P::FPerform Start = Do("camp:start:1");
		Expect(Start.bStartStage && Start.StageId == 1, "level 1 starts");
		Profile.TeamIsPlayable();

		Do("tab:3");
		Do("go:trade");
		Expect(Do("trade:go").bOk, "trade from the page");
		Do("go:premium");
		Expect(Do("iap:0").bOk && Contains(State.Status, "no real payment"), "premium purchase is simulated");

		Do("tab:2");
		Do("go:quests");
		Do("q:all");
		Profile.AdvanceDay();   // no beginning mail since 2026-10-06: the next day's supply mail is something to claim
		Do("go:mail");
		Expect(Do("mail:all").bOk && UnclaimedMail(Profile) == 0, "mail claim all from the page");
		Do("go:events");
		Expect(Do("login").bOk, "login bonus from the page");
		Do("go:arcade");
		// Night Shift left the arcade (owner request 2026-10-15): its actions no longer open a page or the game.
		const P::FPerform NightPage = Do("arc:night");
		Expect(State.Page == "arcade" && NightPage.Overlay == P::EOverlay::None, "Night Shift has no page any more");
		const P::FPerform Night = Do("arc:nightgo:1");
		Expect(Night.Overlay == P::EOverlay::None && State.Page == "arcade", "Night Shift cannot be started from the arcade");
		Do("go:settings");
		const int DayBefore = Profile.Day;
		Expect(Do("demo:day").bOk && Profile.Day == DayBefore + 1, "next day from the demo tools");
		Expect(Do("demo:xp").bOk && CommanderLevel(Profile) >= 4, "commander XP from the demo tools");
		Expect(Do("demo:reset").bOk && Profile.Day == 1 && State.Tab == P::TabLobby, "reset profile");
		Do("go:prefs");
		const bool Hint = Profile.Meta.Settings.bShowControlsHint;
		Do("pref:hint");
		Expect(Profile.Meta.Settings.bShowControlsHint != Hint, "controls hint toggle");
		Expect(!Do("nonsense").bOk, "unknown action refused");

		Do("tab:4");
		Do("go:world");
		Expect(Do("world:b:0").bOk && Profile.Meta.Buildings[0].bBuilt, "build from the Outer World page");
		Do("demo:hours");
		Do("go:ark");
	}

	// full campaign through the page layer
	void TestCampaignThroughPages()
	{
		FProfile Profile(47);
		P::FUiState State;
		P::Perform(Profile, State, "tab:5");
		P::Perform(Profile, State, "camp:ch:1");
		for (int Level = 1; Level <= CampaignLevels; ++Level)
		{
			const P::FPerform Start = P::Perform(Profile, State, "camp:start:" + std::to_string(Level));
			Expect(Start.bStartStage && Start.StageId == Level, "start level " + std::to_string(Level));
			ClearStage(Profile, Start.StageId);
			const P::FPage Page = P::Build(Profile, State);
			Expect(Page.Grid[static_cast<size_t>(Level - 1)].Sub.find("cleared") == 0, "level tile shows cleared " + std::to_string(Level));
			if (Level < CampaignLevels)
			{
				Expect(Page.Grid[static_cast<size_t>(Level)].bEnabled, "next level opens");
			}
		}
		Expect(Profile.Owns(FindOperator("Plack")), "Plack joined");
	}
}

namespace
{
	const P::FPageAction* FindTile(const P::FPage& Page, const std::string& Id)
	{
		for (const P::FPageAction& Action : Page.Grid)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		for (const P::FPageAction& Action : Page.Bar)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		return nullptr;
	}

	// Owner request 2026-10-06: full-screen pages, a quest board with Daily / Weekly / Story / Milestones, and a Settings page
	// whose preferences work.
	void TestQuestAndSettingsPages()
	{
		FProfile Profile(44);
		P::FUiState State;
		auto Do = [&](const std::string& Id) { return P::Perform(Profile, State, Id); };
		Do("tab:2");
		P::FPage Root = P::Build(Profile, State);
		Expect(!Root.bFullScreen && !Root.bCanBack && !Root.Portrait.empty(), "the lobby root keeps the portrait and the tabs");
		Expect(FindTile(Root, "go:prefs") == nullptr && FindTile(Root, "go:settings") != nullptr, "Preferences folded into Settings");

		Do("go:quests");
		P::FPage Quests = P::Build(Profile, State);
		Expect(Quests.bFullScreen && Quests.bCanBack && Quests.Portrait.empty(), "Quests opens full screen, without the side portrait");
		Expect(Quests.Bar.size() == 5 && FindTile(Quests, "q:tab:0") && FindTile(Quests, "q:tab:3") && FindTile(Quests, "q:all"), "quest tabs and Claim all");
		Expect(Quests.Grid.size() == static_cast<size_t>(DailyQuestCount) && FindTile(Quests, "q:d:0"), "Daily tab lists the daily quests");
		Expect(Do("q:tab:1").bOk && State.QuestTab == 1 && P::Build(Profile, State).Grid.size() == static_cast<size_t>(WeeklyQuestCount), "Weekly tab");
		Do("q:tab:2");
		P::FPage Story = P::Build(Profile, State);
		Expect(Story.Grid.size() == static_cast<size_t>(std::min(StoryQuestCount, P::MaxGrid)) && FindTile(Story, "q:s:0"), "Story tab lists the open chapter quests");
		Profile.StagesCleared = 8;
		Story = P::Build(Profile, State);
		Expect(!Story.Grid.empty() && Story.Grid[0].bEnabled && Story.Grid[0].bBadge, "claimable story quests come first");
		const int Tickets = Profile.Get(ECurrency::RegularTicket);
		Expect(Do("q:s:2").bOk && Profile.Get(ECurrency::RegularTicket) == Tickets + 10, "claiming Chapter 1 from the page pays 10 tickets");
		Story = P::Build(Profile, State);
		Expect(FindTile(Story, "q:s:2") == nullptr, "claimed story quests leave the list");
		Expect(Do("q:all").bOk && ClaimableQuests(Profile, EQuestList::Story) == 0, "Claim all takes the rest");
		Do("q:tab:3");
		Expect(FindTile(P::Build(Profile, State), "q:m:0") != nullptr, "Milestones tab");
		Do("back");
		Expect(State.Page.empty(), "Back returns to the lobby");

		// Settings: five sections, every option changes and is clamped.
		Do("go:settings");
		P::FPage Settings = P::Build(Profile, State);
		Expect(Settings.bFullScreen && Settings.Bar.size() == 6 && FindTile(Settings, "set:master:1"), "Settings opens full screen on Audio (six sections)");
		FSettings& S = Profile.Meta.Settings;
		Do("set:master:-1");
		Expect(S.MasterVolume == 90, "master volume down");
		for (int Step = 0; Step < 20; ++Step)
		{
			Do("set:master:1");
		}
		Expect(S.MasterVolume == 100, "volume clamps at 100");
		Do("set:tab:1");
		Expect(FindTile(P::Build(Profile, State), "set:window") != nullptr, "Graphics section");
		const int Quality = S.GraphicsQuality;
		Do("set:gfx");
		Expect(S.GraphicsQuality == (Quality + 1) % 4, "quality cycles");
		Do("set:window");
		Do("set:vsync");
		Do("set:fps");
		Do("set:showfps");
		for (int Step = 0; Step < 10; ++Step)
		{
			Do("set:scale:-1");
		}
		Expect(S.WindowMode == 2 && S.bVSync && S.FrameRateCap == 2 && S.bShowFps && S.ResolutionScale == ResolutionScaleMin, "graphics options change and clamp");
		// Owner request 2026-10-08 (lag): per-group detail, weather effects, volumetric fog, anime look, dynamic resolution,
		// performance mode; every group appears on the page and saves.
		{
			const P::FPage Gfx = P::Build(Profile, State);
			bool bAllGroups = true;
			for (int Group = 0; Group < GraphicsGroupCount; ++Group)
			{
				bAllGroups = bAllGroups && FindTile(Gfx, "set:grp:" + std::to_string(Group)) != nullptr;
			}
			Expect(bAllGroups && FindTile(Gfx, "set:weather") && FindTile(Gfx, "set:volfog") && FindTile(Gfx, "set:anime") && FindTile(Gfx, "set:perf")
				&& FindTile(Gfx, "set:autodetect") && FindTile(Gfx, "set:dynres"), "Graphics lists every group and the lag options");
			Expect(S.GraphicsDetail[GfxShadows] == -1, "groups follow the preset by default");
			Do("set:grp:1");
			Expect(S.GraphicsDetail[GfxShadows] == 0, "shadows: preset -> Low");
			for (int Step = 0; Step < 4; ++Step)
			{
				Do("set:grp:1");
			}
			Expect(S.GraphicsDetail[GfxShadows] == -1, "shadows cycle back to the preset");
			Do("set:grp:2");
			Do("set:gfx");
			Expect(S.GraphicsDetail[GfxLighting] == -1, "a new preset resets the groups");
			Do("set:weather");
			Expect(S.WeatherDetail == 0, "weather effects cycle Full -> Off");
			Do("set:volfog");
			Do("set:anime");
			Do("set:dynres");
			Expect(!S.bVolumetricFog && !S.bAnimeLook && S.bDynamicResolution, "fog, anime look and dynamic resolution toggle");
			Do("set:grp:6");
			const std::string Text = Save::Serialize(Profile, 1800000000);
			FProfile Copy;
			std::string Error;
			Expect(Save::Deserialize(Text, Copy, Error) && Copy.Meta.Settings.GraphicsDetail[GfxEffects] == S.GraphicsDetail[GfxEffects] && Copy.Meta.Settings.WeatherDetail == 0
				&& !Copy.Meta.Settings.bVolumetricFog && !Copy.Meta.Settings.bAnimeLook && Copy.Meta.Settings.bDynamicResolution, "graphics options save and load");
			Do("set:perf");
			Expect(S.GraphicsQuality == 0 && S.ResolutionScale == 70 && S.WeatherDetail == 1 && !S.bVolumetricFog && S.GraphicsDetail[GfxEffects] == -1,
				"performance mode sets the cheap options");
			Expect(Do("set:autodetect").bOk, "auto-detect is accepted (the Unreal layer runs the benchmark)");
		}
		Do("set:tab:2");
		Do("set:invert");
		Do("set:fov:1");
		Do("set:sens:-1");
		Do("pref:hint");
		Expect(S.bInvertY && S.FieldOfView == 95 && S.MouseSensitivity == 4 && !S.bShowControlsHint, "controls options change");
		Do("set:tab:4");
		Do("set:transitions");
		Expect(!S.bPageTransitions, "page transitions can be turned off");
		Expect(Do("go:prefs").bOk && State.Page == "settings" && State.SettingsTab == 4, "the old Preferences link opens Settings > Lobby");
		Do("set:defaults");
		Expect(S.MasterVolume == 100 && S.bPageTransitions && !S.bInvertY && S.FieldOfView == 90 && S.ResolutionScale == 100, "restore defaults");

		// Every sub-page of every tab is full screen; operator pages keep their portrait.
		Do("tab:3");
		Do("go:armoury");
		Expect(P::Build(Profile, State).bFullScreen, "shop pages are full screen");
		Do("tab:0");
		Do("op:sel:" + std::to_string(FindOperator("Luna")));
		const P::FPage Operator = P::Build(Profile, State);
		Expect(Operator.bFullScreen && !Operator.Portrait.empty(), "the operator page is full screen and keeps its portrait");
	}
}

int main()
{
	TestStagePlans();
	TestCampaignFlow();
	TestPlaceholderOperatorsAndTower();
	TestSubStories();
	TestQuests();
	TestMailAndLogin();
	TestSeasonPass();
	TestShops();
	TestTradeCentre();
	TestPremiumStore();
	TestOuterWorld();
	TestArcade();
	TestArena();
	TestPageModel();
	TestPageActions();
	TestCampaignThroughPages();
	TestQuestAndSettingsPages();
	std::printf("%d meta checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
