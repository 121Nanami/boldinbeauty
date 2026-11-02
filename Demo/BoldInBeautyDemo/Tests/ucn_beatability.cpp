// Ultimate Custom Night: the solvability and beatability suite (owner request 2026-10-14, "incredibly hard, but never
// impossible"). Fixed seed set, at least 200 seeds per mode:
//   perfect bot (zero reaction, full information)   must win 100% in all 4 modes; any loss is an impossible state (seed printed)
//   human-model bot (0.35 s +- 0.1, 5% missed inputs, no two actions inside 0.2 s, finds camera things by looking)
//                                                   targets Easy >= 90%, Medium >= 60%, Hard >= 25%, Angel >= 3%
//   do-nothing bot                                  must lose all 4 modes
// plus the fairness counters (no untelegraphed or short-window kill, no night that never ends) and resource sufficiency
// (power, shocks, cameras left for the perfect bot). Build & run: Tests/run_core_tests.sh, or alone:
//   ucn_beatability [--seeds N]
#include "../Source/BoldInBeautyDemo/BIBRules/BIBUcnBots.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

	using namespace BIB::Arcade::Ucn;

	const double HumanTarget[ModeCount] = {0.90, 0.60, 0.25, 0.03};
}

int main(int argc, char** argv)
{
	int Seeds = 200;
	for (int Index = 1; Index + 1 < argc; ++Index)
	{
		if (std::strcmp(argv[Index], "--seeds") == 0)
		{
			Seeds = std::max(1, std::atoi(argv[Index + 1]));
		}
	}
	const FUcnTuning Tuning;
	std::printf("Ultimate Custom Night beatability: %d seeds per mode (seeds 1..%d), reaction budget %.2f s\n", Seeds, Seeds, Tuning.ReactionBudget);
	std::printf("| Mode   | Perfect | Human  | Do-nothing | Human target | Perfect min power | Perfect min shocks left | Perfect min cameras |\n");
	std::printf("|--------|---------|--------|------------|--------------|-------------------|-------------------------|---------------------|\n");
	for (int M = 0; M < ModeCount; ++M)
	{
		const EMode Mode = static_cast<EMode>(M);
		const FSuiteResult Perfect = RunSuite(Mode, EBotKind::Perfect, Seeds, 1u, Tuning);
		const FSuiteResult Human = RunSuite(Mode, EBotKind::Human, Seeds, 1u, Tuning);
		const FSuiteResult Idle = RunSuite(Mode, EBotKind::DoNothing, Seeds, 1u, Tuning);
		std::printf("| %-6s | %6.1f%% | %5.1f%% | %9.1f%% | >= %5.0f%%    | %17.1f | %23d | %19d |\n", ModeName(Mode), 100.0 * Perfect.WinRate(),
			100.0 * Human.WinRate(), 100.0 * Idle.WinRate(), 100.0 * HumanTarget[M], Perfect.MinPower, Perfect.MinShocksLeft, Perfect.MinCamerasWorking);

		Expect(Perfect.Wins == Perfect.Nights, std::string("perfect bot wins every seed - ") + ModeName(Mode));
		for (size_t Index = 0; Index < Perfect.LossSeeds.size(); ++Index)
		{
			std::printf("  IMPOSSIBLE STATE %s seed %u: %s\n", ModeName(Mode), Perfect.LossSeeds[Index], Perfect.LossReasons[Index].c_str());
		}
		Expect(Human.WinRate() >= HumanTarget[M], std::string("human-model bot reaches its target - ") + ModeName(Mode));
		Expect(Idle.Wins == 0, std::string("do-nothing bot loses every night (the threats are real) - ") + ModeName(Mode));
		for (const FSuiteResult* Suite : {&Perfect, &Human, &Idle})
		{
			const std::string Tag = std::string(ModeName(Mode)) + " / " + BotName(Suite->Bot);
			Expect(Suite->UntelegraphedKills == 0, "no untelegraphed kill - " + Tag);
			Expect(Suite->ShortWindowKills == 0, "no kill inside a window shorter than the reaction budget - " + Tag);
		}
		// Resource sufficiency (perfect player): never ran out of power, shocks or cameras on any seed it won (it won them all).
		Expect(Perfect.MinPower > 0.0, std::string("power lasts the night - ") + ModeName(Mode));
		Expect(Perfect.MinShocksLeft >= 0 && Perfect.MinCamerasWorking >= 1, std::string("shocks and cameras last the night - ") + ModeName(Mode));
		// Never hangs: a night that does not end within the cap would be reported as a timed-out loss.
		Expect(Perfect.AverageSeconds < 1500.0 && Human.AverageSeconds < 1500.0, std::string("nights end - ") + ModeName(Mode));

		// Where the human-model bot dies (for the report).
		std::printf("  human-model losses %s:", ModeName(Mode));
		int Shown = 0;
		for (int Killer = 0; Killer <= SubjectCount; ++Killer)
		{
			if (Human.Killers[Killer] > 0)
			{
				std::printf(" %s %d", Killer < SubjectCount ? SubjectInfo(Killer).Name : "power/dark", Human.Killers[Killer]);
				++Shown;
			}
		}
		std::printf("%s; Time Keeper hours lost %d; attacks skipped by the scheduler (perfect) %d\n", Shown == 0 ? " none" : "", Human.HoursLost, Perfect.GaveUp);
	}
	std::printf("%d Ultimate Custom Night beatability checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
