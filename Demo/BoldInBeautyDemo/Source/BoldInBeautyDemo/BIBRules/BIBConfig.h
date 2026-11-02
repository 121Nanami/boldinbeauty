// Boldinbeauty demo — every tunable number in one place.
//
// SOURCE  = taken from BoldInBeauty_MAIN or an operator file (do not change without the owner).
// PLACEHOLDER = MAIN marks the item OPEN or DEFERRED; the demo needs *a* value to run.
//               These are not design decisions — replace them when the owner decides.
#pragma once

namespace BIB
{
	namespace Config
	{
		// ---------------------------------------------------- summoning — SOURCE (MAIN §2)
		constexpr double Rate6 = 0.001;  // 6★ Secret 0.1%
		constexpr double Rate5 = 0.02;   // 5★ Legendary 2%
		constexpr double Rate4 = 0.199;  // 4★ Epic 19.9%
		                                 // 3★ Rare 78% = remainder
		constexpr int DiamondsPerRoll = 300;
		constexpr int TenPullPaidRolls = 9;          // ten results cost nine rolls → 2,700 Diamonds
		constexpr int Pity5Rolls = 100;              // one 5★ per 100 rolls
		constexpr int Pity6Rolls = 2000;             // one 6★ per 2,000 rolls
		constexpr int MileagePerResult = 1;          // non-Daily only
		constexpr int MileageFeatured5Cost = 250;
		constexpr int MileageFeatured6Cost = 2500;
		constexpr double FeaturedShare = 0.5;        // 50/50 within the featured rarity
		constexpr int DuplicateResource = 20;        // per duplicate

		// ---------------------------------------------------- progression — SOURCE (MAIN §3)
		constexpr int BaseLevelCap = 100;
		constexpr int CapPerUpgradeStar = 20;        // WORKING: +20 cap per upgrade star
		constexpr int UpgradeStarCostStep = 20;      // WORKING: star n costs 20n
		constexpr double UpgradeStarDamagePercent = 5.0;  // "approximately 5%"
		constexpr double UpgradeStarDefencePercent = 5.0;
		constexpr int HeartGateEvery = 20;
		constexpr int HeartGateFirstCost = 30;       // doubles each gate, 1.25× from level 100
		constexpr int MoneyPerDiamond = 35;          // supplement values
		constexpr int BytesPerDiamond = 60;

		// ---------------------------------------------------- progression — PLACEHOLDER (MAIN OPEN)
		constexpr int LevelGoldPerLevel = 100;       // Gold cost to go from level L → L+1 = 100·L
		constexpr int LevelBytesPerLevel = 30;       // Bytes cost = 30·L (was 40·L; owner rebalance 2026-10-06: stages pay Bytes at
		                                             // 250 / 800 of the Gold, so the Bytes cost follows that ratio)
		// Owner request 2026-10-06: "a much heavier health and damage increase" per level, with the enemies scaling to match
		// (the campaign tables are solved against this number, Tests/campaign_model.h). Was +1% per level (level 100 = 1.99x);
		// +6% makes level 100 = 6.94x. The exact percentage is PLACEHOLDER.
		constexpr double StatPercentPerLevel = 6.0;  // +6% HP/damage per level above 1
		constexpr int MaxUpgradeStars = 0;           // 0 = no limit enforced (maximum is OPEN)
		constexpr double DefenceCapPercent = 75.0;   // damage-reduction ceiling
		constexpr double JudgeArmourFactor = 0.5;    // SOURCE (Judge.md): 50% less from armour

		// ---------------------------------------------------- equipment — PLACEHOLDER (MAIN §4 OPEN)
		constexpr double EquipMinPercent = 1.0;
		constexpr double EquipMaxPercent = 5.0;
		constexpr double EquipUpgradePercent = 1.0;
		constexpr int EquipMaxUpgrade = 5;
		// cost = 500·(level+1) Bytes, 7,500 to take an item to +5 (balancing pass 2026-10-09, was 100: 1,500 against ~930k Bytes a
		// campaign pays). A +1% upgrade is worth about half a team level late in the game; a full squad (12 items) is ~10% of the
		// campaign's Bytes, a real choice against levelling. PLACEHOLDER.
		constexpr int EquipUpgradeBytesStep = 500;

		// ---------------------------------------------------- merchant — PLACEHOLDER prices
		constexpr int MerchantTicketDiamonds = 300;
		constexpr int MerchantHeartsAmount = 30;
		constexpr int MerchantHeartsGold = 3000;
		constexpr int MerchantEquipmentGold = 1500;

		// ---------------------------------------------------- ultimate — PLACEHOLDER (owner chose "damage dealt")
		constexpr double UltDamageForFullBar = 20000.0;

		// ---------------------------------------------------- featured banner rotation — owner request 2026-10-04
		// The Featured (Special) banners change their featured operator every 3 days, in lock-step for everyone: the period is
		// computed from the UTC clock, so it needs no server and survives save/load. Period 0 starts at the anchor below and
		// shows the owner's launch picks (Judge 5*, Narrator 6*). The operators that follow, and their order, are a
		// PLACEHOLDER (MAIN §2: eligible pools and featured exclusivity are OPEN): each list is the roster order of that
		// rarity, starting with the owner's pick. The owner decides the real pools.
		constexpr int RotationDays = 3;
		constexpr long long RotationAnchorUnix = 1791072000LL;   // 2026-10-04 00:00:00 UTC
		constexpr const char* RotationPool5[] = {"Judge", "Alchemist", "Blood", "Stargazer", "Gambler", "Weather"};
		constexpr const char* RotationPool6[] = {"Narrator", "Jester", "Media", "Timekeeper", "Death", "Adapt"};

		// ---------------------------------------------------- accounts — owner request 2026-10-04
		// The admin account has every operator, unlimited currencies and every level. Which e-mail is the admin is decided by
		// the account server (ADMIN_EMAILS); the game only reads the flag the server sends. This constant documents the owner's
		// address and is used by the tests.
		constexpr const char* AdminEmail = "admin@example.com";
		constexpr int UnlimitedAmount = 999999999;           // what an admin currency reads; spending never lowers it
		constexpr int EquipmentInventoryCap = 1500;          // PLACEHOLDER: at the cap a new drop replaces the weakest spare item
		                                                     // (saves keep 2,000; a 1-2 year player drops more, 2026-10-09)
		constexpr int AdminEquipmentPerSlot = 6;             // PLACEHOLDER: random equipment granted to the admin per slot

		// ---------------------------------------------------- tester build — owner request 2026-10-05
		// In this build every player (guest or signed in, admin included) is a tester and gets the Tester panel (Lobby >
		// Tester panel, BIBTester.h). Set to false for a release build: the panel and its "Skip tutorial" button disappear and
		// the actions refuse to run. The admin account itself is unchanged: unlimited wallet, every operator, every level.
		constexpr bool TesterMode = true;

		// ---------------------------------------------------- tutorial — owner request 2026-10-04
		// New accounts own only Luna, with an empty squad. The tutorial walks them through levelling Luna, filling a squad
		// slot, a forced first summon (Kuu) and Chapter 1 level 1, after which Plack is unlocked for free.
		constexpr int TutorialPlayLevel = 1;

		// ---------------------------------------------------- new accounts — owner request 2026-10-06
		// "On start give them 300 diamonds and no other currency." Everything else is earned from stages and quests.
		constexpr int StartingDiamonds = 300;

		// ---------------------------------------------------- hub — owner request 2026-10-06
		constexpr float PageTransitionSeconds = 0.3f;   // fade + slide between hub pages (Settings > Interface can turn it off)

		// ---------------------------------------------------- classic gacha rebalance — owner request 2026-10-07
		// "Rebalance the game for more classic gacha game standards like NIKKE or ZZZ." The assistant's reading (PLACEHOLDER,
		// reversible, OPEN for the owner): ZZZ-style Charge (stamina) limits campaign replays - first clears stay free - and
		// the Outer World runs on the real clock like NIKKE's outpost (it was 1 world hour per real minute for the demo).
		// Owner decision 2026-10-07: Charge stays in the game (approved). UseCharge = false only for testing.
		constexpr bool UseCharge = true;
		constexpr double WorldHoursPerRealHour = 1.0;   // was 60 (one world hour per real minute)
		// Owner request 2026-10-15 (testing): every profile that loads is topped up to this many Arcade tokens (never reduced). 0 = off.
		constexpr int TestArcadeTokenFloor = 999;

		// ---------------------------------------------------- world — SOURCE (MAIN §7)
		constexpr double SectionRunSecondsCentreToWall = 8.0;
		constexpr int MinEnemiesPerSection = 10;
		constexpr int MaxEnemiesPerSection = 30;
	}
}
