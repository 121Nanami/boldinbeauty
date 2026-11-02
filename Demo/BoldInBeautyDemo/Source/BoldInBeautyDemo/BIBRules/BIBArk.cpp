#include "BIBArk.h"

#include <map>

#include "BIBTutorial.h"

namespace BIB
{
	namespace Ark
	{
		namespace
		{
			using Pages::TabCampaign;
			using Pages::TabLobby;
			using Pages::TabOperator;
			using Pages::TabRevolution;
			using Pages::TabShop;
			using Pages::TabSummon;
			using Pages::TabTeam;

			FStation MakeStation(const char* Id, const char* Name, const char* Area, const char* Blurb, const char* Icon, float R, float G, float B,
				std::vector<FCounter> Counters, bool bMerchant = false, bool bUnmarked = false)
			{
				FStation Station;
				Station.Id = Id;
				Station.Name = Name;
				Station.Area = Area;
				Station.Blurb = Blurb;
				Station.Icon = Icon;
				Station.Colour[0] = R;
				Station.Colour[1] = G;
				Station.Colour[2] = B;
				Station.Counters = std::move(Counters);
				Station.bMerchant = bMerchant;
				Station.bUnmarked = bUnmarked;
				return Station;
			}

			// One row of the menu tree. Kind / Station / Tab / Page / Action as in FNode; Parent "" = child of the root.
			struct FDef
			{
				const char* Id;
				const char* Parent;
				const char* Label;
				const char* Icon;
				ENodeKind Kind;
				const char* Station;
				int Tab;
				const char* Page;
				const char* Action;
				const char* Sub;
				// Tile on one of the old root pages that carries this node's badge / count ("" = none): "<tab>|<page>|<tile id>".
				const char* BadgeFrom;
				float R, G, B;
			};

			constexpr ENodeKind Br = ENodeKind::Branch;
			constexpr ENodeKind Tp = ENodeKind::Teleport;
			constexpr ENodeKind Pg = ENodeKind::Page;
			constexpr ENodeKind Ac = ENodeKind::Action;

			// The flow chart. Branch colours: places cyan, shops amber, operators pink, missions red, commander violet, system grey.
			const std::vector<FDef>& Defs()
			{
				static const std::vector<FDef> Rows = {
					{"the-ark", RootId, "The Ark", "tower", Br, "", 0, "", "", "every place you can walk to", "", 0.25f, 0.85f, 1.f},
					{"shops", RootId, "Shops", "nav_shop", Br, "", 0, "", "", "merchants and their stalls", "", 1.f, 0.72f, 0.2f},
					{"ops", RootId, "Operators", "nav_operator", Br, "", 0, "", "", "roster, squad, summon, gear", "", 1.f, 0.35f, 0.7f},
					{"missions", RootId, "Missions", "nav_campaign", Br, "", 0, "", "", "campaign, tower, arena, arcade", "", 1.f, 0.3f, 0.3f},
					{"me", RootId, "Commander", "profile", Br, "", 0, "", "", "mail, quests, events, profile", "", 0.65f, 0.45f, 1.f},
					{"system", RootId, "System", "settings", Br, "", 0, "", "", "settings and tools", "", 0.7f, 0.74f, 0.82f},

					// The Ark: areas, then places.
					{"operators", "the-ark", "Operators' Dome", "nav_team", Br, "", 0, "", "", "roll call and the operator factory", "", 0.35f, 0.8f, 1.f},
					{"town", "the-ark", "Town Square", "nav_lobby", Br, "", 0, "", "", "government, shops, arcade, task room", "", 0.25f, 0.95f, 0.85f},
					{"underworld", "the-ark", "Underworld", "s_alley", Br, "", 0, "", "", "down the ladders: the trade floor", "", 0.9f, 0.2f, 0.45f},
					{"ark:rollcall", "operators", "Roll Call", "nav_team", Tp, "rollcall", 0, "", "", "", "", 0.35f, 0.8f, 1.f},
					{"ark:factory", "operators", "Operator Generator Factory", "nav_summon", Tp, "factory", 0, "", "", "", "", 1.f, 0.82f, 0.25f},
					{"ark:government", "town", "Government Building", "notice", Tp, "government", 0, "", "", "", "2||go:notices", 1.f, 0.86f, 0.45f},
					{"ark:armoury", "town", "Armoury", "s_armoury", Tp, "armoury", 0, "", "", "", "", 1.f, 0.47f, 0.24f},
					{"merchant-row", "town", "Merchant Row", "nav_shop", Br, "", 0, "", "", "a stall for every merchant", "", 1.f, 0.72f, 0.2f},
					{"ark:arcade", "town", "Arcade", "arcade", Tp, "arcade", 0, "", "", "", "2||go:arcade", 1.f, 0.2f, 0.85f},
					{"ark:taskroom", "town", "Task Room", "stage", Tp, "taskroom", 0, "", "", "", "", 0.6f, 0.65f, 0.72f},
					{"training", "town", "Training Grounds", "tower", Br, "", 0, "", "", "Endless Tower and Arena", "", 1.f, 0.35f, 0.3f},
					{"ark:library", "town", "Library", "book", Tp, "library", 0, "", "", "", "", 0.55f, 0.9f, 0.5f},
					{"ark:ganghall", "town", "Gang Hall", "gang", Tp, "ganghall", 0, "", "", "", "2||go:gang", 0.95f, 0.3f, 0.3f},
					{"ark:resistance", "town", "Unmarked building", "b_shelter", Tp, "resistance", 0, "", "", "", "", 0.85f, 0.1f, 0.1f},
					{"ark:trade", "merchant-row", "Trade Centre", "s_trade", Tp, "trade", 0, "", "", "", "", 0.3f, 0.9f, 1.f},
					{"ark:premium", "merchant-row", "Premium Store", "s_premium", Tp, "premium", 0, "", "", "", "", 1.f, 0.55f, 0.9f},
					{"ark:arkshop", "merchant-row", "Ark Shop", "ark_medal", Tp, "arkshop", 0, "", "", "", "", 0.55f, 0.75f, 1.f},
					{"ark:eventshop", "merchant-row", "Event Shop", "event_token", Tp, "eventshop", 0, "", "", "", "", 1.f, 0.85f, 0.2f},
					{"ark:gangshop", "merchant-row", "Gang Shop", "gang_coin", Tp, "gangshop", 0, "", "", "", "", 0.95f, 0.45f, 0.3f},
					{"ark:tower", "training", "Endless Tower", "tower", Tp, "tower", 0, "", "", "", "", 0.5f, 0.6f, 1.f},
					{"ark:arena", "training", "Arena", "arena", Tp, "arena", 0, "", "", "", "4|ark|go:arena", 1.f, 0.35f, 0.3f},
					{"ark:alley", "underworld", "Back Alley", "s_alley", Tp, "alley", 0, "", "", "", "", 0.9f, 0.2f, 0.45f},

					// Shops: every merchant in one list (each one still teleports to its stall).
					{"shop:trade", "shops", "Trade Centre", "s_trade", Tp, "trade", 0, "", "", "", "", 0.3f, 0.9f, 1.f},
					{"shop:armoury", "shops", "Armoury", "s_armoury", Tp, "armoury", 0, "", "", "", "", 1.f, 0.47f, 0.24f},
					{"shop:premium", "shops", "Premium Store", "s_premium", Tp, "premium", 0, "", "", "", "", 1.f, 0.55f, 0.9f},
					{"shop:arkshop", "shops", "Ark Shop", "ark_medal", Tp, "arkshop", 0, "", "", "", "", 0.55f, 0.75f, 1.f},
					{"shop:eventshop", "shops", "Event Shop", "event_token", Tp, "eventshop", 0, "", "", "", "", 1.f, 0.85f, 0.2f},
					{"shop:gangshop", "shops", "Gang Shop", "gang_coin", Tp, "gangshop", 0, "", "", "", "", 0.95f, 0.45f, 0.3f},
					{"shop:alley", "shops", "Back Alley", "s_alley", Tp, "alley", 0, "", "", "", "", 0.9f, 0.2f, 0.45f},

					// Operators.
					{"op:rollcall", "ops", "Roster and squad", "nav_operator", Tp, "rollcall", 0, "", "", "Roll Call", "", 0.35f, 0.8f, 1.f},
					{"op:factory", "ops", "Summon", "nav_summon", Tp, "factory", 0, "", "", "Operator Generator Factory", "", 1.f, 0.82f, 0.25f},
					{"op:armoury", "ops", "Equipment", "equipment", Tp, "armoury", 0, "", "", "Armoury", "", 1.f, 0.47f, 0.24f},
					{"op:elements", "ops", "Elements", "el_magic", Pg, "", TabLobby, "elements", "", "", "2||go:elements", 0.75f, 0.5f, 1.f},

					// Missions.
					{"ms:campaign", "missions", "Campaign", "nav_campaign", Tp, "taskroom", 0, "", "", "Task Room: 13 chapters", "", 0.6f, 0.65f, 0.72f},
					{"ms:neotokyo", "missions", "Neo Tokyo", "stage", Ac, "", 0, "", "camp:map:NeoTokyo", "free roam with your team", "", 0.3f, 0.95f, 1.f},
					{"ms:tower", "missions", "Endless Tower", "tower", Tp, "tower", 0, "", "", "Training Grounds", "", 0.5f, 0.6f, 1.f},
					{"ms:arena", "missions", "Arena", "arena", Tp, "arena", 0, "", "", "Training Grounds", "4|ark|go:arena", 1.f, 0.35f, 0.3f},
					{"ms:substories", "missions", "Sub-stories", "book", Tp, "library", 0, "", "", "Library", "", 0.55f, 0.9f, 0.5f},
					{"ms:arcade", "missions", "Arcade", "arcade", Tp, "arcade", 0, "", "", "Town Square", "2||go:arcade", 1.f, 0.2f, 0.85f},
					{"ms:world", "missions", "Outer World", "b_shelter", Tp, "resistance", 0, "", "", "rebuild the surface", "", 0.85f, 0.1f, 0.1f},

					// Commander: personal things open straight away (no place to walk to).
					{"me:mail", "me", "Mail", "mail", Pg, "", TabLobby, "mail", "", "", "2||go:mail", 0.65f, 0.45f, 1.f},
					{"me:quests", "me", "Quests", "quest", Pg, "", TabLobby, "quests", "", "", "2||go:quests", 0.65f, 0.45f, 1.f},
					{"me:notices", "me", "Notices", "notice", Pg, "", TabLobby, "notices", "", "", "2||go:notices", 0.65f, 0.45f, 1.f},
					{"me:events", "me", "Events", "event", Pg, "", TabLobby, "events", "", "", "2||go:events", 0.65f, 0.45f, 1.f},
					{"me:rookie", "me", "Rookie week", "star", Pg, "", TabLobby, "rookie", "", "", "2||go:rookie", 0.65f, 0.45f, 1.f},
					{"me:return", "me", "Welcome back", "event", Pg, "", TabLobby, "return", "", "", "2||go:return", 0.65f, 0.45f, 1.f},
					{"me:pass", "me", "Season pass", "seasonpass", Pg, "", TabLobby, "pass", "", "", "2||go:pass", 0.65f, 0.45f, 1.f},
					{"me:inventory", "me", "Inventory", "inventory", Pg, "", TabLobby, "inventory", "", "", "2||go:inventory", 0.65f, 0.45f, 1.f},
					{"me:friends", "me", "Friends", "friends", Pg, "", TabLobby, "friends", "", "", "2||go:friends", 0.65f, 0.45f, 1.f},
					{"me:gang", "me", "Gang", "gang", Tp, "ganghall", 0, "", "", "Gang Hall", "2||go:gang", 0.95f, 0.3f, 0.3f},
					{"me:profile", "me", "Profile", "profile", Pg, "", TabLobby, "profile", "", "", "", 0.65f, 0.45f, 1.f},

					// System.
					{"sys:settings", "system", "Settings", "settings", Pg, "", TabLobby, "settings", "", "audio, graphics, controls", "", 0.7f, 0.74f, 0.82f},
					{"sys:keys", "system", "Key bindings", "keys", Pg, "", TabLobby, "keys", "", "", "", 0.7f, 0.74f, 0.82f},
					{"sys:admin", "system", "Admin desk", "admin", Pg, "", TabLobby, "admin", "", "send mail, post notices", "2||go:admin", 1.f, 0.6f, 0.2f},
					{"sys:tester", "system", "Tester panel", "star", Pg, "", TabLobby, "tester", "", "currencies, unlocks, levels", "2||go:tester", 1.f, 0.6f, 0.2f},
					{"sys:signout", "system", "Sign out", "back", Ac, "", 0, "", "acct:signout", "", "", 0.9f, 0.35f, 0.35f},
				};
				return Rows;
			}

			// Nodes that only exist when their tile exists on the old root page (admin desk, tester panel, return / rookie events).
			bool Conditional(const std::string& Id)
			{
				return Id == "sys:admin" || Id == "sys:tester" || Id == "me:rookie" || Id == "me:return";
			}

			const FDef* FindDef(const std::string& Id)
			{
				for (const FDef& Def : Defs())
				{
					if (Id == Def.Id)
					{
						return &Def;
					}
				}
				return nullptr;
			}

			// Tiles of the old root pages (badges, counts, conditional entries), built once per menu refresh.
			struct FTiles
			{
				std::map<std::string, Pages::FPageAction> ByKey;   // "<tab>|<page>|<tile id>"
			};

			FTiles CollectTiles(FProfile& Profile, const Pages::FUiState& State)
			{
				FTiles Tiles;
				struct FSource
				{
					int Tab;
					const char* Page;
				};
				static const FSource Sources[] = {{TabLobby, ""}, {TabShop, ""}, {TabRevolution, ""}, {TabRevolution, "ark"}};
				for (const FSource& Source : Sources)
				{
					Pages::FUiState Copy = State;
					Copy.Tab = Source.Tab;
					Copy.Page = Source.Page;
					Copy.SummonOps.clear();
					const Pages::FPage Page = Pages::Build(Profile, Copy);
					for (const Pages::FPageAction& Tile : Page.Grid)
					{
						Tiles.ByKey[std::to_string(Source.Tab) + "|" + Source.Page + "|" + Tile.Id] = Tile;
					}
				}
				return Tiles;
			}

			const Pages::FPageAction* BadgeTile(const FTiles& Tiles, const FDef& Def)
			{
				std::string Key = Def.BadgeFrom;
				if (Key.empty())
				{
					return nullptr;
				}
				// "2||go:notices" in the table means: the notices tile of the Lobby root (written as tab 2, page "").
				if (Key == "2||go:notices")
				{
					Key = std::to_string(TabLobby) + "||go:notices";
				}
				const auto Found = Tiles.ByKey.find(Key);
				return Found == Tiles.ByKey.end() ? nullptr : &Found->second;
			}

			FNode MakeNode(const FDef& Def)
			{
				FNode Node;
				Node.Id = Def.Id;
				Node.Label = Def.Label;
				Node.Sub = Def.Sub;
				Node.Icon = Def.Icon;
				Node.Kind = Def.Kind;
				Node.Station = Def.Station;
				Node.Tab = Def.Tab;
				Node.Page = Def.Page;
				Node.Action = Def.Action;
				Node.Colour[0] = Def.R;
				Node.Colour[1] = Def.G;
				Node.Colour[2] = Def.B;
				if (Node.Kind == ENodeKind::Teleport && Node.Sub.empty())
				{
					if (const FStation* Station = FindStation(Node.Station))
					{
						Node.Sub = Station->Blurb;
					}
				}
				return Node;
			}

			void Decorate(FNode& Node, const FDef& Def, const FTiles& Tiles, const std::vector<std::string>& Tutorial)
			{
				if (const Pages::FPageAction* Tile = BadgeTile(Tiles, Def))
				{
					Node.bBadge = Tile->bBadge;
					if (Node.Kind != ENodeKind::Teleport && !Tile->Sub.empty())
					{
						Node.Sub = Tile->Sub;
					}
				}
				for (const std::string& Step : Tutorial)
				{
					if (Step == Node.Id)
					{
						Node.bHighlight = true;
					}
				}
			}

			bool Visible(const FDef& Def, const FTiles& Tiles)
			{
				if (!Conditional(Def.Id))
				{
					return true;
				}
				return BadgeTile(Tiles, Def) != nullptr;
			}

			// A branch carries a badge when anything under it does.
			bool AnyBadge(const std::string& Id, const FTiles& Tiles)
			{
				for (const FDef& Def : Defs())
				{
					if (Id != Def.Parent || !Visible(Def, Tiles))
					{
						continue;
					}
					if (Def.Kind == ENodeKind::Branch ? AnyBadge(Def.Id, Tiles) : (BadgeTile(Tiles, Def) && BadgeTile(Tiles, Def)->bBadge))
					{
						return true;
					}
				}
				return false;
			}
		}

		// ======================================================================= stations
		const std::vector<FStation>& Stations()
		{
			static const std::vector<FStation> List = {
				MakeStation("rollcall", "Roll Call", "operators", "operators, squad and elements", "nav_team", 0.35f, 0.8f, 1.f,
					{{"Operators", TabOperator, "", "nav_operator"}, {"Squad", TabTeam, "", "nav_team"}, {"Elements", TabLobby, "elements", "el_magic"}}),
				MakeStation("factory", "Operator Generator Factory", "operators", "summon operators from the banners", "nav_summon", 1.f, 0.82f, 0.25f,
					{{"Banners", TabSummon, "", "nav_summon"}}),
				MakeStation("government", "Government Building", "town", "notices, quests, events, season pass", "notice", 1.f, 0.86f, 0.45f,
					{{"Notices", TabLobby, "notices", "notice"}, {"Quests", TabLobby, "quests", "quest"}, {"Events", TabLobby, "events", "event"},
					 {"Season pass", TabLobby, "pass", "seasonpass"}}),
				MakeStation("armoury", "Armoury", "town", "armour, blades and equipping your operators", "s_armoury", 1.f, 0.47f, 0.24f,
					{{"Armour shop", TabShop, "armoury", "s_armoury"}, {"Equip", TabOperator, "op:equip", "equipment"}}, true),
				MakeStation("arcade", "Arcade", "town", "Boss Rush, Rhythm, Ark Rally and more", "arcade", 1.f, 0.2f, 0.85f,
					{{"Arcade", TabLobby, "arcade", "arcade"}}),
				MakeStation("taskroom", "Task Room", "town", "chutes to the surface: the campaign", "stage", 0.6f, 0.65f, 0.72f,
					{{"Campaign", TabCampaign, "", "nav_campaign"}}, false, true),
				MakeStation("tower", "Endless Tower", "town", "3-4 star and 5-6 star towers", "tower", 0.5f, 0.6f, 1.f,
					{{"Endless Tower", TabRevolution, "tower", "tower"}}),
				MakeStation("arena", "Arena", "town", "attack and defence teams, trophies", "arena", 1.f, 0.35f, 0.3f,
					{{"Arena", TabRevolution, "arena", "arena"}}),
				MakeStation("library", "Library", "town", "sub-stories", "book", 0.55f, 0.9f, 0.5f,
					{{"Sub-stories", TabRevolution, "substories", "book"}}),
				MakeStation("ganghall", "Gang Hall", "town", "your gang, members and donations", "gang", 0.95f, 0.3f, 0.3f,
					{{"Gang", TabLobby, "gang", "gang"}}),
				MakeStation("resistance", "Unmarked building", "town", "a small red stripe: rebuild the Outer World", "b_shelter", 0.85f, 0.1f, 0.1f,
					{{"Outer World", TabRevolution, "world", "b_shelter"}}, false, true),
				MakeStation("trade", "Trade Centre", "town", "exchange anything for anything", "s_trade", 0.3f, 0.9f, 1.f,
					{{"Trade Centre", TabShop, "trade", "s_trade"}}, true),
				MakeStation("premium", "Premium Store", "town", "in-app purchases (simulated)", "s_premium", 1.f, 0.55f, 0.9f,
					{{"Premium store", TabShop, "premium", "s_premium"}}, true),
				MakeStation("arkshop", "Ark Shop", "town", "Ark medals from the arena and tower", "ark_medal", 0.55f, 0.75f, 1.f,
					{{"Ark shop", TabRevolution, "ark-shop", "ark_medal"}}, true),
				MakeStation("eventshop", "Event Shop", "town", "Event tokens from the weekly operation", "event_token", 1.f, 0.85f, 0.2f,
					{{"Event shop", TabLobby, "event-shop", "event_token"}}, true),
				MakeStation("gangshop", "Gang Shop", "town", "Gang coins from the gang season", "gang_coin", 0.95f, 0.45f, 0.3f,
					{{"Gang shop", TabLobby, "gang-shop", "gang_coin"}}, true),
				MakeStation("alley", "Back Alley", "underworld", "high-end goods for Diamonds", "s_alley", 0.9f, 0.2f, 0.45f,
					{{"Back Alley", TabShop, "alley", "s_alley"}}, true),
			};
			return List;
		}

		const FStation* FindStation(const std::string& Id)
		{
			for (const FStation& Station : Stations())
			{
				if (Station.Id == Id)
				{
					return &Station;
				}
			}
			return nullptr;
		}

		Pages::FPerform OpenCounter(FProfile& Profile, Pages::FUiState& State, const FCounter& Counter)
		{
			Pages::FPerform Done = Pages::Perform(Profile, State, "tab:" + std::to_string(Counter.Tab));
			if (!Done.bOk || Counter.Page.empty())
			{
				return Done;
			}
			if (Counter.Page.find(':') == std::string::npos)
			{
				return Pages::Perform(Profile, State, "go:" + Counter.Page);
			}
			State.Page = Counter.Page;
			State.ListOffset = 0;
			return Done;
		}

		std::string StationForTab(int Tab)
		{
			return StationForPage(Tab, "");
		}

		std::string StationForPage(int Tab, const std::string& Page)
		{
			for (const FStation& Station : Stations())
			{
				for (const FCounter& Counter : Station.Counters)
				{
					if (Counter.Tab == Tab && Counter.Page == Page)
					{
						return Station.Id;
					}
				}
			}
			return "";
		}

		// ======================================================================= menu
		FNode Node(FProfile& Profile, const Pages::FUiState& State, const std::string& Id)
		{
			const FDef* Def = FindDef(Id);
			if (!Def)
			{
				FNode Root;
				Root.Id = RootId;
				Root.Label = "Menu";
				Root.Icon = "nav_lobby";
				return Root;
			}
			const FTiles Tiles = CollectTiles(Profile, State);
			FNode Out = MakeNode(*Def);
			Decorate(Out, *Def, Tiles, TutorialPath(Profile));
			if (Out.Kind == ENodeKind::Branch)
			{
				Out.bBadge = AnyBadge(Out.Id, Tiles);
			}
			return Out;
		}

		std::vector<FNode> Children(FProfile& Profile, const Pages::FUiState& State, const std::string& Id)
		{
			const std::string Parent = Id.empty() ? RootId : Id;
			const FTiles Tiles = CollectTiles(Profile, State);
			const std::vector<std::string> Tutorial = TutorialPath(Profile);
			std::vector<FNode> Out;
			for (const FDef& Def : Defs())
			{
				if (Parent != Def.Parent || !Visible(Def, Tiles))
				{
					continue;
				}
				FNode Child = MakeNode(Def);
				Decorate(Child, Def, Tiles, Tutorial);
				if (Child.Kind == ENodeKind::Branch)
				{
					Child.bBadge = AnyBadge(Child.Id, Tiles);
				}
				Out.push_back(Child);
			}
			return Out;
		}

		std::vector<std::string> PathTo(const std::string& Id)
		{
			std::vector<std::string> Reversed;
			std::string Current = Id;
			for (int Guard = 0; Guard < 16; ++Guard)
			{
				if (Current == RootId)
				{
					Reversed.push_back(RootId);
					return std::vector<std::string>(Reversed.rbegin(), Reversed.rend());
				}
				const FDef* Def = FindDef(Current);
				if (!Def)
				{
					return {};
				}
				Reversed.push_back(Current);
				Current = Def->Parent;
			}
			return {};
		}

		std::vector<std::string> AllNodeIds()
		{
			std::vector<std::string> Ids = {RootId};
			for (const FDef& Def : Defs())
			{
				Ids.push_back(Def.Id);
			}
			return Ids;
		}

		std::vector<std::string> TutorialPath(const FProfile& Profile)
		{
			if (!Tutorial::Active(Profile))
			{
				return {};
			}
			const Tutorial::FCoach Coach = Tutorial::Coach(Profile);
			if (Coach.TargetTab < 0)
			{
				return {};
			}
			const std::string Station = StationForTab(Coach.TargetTab);
			if (Station.empty())
			{
				return {};
			}
			// The place under The Ark (the canonical path; the shortcuts in Shops / Operators / Missions are not highlighted).
			return PathTo("ark:" + Station);
		}

		std::string TutorialLine(const FProfile& Profile)
		{
			if (!Tutorial::Active(Profile))
			{
				return "";
			}
			const Tutorial::FCoach Coach = Tutorial::Coach(Profile);
			std::string Line = "Tutorial " + std::to_string(Coach.Number) + "/" + std::to_string(Tutorial::StepCount) + ": " + Coach.Text;
			if (Coach.TargetTab >= 0)
			{
				const FStation* Station = FindStation(StationForTab(Coach.TargetTab));
				if (Station)
				{
					// The bottom tabs are gone: say where the page lives now.
					std::string Counter;
					for (const FCounter& Entry : Station->Counters)
					{
						if (Entry.Tab == Coach.TargetTab && Entry.Page.empty())
						{
							Counter = Entry.Label;
						}
					}
					Line += "  ->  Menu > The Ark > " + std::string(Station->Area == "operators" ? "Operators' Dome" : "Town Square") + " > " + Station->Name +
						", then press F" + (Station->Counters.size() > 1 ? " and pick " + Counter : std::string()) + ".";
				}
			}
			return Line;
		}
	}
}
