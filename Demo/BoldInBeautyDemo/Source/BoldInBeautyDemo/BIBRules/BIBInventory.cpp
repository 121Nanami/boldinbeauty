// Boldinbeauty demo — the inventory: boosts, Charge and the Monthly supply (see BIBInventory.h). Amounts are PLACEHOLDER.
#include "BIBInventory.h"

#include "BIBCampaign.h"
#include "BIBCore.h"
#include "BIBEvents.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Inventory
	{
		namespace
		{
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

			int Index(EItem Item)
			{
				return std::clamp(static_cast<int>(Item), 0, ItemCount - 1);
			}

			const char* ResourceName(int Resource)
			{
				return Resource == 0 ? "Gold" : (Resource == 1 ? "Bytes" : "Angel hearts");
			}
		}

		// ===================================================================== items
		const char* ItemName(EItem Item)
		{
			static const char* Names[ItemCount] = {
				"Gold boost (3 h)", "Gold boost (6 h)", "Gold boost (12 h)", "Gold boost (24 h)",
				"Bytes boost (3 h)", "Bytes boost (6 h)", "Bytes boost (12 h)", "Bytes boost (24 h)",
				"Angel-heart boost (3 h)", "Angel-heart boost (6 h)", "Angel-heart boost (12 h)", "Angel-heart boost (24 h)",
				"Charge pack",
			};
			return Names[Index(Item)];
		}

		const char* ItemIcon(EItem Item)
		{
			switch (BoostResource(Item))
			{
			case 0: return "boost_gold";
			case 1: return "boost_bytes";
			case 2: return "boost_hearts";
			default: return "charge";
			}
		}

		bool IsBoost(EItem Item)
		{
			return Index(Item) < static_cast<int>(EItem::ChargePack);
		}

		int BoostResource(EItem Item)
		{
			return IsBoost(Item) ? Index(Item) / 4 : -1;
		}

		int BoostHoursOf(EItem Item)
		{
			return IsBoost(Item) ? BoostHours[Index(Item) % 4] : 0;
		}

		EItem BoostItem(int Resource, int HoursIndex)
		{
			return static_cast<EItem>(std::clamp(Resource, 0, 2) * 4 + std::clamp(HoursIndex, 0, 3));
		}

		std::string ItemUse(EItem Item)
		{
			if (Item == EItem::ChargePack)
			{
				return "+" + std::to_string(ChargePackAmount) + " Charge (may go above the " + std::to_string(ChargeCap) + " cap).";
			}
			return "Pays " + std::to_string(BoostHoursOf(Item)) + " hours of the Outer World's " + ResourceName(BoostResource(Item)) +
				" production at once.";
		}

		double AfkRatePerHour(const FProfile& Profile, int Resource)
		{
			double Rate = 0.0;
			for (int Building = 0; Building < BuildingCount; ++Building)
			{
				Rate += Meta::BuildingRate(Profile, Building, Resource);
			}
			return Rate;
		}

		long long BoostValue(const FProfile& Profile, EItem Item)
		{
			const int Resource = BoostResource(Item);
			if (Resource < 0)
			{
				return Item == EItem::ChargePack ? ChargePackAmount : 0;
			}
			const double Floor = Resource == 0 ? MinGoldPerHour : (Resource == 1 ? MinBytesPerHour : MinHeartsPerHour);
			const double Rate = std::max(Floor, AfkRatePerHour(Profile, Resource));
			return static_cast<long long>(std::floor(Rate * BoostHoursOf(Item) * Events::BoostMultiplier(Profile)));
		}

		int Count(const FProfile& Profile, EItem Item)
		{
			return Profile.Meta.Items[Index(Item)];
		}

		void Add(FProfile& Profile, EItem Item, int Amount)
		{
			int& Stored = Profile.Meta.Items[Index(Item)];
			Stored = std::clamp(Stored + Amount, 0, 99999);
		}

		int TotalItems(const FProfile& Profile)
		{
			int Total = 0;
			for (int Stored : Profile.Meta.Items)
			{
				Total += Stored;
			}
			return Total;
		}

		FActionResult Use(FProfile& Profile, EItem Item, int Amount)
		{
			if (Item == EItem::Count || Amount <= 0)
			{
				return Fail("Nothing to use.");
			}
			if (Count(Profile, Item) < Amount)
			{
				return Fail(std::string("You have ") + std::to_string(Count(Profile, Item)) + " " + ItemName(Item) + ".");
			}
			Add(Profile, Item, -Amount);
			if (Item == EItem::ChargePack)
			{
				Profile.Add(ECurrency::Charge, ChargePackAmount * Amount);
				return Ok("Charge +" + std::to_string(ChargePackAmount * Amount) + ".");
			}
			const long long Value = BoostValue(Profile, Item) * Amount;
			const int Granted = static_cast<int>(std::min<long long>(Value, 2000000000LL));
			const int Resource = BoostResource(Item);
			Profile.Add(Resource == 0 ? ECurrency::Gold : (Resource == 1 ? ECurrency::Bytes : ECurrency::AngelHearts), Granted);
			Events::Notify(Profile, Events::EStat::BoostUse, Amount);
			return Ok(std::string(ItemName(Item)) + (Amount > 1 ? " x" + std::to_string(Amount) : "") + ": +" + Meta::Num(Granted) + " " +
				ResourceName(Resource) + (Events::OvertimeActive(Profile) ? " (Overtime weekend +50%)" : "") + ".");
		}

		// ===================================================================== Charge
		int ChargeRefillCost(int RefillsDoneToday)
		{
			return 50 * (std::clamp(RefillsDoneToday, 0, ChargeRefillsPerDay - 1) + 1);
		}

		int ReplayChargeCost(int StageId)
		{
			const int Chapter = ChapterOfStage(StageId);
			if (Chapter <= 0)
			{
				return 0;
			}
			if (Campaign::IsWalkStage(StageId))
			{
				return 0;   // walking Return to Nowhere again costs nothing and pays nothing
			}
			return Chapter <= 4 ? 10 : (Chapter <= 9 ? 15 : 20);
		}

		int StageChargeCost(const FProfile& Profile, int StageId)
		{
			if (DecodeStage(StageId).Kind != EStageKind::Campaign || StageId > Profile.StagesCleared)
			{
				return 0;   // first clears (and every non-campaign mode) are free
			}
			return ReplayChargeCost(StageId);
		}

		void RegenCharge(FProfile& Profile, int64_t Seconds)
		{
			if (Seconds <= 0)
			{
				return;
			}
			const int Have = Profile.Raw(ECurrency::Charge);
			if (Have >= ChargeCap)
			{
				return;
			}
			const int64_t Points = Seconds / ChargeRegenSeconds;
			Profile.SetRaw(ECurrency::Charge, static_cast<int>(std::min<int64_t>(ChargeCap, Have + Points)));
		}

		void RegenChargeTo(FProfile& Profile, int64_t NowUnix)
		{
			FMeta& M = Profile.Meta;
			if (NowUnix <= 0)
			{
				return;
			}
			if (M.ChargeUnix <= 0 || M.ChargeUnix > NowUnix)
			{
				M.ChargeUnix = NowUnix;
				return;
			}
			if (Profile.Raw(ECurrency::Charge) >= ChargeCap)
			{
				M.ChargeUnix = NowUnix;   // full: the regeneration clock waits
				return;
			}
			const int64_t Elapsed = NowUnix - M.ChargeUnix;
			const int64_t Points = Elapsed / ChargeRegenSeconds;
			if (Points > 0)
			{
				RegenCharge(Profile, Points * ChargeRegenSeconds);
				M.ChargeUnix += Points * ChargeRegenSeconds;
				if (Profile.Raw(ECurrency::Charge) >= ChargeCap)
				{
					M.ChargeUnix = NowUnix;
				}
			}
		}

		int64_t SecondsToNextCharge(const FProfile& Profile)
		{
			if (Profile.NowUnix <= 0 || Profile.Meta.ChargeUnix <= 0 || Profile.Raw(ECurrency::Charge) >= ChargeCap)
			{
				return -1;
			}
			return std::max<int64_t>(0, Profile.Meta.ChargeUnix + ChargeRegenSeconds - Profile.NowUnix);
		}

		FActionResult RefillCharge(FProfile& Profile)
		{
			FMeta& M = Profile.Meta;
			if (M.ChargeRefillsToday >= ChargeRefillsPerDay)
			{
				return Fail("No Charge refills left today (" + std::to_string(ChargeRefillsPerDay) + " a day).");
			}
			const int Cost = ChargeRefillCost(M.ChargeRefillsToday);
			if (!Profile.Spend(ECurrency::Diamonds, Cost))
			{
				return Fail("A Charge refill costs " + std::to_string(Cost) + " Diamonds.");
			}
			++M.ChargeRefillsToday;
			Profile.Add(ECurrency::Charge, ChargeRefillAmount);
			return Ok("Charge +" + std::to_string(ChargeRefillAmount) + " for " + std::to_string(Cost) + " Diamonds (" +
				std::to_string(ChargeRefillsPerDay - M.ChargeRefillsToday) + " refills left today).");
		}

		void ResetDailyCounters(FMeta& Meta)
		{
			Meta.ChargeRefillsToday = 0;
		}

		// ===================================================================== Monthly supply
		FActionResult BuyMonthly(FProfile& Profile)
		{
			FMeta& M = Profile.Meta;
			if (M.MonthlyDays + MonthlyDays > MonthlyMaxStackDays)
			{
				return Fail("The Monthly supply is already stocked for " + std::to_string(M.MonthlyDays) + " days.");
			}
			M.MonthlyDays += MonthlyDays;
			Profile.Add(ECurrency::Diamonds, MonthlyInstantDiamonds);
			return Ok("Monthly supply: +" + std::to_string(MonthlyInstantDiamonds) + " Diamonds now and " + std::to_string(MonthlyDailyDiamonds) +
				" Diamonds by mail every day (" + std::to_string(M.MonthlyDays) + " days left).");
		}

		int MonthlyDaysLeft(const FProfile& Profile)
		{
			return Profile.Meta.MonthlyDays;
		}
	}
}
