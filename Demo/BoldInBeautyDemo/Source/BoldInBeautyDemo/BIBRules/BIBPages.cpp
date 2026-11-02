// Boldinbeauty demo — the hub UI as data (see BIBPages.h). Layout reference: Goddess of Victory: Nikke (top resource
// bar, big operator portrait with a lower-third name plate, seven bottom tabs). Every number is a PLACEHOLDER.
#include "BIBPages.h"

#include "BIBCampaign.h"
#include "BIBConfig.h"
#include "BIBArena.h"
#include "BIBElement.h"
#include "BIBEvents.h"
#include "BIBInventory.h"
#include "BIBLiveOps.h"
#include "BIBMetaOps.h"
#include "BIBPagesExtra.h"
#include "BIBPagesGacha.h"
#include "BIBTester.h"
#include "BIBTutorial.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace BIB
{
	namespace Pages
	{
		namespace
		{
			using namespace Meta;

			// ------------------------------------------------------------ helpers
			std::vector<std::string> Split(const std::string& Text, char Separator)
			{
				std::vector<std::string> Parts;
				std::string Current;
				for (char Character : Text)
				{
					if (Character == Separator)
					{
						Parts.push_back(Current);
						Current.clear();
					}
					else
					{
						Current += Character;
					}
				}
				Parts.push_back(Current);
				return Parts;
			}

			int ToInt(const std::vector<std::string>& Parts, size_t Index, int Fallback = 0)
			{
				return Index < Parts.size() && !Parts[Index].empty() ? std::atoi(Parts[Index].c_str()) : Fallback;
			}

			std::string Fixed(double Value, int Decimals)
			{
				char Buffer[48];
				std::snprintf(Buffer, sizeof(Buffer), "%.*f", Decimals, Value);
				return Buffer;
			}

			std::string StarsText(int Stars)
			{
				return StarsLabel(Stars);
			}

			std::string OpName(int RosterIndex)
			{
				return RosterIndex >= 0 && RosterIndex < static_cast<int>(Roster().size()) ? Roster()[static_cast<size_t>(RosterIndex)].Name : "-";
			}

			std::string BonusText(const FEquipment& Item)
			{
				return "+" + Fixed(Item.Percent + Item.UpgradeLevel * Config::EquipUpgradePercent, 1) + "% "
					+ (Item.Bonus == EBonus::Damage ? "damage" : "defence");
			}

			std::string CostText(int Gold, int Bytes)
			{
				std::string Text;
				if (Gold > 0)
				{
					Text += Num(Gold) + " Gold";
				}
				if (Bytes > 0)
				{
					Text += (Text.empty() ? "" : " + ") + Num(Bytes) + " Bytes";
				}
				return Text;
			}

			// Operators sorted by rarity (highest first), then roster order.
			std::vector<int> SortedOperators()
			{
				std::vector<int> Order;
				for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
				{
					Order.push_back(Index);
				}
				std::stable_sort(Order.begin(), Order.end(), [](int A, int B)
				{
					return Roster()[static_cast<size_t>(A)].Stars > Roster()[static_cast<size_t>(B)].Stars;
				});
				return Order;
			}

			class FBuilder
			{
			public:
				FBuilder(FPage& InPage) : Page(InPage) {}

				void Line(const std::string& Text) { Page.Lines.push_back(Text); }

				void Tile(const std::string& Id, const std::string& Label, const std::string& Icon = "", const std::string& Sub = "",
					bool bEnabled = true, bool bHighlight = false, bool bBadge = false)
				{
					if (static_cast<int>(Page.Grid.size()) < MaxGrid)
					{
						Page.Grid.push_back({Id, Label, Sub, Icon, bEnabled, bHighlight, bBadge});
					}
				}

				void Button(const std::string& Id, const std::string& Label, const std::string& Sub = "", bool bEnabled = true,
					bool bHighlight = false, const std::string& Icon = "")
				{
					if (static_cast<int>(Page.Bar.size()) < MaxBar)
					{
						Page.Bar.push_back({Id, Label, Sub, Icon, bEnabled, bHighlight, false});
					}
				}

				void Portrait(int RosterIndex)
				{
					if (RosterIndex >= 0 && RosterIndex < static_cast<int>(Roster().size()))
					{
						const FRosterEntry& Entry = Roster()[static_cast<size_t>(RosterIndex)];
						Page.Portrait = LowerName(RosterIndex);
						Page.PortraitStars = StarsText(Entry.Stars);
						Page.PortraitName = Entry.Name;
					}
				}

				FPage& Page;
			};

			std::string ParentPage(const std::string& Page)
			{
				bool bFound = false;
				const std::string New = Gacha::ParentPage(Page, bFound);
				if (bFound) return New;
				const std::string Extra = Extra::ParentPage(Page, bFound);
				if (bFound) return Extra;
				if (Page == "op:equip") return "op";
				if (Page == "arcade:game") return "arcade";
				if (Page == "tower" || Page == "arena" || Page == "substories") return "ark";
				if (Page == "sub") return "substories";
				if (Page == "tester-op") return "tester-ops";
				if (Page.rfind("tester-", 0) == 0) return "tester";
				if (Page == "tester") return "";
				return "";
			}

			// Stat of the per-team operator option cycle names.
			const char* StageHint(int Level)
			{
				return Level >= CampaignLevels ? "BOSS" : "";
			}
		}

		// ======================================================================= public helpers
		const char* TabName(int Tab)
		{
			static const char* Names[TabCount] = {"Operator", "Team", "Lobby", "Shop", "Revolution", "Campaign", "Summon"};
			return Tab >= 0 && Tab < TabCount ? Names[Tab] : "?";
		}

		const char* TabIcon(int Tab)
		{
			static const char* Icons[TabCount] = {"nav_operator", "nav_team", "nav_lobby", "nav_shop", "nav_revolution", "nav_campaign", "nav_summon"};
			return Tab >= 0 && Tab < TabCount ? Icons[Tab] : "";
		}

		std::string LowerName(int RosterIndex)
		{
			std::string Name = OpName(RosterIndex);
			std::transform(Name.begin(), Name.end(), Name.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
			return Name;
		}

		std::vector<FResourceView> Resources(const FProfile& Profile)
		{
			// Owner request 2026-10-06: the player picks the six currencies (Settings > Lobby).
			std::vector<FResourceView> Views;
			for (int Slot = 0; Slot < TopBarSlots; ++Slot)
			{
				const ECurrency Currency = static_cast<ECurrency>(std::clamp(Profile.Meta.Settings.TopBar[Slot], 0, TopBarChoiceCount - 1));
				Views.push_back({CurrencyIcon(Currency), CurrencyName(Currency), Profile.Get(Currency)});
			}
			return Views;
		}

		std::string CommanderLine(const FProfile& Profile)
		{
			return Profile.Meta.Name + "  Lv " + std::to_string(CommanderLevel(Profile)) + "  (Day " + std::to_string(Profile.Day) + ")";
		}

		std::string Duration(long long Seconds)
		{
			if (Seconds < 0)
			{
				return "--";
			}
			const long long Days = Seconds / 86400;
			const long long Hours = (Seconds % 86400) / 3600;
			const long long Minutes = (Seconds % 3600) / 60;
			char Buffer[48];
			if (Days > 0)
			{
				std::snprintf(Buffer, sizeof(Buffer), "%lldd %lldh", Days, Hours);
			}
			else if (Hours > 0)
			{
				std::snprintf(Buffer, sizeof(Buffer), "%lldh %02lldm", Hours, Minutes);
			}
			else
			{
				std::snprintf(Buffer, sizeof(Buffer), "%lldm %02llds", Minutes, Seconds % 60);
			}
			return Buffer;
		}

		int BannerShowcase(const FProfile& Profile, int BannerIndex)
		{
			if (BannerIndex < 0 || BannerIndex >= static_cast<int>(Profile.Banners.size()))
			{
				return -1;
			}
			const FBanner& Banner = Profile.Banners[static_cast<size_t>(BannerIndex)];
			if (Banner.FeaturedOperator >= 0)
			{
				return Banner.FeaturedOperator;
			}
			// Standard and Daily banners rotate a showcase: 6* for Standard, 5* for Daily (PLACEHOLDER choice).
			std::vector<int> Pool;
			const int Stars = Banner.Type == EBannerType::Daily ? 5 : 6;
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				if (Roster()[static_cast<size_t>(Index)].Stars == Stars)
				{
					Pool.push_back(Index);
				}
			}
			return Pool.empty() ? -1 : Pool[static_cast<size_t>(Profile.Day) % Pool.size()];
		}

		// ======================================================================= page building
		namespace
		{
			// -------------------------------------------------------------- Operator tab
			void BuildOperators(FProfile& Profile, const FUiState& State, FBuilder& B)
			{
				FPage& Page = B.Page;
				Page.Title = "Operators";
				if (State.Page.empty())
				{
					Page.Subtitle = "Owned " + std::to_string(OperatorsOwned(Profile)) + " / " + std::to_string(Roster().size()) + " - tap an operator";
					for (int Index : SortedOperators())
					{
						const FRosterEntry& Entry = Roster()[static_cast<size_t>(Index)];
						const FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(Index)];
						const bool bInTeam = Profile.Team[0] == Index || Profile.Team[1] == Index || Profile.Team[2] == Index;
						B.Tile("op:sel:" + std::to_string(Index), Entry.Name, "portrait:" + LowerName(Index),
							Owned.bOwned ? std::string(StarsText(Entry.Stars)).substr(0, 2) + " " + Element::Name(Element::OfOperator(Index)) + " Lv " + std::to_string(Owned.Level) + " | BP " + Num(OperatorPower(Profile, Index))
										 : std::string(Element::Name(Element::OfOperator(Index))) + " | not owned",
							true, bInTeam, false);
					}
					return;
				}
				const int Index = State.SelOperator;
				const FRosterEntry& Entry = Roster()[static_cast<size_t>(Index)];
				const FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(Index)];
				B.Portrait(Index);
				Page.bCanBack = true;
				if (State.Page == "op:equip")
				{
					const ESlot Slot = static_cast<ESlot>(State.SelSlot);
					Page.Title = std::string(SlotName(Slot)) + " for " + Entry.Name;
					const FEquipment* Worn = Profile.EquippedIn(Index, Slot);
					B.Line(std::string("Equipped: ") + (Worn ? Worn->Name + " (" + BonusText(*Worn) + ")" : "nothing"));
					const FEquipment* Chosen = nullptr;
					for (const FEquipment& Item : Profile.Inventory)
					{
						if (Item.Id == State.SelItem && Item.Slot == Slot)
						{
							Chosen = &Item;
						}
					}
					B.Line(Chosen ? "Selected: " + Chosen->Name + " (" + BonusText(*Chosen) + ", upgrade +" + std::to_string(Chosen->UpgradeLevel) + ")"
								  : "Select an item below, then Equip / Unequip / Upgrade.");
					std::vector<const FEquipment*> Items;
					for (const FEquipment& Item : Profile.Inventory)
					{
						if (Item.Slot == Slot)
						{
							Items.push_back(&Item);
						}
					}
					const int Offset = std::clamp(State.ListOffset, 0, std::max(0, static_cast<int>(Items.size()) - 1) / 12 * 12);
					for (int Row = Offset; Row < std::min(static_cast<int>(Items.size()), Offset + 12); ++Row)
					{
						const FEquipment& Item = *Items[static_cast<size_t>(Row)];
						B.Tile("op:item:" + std::to_string(Item.Id), Item.Name, std::string(Slot == ESlot::Weapon ? "nav_campaign" : "s_armoury"),
							BonusText(Item) + (Item.EquippedBy >= 0 ? " | on " + OpName(Item.EquippedBy) : ""), true, Item.Id == State.SelItem, false);
					}
					const bool bHas = Chosen != nullptr;
					B.Button("op:equip", "Equip", "", bHas && Owned.bOwned);
					B.Button("op:unequip", "Unequip", "", bHas && Chosen->EquippedBy >= 0);
					B.Button("op:upg", "Upgrade", bHas ? Num(FProfile::EquipmentUpgradeCost(Chosen->UpgradeLevel)) + " Bytes" : "", bHas && Chosen->UpgradeLevel < Config::EquipMaxUpgrade);
					if (Items.size() > 12)
					{
						B.Button("op:pg:-1", "Prev page", "", Offset > 0);
						B.Button("op:pg:1", "Next page", "", Offset + 12 < static_cast<int>(Items.size()));
					}
					return;
				}
				Page.Title = Entry.Name;
				Page.Subtitle = StarsText(Entry.Stars);
				if (!Owned.bOwned)
				{
					Gacha::OperatorElement(Profile, Index, Page);
					B.Line("Not owned. Summon to recruit this operator.");
					B.Button("op:prev", "< Previous");
					B.Button("op:next", "Next >");
					return;
				}
				const FCombatStats Stats = Profile.GetCombatStats(Index);
				B.Line("Battle power " + Num(OperatorPower(Profile, Index)) + "  (health, attack and defence after levels, stars and equipment, weighted by rarity)");
				B.Line("Level " + std::to_string(Owned.Level) + " / " + std::to_string(Profile.LevelCap(Index)) + "   Upgrade stars " + std::to_string(Owned.UpgradeStars)
					+ "   Duplicate units " + std::to_string(Owned.DupResource));
				B.Line("HP " + Num(static_cast<long long>(Stats.MaxHP)) + "   Damage x" + Fixed(Stats.DamageMult, 2) + "   Defence " + Fixed(Stats.DefencePercent, 1) + "%");
				B.Line(Entry.Kit == EKit::Placeholder ? "Kit: placeholder fighter (regular attack, block, parry, dodge). Moves are DEFERRED."
													  : "Kit: Move 1-3, Special and Ultimate are playable in the demo.");
				const FCost LevelCost = FProfile::LevelUpCost(Owned.Level);
				if (Profile.NeedsHeartGate(Index))
				{
					B.Line("ANGEL-HEART GATE at level " + std::to_string(Owned.Level) + ": " + Num(FProfile::HeartGateCost(Owned.Level / Config::HeartGateEvery)) + " Angel hearts.");
				}
				const bool bFreeLevel = Profile.TutorialStep == static_cast<int>(ETutorialStep::LevelUpLuna);
				B.Tile("op:lvl", "Level up", "plus", bFreeLevel ? std::string("free (tutorial)") : CostText(LevelCost.Gold, LevelCost.Bytes), !Profile.NeedsHeartGate(Index) && Owned.Level < Profile.LevelCap(Index));
				B.Tile("op:lvld", "Level up + Diamonds", "diamond", "covers missing Gold/Bytes", !Profile.NeedsHeartGate(Index) && Owned.Level < Profile.LevelCap(Index));
				B.Tile("op:gate", "Pay heart gate", "hearts",
					Profile.NeedsHeartGate(Index) ? Num(FProfile::HeartGateCost(Owned.Level / Config::HeartGateEvery)) + " hearts" : "no gate now", Profile.NeedsHeartGate(Index));
				B.Tile("op:star", "Upgrade star", "star", Num(FProfile::UpgradeStarCost(Owned.UpgradeStars + 1)) + " duplicate units");
				Gacha::OperatorElement(Profile, Index, Page);   // element line and the Attunement tile (2026-10-07)
				for (int Slot = 0; Slot < static_cast<int>(ESlot::Count); ++Slot)
				{
					const FEquipment* Worn = Profile.EquippedIn(Index, static_cast<ESlot>(Slot));
					B.Tile("op:slot:" + std::to_string(Slot), SlotName(static_cast<ESlot>(Slot)), Slot == 3 ? "nav_campaign" : "s_armoury",
						Worn ? Worn->Name + " " + BonusText(*Worn) : "empty - change");
				}
				B.Button("op:prev", "< Previous");
				B.Button("op:next", "Next >");
			}

			// -------------------------------------------------------------- Team tab
			void BuildTeam(FProfile& Profile, const FUiState&, FBuilder& B)
			{
				FPage& Page = B.Page;
				Page.Title = "Team";
				Page.Subtitle = "Slot 1 enters first; if the active operator is defeated the next in line steps in. Tab switches in battle.";
				B.Portrait(Profile.Team[0]);
				bool bHasNarrator = false;
				for (int Slot = 0; Slot < 3; ++Slot)
				{
					const int Index = Profile.Team[Slot];
					B.Line("Slot " + std::to_string(Slot + 1) + ": " + (Index >= 0 ? OpName(Index) + " " + Gacha::ElementTag(Index) : std::string("empty")) + (Index >= 0 ? "  " + StarsText(Roster()[static_cast<size_t>(Index)].Stars)
						+ "  Lv " + std::to_string(Profile.Operators[static_cast<size_t>(Index)].Level) + "  BP " + Num(OperatorPower(Profile, Index)) : std::string()));
					B.Tile("team:cycle:" + std::to_string(Slot), "Slot " + std::to_string(Slot + 1) + ": " + (Index >= 0 ? OpName(Index) : std::string("empty")),
						Index >= 0 ? "portrait:" + LowerName(Index) : "", Index >= 0 ? "BP " + Num(OperatorPower(Profile, Index)) + " - tap to change" : "tap to add an operator", true, Slot == 0);
					if (Index >= 0 && Roster()[static_cast<size_t>(Index)].Kit == EKit::Narrator)
					{
						bHasNarrator = true;
					}
				}
				B.Tile("team:swap:0", "Swap 1 <-> 2", "s_trade");
				B.Tile("team:swap:1", "Swap 2 <-> 3", "s_trade");
				B.Tile("team:rot", "Rotate order", "s_trade", "1>2>3>1");
				B.Line("Squad battle power: " + Num(TeamBattlePower(Profile)) + (Profile.TeamIsPlayable() ? "" : "   (needs " + std::to_string(Profile.RequiredTeamSize()) + " different owned operator" + (Profile.RequiredTeamSize() == 1 ? "" : "s") + ")"));
				B.Line("Special mechanics set here apply at the start of each stage:");
				if (bHasNarrator)
				{
					B.Line("Narrator - Global Exchange starts with: from " + std::string(StatName(Profile.Meta.NarratorExchangeFrom)) + " to " + StatName(Profile.Meta.NarratorExchangeTo));
					B.Tile("team:nx:from", "Exchange from", "s_trade", StatName(Profile.Meta.NarratorExchangeFrom));
					B.Tile("team:nx:to", "Exchange to", "s_trade", StatName(Profile.Meta.NarratorExchangeTo));
				}
				else
				{
					B.Line("(None of your team has a configurable mechanic. Narrator's Global Exchange presets appear when she is in the team.)");
				}
				B.Button("team:default", "Default squad", "Luna, Judge, Narrator (if owned)");
			}

			// -------------------------------------------------------------- Lobby tab
			// --------------------------------------------------------- Tester panel
			// Lobby > Tester panel (BIBTester.h). Every page is plain data, so the Unreal widget draws it without changes.
			std::string AccountKind(const FProfile& Profile)
			{
				if (Profile.Account.bAdmin && Profile.Account.bSignedIn) return "admin account";
				if (Profile.Account.bSignedIn) return "signed-in account";
				if (Profile.Account.bGuest) return "guest";
				return "developer profile";
			}

			void BuildTester(FProfile& Profile, const FUiState& State, FBuilder& B)
			{
				FPage& Page = B.Page;
				const std::string& P = State.Page;
				const std::string Step = Num(Tester::StepAmount(State.TesterStep));
				const std::string Mode = Tester::ModeName(State.TesterMode);
				const bool bUnlimited = Profile.bUnlimited;
				if (P == "tester")
				{
					Page.Title = "Tester panel";
					Page.Subtitle = "Everyone who joins this build is a tester";
					B.Line("Playing as: " + Profile.Meta.Name + " (" + AccountKind(Profile) + ")");
					B.Line("Set any currency, operator unlock, level or upgrade star, campaign progress, gear, XP, the day and the summon pity counters.");
					B.Line("Changes are saved with the profile like normal play. The admin account keeps its unlimited wallet, every operator and every level.");
					B.Tile("go:tester-cur", "Currencies", "gold", "every amount");
					B.Tile("go:tester-ops", "Operators", "nav_operator", "unlock, level, stars");
					B.Tile("go:tester-camp", "Campaign and stages", "nav_campaign", "levels cleared, tower, stories");
					B.Tile("go:tester-gear", "Equipment", "s_armoury", "armour and weapons");
					B.Tile("go:tester-prog", "XP, day and more", "xp", "commander, season, day, world");
					B.Tile("go:tester-pity", "Summon pity", "nav_summon", "pity counters, 50/50");
					B.Tile("go:tester-preset", "Presets", "star", "whole-profile shortcuts");
					if (Profile.TutorialActive())
					{
						B.Tile("tst:tut:skip", "Skip tutorial", "next", "unlocks Kuu and Plack", true, true);
					}
					return;
				}
				if (P == "tester-cur")
				{
					Page.Title = "Tester: currencies";
					Page.Subtitle = "Tap a currency to " + std::string(Tester::ModeName(State.TesterMode)) + " " + Step;
					if (bUnlimited)
					{
						B.Line("This account's wallet is unlimited (admin) and cannot be edited.");
					}
					B.Line("Amount per tap: " + Step + "   Mode: " + Mode + "   (use the buttons below to change them)");
					for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
					{
						const ECurrency Currency = static_cast<ECurrency>(Index);
						B.Tile("tst:cur:" + std::to_string(Index), CurrencyName(Currency), CurrencyIcon(Currency), Num(Profile.Get(Currency)), !bUnlimited);
					}
					B.Button("tst:step", "Amount: " + Step, "tap to change", true, false, "plus");
					B.Button("tst:mode", "Mode: " + Mode, "add / remove / set");
					for (int Preset = 0; Preset < Tester::WalletPresetCount; ++Preset)
					{
						B.Button("tst:wallet:" + std::to_string(Preset), Tester::WalletPresetName(Preset), "", !bUnlimited);
					}
					return;
				}
				if (P == "tester-ops")
				{
					Page.Title = "Tester: operators";
					Page.Subtitle = "Owned " + std::to_string(OperatorsOwned(Profile)) + " / " + std::to_string(Roster().size()) + " - tap an operator to edit it";
					for (int Index : SortedOperators())
					{
						const FRosterEntry& Entry = Roster()[static_cast<size_t>(Index)];
						const FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(Index)];
						B.Tile("tst:op:sel:" + std::to_string(Index), Entry.Name, "portrait:" + LowerName(Index),
							Owned.bOwned ? "Lv " + std::to_string(Owned.Level) + "  " + std::to_string(Owned.UpgradeStars) + " stars" : "locked", true, Owned.bOwned);
					}
					B.Button("tst:ops:unlock", "Unlock all");
					B.Button("tst:ops:lock", "Lock all", "keeps Luna");
					B.Button("tst:ops:lv:1", "All to Lv 1");
					B.Button("tst:ops:lv:50", "All to Lv 50");
					B.Button("tst:ops:lv:100", "All to Lv 100");
					B.Button("tst:ops:lv:200", "All to Lv 200", "adds stars");
					return;
				}
				if (P == "tester-op")
				{
					const int Index = std::clamp(State.SelOperator, 0, static_cast<int>(Roster().size()) - 1);
					const FRosterEntry& Entry = Roster()[static_cast<size_t>(Index)];
					const FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(Index)];
					Page.Title = std::string("Tester: ") + Entry.Name;
					Page.Subtitle = Owned.bOwned ? "Unlocked" : "Locked - unlock to edit";
					B.Portrait(Index);
					B.Line(std::string(StarsText(Entry.Stars)) + "   " + (Owned.bOwned ? "unlocked" : "locked"));
					B.Line("Level " + std::to_string(Owned.Level) + " / cap " + std::to_string(Profile.LevelCap(Index)) + "   Upgrade stars " + std::to_string(Owned.UpgradeStars)
						+ "   Duplicate units " + Num(Owned.DupResource));
					B.Line(Profile.NeedsHeartGate(Index) ? "An Angel-heart gate is due at this level." : "No Angel-heart gate due.");
					const bool bOwned = Owned.bOwned;
					B.Tile("tst:op:own", bOwned ? "Lock operator" : "Unlock operator", bOwned ? "lock" : "check");
					B.Tile("tst:op:lv:-10", "Level -10", "back", "", bOwned);
					B.Tile("tst:op:lv:-1", "Level -1", "back", "", bOwned);
					B.Tile("tst:op:lv:1", "Level +1", "next", "", bOwned);
					B.Tile("tst:op:lv:10", "Level +10", "next", "", bOwned);
					B.Tile("tst:op:lvset:1", "Level 1", "xp", "", bOwned);
					B.Tile("tst:op:lvset:50", "Level 50", "xp", "", bOwned);
					B.Tile("tst:op:lvcap", "Level to cap", "xp", "cap " + std::to_string(Profile.LevelCap(Index)), bOwned);
					B.Tile("tst:op:star:-1", "Star -1", "star", "", bOwned);
					B.Tile("tst:op:star:1", "Star +1", "star", "+20 level cap", bOwned);
					B.Tile("tst:op:starset:0", "Stars 0", "star", "", bOwned);
					B.Tile("tst:op:starset:5", "Stars 5", "star", "cap 200", bOwned);
					B.Tile("tst:op:dup", "Duplicates", "plus", Mode + " " + Step, bOwned);
					B.Tile("tst:op:dup0", "Duplicates 0", "cross", "", bOwned);
					const bool bAtGate = bOwned && Owned.Level % Config::HeartGateEvery == 0;
					B.Tile("tst:op:gate", Profile.NeedsHeartGate(Index) ? "Open heart gate" : "Close heart gate", "hearts", bAtGate ? "" : "no gate at this level", bAtGate);
					B.Tile("tst:op:prev", "Previous operator", "back");
					B.Tile("tst:op:next", "Next operator", "next");
					B.Button("tst:step", "Amount: " + Step, "for duplicates");
					B.Button("tst:mode", "Mode: " + Mode);
					return;
				}
				if (P == "tester-camp")
				{
					Page.Title = "Tester: campaign and stages";
					Page.Subtitle = std::to_string(Profile.StagesCleared) + " / " + std::to_string(TotalCampaignLevels) + " levels cleared";
					B.Line(Profile.StagesCleared >= TotalCampaignLevels ? "The campaign is complete." : "Next level: " + StageName(Profile.StagesCleared + 1));
					B.Line("Levels unlock in order, so tapping a chapter clears every level up to its end. Setting progress does not give the Chapter 1 boss reward: unlock Plack on the Operators page.");
					for (int Chapter = 1; Chapter <= CampaignChapters; ++Chapter)
					{
						const int Last = Chapter * CampaignLevels;
						B.Tile("tst:camp:" + std::to_string(Last), "Chapter " + std::to_string(Chapter), Profile.StagesCleared >= Last ? "check" : "stage",
							Profile.StagesCleared >= Last ? "cleared" : "clear to its end");
					}
					B.Tile("tst:tower:0", "Tower: Operator +10", "tower", "best floor " + std::to_string(Profile.Meta.TowerBest[0]));
					B.Tile("tst:tower:1", "Tower: Secrets +10", "tower", "best floor " + std::to_string(Profile.Meta.TowerBest[1]));
					B.Tile("tst:tower:reset", "Tower: reset both", "cross");
					B.Tile("tst:sub:1", "Sub-stories: all cleared", "book");
					B.Tile("tst:sub:0", "Sub-stories: none", "cross");
					B.Button("tst:camp:none", "None cleared");
					B.Button("tst:camp:d:-8", "-1 chapter");
					B.Button("tst:camp:d:-1", "-1 level");
					B.Button("tst:camp:d:1", "+1 level");
					B.Button("tst:camp:d:8", "+1 chapter");
					B.Button("tst:camp:all", "All " + std::to_string(TotalCampaignLevels));
					return;
				}
				if (P == "tester-gear")
				{
					const std::string Percent = Fixed(Tester::GearPercent(State.TesterGear), 0) + "%";
					Page.Title = "Tester: equipment";
					Page.Subtitle = std::to_string(Profile.Inventory.size()) + " pieces owned";
					B.Line("New pieces get a +" + Percent + " bonus (change it with the first button). Wear them from the Operator tab, or use Equip best on team.");
					B.Tile("tst:gear:add:0:1", "Helmet: defence", "s_armoury", "+" + Percent);
					B.Tile("tst:gear:add:0:0", "Helmet: damage", "s_armoury", "+" + Percent);
					B.Tile("tst:gear:add:1:1", "Chest plate: defence", "s_armoury", "+" + Percent);
					B.Tile("tst:gear:add:1:0", "Chest plate: damage", "s_armoury", "+" + Percent);
					B.Tile("tst:gear:add:2:1", "Leggings: defence", "s_armoury", "+" + Percent);
					B.Tile("tst:gear:add:2:0", "Leggings: damage", "s_armoury", "+" + Percent);
					B.Tile("tst:gear:add:3:0", "Weapon: damage", "s_armoury", "+" + Percent);
					B.Button("tst:gear", "Bonus: " + Percent, "tap to change");
					B.Button("tst:gear:up:5", "Upgrade all to +5");
					B.Button("tst:gear:up:0", "Upgrade all to +0");
					B.Button("tst:gear:equip", "Equip best on team");
					B.Button("tst:gear:clear", "Clear all gear");
					return;
				}
				if (P == "tester-prog")
				{
					Page.Title = "Tester: XP, day and more";
					Page.Subtitle = "Amount per tap: " + Step + "   Mode: " + Mode;
					B.Line("Commander level " + std::to_string(CommanderLevel(Profile)) + " (XP " + Num(Profile.Meta.CommanderXp) + ")   Season tier " + std::to_string(SeasonTier(Profile))
						+ " (XP " + Num(Profile.Meta.SeasonXp) + ", " + (Profile.Meta.bPremiumPass ? "premium" : "free") + ")");
					B.Line("Day " + std::to_string(Profile.Day) + "   Outer World hours " + Fixed(Profile.Meta.WorldHours, 1) + "   Arena trophies " + Num(Profile.Meta.Arena.Trophies)
						+ "   Tutorial " + (Profile.TutorialActive() ? "step " + std::to_string(Profile.TutorialStep + 1) : std::string("done")));
					B.Tile("tst:xp:c", "Commander XP", "xp", Mode + " " + Step);
					B.Tile("tst:xp:clset:1", "Commander Lv 1", "xp");
					B.Tile("tst:xp:clset:20", "Commander Lv 20", "xp");
					B.Tile("tst:xp:clset:40", "Commander Lv 40", "xp");
					B.Tile("tst:xp:clset:60", "Commander Lv 60", "xp", "the top level");
					B.Tile("tst:xp:s", "Season XP", "seasonpass", Mode + " " + Step);
					B.Tile("tst:xp:slset:0", "Season tier 0", "seasonpass");
					B.Tile("tst:xp:slset:10", "Season tier 10", "seasonpass");
					B.Tile("tst:xp:slset:25", "Season tier 25", "seasonpass");
					B.Tile("tst:xp:slset:50", "Season tier 50", "seasonpass", "the top tier");
					B.Tile("tst:pass", Profile.Meta.bPremiumPass ? "Premium pass: on" : "Premium pass: off", "seasonpass", "tap to toggle");
					B.Tile("tst:day:1", "Next day", "event", "daily reset");
					B.Tile("tst:day:7", "+7 days", "event");
					B.Tile("tst:hours:12", "+12 Outer World hours", "b_shelter");
					B.Tile("tst:hours:72", "+72 Outer World hours", "b_shelter");
					B.Tile("tst:bld:0", "Outer World: none built", "b_shelter");
					B.Tile("tst:bld:5", "Outer World: all Lv 5", "b_shelter");
					B.Tile("tst:bld:10", "Outer World: all Lv 10", "b_shelter", "the top level");
					B.Tile("tst:arena", "Arena trophies", "arena", Mode + " " + Step);
					B.Tile("tst:arena:att", "Refill arena attempts", "arena");
					B.Tile("tst:return", "Start welcome-back event", "event", "as if away 14 days");
					B.Tile("tst:season", "End the month now", "seasonpass", "season pass, arena, gang shop");
					if (Profile.TutorialActive())
					{
						B.Tile("tst:tut:skip", "Skip tutorial", "next", "unlocks Kuu and Plack", true, true);
					}
					B.Button("tst:step", "Amount: " + Step, "tap to change", true, false, "plus");
					B.Button("tst:mode", "Mode: " + Mode, "add / remove / set");
					return;
				}
				if (P == "tester-pity")
				{
					Page.Title = "Tester: summon pity";
					B.Line("5-star pity guarantees at " + std::to_string(Config::Pity5Rolls) + " rolls, 6-star at " + Num(Config::Pity6Rolls) + " (MAIN section 2). Set a counter one short to test the guarantee on the next roll.");
					for (size_t Index = 0; Index < Profile.Banners.size(); ++Index)
					{
						const FBanner& Banner = Profile.Banners[Index];
						if (Banner.Type == EBannerType::Daily)
						{
							continue;
						}
						const FPityState& Pity = Profile.Pity[Index];
						const std::string Id = std::to_string(Index);
						B.Line(Banner.Title + ": 5-star counter " + std::to_string(Pity.Since5) + ", 6-star counter " + Num(Pity.Since6) + (Pity.bLostFiftyFifty ? ", lost the last 50/50" : ""));
						B.Tile("tst:pity:" + Id + ":5", Banner.Title + ": 5* next", "nav_summon", "counter " + std::to_string(Config::Pity5Rolls - 1));
						B.Tile("tst:pity:" + Id + ":6", Banner.Title + ": 6* next", "nav_summon", "counter " + Num(Config::Pity6Rolls - 1));
						B.Tile("tst:pity:" + Id + ":0", Banner.Title + ": reset", "cross");
						B.Tile("tst:pity:" + Id + ":f", Banner.Title + ": lost 50/50", "star", Pity.bLostFiftyFifty ? "on (tap to clear)" : "off (tap to set)");
					}
					return;
				}
				if (P == "tester-preset")
				{
					Page.Title = "Tester: presets";
					Page.Subtitle = "Each preset replaces this profile's progress (name and id are kept)";
					B.Line("Starting points for testing, not a statement of how far a player should be. Presets 2 to 5 skip the tutorial.");
					if (bUnlimited)
					{
						B.Line("Admin account: the account keeps everything it has (unlimited wallet, every operator, every level). A preset only restores the fresh admin profile.");
					}
					for (int Preset = 0; Preset < Tester::PresetCount; ++Preset)
					{
						B.Tile("tst:preset:" + std::to_string(Preset), std::to_string(Preset + 1) + ". " + Tester::PresetName(Preset), "star", Tester::PresetNote(Preset));
					}
					return;
				}
			}

			void BuildLobby(FProfile& Profile, const FUiState& State, FBuilder& B)
			{
				FPage& Page = B.Page;
				const FMeta& Meta = Profile.Meta;
				Page.Tab = TabLobby;
				const std::string& P = State.Page;
				if (P.empty())
				{
					Extra::LobbyRoot(Profile, State, Page);
					Gacha::LobbyTiles(Profile, Page);
					return;
				}
				Page.bCanBack = true;
				if (Gacha::BuildLobbyPage(Profile, State, Page))
				{
					return;
				}
				if (Extra::BuildLobbyPage(Profile, State, Page))
				{
					return;
				}
				if (P.rfind("tester", 0) == 0)
				{
					if (Tester::Enabled())
					{
						BuildTester(Profile, State, B);
					}
					return;
				}
				if (P == "quests")
				{
					// Full-screen quest board (owner request 2026-10-06): Daily, Weekly, Story (chapter quests) and Milestones.
					Page.Title = "Quests";
					const int ListIndex = std::clamp(State.QuestTab, 0, static_cast<int>(EQuestList::Count) - 1);
					const EQuestList List = static_cast<EQuestList>(ListIndex);
					static const char* const Codes[4] = {"d", "w", "s", "m"};
					switch (List)
					{
					case EQuestList::Daily:
						Page.Subtitle = "Daily quests - reset every day (now day " + std::to_string(Profile.Day) + ")";
						break;
					case EQuestList::Weekly:
					{
						const int Days = DaysUntilWeeklyReset(Profile);
						Page.Subtitle = "Weekly quests - reset in " + std::to_string(Days) + " day" + (Days == 1 ? "" : "s");
						break;
					}
					case EQuestList::Story:
						Page.Subtitle = "Story quests - one-time rewards along the campaign (a quest every 2-3 levels, a ticket for every chapter)";
						break;
					default:
						Page.Subtitle = "Milestones - one-time account goals";
						break;
					}
					int Claimed = 0;
					for (int Index = 0; Index < QuestCount(List); ++Index)
					{
						Claimed += Quest(Profile, List, Index).bClaimed ? 1 : 0;
					}
					B.Line(std::to_string(Claimed) + " / " + std::to_string(QuestCount(List)) + " claimed.  Ready to claim: "
						+ std::to_string(ClaimableQuests(Profile, List)) + " here, " + std::to_string(ClaimableQuests(Profile)) + " in all lists.");
					if (List == EQuestList::Daily && Events::RoadOpen(Profile))
					{
						// Ticket road (2026-10-09): the daily stipend is unlocked by the daily quests.
						const Events::FRoad Road = Events::Road(Profile);
						B.Line("Ticket road (Lobby > Events): " + std::string(Road.bDailyClaimed ? "today's stipend is taken." : Road.bDailyReady ? "today's " +
							std::to_string(Events::RoadDailySpecial) + " Special tickets are ready!" : "claim " + std::to_string(Events::RoadDailyQuests - Road.DailyQuestsDone) +
							" more daily quest(s) for today's " + std::to_string(Events::RoadDailySpecial) + " Special tickets."));
					}
					// Story has 39 quests: show what is still open (claimable first), in campaign order.
					std::vector<int> Order;
					for (int Index = 0; Index < QuestCount(List); ++Index)
					{
						const FQuestInfo Q = Quest(Profile, List, Index);
						if (List != EQuestList::Story || !Q.bClaimed)
						{
							Order.push_back(Index);
						}
					}
					if (List == EQuestList::Story)
					{
						std::stable_sort(Order.begin(), Order.end(), [&Profile](int A, int C)
						{
							const FQuestInfo QA = StoryQuest(Profile, A);
							const FQuestInfo QC = StoryQuest(Profile, C);
							return (QA.bComplete && !QA.bClaimed) > (QC.bComplete && !QC.bClaimed);
						});
						if (Order.empty())
						{
							B.Line("Every story quest is claimed.");
						}
					}
					for (int Index : Order)
					{
						const FQuestInfo Q = Quest(Profile, List, Index);
						const bool bReady = Q.bComplete && !Q.bClaimed;
						const std::string State3 = Q.bClaimed ? "claimed" : (bReady ? "READY" : std::to_string(Q.Progress) + "/" + std::to_string(Q.Target));
						B.Tile(std::string("q:") + Codes[ListIndex] + ":" + std::to_string(Index), Q.Title,
							Q.bClaimed ? "check" : (List == EQuestList::Story ? "nav_campaign" : (List == EQuestList::Milestone ? "star" : "quest")),
							State3 + " | " + RewardText(Q.Reward), bReady, bReady, bReady);
					}
					for (int Tab = 0; Tab < static_cast<int>(EQuestList::Count); ++Tab)
					{
						const int Ready = ClaimableQuests(Profile, static_cast<EQuestList>(Tab));
						B.Button("q:tab:" + std::to_string(Tab), QuestListName(static_cast<EQuestList>(Tab)), Ready > 0 ? std::to_string(Ready) + " ready" : "",
							true, Tab == ListIndex);
					}
					B.Button("q:all", "Claim all", "", ClaimableQuests(Profile) > 0);
					return;
				}
				if (P == "mail")
				{
					Page.Title = "Mail";
					Page.Subtitle = std::to_string(UnclaimedMail(Profile)) + " with unclaimed attachments";
					for (const FMail& Mail : Meta.Mail)
					{
						B.Line(Mail.Title + " - " + RewardText(Mail.Reward) + (Mail.bClaimed ? "  (claimed)" : "") + "   " + Mail.Body);
						B.Tile("mail:" + std::to_string(Mail.Id), Mail.Title, Mail.bClaimed ? "check" : "mail", RewardText(Mail.Reward), !Mail.bClaimed, false, !Mail.bClaimed);
					}
					B.Button("mail:all", "Claim all", "", UnclaimedMail(Profile) > 0);
					B.Button("mail:del", "Delete claimed");
					return;
				}
				if (P == "pass")
				{
					Page.Title = "Season pass";
					Page.Subtitle = "Season XP " + Num(Meta.SeasonXp) + " | tier " + std::to_string(SeasonTier(Profile)) + " / " + std::to_string(SeasonTiers)
						+ " | " + (Meta.bPremiumPass ? "Premium track active" : "Free track only");
					B.Line(std::to_string(SeasonXpPerTier()) + " Season XP per tier from daily (240 a day) and weekly (700 a week) quests: finishing them all reaches tier 50 in about 15 days. "
						+ "The season lasts the calendar month (" + std::to_string(LiveOps::SeasonPassDaysLeft(Profile)) + " days left); unclaimed rewards are mailed when it ends.");
					const int First = std::clamp(State.PassPage, 0, 4) * 10 + 1;
					for (int Tier = First; Tier < First + 10; ++Tier)
					{
						const bool bReached = Tier <= SeasonTier(Profile);
						const bool bFreeDone = Meta.FreeClaimed[Tier];
						const bool bPaidDone = Meta.PremiumClaimed[Tier];
						B.Line("Tier " + std::to_string(Tier) + ":  Free " + RewardText(SeasonReward(Tier, false)) + (bFreeDone ? " (claimed)" : "")
							+ "   |   Paid " + RewardText(SeasonReward(Tier, true)) + (bPaidDone ? " (claimed)" : (Meta.bPremiumPass ? "" : " (locked)")));
						const bool bClaimable = bReached && (!bFreeDone || (Meta.bPremiumPass && !bPaidDone));
						B.Tile("pass:t:" + std::to_string(Tier), "Tier " + std::to_string(Tier), bReached ? (bClaimable ? "seasonpass" : "check") : "lock",
							std::string(bFreeDone ? "free done" : "free") + " / " + (bPaidDone ? "paid done" : (Meta.bPremiumPass ? "paid" : "paid locked")), bClaimable, false, bClaimable);
					}
					B.Button("pass:pg:-1", "Prev tiers", "", State.PassPage > 0);
					B.Button("pass:pg:1", "Next tiers", "", State.PassPage < 4);
					B.Button("pass:all", "Claim all", "", ClaimableSeasonTiers(Profile) > 0);
					B.Button("iap:7", "Premium track", "$9.99 (demo)", !Meta.bPremiumPass);
					return;
				}
				if (P == "profile")
				{
					Page.Title = "Profile";
					B.Line("Username: " + Meta.Name + "   User ID: " + (!Profile.Account.UserId.empty() ? Profile.Account.UserId : std::to_string(Meta.Uid)));
					if (Profile.Account.bSignedIn)
					{
						B.Line("Email: " + Profile.Account.Email + (Profile.Account.bAdmin ? "   ADMIN / OWNER ACCOUNT" : ""));
					}
					else if (Profile.Account.bGuest)
					{
						B.Line("Playing offline as a guest: progress is saved on this device only. Sign in to keep it on your account.");
					}
					B.Line("Commander level " + std::to_string(CommanderLevel(Profile)) + "   XP " + Num(Meta.CommanderXp) + "  (" + std::to_string(XpPerLevel()) + " per level)");
					B.Line("Operators owned: " + std::to_string(OperatorsOwned(Profile)) + " / " + std::to_string(Roster().size()) + "   Summons: " + std::to_string(Meta.Summons));
					B.Line("Campaign: " + std::to_string(Profile.StagesCleared) + " / " + std::to_string(TotalCampaignLevels) + " levels cleared"
						+ (Profile.StagesCleared >= TotalCampaignLevels ? " (complete)" : "  (next: " + StageName(Profile.StagesCleared + 1) + ")"));
					B.Line("Squad battle power " + Num(TeamBattlePower(Profile)) + (State.Net.bInGang ? "   Gang: " + State.Net.Gang.Name : ""));
					B.Line("Ark Tower best: Operator floor " + std::to_string(Meta.TowerBest[0]) + ", Secrets floor " + std::to_string(Meta.TowerBest[1]));
					B.Line("Arena: " + Arena::RankName(Arena::RankOf(Meta.Arena.Trophies)) + ", " + Num(Meta.Arena.Trophies) + " trophies, " + std::to_string(Meta.ArenaWins) + " wins, "
						+ std::to_string(Meta.ArenaLosses) + " losses");
					B.Line("Arcade: Boss Rush best round " + std::to_string(Meta.ArcadeRecords.BossRushBest) + ".");
					B.Line("Simulated purchases in this demo: $" + Fixed(Meta.IapSpentCents / 100.0, 2) + " (no real payment is ever taken).");
					return;
				}
			}

			// -------------------------------------------------------------- Shop tab
			void BuildShop(FProfile& Profile, const FUiState& State, FBuilder& B)
			{
				FPage& Page = B.Page;
				const FMeta& Meta = Profile.Meta;
				const std::string& P = State.Page;
				if (P.empty())
				{
					Page.Title = "Shop";
					Page.Subtitle = "Free-to-play shops use in-game resources. The Premium store uses real-money purchases (simulated in the demo).";
					B.Tile("go:armoury", "Armoury", "s_armoury", "armour for Gold and Bytes");
					B.Tile("go:trade", "Trade Centre", "s_trade", "exchange anything for anything");
					B.Tile("go:alley", "Back Alley", "s_alley", "high-end goods for Diamonds (level locks)");
					B.Tile("go:premium", "Premium store", "s_premium", "in-app purchases (demo)");
					return;
				}
				Page.bCanBack = true;
				if (P == "armoury")
				{
					Page.Title = "Armoury";
					Page.Subtitle = "Armour and blades for Gold and Bytes";
					const std::vector<FShopGear>& Stock = ArmouryStock();
					for (size_t Index = 0; Index < Stock.size(); ++Index)
					{
						const FShopGear& Gear = Stock[Index];
						const bool bAfford = Profile.Get(ECurrency::Gold) >= Gear.Gold && Profile.Get(ECurrency::Bytes) >= Gear.Bytes;
						B.Tile("shop:arm:" + std::to_string(Index), Gear.Name, Gear.Slot == ESlot::Weapon ? "nav_campaign" : "s_armoury",
							"+" + Fixed(Gear.Percent, 1) + "% " + (Gear.Bonus == EBonus::Damage ? "damage" : "defence") + " | " + CostText(Gear.Gold, Gear.Bytes), bAfford);
					}
					return;
				}
				if (P == "alley")
				{
					Page.Title = "Back Alley";
					Page.Subtitle = "High-end materials and armour for raw Diamonds. Items unlock with your Commander level (" + std::to_string(CommanderLevel(Profile)) + ").";
					const std::vector<FAlleyOffer>& Stock = AlleyStock();
					for (size_t Index = 0; Index < Stock.size(); ++Index)
					{
						const FAlleyOffer& Offer = Stock[Index];
						const bool bLocked = CommanderLevel(Profile) < Offer.LevelLock;
						const std::string Effect = Offer.bGear ? "+" + Fixed(Offer.Percent, 1) + "% " + (Offer.Bonus == EBonus::Damage ? "damage" : "defence") : RewardText(Offer.Reward);
						B.Tile("shop:alley:" + std::to_string(Index), Offer.Name, bLocked ? "lock" : "diamond",
							bLocked ? "Commander Lv " + std::to_string(Offer.LevelLock) : Num(Offer.Diamonds) + " Diamonds | " + Effect, !bLocked && Profile.Get(ECurrency::Diamonds) >= Offer.Diamonds);
					}
					return;
				}
				if (P == "trade")
				{
					Page.Title = "Trade Centre";
					Page.Subtitle = "Exchange almost anything for anything else (" + std::to_string(TradeFeePercent()) + "% fee). Diamonds can be given, not received.";
					static const long long Amounts[5] = {1, 10, 100, 1000, 10000};
					const std::vector<ECurrency>& Currencies = TradeCurrencies();
					const ECurrency From = Currencies[static_cast<size_t>(std::clamp(State.TradeFrom, 0, static_cast<int>(Currencies.size()) - 1))];
					const ECurrency To = Currencies[static_cast<size_t>(std::clamp(State.TradeTo, 0, static_cast<int>(Currencies.size()) - 1))];
					const long long Want = Amounts[std::clamp(State.TradeAmount, 0, 4)];
					const long long Cost = TradeCost(From, To, Want);
					B.Line(std::string("Give: ") + CurrencyName(From) + "  (you have " + Num(Profile.Get(From)) + ")");
					B.Line(std::string("Get:  ") + CurrencyName(To) + "  x " + Num(Want));
					B.Line(Cost > 0 ? "Price: " + Num(Cost) + " " + CurrencyName(From) : std::string("This pair is not allowed."));
					B.Tile("trade:from", "Give", "s_trade", CurrencyName(From));
					B.Tile("trade:to", "Get", "s_trade", CurrencyName(To));
					B.Tile("trade:amt", "Amount", "plus", "x " + Num(Want));
					B.Tile("trade:swap", "Swap sides", "s_trade");
					B.Button("trade:go", "Trade", Cost > 0 ? Num(Cost) + " " + CurrencyName(From) : "", Cost > 0 && Profile.Get(From) >= Cost);
					return;
				}
				if (P == "premium")
				{
					Page.Title = "Premium store";
					Page.Subtitle = "DEMO ONLY: purchases are simulated. No real payment, account or store is connected, and prices are placeholders.";
					const std::vector<FIapOffer>& Offers = IapOffers();
					for (size_t Index = 0; Index < Offers.size(); ++Index)
					{
						const FIapOffer& Offer = Offers[Index];
						const bool bDone = (Offer.bOncePerProfile && Meta.bStarterBundleBought) || (Offer.bGivesPass && Meta.bPremiumPass);
						B.Tile("iap:" + std::to_string(Index), Offer.Name, Offer.bGivesPass ? "seasonpass" : "s_premium",
							bDone ? "owned" : "$" + Fixed(Offer.PriceCents / 100.0, 2) + " (demo)", !bDone, false, false);
					}
					B.Line("Simulated spend so far: $" + Fixed(Meta.IapSpentCents / 100.0, 2));
					return;
				}
			}

			// -------------------------------------------------------------- Revolution tab
			void BuildRevolution(FProfile& Profile, const FUiState& State, FBuilder& B)
			{
				FPage& Page = B.Page;
				const FMeta& Meta = Profile.Meta;
				const std::string& P = State.Page;
				if (P.empty())
				{
					Page.Title = "Revolution";
					Page.Subtitle = "Rebuild the ruined outside world with your resources, or train in the Ark";
					B.Tile("go:world", "Outer World", "b_shelter", "buildings that produce while you are away");
					B.Tile("go:ark", "The Ark", "tower", "tower, arena and sub-stories");
					return;
				}
				Page.bCanBack = true;
				if (Gacha::BuildRevolutionPage(Profile, State, Page))
				{
					return;
				}
				if (Extra::BuildRevolutionPage(Profile, State, Page))
				{
					return;
				}
				if (P == "world")
				{
					Page.Title = "Outer World";
					Page.Subtitle = "Build and upgrade places; they passively produce Gold, Bytes and Angel hearts (stores up to " + std::to_string(static_cast<int>(BuildingStorageHours)) + " hours).";
					double GoldRate = 0.0;
					double BytesRate = 0.0;
					double HeartsRate = 0.0;
					for (int Index = 0; Index < BuildingCount; ++Index)
					{
						GoldRate += BuildingRate(Profile, Index, 0);
						BytesRate += BuildingRate(Profile, Index, 1);
						HeartsRate += BuildingRate(Profile, Index, 2);
					}
					B.Line("Production per hour: " + Num(static_cast<long long>(GoldRate)) + " Gold, " + Num(static_cast<long long>(BytesRate)) + " Bytes, " + Fixed(HeartsRate, 1) + " Angel hearts.   World time: "
						+ Fixed(Meta.WorldHours, 1) + " hours (real time; the tester panel skips ahead). Campaign bonus +" + Fixed((WorldCampaignMultiplier(Profile) - 1.0) * 100.0, 0)
						+ "% (1% per level cleared)" + (Events::OvertimeActive(Profile) ? "; OVERTIME WEEKEND: x2 production now." : ".") + " Boosts in the Inventory pay hours of this production at once.");
					double StoredGold = 0.0;
					double StoredBytes = 0.0;
					double StoredHearts = 0.0;
					for (const FBuildingState& State2 : Meta.Buildings)
					{
						StoredGold += State2.StoredGold;
						StoredBytes += State2.StoredBytes;
						StoredHearts += State2.StoredHearts;
					}
					B.Line("Ready to collect: " + Num(static_cast<long long>(StoredGold)) + " Gold, " + Num(static_cast<long long>(StoredBytes)) + " Bytes, " + Num(static_cast<long long>(StoredHearts)) + " Angel hearts.");
					for (int Index = 0; Index < BuildingCount; ++Index)
					{
						const FBuildingDef& Def = BuildingDef(Index);
						const FBuildingState& Building = Meta.Buildings[Index];
						const FCost Cost = BuildingNextCost(Profile, Index);
						std::string Sub;
						if (Building.bBuilt)
						{
							Sub = "Lv " + std::to_string(Building.Level) + " | ";
							Sub += Building.Level >= MaxBuildingLevel ? "max" : "upgrade " + CostText(Cost.Gold, Cost.Bytes);
						}
						else
						{
							Sub = "build " + CostText(Cost.Gold, Cost.Bytes);
						}
						std::string Output;
						if (Def.GoldPerHour > 0) Output += Num(static_cast<long long>(Def.GoldPerHour)) + " Gold/h ";
						if (Def.BytesPerHour > 0) Output += Num(static_cast<long long>(Def.BytesPerHour)) + " Bytes/h ";
						if (Def.HeartsPerHour > 0) Output += Fixed(Def.HeartsPerHour, 0) + " hearts/h ";
						B.Line(std::string(Def.Name) + ": " + Sub + " | base output " + Output + "(per level)");
						B.Tile("world:b:" + std::to_string(Index), Def.Name, Def.Icon, Sub, !Building.bBuilt || Building.Level < MaxBuildingLevel, Building.bBuilt);
					}
					B.Button("world:collect", "Collect all");
					return;
				}
				if (P == "ark")
				{
					Page.Title = "The Ark";
					Page.Subtitle = "Training grounds";
					B.Tile("go:tower", "Endless Tower", "tower", "3-4 star / 5-6 star towers, never reset");
					B.Tile("go:arena", "Arena", "arena", Arena::RankName(Arena::RankOf(Profile.Meta.Arena.Trophies)) + " - " + Num(Profile.Meta.Arena.Trophies) + " trophies");
					B.Tile("go:substories", "Sub-stories", "book", "short side chapters");
					B.Tile("go:ark-shop", "Ark shop", "ark_medal", Num(Profile.Get(ECurrency::ArkMedal)) + " Ark medals");
					return;
				}
				if (P == "substories")
				{
					Page.Title = "Sub-stories";
					Page.Subtitle = "Short extra chapters (1-3 levels) with small rewards. Titles are placeholders.";
					for (int Story = 0; Story < SubStoryCount; ++Story)
					{
						B.Line(SubStoryTitle(Story) + ": " + std::to_string(Meta.SubStoryCleared[Story]) + " / " + std::to_string(SubStoryLevels(Story)) + " levels cleared");
						B.Tile("sub:sel:" + std::to_string(Story), SubStoryTitle(Story), "book", std::to_string(Meta.SubStoryCleared[Story]) + "/" + std::to_string(SubStoryLevels(Story)),
							true, Meta.SubStoryCleared[Story] >= SubStoryLevels(Story));
					}
					return;
				}
				if (P == "sub")
				{
					const int Story = std::clamp(State.SelSubStory, 0, SubStoryCount - 1);
					Page.Title = SubStoryTitle(Story);
					Page.Subtitle = "Levels unlock in order";
					for (int Level = 1; Level <= SubStoryLevels(Story); ++Level)
					{
						const bool bCleared = Level <= Meta.SubStoryCleared[Story];
						const bool bOpen = Level <= Meta.SubStoryCleared[Story] + 1;
						B.Tile("sub:start:" + std::to_string(Level), "Level " + std::to_string(Level), bOpen ? "stage" : "lock",
							bCleared ? "cleared" : (bOpen ? "start" : "locked"), bOpen, bCleared);
					}
					return;
				}
			}

			// -------------------------------------------------------------- Campaign tab
			bool ChapterIsOpen(const FProfile& Profile, int Chapter)
			{
				const bool bAdminView = Profile.Account.bAdmin && Profile.Account.bSignedIn;
				return bAdminView || Chapter <= 1 || Profile.StagesCleared >= CampaignStageId(Chapter - 1, CampaignLevels);
			}

			void BuildCampaign(FProfile& Profile, const FUiState& State, FBuilder& B)
			{
				FPage& Page = B.Page;
				if (State.Page.empty())
				{
					Page.Title = "Campaign";
					Page.Subtitle = "13 chapters of 8 levels. Clear a chapter's boss (level 8) to open the next chapter.";
					B.Line("Cleared " + std::to_string(Profile.StagesCleared) + " / " + std::to_string(TotalCampaignLevels) + " levels. Each level shows the team level it is built for: replay earlier levels for Gold, Bytes and Angel hearts to level up.");
					for (int Chapter = 1; Chapter <= CampaignChapters; ++Chapter)
					{
						const bool bOpen = ChapterIsOpen(Profile, Chapter);
						const int InChapter = std::clamp(Profile.StagesCleared - (Chapter - 1) * CampaignLevels, 0, CampaignLevels);
						const std::string Sub = bOpen ? std::to_string(InChapter) + "/" + std::to_string(CampaignLevels) + " levels | " + Campaign::ChapterTheme(Chapter)
													  : "locked - clear Chapter " + std::to_string(Chapter - 1) + "'s boss";
						B.Tile("camp:ch:" + std::to_string(Chapter), "Chapter " + std::to_string(Chapter), bOpen ? "book" : "lock", Sub, true,
							bOpen && InChapter < CampaignLevels);
					}
					// Owner request 2026-10-07: the Neo Tokyo greybox map under the levels, walked freely with the team.
					B.Tile("camp:map:NeoTokyo", "Neo Tokyo", "stage", "free roam | greybox city, day / night and weather", true, false);
					return;
				}
				const int Chapter = std::clamp(State.SelChapter, 1, CampaignChapters);
				Page.bCanBack = true;
				Page.Title = "Chapter " + std::to_string(Chapter);
				Page.Subtitle = "Eight levels, played in order. Level 8 has the boss; every level ends with a mini-boss. " + Campaign::ChapterTheme(Chapter) + ".";
				B.Line("Team: " + OpName(Profile.Team[0]) + ", " + OpName(Profile.Team[1]) + ", " + OpName(Profile.Team[2]) + (Profile.TeamIsPlayable() ? "" : "  (not ready - Team tab)"));
				const bool bAdminView = Profile.Account.bAdmin && Profile.Account.bSignedIn;
				const int InChapter = std::clamp(Profile.StagesCleared - (Chapter - 1) * CampaignLevels, 0, CampaignLevels);
				B.Line("Cleared " + std::to_string(InChapter) + " / " + std::to_string(CampaignLevels) + " in this chapter. Each level: talk to the story contact (T), then clear every section. 'Lv' is the team level the level is built for (average of your three operators).");
				B.Line(Gacha::StageElementLine(Profile, CampaignStageId(Chapter, 1)));
				if (Config::UseCharge)
				{
					B.Line(Gacha::ChargeLine(Profile));
				}
				if (Chapter == Campaign::WalkChapter)
				{
					// Owner level idea (final): Return to Nowhere has no levels and no bosses - one walk of about five minutes.
					const int Stage = Campaign::WalkEntryStage();
					const bool bCleared = Campaign::WalkLastStage() <= Profile.StagesCleared;
					const bool bOpen = bAdminView || Stage <= Profile.StagesCleared + 1;
					Page.Subtitle = "Return to Nowhere.";
					B.Tile("camp:start:" + std::to_string(Stage), "Return to Nowhere", bOpen ? "stage" : "lock",
						bCleared ? "walked" : (bOpen ? "ready" : "locked"), bOpen, bOpen && !bCleared);
				}
				else
				{
					for (int Level = 1; Level <= CampaignLevels; ++Level)
					{
						const int Stage = CampaignStageId(Chapter, Level);
						const bool bCleared = Stage <= Profile.StagesCleared;
						const bool bOpen = bAdminView || Stage <= Profile.StagesCleared + 1;
						const std::vector<FSectionPlanData> Plan = StagePlan(Stage);
						int Enemies = 0;
						for (const FSectionPlanData& Section : Plan)
						{
							Enemies += Section.Total();
						}
						std::string Sub = bCleared ? "cleared" : (bOpen ? "ready" : "locked");
						Sub += " | Lv " + std::to_string(Campaign::RecommendedLevel(Stage)) + " | " + std::to_string(Plan.size()) + " sections, " + std::to_string(Enemies) + " enemies";
						if (Level >= CampaignLevels)
						{
							Sub += " | BOSS";
						}
						if (Config::UseCharge && bCleared)
						{
							Sub += " | replay " + std::to_string(Inventory::ReplayChargeCost(Stage)) + " Charge";
						}
						const std::string Hint = StageHint(Level);
						B.Tile("camp:start:" + std::to_string(Stage), "Level " + std::to_string(Level) + (Hint.empty() ? "" : " " + Hint), bOpen ? "stage" : "lock", Sub, bOpen, bOpen && !bCleared);
					}
					// Quick clear (2026-10-07): the newest cleared level of this chapter pays its rewards for Charge without playing.
					const int Newest = std::min(Profile.StagesCleared, CampaignStageId(Chapter, CampaignLevels));
					if (Newest >= CampaignStageId(Chapter, 1))
					{
						const int Cost = Config::UseCharge ? Inventory::ReplayChargeCost(Newest) : 0;
						const int Charge = Profile.Get(ECurrency::Charge);
						const std::string Name = "Lv " + std::to_string(LevelOfStage(Newest));
						B.Button("camp:sweep:" + std::to_string(Newest) + ":1", "Quick clear " + Name, std::to_string(Cost) + " Charge", Cost == 0 || Charge >= Cost);
						B.Button("camp:sweep:" + std::to_string(Newest) + ":5", "Quick clear x5", std::to_string(Cost * 5) + " Charge", Cost == 0 || Charge >= Cost * 5);
						B.Button("gx:inv", "Charge", Num(Charge) + " / " + std::to_string(Inventory::ChargeCap));
					}
				}
			}

			// -------------------------------------------------------------- Summon tab
			void BuildSummon(FProfile& Profile, const FUiState& State, FBuilder& B)
			{
				FPage& Page = B.Page;
				if (!State.SummonOps.empty())
				{
					Page.Title = "Summon results";
					Page.Subtitle = Profile.Banners[static_cast<size_t>(std::clamp(State.SelBanner, 0, static_cast<int>(Profile.Banners.size()) - 1))].Title;
					int Best = 0;
					for (size_t Index = 0; Index < State.SummonOps.size(); ++Index)
					{
						if (State.SummonStars[Index] > State.SummonStars[static_cast<size_t>(Best)])
						{
							Best = static_cast<int>(Index);
						}
					}
					B.Portrait(State.SummonOps[static_cast<size_t>(Best)]);
					for (size_t Index = 0; Index < State.SummonOps.size(); ++Index)
					{
						const int Op = State.SummonOps[Index];
						B.Line(StarsText(State.SummonStars[Index]) + "  " + OpName(Op) + (State.SummonNew[Index] ? "  NEW!" : "  (duplicate: +" + std::to_string(Config::DuplicateResource) + " upgrade units)"));
						B.Tile("sum:show:" + std::to_string(Op) + ":" + std::to_string(Index), OpName(Op), "portrait:" + LowerName(Op), StarsText(State.SummonStars[Index]) + (State.SummonNew[Index] ? " NEW" : ""),
							true, State.SummonNew[Index]);
					}
					B.Button("sum:again", "Summon again", State.SummonCount == 10 ? "x10" : "x1");
					B.Button("sum:done", "Back to banners");
					return;
				}
				Page.Title = "Summon";
				const int BannerIndex = std::clamp(State.SelBanner, 0, static_cast<int>(Profile.Banners.size()) - 1);
				const FBanner& Banner = Profile.Banners[static_cast<size_t>(BannerIndex)];
				Page.Subtitle = Banner.Title;
				B.Portrait(BannerShowcase(Profile, BannerIndex));
				const FPityState& Pity = Profile.Pity[static_cast<size_t>(BannerIndex)];
				if (Banner.Type == EBannerType::Daily)
				{
					B.Line("Daily banner: one free ticket a day, no 10x, no Diamonds, no pity, no Mileage.");
				}
				else
				{
					B.Line("Rates: 6* 0.1%   5* 2%   4* 19.9%   3* 78%. One 4*+ guaranteed per 10x; 5* pity at " + std::to_string(Config::Pity5Rolls) + ", 6* pity at " + std::to_string(Config::Pity6Rolls) + ".");
					B.Line("Rolls since last 5*: " + std::to_string(Pity.Since5) + "   since last 6*: " + std::to_string(Pity.Since6) + (Banner.FeaturedOperator >= 0 ? "   Lost last 50/50: " + std::string(Pity.bLostFiftyFifty ? "yes" : "no") : ""));
					B.Line("Mileage: " + std::to_string(Profile.Get(ECurrency::Mileage)));
				}
				if (Banner.RotationStars != 0)
				{
					B.Line("Featured operators change every " + std::to_string(Config::RotationDays) + " days - next change in " + Duration(Profile.SecondsUntilRotation()) + ".");
				}
				const ECurrency Ticket = Banner.Type == EBannerType::Featured ? ECurrency::SpecialTicket : (Banner.Type == EBannerType::Daily ? ECurrency::DailyTicket : ECurrency::RegularTicket);
				B.Line(std::string(CurrencyName(Ticket)) + ": " + std::to_string(Profile.Get(Ticket)) + "   Diamonds: " + Num(Profile.Get(ECurrency::Diamonds)));
				for (int Index = 0; Index < static_cast<int>(Profile.Banners.size()); ++Index)
				{
					const FBanner& Entry = Profile.Banners[static_cast<size_t>(Index)];
					B.Tile("sum:banner:" + std::to_string(Index), Entry.Title, Entry.FeaturedOperator >= 0 ? "portrait:" + LowerName(Entry.FeaturedOperator) : "nav_summon",
						Entry.RotationStars != 0 ? "changes in " + Duration(Profile.SecondsUntilRotation()) : std::string(), true, Index == BannerIndex);
				}
				const bool bDaily = Banner.Type == EBannerType::Daily;
				B.Button("sum:go:1:0", "x1 ticket", "1 " + std::string(CurrencyName(Ticket)), Profile.Get(Ticket) >= 1, false, "ticket_regular");
				B.Button("sum:go:10:0", "x10 tickets", "9 " + std::string(CurrencyName(Ticket)), !bDaily && Profile.Get(Ticket) >= Config::TenPullPaidRolls, false, "ticket_regular");
				B.Button("sum:go:1:1", "x1 Diamonds", Num(Config::DiamondsPerRoll), !bDaily && Profile.Get(ECurrency::Diamonds) >= Config::DiamondsPerRoll, false, "diamond");
				B.Button("sum:go:10:1", "x10 Diamonds", Num(Config::TenPullPaidRolls * Config::DiamondsPerRoll), !bDaily && Profile.Get(ECurrency::Diamonds) >= Config::TenPullPaidRolls * Config::DiamondsPerRoll, false, "diamond");
				B.Button("sum:tut", "Tutorial x1", "Kuu", !Profile.bTutorialSummonDone);
				B.Button("sum:mileage", "Mileage shop", "redeem featured", Banner.FeaturedOperator >= 0);
			}
		}

		FPage Build(FProfile& Profile, const FUiState& State)
		{
			Tutorial::Advance(Profile, State);
			FPage Page;
			Page.Tab = std::clamp(State.Tab, 0, TabCount - 1);
			Page.Status = State.Status;
			FBuilder Builder(Page);
			switch (Page.Tab)
			{
			case TabOperator: BuildOperators(Profile, State, Builder); break;
			case TabTeam: BuildTeam(Profile, State, Builder); break;
			case TabLobby: BuildLobby(Profile, State, Builder); break;
			case TabShop: BuildShop(Profile, State, Builder); break;
			case TabRevolution: BuildRevolution(Profile, State, Builder); break;
			case TabCampaign: BuildCampaign(Profile, State, Builder); break;
			case TabSummon: BuildSummon(Profile, State, Builder); break;
			default: break;
			}
			if (Page.Tab != TabSummon || !State.SummonOps.empty())
			{
				Page.bCanBack = Page.bCanBack || (Page.Tab == TabSummon && !State.SummonOps.empty());
			}
			// Owner request 2026-10-06: every page below a tab's root opens full screen, like the Season pass: the bottom tabs
			// make room (Back returns), and menu pages drop the side portrait. Operator pages and summon results keep theirs,
			// because the operator is what they show.
			Page.bFullScreen = Page.bCanBack;
			if (Page.bFullScreen && Page.Tab != TabOperator && Page.Tab != TabSummon && State.Page != "tester-op")
			{
				Page.Portrait.clear();
				Page.PortraitStars.clear();
				Page.PortraitName.clear();
			}
			Tutorial::Decorate(Profile, Page);
			return Page;
		}

		// ======================================================================= actions
		namespace
		{
			FPerform Result(const FActionResult& Action)
			{
				FPerform Out;
				Out.bOk = Action.bOk;
				Out.Message = Action.Message;
				return Out;
			}

			FPerform Note(const std::string& Message, bool bOk = true)
			{
				FPerform Out;
				Out.Message = Message;
				Out.bOk = bOk;
				return Out;
			}

			int NextOwned(const FProfile& Profile, int From, int Direction)
			{
				const std::vector<int> Order = SortedOperators();
				int Position = 0;
				for (size_t Index = 0; Index < Order.size(); ++Index)
				{
					if (Order[Index] == From)
					{
						Position = static_cast<int>(Index);
					}
				}
				const int Count = static_cast<int>(Order.size());
				for (int Step = 1; Step <= Count; ++Step)
				{
					const int Candidate = Order[static_cast<size_t>(((Position + Direction * Step) % Count + Count) % Count)];
					if (Profile.Owns(Candidate))
					{
						return Candidate;
					}
				}
				return From;
			}

			void RecordSummon(FProfile& Profile, FUiState& State, const FSummonOutcome& Outcome, int Count, int Pay)
			{
				State.SummonOps.clear();
				State.SummonStars.clear();
				State.SummonNew.clear();
				for (const FSummonResult& Item : Outcome.Results)
				{
					State.SummonOps.push_back(Item.Operator);
					State.SummonStars.push_back(Item.Stars);
					State.SummonNew.push_back(Item.bNew ? 1 : 0);
				}
				State.SummonCount = Count;
				State.SummonPay = Pay;
				Profile.Meta.Summons += static_cast<int>(Outcome.Results.size());
				NotifyQuest(Profile, EQuestEvent::Summon);
			}
		}

		FPerform Perform(FProfile& Profile, FUiState& State, const std::string& Id)
		{
			FMeta& Meta = Profile.Meta;
			const std::vector<std::string> Parts = Split(Id, ':');
			const std::string& Head = Parts.empty() ? Id : Parts[0];
			FPerform Out;
			auto Finish = [&State, &Profile](const FPerform& Done)
			{
				State.Status = Done.Message;
				Tutorial::Advance(Profile, State);
				return Done;
			};

			// During the tutorial only the actions the current step asks for run (BIBTutorial.h).
			if (!Tutorial::Allows(Profile, Id))
			{
				return Finish(Note("Follow the tutorial: " + Tutorial::Coach(Profile).Text, false));
			}

			// A pending "tap again" only survives the same action.
			if (State.ConfirmId != Id)
			{
				State.ConfirmId.clear();
			}
			// Settings > Gameplay: confirm Diamond spending with a second tap (owner request 2026-10-06, "preferences").
			const long long DiamondCost = Extra::DiamondCost(Profile, State, Id) + Gacha::DiamondCost(Profile, Id);
			if (DiamondCost > 0 && Meta.Settings.bConfirmSpend && !Profile.bUnlimited && Profile.Get(ECurrency::Diamonds) >= DiamondCost)
			{
				if (State.ConfirmId != Id)
				{
					State.ConfirmId = Id;
					return Finish(Note("Tap again to spend " + Num(DiamondCost) + " Diamonds (Settings > Gameplay turns this off)."));
				}
				State.ConfirmId.clear();
			}

			// The pages added on 2026-10-06 (arcade, arena, tower, friends, gangs, notices, admin desk, welcome back, settings).
			if (Gacha::Perform(Profile, State, Id, Out))
			{
				return Finish(Out);
			}
			if (Extra::Perform(Profile, State, Id, Out))
			{
				return Finish(Out);
			}

			if (Head == "tab")
			{
				State.Tab = std::clamp(ToInt(Parts, 1), 0, TabCount - 1);
				State.Page.clear();
				State.ListOffset = 0;
				State.SummonOps.clear();
				return Finish(Note(""));
			}
			if (Head == "back")
			{
				if (State.Tab == TabSummon && !State.SummonOps.empty())
				{
					State.SummonOps.clear();
				}
				else
				{
					State.Page = ParentPage(State.Page);
				}
				State.ListOffset = 0;
				return Finish(Note(""));
			}
			if (Head == "go")
			{
				State.Page = Parts.size() > 1 ? Parts[1] : "";
				if (State.Page == "prefs")
				{
					State.Page = "settings";   // the old Preferences page is now Settings > Lobby
					State.SettingsTab = 4;
				}
				State.ListOffset = 0;
				return Finish(Note(""));
			}

			// ------------------------------------------------------------ tester panel
			if (Head == "tst")
			{
				if (!Tester::Enabled())
				{
					return Finish(Note("Tester tools are off in this build.", false));
				}
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				const int Operator = std::clamp(State.SelOperator, 0, static_cast<int>(Roster().size()) - 1);
				const long long Amount = Tester::StepAmount(State.TesterStep);
				if (Sub == "step")
				{
					State.TesterStep = (State.TesterStep + 1) % Tester::StepCount;
					return Finish(Note("Amount per tap: " + Num(Tester::StepAmount(State.TesterStep)) + "."));
				}
				if (Sub == "mode")
				{
					State.TesterMode = (State.TesterMode + 1) % Tester::ModeCount;
					return Finish(Note(std::string("Mode: ") + Tester::ModeName(State.TesterMode) + "."));
				}
				if (Sub == "cur")
				{
					const int Currency = ToInt(Parts, 2, -1);
					if (Currency < 0 || Currency >= static_cast<int>(ECurrency::Count))
					{
						return Finish(Note("No such currency.", false));
					}
					return Finish(Result(Tester::ApplyCurrency(Profile, static_cast<ECurrency>(Currency), State.TesterMode, Amount)));
				}
				if (Sub == "wallet")
				{
					return Finish(Result(Tester::WalletPreset(Profile, ToInt(Parts, 2))));
				}
				if (Sub == "op")
				{
					const std::string Act = Parts.size() > 2 ? Parts[2] : "";
					if (Act == "sel")
					{
						State.SelOperator = std::clamp(ToInt(Parts, 3), 0, static_cast<int>(Roster().size()) - 1);
						State.Page = "tester-op";
						return Finish(Note(""));
					}
					if (Act == "prev" || Act == "next")
					{
						const int Count = static_cast<int>(Roster().size());
						State.SelOperator = ((Operator + (Act == "next" ? 1 : -1)) % Count + Count) % Count;
						return Finish(Note(""));
					}
					const FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(Operator)];
					if (Act == "own") return Finish(Result(Tester::SetOwned(Profile, Operator, !Owned.bOwned)));
					if (Act == "lv") return Finish(Result(Tester::SetLevel(Profile, Operator, Owned.Level + ToInt(Parts, 3))));
					if (Act == "lvset") return Finish(Result(Tester::SetLevel(Profile, Operator, ToInt(Parts, 3, 1))));
					if (Act == "lvcap") return Finish(Result(Tester::SetLevel(Profile, Operator, Profile.LevelCap(Operator))));
					if (Act == "star") return Finish(Result(Tester::SetStars(Profile, Operator, Owned.UpgradeStars + ToInt(Parts, 3))));
					if (Act == "starset") return Finish(Result(Tester::SetStars(Profile, Operator, ToInt(Parts, 3))));
					if (Act == "dup") return Finish(Result(Tester::SetDuplicates(Profile, Operator, State.TesterMode, Amount)));
					if (Act == "dup0") return Finish(Result(Tester::SetDuplicates(Profile, Operator, Tester::ModeSet, 0)));
					if (Act == "gate") return Finish(Result(Tester::SetGatePaid(Profile, Operator, Profile.NeedsHeartGate(Operator))));
				}
				if (Sub == "ops")
				{
					const std::string Act = Parts.size() > 2 ? Parts[2] : "";
					if (Act == "unlock")
					{
						Tester::UnlockAllOperators(Profile);
						return Finish(Note("Every operator is unlocked."));
					}
					if (Act == "lock")
					{
						Tester::LockAllOperators(Profile);
						return Finish(Note("Every operator except Luna is locked."));
					}
					if (Act == "lv")
					{
						Tester::SetAllLevels(Profile, ToInt(Parts, 3, 1));
						return Finish(Note("Every unlocked operator is now level " + std::to_string(std::min(ToInt(Parts, 3, 1), Tester::MaxOperatorLevel)) + "."));
					}
				}
				if (Sub == "camp")
				{
					const std::string Act = Parts.size() > 2 ? Parts[2] : "";
					if (Act == "none") Tester::SetCampaignProgress(Profile, 0);
					else if (Act == "all") Tester::SetCampaignProgress(Profile, TotalCampaignLevels);
					else if (Act == "d") Tester::SetCampaignProgress(Profile, Profile.StagesCleared + ToInt(Parts, 3));
					else Tester::SetCampaignProgress(Profile, ToInt(Parts, 2));
					return Finish(Note(std::to_string(Profile.StagesCleared) + " of " + std::to_string(TotalCampaignLevels) + " campaign levels are cleared."));
				}
				if (Sub == "tower")
				{
					if (Parts.size() > 2 && Parts[2] == "reset")
					{
						Tester::SetTowerBest(Profile, 0, 0);
						Tester::SetTowerBest(Profile, 1, 0);
						return Finish(Note("Tower best floors reset."));
					}
					const int Mode = ToInt(Parts, 2);
					Tester::SetTowerBest(Profile, Mode, Meta.TowerBest[std::clamp(Mode, 0, 1)] + 10);
					return Finish(Note("Tower best floor: " + std::to_string(Meta.TowerBest[std::clamp(Mode, 0, 1)]) + "."));
				}
				if (Sub == "sub")
				{
					Tester::SetSubStories(Profile, ToInt(Parts, 2) != 0);
					return Finish(Note(ToInt(Parts, 2) != 0 ? "Every sub-story level is cleared." : "Sub-story progress cleared."));
				}
				if (Sub == "gear")
				{
					const std::string Act = Parts.size() > 2 ? Parts[2] : "";
					if (Act.empty())
					{
						State.TesterGear = (State.TesterGear + 1) % Tester::GearPercentSteps;
						return Finish(Note("New gear bonus: +" + Fixed(Tester::GearPercent(State.TesterGear), 0) + "%."));
					}
					if (Act == "add")
					{
						const ESlot Slot = static_cast<ESlot>(std::clamp(ToInt(Parts, 3), 0, static_cast<int>(ESlot::Count) - 1));
						const FEquipment& Item = Tester::GrantGear(Profile, Slot, ToInt(Parts, 4) == 0 ? EBonus::Damage : EBonus::Defence, Tester::GearPercent(State.TesterGear));
						return Finish(Note("Added " + Item.Name + " (" + BonusText(Item) + ")."));
					}
					if (Act == "up")
					{
						Tester::SetAllGearUpgrade(Profile, ToInt(Parts, 3));
						return Finish(Note("Every piece is now at upgrade +" + std::to_string(std::clamp(ToInt(Parts, 3), 0, Config::EquipMaxUpgrade)) + "."));
					}
					if (Act == "equip")
					{
						const int Count = Tester::EquipBestOnTeam(Profile);
						return Finish(Note(Count > 0 ? "Equipped " + std::to_string(Count) + " pieces on the squad." : "Nothing new to equip (the squad already wears the best pieces)."));
					}
					if (Act == "clear")
					{
						Tester::ClearGear(Profile);
						State.SelItem = -1;
						return Finish(Note("All gear removed."));
					}
				}
				if (Sub == "xp")
				{
					const std::string Act = Parts.size() > 2 ? Parts[2] : "";
					if (Act == "c") return Finish(Result(Tester::SetCommanderXp(Profile, State.TesterMode, Amount)));
					if (Act == "s") return Finish(Result(Tester::SetSeasonXp(Profile, State.TesterMode, Amount)));
					if (Act == "clset")
					{
						Tester::SetCommanderLevel(Profile, ToInt(Parts, 3, 1));
						return Finish(Note("Commander level is now " + std::to_string(CommanderLevel(Profile)) + "."));
					}
					if (Act == "slset")
					{
						Tester::SetSeasonTier(Profile, ToInt(Parts, 3));
						return Finish(Note("Season tier is now " + std::to_string(SeasonTier(Profile)) + "."));
					}
				}
				if (Sub == "pass")
				{
					Meta.bPremiumPass = !Meta.bPremiumPass;
					return Finish(Note(Meta.bPremiumPass ? "Premium pass track unlocked." : "Premium pass track locked."));
				}
				if (Sub == "day")
				{
					for (int Step = 0; Step < std::clamp(ToInt(Parts, 2, 1), 1, 30); ++Step)
					{
						Profile.AdvanceDay();
					}
					return Finish(Note("It is now day " + std::to_string(Profile.Day) + ". Daily quests, tickets, tokens and arena attempts reset."));
				}
				if (Sub == "hours")
				{
					AdvanceWorld(Profile, static_cast<double>(std::clamp(ToInt(Parts, 2, 12), 1, 720)));
					return Finish(Note("Outer World hours passed."));
				}
				if (Sub == "bld")
				{
					Tester::SetBuildings(Profile, ToInt(Parts, 2));
					return Finish(Note("Outer World buildings set."));
				}
				if (Sub == "return")
				{
					LiveOps::StartReturn(Profile, Profile.NowUnix > 0 ? Profile.NowUnix : 1);
					return Finish(Note("Welcome-back event started (Lobby > Welcome back)."));
				}
				if (Sub == "season")
				{
					// Pretend the month just ended: the next clock update applies every season change.
					Meta.SeasonPassKey = std::max(1, Meta.SeasonPassKey - 1);
					Meta.Arena.SeasonKey = std::max(1, Meta.Arena.SeasonKey - 1);
					Meta.Gang.ShopSeason = std::max(1, Meta.Gang.ShopSeason - 1);
					const std::vector<std::string> Lines = LiveOps::UpdateClock(Profile, Profile.NowUnix > 0 ? Profile.NowUnix : 1);
					return Finish(Note(Lines.empty() ? std::string("Season ended.") : Lines.back()));
				}
				if (Sub == "arena")
				{
					if (Parts.size() > 2 && Parts[2] == "att")
					{
						Meta.ArenaAttempts = std::max(Meta.ArenaAttempts, 5);
						return Finish(Note("Arena attempts refilled."));
					}
					const int Points = static_cast<int>(std::min<long long>(Tester::MaxAmount, State.TesterMode == Tester::ModeSet ? Amount
						: State.TesterMode == Tester::ModeRemove ? std::max<long long>(0, Meta.Arena.Trophies - Amount) : Meta.Arena.Trophies + Amount));
					Tester::SetArenaPoints(Profile, Points);
					return Finish(Note("Arena trophies: " + Num(Meta.Arena.Trophies) + " (" + Arena::RankName(Arena::RankOf(Meta.Arena.Trophies)) + ")."));
				}
				if (Sub == "pity")
				{
					const int Banner = ToInt(Parts, 2, -1);
					if (Banner < 0 || Banner >= static_cast<int>(Profile.Pity.size()))
					{
						return Finish(Note("No such banner.", false));
					}
					const FPityState Now = Profile.Pity[static_cast<size_t>(Banner)];
					const std::string Act = Parts.size() > 3 ? Parts[3] : "";
					if (Act == "5") Tester::SetPity(Profile, Banner, Config::Pity5Rolls - 1, Now.Since6, Now.bLostFiftyFifty);
					else if (Act == "6") Tester::SetPity(Profile, Banner, Now.Since5, Config::Pity6Rolls - 1, Now.bLostFiftyFifty);
					else if (Act == "f") Tester::SetPity(Profile, Banner, Now.Since5, Now.Since6, !Now.bLostFiftyFifty);
					else Tester::SetPity(Profile, Banner, 0, 0, false);
					return Finish(Note("Pity counters updated."));
				}
				if (Sub == "tut")
				{
					if (!Profile.TutorialActive())
					{
						return Finish(Note("The tutorial is already finished."));
					}
					Tester::SkipTutorial(Profile);
					State.Tab = TabLobby;
					State.Page.clear();
					return Finish(Note("Tutorial skipped: Kuu and Plack unlocked, squad Luna / Kuu / Plack."));
				}
				if (Sub == "preset")
				{
					const int Preset = std::clamp(ToInt(Parts, 2), 0, Tester::PresetCount - 1);
					Tester::ApplyPreset(Profile, Preset);
					const int KeepSelection = State.SelOperator;
					State = FUiState();
					State.SelOperator = KeepSelection;
					State.Tab = TabLobby;
					State.Page = "tester-preset";
					return Finish(Note(std::string("Preset applied: ") + Tester::PresetName(Preset) + "."));
				}
				return Finish(Note("Unknown tester action.", false));
			}

			// ----------------------------------------------------------------- operators
			if (Head == "op")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				const int Index = State.SelOperator;
				if (Sub == "sel")
				{
					State.SelOperator = std::clamp(ToInt(Parts, 2), 0, static_cast<int>(Roster().size()) - 1);
					State.Page = "op";
					return Finish(Note(""));
				}
				if (Sub == "prev" || Sub == "next")
				{
					State.SelOperator = NextOwned(Profile, Index, Sub == "next" ? 1 : -1);
					return Finish(Note(""));
				}
				if (Sub == "lvl" || Sub == "lvld")
				{
					const FActionResult R = Profile.TryLevelUp(Index, Sub == "lvld");
					if (R.bOk)
					{
						NotifyQuest(Profile, EQuestEvent::LevelUp);
					}
					return Finish(Result(R));
				}
				if (Sub == "gate")
				{
					return Finish(Result(Profile.TryPayHeartGate(Index)));
				}
				if (Sub == "star")
				{
					return Finish(Result(Profile.TryUpgradeStar(Index)));
				}
				if (Sub == "slot")
				{
					State.SelSlot = std::clamp(ToInt(Parts, 2), 0, static_cast<int>(ESlot::Count) - 1);
					State.SelItem = -1;
					State.ListOffset = 0;
					State.Page = "op:equip";
					return Finish(Note(""));
				}
				if (Sub == "item")
				{
					State.SelItem = ToInt(Parts, 2, -1);
					return Finish(Note(""));
				}
				if (Sub == "pg")
				{
					State.ListOffset = std::max(0, State.ListOffset + ToInt(Parts, 2) * 12);
					return Finish(Note(""));
				}
				if (Sub == "equip" || Sub == "unequip" || Sub == "upg")
				{
					FEquipment* Item = Profile.FindEquipment(State.SelItem);
					if (!Item)
					{
						return Finish(Note("Select an item first.", false));
					}
					if (Sub == "equip")
					{
						const FActionResult R = Profile.Equip(Item->Id, Index);
						if (R.bOk)
						{
							NotifyQuest(Profile, EQuestEvent::EquipChange);
						}
						return Finish(Result(R));
					}
					if (Sub == "unequip")
					{
						const FActionResult R = Profile.Unequip(Item->Id);
						if (R.bOk)
						{
							NotifyQuest(Profile, EQuestEvent::EquipChange);
						}
						return Finish(Result(R));
					}
					return Finish(Result(Profile.UpgradeEquipment(Item->Id)));
				}
			}

			// ----------------------------------------------------------------- team
			if (Head == "team")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "cycle")
				{
					const int Slot = std::clamp(ToInt(Parts, 2), 0, 2);
					int Candidate = Profile.Team[Slot];
					for (int Step = 0; Step < static_cast<int>(Roster().size()); ++Step)
					{
						Candidate = NextOwned(Profile, Candidate < 0 ? 0 : Candidate, 1);
						if (Candidate != Profile.Team[(Slot + 1) % 3] && Candidate != Profile.Team[(Slot + 2) % 3])
						{
							break;
						}
					}
					if (Candidate == Profile.Team[(Slot + 1) % 3] || Candidate == Profile.Team[(Slot + 2) % 3])
					{
						return Finish(Note("Every operator you own is already in the squad.", false));
					}
					return Finish(Result(Profile.SetTeamSlot(Slot, Candidate)));
				}
				if (Sub == "swap")
				{
					const int Slot = std::clamp(ToInt(Parts, 2), 0, 1);
					std::swap(Profile.Team[Slot], Profile.Team[Slot + 1]);
					return Finish(Note("Swapped slots " + std::to_string(Slot + 1) + " and " + std::to_string(Slot + 2) + "."));
				}
				if (Sub == "rot")
				{
					const int First = Profile.Team[0];
					Profile.Team[0] = Profile.Team[1];
					Profile.Team[1] = Profile.Team[2];
					Profile.Team[2] = First;
					return Finish(Note("Team order rotated."));
				}
				if (Sub == "nx")
				{
					int& Stat = Parts.size() > 2 && Parts[2] == "to" ? Meta.NarratorExchangeTo : Meta.NarratorExchangeFrom;
					Stat = (Stat + 1) % 5;
					if (Meta.NarratorExchangeFrom == Meta.NarratorExchangeTo)
					{
						Stat = (Stat + 1) % 5;
					}
					return Finish(Note(std::string("Global Exchange preset: from ") + StatName(Meta.NarratorExchangeFrom) + " to " + StatName(Meta.NarratorExchangeTo) + "."));
				}
				if (Sub == "default")
				{
					const char* Defaults[3] = {"Luna", "Judge", "Narrator"};
					for (int Slot = 0; Slot < 3; ++Slot)
					{
						const int Index = FindOperator(Defaults[Slot]);
						Profile.Team[Slot] = Profile.Owns(Index) ? Index : -1;
					}
					return Finish(Note("Squad set to Luna, Judge and Narrator (those you own)."));
				}
			}

			// ----------------------------------------------------------------- lobby
			if (Head == "q")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "all")
				{
					return Finish(Result(ClaimAllQuests(Profile)));
				}
				if (Sub == "tab")
				{
					State.QuestTab = std::clamp(ToInt(Parts, 2), 0, static_cast<int>(EQuestList::Count) - 1);
					return Finish(Note(""));
				}
				EQuestList List = EQuestList::Daily;
				if (Sub == "w") List = EQuestList::Weekly;
				else if (Sub == "s") List = EQuestList::Story;
				else if (Sub == "m") List = EQuestList::Milestone;
				return Finish(Result(ClaimQuest(Profile, List, ToInt(Parts, 2))));
			}
			if (Head == "mail")
			{
				if (Parts.size() > 1 && Parts[1] == "all")
				{
					return Finish(Result(ClaimAllMail(Profile)));
				}
				if (Parts.size() > 1 && Parts[1] == "del")
				{
					return Finish(Note("Deleted " + std::to_string(DeleteClaimedMail(Profile)) + " claimed mails."));
				}
				return Finish(Result(ClaimMail(Profile, ToInt(Parts, 1))));
			}
			if (Head == "login")
			{
				return Finish(Result(ClaimLogin(Profile)));
			}
			if (Head == "pass")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "pg")
				{
					State.PassPage = std::clamp(State.PassPage + ToInt(Parts, 2), 0, 4);
					return Finish(Note(""));
				}
				if (Sub == "all")
				{
					return Finish(Result(ClaimAllSeason(Profile)));
				}
				return Finish(Result(ClaimSeasonTier(Profile, ToInt(Parts, 2))));
			}
			if (Head == "pref")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "hint")
				{
					Meta.Settings.bShowControlsHint = !Meta.Settings.bShowControlsHint;
				}
				else if (Sub == "sens")
				{
					Meta.Settings.MouseSensitivity = Meta.Settings.MouseSensitivity >= 10 ? 1 : Meta.Settings.MouseSensitivity + 1;
				}
				else if (Sub == "lobby")
				{
					const int From = Meta.Settings.LobbyOperator >= 0 ? Meta.Settings.LobbyOperator : Profile.Team[0];
					Meta.Settings.LobbyOperator = NextOwned(Profile, From, 1);
				}
				else if (Sub == "name")
				{
					static const char* Names[4] = {"Commander", "Captain", "Chief", "Nova"};
					int Position = 0;
					for (int Index = 0; Index < 4; ++Index)
					{
						Position = Meta.Name == Names[Index] ? Index : Position;
					}
					Meta.Name = Names[(Position + 1) % 4];
				}
				return Finish(Note("Preference changed."));
			}
			if (Head == "demo")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "currency")
				{
					Profile.GrantDemoCurrency();
					return Finish(Note("Granted demo currency."));
				}
				if (Sub == "day")
				{
					Profile.AdvanceDay();
					return Finish(Note("It is now day " + std::to_string(Profile.Day) + ". Daily quests, tickets, tokens and arena attempts reset; supply mail arrived."));
				}
				if (Sub == "xp")
				{
					AddCommanderXp(Profile, 1000);
					return Finish(Note("Commander level is now " + std::to_string(CommanderLevel(Profile)) + "."));
				}
				if (Sub == "season")
				{
					Meta.SeasonXp += 500;
					return Finish(Note("Season tier is now " + std::to_string(SeasonTier(Profile)) + "."));
				}
				if (Sub == "hours")
				{
					AdvanceWorld(Profile, 12.0);
					return Finish(Note("12 Outer World hours passed."));
				}
				if (Sub == "reset")
				{
					Profile.ResetForAccount();
					State = FUiState();
					State.Status = "Profile reset.";
					return Note(State.Status);
				}
			}

			if (Head == "acct")
			{
				if (Parts.size() > 1 && Parts[1] == "signout")
				{
					Out.bSignOut = true;
					Out.Message = "Signing out...";
					return Finish(Out);
				}
			}

			// ----------------------------------------------------------------- shop
			if (Head == "shop")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				return Finish(Result(Sub == "arm" ? BuyArmoury(Profile, ToInt(Parts, 2)) : BuyAlley(Profile, ToInt(Parts, 2))));
			}
			if (Head == "trade")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				const int Count = static_cast<int>(TradeCurrencies().size());
				if (Sub == "from")
				{
					State.TradeFrom = (State.TradeFrom + 1) % Count;
					return Finish(Note(""));
				}
				if (Sub == "to")
				{
					State.TradeTo = (State.TradeTo + 1) % Count;
					return Finish(Note(""));
				}
				if (Sub == "amt")
				{
					State.TradeAmount = (State.TradeAmount + 1) % 5;
					return Finish(Note(""));
				}
				if (Sub == "swap")
				{
					std::swap(State.TradeFrom, State.TradeTo);
					return Finish(Note(""));
				}
				static const long long Amounts[5] = {1, 10, 100, 1000, 10000};
				return Finish(Result(Trade(Profile, TradeCurrencies()[static_cast<size_t>(State.TradeFrom % Count)], TradeCurrencies()[static_cast<size_t>(State.TradeTo % Count)],
					Amounts[std::clamp(State.TradeAmount, 0, 4)])));
			}
			if (Head == "iap")
			{
				return Finish(Result(SimulatePurchase(Profile, ToInt(Parts, 1))));
			}

			// ----------------------------------------------------------------- revolution
			if (Head == "world")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "collect")
				{
					return Finish(Result(CollectWorld(Profile)));
				}
				return Finish(Result(BuildOrUpgrade(Profile, ToInt(Parts, 2))));
			}
			if (Head == "tower")
			{
				const int Mode = std::clamp(ToInt(Parts, 2), 0, 1);
				const int StageId = TowerStageId(Mode, TowerNextFloor(Profile, Mode));
				const FActionResult Can = CanStartStage(Profile, StageId);
				if (!Can.bOk)
				{
					return Finish(Result(Can));
				}
				Out.bStartStage = true;
				Out.StageId = StageId;
				Out.Message = "Starting " + StageName(StageId) + ".";
				return Finish(Out);
			}
			if (Head == "sub")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "sel")
				{
					State.SelSubStory = std::clamp(ToInt(Parts, 2), 0, SubStoryCount - 1);
					State.Page = "sub";
					return Finish(Note(""));
				}
				const int StageId = SubStoryStageId(State.SelSubStory, ToInt(Parts, 2));
				const FActionResult Can = CanStartStage(Profile, StageId);
				if (!Can.bOk)
				{
					return Finish(Result(Can));
				}
				Out.bStartStage = true;
				Out.StageId = StageId;
				Out.Message = "Starting " + StageName(StageId) + ".";
				return Finish(Out);
			}

			// ----------------------------------------------------------------- campaign
			if (Head == "camp")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "ch")
				{
					const int Chapter = ToInt(Parts, 2);
					if (Chapter < 1 || Chapter > CampaignChapters)
					{
						return Finish(Note("That chapter does not exist.", false));
					}
					if (!ChapterIsOpen(Profile, Chapter))
					{
						return Finish(Note("Chapter " + std::to_string(Chapter) + " is locked - clear Chapter " + std::to_string(Chapter - 1) + "'s boss (level 8) first.", false));
					}
					State.SelChapter = Chapter;
					State.Page = "chapter";
					return Finish(Note(""));
				}
				if (Sub == "map")
				{
					const std::string Map = Parts.size() > 2 ? Parts[2] : "";
					if (Map != "NeoTokyo")
					{
						return Finish(Note("That level does not exist.", false));
					}
					Out.FreeRoamMap = Map;
					Out.Message = "Opening Neo Tokyo.";
					return Finish(Out);
				}
				const int StageId = ToInt(Parts, 2);   // the campaign stage id: 1..104 counted through all chapters
				if (StageId < 1 || StageId > TotalCampaignLevels)
				{
					return Finish(Note("That level does not exist.", false));
				}
				if (Sub == "sweep")
				{
					return Finish(Result(QuickClear(Profile, StageId, std::max(1, ToInt(Parts, 3)))));
				}
				const FActionResult Can = CanStartStage(Profile, StageId);
				if (!Can.bOk)
				{
					return Finish(Result(Can));
				}
				Out.bStartStage = true;
				Out.StageId = StageId;
				Out.Message = "Starting " + StageName(StageId) + ".";
				return Finish(Out);
			}

			// ----------------------------------------------------------------- summon
			if (Head == "sum")
			{
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Sub == "banner")
				{
					State.SelBanner = std::clamp(ToInt(Parts, 2), 0, static_cast<int>(Profile.Banners.size()) - 1);
					return Finish(Note(""));
				}
				if (Sub == "done")
				{
					State.SummonOps.clear();
					return Finish(Note(""));
				}
				if (Sub == "show")
				{
					return Finish(Note(OpName(ToInt(Parts, 2)) + " - " + StarsText(Roster()[static_cast<size_t>(ToInt(Parts, 2))].Stars) + "."));
				}
				if (Sub == "tut")
				{
					const FSummonOutcome Outcome = Profile.TutorialSummon();
					if (Outcome.bOk)
					{
						RecordSummon(Profile, State, Outcome, 1, 0);
					}
					return Finish(Note(Outcome.Message, Outcome.bOk));
				}
				if (Sub == "mileage")
				{
					const FSummonOutcome Outcome = Profile.RedeemMileage(State.SelBanner);
					if (Outcome.bOk)
					{
						RecordSummon(Profile, State, Outcome, 1, 0);
					}
					return Finish(Note(Outcome.Message, Outcome.bOk));
				}
				int Count = ToInt(Parts, 2, 1);
				int Pay = ToInt(Parts, 3);
				if (Sub == "again")
				{
					Count = State.SummonCount == 10 ? 10 : 1;
					Pay = State.SummonPay;
				}
				const FSummonOutcome Outcome = Profile.Summon(State.SelBanner, Count, Pay == 0 ? EPay::Ticket : EPay::Diamonds);
				if (Outcome.bOk)
				{
					RecordSummon(Profile, State, Outcome, Count, Pay);
				}
				return Finish(Note(Outcome.Message, Outcome.bOk));
			}
			return Finish(Note("Unknown action: " + Id, false));
		}
	}
}
