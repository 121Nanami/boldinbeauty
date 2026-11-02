// Boldinbeauty demo — Battle power and the Ark arena (owner request 2026-10-06). Engine-independent.
//
// Battle power: one number per operator that sums up how strong it is (like Goddess of Victory: Nikke's power number),
// shown next to operators and used for arena matchmaking. Formula PLACEHOLDER: health, attack and defence after levels,
// upgrade stars and equipment, weighted by rarity.
//
// Arena (semi-online): the player sets an Attack team and a Defence team. Pressing Fight matches the attack team against
// another player's defence (the account server picks it: similar strength first, then similar rank) and both sides are
// played by the AI. The winner takes trophies from the loser. Ranks: Bronze 1-3, Silver 1-3, Gold 1-3, Diamond 1-3,
// Platinum 1-3 and Champions (owner's order); 100 trophies a rank (PLACEHOLDER). A season is a calendar month: at the end
// everyone drops 3-4 ranks; a Champions player cannot fall below Champions during the season. Guests (no server) fight
// generated rivals of similar strength with the same rules.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BIBMeta.h"

namespace BIB
{
	class FProfile;
	struct FActionResult;

	// ------------------------------------------------------------------ battle power
	int OperatorPower(const FProfile& Profile, int RosterIndex);   // 0 when not owned
	int SquadPower(const FProfile& Profile, const int Team[3]);
	int TeamBattlePower(const FProfile& Profile);                 // the squad on the Team tab

	namespace Arena
	{
		constexpr int RankCount = 16;              // 15 divisions + Champions
		constexpr int TrophiesPerRank = 100;       // PLACEHOLDER
		constexpr int ChampionRank = 15;
		constexpr int ChampionTrophies = ChampionRank * TrophiesPerRank;
		constexpr int DailyAttempts = 5;           // PLACEHOLDER (the old arena's number)

		int RankOf(int Trophies);
		std::string RankName(int Rank);            // "Bronze 1" .. "Platinum 3", "Champions"
		const char* RankTier(int Rank);            // "Bronze" .. "Champions"
		int TrophyDelta(int WinnerTrophies, int LoserTrophies);   // zero-sum, 10..50 (same formula as the server)
		int RankAfterSeason(int Rank);             // drop 3 ranks, 4 from Diamond 1 upward
		FReward SeasonReward(int Rank);            // paid once for the rank held when a season ends (PLACEHOLDER)
		// Applies a trophy change with the floors: never below 0; a Champions player never below Champions this season.
		int ApplyTrophies(int Trophies, int Delta, bool& bChampionFloor);

		// ------------------------------------------------------------- lineups
		struct FFighter
		{
			int Op = -1;            // roster index
			int Level = 1;
			int Stars = 0;          // upgrade stars
			double Hp = 0.0;        // max health
			double Attack = 0.0;    // regular attack after multipliers
			double Defence = 0.0;   // % damage reduction
			int Power = 0;
			int Attunement = 0;     // Element Attunement (BIBElement.h): raises the advantage bonus; the element follows Op
		};

		struct FLineup
		{
			std::string Uid;        // "" for a generated rival
			std::string Name;
			int Trophies = 0;
			int Power = 0;
			bool bBot = false;
			std::vector<FFighter> Team;
		};

		FFighter MakeFighter(const FProfile& Profile, int RosterIndex);
		FLineup MakeLineup(const FProfile& Profile, const int Team[3], const std::string& Name);
		// A generated rival for guests / an empty server: power within about 10% of the player's, trophies close to theirs.
		FLineup MakeRival(const FProfile& Profile, uint32_t Seed);

		// ------------------------------------------------------------- the fight
		enum class EAction : int
		{
			Attack = 0,
			Special,
			Ultimate
		};

		struct FBattleEvent
		{
			double Time = 0.0;
			int Side = 0;           // 0 = attacker (the player), 1 = defender
			int Actor = 0;          // index in that side's team
			int Target = 0;         // index in the other side's team (-1 = everyone: an ultimate)
			EAction Action = EAction::Attack;
			double Damage = 0.0;
			bool bKill = false;
		};

		struct FBattle
		{
			std::vector<FBattleEvent> Events;
			bool bAttackerWon = false;
			double Duration = 0.0;
			std::vector<double> HpLeft[2];   // per fighter at the end
		};

		constexpr double BattleTimeLimit = 60.0;   // seconds of simulated time; on time-out the side with more health left (%) wins
		// Both sides are played by the AI: each fighter attacks on its own rhythm, every fourth action is a Special, and an
		// Ultimate (all enemies) fires when its charge (from damage dealt and taken) is full. Deterministic for a seed.
		FBattle Simulate(const FLineup& Attacker, const FLineup& Defender, uint32_t Seed);
		std::string ActionName(EAction Action);

		// ------------------------------------------------------------- the profile side
		bool TeamIsValid(const FProfile& Profile, const int Team[3]);
		FActionResult SetSlot(FProfile& Profile, bool bDefence, int Slot, int RosterIndex);  // RosterIndex -1 empties the slot
		FActionResult AutoFill(FProfile& Profile, bool bDefence);   // the strongest owned operators
		// Season bookkeeping for the local copy (guests; signed-in players get the same from the server).
		void UpdateSeason(FProfile& Profile, int SeasonKey);
		FActionResult ClaimSeasonReward(FProfile& Profile);
		// Records a finished fight (attempt spent, trophies moved, rewards paid). Delta is positive for a win.
		std::string RecordFight(FProfile& Profile, const FLineup& Opponent, bool bWon, int NewTrophies, int Delta);
		FReward FightReward(bool bWon);
	}
}
