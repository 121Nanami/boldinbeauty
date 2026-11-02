// Boldinbeauty demo — the inventory: consumable items, Charge (stamina) and the Monthly supply card (owner request 2026-10-07:
// "flesh out the inventory with more currencies and 3, 6, 12 and 24 hour currency boosts which correlate with the AFK rewards
// from the Outer World ... extra inventory stuff and currencies must have a practical use and fit the balancing").
// Engine-independent. Every amount is PLACEHOLDER (the owner decides).
//
//   * Boosts (NIKKE / AFK-style "credit cases"): using one pays out that many hours of the Outer World's current production
//     at once (Gold, Bytes or Angel hearts), so they grow with the buildings and the campaign progress. Before the Outer World
//     produces much, a floor rate keeps an early boost worth using.
//   * Charge (ZZZ-style battery): first clears are free; replaying a campaign level for its Gold / Bytes / Angel hearts costs
//     Charge. It refills over time (1 per 6 minutes, up to 240), from Charge packs, and up to three times a day for Diamonds.
//   * Monthly supply: a SIMULATED purchase that pays 300 Diamonds now and 100 Diamonds a day by mail for 30 days.
#pragma once

#include <cstdint>
#include <string>

#include "BIBMeta.h"

namespace BIB
{
	class FProfile;
	struct FActionResult;

	enum class EItem : int
	{
		GoldBoost3h = 0,
		GoldBoost6h,
		GoldBoost12h,
		GoldBoost24h,
		BytesBoost3h,
		BytesBoost6h,
		BytesBoost12h,
		BytesBoost24h,
		HeartsBoost3h,
		HeartsBoost6h,
		HeartsBoost12h,
		HeartsBoost24h,
		ChargePack,       // +60 Charge (may go above the cap)
		Count
	};
	static_assert(static_cast<int>(EItem::Count) == ItemCount, "BIBMeta.h ItemCount must match EItem");

	namespace Inventory
	{
		// ------------------------------------------------------------------ boosts
		constexpr int BoostHours[4] = {3, 6, 12, 24};
		// Floor rates per hour when the Outer World produces less (PLACEHOLDER): a fresh account's 24 h Gold boost is still
		// worth about four early stage clears.
		constexpr double MinGoldPerHour = 150.0;
		constexpr double MinBytesPerHour = 50.0;
		constexpr double MinHeartsPerHour = 0.5;

		const char* ItemName(EItem Item);        // "Gold boost (6 h)"
		const char* ItemIcon(EItem Item);        // "boost_gold"
		std::string ItemUse(EItem Item);         // what it does
		bool IsBoost(EItem Item);
		int BoostResource(EItem Item);           // 0 Gold, 1 Bytes, 2 Angel hearts (-1 when not a boost)
		int BoostHoursOf(EItem Item);
		EItem BoostItem(int Resource, int HoursIndex);   // Resource 0..2, HoursIndex 0..3 (3, 6, 12, 24 h)

		// The Outer World's production per hour for a resource, including the campaign bonus (the AFK rate boosts pay from).
		double AfkRatePerHour(const FProfile& Profile, int Resource);
		long long BoostValue(const FProfile& Profile, EItem Item);   // what using it pays now

		int Count(const FProfile& Profile, EItem Item);
		void Add(FProfile& Profile, EItem Item, int Amount);
		FActionResult Use(FProfile& Profile, EItem Item, int Amount = 1);
		int TotalItems(const FProfile& Profile);

		// ------------------------------------------------------------------ Charge (stamina)
		constexpr int ChargeCap = 240;
		constexpr int ChargeRegenSeconds = 360;   // 1 Charge every 6 minutes = 240 a day
		constexpr int ChargePackAmount = 60;
		constexpr int ChargeRefillAmount = 60;
		constexpr int ChargeRefillsPerDay = 3;
		int ChargeRefillCost(int RefillsDoneToday);   // 50, 100, 150 Diamonds
		// Charge a replay of a campaign level costs: 10 in chapters 1-4, 15 in 5-9, 20 in 10-13. First clears are free.
		int ReplayChargeCost(int StageId);
		int StageChargeCost(const FProfile& Profile, int StageId);   // 0 for a first clear or a non-campaign stage
		// Adds the Charge that Seconds of real time regenerate (never above the cap; Charge above it from packs is kept).
		void RegenCharge(FProfile& Profile, int64_t Seconds);
		// Real-clock regeneration (LiveOps::UpdateClock): uses Meta.ChargeUnix.
		void RegenChargeTo(FProfile& Profile, int64_t NowUnix);
		int64_t SecondsToNextCharge(const FProfile& Profile);   // -1 when full or no clock
		FActionResult RefillCharge(FProfile& Profile);        // Diamonds -> +60 Charge
		void ResetDailyCounters(FMeta& Meta);                 // refills done today, monthly supply delivery

		// ------------------------------------------------------------------ Monthly supply (SIMULATED purchase)
		constexpr int MonthlyDays = 30;
		constexpr int MonthlyInstantDiamonds = 300;
		constexpr int MonthlyDailyDiamonds = 100;
		constexpr int MonthlyMaxStackDays = 180;              // buying again adds 30 days, up to 180
		FActionResult BuyMonthly(FProfile& Profile);          // demo purchase, no payment
		int MonthlyDaysLeft(const FProfile& Profile);
	}
}
