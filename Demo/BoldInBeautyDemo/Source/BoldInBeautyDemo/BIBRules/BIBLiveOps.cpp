// Boldinbeauty demo — live operations on the real clock (see BIBLiveOps.h). Amounts are PLACEHOLDER unless noted.
#include "BIBLiveOps.h"

#include "BIBArena.h"
#include "BIBConfig.h"
#include "BIBCore.h"
#include "BIBInventory.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace BIB
{
	namespace LiveOps
	{
		namespace
		{
			constexpr int64_t Day = 86400;

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

			// Howard Hinnant's civil-from-days and days-from-civil (proleptic Gregorian, UTC).
			void CivilFromDays(int64_t Z, int& Year, int& Month, int& DayOfMonth)
			{
				Z += 719468;
				const int64_t Era = (Z >= 0 ? Z : Z - 146096) / 146097;
				const int64_t Doe = Z - Era * 146097;
				const int64_t Yoe = (Doe - Doe / 1460 + Doe / 36524 - Doe / 146096) / 365;
				const int64_t Doy = Doe - (365 * Yoe + Yoe / 4 - Yoe / 100);
				const int64_t Mp = (5 * Doy + 2) / 153;
				DayOfMonth = static_cast<int>(Doy - (153 * Mp + 2) / 5 + 1);
				Month = static_cast<int>(Mp < 10 ? Mp + 3 : Mp - 9);
				Year = static_cast<int>(Yoe + Era * 400 + (Month <= 2 ? 1 : 0));
			}

			int64_t DaysFromCivil(int Year, int Month, int DayOfMonth)
			{
				const int64_t Y = Year - (Month <= 2 ? 1 : 0);
				const int64_t Era = (Y >= 0 ? Y : Y - 399) / 400;
				const int64_t Yoe = Y - Era * 400;
				const int64_t Doy = (153 * (Month > 2 ? Month - 3 : Month + 9) + 2) / 5 + DayOfMonth - 1;
				const int64_t Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;
				return Era * 146097 + Doe - 719468;
			}

			// Returned quests (owner: in total 5 tickets, 600 Diamonds and some loot). Counters live in FReturnState.
			struct FReturnDef
			{
				const char* Title;
				int Target;
				FReward Reward;
			};

			const FReturnDef ReturnDefs[ReturnQuestCount] = {
				{"Claim a welcome-back check-in", 1, {.Diamonds = 100, .RegularTickets = 1}},
				{"Clear 3 campaign levels", 3, {.Gold = 5000, .Diamonds = 100}},
				{"Summon 5 times", 5, {.Diamonds = 100, .RegularTickets = 1}},
				{"Level up operators 10 times", 10, {.Bytes = 3000, .Hearts = 20, .Diamonds = 100}},
				{"Complete 5 daily quests", 5, {.Diamonds = 100, .RegularTickets = 1, .Equipment = 1}},
				{"Fight 3 times in the Ark (tower or arena)", 3, {.Gold = 10000, .Diamonds = 100, .RegularTickets = 2, .Equipment = 2}},
			};

			int ReturnProgress(const FProfile& Profile, int Index)
			{
				const FReturnState& R = Profile.Meta.Return;
				switch (Index)
				{
				case 0: return R.DaysClaimed != 0 ? 1 : 0;
				case 1: return R.StageClears;
				case 2: return R.Summons;
				case 3: return R.LevelUps;
				case 4: return R.DailyQuests;
				case 5: return R.ArkFights;
				default: return 0;
				}
			}

			FReward SumRewards(const FReward& A, const FReward& B)
			{
				FReward S = A;
				S.Gold += B.Gold;
				S.Bytes += B.Bytes;
				S.Hearts += B.Hearts;
				S.Diamonds += B.Diamonds;
				S.RegularTickets += B.RegularTickets;
				S.SpecialTickets += B.SpecialTickets;
				S.ArcadeTokens += B.ArcadeTokens;
				S.GangCoins += B.GangCoins;
				S.SeasonXp += B.SeasonXp;
				S.CommanderXp += B.CommanderXp;
				S.Equipment += B.Equipment;
				return S;
			}
		}

		// ===================================================================== calendar
		int UtcDayIndex(int64_t UnixSeconds)
		{
			return static_cast<int>((UnixSeconds >= 0 ? UnixSeconds : UnixSeconds - (Day - 1)) / Day);
		}

		int SeasonKeyOf(int64_t UnixSeconds)
		{
			int Year = 1970;
			int Month = 1;
			int DayOfMonth = 1;
			CivilFromDays(UtcDayIndex(UnixSeconds), Year, Month, DayOfMonth);
			return Year * 12 + Month - 1;
		}

		std::string SeasonLabel(int SeasonKey)
		{
			static const char* Months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
			if (SeasonKey <= 0)
			{
				return "-";
			}
			return std::string(Months[SeasonKey % 12]) + " " + std::to_string(SeasonKey / 12);
		}

		std::string SeasonCode(int SeasonKey)
		{
			char Buffer[16];
			std::snprintf(Buffer, sizeof(Buffer), "%04d-%02d", SeasonKey / 12, SeasonKey % 12 + 1);
			return Buffer;
		}

		int SeasonKeyFromCode(const std::string& Code)
		{
			if (Code.size() != 7 || Code[4] != '-')
			{
				return 0;
			}
			for (size_t Index = 0; Index < Code.size(); ++Index)
			{
				if (Index != 4 && (Code[Index] < '0' || Code[Index] > '9'))
				{
					return 0;
				}
			}
			const int Year = std::atoi(Code.substr(0, 4).c_str());
			const int Month = std::atoi(Code.substr(5, 2).c_str());
			return Month >= 1 && Month <= 12 ? Year * 12 + Month - 1 : 0;
		}

		int64_t SeasonStartUnix(int SeasonKey)
		{
			return DaysFromCivil(SeasonKey / 12, SeasonKey % 12 + 1, 1) * Day;
		}

		int64_t SeasonEndUnix(int SeasonKey)
		{
			return SeasonStartUnix(SeasonKey + 1);
		}

		int DaysLeftInSeason(int64_t UnixSeconds)
		{
			const int64_t End = SeasonEndUnix(SeasonKeyOf(UnixSeconds));
			return static_cast<int>(std::max<int64_t>(0, (End - UnixSeconds + Day - 1) / Day));
		}

		int SeasonPassDaysLeft(const FProfile& Profile)
		{
			return Profile.NowUnix > 0 ? DaysLeftInSeason(Profile.NowUnix) : 0;
		}

		// ===================================================================== the clock
		std::vector<std::string> UpdateClock(FProfile& Profile, int64_t NowUnix)
		{
			std::vector<std::string> Lines;
			if (NowUnix <= 0)
			{
				return Lines;
			}
			FMeta& M = Profile.Meta;
			const int Key = SeasonKeyOf(NowUnix);

			// Returning player: away 14 days or more (owner request). The Outer World produced meanwhile (real time since the
			// 2026-10-07 rebalance, Config::WorldHoursPerRealHour; capped by each building's storage).
			if (M.LastActiveUnix > 0 && NowUnix > M.LastActiveUnix)
			{
				const int64_t Away = NowUnix - M.LastActiveUnix;
				if (Away >= 60)
				{
					Meta::AdvanceWorld(Profile, static_cast<double>(Away) / 3600.0 * Config::WorldHoursPerRealHour);
				}
				if (Away >= ReturnAwayDays * Day && !Profile.TutorialActive())
				{
					StartReturn(Profile, NowUnix);
					Lines.push_back("Welcome back, Commander! A 7-day welcome-back event has started (Lobby > Welcome back).");
				}
			}
			if (M.Return.StartUnix > 0 && NowUnix >= M.Return.StartUnix + ReturnEventDays * Day)
			{
				M.Return = FReturnState();   // the event is over
			}

			// Daily reset on the UTC day (the demo's "Next day" tool keeps working on top of it).
			const int Today = UtcDayIndex(NowUnix);
			if (M.LastUtcDay == 0)
			{
				M.LastUtcDay = Today;
			}
			else if (Today > M.LastUtcDay)
			{
				const int Elapsed = Today - M.LastUtcDay;
				M.LastUtcDay = Today;
				Profile.Day += std::min(Elapsed, 3650) - 1;   // weeks keep counting; one reset, one supply mail
				Profile.AdvanceDay();
			}

			// Season pass: one calendar month. Unclaimed rewards of tiers already reached are mailed, then it starts over.
			if (M.SeasonPassKey == 0)
			{
				M.SeasonPassKey = Key;
			}
			else if (Key > M.SeasonPassKey)
			{
				FReward Unclaimed;
				int Tiers = 0;
				for (int Tier = 1; Tier <= Meta::SeasonTier(Profile); ++Tier)
				{
					if (!M.FreeClaimed[Tier])
					{
						Unclaimed = SumRewards(Unclaimed, Meta::SeasonReward(Tier, false));
						++Tiers;
					}
					if (M.bPremiumPass && !M.PremiumClaimed[Tier])
					{
						Unclaimed = SumRewards(Unclaimed, Meta::SeasonReward(Tier, true));
						++Tiers;
					}
				}
				if (Tiers > 0)
				{
					FMail Mail;
					Mail.Id = M.NextMailId++;
					Mail.Title = "Season pass " + SeasonLabel(M.SeasonPassKey) + ": unclaimed rewards";
					Mail.Body = "The season ended; here is everything you reached but had not claimed.";
					Mail.Reward = Unclaimed;
					M.Mail.push_back(Mail);
				}
				M.SeasonXp = 0;
				M.bPremiumPass = false;
				std::fill(std::begin(M.FreeClaimed), std::end(M.FreeClaimed), false);
				std::fill(std::begin(M.PremiumClaimed), std::end(M.PremiumClaimed), false);
				M.SeasonPassKey = Key;
				Lines.push_back("A new season pass has started (" + SeasonLabel(Key) + ").");
			}

			// Arena season: the server keeps it for signed-in players; guests keep their own copy.
			if (!Profile.Account.bSignedIn)
			{
				Arena::UpdateSeason(Profile, Key);
			}

			// The gang shop's monthly stock.
			if (M.Gang.ShopSeason != Key)
			{
				M.Gang.ShopSeason = Key;
				std::fill(std::begin(M.Gang.ShopBought), std::end(M.Gang.ShopBought), 0);
			}
			// Charge (stamina) refills on the real clock: 1 every 6 minutes up to the cap (BIBInventory.h).
			Inventory::RegenChargeTo(Profile, NowUnix);
			M.LastActiveUnix = NowUnix;
			return Lines;
		}

		// ===================================================================== returning players
		void StartReturn(FProfile& Profile, int64_t NowUnix)
		{
			Profile.Meta.Return = FReturnState();
			Profile.Meta.Return.StartUnix = NowUnix;
		}

		bool ReturnActive(const FProfile& Profile)
		{
			const FReturnState& R = Profile.Meta.Return;
			return R.StartUnix > 0 && Profile.NowUnix >= R.StartUnix && Profile.NowUnix < R.StartUnix + ReturnEventDays * Day;
		}

		int ReturnDay(const FProfile& Profile)
		{
			if (!ReturnActive(Profile))
			{
				return 0;
			}
			return std::clamp(static_cast<int>((Profile.NowUnix - Profile.Meta.Return.StartUnix) / Day) + 1, 1, ReturnEventDays);
		}

		int64_t ReturnSecondsLeft(const FProfile& Profile)
		{
			return ReturnActive(Profile) ? Profile.Meta.Return.StartUnix + ReturnEventDays * Day - Profile.NowUnix : 0;
		}

		FReward ReturnCheckInReward(int DayNumber)
		{
			static const FReward Rewards[ReturnEventDays] = {
				{.Gold = 5000, .Bytes = 1500, .Hearts = 10},
				{.Gold = 6000, .ArcadeTokens = 3, .Equipment = 1},
				{.Gold = 4000, .Bytes = 3000, .Hearts = 15},
				{.Gold = 8000, .Bytes = 2500, .Equipment = 1},
				{.Gold = 6000, .Hearts = 20, .ArcadeTokens = 3},
				{.Gold = 10000, .Bytes = 3500, .Hearts = 20},
				{.Gold = 15000, .Bytes = 5000, .Equipment = 2},
			};
			return Rewards[std::clamp(DayNumber, 1, ReturnEventDays) - 1];
		}

		bool ReturnCheckInClaimed(const FProfile& Profile, int DayNumber)
		{
			return DayNumber >= 1 && DayNumber <= ReturnEventDays && (Profile.Meta.Return.DaysClaimed & (1 << (DayNumber - 1))) != 0;
		}

		FActionResult ClaimReturnCheckIn(FProfile& Profile, int DayNumber)
		{
			if (!ReturnActive(Profile))
			{
				return Fail("No welcome-back event is running.");
			}
			if (DayNumber < 1 || DayNumber > ReturnDay(Profile))
			{
				return Fail("Day " + std::to_string(DayNumber) + " is not open yet.");
			}
			if (ReturnCheckInClaimed(Profile, DayNumber))
			{
				return Fail("Already claimed.");
			}
			Profile.Meta.Return.DaysClaimed |= 1 << (DayNumber - 1);
			return Ok("Welcome back, day " + std::to_string(DayNumber) + ": " + Meta::ApplyReward(Profile, ReturnCheckInReward(DayNumber)));
		}

		FReturnQuest ReturnQuest(const FProfile& Profile, int Index)
		{
			FReturnQuest Quest;
			if (Index < 0 || Index >= ReturnQuestCount)
			{
				return Quest;
			}
			const FReturnDef& Def = ReturnDefs[Index];
			Quest.Title = Def.Title;
			Quest.Target = Def.Target;
			Quest.Progress = std::min(Def.Target, ReturnProgress(Profile, Index));
			Quest.bClaimed = Profile.Meta.Return.QuestClaimed[Index];
			Quest.bComplete = Quest.Progress >= Def.Target;
			Quest.Reward = Def.Reward;
			return Quest;
		}

		FActionResult ClaimReturnQuest(FProfile& Profile, int Index)
		{
			if (!ReturnActive(Profile))
			{
				return Fail("No welcome-back event is running.");
			}
			const FReturnQuest Quest = ReturnQuest(Profile, Index);
			if (Quest.Title.empty())
			{
				return Fail("Unknown quest.");
			}
			if (Quest.bClaimed)
			{
				return Fail("Already claimed.");
			}
			if (!Quest.bComplete)
			{
				return Fail("Quest not complete yet.");
			}
			Profile.Meta.Return.QuestClaimed[Index] = true;
			return Ok("Returned quest: " + Quest.Title + " - " + Meta::ApplyReward(Profile, Quest.Reward));
		}

		FReward ReturnQuestTotal()
		{
			FReward Total;
			for (const FReturnDef& Def : ReturnDefs)
			{
				Total = SumRewards(Total, Def.Reward);
			}
			return Total;
		}

		int ClaimableReturn(const FProfile& Profile)
		{
			if (!ReturnActive(Profile))
			{
				return 0;
			}
			int Count = 0;
			for (int D = 1; D <= ReturnDay(Profile); ++D)
			{
				Count += ReturnCheckInClaimed(Profile, D) ? 0 : 1;
			}
			for (int Index = 0; Index < ReturnQuestCount; ++Index)
			{
				const FReturnQuest Quest = ReturnQuest(Profile, Index);
				Count += Quest.bComplete && !Quest.bClaimed ? 1 : 0;
			}
			return Count;
		}
	}
}
