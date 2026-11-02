// Boldinbeauty demo — engine-independent game rules (see BIBCore.h).
#include "BIBCore.h"
#include "BIBCampaign.h"
#include "BIBConfig.h"
#include "BIBEvents.h"
#include "BIBInventory.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace BIB
{
	// ================================================================= roster
	const std::vector<FRosterEntry>& Roster()
	{
		// MAIN §5 — 20 operators. Stats written by the owner exist for the three bespoke kits:
		//   Luna.md      "Starting stat values: 4275 health 783 reg attack base"
		//   Judge.md     "1666 health" ; "dmg lvl 1-200" read as 200 regular-attack damage (WORKING)
		//   Narrator.md  "Reg attack 272 Damage 999 Health"
		// The other 17 are data-driven kits (BIBKitData*.cpp). Their health/attack come from those tables: PLACEHOLDER
		// for Alchemist, Arita and Adapt (the owner wrote no stats) and PROPOSED for the 14 designed on 2026-10-04.
		static const std::vector<FRosterEntry> Entries = {
			{"Myrcia", 3, EKit::Myrcia, 3000, 430},
			{"Fujita", 3, EKit::Fujita, 1700, 220},
			{"Ishii", 3, EKit::Ishii, 3000, 330},
			{"Shiraishi", 3, EKit::Shiraishi, 1400, 140},
			{"Arita", 3, EKit::Arita, 2000, 240},
			{"Luna", 4, EKit::Luna, 4275, 783},
			{"Plack", 4, EKit::Plack, 3800, 540},
			{"Kuu", 4, EKit::Kuu, 5200, 500},
			{"Alchemist", 5, EKit::Alchemist, 5200, 1300},
			{"Blood", 5, EKit::Blood, 3600, 420},
			{"Stargazer", 5, EKit::Stargazer, 1900, 440},
			{"Gambler", 5, EKit::Gambler, 2600, 440},
			{"Weather", 5, EKit::Weather, 3000, 330},
			{"Judge", 5, EKit::Judge, 1666, 200},
			{"Narrator", 6, EKit::Narrator, 999, 272},
			{"Jester", 6, EKit::Jester, 4200, 560},
			{"Media", 6, EKit::Media, 2000, 335},
			{"Timekeeper", 6, EKit::Timekeeper, 2600, 280},
			{"Death", 6, EKit::Death, 3400, 950},
			{"Adapt", 6, EKit::Adapt, 5000, 640},
		};
		return Entries;
	}

	int FindOperator(const char* Name)
	{
		const std::vector<FRosterEntry>& Entries = Roster();
		for (int Index = 0; Index < static_cast<int>(Entries.size()); ++Index)
		{
			if (std::string(Entries[Index].Name) == Name)
			{
				return Index;
			}
		}
		return -1;
	}

	const char* StarsLabel(int Stars)
	{
		switch (Stars)
		{
		case 3: return "3* Rare";
		case 4: return "4* Epic";
		case 5: return "5* Legendary";
		case 6: return "6* Secret";
		default: return "?";
		}
	}

	const char* CurrencyName(ECurrency C)
	{
		switch (C)
		{
		case ECurrency::Gold: return "Gold";
		case ECurrency::Diamonds: return "Diamonds";
		case ECurrency::Bytes: return "Bytes";
		case ECurrency::AngelHearts: return "Angel hearts";
		case ECurrency::RegularTicket: return "Regular tickets";
		case ECurrency::SpecialTicket: return "Special tickets";
		case ECurrency::DailyTicket: return "Daily ticket";
		case ECurrency::Mileage: return "Mileage";
		case ECurrency::ArcadeToken: return "Arcade tokens";
		case ECurrency::GangCoin: return "Gang coins";
		case ECurrency::Charge: return "Charge";
		case ECurrency::ElementCore: return "Element cores";
		case ECurrency::EventToken: return "Event tokens";
		case ECurrency::ArkMedal: return "Ark medals";
		default: return "?";
		}
	}

	const char* SlotName(ESlot S)
	{
		switch (S)
		{
		case ESlot::Helmet: return "Helmet";
		case ESlot::Chest: return "Chest plate";
		case ESlot::Leggings: return "Leggings";
		case ESlot::Weapon: return "Weapon";
		default: return "?";
		}
	}

	// ================================================================ profile
	FProfile::FProfile(uint32_t Seed)
		: Random(Seed)
	{
		ResetToNewGame();
	}

	void FProfile::PlaceDefaultBanners()
	{
		Banners.clear();
		Banners.push_back({"standard", "Standard banner", EBannerType::Standard, -1, 0});
		// Owner choice for the demo (2026-10-02): Judge featured 5*, Narrator featured 6*. Since 2026-10-04 these two
		// Featured banners rotate through their rarity's pool every 3 days (ApplyRotation fills in the operator and title).
		Banners.push_back({"featured-5", "Featured (5*)", EBannerType::Featured, -1, 5});
		Banners.push_back({"featured-6", "Featured (6*)", EBannerType::Featured, -1, 6});
		Banners.push_back({"daily", "Daily banner", EBannerType::Daily, -1, 0});
		Pity.assign(Banners.size(), FPityState());
		ApplyRotation(false);
	}

	void FProfile::ResetToNewGame()
	{
		for (int& Value : Currencies)
		{
			Value = 0;
		}
		// PLACEHOLDER starting wallet so every screen can be exercised.
		Currencies[static_cast<int>(ECurrency::Gold)] = 20000;
		Currencies[static_cast<int>(ECurrency::Diamonds)] = 3000;
		Currencies[static_cast<int>(ECurrency::Bytes)] = 10000;
		Currencies[static_cast<int>(ECurrency::AngelHearts)] = 100;
		Currencies[static_cast<int>(ECurrency::RegularTicket)] = 10;
		Currencies[static_cast<int>(ECurrency::SpecialTicket)] = 10;
		Currencies[static_cast<int>(ECurrency::DailyTicket)] = 1;

		Operators.assign(Roster().size(), FOwnedOperator());
		Inventory.clear();
		NextEquipmentId = 1;
		bTutorialSummonDone = false;
		TutorialStep = static_cast<int>(ETutorialStep::Complete); // the developer profile has no tutorial; StartNewPlayer() sets one
		bUnlimited = false;
		Day = 1;
		StagesCleared = 0;
		Meta = MakeNewMeta();
		Currencies[static_cast<int>(ECurrency::ArcadeToken)] = 5;
		Currencies[static_cast<int>(ECurrency::Charge)] = Inventory::ChargeCap;   // the developer profile starts full

		// MAIN §5: Luna is granted automatically in Chapter 1.
		const int LunaIndex = FindOperator("Luna");
		GrantOperator(LunaIndex);
		// Demo grant (owner chose Luna/Judge/Narrator as the playable team).
		const int JudgeIndex = FindOperator("Judge");
		const int NarratorIndex = FindOperator("Narrator");
		GrantOperator(JudgeIndex);
		GrantOperator(NarratorIndex);
		Team[0] = LunaIndex;
		Team[1] = JudgeIndex;
		Team[2] = NarratorIndex;

		AddRandomEquipment();
		AddRandomEquipment();
		// Keep the clock across a reset: the banners must show the current rotation period, not period 0.
		RotationIndex = NowUnix > 0 ? RotationIndexAt(NowUnix) : 0;
		PlaceDefaultBanners();
	}

	int FProfile::Get(ECurrency C) const
	{
		return bUnlimited ? Config::UnlimitedAmount : Currencies[static_cast<int>(C)];
	}

	int FProfile::Raw(ECurrency C) const
	{
		return Currencies[static_cast<int>(C)];
	}

	void FProfile::SetRaw(ECurrency C, int Amount)
	{
		Currencies[static_cast<int>(C)] = std::max(0, Amount);
	}

	void FProfile::Add(ECurrency C, int Amount)
	{
		if (bUnlimited)
		{
			return;
		}
		Currencies[static_cast<int>(C)] += Amount;
	}

	bool FProfile::Spend(ECurrency C, int Amount)
	{
		if (Amount < 0)
		{
			return false;
		}
		if (bUnlimited)
		{
			return true;
		}
		if (Currencies[static_cast<int>(C)] < Amount)
		{
			return false;
		}
		Currencies[static_cast<int>(C)] -= Amount;
		return true;
	}

	bool FProfile::Owns(int RosterIndex) const
	{
		return RosterIndex >= 0 && RosterIndex < static_cast<int>(Operators.size()) && Operators[RosterIndex].bOwned;
	}

	bool FProfile::GrantOperator(int RosterIndex)
	{
		if (RosterIndex < 0 || RosterIndex >= static_cast<int>(Operators.size()))
		{
			return false;
		}
		FOwnedOperator& Owned = Operators[RosterIndex];
		if (Owned.bOwned)
		{
			Owned.DupResource += Config::DuplicateResource;
			return false;
		}
		Owned = FOwnedOperator();
		Owned.bOwned = true;
		return true;
	}

	void FProfile::GrantAllOperators()
	{
		for (int Index = 0; Index < static_cast<int>(Operators.size()); ++Index)
		{
			if (!Operators[static_cast<size_t>(Index)].bOwned)
			{
				Operators[static_cast<size_t>(Index)] = FOwnedOperator();
				Operators[static_cast<size_t>(Index)].bOwned = true;
			}
		}
	}

	// ============================================================ progression
	int FProfile::LevelCap(int RosterIndex) const
	{
		if (!Owns(RosterIndex))
		{
			return Config::BaseLevelCap;
		}
		return Config::BaseLevelCap + Config::CapPerUpgradeStar * Operators[RosterIndex].UpgradeStars;
	}

	FCost FProfile::LevelUpCost(int FromLevel)
	{
		FCost Cost;
		Cost.Gold = Config::LevelGoldPerLevel * FromLevel;
		Cost.Bytes = Config::LevelBytesPerLevel * FromLevel;
		return Cost;
	}

	int FProfile::HeartGateCost(int GateNumber)
	{
		// "every 20 levels you need to spend angel hearts and the amount double each 20 levels
		//  and at 100 it only 1.25x starting at 30" → 30, 60, 120, 240, 300, 375, 468.75 …
		// Rounding of fractional costs is OPEN; the demo rounds up (PLACEHOLDER).
		if (GateNumber < 1)
		{
			return 0;
		}
		double Cost = Config::HeartGateFirstCost;
		for (int Gate = 2; Gate <= GateNumber; ++Gate)
		{
			const int GateLevel = Gate * Config::HeartGateEvery;
			Cost *= (GateLevel < 100) ? 2.0 : 1.25;
		}
		return static_cast<int>(std::ceil(Cost - 1e-9));
	}

	bool FProfile::NeedsHeartGate(int RosterIndex) const
	{
		if (!Owns(RosterIndex))
		{
			return false;
		}
		const FOwnedOperator& Owned = Operators[RosterIndex];
		return Owned.Level % Config::HeartGateEvery == 0 && Owned.HeartGatesPaid < Owned.Level / Config::HeartGateEvery;
	}

	int FProfile::UpgradeStarCost(int TargetStar)
	{
		return Config::UpgradeStarCostStep * TargetStar;
	}

	int FProfile::DiamondsToCover(int GoldShort, int BytesShort)
	{
		// 1 Diamond = 35 Money OR 60 Bytes (supplement). Partial-Diamond rounding is OPEN → round up.
		const int ForGold = GoldShort > 0 ? (GoldShort + Config::MoneyPerDiamond - 1) / Config::MoneyPerDiamond : 0;
		const int ForBytes = BytesShort > 0 ? (BytesShort + Config::BytesPerDiamond - 1) / Config::BytesPerDiamond : 0;
		return ForGold + ForBytes;
	}

	FActionResult FProfile::TryLevelUp(int RosterIndex, bool bSupplementWithDiamonds)
	{
		FActionResult Result;
		if (!Owns(RosterIndex))
		{
			Result.Message = "Operator not owned.";
			return Result;
		}
		FOwnedOperator& Owned = Operators[RosterIndex];
		if (Owned.Level >= LevelCap(RosterIndex))
		{
			Result.Message = "Level cap reached - raise it with duplicate upgrade stars.";
			return Result;
		}
		if (NeedsHeartGate(RosterIndex))
		{
			Result.Message = "Angel-heart gate: pay " + std::to_string(HeartGateCost(Owned.Level / Config::HeartGateEvery)) + " Angel hearts first.";
			return Result;
		}
		// The tutorial's "level Luna up once" step is free: a new account owns no Gold or Bytes (owner request 2026-10-06).
		const FCost Cost = TutorialStep == static_cast<int>(ETutorialStep::LevelUpLuna) ? FCost() : LevelUpCost(Owned.Level);
		const int GoldShort = std::max(0, Cost.Gold - Get(ECurrency::Gold));
		const int BytesShort = std::max(0, Cost.Bytes - Get(ECurrency::Bytes));
		int DiamondCost = 0;
		if (GoldShort > 0 || BytesShort > 0)
		{
			if (!bSupplementWithDiamonds)
			{
				Result.Message = "Not enough Gold/Bytes (short " + std::to_string(GoldShort) + " Gold, " + std::to_string(BytesShort) + " Bytes).";
				return Result;
			}
			DiamondCost = DiamondsToCover(GoldShort, BytesShort);
			if (Get(ECurrency::Diamonds) < DiamondCost)
			{
				Result.Message = "Not enough Diamonds to supplement (" + std::to_string(DiamondCost) + " needed).";
				return Result;
			}
		}
		Spend(ECurrency::Diamonds, DiamondCost);
		Spend(ECurrency::Gold, Cost.Gold - GoldShort);
		Spend(ECurrency::Bytes, Cost.Bytes - BytesShort);
		++Owned.Level;
		Result.bOk = true;
		Result.Message = std::string(Roster()[RosterIndex].Name) + " reached level " + std::to_string(Owned.Level) +
			(DiamondCost > 0 ? " (" + std::to_string(DiamondCost) + " Diamonds supplemented)" : std::string());
		return Result;
	}

	FActionResult FProfile::TryPayHeartGate(int RosterIndex)
	{
		FActionResult Result;
		if (!NeedsHeartGate(RosterIndex))
		{
			Result.Message = "No Angel-heart gate due.";
			return Result;
		}
		FOwnedOperator& Owned = Operators[RosterIndex];
		const int GateNumber = Owned.Level / Config::HeartGateEvery;
		const int Cost = HeartGateCost(GateNumber);
		if (!Spend(ECurrency::AngelHearts, Cost))
		{
			Result.Message = "Need " + std::to_string(Cost) + " Angel hearts.";
			return Result;
		}
		Owned.HeartGatesPaid = GateNumber;
		Result.bOk = true;
		Result.Message = "Gate at level " + std::to_string(Owned.Level) + " opened for " + std::to_string(Cost) + " Angel hearts.";
		return Result;
	}

	FActionResult FProfile::TryUpgradeStar(int RosterIndex)
	{
		FActionResult Result;
		if (!Owns(RosterIndex))
		{
			Result.Message = "Operator not owned.";
			return Result;
		}
		FOwnedOperator& Owned = Operators[RosterIndex];
		if (Config::MaxUpgradeStars > 0 && Owned.UpgradeStars >= Config::MaxUpgradeStars)
		{
			Result.Message = "Maximum upgrade stars reached.";
			return Result;
		}
		const int Cost = UpgradeStarCost(Owned.UpgradeStars + 1);
		if (!bUnlimited && Owned.DupResource < Cost)
		{
			Result.Message = "Need " + std::to_string(Cost) + " duplicate units (have " + std::to_string(Owned.DupResource) + ").";
			return Result;
		}
		if (!bUnlimited)
		{
			Owned.DupResource -= Cost;
		}
		++Owned.UpgradeStars;
		Result.bOk = true;
		Result.Message = std::string(Roster()[RosterIndex].Name) + " upgrade star " + std::to_string(Owned.UpgradeStars) +
			": level cap now " + std::to_string(LevelCap(RosterIndex)) + ".";
		return Result;
	}

	FCombatStats FProfile::GetCombatStats(int RosterIndex) const
	{
		FCombatStats Stats;
		if (RosterIndex < 0 || RosterIndex >= static_cast<int>(Roster().size()))
		{
			return Stats;
		}
		const FRosterEntry& Entry = Roster()[RosterIndex];
		const FOwnedOperator Owned = Owns(RosterIndex) ? Operators[RosterIndex] : FOwnedOperator();
		const double LevelMult = 1.0 + (Owned.Level - 1) * Config::StatPercentPerLevel / 100.0;
		double EquipDamage = 0.0;
		double EquipDefence = 0.0;
		for (const FEquipment& Item : Inventory)
		{
			if (Item.EquippedBy != RosterIndex)
			{
				continue;
			}
			const double Value = Item.Percent + Item.UpgradeLevel * Config::EquipUpgradePercent;
			if (Item.Bonus == EBonus::Damage)
			{
				EquipDamage += Value;
			}
			else
			{
				EquipDefence += Value;
			}
		}
		if (Entry.Kit == EKit::Judge)
		{
			EquipDamage *= Config::JudgeArmourFactor;
			EquipDefence *= Config::JudgeArmourFactor;
		}
		Stats.MaxHP = Entry.BaseHP * LevelMult;
		Stats.DamageMult = LevelMult * (1.0 + (Owned.UpgradeStars * Config::UpgradeStarDamagePercent + EquipDamage) / 100.0);
		Stats.DefencePercent = std::min(Config::DefenceCapPercent, Owned.UpgradeStars * Config::UpgradeStarDefencePercent + EquipDefence);
		return Stats;
	}

	// ============================================================== equipment
	FEquipment& FProfile::AddRandomEquipment()
	{
		FEquipment Item;
		Item.Id = NextEquipmentId++;
		Item.Slot = static_cast<ESlot>(std::uniform_int_distribution<int>(0, static_cast<int>(ESlot::Count) - 1)(Random));
		// Weapons add damage; armour adds defence OR damage (MAIN §4).
		Item.Bonus = Item.Slot == ESlot::Weapon ? EBonus::Damage
			: (std::uniform_int_distribution<int>(0, 1)(Random) == 0 ? EBonus::Damage : EBonus::Defence);
		const int HalfSteps = std::uniform_int_distribution<int>(
			static_cast<int>(Config::EquipMinPercent * 2.0), static_cast<int>(Config::EquipMaxPercent * 2.0))(Random);
		Item.Percent = HalfSteps / 2.0;
		Item.Name = std::string(SlotName(Item.Slot)) + " #" + std::to_string(Item.Id);
		MakeRoomForEquipment();
		Inventory.push_back(Item);
		return Inventory.back();
	}

	FEquipment& FProfile::AddEquipment(ESlot Slot, EBonus Bonus, double Percent, const std::string& Name)
	{
		FEquipment Item;
		Item.Id = NextEquipmentId++;
		Item.Slot = Slot;
		Item.Bonus = Bonus;
		Item.Percent = Percent;
		Item.Name = Name + " #" + std::to_string(Item.Id);
		MakeRoomForEquipment();
		Inventory.push_back(Item);
		return Inventory.back();
	}

	void FProfile::MakeRoomForEquipment()
	{
		while (static_cast<int>(Inventory.size()) >= Config::EquipmentInventoryCap)
		{
			int Weakest = -1;
			for (int Index = 0; Index < static_cast<int>(Inventory.size()); ++Index)
			{
				const FEquipment& Item = Inventory[static_cast<size_t>(Index)];
				if (Item.EquippedBy >= 0 || Item.UpgradeLevel > 0)
				{
					continue;
				}
				if (Weakest < 0 || Item.Percent < Inventory[static_cast<size_t>(Weakest)].Percent)
				{
					Weakest = Index;
				}
			}
			if (Weakest < 0)
			{
				return;   // everything is equipped or upgraded: keep it all
			}
			Inventory.erase(Inventory.begin() + Weakest);
		}
	}

	FEquipment* FProfile::FindEquipment(int EquipmentId)
	{
		for (FEquipment& Item : Inventory)
		{
			if (Item.Id == EquipmentId)
			{
				return &Item;
			}
		}
		return nullptr;
	}

	const FEquipment* FProfile::EquippedIn(int RosterIndex, ESlot Slot) const
	{
		for (const FEquipment& Item : Inventory)
		{
			if (Item.EquippedBy == RosterIndex && Item.Slot == Slot)
			{
				return &Item;
			}
		}
		return nullptr;
	}

	FActionResult FProfile::Equip(int EquipmentId, int RosterIndex)
	{
		FActionResult Result;
		FEquipment* Item = FindEquipment(EquipmentId);
		if (!Item || !Owns(RosterIndex))
		{
			Result.Message = "Cannot equip.";
			return Result;
		}
		for (FEquipment& Other : Inventory)
		{
			if (Other.EquippedBy == RosterIndex && Other.Slot == Item->Slot)
			{
				Other.EquippedBy = -1;
			}
		}
		Item->EquippedBy = RosterIndex;
		Result.bOk = true;
		Result.Message = Item->Name + " equipped on " + Roster()[RosterIndex].Name + ".";
		return Result;
	}

	FActionResult FProfile::Unequip(int EquipmentId)
	{
		FActionResult Result;
		FEquipment* Item = FindEquipment(EquipmentId);
		if (!Item || Item->EquippedBy < 0)
		{
			Result.Message = "Not equipped.";
			return Result;
		}
		Item->EquippedBy = -1;
		Result.bOk = true;
		Result.Message = Item->Name + " unequipped.";
		return Result;
	}

	int FProfile::EquipmentUpgradeCost(int CurrentUpgradeLevel)
	{
		return Config::EquipUpgradeBytesStep * (CurrentUpgradeLevel + 1);
	}

	FActionResult FProfile::UpgradeEquipment(int EquipmentId)
	{
		FActionResult Result;
		FEquipment* Item = FindEquipment(EquipmentId);
		if (!Item)
		{
			Result.Message = "No such item.";
			return Result;
		}
		if (Item->UpgradeLevel >= Config::EquipMaxUpgrade)
		{
			Result.Message = "Item fully upgraded.";
			return Result;
		}
		const int Cost = EquipmentUpgradeCost(Item->UpgradeLevel);
		if (!Spend(ECurrency::Bytes, Cost))
		{
			Result.Message = "Need " + std::to_string(Cost) + " Bytes.";
			return Result;
		}
		++Item->UpgradeLevel;
		Result.bOk = true;
		Result.Message = Item->Name + " upgraded to +" + std::to_string(Item->UpgradeLevel) + ".";
		return Result;
	}

	// =================================================================== team
	FActionResult FProfile::SetTeamSlot(int Slot, int RosterIndex)
	{
		FActionResult Result;
		if (Slot < 0 || Slot > 2 || !Owns(RosterIndex) || Roster()[RosterIndex].Kit == EKit::None)
		{
			Result.Message = "Only owned operators can join the team.";
			return Result;
		}
		for (int Other = 0; Other < 3; ++Other)
		{
			if (Other != Slot && Team[Other] == RosterIndex)
			{
				Team[Other] = Team[Slot]; // swap
			}
		}
		Team[Slot] = RosterIndex;
		Result.bOk = true;
		Result.Message = std::string(Roster()[RosterIndex].Name) + " placed in slot " + std::to_string(Slot + 1) + ".";
		return Result;
	}

	int FProfile::PlayableOwnedCount() const
	{
		int Count = 0;
		for (int Index = 0; Index < static_cast<int>(Operators.size()); ++Index)
		{
			if (Owns(Index) && Roster()[static_cast<size_t>(Index)].Kit != EKit::None)
			{
				++Count;
			}
		}
		return Count;
	}

	int FProfile::RequiredTeamSize() const
	{
		return std::min(3, std::max(1, PlayableOwnedCount()));
	}

	bool FProfile::TeamIsPlayable() const
	{
		int Filled = 0;
		for (int Slot = 0; Slot < 3; ++Slot)
		{
			const int RosterIndex = Team[Slot];
			if (RosterIndex < 0)
			{
				continue;
			}
			if (!Owns(RosterIndex) || Roster()[static_cast<size_t>(RosterIndex)].Kit == EKit::None)
			{
				return false;
			}
			for (int Other = 0; Other < Slot; ++Other)
			{
				if (Team[Other] == RosterIndex)
				{
					return false;
				}
			}
			++Filled;
		}
		return Filled >= RequiredTeamSize();
	}

	bool FProfile::AutoPlaceInTeam(int RosterIndex)
	{
		if (!Owns(RosterIndex) || Roster()[static_cast<size_t>(RosterIndex)].Kit == EKit::None)
		{
			return false;
		}
		for (int Slot = 0; Slot < 3; ++Slot)
		{
			if (Team[Slot] == RosterIndex)
			{
				return false;
			}
		}
		for (int Slot = 0; Slot < 3; ++Slot)
		{
			if (Team[Slot] < 0)
			{
				Team[Slot] = RosterIndex;
				return true;
			}
		}
		return false;
	}

	// ============================================================== summoning
	double FProfile::Roll01()
	{
		return std::uniform_real_distribution<double>(0.0, 1.0)(Random);
	}

	int FProfile::PickUniform(const std::vector<int>& Candidates)
	{
		if (Candidates.empty())
		{
			return -1;
		}
		return Candidates[std::uniform_int_distribution<int>(0, static_cast<int>(Candidates.size()) - 1)(Random)];
	}

	std::vector<int> FProfile::OperatorsOfStars(int Stars, int Excluding) const
	{
		// Eligible pools are OPEN — the demo treats every roster operator of the rarity as eligible.
		std::vector<int> Result;
		const std::vector<FRosterEntry>& Entries = Roster();
		for (int Index = 0; Index < static_cast<int>(Entries.size()); ++Index)
		{
			if (Entries[Index].Stars == Stars && Index != Excluding)
			{
				Result.push_back(Index);
			}
		}
		return Result;
	}

	int FProfile::RollRarity()
	{
		const double R = Roll01();
		if (R < Config::Rate6)
		{
			return 6;
		}
		if (R < Config::Rate6 + Config::Rate5)
		{
			return 5;
		}
		if (R < Config::Rate6 + Config::Rate5 + Config::Rate4)
		{
			return 4;
		}
		return 3;
	}

	int FProfile::FindBanner(const std::string& Id) const
	{
		for (int Index = 0; Index < static_cast<int>(Banners.size()); ++Index)
		{
			if (Banners[Index].Id == Id)
			{
				return Index;
			}
		}
		return -1;
	}

	FSummonResult FProfile::RollOne(int BannerIndex, bool bCountPity, bool bForceAtLeastEpic)
	{
		const FBanner& Banner = Banners[BannerIndex];
		FPityState& State = Pity[BannerIndex];
		FSummonResult Out;
		Out.Stars = RollRarity();

		if (bCountPity)
		{
			++State.Since5;
			++State.Since6;
			if (State.Since6 >= Config::Pity6Rolls && Out.Stars < 6)
			{
				Out.Stars = 6;
				Out.bPity = true;
			}
			else if (State.Since5 >= Config::Pity5Rolls && Out.Stars < 5)
			{
				Out.Stars = 5;
				Out.bPity = true;
			}
		}
		if (bForceAtLeastEpic && Out.Stars < 4)
		{
			Out.Stars = 4;
			Out.bTenPullGuarantee = true;
		}
		if (bCountPity)
		{
			// Counter resets after a high-rarity result. Whether a 6* also satisfies 5* pity is OPEN;
			// the demo resets both (PLACEHOLDER).
			if (Out.Stars >= 6)
			{
				State.Since6 = 0;
				State.Since5 = 0;
			}
			else if (Out.Stars == 5)
			{
				State.Since5 = 0;
			}
		}

		const bool bFeaturedRarity = Banner.Type == EBannerType::Featured && Banner.FeaturedOperator >= 0 &&
			Roster()[Banner.FeaturedOperator].Stars == Out.Stars;
		if (bFeaturedRarity)
		{
			if (State.bLostFiftyFifty)
			{
				// WORKING: after a lost 50/50, the next result of the featured rarity is the featured operator.
				Out.Operator = Banner.FeaturedOperator;
				Out.bFiftyFiftyGuarantee = true;
				State.bLostFiftyFifty = false;
			}
			else if (Roll01() < Config::FeaturedShare)
			{
				Out.Operator = Banner.FeaturedOperator;
			}
			else
			{
				Out.Operator = PickUniform(OperatorsOfStars(Out.Stars, Banner.FeaturedOperator));
				if (Out.Operator < 0)
				{
					Out.Operator = Banner.FeaturedOperator;
				}
				else
				{
					State.bLostFiftyFifty = true;
				}
			}
			Out.bFeatured = Out.Operator == Banner.FeaturedOperator;
		}
		else
		{
			// Standard/Daily: equal split within the rarity.
			Out.Operator = PickUniform(OperatorsOfStars(Out.Stars, -1));
		}
		Out.bNew = GrantOperator(Out.Operator);
		return Out;
	}

	FSummonOutcome FProfile::Summon(int BannerIndex, int Count, EPay Pay)
	{
		FSummonOutcome Outcome;
		if (BannerIndex < 0 || BannerIndex >= static_cast<int>(Banners.size()) || (Count != 1 && Count != 10))
		{
			Outcome.Message = "Invalid summon request.";
			return Outcome;
		}
		const FBanner& Banner = Banners[BannerIndex];
		if (Banner.Type == EBannerType::Daily)
		{
			if (Count != 1)
			{
				Outcome.Message = "Daily banner: single summons only (tickets cannot be saved for a 10x).";
				return Outcome;
			}
			if (Pay != EPay::Ticket)
			{
				Outcome.Message = "Daily banner cannot use Diamonds.";
				return Outcome;
			}
			if (!Spend(ECurrency::DailyTicket, 1))
			{
				Outcome.Message = "No Daily ticket today.";
				return Outcome;
			}
			// No pity credit, no Mileage, no 10x guarantee.
			Outcome.Results.push_back(RollOne(BannerIndex, false, false));
			Outcome.bOk = true;
			Outcome.Message = "Daily summon complete.";
			return Outcome;
		}

		const ECurrency TicketType = Banner.Type == EBannerType::Featured ? ECurrency::SpecialTicket : ECurrency::RegularTicket;
		const int PaidRolls = Count == 1 ? 1 : Config::TenPullPaidRolls;
		const bool bPaid = Pay == EPay::Ticket ? Spend(TicketType, PaidRolls) : Spend(ECurrency::Diamonds, PaidRolls * Config::DiamondsPerRoll);
		if (!bPaid)
		{
			Outcome.Message = Pay == EPay::Ticket
				? "Need " + std::to_string(PaidRolls) + " " + CurrencyName(TicketType) + "."
				: "Need " + std::to_string(PaidRolls * Config::DiamondsPerRoll) + " Diamonds.";
			return Outcome;
		}

		bool bHasEpicOrBetter = false;
		for (int Roll = 0; Roll < Count; ++Roll)
		{
			// One Epic (4*) or better per 10-pull. Whether ten singles qualify is OPEN (not applied).
			const bool bForce = Count == 10 && Roll == Count - 1 && !bHasEpicOrBetter;
			FSummonResult One = RollOne(BannerIndex, true, bForce);
			bHasEpicOrBetter = bHasEpicOrBetter || One.Stars >= 4;
			Outcome.Results.push_back(One);
		}
		Add(ECurrency::Mileage, Count * Config::MileagePerResult);
		Outcome.bOk = true;
		Outcome.Message = std::to_string(Count) + "x summon complete (+" + std::to_string(Count) + " Mileage).";
		return Outcome;
	}

	FSummonOutcome FProfile::TutorialSummon()
	{
		FSummonOutcome Outcome;
		if (bTutorialSummonDone)
		{
			Outcome.Message = "Tutorial summon already used.";
			return Outcome;
		}
		// MAIN §5: Kuu comes from the tutorial 1x summon.
		FSummonResult One;
		One.Operator = FindOperator("Kuu");
		One.Stars = Roster()[One.Operator].Stars;
		One.bNew = GrantOperator(One.Operator);
		Outcome.Results.push_back(One);
		bTutorialSummonDone = true;
		Outcome.bOk = true;
		Outcome.Message = "Tutorial summon complete.";
		return Outcome;
	}

	FSummonOutcome FProfile::RedeemMileage(int BannerIndex)
	{
		FSummonOutcome Outcome;
		if (BannerIndex < 0 || BannerIndex >= static_cast<int>(Banners.size()) ||
			Banners[BannerIndex].Type != EBannerType::Featured || Banners[BannerIndex].FeaturedOperator < 0)
		{
			Outcome.Message = "Mileage redemption needs a featured banner.";
			return Outcome;
		}
		const int Featured = Banners[BannerIndex].FeaturedOperator;
		const int Stars = Roster()[Featured].Stars;
		// 250 → featured 5* only while a 5* is featured; 2,500 → featured 6* only while a 6* is featured.
		const int Cost = Stars == 6 ? Config::MileageFeatured6Cost : (Stars == 5 ? Config::MileageFeatured5Cost : -1);
		if (Cost < 0)
		{
			Outcome.Message = "Only featured 5* or 6* operators can be redeemed.";
			return Outcome;
		}
		if (!Spend(ECurrency::Mileage, Cost))
		{
			Outcome.Message = "Need " + std::to_string(Cost) + " Mileage.";
			return Outcome;
		}
		FSummonResult One;
		One.Operator = Featured;
		One.Stars = Stars;
		One.bFeatured = true;
		One.bNew = GrantOperator(Featured);
		Outcome.Results.push_back(One);
		Outcome.bOk = true;
		Outcome.Message = std::string("Redeemed ") + Roster()[Featured].Name + " for " + std::to_string(Cost) + " Mileage.";
		return Outcome;
	}

	void FProfile::AdvanceDay()
	{
		++Day;
		// One Daily ticket per day; an unused ticket expires and never accumulates.
		Currencies[static_cast<int>(ECurrency::DailyTicket)] = 1;
		ResetDaily(Meta, Day);
		// Arcade tokens: a daily top-up to 5 (never reduces tokens won elsewhere).
		int& Tokens = Currencies[static_cast<int>(ECurrency::ArcadeToken)];
		Tokens = std::max(Tokens, 5);
		// Weekly operation "Arcade Fever" (BIBEvents.h): +2 Arcade tokens a day on top of the top-up.
		Events::Refresh(*this);
		if (Events::Theme(*this) == Events::ETheme::ArcadeFever)
		{
			Tokens += Events::ArcadeFeverTokens;
		}
	}

	// =============================================================== merchant
	int FProfile::MerchantOfferCount()
	{
		return 4;
	}

	std::string FProfile::MerchantOfferLabel(int OfferIndex)
	{
		switch (OfferIndex)
		{
		case 0: return "Regular ticket - " + std::to_string(Config::MerchantTicketDiamonds) + " Diamonds";
		case 1: return "Special ticket - " + std::to_string(Config::MerchantTicketDiamonds) + " Diamonds";
		case 2: return std::to_string(Config::MerchantHeartsAmount) + " Angel hearts - " + std::to_string(Config::MerchantHeartsGold) + " Gold";
		case 3: return "Random equipment - " + std::to_string(Config::MerchantEquipmentGold) + " Gold";
		default: return "?";
		}
	}

	FActionResult FProfile::BuyFromMerchant(int OfferIndex)
	{
		FActionResult Result;
		switch (OfferIndex)
		{
		case 0:
		case 1:
			if (!Spend(ECurrency::Diamonds, Config::MerchantTicketDiamonds))
			{
				Result.Message = "Not enough Diamonds.";
				return Result;
			}
			Add(OfferIndex == 0 ? ECurrency::RegularTicket : ECurrency::SpecialTicket, 1);
			break;
		case 2:
			if (!Spend(ECurrency::Gold, Config::MerchantHeartsGold))
			{
				Result.Message = "Not enough Gold.";
				return Result;
			}
			Add(ECurrency::AngelHearts, Config::MerchantHeartsAmount);
			break;
		case 3:
			if (!Spend(ECurrency::Gold, Config::MerchantEquipmentGold))
			{
				Result.Message = "Not enough Gold.";
				return Result;
			}
			AddRandomEquipment();
			break;
		default:
			Result.Message = "Unknown offer.";
			return Result;
		}
		Result.bOk = true;
		Result.Message = "Bought: " + MerchantOfferLabel(OfferIndex);
		return Result;
	}

	// ================================================================= stages
	std::vector<std::string> FProfile::ApplyStageRewards(int StageNumber, bool bChapterFinal)
	{
		// Level rewards are a MAIN-listed source; the quantities are DEFERRED → PLACEHOLDER amounts that rise with the stage
		// (BIBCampaign.h StageReward; docs/campaign.md). Gold, Bytes and Angel hearts are paid on every clear, which is what
		// grinding means; Diamonds only on the first clear of a stage (owner request 2026-10-06); the equipment drop on every
		// clear. Tickets come from the chapter quests (Meta::StoryQuest), not from the stage itself.
		std::vector<std::string> Lines;
		const bool bFirstClear = StageNumber > StagesCleared;
		const Campaign::FStageReward Reward = Campaign::StageReward(StageNumber);
		Add(ECurrency::Gold, Reward.Gold);
		Add(ECurrency::Bytes, Reward.Bytes);
		Add(ECurrency::AngelHearts, Reward.Hearts);
		Lines.push_back("+" + std::to_string(Reward.Gold) + " Gold");
		Lines.push_back("+" + std::to_string(Reward.Bytes) + " Bytes");
		Lines.push_back("+" + std::to_string(Reward.Hearts) + " Angel hearts");
		if (bFirstClear)
		{
			Add(ECurrency::Diamonds, Reward.Diamonds);
			Lines.push_back("+" + std::to_string(Reward.Diamonds) + " Diamonds (first clear)");
			if (bChapterFinal)
			{
				Lines.push_back("Chapter complete: claim the chapter quest in Lobby > Quests > Story.");
			}
		}
		else
		{
			Lines.push_back("Diamonds are paid on the first clear only.");
		}
		const FEquipment& Drop = AddRandomEquipment();
		char Buffer[96];
		std::snprintf(Buffer, sizeof(Buffer), "Equipment drop: %s (+%.1f%% %s)", Drop.Name.c_str(), Drop.Percent,
			Drop.Bonus == EBonus::Damage ? "damage" : "defence");
		Lines.push_back(Buffer);
		const int PlackIndex = FindOperator("Plack");
		if (TutorialStep == static_cast<int>(ETutorialStep::PlayLevel1) && StageNumber >= Config::TutorialPlayLevel)
		{
			// Owner request 2026-10-04: clearing the tutorial level unlocks Plack for free.
			const bool bNew = GrantOperator(PlackIndex);
			TutorialStep = static_cast<int>(ETutorialStep::Complete);
			Lines.push_back("TUTORIAL COMPLETE! " + std::string(bNew ? "Plack is unlocked for free." : "Plack was already yours."));
			if (AutoPlaceInTeam(PlackIndex))
			{
				Lines.push_back("Plack joined your squad.");
			}
		}
		if (bChapterFinal && StageNumber <= CampaignLevels)
		{
			// MAIN §5: Plack joins after defeating the Chapter 1 boss (only that one: the other chapter bosses give no operator). A
			// player who already has Plack (the tutorial gave him for free, or a summon did) gets the normal duplicate reward.
			const bool bNew = GrantOperator(PlackIndex);
			Lines.push_back(bNew ? "Plack joined the roster (Chapter 1 boss defeated)." : "Plack duplicate: +20 upgrade units.");
		}
		StagesCleared = std::max(StagesCleared, std::min(StageNumber, TotalCampaignLevels));
		return Lines;
	}

	// ================================================================ accounts
	void FProfile::StartNewPlayer()
	{
		ResetToNewGame(); // wallet, banners, meta, starter equipment; clears bUnlimited
		// Owner request 2026-10-06: a new account starts with 300 Diamonds and no other currency.
		for (int& Value : Currencies)
		{
			Value = 0;
		}
		Currencies[static_cast<int>(ECurrency::Diamonds)] = Config::StartingDiamonds;
		Meta.Mail.clear();   // owner request 2026-10-06: no beginning mail
		Operators.assign(Roster().size(), FOwnedOperator());
		GrantOperator(FindOperator("Luna")); // owner request 2026-10-04: a new account owns only Luna
		for (int& Slot : Team)
		{
			Slot = -1;                        // ...and an empty squad: the tutorial has the player fill it
		}
		TutorialStep = static_cast<int>(ETutorialStep::OpenOperators);
		Events::StartRookie(*this);           // classic beginner missions for the first 7 days (BIBEvents.h)
	}

	void FProfile::StartAdmin()
	{
		StartNewPlayer();
		ApplyAdminEntitlements();
	}

	void FProfile::ApplyAdminEntitlements()
	{
		// Owner/admin account: every operator, unlimited currencies and items, every level. Safe to repeat after a load.
		bUnlimited = true;
		GrantAllOperators();
		for (FOwnedOperator& Owned : Operators)
		{
			Owned.DupResource = Config::UnlimitedAmount;
		}
		TutorialStep = static_cast<int>(ETutorialStep::Complete);
		bTutorialSummonDone = true;
		if (Team[0] < 0 && Team[1] < 0 && Team[2] < 0)
		{
			Team[0] = FindOperator("Luna"); // the owner's playable trio
			Team[1] = FindOperator("Judge");
			Team[2] = FindOperator("Narrator");
		}
		StagesCleared = TotalCampaignLevels;
		for (int Story = 0; Story < SubStoryCount; ++Story)
		{
			Meta.SubStoryCleared[Story] = SubStoryLevels(Story);
		}
		int Missing[static_cast<int>(ESlot::Count)] = {};
		for (const FEquipment& Item : Inventory)
		{
			++Missing[static_cast<int>(Item.Slot)];
		}
		for (int Slot = 0; Slot < static_cast<int>(ESlot::Count); ++Slot)
		{
			while (Missing[Slot] < Config::AdminEquipmentPerSlot)
			{
				const ESlot Chosen = static_cast<ESlot>(Slot);
				const EBonus Bonus = Chosen == ESlot::Weapon ? EBonus::Damage
					: (std::uniform_int_distribution<int>(0, 1)(Random) == 0 ? EBonus::Damage : EBonus::Defence);
				const int HalfSteps = std::uniform_int_distribution<int>(
					static_cast<int>(Config::EquipMinPercent * 2.0), static_cast<int>(Config::EquipMaxPercent * 2.0))(Random);
				AddEquipment(Chosen, Bonus, HalfSteps / 2.0, SlotName(Chosen));
				++Missing[Slot];
			}
		}
	}

	void FProfile::ResetForAccount()
	{
		if (Account.bAdmin)
		{
			StartAdmin();
		}
		else if (Account.bSignedIn || Account.bGuest)
		{
			StartNewPlayer();
		}
		else
		{
			ResetToNewGame();
		}
		// Names and ids live on the account, not in the progress being reset.
		const FAccountInfo Keep = Account;
		ApplyAccount(Keep);
	}

	void FProfile::ApplyAccount(const FAccountInfo& Info)
	{
		Account = Info;
		if (!Info.Username.empty())
		{
			Meta.Name = Info.Username;
		}
		if (!Info.UserId.empty())
		{
			const int Id = std::atoi(Info.UserId.c_str());
			if (Id > 0)
			{
				Meta.Uid = Id;
			}
		}
		if (Info.bAdmin && Info.bSignedIn)
		{
			ApplyAdminEntitlements();
		}
		else
		{
			bUnlimited = false;
		}
	}

	// =============================================================== rotation
	namespace
	{
		int64_t FloorDivide(int64_t Numerator, int64_t Denominator)
		{
			int64_t Quotient = Numerator / Denominator;
			if ((Numerator % Denominator != 0) && ((Numerator < 0) != (Denominator < 0)))
			{
				--Quotient;
			}
			return Quotient;
		}

		constexpr int64_t RotationSeconds = static_cast<int64_t>(Config::RotationDays) * 86400;
	}

	int64_t FProfile::RotationIndexAt(int64_t UnixSeconds)
	{
		return FloorDivide(UnixSeconds - Config::RotationAnchorUnix, RotationSeconds);
	}

	int64_t FProfile::RotationStartUnix(int64_t Index)
	{
		return Config::RotationAnchorUnix + Index * RotationSeconds;
	}

	int FProfile::FeaturedOperatorFor(int Stars, int64_t Index)
	{
		const char* const* Pool = Stars == 6 ? Config::RotationPool6 : Config::RotationPool5;
		const int64_t Count = Stars == 6 ? static_cast<int64_t>(sizeof(Config::RotationPool6) / sizeof(Config::RotationPool6[0]))
										 : static_cast<int64_t>(sizeof(Config::RotationPool5) / sizeof(Config::RotationPool5[0]));
		const int64_t Slot = ((Index % Count) + Count) % Count;
		return FindOperator(Pool[Slot]);
	}

	void FProfile::ApplyRotation(bool bResetPityOnChange)
	{
		for (size_t Index = 0; Index < Banners.size(); ++Index)
		{
			FBanner& Banner = Banners[Index];
			if (Banner.Type != EBannerType::Featured || Banner.RotationStars == 0)
			{
				continue;
			}
			const int Featured = FeaturedOperatorFor(Banner.RotationStars, RotationIndex);
			if (Featured < 0)
			{
				continue;
			}
			if (bResetPityOnChange && Banner.FeaturedOperator != Featured && Index < Pity.size())
			{
				Pity[Index] = FPityState();
			}
			Banner.FeaturedOperator = Featured;
			Banner.Title = std::string("Featured: ") + Roster()[static_cast<size_t>(Featured)].Name + " (" + std::to_string(Banner.RotationStars) + "*)";
		}
	}

	void FProfile::SetTime(int64_t UnixSeconds)
	{
		if (UnixSeconds <= 0)
		{
			return;
		}
		NowUnix = UnixSeconds;
		const int64_t Index = RotationIndexAt(UnixSeconds);
		if (Index != RotationIndex)
		{
			RotationIndex = Index;
			ApplyRotation(true);
		}
	}

	int64_t FProfile::SecondsUntilRotation() const
	{
		if (NowUnix <= 0)
		{
			return -1;
		}
		return std::max<int64_t>(0, RotationStartUnix(RotationIndex + 1) - NowUnix);
	}

	void FProfile::GrantDemoCurrency()
	{
		// Demo-only stand-in for quests / real-world purchases (no real purchases in the demo).
		Add(ECurrency::Gold, 50000);
		Add(ECurrency::Diamonds, 10000);
		Add(ECurrency::Bytes, 50000);
		Add(ECurrency::AngelHearts, 2000);
		Add(ECurrency::RegularTicket, 20);
		Add(ECurrency::SpecialTicket, 20);
		Add(ECurrency::Mileage, 2500);
		Add(ECurrency::ArcadeToken, 10);
	}
}
