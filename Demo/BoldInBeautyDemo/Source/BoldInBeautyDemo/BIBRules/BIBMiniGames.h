// Boldinbeauty demo — three small arcade mini-games (owner request 2026-10-07: "a few more simple ... arcade mini games that fit
// the bb style"). Engine-independent and deterministic, so they are unit-tested without Unreal (Tests/minigame_tests.cpp); the
// Unreal layer (BIBGame/BIBArcadeOverlay) draws them and reads the keys, exactly like Night Shift and Rhythm (BIBArcade.h).
//
//   1. Halo Fall       — an angel shooter (Space Invaders / Galaga style): angels of light descend in formation and fire light
//                        bolts; move left / right and shoot them down wave after wave.
//   2. Neon Memory     — a memory match with the operators' portraits: flip two cards, keep the pairs, chain matches for combos.
//   3. Firewall Breach — a timing hack: a cursor sweeps the bar, stop it inside the open port; every layer is narrower and faster.
//
// The names, themes, rules and every number below are PROPOSED / PLACEHOLDER (the owner decides). They follow the existing arcade
// pattern: Easy / Medium / Hard, 1 arcade token a play, best score and stars saved, a reward when the game ends.
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "BIBArcade.h"

namespace BIB
{
	namespace Arcade
	{
		// Index of the three mini-games inside FArcadeRecords::MiniBest / MiniStars (game * DifficultyCount + difficulty).
		enum class EMiniGame : int
		{
			HaloFall = 0,
			NeonMemory,
			FirewallBreach,
			ArkRally,         // 2026-10-15, its own file: BIBRally.h
			Count
		};
		constexpr int MiniGameCount = static_cast<int>(EMiniGame::Count);
		EGame GameOf(EMiniGame Mini);
		const char* MiniGameName(EMiniGame Mini);
		int MiniRecordSlot(EMiniGame Mini, EDifficulty Difficulty);

		// ================================================================== Halo Fall
		// The field is a 0..1 square: x left to right, y top to bottom. The commander's turret sits on HaloPlayerY.
		constexpr double HaloPlayerY = 0.92;
		constexpr double HaloStep = 1.0 / 60.0;          // fixed simulation step (seconds)
		constexpr double HaloAngelHalfW = 0.03;          // hit box of an angel
		constexpr double HaloAngelHalfH = 0.025;
		constexpr double HaloPlayerHalfW = 0.035;        // hit box of the turret
		constexpr double HaloShotSpeed = 1.6;            // field heights per second
		constexpr double HaloInvulnerable = 1.5;         // seconds after a hit
		constexpr int HaloCols = 8;

		struct FHaloRules
		{
			int Waves = 3;               // waves to clear for the win
			int Lives = 4;
			double MarchSpeed = 0.06;    // formation side speed (field widths per second) on wave 1
			double MarchGrowth = 0.15;   // +15% per wave
			double BoltsPerSecond = 0.8; // light bolts fired by the whole formation per second on wave 1 (+20% per wave)
			double BoltSpeed = 0.45;
			double FireCooldown = 0.32;  // the turret's shot interval
			double MoveSpeed = 0.9;      // turret speed (field widths per second)
		};
		FHaloRules HaloRules(EDifficulty Difficulty);

		struct FHaloAngel
		{
			double X = 0.0;
			double Y = 0.0;
			int Row = 0;      // 0 = top row (Seraph, worth more)
			bool bAlive = true;
		};

		struct FHaloShot
		{
			double X = 0.0;
			double Y = 0.0;
			bool bEnemy = false;   // a light bolt from an angel (falls) or the turret's shot (rises)
		};

		enum class EHaloEventKind : int
		{
			Shot = 0,     // the turret fired
			AngelDown,    // A = points
			SeraphDown,   // the golden Seraph crossing the top was shot (A = points)
			PlayerHit,    // A = lives left
			WaveCleared,  // A = wave number cleared
			Won,
			Lost
		};

		struct FHaloEvent
		{
			EHaloEventKind Kind = EHaloEventKind::Shot;
			int A = 0;
		};

		class FHaloFall
		{
		public:
			void Start(EDifficulty InDifficulty, uint32_t Seed);
			void Tick(double DeltaSeconds);       // fixed 1/60 s steps inside
			void SetMove(int Direction);          // -1 left, 0 stop, +1 right
			void SetFiring(bool bHold);           // hold to keep shooting (one shot per FireCooldown)

			EDifficulty GetDifficulty() const { return Difficulty; }
			const FHaloRules& Rules() const { return R; }
			double Time() const { return Clock; }
			double PlayerX() const { return X; }
			int Lives() const { return LivesLeft; }
			int Wave() const { return WaveNumber; }     // 1-based, the wave being fought
			int WavesCleared() const { return Cleared; }
			long long Score() const { return Points; }
			bool IsInvulnerable() const { return Invulnerable > 0.0; }
			bool IsOver() const { return bOver; }
			bool IsWon() const { return bWon; }
			int Stars() const;                          // 0..5 at the end
			const std::vector<FHaloAngel>& Angels() const { return Formation; }
			const std::vector<FHaloShot>& Shots() const { return Bullets; }
			int AliveAngels() const;
			bool SeraphActive() const { return bSeraph; }
			double SeraphX() const { return SeraphPos; }
			std::vector<FHaloEvent> PopEvents();

			// For tests and the verify script: shoot down the whole current wave.
			void ForceClearWave();

		private:
			void Step(double Dt);
			void SpawnWave();
			void Emit(EHaloEventKind Kind, int A = 0) { Events.push_back({Kind, A}); }
			void Finish(bool bVictory);

			EDifficulty Difficulty = EDifficulty::Easy;
			FHaloRules R;
			std::mt19937 Rng;
			double Clock = 0.0;
			double Pending = 0.0;
			double X = 0.5;
			int Move = 0;
			bool bFiring = false;
			double Cooldown = 0.0;
			double Invulnerable = 0.0;
			int LivesLeft = 3;
			int WaveNumber = 1;
			int Cleared = 0;
			int Hits = 0;
			long long Points = 0;
			double MarchDir = 1.0;
			double WaveDelay = 0.0;     // pause between waves
			bool bSeraph = false;
			double SeraphPos = -0.1;
			double SeraphTimer = 0.0;
			bool bOver = false;
			bool bWon = false;
			std::vector<FHaloAngel> Formation;
			std::vector<FHaloShot> Bullets;
			std::vector<FHaloEvent> Events;
		};

		// ================================================================ Neon Memory
		struct FMemoryRules
		{
			int Cols = 4;
			int Rows = 3;
			double TimeLimit = 90.0;     // seconds
			double Preview = 3.0;        // every card is shown face up at the start
			double MismatchShow = 0.8;   // a wrong pair stays face up this long (or until the next flip)
		};
		FMemoryRules MemoryRules(EDifficulty Difficulty);

		class FNeonMemory
		{
		public:
			void Start(EDifficulty InDifficulty, uint32_t Seed);
			void Tick(double DeltaSeconds);
			// Flips a card. False when it cannot be flipped now (preview, matched, already face up, game over).
			bool Flip(int Card);

			EDifficulty GetDifficulty() const { return Difficulty; }
			const FMemoryRules& Rules() const { return R; }
			int Cols() const { return R.Cols; }
			int Rows() const { return R.Rows; }
			int CardCount() const { return static_cast<int>(Faces.size()); }
			int Pairs() const { return CardCount() / 2; }
			int FaceOf(int Card) const;          // roster index of the operator on the card
			bool IsFaceUp(int Card) const;       // shown right now (preview, matched or flipped)
			bool IsMatched(int Card) const;
			bool InPreview() const { return Clock < R.Preview; }
			double TimeLeft() const;
			double Time() const { return Clock; }
			int Moves() const { return MoveCount; }     // pairs turned over
			int Matched() const { return MatchedPairs; }
			int Combo() const { return Streak; }
			int BestCombo() const { return BestStreak; }
			long long Score() const { return Points; }
			bool IsOver() const { return bOver; }
			bool IsWon() const { return bWon; }
			int Stars() const;                   // 0..5 at the end (by moves)
			int LastMatch() const { return LastMatchedFace; }   // roster index of the last pair found (-1 = none)

			// For tests and the verify script: the other card with the same face (-1 = none).
			int PartnerOf(int Card) const;

		private:
			void HideMismatch();
			EDifficulty Difficulty = EDifficulty::Easy;
			FMemoryRules R;
			double Clock = 0.0;
			std::vector<int> Faces;
			std::vector<uint8_t> State;   // 0 face down, 1 face up (flipped), 2 matched
			int First = -1;
			int Second = -1;
			double HideAt = -1.0;
			int MoveCount = 0;
			int MatchedPairs = 0;
			int Streak = 0;
			int BestStreak = 0;
			long long Points = 0;
			int LastMatchedFace = -1;
			bool bOver = false;
			bool bWon = false;
		};

		// ============================================================= Firewall Breach
		struct FBreachRules
		{
			int Layers = 8;              // layers to breach for the win
			int Integrity = 3;           // misses allowed
			double Window = 0.12;        // half-width of the open port on layer 1 (share of the bar)
			double WindowDecay = 0.90;   // x per layer
			double Speed = 0.6;          // cursor speed on layer 1 (bars per second, it bounces)
			double SpeedGrowth = 1.08;   // x per layer
			double TraceSeconds = 6.0;   // a layer not breached in time counts as a miss
		};
		FBreachRules BreachRules(EDifficulty Difficulty);
		constexpr double BreachPerfectShare = 0.3;   // the inner 30% of the port is a Perfect

		enum class EBreachJudge : int
		{
			Perfect = 0,
			Good,
			Miss,
			None
		};
		const char* BreachJudgeName(EBreachJudge Judge);

		class FFirewallBreach
		{
		public:
			void Start(EDifficulty InDifficulty, uint32_t Seed);
			void Tick(double DeltaSeconds);
			EBreachJudge Press();   // stop the cursor

			EDifficulty GetDifficulty() const { return Difficulty; }
			const FBreachRules& Rules() const { return R; }
			double Cursor() const { return Pos; }               // 0..1
			double PortCentre() const { return Centre; }
			double PortHalfWidth() const { return Half; }
			double CursorSpeed() const { return Speed; }
			int Layer() const { return LayerIndex + 1; }        // 1-based, the layer being hacked
			int LayersBreached() const { return LayerIndex; }
			int Integrity() const { return IntegrityLeft; }
			double TraceLeft() const { return Trace; }
			int Perfects() const { return PerfectCount; }
			int Goods() const { return GoodCount; }
			int Misses() const { return MissCount; }
			int Combo() const { return Streak; }
			long long Score() const { return Points; }
			bool IsOver() const { return bOver; }
			bool IsWon() const { return bWon; }
			int Stars() const;
			EBreachJudge LastJudge() const { return Last; }
			double LastJudgeTime() const { return LastTime; }
			double Time() const { return Clock; }
			// Seconds until the cursor next crosses the port centre (the verify script's autoplay).
			double SecondsToCentre() const;

		private:
			void NewPort();
			void Judge(EBreachJudge Result);
			EDifficulty Difficulty = EDifficulty::Easy;
			FBreachRules R;
			std::mt19937 Rng;
			double Clock = 0.0;
			double Pos = 0.0;
			double Dir = 1.0;
			double Centre = 0.5;
			double Half = 0.1;
			double Speed = 0.6;
			double Trace = 6.0;
			int LayerIndex = 0;
			int IntegrityLeft = 3;
			int PerfectCount = 0;
			int GoodCount = 0;
			int MissCount = 0;
			int Streak = 0;
			long long Points = 0;
			bool bOver = false;
			bool bWon = false;
			EBreachJudge Last = EBreachJudge::None;
			double LastTime = -10.0;
		};

		// ============================================================ the profile side
		// Same scale as Rhythm (PLACEHOLDER): a cleared game pays Gold / Bytes by stars, Angel hearts from 3 stars, Diamonds at 5.
		FReward MiniReward(EMiniGame Mini, EDifficulty Difficulty, bool bWon, int Stars);
		// Records the best score and stars, pays the reward and returns the result line. StartGame (BIBArcade.h) took the token.
		std::string FinishMini(FProfile& Profile, EMiniGame Mini, EDifficulty Difficulty, bool bWon, long long Score, int Stars);
		std::string FinishHalo(FProfile& Profile, const FHaloFall& Game);
		std::string FinishMemory(FProfile& Profile, const FNeonMemory& Game);
		std::string FinishBreach(FProfile& Profile, const FFirewallBreach& Game);
	}
}
