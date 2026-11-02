// Boldinbeauty demo — campaign archetypes, level plans, scaling and rewards (see BIBCampaign.h).
#include "BIBCampaign.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Campaign
	{
		// BIB_CAMPAIGN_BOOTSTRAP: the generator tool (Tests/dump_campaign.cpp) builds this file without the generated tables,
		// because it produces them. Everything it needs (archetypes, plan generator, rewards) does not read the tables.
#ifndef BIB_CAMPAIGN_BOOTSTRAP
#include "BIBCampaignData.inc"
#endif

		namespace
		{
			// Names and base numbers. The Melee, Brute, MiniBoss and Boss rows are the original demo enemies (BIBTuning.h:
			// Tests/campaign_tests.cpp checks they agree); Ranged, Mage and Assassin are new PLACEHOLDERS.
			const FArchetype Table[ArchetypeCount] = {
				// Name, Role, Hp, Damage, Interval, Windup, Speed, Range, KeepMin, KeepMax, AoeRadius, Radius, HalfHeight, AbilityId,
				// Avoid, Reach, Group, SlamDamage, SlamInterval, SlamWindup, SlamRadius
				{"Angel", "Basic melee enemy: walks up and hits.", 2000.0, 40.0, 1.6, 0.5, 330.0, 0.0, 0.0, 0.0, 0.0, 40.0, 90.0, 1,
					0.45, 1.0, EThreatGroup::Melee, 0.0, 0.0, 0.0, 0.0},
				{"Ranged angel", "Ranged enemy: keeps its distance and shoots after a short telegraph.", 1400.0, 32.0, 2.0, 0.6, 300.0, 1500.0, 800.0, 1300.0, 0.0, 40.0, 90.0, 5,
					0.35, 0.75, EThreatGroup::Far, 0.0, 0.0, 0.0, 0.0},
				{"Mage angel", "Mage enemy: marks the ground under the operator and blasts it (area damage); move out of the circle.", 1600.0, 95.0, 4.0, 1.5, 280.0, 1300.0, 700.0, 1100.0, 380.0, 40.0, 90.0, 6,
					0.55, 0.80, EThreatGroup::Far, 0.0, 0.0, 0.0, 0.0},
				{"Heavy angel", "Brute enemy: a slow tank with a hard hit.", 5000.0, 80.0, 1.6, 0.5, 260.0, 0.0, 0.0, 0.0, 0.0, 60.0, 115.0, 2,
					0.45, 1.0, EThreatGroup::Melee, 0.0, 0.0, 0.0, 0.0},
				{"Assassin angel", "Assassin enemy: dashes in and strikes fast and hard; very low health.", 900.0, 75.0, 1.0, 0.3, 560.0, 0.0, 0.0, 0.0, 0.0, 36.0, 90.0, 7,
					0.30, 1.0, EThreatGroup::Rush, 0.0, 0.0, 0.0, 0.0},
				{"MINI-BOSS", "Sub boss: the end of every level.", 40000.0, 120.0, 1.6, 0.5, 300.0, 0.0, 0.0, 0.0, 0.0, 95.0, 190.0, 3,
					0.55, 1.0, EThreatGroup::Boss, 0.0, 0.0, 0.0, 0.0},
				{"CHAPTER BOSS", "Main boss: the end of every chapter; adds a telegraphed slam.", 120000.0, 200.0, 1.6, 0.5, 280.0, 0.0, 0.0, 0.0, 0.0, 140.0, 280.0, 3,
					0.60, 1.0, EThreatGroup::Boss, 300.0, 7.0, 1.5, 700.0},
			};

			// Regular archetypes, introduced chapter by chapter. The mix is the share of the regular enemies in a section.
			double RawWeight(int Chapter, EArchetype Kind)
			{
				// Melee, Ranged, Mage, Brute, Assassin
				double W[RegularArchetypeCount] = {0.36, 0.18, 0.14, 0.16, 0.16};
				if (Chapter <= 2)
				{
					const double C2[RegularArchetypeCount] = {0.62, 0.22, 0.0, 0.16, 0.0};
					std::copy(C2, C2 + RegularArchetypeCount, W);
				}
				else if (Chapter == 3)
				{
					const double C3[RegularArchetypeCount] = {0.50, 0.20, 0.10, 0.20, 0.0};
					std::copy(C3, C3 + RegularArchetypeCount, W);
				}
				else if (Chapter == 4)
				{
					const double C4[RegularArchetypeCount] = {0.42, 0.18, 0.12, 0.16, 0.12};
					std::copy(C4, C4 + RegularArchetypeCount, W);
				}
				else
				{
					// A PLACEHOLDER theme per chapter: one or two archetypes are 60% more common.
					static const int Lean[CampaignChapters + 1][2] = {
						{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
						{1, -1},   // 5  ranged
						{3, -1},   // 6  brutes
						{4, -1},   // 7  assassins
						{2, -1},   // 8  mages
						{0, 1},    // 9  basic + ranged
						{3, 2},    // 10 brutes + mages
						{4, 1},    // 11 assassins + ranged
						{2, 4},    // 12 mages + assassins
						{-1, -1}}; // 13 balanced finale
					const int Safe = std::clamp(Chapter, 0, CampaignChapters);
					for (int Pick : Lean[Safe])
					{
						if (Pick >= 0)
						{
							W[Pick] *= 1.6;
						}
					}
				}
				const int Index = static_cast<int>(Kind);
				return Index >= 0 && Index < RegularArchetypeCount ? W[Index] : 0.0;
			}

			// Largest-remainder split of Total over the weights; every introduced archetype gets at least one enemy when the
			// section has room for them. Deterministic: the tool and the game must produce the same plan.
			void Apportion(int Total, const double* Weights, int Count, int* Out)
			{
				double Sum = 0.0;
				for (int Index = 0; Index < Count; ++Index)
				{
					Sum += Weights[Index];
				}
				double Remainder[RegularArchetypeCount] = {};
				int Used = 0;
				for (int Index = 0; Index < Count; ++Index)
				{
					const double Raw = Sum > 0.0 ? Total * Weights[Index] / Sum : 0.0;
					Out[Index] = static_cast<int>(std::floor(Raw));
					Remainder[Index] = Raw - Out[Index];
					Used += Out[Index];
				}
				for (int Left = Total - Used; Left > 0; --Left)
				{
					int Best = -1;
					for (int Index = 0; Index < Count; ++Index)
					{
						if (Weights[Index] > 0.0 && (Best < 0 || Remainder[Index] > Remainder[Best]))
						{
							Best = Index;
						}
					}
					if (Best < 0)
					{
						break;
					}
					++Out[Best];
					Remainder[Best] = -1.0;
				}
				if (Total >= 12)
				{
					for (int Index = 0; Index < Count; ++Index)
					{
						if (Weights[Index] > 0.0 && Out[Index] == 0)
						{
							int Donor = 0;
							for (int Other = 1; Other < Count; ++Other)
							{
								if (Out[Other] > Out[Donor])
								{
									Donor = Other;
								}
							}
							--Out[Donor];
							Out[Index] = 1;
						}
					}
				}
			}

			void SetCount(FSectionPlanData& Section, EArchetype Kind, int Count)
			{
				switch (Kind)
				{
				case EArchetype::Melee: Section.Grunts = Count; break;
				case EArchetype::Ranged: Section.Ranged = Count; break;
				case EArchetype::Mage: Section.Mages = Count; break;
				case EArchetype::Brute: Section.Brutes = Count; break;
				case EArchetype::Assassin: Section.Assassins = Count; break;
				case EArchetype::MiniBoss: Section.MiniBosses = Count; break;
				case EArchetype::Boss: Section.Bosses = Count; break;
				case EArchetype::Count: break;
				}
			}
		}

		// ------------------------------------------------------------------ archetypes
		const FArchetype& Archetype(EArchetype Kind)
		{
			const int Index = std::clamp(static_cast<int>(Kind), 0, ArchetypeCount - 1);
			return Table[Index];
		}

		const char* ArchetypeName(EArchetype Kind)
		{
			return Archetype(Kind).Name;
		}

		// ---------------------------------------------------------------------- plans
		int SectionsInLevel(int Chapter, int Level)
		{
			// Levels get longer through a chapter; the last level of a chapter is the longest (mini-boss section, boss section).
			static const int Sections[CampaignLevels] = {2, 3, 3, 3, 4, 4, 5, 5};
			(void)Chapter;
			return Sections[std::clamp(Level, 1, CampaignLevels) - 1];
		}

		double ArchetypeWeight(int Chapter, EArchetype Kind)
		{
			if (Chapter < FirstGeneratedChapter)
			{
				return Kind == EArchetype::Melee || Kind == EArchetype::Brute ? 1.0 : 0.0;
			}
			double Sum = 0.0;
			for (int Index = 0; Index < RegularArchetypeCount; ++Index)
			{
				Sum += RawWeight(Chapter, static_cast<EArchetype>(Index));
			}
			return Sum > 0.0 ? RawWeight(Chapter, Kind) / Sum : 0.0;
		}

		bool IsIntroduced(int Chapter, EArchetype Kind)
		{
			return ArchetypeWeight(Chapter, Kind) > 0.0;
		}

		std::string ChapterTheme(int Chapter)
		{
			switch (Chapter)
			{
			case 1: return "Basic and heavy enemies";
			case 2: return "Ranged enemies join the fight";
			case 3: return "Mages (area blasts) join the fight";
			case 4: return "Assassins join the fight";
			case 5: return "All enemy types, more ranged enemies";
			case 6: return "All enemy types, more brutes";
			case 7: return "All enemy types, more assassins";
			case 8: return "All enemy types, more mages";
			case 9: return "All enemy types, more basic and ranged enemies";
			case 10: return "All enemy types, more brutes and mages";
			case 11: return "Return to Nowhere (a walk, no enemies)";
			case 12: return "All enemy types, more mages and assassins";
			case 13: return "All enemy types, in force";
			default: return "";
			}
		}

		std::vector<FSectionPlanData> GeneratePlan(int Chapter, int Level, int SectionSize)
		{
			std::vector<FSectionPlanData> Plan;
			if (Chapter < FirstGeneratedChapter || Chapter > CampaignChapters || Level < 1 || Level > CampaignLevels)
			{
				return Plan;
			}
			const int Size = std::clamp(SectionSize, MinSectionSize, MaxSectionSize);
			const int Sections = SectionsInLevel(Chapter, Level);
			const bool bFinal = Level == CampaignLevels;
			double Weights[RegularArchetypeCount];
			for (int Index = 0; Index < RegularArchetypeCount; ++Index)
			{
				Weights[Index] = ArchetypeWeight(Chapter, static_cast<EArchetype>(Index));
			}
			for (int Section = 0; Section < Sections; ++Section)
			{
				// The mini boss closes every level; in a chapter's last level it holds the section before the boss.
				const bool bMini = bFinal ? Section == Sections - 2 : Section == Sections - 1;
				const bool bBoss = bFinal && Section == Sections - 1;
				const int Regular = Size - (bMini || bBoss ? 1 : 0);
				int Counts[RegularArchetypeCount] = {};
				Apportion(Regular, Weights, RegularArchetypeCount, Counts);
				FSectionPlanData Data;
				for (int Index = 0; Index < RegularArchetypeCount; ++Index)
				{
					SetCount(Data, static_cast<EArchetype>(Index), Counts[Index]);
				}
				Data.MiniBosses = bMini ? 1 : 0;
				Data.Bosses = bBoss ? 1 : 0;
				Plan.push_back(Data);
			}
			return Plan;
		}

		int SectionSizeFor(int Chapter, int Level)
		{
#ifdef BIB_CAMPAIGN_BOOTSTRAP
			(void)Chapter;
			(void)Level;
			return 20;
#else
			if (Chapter < FirstGeneratedChapter || Chapter > CampaignChapters || Level < 1 || Level > CampaignLevels)
			{
				return MinSectionSize;
			}
			return Data::SectionSize[Chapter - FirstGeneratedChapter][Level - 1];
#endif
		}

		std::vector<FSectionPlanData> ChapterPlan(int Chapter, int Level)
		{
			if (Chapter == WalkChapter)
			{
				return {};   // Return to Nowhere: no enemies
			}
			return GeneratePlan(Chapter, Level, SectionSizeFor(Chapter, Level));
		}

		int CountOf(const FSectionPlanData& Section, EArchetype Kind)
		{
			switch (Kind)
			{
			case EArchetype::Melee: return Section.Grunts;
			case EArchetype::Ranged: return Section.Ranged;
			case EArchetype::Mage: return Section.Mages;
			case EArchetype::Brute: return Section.Brutes;
			case EArchetype::Assassin: return Section.Assassins;
			case EArchetype::MiniBoss: return Section.MiniBosses;
			case EArchetype::Boss: return Section.Bosses;
			case EArchetype::Count: break;
			}
			return 0;
		}

		std::string EnemyMixText(const std::vector<FSectionPlanData>& Plan)
		{
			static const char* const Labels[ArchetypeCount] = {"basic", "ranged", "mage", "brute", "assassin", "mini-boss", "boss"};
			static const char* const Plurals[ArchetypeCount] = {"basic", "ranged", "mages", "brutes", "assassins", "mini-bosses", "bosses"};
			std::string Text;
			for (int Index = 0; Index < ArchetypeCount; ++Index)
			{
				int Count = 0;
				for (const FSectionPlanData& Section : Plan)
				{
					Count += CountOf(Section, static_cast<EArchetype>(Index));
				}
				if (Count > 0)
				{
					if (!Text.empty())
					{
						Text += ", ";
					}
					Text += std::to_string(Count) + " " + (Count == 1 ? Labels[Index] : Plurals[Index]);
				}
			}
			return Text;
		}

		// ------------------------------------------------------------------ scaling and rewards
		FEnemyScale CampaignScales(int GlobalLevel)
		{
			FEnemyScale Scale;
			const int Level = std::clamp(GlobalLevel, 1, TotalCampaignLevels);
#ifdef BIB_CAMPAIGN_BOOTSTRAP
			(void)Level;
#else
			Scale.Hp = Data::HpScale[Level - 1];
			Scale.Damage = Data::DamageScale[Level - 1];
#endif
			return Scale;
		}

		int RecommendedLevel(int GlobalLevel)
		{
#ifdef BIB_CAMPAIGN_BOOTSTRAP
			return std::clamp(GlobalLevel, 1, TotalCampaignLevels);
#else
			return Data::Recommended[std::clamp(GlobalLevel, 1, TotalCampaignLevels) - 1];
#endif
		}

		bool IsWalkStage(int GlobalLevel)
		{
			return GlobalLevel >= WalkEntryStage() && GlobalLevel <= WalkLastStage();
		}

		int WalkEntryStage()
		{
			return (WalkChapter - 1) * CampaignLevels + 1;
		}

		int WalkLastStage()
		{
			return WalkChapter * CampaignLevels;
		}

		bool IsBossStage(int GlobalLevel)
		{
			return GlobalLevel >= 1 && GlobalLevel <= TotalCampaignLevels && GlobalLevel % CampaignLevels == 0 && !IsWalkStage(GlobalLevel);
		}

		FStageReward StageReward(int GlobalLevel)
		{
			if (IsWalkStage(GlobalLevel))
			{
				return FStageReward();   // nothing happens
			}
			return CurveReward(GlobalLevel);
		}

		FStageReward CurveReward(int GlobalLevel)
		{
			const int Level = std::clamp(GlobalLevel, 1, TotalCampaignLevels);
			const int Chapter = (Level - 1) / CampaignLevels + 1;
			const double Growth = (1.0 + RewardGrowthPerStage * (Level - 1)) * (IsBossStage(Level) ? RewardBossMultiplier : 1.0);
			auto Round10 = [](double Value) { return static_cast<int>(std::lround(Value / 10.0)) * 10; };
			FStageReward Reward;
			Reward.Gold = Round10(RewardGoldBase * Growth);
			Reward.Bytes = Round10(RewardBytesBase * Growth);
			const double HeartGrowth = (1.0 + RewardHeartsGrowthPerStage * (Level - 1)) * (IsBossStage(Level) ? RewardBossMultiplier : 1.0);
			Reward.Hearts = static_cast<int>(std::lround(RewardHeartsBase * HeartGrowth));
			Reward.Diamonds = (RewardDiamondsBase + RewardDiamondsPerChapter * (Chapter - 1)) * (IsBossStage(Level) ? RewardBossDiamondsMultiplier : 1);
			return Reward;
		}
	}
}
