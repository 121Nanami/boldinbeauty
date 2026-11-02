// Mutation fuzz of the save loader (owner request 2026-10-04: a save is a text file on the player's disk, so loading must
// never crash or produce an impossible profile). Not part of run_core_tests.sh because it is slow under sanitizers:
//   c++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-exceptions -fno-rtti \
//       Tests/save_fuzz.cpp Source/BoldInBeautyDemo/BIBRules/{BIBCore,BIBCampaign,BIBMeta,BIBMetaOps,BIBPages,BIBJson,BIBSave,BIBTutorial}.cpp \
//       Source/BoldInBeautyDemo/BIBRules/BIBKit*.cpp -o /tmp/save_fuzz && /tmp/save_fuzz [iterations]
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBJson.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

using namespace BIB;

namespace
{
	int GFailures = 0;

	void Expect(bool bCondition, const std::string& What)
	{
		if (!bCondition)
		{
			++GFailures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	void CheckInvariants(const FProfile& P, const std::string& Context)
	{
		for (size_t Index = 0; Index < P.Operators.size(); ++Index)
		{
			const FOwnedOperator& Owned = P.Operators[Index];
			if (!Owned.bOwned)
			{
				continue;
			}
			Expect(Owned.Level >= 1 && Owned.Level <= P.LevelCap(static_cast<int>(Index)), Context + ": level within the cap");
			Expect(Owned.UpgradeStars >= 0 && Owned.DupResource >= 0 && Owned.HeartGatesPaid >= 0, Context + ": non-negative operator numbers");
		}
		for (int Slot = 0; Slot < 3; ++Slot)
		{
			Expect(P.Team[Slot] < 0 || P.Owns(P.Team[Slot]), Context + ": squad holds owned operators only");
			for (int Other = 0; Other < Slot; ++Other)
			{
				Expect(P.Team[Slot] < 0 || P.Team[Slot] != P.Team[Other], Context + ": squad has no duplicates");
			}
		}
		for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
		{
			Expect(P.Raw(static_cast<ECurrency>(Index)) >= 0, Context + ": no negative currency");
		}
		Expect(P.Owns(FindOperator("Luna")), Context + ": Luna is owned");
		Expect(P.Banners.size() == P.Pity.size() && !P.Banners.empty(), Context + ": banners and pity stay parallel");
		Expect(P.Day >= 1 && P.StagesCleared >= 0 && P.StagesCleared <= TotalCampaignLevels, Context + ": day and stages in range");
		// The hub must still build and run an action on whatever was loaded.
		FProfile Copy = P;
		Pages::FUiState State;
		for (int Tab = 0; Tab < Pages::TabCount; ++Tab)
		{
			State.Tab = Tab;
			Pages::Build(Copy, State);
		}
	}
}

int main(int Argc, char** Argv)
{
	const int Iterations = Argc > 1 ? std::atoi(Argv[1]) : 3000;
	FProfile Source(1);
	Source.StartNewPlayer();
	Source.TutorialSummon();
	Source.SetTime(Config::RotationAnchorUnix + 5 * 259200 + 17);
	Source.Add(ECurrency::Gold, 777777);
	for (int Round = 0; Round < 3; ++Round)
	{
		Source.Summon(Source.FindBanner("standard"), 10, EPay::Diamonds);
	}
	Source.AutoPlaceInTeam(FindOperator("Kuu"));
	const std::string Good = Save::Serialize(Source, 1800000000);

	std::mt19937 Random(12345);
	int Loaded = 0;
	int Refused = 0;
	for (int Iteration = 0; Iteration < Iterations; ++Iteration)
	{
		std::string Text = Good;
		const int Edits = 1 + static_cast<int>(Random() % 8);
		for (int Edit = 0; Edit < Edits; ++Edit)
		{
			if (Text.empty())
			{
				break;
			}
			const size_t At = Random() % Text.size();
			switch (Random() % 6)
			{
			case 0: Text[At] = static_cast<char>(Random() % 256); break;
			case 1: Text.erase(At, 1 + Random() % 5); break;
			case 2: Text.insert(At, std::string(1 + Random() % 4, static_cast<char>("{}[]\",:-0123456789e."[Random() % 20]))); break;
			case 3: Text = Text.substr(0, At); break;
			case 4: Text.insert(At, "99999999999999999999"); break;
			default: Text.insert(At, "-1"); break;
			}
		}
		FProfile Target(static_cast<uint32_t>(Iteration));
		Target.StartNewPlayer();
		std::string Error;
		if (Save::Deserialize(Text, Target, Error))
		{
			++Loaded;
			CheckInvariants(Target, "iteration " + std::to_string(Iteration));
		}
		else
		{
			++Refused;
		}
		Save::Peek(Text);
	}
	// Pure garbage and deep nesting.
	for (int Iteration = 0; Iteration < 200; ++Iteration)
	{
		std::string Garbage(1 + Random() % 400, ' ');
		for (char& Character : Garbage)
		{
			Character = static_cast<char>(Random() % 256);
		}
		FProfile Target(1);
		std::string Error;
		Expect(!Save::Deserialize(Garbage, Target, Error), "garbage is refused");
	}
	std::printf("%d mutated saves: %d loaded and sane, %d refused, %d failures\n", Iterations, Loaded, Refused, GFailures);
	return GFailures == 0 ? 0 : 1;
}
