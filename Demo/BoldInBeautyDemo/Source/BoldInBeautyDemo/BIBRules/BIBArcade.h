// Boldinbeauty demo — the arcade (owner request 2026-10-06): three games, all engine-independent so they are unit-tested
// without Unreal. The Unreal layer (BIBGame/BIBArcadeOverlay) draws them, reads the keys and plays the sounds.
//
//   1. Night Shift  — a Five Nights at Freddy's style survival night with Luna, Plack, Kuu, Media and Narrator.
//   2. Boss Rush    — boss after boss in the real 3D combat until the team falls; every round pays (BIBGameMode runs it).
//   3. Rhythm       — a simple Guitar Hero style lane game in the Boldinbeauty look.
// Three smaller mini-games (Halo Fall, Neon Memory, Firewall Breach, 2026-10-07) live in BIBMiniGames.h.
//
// The owner gave: the five characters and how each behaves, 100% power and two doors, three modes with the movement
// opportunity rules (Easy 50% every 7 s, Medium 75% every 6 s, Hard 100% every 5 s) and Media's reaction window (2 / 1.5 / 1 s).
// Everything else (room layout, night length, power drain, Kuu's door repair time, rewards, songs) is PLACEHOLDER, chosen so a
// careful player survives Easy almost always and Hard rarely (Tests/arcade_tests.cpp runs a bot over hundreds of nights).
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "BIBMeta.h"

namespace BIB
{
	class FProfile;
	struct FActionResult;

	namespace Arcade
	{
		enum class EDifficulty : int
		{
			Easy = 0,
			Medium,
			Hard,
			Count
		};
		constexpr int DifficultyCount = static_cast<int>(EDifficulty::Count);
		const char* DifficultyName(EDifficulty Difficulty);

		enum class EGame : int
		{
			NightShift = 0,
			BossRush,
			Rhythm,
			HaloFall,         // the mini-games of 2026-10-07 (BIBMiniGames.h)
			NeonMemory,
			FirewallBreach,
			ArkRally,         // 2026-10-15 (BIBRally.h)
			Count
		};
		constexpr int GameCount = static_cast<int>(EGame::Count);
		const char* GameName(EGame Game);
		constexpr int TokenCost = 1;   // arcade tokens per play (PLACEHOLDER)

		// =============================================================== Night Shift
		struct FNightRules
		{
			double OpportunitySeconds = 7.0;   // owner: Easy 7 s, Medium 6 s, Hard 5 s
			double MoveChance = 0.5;           // owner: Easy 50%, Medium 75%, Hard 100%
			double MediaWindow = 2.0;          // owner: Easy 2 s, Medium 1.5 s, Hard 1 s to close the door Media appears at
			double DoorRepairSeconds = 15.0;   // PLACEHOLDER: a door Kuu breaks cannot close for this long
			int MediaChargeToTeleport = 6;     // PLACEHOLDER: Media teleports after this many successful opportunities
			int NarratorStepsPerRoom = 2;      // PLACEHOLDER: like Freddy, she needs several successful rolls per room
		};
		FNightRules NightRules(EDifficulty Difficulty);

		constexpr double NightHourSeconds = 50.0;        // PLACEHOLDER: 12 AM to 6 AM = 6 x 50 s = 5 minutes
		constexpr int NightHours = 6;
		constexpr double NightGraceSeconds = 15.0;       // nobody moves in the first seconds of the night
		constexpr double PowerDrainPerSecond = 1.0 / 6.0; // % per second at usage 1 (FNAF-style: each door, light or the camera adds 1)
		constexpr double KuuWindupSeconds = 5.0;          // owner: Kuu winds up a punch for 5 seconds

		enum class ESide : int
		{
			Left = 0,
			Right = 1
		};

		enum class ENightChar : int
		{
			Luna = 0,   // like Bonnie: comes to the LEFT door
			Plack,      // like Chica: comes to the RIGHT door
			Kuu,        // goes to either door, winds up a punch for 5 s: a closed door breaks, an open doorway is a miss
			Media,      // teleports to a doorway with a panning sound: close that door within the window or lose
			Narrator,   // like Freddy: watching her camera pauses her; at the door she never leaves
			Count
		};
		constexpr int NightCharCount = static_cast<int>(ENightChar::Count);
		const char* NightCharName(ENightChar Char);
		const char* NightCharPortrait(ENightChar Char);   // lower-case operator name (portrait texture id)

		// Rooms. The ten cameras show rooms 0..9; the two door spots are only seen with that door's light.
		enum class ERoom : int
		{
			Stage = 0,
			Dining,
			Backroom,
			Storage,
			WestHall,
			WestCorner,
			Kitchen,
			Restrooms,
			EastHall,
			EastCorner,
			LeftDoor,
			RightDoor,
			Inside,     // got into the office
			Count
		};
		constexpr int CameraCount = 10;
		const char* RoomName(ERoom Room);
		// Camera map layout, in a 0..1 square (for the overlay's map), plus the connections drawn between rooms.
		void RoomMapPosition(ERoom Room, double& OutX, double& OutY);

		enum class ENightEventKind : int
		{
			Moved = 0,        // a character changed room (A = character, B = new room)
			MediaArrived,     // Media teleported to a doorway (A = side): play the panning cue
			MediaLeft,        // the door was closed in time (A = side)
			KuuWindup,        // Kuu started winding up at a door (A = side)
			KuuPunch,         // Kuu punched (A = side, B = 1 when the door broke, 0 when she hit nothing)
			DoorRepaired,     // (A = side)
			DoorBlocked,      // tried to close a broken door (A = side)
			Knock,            // a character hit a closed door and went away (A = side, B = character)
			HourChanged,      // (A = hour 1..6)
			PowerOut,
			Jumpscare,        // the night is lost (A = character)
			Survived          // 6 AM
		};

		struct FNightEvent
		{
			ENightEventKind Kind = ENightEventKind::Moved;
			int A = 0;
			int B = 0;
			double Time = 0.0;
		};

		class FNightShift
		{
		public:
			void Start(EDifficulty InDifficulty, uint32_t Seed);
			// Advances the night; call every frame. Internally steps at most 0.05 s at a time.
			void Tick(double DeltaSeconds);

			// ---- player controls
			bool ToggleDoor(ESide Side);           // false when the door is broken (it cannot close)
			bool SetDoor(ESide Side, bool bClosed);
			void SetLight(ESide Side, bool bOn);   // hold to see who is at the door
			void SetCamera(bool bUp);
			void ToggleCamera() { SetCamera(!bCameraUp); }
			void SelectCamera(int Camera);         // 0..9

			// ---- state for the screen
			EDifficulty GetDifficulty() const { return Difficulty; }
			const FNightRules& Rules() const { return R; }
			double Time() const { return Clock; }
			int Hour() const;                      // 0 = 12 AM .. 6 = 6 AM
			std::string ClockText() const;         // "12 AM", "1 AM", ...
			double Power() const { return PowerLeft; }
			int Usage() const;                     // 1..5 bars
			bool IsPowerOut() const { return bPowerOut; }
			bool IsDoorClosed(ESide Side) const { return Doors[SideIndex(Side)].bClosed; }
			bool IsDoorBroken(ESide Side) const { return Doors[SideIndex(Side)].RepairLeft > 0.0; }
			double DoorRepairLeft(ESide Side) const { return Doors[SideIndex(Side)].RepairLeft; }
			bool IsLightOn(ESide Side) const { return Lights[SideIndex(Side)]; }
			bool IsCameraUp() const { return bCameraUp; }
			int SelectedCamera() const { return Camera; }
			ERoom RoomOf(ENightChar Char) const { return Chars[CharIndex(Char)].Room; }
			bool IsOver() const { return bOver; }
			bool IsWon() const { return bWon; }
			ENightChar Killer() const { return KilledBy; }
			std::string LostReason() const { return Reason; }
			// Who the player can see right now: in the selected camera's room (camera up) or at a lit door.
			std::vector<ENightChar> Visible() const;
			std::vector<ENightChar> VisibleInRoom(ERoom Room) const;   // what the camera of that room shows (ignores the camera state)
			std::vector<ENightChar> AtDoor(ESide Side) const;          // who stands at that door (the light shows it)
			bool IsKuuWindingUp(ESide Side) const;                     // the thumping the player hears
			double KuuWindupLeft() const { return KuuWindup; }
			bool IsMediaAtDoor(ESide Side) const;
			double MediaTimeLeft() const { return MediaTimer; }
			bool IsNarratorWatched() const;
			// Events since the last call (sounds, messages). The overlay plays the Media cue panned to MediaArrived's side.
			std::vector<FNightEvent> PopEvents();

			// For tests and the balance bot.
			void ForceRoom(ENightChar Char, ERoom Room);
			void ForceMediaTeleport(ESide Side);
			int Opportunities() const { return OpportunityCount; }

		private:
			struct FDoor
			{
				bool bClosed = false;
				double RepairLeft = 0.0;
			};
			struct FChar
			{
				ERoom Room = ERoom::Stage;
				double NextOpportunity = 0.0;
				int Progress = 0;   // Narrator: successful rolls in this room; Media: charge
				ESide Side = ESide::Left;
			};

			static int SideIndex(ESide Side) { return Side == ESide::Left ? 0 : 1; }
			static int CharIndex(ENightChar Char) { return static_cast<int>(Char); }
			void Step(double Dt);
			void Opportunity(ENightChar Char);
			bool Roll(double Chance);
			ERoom Pick(const std::vector<std::pair<ERoom, double>>& Options);
			void MoveTo(ENightChar Char, ERoom Room);
			void Lose(ENightChar Char, const std::string& Why);
			void Emit(ENightEventKind Kind, int A = 0, int B = 0);
			ERoom DoorRoom(ESide Side) const { return Side == ESide::Left ? ERoom::LeftDoor : ERoom::RightDoor; }

			EDifficulty Difficulty = EDifficulty::Easy;
			FNightRules R;
			std::mt19937 Rng;
			double Clock = 0.0;
			double PowerLeft = 100.0;
			bool bPowerOut = false;
			double PowerOutDeath = 0.0;   // seconds after the power goes out until Narrator strikes
			FDoor Doors[2];
			bool Lights[2] = {false, false};
			bool bCameraUp = false;
			int Camera = 0;
			FChar Chars[NightCharCount];
			double KuuWindup = 0.0;       // > 0 while Kuu winds up at her door
			double KuuRest = 0.0;         // seconds Kuu waits in the back room after a punch
			bool bMediaAtDoor = false;
			double MediaTimer = 0.0;
			bool bOver = false;
			bool bWon = false;
			ENightChar KilledBy = ENightChar::Count;
			std::string Reason;
			int LastHour = 0;
			int OpportunityCount = 0;
			std::vector<FNightEvent> Events;
		};

		// Rewards for surviving a night (PLACEHOLDER), and a small consolation per hour survived.
		FReward NightReward(EDifficulty Difficulty, bool bWon, int HoursSurvived);

		// ================================================================ Rhythm
		constexpr int RhythmMaxLanes = 5;
		constexpr double RhythmPerfect = 0.050;   // seconds either side of the note (PLACEHOLDER windows)
		constexpr double RhythmGreat = 0.100;
		constexpr double RhythmGood = 0.150;
		constexpr double RhythmLeadIn = 2.0;      // seconds before the first note
		constexpr double RhythmScrollSeconds = 1.6; // how long a note is on screen before it reaches the line

		struct FSong
		{
			const char* Title;
			int Bpm;
			int Bars;      // 4/4 bars of notes
			double LaneHz[RhythmMaxLanes];   // the tone each lane plays when hit
		};
		int SongCount();
		const FSong& Song(int Index);
		int LanesFor(EDifficulty Difficulty);   // Easy 3, Medium 4, Hard 5

		struct FNote
		{
			double Time = 0.0;   // seconds from the start of the song
			int Lane = 0;
		};
		// Deterministic chart for a song and difficulty (same notes every time).
		std::vector<FNote> MakeChart(int SongIndex, EDifficulty Difficulty);
		double SongLength(int SongIndex);   // seconds including the lead-in and a short tail

		enum class EJudge : int
		{
			Perfect = 0,
			Great,
			Good,
			Miss,
			Count
		};
		const char* JudgeName(EJudge Judge);

		class FRhythm
		{
		public:
			void Start(int InSong, EDifficulty InDifficulty);
			void Tick(double DeltaSeconds);         // advances the song clock and marks notes that passed as misses
			// A lane key went down. Returns the judgement (Miss when nothing was close: an empty strum breaks the combo).
			EJudge Press(int Lane);

			double Time() const { return Clock; }
			int SongIndex() const { return SongIdx; }
			EDifficulty GetDifficulty() const { return Difficulty; }
			int Lanes() const { return LaneCount; }
			const std::vector<FNote>& Notes() const { return Chart; }
			bool IsHit(size_t NoteIndex) const { return NoteIndex < State.size() && State[NoteIndex] == 1; }
			bool IsMissed(size_t NoteIndex) const { return NoteIndex < State.size() && State[NoteIndex] == 2; }
			long long Score() const { return Points; }
			int Combo() const { return Streak; }
			int MaxCombo() const { return BestStreak; }
			int Multiplier() const;                 // 1..4: +1 every 10 notes in a row
			double Meter() const { return Health; } // 0..1: misses drain it; empty = the song fails
			int Count(EJudge Judge) const { return Counts[static_cast<int>(Judge)]; }
			double Accuracy() const;                // 0..1
			int Stars() const;                      // 0..5 at the end
			bool IsOver() const { return bOver; }
			bool IsPassed() const { return bOver && !bFailed; }
			bool IsFailed() const { return bFailed; }
			EJudge LastJudge() const { return Last; }
			double LastJudgeTime() const { return LastTime; }

		private:
			void Judge(EJudge Result);
			int SongIdx = 0;
			EDifficulty Difficulty = EDifficulty::Easy;
			int LaneCount = 3;
			std::vector<FNote> Chart;
			std::vector<uint8_t> State;   // 0 pending, 1 hit, 2 missed
			size_t FirstPending = 0;
			double Clock = 0.0;
			double Length = 0.0;
			long long Points = 0;
			int Streak = 0;
			int BestStreak = 0;
			double Health = 0.5;
			int Counts[static_cast<int>(EJudge::Count)] = {};
			bool bOver = false;
			bool bFailed = false;
			EJudge Last = EJudge::Count;
			double LastTime = -10.0;
		};

		FReward RhythmReward(EDifficulty Difficulty, bool bPassed, int Stars);

		// ============================================================== Boss Rush
		// Stage id 4,000,000 + base * 1000 + round: "base" is the campaign level the bosses are scaled from (the player's progress,
		// 1..104) and "round" counts the bosses beaten + 1. BIBGameMode chains the rounds in one arena.
		constexpr int BossRushStageBase = 4000000;
		constexpr int BossRushMaxRound = 999;
		int BossRushStageId(int BaseStage, int Round);
		int BossRushBaseOf(int StageId);
		int BossRushRoundOf(int StageId);
		FSectionPlanData BossRushRound(int Round);       // who fights in that round
		FEnemyScale BossRushScales(int StageId);         // the base stage's scale, +12% per round (PLACEHOLDER)
		FReward BossRushRoundReward(int Round);          // paid for every round cleared (PLACEHOLDER)
		constexpr double BossRushHealBetweenRounds = 0.35; // share of max health every operator regains before the next boss

		// ============================================================ the profile side
		// Starting a game spends a token; finishing pays. The night and the song are played by the overlay, which reports here.
		FActionResult StartGame(FProfile& Profile, EGame Game);
		std::string FinishNight(FProfile& Profile, const FNightShift& Night);
		std::string FinishRhythm(FProfile& Profile, const FRhythm& Song);
		// Boss Rush: a round cleared (reward lines), and the run over (best round, summary).
		std::vector<std::string> BossRushRoundCleared(FProfile& Profile, int Round);
		std::vector<std::string> BossRushOver(FProfile& Profile, int RoundReached);
	}
}
