// Tests for the 13-chapter campaign: stage ids and unlocks, the level plans (2-5 sections, 10-30 enemies, a mini boss at the
// end of every level, the boss at the end of every chapter), the enemy archetypes, rewards, saves, the hub pages, and the
// balance tables (they must be the ones the balance model produces from the current rules).
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "campaign_model.h"

#include "../Source/BoldInBeautyDemo/BIBRules/BIBInventory.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBKitTypes.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
using int32 = int;   // BIBTuning.h is shared with the Unreal layer
#include "../Source/BoldInBeautyDemo/BIBGame/BIBTuning.h"

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
	using namespace BIB::Campaign;
	namespace P = BIB::Pages;

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	int PlanTotal(const std::vector<FSectionPlanData>& Plan)
	{
		int Total = 0;
		for (const FSectionPlanData& Section : Plan)
		{
			Total += Section.Total();
		}
		return Total;
	}

	int PlanCount(const std::vector<FSectionPlanData>& Plan, EArchetype Kind)
	{
		int Count = 0;
		for (const FSectionPlanData& Section : Plan)
		{
			Count += CountOf(Section, Kind);
		}
		return Count;
	}

	// ---------------------------------------------------------------------- stage ids
	void TestStageIds()
	{
		Expect(CampaignLevels == 8 && CampaignChapters == 13 && TotalCampaignLevels == 104, "13 chapters of 8 levels = 104 campaign stages");
		Expect(CampaignStageId(1) == 1 && CampaignStageId(8) == 8 && CampaignStageId(1, 8) == 8, "Chapter 1 stage ids are the original 1..8");
		Expect(CampaignStageId(2, 1) == 9 && CampaignStageId(13, 8) == 104, "later chapters continue the numbering");
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			const int Chapter = ChapterOfStage(Stage);
			const int Level = LevelOfStage(Stage);
			Expect(Chapter >= 1 && Chapter <= 13 && Level >= 1 && Level <= 8 && CampaignStageId(Chapter, Level) == Stage, "stage " + std::to_string(Stage) + " round-trips");
			const FStageRef Ref = DecodeStage(Stage);
			Expect(Ref.Kind == EStageKind::Campaign && Ref.A == Chapter && Ref.B == Level && IsValidStage(Stage), "stage " + std::to_string(Stage) + " decodes");
			Expect(IsChapterFinal(Stage) == (Level == CampaignLevels), "chapter-final flag of stage " + std::to_string(Stage));
			Expect(Contains(StageName(Stage), "Chapter " + std::to_string(Chapter) + (IsWalkStage(Stage) ? std::string(" - Return to Nowhere") : " - Level " + std::to_string(Level))), "stage " + std::to_string(Stage) + " has its name");
		}
		Expect(!IsValidStage(0) && !IsValidStage(105) && !IsValidStage(999), "stages outside 1..104 are not campaign stages");
		Expect(IsValidStage(TowerStageId(0, 1)) && DecodeStage(TowerStageId(0, 1)).Kind == EStageKind::Tower, "tower stage ids are unchanged");
		Expect(ChapterOfStage(0) == 0 && ChapterOfStage(105) == 0 && LevelOfStage(0) == 0, "no chapter outside the campaign");
		Expect(ChapterFirstStage(5) == 33, "chapter 5 starts at stage 33");
		Expect(Contains(StageName(16), "chapter end") && !Contains(StageName(15), "chapter end"), "the last level of a chapter says so");
	}

	// ---------------------------------------------------------------------- plans
	void TestPlans()
	{
		int Saturated = 0;
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			const int Chapter = ChapterOfStage(Stage);
			const int Level = LevelOfStage(Stage);
			const bool bFinal = Level == CampaignLevels;
			const std::string Name = StageName(Stage);
			const std::vector<FSectionPlanData> Plan = StagePlan(Stage);
			if (IsWalkStage(Stage))
			{
				// Return to Nowhere (owner level idea): no enemies, no boss, no rewards, no Charge (2026-10-09)
				const FStageReward Nothing = StageReward(Stage);
				Expect(Plan.empty() && !IsBossStage(Stage) && Nothing.Gold == 0 && Nothing.Diamonds == 0 && Inventory::ReplayChargeCost(Stage) == 0, Name + ": a walk with nothing in it");
				continue;
			}
			Expect(Plan.size() >= static_cast<size_t>(MinSections) && Plan.size() <= static_cast<size_t>(MaxSections), Name + ": 2-5 sections");
			int Minis = 0;
			int Bosses = 0;
			for (size_t Index = 0; Index < Plan.size(); ++Index)
			{
				const int Total = Plan[Index].Total();
				Expect(Total >= MinSectionSize && Total <= MaxSectionSize, Name + " section " + std::to_string(Index + 1) + ": 10-30 enemies (" + std::to_string(Total) + ")");
				Minis += Plan[Index].MiniBosses;
				Bosses += Plan[Index].Bosses;
				Expect(Plan[Index].MiniBosses <= 1 && Plan[Index].Bosses <= 1, Name + ": at most one mini-boss and one boss per section");
			}
			Expect(Minis == 1, Name + ": exactly one mini-boss per level (" + std::to_string(Minis) + ")");
			Expect(Bosses == (bFinal ? 1 : 0), Name + ": the boss only at the chapter end");
			Expect(Plan.back().MiniBosses + Plan.back().Bosses >= 1, Name + ": the last section ends the level with a boss fight");
			if (Chapter >= FirstGeneratedChapter)
			{
				if (bFinal)
				{
					Expect(Plan.size() == 5, Name + ": the chapter's last level has five sections");
					Expect(Plan[Plan.size() - 2].MiniBosses == 1 && Plan[Plan.size() - 2].Bosses == 0, Name + ": the mini-boss closes the section before the boss");
					Expect(Plan.back().Bosses == 1 && Plan.back().MiniBosses == 0, Name + ": the boss closes the last section");
				}
				else
				{
					Expect(Plan.back().MiniBosses == 1 && Plan.back().Bosses == 0, Name + ": the mini-boss closes the last section");
					for (size_t Index = 0; Index + 1 < Plan.size(); ++Index)
					{
						Expect(Plan[Index].MiniBosses == 0, Name + ": no early mini-boss");
					}
				}
				Expect(StagePlan(Stage).size() == Plan.size() && PlanTotal(StagePlan(Stage)) == PlanTotal(Plan), Name + ": the plan is deterministic");
				const int Size = SectionSizeFor(Chapter, Level);
				Expect(Size >= MinSectionSize && Size <= MaxSectionSize, Name + ": the generated section size is in range");
				if (Size == MinSectionSize || Size == MaxSectionSize)
				{
					++Saturated;
				}
				// every archetype that has been introduced is in every level
				for (int Index = 0; Index < RegularArchetypeCount; ++Index)
				{
					const EArchetype Kind = static_cast<EArchetype>(Index);
					const int Count = PlanCount(Plan, Kind);
					Expect(IsIntroduced(Chapter, Kind) == (Count > 0), Name + ": " + ArchetypeName(Kind) + " appears exactly when introduced");
				}
			}
			else
			{
				Expect(PlanCount(Plan, EArchetype::Ranged) == 0 && PlanCount(Plan, EArchetype::Mage) == 0 && PlanCount(Plan, EArchetype::Assassin) == 0, Name + ": Chapter 1 has only basic and heavy enemies");
			}
		}
		// A level at the smallest size (10) may still be harder than its target: the section size cannot go lower (the model says so).
		Expect(Saturated <= 48, "at most half of the 96 generated levels sit at the smallest or largest section size (" + std::to_string(Saturated) + ")");
		// Chapter 1 stays as the owner saw it: levels 1-8 have 2,2,3,3,3,4,4,5 sections and the boss shares the last section with the mini-boss.
		const size_t ChapterOneSections[8] = {2, 2, 3, 3, 3, 4, 4, 5};
		for (int Level = 1; Level <= 8; ++Level)
		{
			Expect(StagePlan(CampaignStageId(1, Level)).size() == ChapterOneSections[Level - 1], "Chapter 1 level " + std::to_string(Level) + " keeps its sections");
		}
		const std::vector<FSectionPlanData> Last = StagePlan(CampaignStageId(1, 8));
		Expect(Last.back().Bosses == 1 && Last.back().MiniBosses == 1 && Last.back().Grunts == 8 && Last.back().Brutes == 6, "Chapter 1 level 8 is unchanged");

		// archetype introduction: ranged in 2, mages in 3, assassins in 4, everything from 4 on
		Expect(IsIntroduced(2, EArchetype::Ranged) && !IsIntroduced(2, EArchetype::Mage) && !IsIntroduced(2, EArchetype::Assassin), "chapter 2: ranged only");
		Expect(IsIntroduced(3, EArchetype::Mage) && !IsIntroduced(3, EArchetype::Assassin), "chapter 3: mages join");
		for (int Chapter = 4; Chapter <= CampaignChapters; ++Chapter)
		{
			for (int Index = 0; Index < RegularArchetypeCount; ++Index)
			{
				Expect(IsIntroduced(Chapter, static_cast<EArchetype>(Index)), "chapter " + std::to_string(Chapter) + " uses every archetype");
			}
			double Sum = 0.0;
			for (int Index = 0; Index < RegularArchetypeCount; ++Index)
			{
				Sum += ArchetypeWeight(Chapter, static_cast<EArchetype>(Index));
			}
			Expect(std::fabs(Sum - 1.0) < 1e-9, "chapter " + std::to_string(Chapter) + " weights add up to 1");
			Expect(!ChapterTheme(Chapter).empty(), "chapter " + std::to_string(Chapter) + " has a theme line");
		}
		Expect(ArchetypeWeight(7, EArchetype::Assassin) > ArchetypeWeight(5, EArchetype::Assassin), "chapter 7 leans on assassins");
		Expect(ArchetypeWeight(8, EArchetype::Mage) > ArchetypeWeight(7, EArchetype::Mage), "chapter 8 leans on mages");
		Expect(GeneratePlan(1, 1, 20).empty() && GeneratePlan(14, 1, 20).empty() && GeneratePlan(2, 9, 20).empty(), "no generated plan outside chapters 2-13, levels 1-8");
		Expect(GeneratePlan(2, 1, 3).front().Total() == MinSectionSize && GeneratePlan(2, 1, 99).front().Total() == MaxSectionSize, "section size is clamped to 10-30");
		const std::string Mix = EnemyMixText(StagePlan(CampaignStageId(4, 3)));
		Expect(Contains(Mix, "basic") && Contains(Mix, "ranged") && Contains(Mix, "mages") && Contains(Mix, "brutes") && Contains(Mix, "assassins") && Contains(Mix, "1 mini-boss"), "the mix text names every type: " + Mix);
	}

	// ----------------------------------------------------------------- archetypes
	void TestArchetypes()
	{
		Expect(ArchetypeCount == 7 && RegularArchetypeCount == 5, "seven archetypes");
		// the original demo enemies are unchanged (BIBTuning.h is the Unreal side's copy of them)
		const FArchetype& Melee = Archetype(EArchetype::Melee);
		const FArchetype& Brute = Archetype(EArchetype::Brute);
		const FArchetype& Mini = Archetype(EArchetype::MiniBoss);
		const FArchetype& Boss = Archetype(EArchetype::Boss);
		Expect(Melee.Hp == BIBTuning::GruntHP && Melee.Damage == BIBTuning::GruntDamage && Melee.Speed == BIBTuning::GruntSpeed, "basic enemy = BIBTuning grunt");
		Expect(Brute.Hp == BIBTuning::BruteHP && Brute.Damage == BIBTuning::BruteDamage && Brute.Speed == BIBTuning::BruteSpeed, "brute = BIBTuning brute");
		Expect(Mini.Hp == BIBTuning::MiniBossHP && Mini.Damage == BIBTuning::MiniBossDamage && Mini.Speed == BIBTuning::MiniBossSpeed, "mini-boss = BIBTuning mini-boss");
		Expect(Boss.Hp == BIBTuning::BossHP && Boss.Damage == BIBTuning::BossDamage && Boss.Speed == BIBTuning::BossSpeed, "boss = BIBTuning boss");
		Expect(Boss.SlamDamage == BIBTuning::BossSlamDamage && Boss.SlamInterval == BIBTuning::BossSlamInterval && Boss.SlamWindup == BIBTuning::BossSlamWindup && Boss.SlamRadius == BIBTuning::BossSlamRadius, "boss slam = BIBTuning");
		Expect(std::fabs(Melee.Interval - BIBTuning::EnemyAttackInterval) < 1e-6 && std::fabs(Melee.Windup - BIBTuning::EnemyAttackWindup) < 1e-6, "attack timing = BIBTuning");
		// the roles the owner listed
		const FArchetype& Ranged = Archetype(EArchetype::Ranged);
		const FArchetype& Mage = Archetype(EArchetype::Mage);
		const FArchetype& Assassin = Archetype(EArchetype::Assassin);
		Expect(Ranged.Range > 1000.0 && Ranged.KeepMin > 0.0 && Ranged.KeepMax > Ranged.KeepMin && Ranged.Range >= Ranged.KeepMax, "ranged enemies keep their distance and shoot from afar");
		Expect(Mage.AoeRadius > 0.0 && Mage.Windup > Melee.Windup && Mage.Damage > Melee.Damage && Mage.Interval > Melee.Interval, "mages deal telegraphed area damage, slowly");
		Expect(Brute.Hp > 2.0 * Melee.Hp && Brute.Speed < Melee.Speed, "brutes are slow tanks");
		Expect(Assassin.Hp < Melee.Hp && Assassin.Damage / Assassin.Interval > 2.0 * Melee.Damage / Melee.Interval && Assassin.Speed > Melee.Speed && Assassin.Range == 0.0,
			"assassins: low health, high damage at close range, fast");
		Expect(Mini.Hp > 5.0 * Brute.Hp && Boss.Hp > 2.0 * Mini.Hp, "mini-bosses and bosses are far above the regular enemies");
		std::set<int> Ids;
		for (int Index = 0; Index < ArchetypeCount; ++Index)
		{
			const FArchetype& A = Archetype(static_cast<EArchetype>(Index));
			Expect(A.Hp > 0.0 && A.Damage > 0.0 && A.Interval > 0.0 && A.Windup > 0.0 && A.Windup < A.Interval && A.Speed > 0.0 && A.Radius > 0.0 && A.HalfHeight > 0.0, std::string(A.Name) + " has sane numbers");
			Expect(A.Avoid > 0.0 && A.Avoid < 1.0 && A.Reach > 0.0 && A.Reach <= 1.0, std::string(A.Name) + " has sane balance-model numbers");
			Expect(A.Name && A.Role && std::string(A.Name).size() > 0, "every archetype is named");
			Ids.insert(A.AbilityId);
		}
		Expect(Ids.size() >= 6, "the new archetypes have their own enemy ability ids (Adapt copies them)");
		for (const int Id : Ids)
		{
			bool bKnown = false;
			for (const BIB::Kits::FEnemyAbility& Ability : BIB::Kits::EnemyAbilities())
			{
				bKnown = bKnown || Ability.Id == Id;
			}
			Expect(bKnown, "enemy ability id " + std::to_string(Id) + " is known to the kit registry");
		}
	}

	// ---------------------------------------------------------------------- scales and tables
	void TestScales()
	{
		Expect(StageScales(1).Hp == 1.0 && StageScales(1).Damage == 1.0, "stage 1 enemies are at base strength");
		Expect(StageScales(8).Hp > 1.8 && StageScales(8).Hp < 2.6, "the Chapter 1 boss level stays near the old 2.05x (now solved from the reference player)");
		Expect(Config::StatPercentPerLevel >= 5.0, "operators grow much faster per level than the old +1% (owner rebalance 2026-10-06)");
		Expect(StageEnemyScale(CampaignStageId(8)) == StageScales(CampaignStageId(8)).Hp, "StageEnemyScale is the health scale");
		double Previous = 0.0;
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			const FEnemyScale Scale = StageScales(Stage);
			Expect(Scale.Hp >= 1.0 && Scale.Damage >= 1.0, "stage " + std::to_string(Stage) + ": enemies are never weaker than base");
			if (Stage > 1)
			{
				Expect(Scale.Hp >= Previous * 0.94, "stage " + std::to_string(Stage) + ": health scale never falls more than 6% (a chapter with a more dangerous mix)");
			}
			Previous = Scale.Hp;
			if (Stage > CampaignLevels)
			{
				Expect(Scale.Hp > 2.0 && Scale.Hp < 16.0 && Scale.Damage > 2.0 && Scale.Damage < 16.0, "stage " + std::to_string(Stage) + ": scales stay in a sane range");
			}
		}
		Expect(StageScales(TotalCampaignLevels).Hp > 1.8 * StageScales(CampaignLevels + 1).Hp, "enemies roughly double from Chapter 2 to Chapter 13");
		Expect(StageScales(TotalCampaignLevels).Damage > StageScales(TotalCampaignLevels).Hp, "late enemies hit harder than they tank: the player's health grows more than their damage");
		// every chapter has easier early and harder late levels
		for (int Chapter = 2; Chapter <= CampaignChapters; ++Chapter)
		{
			Expect(StageScales(CampaignStageId(Chapter, 8)).Hp > StageScales(CampaignStageId(Chapter, 1)).Hp, "chapter " + std::to_string(Chapter) + " gets harder through its levels");
		}
		for (int Stage = 2; Stage <= TotalCampaignLevels; ++Stage)
		{
			Expect(RecommendedLevel(Stage) >= RecommendedLevel(Stage - 1), "recommended level never falls (stage " + std::to_string(Stage) + ")");
		}
		Expect(RecommendedLevel(1) == 1, "the first level is for level-1 operators");
		Expect(RecommendedLevel(TotalCampaignLevels) >= 80 && RecommendedLevel(TotalCampaignLevels) <= Config::BaseLevelCap + 100, "the last stage asks for a team near the base level cap");
		Expect(RecommendedLevel(0) == RecommendedLevel(1) && RecommendedLevel(500) == RecommendedLevel(TotalCampaignLevels), "recommended level clamps");
	}

	// ----------------------------------------------------------------------- balance model
	void TestBalanceModel()
	{
		using namespace CampaignModel;
		const FSolved Solved = SolveAll();
		const FStageTables& Tables = Solved.Tables;
		// the shipped tables are the model's answer for the current rules
		bool bCurrent = true;
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			const FEnemyScale Scale = StageScales(Stage);
			const FEnemyScale& Model = Tables.Scale[static_cast<size_t>(Stage)];
			if (std::fabs(Scale.Hp - Model.Hp) > 1e-6 * Model.Hp || std::fabs(Scale.Damage - Model.Damage) > 1e-6 * Model.Damage ||
				RecommendedLevel(Stage) != Tables.Recommended[static_cast<size_t>(Stage)])
			{
				bCurrent = false;
			}
		}
		for (int Chapter = FirstGeneratedChapter; Chapter <= CampaignChapters; ++Chapter)
		{
			for (int Level = 1; Level <= CampaignLevels; ++Level)
			{
				if (SectionSizeFor(Chapter, Level) != Solved.Sizes[static_cast<size_t>((Chapter - FirstGeneratedChapter) * CampaignLevels + Level - 1)].Size)
				{
					bCurrent = false;
				}
			}
		}
		Expect(bCurrent, "BIBCampaignData.inc is current: rerun Tools/gen_campaign_data.py after changing rewards, costs or the model");

		// the reference player: a bit of grinding, never a wall
		int TotalReplays = 0;
		int MostReplays = 0;
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			const FPaceRow& Row = Tables.Pace[static_cast<size_t>(Stage)];
			TotalReplays += Row.Replays;
			MostReplays = std::max(MostReplays, Row.Replays);
			Expect(Row.LevelRequired + 1e-9 >= Row.LevelAtPace - 1.0, "stage " + std::to_string(Stage) + ": the requirement is not below the player's level");
			Expect(Row.Replays <= 14, "stage " + std::to_string(Stage) + ": at most fourteen quick clears to be ready (" + std::to_string(Row.Replays) + ")");
		}
		const double ClearsPerStage = static_cast<double>(TotalCampaignLevels + TotalReplays) / TotalCampaignLevels;
		// Classic gacha rebalance (2026-10-07): the grind is Charge spent on quick clears, so several clears per stage are fine.
		Expect(ClearsPerStage >= 4.0 && ClearsPerStage <= 8.0, "about five to six clears per stage, mostly quick clears for Charge (" + std::to_string(ClearsPerStage) + ")");
		Expect(MostReplays <= 14, "no stage needs more than fourteen quick clears (" + std::to_string(MostReplays) + ")");
		for (int Chapter = FirstGeneratedChapter; Chapter <= CampaignChapters; ++Chapter)
		{
			const int Boss = CampaignStageId(Chapter, CampaignLevels);
			// (a boss at the level ceiling of the unlucky reference player, average 140 without a third star, has nothing left to grind for)
			const bool bAtCeiling = Tables.Pace[static_cast<size_t>(Boss)].LevelRequired <= Tables.Pace[static_cast<size_t>(Boss)].LevelAtPace + 1e-9;
			Expect(bAtCeiling || Tables.Pace[static_cast<size_t>(Boss)].Replays >= Tables.Pace[static_cast<size_t>(Boss - 1)].Replays, "chapter " + std::to_string(Chapter) + ": the boss needs at least as much grinding as the level before");
		}
		Expect(Tables.Pace[TotalCampaignLevels].Stars >= 0 && Tables.Pace[TotalCampaignLevels].Stars <= 8, "the reference player has at most a few upgrade stars by the end");
		// Owner rebalance 2026-10-06: Diamonds are scarce (300 to start, 50+ per first clear); the story quests add the tickets.
		Expect(Tables.Pace[TotalCampaignLevels].Pulls > 50 && Tables.Pace[TotalCampaignLevels].Pulls < 200, "dozens of summons over the whole campaign (quests and dailies add more)");

		// each generated level is near its threat target (a level at the smallest size that is still too hard is the exception)
		const FThreatContext Context = MakeThreatContext(Tables);
		int OffTarget = 0;
		for (int Chapter = FirstGeneratedChapter; Chapter <= CampaignChapters; ++Chapter)
		{
			double Previous = 0.0;
			for (int Level = 1; Level <= CampaignLevels; ++Level)
			{
				const FSizeChoice& Choice = Solved.Sizes[static_cast<size_t>((Chapter - FirstGeneratedChapter) * CampaignLevels + Level - 1)];
				const double Error = std::fabs(std::log(Choice.Threat / Choice.Target));
				if (Error > 0.15)
				{
					++OffTarget;
				}
				Expect(Choice.Threat <= Choice.Target * OverTargetTrim + 1e-9, "chapter " + std::to_string(Chapter) + " level " + std::to_string(Level) + ": no level is more than 5% harder than its threat target (2026-10-09 trim)");
				if (Level < CampaignLevels)
				{
					// one enemy more or less per section moves a level by about 10-25% (the mix is rounded), so allow some noise
					Expect(Choice.Threat >= Previous * 0.75, "chapter " + std::to_string(Chapter) + " level " + std::to_string(Level) + ": the threat does not fall through a chapter");
					Previous = Choice.Threat;
				}
				else
				{
					Expect(Choice.Threat > 1.5 * Previous, "chapter " + std::to_string(Chapter) + ": the boss level is a real gate (" + std::to_string(Choice.Threat) + " vs " + std::to_string(Previous) + ")");
				}
			}
		}
		Expect(OffTarget <= 8, "at most 8 of the 96 generated levels miss their threat target by more than 15% (" + std::to_string(OffTarget) + ")");
		// the threat the model reports for the shipped plans is the one the solver chose
		const int Stage = CampaignStageId(6, 5);
		const double Threat = Context.Threat(Stage, StagePlan(Stage));
		Expect(std::fabs(Threat - Solved.Sizes[static_cast<size_t>((6 - FirstGeneratedChapter) * CampaignLevels + 4)].Threat) < 1e-9, "the shipped plan is the solved plan");
		Expect(std::fabs(Context.Threat(CampaignStageId(7), StagePlan(CampaignStageId(7))) - 1.0) < 1e-9, "Chapter 1 level 7 is the threat reference (1.0)");
		Expect(TeamHealth() > 10000.0 && TeamHealth() < 15000.0, "the reference team's health pool is Luna + Plack + Kuu");
		// the model reacts to the economy: doubling every reward lets the player level faster, so the same stage asks for more
		Expect(UpgradeUnitsPerPull > 1.0 && UpgradeUnitsPerPull < 2.0, "a pull brings about 1.3 upgrade units to a given 4-star");
	}

	// ---------------------------------------------------------------------- rewards
	void TestRewards()
	{
		const FStageReward One = StageReward(1);
		Expect(One.Gold == 800 && One.Bytes == 250 && One.Hearts == 2 && One.Diamonds == 50, "owner: stage 1 pays 800 Gold, 250 Bytes, 2 Angel hearts and 50 Diamonds");
		for (int Stage = 2; Stage <= TotalCampaignLevels; ++Stage)
		{
			if (IsWalkStage(Stage) || IsWalkStage(Stage - 1) || IsWalkStage(Stage - 2))
			{
				continue;   // the walk pays nothing; the curve resumes after it (CurveReward)
			}
			const FStageReward Now = StageReward(Stage);
			// The normal curve rises every stage; a boss level pays double, so compare normal levels with normal levels.
			const int Previous = IsBossStage(Stage - 1) ? Stage - 2 : Stage - 1;
			const FStageReward Before = StageReward(Previous);
			Expect(Now.Gold > Before.Gold && Now.Bytes > Before.Bytes && Now.Hearts >= Before.Hearts, "stage " + std::to_string(Stage) + ": Gold, Bytes and hearts rise");
			if (IsBossStage(Stage))
			{
				Expect(Now.Gold > StageReward(Stage - 1).Gold && Now.Diamonds == 3 * StageReward(Stage - 1).Diamonds, "stage " + std::to_string(Stage) + ": a boss level pays more");
			}
		}
		Expect(StageReward(16).Diamonds == 180 && StageReward(15).Diamonds == 60 && StageReward(TotalCampaignLevels - 1).Diamonds == 170, "first-clear Diamonds: 50 + 10 per chapter, x3 on a boss level");
		Expect(StageReward(0).Gold == StageReward(1).Gold && StageReward(500).Gold == StageReward(TotalCampaignLevels).Gold, "rewards clamp");

		// first clear vs replay
		FProfile Player(101);
		Player.StartNewPlayer();
		Player.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		const int Diamonds = Player.Get(ECurrency::Diamonds);
		const int Gold = Player.Get(ECurrency::Gold);
		const int Tickets = Player.Get(ECurrency::RegularTicket);
		const std::vector<std::string> First = Player.ApplyStageRewards(1, false);
		Expect(Player.Get(ECurrency::Diamonds) == Diamonds + 50 && Player.Get(ECurrency::RegularTicket) == Tickets && Player.Get(ECurrency::Gold) == Gold + 800
			&& Player.Get(ECurrency::Bytes) == 250 && Player.Get(ECurrency::AngelHearts) == 2, "first clear: 50 Diamonds, 800 Gold, 250 Bytes, 2 hearts (tickets come from quests)");
		Expect(Player.StagesCleared == 1, "the first clear is counted");
		const std::vector<std::string> Replay = Player.ApplyStageRewards(1, false);
		Expect(Player.Get(ECurrency::Diamonds) == Diamonds + 50, "replay: no Diamonds");
		Expect(Player.Get(ECurrency::Gold) == Gold + 1600 && Player.Get(ECurrency::Bytes) == 500 && Player.Get(ECurrency::AngelHearts) == 4, "replay: Gold, Bytes and hearts again (that is the grind)");
		bool bNote = false;
		for (const std::string& Line : Replay)
		{
			bNote = bNote || Contains(Line, "first clear only");
		}
		Expect(bNote && Player.StagesCleared == 1, "a replay says why there are no Diamonds");
		(void)First;

		// chapter bosses: Plack only from the Chapter 1 boss; the others pay a Special ticket and give no operator
		FProfile Late(102);
		Late.StartNewPlayer();
		Late.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		const int Plack = FindOperator("Plack");
		const int Special = Late.Get(ECurrency::SpecialTicket);
		const std::vector<std::string> Boss2 = Late.ApplyStageRewards(16, true);
		Expect(!Late.Owns(Plack) && Late.Get(ECurrency::SpecialTicket) == Special && Late.StagesCleared == 16, "the Chapter 2 boss gives no operator (its ticket is the chapter quest)");
		bool bPlackLine = false;
		for (const std::string& Line : Boss2)
		{
			bPlackLine = bPlackLine || Contains(Line, "Plack");
		}
		Expect(!bPlackLine, "...and no Plack line");
		Late.ApplyStageRewards(8, true);
		Expect(Late.Owns(Plack) && Late.StagesCleared == 16, "the Chapter 1 boss still gives Plack, and the cleared count never goes down");
		Late.ApplyStageRewards(500, false);
		Expect(Late.StagesCleared == TotalCampaignLevels, "the cleared count is capped at the last stage");
	}

	// ---------------------------------------------------------------------- unlocking
	void TestUnlocking()
	{
		FProfile Player(103);
		Player.StartNewPlayer();
		Player.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		Player.GrantAllOperators();
		Player.Team[0] = FindOperator("Luna");
		Player.Team[1] = FindOperator("Plack");
		Player.Team[2] = FindOperator("Kuu");
		Expect(CanStartStage(Player, CampaignStageId(1, 1)).bOk, "level 1 is open");
		Expect(!CanStartStage(Player, CampaignStageId(2, 1)).bOk && Contains(CanStartStage(Player, CampaignStageId(2, 1)).Message, "Chapter 1 - Level 8"), "Chapter 2 is locked behind the Chapter 1 boss (and says which stage)");
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			if (IsWalkStage(Stage) && Stage != WalkEntryStage())
			{
				Expect(Player.StagesCleared >= Stage && !CanStartStage(Player, Stage).bOk, "stage " + std::to_string(Stage) + " was cleared by the walk and cannot be started on its own");
				continue;
			}
			if (Stage == WalkEntryStage())
			{
				Expect(CanStartStage(Player, Stage).bOk && !CanStartStage(Player, Stage + 1).bOk, "the Chapter 11 walk opens after 10-8");
				const std::vector<std::string> Walk = ClearStage(Player, Stage);
				Expect(Player.StagesCleared == WalkLastStage() && Walk.size() == 1 && Contains(Walk[0], "Chapter 12 unlocked"), "walking out clears Chapter 11 and opens Chapter 12, nothing else");
				Expect(!QuickClear(Player, Stage, 1).bOk && CanStartStage(Player, Stage).bOk, "nothing to quick clear; the walk can be walked again");
				const int Gold = Player.Get(ECurrency::Gold);
				Expect(ClearStage(Player, Stage).empty() && Player.Get(ECurrency::Gold) == Gold, "walking it again does nothing");
				continue;
			}
			Expect(CanStartStage(Player, Stage).bOk, "stage " + std::to_string(Stage) + " opens after the previous clear");
			if (Stage < TotalCampaignLevels)
			{
				Expect(!CanStartStage(Player, Stage + 1).bOk, "stage " + std::to_string(Stage + 1) + " stays locked before stage " + std::to_string(Stage) + " is cleared");
			}
			const std::vector<std::string> Lines = ClearStage(Player, Stage);
			Expect(Player.StagesCleared == Stage, "cleared count after stage " + std::to_string(Stage));
			bool bUnlockLine = false;
			for (const std::string& Line : Lines)
			{
				bUnlockLine = bUnlockLine || Contains(Line, "unlocked") || Contains(Line, "campaign is complete");
			}
			Expect(bUnlockLine, "clearing stage " + std::to_string(Stage) + " announces what opens");
			if (Stage % CampaignLevels == 0 && Stage < TotalCampaignLevels)
			{
				bool bChapterLine = false;
				for (const std::string& Line : Lines)
				{
					bChapterLine = bChapterLine || Contains(Line, "Chapter " + std::to_string(Stage / CampaignLevels + 1) + " unlocked");
				}
				Expect(bChapterLine, "a chapter boss announces the next chapter");
			}
		}
		// Classic gacha rebalance (2026-10-07): replays cost Charge, first clears are free.
		Player.SetRaw(ECurrency::Charge, 0);
		Expect(!CanStartStage(Player, CampaignStageId(5, 3)).bOk == Config::UseCharge, "a replay needs Charge");
		Player.SetRaw(ECurrency::Charge, 240);
		Expect(CanStartStage(Player, CampaignStageId(5, 3)).bOk, "cleared levels can be replayed");
		ClearStage(Player, CampaignStageId(5, 3));
		Expect(Player.Get(ECurrency::Charge) == 240 - (Config::UseCharge ? 15 : 0), "a chapter 5 replay costs 15 Charge");

		// admin: every level, and Diamonds/tickets are unlimited anyway
		FProfile Admin(104);
		FAccountInfo Info;
		Info.UserId = "10000001";
		Info.Username = "Owner";
		Info.Email = Config::AdminEmail;
		Info.bSignedIn = true;
		Info.bAdmin = true;
		Admin.StartNewPlayer();
		Admin.ApplyAccount(Info);
		Expect(Admin.StagesCleared == TotalCampaignLevels, "the admin account has cleared all 104 stages");
		Expect(CanStartStage(Admin, CampaignStageId(13, 8)).bOk && CanStartStage(Admin, CampaignStageId(7, 4)).bOk, "the admin can enter any stage");
		FProfile Guest(105);
		Guest.StartNewPlayer();
		Guest.Account.bGuest = true;
		Guest.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		Guest.GrantAllOperators();
		Guest.Team[0] = FindOperator("Luna");
		Guest.Team[1] = FindOperator("Plack");
		Guest.Team[2] = FindOperator("Kuu");
		Expect(!CanStartStage(Guest, CampaignStageId(13, 8)).bOk && !CanStartStage(Guest, CampaignStageId(2, 1)).bOk, "a guest has to play through in order");
	}

	// ------------------------------------------------------------------------- hub pages
	bool HasTile(const P::FPage& Page, const std::string& Id)
	{
		for (const P::FPageAction& Action : Page.Grid)
		{
			if (Action.Id == Id)
			{
				return true;
			}
		}
		return false;
	}

	const P::FPageAction* FindTile(const P::FPage& Page, const std::string& Id)
	{
		for (const P::FPageAction& Action : Page.Grid)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		return nullptr;
	}

	void TestPages()
	{
		FProfile Player(106);
		Player.StartNewPlayer();
		Player.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		Player.GrantAllOperators();
		Player.Team[0] = FindOperator("Luna");
		Player.Team[1] = FindOperator("Plack");
		Player.Team[2] = FindOperator("Kuu");
		P::FUiState State;
		State.Tab = P::TabCampaign;
		P::FPage Root = P::Build(Player, State);
		int ChapterTiles = 0;
		for (int Chapter = 1; Chapter <= CampaignChapters; ++Chapter)
		{
			ChapterTiles += HasTile(Root, "camp:ch:" + std::to_string(Chapter)) ? 1 : 0;
		}
		Expect(ChapterTiles == 13, "the Campaign page lists all 13 chapters");
		const P::FPageAction* Locked = FindTile(Root, "camp:ch:2");
		Expect(Locked && Locked->Icon == "lock" && Contains(Locked->Sub, "locked"), "Chapter 2 shows as locked");
		const P::FPageAction* Open = FindTile(Root, "camp:ch:1");
		Expect(Open && Open->Icon == "book" && Contains(Open->Sub, "0/8"), "Chapter 1 shows 0/8");

		const P::FPageAction* NeoTokyo = FindTile(Root, "camp:map:NeoTokyo");
		Expect(NeoTokyo && NeoTokyo->bEnabled && NeoTokyo->Label == "Neo Tokyo", "the Campaign page lists Neo Tokyo (free roam) under the levels");
		{
			P::FUiState RoamState = State;
			const P::FPerform Roam = P::Perform(Player, RoamState, "camp:map:NeoTokyo");
			Expect(Roam.FreeRoamMap == "NeoTokyo" && !Roam.bStartStage, "tapping Neo Tokyo asks the engine to open the map");
			const P::FPerform Unknown = P::Perform(Player, RoamState, "camp:map:Nowhere");
			Expect(Unknown.FreeRoamMap.empty(), "an unknown map does nothing");
		}

		P::FPerform Tap = P::Perform(Player, State, "camp:ch:2");
		Expect(!Tap.bStartStage && Contains(State.Status, "locked") && State.Page.empty(), "tapping a locked chapter explains and stays on the list");
		Tap = P::Perform(Player, State, "camp:ch:99");
		Expect(!Tap.bStartStage && State.Page.empty(), "an unknown chapter does nothing");
		Tap = P::Perform(Player, State, "camp:ch:1");
		Expect(State.Page == "chapter" && State.SelChapter == 1, "Chapter 1 opens");
		P::FPage Chapter1 = P::Build(Player, State);
		Expect(Chapter1.Title == "Chapter 1" && HasTile(Chapter1, "camp:start:1") && HasTile(Chapter1, "camp:start:8") && !HasTile(Chapter1, "camp:start:9"), "Chapter 1 page lists stages 1-8");
		const P::FPageAction* Level2 = FindTile(Chapter1, "camp:start:2");
		Expect(Level2 && !Level2->bEnabled && Contains(Level2->Sub, "locked"), "level 2 is locked at the start");
		Tap = P::Perform(Player, State, "camp:start:2");
		Expect(!Tap.bStartStage && Contains(State.Status, "Chapter 1 - Level 1"), "tapping a locked level names the level to clear first");
		Tap = P::Perform(Player, State, "camp:start:1");
		Expect(Tap.bStartStage && Tap.StageId == 1, "level 1 starts");
		Tap = P::Perform(Player, State, "camp:start:1001");
		Expect(!Tap.bStartStage, "a tower stage id is not a campaign level");
		Tap = P::Perform(Player, State, "camp:start:0");
		Expect(!Tap.bStartStage, "stage 0 does nothing");

		Player.StagesCleared = 8;
		State.Page.clear();
		Root = P::Build(Player, State);
		Open = FindTile(Root, "camp:ch:1");
		Expect(Open && Contains(Open->Sub, "8/8"), "Chapter 1 shows 8/8 once its boss is down");
		Locked = FindTile(Root, "camp:ch:2");
		Expect(Locked && Locked->Icon == "book" && Contains(Locked->Sub, "0/8") && Contains(Locked->Sub, "Ranged"), "Chapter 2 opens with its theme");
		Tap = P::Perform(Player, State, "camp:ch:2");
		Expect(State.Page == "chapter" && State.SelChapter == 2, "Chapter 2 opens");
		P::FPage Chapter2 = P::Build(Player, State);
		Expect(Chapter2.Title == "Chapter 2" && Chapter2.bCanBack, "Chapter 2 page");
		int Starts = 0;
		for (int Stage = 9; Stage <= 16; ++Stage)
		{
			Starts += HasTile(Chapter2, "camp:start:" + std::to_string(Stage)) ? 1 : 0;
		}
		Expect(Starts == 8 && !HasTile(Chapter2, "camp:start:8") && !HasTile(Chapter2, "camp:start:17"), "Chapter 2 page lists stages 9-16");
		const P::FPageAction* First = FindTile(Chapter2, "camp:start:9");
		Expect(First && First->bEnabled && Contains(First->Sub, "Lv " + std::to_string(RecommendedLevel(9))) && Contains(First->Sub, "sections"), "the first Chapter 2 level is open and shows its recommended level");
		const P::FPageAction* Boss = FindTile(Chapter2, "camp:start:16");
		Expect(Boss && !Boss->bEnabled && Contains(Boss->Sub, "BOSS") && Contains(Boss->Label, "BOSS"), "the Chapter 2 boss level is marked");
		Tap = P::Perform(Player, State, "camp:start:9");
		Expect(Tap.bStartStage && Tap.StageId == 9 && Contains(Tap.Message, "Chapter 2 - Level 1"), "Chapter 2 level 1 starts");
		Tap = P::Perform(Player, State, "camp:start:10");
		Expect(!Tap.bStartStage, "Chapter 2 level 2 is locked");

		// the profile page counts the whole campaign
		State.Tab = P::TabLobby;
		State.Page = "profile";
		const P::FPage Profile = P::Build(Player, State);
		bool bLine = false;
		for (const std::string& Line : Profile.Lines)
		{
			bLine = bLine || (Contains(Line, "Campaign: 8 / 104") && Contains(Line, "Chapter 2 - Level 1"));
		}
		Expect(bLine, "the profile says 8 / 104 and what comes next");

		// admin sees every chapter open
		FProfile Admin(107);
		FAccountInfo Info;
		Info.UserId = "10000001";
		Info.Username = "Owner";
		Info.Email = Config::AdminEmail;
		Info.bSignedIn = true;
		Info.bAdmin = true;
		Admin.StartNewPlayer();
		Admin.ApplyAccount(Info);
		P::FUiState AdminState;
		AdminState.Tab = P::TabCampaign;
		const P::FPage AdminRoot = P::Build(Admin, AdminState);
		int OpenChapters = 0;
		for (const P::FPageAction& Action : AdminRoot.Grid)
		{
			OpenChapters += Action.Icon == "book" ? 1 : 0;
		}
		Expect(OpenChapters == 13, "the admin sees all 13 chapters open");
		P::Perform(Admin, AdminState, "camp:ch:13");
		const P::FPage AdminChapter = P::Build(Admin, AdminState);
		bool bAllEnabled = true;
		for (int Stage = 97; Stage <= 104; ++Stage)
		{
			const P::FPageAction* Tile = FindTile(AdminChapter, "camp:start:" + std::to_string(Stage));
			bAllEnabled = bAllEnabled && Tile && Tile->bEnabled;
		}
		Expect(bAllEnabled, "the admin can start every Chapter 13 level");
	}

	// ----------------------------------------------------------------------- saving
	void TestSaves()
	{
		FProfile Player(108);
		Player.StartNewPlayer();
		Player.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		Player.StagesCleared = 77;
		const std::string Json = Save::Serialize(Player, 1000);
		FProfile Loaded(109);
		std::string Error;
		Expect(Save::Deserialize(Json, Loaded, Error) && Loaded.StagesCleared == 77, "a mid-campaign save round-trips (stage 77): " + Error);
		// an old single-chapter save still loads
		FProfile Old(110);
		Old.StartNewPlayer();
		Old.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		Old.StagesCleared = 8;
		FProfile OldLoaded(111);
		Expect(Save::Deserialize(Save::Serialize(Old, 1000), OldLoaded, Error) && OldLoaded.StagesCleared == 8, "a Chapter 1 save keeps its progress");
		// a hacked value is clamped
		std::string Hacked = Json;
		const std::string Needle = "\"stagesCleared\":77";
		const size_t At = Hacked.find(Needle);
		Expect(At != std::string::npos, "the save names stagesCleared");
		if (At != std::string::npos)
		{
			Hacked.replace(At, Needle.size(), "\"stagesCleared\":9999");
			FProfile Clamped(112);
			Expect(Save::Deserialize(Hacked, Clamped, Error) && Clamped.StagesCleared == TotalCampaignLevels, "a save claiming more than 104 stages is clamped");
		}
	}
}

int main()
{
	TestStageIds();
	TestPlans();
	TestArchetypes();
	TestScales();
	TestRewards();
	TestUnlocking();
	TestPages();
	TestSaves();
	TestBalanceModel();
	std::printf("%d campaign checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
