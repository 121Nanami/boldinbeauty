// Unit tests for the Tester panel (owner request 2026-10-05): every guest, signed-in, admin and developer profile is a tester;
// the panel's currency, operator, campaign, gear, XP, pity and preset tools keep every profile inside the rules the save
// loader enforces. Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBTester.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBTutorial.h"

#include <cstdio>
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

	enum EKind : int
	{
		KindDeveloper = 0,
		KindGuest,
		KindSignedIn,
		KindAdmin,
		KindCount
	};

	const char* KindName(int Kind)
	{
		static const char* Names[KindCount] = {"developer", "guest", "signed-in", "admin"};
		return Names[Kind];
	}

	FProfile MakeProfile(int Kind)
	{
		FProfile Profile(7u);
		FAccountInfo Info;
		switch (Kind)
		{
		case KindGuest:
			Info.bGuest = true;
			Info.Username = "Guest";
			Profile.Account = Info;
			Profile.ResetForAccount();
			break;
		case KindSignedIn:
			Info.bSignedIn = true;
			Info.UserId = "10000042";
			Info.Username = "Tester";
			Info.Email = "tester@example.com";
			Profile.Account = Info;
			Profile.ResetForAccount();
			break;
		case KindAdmin:
			Info.bSignedIn = true;
			Info.bAdmin = true;
			Info.UserId = "10000001";
			Info.Username = "Owner";
			Info.Email = Config::AdminEmail;
			Profile.Account = Info;
			Profile.ResetForAccount();
			break;
		default:
			Profile.ResetToNewGame();
			break;
		}
		return Profile;
	}

	int Op(const char* Name)
	{
		return FindOperator(Name);
	}

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	// A save written and read back must be identical: the tester tools may never leave a state the loader would change.
	void ExpectSaveStable(const FProfile& Profile, const std::string& What)
	{
		const std::string Text = Save::Serialize(Profile, 1800000000);
		FProfile Loaded(9u);
		Loaded.Account = Profile.Account;
		std::string Error;
		const bool bLoaded = Save::Deserialize(Text, Loaded, Error);
		Expect(bLoaded, What + ": the save loads (" + Error + ")");
		if (bLoaded)
		{
			Expect(Save::Serialize(Loaded, 1800000000) == Text, What + ": saving what was loaded gives identical text");
		}
	}

	// -------------------------------------------------------------------- everyone is a tester
	void TestEveryoneGetsThePanel()
	{
		Expect(Tester::Enabled(), "tester mode is on in this build");
		for (int Kind = 0; Kind < KindCount; ++Kind)
		{
			FProfile Profile = MakeProfile(Kind);
			P::FUiState State;
			State.Tab = P::TabLobby;
			State.Page = "";
			if (Profile.TutorialActive())
			{
				Profile.TutorialStep = static_cast<int>(ETutorialStep::Complete);
			}
			const P::FPage Page = P::Build(Profile, State);
			bool bTile = false;
			for (const P::FPageAction& Action : Page.Grid)
			{
				bTile = bTile || Action.Id == "go:tester";
			}
			Expect(bTile, std::string("the Lobby has a Tester panel tile for the ") + KindName(Kind) + " profile");
			State.Page = "tester";
			const P::FPage Root = P::Build(Profile, State);
			Expect(Root.Title == "Tester panel" && Root.bCanBack && Root.Grid.size() >= 7, std::string("tester root page for the ") + KindName(Kind) + " profile");
			Expect(Contains(Root.Lines.empty() ? std::string() : Root.Lines[0], Profile.Meta.Name), "the root page names the player");
		}
	}

	// ------------------------------------------------------------------------- currencies
	void TestCurrencies()
	{
		for (int Kind = 0; Kind < KindAdmin; ++Kind)
		{
			FProfile Profile = MakeProfile(Kind);
			Profile.SetRaw(ECurrency::Gold, 100);
			Expect(Tester::ApplyCurrency(Profile, ECurrency::Gold, Tester::ModeAdd, 50).bOk && Profile.Get(ECurrency::Gold) == 150, "add");
			Expect(Tester::ApplyCurrency(Profile, ECurrency::Gold, Tester::ModeRemove, 1000).bOk && Profile.Get(ECurrency::Gold) == 0, "remove stops at zero");
			Expect(Tester::ApplyCurrency(Profile, ECurrency::Gold, Tester::ModeSet, 777).bOk && Profile.Get(ECurrency::Gold) == 777, "set");
			Tester::ApplyCurrency(Profile, ECurrency::Gold, Tester::ModeAdd, 100000000000LL);
			Expect(Profile.Get(ECurrency::Gold) == Tester::MaxAmount, "an amount never passes the save limit");
			Tester::ApplyCurrency(Profile, ECurrency::Gold, Tester::ModeSet, -5);
			Expect(Profile.Get(ECurrency::Gold) == 0, "a negative amount is treated as zero");
			for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
			{
				const ECurrency Currency = static_cast<ECurrency>(Index);
				Expect(Tester::ApplyCurrency(Profile, Currency, Tester::ModeSet, 4242).bOk && Profile.Get(Currency) == 4242,
					std::string("every currency can be set: ") + CurrencyName(Currency));
			}
			for (int Preset = 0; Preset < Tester::WalletPresetCount; ++Preset)
			{
				Expect(Tester::WalletPreset(Profile, Preset).bOk, std::string("wallet preset ") + Tester::WalletPresetName(Preset));
				ExpectSaveStable(Profile, std::string("wallet preset ") + std::to_string(Preset));
			}
			Tester::WalletPreset(Profile, 1);
			Expect(Profile.Get(ECurrency::Gold) == 0 && Profile.Get(ECurrency::Diamonds) == Config::StartingDiamonds && Profile.Get(ECurrency::Bytes) == 0, "the starting wallet matches a new account (300 Diamonds only)");
			Tester::WalletPreset(Profile, 3);
			Profile.Add(ECurrency::Gold, 1000000);
			Expect(Profile.Get(ECurrency::Gold) > 0, "a very rich wallet still accepts rewards without overflowing");
		}
		FProfile Admin = MakeProfile(KindAdmin);
		Expect(!Tester::ApplyCurrency(Admin, ECurrency::Gold, Tester::ModeSet, 5).bOk, "the admin wallet is refused");
		Expect(!Tester::WalletPreset(Admin, 0).bOk, "no wallet preset on the admin wallet");
		Expect(Admin.bUnlimited && Admin.Get(ECurrency::Gold) == Config::UnlimitedAmount, "the admin wallet stays unlimited");
	}

	// ------------------------------------------------------------------------- operators
	void TestOperators()
	{
		for (int Kind = 0; Kind < KindAdmin; ++Kind)
		{
			FProfile Profile = MakeProfile(Kind);
			const int Judge = Op("Judge");
			const int Death = Op("Death");
			Tester::SetOwned(Profile, Death, true);
			Expect(Profile.Owns(Death) && Profile.Operators[static_cast<size_t>(Death)].Level == 1, "unlock gives a level 1 operator");
			Expect(Tester::SetLevel(Profile, Death, 50).bOk && Profile.Operators[static_cast<size_t>(Death)].Level == 50, "set level");
			Expect(Profile.Operators[static_cast<size_t>(Death)].HeartGatesPaid == 2, "gates below the level count as paid");
			Expect(!Profile.NeedsHeartGate(Death), "level 50 is not on a gate");
			Tester::SetLevel(Profile, Death, 40);
			Expect(Profile.NeedsHeartGate(Death), "level 40 leaves its gate due, as in normal play");
			Expect(Tester::SetGatePaid(Profile, Death, true).bOk && !Profile.NeedsHeartGate(Death), "open the gate");
			Expect(Tester::SetGatePaid(Profile, Death, false).bOk && Profile.NeedsHeartGate(Death), "close the gate");
			Tester::SetLevel(Profile, Death, 41);
			Expect(!Tester::SetGatePaid(Profile, Death, true).bOk, "no gate at level 41");
			Tester::SetLevel(Profile, Death, 160);
			Expect(Profile.Operators[static_cast<size_t>(Death)].UpgradeStars == 3 && Profile.LevelCap(Death) == 160, "a level above the cap raises the stars");
			Tester::SetLevel(Profile, Death, 100000);
			Expect(Profile.Operators[static_cast<size_t>(Death)].Level == Tester::MaxOperatorLevel, "level is clamped");
			Tester::SetLevel(Profile, Death, 0);
			Expect(Profile.Operators[static_cast<size_t>(Death)].Level == 1, "level never drops below 1");
			Tester::SetLevel(Profile, Death, 150);
			Tester::SetStars(Profile, Death, 0);
			Expect(Profile.Operators[static_cast<size_t>(Death)].Level == 100, "fewer stars pull the level back under the new cap");
			Expect(Profile.Operators[static_cast<size_t>(Death)].HeartGatesPaid <= 5, "and the gates with it");
			Tester::SetStars(Profile, Death, 99);
			Expect(Profile.Operators[static_cast<size_t>(Death)].UpgradeStars == 5, "stars are clamped");
			Tester::SetDuplicates(Profile, Death, Tester::ModeSet, 90);
			Expect(Profile.Operators[static_cast<size_t>(Death)].DupResource == 90, "duplicates set");
			Tester::SetDuplicates(Profile, Death, Tester::ModeRemove, 100);
			Expect(Profile.Operators[static_cast<size_t>(Death)].DupResource == 0, "duplicates stop at zero");
			Expect(Profile.TryUpgradeStar(Death).bOk == false, "no duplicates, no star");
			Tester::SetDuplicates(Profile, Death, Tester::ModeAdd, 1000);
			Expect(Profile.TryUpgradeStar(Death).bOk, "duplicates from the panel pay for a normal upgrade star");

			// locking removes the operator from the squad and from gear
			Profile.Team[1] = Judge;
			Tester::SetOwned(Profile, Judge, true);
			Profile.Team[1] = Judge;
			FEquipment& Item = Profile.AddEquipment(ESlot::Helmet, EBonus::Defence, 3.0, "Helmet");
			Profile.Equip(Item.Id, Judge);
			Tester::SetOwned(Profile, Judge, false);
			Expect(!Profile.Owns(Judge) && Profile.Team[0] != Judge && Profile.Team[1] != Judge && Profile.Team[2] != Judge, "a locked operator leaves the squad");
			Expect(Profile.EquippedIn(Judge, ESlot::Helmet) == nullptr, "and drops its gear");
			Expect(!Tester::SetLevel(Profile, Judge, 5).bOk, "a locked operator cannot be levelled");
			Expect(!Tester::SetOwned(Profile, 999, true).bOk && !Tester::SetLevel(Profile, -1, 5).bOk, "a bad index is refused");

			Tester::UnlockAllOperators(Profile);
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				Expect(Profile.Owns(Index), "unlock all: " + std::string(Roster()[static_cast<size_t>(Index)].Name));
			}
			Tester::SetAllLevels(Profile, 200);
			for (const FOwnedOperator& Owned : Profile.Operators)
			{
				Expect(Owned.Level == 200 && Owned.UpgradeStars == 5, "all to level 200");
			}
			ExpectSaveStable(Profile, "all operators at the top");
			Tester::LockAllOperators(Profile);
			int Owned = 0;
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				Owned += Profile.Owns(Index) ? 1 : 0;
			}
			Expect(Owned == 1 && Profile.Owns(Op("Luna")), "lock all keeps only Luna");
			ExpectSaveStable(Profile, "only Luna");
		}
	}

	// ------------------------------------------------------------------------- campaign
	void TestCampaign()
	{
		FProfile Profile = MakeProfile(KindGuest);
		Tester::SkipTutorial(Profile);
		Tester::SetCampaignProgress(Profile, 16);
		Expect(Profile.StagesCleared == 16, "chapters 1-2 cleared");
		Expect(Meta::CanStartStage(Profile, 17).bOk, "the next level is open");
		Expect(!Meta::CanStartStage(Profile, 18).bOk, "levels still unlock in order");
		Tester::SetCampaignProgress(Profile, 5000);
		Expect(Profile.StagesCleared == TotalCampaignLevels, "progress is clamped to the campaign length");
		Tester::SetCampaignProgress(Profile, -4);
		Expect(Profile.StagesCleared == 0, "progress never drops below zero");
		Tester::SetTowerBest(Profile, 0, 50);
		Tester::SetTowerBest(Profile, 1, 99999999);
		Expect(Profile.Meta.TowerBest[0] == 50 && Profile.Meta.TowerBest[1] == MaxTowerFloor, "tower floors are clamped (the tower is endless up to 999,999)");
		Tester::SetSubStories(Profile, true);
		for (int Story = 0; Story < SubStoryCount; ++Story)
		{
			Expect(Profile.Meta.SubStoryCleared[Story] == SubStoryLevels(Story), "sub-story cleared");
		}
		Tester::SetSubStories(Profile, false);
		Expect(Profile.Meta.SubStoryCleared[0] == 0, "sub-stories cleared again");
		Tester::SetCampaignProgress(Profile, 8);
		ExpectSaveStable(Profile, "campaign progress");
	}

	// ----------------------------------------------------------------------------- gear
	void TestGear()
	{
		FProfile Profile = MakeProfile(KindGuest);
		Tester::SkipTutorial(Profile);
		const size_t Before = Profile.Inventory.size();
		const FEquipment& Weapon = Tester::GrantGear(Profile, ESlot::Weapon, EBonus::Defence, 4.0);
		Expect(Weapon.Bonus == EBonus::Damage && Profile.Inventory.size() == Before + 1, "a weapon only gives damage");
		Tester::GrantGear(Profile, ESlot::Helmet, EBonus::Defence, 5.0);
		Tester::SetAllGearUpgrade(Profile, 99);
		for (const FEquipment& Item : Profile.Inventory)
		{
			Expect(Item.UpgradeLevel == Config::EquipMaxUpgrade, "gear upgrade is clamped");
		}
		const int Equipped = Tester::EquipBestOnTeam(Profile);
		Expect(Equipped >= 1, "equip best puts gear on the squad");
		Expect(Tester::EquipBestOnTeam(Profile) == 0, "and does nothing the second time");
		int Wearers = 0;
		for (const FEquipment& Item : Profile.Inventory)
		{
			Wearers += Item.EquippedBy >= 0 ? 1 : 0;
		}
		Expect(Wearers == Equipped, "each piece is worn by one operator");
		ExpectSaveStable(Profile, "gear");
		Tester::ClearGear(Profile);
		Expect(Profile.Inventory.empty(), "clear gear");
		ExpectSaveStable(Profile, "no gear");
	}

	// ------------------------------------------------------------------------ progression
	void TestProgression()
	{
		FProfile Profile = MakeProfile(KindSignedIn);
		Tester::SkipTutorial(Profile);
		Tester::SetCommanderLevel(Profile, 40);
		Expect(Meta::CommanderLevel(Profile) == 40, "commander level 40");
		Tester::SetCommanderLevel(Profile, 500);
		Expect(Meta::CommanderLevel(Profile) == 60, "commander level 60 is the top");
		Tester::SetCommanderXp(Profile, Tester::ModeSet, 0);
		Expect(Meta::CommanderLevel(Profile) == 1, "XP set to zero");
		Tester::SetSeasonTier(Profile, 25);
		Expect(Meta::SeasonTier(Profile) == 25, "season tier 25");
		Tester::SetSeasonTier(Profile, 90);
		Expect(Meta::SeasonTier(Profile) == SeasonTiers, "season tier is clamped");
		Tester::SetBuildings(Profile, 99);
		for (const FBuildingState& Building : Profile.Meta.Buildings)
		{
			Expect(Building.bBuilt && Building.Level == MaxBuildingLevel, "buildings at the top level");
		}
		Tester::SetBuildings(Profile, 0);
		Expect(!Profile.Meta.Buildings[0].bBuilt && Profile.Meta.Buildings[0].Level == 0, "no buildings");
		Tester::SetArenaPoints(Profile, -1);
		Expect(Profile.Meta.Arena.Trophies == 0, "arena trophies never drop below zero");
		ExpectSaveStable(Profile, "progression");
	}

	// ------------------------------------------------------------------------------ pity
	void TestPity()
	{
		FProfile Profile = MakeProfile(KindGuest);
		Tester::SkipTutorial(Profile);
		Tester::SetPity(Profile, 0, 99999, 99999, true);
		Expect(Profile.Pity[0].Since5 == Config::Pity5Rolls - 1 && Profile.Pity[0].Since6 == Config::Pity6Rolls - 1 && Profile.Pity[0].bLostFiftyFifty, "pity clamped to one short");
		Tester::SetPity(Profile, 0, Config::Pity5Rolls - 1, 0, false);
		Profile.SetRaw(ECurrency::RegularTicket, 5);
		const FSummonOutcome Outcome = Profile.Summon(0, 1, EPay::Ticket);
		Expect(Outcome.bOk && Outcome.Results.size() == 1 && Outcome.Results[0].Stars >= 5 && Outcome.Results[0].bPity, "one short of the 5-star pity: the next roll is a 5-star");
		Tester::SetPity(Profile, 99, 1, 1, false); // a bad banner index changes nothing and does not crash
		ExpectSaveStable(Profile, "pity");
	}

	// -------------------------------------------------------------------------- tutorial
	void TestTutorialSkip()
	{
		for (int Kind = KindGuest; Kind <= KindSignedIn; ++Kind)
		{
			FProfile Profile = MakeProfile(Kind);
			P::FUiState State;
			State.Tab = P::TabOperator;
			Expect(Profile.TutorialActive(), std::string("a new ") + KindName(Kind) + " profile starts the tutorial");
			const P::FPage Page = P::Build(Profile, State);
			bool bSkip = false;
			for (const P::FPageAction& Action : Page.Bar)
			{
				bSkip = bSkip || (Action.Id == "tst:tut:skip" && Action.bEnabled);
			}
			Expect(bSkip, "the tutorial page offers a Skip tutorial button");
			const P::FPerform Result = P::Perform(Profile, State, "tst:tut:skip");
			Expect(Result.bOk && !Profile.TutorialActive(), "skipping ends the tutorial");
			Expect(Profile.Owns(Op("Luna")) && Profile.Owns(Op("Kuu")) && Profile.Owns(Op("Plack")), "Luna, Kuu and Plack are unlocked");
			Expect(Profile.bTutorialSummonDone && Profile.StagesCleared >= 1, "the forced summon and level 1 count as done");
			Expect(Profile.TeamIsPlayable() && Profile.Team[0] == Op("Luna") && Profile.Team[1] == Op("Kuu") && Profile.Team[2] == Op("Plack"), "squad Luna / Kuu / Plack");
			ExpectSaveStable(Profile, "after skipping the tutorial");
			Expect(P::Perform(Profile, State, "tst:tut:skip").bOk, "skipping twice is harmless");
		}
		// while the tutorial runs, only the skip button of the tester tools works
		FProfile Profile = MakeProfile(KindGuest);
		P::FUiState State;
		Expect(!P::Perform(Profile, State, "tst:cur:0").bOk && Profile.Get(ECurrency::Gold) == 0, "other tester actions are held back during the tutorial");
	}

	// ----------------------------------------------------------------------------- presets
	void TestPresets()
	{
		for (int Kind = 0; Kind < KindCount; ++Kind)
		{
			for (int Preset = 0; Preset < Tester::PresetCount; ++Preset)
			{
				FProfile Profile = MakeProfile(Kind);
				const std::string Name = std::string(KindName(Kind)) + " / " + Tester::PresetName(Preset);
				const std::string Username = Profile.Meta.Name;
				const int Uid = Profile.Meta.Uid;
				Tester::ApplyPreset(Profile, Preset);
				Expect(Profile.Meta.Name == Username && Profile.Meta.Uid == Uid, Name + ": name and id are kept");
				Expect(Profile.bUnlimited == (Kind == KindAdmin), Name + ": only the admin wallet is unlimited");
				if (Preset == 0 && Kind != KindDeveloper && Kind != KindAdmin)
				{
					Expect(Profile.TutorialActive() && Profile.StagesCleared == 0, Name + ": a fresh player has the tutorial");
				}
				else
				{
					Expect(!Profile.TutorialActive(), Name + ": no tutorial");
				}
				if (Kind == KindAdmin)
				{
					bool bEveryone = true;
					for (const FOwnedOperator& Owned : Profile.Operators)
					{
						bEveryone = bEveryone && Owned.bOwned && Owned.Level == 1;
					}
					Expect(bEveryone, Name + ": the admin profile is simply restored (every operator), never reduced");
					Expect(Profile.StagesCleared == TotalCampaignLevels && Profile.Get(ECurrency::Gold) == Config::UnlimitedAmount, Name + ": every level and an unlimited wallet");
					continue;
				}
				if (Preset >= 1)
				{
					Expect(Profile.TeamIsPlayable(), Name + ": the squad is playable");
				}
				if (Preset == 1)
				{
					int Owned = 0;
					for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
					{
						Owned += Profile.Owns(Index) ? 1 : 0;
					}
					Expect(Owned == 3 && Profile.Owns(Op("Luna")) && Profile.Owns(Op("Kuu")) && Profile.Owns(Op("Plack")), Name + ": early game owns exactly Luna, Kuu and Plack");
					Expect(Profile.Operators[static_cast<size_t>(Op("Luna"))].Level == 10 && Profile.StagesCleared == 3, Name + ": early game levels");
				}
				if (Preset == 2)
				{
					Expect(Profile.StagesCleared == 32 && Profile.Owns(Op("Judge")), Name + ": middle game");
					Expect(!Profile.Owns(Op("Narrator")) && !Profile.Owns(Op("Death")), Name + ": the middle game owns only the 3-4 star operators and Judge");
				}
				if (Preset == 3)
				{
					int Owned = 0;
					for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
					{
						Owned += Profile.Owns(Index) ? 1 : 0;
					}
					Expect(Owned == static_cast<int>(Roster().size()) && Profile.StagesCleared == 80, Name + ": late game");
					Expect(Profile.Operators[static_cast<size_t>(Op("Luna"))].Level == 100, Name + ": level 100");
				}
				if (Preset == 4)
				{
					Expect(Profile.StagesCleared == TotalCampaignLevels && Profile.Meta.bPremiumPass, Name + ": everything cleared");
					Expect(Profile.Operators[static_cast<size_t>(Op("Narrator"))].Level == Tester::MaxOperatorLevel, Name + ": operators at the top");
					Expect(Meta::CommanderLevel(Profile) == 60 && Meta::SeasonTier(Profile) == SeasonTiers, Name + ": top commander level and season tier");
				}
				ExpectSaveStable(Profile, Name);
				// the next level in the campaign can be started by a preset that cleared something
				if (Preset >= 1 && Profile.StagesCleared < TotalCampaignLevels)
				{
					Expect(Meta::CanStartStage(Profile, Profile.StagesCleared + 1).bOk, Name + ": the next campaign level is playable");
				}
			}
		}
	}

	// ---------------------------------------------------- every action on every Tester page works
	std::string IconDirectory()
	{
		std::string Path = __FILE__;
		const size_t Cut = Path.find_last_of("/\\");
		return (Cut == std::string::npos ? std::string(".") : Path.substr(0, Cut)) + "/../Art/Icons/";
	}

	bool IconExists(const std::string& Icon)
	{
		if (Icon.empty() || Icon.rfind("portrait:", 0) == 0)
		{
			return true;
		}
		std::FILE* File = std::fopen((IconDirectory() + Icon + ".png").c_str(), "rb");
		if (File)
		{
			std::fclose(File);
		}
		return File != nullptr;
	}

	void TestPagesAndActions()
	{
		const char* Pages[] = {"tester", "tester-cur", "tester-ops", "tester-op", "tester-camp", "tester-gear", "tester-prog", "tester-pity", "tester-preset"};
		std::set<std::string> Reached;
		for (int Kind = 0; Kind < KindCount; ++Kind)
		{
			for (const char* PageName : Pages)
			{
				FProfile Base = MakeProfile(Kind);
				if (Base.TutorialActive())
				{
					Tester::SkipTutorial(Base);
				}
				P::FUiState StateBase;
				StateBase.Tab = P::TabLobby;
				StateBase.Page = PageName;
				StateBase.SelOperator = Op("Judge");
				const P::FPage Page = P::Build(Base, StateBase);
				const std::string Where = std::string(KindName(Kind)) + " / " + PageName;
				Expect(Page.bCanBack && !Page.Title.empty(), Where + ": titled, with a back button");
				Expect(static_cast<int>(Page.Grid.size()) <= P::MaxGrid && static_cast<int>(Page.Bar.size()) <= P::MaxBar, Where + ": fits the widget");
				std::vector<P::FPageAction> All = Page.Grid;
				All.insert(All.end(), Page.Bar.begin(), Page.Bar.end());
				std::set<std::string> Ids;
				for (const P::FPageAction& Action : All)
				{
					Expect(Ids.insert(Action.Id).second, Where + ": unique action id " + Action.Id);
					Expect(!Action.Label.empty(), Where + ": every action has a label (" + Action.Id + ")");
					Expect(IconExists(Action.Icon), Where + ": icon exists: " + Action.Icon);
					if (!Action.bEnabled)
					{
						continue;
					}
					Reached.insert(Action.Id);
					FProfile Profile = Base;
					P::FUiState State = StateBase;
					const P::FPerform Result = P::Perform(Profile, State, Action.Id);
					Expect(!Contains(Result.Message, "Unknown tester action") && !Contains(Result.Message, "No such"), Where + ": " + Action.Id + " is handled (" + Result.Message + ")");
					if (Action.Id.rfind("go:", 0) != 0 && Action.Id.rfind("tst:op:sel", 0) != 0)
					{
						// a tester tool must leave a profile the save loader accepts unchanged
						ExpectSaveStable(Profile, Where + " after " + Action.Id);
					}
					// the page still builds after the action
					const P::FPage After = P::Build(Profile, State);
					Expect(static_cast<int>(After.Grid.size()) <= P::MaxGrid, Where + ": still builds after " + Action.Id);
				}
			}
		}
		Expect(Reached.size() > 100, "the Tester panel has more than a hundred distinct working actions (" + std::to_string(Reached.size()) + ")");
	}

	// ------------------------------------------------------------ walking the panel as a player
	void TestPanelFlow()
	{
		FProfile Profile = MakeProfile(KindGuest);
		P::FUiState State;
		Expect(P::Perform(Profile, State, "tst:tut:skip").bOk, "skip the tutorial");
		P::Perform(Profile, State, "tab:" + std::to_string(P::TabLobby));
		P::Perform(Profile, State, "go:tester");
		Expect(State.Page == "tester", "Lobby > Tester panel");
		P::Perform(Profile, State, "go:tester-cur");
		P::Perform(Profile, State, "tst:mode");  // remove
		P::Perform(Profile, State, "tst:mode");  // set
		Expect(State.TesterMode == Tester::ModeSet, "mode cycles add, remove, set");
		P::Perform(Profile, State, "tst:step");  // 1,000 -> 10,000
		Expect(Tester::StepAmount(State.TesterStep) == 10000, "amount cycles through the presets");
		P::Perform(Profile, State, "tst:cur:" + std::to_string(static_cast<int>(ECurrency::Diamonds)));
		Expect(Profile.Get(ECurrency::Diamonds) == 10000, "set Diamonds to 10,000 from the panel");
		P::Perform(Profile, State, "back");
		Expect(State.Page == "tester", "back from a tester page goes to the tester root");
		P::Perform(Profile, State, "back");
		Expect(State.Page.empty(), "back from the tester root goes to the Lobby");
		// summon with the panel's Diamonds, level an operator with the panel's Gold, then save
		P::Perform(Profile, State, "go:tester");
		P::Perform(Profile, State, "go:tester-ops");
		P::Perform(Profile, State, "tst:op:sel:" + std::to_string(Op("Alchemist")));
		Expect(State.Page == "tester-op" && State.SelOperator == Op("Alchemist"), "pick an operator");
		P::Perform(Profile, State, "tst:op:own");
		Expect(Profile.Owns(Op("Alchemist")), "unlock it");
		P::Perform(Profile, State, "tst:op:lvset:50");
		Expect(Profile.Operators[static_cast<size_t>(Op("Alchemist"))].Level == 50, "level 50");
		P::Perform(Profile, State, "tst:op:next");
		Expect(State.SelOperator == (Op("Alchemist") + 1) % static_cast<int>(Roster().size()), "next operator");
		ExpectSaveStable(Profile, "after the panel walk");
	}
}

int main()
{
	TestEveryoneGetsThePanel();
	TestCurrencies();
	TestOperators();
	TestCampaign();
	TestGear();
	TestProgression();
	TestPity();
	TestTutorialSkip();
	TestPresets();
	TestPagesAndActions();
	TestPanelFlow();
	std::printf("%d tester checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
