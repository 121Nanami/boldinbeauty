// Unit tests for the 2026-10-07 owner request: the classic gacha rebalance (monthly pull budget, Charge, quick clears), the
// element system, the inventory (Outer World boosts, new currencies, Monthly supply) and the simple events (Rookie week,
// weekly operation, event shop, Overtime weekend, Element Surge, Ark shop). Build & run: Tests/run_core_tests.sh
//   gacha_tests            the checks
//   gacha_tests --budget   also prints the monthly pull budget of a free player
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArena.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBElement.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBEvents.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBInventory.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBLiveOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBTester.h"

#include <cstdio>
#include <cstring>
#include <set>
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

	using namespace BIB;
	namespace P = BIB::Pages;

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	FProfile NewPlayer(uint32_t Seed)
	{
		FProfile Profile(Seed);
		FAccountInfo Guest;
		Guest.bGuest = true;
		Profile.Account = Guest;
		Profile.ResetForAccount();
		Tester::SkipTutorial(Profile);
		return Profile;
	}

	bool HasTile(const P::FPage& Page, const std::string& Prefix)
	{
		for (const P::FPageAction& Action : Page.Grid)
		{
			if (Action.Id.rfind(Prefix, 0) == 0)
			{
				return true;
			}
		}
		for (const P::FPageAction& Action : Page.Bar)
		{
			if (Action.Id.rfind(Prefix, 0) == 0)
			{
				return true;
			}
		}
		return false;
	}

	// ================================================================== elements
	void TestElements()
	{
		std::set<int> Beaten;
		for (int Index = 0; Index < ElementCount; ++Index)
		{
			const EElement E = static_cast<EElement>(Index);
			const EElement Strong = Element::StrongAgainst(E);
			Expect(Strong != EElement::None && Strong != E, std::string(Element::Name(E)) + " is strong against another element");
			Expect(Element::WeakTo(Strong) == E, std::string(Element::Name(E)) + ": the ring is consistent");
			Expect(Element::HasAdvantage(E, Strong) && !Element::HasAdvantage(Strong, E), std::string(Element::Name(E)) + ": advantage is one-way");
			Beaten.insert(static_cast<int>(Strong));
			Expect(std::strlen(Element::Icon(E)) > 3 && std::strcmp(Element::Name(E), "-") != 0, "element has a name and an icon");
		}
		Expect(static_cast<int>(Beaten.size()) == ElementCount, "every element is beaten by exactly one other (two rings of four)");
		Expect(!Element::HasAdvantage(EElement::None, EElement::Fire) && !Element::HasAdvantage(EElement::Fire, EElement::None), "no element, no advantage");
		Expect(std::string(Element::Name(EElement::Fire)) == "Fire" && std::string(Element::Name(EElement::Dark)) == "Dark" && std::string(Element::Name(EElement::Water)) == "Water"
			&& std::string(Element::Name(EElement::Nature)) == "Nature" && std::string(Element::Name(EElement::Magic)) == "Magic", "the owner's five elements exist");

		int PerElement[ElementCount] = {};
		for (int Op = 0; Op < static_cast<int>(Roster().size()); ++Op)
		{
			const EElement E = Element::OfOperator(Op);
			Expect(E != EElement::None, std::string(Roster()[static_cast<size_t>(Op)].Name) + " has an element");
			if (E != EElement::None)
			{
				++PerElement[static_cast<int>(E)];
			}
		}
		for (int Index = 0; Index < ElementCount; ++Index)
		{
			Expect(PerElement[Index] >= 2, std::string(Element::Name(static_cast<EElement>(Index))) + " has at least two operators");
		}

		// stages
		Expect(Element::OfStage(CampaignStageId(1, 3)) == EElement::None, "Chapter 1 has no element (tutorial chapter)");
		std::set<int> ChapterElements;
		for (int Chapter = 2; Chapter <= CampaignChapters; ++Chapter)
		{
			const EElement E = Element::OfChapter(Chapter);
			Expect(E != EElement::None, "chapter " + std::to_string(Chapter) + " has an element");
			ChapterElements.insert(static_cast<int>(E));
			Expect(Element::CounterOf(CampaignStageId(Chapter, 1)) == Element::WeakTo(E), "the counter is the element that beats the chapter");
		}
		Expect(static_cast<int>(ChapterElements.size()) == ElementCount, "chapters 2-13 use all eight elements");
		const int Late = CampaignStageId(11, 2);
		Expect(Element::OfStage(Late, 1) != Element::OfStage(Late, 0), "chapters 10-13 alternate two elements by section");
		Expect(Element::OfStage(TowerStageId(0, 3)) == EElement::None && Element::OfStage(TowerStageId(0, 5)) != EElement::None, "tower floors gain elements from floor 5");
		Expect(Element::OfStage(SubStoryStageId(0, 1)) == EElement::None, "sub-stories have no element");

		// damage and attunement
		FProfile Profile = NewPlayer(7);
		const int Plack = FindOperator("Plack");   // Fire
		Expect(Element::OfOperator(Plack) == EElement::Fire, "Plack is Fire (PROPOSED)");
		Expect(std::abs(Element::DamageMultiplier(Profile, Plack, EElement::Nature) - 1.15) < 1e-9, "advantage: +15%");
		Expect(Element::DamageMultiplier(Profile, Plack, EElement::Water) == 1.0 && Element::DamageMultiplier(Profile, Plack, EElement::None) == 1.0, "no penalty, no bonus otherwise");
		Expect(!Element::TryAttune(Profile, Plack).bOk, "Attunement needs Element cores");
		Profile.Add(ECurrency::ElementCore, 1000);
		Profile.Add(ECurrency::Gold, 1000000);
		const int PowerBefore = OperatorPower(Profile, Plack);
		const int CoresBefore = Profile.Get(ECurrency::ElementCore);
		const int Expected = Element::AttunementCoresFor(Profile, Plack, 1);
		Expect(Element::TryAttune(Profile, Plack).bOk && Element::Attunement(Profile, Plack) == 1, "attune to 1");
		Expect(CoresBefore - Profile.Get(ECurrency::ElementCore) == Expected, "attunement spends the shown cores");
		Expect(std::abs(Element::DamageMultiplier(Profile, Plack, EElement::Nature) - 1.18) < 1e-9, "Attunement 1: +18%");
		Expect(OperatorPower(Profile, Plack) > PowerBefore, "Attunement raises battle power");
		for (int Level = 2; Level <= Element::MaxAttunement; ++Level)
		{
			Element::TryAttune(Profile, Plack);
		}
		Expect(Element::Attunement(Profile, Plack) == Element::MaxAttunement && !Element::TryAttune(Profile, Plack).bOk, "Attunement stops at 10");
		Expect(std::abs(Element::AdvantageBonusOf(Profile, Plack) - 0.45) < 1e-9, "Attunement 10: +45% on advantaged hits");
		Expect(!Element::TryAttune(Profile, FindOperator("Death")).bOk, "cannot attune an operator you do not own");
		int Total = 0;
		for (int Level = 1; Level <= Element::MaxAttunement; ++Level)
		{
			Total += Element::AttunementCores(Level);
		}
		Expect(Total == 550, "a full Attunement costs 550 Element cores (weeks of play)");

		// Surge discount
		FProfile Surge = NewPlayer(8);
		for (int Day = 1; Day <= 7 * ElementCount; ++Day)
		{
			if (Events::SurgeElement(Surge) == EElement::Fire)
			{
				break;
			}
			Surge.AdvanceDay();
		}
		Expect(Events::SurgeElement(Surge) == EElement::Fire, "Fire Surge week found");
		Expect(Element::AttunementCoresFor(Surge, Plack, 1) == 7 && Element::AttunementCoresFor(Surge, FindOperator("Luna"), 1) == 10, "Surge: 30% fewer cores for its element only");

		// arena: an advantaged lineup wins more often against an equal one
		FProfile Arena = NewPlayer(9);
		Arena.GrantAllOperators();
		const int FireTeam[3] = {FindOperator("Plack"), FindOperator("Gambler"), FindOperator("Shiraishi")};
		const int NatureTeam[3] = {FindOperator("Kuu"), FindOperator("Alchemist"), FindOperator("Arita")};
		Arena::FLineup Fire = Arena::MakeLineup(Arena, FireTeam, "Fire");
		Arena::FLineup Nature = Arena::MakeLineup(Arena, NatureTeam, "Nature");
		// Mirror the stats so only the elements differ.
		for (size_t Slot = 0; Slot < 3; ++Slot)
		{
			Nature.Team[Slot].Hp = Fire.Team[Slot].Hp;
			Nature.Team[Slot].Attack = Fire.Team[Slot].Attack;
			Nature.Team[Slot].Defence = Fire.Team[Slot].Defence;
		}
		int FireWins = 0;
		for (uint32_t Seed = 1; Seed <= 200; ++Seed)
		{
			FireWins += Arena::Simulate(Fire, Nature, Seed).bAttackerWon ? 1 : 0;
		}
		Expect(FireWins > 120, "arena: Fire beats an equally strong Nature team most of the time (" + std::to_string(FireWins) + "/200)");
	}

	// ================================================================== inventory and Charge
	void TestInventory()
	{
		FProfile Profile = NewPlayer(11);
		for (int Index = 0; Index < ItemCount; ++Index)
		{
			const EItem Item = static_cast<EItem>(Index);
			Expect(std::strlen(Inventory::ItemName(Item)) > 3 && !Inventory::ItemUse(Item).empty(), "item has a name and a use");
		}
		Expect(Inventory::BoostHoursOf(EItem::GoldBoost3h) == 3 && Inventory::BoostHoursOf(EItem::GoldBoost24h) == 24 && Inventory::BoostHoursOf(EItem::HeartsBoost6h) == 6,
			"boosts come in 3, 6, 12 and 24 hours");
		Expect(Inventory::BoostItem(1, 2) == EItem::BytesBoost12h && Inventory::BoostResource(EItem::HeartsBoost24h) == 2 && !Inventory::IsBoost(EItem::ChargePack), "boost table");

		// value follows the Outer World (floor before anything is built)
		const long long Floor = Inventory::BoostValue(Profile, EItem::GoldBoost24h) ;
		const bool bOvertime = Events::OvertimeActive(Profile);
		Expect(Floor == static_cast<long long>(Inventory::MinGoldPerHour * 24 * (bOvertime ? 1.5 : 1.0)), "an early 24 h Gold boost pays the floor rate");
		Profile.Add(ECurrency::Gold, 2000000);
		Profile.Add(ECurrency::Bytes, 2000000);
		for (int Building = 0; Building < BuildingCount; ++Building)
		{
			for (int Level = 0; Level < 5; ++Level)
			{
				Meta::BuildOrUpgrade(Profile, Building);
			}
		}
		const double Rate = Inventory::AfkRatePerHour(Profile, 0);
		Expect(Rate > Inventory::MinGoldPerHour, "the Outer World now out-produces the floor");
		const long long Six = Inventory::BoostValue(Profile, EItem::GoldBoost6h);
		const long long Twelve = Inventory::BoostValue(Profile, EItem::GoldBoost12h);
		Expect(Twelve >= 2 * Six - 1 && Twelve <= 2 * Six + 1, "12 h pays twice 6 h");
		Profile.StagesCleared = 50;
		Expect(Inventory::AfkRatePerHour(Profile, 0) > Rate * 1.45, "campaign progress raises the AFK rate (+1% a level)");

		Inventory::Add(Profile, EItem::GoldBoost12h, 2);
		const int Gold = Profile.Get(ECurrency::Gold);
		const long long Value = Inventory::BoostValue(Profile, EItem::GoldBoost12h);
		Expect(Inventory::Use(Profile, EItem::GoldBoost12h).bOk && Profile.Get(ECurrency::Gold) == Gold + Value, "using a boost pays its value");
		Expect(Inventory::Count(Profile, EItem::GoldBoost12h) == 1 && !Inventory::Use(Profile, EItem::GoldBoost12h, 5).bOk, "items are counted");

		// Charge
		Profile.SetRaw(ECurrency::Charge, 0);
		Inventory::Add(Profile, EItem::ChargePack, 1);
		Expect(Inventory::Use(Profile, EItem::ChargePack).bOk && Profile.Get(ECurrency::Charge) == 60, "a Charge pack gives 60");
		Inventory::RegenCharge(Profile, 3600);
		Expect(Profile.Get(ECurrency::Charge) == 70, "1 Charge every 6 minutes");
		Inventory::RegenCharge(Profile, 86400 * 3);
		Expect(Profile.Get(ECurrency::Charge) == Inventory::ChargeCap, "regeneration stops at the cap");
		Profile.Add(ECurrency::Charge, 60);
		Inventory::RegenCharge(Profile, 86400);
		Expect(Profile.Get(ECurrency::Charge) == Inventory::ChargeCap + 60, "Charge above the cap (packs) is kept");

		// real clock
		FProfile Clock = NewPlayer(12);
		const int64_t Start = 1791072000LL + 3600;
		Clock.SetTime(Start);
		LiveOps::UpdateClock(Clock, Start);
		const int Charge0 = Clock.Get(ECurrency::Charge);
		Clock.SetTime(Start + 1800);
		LiveOps::UpdateClock(Clock, Start + 1800);
		Expect(Clock.Get(ECurrency::Charge) == Charge0 + 5, "30 real minutes regenerate 5 Charge");
		Expect(Inventory::SecondsToNextCharge(Clock) >= 0 && Inventory::SecondsToNextCharge(Clock) <= Inventory::ChargeRegenSeconds, "countdown to the next Charge");

		// refills
		FProfile Refill = NewPlayer(13);
		Refill.SetRaw(ECurrency::Diamonds, 1000);
		Expect(Inventory::RefillCharge(Refill).bOk && Refill.Get(ECurrency::Diamonds) == 950, "first refill 50 Diamonds");
		Expect(Inventory::RefillCharge(Refill).bOk && Refill.Get(ECurrency::Diamonds) == 850, "second refill 100");
		Expect(Inventory::RefillCharge(Refill).bOk && Refill.Get(ECurrency::Diamonds) == 700, "third refill 150");
		Expect(!Inventory::RefillCharge(Refill).bOk, "three refills a day");
		Refill.AdvanceDay();
		Expect(Inventory::RefillCharge(Refill).bOk, "refills reset with the day");

		// Monthly supply (simulated purchase)
		FProfile Monthly = NewPlayer(14);
		int MonthlyOffer = -1;
		for (int Index = 0; Index < static_cast<int>(Meta::IapOffers().size()); ++Index)
		{
			MonthlyOffer = Meta::IapOffers()[static_cast<size_t>(Index)].bMonthly ? Index : MonthlyOffer;
		}
		Expect(MonthlyOffer >= 0, "the store sells the Monthly supply");
		const int Diamonds = Monthly.Get(ECurrency::Diamonds);
		Expect(Meta::SimulatePurchase(Monthly, MonthlyOffer).bOk && Monthly.Get(ECurrency::Diamonds) == Diamonds + 300, "Monthly supply: 300 Diamonds now");
		for (int Day = 0; Day < 31; ++Day)
		{
			Monthly.AdvanceDay();
			Meta::ClaimAllMail(Monthly);   // the mailbox keeps 20 mails
		}
		Expect(Monthly.Get(ECurrency::Diamonds) >= Diamonds + 300 + 30 * 100, "then 100 a day by mail for 30 days");
		Expect(Inventory::MonthlyDaysLeft(Monthly) == 0, "the supply runs out");
	}

	// ================================================================== Charge, quick clears and pacing hooks
	void TestCharge()
	{
		FProfile Profile = NewPlayer(21);
		Profile.SetRaw(ECurrency::Charge, 0);
		Expect(Inventory::StageChargeCost(Profile, 2) == 0 && Meta::CanStartStage(Profile, 2).bOk, "first clears are free");
		Expect(Inventory::StageChargeCost(Profile, 1) == 10, "replaying a Chapter 1 level costs 10 Charge");
		Expect(!Meta::CanStartStage(Profile, 1).bOk == Config::UseCharge, "no Charge, no replay");
		Expect(Inventory::ReplayChargeCost(CampaignStageId(5, 1)) == 15 && Inventory::ReplayChargeCost(CampaignStageId(12, 1)) == 20, "later chapters cost more");
		Expect(Inventory::StageChargeCost(Profile, TowerStageId(0, 1)) == 0, "the Ark does not use Charge");
		Expect(!Meta::QuickClear(Profile, 1, 1).bOk, "quick clear needs Charge");
		Expect(!Meta::QuickClear(Profile, 5, 1).bOk, "quick clear needs a cleared level");
		Profile.SetRaw(ECurrency::Charge, 35);
		const int Gold = Profile.Get(ECurrency::Gold);
		const FActionResult Sweep = Meta::QuickClear(Profile, 1, 5);
		Expect(Sweep.bOk && Contains(Sweep.Message, "x3"), "quick clear stops when the Charge runs out (3 of 5)");
		Expect(Profile.Get(ECurrency::Charge) == 5 && Profile.Get(ECurrency::Gold) > Gold, "it spends Charge and pays the level's rewards");
		Expect(Profile.StagesCleared == 1, "a quick clear never unlocks anything");
	}

	// ================================================================== events
	void TestEvents()
	{
		FProfile Dev(31);   // the developer profile: no Rookie week
		Expect(!Events::RookieActive(Dev), "the developer profile has no Rookie week");
		FProfile Profile = NewPlayer(32);
		Expect(Events::RookieActive(Profile) && Events::RookieDay(Profile) == 1, "a new account starts its Rookie week");
		const FReward Total = Events::RookieTotal();
		Expect(Total.RegularTickets == 20 && Total.SpecialTickets == 5 && Total.Diamonds == 2000, "Rookie week: 20 Regular, 5 Special tickets, 2,000 Diamonds");
		Expect(Events::RookieMission(Profile, 3).bLocked && !Events::RookieMission(Profile, 0).bLocked, "day 2 missions are locked on day 1");
		Meta::NotifyQuest(Profile, Meta::EQuestEvent::StageClear, 2);
		const int Regular = Profile.Get(ECurrency::RegularTicket);
		Expect(Events::ClaimRookie(Profile, 0).bOk && Profile.Get(ECurrency::RegularTicket) == Regular + 2, "claim a Rookie mission");
		Expect(!Events::ClaimRookie(Profile, 0).bOk, "only once");
		Expect(!Events::ClaimRookie(Profile, 1).bOk, "an unfinished mission cannot be claimed");
		for (int Day = 0; Day < 7; ++Day)
		{
			Profile.AdvanceDay();
		}
		Expect(!Events::RookieActive(Profile) && !Events::ClaimRookie(Profile, 1).bOk, "the Rookie week ends after 7 days");

		// weekly operation
		FProfile Week = NewPlayer(33);
		Expect(Events::Theme(Week) == Events::ETheme::AngelHunt, "week 1: Angel Hunt");
		Expect(Events::TokensPerWeek() == 1400, "missions pay 1,400 Event tokens a week");
		Meta::NotifyQuest(Week, Meta::EQuestEvent::StageClear, 10);
		Expect(Events::Mission(Week, 0).bComplete && !Events::Mission(Week, 1).bComplete, "mission progress counts stage clears");
		Expect(Events::ClaimMission(Week, 0).bOk && Week.Get(ECurrency::EventToken) == 150, "claim pays tokens");
		Expect(!Events::ClaimMission(Week, 0).bOk, "once a week");
		Expect(Events::BuyEventShop(Week, 1).bOk && Week.Get(ECurrency::RegularTicket) >= 1 && !Events::BuyEventShop(Week, 1).bOk, "event shop sells one Regular ticket a week");
		const int Tokens = Week.Get(ECurrency::ArcadeToken);
		for (int Day = 0; Day < 7; ++Day)
		{
			Week.AdvanceDay();
		}
		Expect(Events::Theme(Week) == Events::ETheme::ArcadeFever, "week 2: Arcade Fever");
		Expect(Week.Get(ECurrency::ArcadeToken) >= std::max(Tokens, 5) + Events::ArcadeFeverTokens, "Arcade Fever: +2 Arcade tokens a day");
		Expect(!Events::Mission(Week, 0).bClaimed && Events::Mission(Week, 0).Progress == 0, "a new week resets the missions");
		Expect(Events::EventShopLeft(Week, 1) == 1, "and the shop stock");

		// Overtime weekend
		FProfile Weekend = NewPlayer(34);
		for (int Day = 1; Day < 6; ++Day)
		{
			Expect(!Events::OvertimeActive(Weekend), "no Overtime on day " + std::to_string(Day));
			Weekend.AdvanceDay();
		}
		Expect(Events::OvertimeActive(Weekend) && Events::WorldMultiplier(Weekend) == 2.0, "Overtime on day 6");
		Weekend.Add(ECurrency::Gold, 100000);
		Weekend.Add(ECurrency::Bytes, 100000);
		Meta::BuildOrUpgrade(Weekend, 1);   // Police Station: Gold
		Meta::AdvanceWorld(Weekend, 1.0);
		Expect(Weekend.Meta.Buildings[1].StoredGold >= 2 * Meta::BuildingRate(Weekend, 1, 0) - 1e-6, "Overtime doubles the Outer World");

		// Element Surge
		FProfile Surge = NewPlayer(35);
		const EElement First = Events::SurgeElement(Surge);
		for (int Day = 0; Day < 7; ++Day)
		{
			Surge.AdvanceDay();
		}
		Expect(Events::SurgeElement(Surge) != First, "the Surge element changes every week");
		int Member = -1;
		for (int Op = 0; Op < static_cast<int>(Roster().size()); ++Op)
		{
			Member = Element::OfOperator(Op) == Events::SurgeElement(Surge) ? Op : Member;
		}
		Surge.GrantOperator(Member);
		Surge.Team[0] = Member;
		Surge.SetRaw(ECurrency::Charge, 100);
		const int Cores = Surge.Get(ECurrency::ElementCore);
		Meta::ClearStage(Surge, 1);
		Expect(Surge.Get(ECurrency::ElementCore) == Cores + Events::SurgeCoresPerClear, "a clear with a Surge operator pays Element cores");

		// Ark shop and medals
		FProfile Ark = NewPlayer(36);
		Expect(Arena::FightReward(true).ArkMedals == 30 && Arena::FightReward(false).ArkMedals == 10, "arena fights pay Ark medals");
		Ark.Add(ECurrency::ArkMedal, 5000);
		Expect(Events::BuyArkShop(Ark, 0).bOk && !Events::BuyArkShop(Ark, 0).bOk, "one Special ticket a month in the Ark shop");
		Expect(Ark.Get(ECurrency::SpecialTicket) == 1 && Ark.Get(ECurrency::ArkMedal) == 3800, "it costs 1,200 medals");
		Meta::ClearStage(Ark, TowerStageId(0, 1));
		Expect(Ark.Get(ECurrency::ArkMedal) == 3800 + Events::ArkMedalsForFloor(1), "a new tower floor pays Ark medals");
	}

	// ================================================================== the classic gacha pull budget
	struct FBudget
	{
		double Diamonds = 0.0;
		double Regular = 0.0;
		double Special = 0.0;
		double Pulls() const { return Diamonds / Config::DiamondsPerRoll + Regular + Special; }
	};

	void Add(FBudget& Budget, const FReward& Reward, double Times)
	{
		Budget.Diamonds += Reward.Diamonds * Times;
		Budget.Regular += Reward.RegularTickets * Times;
		Budget.Special += Reward.SpecialTickets * Times;
	}

	void TestPullBudget(bool bPrint)
	{
		// A free player who has finished the campaign: what a 30-day month pays in summons (Daily-banner tickets excluded:
		// they have no pity or Mileage, like NIKKE's social recruit). Classic references: NIKKE and ZZZ give a free player
		// roughly 40-60 pulls a month.
		const FProfile Profile = NewPlayer(41);
		const double Weeks = 30.0 / 7.0;
		FBudget Dailies;
		FBudget Weeklies;
		FBudget Login;
		FBudget Pass;
		FBudget EventShop;
		FBudget ArkShop;
		FBudget Arena;
		for (int Index = 0; Index < Meta::QuestCount(Meta::EQuestList::Daily); ++Index)
		{
			Add(Dailies, Meta::Quest(Profile, Meta::EQuestList::Daily, Index).Reward, 30.0);
		}
		for (int Index = 0; Index < Meta::QuestCount(Meta::EQuestList::Weekly); ++Index)
		{
			Add(Weeklies, Meta::Quest(Profile, Meta::EQuestList::Weekly, Index).Reward, Weeks);
		}
		for (int Day = 1; Day <= 7; ++Day)
		{
			Add(Login, Meta::LoginReward(Day), Weeks);
		}
		for (int Tier = 1; Tier <= SeasonTiers; ++Tier)
		{
			Add(Pass, Meta::SeasonReward(Tier, false), 1.0);
		}
		// Event tokens go to pulls first: Special, Regular, then Diamonds.
		int Tokens = Events::TokensPerWeek();
		for (int Index : {0, 1, 8})
		{
			const Events::FShopItem& Item = Events::EventShopItem(Index);
			for (int Bought = 0; Bought < Item.Limit && Tokens >= Item.Price; ++Bought)
			{
				Tokens -= Item.Price;
				Add(EventShop, Item.Reward, Weeks);
			}
		}
		// Ark medals: 5 arena fights a day at a 50% win rate plus a mid-table season reward.
		const double Medals = 30.0 * 5.0 * (Events::ArkMedalsForArena(true) + Events::ArkMedalsForArena(false)) / 2.0 + Arena::SeasonReward(6).ArkMedals;
		double MedalsLeft = Medals;
		for (int Index : {0, 1})
		{
			const Events::FShopItem& Item = Events::ArkShopItem(Index);
			for (int Bought = 0; Bought < Item.Limit && MedalsLeft >= Item.Price; ++Bought)
			{
				MedalsLeft -= Item.Price;
				Add(ArkShop, Item.Reward, 1.0);
			}
		}
		Add(Arena, Arena::FightReward(true), 30.0 * 5.0 / 2.0);
		Add(Arena, Arena::SeasonReward(6), 1.0);
		const FBudget* Parts[] = {&Dailies, &Weeklies, &Login, &Pass, &EventShop, &ArkShop, &Arena};
		const char* Names[] = {"daily quests", "weekly quests", "7-day login", "season pass (free)", "event shop", "Ark shop", "arena"};
		double Total = 0.0;
		for (int Index = 0; Index < 7; ++Index)
		{
			Total += Parts[Index]->Pulls();
			if (bPrint)
			{
				std::printf("  %-20s %6.1f pulls  (%5.0f Diamonds, %4.1f Regular, %4.1f Special)\n", Names[Index], Parts[Index]->Pulls(), Parts[Index]->Diamonds,
					Parts[Index]->Regular, Parts[Index]->Special);
			}
		}
		if (bPrint)
		{
			std::printf("  %-20s %6.1f pulls a month (+30 Daily-banner tickets; Monthly supply +11, Premium pass +~13)\n", "TOTAL (free)", Total);
		}
		Expect(Total >= 40.0 && Total <= 60.0, "a free player earns 40-60 pulls a month (classic gacha), got " + std::to_string(Total));
		Expect(Dailies.Pulls() >= 7.0 && Dailies.Pulls() <= 10.0, "the dailies are worth 7-10 pulls a month");
		// A featured 5-star (2% with pity at 100, 50/50 with a guarantee) is about 65 pulls on average: a free player gets one
		// every month or two, the Mileage spark (250) as a backstop - the NIKKE / ZZZ rhythm.
		Expect(Total * 2.0 > 65.0 && Total < 65.0, "a featured 5-star takes a free player one to two months");

		// Paid value: the biggest pack about $2.80 a pull, the Monthly supply the best value.
		const std::vector<Meta::FIapOffer>& Offers = Meta::IapOffers();
		double BestPack = 1e9;
		double MonthlyValue = 0.0;
		for (const Meta::FIapOffer& Offer : Offers)
		{
			if (Offer.Reward.Diamonds > 0 && !Offer.bOncePerProfile)
			{
				BestPack = std::min(BestPack, Offer.PriceCents / (Offer.Reward.Diamonds / 300.0));
			}
			if (Offer.bMonthly)
			{
				MonthlyValue = Offer.PriceCents / ((Inventory::MonthlyInstantDiamonds + Inventory::MonthlyDays * Inventory::MonthlyDailyDiamonds) / 300.0);
			}
		}
		Expect(BestPack > 250.0 && BestPack < 320.0, "the best Diamond pack is about $2.50-3.20 a pull");
		Expect(MonthlyValue > 0.0 && MonthlyValue < BestPack / 3.0, "the Monthly supply is the best value by far");
	}

	// ================================================================== saves
	void TestSaves()
	{
		FProfile Profile = NewPlayer(51);
		Profile.Add(ECurrency::ElementCore, 123);
		Profile.Add(ECurrency::EventToken, 45);
		Profile.Add(ECurrency::ArkMedal, 678);
		Profile.SetRaw(ECurrency::Charge, 199);
		Inventory::Add(Profile, EItem::BytesBoost24h, 3);
		Inventory::Add(Profile, EItem::ChargePack, 2);
		Profile.Operators[static_cast<size_t>(FindOperator("Luna"))].Attunement = 4;
		Profile.Meta.MonthlyDays = 17;
		Profile.Meta.ChargeUnix = 1791072000LL;
		Profile.Meta.ChargeRefillsToday = 2;
		Meta::NotifyQuest(Profile, Meta::EQuestEvent::Summon, 3);
		Events::ClaimRookie(Profile, 1);
		Events::BuyArkShop(Profile, 2);
		FMail Mail;
		Mail.Id = 999;
		Mail.Title = "Test";
		Mail.Reward.ElementCores = 5;
		Mail.Reward.Item = static_cast<int>(EItem::GoldBoost6h);
		Mail.Reward.ItemAmount = 2;
		Profile.Meta.Mail.push_back(Mail);
		const std::string Text = Save::Serialize(Profile, 1791072000LL);
		FProfile Loaded(52);
		Loaded.Account = Profile.Account;
		std::string Error;
		Expect(Save::Deserialize(Text, Loaded, Error), "save loads: " + Error);
		Expect(Loaded.Get(ECurrency::ElementCore) == Profile.Get(ECurrency::ElementCore) && Loaded.Get(ECurrency::EventToken) == 45 && Loaded.Get(ECurrency::ArkMedal) == Profile.Get(ECurrency::ArkMedal)
			&& Loaded.Get(ECurrency::Charge) == 199, "new currencies saved");
		Expect(Inventory::Count(Loaded, EItem::BytesBoost24h) == 3 && Inventory::Count(Loaded, EItem::ChargePack) == 2, "items saved");
		Expect(Loaded.Operators[static_cast<size_t>(FindOperator("Luna"))].Attunement == 4, "Attunement saved");
		Expect(Loaded.Meta.MonthlyDays == 17 && Loaded.Meta.ChargeUnix == 1791072000LL && Loaded.Meta.ChargeRefillsToday == 2, "Charge clock and Monthly supply saved");
		Expect(Loaded.Meta.Events.RookieStartDay == Profile.Meta.Events.RookieStartDay && Loaded.Meta.Events.RookieClaimed[1] == Profile.Meta.Events.RookieClaimed[1]
			&& Loaded.Meta.Events.Counters[static_cast<int>(Events::EStat::Summon)] == 3 && Loaded.Meta.Events.ArkShopBought[2] == 1, "event state saved");
		bool bMail = false;
		for (const FMail& Saved : Loaded.Meta.Mail)
		{
			bMail = bMail || (Saved.Id == 999 && Saved.Reward.ElementCores == 5 && Saved.Reward.Item == static_cast<int>(EItem::GoldBoost6h) && Saved.Reward.ItemAmount == 2);
		}
		Expect(bMail, "mail rewards keep the new fields");
	}

	// ================================================================== pages
	void TestPages()
	{
		FProfile Profile = NewPlayer(61);
		P::FUiState S;
		S.Tab = P::TabLobby;
		P::FPage Page = P::Build(Profile, S);
		Expect(HasTile(Page, "go:inventory") && HasTile(Page, "go:elements") && HasTile(Page, "go:rookie"), "lobby: Inventory, Elements, Rookie week");
		S.Page = "inventory";
		Inventory::Add(Profile, EItem::GoldBoost3h, 1);
		Page = P::Build(Profile, S);
		Expect(Page.Grid.size() == static_cast<size_t>(ItemCount) && HasTile(Page, "gx:refill"), "inventory: every item and the refill button");
		bool bChargeLine = false;
		for (const std::string& Line : Page.Lines)
		{
			bChargeLine = bChargeLine || Contains(Line, "Charge ");
		}
		Expect(bChargeLine, "inventory shows Charge");
		const int Gold = Profile.Get(ECurrency::Gold);
		P::FPerform Out = P::Perform(Profile, S, "gx:use:0");
		Expect(Out.bOk && Profile.Get(ECurrency::Gold) > Gold, "use a boost from the page");
		for (const char* Name : {"elements", "events", "rookie", "event-ops", "event-shop"})
		{
			S.Page = Name;
			Page = P::Build(Profile, S);
			Expect(!Page.Title.empty() && !Page.Grid.empty(), std::string("page builds: ") + Name);
		}
		S.Page = "elements";
		Expect(P::Build(Profile, S).Grid.size() == static_cast<size_t>(ElementCount), "element guide: eight tiles");
		S.Page = "rookie";
		Expect(P::Build(Profile, S).Grid.size() == static_cast<size_t>(RookieMissionCount), "Rookie week: 21 missions");
		S.Page = "event-ops";
		Expect(P::Perform(Profile, S, "back").bOk && S.Page == "events", "event pages go back to Events");
		S.Tab = P::TabRevolution;
		S.Page = "ark";
		Expect(HasTile(P::Build(Profile, S), "go:ark-shop"), "the Ark has a shop");
		S.Page = "ark-shop";
		Expect(P::Build(Profile, S).Grid.size() == static_cast<size_t>(ArkShopCount), "Ark shop: eight goods");

		// operator page: element and Attunement
		S.Tab = P::TabOperator;
		S.Page = "op";
		S.SelOperator = FindOperator("Luna");
		Page = P::Build(Profile, S);
		bool bElement = false;
		for (const std::string& Line : Page.Lines)
		{
			bElement = bElement || Contains(Line, "Element: Dark");
		}
		Expect(bElement && HasTile(Page, "gx:attune"), "operator page: element line and Attune tile");

		// campaign: enemy element, Charge, quick clear
		S.Tab = P::TabCampaign;
		S.Page = "chapter";
		S.SelChapter = 1;
		Profile.SetRaw(ECurrency::Charge, 100);
		Page = P::Build(Profile, S);
		Expect(HasTile(Page, "camp:sweep:1:1") && HasTile(Page, "gx:inv"), "chapter page: quick clear and Charge buttons");
		Out = P::Perform(Profile, S, "camp:sweep:1:5");
		Expect(Out.bOk && Profile.Get(ECurrency::Charge) == 50, "quick clear x5 from the page");
		Out = P::Perform(Profile, S, "gx:inv");
		Expect(S.Tab == P::TabLobby && S.Page == "inventory", "the Charge button opens the inventory");
		S.Tab = P::TabCampaign;
		S.Page = "chapter";
		S.SelChapter = 3;
		Profile.StagesCleared = 16;
		Page = P::Build(Profile, S);
		bool bEnemyLine = false;
		for (const std::string& Line : Page.Lines)
		{
			bEnemyLine = bEnemyLine || Contains(Line, "Enemies: ");
		}
		Expect(bEnemyLine, "chapter page: enemy element and counter");

		// every currency has an icon and appears in the top-bar choice
		for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
		{
			Expect(std::strlen(P::CurrencyIcon(static_cast<ECurrency>(Index))) > 0, std::string("icon for ") + CurrencyName(static_cast<ECurrency>(Index)));
		}
		Expect(TopBarChoiceCount == static_cast<int>(ECurrency::Count), "the top bar can show every currency");
	}
}

int main(int argc, char** argv)
{
	const bool bBudget = argc > 1 && std::string(argv[1]) == "--budget";
	TestElements();
	TestInventory();
	TestCharge();
	TestEvents();
	TestPullBudget(bBudget);
	TestSaves();
	TestPages();
	std::printf("%d gacha/element/inventory/event checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
