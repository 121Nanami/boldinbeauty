// Boldinbeauty demo — tester tools (see BIBTester.h).
#include "BIBTester.h"

#include <algorithm>

#include "BIBConfig.h"
#include "BIBMetaOps.h"

namespace BIB
{
	namespace Tester
	{
		namespace
		{
			constexpr int MaxStars = (MaxOperatorLevel - Config::BaseLevelCap) / Config::CapPerUpgradeStar; // 5 stars -> cap 200

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

			int ClampInt(long long Value, int Low, int High)
			{
				return static_cast<int>(std::min<long long>(std::max<long long>(Value, Low), High));
			}

			// The result of adding, removing or setting a stored amount, kept inside what the save system accepts.
			int Combine(int Current, int Mode, long long Amount)
			{
				const long long Safe = std::max<long long>(0, Amount);
				switch (Mode)
				{
				case ModeRemove: return ClampInt(static_cast<long long>(Current) - Safe, 0, MaxAmount);
				case ModeSet: return ClampInt(Safe, 0, MaxAmount);
				default: return ClampInt(static_cast<long long>(Current) + Safe, 0, MaxAmount);
				}
			}

			FActionResult UnknownOperator()
			{
				return Fail("No such operator.");
			}

			bool ValidIndex(const FProfile& Profile, int RosterIndex)
			{
				return RosterIndex >= 0 && RosterIndex < static_cast<int>(Profile.Operators.size());
			}

			// Keeps one owned operator inside the rules the save loader enforces: level inside the cap, gates inside the level.
			void Normalise(FOwnedOperator& Owned)
			{
				Owned.UpgradeStars = ClampInt(Owned.UpgradeStars, 0, MaxStars);
				const int Cap = Config::BaseLevelCap + Config::CapPerUpgradeStar * Owned.UpgradeStars;
				Owned.Level = ClampInt(Owned.Level, 1, Cap);
				Owned.HeartGatesPaid = ClampInt(Owned.HeartGatesPaid, 0, Owned.Level / Config::HeartGateEvery);
			}

			void SetWallet(FProfile& Profile, long long Gold, long long Diamonds, long long Bytes, long long Hearts, long long Regular,
				long long Special, long long Daily, long long Mileage, long long Arcade)
			{
				Profile.SetRaw(ECurrency::Gold, ClampInt(Gold, 0, MaxAmount));
				Profile.SetRaw(ECurrency::Diamonds, ClampInt(Diamonds, 0, MaxAmount));
				Profile.SetRaw(ECurrency::Bytes, ClampInt(Bytes, 0, MaxAmount));
				Profile.SetRaw(ECurrency::AngelHearts, ClampInt(Hearts, 0, MaxAmount));
				Profile.SetRaw(ECurrency::RegularTicket, ClampInt(Regular, 0, MaxAmount));
				Profile.SetRaw(ECurrency::SpecialTicket, ClampInt(Special, 0, MaxAmount));
				Profile.SetRaw(ECurrency::DailyTicket, ClampInt(Daily, 0, MaxAmount));
				Profile.SetRaw(ECurrency::Mileage, ClampInt(Mileage, 0, MaxAmount));
				Profile.SetRaw(ECurrency::ArcadeToken, ClampInt(Arcade, 0, MaxAmount));
			}

			void PlaceSquad(FProfile& Profile, const char* A, const char* B, const char* C)
			{
				const char* Names[3] = {A, B, C};
				int Slot = 0;
				for (const char* Name : Names)
				{
					const int Index = FindOperator(Name);
					if (Slot < 3 && Index >= 0 && Profile.Owns(Index))
					{
						Profile.Team[Slot++] = Index;
					}
				}
				for (; Slot < 3; ++Slot)
				{
					Profile.Team[Slot] = -1;
				}
			}

			// Gear for the presets: `Pieces` of every slot and bonus kind at one percentage and upgrade level.
			void StockGear(FProfile& Profile, int Pieces, double Percent, int Upgrade)
			{
				for (int Slot = 0; Slot < static_cast<int>(ESlot::Count); ++Slot)
				{
					const ESlot Chosen = static_cast<ESlot>(Slot);
					for (int Bonus = 0; Bonus < 2; ++Bonus)
					{
						if (Chosen == ESlot::Weapon && Bonus == 1)
						{
							continue; // weapons only give damage (MAIN §4 / the save loader)
						}
						for (int Piece = 0; Piece < Pieces; ++Piece)
						{
							FEquipment& Item = Profile.AddEquipment(Chosen, static_cast<EBonus>(Bonus), Percent, SlotName(Chosen));
							Item.UpgradeLevel = ClampInt(Upgrade, 0, Config::EquipMaxUpgrade);
						}
					}
				}
			}
		}

		bool Enabled()
		{
			return Config::TesterMode;
		}

		long long StepAmount(int StepIndex)
		{
			static const long long Steps[StepCount] = {1, 10, 100, 1000, 10000, 100000, 1000000, 100000000};
			return Steps[ClampInt(StepIndex, 0, StepCount - 1)];
		}

		const char* ModeName(int Mode)
		{
			static const char* Names[ModeCount] = {"Add", "Remove", "Set"};
			return Names[ClampInt(Mode, 0, ModeCount - 1)];
		}

		double GearPercent(int Index)
		{
			return static_cast<double>(ClampInt(Index, 0, GearPercentSteps - 1) + 1);
		}

		// ------------------------------------------------------------ currencies
		FActionResult ApplyCurrency(FProfile& Profile, ECurrency Currency, int Mode, long long Amount)
		{
			if (!Enabled())
			{
				return Fail("Tester tools are off in this build.");
			}
			if (Profile.bUnlimited)
			{
				return Fail("This account's wallet is unlimited and cannot be edited.");
			}
			const int Next = Combine(Profile.Raw(Currency), Mode, Amount);
			Profile.SetRaw(Currency, Next);
			return Ok(std::string(CurrencyName(Currency)) + " is now " + Meta::Num(Next) + ".");
		}

		const char* WalletPresetName(int Preset)
		{
			static const char* Names[WalletPresetCount] = {"Empty wallet", "Starting wallet", "Rich wallet", "Very rich wallet"};
			return Names[ClampInt(Preset, 0, WalletPresetCount - 1)];
		}

		FActionResult WalletPreset(FProfile& Profile, int Preset)
		{
			if (!Enabled())
			{
				return Fail("Tester tools are off in this build.");
			}
			if (Profile.bUnlimited)
			{
				return Fail("This account's wallet is unlimited and cannot be edited.");
			}
			switch (ClampInt(Preset, 0, WalletPresetCount - 1))
			{
			case 0: SetWallet(Profile, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
			case 1: SetWallet(Profile, 0, Config::StartingDiamonds, 0, 0, 0, 0, 0, 0, 0); break; // a new account: 300 Diamonds only (owner 2026-10-06)
			case 2: SetWallet(Profile, 5000000, 100000, 2000000, 50000, 500, 500, 1, 2500, 100); break;
			default: SetWallet(Profile, 1000000000, 1000000000, 1000000000, 1000000000, 100000, 100000, 1, 100000, 100000); break;
			}
			return Ok(std::string(WalletPresetName(Preset)) + " applied.");
		}

		// ------------------------------------------------------------- operators
		FActionResult SetOwned(FProfile& Profile, int RosterIndex, bool bOwned)
		{
			if (!ValidIndex(Profile, RosterIndex))
			{
				return UnknownOperator();
			}
			const char* Name = Roster()[static_cast<size_t>(RosterIndex)].Name;
			FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(RosterIndex)];
			if (bOwned)
			{
				if (!Owned.bOwned)
				{
					Owned = FOwnedOperator();
					Owned.bOwned = true;
				}
				return Ok(std::string(Name) + " unlocked.");
			}
			Owned = FOwnedOperator();
			for (int& Slot : Profile.Team)
			{
				if (Slot == RosterIndex)
				{
					Slot = -1;
				}
			}
			for (FEquipment& Item : Profile.Inventory)
			{
				if (Item.EquippedBy == RosterIndex)
				{
					Item.EquippedBy = -1;
				}
			}
			return Ok(std::string(Name) + " locked.");
		}

		FActionResult SetLevel(FProfile& Profile, int RosterIndex, int Level)
		{
			if (!ValidIndex(Profile, RosterIndex))
			{
				return UnknownOperator();
			}
			if (!Profile.Owns(RosterIndex))
			{
				return Fail(std::string(Roster()[static_cast<size_t>(RosterIndex)].Name) + " is locked: unlock the operator first.");
			}
			FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(RosterIndex)];
			const int Target = ClampInt(Level, 1, MaxOperatorLevel);
			const int StarsNeeded = Target > Config::BaseLevelCap
				? (Target - Config::BaseLevelCap + Config::CapPerUpgradeStar - 1) / Config::CapPerUpgradeStar : 0;
			Owned.UpgradeStars = std::max(Owned.UpgradeStars, StarsNeeded);
			Owned.Level = Target;
			Owned.HeartGatesPaid = (Target - 1) / Config::HeartGateEvery;
			Normalise(Owned);
			return Ok(std::string(Roster()[static_cast<size_t>(RosterIndex)].Name) + " is level " + std::to_string(Owned.Level)
				+ " (cap " + std::to_string(Profile.LevelCap(RosterIndex)) + ").");
		}

		FActionResult SetStars(FProfile& Profile, int RosterIndex, int Stars)
		{
			if (!ValidIndex(Profile, RosterIndex))
			{
				return UnknownOperator();
			}
			if (!Profile.Owns(RosterIndex))
			{
				return Fail(std::string(Roster()[static_cast<size_t>(RosterIndex)].Name) + " is locked: unlock the operator first.");
			}
			FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(RosterIndex)];
			Owned.UpgradeStars = ClampInt(Stars, 0, MaxStars);
			Normalise(Owned);
			return Ok(std::string(Roster()[static_cast<size_t>(RosterIndex)].Name) + " has " + std::to_string(Owned.UpgradeStars)
				+ " upgrade stars (level cap " + std::to_string(Profile.LevelCap(RosterIndex)) + ", level " + std::to_string(Owned.Level) + ").");
		}

		FActionResult SetDuplicates(FProfile& Profile, int RosterIndex, int Mode, long long Amount)
		{
			if (!ValidIndex(Profile, RosterIndex))
			{
				return UnknownOperator();
			}
			if (!Profile.Owns(RosterIndex))
			{
				return Fail(std::string(Roster()[static_cast<size_t>(RosterIndex)].Name) + " is locked: unlock the operator first.");
			}
			FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(RosterIndex)];
			Owned.DupResource = Combine(Owned.DupResource, Mode, Amount);
			return Ok("Duplicate units: " + Meta::Num(Owned.DupResource) + ".");
		}

		FActionResult SetGatePaid(FProfile& Profile, int RosterIndex, bool bPaid)
		{
			if (!ValidIndex(Profile, RosterIndex) || !Profile.Owns(RosterIndex))
			{
				return Fail("Select an unlocked operator.");
			}
			FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(RosterIndex)];
			if (Owned.Level % Config::HeartGateEvery != 0)
			{
				return Fail("There is no Angel-heart gate at level " + std::to_string(Owned.Level) + " (gates sit at 20, 40, 60...).");
			}
			const int Gate = Owned.Level / Config::HeartGateEvery;
			Owned.HeartGatesPaid = bPaid ? Gate : Gate - 1;
			return Ok(std::string("Gate at level ") + std::to_string(Owned.Level) + (bPaid ? " opened." : " closed."));
		}

		void UnlockAllOperators(FProfile& Profile)
		{
			Profile.GrantAllOperators();
		}

		void LockAllOperators(FProfile& Profile)
		{
			const int Luna = FindOperator("Luna");
			for (int Index = 0; Index < static_cast<int>(Profile.Operators.size()); ++Index)
			{
				if (Index != Luna)
				{
					SetOwned(Profile, Index, false);
				}
			}
			SetOwned(Profile, Luna, true);
		}

		void SetAllLevels(FProfile& Profile, int Level)
		{
			for (int Index = 0; Index < static_cast<int>(Profile.Operators.size()); ++Index)
			{
				if (Profile.Owns(Index))
				{
					SetLevel(Profile, Index, Level);
				}
			}
		}

		void SetAllStars(FProfile& Profile, int Stars)
		{
			for (int Index = 0; Index < static_cast<int>(Profile.Operators.size()); ++Index)
			{
				if (Profile.Owns(Index))
				{
					SetStars(Profile, Index, Stars);
				}
			}
		}

		// -------------------------------------------------------------- campaign
		void SetCampaignProgress(FProfile& Profile, int LevelsCleared)
		{
			Profile.StagesCleared = ClampInt(LevelsCleared, 0, TotalCampaignLevels);
		}

		void SetTowerBest(FProfile& Profile, int Mode, int Floor)
		{
			if (Mode >= 0 && Mode < 2)
			{
				Profile.Meta.TowerBest[Mode] = ClampInt(Floor, 0, MaxTowerFloor);
			}
		}

		void SetSubStories(FProfile& Profile, bool bAllCleared)
		{
			for (int Story = 0; Story < SubStoryCount; ++Story)
			{
				Profile.Meta.SubStoryCleared[Story] = bAllCleared ? SubStoryLevels(Story) : 0;
			}
		}

		// ------------------------------------------------------------------ gear
		FEquipment& GrantGear(FProfile& Profile, ESlot Slot, EBonus Bonus, double Percent)
		{
			return Profile.AddEquipment(Slot, Slot == ESlot::Weapon ? EBonus::Damage : Bonus, Percent, SlotName(Slot));
		}

		void SetAllGearUpgrade(FProfile& Profile, int Level)
		{
			for (FEquipment& Item : Profile.Inventory)
			{
				Item.UpgradeLevel = ClampInt(Level, 0, Config::EquipMaxUpgrade);
			}
		}

		void ClearGear(FProfile& Profile)
		{
			Profile.Inventory.clear();
		}

		int EquipBestOnTeam(FProfile& Profile)
		{
			int Equipped = 0;
			for (int Member : Profile.Team)
			{
				if (Member < 0 || !Profile.Owns(Member))
				{
					continue;
				}
				for (int Slot = 0; Slot < static_cast<int>(ESlot::Count); ++Slot)
				{
					const FEquipment* Best = nullptr;
					double BestValue = -1.0;
					for (const FEquipment& Item : Profile.Inventory)
					{
						if (static_cast<int>(Item.Slot) != Slot || (Item.EquippedBy >= 0 && Item.EquippedBy != Member))
						{
							continue;
						}
						const double Value = Item.Percent + Item.UpgradeLevel * Config::EquipUpgradePercent;
						if (Value > BestValue)
						{
							BestValue = Value;
							Best = &Item;
						}
					}
					if (Best && Best->EquippedBy != Member)
					{
						Profile.Equip(Best->Id, Member);
						++Equipped;
					}
				}
			}
			return Equipped;
		}

		// ----------------------------------------------------------- progression
		FActionResult SetCommanderXp(FProfile& Profile, int Mode, long long Amount)
		{
			Profile.Meta.CommanderXp = Combine(Profile.Meta.CommanderXp, Mode, Amount);
			return Ok("Commander level " + std::to_string(Meta::CommanderLevel(Profile)) + ", XP " + Meta::Num(Profile.Meta.CommanderXp) + ".");
		}

		void SetCommanderLevel(FProfile& Profile, int Level)
		{
			Profile.Meta.CommanderXp = (ClampInt(Level, 1, 60) - 1) * Meta::XpPerLevel();
		}

		FActionResult SetSeasonXp(FProfile& Profile, int Mode, long long Amount)
		{
			Profile.Meta.SeasonXp = Combine(Profile.Meta.SeasonXp, Mode, Amount);
			return Ok("Season tier " + std::to_string(Meta::SeasonTier(Profile)) + ", XP " + Meta::Num(Profile.Meta.SeasonXp) + ".");
		}

		void SetSeasonTier(FProfile& Profile, int Tier)
		{
			Profile.Meta.SeasonXp = ClampInt(Tier, 0, SeasonTiers) * Meta::SeasonXpPerTier();
		}

		void SetArenaPoints(FProfile& Profile, int Points)
		{
			// Arena trophies (owner request 2026-10-06: ranks Bronze 1 .. Champions, 100 trophies a rank).
			Profile.Meta.Arena.Trophies = ClampInt(Points, 0, MaxAmount);
			Profile.Meta.Arena.bChampionFloor = Profile.Meta.Arena.Trophies >= 1500;
		}

		void SetBuildings(FProfile& Profile, int Level)
		{
			const int Target = ClampInt(Level, 0, MaxBuildingLevel);
			for (FBuildingState& Building : Profile.Meta.Buildings)
			{
				Building.bBuilt = Target > 0;
				Building.Level = Target;
			}
		}

		void SetPity(FProfile& Profile, int BannerIndex, int Since5, int Since6, bool bLostFiftyFifty)
		{
			if (BannerIndex < 0 || BannerIndex >= static_cast<int>(Profile.Pity.size()))
			{
				return;
			}
			FPityState& State = Profile.Pity[static_cast<size_t>(BannerIndex)];
			State.Since5 = ClampInt(Since5, 0, Config::Pity5Rolls - 1);
			State.Since6 = ClampInt(Since6, 0, Config::Pity6Rolls - 1);
			State.bLostFiftyFifty = bLostFiftyFifty;
		}

		void SkipTutorial(FProfile& Profile)
		{
			if (!Profile.TutorialActive())
			{
				return;
			}
			const int Luna = FindOperator("Luna");
			const int Kuu = FindOperator("Kuu");
			const int Plack = FindOperator("Plack");
			if (!Profile.Owns(Luna))
			{
				Profile.GrantOperator(Luna);
			}
			if (!Profile.Owns(Kuu))
			{
				Profile.GrantOperator(Kuu);
			}
			if (!Profile.Owns(Plack))
			{
				Profile.GrantOperator(Plack);
			}
			Profile.bTutorialSummonDone = true;
			FOwnedOperator& LunaState = Profile.Operators[static_cast<size_t>(Luna)];
			LunaState.Level = std::max(LunaState.Level, 2);
			Profile.StagesCleared = std::max(Profile.StagesCleared, Config::TutorialPlayLevel);
			Profile.AutoPlaceInTeam(Luna);
			Profile.AutoPlaceInTeam(Kuu);
			Profile.AutoPlaceInTeam(Plack);
			Profile.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		}

		// --------------------------------------------------------------- presets
		const char* PresetName(int Preset)
		{
			static const char* Names[PresetCount] = {"New player", "Early game", "Middle game", "Late game", "Everything unlocked"};
			return Names[ClampInt(Preset, 0, PresetCount - 1)];
		}

		const char* PresetNote(int Preset)
		{
			static const char* Notes[PresetCount] = {
				"fresh start, tutorial on",
				"Luna, Kuu, Plack Lv 10, 3 levels cleared",
				"9 operators Lv 40, chapters 1-4 cleared, rich wallet",
				"all 20 operators Lv 100, chapters 1-10 cleared",
				"all operators at the top, all 104 levels, max gear"};
			return Notes[ClampInt(Preset, 0, PresetCount - 1)];
		}

		void ApplyPreset(FProfile& Profile, int Preset)
		{
			if (!Enabled())
			{
				return;
			}
			const int Chosen = ClampInt(Preset, 0, PresetCount - 1);
			Profile.ResetForAccount(); // the same kind of profile (admin, signed-in, guest, developer), fresh; keeps name and id
			if (Chosen == 0 || Profile.bUnlimited)
			{
				return; // the admin account keeps everything it has: a preset only restores the fresh admin profile
			}
			SkipTutorial(Profile);
			if (Chosen == 1)
			{
				LockAllOperators(Profile); // exactly Luna, Kuu and Plack, whatever kind of profile this was
				SetOwned(Profile, FindOperator("Kuu"), true);
				SetOwned(Profile, FindOperator("Plack"), true);
				Profile.bTutorialSummonDone = true;
				SetAllLevels(Profile, 10);
				SetCampaignProgress(Profile, 3);
				PlaceSquad(Profile, "Luna", "Kuu", "Plack");
				SetCommanderLevel(Profile, 3);
				EquipBestOnTeam(Profile);
				return;
			}
			if (Chosen == 2)
			{
				LockAllOperators(Profile);
				for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
				{
					if (Roster()[static_cast<size_t>(Index)].Stars <= 4 || std::string(Roster()[static_cast<size_t>(Index)].Name) == "Judge")
					{
						SetOwned(Profile, Index, true);
					}
				}
				SetAllLevels(Profile, 40);
				SetCampaignProgress(Profile, 32);
				WalletPreset(Profile, 2);
				PlaceSquad(Profile, "Luna", "Judge", "Plack");
				SetCommanderLevel(Profile, 20);
				SetSeasonTier(Profile, 10);
				StockGear(Profile, 2, 3.0, 1);
				EquipBestOnTeam(Profile);
				return;
			}
			UnlockAllOperators(Profile);
			WalletPreset(Profile, Chosen == 3 ? 2 : 3);
			PlaceSquad(Profile, "Luna", "Judge", "Narrator");
			if (Chosen == 3)
			{
				SetAllStars(Profile, 1);
				SetAllLevels(Profile, 100);
				SetCampaignProgress(Profile, 80);
				SetCommanderLevel(Profile, 50);
				SetSeasonTier(Profile, 30);
				SetBuildings(Profile, 6);
				StockGear(Profile, 2, 4.0, 3);
			}
			else
			{
				SetAllStars(Profile, MaxStars);
				SetAllLevels(Profile, MaxOperatorLevel);
				SetCampaignProgress(Profile, TotalCampaignLevels);
				SetSubStories(Profile, true);
				SetTowerBest(Profile, 0, 100);
				SetTowerBest(Profile, 1, 100);
				SetCommanderLevel(Profile, 60);
				SetSeasonTier(Profile, SeasonTiers);
				Profile.Meta.bPremiumPass = true;
				SetBuildings(Profile, MaxBuildingLevel);
				StockGear(Profile, 3, 5.0, Config::EquipMaxUpgrade);
			}
			EquipBestOnTeam(Profile);
		}
	}
}
