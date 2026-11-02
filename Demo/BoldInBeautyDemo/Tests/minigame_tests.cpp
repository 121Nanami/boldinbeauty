// Tests for the arcade mini-games (BIBRules/BIBMiniGames.h, BIBRally.h): Halo Fall, Neon Memory, Firewall Breach and Ark Rally. Bots play
// hundreds of games per difficulty (a careful bot clears Easy almost always, Hard is hard), plus scoring, stars, rewards, the hub
// pages and the save round-trip. Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
//   minigame_tests --balance   also prints the bots' win-rate table.
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMiniGames.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBRally.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>

namespace
{
	int GFailures = 0;
	int GChecks = 0;
	bool GPrintBalance = false;

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
	using namespace BIB::Arcade;
	namespace P = BIB::Pages;

	bool Has(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	const P::FPageAction* Find(const P::FPage& Page, const std::string& Id)
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

	// ========================================================================= Halo Fall bot
	// Stands under the angel closest to the street and shoots; steps aside from bolts about to land. Skill 0..1 scales its
	// reaction (a lower skill reacts later to bolts and wobbles while aiming).
	bool PlayHalo(EDifficulty Difficulty, uint32_t Seed, double Skill, FHaloFall* Out = nullptr)
	{
		FHaloFall Game;
		Game.Start(Difficulty, Seed);
		std::mt19937 Noise(Seed * 7u + 1u);
		std::uniform_real_distribution<double> U(-1.0, 1.0);
		double Aim = 0.5;
		double Retarget = 0.0;
		for (int Frame = 0; Frame < 60 * 600 && !Game.IsOver(); ++Frame)
		{
			Retarget -= HaloStep;
			if (Retarget <= 0.0)
			{
				// Target the lowest living angel; a weaker player picks with some slop.
				double BestY = -1.0;
				for (const FHaloAngel& Angel : Game.Angels())
				{
					if (Angel.bAlive && (Angel.Y > BestY + 0.01 || (std::abs(Angel.Y - BestY) <= 0.01 && std::abs(Angel.X - Game.PlayerX()) < std::abs(Aim - Game.PlayerX()))))
					{
						BestY = Angel.Y;
						Aim = Angel.X;
					}
				}
				Aim += U(Noise) * 0.03 * (1.0 - Skill);
				Retarget = 0.15 + 0.35 * (1.0 - Skill);
			}
			// Dodge: a bolt that lands near the turret within the reaction horizon.
			const double Horizon = 0.12 + 0.25 * Skill;
			int Dodge = 0;
			for (const FHaloShot& Shot : Game.Shots())
			{
				if (Shot.bEnemy && Shot.Y > HaloPlayerY - Horizon && Shot.Y < HaloPlayerY + 0.02 && std::abs(Shot.X - Game.PlayerX()) < HaloPlayerHalfW + 0.03)
				{
					Dodge = Shot.X > Game.PlayerX() ? -1 : 1;
					if (Game.PlayerX() < 0.08) Dodge = 1;
					if (Game.PlayerX() > 0.92) Dodge = -1;
				}
			}
			if (Dodge != 0)
			{
				Game.SetMove(Dodge);
			}
			else
			{
				const double Error = Aim - Game.PlayerX();
				Game.SetMove(std::abs(Error) < 0.01 ? 0 : (Error > 0.0 ? 1 : -1));
			}
			Game.SetFiring(true);
			Game.Tick(HaloStep);
		}
		if (Out)
		{
			*Out = Game;
		}
		return Game.IsWon();
	}

	// ======================================================================= Neon Memory bot
	// Remembers each card it has seen with probability Recall; flips a known pair when it has one.
	bool PlayMemory(EDifficulty Difficulty, uint32_t Seed, double Recall, double SecondsPerFlip, FNeonMemory* Out = nullptr)
	{
		FNeonMemory Game;
		Game.Start(Difficulty, Seed);
		std::mt19937 Rng(Seed * 13u + 5u);
		std::uniform_real_distribution<double> U(0.0, 1.0);
		std::vector<int> Known(static_cast<size_t>(Game.CardCount()), -1);
		// The preview: the bot memorises some cards.
		for (int Card = 0; Card < Game.CardCount(); ++Card)
		{
			if (U(Rng) < Recall * 0.5)
			{
				Known[static_cast<size_t>(Card)] = Game.FaceOf(Card);
			}
		}
		Game.Tick(Game.Rules().Preview + 0.01);
		auto See = [&](int Card)
		{
			if (U(Rng) < Recall)
			{
				Known[static_cast<size_t>(Card)] = Game.FaceOf(Card);
			}
		};
		for (int Guard = 0; Guard < 1000 && !Game.IsOver(); ++Guard)
		{
			int A = -1;
			int B = -1;
			for (int I = 0; I < Game.CardCount() && A < 0; ++I)
			{
				for (int J = I + 1; J < Game.CardCount(); ++J)
				{
					if (!Game.IsMatched(I) && !Game.IsMatched(J) && Known[static_cast<size_t>(I)] >= 0 && Known[static_cast<size_t>(I)] == Known[static_cast<size_t>(J)])
					{
						A = I;
						B = J;
						break;
					}
				}
			}
			if (A < 0)
			{
				// Flip an unknown card, then its known partner if remembered, else another unknown card.
				std::vector<int> Unknown;
				for (int I = 0; I < Game.CardCount(); ++I)
				{
					if (!Game.IsMatched(I) && Known[static_cast<size_t>(I)] < 0)
					{
						Unknown.push_back(I);
					}
				}
				if (Unknown.empty())
				{
					for (int I = 0; I < Game.CardCount(); ++I)
					{
						if (!Game.IsMatched(I))
						{
							Unknown.push_back(I);
						}
					}
				}
				A = Unknown[static_cast<size_t>(U(Rng) * static_cast<double>(Unknown.size())) % Unknown.size()];
				Game.Flip(A);
				See(A);
				Game.Tick(SecondsPerFlip);
				for (int J = 0; J < Game.CardCount(); ++J)
				{
					if (J != A && !Game.IsMatched(J) && Known[static_cast<size_t>(J)] == Game.FaceOf(A))
					{
						B = J;
					}
				}
				if (B < 0)
				{
					do
					{
						B = Unknown[static_cast<size_t>(U(Rng) * static_cast<double>(Unknown.size())) % Unknown.size()];
					} while (B == A && Unknown.size() > 1);
					if (B == A)
					{
						for (int J = 0; J < Game.CardCount(); ++J)
						{
							if (J != A && !Game.IsMatched(J))
							{
								B = J;
								break;
							}
						}
					}
				}
				Game.Flip(B);
				See(B);
				Game.Tick(SecondsPerFlip);
				continue;
			}
			Game.Flip(A);
			Game.Tick(SecondsPerFlip);
			Game.Flip(B);
			Game.Tick(SecondsPerFlip);
		}
		if (!Game.IsOver())
		{
			Game.Tick(1000.0);
		}
		if (Out)
		{
			*Out = Game;
		}
		return Game.IsWon();
	}

	// ==================================================================== Firewall Breach bot
	// Aims at the port centre and presses with a Gaussian timing error (Sigma seconds).
	bool PlayBreach(EDifficulty Difficulty, uint32_t Seed, double Sigma, FFirewallBreach* Out = nullptr)
	{
		FFirewallBreach Game;
		Game.Start(Difficulty, Seed);
		std::mt19937 Rng(Seed * 31u + 3u);
		std::normal_distribution<double> Error(0.0, std::max(1e-9, Sigma));
		for (int Guard = 0; Guard < 200 && !Game.IsOver(); ++Guard)
		{
			const double Wait = std::max(0.0, Game.SecondsToCentre() + (Sigma > 0.0 ? Error(Rng) : 0.0));
			Game.Tick(Wait);
			if (!Game.IsOver())
			{
				Game.Press();
			}
		}
		if (Out)
		{
			*Out = Game;
		}
		return Game.IsWon();
	}

	double WinRate(int Games, const std::function<bool(uint32_t)>& Play)
	{
		int Wins = 0;
		for (int Index = 0; Index < Games; ++Index)
		{
			Wins += Play(static_cast<uint32_t>(1000 + Index * 7919)) ? 1 : 0;
		}
		return static_cast<double>(Wins) / Games;
	}

	// ============================================================================== tests
	void TestHalo()
	{
		FHaloFall Game;
		Game.Start(EDifficulty::Easy, 42);
		Expect(Game.Lives() == HaloRules(EDifficulty::Easy).Lives && Game.Wave() == 1 && Game.AliveAngels() == 3 * HaloCols && !Game.IsOver(),
			"Halo Fall starts on wave 1 with 3 rows of 8 angels and every life");
		Game.SetMove(-1);
		Game.Tick(2.0);
		Expect(Game.PlayerX() < 0.5, "the turret moves left");
		Game.SetMove(1);
		Game.Tick(0.5);
		const double X = Game.PlayerX();
		Game.SetMove(0);
		Game.Tick(0.5);
		Expect(std::abs(Game.PlayerX() - X) < 1e-9, "the turret stops");
		Game.SetFiring(true);
		Game.Tick(1.0 / 60.0);
		bool bShot = false;
		for (const FHaloEvent& Event : Game.PopEvents())
		{
			bShot = bShot || Event.Kind == EHaloEventKind::Shot;
		}
		Expect(bShot && !Game.Shots().empty(), "holding fire shoots");
		Game.SetFiring(false);

		// Clearing every wave wins and pays a life bonus.
		FHaloFall Win;
		Win.Start(EDifficulty::Medium, 7);
		for (int Wave = 0; Wave < 10 && !Win.IsOver(); ++Wave)
		{
			Win.ForceClearWave();
			Win.Tick(2.0);
		}
		Expect(Win.IsWon() && Win.WavesCleared() == HaloRules(EDifficulty::Medium).Waves && Win.Stars() == 5, "clearing every wave wins flawlessly (5 stars)");
		Expect(Win.Score() >= 1000LL * Win.Lives(), "lives left add to the score");

		// Standing still without shooting loses: the angels reach the street or the bolts run out the lives.
		FHaloFall Idle;
		Idle.Start(EDifficulty::Hard, 3);
		Idle.Tick(600.0);
		Expect(Idle.IsOver() && !Idle.IsWon() && Idle.Stars() == 0, "an idle turret loses with no star");

		// Determinism.
		FHaloFall A;
		FHaloFall B;
		PlayHalo(EDifficulty::Medium, 99, 0.8, &A);
		PlayHalo(EDifficulty::Medium, 99, 0.8, &B);
		Expect(A.Score() == B.Score() && A.IsWon() == B.IsWon() && A.Time() == B.Time(), "the same seed plays the same game");

		// Balance: a careful bot clears Easy nearly always; Hard is hard for a casual one.
		const double EasyGood = WinRate(200, [](uint32_t S) { return PlayHalo(EDifficulty::Easy, S, 0.85); });
		const double HardCasual = WinRate(200, [](uint32_t S) { return PlayHalo(EDifficulty::Hard, S, 0.5); });
		const double HardGood = WinRate(200, [](uint32_t S) { return PlayHalo(EDifficulty::Hard, S, 0.85); });
		Expect(EasyGood >= 0.85, "Halo Fall Easy: a careful player wins >= 85% (" + std::to_string(EasyGood) + ")");
		Expect(HardCasual <= 0.5, "Halo Fall Hard: a casual player wins <= 50% (" + std::to_string(HardCasual) + ")");
		Expect(HardGood > 0.0 && HardGood < EasyGood, "Halo Fall Hard is winnable but harder than Easy (" + std::to_string(HardGood) + ")");
		if (GPrintBalance)
		{
			std::printf("Halo Fall win rate        careful(0.85)  casual(0.5)\n");
			for (int D = 0; D < DifficultyCount; ++D)
			{
				const EDifficulty Diff = static_cast<EDifficulty>(D);
				std::printf("  %-8s                 %5.0f%%        %5.0f%%\n", DifficultyName(Diff),
					100.0 * WinRate(200, [Diff](uint32_t S) { return PlayHalo(Diff, S, 0.85); }), 100.0 * WinRate(200, [Diff](uint32_t S) { return PlayHalo(Diff, S, 0.5); }));
			}
		}
	}

	void TestMemory()
	{
		for (int D = 0; D < DifficultyCount; ++D)
		{
			const EDifficulty Diff = static_cast<EDifficulty>(D);
			FNeonMemory Game;
			Game.Start(Diff, 5u + static_cast<uint32_t>(D));
			const FMemoryRules Rules = MemoryRules(Diff);
			Expect(Game.CardCount() == Rules.Cols * Rules.Rows && Game.CardCount() % 2 == 0, "the grid has every card (" + std::string(DifficultyName(Diff)) + ")");
			bool bPairs = true;
			for (int Card = 0; Card < Game.CardCount(); ++Card)
			{
				const int Partner = Game.PartnerOf(Card);
				int Same = 0;
				for (int Other = 0; Other < Game.CardCount(); ++Other)
				{
					Same += Game.FaceOf(Other) == Game.FaceOf(Card) ? 1 : 0;
				}
				bPairs = bPairs && Partner >= 0 && Same == 2 && Game.FaceOf(Card) >= 0 && Game.FaceOf(Card) < static_cast<int>(Roster().size());
			}
			Expect(bPairs, "every operator face appears exactly twice (" + std::string(DifficultyName(Diff)) + ")");
		}
		Expect(MemoryRules(EDifficulty::Hard).Cols * MemoryRules(EDifficulty::Hard).Rows > MemoryRules(EDifficulty::Easy).Cols * MemoryRules(EDifficulty::Easy).Rows,
			"the grid grows with the difficulty");

		FNeonMemory Game;
		Game.Start(EDifficulty::Easy, 11);
		Expect(Game.InPreview() && Game.IsFaceUp(0) && !Game.Flip(0), "the preview shows every card and blocks flips");
		Game.Tick(Game.Rules().Preview + 0.01);
		Expect(!Game.InPreview() && !Game.IsFaceUp(0), "after the preview the cards are face down");
		// A mismatch turns back after a moment.
		int Wrong = 1;
		while (Game.FaceOf(Wrong) == Game.FaceOf(0))
		{
			++Wrong;
		}
		Expect(Game.Flip(0) && Game.Flip(Wrong) && Game.IsFaceUp(0) && Game.Moves() == 1 && Game.Combo() == 0, "a wrong pair is shown and counts a move");
		Game.Tick(Game.Rules().MismatchShow + 0.01);
		Expect(!Game.IsFaceUp(0) && !Game.IsFaceUp(Wrong), "a wrong pair turns back");
		// Two right pairs in a row build a combo.
		const int P0 = Game.PartnerOf(0);
		Game.Flip(0);
		Game.Flip(P0);
		const int Next = Wrong;
		Game.Flip(Next);
		Game.Flip(Game.PartnerOf(Next));
		Expect(Game.IsMatched(0) && Game.IsMatched(P0) && Game.Combo() == 2 && Game.Score() == 100 + 200, "matches in a row score 100, 200 ...");
		Expect(!Game.Flip(0), "a matched card cannot be flipped");

		// A perfect game: 5 stars, time bonus.
		FNeonMemory Perfect;
		Perfect.Start(EDifficulty::Hard, 21);
		Perfect.Tick(Perfect.Rules().Preview + 0.01);
		for (int Card = 0; Card < Perfect.CardCount(); ++Card)
		{
			if (!Perfect.IsMatched(Card))
			{
				Perfect.Flip(Card);
				Perfect.Flip(Perfect.PartnerOf(Card));
				Perfect.Tick(0.5);
			}
		}
		Expect(Perfect.IsWon() && Perfect.Moves() == Perfect.Pairs() && Perfect.Stars() == 5 && Perfect.BestCombo() == Perfect.Pairs(), "a perfect game wins with 5 stars");
		// Running out of time fails.
		FNeonMemory Late;
		Late.Start(EDifficulty::Medium, 2);
		Late.Tick(1000.0);
		Expect(Late.IsOver() && !Late.IsWon() && Late.Stars() == 0 && !Late.Flip(0), "the timer running out fails the game");

		const double EasyGood = WinRate(200, [](uint32_t S) { return PlayMemory(EDifficulty::Easy, S, 0.85, 1.2); });
		const double HardForgetful = WinRate(200, [](uint32_t S) { return PlayMemory(EDifficulty::Hard, S, 0.35, 1.6); });
		Expect(EasyGood >= 0.9, "Neon Memory Easy: a good memory wins >= 90% (" + std::to_string(EasyGood) + ")");
		Expect(HardForgetful <= 0.6, "Neon Memory Hard: a forgetful player wins <= 60% (" + std::to_string(HardForgetful) + ")");
		if (GPrintBalance)
		{
			std::printf("Neon Memory win rate      good(0.85, 1.2 s)  forgetful(0.35, 1.6 s)\n");
			for (int D = 0; D < DifficultyCount; ++D)
			{
				const EDifficulty Diff = static_cast<EDifficulty>(D);
				std::printf("  %-8s                 %5.0f%%             %5.0f%%\n", DifficultyName(Diff),
					100.0 * WinRate(200, [Diff](uint32_t S) { return PlayMemory(Diff, S, 0.85, 1.2); }),
					100.0 * WinRate(200, [Diff](uint32_t S) { return PlayMemory(Diff, S, 0.35, 1.6); }));
			}
		}
	}

	void TestBreach()
	{
		FFirewallBreach Game;
		Game.Start(EDifficulty::Easy, 8);
		Expect(Game.Layer() == 1 && Game.Integrity() == BreachRules(EDifficulty::Easy).Integrity && Game.PortHalfWidth() > 0.0, "Firewall Breach starts on layer 1");
		const double Half1 = Game.PortHalfWidth();
		const double Speed1 = Game.CursorSpeed();
		Game.Tick(Game.SecondsToCentre());
		Expect(std::abs(Game.Cursor() - Game.PortCentre()) < 0.01, "SecondsToCentre brings the cursor to the port");
		Expect(Game.Press() == EBreachJudge::Perfect && Game.Layer() == 2 && Game.Combo() == 1, "stopping in the centre is a Perfect and opens the next layer");
		Expect(Game.PortHalfWidth() < Half1 && Game.CursorSpeed() > Speed1, "every layer is narrower and faster");
		// A miss: costs integrity and keeps the layer.
		const double Centre = Game.PortCentre();
		const double Far = Centre > 0.5 ? 0.05 : 0.95;
		for (int Guard = 0; Guard < 2000 && std::abs(Game.Cursor() - Far) > 0.02; ++Guard)
		{
			Game.Tick(0.002);
		}
		const int Integrity = Game.Integrity();
		Expect(Game.Press() == EBreachJudge::Miss && Game.Integrity() == Integrity - 1 && Game.Layer() == 2 && Game.Combo() == 0, "a press outside the port is a miss");
		// The trace running out is a miss too.
		Game.Tick(Game.Rules().TraceSeconds + 0.01);
		Expect(Game.Integrity() == Integrity - 2 && Game.Misses() == 2, "the trace running out costs integrity");
		Game.Tick(Game.Rules().TraceSeconds + 0.01);
		Expect(Game.IsOver() && !Game.IsWon() && Game.Stars() == 0, "no integrity left fails the breach");

		FFirewallBreach Perfect;
		Expect(PlayBreach(EDifficulty::Hard, 77, 0.0, &Perfect) && Perfect.Stars() == 5 && Perfect.Misses() == 0, "perfect timing breaches Hard with 5 stars");

		const double EasyGood = WinRate(300, [](uint32_t S) { return PlayBreach(EDifficulty::Easy, S, 0.035); });
		const double HardGood = WinRate(300, [](uint32_t S) { return PlayBreach(EDifficulty::Hard, S, 0.035); });
		Expect(EasyGood >= 0.85, "Firewall Breach Easy: a steady hand (35 ms) wins >= 85% (" + std::to_string(EasyGood) + ")");
		Expect(HardGood <= 0.45, "Firewall Breach Hard: the same hand wins <= 45% (" + std::to_string(HardGood) + ")");
		if (GPrintBalance)
		{
			std::printf("Firewall Breach win rate  20 ms   35 ms   60 ms timing error\n");
			for (int D = 0; D < DifficultyCount; ++D)
			{
				const EDifficulty Diff = static_cast<EDifficulty>(D);
				std::printf("  %-8s                 %4.0f%%   %4.0f%%   %4.0f%%\n", DifficultyName(Diff),
					100.0 * WinRate(300, [Diff](uint32_t S) { return PlayBreach(Diff, S, 0.020); }),
					100.0 * WinRate(300, [Diff](uint32_t S) { return PlayBreach(Diff, S, 0.035); }),
					100.0 * WinRate(300, [Diff](uint32_t S) { return PlayBreach(Diff, S, 0.060); }));
			}
		}
	}

	void TestProfileSide()
	{
		// Rewards: the Rhythm scale, more stars and higher difficulty pay more, a loss pays a little Gold.
		Expect(MiniReward(EMiniGame::HaloFall, EDifficulty::Easy, false, 0).Gold == 300 && MiniReward(EMiniGame::HaloFall, EDifficulty::Easy, false, 0).Diamonds == 0,
			"a lost game pays 300 Gold");
		Expect(MiniReward(EMiniGame::NeonMemory, EDifficulty::Hard, true, 5).Gold > MiniReward(EMiniGame::NeonMemory, EDifficulty::Easy, true, 5).Gold
			&& MiniReward(EMiniGame::NeonMemory, EDifficulty::Hard, true, 5).Gold > MiniReward(EMiniGame::NeonMemory, EDifficulty::Hard, true, 3).Gold,
			"harder and better games pay more");
		Expect(MiniReward(EMiniGame::FirewallBreach, EDifficulty::Medium, true, 5).Diamonds == 20 && MiniReward(EMiniGame::FirewallBreach, EDifficulty::Medium, true, 4).Diamonds == 0,
			"5 stars pay Diamonds");
		Expect(MiniReward(EMiniGame::HaloFall, EDifficulty::Hard, true, 5).Gold == RhythmReward(EDifficulty::Hard, true, 5).Gold, "the mini-games pay on the Rhythm scale");
		Expect(std::string(GameName(EGame::HaloFall)) == "Halo Fall" && std::string(MiniGameName(EMiniGame::FirewallBreach)) == "Firewall Breach"
			&& GameOf(EMiniGame::NeonMemory) == EGame::NeonMemory, "game names");

		FProfile Profile;
		Profile.ResetToNewGame();
		Profile.Add(ECurrency::ArcadeToken, 5);
		const int Tokens = Profile.Get(ECurrency::ArcadeToken);
		Expect(StartGame(Profile, EGame::NeonMemory).bOk && Profile.Get(ECurrency::ArcadeToken) == Tokens - 1
			&& Profile.Meta.ArcadeRecords.Plays[static_cast<int>(EGame::NeonMemory)] == 1, "a mini-game costs one token and counts a play");

		FNeonMemory Memory;
		Memory.Start(EDifficulty::Medium, 3);
		Memory.Tick(Memory.Rules().Preview + 0.1);
		for (int Card = 0; Card < Memory.CardCount(); ++Card)
		{
			if (!Memory.IsMatched(Card))
			{
				Memory.Flip(Card);
				Memory.Flip(Memory.PartnerOf(Card));
			}
		}
		const int Gold = Profile.Get(ECurrency::Gold);
		const std::string Text = FinishMemory(Profile, Memory);
		const int Slot = MiniRecordSlot(EMiniGame::NeonMemory, EDifficulty::Medium);
		Expect(Has(Text, "cleared") && Has(Text, "new best") && Profile.Get(ECurrency::Gold) == Gold + MiniReward(EMiniGame::NeonMemory, EDifficulty::Medium, true, 5).Gold,
			"a cleared game pays its reward (" + Text + ")");
		Expect(Profile.Meta.ArcadeRecords.MiniBest[Slot] == static_cast<int>(Memory.Score()) && Profile.Meta.ArcadeRecords.MiniStars[Slot] == 5
			&& Profile.Meta.ArcadeRecords.MiniWins[Slot] == 1, "the best score, stars and wins are recorded");
		FHaloFall Lost;
		Lost.Start(EDifficulty::Easy, 1);
		Lost.Tick(900.0);
		const std::string LostText = FinishHalo(Profile, Lost);
		Expect(Has(LostText, "failed") && Profile.Meta.ArcadeRecords.MiniWins[MiniRecordSlot(EMiniGame::HaloFall, EDifficulty::Easy)] == 0, "a lost game is recorded as failed");
		// A worse score does not replace the best.
		FNeonMemory Worse;
		Worse.Start(EDifficulty::Medium, 3);
		Worse.Tick(1000.0);
		FinishMemory(Profile, Worse);
		Expect(Profile.Meta.ArcadeRecords.MiniBest[Slot] == static_cast<int>(Memory.Score()) && Profile.Meta.ArcadeRecords.MiniStars[Slot] == 5, "a worse game keeps the best");

		// Save round-trip, and an old save without the arrays.
		const std::string Json = Save::Serialize(Profile, 1791072000LL);
		FProfile Loaded;
		std::string Error;
		Expect(Save::Deserialize(Json, Loaded, Error), "the save loads (" + Error + ")");
		Expect(std::equal(std::begin(Loaded.Meta.ArcadeRecords.MiniBest), std::end(Loaded.Meta.ArcadeRecords.MiniBest), std::begin(Profile.Meta.ArcadeRecords.MiniBest))
			&& std::equal(std::begin(Loaded.Meta.ArcadeRecords.MiniStars), std::end(Loaded.Meta.ArcadeRecords.MiniStars), std::begin(Profile.Meta.ArcadeRecords.MiniStars))
			&& std::equal(std::begin(Loaded.Meta.ArcadeRecords.MiniWins), std::end(Loaded.Meta.ArcadeRecords.MiniWins), std::begin(Profile.Meta.ArcadeRecords.MiniWins))
			&& Loaded.Meta.ArcadeRecords.Plays[static_cast<int>(EGame::NeonMemory)] == 1, "the mini-game records survive a save");
		std::string Old = Json;
		for (const char* Key : {"\"miniBest\"", "\"miniStars\"", "\"miniWins\""})
		{
			const size_t At = Old.find(Key);
			if (At != std::string::npos)
			{
				Old.replace(At, std::strlen(Key), std::string("\"x") + (Key + 1));
			}
		}
		FProfile OldLoaded;
		Expect(Save::Deserialize(Old, OldLoaded, Error) && OldLoaded.Meta.ArcadeRecords.MiniBest[Slot] == 0, "an older save without the mini-game records loads with zeros");

		// Hub pages: the arcade lists the games, each game page starts an overlay for one token.
		P::FUiState State;
		State.Tab = P::TabLobby;
		State.Page = "arcade";
		P::FPage Arcade = P::Build(Profile, State);
		Expect(Find(Arcade, "arc:halo") && Find(Arcade, "arc:memory") && Find(Arcade, "arc:breach") && Find(Arcade, "arc:rally") && !Find(Arcade, "arc:night"),
			"the arcade page lists the mini-games, not Night Shift");
		P::Perform(Profile, State, "arc:breach");
		Expect(State.Page == "arcade-breach", "the Firewall Breach page opens");
		P::FPage BreachPage = P::Build(Profile, State);
		Expect(BreachPage.Title == "Firewall Breach" && Find(BreachPage, "arc:breachgo:0") && Find(BreachPage, "arc:breachgo:2") && BreachPage.bCanBack,
			"the game page has the three difficulties");
		const int Before = Profile.Get(ECurrency::ArcadeToken);
		const P::FPerform Go = P::Perform(Profile, State, "arc:breachgo:2");
		Expect(Go.Overlay == P::EOverlay::FirewallBreach && Go.OverlayA == 2 && Profile.Get(ECurrency::ArcadeToken) == Before - 1, "Hard opens the overlay for a token");
		State.Page = "arcade-halo";
		const P::FPerform Halo = P::Perform(Profile, State, "arc:halogo:0");
		Expect(Halo.Overlay == P::EOverlay::HaloFall && Halo.OverlayA == 0, "Halo Fall opens");
		State.Page = "arcade-memory";
		const P::FPerform Mem = P::Perform(Profile, State, "arc:memorygo:1");
		Expect(Mem.Overlay == P::EOverlay::NeonMemory && Mem.OverlayA == 1, "Neon Memory opens");
		P::Perform(Profile, State, "back");
		Expect(State.Page == "arcade", "back returns to the arcade");
		while (Profile.Get(ECurrency::ArcadeToken) > 0)
		{
			Profile.Spend(ECurrency::ArcadeToken, 1);
		}
		const P::FPerform Broke = P::Perform(Profile, State, "arc:halogo:0");
		Expect(Broke.Overlay == P::EOverlay::None && !Broke.bOk, "no token, no game");
	}

	// ========================================================================= Ark Rally
	// Rides a race to the end with the given input function (called every step); returns the finished game.
	FArkRally Ride(FArkRally Game, const std::function<void(FArkRally&)>& Input, double MaxSeconds = 400.0)
	{
		while (!Game.IsOver() && Game.Time() < MaxSeconds)
		{
			Input(Game);
			Game.Tick(RallyStep);
		}
		return Game;
	}

	bool HasEvent(const std::vector<FRallyEvent>& Events, ERallyEventKind Kind)
	{
		return std::any_of(Events.begin(), Events.end(), [Kind](const FRallyEvent& Event) { return Event.Kind == Kind; });
	}

	// Rides a custom track with fixed inputs for some seconds, collecting the events.
	FArkRally RideFor(const std::string& Code, int Throttle, int Lean, double Seconds, std::vector<FRallyEvent>& Events)
	{
		FArkRally Game;
		std::string Error;
		Expect(Game.StartCustom(Code, 1, Error), "the test track \"" + Code + "\" builds (" + Error + ")");
		Game.PopEvents();
		Game.SetThrottle(Throttle);
		Game.SetLean(Lean);
		for (double Done = 0.0; Done < Seconds && !Game.IsOver(); Done += RallyStep)
		{
			Game.Tick(RallyStep);
			for (const FRallyEvent& Event : Game.PopEvents())
			{
				Events.push_back(Event);
			}
		}
		return Game;
	}

	void TestRally()
	{
		// The track code.
		{
			FRallyTrack Track;
			std::string Error;
			Expect(BuildTrack("f10 h20,2 C3 k", "t", Track, Error) && Track.Coins == 3 && std::abs(Track.FinishX - 40.0) < 1e-9,
				"lower-case codes read, the finish is 2 m after the last piece (after the 8 m run-up)");
			Expect(std::abs(Track.HeightAt(28.0) - 2.0) < 0.05 && std::abs(Track.HeightAt(5.0)) < 1e-9, "a hill peaks in its middle");
			Expect(BuildTrack("F10 G5 F10", "t", Track, Error) && Track.Pits.size() == 1 && Track.HeightAt(20.0) < -20.0 && Track.HeightAt(25.0) == 0.0,
				"a gap is a pit");
			Expect(BuildTrack("F10 R10,3 F10", "t", Track, Error) && std::abs(Track.HeightAt(26.0) - 3.0 * std::pow(0.8, 1.6)) < 0.05 && Track.HeightAt(28.5) == 0.0,
				"a ramp rises, then drops back");
			Expect(BuildTrack("F10 U20,5 F10", "t", Track, Error) && std::abs(Track.HeightAt(45.0) - 5.0) < 1e-9, "a slope changes the ground level for good");
			std::string Long;
			for (int Index = 0; Index < 11; ++Index)
			{
				Long += "F190 ";
			}
			const char* const Bad[] = {"", "   ", "X5", "F0", "F300", "H5,10", "G30", "R10,-1", "C50", "F", "F10,2", "W10,0.9", "F10 Q", "Fx", "C3", "H20", "F10 BB",
				Long.c_str()};
			for (const char* Code : Bad)
			{
				Error.clear();
				Expect(!BuildTrack(Code, "t", Track, Error) && !Error.empty(), std::string("\"") + Code + "\" is refused with a reason (" + Error + ")");
			}
			Expect(!BuildTrack(std::string(RallyCodeMaxLength + 1, 'F'), "t", Track, Error), "a code longer than the limit is refused");
		}

		// The built-in tracks: the autopilot rides each one clean (the rivals' ghosts), within sensible times.
		for (int D = 0; D < DifficultyCount; ++D)
		{
			const EDifficulty Diff = static_cast<EDifficulty>(D);
			FArkRally Game;
			Game.Start(Diff, 11);
			const std::string Name = RallyTrackName(Diff);
			Expect(Game.HasGhosts() && Game.ReferenceTime() > 15.0 && Game.ReferenceTime() < 90.0, Name + ": the reference run finishes (" + std::to_string(Game.ReferenceTime()) + " s)");
			Expect(Game.TimeLimit() >= 60.0 && Game.TimeLimit() >= Game.ReferenceTime() * 2.0, Name + ": the time limit leaves room");
			Expect(Game.RivalTime(0) < Game.RivalTime(1) && Game.RivalTime(1) < Game.RivalTime(2), Name + ": the rivals are ordered by pace");
			Expect(Game.RivalFace(0) != Game.RivalFace(1) && Game.RivalFace(1) != Game.RivalFace(2) && Game.RivalFace(0) != Game.RivalFace(2), Name + ": three different rivals");
			const FRallyGhostFrame End = Game.RivalAt(0, Game.RivalTime(0) + 1.0);
			Expect(End.X >= Game.Track().FinishX - 0.5, Name + ": a ghost crosses the line at its time");
			const FArkRally Bot = Ride(Game, [](FArkRally& G) { G.Autopilot(); });
			Expect(Bot.IsWon() && Bot.Crashes() == 0 && std::abs(Bot.Time() - Game.ReferenceTime()) < 0.01, Name + ": the autopilot replays the reference run clean");
			Expect(Bot.Stars() >= 1 && Bot.Score() > 0 && Bot.Place() == 1 + static_cast<int>(std::count_if(std::begin(Game.Rules().RivalPace), std::end(Game.Rules().RivalPace),
				[](double Pace) { return Pace < 1.0; })), Name + ": the place follows the rivals' pace (" + std::to_string(Bot.Place()) + ")");
			FArkRally Again;
			Again.Start(Diff, 999);
			Expect(std::abs(Again.ReferenceTime() - Game.ReferenceTime()) < 1e-9, Name + ": deterministic (the seed only picks the rivals)");
		}
		{
			FArkRally Easy;
			Easy.Start(EDifficulty::Easy, 2);
			const FArkRally Won = Ride(Easy, [](FArkRally& G) { G.Autopilot(); });
			Expect(Won.Place() == 1 && Won.Stars() == 5 && Won.IsWon(), "a clean first place on Easy is 5 stars");
		}

		// A human-like rider: sees the bike 0.15 s late and wobbles on the lean. Easy should be finished nearly always.
		int Rides[DifficultyCount] = {};
		int Finished[DifficultyCount] = {};
		int Podium[DifficultyCount] = {};
		for (int D = 0; D < DifficultyCount; ++D)
		{
			for (uint32_t Seed = 1; Seed <= 12; ++Seed)
			{
				FArkRally Game;
				Game.Start(static_cast<EDifficulty>(D), Seed);
				std::mt19937 Rng(Seed * 31u + static_cast<uint32_t>(D));
				double Next = 0.0;
				const FArkRally Out = Ride(Game, [&Rng, &Next](FArkRally& G)
				{
					if (G.Time() >= Next)
					{
						G.Autopilot();
						if (std::uniform_real_distribution<double>(0.0, 1.0)(Rng) < 0.15)
						{
							G.SetLean(static_cast<int>(Rng() % 3) - 1);
						}
						Next = G.Time() + 0.15;
					}
				});
				++Rides[D];
				Finished[D] += Out.IsWon() ? 1 : 0;
				Podium[D] += Out.IsWon() && Out.Place() <= 3 ? 1 : 0;
			}
		}
		Expect(Finished[0] >= 10, "a sloppy rider finishes Easy nearly always (" + std::to_string(Finished[0]) + "/12)");
		if (GPrintBalance)
		{
			for (int D = 0; D < DifficultyCount; ++D)
			{
				std::printf("Ark Rally %-6s sloppy rider: finished %2d/%d, top 3 %2d\n", DifficultyName(static_cast<EDifficulty>(D)), Finished[D], Rides[D], Podium[D]);
			}
		}

		// The bike: still at rest, quick on the gas, brakes then reverses.
		{
			std::vector<FRallyEvent> Events;
			const FArkRally Idle = RideFor("F40", 0, 0, 10.0, Events);
			Expect(Idle.Crashes() == 0 && Idle.Speed() < 0.05 && std::abs(Idle.Chassis().Pos.X - 4.0) < 0.05 && std::abs(Idle.Chassis().Angle) < 0.01,
				"a bike left alone stands still");
			const FArkRally Gas = RideFor("F120", 1, 0, 3.0, Events);
			Expect(Gas.Chassis().Vel.X > 9.0 && Gas.Crashes() == 0, "3 s of gas reaches 9+ m/s (" + std::to_string(Gas.Chassis().Vel.X) + ")");
			FArkRally Back = Gas;
			Back.SetThrottle(-1);
			Back.Tick(5.0);
			Expect(Back.Chassis().Vel.X < -1.0, "the brake stops the bike, then it reverses");
			// Owner feedback 2026-10-15 ("way too easy to die"): a held wheelie balances instead of looping over.
			FArkRally Wheelie = Gas;
			Wheelie.SetLean(1);
			double Peak = 0.0;
			for (int Step = 0; Step < 4 * 240; ++Step)
			{
				Wheelie.Tick(RallyStep);
				Peak = std::max(Peak, Wheelie.Chassis().Angle);
			}
			Expect(Wheelie.Crashes() == 0 && Peak > 0.3 && Peak < 1.2, "a held wheelie lifts the front and balances (peak " + std::to_string(Peak) + " rad)");
		}

		// Track things.
		{
			std::vector<FRallyEvent> Events;
			const FArkRally Coins = RideFor("F6 C5 F40", 1, 0, 5.0, Events);
			Expect(Coins.Coins() == 5 && HasEvent(Events, ERallyEventKind::Coin), "riding through a coin row takes every coin");
			Events.clear();
			const FArkRally Spring = RideFor("F4 S F60", 1, 0, 2.0, Events);
			Expect(HasEvent(Events, ERallyEventKind::Spring), "a spring launches the bike");
			Events.clear();
			RideFor("F6 B F60", 1, 0, 3.0, Events);
			Expect(HasEvent(Events, ERallyEventKind::Bomb), "a bomb goes off");
			Events.clear();
			const FArkRally Boosted = RideFor("F4 P F120", 1, 0, 3.0, Events);
			std::vector<FRallyEvent> None;
			const FArkRally Plain = RideFor("F4 F120", 1, 0, 3.0, None);
			Expect(HasEvent(Events, ERallyEventKind::Boost) && Boosted.Chassis().Pos.X > Plain.Chassis().Pos.X + 3.0, "a boost pad speeds the bike up");
			Events.clear();
			const FArkRally Pit = RideFor("F10 K F6 G20 F10", 1, 0, 5.0, Events);
			Expect(HasEvent(Events, ERallyEventKind::Checkpoint) && HasEvent(Events, ERallyEventKind::Crash) && HasEvent(Events, ERallyEventKind::Respawn),
				"falling into a gap is a crash and the bike comes back");
			Expect(Pit.Crashes() >= 1 && Pit.Chassis().Pos.X > 17.0, "the bike respawns at the checkpoint");
			Events.clear();
			FArkRally Stuck = RideFor("F40", 0, 0, 1.0, Events);
			Stuck.Respawn();
			Expect(Stuck.IsCrashed() && Stuck.Crashes() == 1, "Respawn sends a stuck bike back (as a crash)");
			Stuck.Tick(RallyCrashPause + 0.1);
			Expect(!Stuck.IsCrashed() && std::abs(Stuck.Chassis().Pos.X - 4.0) < 0.1, "...to the start when no checkpoint was passed");
			Events.clear();
			const FArkRally Late = RideFor("F40", 0, 0, 200.0, Events);
			Expect(Late.IsOver() && !Late.IsWon() && Late.Stars() == 0 && HasEvent(Events, ERallyEventKind::TimeUp) && std::abs(Late.Time() - Late.TimeLimit()) < 0.01,
				"standing still runs out the clock");
		}

		// The profile side: a race pays and records the fourth mini-game slot; a custom track pays nothing.
		{
			Expect(std::string(GameName(EGame::ArkRally)) == "Ark Rally" && GameOf(EMiniGame::ArkRally) == EGame::ArkRally
				&& GameOf(EMiniGame::FirewallBreach) == EGame::FirewallBreach, "Ark Rally names");
			FProfile Profile;
			Profile.ResetToNewGame();
			Profile.Add(ECurrency::ArcadeToken, 5);
			Expect(StartGame(Profile, EGame::ArkRally).bOk && Profile.Meta.ArcadeRecords.Plays[static_cast<int>(EGame::ArkRally)] == 1, "a race costs a token and counts a play");
			FArkRally Race;
			Race.Start(EDifficulty::Medium, 5);
			const FArkRally Done = Ride(Race, [](FArkRally& G) { G.Autopilot(); });
			const int Gold = Profile.Get(ECurrency::Gold);
			const std::string Text = FinishRally(Profile, Done);
			const int Slot = MiniRecordSlot(EMiniGame::ArkRally, EDifficulty::Medium);
			Expect(Slot == 10 && Has(Text, "Ark Rally") && Has(Text, "cleared") && Profile.Meta.ArcadeRecords.MiniBest[Slot] == static_cast<int>(Done.Score())
				&& Profile.Meta.ArcadeRecords.MiniWins[Slot] == 1 && Profile.Get(ECurrency::Gold) > Gold, "a finished race pays and is recorded (" + Text + ")");
			FArkRally Custom;
			std::string Error;
			Custom.StartCustom("F10 H20,2 F10", 1, Error);
			const FArkRally CustomDone = Ride(Custom, [](FArkRally& G) { G.Autopilot(); });
			const int GoldBefore = Profile.Get(ECurrency::Gold);
			const std::string CustomText = FinishRally(Profile, CustomDone);
			Expect(CustomDone.IsCustom() && Has(CustomText, "no reward") && Profile.Get(ECurrency::Gold) == GoldBefore
				&& Profile.Meta.ArcadeRecords.MiniWins[Slot] == 1, "a custom track pays and records nothing (" + CustomText + ")");

			// The player's track survives a save; an older save (3 mini-games, no track) still loads.
			Profile.Meta.ArcadeRecords.RallyTrack = "F10 C3 H20,2 \"quoted\" K";
			const std::string Json = Save::Serialize(Profile, 1791072000LL);
			FProfile Loaded;
			Expect(Save::Deserialize(Json, Loaded, Error) && Loaded.Meta.ArcadeRecords.RallyTrack == Profile.Meta.ArcadeRecords.RallyTrack
				&& Loaded.Meta.ArcadeRecords.MiniBest[Slot] == Profile.Meta.ArcadeRecords.MiniBest[Slot], "the rally record and track survive a save");
			std::string Old = Json;
			const size_t At = Old.find("\"rallyTrack\"");
			if (At != std::string::npos)
			{
				Old.replace(At, 12, "\"xrallyTrack\"");
			}
			FProfile OldLoaded;
			Expect(Save::Deserialize(Old, OldLoaded, Error) && OldLoaded.Meta.ArcadeRecords.RallyTrack.empty(), "a save without a track loads");

			// Hub pages.
			P::FUiState State;
			State.Tab = P::TabLobby;
			State.Page = "arcade";
			Expect(Find(P::Build(Profile, State), "arc:rally") != nullptr, "the arcade lists Ark Rally");
			P::Perform(Profile, State, "arc:rally");
			Profile.Meta.ArcadeRecords.RallyTrack.clear();
			P::FPage Page = P::Build(Profile, State);
			Expect(State.Page == "arcade-rally" && Page.Title == "Ark Rally" && Find(Page, "arc:rallygo:0") && Find(Page, "arc:rallygo:2") && Find(Page, "arc:rallyedit")
				&& Find(Page, "arc:rallytry") && !Find(Page, "arc:rallytry")->bEnabled && Page.bCanBack, "the Ark Rally page: three tracks and the editor");
			const P::FPerform Edit = P::Perform(Profile, State, "arc:rallyedit");
			Expect(Edit.Prompt.Key == "rallytrack" && Edit.Prompt.Initial == RallyTrackCode(EDifficulty::Easy) && Edit.Prompt.MaxLength == RallyCodeMaxLength,
				"editing asks for the track code, starting from the Easy track");
			State.InputText = "F10 H5,10";
			const P::FPerform Typo = P::Perform(Profile, State, "in:rallytrack");
			Expect(!Typo.bOk && Has(Typo.Message, "steep") && Profile.Meta.ArcadeRecords.RallyTrack == "F10 H5,10", "a track with a mistake is kept and the mistake explained");
			Expect(!P::Perform(Profile, State, "arc:rallytry").bOk, "a broken track cannot be ridden");
			State.InputText = "F10 C5 H20,3 K R10,2 G4 F20";
			const P::FPerform Saved = P::Perform(Profile, State, "in:rallytrack");
			Expect(Saved.bOk && Has(Saved.Message, "5 coins"), "a good track is saved (" + Saved.Message + ")");
			const int Tokens = Profile.Get(ECurrency::ArcadeToken);
			const P::FPerform Try = P::Perform(Profile, State, "arc:rallytry");
			Expect(Try.Overlay == P::EOverlay::ArkRally && Try.OverlayB == 1 && Profile.Get(ECurrency::ArcadeToken) == Tokens, "riding your own track is free");
			const P::FPerform Copy = P::Perform(Profile, State, "arc:rallycopy:2");
			Expect(Profile.Meta.ArcadeRecords.RallyTrack != RallyTrackCode(EDifficulty::Hard) && Has(Copy.Message, "tap again"), "replacing your track asks first");
			P::Perform(Profile, State, "arc:rallycopy:2");
			Expect(Profile.Meta.ArcadeRecords.RallyTrack == RallyTrackCode(EDifficulty::Hard), "...then copies the Hard track");
			const P::FPerform Go = P::Perform(Profile, State, "arc:rallygo:1");
			Expect(Go.Overlay == P::EOverlay::ArkRally && Go.OverlayA == 1 && Go.OverlayB == 0 && Profile.Get(ECurrency::ArcadeToken) == Tokens - 1, "a race costs a token");
			P::Perform(Profile, State, "back");
			Expect(State.Page == "arcade", "back returns to the arcade");
		}
	}

	// ======================================================= Ark Rally: vehicles, flips, nitro, forgiving crashes (2026-10-15)
	// Gas off a ramp, lean back after take-off until a turn is nearly done, then let the vehicle settle for the landing.
	FArkRally ScriptedFlip(const std::string& Code, ERallyVehicle Vehicle, double Seconds, double* MaxSpin = nullptr, bool bOverdo = false)
	{
		FArkRally Game;
		std::string Error;
		Game.StartCustom(Code, 1, Error, Vehicle);
		Game.SetThrottle(1);
		bool bTookOff = false;
		double Spin = 0.0;
		for (double Done = 0.0; Done < Seconds && !Game.IsOver(); Done += RallyStep)
		{
			bTookOff = bTookOff || (!Game.IsGrounded() && Game.Time() > 1.0);   // the jump, not the drop-in at the spawn
			if (bTookOff && !Game.IsGrounded())
			{
				const double R = Game.AirSpin();
				Spin = std::max(Spin, std::abs(R));
				Game.SetLean(bOverdo ? 1 : (R < 2.0 * 3.14159265 - 0.9 ? 1 : (R > 2.0 * 3.14159265 - 0.2 ? -1 : 0)));
			}
			else
			{
				Game.SetLean(0);
			}
			Game.Tick(RallyStep);
		}
		if (MaxSpin)
		{
			*MaxSpin = Spin;
		}
		return Game;
	}

	double CrashesPerRace(ERallyVehicle Vehicle, EDifficulty Difficulty, double Mistakes)
	{
		int Crashes = 0;
		for (uint32_t Seed = 1; Seed <= 8; ++Seed)
		{
			FArkRally Game;
			Game.Start(Difficulty, Seed, Vehicle);
			std::mt19937 Rng(Seed * 31u + static_cast<uint32_t>(Difficulty));
			double Next = 0.0;
			const FArkRally Out = Ride(Game, [&Rng, &Next, Mistakes](FArkRally& G)
			{
				if (G.Time() >= Next)
				{
					G.Autopilot();
					if (std::uniform_real_distribution<double>(0.0, 1.0)(Rng) < Mistakes)
					{
						G.SetLean(static_cast<int>(Rng() % 3) - 1);
					}
					Next = G.Time() + 0.15;
				}
			});
			Crashes += Out.Crashes();
		}
		return Crashes / 8.0;
	}

	void TestRallyVehicles()
	{
		const FRallyVehicleSpec& Bike = RallyVehicleSpec(ERallyVehicle::DirtBike);
		const FRallyVehicleSpec& Buggy = RallyVehicleSpec(ERallyVehicle::Buggy);
		Expect(std::string(RallyVehicleName(ERallyVehicle::DirtBike)) == "Dirt bike" && std::string(RallyVehicleName(ERallyVehicle::Buggy)) == "Buggy", "vehicle names");
		Expect(Bike.MaxSpeed > Buggy.MaxSpeed && Buggy.CrashTilt > Bike.CrashTilt && Buggy.RoofTime > 0.0 && Buggy.WheelieTorque == 0.0, "the buggy is slower and sturdier");

		// Top speed on the flat.
		double Top[RallyVehicleCount] = {};
		for (int V = 0; V < RallyVehicleCount; ++V)
		{
			std::vector<FRallyEvent> Events;
			FArkRally Flat;
			std::string Error;
			Flat.StartCustom("F150 F150", 1, Error, static_cast<ERallyVehicle>(V));
			Flat.SetThrottle(1);
			Flat.Tick(9.0);
			Top[V] = Flat.Speed();
			Expect(Flat.Crashes() == 0 && Flat.Vehicle() == static_cast<ERallyVehicle>(V), std::string(RallyVehicleName(static_cast<ERallyVehicle>(V))) + " rides the flat");
		}
		Expect(Top[0] > Top[1] + 3.0, "the dirt bike is faster (" + std::to_string(Top[0]) + " vs " + std::to_string(Top[1]) + " m/s)");

		// Both ride every built-in track clean on autopilot, and the rivals ride the player's vehicle (the bike's runs are quicker).
		for (int D = 0; D < DifficultyCount; ++D)
		{
			double Ref[RallyVehicleCount] = {};
			for (int V = 0; V < RallyVehicleCount; ++V)
			{
				FArkRally Game;
				Game.Start(static_cast<EDifficulty>(D), 3, static_cast<ERallyVehicle>(V));
				const FArkRally Bot = Ride(Game, [](FArkRally& G) { G.Autopilot(); });
				Ref[V] = Game.ReferenceTime();
				Expect(Game.HasGhosts() && Bot.IsWon() && Bot.Crashes() == 0, std::string(RallyVehicleName(static_cast<ERallyVehicle>(V))) + " rides " + RallyTrackName(static_cast<EDifficulty>(D)) + " clean");
			}
			Expect(Ref[0] < Ref[1], std::string(RallyTrackName(static_cast<EDifficulty>(D))) + ": the dirt bike's rivals are quicker than the buggy's");
		}

		// Flips fill the nitro meter; the buggy needs a much bigger jump.
		double BikeSpin = 0.0;
		double BuggySpin = 0.0;
		const FArkRally BikeFlip = ScriptedFlip("F14 R12,4 F150", ERallyVehicle::DirtBike, 5.0, &BikeSpin);
		const FArkRally BuggyTry = ScriptedFlip("F14 R12,4 F150", ERallyVehicle::Buggy, 5.0, &BuggySpin);
		Expect(BikeFlip.Flips() == 1 && BikeFlip.Crashes() == 0 && std::abs(BikeFlip.Nitro() - RallyNitroPerFlip) < 0.02,
			"a landed dirt-bike flip pays nitro (flips " + std::to_string(BikeFlip.Flips()) + ", nitro " + std::to_string(BikeFlip.Nitro()) + ")");
		Expect(BuggyTry.Flips() == 0 && BuggySpin < BikeSpin * 0.7, "the buggy turns much slower in the air (" + std::to_string(BuggySpin) + " vs " + std::to_string(BikeSpin) + " rad)");
		const FArkRally BuggyFlip = ScriptedFlip("F20 R14,8 F150", ERallyVehicle::Buggy, 10.0);
		Expect(BuggyFlip.Flips() >= 1 && BuggyFlip.Nitro() > 0.3, "off a big jump the buggy can flip too");
		const FArkRally Overdone = ScriptedFlip("F14 R14,6 F150", ERallyVehicle::DirtBike, 6.0, nullptr, true);
		Expect(Overdone.Crashes() >= 1 && Overdone.Flips() == 0 && Overdone.Nitro() == 0.0, "a flip that ends on the rider's head pays nothing");

		// Nitro: hold it to burn the meter for extra speed; no meter, no push. Measured on the ground after the landing.
		FArkRally Landed = BikeFlip;
		Landed.SetLean(0);
		for (int Step = 0; Step < 240 * 4 && !(Landed.IsGrounded() && Landed.Chassis().Vel.Y > -0.5 && Landed.Chassis().Vel.Y < 0.5); ++Step)
		{
			Landed.Tick(RallyStep);
		}
		Landed.Tick(0.3);
		FArkRally Burn = Landed;
		FArkRally NoBurn = Landed;
		Burn.SetNitro(true);
		Burn.PopEvents();
		Burn.Tick(0.6);
		NoBurn.Tick(0.6);
		Expect(Burn.Speed() > NoBurn.Speed() + 3.0 && Burn.Nitro() < Landed.Nitro() && HasEvent(Burn.PopEvents(), ERallyEventKind::Nitro),
			"nitro speeds the vehicle up and drains the meter (" + std::to_string(Burn.Speed()) + " vs " + std::to_string(NoBurn.Speed()) + " m/s)");
		Burn.Tick(2.0);
		Expect(Burn.Nitro() == 0.0 && !Burn.IsNitroOn(), "the meter runs out");
		FArkRally Empty;
		std::string Error;
		Empty.StartCustom("F200", 1, Error);
		Empty.SetNitro(true);
		Empty.Tick(0.5);
		Expect(!Empty.IsNitroOn() && Empty.Speed() < 0.1, "no nitro without flips");

		// A crash comes back on safe ground just before the mistake, with most of the speed: not at the start at a standstill.
		{
			FArkRally Pit;
			Pit.StartCustom("F60 G20 F20", 1, Error);
			Pit.SetThrottle(1);
			double CrashX = 0.0;
			bool bBack = false;
			for (int Step = 0; Step < 240 * 8 && !bBack; ++Step)
			{
				Pit.Tick(RallyStep);
				for (const FRallyEvent& Event : Pit.PopEvents())
				{
					CrashX = Event.Kind == ERallyEventKind::Crash ? Pit.Chassis().Pos.X : CrashX;
					bBack = bBack || Event.Kind == ERallyEventKind::Respawn;
				}
			}
			Expect(bBack && Pit.Chassis().Pos.X > 25.0 && Pit.Chassis().Pos.X < 66.0 && Pit.Chassis().Vel.X > 3.0,
				"after a fall the vehicle comes back just before the gap, already moving (x " + std::to_string(Pit.Chassis().Pos.X) + ", " + std::to_string(Pit.Chassis().Vel.X)
					+ " m/s; fell at " + std::to_string(CrashX) + ")");
			const double FirstBack = Pit.Chassis().Pos.X;
			bool bSecond = false;
			for (int Step = 0; Step < 240 * 8 && !bSecond; ++Step)
			{
				Pit.Tick(RallyStep);
				for (const FRallyEvent& Event : Pit.PopEvents())
				{
					bSecond = bSecond || Event.Kind == ERallyEventKind::Respawn;
				}
			}
			Expect(bSecond && Pit.Chassis().Pos.X < FirstBack, "a quick repeat crash goes one safe spot further back");
		}

		// Rage check: a clumsy rider (35% wrong leans) crashes far less than before, and the buggy hardly ever.
		const double BikeHard = CrashesPerRace(ERallyVehicle::DirtBike, EDifficulty::Hard, 0.35);
		const double BuggyHard = CrashesPerRace(ERallyVehicle::Buggy, EDifficulty::Hard, 0.35);
		Expect(BikeHard <= 2.5, "a clumsy rider crashes the dirt bike at most 2.5 times on Hard (" + std::to_string(BikeHard) + "; 3.9 before)");
		Expect(BuggyHard < BikeHard, "the buggy crashes less (" + std::to_string(BuggyHard) + ")");
		if (GPrintBalance)
		{
			std::printf("Ark Rally clumsy rider crashes / Hard race: dirt bike %.2f, buggy %.2f; top speed %.1f vs %.1f m/s\n", BikeHard, BuggyHard, Top[0], Top[1]);
		}

		// The vehicle choice: a page toggle, saved with the profile.
		FProfile Profile;
		Profile.ResetToNewGame();
		P::FUiState State;
		State.Tab = P::TabLobby;
		State.Page = "arcade-rally";
		Expect(Find(P::Build(Profile, State), "arc:rallycar") != nullptr && Profile.Meta.ArcadeRecords.RallyVehicle == 0, "the Ark Rally page offers the vehicle (dirt bike first)");
		const P::FPerform Swap = P::Perform(Profile, State, "arc:rallycar");
		Expect(Profile.Meta.ArcadeRecords.RallyVehicle == 1 && Has(Swap.Message, "Buggy"), "tapping it picks the buggy");
		FProfile Loaded;
		Expect(Save::Deserialize(Save::Serialize(Profile, 1791072000LL), Loaded, Error) && Loaded.Meta.ArcadeRecords.RallyVehicle == 1, "the vehicle survives a save");
		P::Perform(Profile, State, "arc:rallycar");
		Expect(Profile.Meta.ArcadeRecords.RallyVehicle == 0, "and back to the dirt bike");
	}
}

int main(int argc, char** argv)
{
	for (int Index = 1; Index < argc; ++Index)
	{
		GPrintBalance = GPrintBalance || std::strcmp(argv[Index], "--balance") == 0;
	}
	TestHalo();
	TestMemory();
	TestBreach();
	TestRally();
	TestRallyVehicles();
	TestProfileSide();
	std::printf("%d mini-game checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
