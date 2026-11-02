// Unit tests for the account-related rules (owner request 2026-10-04): the JSON helper, the save system, the 3-day
// featured banner rotation, the new-account start state, the admin account and the guided tutorial.
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBJson.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArcade.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBTutorial.h"

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

	using namespace BIB;
	using namespace BIB::Meta;
	namespace P = BIB::Pages;
	using Json::FValue;

	bool Contains(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	bool PageHas(const P::FPage& Page, const std::string& Part)
	{
		for (const std::string& Line : Page.Lines)
		{
			if (Contains(Line, Part))
			{
				return true;
			}
		}
		return false;
	}

	const P::FPageAction* FindAction(const P::FPage& Page, const std::string& Id)
	{
		for (const P::FPageAction& Action : Page.Grid)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		for (const P::FPageAction& Action : Page.Bar)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		return nullptr;
	}

	constexpr int64_t Anchor = Config::RotationAnchorUnix;
	constexpr int64_t Period = static_cast<int64_t>(Config::RotationDays) * 86400;

	// =================================================================== JSON
	void TestJson()
	{
		FValue Doc = FValue::MakeObject();
		Doc.Set("text", FValue::MakeString("quote \" backslash \\ newline \n tab \t unicode \xC3\xA9 \xF0\x9F\x98\x80"));
		Doc.Set("int", FValue::MakeNumber(-42));
		Doc.Set("big", FValue::MakeNumber(1791072000.0));
		Doc.Set("pi", FValue::MakeNumber(3.141592653589793));
		Doc.Set("yes", FValue::MakeBool(true));
		Doc.Set("none", FValue());
		FValue List = FValue::MakeArray();
		List.Push(FValue::MakeNumber(1));
		List.Push(FValue::MakeString("two"));
		List.Push(FValue::MakeArray());
		Doc.Set("list", List);
		const std::string Text = Json::Write(Doc);
		FValue Back;
		std::string Error;
		Expect(Json::Parse(Text, Back, Error), "JSON round trip parses: " + Error);
		Expect(Json::Write(Back) == Text, "JSON write -> parse -> write is stable");
		Expect(Back.GetString("text") == Doc.GetString("text"), "JSON keeps quotes, escapes and UTF-8");
		Expect(Back.GetInt("int") == -42 && Back.GetInt64("big") == 1791072000LL, "JSON keeps integers");
		Expect(Back.GetNumber("pi") == 3.141592653589793, "JSON keeps full double precision");
		Expect(Back.GetBool("yes") && Back.Find("none") && Back.Find("none")->IsNull(), "JSON keeps booleans and null");
		Expect(Back.Find("list") && Back.Find("list")->Size() == 3 && Back.Find("list")->At(1).AsString() == "two", "JSON keeps arrays");
		Expect(Back.GetInt("missing", 7) == 7 && Back.GetString("int", "fallback") == "fallback", "JSON typed reads use the fallback");
		Expect(Text.find("\n") == std::string::npos, "JSON output is a single line");

		FValue Parsed;
		Expect(Json::Parse(" { \"a\" : [ 1 , 2.5e2 , -0.5 ] , \"s\" : \"\\u00e9\\ud83d\\ude00\\/\" } ", Parsed, Error), "JSON parses whitespace, exponents and \\u escapes");
		Expect(Parsed.Find("a") && Parsed.Find("a")->At(1).AsNumber() == 250.0, "JSON exponent value");
		Expect(Parsed.GetString("s") == "\xC3\xA9\xF0\x9F\x98\x80/", "JSON decodes a surrogate pair to UTF-8");
		Expect(Json::Parse("{\"k\":1,\"k\":2}", Parsed, Error) && Parsed.GetInt("k") == 1 + 1, "JSON: a repeated key keeps the last value");

		const char* Bad[] = {"", "{", "[1,2", "{\"a\":}", "{\"a\" 1}", "[1,]", "{\"a\":1,}", "tru", "nul", "01", "1.", "-", "\"abc", "\"\\x\"",
			"\"\\ud83d\"", "\"\\ude00\"", "{\"a\":1} extra", "[1] [2]", "'single'", "{a:1}", "\"tab\there\"", "1e", "+1"};
		for (const char* Source : Bad)
		{
			FValue Unused;
			Expect(!Json::Parse(Source, Unused, Error) && !Error.empty(), std::string("JSON rejects: ") + Source);
		}
		std::string Deep(200, '[');
		FValue Unused;
		Expect(!Json::Parse(Deep, Unused, Error), "JSON rejects absurd nesting without crashing");
		std::string Shallow(60, '[');
		Shallow += std::string(60, ']');
		Expect(Json::Parse(Shallow, Unused, Error), "JSON accepts reasonable nesting");
	}

	// ============================================================ new player / admin
	void TestNewPlayer()
	{
		FProfile Profile(5);
		Profile.StartNewPlayer();
		Expect(Profile.Owns(FindOperator("Luna")), "new account owns Luna");
		int Owned = 0;
		for (const FOwnedOperator& Entry : Profile.Operators)
		{
			Owned += Entry.bOwned ? 1 : 0;
		}
		Expect(Owned == 1, "new account owns ONLY Luna");
		Expect(Profile.Team[0] < 0 && Profile.Team[1] < 0 && Profile.Team[2] < 0, "new account's squad is empty");
		Expect(!Profile.TeamIsPlayable(), "an empty squad is not playable");
		Expect(Profile.TutorialActive() && Profile.TutorialStep == static_cast<int>(ETutorialStep::OpenOperators), "tutorial starts at step 1");
		Expect(!Profile.bUnlimited && Profile.Get(ECurrency::Diamonds) == Config::StartingDiamonds, "new account starts with 300 Diamonds");
		bool bOnlyDiamonds = true;
		for (int Currency = 0; Currency < static_cast<int>(ECurrency::Count); ++Currency)
		{
			bOnlyDiamonds = bOnlyDiamonds && (static_cast<ECurrency>(Currency) == ECurrency::Diamonds || Profile.Get(static_cast<ECurrency>(Currency)) == 0);
		}
		Expect(bOnlyDiamonds, "...and no other currency (owner request 2026-10-06)");
		Expect(Meta::UnclaimedMail(Profile) == 0, "no currency hidden in the welcome mail either");
		Expect(!CanStartStage(Profile, CampaignStageId(1)).bOk, "an empty squad cannot start a level");

		// The squad rule: as full as the collection allows, up to three.
		Profile.Team[0] = FindOperator("Luna");
		Expect(Profile.RequiredTeamSize() == 1 && Profile.TeamIsPlayable(), "owning only Luna, a squad of Luna is playable");
		Profile.TutorialSummon();
		Expect(Profile.RequiredTeamSize() == 2 && !Profile.TeamIsPlayable(), "owning Luna and Kuu, Luna alone is not enough");
		Expect(Profile.AutoPlaceInTeam(FindOperator("Kuu")) && Profile.Team[1] == FindOperator("Kuu"), "AutoPlaceInTeam uses the first empty slot");
		Expect(Profile.TeamIsPlayable(), "Luna and Kuu is playable");
		Expect(!Profile.AutoPlaceInTeam(FindOperator("Kuu")), "AutoPlaceInTeam refuses a member already in the squad");
		Expect(!Profile.AutoPlaceInTeam(FindOperator("Judge")), "AutoPlaceInTeam refuses an operator the player does not own");
		Profile.GrantOperator(FindOperator("Plack"));
		Expect(Profile.RequiredTeamSize() == 3 && !Profile.TeamIsPlayable(), "owning three, a squad of two is not enough");
		Profile.AutoPlaceInTeam(FindOperator("Plack"));
		Expect(Profile.TeamIsPlayable(), "a full squad is playable");
		Profile.Team[2] = Profile.Team[0];
		Expect(!Profile.TeamIsPlayable(), "the same operator twice is not playable");
		Profile.Team[2] = FindOperator("Judge");
		Expect(!Profile.TeamIsPlayable(), "an operator that is not owned is not playable");

		// The developer profile keeps its old shape (Luna, Judge and Narrator, no tutorial).
		FProfile Dev(5);
		Expect(!Dev.TutorialActive() && Dev.Owns(FindOperator("Judge")) && Dev.TeamIsPlayable(), "the developer profile is unchanged");
	}

	void TestAdmin()
	{
		FProfile Admin(8);
		FAccountInfo Info;
		Info.UserId = "10000001";
		Info.Username = "Owner";
		Info.Email = Config::AdminEmail;
		Info.bSignedIn = true;
		Info.bAdmin = true;
		Admin.StartNewPlayer();
		Admin.ApplyAccount(Info);
		Expect(Admin.bUnlimited, "a signed-in admin has unlimited currencies");
		Expect(OperatorsOwned(Admin) == static_cast<int>(Roster().size()), "the admin owns every operator");
		Expect(!Admin.TutorialActive(), "the admin skips the tutorial");
		Expect(Admin.StagesCleared == TotalCampaignLevels, "the admin has every campaign level (all 13 chapters)");
		Expect(Admin.Meta.Name == "Owner" && Admin.Meta.Uid == 10000001, "the account name and id reach the profile");
		for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
		{
			Expect(Admin.Get(static_cast<ECurrency>(Index)) == Config::UnlimitedAmount, "admin currency reads unlimited #" + std::to_string(Index));
		}
		Expect(Admin.Spend(ECurrency::Diamonds, 2700) && Admin.Get(ECurrency::Diamonds) == Config::UnlimitedAmount, "spending never lowers an admin currency");
		Admin.Add(ECurrency::Gold, 5000);
		Expect(Admin.Get(ECurrency::Gold) == Config::UnlimitedAmount, "an admin currency does not grow either");
		Expect(!Admin.Spend(ECurrency::Gold, -1), "a negative spend is still refused");
		const int Narrator = FindOperator("Narrator");
		const int LevelBefore = Admin.Operators[static_cast<size_t>(Narrator)].Level;
		Expect(Admin.TryLevelUp(Narrator, false).bOk && Admin.Operators[static_cast<size_t>(Narrator)].Level == LevelBefore + 1, "the admin can level up for free");
		Expect(Admin.TryUpgradeStar(Narrator).bOk && Admin.Operators[static_cast<size_t>(Narrator)].UpgradeStars == 1, "the admin can upgrade stars without duplicates");
		int PerSlot[4] = {};
		for (const FEquipment& Item : Admin.Inventory)
		{
			++PerSlot[static_cast<int>(Item.Slot)];
		}
		Expect(PerSlot[0] >= Config::AdminEquipmentPerSlot && PerSlot[1] >= Config::AdminEquipmentPerSlot && PerSlot[2] >= Config::AdminEquipmentPerSlot
			&& PerSlot[3] >= Config::AdminEquipmentPerSlot, "the admin has equipment for every slot");
		Expect(Admin.TeamIsPlayable(), "the admin's squad is playable");

		// Every level is open to the admin, in any order, and only to the admin.
		Expect(CanStartStage(Admin, CampaignStageId(8)).bOk && CanStartStage(Admin, SubStoryStageId(0, 3)).bOk, "the admin may enter any campaign level and sub-story");
		Expect(CanStartStage(Admin, TowerStageId(1, 40)).bOk, "the admin may start any tower floor");
		FProfile Player(8);
		Player.StartNewPlayer();
		Player.GrantAllOperators();
		FAccountInfo PlayerInfo;
		PlayerInfo.UserId = "10000002";
		PlayerInfo.Username = "Someone";
		PlayerInfo.Email = "someone@example.com";
		PlayerInfo.bSignedIn = true;
		Player.ApplyAccount(PlayerInfo);
		Player.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		Player.SetTeamSlot(0, FindOperator("Luna"));
		Player.SetTeamSlot(1, FindOperator("Judge"));
		Player.SetTeamSlot(2, FindOperator("Narrator"));
		Expect(!Player.bUnlimited && !CanStartStage(Player, CampaignStageId(8)).bOk && !CanStartStage(Player, TowerStageId(1, 40)).bOk, "an ordinary account keeps the level order");

		// A guest or an unconfirmed account can never be an admin, whatever a flag says.
		FProfile Guest(8);
		FAccountInfo GuestInfo;
		GuestInfo.bGuest = true;
		GuestInfo.bAdmin = true; // forged
		Guest.StartNewPlayer();
		Guest.ApplyAccount(GuestInfo);
		Expect(!Guest.bUnlimited && OperatorsOwned(Guest) == 1, "a guest is never an admin");

		// Reset keeps the kind of account.
		Admin.Operators[static_cast<size_t>(Narrator)].Level = 50;
		Admin.ResetForAccount();
		Expect(Admin.bUnlimited && Admin.Operators[static_cast<size_t>(Narrator)].Level == 1 && OperatorsOwned(Admin) == 20, "resetting an admin gives a fresh admin profile");
		Expect(Admin.Meta.Name == "Owner" && Admin.Account.Email == Config::AdminEmail, "resetting keeps the account name and e-mail");
		Player.ResetForAccount();
		Expect(OperatorsOwned(Player) == 1 && Player.TutorialActive() && Player.Meta.Name == "Someone", "resetting a player restarts the tutorial");
		FProfile Dev(8);
		Dev.GrantOperator(FindOperator("Kuu"));
		Dev.ResetForAccount();
		Expect(!Dev.Owns(FindOperator("Kuu")) && Dev.Owns(FindOperator("Judge")), "resetting the developer profile gives the developer profile");
	}

	// ====================================================================== rotation
	void TestRotation()
	{
		Expect(FProfile::RotationIndexAt(Anchor) == 0 && FProfile::RotationIndexAt(Anchor + Period - 1) == 0, "period 0 lasts 3 days from the anchor");
		Expect(FProfile::RotationIndexAt(Anchor + Period) == 1 && FProfile::RotationIndexAt(Anchor + 2 * Period) == 2, "a new period every 3 days");
		Expect(FProfile::RotationIndexAt(Anchor - 1) == -1 && FProfile::RotationIndexAt(Anchor - Period) == -1 && FProfile::RotationIndexAt(Anchor - Period - 1) == -2,
			"periods before the anchor count down correctly");
		Expect(FProfile::RotationStartUnix(3) == Anchor + 3 * Period, "period start time");
		Expect(Period == 259200, "3 days is 259,200 seconds");

		const int Judge = FindOperator("Judge");
		const int Narrator = FindOperator("Narrator");
		Expect(FProfile::FeaturedOperatorFor(5, 0) == Judge && FProfile::FeaturedOperatorFor(6, 0) == Narrator, "period 0 features the owner's picks: Judge and Narrator");
		Expect(FProfile::FeaturedOperatorFor(5, 1) == FindOperator("Alchemist") && FProfile::FeaturedOperatorFor(6, 1) == FindOperator("Jester"), "period 1 moves on");
		Expect(FProfile::FeaturedOperatorFor(5, 6) == Judge && FProfile::FeaturedOperatorFor(6, 6) == Narrator, "the cycle wraps");
		Expect(FProfile::FeaturedOperatorFor(5, -1) == FindOperator("Weather") && FProfile::FeaturedOperatorFor(6, -1) == FindOperator("Adapt"), "the cycle wraps backwards too");
		for (int64_t Index = -7; Index < 20; ++Index)
		{
			Expect(Roster()[static_cast<size_t>(FProfile::FeaturedOperatorFor(5, Index))].Stars == 5
				&& Roster()[static_cast<size_t>(FProfile::FeaturedOperatorFor(6, Index))].Stars == 6, "every rotation entry has the right rarity #" + std::to_string(Index));
		}

		FProfile Profile(21);
		const int Featured5 = Profile.FindBanner("featured-5");
		const int Featured6 = Profile.FindBanner("featured-6");
		const int Standard = Profile.FindBanner("standard");
		Expect(Profile.Banners[static_cast<size_t>(Featured5)].FeaturedOperator == Judge && Profile.Banners[static_cast<size_t>(Featured6)].FeaturedOperator == Narrator,
			"a new profile starts on period 0");
		Expect(Profile.SecondsUntilRotation() == -1, "the countdown is unknown until the clock is set");
		Profile.SetTime(0);
		Expect(Profile.NowUnix == 0, "a zero clock is ignored");

		// Pity on the rotating banners and on Standard.
		Profile.Pity[static_cast<size_t>(Featured5)].Since5 = 40;
		Profile.Pity[static_cast<size_t>(Featured5)].Since6 = 700;
		Profile.Pity[static_cast<size_t>(Featured5)].bLostFiftyFifty = true;
		Profile.Pity[static_cast<size_t>(Standard)].Since5 = 12;

		Profile.SetTime(Anchor + 1000);
		Expect(Profile.RotationIndex == 0 && Profile.Pity[static_cast<size_t>(Featured5)].Since5 == 40, "inside period 0 nothing changes");
		Expect(Profile.SecondsUntilRotation() == Period - 1000, "the countdown counts down to the next period");
		Profile.SetTime(Anchor + Period - 1);
		Expect(Profile.SecondsUntilRotation() == 1 && Profile.RotationIndex == 0, "one second before the change");
		Profile.SetTime(Anchor + Period);
		Expect(Profile.RotationIndex == 1, "the rotation happens exactly at the 3-day mark");
		Expect(Profile.Banners[static_cast<size_t>(Featured5)].FeaturedOperator == FindOperator("Alchemist")
			&& Profile.Banners[static_cast<size_t>(Featured6)].FeaturedOperator == FindOperator("Jester"), "the featured operators changed");
		Expect(Profile.Banners[static_cast<size_t>(Featured5)].Title == "Featured: Alchemist (5*)" && Profile.Banners[static_cast<size_t>(Featured6)].Title == "Featured: Jester (6*)",
			"the banner titles follow");
		const FPityState& Reset = Profile.Pity[static_cast<size_t>(Featured5)];
		Expect(Reset.Since5 == 0 && Reset.Since6 == 0 && !Reset.bLostFiftyFifty, "a new featured banner starts with fresh pity");
		Expect(Profile.Pity[static_cast<size_t>(Standard)].Since5 == 12, "the Standard banner's pity is not touched");
		Expect(Profile.SecondsUntilRotation() == Period, "a full period is left right after a change");

		// Being away for a while skips straight to the right period, once.
		Profile.Pity[static_cast<size_t>(Featured6)].Since6 = 300;
		Profile.SetTime(Anchor + 9 * Period + 5);
		Expect(Profile.RotationIndex == 9 && Profile.Banners[static_cast<size_t>(Featured5)].FeaturedOperator == FProfile::FeaturedOperatorFor(5, 9), "catching up after a long absence");
		Expect(Profile.Pity[static_cast<size_t>(Featured6)].Since6 == 0, "pity resets when the featured operator changes");
		Profile.SetTime(Anchor + 9 * Period + 50);
		Profile.Pity[static_cast<size_t>(Featured6)].Since6 = 5;
		Profile.SetTime(Anchor + 9 * Period + 90);
		Expect(Profile.Pity[static_cast<size_t>(Featured6)].Since6 == 5, "same period again: nothing resets");
		Profile.SetTime(Anchor + 5 * Period);
		Expect(Profile.RotationIndex == 5, "the clock going backwards re-selects that period (no crash)");

		// The rotation really drives the summon rules.
		Profile.SetTime(Anchor + Period);
		const int Alchemist = FindOperator("Alchemist");
		Profile.Add(ECurrency::Mileage, 250);
		const FSummonOutcome Redeemed = Profile.RedeemMileage(Featured5);
		Expect(Redeemed.bOk && Redeemed.Results[0].Operator == Alchemist && Profile.Owns(Alchemist), "Mileage redeems the CURRENT featured 5-star");
		Profile.Add(ECurrency::SpecialTicket, 500);
		int FeaturedHits = 0;
		int FiveStars = 0;
		for (int Pull = 0; Pull < 400; ++Pull)
		{
			const FSummonOutcome Pull10 = Profile.Summon(Featured5, 10, EPay::Ticket);
			for (const FSummonResult& Result : Pull10.Results)
			{
				FiveStars += Result.Stars == 5 ? 1 : 0;
				FeaturedHits += Result.bFeatured ? 1 : 0;
				Expect(Result.Operator != Judge || Result.Stars != 5 || !Result.bFeatured, "the previous featured operator is no longer the featured result");
			}
			if (Profile.Get(ECurrency::SpecialTicket) < 9)
			{
				Profile.Add(ECurrency::SpecialTicket, 500);
			}
		}
		Expect(FiveStars > 30 && FeaturedHits > 0, "the new featured operator shows up in featured summons");

		// A reset keeps the clock, so the banners do not jump back to period 0.
		Profile.ResetToNewGame();
		Expect(Profile.RotationIndex == 1 && Profile.Banners[static_cast<size_t>(Profile.FindBanner("featured-5"))].FeaturedOperator == Alchemist, "reset keeps the current period");

		// Pages show the countdown.
		FProfile Page(2);
		Page.SetTime(Anchor + Period - 3 * 3600 - 120);
		P::FUiState State;
		State.Tab = P::TabSummon;
		State.SelBanner = Page.FindBanner("featured-5");
		const P::FPage Built = P::Build(Page, State);
		Expect(PageHas(Built, "change every 3 days") && PageHas(Built, "next change in 3h 02m"), "the Summon page shows the countdown");
		bool bTileShows = false;
		for (const P::FPageAction& Tile : Built.Grid)
		{
			bTileShows = bTileShows || Contains(Tile.Sub, "changes in 3h 02m");
		}
		Expect(bTileShows, "the banner tiles show the countdown");
		Expect(P::Duration(90061) == "1d 1h" && P::Duration(3725) == "1h 02m" && P::Duration(65) == "1m 05s" && P::Duration(-1) == "--", "duration formatting");
	}

	// ======================================================================= save
	// A profile with a lot of progress in every system, used by the round-trip tests.
	void Progress(FProfile& Profile)
	{
		Profile.SetTime(Anchor + 2 * Period + 777);
		Profile.Add(ECurrency::Gold, 123456);
		Profile.Add(ECurrency::SpecialTicket, 90);
		for (int Round = 0; Round < 6; ++Round)
		{
			Profile.Summon(Profile.FindBanner("standard"), 10, EPay::Diamonds);
			Profile.Summon(Profile.FindBanner("featured-5"), 10, EPay::Ticket);
		}
		const int Luna = FindOperator("Luna");
		for (int Level = 0; Level < 25; ++Level)
		{
			Profile.TryLevelUp(Luna, true);
			if (Profile.NeedsHeartGate(Luna))
			{
				Profile.TryPayHeartGate(Luna);
			}
		}
		Profile.Operators[static_cast<size_t>(Luna)].DupResource = 80;
		Profile.TryUpgradeStar(Luna);
		Profile.Equip(Profile.Inventory[0].Id, Luna);
		Profile.Equip(Profile.Inventory[1].Id, FindOperator("Judge"));
		Profile.UpgradeEquipment(Profile.Inventory[0].Id);
		Profile.AddRandomEquipment();
		Profile.SetTeamSlot(1, FindOperator("Narrator"));
		ClearStage(Profile, CampaignStageId(1));
		ClearStage(Profile, CampaignStageId(2));
		ClearStage(Profile, TowerStageId(0, 1));
		ClearStage(Profile, SubStoryStageId(1, 1));
		Profile.AdvanceDay();
		ClaimLogin(Profile);
		AdvanceWorld(Profile, 30.5);
		Profile.Add(ECurrency::Gold, 90000);
		Profile.Add(ECurrency::Bytes, 90000);
		BuildOrUpgrade(Profile, 0);
		BuildOrUpgrade(Profile, 3);
		AddCommanderXp(Profile, 4321);
		Profile.Meta.SeasonXp = 1500;
		ClaimAllSeason(Profile);
		Arcade::StartGame(Profile, Arcade::EGame::Rhythm);
		Profile.Meta.ArcadeRecords.NightWins[2] = 3;
		Profile.Meta.ArcadeRecords.RhythmBest[4] = 98765;
		Profile.Meta.ArcadeRecords.BossRushBest = 12;
		Profile.Meta.TowerLostAt[1] = 37;
		Profile.Meta.Arena.Attack[0] = Luna;
		Profile.Meta.Arena.Defence[2] = FindOperator("Narrator");
		Profile.Meta.Arena.Trophies = 1234;
		Profile.Meta.Arena.PendingRewardRank = 7;
		Profile.Meta.Settings.TopBar[5] = static_cast<int>(ECurrency::GangCoin);
		Profile.Meta.Settings.KeyBinds[4] = "LeftControl";
		Profile.Meta.Settings.HudScale = 115;
		Profile.Meta.Settings.bSoundCaptions = true;
		Profile.Meta.LastActiveUnix = 1791072000;
		Profile.Meta.SeasonPassKey = 2026 * 12 + 9;
		Profile.Meta.Return.StartUnix = 1791072000;
		Profile.Meta.Return.DaysClaimed = 5;
		Profile.Meta.Return.QuestClaimed[3] = true;
		Profile.Meta.Gang.ShopBought[1] = 2;
		Profile.Meta.ReadNotices = {"n1", "n2"};
		Profile.Meta.ImportedMail = {"m9"};
		Profile.Add(ECurrency::GangCoin, 345);
		Profile.Meta.Settings.MusicVolume = 30;
		Profile.Meta.Settings.MasterVolume = 40;
		Profile.Meta.Settings.bInvertY = true;
		Profile.Meta.Settings.FieldOfView = 105;
		Profile.Meta.Settings.bPageTransitions = false;
		Profile.Meta.Weekly[2].Progress = 7;
		Profile.Meta.Story[4].bClaimed = true;
		Profile.Meta.Settings.LobbyOperator = Luna;
		Profile.Meta.Name = "Nova \"the\" Commander";
		Profile.Pity[static_cast<size_t>(Profile.FindBanner("featured-6"))].Since6 = 321;
		Profile.Pity[static_cast<size_t>(Profile.FindBanner("featured-6"))].bLostFiftyFifty = true;
		Profile.bTutorialSummonDone = true;
	}

	void TestSaveRoundTrip()
	{
		FProfile Original(77);
		Progress(Original);
		const std::string Text = Save::Serialize(Original, 1800000000);

		FProfile Loaded(1234); // a different seed and a fresh default state, as after a restart
		std::string Error;
		Expect(Save::Deserialize(Text, Loaded, Error), "a save loads: " + Error);
		Expect(Save::Serialize(Loaded, 1800000000) == Text, "saving what was loaded gives byte-identical text");

		for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
		{
			Expect(Loaded.Raw(static_cast<ECurrency>(Index)) == Original.Raw(static_cast<ECurrency>(Index)), "currency kept #" + std::to_string(Index));
		}
		for (size_t Index = 0; Index < Roster().size(); ++Index)
		{
			const FOwnedOperator& A = Original.Operators[Index];
			const FOwnedOperator& B = Loaded.Operators[Index];
			Expect(A.bOwned == B.bOwned && A.Level == B.Level && A.UpgradeStars == B.UpgradeStars && A.DupResource == B.DupResource && A.HeartGatesPaid == B.HeartGatesPaid,
				std::string("operator kept: ") + Roster()[Index].Name);
		}
		Expect(Loaded.Operators[static_cast<size_t>(FindOperator("Luna"))].Level > 20, "Luna's level survived (it was raised in the test)");
		Expect(Loaded.Inventory.size() == Original.Inventory.size(), "equipment count kept");
		for (size_t Index = 0; Index < Original.Inventory.size(); ++Index)
		{
			const FEquipment& A = Original.Inventory[Index];
			const FEquipment& B = Loaded.Inventory[Index];
			Expect(A.Id == B.Id && A.Slot == B.Slot && A.Bonus == B.Bonus && A.Percent == B.Percent && A.UpgradeLevel == B.UpgradeLevel && A.EquippedBy == B.EquippedBy && A.Name == B.Name,
				"equipment kept #" + std::to_string(A.Id));
		}
		Expect(Loaded.AddRandomEquipment().Id == Original.AddRandomEquipment().Id, "new equipment ids continue where they left off");
		for (int Slot = 0; Slot < 3; ++Slot)
		{
			Expect(Loaded.Team[Slot] == Original.Team[Slot], "squad kept");
		}
		for (size_t Index = 0; Index < Original.Banners.size(); ++Index)
		{
			Expect(Loaded.Banners[Index].Id == Original.Banners[Index].Id && Loaded.Banners[Index].FeaturedOperator == Original.Banners[Index].FeaturedOperator
				&& Loaded.Banners[Index].Title == Original.Banners[Index].Title, "banner kept: " + Original.Banners[Index].Id);
			Expect(Loaded.Pity[Index].Since5 == Original.Pity[Index].Since5 && Loaded.Pity[Index].Since6 == Original.Pity[Index].Since6
				&& Loaded.Pity[Index].bLostFiftyFifty == Original.Pity[Index].bLostFiftyFifty, "pity kept: " + Original.Banners[Index].Id);
		}
		Expect(Loaded.RotationIndex == 2 && Loaded.Banners[static_cast<size_t>(Loaded.FindBanner("featured-5"))].FeaturedOperator == FProfile::FeaturedOperatorFor(5, 2), "the 3-day rotation period kept");
		Expect(Loaded.bTutorialSummonDone && Loaded.TutorialStep == Original.TutorialStep, "tutorial state kept");
		Expect(Loaded.Day == Original.Day && Loaded.StagesCleared == Original.StagesCleared && Loaded.StagesCleared == 2, "day and stages cleared kept");
		Expect(Loaded.Meta.Name == "Nova \"the\" Commander" && Loaded.Meta.CommanderXp == Original.Meta.CommanderXp && Loaded.Meta.SeasonXp == 1500, "commander data kept");
		Expect(Loaded.Meta.TowerBest[0] == 1 && Loaded.Meta.SubStoryCleared[1] == 1, "tower and sub-story progress kept");
		Expect(Loaded.Meta.Mail.size() == Original.Meta.Mail.size() && Loaded.Meta.NextMailId == Original.Meta.NextMailId, "mail kept");
		Expect(Loaded.Meta.Buildings[0].bBuilt && Loaded.Meta.Buildings[3].bBuilt && Loaded.Meta.WorldHours == Original.Meta.WorldHours, "Outer World kept");
		Expect(Loaded.Meta.Settings.MusicVolume == 30 && Loaded.Meta.Settings.LobbyOperator == FindOperator("Luna"), "settings kept");
		Expect(Loaded.Meta.Settings.MasterVolume == 40 && Loaded.Meta.Settings.bInvertY && Loaded.Meta.Settings.FieldOfView == 105 && !Loaded.Meta.Settings.bPageTransitions, "new settings kept");
		Expect(Loaded.Meta.Weekly[2].Progress == 7 && Loaded.Meta.Story[4].bClaimed, "weekly and story quests kept");
		Expect(Loaded.Meta.ArcadeRecords.Plays[2] == 1 && Loaded.Meta.ArcadeRecords.NightWins[2] == 3 && Loaded.Meta.ArcadeRecords.RhythmBest[4] == 98765
			&& Loaded.Meta.ArcadeRecords.BossRushBest == 12, "arcade records kept");
		Expect(Loaded.Meta.TowerLostAt[1] == 37 && Loaded.Meta.Arena.Attack[0] == FindOperator("Luna") && Loaded.Meta.Arena.Defence[2] == FindOperator("Narrator")
			&& Loaded.Meta.Arena.Trophies == 1234 && Loaded.Meta.Arena.PendingRewardRank == 7, "tower and arena kept");
		Expect(Loaded.Meta.Settings.TopBar[5] == static_cast<int>(ECurrency::GangCoin) && Loaded.Meta.Settings.KeyBinds[4] == "LeftControl"
			&& Loaded.Meta.Settings.HudScale == 115 && Loaded.Meta.Settings.bSoundCaptions, "top bar, key bindings and preferences kept");
		Expect(Loaded.Meta.LastActiveUnix == 1791072000 && Loaded.Meta.SeasonPassKey == 2026 * 12 + 9 && Loaded.Meta.Return.StartUnix == 1791072000
			&& Loaded.Meta.Return.DaysClaimed == 5 && Loaded.Meta.Return.QuestClaimed[3], "live-ops state kept");
		Expect(Loaded.Meta.Gang.ShopBought[1] == 2 && Loaded.Meta.ReadNotices.size() == 2 && Loaded.Meta.ImportedMail.size() == 1
			&& Loaded.Get(ECurrency::GangCoin) == Original.Get(ECurrency::GangCoin), "gang shop, notices read, imported mail and Gang coins kept");
		for (int Tier = 0; Tier <= SeasonTiers; ++Tier)
		{
			Expect(Loaded.Meta.FreeClaimed[Tier] == Original.Meta.FreeClaimed[Tier] && Loaded.Meta.PremiumClaimed[Tier] == Original.Meta.PremiumClaimed[Tier], "season claim kept");
		}

		// The header.
		const Save::FSaveInfo Info = Save::Peek(Text);
		Expect(Info.bValid && Info.Version == Save::FormatVersion && Info.SavedAtUnix == 1800000000 && Info.UserId.empty(), "save header");
		FProfile Owned(3);
		FAccountInfo Account;
		Account.UserId = "10000007";
		Account.bSignedIn = true;
		Owned.ApplyAccount(Account);
		Expect(Save::Peek(Save::Serialize(Owned, 5)).UserId == "10000007", "the save records which account it belongs to");
		Expect(!Save::Peek("not json").bValid && !Save::Peek("{\"format\":\"other\"}").bValid, "Peek rejects other files");

		// Loading keeps the account, the clock and does not touch the random generator's owner.
		FProfile Signed(9);
		Signed.ApplyAccount(Account);
		Signed.SetTime(Anchor + 5 * Period);
		Expect(Save::Deserialize(Text, Signed, Error), "load into a signed-in profile");
		Expect(Signed.Account.UserId == "10000007" && Signed.NowUnix == Anchor + 5 * Period, "loading keeps the account and the clock");
		Signed.SetTime(Anchor + 5 * Period);
		Expect(Signed.RotationIndex == 5, "after loading, the clock catches the rotation up");

		// Every tutorial step survives a save.
		for (int Step = 0; Step <= static_cast<int>(ETutorialStep::Complete); ++Step)
		{
			FProfile Step1(4);
			Step1.StartNewPlayer();
			Step1.TutorialStep = Step;
			FProfile Step2(4);
			Expect(Save::Deserialize(Save::Serialize(Step1, 10), Step2, Error) && Step2.TutorialStep == Step, "tutorial step survives a save #" + std::to_string(Step));
		}
	}

	void TestSaveSafety()
	{
		FProfile Profile(5);
		Progress(Profile);
		const std::string Before = Save::Serialize(Profile, 1);
		std::string Error;

		const char* Broken[] = {"", "not json", "[]", "{}", "{\"format\":\"boldinbeauty-save\",\"version\":1}",
			"{\"format\":\"boldinbeauty-save\",\"version\":99,\"profile\":{}}", "{\"format\":\"nope\",\"version\":1,\"profile\":{}}",
			"{\"format\":\"boldinbeauty-save\",\"version\":1,\"profile\":[1]}"};
		for (const char* Text : Broken)
		{
			Expect(!Save::Deserialize(Text, Profile, Error) && !Error.empty(), std::string("a broken save is refused: ") + Text);
		}
		Expect(Save::Serialize(Profile, 1) == Before, "a refused save leaves the profile untouched");

		// A truncated save (power cut) is refused, never half-loaded.
		const std::string Good = Save::Serialize(Profile, 1);
		for (size_t Cut : {size_t(1), Good.size() / 4, Good.size() / 2, Good.size() - 1})
		{
			FProfile Target(1);
			const std::string Snapshot = Save::Serialize(Target, 1);
			Expect(!Save::Deserialize(Good.substr(0, Cut), Target, Error), "a truncated save is refused");
			Expect(Save::Serialize(Target, 1) == Snapshot, "a truncated save changes nothing");
		}

		// Hostile numbers and names are sanitised, not trusted.
		const std::string Hostile = std::string("{\"format\":\"boldinbeauty-save\",\"version\":1,\"savedAt\":1,\"profile\":{")
			+ "\"currencies\":{\"gold\":-500,\"diamonds\":99999999999999,\"bytes\":1.5e300},"
			+ "\"operators\":{\"Luna\":{\"level\":999999,\"stars\":-4,\"dup\":-1,\"gates\":77},\"Nobody\":{\"level\":3},\"Judge\":{\"level\":0}},"
			+ "\"inventory\":[{\"id\":5,\"slot\":2,\"bonus\":1,\"percent\":9999,\"upgrade\":99,\"equippedBy\":\"Luna\",\"name\":\"A\"},"
			+ "{\"id\":5,\"slot\":2,\"bonus\":1,\"percent\":3,\"equippedBy\":\"Luna\",\"name\":\"duplicate id\"},"
			+ "{\"id\":6,\"slot\":2,\"bonus\":0,\"percent\":3,\"equippedBy\":\"Luna\",\"name\":\"same slot\"},"
			+ "{\"id\":7,\"slot\":9,\"bonus\":9,\"percent\":-5,\"equippedBy\":\"Kuu\",\"name\":\"bad slot, unowned wearer\"}],"
			+ "\"nextEquipmentId\":1,\"team\":[\"Luna\",\"Luna\",\"Kuu\"],\"day\":-3,\"stagesCleared\":500,"
			+ "\"tutorial\":{\"step\":\"made-up\"},\"meta\":{\"name\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"uid\":-1,\"seasonXp\":-9,\"arcadeRecords\":{\"nightBestHour\":[99,-3]},"
			+ "\"arena\":{\"trophies\":-5,\"pendingRank\":40,\"attack\":[\"Nobody\",\"Luna\"]},\"return\":{\"days\":99999},\"readNotices\":[1,2],"
			+ "\"settings\":{\"music\":900,\"graphics\":-2,\"mouseSensitivity\":0,\"hudScale\":999,\"topBar\":[77],\"keys\":{\"0\":\"Bad Key!\"}}}}}";
		FProfile Target(2);
		Expect(Save::Deserialize(Hostile, Target, Error), "a hostile save still loads, sanitised: " + Error);
		Expect(Target.Raw(ECurrency::Gold) == 0 && Target.Raw(ECurrency::Diamonds) == 2000000000 && Target.Raw(ECurrency::Bytes) >= 0, "currencies are clamped");
		const FOwnedOperator& Luna = Target.Operators[static_cast<size_t>(FindOperator("Luna"))];
		Expect(Luna.bOwned && Luna.UpgradeStars == 0 && Luna.Level == Config::BaseLevelCap && Luna.DupResource == 0 && Luna.HeartGatesPaid == Luna.Level / Config::HeartGateEvery,
			"Luna's level is capped by her upgrade stars");
		Expect(Target.Operators[static_cast<size_t>(FindOperator("Judge"))].Level == 1, "a level below 1 becomes 1");
		Expect(Target.Inventory.size() == 3, "a duplicate equipment id is dropped");
		const FEquipment* Worn = Target.EquippedIn(FindOperator("Luna"), ESlot::Leggings);
		Expect(Worn && Worn->Id == 5 && Worn->Percent <= 100.0 && Worn->UpgradeLevel == Config::EquipMaxUpgrade, "equipment values are clamped");
		int Wearing = 0;
		for (const FEquipment& Item : Target.Inventory)
		{
			Wearing += (Item.EquippedBy == FindOperator("Luna") && Item.Slot == ESlot::Leggings) ? 1 : 0;
			Expect(Item.EquippedBy != FindOperator("Kuu"), "an operator that is not owned cannot wear equipment");
		}
		Expect(Wearing == 1, "only one item per slot is worn");
		Expect(Target.Team[0] == FindOperator("Luna") && Target.Team[1] < 0 && Target.Team[2] < 0, "squad: duplicates and unowned operators are removed");
		Expect(Target.NextEquipmentIdValue() > 7, "the next equipment id is above every existing id");
		Expect(Target.Day == 1 && Target.StagesCleared == TotalCampaignLevels, "day and stages are clamped (to the 104 campaign stages)");
		Expect(Target.TutorialStep == static_cast<int>(ETutorialStep::Complete), "an unknown tutorial step never traps the player");
		Expect(Target.Meta.Name.size() <= 24 && Target.Meta.Uid >= 1 && Target.Meta.SeasonXp == 0 && Target.Meta.ArcadeRecords.NightBestHour[0] == 6
			&& Target.Meta.ArcadeRecords.NightBestHour[1] == 0, "meta values are clamped");
		Expect(Target.Meta.Arena.Trophies == 0 && Target.Meta.Arena.PendingRewardRank == 15 && Target.Meta.Arena.Attack[0] == -1 && Target.Meta.Arena.Attack[1] == FindOperator("Luna")
			&& Target.Meta.Return.DaysClaimed == 127 && Target.Meta.ReadNotices.empty(), "arena, return and notice state are sanitised");
		Expect(Target.Meta.Settings.HudScale == 125 && Target.Meta.Settings.TopBar[0] == TopBarChoiceCount - 1 && Target.Meta.Settings.KeyBinds[0].empty(),
			"new settings are clamped; a key name with odd characters is dropped");
		Expect(Target.Meta.Settings.MusicVolume == 100 && Target.Meta.Settings.GraphicsQuality == 0 && Target.Meta.Settings.MouseSensitivity == 1, "settings are clamped");

		// An empty collection still has Luna (MAIN section 5).
		FProfile Bare(2);
		const std::string NoOperators = "{\"format\":\"boldinbeauty-save\",\"version\":1,\"profile\":{\"operators\":{}}}";
		Expect(Save::Deserialize(NoOperators, Bare, Error) && Bare.Owns(FindOperator("Luna")), "Luna is always restored");

		// A save from before the rotation pools changed: the old pity does not carry over to a different featured operator.
		FProfile Old(2);
		const std::string Stale = "{\"format\":\"boldinbeauty-save\",\"version\":1,\"profile\":{\"rotationIndex\":0,\"banners\":{\"featured-5\":{\"since5\":88,\"featured\":\"Weather\"},\"featured-6\":{\"since5\":55,\"featured\":\"Narrator\"}}}}";
		Expect(Save::Deserialize(Stale, Old, Error), "stale banner save loads");
		Expect(Old.Pity[static_cast<size_t>(Old.FindBanner("featured-5"))].Since5 == 0 && Old.Pity[static_cast<size_t>(Old.FindBanner("featured-6"))].Since5 == 55, "pity only carries over to the same featured operator");
	}

	void TestSaveAdmin()
	{
		FProfile Admin(8);
		FAccountInfo Info;
		Info.UserId = "10000001";
		Info.Username = "Owner";
		Info.Email = Config::AdminEmail;
		Info.bSignedIn = true;
		Info.bAdmin = true;
		Admin.StartNewPlayer();
		Admin.ApplyAccount(Info);
		Admin.TryLevelUp(FindOperator("Narrator"), false);
		const std::string Text = Save::Serialize(Admin, 99);
		Expect(!Contains(Text, "999999999\"") && !Contains(Text, ":999999999,\"diamonds\""), "the unlimited display value is not what gets saved");
		Expect(Save::Peek(Text).UserId == "10000001", "an admin save records the id");

		FProfile Again(8);
		Again.ApplyAccount(Info);
		std::string Error;
		Expect(Save::Deserialize(Text, Again, Error), "an admin save loads");
		Again.ApplyAccount(Info);
		Expect(Again.bUnlimited && OperatorsOwned(Again) == 20 && Again.Operators[static_cast<size_t>(FindOperator("Narrator"))].Level == 2, "after loading, the admin entitlements apply and progress is kept");

		// A player cannot become an admin by editing a save: there is no admin field in it.
		std::string Edited = Text;
		Expect(!Contains(Text, "admin") && !Contains(Text, "Admin") && !Contains(Text, "unlimited"), "a save carries no admin flag to forge");
		FProfile Player(8);
		FAccountInfo PlayerInfo;
		PlayerInfo.UserId = "10000002";
		PlayerInfo.bSignedIn = true;
		Player.ApplyAccount(PlayerInfo);
		Expect(Save::Deserialize(Edited, Player, Error), "an admin save file loads into another account's profile (the game compares the user id before doing this)");
		Player.ApplyAccount(PlayerInfo);
		Expect(!Player.bUnlimited, "...but never makes that account an admin");
	}

	// =================================================================== tutorial
	void TestTutorialWalkthrough()
	{
		FProfile Profile(31);
		Profile.StartNewPlayer();
		const int Luna = FindOperator("Luna");
		const int Kuu = FindOperator("Kuu");
		const int Plack = FindOperator("Plack");
		P::FUiState State; // the hub opens on the Lobby tab
		auto Do = [&](const std::string& Id) { return P::Perform(Profile, State, Id); };
		auto Step = [&]() { return static_cast<ETutorialStep>(Profile.TutorialStep); };

		// Step 1: nothing but the Operator tab.
		P::FPage Page = P::Build(Profile, State);
		Expect(Contains(Page.Coach, "TUTORIAL 1/9") && Contains(Page.Coach, "Luna") && !Page.Lines.empty() && Page.Lines[0] == Page.Coach, "step 1: the instruction is the first line");
		Expect(Page.NavHighlight == P::TabOperator && !Page.NavLocked[P::TabOperator] && Page.NavLocked[P::TabTeam] && Page.NavLocked[P::TabSummon] && Page.NavLocked[P::TabCampaign],
			"step 1: only the Operator tab is open and highlighted");
		for (const P::FPageAction& Tile : Page.Grid)
		{
			Expect(!Tile.bEnabled, "step 1: lobby tile is greyed out: " + Tile.Id);
		}
		const P::FPerform Refused = Do("go:profile");
		Expect(!Refused.bOk && Contains(Refused.Message, "Follow the tutorial") && State.Page.empty(), "step 1: other actions are refused and do nothing");
		Expect(!Do("tab:6").bOk && !Do("tab:5").bOk && !Do("demo:currency").bOk && State.Tab == P::TabLobby, "step 1: other tabs and the demo tools are refused");
		Expect(Profile.Get(ECurrency::Gold) == 0 && Profile.Get(ECurrency::Diamonds) == Config::StartingDiamonds, "step 1: a refused action had no effect");

		// Step 2: tap Luna (the only enabled tile).
		Expect(Do("tab:0").bOk && Step() == ETutorialStep::SelectLuna, "step 2: opening the Operator tab advances");
		Page = P::Build(Profile, State);
		const P::FPageAction* LunaTile = FindAction(Page, "op:sel:" + std::to_string(Luna));
		Expect(LunaTile && LunaTile->bEnabled && LunaTile->bHighlight, "step 2: Luna's tile is enabled and highlighted");
		int Enabled = 0;
		for (const P::FPageAction& Tile : Page.Grid)
		{
			Enabled += Tile.bEnabled ? 1 : 0;
		}
		Expect(Enabled == 1, "step 2: Luna's is the only enabled tile");
		Expect(!Do("op:sel:" + std::to_string(FindOperator("Judge"))).bOk, "step 2: another operator cannot be opened");
		Expect(Do("op:sel:" + std::to_string(Luna)).bOk && Step() == ETutorialStep::LevelUpLuna, "step 3: tapping Luna advances");

		// Step 3: level up once.
		Page = P::Build(Profile, State);
		const P::FPageAction* Level = FindAction(Page, "op:lvl");
		const P::FPageAction* LevelDiamonds = FindAction(Page, "op:lvld");
		Expect(Level && Level->bEnabled && Level->bHighlight && LevelDiamonds && !LevelDiamonds->bEnabled, "step 3: only plain Level up is allowed");
		Expect(!Do("op:star").bOk && !Do("op:slot:0").bOk && !Do("op:next").bOk, "step 3: nothing else on the page works");
		Expect(Do("op:lvl").bOk && Profile.Operators[static_cast<size_t>(Luna)].Level == 2 && Profile.Get(ECurrency::Diamonds) == Config::StartingDiamonds, "step 3: Luna reached level 2 (free: the new wallet has no Gold)");
		Expect(Step() == ETutorialStep::OpenTeam, "step 4: levelling up advances");

		// Step 4/5: the Team tab, an empty squad, tap a slot.
		Page = P::Build(Profile, State);
		Expect(Page.NavHighlight == P::TabTeam && !Page.NavLocked[P::TabTeam], "step 4: the Team tab is highlighted");
		Expect(!Do("tab:6").bOk, "step 4: Summon is still locked");
		Expect(Do("tab:1").bOk && Step() == ETutorialStep::AddLuna, "step 5: opening the Team tab advances");
		Page = P::Build(Profile, State);
		Expect(PageHas(Page, "Slot 1: empty") && PageHas(Page, "Slot 2: empty") && PageHas(Page, "Slot 3: empty"), "step 5: the squad is shown empty");
		Expect(!Do("team:swap:0").bOk && !Do("team:rot").bOk && !Do("team:default").bOk, "step 5: only filling a slot is allowed");
		Expect(Do("team:cycle:2").bOk, "step 5: tapping an empty slot works");
		Expect(Profile.Team[0] == Luna && Profile.Team[1] < 0 && Profile.Team[2] < 0, "step 5: Luna (the only owned operator) was added, and the squad is tidied so she leads");
		Expect(Step() == ETutorialStep::OpenSummon, "step 6: filling the slot advances");
		Expect(!Do("team:cycle:1").bOk && Profile.Team[1] < 0, "step 6: the squad can no longer be edited");

		// Step 6/7: the forced summon.
		Expect(Do("tab:6").bOk && Step() == ETutorialStep::SummonKuu, "step 7: opening the Summon tab advances");
		Page = P::Build(Profile, State);
		const P::FPageAction* Forced = FindAction(Page, "sum:tut");
		Expect(Forced && Forced->bEnabled && Forced->bHighlight, "step 7: the tutorial summon is highlighted");
		for (const char* Blocked : {"sum:go:1:0", "sum:go:10:0", "sum:go:1:1", "sum:go:10:1", "sum:mileage", "sum:banner:2"})
		{
			const P::FPageAction* Button = FindAction(Page, Blocked);
			Expect(!Button || !Button->bEnabled, std::string("step 7: summon option is greyed out: ") + Blocked);
			Expect(!Do(Blocked).bOk, std::string("step 7: summon option is refused: ") + Blocked);
		}
		Expect(Profile.Get(ECurrency::RegularTicket) == 0 && Profile.Get(ECurrency::Diamonds) == Config::StartingDiamonds && Profile.Meta.Summons == 0, "step 7: refused summons cost nothing");
		Expect(Do("sum:tut").bOk, "step 7: the forced summon works");
		Expect(State.SummonOps.size() == 1 && State.SummonOps[0] == Kuu && Profile.Owns(Kuu), "step 7: it summons Kuu, always");
		Expect(Profile.Team[1] == Kuu, "step 8: Kuu joins the squad");
		Expect(Step() == ETutorialStep::OpenCampaign, "step 8: the summon advances");
		Page = P::Build(Profile, State);
		Expect(Page.NavHighlight == P::TabCampaign && !Page.NavLocked[P::TabCampaign], "step 8: the Campaign tab is highlighted");
		Expect(!Do("sum:again").bOk && !Do("sum:mileage").bOk, "step 8: no second summon");

		// Step 8/9: play 1-1.
		Expect(Do("tab:5").bOk && Step() == ETutorialStep::PlayLevel1, "step 9: opening the Campaign tab advances");
		Expect(Do("camp:ch:1").bOk, "step 9: Chapter 1 opens");
		Page = P::Build(Profile, State);
		const P::FPageAction* Start1 = FindAction(Page, "camp:start:1");
		const P::FPageAction* Start2 = FindAction(Page, "camp:start:2");
		Expect(Start1 && Start1->bEnabled && Start1->bHighlight && Start2 && !Start2->bEnabled, "step 9: only Level 1 is open");
		Expect(!Do("camp:start:2").bOk, "step 9: Level 2 is refused");
		Expect(Profile.TeamIsPlayable() && Profile.RequiredTeamSize() == 2, "step 9: Luna and Kuu make a playable squad");
		const P::FPerform Start = Do("camp:start:1");
		Expect(Start.bOk && Start.bStartStage && Start.StageId == CampaignStageId(1), "step 9: Level 1 starts");
		Expect(!Profile.Owns(Plack) && Step() == ETutorialStep::PlayLevel1, "step 9: Plack is not given until the level is cleared");

		// Clearing it unlocks Plack for free and ends the tutorial.
		const std::vector<std::string> Rewards = ClearStage(Profile, CampaignStageId(1));
		bool bAnnounced = false;
		for (const std::string& Line : Rewards)
		{
			bAnnounced = bAnnounced || (Contains(Line, "TUTORIAL COMPLETE") && Contains(Line, "Plack"));
		}
		Expect(bAnnounced, "clearing Level 1 announces the tutorial reward");
		Expect(Profile.Owns(Plack) && Profile.Team[2] == Plack, "Plack is unlocked for free and joins the squad");
		Expect(Step() == ETutorialStep::Complete && !Profile.TutorialActive(), "the tutorial is complete");
		Expect(Profile.StagesCleared == 1 && Profile.Operators[static_cast<size_t>(Plack)].DupResource == 0, "level 1 is cleared; Plack is a real unlock, not a duplicate");
		Expect(Profile.TeamIsPlayable() && Profile.RequiredTeamSize() == 3, "Luna, Kuu and Plack make a full squad");

		// Afterwards everything is free again.
		Page = P::Build(Profile, State);
		Expect(Page.Coach.empty() && Page.NavHighlight < 0, "no coaching after the tutorial");
		for (int Tab = 0; Tab < P::TabCount; ++Tab)
		{
			Expect(!Page.NavLocked[Tab], "all tabs open after the tutorial");
		}
		Expect(Do("tab:2").bOk && Do("go:profile").bOk && State.Page == "profile", "normal navigation works afterwards");
		Expect(Do("tab:6").bOk, "the Summon tab works afterwards");
		State.SelBanner = Profile.FindBanner("standard");
		Expect(Do("sum:go:1:1").bOk, "ordinary summons work afterwards (the starting Diamonds pay for one)");
		Expect(Do("tab:5").bOk && Do("camp:ch:1").bOk && Do("camp:start:2").bStartStage, "level 2 opens after level 1");
	}

	void TestTutorialResumeAndEdgeCases()
	{
		// Resuming from a save at every step: the page model never gets stuck.
		const int Luna = FindOperator("Luna");
		for (int Step = 0; Step < static_cast<int>(ETutorialStep::Complete); ++Step)
		{
			FProfile Profile(40 + Step);
			Profile.StartNewPlayer();
			Profile.TutorialStep = Step;
			if (Step >= static_cast<int>(ETutorialStep::OpenTeam))
			{
				Profile.Operators[static_cast<size_t>(Luna)].Level = 2;
			}
			if (Step >= static_cast<int>(ETutorialStep::OpenSummon))
			{
				Profile.Team[0] = Luna;
			}
			if (Step >= static_cast<int>(ETutorialStep::OpenCampaign))
			{
				Profile.TutorialSummon();
				Profile.AutoPlaceInTeam(FindOperator("Kuu"));
			}
			P::FUiState State;
			const P::FPage Page = P::Build(Profile, State);
			Expect(!Page.Coach.empty(), "an instruction is shown at step " + std::to_string(Step + 1));
			bool bSomethingToDo = false;
			for (int Tab = 0; Tab < P::TabCount; ++Tab)
			{
				bSomethingToDo = bSomethingToDo || !Page.NavLocked[Tab];
			}
			for (const P::FPageAction& Action : Page.Grid)
			{
				bSomethingToDo = bSomethingToDo || Action.bEnabled;
			}
			Expect(bSomethingToDo, "there is always something the player can do at step " + std::to_string(Step + 1));
		}

		// Already standing on the right tab: no click needed to move on.
		FProfile Profile(50);
		Profile.StartNewPlayer();
		P::FUiState State;
		State.Tab = P::TabOperator;
		State.Page = "op";
		const P::FPage Page = P::Build(Profile, State);
		Expect(Profile.TutorialStep == static_cast<int>(ETutorialStep::LevelUpLuna), "steps 1 and 2 are skipped when the player is already looking at Luna");
		Expect(PageHas(Page, "Level up"), "...and the instruction follows");

		// Back is always allowed.
		Expect(P::Perform(Profile, State, "back").bOk, "back is always allowed");

		// The old path: a profile that already cleared level 1 finishes the tutorial instead of getting stuck.
		FProfile Old(51);
		Old.StartNewPlayer();
		Old.TutorialStep = static_cast<int>(ETutorialStep::PlayLevel1);
		Old.StagesCleared = 3;
		P::FUiState OldState;
		P::Build(Old, OldState);
		Expect(!Old.TutorialActive() && Old.Owns(FindOperator("Plack")), "a profile that already cleared level 1 completes the tutorial");

		// The Chapter 1 boss: a player who already has Plack (the tutorial gave him) gets the normal duplicate reward.
		FProfile Boss(52);
		Boss.StartNewPlayer();
		Boss.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		Boss.GrantOperator(FindOperator("Plack"));
		const std::vector<std::string> BossLines = Boss.ApplyStageRewards(8, true);
		Expect(Boss.Operators[static_cast<size_t>(FindOperator("Plack"))].DupResource == Config::DuplicateResource, "the boss gives the duplicate bonus to a player who has Plack");
		bool bDuplicateLine = false;
		for (const std::string& Line : BossLines)
		{
			bDuplicateLine = bDuplicateLine || Contains(Line, "Plack duplicate");
		}
		Expect(bDuplicateLine, "...and says so");
		FProfile Legacy(53);
		Legacy.ApplyStageRewards(1, false);
		Expect(!Legacy.Owns(FindOperator("Plack")), "without the tutorial, level 1 gives no Plack");
		Legacy.ApplyStageRewards(8, true);
		Expect(Legacy.Owns(FindOperator("Plack")), "without the tutorial, the Chapter 1 boss still gives Plack");

		// Admin: no tutorial, normal pages.
		FProfile Admin(54);
		FAccountInfo Info;
		Info.UserId = "10000001";
		Info.Username = "Owner";
		Info.Email = Config::AdminEmail;
		Info.bSignedIn = true;
		Info.bAdmin = true;
		Admin.StartNewPlayer();
		Admin.ApplyAccount(Info);
		P::FUiState AdminState;
		P::Build(Admin, AdminState);
		Expect(P::Perform(Admin, AdminState, "go:profile").bOk && P::Perform(Admin, AdminState, "tab:6").bOk, "the admin is never held by the tutorial");
		P::FUiState Campaign;
		Campaign.Tab = P::TabCampaign;
		Campaign.Page = "chapter";
		const P::FPage CampaignPage = P::Build(Admin, Campaign);
		const P::FPageAction* Eight = FindAction(CampaignPage, "camp:start:8");
		Expect(Eight && Eight->bEnabled, "the admin sees every campaign level open");
	}

	// ==================================================================== accounts on pages
	void TestAccountPages()
	{
		FProfile Profile(60);
		FAccountInfo Info;
		Info.UserId = "10000042";
		Info.Username = "Nova";
		Info.Email = "nova@example.com";
		Info.bSignedIn = true;
		Profile.StartNewPlayer();
		Profile.ApplyAccount(Info);
		Profile.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		P::FUiState State;
		State.Tab = P::TabLobby;
		State.Page = "profile";
		P::FPage Page = P::Build(Profile, State);
		Expect(PageHas(Page, "Username: Nova") && PageHas(Page, "User ID: 10000042") && PageHas(Page, "Email: nova@example.com"), "the profile page shows username, id and e-mail");
		Expect(!PageHas(Page, "ADMIN"), "an ordinary account is not marked admin");
		Info.bAdmin = true;
		Info.Email = Config::AdminEmail;
		Profile.ApplyAccount(Info);
		Page = P::Build(Profile, State);
		Expect(PageHas(Page, "ADMIN") && PageHas(Page, Config::AdminEmail), "the admin account is marked on the profile page");

		State.Page = "settings";
		State.SettingsTab = 5;   // Account (sixth section since 2026-10-06)
		Page = P::Build(Profile, State);
		Expect(FindAction(Page, "acct:signout") != nullptr, "settings > Account has a sign-out tile");
		const P::FPerform SignOut = P::Perform(Profile, State, "acct:signout");
		Expect(SignOut.bOk && SignOut.bSignOut, "sign out is passed to the engine layer");

		FProfile Guest(61);
		FAccountInfo GuestInfo;
		GuestInfo.bGuest = true;
		GuestInfo.Username = "Guest";
		Guest.StartNewPlayer();
		Guest.ApplyAccount(GuestInfo);
		Guest.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		P::FUiState GuestState;
		GuestState.Tab = P::TabLobby;
		GuestState.Page = "profile";
		Expect(PageHas(P::Build(Guest, GuestState), "offline as a guest"), "a guest is told progress stays on this device");

		FProfile Dev(62);
		P::FUiState DevState;
		DevState.Tab = P::TabLobby;
		DevState.Page = "settings";
		DevState.SettingsTab = 5;   // Account (sixth section since 2026-10-06)
		Expect(FindAction(P::Build(Dev, DevState), "acct:signout") == nullptr, "the developer profile has no sign-out tile");

		// Reset profile through the page keeps an admin an admin.
		FProfile Admin(63);
		FAccountInfo AdminInfo;
		AdminInfo.UserId = "10000001";
		AdminInfo.Username = "Owner";
		AdminInfo.Email = Config::AdminEmail;
		AdminInfo.bSignedIn = true;
		AdminInfo.bAdmin = true;
		Admin.StartNewPlayer();
		Admin.ApplyAccount(AdminInfo);
		P::FUiState AdminState;
		AdminState.Tab = P::TabLobby;
		P::Perform(Admin, AdminState, "demo:reset");
		Expect(Admin.bUnlimited && OperatorsOwned(Admin) == 20, "Reset profile keeps the admin account's entitlements");
	}
}

int main()
{
	TestJson();
	TestNewPlayer();
	TestAdmin();
	TestRotation();
	TestSaveRoundTrip();
	TestSaveSafety();
	TestSaveAdmin();
	TestTutorialWalkthrough();
	TestTutorialResumeAndEdgeCases();
	TestAccountPages();
	std::printf("%d account checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
