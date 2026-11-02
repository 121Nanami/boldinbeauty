// Boldinbeauty demo — battle power and the arena (see BIBArena.h). Every number is PLACEHOLDER unless noted.
#include "BIBArena.h"
#include "BIBElement.h"
#include "BIBEvents.h"

#include "BIBConfig.h"
#include "BIBCore.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace BIB
{
	namespace
	{
		double RarityFactor(int Stars)
		{
			switch (Stars)
			{
			case 6: return 1.4;
			case 5: return 1.25;
			case 4: return 1.1;
			default: return 1.0;
			}
		}

		int PowerOf(double Hp, double Attack, double Defence, int Stars)
		{
			const double Raw = 10.0 * (Hp * 0.08 + Attack * 1.2) * (1.0 + Defence / 100.0) * RarityFactor(Stars);
			return static_cast<int>(std::min(2.0e9, std::round(Raw)));
		}

		FActionResult Ok(const std::string& Message)
		{
			FActionResult Result;
			Result.bOk = true;
			Result.Message = Message;
			return Result;
		}

		FActionResult Fail(const std::string& Message)
		{
			FActionResult Result;
			Result.Message = Message;
			return Result;
		}

		bool Playable(const FProfile& Profile, int RosterIndex)
		{
			return RosterIndex >= 0 && RosterIndex < static_cast<int>(Roster().size()) && Profile.Owns(RosterIndex)
				&& Roster()[static_cast<size_t>(RosterIndex)].Kit != EKit::None;
		}
	}

	// =============================================================================== battle power
	int OperatorPower(const FProfile& Profile, int RosterIndex)
	{
		if (!Playable(Profile, RosterIndex))
		{
			return 0;
		}
		const FRosterEntry& Entry = Roster()[static_cast<size_t>(RosterIndex)];
		const FCombatStats Stats = Profile.GetCombatStats(RosterIndex);
		const int Raw = PowerOf(Stats.MaxHP, Entry.BaseAttack * Stats.DamageMult, Stats.DefencePercent, Entry.Stars);
		// Element Attunement adds 2% a level (owner request 2026-10-07, PLACEHOLDER).
		return static_cast<int>(std::min(2.0e9, std::round(Raw * (1.0 + Element::AttunementPowerPerLevel * Element::Attunement(Profile, RosterIndex)))));
	}

	int SquadPower(const FProfile& Profile, const int Team[3])
	{
		long long Total = 0;
		for (int Slot = 0; Slot < 3; ++Slot)
		{
			Total += OperatorPower(Profile, Team[Slot]);
		}
		return static_cast<int>(std::min<long long>(Total, 2000000000LL));
	}

	int TeamBattlePower(const FProfile& Profile)
	{
		return SquadPower(Profile, Profile.Team);
	}

	namespace Arena
	{
		// ============================================================================ ranks
		int RankOf(int Trophies)
		{
			return std::clamp(Trophies / TrophiesPerRank, 0, ChampionRank);
		}

		const char* RankTier(int Rank)
		{
			static const char* Tiers[6] = {"Bronze", "Silver", "Gold", "Diamond", "Platinum", "Champions"};
			return Tiers[std::clamp(Rank, 0, ChampionRank) / 3];
		}

		std::string RankName(int Rank)
		{
			const int R = std::clamp(Rank, 0, ChampionRank);
			if (R == ChampionRank)
			{
				return "Champions";
			}
			return std::string(RankTier(R)) + " " + std::to_string(R % 3 + 1);
		}

		int TrophyDelta(int WinnerTrophies, int LoserTrophies)
		{
			const double Raw = 30.0 + (LoserTrophies - WinnerTrophies) / 25.0;
			return std::clamp(static_cast<int>(std::lround(Raw)), 10, 50);
		}

		int RankAfterSeason(int Rank)
		{
			const int R = std::clamp(Rank, 0, ChampionRank);
			return std::max(0, R - (R >= 9 ? 4 : 3));
		}

		FReward SeasonReward(int Rank)
		{
			const int R = std::clamp(Rank, 0, ChampionRank);
			FReward Reward;
			Reward.Diamonds = 50 + 20 * R;
			Reward.Gold = 5000 + 2000 * R;
			Reward.RegularTickets = R >= ChampionRank ? 3 : (R >= 12 ? 2 : (R >= 6 ? 1 : 0));
			Reward.SpecialTickets = R >= ChampionRank ? 1 : 0;
			Reward.ArkMedals = 200 + 50 * R;   // 2026-10-07: the Ark shop (PLACEHOLDER)
			return Reward;
		}

		int ApplyTrophies(int Trophies, int Delta, bool& bChampionFloor)
		{
			int Next = std::max(0, Trophies + Delta);
			if (Next >= ChampionTrophies)
			{
				bChampionFloor = true;
			}
			if (bChampionFloor)
			{
				Next = std::max(Next, ChampionTrophies);
			}
			return Next;
		}

		// ============================================================================ lineups
		FFighter MakeFighter(const FProfile& Profile, int RosterIndex)
		{
			FFighter Fighter;
			if (!Playable(Profile, RosterIndex))
			{
				return Fighter;
			}
			const FRosterEntry& Entry = Roster()[static_cast<size_t>(RosterIndex)];
			const FCombatStats Stats = Profile.GetCombatStats(RosterIndex);
			Fighter.Op = RosterIndex;
			Fighter.Level = Profile.Operators[static_cast<size_t>(RosterIndex)].Level;
			Fighter.Stars = Profile.Operators[static_cast<size_t>(RosterIndex)].UpgradeStars;
			Fighter.Hp = Stats.MaxHP;
			Fighter.Attack = Entry.BaseAttack * Stats.DamageMult;
			Fighter.Defence = Stats.DefencePercent;
			Fighter.Power = OperatorPower(Profile, RosterIndex);
			Fighter.Attunement = Element::Attunement(Profile, RosterIndex);
			return Fighter;
		}

		FLineup MakeLineup(const FProfile& Profile, const int Team[3], const std::string& Name)
		{
			FLineup Lineup;
			Lineup.Name = Name;
			Lineup.Uid = Profile.Account.UserId;
			Lineup.Trophies = Profile.Meta.Arena.Trophies;
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				const FFighter Fighter = MakeFighter(Profile, Team[Slot]);
				if (Fighter.Op >= 0)
				{
					Lineup.Team.push_back(Fighter);
					Lineup.Power += Fighter.Power;
				}
			}
			return Lineup;
		}

		FLineup MakeRival(const FProfile& Profile, uint32_t Seed)
		{
			std::mt19937 Rng(Seed);
			FLineup Rival;
			Rival.bBot = true;
			Rival.Name = "Commander " + std::to_string(std::uniform_int_distribution<int>(1000, 9999)(Rng));
			const int Mine = std::max(3000, SquadPower(Profile, Profile.Meta.Arena.Attack));
			const double Target = Mine * std::uniform_real_distribution<double>(0.9, 1.1)(Rng);
			std::vector<int> Pool;
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				if (Roster()[static_cast<size_t>(Index)].Kit != EKit::None)
				{
					Pool.push_back(Index);
				}
			}
			// Several random trios: keep the one whose level-1 power is closest under the target (levels then close the gap).
			auto BaseOf = [](const std::vector<int>& Trio)
			{
				double Sum = 0.0;
				for (int Slot = 0; Slot < 3 && Slot < static_cast<int>(Trio.size()); ++Slot)
				{
					const FRosterEntry& Entry = Roster()[static_cast<size_t>(Trio[static_cast<size_t>(Slot)])];
					Sum += PowerOf(Entry.BaseHP, Entry.BaseAttack, 0.0, Entry.Stars);
				}
				return Sum;
			};
			std::vector<int> Best;
			double BestBase = 0.0;
			for (int Try = 0; Try < 40; ++Try)
			{
				std::shuffle(Pool.begin(), Pool.end(), Rng);
				const std::vector<int> Trio(Pool.begin(), Pool.begin() + std::min<size_t>(3, Pool.size()));
				const double TrioBase = BaseOf(Trio);
				const bool bUnder = TrioBase <= Target;
				const bool bBestUnder = !Best.empty() && BestBase <= Target;
				if (Best.empty() || (bUnder && (!bBestUnder || TrioBase > BestBase)) || (!bUnder && !bBestUnder && TrioBase < BestBase))
				{
					Best = Trio;
					BestBase = TrioBase;
				}
			}
			Pool = Best;
			const double Base = BestBase;
			// Power grows linearly with the level multiplier, so one level for the whole team hits the target.
			const double Mult = std::max(1.0, Target / std::max(1.0, Base));
			const int Level = std::clamp(1 + static_cast<int>(std::round((Mult - 1.0) * 100.0 / Config::StatPercentPerLevel)), 1, 9999);
			const double LevelMult = 1.0 + (Level - 1) * Config::StatPercentPerLevel / 100.0;
			for (int Slot = 0; Slot < 3 && Slot < static_cast<int>(Pool.size()); ++Slot)
			{
				const int Op = Pool[static_cast<size_t>(Slot)];
				const FRosterEntry& Entry = Roster()[static_cast<size_t>(Op)];
				FFighter Fighter;
				Fighter.Op = Op;
				Fighter.Level = Level;
				Fighter.Hp = Entry.BaseHP * LevelMult;
				Fighter.Attack = Entry.BaseAttack * LevelMult;
				Fighter.Power = PowerOf(Fighter.Hp, Fighter.Attack, 0.0, Entry.Stars);
				Rival.Power += Fighter.Power;
				Rival.Team.push_back(Fighter);
			}
			Rival.Trophies = std::max(0, Profile.Meta.Arena.Trophies + std::uniform_int_distribution<int>(-60, 60)(Rng));
			return Rival;
		}

		// ============================================================================ the fight
		std::string ActionName(EAction Action)
		{
			switch (Action)
			{
			case EAction::Attack: return "attacks";
			case EAction::Special: return "uses a Special on";
			case EAction::Ultimate: return "unleashes an Ultimate";
			}
			return "";
		}

		FBattle Simulate(const FLineup& Attacker, const FLineup& Defender, uint32_t Seed)
		{
			struct FUnit
			{
				double Hp = 0.0;
				double MaxHp = 1.0;
				double Attack = 0.0;
				double Defence = 0.0;
				double Next = 0.0;
				double Interval = 1.5;
				double Charge = 0.0;
				int Actions = 0;
				EElement Element = EElement::None;
				double ElementBonus = 0.0;   // advantage bonus of this unit's hits (element system, 2026-10-07)
			};
			std::mt19937 Rng(Seed);
			std::uniform_real_distribution<double> U(0.0, 1.0);
			std::vector<FUnit> Units[2];
			const FLineup* Sides[2] = {&Attacker, &Defender};
			for (int Side = 0; Side < 2; ++Side)
			{
				for (const FFighter& Fighter : Sides[Side]->Team)
				{
					FUnit Unit;
					Unit.MaxHp = std::max(1.0, Fighter.Hp);
					Unit.Hp = Unit.MaxHp;
					Unit.Attack = std::max(1.0, Fighter.Attack);
					Unit.Defence = std::clamp(Fighter.Defence, 0.0, Config::DefenceCapPercent);
					Unit.Element = Element::OfOperator(Fighter.Op);
					Unit.ElementBonus = Element::AdvantageBonus + Element::AttunementBonusPerLevel * std::clamp(Fighter.Attunement, 0, Element::MaxAttunement);
					Unit.Interval = 1.3 + 0.4 * U(Rng);
					Unit.Next = 0.6 + Unit.Interval * U(Rng);
					Units[Side].push_back(Unit);
				}
			}
			auto Alive = [&Units](int Side)
			{
				int Count = 0;
				for (const FUnit& Unit : Units[Side])
				{
					Count += Unit.Hp > 0.0 ? 1 : 0;
				}
				return Count;
			};
			auto HealthShare = [&Units](int Side)
			{
				double Left = 0.0;
				double Max = 0.0;
				for (const FUnit& Unit : Units[Side])
				{
					Left += std::max(0.0, Unit.Hp);
					Max += Unit.MaxHp;
				}
				return Max > 0.0 ? Left / Max : 0.0;
			};

			FBattle Battle;
			double Clock = 0.0;
			while (Alive(0) > 0 && Alive(1) > 0 && Clock < BattleTimeLimit)
			{
				// The next fighter to act, across both sides.
				int Side = -1;
				int Actor = -1;
				double Soonest = BattleTimeLimit + 1.0;
				for (int S = 0; S < 2; ++S)
				{
					for (size_t I = 0; I < Units[S].size(); ++I)
					{
						if (Units[S][I].Hp > 0.0 && Units[S][I].Next < Soonest)
						{
							Soonest = Units[S][I].Next;
							Side = S;
							Actor = static_cast<int>(I);
						}
					}
				}
				if (Side < 0)
				{
					break;
				}
				Clock = Soonest;
				if (Clock >= BattleTimeLimit)
				{
					break;
				}
				FUnit& Unit = Units[Side][static_cast<size_t>(Actor)];
				Unit.Next += Unit.Interval;
				++Unit.Actions;
				const int Foe = 1 - Side;
				EAction Action = EAction::Attack;
				double Mult = 1.0;
				if (Unit.Charge >= 100.0)
				{
					Action = EAction::Ultimate;
					Mult = 2.5;
					Unit.Charge = 0.0;
				}
				else if (Unit.Actions % 4 == 0)
				{
					Action = EAction::Special;
					Mult = 2.2;
				}
				// Targets: an ultimate hits everyone; otherwise the weakest standing enemy (most of the time).
				std::vector<int> Targets;
				if (Action == EAction::Ultimate)
				{
					for (size_t I = 0; I < Units[Foe].size(); ++I)
					{
						if (Units[Foe][I].Hp > 0.0)
						{
							Targets.push_back(static_cast<int>(I));
						}
					}
				}
				else
				{
					int Pick = -1;
					const bool bFocus = U(Rng) < 0.7;
					int Seen = 0;
					for (size_t I = 0; I < Units[Foe].size(); ++I)
					{
						const FUnit& Enemy = Units[Foe][I];
						if (Enemy.Hp <= 0.0)
						{
							continue;
						}
						++Seen;
						if (Pick < 0 || (bFocus ? Enemy.Hp < Units[Foe][static_cast<size_t>(Pick)].Hp : U(Rng) < 1.0 / Seen))
						{
							Pick = static_cast<int>(I);
						}
					}
					Targets.push_back(Pick);
				}
				for (int Target : Targets)
				{
					FUnit& Enemy = Units[Foe][static_cast<size_t>(Target)];
					const double Advantage = Element::HasAdvantage(Unit.Element, Enemy.Element) ? 1.0 + Unit.ElementBonus : 1.0;
					const double Damage = Unit.Attack * Mult * Advantage * (0.9 + 0.2 * U(Rng)) * (1.0 - Enemy.Defence / 100.0);
					Enemy.Hp -= Damage;
					Enemy.Charge += 8.0;
					FBattleEvent Event;
					Event.Time = Clock;
					Event.Side = Side;
					Event.Actor = Actor;
					Event.Target = Action == EAction::Ultimate ? -1 : Target;
					Event.Action = Action;
					Event.Damage = Damage;
					Event.bKill = Enemy.Hp <= 0.0;
					if (Action == EAction::Ultimate)
					{
						Event.Target = Target;   // one event per enemy hit, all at the same time
					}
					Battle.Events.push_back(Event);
				}
				Unit.Charge += 14.0;
			}
			const int AliveA = Alive(0);
			const int AliveD = Alive(1);
			Battle.bAttackerWon = AliveD == 0 ? AliveA > 0 : (AliveA == 0 ? false : HealthShare(0) > HealthShare(1));
			Battle.Duration = std::min(Clock, BattleTimeLimit);
			for (int Side = 0; Side < 2; ++Side)
			{
				for (const FUnit& Unit : Units[Side])
				{
					Battle.HpLeft[Side].push_back(std::max(0.0, Unit.Hp));
				}
			}
			return Battle;
		}

		// ============================================================================ profile side
		bool TeamIsValid(const FProfile& Profile, const int Team[3])
		{
			int Filled = 0;
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				if (Team[Slot] < 0)
				{
					continue;
				}
				if (!Playable(Profile, Team[Slot]))
				{
					return false;
				}
				for (int Other = 0; Other < Slot; ++Other)
				{
					if (Team[Other] == Team[Slot])
					{
						return false;
					}
				}
				++Filled;
			}
			return Filled >= std::min(3, Profile.PlayableOwnedCount()) && Filled > 0;
		}

		FActionResult SetSlot(FProfile& Profile, bool bDefence, int Slot, int RosterIndex)
		{
			if (Slot < 0 || Slot > 2)
			{
				return Fail("No such slot.");
			}
			int* Team = bDefence ? Profile.Meta.Arena.Defence : Profile.Meta.Arena.Attack;
			if (RosterIndex >= 0 && !Playable(Profile, RosterIndex))
			{
				return Fail("You do not own that operator.");
			}
			for (int Other = 0; Other < 3; ++Other)
			{
				if (Other != Slot && Team[Other] == RosterIndex && RosterIndex >= 0)
				{
					Team[Other] = Team[Slot];   // swap instead of a duplicate
				}
			}
			Team[Slot] = RosterIndex;
			return Ok(std::string(bDefence ? "Defence" : "Attack") + " slot " + std::to_string(Slot + 1) + ": "
				+ (RosterIndex >= 0 ? Roster()[static_cast<size_t>(RosterIndex)].Name : "empty") + ".");
		}

		FActionResult AutoFill(FProfile& Profile, bool bDefence)
		{
			std::vector<int> Owned;
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				if (Playable(Profile, Index))
				{
					Owned.push_back(Index);
				}
			}
			if (Owned.empty())
			{
				return Fail("You have no operators yet.");
			}
			std::stable_sort(Owned.begin(), Owned.end(), [&Profile](int A, int B) { return OperatorPower(Profile, A) > OperatorPower(Profile, B); });
			int* Team = bDefence ? Profile.Meta.Arena.Defence : Profile.Meta.Arena.Attack;
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				Team[Slot] = Slot < static_cast<int>(Owned.size()) ? Owned[static_cast<size_t>(Slot)] : -1;
			}
			return Ok(std::string(bDefence ? "Defence" : "Attack") + " team filled with your strongest operators (power "
				+ Meta::Num(SquadPower(Profile, Team)) + ").");
		}

		void UpdateSeason(FProfile& Profile, int SeasonKey)
		{
			FArenaState& A = Profile.Meta.Arena;
			if (SeasonKey <= 0)
			{
				return;
			}
			if (A.SeasonKey == 0)
			{
				A.SeasonKey = SeasonKey;
				return;
			}
			if (SeasonKey <= A.SeasonKey)
			{
				return;
			}
			const int Rank = RankOf(A.Trophies);
			if (A.Trophies > 0 || Profile.Meta.ArenaWins + Profile.Meta.ArenaLosses > 0)
			{
				A.PendingRewardRank = Rank;
				A.PendingRewardSeason = A.SeasonKey;
			}
			A.Trophies = RankAfterSeason(Rank) * TrophiesPerRank;
			A.bChampionFloor = false;
			A.SeasonKey = SeasonKey;
		}

		FActionResult ClaimSeasonReward(FProfile& Profile)
		{
			FArenaState& A = Profile.Meta.Arena;
			if (A.PendingRewardRank < 0)
			{
				return Fail("No arena season reward to claim.");
			}
			const int Rank = A.PendingRewardRank;
			A.PendingRewardRank = -1;
			return Ok("Arena season reward (" + RankName(Rank) + "): " + Meta::ApplyReward(Profile, SeasonReward(Rank)));
		}

		FReward FightReward(bool bWon)
		{
			FReward Reward;
			Reward.Gold = bWon ? 1500 : 300;
			Reward.Bytes = bWon ? 500 : 0;
			// Classic arena pay (2026-10-07 rebalance, PLACEHOLDER): a few Diamonds for a win and Ark medals for the Ark shop.
			Reward.Diamonds = bWon ? 10 : 0;
			Reward.ArkMedals = Events::ArkMedalsForArena(bWon);
			return Reward;
		}

		std::string RecordFight(FProfile& Profile, const FLineup& Opponent, bool bWon, int NewTrophies, int Delta)
		{
			FMeta& M = Profile.Meta;
			M.ArenaAttempts = std::max(0, M.ArenaAttempts - 1);
			(bWon ? M.ArenaWins : M.ArenaLosses) += 1;
			const int Before = RankOf(M.Arena.Trophies);
			M.Arena.Trophies = std::max(0, NewTrophies);
			if (M.Arena.Trophies >= ChampionTrophies)
			{
				M.Arena.bChampionFloor = true;
			}
			const int After = RankOf(M.Arena.Trophies);
			std::string Text = std::string(bWon ? "VICTORY" : "DEFEAT") + " vs " + Opponent.Name + " (power " + Meta::Num(Opponent.Power) + "): "
				+ (Delta >= 0 ? "+" : "") + std::to_string(Delta) + " trophies -> " + Meta::Num(M.Arena.Trophies) + " (" + RankName(After) + ")";
			if (After != Before)
			{
				Text += After > Before ? " - promoted!" : " - demoted.";
			}
			Text += " Reward: " + Meta::ApplyReward(Profile, FightReward(bWon));
			M.ArenaLog = Text;
			Meta::NotifyQuest(Profile, Meta::EQuestEvent::ArkPlay);
			return Text;
		}
	}
}
