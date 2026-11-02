// Campaign balance model — used by the generator (Tests/dump_campaign.cpp) and the tests (Tests/campaign_tests.cpp).
// It is NOT shipped with the game: the game only reads the tables the model produced (BIBRules/BIBCampaignData.inc).
//
// The question the owner asked (2026-10-05): balance each stage to the currency, levels and operators a player has at that
// point of the game, a bit harder so that some grinding is needed. The model answers it with a reference player:
//
//   1. ECONOMY. A reference player clears the stages in order, levels the team of three operators evenly with everything
//      the stages pay (BIB::Campaign::StageReward, the real level-up and Angel-heart costs), and replays the newest stage
//      whenever the next one asks for more levels than they have. "Asks for" is the design knob: a stage requires the team
//      level the player would have after ReplaysNormal extra clears (ReplaysChapterEnd before a boss), i.e. every stage
//      is "a bit harder than the pace" and needs about one replay of grinding, a boss two or three.
//   2. OPERATORS. Pulls (Diamonds / 300 plus the story-quest tickets and Diamonds) bring duplicates, which give upgrade stars (5% damage
//      and defence each, +20 level cap); equipment drops add a few percent; owning more operators lets the player pick a
//      better team (a small bonus). All of it is read off the real rules and PLACEHOLDER rates in BIBConfig.h.
//   3. SCALING. Enemy health follows the damage the team deals at the required level and enemy damage follows the health it
//      has there, relative to stage 1 (base enemy numbers against a fresh team), times a hardness ramp over Chapter 1 that
//      reproduces how much tougher the old Chapter 1 boss was than its player. Since the owner's rebalance (2026-10-06:
//      a much steeper per-level growth, Config::StatPercentPerLevel) Chapter 1 is solved the same way as the rest.
//   4. COMPOSITION. A fluid fight model (enemies killed in a danger order, damage taken meanwhile, no healing between the
//      sections of a level) gives each level a threat index relative to Chapter 1 level 7 (= 1.0). The section sizes of the
//      generated levels are solved so that a chapter ramps from easy (about 0.6) to hard (about 1.1) and ends in a boss gate.
//
// Every constant here is a PLACEHOLDER assumption about play (uptime, dodging, pack damage), documented, not measured. The
// absolute difficulty is only as good as these; the relative shape (monotone scaling, grind per stage, ramps) is what the
// tests guard. The owner changes a constant, reruns Tools/gen_campaign_data.py, and the tables follow.
#pragma once

#include "../Source/BoldInBeautyDemo/BIBRules/BIBCampaign.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMeta.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace CampaignModel
{
	using namespace BIB;
	using namespace BIB::Campaign;

	// ------------------------------------------------------------------ reference player (PLACEHOLDER assumptions)
	constexpr double SpendEfficiency = 0.85;     // share of the currency that reaches the three team operators
	// Classic gacha rebalance (owner request 2026-10-07, PLACEHOLDER): progress is gated by Charge (stamina) like ZZZ, so a
	// stage asks for about five quick clears (Meta::QuickClear, "sweeps") of the newest level on top of the pace and a chapter
	// boss for about twelve. Was 1.0 / 2.5 (2026-10-05). Tests/playthrough.cpp: every daily play style needs about 3-4 weeks.
	constexpr double ReplaysNormal = 5.0;
	constexpr double ReplaysChapterEnd = 12.0;
	constexpr double ReplayRampStart = 0.3;      // chapter 2 eases in from this share of ReplaysNormal
	constexpr double UpgradeUnitsPerPull = Config::Rate4 / 3.0 * Config::DuplicateResource; // a pull duplicates a given 4-star at ~6.6%
	// Balancing pass 2026-10-09 (PLACEHOLDER): the team's upgrade stars are planned for an unlucky player, not the average one.
	// Luna, Plack and Kuu are the only 4-stars, so their duplicates (the only way past level cap 100 / 120 / 140) are pure luck;
	// planning for the mean walled about one player in five at the cap for weeks (Tests/playthrough.cpp: p90 34-46 days).
	// The pulls also count the daily / weekly free income (login, quests, season, events), which the bot measured at about 0.35
	// pulls per stage for a regular player; without it the model saw only 71 pulls and capped the last 11 stages at level 140.
	constexpr double StarLuckPercentile = 0.20;  // copies of a given 4-star that 80% of players have at this many pulls
	constexpr double FreePullsPerStage = 0.35;
	constexpr double EquipmentBasePercent = 3.0;
	constexpr double EquipmentPercentPerLevel = 0.17;
	constexpr double EquipmentMaxPercent = 20.0; // damage and defence each, best items plus upgrades
	constexpr double RosterBonusMax = 0.10;      // a bigger roster lets the player pick a better team
	constexpr double RosterBonusPulls = 120.0;
	constexpr double HardnessRamp = 0.5;         // Chapter 1 ramps from 1.0x to 1.5x the pace; later chapters keep 1.5x (the old boss gap)

	// ------------------------------------------------------------------ reference team and fight (PLACEHOLDER assumptions)
	// Luna, Plack and Kuu: the operators every player has by Chapter 1. Their dps with the ultimate comes from the kit balance
	// table (Tests/run_core_tests.sh --balance): Luna ~6400, Plack ~3700, Kuu ~1800 (mean ~3965) since the 2026-10-07 moveset
	// pass raised the 4-stars to their rarity band (was Plack ~1900, Kuu ~800).
	constexpr double TeamDps = (6402.0 + 3695.0 + 1799.0) / 3.0;
	constexpr double UptimePack = 0.60;          // share of the time the operator is actually hitting a crowd
	constexpr double UptimeSingle = 0.80;        // ... a single big target
	constexpr double PackRatio = 3.0;            // damage against a crowd (area moves) versus a single target
	constexpr double PackSaturation = 9.0;       // enemies needed for the full crowd bonus minus one
	constexpr int MeleeSlots = 4;                // melee enemies that can reach the operator at once
	constexpr double FarInRange = 0.70;          // share of the time ranged and mage enemies are in range

	// Target threat of a normal level (Chapter 1 level 7 = 1.0): chapter factor times level shape; a chapter's last level
	// is a gate at FinalThreat times level 7's target (Chapter 1's own boss level is 2.7 times its level 7).
	inline double ChapterThreat(int Chapter) { return 0.80 + 0.03 * (Chapter - 2); }
	inline double LevelShape(int Level)
	{
		static const double Shape[CampaignLevels - 1] = {0.70, 0.80, 0.85, 0.90, 1.00, 1.05, 1.10};
		return Shape[std::clamp(Level, 1, CampaignLevels - 1) - 1];
	}
	constexpr double FinalThreat = 2.2;
	constexpr double OverTargetTrim = 1.05;    // a level more than 5% over its threat target gets its enemy damage trimmed (2026-10-09)
	constexpr double MinChapterFactor = 0.5;   // a chapter's stat multipliers are lowered at most this far to make room for its mix

	// ------------------------------------------------------------------ economy
	struct FPaceRow
	{
		double LevelAtPace = 1.0;     // average team level when the player gets to the stage (grinding of earlier stages included)
		double LevelRequired = 1.0;   // the level the stage asks for
		int Replays = 0;              // extra clears the reference player needs to get there
		int Stars = 0;                // upgrade stars on the team at that point
		double Pulls = 0.0;           // summons earned by first clears up to and including the stage
	};

	inline bool IsChapterEnd(int Global) { return Global % CampaignLevels == 0; }

	// Balancing pass 2026-10-09 (PLACEHOLDER): Chapter 1 is onboarding. It used to ask for the full five replays from level 2 on
	// (32 in the chapter, more than Chapter 2's 26) and a new player finished it on day 3; it now eases in like Chapter 2,
	// from almost no grinding at level 2 up to where Chapter 2 starts, and its boss asks for half a normal boss's grind.
	constexpr double ChapterOneBossShare = 0.5;

	inline double ReplayTarget(int Global)
	{
		if (IsWalkStage(Global))
		{
			return 0.0;   // Return to Nowhere (Chapter 11): a walk, nothing to grind for (2026-10-09)
		}
		if (Global > 1 && Global <= CampaignLevels)   // stage 1 (the tutorial) stays the anchor every enemy scale is relative to
		{
			if (IsChapterEnd(Global))
			{
				return ReplaysChapterEnd * ChapterOneBossShare;
			}
			const double T = static_cast<double>(Global - 1) / CampaignLevels;
			return ReplaysNormal * ReplayRampStart * T;
		}
		if (IsChapterEnd(Global))
		{
			return ReplaysChapterEnd;
		}
		if (Global > CampaignLevels && Global <= CampaignLevels * 2)
		{
			// Chapter 2 eases the player in.
			const double T = static_cast<double>(Global - CampaignLevels) / CampaignLevels;
			return ReplaysNormal * (ReplayRampStart + (1.0 - ReplayRampStart) * T);
		}
		return ReplaysNormal;
	}

	// What the story quests completed by this stage's first clear pay (BIB::Meta::StoryQuestDef).
	inline FReward StoryLootAt(int Global)
	{
		FReward Loot;
		for (int Index = 0; Index < StoryQuestCount; ++Index)
		{
			const Meta::FStoryQuestDef Def = Meta::StoryQuestDef(Index);
			if (Def.Stage == Global)
			{
				Loot.Gold += Def.Reward.Gold;
				Loot.Bytes += Def.Reward.Bytes;
				Loot.Hearts += Def.Reward.Hearts;
				Loot.Diamonds += Def.Reward.Diamonds;
				Loot.RegularTickets += Def.Reward.RegularTickets;
			}
		}
		return Loot;
	}

	inline double PullsFromStage(int Global)
	{
		const FStageReward Reward = StageReward(Global);
		const FReward Loot = StoryLootAt(Global);
		return (Reward.Diamonds + Loot.Diamonds) / static_cast<double>(Config::DiamondsPerRoll) + Loot.RegularTickets;
	}

	inline double PullsUpTo(int Global)
	{
		double Pulls = FreePullsPerStage * std::max(0, Global);
		for (int Stage = 1; Stage <= Global; ++Stage)
		{
			Pulls += PullsFromStage(Stage);
		}
		return Pulls;
	}

	// Copies of one given 4-star that a share StarLuckPercentile of players fall short of (Poisson quantile of the pulls).
	inline int CopiesAtPercentile(double Pulls)
	{
		const double Lambda = Pulls * Config::Rate4 / 3.0;
		double Term = std::exp(-Lambda);
		double Sum = Term;
		int Copies = 0;
		while (Sum < StarLuckPercentile && Copies < 1000)
		{
			++Copies;
			Term *= Lambda / Copies;
			Sum += Term;
		}
		return Copies;
	}

	inline int StarsForPulls(double Pulls)
	{
		const double Units = static_cast<double>(CopiesAtPercentile(Pulls) * Config::DuplicateResource);
		int Stars = 0;
		double Cost = 0.0;
		while (true)
		{
			Cost += FProfile::UpgradeStarCost(Stars + 1);
			if (Cost > Units)
			{
				return Stars;
			}
			++Stars;
		}
	}

	inline double EquipmentPercent(int Global)
	{
		return std::min(EquipmentMaxPercent, EquipmentBasePercent + EquipmentPercentPerLevel * Global);
	}

	// Three operators levelled evenly; the same costs and gates as the real rules.
	struct FTeam
	{
		int Level[3] = {1, 1, 1};
		int Gates[3] = {0, 0, 0};
		double Gold = 0.0, Bytes = 0.0, Hearts = 0.0;
		int Stars = 0;
		int Cap() const { return Config::BaseLevelCap + Config::CapPerUpgradeStar * Stars; }
		double Average() const { return (Level[0] + Level[1] + Level[2]) / 3.0; }
		void Pay(const FStageReward& Reward, double Share)
		{
			Gold += Reward.Gold * Share;
			Bytes += Reward.Bytes * Share;
			Hearts += Reward.Hearts * Share;
		}
		void Spend()
		{
			while (true)
			{
				int Lowest = 0;
				for (int Index = 1; Index < 3; ++Index)
				{
					if (Level[Index] < Level[Lowest])
					{
						Lowest = Index;
					}
				}
				const int L = Level[Lowest];
				if (L >= Cap())
				{
					return;
				}
				if (L % Config::HeartGateEvery == 0 && Gates[Lowest] < L / Config::HeartGateEvery)
				{
					const int GateCost = FProfile::HeartGateCost(L / Config::HeartGateEvery);
					if (Hearts >= GateCost)
					{
						Hearts -= GateCost;
						++Gates[Lowest];
						continue;
					}
					return;
				}
				const FCost Cost = FProfile::LevelUpCost(L);
				const double GoldCost = Cost.Gold / SpendEfficiency;
				const double BytesCost = Cost.Bytes / SpendEfficiency;
				if (Gold >= GoldCost && Bytes >= BytesCost)
				{
					Gold -= GoldCost;
					Bytes -= BytesCost;
					++Level[Lowest];
					continue;
				}
				return;
			}
		}
	};

	inline std::vector<FPaceRow> SimulatePace()
	{
		std::vector<FPaceRow> Rows(static_cast<size_t>(TotalCampaignLevels) + 1);
		FTeam Team;
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			Team.Stars = StarsForPulls(PullsUpTo(Stage - 1));
			Team.Spend();
			FPaceRow& Row = Rows[static_cast<size_t>(Stage)];
			Row.LevelAtPace = Team.Average();
			// What the stage asks for: the level after ReplayTarget extra clears of the newest stage the player has.
			FTeam Probe = Team;
			const FStageReward Farm = StageReward(std::max(1, Stage - 1));
			const double Replays = ReplayTarget(Stage);
			Probe.Pay(Farm, Replays);
			Probe.Stars = StarsForPulls(PullsUpTo(Stage));
			Probe.Spend();
			Row.LevelRequired = Probe.Average();
			int Count = 0;
			// Stage 1 (the tutorial) only sets the anchor level; a new player does not farm it (2026-10-09).
			while (Stage > 1 && !IsWalkStage(Stage) && Team.Average() + 1e-9 < Row.LevelRequired && Count < 80)
			{
				Team.Pay(Farm, 1.0);
				Team.Spend();
				++Count;
			}
			Row.Replays = Count;
			Row.Stars = Team.Stars;
			Row.Pulls = PullsUpTo(Stage);
			Team.Pay(StageReward(Stage), 1.0);
			const FReward Loot = StoryLootAt(Stage);
			Team.Gold += Loot.Gold;
			Team.Bytes += Loot.Bytes;
			Team.Hearts += Loot.Hearts;
			Team.Stars = StarsForPulls(PullsUpTo(Stage));
			Team.Spend();
		}
		return Rows;
	}

	// ------------------------------------------------------------------ player power at a team level
	struct FPower
	{
		double Health = 1.0;   // effective health: level multiplier over the damage left after defence
		double Damage = 1.0;   // damage dealt: level, stars, equipment and the roster bonus
	};

	inline FPower PowerAt(double TeamLevel, int Stage, double Pulls)
	{
		const int Stars = StarsForPulls(Pulls);
		const double LevelMult = 1.0 + (TeamLevel - 1.0) * Config::StatPercentPerLevel / 100.0;
		const double Equipment = EquipmentPercent(Stage) / 100.0;
		const double Defence = std::min(Config::DefenceCapPercent / 100.0, Stars * Config::UpgradeStarDefencePercent / 100.0 + Equipment);
		const double Roster = 1.0 + RosterBonusMax * (1.0 - std::exp(-Pulls / RosterBonusPulls));
		FPower Power;
		Power.Health = LevelMult / (1.0 - Defence);
		Power.Damage = LevelMult * (1.0 + Stars * Config::UpgradeStarDamagePercent / 100.0 + Equipment) * Roster;
		return Power;
	}

	// ------------------------------------------------------------------ scales, recommended levels
	struct FStageTables
	{
		std::vector<FPaceRow> Pace;
		std::vector<FEnemyScale> Scale;   // index = stage id, chapter factors applied
		std::vector<int> Recommended;
		std::vector<double> ChapterFactor;   // index = chapter; 1.0 unless the chapter's enemy mix is more dangerous than the pace allows
	};

	inline double Hardness(int Stage)
	{
		return 1.0 + HardnessRamp * std::min(1.0, (Stage - 1) / static_cast<double>(CampaignLevels - 1));
	}

	inline FStageTables BuildStageTables()
	{
		FStageTables Tables;
		Tables.Pace = SimulatePace();
		Tables.Scale.assign(static_cast<size_t>(TotalCampaignLevels) + 1, FEnemyScale());
		Tables.Recommended.assign(static_cast<size_t>(TotalCampaignLevels) + 1, 1);
		Tables.ChapterFactor.assign(static_cast<size_t>(CampaignChapters) + 1, 1.0);
		// Stage 1 is the anchor: the base enemy numbers against the team a new player brings to it.
		const FPaceRow& AnchorRow = Tables.Pace[1];
		const FPower AnchorPower = PowerAt(AnchorRow.LevelRequired, 1, AnchorRow.Pulls);
		for (int Stage = 1; Stage <= TotalCampaignLevels; ++Stage)
		{
			const FPaceRow& Row = Tables.Pace[static_cast<size_t>(Stage)];
			const FPower Power = PowerAt(Row.LevelRequired, Stage, Row.Pulls);
			FEnemyScale Scale;
			// never below base: the tutorial's anchor team is a little stronger than a new player at level 2 (2026-10-09)
			Scale.Hp = std::max(1.0, Hardness(Stage) * Power.Damage / AnchorPower.Damage);
			Scale.Damage = std::max(1.0, Hardness(Stage) * Power.Health / AnchorPower.Health);
			Tables.Scale[static_cast<size_t>(Stage)] = Scale;
			Tables.Recommended[static_cast<size_t>(Stage)] = std::max(1, static_cast<int>(std::lround(Row.LevelRequired)));
		}
		Tables.Recommended[1] = 1;   // the tutorial level: a new account plays it with whatever it has
		return Tables;
	}

	// ------------------------------------------------------------------ fluid fight model
	inline double TeamHealth()
	{
		const char* Names[3] = {"Luna", "Plack", "Kuu"};
		double Sum = 0.0;
		for (const char* Name : Names)
		{
			const int Index = FindOperator(Name);
			Sum += Index >= 0 ? Roster()[static_cast<size_t>(Index)].BaseHP : 0.0;
		}
		return Sum;
	}

	// Damage the team takes (before its own defence) clearing one section at base enemy stats and the base operator level.
	inline double SectionDamage(const FSectionPlanData& Section)
	{
		// Kill order: the dangerous squishies first, the tanks and bosses last.
		static const EArchetype Order[ArchetypeCount] = {EArchetype::Assassin, EArchetype::Mage, EArchetype::Ranged, EArchetype::Melee,
			EArchetype::Brute, EArchetype::MiniBoss, EArchetype::Boss};
		int Alive[ArchetypeCount];
		for (int Index = 0; Index < ArchetypeCount; ++Index)
		{
			Alive[Index] = CountOf(Section, static_cast<EArchetype>(Index));
		}
		double Damage = 0.0;
		for (const EArchetype Kind : Order)
		{
			const FArchetype& Target = Archetype(Kind);
			for (int Unit = 0; Unit < Alive[static_cast<int>(Kind)]; ++Unit)
			{
				int Total = 0;
				for (int Index = 0; Index < ArchetypeCount; ++Index)
				{
					Total += Alive[Index];
				}
				double Dps;
				if (Target.Group == EThreatGroup::Boss && Total == 1)
				{
					Dps = TeamDps * UptimeSingle * Target.Reach;
				}
				else
				{
					const double Pack = 1.0 + (PackRatio - 1.0) * std::min(1.0, std::max(0, Total - 1) / PackSaturation);
					Dps = TeamDps * UptimePack * Pack * Target.Reach;
				}
				const double TimeToKill = Target.Hp / Dps;
				const int MeleeAlive = Alive[static_cast<int>(EArchetype::Melee)] + Alive[static_cast<int>(EArchetype::Brute)];
				double Rate = 0.0;
				for (int Index = 0; Index < ArchetypeCount; ++Index)
				{
					const int Count = Alive[Index];
					if (Count == 0)
					{
						continue;
					}
					const FArchetype& Other = Archetype(static_cast<EArchetype>(Index));
					double PerEnemy = Other.Damage / Other.Interval;
					if (Other.SlamInterval > 0.0)
					{
						PerEnemy += Other.SlamDamage / Other.SlamInterval;
					}
					PerEnemy *= 1.0 - Other.Avoid;
					switch (Other.Group)
					{
					case EThreatGroup::Melee: Rate += PerEnemy * std::min(static_cast<double>(Count), MeleeSlots * static_cast<double>(Count) / std::max(1, MeleeAlive)); break;
					case EThreatGroup::Far: Rate += PerEnemy * Count * FarInRange; break;
					case EThreatGroup::Rush:
					case EThreatGroup::Boss: Rate += PerEnemy * Count; break;
					}
				}
				Damage += Rate * TimeToKill;
				--Alive[static_cast<int>(Kind)];
			}
		}
		return Damage;
	}

	inline double LevelDamage(const std::vector<FSectionPlanData>& Plan)
	{
		double Damage = 0.0;
		for (const FSectionPlanData& Section : Plan)
		{
			Damage += SectionDamage(Section);
		}
		return Damage;
	}

	// ------------------------------------------------------------------ threat and section sizes
	struct FThreatContext
	{
		const FStageTables* Tables = nullptr;
		double Reference = 1.0;

		// Damage taken (at base enemy stats) times the stage's two scales over the player power at the required level: the
		// inverse of the survival margin. Chapter 1 level 7 is 1.0.
		double Norm(int Stage) const
		{
			const FPaceRow& Row = Tables->Pace[static_cast<size_t>(Stage)];
			const FPower Power = PowerAt(Row.LevelRequired, Stage, Row.Pulls);
			const FEnemyScale& Scale = Tables->Scale[static_cast<size_t>(Stage)];
			return Scale.Hp * Scale.Damage / (Power.Health * Power.Damage);
		}
		double Threat(int Stage, const std::vector<FSectionPlanData>& Plan) const { return LevelDamage(Plan) * Norm(Stage) / Reference; }
	};

	inline FThreatContext MakeThreatContext(const FStageTables& Tables)
	{
		FThreatContext Context;
		Context.Tables = &Tables;
		Context.Reference = 1.0;
		const int Anchor = CampaignStageId(7);
		Context.Reference = LevelDamage(StagePlan(Anchor)) * Context.Norm(Anchor);
		return Context;
	}

	inline double TargetThreat(int Chapter, int Level)
	{
		return ChapterThreat(Chapter) * (Level < CampaignLevels ? LevelShape(Level) : FinalThreat * LevelShape(CampaignLevels - 1));
	}

	struct FSizeChoice
	{
		int Size = MinSectionSize;
		double Threat = 0.0;
		double Target = 0.0;
	};

	inline FSizeChoice SolveSectionSize(const FThreatContext& Context, int Chapter, int Level)
	{
		FSizeChoice Best;
		Best.Target = TargetThreat(Chapter, Level);
		double BestError = 1e300;
		const int Stage = CampaignStageId(Chapter, Level);
		for (int Size = MinSectionSize; Size <= MaxSectionSize; ++Size)
		{
			const double Threat = Context.Threat(Stage, GeneratePlan(Chapter, Level, Size));
			const double Error = std::fabs(std::log(Threat / Best.Target));
			if (Error < BestError)
			{
				BestError = Error;
				Best.Size = Size;
				Best.Threat = Threat;
			}
		}
		return Best;
	}

	struct FSolved
	{
		FStageTables Tables;
		std::vector<FSizeChoice> Sizes;   // chapters 2..13 x levels 1..8, row-major
	};

	inline FSolved SolveAll()
	{
		FSolved Solved;
		Solved.Tables = BuildStageTables();
		const FThreatContext Context = MakeThreatContext(Solved.Tables);
		for (int Chapter = FirstGeneratedChapter; Chapter <= CampaignChapters; ++Chapter)
		{
			// A chapter whose enemy mix is more dangerous than the pace allows even at the smallest sections (10 enemies) gets
			// proportionally lower health and damage multipliers: the geometric mean over levels 1-7 of what is needed.
			double LogSum = 0.0;
			for (int Level = 1; Level < CampaignLevels; ++Level)
			{
				const double Threat = Context.Threat(CampaignStageId(Chapter, Level), GeneratePlan(Chapter, Level, MinSectionSize));
				LogSum += 0.5 * std::log(TargetThreat(Chapter, Level) / Threat);
			}
			const double Factor = std::clamp(std::exp(LogSum / (CampaignLevels - 1)), MinChapterFactor, 1.0);
			Solved.Tables.ChapterFactor[static_cast<size_t>(Chapter)] = Factor;
			for (int Level = 1; Level <= CampaignLevels; ++Level)
			{
				FEnemyScale& Scale = Solved.Tables.Scale[static_cast<size_t>(CampaignStageId(Chapter, Level))];
				Scale.Hp *= Factor;
				Scale.Damage *= Factor;
			}
			for (int Level = 1; Level <= CampaignLevels; ++Level)
			{
				FSizeChoice Choice = SolveSectionSize(Context, Chapter, Level);
				// Balancing pass 2026-10-09: a level that is still too hard at its best size (most often a boss level already at the
				// smallest sections: 3-8 was 24% over, 12-8 13%) has its enemy damage trimmed to the target. Health stays, so the
				// fight is as long as planned but the gate is the one the chapter ramp asks for.
				if (Choice.Threat > Choice.Target * OverTargetTrim)
				{
					Solved.Tables.Scale[static_cast<size_t>(CampaignStageId(Chapter, Level))].Damage *= Choice.Target / Choice.Threat;
					Choice.Threat = Context.Threat(CampaignStageId(Chapter, Level), GeneratePlan(Chapter, Level, Choice.Size));
				}
				Solved.Sizes.push_back(Choice);
			}
		}
		return Solved;
	}
}
