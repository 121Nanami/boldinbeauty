// Boldinbeauty demo — Ultimate Custom Night: the headless bots and the beatability suite (owner request 2026-10-14).
// Perfect bot (zero reaction, full information), budget bot (exactly the scheduler's reaction budget and UI travel, full
// information), human-model bot (reaction 0.35 s +- 0.1, 5% missed inputs, no two actions inside 0.2 s, finds camera things
// by looking) and the do-nothing bot. All of them act only through the player's own controls.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BIBUltimateNight.h"

namespace BIB
{
	namespace Arcade
	{
		namespace Ucn
		{
			enum class EBotKind : int
			{
				Perfect = 0,
				Budget,
				Human,
				DoNothing,
				Count
			};
			const char* BotName(EBotKind Kind);

			struct FBotProfile
			{
				double ReactionMean = 0.0;     // seconds before a newly noticed duty is acted on
				double ReactionSigma = 0.0;
				double ActionGap = 0.0;        // seconds between two of its actions (>= 0.2 for the human)
				double MissChance = 0.0;       // an input that does nothing
				bool bFullInfo = true;         // knows which camera a duty is on without looking
				bool bActs = true;
				double PatrolEvery = 10.0;     // human: a sweep of all cameras this often
			};
			FBotProfile ProfileFor(EBotKind Kind, const FUcnTuning& Tuning);

			class FBot
			{
			public:
				FBot(EBotKind InKind, uint32_t Seed, const FUcnTuning& Tuning);
				// Call before every Tick; may take several actions in one call (perfect) or at most one (the others).
				void Act(FUltimateNight& W);
				int ActionsTaken() const { return Taken; }

			private:
				struct FAct
				{
					enum EKind
					{
						None,
						Door,
						Camera,
						Select,
						Shock,
						Vaccine,
						Click,
						Ad,
						Window
					} Kind = None;
					int A = 0;
					int B = 0;
				};

				FAct Decide(FUltimateNight& W);
				bool Execute(FUltimateNight& W, const FAct& Act);
				bool Noticed(FUltimateNight& W, const FDuty& Duty);
				int KnownCam(const FUltimateNight& W, const FDuty& Duty) const;
				bool WatchNeeded(const FUltimateNight& W) const;
				double Sample(double Mean, double Sigma);
				double Uniform01();

				EBotKind Kind;
				FBotProfile Profile;
				FUcnTuning T;
				uint32_t RngState;
				double NextAct = 0.0;
				int Taken = 0;
				std::vector<std::pair<int, double>> NoticeAt;   // duty id -> time it may be acted on
				std::vector<std::pair<int, int>> Seen;          // duty id -> camera found by looking
				bool bStarWarn = false;
				int StarStop = 0;
				double StarSeenAt = 0.0;   // when the bot has reacted to Stargazer's current stop
				double LastSweep = -100.0;
				int SweepLeft = 0;
				int SweepNext = 0;
			};

			// One whole night with a bot. MaxSeconds is a hard cap: a night that does not end is a loss (never hangs).
			struct FNightResult
			{
				uint32_t Seed = 0;
				bool bWon = false;
				bool bTimedOut = false;
				double Seconds = 0.0;
				int Hour = 0;
				int Killer = -1;
				std::string Reason;
				FUcnStats Stats;
				int ShocksLeft = 0;
				int CamerasWorking = 0;
				double PowerLeft = 0.0;
				int Actions = 0;
			};
			FNightResult RunNight(EMode Mode, uint32_t Seed, EBotKind Bot, const FUcnTuning& Tuning = FUcnTuning(), double MaxSeconds = 1500.0);

			struct FSuiteResult
			{
				EMode Mode = EMode::Easy;
				EBotKind Bot = EBotKind::Perfect;
				int Nights = 0;
				int Wins = 0;
				std::vector<uint32_t> LossSeeds;   // first few
				std::vector<std::string> LossReasons;
				int Killers[SubjectCount + 1] = {};   // [SubjectCount] = the dark / other
				int UntelegraphedKills = 0;
				int ShortWindowKills = 0;
				double MinLethalWindow = 1e9;
				double MinPower = 100.0;           // lowest power seen in any night
				int MinShocksLeft = 1 << 20;       // fewest shock charges left at the end of any WON night
				int MinCamerasWorking = 1 << 20;
				int Deferrals = 0;
				int GaveUp = 0;
				int HoursLost = 0;
				double AverageSeconds = 0.0;
				double WinRate() const { return Nights > 0 ? static_cast<double>(Wins) / Nights : 0.0; }
			};
			FSuiteResult RunSuite(EMode Mode, EBotKind Bot, int Seeds, uint32_t FirstSeed = 1, const FUcnTuning& Tuning = FUcnTuning());
		}
	}
}
