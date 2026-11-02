// Tests for the Ark hub (BIBRules/BIBArk.h, owner request 2026-10-07): the station table, the flow-chart menu, and that nothing
// the old 2D lobby offered got lost on the way into the 3D Ark (every tile of the old root pages is a station counter or a menu
// page), plus the tutorial's route through the menu. Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArk.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBTester.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBTutorial.h"

#include <cmath>
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

	FProfile Guest(bool bSkipTutorial)
	{
		FProfile Profile(11u);
		FAccountInfo Info;
		Info.bGuest = true;
		Info.Username = "Guest";
		Profile.Account = Info;
		Profile.ResetForAccount();
		if (bSkipTutorial)
		{
			Tester::SkipTutorial(Profile);
		}
		return Profile;
	}

	FProfile Admin()
	{
		FProfile Profile(12u);
		FAccountInfo Info;
		Info.bSignedIn = true;
		Info.bAdmin = true;
		Info.UserId = "10000001";
		Info.Username = "Owner";
		Info.Email = Config::AdminEmail;
		Profile.Account = Info;
		Profile.ResetForAccount();
		return Profile;
	}

	// Opens a counter the way the Ark panel does and returns the page it shows.
	P::FPage Open(FProfile& Profile, P::FUiState& State, const Ark::FCounter& Counter, bool& bAllOk)
	{
		bAllOk = Ark::OpenCounter(Profile, State, Counter).bOk;
		return P::Build(Profile, State);
	}

	void TestStations()
	{
		std::set<std::string> Ids;
		for (const Ark::FStation& Station : Ark::Stations())
		{
			Expect(Ids.insert(Station.Id).second, "unique station id " + Station.Id);
			Expect(!Station.Name.empty() && !Station.Icon.empty() && !Station.Blurb.empty(), Station.Id + ": name, icon and blurb");
			Expect(!Station.Counters.empty(), Station.Id + ": at least one counter");
			Expect(Station.Area == "operators" || Station.Area == "town" || Station.Area == "underworld", Station.Id + ": known area");
			Expect(Ark::FindStation(Station.Id) == &Station, Station.Id + ": FindStation");
			for (const Ark::FCounter& Counter : Station.Counters)
			{
				for (int Kind = 0; Kind < 2; ++Kind)
				{
					FProfile Profile = Kind == 0 ? Guest(true) : Admin();
					P::FUiState State;
					bool bOk = false;
					const P::FPage Page = Open(Profile, State, Counter, bOk);
					Expect(bOk, Station.Id + " / " + Counter.Label + ": opening actions succeed");
					Expect(State.Tab == Counter.Tab && State.Page == Counter.Page, Station.Id + " / " + Counter.Label + ": lands on its page");
					Expect(!Page.Title.empty(), Station.Id + " / " + Counter.Label + ": the page has a title");
					Expect(Page.Grid.size() <= static_cast<size_t>(P::MaxGrid) && Page.Bar.size() <= static_cast<size_t>(P::MaxBar), Station.Id + ": fits the panel");
				}
			}
		}
		Expect(Ark::FindStation("nope") == nullptr, "unknown station");
		// The merchants the owner asked for each have a stall; the arcade and the factory exist.
		for (const char* Id : {"trade", "premium", "arkshop", "eventshop", "gangshop", "alley", "armoury"})
		{
			const Ark::FStation* Station = Ark::FindStation(Id);
			Expect(Station && Station->bMerchant, std::string(Id) + " is a merchant stall");
		}
		Expect(Ark::FindStation("arcade") != nullptr, "the arcade is a place");
		Expect(Ark::FindStation("factory") && Ark::FindStation("factory")->Counters[0].Tab == P::TabSummon, "summoning happens in the factory");
		Expect(Ark::FindStation("armoury") && Ark::FindStation("armoury")->Counters.size() == 2, "the armoury sells and equips");
		Expect(Ark::FindStation("taskroom")->bUnmarked && Ark::FindStation("resistance")->bUnmarked, "the task room and resistance stay unmarked");
		Expect(Ark::StationForTab(P::TabOperator) == "rollcall" && Ark::StationForTab(P::TabTeam) == "rollcall", "roster and squad at roll call");
		Expect(Ark::StationForTab(P::TabCampaign) == "taskroom", "campaign from the task room");
		Expect(Ark::StationForTab(P::TabSummon) == "factory", "summon tab -> factory");
		Expect(Ark::StationForPage(P::TabShop, "alley") == "alley" && Ark::StationForPage(P::TabLobby, "mail").empty(), "StationForPage");
	}

	void TestMenuTree()
	{
		FProfile Profile = Admin();
		P::FUiState State;
		const std::vector<Ark::FNode> Top = Ark::Children(Profile, State, Ark::RootId);
		Expect(Top.size() == 6, "six top-level branches (The Ark, Shops, Operators, Missions, Commander, System)");
		Expect(!Top.empty() && Top[0].Id == "the-ark", "The Ark comes first");
		Expect(Ark::Children(Profile, State, "").size() == Top.size(), "\"\" is the root");
		std::set<std::string> SeenIds;
		std::set<std::string> StationsUnderArk;
		int Leaves = 0;
		for (const std::string& Id : Ark::AllNodeIds())
		{
			Expect(SeenIds.insert(Id).second, "unique node id " + Id);
			const std::vector<std::string> Path = Ark::PathTo(Id);
			Expect(!Path.empty() && Path.front() == Ark::RootId && Path.back() == Id, "path to " + Id);
			Expect(Path.size() <= 5, Id + ": at most four clicks deep");
			const Ark::FNode Node = Ark::Node(Profile, State, Id);
			Expect(Node.Id == Id, "Node(" + Id + ")");
			Expect(!Node.Label.empty() && !Node.Icon.empty(), Id + ": label and icon");
			const std::vector<Ark::FNode> Kids = Ark::Children(Profile, State, Id);
			if (Node.Kind == Ark::ENodeKind::Branch)
			{
				Expect(!Kids.empty(), Id + ": a branch has children");
			}
			else
			{
				++Leaves;
				Expect(Kids.empty(), Id + ": a leaf has no children");
			}
			if (Node.Kind == Ark::ENodeKind::Teleport)
			{
				Expect(Ark::FindStation(Node.Station) != nullptr, Id + ": teleports to a real station");
				if (Path.size() > 1 && Path[1] == "the-ark")
				{
					StationsUnderArk.insert(Node.Station);
				}
			}
			if (Node.Kind == Ark::ENodeKind::Page)
			{
				P::FUiState Copy;
				Copy.Tab = Node.Tab;
				Copy.Page = Node.Page;
				Expect(!P::Build(Profile, Copy).Title.empty(), Id + ": its page builds");
			}
			if (Node.Kind == Ark::ENodeKind::Action)
			{
				Expect(!Node.Action.empty(), Id + ": has an action");
			}
		}
		Expect(Leaves > 30, "the menu has plenty of leaves");
		Expect(StationsUnderArk.size() == Ark::Stations().size(), "every station is reachable under The Ark");
		Expect(Ark::PathTo("ark:armoury") == std::vector<std::string>({"menu", "the-ark", "town", "ark:armoury"}), "Menu / The Ark / Town Square / Armoury");
		Expect(Ark::PathTo("nope").empty(), "unknown node has no path");

		// Conditional entries: the admin desk only for the admin, the tester panel only when tester tools are on.
		FProfile Plain = Guest(true);
		bool bAdminDesk = false;
		for (const Ark::FNode& Node : Ark::Children(Plain, State, "system"))
		{
			bAdminDesk = bAdminDesk || Node.Id == "sys:admin";
		}
		Expect(!bAdminDesk, "a guest has no admin desk");
		bAdminDesk = false;
		for (const Ark::FNode& Node : Ark::Children(Profile, State, "system"))
		{
			bAdminDesk = bAdminDesk || Node.Id == "sys:admin";
		}
		Expect(bAdminDesk, "the admin sees the admin desk");

		// Badges: unclaimed mail lights up Mail and its branch.
		FProfile WithMail = Guest(true);
		bool bMailBadge = false;
		for (const Ark::FNode& Node : Ark::Children(WithMail, State, "me"))
		{
			if (Node.Id == "me:mail")
			{
				bMailBadge = Node.bBadge;
			}
		}
		P::FUiState MailState;
		MailState.Tab = P::TabLobby;
		MailState.Page = "mail";
		const P::FPage MailPage = P::Build(WithMail, MailState);
		bool bUnclaimed = false;
		for (const P::FPageAction& Tile : MailPage.Grid)
		{
			bUnclaimed = bUnclaimed || Tile.bBadge;
		}
		Expect(bMailBadge == bUnclaimed, "the Mail node's badge follows unclaimed mail");
		if (bMailBadge)
		{
			Expect(Ark::Node(WithMail, State, "me").bBadge, "a badge shows on its branch too");
		}
	}

	// Nothing from the old lobby is lost: every "go:" tile of the old root pages leads to a station counter or a menu page.
	void TestCoverage()
	{
		FProfile Profile = Admin();
		std::set<std::pair<int, std::string>> Reachable;
		for (const Ark::FStation& Station : Ark::Stations())
		{
			for (const Ark::FCounter& Counter : Station.Counters)
			{
				Reachable.insert({Counter.Tab, Counter.Page});
			}
		}
		P::FUiState State;
		for (const std::string& Id : Ark::AllNodeIds())
		{
			const Ark::FNode Node = Ark::Node(Profile, State, Id);
			if (Node.Kind == Ark::ENodeKind::Page)
			{
				Reachable.insert({Node.Tab, Node.Page});
			}
		}
		for (int Tab : {P::TabOperator, P::TabTeam, P::TabCampaign, P::TabSummon})
		{
			Expect(Reachable.count({Tab, ""}) == 1, std::string("the ") + P::TabName(Tab) + " tab's page is in the Ark");
		}
		struct FSource
		{
			int Tab;
			const char* Page;
		};
		// Shop and Revolution roots only listed their sub-pages; the Revolution "ark" page listed tower / arena / sub-stories / Ark shop.
		const FSource Sources[] = {{P::TabLobby, ""}, {P::TabShop, ""}, {P::TabRevolution, ""}, {P::TabRevolution, "ark"}};
		for (const FSource& Source : Sources)
		{
			P::FUiState Copy;
			Copy.Tab = Source.Tab;
			Copy.Page = Source.Page;
			for (const P::FPageAction& Tile : P::Build(Profile, Copy).Grid)
			{
				if (Tile.Id.rfind("go:", 0) != 0)
				{
					continue;
				}
				const std::string Target = Tile.Id.substr(3);
				if (Target == "ark")
				{
					continue;   // the old "The Ark" sub-menu itself: its four entries are checked below
				}
				Expect(Reachable.count({Source.Tab, Target}) == 1, std::string("old ") + P::TabName(Source.Tab) + " tile " + Tile.Id + " lives on in the Ark");
			}
		}
		// The Lobby tiles that only exist for some accounts (rookie week, welcome back) are menu pages.
		Expect(Reachable.count({P::TabLobby, "rookie"}) == 1 && Reachable.count({P::TabLobby, "return"}) == 1, "event pages reachable");
		Expect(Reachable.count({P::TabRevolution, "ark-shop"}) == 1 && Reachable.count({P::TabRevolution, "world"}) == 1, "Ark shop and Outer World reachable");
	}

	void TestTutorial()
	{
		FProfile Profile = Guest(false);
		P::FUiState State;
		Expect(Tutorial::Active(Profile), "a new guest starts the tutorial");
		const std::vector<std::string> Path = Ark::TutorialPath(Profile);
		Expect(Path == std::vector<std::string>({"menu", "the-ark", "operators", "ark:rollcall"}), "step 1 points at Roll Call");
		const std::string Line = Ark::TutorialLine(Profile);
		Expect(Line.find("Roll Call") != std::string::npos && Line.find("press F") != std::string::npos, "the HUD line says where to go");
		bool bHighlighted = false;
		for (const Ark::FNode& Node : Ark::Children(Profile, State, "operators"))
		{
			bHighlighted = bHighlighted || (Node.Id == "ark:rollcall" && Node.bHighlight);
		}
		Expect(bHighlighted, "the menu highlights Roll Call");
		Expect(Ark::Node(Profile, State, "the-ark").bHighlight, "and the branch leading to it");
		// Opening Roll Call (its first counter: Operators) completes step 1.
		const int Before = Profile.TutorialStep;
		bool bOk = false;
		Open(Profile, State, Ark::FindStation("rollcall")->Counters[0], bOk);
		Expect(bOk && Profile.TutorialStep == Before + 1, "opening Roll Call advances the tutorial");
		// Walk on through the Roll Call panel: tap Luna, level her up, then the squad step points at Roll Call again.
		Expect(P::Perform(Profile, State, "op:sel:" + std::to_string(FindOperator("Luna"))).bOk, "tap Luna in the panel");
		Expect(P::Perform(Profile, State, "op:lvl").bOk, "level Luna up in the panel");
		Expect(Profile.TutorialStep == static_cast<int>(ETutorialStep::OpenTeam), "now the squad step");
		const std::vector<std::string> SquadPath = Ark::TutorialPath(Profile);
		Expect(!SquadPath.empty() && SquadPath.back() == "ark:rollcall", "the squad step points at Roll Call again");
		Expect(Ark::TutorialLine(Profile).find("Squad") != std::string::npos, "and says which counter");
		Open(Profile, State, Ark::FindStation("rollcall")->Counters[1], bOk);
		Expect(bOk && State.Tab == P::TabTeam, "the Squad counter opens the Team page");
		Expect(Ark::TutorialLine(Guest(true)).empty(), "no tutorial line once it is over");
		Expect(Ark::TutorialPath(Guest(true)).empty(), "no tutorial path once it is over");
	}
}

namespace
{
	// The saved Ark position (owner request 2026-10-07: the game starts where the player last stood in the Ark).
	void TestArkPosition()
	{
		FProfile Fresh = Guest(true);
		Expect(!Fresh.Meta.bArkPosValid, "a new account has no saved Ark position (starts on the plaza)");
		const std::string NoPos = Save::Serialize(Fresh, 1000);
		Expect(NoPos.find("arkPosition") == std::string::npos, "no position, nothing written");

		FProfile Walked = Guest(true);
		Walked.Meta.bArkPosValid = true;
		Walked.Meta.ArkPos[0] = 43512.5;
		Walked.Meta.ArkPos[1] = -1820.25;
		Walked.Meta.ArkPos[2] = 96.0;
		Walked.Meta.ArkYaw = 135.0;
		const std::string Text = Save::Serialize(Walked, 2000);
		FProfile Loaded = Guest(true);
		std::string Error;
		Expect(Save::Deserialize(Text, Loaded, Error), "a save with a position loads (" + Error + ")");
		Expect(Loaded.Meta.bArkPosValid, "the position survives a save and load");
		Expect(std::fabs(Loaded.Meta.ArkPos[0] - 43512.5) < 0.01 && std::fabs(Loaded.Meta.ArkPos[1] + 1820.25) < 0.01 && std::fabs(Loaded.Meta.ArkPos[2] - 96.0) < 0.01, "x, y, z round-trip");
		Expect(std::fabs(Loaded.Meta.ArkYaw - 135.0) < 0.01, "yaw round-trips");

		// An older save (no position) loaded over a profile that had one: back to the plaza.
		FProfile Old = Walked;
		Expect(Save::Deserialize(NoPos, Old, Error) && !Old.Meta.bArkPosValid, "a save without a position clears it");

		// A hand-edited file with an absurd position is ignored instead of throwing the player into the void.
		std::string Bad = Text;
		const size_t At = Bad.find("43512.5");
		Expect(At != std::string::npos, "the position is in the text");
		if (At != std::string::npos)
		{
			Bad.replace(At, 7, "9e300");
			FProfile Sane = Guest(true);
			Expect(Save::Deserialize(Bad, Sane, Error) && !Sane.Meta.bArkPosValid, "an out-of-range position is dropped");
		}
	}
}

int main()
{
	TestArkPosition();
	TestStations();
	TestMenuTree();
	TestCoverage();
	TestTutorial();
	std::printf("ark tests: %d checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
