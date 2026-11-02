// Boldinbeauty demo — operations on the meta-game state (see BIBMetaOps.h). Every number is a PLACEHOLDER.
#include "BIBMetaOps.h"
#include "BIBArcade.h"
#include "BIBArena.h"
#include "BIBCampaign.h"
#include "BIBConfig.h"
#include "BIBElement.h"
#include "BIBEvents.h"
#include "BIBInventory.h"
#include "BIBLiveOps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace BIB
{
	namespace Meta
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
				Result.bOk = false;
				Result.Message = Message;
				return Result;
			}

			std::string Fixed(double Value, int Decimals)
			{
				char Buffer[48];
				std::snprintf(Buffer, sizeof(Buffer), "%.*f", Decimals, Value);
				return Buffer;
			}
		}

		// ======================================================================= text
		std::string Num(long long Value)
		{
			std::string Digits = std::to_string(Value < 0 ? -Value : Value);
			std::string Result;
			int Count = 0;
			for (auto It = Digits.rbegin(); It != Digits.rend(); ++It)
			{
				if (Count > 0 && Count % 3 == 0)
				{
					Result.insert(Result.begin(), ',');
				}
				Result.insert(Result.begin(), *It);
				++Count;
			}
			return Value < 0 ? "-" + Result : Result;
		}

		std::string RewardText(const FReward& Reward)
		{
			std::string Text;
			auto Part = [&Text](const char* Label, int Amount)
			{
				if (Amount != 0)
				{
					Text += (Text.empty() ? "" : ", ") + std::string(Label) + " " + Num(Amount);
				}
			};
			Part("Gold", Reward.Gold);
			Part("Bytes", Reward.Bytes);
			Part("Angel hearts", Reward.Hearts);
			Part("Diamonds", Reward.Diamonds);
			Part("Regular tickets", Reward.RegularTickets);
			Part("Special tickets", Reward.SpecialTickets);
			Part("Arcade tokens", Reward.ArcadeTokens);
			Part("Gang coins", Reward.GangCoins);
			Part("Equipment", Reward.Equipment);
			Part("Element cores", Reward.ElementCores);
			Part("Event tokens", Reward.EventTokens);
			Part("Ark medals", Reward.ArkMedals);
			Part("Charge", Reward.Charge);
			if (Reward.Item >= 0 && Reward.Item < ItemCount && Reward.ItemAmount > 0)
			{
				Text += (Text.empty() ? "" : ", ") + std::string(Inventory::ItemName(static_cast<EItem>(Reward.Item))) +
					(Reward.ItemAmount > 1 ? " x" + std::to_string(Reward.ItemAmount) : "");
			}
			Part("Season XP", Reward.SeasonXp);
			Part("Commander XP", Reward.CommanderXp);
			return Text.empty() ? "nothing" : Text;
		}

		std::string ApplyReward(FProfile& Profile, const FReward& Reward)
		{
			Profile.Add(ECurrency::Gold, Reward.Gold);
			Profile.Add(ECurrency::Bytes, Reward.Bytes);
			Profile.Add(ECurrency::AngelHearts, Reward.Hearts);
			Profile.Add(ECurrency::Diamonds, Reward.Diamonds);
			Profile.Add(ECurrency::RegularTicket, Reward.RegularTickets);
			Profile.Add(ECurrency::SpecialTicket, Reward.SpecialTickets);
			Profile.Add(ECurrency::ArcadeToken, Reward.ArcadeTokens);
			Profile.Add(ECurrency::GangCoin, Reward.GangCoins);
			for (int Index = 0; Index < Reward.Equipment; ++Index)
			{
				Profile.AddRandomEquipment();
			}
			Profile.Add(ECurrency::ElementCore, Reward.ElementCores);
			Profile.Add(ECurrency::EventToken, Reward.EventTokens);
			Profile.Add(ECurrency::ArkMedal, Reward.ArkMedals);
			Profile.Add(ECurrency::Charge, Reward.Charge);
			if (Reward.Item >= 0 && Reward.Item < ItemCount && Reward.ItemAmount > 0)
			{
				Inventory::Add(Profile, static_cast<EItem>(Reward.Item), Reward.ItemAmount);
			}
			Profile.Meta.SeasonXp += Reward.SeasonXp;
			Profile.Meta.CommanderXp += Reward.CommanderXp;
			return RewardText(Reward);
		}

		// ================================================================== commander
		int XpPerLevel()
		{
			return 250; // PLACEHOLDER curve
		}

		int CommanderLevel(const FProfile& Profile)
		{
			return std::min(60, 1 + Profile.Meta.CommanderXp / XpPerLevel());
		}

		void AddCommanderXp(FProfile& Profile, int Amount)
		{
			Profile.Meta.CommanderXp += std::max(0, Amount);
		}

		// ===================================================================== quests
		// Owner request 2026-10-06: a quest system with chapter (story) quests - one Regular ticket for every chapter, ten for
		// Chapter 1 - an extra quest every 2-3 levels worth about 20-50 Diamonds or Gold / Bytes / Angel hearts, and daily and
		// weekly quests. Titles and amounts are PLACEHOLDER (balanced to BIBCampaign.h StageReward).
		namespace
		{
			struct FQuestDef
			{
				const char* Title;
				int Target;
				EQuestEvent Event;
				FReward Reward;
			};

			// Season XP (owner request 2026-10-06): the pass is paced by the daily and weekly quests only - every daily quest is worth
			// 240 XP a day and every weekly quest 700 a week, so a player who finishes all of them reaches tier 50 (5,000 XP) on
			// day 15 of the month-long season (15 x 240 + 2 x 700 = 5,000). Stages, the Ark, story quests and milestones give none.
			constexpr int ItemId(EItem Item) { return static_cast<int>(Item); }

			// Rebalanced 2026-10-07 to classic gacha income (owner request: "more classic gacha standards like NIKKE or ZZZ"):
			// the dailies pay 80 Diamonds a day, the weeklies 200 Diamonds, a Regular and a Special ticket a week, plus boosts,
			// Charge and Element cores (Tests/gacha_tests.cpp checks the monthly pull budget). PLACEHOLDER amounts.
			const std::vector<FQuestDef>& DailyDefs()
			{
				static const std::vector<FQuestDef> Defs = {
					{"Claim the login bonus", 1, EQuestEvent::LoginBonus, {.Gold = 300, .Diamonds = 10, .SeasonXp = 25, .CommanderXp = 20}},
					{"Clear 3 campaign levels", 3, EQuestEvent::StageClear, {.Gold = 800, .Bytes = 250, .Diamonds = 10, .SeasonXp = 45, .CommanderXp = 40}},
					{"Summon once", 1, EQuestEvent::Summon, {.Bytes = 300, .Diamonds = 10, .SeasonXp = 30, .CommanderXp = 30}},
					{"Level up operators 3 times", 3, EQuestEvent::LevelUp, {.Gold = 500, .Diamonds = 10, .SeasonXp = 30, .CommanderXp = 30}},
					{"Change equipment", 1, EQuestEvent::EquipChange, {.Bytes = 200, .SeasonXp = 25, .CommanderXp = 25, .ElementCores = 3}},
					{"Play the arcade 3 times", 3, EQuestEvent::ArcadePlay, {.ArcadeTokens = 2, .SeasonXp = 25, .CommanderXp = 25}},
					{"Collect from the Outer World", 1, EQuestEvent::WorldCollect, {.Hearts = 2, .SeasonXp = 25, .CommanderXp = 25, .Item = ItemId(EItem::GoldBoost3h), .ItemAmount = 1}},
					{"Complete 5 other daily quests", 5, EQuestEvent::DailyDone, {.Diamonds = 40, .SeasonXp = 35, .CommanderXp = 40, .Charge = 30}},
				};
				return Defs;
			}

			const std::vector<FQuestDef>& WeeklyDefs()
			{
				static const std::vector<FQuestDef> Defs = {
					{"Clear 15 campaign levels", 15, EQuestEvent::StageClear, {.Gold = 4000, .Bytes = 1250, .SeasonXp = 120, .CommanderXp = 150, .ElementCores = 10}},
					{"Summon 10 times", 10, EQuestEvent::Summon, {.RegularTickets = 1, .SeasonXp = 90, .CommanderXp = 100}},
					{"Level up operators 20 times", 20, EQuestEvent::LevelUp, {.Gold = 2000, .Hearts = 10, .SeasonXp = 90, .CommanderXp = 100}},
					{"Buy 3 things in the shops", 3, EQuestEvent::ShopBuy, {.Gold = 2000, .SeasonXp = 70, .CommanderXp = 80, .Item = ItemId(EItem::GoldBoost6h), .ItemAmount = 1}},
					{"Fight 5 times in the Ark (tower or arena)", 5, EQuestEvent::ArkPlay, {.Diamonds = 50, .SeasonXp = 90, .CommanderXp = 100, .ArkMedals = 100}},
					{"Collect from the Outer World 5 times", 5, EQuestEvent::WorldCollect, {.Bytes = 1500, .Hearts = 5, .SeasonXp = 70, .CommanderXp = 80, .Item = ItemId(EItem::BytesBoost12h), .ItemAmount = 1}},
					{"Complete 25 daily quests", 25, EQuestEvent::DailyDone, {.Diamonds = 150, .SpecialTickets = 1, .SeasonXp = 170, .CommanderXp = 200, .Charge = 60}},
				};
				return Defs;
			}

			enum class EMetric : int
			{
				OperatorsOwnedValue,
				HighestOperatorLevel,
				CommanderLevelValue,
				BuildingsBuilt,
				TowerFloors,
				SeasonTierValue
			};

			struct FMilestoneDef
			{
				const char* Title;
				int Target;
				EMetric Metric;
				FReward Reward;
			};

			// Campaign milestones moved to the story quests (2026-10-06); these track the rest of the game.
			const std::vector<FMilestoneDef>& MilestoneDefs()
			{
				static const std::vector<FMilestoneDef> Defs = {
					{"Own 5 operators", 5, EMetric::OperatorsOwnedValue, {.Diamonds = 50, .CommanderXp = 100}},
					{"Raise an operator to level 20", 20, EMetric::HighestOperatorLevel, {.Gold = 2000, .Hearts = 10, .CommanderXp = 100}},
					{"Reach Commander level 5", 5, EMetric::CommanderLevelValue, {.Diamonds = 30, .RegularTickets = 1}},
					{"Build 4 Outer World buildings", 4, EMetric::BuildingsBuilt, {.Gold = 3000, .Bytes = 1000, .CommanderXp = 100}},
					{"Reach Ark Tower floor 5 (either mode)", 5, EMetric::TowerFloors, {.Hearts = 15, .Diamonds = 30, .CommanderXp = 100}},
					{"Reach Season pass tier 10", 10, EMetric::SeasonTierValue, {.Diamonds = 50, .CommanderXp = 100, .Equipment = 1}},
				};
				return Defs;
			}

			int MetricValue(const FProfile& Profile, EMetric Metric)
			{
				switch (Metric)
				{
				case EMetric::OperatorsOwnedValue: return OperatorsOwned(Profile);
				case EMetric::HighestOperatorLevel:
				{
					int Best = 0;
					for (const FOwnedOperator& Owned : Profile.Operators)
					{
						Best = Owned.bOwned ? std::max(Best, Owned.Level) : Best;
					}
					return Best;
				}
				case EMetric::CommanderLevelValue: return CommanderLevel(Profile);
				case EMetric::BuildingsBuilt: return BuildingBuiltCount(Profile);
				case EMetric::TowerFloors: return std::max(Profile.Meta.TowerBest[0], Profile.Meta.TowerBest[1]);
				case EMetric::SeasonTierValue: return SeasonTier(Profile);
				}
				return 0;
			}

			FQuestInfo FromDef(const FQuestDef& Def, const FQuestProgress& Progress)
			{
				FQuestInfo Info;
				Info.Title = Def.Title;
				Info.Target = Def.Target;
				Info.Progress = std::min(Progress.Progress, Def.Target);
				Info.bClaimed = Progress.bClaimed;
				Info.bComplete = Info.Progress >= Def.Target;
				Info.Reward = Def.Reward;
				return Info;
			}

			std::vector<FQuestProgress>& ProgressList(FMeta& Meta, EQuestList List)
			{
				switch (List)
				{
				case EQuestList::Daily: return Meta.Daily;
				case EQuestList::Weekly: return Meta.Weekly;
				case EQuestList::Story: return Meta.Story;
				case EQuestList::Milestone:
				case EQuestList::Count: break;
				}
				return Meta.Milestones;
			}
		}

		// ------------------------------------------------------------------ story quests
		FStoryQuestDef StoryQuestDef(int Index)
		{
			FStoryQuestDef Def;
			if (Index < 0 || Index >= StoryQuestCount)
			{
				return Def;
			}
			Def.Chapter = Index / StoryQuestsPerChapter + 1;
			const int Kind = Index % StoryQuestsPerChapter;
			static const int Levels[StoryQuestsPerChapter] = {3, 6, CampaignLevels};   // a quest every 2-3 levels
			Def.Level = Levels[Kind];
			Def.Stage = CampaignStageId(Def.Chapter, Def.Level);
			const Campaign::FStageReward Stage = Campaign::CurveReward(Def.Stage);   // Chapter 11 quests pay like the others
			const std::string Where = "Chapter " + std::to_string(Def.Chapter);
			if (Kind == 0)
			{
				Def.Title = Where + ": clear level 3";
				Def.Reward.Diamonds = 20 + 5 * ((Def.Chapter - 1) / 2);   // 20 in Chapter 1 ... 50 in Chapter 13
				Def.Reward.CommanderXp = 50;
			}
			else if (Kind == 1)
			{
				Def.Title = Where + ": clear level 6";
				if (Def.Chapter % 2 == 1)
				{
					Def.Reward.Gold = 2 * Stage.Gold;     // two clears' worth of Gold and Bytes
					Def.Reward.Bytes = 2 * Stage.Bytes;
				}
				else
				{
					Def.Reward.Hearts = 3 * Stage.Hearts; // three clears' worth of Angel hearts, plus one of Gold
					Def.Reward.Gold = Stage.Gold;
				}
				Def.Reward.CommanderXp = 50;
			}
			else
			{
				Def.Title = "Complete " + Where + " (defeat the boss)";
				Def.Reward.RegularTickets = Def.Chapter == 1 ? 10 : 1;   // owner: 10 for Chapter 1, 1 for every other chapter
				Def.Reward.CommanderXp = 100;
			}
			return Def;
		}

		FQuestInfo StoryQuest(const FProfile& Profile, int Index)
		{
			FQuestInfo Info;
			if (Index < 0 || Index >= StoryQuestCount)
			{
				return Info;
			}
			const FStoryQuestDef Def = StoryQuestDef(Index);
			Info.Title = Def.Title;
			Info.Target = 1;
			Info.Progress = Profile.StagesCleared >= Def.Stage ? 1 : 0;
			Info.bComplete = Info.Progress >= 1;
			Info.bClaimed = static_cast<size_t>(Index) < Profile.Meta.Story.size() && Profile.Meta.Story[static_cast<size_t>(Index)].bClaimed;
			Info.Reward = Def.Reward;
			return Info;
		}

		// ------------------------------------------------------------------ all lists
		int QuestCount(EQuestList List)
		{
			switch (List)
			{
			case EQuestList::Daily: return DailyQuestCount;
			case EQuestList::Weekly: return WeeklyQuestCount;
			case EQuestList::Story: return StoryQuestCount;
			case EQuestList::Milestone: return MilestoneCount;
			case EQuestList::Count: break;
			}
			return 0;
		}

		const char* QuestListName(EQuestList List)
		{
			switch (List)
			{
			case EQuestList::Daily: return "Daily";
			case EQuestList::Weekly: return "Weekly";
			case EQuestList::Story: return "Story";
			case EQuestList::Milestone: return "Milestones";
			case EQuestList::Count: break;
			}
			return "?";
		}

		FQuestInfo Quest(const FProfile& Profile, EQuestList List, int Index)
		{
			switch (List)
			{
			case EQuestList::Daily:
			{
				const std::vector<FQuestDef>& Defs = DailyDefs();
				if (Index < 0 || Index >= static_cast<int>(Defs.size()) || static_cast<size_t>(Index) >= Profile.Meta.Daily.size())
				{
					return FQuestInfo();
				}
				return FromDef(Defs[static_cast<size_t>(Index)], Profile.Meta.Daily[static_cast<size_t>(Index)]);
			}
			case EQuestList::Weekly:
			{
				const std::vector<FQuestDef>& Defs = WeeklyDefs();
				if (Index < 0 || Index >= static_cast<int>(Defs.size()) || static_cast<size_t>(Index) >= Profile.Meta.Weekly.size())
				{
					return FQuestInfo();
				}
				return FromDef(Defs[static_cast<size_t>(Index)], Profile.Meta.Weekly[static_cast<size_t>(Index)]);
			}
			case EQuestList::Story: return StoryQuest(Profile, Index);
			case EQuestList::Milestone:
			{
				FQuestInfo Info;
				const std::vector<FMilestoneDef>& Defs = MilestoneDefs();
				if (Index < 0 || Index >= static_cast<int>(Defs.size()) || static_cast<size_t>(Index) >= Profile.Meta.Milestones.size())
				{
					return Info;
				}
				const FMilestoneDef& Def = Defs[static_cast<size_t>(Index)];
				Info.Title = Def.Title;
				Info.Target = Def.Target;
				Info.Progress = std::min(MetricValue(Profile, Def.Metric), Def.Target);
				Info.bClaimed = Profile.Meta.Milestones[static_cast<size_t>(Index)].bClaimed;
				Info.bComplete = Info.Progress >= Def.Target;
				Info.Reward = Def.Reward;
				return Info;
			}
			case EQuestList::Count: break;
			}
			return FQuestInfo();
		}

		FQuestInfo DailyQuest(const FProfile& Profile, int Index)
		{
			return Quest(Profile, EQuestList::Daily, Index);
		}

		FQuestInfo WeeklyQuest(const FProfile& Profile, int Index)
		{
			return Quest(Profile, EQuestList::Weekly, Index);
		}

		FQuestInfo MilestoneQuest(const FProfile& Profile, int Index)
		{
			return Quest(Profile, EQuestList::Milestone, Index);
		}

		void NotifyQuest(FProfile& Profile, EQuestEvent Event, int Amount)
		{
			// Returned quests (welcome-back event) and gang activity points count the same events.
			if (LiveOps::ReturnActive(Profile))
			{
				FReturnState& R = Profile.Meta.Return;
				switch (Event)
				{
				case EQuestEvent::StageClear: R.StageClears += Amount; break;
				case EQuestEvent::Summon: R.Summons += Amount; break;
				case EQuestEvent::LevelUp: R.LevelUps += Amount; break;
				case EQuestEvent::DailyDone: R.DailyQuests += Amount; break;
				case EQuestEvent::ArkPlay: R.ArkFights += Amount; break;
				default: break;
				}
			}
			// Weekly operation and Rookie week counters (BIBEvents.h).
			switch (Event)
			{
			case EQuestEvent::StageClear: Events::Notify(Profile, Events::EStat::StageClear, Amount); break;
			case EQuestEvent::Summon: Events::Notify(Profile, Events::EStat::Summon, Amount); break;
			case EQuestEvent::LevelUp: Events::Notify(Profile, Events::EStat::LevelUp, Amount); break;
			case EQuestEvent::ArcadePlay: Events::Notify(Profile, Events::EStat::ArcadePlay, Amount); break;
			case EQuestEvent::WorldCollect: Events::Notify(Profile, Events::EStat::WorldCollect, Amount); break;
			case EQuestEvent::ArkPlay: Events::Notify(Profile, Events::EStat::ArkPlay, Amount); break;
			case EQuestEvent::DailyDone: Events::Notify(Profile, Events::EStat::DailyDone, Amount); break;
			case EQuestEvent::LoginBonus: Events::Notify(Profile, Events::EStat::LoginBonus, Amount); break;
			default: break;
			}
			int& Pending = Profile.Meta.Gang.PendingActivity;
			if (Event == EQuestEvent::DailyDone)
			{
				Pending = std::min(200, Pending + 5 * Amount);
			}
			else if (Event == EQuestEvent::StageClear)
			{
				Pending = std::min(200, Pending + 2 * Amount);
			}
			const std::vector<FQuestDef>& Daily = DailyDefs();
			for (size_t Index = 0; Index < Daily.size() && Index < Profile.Meta.Daily.size(); ++Index)
			{
				if (Daily[Index].Event == Event)
				{
					Profile.Meta.Daily[Index].Progress += Amount;
				}
			}
			const std::vector<FQuestDef>& Weekly = WeeklyDefs();
			for (size_t Index = 0; Index < Weekly.size() && Index < Profile.Meta.Weekly.size(); ++Index)
			{
				if (Weekly[Index].Event == Event)
				{
					Profile.Meta.Weekly[Index].Progress += Amount;
				}
			}
		}

		FActionResult ClaimQuest(FProfile& Profile, EQuestList List, int Index)
		{
			const FQuestInfo Info = Quest(Profile, List, Index);
			if (Info.Title.empty())
			{
				return Fail("Unknown quest.");
			}
			if (Info.bClaimed)
			{
				return Fail("Already claimed.");
			}
			if (!Info.bComplete)
			{
				return Fail("Quest not complete yet.");
			}
			std::vector<FQuestProgress>& Progress = ProgressList(Profile.Meta, List);
			if (static_cast<size_t>(Index) >= Progress.size())
			{
				Progress.resize(static_cast<size_t>(QuestCount(List)));
			}
			Progress[static_cast<size_t>(Index)].bClaimed = true;
			const std::string Text = ApplyReward(Profile, Info.Reward);
			if (List == EQuestList::Daily)
			{
				NotifyQuest(Profile, EQuestEvent::DailyDone);   // feeds the daily bonus quest and the weekly "25 daily quests"
			}
			return Ok(Info.Title + " - reward: " + Text);
		}

		FActionResult ClaimQuest(FProfile& Profile, bool bMilestone, int Index)
		{
			return ClaimQuest(Profile, bMilestone ? EQuestList::Milestone : EQuestList::Daily, Index);
		}

		int ClaimableQuests(const FProfile& Profile, EQuestList List)
		{
			int Count = 0;
			for (int Index = 0; Index < QuestCount(List); ++Index)
			{
				const FQuestInfo Info = Quest(Profile, List, Index);
				Count += Info.bComplete && !Info.bClaimed ? 1 : 0;
			}
			return Count;
		}

		int ClaimableQuests(const FProfile& Profile)
		{
			return ClaimableQuests(Profile, EQuestList::Daily) + ClaimableQuests(Profile, EQuestList::Weekly)
				+ ClaimableQuests(Profile, EQuestList::Story) + ClaimableQuests(Profile, EQuestList::Milestone);
		}

		FActionResult ClaimAllQuests(FProfile& Profile)
		{
			int Count = 0;
			// Repeat: claiming daily quests can complete the daily bonus quest.
			for (int Pass = 0; Pass < 3; ++Pass)
			{
				for (EQuestList List : {EQuestList::Story, EQuestList::Daily, EQuestList::Weekly, EQuestList::Milestone})
				{
					for (int Index = 0; Index < QuestCount(List); ++Index)
					{
						const FQuestInfo Info = Quest(Profile, List, Index);
						if (Info.bComplete && !Info.bClaimed && ClaimQuest(Profile, List, Index).bOk)
						{
							++Count;
						}
					}
				}
			}
			return Count > 0 ? Ok("Claimed " + std::to_string(Count) + " quest" + (Count == 1 ? "" : "s") + ".") : Fail("Nothing to claim.");
		}

		int DaysUntilWeeklyReset(const FProfile& Profile)
		{
			return 7 - (std::max(1, Profile.Day) - 1) % 7;
		}

		// ======================================================================= mail
		int UnclaimedMail(const FProfile& Profile)
		{
			int Count = 0;
			for (const FMail& Mail : Profile.Meta.Mail)
			{
				Count += Mail.bClaimed ? 0 : 1;
			}
			return Count;
		}

		FActionResult ClaimMail(FProfile& Profile, int MailId)
		{
			for (FMail& Mail : Profile.Meta.Mail)
			{
				if (Mail.Id == MailId)
				{
					if (Mail.bClaimed)
					{
						return Fail("Already claimed.");
					}
					Mail.bClaimed = true;
					return Ok("Mail: " + Mail.Title + " - " + ApplyReward(Profile, Mail.Reward));
				}
			}
			return Fail("No such mail.");
		}

		FActionResult ClaimAllMail(FProfile& Profile)
		{
			int Count = 0;
			for (FMail& Mail : Profile.Meta.Mail)
			{
				if (!Mail.bClaimed)
				{
					Mail.bClaimed = true;
					ApplyReward(Profile, Mail.Reward);
					++Count;
				}
			}
			return Count > 0 ? Ok("Claimed " + std::to_string(Count) + " mail attachments.") : Fail("Nothing to claim.");
		}

		int DeleteClaimedMail(FProfile& Profile)
		{
			std::vector<FMail>& Mail = Profile.Meta.Mail;
			const size_t Before = Mail.size();
			Mail.erase(std::remove_if(Mail.begin(), Mail.end(), [](const FMail& Entry) { return Entry.bClaimed; }), Mail.end());
			return static_cast<int>(Before - Mail.size());
		}

		// ================================================================ events/login
		bool CanClaimLogin(const FProfile& Profile)
		{
			return Profile.Meta.LoginLastDay != Profile.Day;
		}

		FReward LoginReward(int StreakDay)
		{
			static const FReward Rewards[7] = {
				// Rescaled 2026-10-06 to the new stage economy (stage 1 pays 800 Gold). PLACEHOLDER.
				// Rebalanced 2026-10-07 (classic gacha 7-day login: small daily items, a ticket on day 6, the big pull on day 7).
				// Day 1 Charge 30 -> 120 (balancing pass 2026-10-09): a new account starts without Charge (owner rule: 300 Diamonds only)
				// and the Chapter 1 levels after the tutorial need about seven replays (70 Charge); at 30 plus 1 per 6 minutes a first
				// session stalled in the onboarding chapter.
				{.Gold = 600, .Charge = 120}, {.Bytes = 300, .Item = ItemId(EItem::GoldBoost3h), .ItemAmount = 1}, {.Hearts = 4, .ElementCores = 10},
				{.ArcadeTokens = 3, .Item = ItemId(EItem::ChargePack), .ItemAmount = 1}, {.Diamonds = 50, .Item = ItemId(EItem::GoldBoost6h), .ItemAmount = 1},
				{.RegularTickets = 1}, {.Diamonds = 50, .SpecialTickets = 1},
			};
			return Rewards[std::clamp(StreakDay, 1, 7) - 1];
		}

		FActionResult ClaimLogin(FProfile& Profile)
		{
			if (!CanClaimLogin(Profile))
			{
				return Fail("Today's login bonus is already claimed.");
			}
			FMeta& Meta = Profile.Meta;
			Meta.LoginStreak = (Meta.LoginLastDay == Profile.Day - 1 || Meta.LoginLastDay == 0) ? Meta.LoginStreak + 1 : 1;
			Meta.LoginLastDay = Profile.Day;
			const int StreakDay = (Meta.LoginStreak - 1) % 7 + 1;
			NotifyQuest(Profile, EQuestEvent::LoginBonus);
			return Ok("Login bonus day " + std::to_string(StreakDay) + ": " + ApplyReward(Profile, LoginReward(StreakDay)));
		}

		int NoticeCount()
		{
			return 4;
		}

		std::string NoticeTitle(int Index)
		{
			static const char* Titles[4] = {"Welcome to the demo", "Placeholder content", "Maintenance schedule (placeholder)", "Season 1 pass (placeholder)"};
			return Index >= 0 && Index < 4 ? Titles[Index] : "?";
		}

		std::string NoticeBody(int Index)
		{
			static const char* Bodies[4] = {
				"This is a gameplay and UI demo of Boldinbeauty. Art is placeholder.",
				"Names, prices, rewards and rates in the menus are stand-ins; the owner replaces them.",
				"No maintenance is scheduled. This notice shows where announcements will appear.",
				"The 50-tier season pass here is a template: a free track and a paid track.",
			};
			return Index >= 0 && Index < 4 ? Bodies[Index] : "";
		}

		int EventCount()
		{
			return 3;
		}

		std::string EventTitle(int Index)
		{
			static const char* Titles[3] = {"7-day login bonus", "Featured summons: Judge and Narrator", "Arcade opening week"};
			return Index >= 0 && Index < 3 ? Titles[Index] : "?";
		}

		std::string EventBody(int Index)
		{
			static const char* Bodies[3] = {
				"Log in each day for a reward; the 7th day is the biggest.",
				"Judge (5*) and Narrator (6*) have their own featured banners in the Summon tab.",
				"Arcade tokens are in the daily supply mail; the arcade is in the Lobby.",
			};
			return Index >= 0 && Index < 3 ? Bodies[Index] : "";
		}

		// ================================================================ season pass
		int SeasonXpPerTier()
		{
			return 100;
		}

		int SeasonTier(const FProfile& Profile)
		{
			return std::min(SeasonTiers, Profile.Meta.SeasonXp / SeasonXpPerTier());
		}

		FReward SeasonReward(int Tier, bool bPremiumTrack)
		{
			FReward Reward;
			if (Tier < 1 || Tier > SeasonTiers)
			{
				return Reward;
			}
			if (!bPremiumTrack)
			{
				// Rebalanced 2026-10-07 (classic free pass: about 8 pulls a month - 5 Regular and 2 Special tickets and 400
				// Diamonds - plus Element cores, boosts and Charge). PLACEHOLDER.
				Reward.Gold = 250 + Tier * 50;
				if (Tier % 5 == 0)
				{
					Reward.Hearts = 4;
					Reward.ElementCores = 10;
				}
				if (Tier % 10 == 0)
				{
					Reward.RegularTickets = 1;
					Reward.Diamonds = 50;
				}
				if (Tier == 15 || Tier == 35)
				{
					Reward.Item = ItemId(EItem::GoldBoost12h);
					Reward.ItemAmount = 1;
				}
				if (Tier == 45)
				{
					Reward.Item = ItemId(EItem::ChargePack);
					Reward.ItemAmount = 2;
				}
				if (Tier == 25)
				{
					Reward.SpecialTickets = 1;
				}
				if (Tier == SeasonTiers)
				{
					Reward.SpecialTickets = 1;
					Reward.Diamonds += 150;
				}
			}
			else
			{
				Reward.Gold = Tier * 25;
				Reward.Bytes = 125 + Tier * 40;
				if (Tier % 5 == 0)
				{
					Reward.SpecialTickets = 1;
				}
				if (Tier % 10 == 0)
				{
					Reward.Diamonds = 100;
					Reward.Equipment = 1;
				}
				if (Tier == 25)
				{
					Reward.Equipment = 2;
					Reward.ElementCores = 50;
				}
				if (Tier % 10 == 5 && Tier != 25)
				{
					Reward.Item = ItemId(Tier < 25 ? EItem::BytesBoost12h : EItem::HeartsBoost12h);
					Reward.ItemAmount = 1;
				}
				if (Tier == SeasonTiers)
				{
					Reward.Diamonds = 300;
					Reward.Equipment = 3;
					Reward.Item = ItemId(EItem::GoldBoost24h);
					Reward.ItemAmount = 2;
				}
			}
			return Reward;
		}

		FActionResult ClaimSeasonTier(FProfile& Profile, int Tier)
		{
			FMeta& Meta = Profile.Meta;
			if (Tier < 1 || Tier > SeasonTiers)
			{
				return Fail("No such tier.");
			}
			if (Tier > SeasonTier(Profile))
			{
				return Fail("Tier " + std::to_string(Tier) + " is not reached yet.");
			}
			std::string Text;
			if (!Meta.FreeClaimed[Tier])
			{
				Meta.FreeClaimed[Tier] = true;
				Text += "Free: " + ApplyReward(Profile, SeasonReward(Tier, false));
			}
			if (Meta.bPremiumPass && !Meta.PremiumClaimed[Tier])
			{
				Meta.PremiumClaimed[Tier] = true;
				Text += std::string(Text.empty() ? "" : " | ") + "Paid: " + ApplyReward(Profile, SeasonReward(Tier, true));
			}
			if (Text.empty())
			{
				return Fail(Meta.bPremiumPass ? "Tier already claimed." : "Free reward already claimed (the paid track needs the Premium pass).");
			}
			return Ok("Tier " + std::to_string(Tier) + " - " + Text);
		}

		int ClaimableSeasonTiers(const FProfile& Profile)
		{
			int Count = 0;
			for (int Tier = 1; Tier <= SeasonTier(Profile); ++Tier)
			{
				Count += (!Profile.Meta.FreeClaimed[Tier] || (Profile.Meta.bPremiumPass && !Profile.Meta.PremiumClaimed[Tier])) ? 1 : 0;
			}
			return Count;
		}

		FActionResult ClaimAllSeason(FProfile& Profile)
		{
			int Count = 0;
			for (int Tier = 1; Tier <= SeasonTier(Profile); ++Tier)
			{
				if (ClaimSeasonTier(Profile, Tier).bOk)
				{
					++Count;
				}
			}
			return Count > 0 ? Ok("Claimed " + std::to_string(Count) + " tiers.") : Fail("Nothing to claim.");
		}

		// ======================================================================= shops
		const std::vector<FShopGear>& ArmouryStock()
		{
			static const std::vector<FShopGear> Stock = {
				// Prices halved 2026-10-06 for the new stage economy (PLACEHOLDER).
				{"Scrap Helmet", ESlot::Helmet, EBonus::Defence, 1.5, 1000, 250},
				{"Scrap Chestplate", ESlot::Chest, EBonus::Defence, 2.0, 1500, 400},
				{"Scrap Leggings", ESlot::Leggings, EBonus::Defence, 1.5, 1250, 300},
				{"Scrap Blade", ESlot::Weapon, EBonus::Damage, 2.0, 1750, 500},
				{"Plated Helmet", ESlot::Helmet, EBonus::Damage, 3.5, 4000, 1250},
				{"Plated Chestplate", ESlot::Chest, EBonus::Defence, 4.0, 5000, 1500},
				{"Plated Leggings", ESlot::Leggings, EBonus::Defence, 3.5, 4500, 1400},
				{"Plated Blade", ESlot::Weapon, EBonus::Damage, 4.5, 6000, 2000},
			};
			return Stock;
		}

		FActionResult BuyArmoury(FProfile& Profile, int Index)
		{
			const std::vector<FShopGear>& Stock = ArmouryStock();
			if (Index < 0 || Index >= static_cast<int>(Stock.size()))
			{
				return Fail("No such item.");
			}
			const FShopGear& Gear = Stock[static_cast<size_t>(Index)];
			if (Profile.Get(ECurrency::Gold) < Gear.Gold || Profile.Get(ECurrency::Bytes) < Gear.Bytes)
			{
				return Fail("Not enough Gold or Bytes for " + Gear.Name + ".");
			}
			Profile.Spend(ECurrency::Gold, Gear.Gold);
			Profile.Spend(ECurrency::Bytes, Gear.Bytes);
			const FEquipment& Item = Profile.AddEquipment(Gear.Slot, Gear.Bonus, Gear.Percent, Gear.Name);
			NotifyQuest(Profile, EQuestEvent::ShopBuy);
			return Ok("Bought " + Item.Name + " (+" + Fixed(Gear.Percent, 1) + "% " + (Gear.Bonus == EBonus::Damage ? "damage" : "defence") + ").");
		}

		const std::vector<FAlleyOffer>& AlleyStock()
		{
			static const std::vector<FAlleyOffer> Stock = {
				// Amounts cut 2026-10-06: Diamonds are scarce now (300 to start, 50+ per first clear), and the old hoards were worth
				// a hundred stage clears. Near the MAIN supplement rates (35 Gold / 60 Bytes per Diamond) with a small bonus. PLACEHOLDER.
				{"Angel-heart cache (40)", 150, 5, {.Hearts = 40}},
				{"Byte vault (6,000)", 100, 8, {.Bytes = 6000}},
				{"Gold hoard (5,000)", 80, 10, {.Gold = 5000}},
				{"Special tickets x3", 450, 12, {.SpecialTickets = 3}},
				{"Prime Helmet", 400, 15, {}, true, ESlot::Helmet, EBonus::Damage, 8.0},
				{"Prime Chestplate", 400, 20, {}, true, ESlot::Chest, EBonus::Defence, 8.0},
				{"Prime Leggings", 400, 25, {}, true, ESlot::Leggings, EBonus::Defence, 8.0},
				{"Prime Blade", 600, 30, {}, true, ESlot::Weapon, EBonus::Damage, 10.0},
			};
			return Stock;
		}

		FActionResult BuyAlley(FProfile& Profile, int Index)
		{
			const std::vector<FAlleyOffer>& Stock = AlleyStock();
			if (Index < 0 || Index >= static_cast<int>(Stock.size()))
			{
				return Fail("No such item.");
			}
			const FAlleyOffer& Offer = Stock[static_cast<size_t>(Index)];
			if (CommanderLevel(Profile) < Offer.LevelLock)
			{
				return Fail(Offer.Name + " unlocks at Commander level " + std::to_string(Offer.LevelLock) + ".");
			}
			if (!Profile.Spend(ECurrency::Diamonds, Offer.Diamonds))
			{
				return Fail("Not enough Diamonds (" + std::to_string(Offer.Diamonds) + ").");
			}
			std::string Text = Offer.Name;
			if (Offer.bGear)
			{
				const FEquipment& Item = Profile.AddEquipment(Offer.Slot, Offer.Bonus, Offer.Percent, Offer.Name);
				Text = Item.Name + " (+" + Fixed(Offer.Percent, 1) + "% " + (Offer.Bonus == EBonus::Damage ? "damage" : "defence") + ")";
			}
			else
			{
				ApplyReward(Profile, Offer.Reward);
			}
			NotifyQuest(Profile, EQuestEvent::ShopBuy);
			return Ok("The Back Alley dealer hands over: " + Text + ".");
		}

		// ---------------------------------------------------------------- Trade Centre
		namespace
		{
			// Value of one unit, in tenths of a Gold. Derived from the existing exchange rates (1 Diamond = 35 Gold,
			// 30 Angel hearts = 3,000 Gold, a ticket = 300 Diamonds) so no loop makes a profit. PLACEHOLDER.
			long long UnitValue(ECurrency Currency)
			{
				switch (Currency)
				{
				case ECurrency::Gold: return 10;
				case ECurrency::Bytes: return 6;
				case ECurrency::AngelHearts: return 1000;
				case ECurrency::Diamonds: return 350;
				case ECurrency::RegularTicket: return 105000;
				case ECurrency::SpecialTicket: return 105000;
				case ECurrency::ArcadeToken: return 5000;
				default: return 0;
				}
			}
		}

		const std::vector<ECurrency>& TradeCurrencies()
		{
			static const std::vector<ECurrency> List = {
				ECurrency::Gold, ECurrency::Bytes, ECurrency::AngelHearts, ECurrency::RegularTicket,
				ECurrency::SpecialTicket, ECurrency::ArcadeToken, ECurrency::Diamonds,
			};
			return List;
		}

		bool CanReceiveInTrade(ECurrency Currency)
		{
			return Currency != ECurrency::Diamonds && UnitValue(Currency) > 0;
		}

		int TradeFeePercent()
		{
			return 15; // PLACEHOLDER
		}

		int TradePremiumMarkup(ECurrency To)
		{
			// Balancing pass 2026-10-09 (PLACEHOLDER): summon tickets are Diamonds in another form (a ticket = one 300-Diamond roll), and
			// MAIN records the 1 Diamond = 35 Gold supplement rate as one-way ("reversible exchange is not established"); Diamonds were
			// already not receivable. At the plain rate a finished campaign's daily sweeps (240 Charge, 12 clears of 13-8) bought about
			// 27 pulls a day - some 800 a month against the ~55 free ones. With the markup a day of that farming buys about one pull.
			// Arcade tokens likewise: a hard Night Shift win pays about 15,000 Gold's worth (Gold, Bytes, hearts, 50 Diamonds) and is
			// repeatable, so a 588-Gold token made the arcade a Charge-free farm; at x30 (about 17,600 Gold) buying one never pays.
			switch (To)
			{
			case ECurrency::RegularTicket:
			case ECurrency::SpecialTicket: return 25;
			case ECurrency::ArcadeToken: return 30;
			default: return 1;
			}
		}

		long long TradeCost(ECurrency From, ECurrency To, long long AmountTo)
		{
			if (From == To || AmountTo <= 0 || UnitValue(From) == 0 || !CanReceiveInTrade(To))
			{
				return 0;
			}
			const long long Numerator = AmountTo * UnitValue(To) * TradePremiumMarkup(To) * 100;
			const long long Denominator = UnitValue(From) * (100 - TradeFeePercent());
			return (Numerator + Denominator - 1) / Denominator;
		}

		FActionResult Trade(FProfile& Profile, ECurrency From, ECurrency To, long long AmountTo)
		{
			const long long Cost = TradeCost(From, To, AmountTo);
			if (Cost <= 0)
			{
				return Fail(From == To ? "Pick two different things to trade." : "The Trade Centre will not give out that item.");
			}
			if (Profile.Get(From) < Cost)
			{
				return Fail("Not enough " + std::string(CurrencyName(From)) + " (need " + Num(Cost) + ").");
			}
			Profile.Spend(From, static_cast<int>(Cost));
			Profile.Add(To, static_cast<int>(AmountTo));
			NotifyQuest(Profile, EQuestEvent::ShopBuy);
			return Ok("Traded " + Num(Cost) + " " + CurrencyName(From) + " for " + Num(AmountTo) + " " + CurrencyName(To) + ".");
		}

		// ---------------------------------------------------------------- Premium (simulated)
		const std::vector<FIapOffer>& IapOffers()
		{
			static const std::vector<FIapOffer> Offers = {
				// SIMULATED. Rebalanced 2026-10-07 to classic gacha value: the biggest pack is about $2.80 a pull (300 Diamonds),
				// the Monthly supply (3,300 Diamonds over 30 days for $4.99) is the best value, as in NIKKE and ZZZ. PLACEHOLDER.
				{"90 Diamonds", 99, {.Diamonds = 90}},
				{"480 Diamonds", 499, {.Diamonds = 480}},
				{"980 Diamonds", 999, {.Diamonds = 980}},
				{"2,000 Diamonds", 1999, {.Diamonds = 2000}},
				{"5,200 Diamonds", 4999, {.Diamonds = 5200}},
				{"10,800 Diamonds", 9999, {.Diamonds = 10800}},
				{"Starter bundle (once)", 499, {.Gold = 5000, .Diamonds = 300, .SpecialTickets = 5, .Equipment = 2}, false, true},
				{"Season pass: Premium track", 999, {}, true, false},
				{"Monthly supply (30 days)", 499, {}, false, false, true},
			};
			return Offers;
		}

		FActionResult SimulatePurchase(FProfile& Profile, int Index)
		{
			const std::vector<FIapOffer>& Offers = IapOffers();
			if (Index < 0 || Index >= static_cast<int>(Offers.size()))
			{
				return Fail("No such offer.");
			}
			const FIapOffer& Offer = Offers[static_cast<size_t>(Index)];
			if (Offer.bOncePerProfile && Profile.Meta.bStarterBundleBought)
			{
				return Fail("Already bought.");
			}
			if (Offer.bGivesPass && Profile.Meta.bPremiumPass)
			{
				return Fail("Premium pass already active.");
			}
			if (Offer.bMonthly)
			{
				const FActionResult Monthly = Inventory::BuyMonthly(Profile);
				if (Monthly.bOk)
				{
					Profile.Meta.IapSpentCents += Offer.PriceCents;
					return Ok("DEMO purchase (no real payment): " + Monthly.Message);
				}
				return Monthly;
			}
			if (Offer.bOncePerProfile)
			{
				Profile.Meta.bStarterBundleBought = true;
			}
			if (Offer.bGivesPass)
			{
				Profile.Meta.bPremiumPass = true;
			}
			Profile.Meta.IapSpentCents += Offer.PriceCents;
			const std::string Granted = Offer.bGivesPass ? "Premium season track unlocked" : ApplyReward(Profile, Offer.Reward);
			return Ok("DEMO purchase (no real payment): " + Offer.Name + " - " + Granted + ".");
		}

		// ================================================================ Outer World
		const FBuildingDef& BuildingDef(int Index)
		{
			static const FBuildingDef Defs[BuildingCount] = {
				// Rescaled 2026-10-06 for the new stage economy: half the build cost, 40% of the output (PLACEHOLDER).
				{"School", "b_school", 2500, 500, 0.0, 16.0, 0.0},
				{"Police Station", "b_police", 4000, 1000, 48.0, 0.0, 0.0},
				{"Fire Station", "b_fire", 3000, 750, 36.0, 4.0, 0.0},
				{"Hospital", "b_hospital", 5000, 1500, 0.0, 0.0, 0.5},
				{"Gym", "b_gym", 3500, 1000, 0.0, 24.0, 0.0},
				{"Market", "b_market", 3000, 500, 60.0, 0.0, 0.0},
				{"Workshop", "b_workshop", 4500, 1250, 0.0, 32.0, 0.0},
				{"Shelter", "b_shelter", 4000, 1000, 20.0, 0.0, 0.25},
			};
			return Defs[std::clamp(Index, 0, BuildingCount - 1)];
		}

		int BuildingBuiltCount(const FProfile& Profile)
		{
			int Count = 0;
			for (const FBuildingState& State : Profile.Meta.Buildings)
			{
				Count += State.bBuilt ? 1 : 0;
			}
			return Count;
		}

		FCost BuildingNextCost(const FProfile& Profile, int Index)
		{
			const FBuildingDef& Def = BuildingDef(Index);
			const FBuildingState& State = Profile.Meta.Buildings[std::clamp(Index, 0, BuildingCount - 1)];
			const int Multiplier = State.bBuilt ? State.Level : 1;
			FCost Cost;
			Cost.Gold = Def.CostGold * Multiplier;
			Cost.Bytes = Def.CostBytes * Multiplier;
			return Cost;
		}

		double WorldCampaignMultiplier(const FProfile& Profile)
		{
			// Owner request 2026-10-07 (classic AFK rewards, NIKKE outpost-style): production grows with the campaign, +1% per
			// level cleared (+104% after the last chapter). PLACEHOLDER.
			return 1.0 + WorldPercentPerStage / 100.0 * std::clamp(Profile.StagesCleared, 0, TotalCampaignLevels);
		}

		double BuildingRate(const FProfile& Profile, int Index, int Resource)
		{
			const FBuildingDef& Def = BuildingDef(Index);
			const FBuildingState& State = Profile.Meta.Buildings[std::clamp(Index, 0, BuildingCount - 1)];
			if (!State.bBuilt)
			{
				return 0.0;
			}
			const double Base = Resource == 0 ? Def.GoldPerHour : (Resource == 1 ? Def.BytesPerHour : Def.HeartsPerHour);
			return Base * State.Level * WorldCampaignMultiplier(Profile);
		}

		FActionResult BuildOrUpgrade(FProfile& Profile, int Index)
		{
			if (Index < 0 || Index >= BuildingCount)
			{
				return Fail("No such building.");
			}
			FBuildingState& State = Profile.Meta.Buildings[Index];
			if (State.bBuilt && State.Level >= MaxBuildingLevel)
			{
				return Fail(std::string(BuildingDef(Index).Name) + " is at the maximum level.");
			}
			const FCost Cost = BuildingNextCost(Profile, Index);
			if (Profile.Get(ECurrency::Gold) < Cost.Gold || Profile.Get(ECurrency::Bytes) < Cost.Bytes)
			{
				return Fail("Need " + Num(Cost.Gold) + " Gold and " + Num(Cost.Bytes) + " Bytes.");
			}
			Profile.Spend(ECurrency::Gold, Cost.Gold);
			Profile.Spend(ECurrency::Bytes, Cost.Bytes);
			const bool bNew = !State.bBuilt;
			State.bBuilt = true;
			++State.Level;
			return Ok(std::string(BuildingDef(Index).Name) + (bNew ? " built." : " upgraded to level " + std::to_string(State.Level) + "."));
		}

		void AdvanceWorld(FProfile& Profile, double Hours)
		{
			if (Hours <= 0.0)
			{
				return;
			}
			Profile.Meta.WorldHours += Hours;
			// Overtime weekend (BIBEvents.h) doubles the production; the storage limit stays 12 hours of the normal rate.
			const double Speed = Events::WorldMultiplier(Profile);
			for (int Index = 0; Index < BuildingCount; ++Index)
			{
				FBuildingState& State = Profile.Meta.Buildings[Index];
				if (!State.bBuilt)
				{
					continue;
				}
				const double GoldRate = BuildingRate(Profile, Index, 0);
				const double BytesRate = BuildingRate(Profile, Index, 1);
				const double HeartsRate = BuildingRate(Profile, Index, 2);
				State.StoredGold = std::min(GoldRate * BuildingStorageHours, State.StoredGold + GoldRate * Hours * Speed);
				State.StoredBytes = std::min(BytesRate * BuildingStorageHours, State.StoredBytes + BytesRate * Hours * Speed);
				State.StoredHearts = std::min(HeartsRate * BuildingStorageHours, State.StoredHearts + HeartsRate * Hours * Speed);
			}
		}

		FActionResult CollectWorld(FProfile& Profile)
		{
			long long Gold = 0;
			long long Bytes = 0;
			long long Hearts = 0;
			for (FBuildingState& State : Profile.Meta.Buildings)
			{
				const double WholeGold = std::floor(State.StoredGold);
				const double WholeBytes = std::floor(State.StoredBytes);
				const double WholeHearts = std::floor(State.StoredHearts);
				Gold += static_cast<long long>(WholeGold);
				Bytes += static_cast<long long>(WholeBytes);
				Hearts += static_cast<long long>(WholeHearts);
				State.StoredGold -= WholeGold;
				State.StoredBytes -= WholeBytes;
				State.StoredHearts -= WholeHearts;
			}
			if (Gold + Bytes + Hearts == 0)
			{
				return Fail("Nothing to collect yet - buildings produce over time.");
			}
			Profile.Add(ECurrency::Gold, static_cast<int>(Gold));
			Profile.Add(ECurrency::Bytes, static_cast<int>(Bytes));
			Profile.Add(ECurrency::AngelHearts, static_cast<int>(Hearts));
			NotifyQuest(Profile, EQuestEvent::WorldCollect);
			return Ok("Collected " + Num(Gold) + " Gold, " + Num(Bytes) + " Bytes, " + Num(Hearts) + " Angel hearts.");
		}

		// ============================================================ tower / stories
		namespace
		{
			bool InStarRange(int Stars, int Mode)
			{
				return Mode == 0 ? (Stars == 3 || Stars == 4) : (Stars == 5 || Stars == 6);
			}
		}

		std::vector<int> TowerTeam(const FProfile& Profile, int Mode)
		{
			// The squad's eligible members first, then the strongest other eligible operators the player owns (up to three), so
			// each mode always fields a full team when the collection allows it.
			std::vector<int> Members;
			auto Eligible = [&Profile, Mode](int Index)
			{
				return Index >= 0 && Profile.Owns(Index) && Roster()[static_cast<size_t>(Index)].Kit != EKit::None
					&& InStarRange(Roster()[static_cast<size_t>(Index)].Stars, Mode);
			};
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				const int Index = Profile.Team[Slot];
				if (Eligible(Index) && std::find(Members.begin(), Members.end(), Index) == Members.end())
				{
					Members.push_back(Index);
				}
			}
			std::vector<int> Others;
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				if (Eligible(Index) && std::find(Members.begin(), Members.end(), Index) == Members.end())
				{
					Others.push_back(Index);
				}
			}
			std::stable_sort(Others.begin(), Others.end(), [&Profile](int A, int B) { return OperatorPower(Profile, A) > OperatorPower(Profile, B); });
			for (int Index : Others)
			{
				if (Members.size() >= 3)
				{
					break;
				}
				Members.push_back(Index);
			}
			return Members;
		}

		int TowerNextFloor(const FProfile& Profile, int Mode)
		{
			return std::min(MaxTowerFloor, Profile.Meta.TowerBest[Mode == 0 ? 0 : 1] + 1);
		}

		static std::string TeamNeedText(const FProfile& Profile)
		{
			const int Need = Profile.RequiredTeamSize();
			return "Your squad needs " + std::to_string(Need) + " different owned operator" + (Need == 1 ? "" : "s") + " with written kits (Team tab).";
		}

		FActionResult CanStartStage(const FProfile& Profile, int StageId)
		{
			if (!IsValidStage(StageId))
			{
				return Fail("That stage does not exist.");
			}
			const FStageRef Ref = DecodeStage(StageId);
			const bool bAdmin = Profile.Account.bAdmin && Profile.Account.bSignedIn; // the owner/admin account may enter any level
			switch (Ref.Kind)
			{
			case EStageKind::Campaign:
				if (!Profile.TeamIsPlayable())
				{
					return Fail(TeamNeedText(Profile));
				}
				if (Campaign::IsWalkStage(StageId) && StageId != Campaign::WalkEntryStage())
				{
					return Fail("Return to Nowhere is a single walk - start it from its first stage.");
				}
				if (!bAdmin && StageId > Profile.StagesCleared + 1)
				{
					const int Before = StageId - 1;
					return Fail("Clear " + StageName(Before) + " first - levels unlock in order.");
				}
				if (Config::UseCharge)
				{
					const int Cost = Inventory::StageChargeCost(Profile, StageId);
					if (Cost > 0 && Profile.Get(ECurrency::Charge) < Cost)
					{
						return Fail("Replaying " + StageName(StageId) + " costs " + std::to_string(Cost) + " Charge (you have " +
							std::to_string(Profile.Get(ECurrency::Charge)) + "). First clears are free; Charge refills over time, from Charge packs or for Diamonds (Lobby > Inventory).");
					}
				}
				break;
			case EStageKind::Tower:
				if (TowerTeam(Profile, Ref.A).empty())
				{
					return Fail(Ref.A == 0 ? "Operator mode needs a 3-4 star operator (you own none yet)."
											: "Secrets mode needs a 5-6 star operator (you own none yet).");
				}
				if (!bAdmin && Ref.B != TowerNextFloor(Profile, Ref.A))
				{
					return Fail("Climb floor by floor.");
				}
				break;
			case EStageKind::SubStory:
				if (!Profile.TeamIsPlayable())
				{
					return Fail(TeamNeedText(Profile));
				}
				if (!bAdmin && Ref.B > Profile.Meta.SubStoryCleared[Ref.A] + 1)
				{
					return Fail("Clear the earlier level first.");
				}
				break;
			case EStageKind::BossRush:
				if (!Profile.TeamIsPlayable())
				{
					return Fail(TeamNeedText(Profile));
				}
				break;
			}
			return Ok("");
		}

		void EffectiveTeam(const FProfile& Profile, int StageId, int OutTeam[3])
		{
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				OutTeam[Slot] = Profile.Team[Slot];
			}
			const FStageRef Ref = DecodeStage(StageId);
			if (Ref.Kind == EStageKind::Tower)
			{
				const std::vector<int> Eligible = TowerTeam(Profile, Ref.A);
				for (int Slot = 0; Slot < 3; ++Slot)
				{
					OutTeam[Slot] = Slot < static_cast<int>(Eligible.size()) ? Eligible[static_cast<size_t>(Slot)] : -1;
				}
			}
		}

		std::vector<std::string> ClearStage(FProfile& Profile, int StageId)
		{
			std::vector<std::string> Lines;
			if (!IsValidStage(StageId))
			{
				return Lines;
			}
			const FStageRef Ref = DecodeStage(StageId);
			FReward Extra;
			switch (Ref.Kind)
			{
			case EStageKind::Campaign:
			{
				if (Campaign::IsWalkStage(StageId))
				{
					// Return to Nowhere (owner brief): you walk out and nothing happens. The walk clears the whole chapter.
					const bool bFirst = Profile.StagesCleared < Campaign::WalkLastStage();
					Profile.StagesCleared = std::max(Profile.StagesCleared, Campaign::WalkLastStage());
					if (bFirst)
					{
						NotifyQuest(Profile, EQuestEvent::StageClear);
						Lines.push_back("Chapter " + std::to_string(Campaign::WalkChapter + 1) + " unlocked.");
					}
					break;
				}
				const bool bFirstClear = StageId > Profile.StagesCleared;
				const int Charge = Config::UseCharge ? Inventory::StageChargeCost(Profile, StageId) : 0;
				if (Charge > 0)
				{
					Profile.Spend(ECurrency::Charge, std::min(Charge, Profile.Get(ECurrency::Charge)));
				}
				const int SurgeCores = Events::SurgeCoresFor(Profile, StageId, bFirstClear);
				Lines = Profile.ApplyStageRewards(StageId, IsChapterFinal(StageId));
				if (Charge > 0)
				{
					Lines.push_back("-" + std::to_string(Charge) + " Charge (replay)");
				}
				if (SurgeCores > 0)
				{
					Profile.Add(ECurrency::ElementCore, SurgeCores);
					Lines.push_back("+" + std::to_string(SurgeCores) + " Element cores (" + Element::Name(Events::SurgeElement(Profile)) + " Surge)");
				}
				Extra.CommanderXp = 100;
				NotifyQuest(Profile, EQuestEvent::StageClear);
				if (StageId < TotalCampaignLevels)
				{
					Lines.push_back(Ref.B < CampaignLevels ? StageName(StageId + 1) + " unlocked."
						: "Chapter " + std::to_string(Ref.A + 1) + " unlocked.");
				}
				else
				{
					Lines.push_back("The campaign is complete.");
				}
				break;
			}
			case EStageKind::Tower:
			{
				NotifyQuest(Profile, EQuestEvent::ArkPlay);
				const int Mode = Ref.A == 0 ? 0 : 1;
				int& Best = Profile.Meta.TowerBest[Mode];
				if (Ref.B > Best)
				{
					Best = Ref.B;
					FReward Floor = TowerFloorReward(Mode, Ref.B);
					Floor.ArkMedals += Events::ArkMedalsForFloor(Ref.B);
					Lines.push_back("Floor " + std::to_string(Ref.B) + " cleared. Reward: " + ApplyReward(Profile, Floor));
					Lines.push_back("Next: floor " + std::to_string(Best + 1) + ". The tower never resets.");
				}
				break;
			}
			case EStageKind::BossRush:
				break;   // BIBGameMode pays every round as it is cleared (Arcade::BossRushRoundCleared)
			case EStageKind::SubStory:
			{
				int& Cleared = Profile.Meta.SubStoryCleared[Ref.A];
				if (Ref.B > Cleared)
				{
					Cleared = Ref.B;
					FReward Reward;
					Reward.Gold = 600;   // rescaled 2026-10-06 (PLACEHOLDER)
					Reward.Bytes = 200;
					Reward.CommanderXp = 30;
					if (Ref.B == SubStoryLevels(Ref.A))
					{
						Reward.RegularTickets = 1;
						Reward.Hearts = 10;
					}
					Lines.push_back("First clear reward: " + ApplyReward(Profile, Reward));
				}
				else
				{
					Lines.push_back("Already cleared - no further rewards from this level.");
				}
				break;
			}
			}
			if (Extra.CommanderXp > 0)
			{
				Lines.push_back("Also: " + ApplyReward(Profile, Extra));
			}
			return Lines;
		}

		FActionResult QuickClear(FProfile& Profile, int StageId, int Times)
		{
			if (DecodeStage(StageId).Kind != EStageKind::Campaign || !IsValidStage(StageId))
			{
				return Fail("Quick clear works on campaign levels.");
			}
			if (Campaign::IsWalkStage(StageId))
			{
				return Fail("There is nothing to clear in Return to Nowhere.");
			}
			if (StageId > Profile.StagesCleared)
			{
				return Fail("Clear " + StageName(StageId) + " once by playing it to unlock quick clears.");
			}
			if (!Profile.TeamIsPlayable())
			{
				return Fail(TeamNeedText(Profile));
			}
			const int Cost = Config::UseCharge ? Inventory::ReplayChargeCost(StageId) : 0;
			const int Affordable = Cost > 0 ? Profile.Get(ECurrency::Charge) / Cost : Times;
			const int Count = std::min(std::max(1, Times), Affordable);
			if (Count <= 0)
			{
				return Fail("A quick clear of " + StageName(StageId) + " costs " + std::to_string(Cost) + " Charge (you have " +
					std::to_string(Profile.Get(ECurrency::Charge)) + ").");
			}
			const long long Before[3] = {Profile.Get(ECurrency::Gold), Profile.Get(ECurrency::Bytes), Profile.Get(ECurrency::AngelHearts)};
			const int CoresBefore = Profile.Get(ECurrency::ElementCore);
			for (int Run = 0; Run < Count; ++Run)
			{
				ClearStage(Profile, StageId);
			}
			return Ok("Quick clear x" + std::to_string(Count) + " " + StageName(StageId) + ": +" + Num(Profile.Get(ECurrency::Gold) - Before[0]) + " Gold, +" +
				Num(Profile.Get(ECurrency::Bytes) - Before[1]) + " Bytes, +" + Num(Profile.Get(ECurrency::AngelHearts) - Before[2]) + " Angel hearts" +
				(Profile.Get(ECurrency::ElementCore) > CoresBefore ? ", +" + Num(Profile.Get(ECurrency::ElementCore) - CoresBefore) + " Element cores" : std::string()) +
				", equipment x" + std::to_string(Count) + " (-" + std::to_string(Cost * Count) + " Charge).");
		}

		FReward TowerFloorReward(int Mode, int Floor)
		{
			// First clear of a floor only (owner request 2026-10-06: endless, never resets, "you balance the rewards"). Grows with the
			// floor like the enemies do; Secrets mode (5-6* operators) pays 25% more. PLACEHOLDER.
			const double Mult = Mode == 1 ? 1.25 : 1.0;
			FReward Reward;
			Reward.Gold = static_cast<int>((400 + 80 * static_cast<double>(Floor)) * Mult);
			Reward.Bytes = static_cast<int>((125 + 25 * static_cast<double>(Floor)) * Mult);
			Reward.Hearts = static_cast<int>((2 + Floor / 10) * Mult);
			Reward.Diamonds = Floor % 5 == 0 ? 30 : 0;
			Reward.RegularTickets = Floor % 10 == 0 ? 1 : 0;
			if (Floor % 25 == 0)
			{
				Reward.Diamonds += 100;
				Reward.Equipment = 1;
			}
			if (Floor % 50 == 0)
			{
				Reward.SpecialTickets = 1;
			}
			Reward.CommanderXp = 30;
			return Reward;
		}

		std::vector<std::string> FailStage(FProfile& Profile, int StageId)
		{
			std::vector<std::string> Lines;
			const FStageRef Ref = DecodeStage(StageId);
			if (Ref.Kind == EStageKind::Tower && IsValidStage(StageId))
			{
				const int Mode = Ref.A == 0 ? 0 : 1;
				Profile.Meta.TowerLostAt[Mode] = Ref.B;
				NotifyQuest(Profile, EQuestEvent::ArkPlay);
				Lines.push_back("You fell on floor " + std::to_string(Ref.B) + " - saved. Your best is floor " + std::to_string(Profile.Meta.TowerBest[Mode])
					+ "; the climb resumes from floor " + std::to_string(TowerNextFloor(Profile, Mode)) + ".");
			}
			return Lines;
		}

		// ==================================================================== arena
		// The arena lives in BIBArena.h (owner request 2026-10-06); the team's strength is its battle power.
		double TeamPower(const FProfile& Profile)
		{
			return static_cast<double>(TeamBattlePower(Profile));
		}

		// ================================================================= profile
		int OperatorsOwned(const FProfile& Profile)
		{
			int Count = 0;
			for (const FOwnedOperator& Owned : Profile.Operators)
			{
				Count += Owned.bOwned ? 1 : 0;
			}
			return Count;
		}

		const char* StatName(int Stat)
		{
			static const char* Names[5] = {"Damage", "Health", "Speed", "Ultimate", "Cooldown"};
			return Stat >= 0 && Stat < 5 ? Names[Stat] : "?";
		}
	}
}
