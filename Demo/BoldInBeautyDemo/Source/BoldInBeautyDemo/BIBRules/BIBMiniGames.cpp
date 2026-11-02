// Boldinbeauty demo — the arcade mini-games (see BIBMiniGames.h). Engine-independent; every number is PLACEHOLDER.
#include "BIBMiniGames.h"

#include "BIBCore.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Arcade
	{
		namespace
		{
			int DiffOf(EDifficulty Difficulty)
			{
				return std::clamp(static_cast<int>(Difficulty), 0, DifficultyCount - 1);
			}

			double Uniform(std::mt19937& Rng)
			{
				return std::uniform_real_distribution<double>(0.0, 1.0)(Rng);
			}
		}

		EGame GameOf(EMiniGame Mini)
		{
			switch (Mini)
			{
			case EMiniGame::HaloFall: return EGame::HaloFall;
			case EMiniGame::NeonMemory: return EGame::NeonMemory;
			case EMiniGame::FirewallBreach: return EGame::FirewallBreach;
			case EMiniGame::ArkRally:
			case EMiniGame::Count: break;
			}
			return EGame::ArkRally;
		}

		const char* MiniGameName(EMiniGame Mini)
		{
			return GameName(GameOf(Mini));
		}

		int MiniRecordSlot(EMiniGame Mini, EDifficulty Difficulty)
		{
			return std::clamp(static_cast<int>(Mini), 0, MiniGameCount - 1) * DifficultyCount + DiffOf(Difficulty);
		}

		// ===================================================================== Halo Fall
		FHaloRules HaloRules(EDifficulty Difficulty)
		{
			FHaloRules Rules;
			switch (Difficulty)
			{
			case EDifficulty::Easy:
				Rules = {3, 4, 0.060, 0.15, 0.55, 0.40, 0.30, 0.95};
				break;
			case EDifficulty::Medium:
				Rules = {4, 3, 0.075, 0.15, 0.85, 0.48, 0.32, 0.90};
				break;
			case EDifficulty::Hard:
			case EDifficulty::Count:
				Rules = {5, 3, 0.085, 0.16, 1.05, 0.55, 0.32, 0.90};
				break;
			}
			return Rules;
		}

		void FHaloFall::Start(EDifficulty InDifficulty, uint32_t Seed)
		{
			*this = FHaloFall();
			Difficulty = InDifficulty;
			R = HaloRules(InDifficulty);
			Rng.seed(Seed);
			LivesLeft = R.Lives;
			SeraphTimer = 8.0 + 6.0 * Uniform(Rng);
			SpawnWave();
		}

		void FHaloFall::SpawnWave()
		{
			Formation.clear();
			Bullets.clear();
			const int Rows = std::min(5, 2 + WaveNumber);   // wave 1: 3 rows of 8, up to 5 rows
			const double SpacingX = 0.08;
			const double Left = 0.5 - SpacingX * (HaloCols - 1) * 0.5;
			for (int Row = 0; Row < Rows; ++Row)
			{
				for (int Col = 0; Col < HaloCols; ++Col)
				{
					FHaloAngel Angel;
					Angel.X = Left + SpacingX * Col;
					Angel.Y = 0.12 + 0.07 * Row;
					Angel.Row = Row;
					Formation.push_back(Angel);
				}
			}
			MarchDir = WaveNumber % 2 == 1 ? 1.0 : -1.0;
		}

		void FHaloFall::Tick(double DeltaSeconds)
		{
			if (bOver)
			{
				return;
			}
			Pending += std::max(0.0, DeltaSeconds);
			while (Pending >= HaloStep && !bOver)
			{
				Pending -= HaloStep;
				Step(HaloStep);
			}
		}

		void FHaloFall::SetMove(int Direction)
		{
			Move = std::clamp(Direction, -1, 1);
		}

		void FHaloFall::SetFiring(bool bHold)
		{
			bFiring = bHold;
		}

		int FHaloFall::AliveAngels() const
		{
			int Count = 0;
			for (const FHaloAngel& Angel : Formation)
			{
				Count += Angel.bAlive ? 1 : 0;
			}
			return Count;
		}

		void FHaloFall::Finish(bool bVictory)
		{
			bOver = true;
			bWon = bVictory;
			if (bVictory)
			{
				Points += 1000LL * LivesLeft;   // every life left is worth 1,000
			}
			Emit(bVictory ? EHaloEventKind::Won : EHaloEventKind::Lost);
		}

		void FHaloFall::ForceClearWave()
		{
			for (FHaloAngel& Angel : Formation)
			{
				if (Angel.bAlive)
				{
					Angel.bAlive = false;
					Points += 100LL * WaveNumber;
				}
			}
		}

		void FHaloFall::Step(double Dt)
		{
			Clock += Dt;
			Cooldown = std::max(0.0, Cooldown - Dt);
			Invulnerable = std::max(0.0, Invulnerable - Dt);
			X = std::clamp(X + Move * R.MoveSpeed * Dt, 0.04, 0.96);

			// Between waves: the next formation drops in after a short pause.
			if (WaveDelay > 0.0)
			{
				WaveDelay -= Dt;
				if (WaveDelay <= 0.0)
				{
					SpawnWave();
				}
				return;
			}

			if (bFiring && Cooldown <= 0.0)
			{
				Bullets.push_back({X, HaloPlayerY - 0.03, false});
				Cooldown = R.FireCooldown;
				Emit(EHaloEventKind::Shot);
			}

			// The formation marches sideways and steps down at the edges; the fewer angels are left, the faster they march.
			const int Alive = AliveAngels();
			const double Total = static_cast<double>(std::max<size_t>(1, Formation.size()));
			const double Speed = R.MarchSpeed * std::pow(1.0 + R.MarchGrowth, WaveNumber - 1) * (1.0 + 0.8 * (1.0 - static_cast<double>(Alive) / Total));
			double MinX = 1.0;
			double MaxX = 0.0;
			for (const FHaloAngel& Angel : Formation)
			{
				if (Angel.bAlive)
				{
					MinX = std::min(MinX, Angel.X);
					MaxX = std::max(MaxX, Angel.X);
				}
			}
			bool bDrop = false;
			if ((MarchDir > 0.0 && MaxX + Speed * Dt > 0.96) || (MarchDir < 0.0 && MinX - Speed * Dt < 0.04))
			{
				MarchDir = -MarchDir;
				bDrop = true;
			}
			for (FHaloAngel& Angel : Formation)
			{
				Angel.X += bDrop ? 0.0 : MarchDir * Speed * Dt;
				Angel.Y += bDrop ? 0.035 : 0.0;
				if (Angel.bAlive && Angel.Y >= HaloPlayerY - 0.06)
				{
					// The angels reached the commander's line: the city falls.
					LivesLeft = 0;
					Emit(EHaloEventKind::PlayerHit, 0);
					Finish(false);
					return;
				}
			}

			// Light bolts: a random angel from the lowest row of a random column fires.
			const double BoltRate = R.BoltsPerSecond * std::pow(1.2, WaveNumber - 1);
			if (Alive > 0 && Uniform(Rng) < BoltRate * Dt)
			{
				std::vector<int> Shooters;
				for (int Col = 0; Col < HaloCols; ++Col)
				{
					int Lowest = -1;
					for (int Index = Col; Index < static_cast<int>(Formation.size()); Index += HaloCols)
					{
						if (Formation[static_cast<size_t>(Index)].bAlive)
						{
							Lowest = Index;
						}
					}
					if (Lowest >= 0)
					{
						Shooters.push_back(Lowest);
					}
				}
				const FHaloAngel& Shooter = Formation[static_cast<size_t>(Shooters[std::min(Shooters.size() - 1, static_cast<size_t>(Uniform(Rng) * static_cast<double>(Shooters.size())))])];
				Bullets.push_back({Shooter.X, Shooter.Y + 0.03, true});
			}

			// The golden Seraph crosses the top now and then: a bonus target.
			if (!bSeraph)
			{
				SeraphTimer -= Dt;
				if (SeraphTimer <= 0.0)
				{
					bSeraph = true;
					SeraphPos = -0.05;
				}
			}
			else
			{
				SeraphPos += 0.22 * Dt;
				if (SeraphPos > 1.05)
				{
					bSeraph = false;
					SeraphTimer = 10.0 + 8.0 * Uniform(Rng);
				}
			}

			// Move the shots and resolve hits.
			for (FHaloShot& Shot : Bullets)
			{
				Shot.Y += (Shot.bEnemy ? R.BoltSpeed : -HaloShotSpeed) * Dt;
			}
			for (size_t Index = 0; Index < Bullets.size();)
			{
				FHaloShot& Shot = Bullets[Index];
				bool bGone = Shot.Y < 0.0 || Shot.Y > 1.0;
				if (!bGone && !Shot.bEnemy)
				{
					for (FHaloAngel& Angel : Formation)
					{
						if (Angel.bAlive && std::abs(Angel.X - Shot.X) < HaloAngelHalfW && std::abs(Angel.Y - Shot.Y) < HaloAngelHalfH)
						{
							Angel.bAlive = false;
							bGone = true;
							const int Worth = (Angel.Row == 0 ? 300 : 100) * WaveNumber;
							Points += Worth;
							Emit(EHaloEventKind::AngelDown, Worth);
							break;
						}
					}
					if (!bGone && bSeraph && std::abs(SeraphPos - Shot.X) < 0.045 && Shot.Y < 0.08)
					{
						bSeraph = false;
						SeraphTimer = 10.0 + 8.0 * Uniform(Rng);
						bGone = true;
						const int Worth = 1500 * WaveNumber;
						Points += Worth;
						Emit(EHaloEventKind::SeraphDown, Worth);
					}
				}
				else if (!bGone && Shot.bEnemy && Invulnerable <= 0.0 && std::abs(Shot.X - X) < HaloPlayerHalfW && Shot.Y > HaloPlayerY - 0.025
					&& Shot.Y < HaloPlayerY + 0.025)
				{
					bGone = true;
					--LivesLeft;
					++Hits;
					Invulnerable = HaloInvulnerable;
					Emit(EHaloEventKind::PlayerHit, LivesLeft);
					if (LivesLeft <= 0)
					{
						Finish(false);
						return;
					}
					// A hit clears the falling bolts so the next life starts fair.
					Bullets.erase(std::remove_if(Bullets.begin(), Bullets.end(), [](const FHaloShot& S) { return S.bEnemy; }), Bullets.end());
					break;
				}
				if (bGone)
				{
					Bullets.erase(Bullets.begin() + static_cast<std::ptrdiff_t>(Index));
				}
				else
				{
					++Index;
				}
			}

			if (AliveAngels() == 0)
			{
				++Cleared;
				Points += 500LL * WaveNumber;
				Emit(EHaloEventKind::WaveCleared, WaveNumber);
				if (Cleared >= R.Waves)
				{
					Finish(true);
					return;
				}
				++WaveNumber;
				WaveDelay = 1.5;
				Bullets.clear();
			}
		}

		int FHaloFall::Stars() const
		{
			if (!bOver)
			{
				return 0;
			}
			if (!bWon)
			{
				return std::min(2, Cleared);   // a lost run still earns a star per wave cleared (max 2)
			}
			return std::max(3, 5 - Hits);      // flawless 5, one hit 4, more 3
		}

		std::vector<FHaloEvent> FHaloFall::PopEvents()
		{
			std::vector<FHaloEvent> Out;
			Out.swap(Events);
			return Out;
		}

		// =================================================================== Neon Memory
		FMemoryRules MemoryRules(EDifficulty Difficulty)
		{
			FMemoryRules Rules;
			switch (Difficulty)
			{
			case EDifficulty::Easy:
				Rules = {4, 3, 60.0, 3.0, 0.8};
				break;
			case EDifficulty::Medium:
				Rules = {4, 4, 70.0, 2.5, 0.7};
				break;
			case EDifficulty::Hard:
			case EDifficulty::Count:
				Rules = {6, 4, 70.0, 2.0, 0.6};
				break;
			}
			return Rules;
		}

		void FNeonMemory::Start(EDifficulty InDifficulty, uint32_t Seed)
		{
			*this = FNeonMemory();
			Difficulty = InDifficulty;
			R = MemoryRules(InDifficulty);
			std::mt19937 Rng(Seed);
			// Pick distinct operators for the pairs, then shuffle the cards.
			std::vector<int> Pool;
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				if (Roster()[static_cast<size_t>(Index)].Kit != EKit::Placeholder)
				{
					Pool.push_back(Index);
				}
			}
			std::shuffle(Pool.begin(), Pool.end(), Rng);
			const int PairCount = R.Cols * R.Rows / 2;
			for (int Pair = 0; Pair < PairCount; ++Pair)
			{
				const int Face = Pool[static_cast<size_t>(Pair) % Pool.size()];
				Faces.push_back(Face);
				Faces.push_back(Face);
			}
			std::shuffle(Faces.begin(), Faces.end(), Rng);
			State.assign(Faces.size(), 0);
		}

		void FNeonMemory::Tick(double DeltaSeconds)
		{
			if (bOver)
			{
				return;
			}
			Clock += std::max(0.0, DeltaSeconds);
			if (HideAt >= 0.0 && Clock >= HideAt)
			{
				HideMismatch();
			}
			if (Clock >= R.Preview + R.TimeLimit)
			{
				bOver = true;
				bWon = false;
			}
		}

		void FNeonMemory::HideMismatch()
		{
			if (First >= 0 && State[static_cast<size_t>(First)] == 1)
			{
				State[static_cast<size_t>(First)] = 0;
			}
			if (Second >= 0 && State[static_cast<size_t>(Second)] == 1)
			{
				State[static_cast<size_t>(Second)] = 0;
			}
			First = -1;
			Second = -1;
			HideAt = -1.0;
		}

		bool FNeonMemory::Flip(int Card)
		{
			if (bOver || InPreview() || Card < 0 || Card >= CardCount())
			{
				return false;
			}
			if (Second >= 0)
			{
				HideMismatch();   // flipping a third card turns the wrong pair back at once
			}
			if (State[static_cast<size_t>(Card)] != 0)
			{
				return false;
			}
			State[static_cast<size_t>(Card)] = 1;
			if (First < 0)
			{
				First = Card;
				return true;
			}
			Second = Card;
			++MoveCount;
			if (Faces[static_cast<size_t>(First)] == Faces[static_cast<size_t>(Second)])
			{
				State[static_cast<size_t>(First)] = 2;
				State[static_cast<size_t>(Second)] = 2;
				LastMatchedFace = Faces[static_cast<size_t>(Card)];
				++Streak;
				BestStreak = std::max(BestStreak, Streak);
				++MatchedPairs;
				Points += 100LL * Streak;   // matches in a row are worth more
				First = -1;
				Second = -1;
				if (MatchedPairs == Pairs())
				{
					bOver = true;
					bWon = true;
					Points += static_cast<long long>(std::floor(TimeLeft())) * 10;   // 10 a second left
				}
			}
			else
			{
				Streak = 0;
				HideAt = Clock + R.MismatchShow;
			}
			return true;
		}

		int FNeonMemory::FaceOf(int Card) const
		{
			return Card >= 0 && Card < CardCount() ? Faces[static_cast<size_t>(Card)] : -1;
		}

		bool FNeonMemory::IsFaceUp(int Card) const
		{
			return Card >= 0 && Card < CardCount() && (InPreview() || State[static_cast<size_t>(Card)] != 0);
		}

		bool FNeonMemory::IsMatched(int Card) const
		{
			return Card >= 0 && Card < CardCount() && State[static_cast<size_t>(Card)] == 2;
		}

		double FNeonMemory::TimeLeft() const
		{
			return std::clamp(R.Preview + R.TimeLimit - Clock, 0.0, R.TimeLimit);
		}

		int FNeonMemory::PartnerOf(int Card) const
		{
			for (int Other = 0; Other < CardCount(); ++Other)
			{
				if (Other != Card && FaceOf(Other) == FaceOf(Card))
				{
					return Other;
				}
			}
			return -1;
		}

		int FNeonMemory::Stars() const
		{
			if (!bOver || !bWon)
			{
				return 0;
			}
			const double Ratio = static_cast<double>(MoveCount) / std::max(1, Pairs());
			return Ratio <= 1.25 ? 5 : (Ratio <= 1.6 ? 4 : (Ratio <= 2.0 ? 3 : (Ratio <= 2.6 ? 2 : 1)));
		}

		// ================================================================ Firewall Breach
		FBreachRules BreachRules(EDifficulty Difficulty)
		{
			FBreachRules Rules;
			switch (Difficulty)
			{
			case EDifficulty::Easy:
				Rules = {8, 3, 0.12, 0.92, 0.55, 1.07, 7.0};
				break;
			case EDifficulty::Medium:
				Rules = {10, 3, 0.10, 0.92, 0.70, 1.08, 6.0};
				break;
			case EDifficulty::Hard:
			case EDifficulty::Count:
				Rules = {12, 2, 0.09, 0.92, 0.85, 1.08, 5.0};
				break;
			}
			return Rules;
		}

		const char* BreachJudgeName(EBreachJudge Judge)
		{
			switch (Judge)
			{
			case EBreachJudge::Perfect: return "PERFECT";
			case EBreachJudge::Good: return "BREACHED";
			case EBreachJudge::Miss: return "BLOCKED";
			case EBreachJudge::None: break;
			}
			return "";
		}

		void FFirewallBreach::Start(EDifficulty InDifficulty, uint32_t Seed)
		{
			*this = FFirewallBreach();
			Difficulty = InDifficulty;
			R = BreachRules(InDifficulty);
			Rng.seed(Seed);
			IntegrityLeft = R.Integrity;
			NewPort();
		}

		void FFirewallBreach::NewPort()
		{
			Half = R.Window * std::pow(R.WindowDecay, LayerIndex);
			Speed = R.Speed * std::pow(R.SpeedGrowth, LayerIndex);
			Centre = Half + 0.02 + (1.0 - 2.0 * (Half + 0.02)) * Uniform(Rng);
			// The cursor restarts at an edge, away from the port.
			Pos = Centre > 0.5 ? 0.0 : 1.0;
			Dir = Centre > 0.5 ? 1.0 : -1.0;
			Trace = R.TraceSeconds;
		}

		void FFirewallBreach::Tick(double DeltaSeconds)
		{
			if (bOver)
			{
				return;
			}
			double Left = std::max(0.0, DeltaSeconds);
			while (Left > 0.0 && !bOver)
			{
				const double Dt = std::min(Left, 0.005);
				Left -= Dt;
				Clock += Dt;
				Pos += Dir * Speed * Dt;
				if (Pos > 1.0)
				{
					Pos = 2.0 - Pos;
					Dir = -1.0;
				}
				else if (Pos < 0.0)
				{
					Pos = -Pos;
					Dir = 1.0;
				}
				Trace -= Dt;
				if (Trace <= 0.0)
				{
					Judge(EBreachJudge::Miss);   // the trace caught up: a miss
				}
			}
		}

		EBreachJudge FFirewallBreach::Press()
		{
			if (bOver)
			{
				return EBreachJudge::None;
			}
			const double Off = std::abs(Pos - Centre);
			const EBreachJudge Result = Off <= Half * BreachPerfectShare ? EBreachJudge::Perfect : (Off <= Half ? EBreachJudge::Good : EBreachJudge::Miss);
			Judge(Result);
			return Result;
		}

		void FFirewallBreach::Judge(EBreachJudge Result)
		{
			Last = Result;
			LastTime = Clock;
			if (Result == EBreachJudge::Miss)
			{
				++MissCount;
				Streak = 0;
				--IntegrityLeft;
				if (IntegrityLeft <= 0)
				{
					bOver = true;
					bWon = false;
					return;
				}
				NewPort();   // same layer, a fresh port
				return;
			}
			++Streak;
			if (Result == EBreachJudge::Perfect)
			{
				++PerfectCount;
				Points += 300LL * (LayerIndex + 1) * std::min(Streak, 5);
			}
			else
			{
				++GoodCount;
				Points += 150LL * (LayerIndex + 1);
			}
			++LayerIndex;
			if (LayerIndex >= R.Layers)
			{
				bOver = true;
				bWon = true;
				Points += 2000LL * IntegrityLeft;   // integrity left is a bonus
				return;
			}
			NewPort();
		}

		int FFirewallBreach::Stars() const
		{
			if (!bOver || !bWon)
			{
				return 0;
			}
			const double PerfectShare = static_cast<double>(PerfectCount) / std::max(1, R.Layers);
			return 2 + (MissCount == 0 ? 1 : 0) + (PerfectShare >= 0.5 ? 1 : 0) + (PerfectShare >= 0.8 ? 1 : 0);
		}

		double FFirewallBreach::SecondsToCentre() const
		{
			// Walk the bouncing path until it crosses the centre.
			double P = Pos;
			double D = Dir;
			double T = 0.0;
			for (int Guard = 0; Guard < 4; ++Guard)
			{
				const double Target = D > 0.0 ? (Centre >= P ? Centre : 1.0) : (Centre <= P ? Centre : 0.0);
				const double Leg = std::abs(Target - P) / std::max(1e-6, Speed);
				T += Leg;
				if (std::abs(Target - Centre) < 1e-9)
				{
					return T;
				}
				P = Target;
				D = -D;
			}
			return T;
		}

		// ============================================================== the profile side
		FReward MiniReward(EMiniGame Mini, EDifficulty Difficulty, bool bWon, int Stars)
		{
			(void)Mini;   // one table for the mini-games (PLACEHOLDER), on the Rhythm scale
			FReward Reward;
			if (!bWon)
			{
				Reward.Gold = 300;
				return Reward;
			}
			const int D = DiffOf(Difficulty);
			static const int Gold[DifficultyCount] = {1500, 3000, 5000};
			static const int Bytes[DifficultyCount] = {500, 1000, 1700};
			static const int Hearts[DifficultyCount] = {0, 5, 10};
			const double Share = 0.4 + 0.12 * std::clamp(Stars, 0, 5);
			Reward.Gold = static_cast<int>(Gold[D] * Share);
			Reward.Bytes = static_cast<int>(Bytes[D] * Share);
			Reward.Hearts = Stars >= 3 ? Hearts[D] : 0;
			Reward.Diamonds = Stars >= 5 ? 10 + 10 * D : 0;
			return Reward;
		}

		std::string FinishMini(FProfile& Profile, EMiniGame Mini, EDifficulty Difficulty, bool bWon, long long Score, int Stars)
		{
			FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
			const int Slot = MiniRecordSlot(Mini, Difficulty);
			const int Clamped = static_cast<int>(std::clamp<long long>(Score, 0, 2000000000LL));
			const bool bBest = Clamped > Records.MiniBest[Slot];
			Records.MiniBest[Slot] = std::max(Records.MiniBest[Slot], Clamped);
			Records.MiniStars[Slot] = std::max(Records.MiniStars[Slot], std::clamp(Stars, 0, 5));
			Records.MiniWins[Slot] += bWon ? 1 : 0;
			const std::string Text = Meta::ApplyReward(Profile, MiniReward(Mini, Difficulty, bWon, Stars));
			return std::string(MiniGameName(Mini)) + " (" + DifficultyName(Difficulty) + ") " + (bWon ? "cleared" : "failed") + " - score " + Meta::Num(Score) + ", "
				+ std::to_string(Stars) + " stars" + (bBest ? " (new best)" : "") + ". Reward: " + Text;
		}

		std::string FinishHalo(FProfile& Profile, const FHaloFall& Game)
		{
			return FinishMini(Profile, EMiniGame::HaloFall, Game.GetDifficulty(), Game.IsWon(), Game.Score(), Game.Stars());
		}

		std::string FinishMemory(FProfile& Profile, const FNeonMemory& Game)
		{
			return FinishMini(Profile, EMiniGame::NeonMemory, Game.GetDifficulty(), Game.IsWon(), Game.Score(), Game.Stars());
		}

		std::string FinishBreach(FProfile& Profile, const FFirewallBreach& Game)
		{
			return FinishMini(Profile, EMiniGame::FirewallBreach, Game.GetDifficulty(), Game.IsWon(), Game.Score(), Game.Stars());
		}
	}
}
