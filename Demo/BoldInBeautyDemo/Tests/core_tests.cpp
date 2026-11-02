// Unit tests for the engine-independent Boldinbeauty demo rules.
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
	int GFailures = 0;
	int GChecks = 0;

	void Expect(bool bCondition, const std::string& What)
	{
		++GChecks;
		if (!bCondition)
		{
			++GFailures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	void ExpectNear(double Actual, double Expected, double Tolerance, const std::string& What)
	{
		Expect(std::fabs(Actual - Expected) <= Tolerance,
			What + " (actual " + std::to_string(Actual) + ", expected " + std::to_string(Expected) + " +/- " + std::to_string(Tolerance) + ")");
	}

	using namespace BIB;

	void TestRoster()
	{
		Expect(Roster().size() == 20, "roster has 20 operators");
		int Counts[7] = {};
		for (const FRosterEntry& Entry : Roster())
		{
			++Counts[Entry.Stars];
		}
		Expect(Counts[3] == 5 && Counts[4] == 3 && Counts[5] == 6 && Counts[6] == 6, "rarity counts 5/3/6/6");
		Expect(Roster()[FindOperator("Luna")].Stars == 4, "Luna is 4*");
		Expect(Roster()[FindOperator("Judge")].Stars == 5, "Judge is 5*");
		Expect(Roster()[FindOperator("Narrator")].Stars == 6, "Narrator is 6*");
		Expect(FindOperator("Nobody") == -1, "unknown operator");
	}

	void TestNewGame()
	{
		FProfile P(1);
		Expect(P.Owns(FindOperator("Luna")), "Luna granted at start");
		Expect(P.TeamIsPlayable(), "default team playable");
		Expect(P.Get(ECurrency::DailyTicket) == 1, "one Daily ticket on day 1");
		Expect(P.Banners.size() == 4 && P.Pity.size() == 4, "four demo banners");
	}

	void TestRates()
	{
		FProfile P(12345);
		const int Rolls = 2000000;
		P.Add(ECurrency::DailyTicket, Rolls);
		const int Daily = P.FindBanner("daily");
		long long Counts[7] = {};
		for (int Roll = 0; Roll < Rolls; ++Roll)
		{
			const FSummonOutcome Out = P.Summon(Daily, 1, EPay::Ticket);
			if (!Out.bOk)
			{
				Expect(false, "daily summon failed: " + Out.Message);
				return;
			}
			++Counts[Out.Results[0].Stars];
		}
		ExpectNear(100.0 * static_cast<double>(Counts[6]) / Rolls, 0.1, 0.02, "6* rate 0.1%");
		ExpectNear(100.0 * static_cast<double>(Counts[5]) / Rolls, 2.0, 0.08, "5* rate 2%");
		ExpectNear(100.0 * static_cast<double>(Counts[4]) / Rolls, 19.9, 0.2, "4* rate 19.9%");
		ExpectNear(100.0 * static_cast<double>(Counts[3]) / Rolls, 78.0, 0.2, "3* rate 78%");
		Expect(P.Get(ECurrency::Mileage) == 0, "Daily earns no Mileage");
		Expect(P.Pity[Daily].Since5 == 0 && P.Pity[Daily].Since6 == 0, "Daily gives no pity credit");
	}

	void TestDailyRules()
	{
		FProfile P(7);
		const int Daily = P.FindBanner("daily");
		Expect(!P.Summon(Daily, 1, EPay::Diamonds).bOk, "Daily forbids Diamonds");
		Expect(!P.Summon(Daily, 10, EPay::Ticket).bOk, "Daily has no 10x");
		Expect(P.Summon(Daily, 1, EPay::Ticket).bOk, "Daily single with ticket");
		Expect(!P.Summon(Daily, 1, EPay::Ticket).bOk, "only one Daily ticket per day");
		P.AdvanceDay();
		P.AdvanceDay();
		Expect(P.Get(ECurrency::DailyTicket) == 1, "unused Daily ticket expires, never accumulates");
	}

	void TestCosts()
	{
		FProfile P(9);
		const int Standard = P.FindBanner("standard");
		const int FeaturedJudge = P.FindBanner("featured-5");
		const int Diamonds = P.Get(ECurrency::Diamonds);
		Expect(P.Summon(Standard, 1, EPay::Diamonds).bOk, "1x with diamonds");
		Expect(P.Get(ECurrency::Diamonds) == Diamonds - 300, "1x costs 300 Diamonds");
		P.Add(ECurrency::Diamonds, 2700);
		const int Before10 = P.Get(ECurrency::Diamonds);
		Expect(P.Summon(Standard, 10, EPay::Diamonds).bOk, "10x with diamonds");
		Expect(P.Get(ECurrency::Diamonds) == Before10 - 2700, "10x costs 2,700 Diamonds");
		const int Regular = P.Get(ECurrency::RegularTicket);
		Expect(P.Summon(Standard, 10, EPay::Ticket).bOk, "10x with regular tickets");
		Expect(P.Get(ECurrency::RegularTicket) == Regular - 9, "10x costs nine tickets");
		const int Special = P.Get(ECurrency::SpecialTicket);
		Expect(P.Summon(FeaturedJudge, 1, EPay::Ticket).bOk, "featured uses special ticket");
		Expect(P.Get(ECurrency::SpecialTicket) == Special - 1, "featured 1x costs one special ticket");
		Expect(P.Get(ECurrency::Mileage) == 22, "Mileage: 1 + 10 + 10 + 1 results");
		Expect(!P.Summon(Standard, 5, EPay::Ticket).bOk, "only 1x or 10x");
	}

	void TestPityAndTenPull()
	{
		FProfile P(4242);
		const int Standard = P.FindBanner("standard");
		int Since5 = 0;
		int Since6 = 0;
		int Max5Gap = 0;
		int Max6Gap = 0;
		bool bEveryTenHasEpic = true;
		int Pity5Hits = 0;
		for (int Batch = 0; Batch < 30000; ++Batch)
		{
			P.Add(ECurrency::Diamonds, 2700);
			const FSummonOutcome Out = P.Summon(Standard, 10, EPay::Diamonds);
			if (!Out.bOk)
			{
				Expect(false, "10x failed: " + Out.Message);
				return;
			}
			bool bEpic = false;
			for (const FSummonResult& R : Out.Results)
			{
				++Since5;
				++Since6;
				bEpic = bEpic || R.Stars >= 4;
				Pity5Hits += (R.bPity && R.Stars == 5) ? 1 : 0;
				if (R.Stars >= 5)
				{
					Max5Gap = Since5 > Max5Gap ? Since5 : Max5Gap;
					Since5 = 0;
				}
				if (R.Stars == 6)
				{
					Max6Gap = Since6 > Max6Gap ? Since6 : Max6Gap;
					Since6 = 0;
				}
			}
			bEveryTenHasEpic = bEveryTenHasEpic && bEpic;
		}
		Expect(Max5Gap <= 100, "a 5* (or better) at least every 100 rolls, max gap " + std::to_string(Max5Gap));
		Expect(Max6Gap <= 2000, "a 6* at least every 2,000 rolls, max gap " + std::to_string(Max6Gap));
		Expect(Pity5Hits > 0, "5* pity actually triggered");
		Expect(bEveryTenHasEpic, "every 10x contains a 4* or better");
	}

	void TestFiftyFifty()
	{
		FProfile P(99);
		const int Banner = P.FindBanner("featured-5");
		const int Judge = FindOperator("Judge");
		int FiveStars = 0;
		int Featured = 0;
		bool bLostPending = false;
		bool bGuaranteeHonoured = true;
		for (int Batch = 0; Batch < 20000; ++Batch)
		{
			P.Add(ECurrency::SpecialTicket, 9);
			const FSummonOutcome Out = P.Summon(Banner, 10, EPay::Ticket);
			for (const FSummonResult& R : Out.Results)
			{
				if (R.Stars != 5)
				{
					continue;
				}
				++FiveStars;
				if (bLostPending && R.Operator != Judge)
				{
					bGuaranteeHonoured = false;
				}
				Featured += R.Operator == Judge ? 1 : 0;
				bLostPending = R.Operator != Judge;
			}
		}
		Expect(bGuaranteeHonoured, "after losing the 50/50 the next featured-rarity result is featured");
		// 50/50 with a guarantee after each loss → featured share converges to 2/3.
		ExpectNear(static_cast<double>(Featured) / FiveStars, 2.0 / 3.0, 0.02, "featured share of 5* results");
	}

	void TestMileage()
	{
		FProfile P(5);
		const int Standard = P.FindBanner("standard");
		const int FeaturedJudge = P.FindBanner("featured-5");
		const int FeaturedNarrator = P.FindBanner("featured-6");
		Expect(!P.RedeemMileage(Standard).bOk, "no redemption on the Standard banner");
		P.Add(ECurrency::Mileage, 249);
		Expect(!P.RedeemMileage(FeaturedJudge).bOk, "featured 5* needs 250 Mileage");
		P.Add(ECurrency::Mileage, 1);
		Expect(P.RedeemMileage(FeaturedJudge).bOk && P.Get(ECurrency::Mileage) == 0, "featured 5* for 250 Mileage");
		P.Add(ECurrency::Mileage, 2499);
		Expect(!P.RedeemMileage(FeaturedNarrator).bOk, "featured 6* needs 2,500 Mileage");
		P.Add(ECurrency::Mileage, 1);
		Expect(P.RedeemMileage(FeaturedNarrator).bOk && P.Get(ECurrency::Mileage) == 0, "featured 6* for 2,500 Mileage");
	}

	void TestDuplicatesAndStars()
	{
		FProfile P(3);
		const int Luna = FindOperator("Luna");
		Expect(!P.GrantOperator(Luna), "Luna already owned");
		Expect(P.Operators[Luna].DupResource == 20, "duplicate grants 20 units");
		Expect(FProfile::UpgradeStarCost(1) == 20 && FProfile::UpgradeStarCost(2) == 40, "star costs 20, 40");
		Expect(P.LevelCap(Luna) == 100, "base cap 100");
		Expect(P.TryUpgradeStar(Luna).bOk, "first upgrade star");
		Expect(P.LevelCap(Luna) == 120, "cap +20 per upgrade star");
		Expect(!P.TryUpgradeStar(Luna).bOk, "second star needs 40 units");
		const FCombatStats Stats = P.GetCombatStats(Luna);
		ExpectNear(Stats.DamageMult, 1.05, 1e-9, "upgrade star +5% damage");
		ExpectNear(Stats.DefencePercent, 5.0, 1e-9, "upgrade star +5% defence");
	}

	void TestHeartsAndLevels()
	{
		const int Expected[] = {30, 60, 120, 240, 300, 375, 469};
		for (int Gate = 1; Gate <= 7; ++Gate)
		{
			Expect(FProfile::HeartGateCost(Gate) == Expected[Gate - 1],
				"heart gate " + std::to_string(Gate) + " = " + std::to_string(Expected[Gate - 1]) + " (got " + std::to_string(FProfile::HeartGateCost(Gate)) + ")");
		}
		FProfile P(11);
		P.GrantDemoCurrency();
		P.GrantDemoCurrency();
		const int Luna = FindOperator("Luna");
		for (int Step = 0; Step < 19; ++Step)
		{
			P.TryLevelUp(Luna, false);
		}
		Expect(P.Operators[Luna].Level == 20, "reached level 20");
		Expect(!P.TryLevelUp(Luna, false).bOk, "level 20 gate blocks");
		const int Hearts = P.Get(ECurrency::AngelHearts);
		Expect(P.TryPayHeartGate(Luna).bOk && P.Get(ECurrency::AngelHearts) == Hearts - 30, "gate 1 costs 30 hearts");
		Expect(P.TryLevelUp(Luna, false).bOk && P.Operators[Luna].Level == 21, "level 21 after gate");

		Expect(FProfile::DiamondsToCover(35, 0) == 1 && FProfile::DiamondsToCover(36, 0) == 2, "35 Money per Diamond, rounded up");
		Expect(FProfile::DiamondsToCover(0, 60) == 1 && FProfile::DiamondsToCover(0, 61) == 2, "60 Bytes per Diamond, rounded up");

		FProfile Poor(12);
		const int PoorLuna = FindOperator("Luna");
		Poor.Spend(ECurrency::Gold, Poor.Get(ECurrency::Gold));
		Expect(!Poor.TryLevelUp(PoorLuna, false).bOk, "no gold → no level");
		const int DiamondsBefore = Poor.Get(ECurrency::Diamonds);
		Expect(Poor.TryLevelUp(PoorLuna, true).bOk, "Diamond supplement covers Gold");
		Expect(Poor.Get(ECurrency::Diamonds) == DiamondsBefore - FProfile::DiamondsToCover(100, 0), "supplement spends ceil(100/35) Diamonds");
	}

	void TestEquipment()
	{
		FProfile P(21);
		P.Inventory.clear();
		const int Judge = FindOperator("Judge");
		FEquipment A = P.AddRandomEquipment();
		FEquipment B = P.AddRandomEquipment();
		P.FindEquipment(B.Id)->Slot = A.Slot;
		P.FindEquipment(A.Id)->Bonus = EBonus::Damage;
		P.FindEquipment(A.Id)->Percent = 4.0;
		P.FindEquipment(A.Id)->Slot = ESlot::Helmet;
		P.FindEquipment(B.Id)->Slot = ESlot::Helmet;
		Expect(P.Equip(A.Id, Judge).bOk, "equip A");
		Expect(P.Equip(B.Id, Judge).bOk, "equip B in the same slot");
		Expect(P.FindEquipment(A.Id)->EquippedBy == -1, "one item per slot");
		P.Unequip(B.Id);
		P.Equip(A.Id, Judge);
		ExpectNear(P.GetCombatStats(Judge).DamageMult, 1.02, 1e-9, "Judge gets 50% of armour bonus");
		const int Bytes = P.Get(ECurrency::Bytes);
		Expect(P.UpgradeEquipment(A.Id).bOk && P.Get(ECurrency::Bytes) == Bytes - Config::EquipUpgradeBytesStep, "equipment upgrade costs Bytes");
	}

	void TestTutorialAndChapter()
	{
		FProfile P(31);
		const int Kuu = FindOperator("Kuu");
		const int Plack = FindOperator("Plack");
		Expect(!P.Owns(Kuu) && !P.Owns(Plack), "Kuu/Plack not owned at start");
		Expect(P.TutorialSummon().bOk && P.Owns(Kuu), "tutorial 1x summon grants Kuu");
		Expect(!P.TutorialSummon().bOk, "tutorial summon only once");
		P.ApplyStageRewards(1, false);
		Expect(!P.Owns(Plack), "Plack not from a normal stage");
		P.ApplyStageRewards(2, true);
		Expect(P.Owns(Plack), "Plack after the Chapter 1 boss");
		Expect(P.StagesCleared == 2, "stages cleared tracked");
	}

	void TestTeam()
	{
		FProfile P(41);
		const int Luna = FindOperator("Luna");
		const int Judge = FindOperator("Judge");
		Expect(P.SetTeamSlot(0, Judge).bOk, "move Judge to slot 1");
		Expect(P.Team[0] == Judge && P.Team[1] == Luna, "slots swapped");
		Expect(!P.SetTeamSlot(0, FindOperator("Myrcia")).bOk, "operators that are not owned cannot join");
		P.GrantOperator(FindOperator("Myrcia"));
		Expect(P.SetTeamSlot(2, FindOperator("Myrcia")).bOk, "owned placeholder operators can join (owner allowed placeholders)");
		Expect(P.TeamIsPlayable(), "team still playable");
		for (const FRosterEntry& Entry : Roster())
		{
			Expect(Entry.BaseHP > 0 && Entry.BaseAttack > 0 && Entry.Kit != EKit::None, std::string(Entry.Name) + " has stats and a kit (written or placeholder)");
		}
	}
}

int main()
{
	TestRoster();
	TestNewGame();
	TestRates();
	TestDailyRules();
	TestCosts();
	TestPityAndTenPull();
	TestFiftyFifty();
	TestMileage();
	TestDuplicatesAndStars();
	TestHeartsAndLevels();
	TestEquipment();
	TestTutorialAndChapter();
	TestTeam();
	std::printf("%d checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
