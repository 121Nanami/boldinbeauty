// Boldinbeauty demo — hub pages added on 2026-10-07 (see BIBPagesGacha.h). Every amount is PLACEHOLDER.
#include "BIBPagesGacha.h"

#include "BIBArena.h"
#include "BIBConfig.h"
#include "BIBElement.h"
#include "BIBEvents.h"
#include "BIBInventory.h"
#include "BIBMetaOps.h"
#include "BIBTester.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace BIB
{
	namespace Pages
	{
		namespace Gacha
		{
			namespace
			{
				using namespace Meta;

				struct FB
				{
					FPage& Page;
					void Line(const std::string& Text) { Page.Lines.push_back(Text); }
					void Tile(const std::string& Id, const std::string& Label, const std::string& Icon = "", const std::string& Sub = "",
						bool bEnabled = true, bool bHighlight = false, bool bBadge = false)
					{
						if (static_cast<int>(Page.Grid.size()) < MaxGrid)
						{
							Page.Grid.push_back({Id, Label, Sub, Icon, bEnabled, bHighlight, bBadge});
						}
					}
					void Button(const std::string& Id, const std::string& Label, const std::string& Sub = "", bool bEnabled = true, bool bHighlight = false)
					{
						if (static_cast<int>(Page.Bar.size()) < MaxBar)
						{
							Page.Bar.push_back({Id, Label, Sub, "", bEnabled, bHighlight, false});
						}
					}
				};

				std::vector<std::string> Split(const std::string& Text)
				{
					std::vector<std::string> Parts;
					std::string Current;
					for (char Character : Text)
					{
						if (Character == ':')
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

				int ToInt(const std::vector<std::string>& Parts, size_t Index)
				{
					return Index < Parts.size() && !Parts[Index].empty() ? std::atoi(Parts[Index].c_str()) : -1;
				}

				FPerform Note(const std::string& Message, bool bOk = true)
				{
					FPerform Out;
					Out.Message = Message;
					Out.bOk = bOk;
					return Out;
				}

				FPerform Result(const FActionResult& Action)
				{
					return Note(Action.Message, Action.bOk);
				}

				std::string OpName(int RosterIndex)
				{
					return RosterIndex >= 0 && RosterIndex < static_cast<int>(Roster().size()) ? Roster()[static_cast<size_t>(RosterIndex)].Name : "-";
				}

				const char* CurrencyUse(ECurrency Currency)
				{
					switch (Currency)
					{
					case ECurrency::Gold: return "level-ups, Attunement, armour, Outer World buildings";
					case ECurrency::Diamonds: return "summons (300 a pull), Charge refills, Back Alley, covering missing Gold / Bytes";
					case ECurrency::Bytes: return "level-ups, equipment upgrades, Outer World buildings";
					case ECurrency::AngelHearts: return "the Angel-heart gate every 20 levels";
					case ECurrency::RegularTicket: return "Standard banner summons";
					case ECurrency::SpecialTicket: return "Featured banner summons";
					case ECurrency::DailyTicket: return "one free Daily-banner summon a day (expires)";
					case ECurrency::Mileage: return "1 per summon: a featured 5* at 250, a featured 6* at 2,500";
					case ECurrency::ArcadeToken: return "one arcade play each";
					case ECurrency::GangCoin: return "the gang shop";
					case ECurrency::Charge: return "campaign replays (first clears are free)";
					case ECurrency::ElementCore: return "Element Attunement (more damage on advantaged hits)";
					case ECurrency::EventToken: return "the weekly event shop";
					case ECurrency::ArkMedal: return "the Ark shop (arena fights, tower floors)";
					case ECurrency::Count: break;
					}
					return "";
				}

				std::string MissionSub(const Events::FMission& M)
				{
					if (M.bLocked)
					{
						return "unlocks on day " + std::to_string(M.UnlockDay);
					}
					return std::to_string(M.Progress) + "/" + std::to_string(M.Target) + " | " + (M.bClaimed ? "claimed" : RewardText(M.Reward));
				}

				int ClaimableRookie(const FProfile& Profile)
				{
					int Count = 0;
					if (Events::RookieActive(Profile))
					{
						for (int Index = 0; Index < RookieMissionCount; ++Index)
						{
							const Events::FMission M = Events::RookieMission(Profile, Index);
							Count += !M.bLocked && M.bComplete && !M.bClaimed ? 1 : 0;
						}
					}
					return Count;
				}

				int ClaimableMissions(const FProfile& Profile)
				{
					int Count = 0;
					for (int Index = 0; Index < EventMissionCount; ++Index)
					{
						const Events::FMission M = Events::Mission(Profile, Index);
						Count += M.bComplete && !M.bClaimed ? 1 : 0;
					}
					return Count;
				}

				std::string Decimal(double Value)
				{
					char Buffer[32];
					std::snprintf(Buffer, sizeof(Buffer), "%.1f", Value);
					return Buffer;
				}

				std::string PercentText(double Fraction)
				{
					return std::to_string(static_cast<int>(Fraction * 100.0 + 0.5)) + "%";
				}
			}

			// ===================================================================== shared lines
			std::string ElementTag(int RosterIndex)
			{
				return std::string("[") + Element::Name(Element::OfOperator(RosterIndex)) + "]";
			}

			std::string ChargeLine(const FProfile& Profile)
			{
				std::string Text = "Charge " + Num(Profile.Get(ECurrency::Charge)) + " / " + std::to_string(Inventory::ChargeCap);
				const int64_t Next = Inventory::SecondsToNextCharge(Profile);
				if (Next >= 0)
				{
					Text += " (+1 in " + Duration(Next) + ")";
				}
				Text += ". Replays cost 10 / 15 / 20 Charge (chapters 1-4 / 5-9 / 10-13); first clears are free.";
				return Text;
			}

			void OperatorElement(const FProfile& Profile, int RosterIndex, FPage& Page)
			{
				FB B{Page};
				const EElement E = Element::OfOperator(RosterIndex);
				const int Level = Element::Attunement(Profile, RosterIndex);
				B.Line("Element: " + Element::Describe(E) + ".  Attunement " + std::to_string(Level) + " / " + std::to_string(Element::MaxAttunement) +
					": +" + PercentText(Element::AdvantageBonusOf(Profile, RosterIndex)) + " damage against " + Element::Name(Element::StrongAgainst(E)) + " enemies.");
				if (!Profile.Owns(RosterIndex))
				{
					return;
				}
				if (Level >= Element::MaxAttunement)
				{
					B.Tile("gx:attune", "Attunement", Element::Icon(E), "maximum", false, true);
					return;
				}
				const int Cores = Element::AttunementCoresFor(Profile, RosterIndex, Level + 1);
				const int Gold = Element::AttunementGold(Level + 1);
				const bool bCan = Profile.Get(ECurrency::ElementCore) >= Cores && Profile.Get(ECurrency::Gold) >= Gold;
				B.Tile("gx:attune", "Attune to " + std::to_string(Level + 1), Element::Icon(E), Num(Cores) + " cores, " + Num(Gold) + " Gold", bCan,
					Events::SurgeElement(Profile) == E);
			}

			std::string StageElementLine(const FProfile& Profile, int StageId)
			{
				const EElement Enemy = Element::OfStage(StageId, 0);
				if (Enemy == EElement::None)
				{
					return "Enemies: no element.";
				}
				std::string Text = std::string("Enemies: ") + Element::Name(Enemy);
				const EElement Second = Element::OfStage(StageId, 1);
				if (Second != Enemy)
				{
					Text += " and " + std::string(Element::Name(Second)) + " (alternating sections)";
				}
				Text += ".  Bring " + std::string(Element::Name(Element::CounterOf(StageId))) + " operators for +" + PercentText(Element::AdvantageBonus) + " damage or more.";
				int Counters = 0;
				for (int Slot = 0; Slot < 3; ++Slot)
				{
					Counters += Profile.Team[Slot] >= 0 && Element::HasAdvantage(Element::OfOperator(Profile.Team[Slot]), Enemy) ? 1 : 0;
				}
				Text += "  Your squad: " + std::to_string(Counters) + " with the advantage.";
				return Text;
			}

			int ClaimableEvents(const FProfile& Profile)
			{
				return ClaimableRookie(Profile) + ClaimableMissions(Profile) + (CanClaimLogin(Profile) ? 1 : 0) + Events::RoadClaimable(Profile);
			}

			long long DiamondCost(const FProfile& Profile, const std::string& Id)
			{
				if (Id == "gx:refill" && Profile.Meta.ChargeRefillsToday < Inventory::ChargeRefillsPerDay)
				{
					return Inventory::ChargeRefillCost(Profile.Meta.ChargeRefillsToday);
				}
				return 0;
			}

			// ===================================================================== lobby root tiles
			void LobbyTiles(FProfile& Profile, FPage& Page)
			{
				FB B{Page};
				const bool bItems = Inventory::TotalItems(Profile) > 0;
				B.Tile("go:inventory", "Inventory", "inventory", Num(Profile.Get(ECurrency::Charge)) + " Charge | " + std::to_string(Inventory::TotalItems(Profile)) + " items",
					true, false, bItems);
				B.Tile("go:elements", "Elements", Element::Icon(Events::SurgeElement(Profile)),
					std::string(Element::Name(Events::SurgeElement(Profile))) + " Surge this week");
				if (Events::RookieActive(Profile))
				{
					B.Tile("go:rookie", "Rookie week", "star", "day " + std::to_string(Events::RookieDay(Profile)) + " of 7", true, true, ClaimableRookie(Profile) > 0);
				}
				// The Events tile already exists (BIBPagesExtra.cpp): give it the new badge.
				for (FPageAction& Action : Page.Grid)
				{
					if (Action.Id == "go:events")
					{
						Action.bBadge = ClaimableEvents(Profile) > 0;
						Action.Sub = std::string(Events::ThemeName(Events::Theme(Profile))) + (Events::OvertimeActive(Profile) ? " | Overtime!" : "");
					}
				}
			}

			// ===================================================================== parents
			std::string ParentPage(const std::string& Page, bool& bFound)
			{
				bFound = true;
				if (Page == "rookie" || Page == "event-ops" || Page == "event-shop" || Page == "ticket-road") return "events";
				if (Page == "ark-shop") return "ark";
				if (Page == "inventory" || Page == "elements" || Page == "events") return "";
				bFound = false;
				return "";
			}

			// ===================================================================== lobby pages
			bool BuildLobbyPage(FProfile& Profile, const FUiState& State, FPage& Page)
			{
				FB B{Page};
				const std::string& P = State.Page;
				const FMeta& M = Profile.Meta;
				Events::Refresh(Profile);

				if (P == "inventory")
				{
					Page.Title = "Inventory";
					Page.Subtitle = "Currencies and items. Boosts pay hours of your Outer World's production at once.";
					Page.bFullScreen = true;
					B.Line(ChargeLine(Profile));
					B.Line("Outer World per hour: " + Num(static_cast<long long>(Inventory::AfkRatePerHour(Profile, 0))) + " Gold, " +
						Num(static_cast<long long>(Inventory::AfkRatePerHour(Profile, 1))) + " Bytes, " +
						Decimal(Inventory::AfkRatePerHour(Profile, 2)) + " Angel hearts (campaign bonus +" +
						PercentText(WorldCampaignMultiplier(Profile) - 1.0) + ")" + (Events::OvertimeActive(Profile) ? ". Overtime weekend: boosts +50%." : "."));
					for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
					{
						const ECurrency Currency = static_cast<ECurrency>(Index);
						B.Line(std::string(CurrencyName(Currency)) + ": " + Num(Profile.Get(Currency)) + "  -  " + CurrencyUse(Currency));
					}
					if (M.MonthlyDays > 0)
					{
						B.Line("Monthly supply: 100 Diamonds by mail for " + std::to_string(M.MonthlyDays) + " more days.");
					}
					for (int Index = 0; Index < ItemCount; ++Index)
					{
						const EItem Item = static_cast<EItem>(Index);
						const int Have = Inventory::Count(Profile, Item);
						const std::string Value = Inventory::IsBoost(Item) ? "+" + Num(Inventory::BoostValue(Profile, Item)) : "+" + std::to_string(Inventory::ChargePackAmount) + " Charge";
						B.Tile("gx:use:" + std::to_string(Index), Inventory::ItemName(Item), Inventory::ItemIcon(Item), "x" + std::to_string(Have) + " | use: " + Value, Have > 0, false, false);
					}
					const bool bRefill = M.ChargeRefillsToday < Inventory::ChargeRefillsPerDay;
					B.Button("gx:refill", "Refill Charge", bRefill ? "+60 for " + std::to_string(Inventory::ChargeRefillCost(M.ChargeRefillsToday)) + " Diamonds" : "none left today", bRefill);
					B.Button("gx:useall", "Use all boosts", "", Inventory::TotalItems(Profile) - Inventory::Count(Profile, EItem::ChargePack) > 0);
					B.Button("go:elements", "Element guide");
					if (Tester::Enabled())
					{
						B.Button("gx:tst:items", "Tester: +5 items", "every boost and pack", true, true);
						B.Button("gx:tst:charge", "Tester: Charge to 240", "", true, true);
					}
					return true;
				}

				if (P == "elements")
				{
					Page.Title = "Elements";
					Page.Subtitle = "Hit an enemy your element is strong against for +" + PercentText(Element::AdvantageBonus) + " damage, more with Attunement. No penalty otherwise.";
					Page.bFullScreen = true;
					B.Line("Rings: Fire > Nature > Volt > Water > Fire,   Halo > Dark > Glitch > Magic > Halo.");
					B.Line("Attunement (Operator tab): up to level " + std::to_string(Element::MaxAttunement) + ", +" + PercentText(Element::AttunementBonusPerLevel) +
						" advantage damage per level, paid with Element cores and Gold.");
					B.Line("Element Surge this week: " + std::string(Element::Name(Events::SurgeElement(Profile))) + " - clears with one in your squad pay Element cores; its Attunement costs " +
						std::to_string(Events::SurgeDiscountPercent) + "% fewer cores. Volt, Halo and Glitch are new elements (PROPOSED); every operator's element is PROPOSED.");
					for (int Index = 0; Index < ElementCount; ++Index)
					{
						const EElement E = static_cast<EElement>(Index);
						std::string Members;
						for (int Op = 0; Op < static_cast<int>(Roster().size()); ++Op)
						{
							if (Element::OfOperator(Op) == E)
							{
								Members += (Members.empty() ? "" : ", ") + OpName(Op) + (Profile.Owns(Op) ? " (A" + std::to_string(Element::Attunement(Profile, Op)) + ")" : "");
							}
						}
						B.Tile("gx:el:" + std::to_string(Index), Element::Name(E), Element::Icon(E), std::string("beats ") + Element::Name(Element::StrongAgainst(E)) + " | " + Members,
							true, Events::SurgeElement(Profile) == E);
					}
					return true;
				}

				if (P == "events")
				{
					Page.Title = "Events";
					Page.Subtitle = "Week " + std::to_string(Events::WeekIndex(Profile) + 1) + ", day " + std::to_string(Events::DayOfWeek(Profile)) + " of 7 - " +
						std::to_string(Events::DaysLeftInWeek(Profile)) + " day(s) left this week.";
					Page.bFullScreen = true;
					const Events::ETheme Theme = Events::Theme(Profile);
					B.Line(std::string("Weekly operation - ") + Events::ThemeName(Theme) + ": " + Events::ThemeBlurb(Theme));
					B.Line(std::string("Element Surge - ") + Element::Name(Events::SurgeElement(Profile)) + ": campaign clears with a " + Element::Name(Events::SurgeElement(Profile)) +
						" operator in the squad pay " + std::to_string(Events::SurgeCoresPerClear) + " Element cores (" + std::to_string(Events::SurgeCoresFirstClear) + " on a first clear).");
					B.Line(std::string("Overtime weekend (days 6-7): ") + (Events::OvertimeActive(Profile) ? "ON NOW - the Outer World produces twice as fast and boosts pay 50% more."
						: "starts in " + std::to_string(std::max(0, 6 - Events::DayOfWeek(Profile))) + " day(s): double Outer World production, boosts +50%."));
					if (Events::RookieActive(Profile))
					{
						const FReward Total = Events::RookieTotal();
						B.Line("Rookie week - day " + std::to_string(Events::RookieDay(Profile)) + " of 7: " + std::to_string(Total.RegularTickets) + " Regular tickets, " +
							std::to_string(Total.SpecialTickets) + " Special tickets and " + Num(Total.Diamonds) + " Diamonds in beginner missions.");
						B.Tile("go:rookie", "Rookie week", "star", "day " + std::to_string(Events::RookieDay(Profile)) + " of 7", true, true, ClaimableRookie(Profile) > 0);
					}
					B.Tile("go:event-ops", Events::ThemeName(Theme), Theme == Events::ETheme::ArcadeFever ? "arcade" : "event",
						std::to_string(ClaimableMissions(Profile)) + " to claim | " + Num(Profile.Get(ECurrency::EventToken)) + " tokens", true, false, ClaimableMissions(Profile) > 0);
					B.Tile("go:event-shop", "Event shop", "event_token", Num(Profile.Get(ECurrency::EventToken)) + " Event tokens");
					const int NextDay = (M.LoginStreak % 7) + 1;
					B.Tile("login", "Login bonus", "event", CanClaimLogin(Profile) ? "day " + std::to_string(NextDay) + ": " + RewardText(LoginReward(NextDay)) : "claimed today",
						CanClaimLogin(Profile), false, CanClaimLogin(Profile));
					B.Tile("go:elements", std::string(Element::Name(Events::SurgeElement(Profile))) + " Surge", Element::Icon(Events::SurgeElement(Profile)), "Element cores this week");
					B.Tile("gx:overtime", "Overtime weekend", "b_shelter", Events::OvertimeActive(Profile) ? "x2 Outer World now" : "days 6-7", true, Events::OvertimeActive(Profile));
					const Events::FRoad Road = Events::Road(Profile);
					B.Tile("go:ticket-road", "Ticket road", "ticket_special", Road.bOpen ? "day " + std::to_string(Road.TotalDays) + " | " + Num(Road.TicketsEarned) + " tickets so far"
						: "opens after Chapter 13", true, Road.bOpen, Events::RoadClaimable(Profile) > 0);
					return true;
				}

				if (P == "ticket-road")
				{
					const Events::FRoad Road = Events::Road(Profile);
					Page.Title = "Ticket road";
					Page.Subtitle = "A steady ticket income for the long hunt for Secrets (6*): about " + Num(static_cast<long long>(Events::RoadTicketsPer30Days() + 0.5)) +
						" tickets every 30 days if you claim daily. PLACEHOLDER amounts.";
					Page.bFullScreen = true;
					if (!Road.bOpen)
					{
						B.Line("Opens when the campaign is finished (defeat the Chapter 13 boss). Campaign progress: " + std::to_string(std::min(Profile.StagesCleared, TotalCampaignLevels)) +
							" / " + std::to_string(TotalCampaignLevels) + " levels.");
					}
					B.Line("Daily stipend: claim " + std::to_string(Events::RoadDailyQuests) + " daily quests, then take " + std::to_string(Events::RoadDailySpecial) +
						" Special tickets (once a day). Today: " + std::to_string(std::min(Road.DailyQuestsDone, Events::RoadDailyQuests)) + " / " + std::to_string(Events::RoadDailyQuests) +
						(Road.bDailyClaimed ? " - taken." : "."));
					B.Line("Weekly bonus: " + std::to_string(Events::RoadWeeklyDays) + " stipends this week pay " + std::to_string(Events::RoadWeeklyRegular) +
						" Regular tickets. This week: " + std::to_string(std::min(Road.WeekDays, Events::RoadWeeklyDays)) + " / " + std::to_string(Events::RoadWeeklyDays) +
						(Road.bWeeklyClaimed ? " - claimed." : "") + " Resets in " + std::to_string(Events::DaysLeftInWeek(Profile)) + " day(s).");
					B.Line("Road milestones: every " + std::to_string(Events::RoadMilestoneDays) + " stipend days pay " + std::to_string(Events::RoadMilestoneSpecial) +
						" Special tickets, without end. Road day " + std::to_string(Road.TotalDays) + ", next milestone at day " + std::to_string(Road.NextMilestoneDay) +
						" (" + std::to_string(Road.MilestonesClaimed) + " reached).");
					int SecretsOwned = 0;
					int SecretsTotal = 0;
					for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
					{
						if (Roster()[static_cast<size_t>(Index)].Stars == 6)
						{
							++SecretsTotal;
							SecretsOwned += Profile.Owns(Index) ? 1 : 0;
						}
					}
					B.Line("Secrets owned: " + std::to_string(SecretsOwned) + " / " + std::to_string(SecretsTotal) + ". Mileage " + Num(Profile.Get(ECurrency::Mileage)) + " / " +
						Num(Config::MileageFeatured6Cost) + " for the featured Secret. Tickets from the road so far: " + Num(Road.TicketsEarned) + ".");
					B.Tile("gx:road:daily", "Daily stipend", "ticket_special", Road.bDailyClaimed ? "taken today" : std::to_string(Events::RoadDailySpecial) + " Special tickets",
						Road.bDailyReady, Road.bDailyClaimed, Road.bDailyReady);
					B.Tile("gx:road:weekly", "Weekly bonus", "ticket_regular", std::to_string(std::min(Road.WeekDays, Events::RoadWeeklyDays)) + " / " + std::to_string(Events::RoadWeeklyDays) +
						" days | " + std::to_string(Events::RoadWeeklyRegular) + " Regular", Road.bWeeklyReady, Road.bWeeklyClaimed, Road.bWeeklyReady);
					B.Tile("gx:road:milestone", "Milestone " + std::to_string(Road.MilestonesClaimed + 1), "star", "day " + std::to_string(Road.TotalDays) + " / " +
						std::to_string(Road.NextMilestoneDay) + " | " + std::to_string(Events::RoadMilestoneSpecial) + " Special", Road.bMilestoneReady, false, Road.bMilestoneReady);
					B.Button("gx:road", "Claim", "", Events::RoadClaimable(Profile) > 0);
					B.Button("go:quests", "Daily quests");
					return true;
				}

				if (P == "rookie")
				{
					Page.Title = "Rookie week";
					Page.Subtitle = Events::RookieActive(Profile) ? "Day " + std::to_string(Events::RookieDay(Profile)) + " of 7 - three new missions unlock every day."
						: "The Rookie week is over (it runs for a new account's first 7 days).";
					Page.bFullScreen = true;
					for (int Index = 0; Index < RookieMissionCount; ++Index)
					{
						const Events::FMission Mission = Events::RookieMission(Profile, Index);
						B.Tile("gx:rookie:" + std::to_string(Index), "Day " + std::to_string(Mission.UnlockDay) + ": " + Mission.Title,
							Mission.bLocked ? "lock" : (Mission.bClaimed ? "check" : "quest"), MissionSub(Mission),
							Events::RookieActive(Profile) && !Mission.bLocked && Mission.bComplete && !Mission.bClaimed, Mission.bClaimed,
							Events::RookieActive(Profile) && !Mission.bLocked && Mission.bComplete && !Mission.bClaimed);
					}
					B.Button("gx:rookie-all", "Claim all", "", ClaimableRookie(Profile) > 0);
					return true;
				}

				if (P == "event-ops")
				{
					const Events::ETheme Theme = Events::Theme(Profile);
					Page.Title = Events::ThemeName(Theme);
					Page.Subtitle = Events::ThemeBlurb(Theme) + " Resets in " + std::to_string(Events::DaysLeftInWeek(Profile)) + " day(s).";
					Page.bFullScreen = true;
					B.Line(Num(Profile.Get(ECurrency::EventToken)) + " Event tokens. Missions pay " + Num(Events::TokensPerWeek()) + " a week; the event shop restocks weekly.");
					for (int Index = 0; Index < EventMissionCount; ++Index)
					{
						const Events::FMission Mission = Events::Mission(Profile, Index);
						B.Tile("gx:op:" + std::to_string(Index), Mission.Title, Mission.bClaimed ? "check" : "event_token", MissionSub(Mission),
							Mission.bComplete && !Mission.bClaimed, Mission.bClaimed, Mission.bComplete && !Mission.bClaimed);
					}
					B.Button("gx:op-all", "Claim all", "", ClaimableMissions(Profile) > 0);
					B.Button("go:event-shop", "Event shop");
					return true;
				}

				if (P == "event-shop")
				{
					Page.Title = "Event shop";
					Page.Subtitle = Num(Profile.Get(ECurrency::EventToken)) + " Event tokens - weekly stock, resets in " + std::to_string(Events::DaysLeftInWeek(Profile)) + " day(s).";
					Page.bFullScreen = true;
					for (int Index = 0; Index < EventShopCount; ++Index)
					{
						const Events::FShopItem& Item = Events::EventShopItem(Index);
						const int Left = Events::EventShopLeft(Profile, Index);
						B.Tile("gx:eshop:" + std::to_string(Index), Item.Name, Item.Icon, Num(Item.Price) + " tokens | " + std::to_string(Left) + " left",
							Left > 0 && Profile.Get(ECurrency::EventToken) >= Item.Price);
					}
					return true;
				}
				return false;
			}

			// ===================================================================== revolution pages
			bool BuildRevolutionPage(FProfile& Profile, const FUiState& State, FPage& Page)
			{
				FB B{Page};
				if (State.Page != "ark-shop")
				{
					return false;
				}
				Events::Refresh(Profile);
				Page.Title = "Ark shop";
				Page.Subtitle = Num(Profile.Get(ECurrency::ArkMedal)) + " Ark medals - monthly stock. Arena fights pay " + std::to_string(Events::ArkMedalsForArena(true)) +
					" (win) / " + std::to_string(Events::ArkMedalsForArena(false)) + " (loss); new tower floors pay more the higher they are.";
				Page.bFullScreen = true;
				for (int Index = 0; Index < ArkShopCount; ++Index)
				{
					const Events::FShopItem& Item = Events::ArkShopItem(Index);
					const int Left = Events::ArkShopLeft(Profile, Index);
					B.Tile("gx:ashop:" + std::to_string(Index), Item.Name, Item.Icon, Num(Item.Price) + " medals | " + std::to_string(Left) + " left",
						Left > 0 && Profile.Get(ECurrency::ArkMedal) >= Item.Price);
				}
				return true;
			}

			// ===================================================================== actions
			bool Perform(FProfile& Profile, FUiState& State, const std::string& Id, FPerform& Out)
			{
				if (Id.rfind("gx:", 0) != 0)
				{
					return false;
				}
				const std::vector<std::string> Parts = Split(Id);
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				const int Arg = ToInt(Parts, 2);
				if (Sub == "use")
				{
					Out = Arg >= 0 && Arg < ItemCount ? Result(Inventory::Use(Profile, static_cast<EItem>(Arg))) : Note("No such item.", false);
					return true;
				}
				if (Sub == "useall")
				{
					long long Uses = 0;
					for (int Index = 0; Index < static_cast<int>(EItem::ChargePack); ++Index)
					{
						const int Have = Inventory::Count(Profile, static_cast<EItem>(Index));
						if (Have > 0)
						{
							Inventory::Use(Profile, static_cast<EItem>(Index), Have);
							Uses += Have;
						}
					}
					Out = Uses > 0 ? Note("Used " + std::to_string(Uses) + " boosts.") : Note("No boosts to use.", false);
					return true;
				}
				if (Sub == "tst")
				{
					if (!Tester::Enabled())
					{
						Out = Note("Tester tools are off in this build.", false);
						return true;
					}
					const std::string What = Parts.size() > 2 ? Parts[2] : "";
					if (What == "items")
					{
						for (int Index = 0; Index < ItemCount; ++Index)
						{
							Inventory::Add(Profile, static_cast<EItem>(Index), 5);
						}
						Out = Note("Tester: +5 of every item.");
					}
					else
					{
						Profile.SetRaw(ECurrency::Charge, Inventory::ChargeCap);
						Out = Note("Tester: Charge set to " + std::to_string(Inventory::ChargeCap) + ".");
					}
					return true;
				}
				if (Sub == "inv")
				{
					// From any tab (the Campaign tab's Charge button): open Lobby > Inventory.
					State.Tab = TabLobby;
					State.Page = "inventory";
					State.ListOffset = 0;
					Out = Note("");
					return true;
				}
				if (Sub == "road")
				{
					// Ticket road (2026-10-09): any of its tiles or the Claim button claims every part that is ready.
					Out = Result(Events::ClaimRoad(Profile));
					return true;
				}
				if (Sub == "refill")
				{
					Out = Result(Inventory::RefillCharge(Profile));
					return true;
				}
				if (Sub == "attune")
				{
					Out = Result(Element::TryAttune(Profile, State.SelOperator));
					return true;
				}
				if (Sub == "overtime")
				{
					Out = Note(Events::OvertimeActive(Profile) ? "Overtime weekend is on: the Outer World (Revolution tab) produces twice as fast and boosts pay 50% more."
						: "Overtime weekend runs on days 6 and 7 of every week.");
					return true;
				}
				if (Sub == "el")
				{
					const EElement E = Element::FromIndex(Arg);
					Out = Note(Element::Describe(E) + ". " + Element::Blurb(E));
					return true;
				}
				if (Sub == "rookie")
				{
					Out = Result(Events::ClaimRookie(Profile, Arg));
					return true;
				}
				if (Sub == "rookie-all" || Sub == "op-all")
				{
					const bool bRookie = Sub == "rookie-all";
					int Claimed = 0;
					const int Count = bRookie ? RookieMissionCount : EventMissionCount;
					for (int Index = 0; Index < Count; ++Index)
					{
						Claimed += (bRookie ? Events::ClaimRookie(Profile, Index) : Events::ClaimMission(Profile, Index)).bOk ? 1 : 0;
					}
					Out = Claimed > 0 ? Note("Claimed " + std::to_string(Claimed) + " mission(s).") : Note("Nothing to claim.", false);
					return true;
				}
				if (Sub == "road")
				{
					Out = Result(Events::ClaimRoad(Profile));
					return true;
				}
				if (Sub == "op")
				{
					Out = Result(Events::ClaimMission(Profile, Arg));
					return true;
				}
				if (Sub == "eshop")
				{
					Out = Result(Events::BuyEventShop(Profile, Arg));
					return true;
				}
				if (Sub == "ashop")
				{
					Out = Result(Events::BuyArkShop(Profile, Arg));
					return true;
				}
				Out = Note("Unknown action.", false);
				return true;
			}
		}
	}
}
