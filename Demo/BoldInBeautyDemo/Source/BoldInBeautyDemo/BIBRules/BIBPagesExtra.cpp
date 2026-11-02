// Boldinbeauty demo — hub pages added on 2026-10-06 (see BIBPagesExtra.h). Every amount is PLACEHOLDER unless the owner gave it.
#include "BIBPagesExtra.h"

#include "BIBArcade.h"
#include "BIBMiniGames.h"
#include "BIBRally.h"
#include "BIBArena.h"
#include "BIBConfig.h"
#include "BIBLiveOps.h"
#include "BIBMetaOps.h"
#include "BIBOnline.h"
#include "BIBTester.h"
#include "BIBUltimateNight.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace BIB
{
	namespace Pages
	{
		namespace Extra
		{
			namespace
			{
				using namespace Meta;

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

				std::string OpName(int RosterIndex)
				{
					return RosterIndex >= 0 && RosterIndex < static_cast<int>(Roster().size()) ? Roster()[static_cast<size_t>(RosterIndex)].Name : "-";
				}

				bool IsAdmin(const FProfile& Profile)
				{
					return Profile.Account.bAdmin && Profile.Account.bSignedIn;
				}

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

				FPerform Ask(const std::string& Key, const std::string& Title, const std::string& Hint, const std::string& Initial = "", int Max = 60)
				{
					FPerform Out;
					Out.Prompt.Key = Key;
					Out.Prompt.Title = Title;
					Out.Prompt.Hint = Hint;
					Out.Prompt.Initial = Initial;
					Out.Prompt.MaxLength = Max;
					return Out;
				}

				FPerform Send(const Online::FCall& Call, const std::string& Message = "")
				{
					FPerform Out;
					Out.Calls.push_back(Call);
					Out.Message = Message;
					return Out;
				}

				const char* OfflineText()
				{
					return "Online features need a signed-in account and the account server (guests play offline).";
				}

				// Two taps for anything that cannot be undone (leave a gang, remove a friend, send mail to everyone...).
				bool Confirmed(FUiState& State, const std::string& Id)
				{
					if (State.ConfirmId == Id)
					{
						State.ConfirmId.clear();
						return true;
					}
					State.ConfirmId = Id;
					return false;
				}

				std::string OnOff(bool bValue)
				{
					return bValue ? "On" : "Off";
				}

				std::string SeenText(const std::string& Iso)
				{
					// "2026-10-06T10:17:23.171Z" -> "2026-10-06 10:17"
					if (Iso.size() >= 16)
					{
						return Iso.substr(0, 10) + " " + Iso.substr(11, 5);
					}
					return Iso.empty() ? "-" : Iso;
				}

				int EffectiveKeyIndexOwner(const FSettings& S, int Group, const std::string& Key, int Except)
				{
					const int First = Group == 0 ? 0 : (Group == 1 ? NightBindFirst : RhythmBindFirst);
					const int Last = Group == 0 ? NightBindFirst : (Group == 1 ? RhythmBindFirst : KeyBindCount);
					for (int Index = First; Index < Last; ++Index)
					{
						const std::string Current = S.KeyBinds[Index].empty() ? DefaultKeyName(Index) : S.KeyBinds[Index];
						if (Index != Except && Current == Key)
						{
							return Index;
						}
					}
					return -1;
				}

				int GroupOf(int Index)
				{
					return Index < NightBindFirst ? 0 : (Index < RhythmBindFirst ? 1 : 2);
				}

				std::string KeyOf(const FSettings& S, int Index)
				{
					return S.KeyBinds[Index].empty() ? DefaultKeyName(Index) : S.KeyBinds[Index];
				}

				// Gang requirement presets (leader settings / creation form).
				const int MinLevels[6] = {0, 5, 10, 20, 30, 40};
				const int MinPowers[6] = {0, 10000, 30000, 60000, 100000, 200000};

				template <size_t N>
				int NextPreset(const int (&Values)[N], int Current)
				{
					for (size_t Index = 0; Index < N; ++Index)
					{
						if (Values[Index] == Current)
						{
							return Values[(Index + 1) % N];
						}
					}
					return Values[0];
				}

				std::string NoticeTag(const std::string& Category)
				{
					std::string Tag = Category.empty() ? "notice" : Category;
					std::transform(Tag.begin(), Tag.end(), Tag.begin(), [](unsigned char C) { return static_cast<char>(std::toupper(C)); });
					return "[" + Tag + "]";
				}

				bool NoticeRead(const FProfile& Profile, const std::string& Id)
				{
					const std::vector<std::string>& Read = Profile.Meta.ReadNotices;
					return std::find(Read.begin(), Read.end(), Id) != Read.end();
				}
			}

			std::vector<Online::FCall> SessionCalls(const FProfile& Profile)
			{
				return {Online::PushProfile(Profile), Online::PutDefence(Profile), Online::FetchMail(), Online::FetchNotices(), Online::MyGang(),
					Online::GetFriends(), Online::GetArena()};
			}

			// =============================================================================== parents
			std::string ParentPage(const std::string& Page, bool& bFound)
			{
				bFound = true;
				if (Page == "notice") return "notices";
				if (Page == "friend") return "friends";
				if (Page == "gang-members" || Page == "gang-shop" || Page == "gang-settings" || Page == "gang-create") return "gang";
				if (Page == "arcade-rhythm" || Page == "arcade-halo" || Page == "arcade-memory" || Page == "arcade-breach" || Page == "arcade-rally" || Page == "arcade-ucn") return "arcade";
				if (Page == "keys") return "settings";
				if (Page == "arena-team") return "arena";
				if (Page == "notices" || Page == "friends" || Page == "gang" || Page == "admin" || Page == "return" || Page == "arcade" || Page == "settings")
				{
					return "";
				}
				bFound = false;
				return "";
			}

			// =============================================================================== lobby root
			void LobbyRoot(FProfile& Profile, const FUiState& State, FPage& Page)
			{
				FB B{Page};
				const FMeta& M = Profile.Meta;
				Page.Title = "Lobby";
				const int Shown = M.Settings.LobbyOperator >= 0 ? M.Settings.LobbyOperator : Profile.Team[0];
				if (Shown >= 0 && Shown < static_cast<int>(Roster().size()))
				{
					Page.Portrait = LowerName(Shown);
					Page.PortraitStars = StarsLabel(Roster()[static_cast<size_t>(Shown)].Stars);
					Page.PortraitName = Roster()[static_cast<size_t>(Shown)].Name;
				}
				Page.Subtitle = "Welcome back, " + M.Name;
				B.Line("Commander level " + std::to_string(CommanderLevel(Profile)) + "  -  Squad power " + Num(TeamBattlePower(Profile)) + "  -  Day " + std::to_string(Profile.Day));
				B.Line("Quests to claim: " + std::to_string(ClaimableQuests(Profile)) + "   Unclaimed mail: " + std::to_string(UnclaimedMail(Profile))
					+ "   Season pass tier " + std::to_string(SeasonTier(Profile)) + " / " + std::to_string(SeasonTiers));
				const Online::FCache& Net = State.Net;
				if (Net.bInGang)
				{
					B.Line("Gang: " + Net.Gang.Name + " (level " + std::to_string(Net.Gang.Level) + ", " + std::to_string(Net.Gang.Members) + "/30)");
				}
				const bool bReturn = LiveOps::ReturnActive(Profile);
				if (bReturn)
				{
					B.Line("Welcome back! Your 7-day return event runs for " + Duration(LiveOps::ReturnSecondsLeft(Profile)) + ".");
					B.Tile("go:return", "Welcome back", "event", "day " + std::to_string(LiveOps::ReturnDay(Profile)) + " of 7", true, true, LiveOps::ClaimableReturn(Profile) > 0);
				}
				B.Tile("go:quests", "Quests", "quest", "", true, false, ClaimableQuests(Profile) > 0);
				B.Tile("go:mail", "Mail", "mail", "", true, false, UnclaimedMail(Profile) > 0);
				const int Unread = UnreadNotices(Profile, State);
				B.Tile("go:notices", "Notices", "notice", Unread > 0 ? std::to_string(Unread) + " new" : "", true, false, Unread > 0);
				B.Tile("go:events", "Events", "event", "", true, false, CanClaimLogin(Profile));
				B.Tile("go:friends", "Friends", "friends", Net.bFriendsLoaded ? std::to_string(Net.Friends.size()) + " friends" : "", true, false, !Net.Incoming.empty());
				B.Tile("go:gang", "Gang", "gang", Net.bInGang ? Net.Gang.Name : "join or found one", true, false, Net.PendingCoins > 0);
				B.Tile("go:arcade", "Arcade", "arcade", Num(Profile.Get(ECurrency::ArcadeToken)) + " tokens");
				B.Tile("go:pass", "Season pass", "seasonpass", "Tier " + std::to_string(SeasonTier(Profile)) + " - " + std::to_string(LiveOps::SeasonPassDaysLeft(Profile)) + " days left",
					true, false, ClaimableSeasonTiers(Profile) > 0);
				B.Tile("go:profile", "Profile", "profile");
				B.Tile("go:settings", "Settings", "settings", "audio, graphics, keys, gameplay, lobby");
				if (IsAdmin(Profile))
				{
					B.Tile("go:admin", "Admin desk", "admin", "send mail, post notices", true, true);
				}
				if (Tester::Enabled())
				{
					B.Tile("go:tester", "Tester panel", "star", "currencies, unlocks, levels", true, true);
				}
			}

			// =============================================================================== lobby pages
			bool BuildLobbyPage(FProfile& Profile, const FUiState& State, FPage& Page)
			{
				FB B{Page};
				FMeta& M = Profile.Meta;
				const Online::FCache& Net = State.Net;
				const std::string& P = State.Page;

				// ------------------------------------------------------------------ notices
				if (P == "notices")
				{
					Page.Title = "Notices";
					Page.Subtitle = Net.bOnline ? "Announcements from the Boldinbeauty team" : "Announcements (offline: sign in to receive new notices)";
					int Shown = 0;
					for (size_t Index = 0; Index < Net.Notices.size(); ++Index)
					{
						const Online::FNotice& Notice = Net.Notices[Index];
						const bool bNew = !NoticeRead(Profile, Notice.Id);
						B.Tile("ntc:open:" + std::to_string(Index), Notice.Title, Notice.Category == "event" ? "event" : "notice",
							NoticeTag(Notice.Category) + " " + SeenText(Notice.CreatedAt).substr(0, 10) + (Notice.bPinned ? " - pinned" : ""), true, bNew, bNew);
						++Shown;
					}
					if (Shown == 0)
					{
						for (int Index = 0; Index < NoticeCount(); ++Index)
						{
							B.Line(NoticeTitle(Index));
							B.Line("    " + NoticeBody(Index));
						}
					}
					B.Button("ntc:refresh", "Refresh", "", Net.bOnline);
					return true;
				}
				if (P == "notice")
				{
					if (State.SelNotice < 0 || State.SelNotice >= static_cast<int>(Net.Notices.size()))
					{
						Page.Title = "Notice";
						B.Line("That notice is gone. Go back to the list.");
						return true;
					}
					const Online::FNotice& Notice = Net.Notices[static_cast<size_t>(State.SelNotice)];
					Page.Title = Notice.Title;
					Page.Subtitle = NoticeTag(Notice.Category) + "  " + SeenText(Notice.CreatedAt) + " UTC";
					for (const std::string& Line : Split(Notice.Body, '\n'))
					{
						B.Line(Line);
					}
					return true;
				}

				// ------------------------------------------------------------------ welcome back
				if (P == "return")
				{
					Page.Title = "Welcome back";
					if (!LiveOps::ReturnActive(Profile))
					{
						Page.Subtitle = "For commanders who were away 14 days or more";
						B.Line("No welcome-back event is running. It starts by itself when you return after 14 days away.");
						return true;
					}
					const int Today = LiveOps::ReturnDay(Profile);
					Page.Subtitle = "Day " + std::to_string(Today) + " of 7 - ends in " + Duration(LiveOps::ReturnSecondsLeft(Profile))
						+ ". Returned quests: " + RewardText(LiveOps::ReturnQuestTotal()) + " in all.";
					for (int Day = 1; Day <= ReturnEventDays; ++Day)
					{
						const bool bClaimed = LiveOps::ReturnCheckInClaimed(Profile, Day);
						const bool bOpen = Day <= Today && !bClaimed;
						B.Tile("ret:day:" + std::to_string(Day), "Day " + std::to_string(Day), bClaimed ? "check" : (Day <= Today ? "event" : "lock"),
							RewardText(LiveOps::ReturnCheckInReward(Day)), bOpen, Day == Today, bOpen);
					}
					for (int Index = 0; Index < ReturnQuestCount; ++Index)
					{
						const LiveOps::FReturnQuest Quest = LiveOps::ReturnQuest(Profile, Index);
						const bool bReady = Quest.bComplete && !Quest.bClaimed;
						B.Tile("ret:q:" + std::to_string(Index), Quest.Title, Quest.bClaimed ? "check" : "quest",
							(Quest.bClaimed ? std::string("claimed") : (bReady ? std::string("READY") : std::to_string(Quest.Progress) + "/" + std::to_string(Quest.Target)))
								+ " | " + RewardText(Quest.Reward),
							bReady, bReady, bReady);
					}
					B.Button("ret:all", "Claim all", "", LiveOps::ClaimableReturn(Profile) > 0);
					return true;
				}

				// ------------------------------------------------------------------ friends
				if (P == "friends")
				{
					Page.Title = "Friends";
					if (!Net.bOnline)
					{
						Page.Subtitle = OfflineText();
						B.Line("Your user id (for friends to add you): " + (Profile.Account.UserId.empty() ? std::string("sign in first") : Profile.Account.UserId));
						return true;
					}
					Page.Subtitle = std::to_string(Net.Friends.size()) + " / 50 friends.  Your user id: " + Profile.Account.UserId;
					if (!Net.bFriendsLoaded)
					{
						B.Line("Loading...");
					}
					for (size_t Index = 0; Index < Net.Incoming.size(); ++Index)
					{
						const Online::FCard& Card = Net.Incoming[Index];
						B.Tile("fr:acc:" + std::to_string(Index), "Accept " + Card.Name, "plus", "id " + Card.Uid + " | Lv " + std::to_string(Card.Level), true, true, true);
						B.Tile("fr:dec:" + std::to_string(Index), "Decline " + Card.Name, "cross", "friend request");
					}
					for (size_t Index = 0; Index < Net.Friends.size(); ++Index)
					{
						const Online::FCard& Card = Net.Friends[Index];
						B.Tile("fr:sel:" + std::to_string(Index), Card.Name, "profile", "Lv " + std::to_string(Card.Level) + " | power " + Num(Card.Power)
							+ (Card.Gang.empty() ? "" : " | " + Card.Gang));
					}
					for (size_t Index = 0; Index < Net.Outgoing.size(); ++Index)
					{
						const Online::FCard& Card = Net.Outgoing[Index];
						B.Tile("fr:cancel:" + std::to_string(Index), "Cancel request: " + Card.Name, "cross", "waiting for them");
					}
					if (Net.Friends.empty() && Net.Incoming.empty() && Net.bFriendsLoaded)
					{
						B.Line("No friends yet. Add someone by their user id or username.");
					}
					B.Button("fr:add", "Add friend", "user id or name");
					B.Button("fr:refresh", "Refresh");
					return true;
				}
				if (P == "friend")
				{
					if (State.SelFriend < 0 || State.SelFriend >= static_cast<int>(Net.Friends.size()))
					{
						Page.Title = "Friend";
						B.Line("Go back to the list.");
						return true;
					}
					const Online::FCard& Card = Net.Friends[static_cast<size_t>(State.SelFriend)];
					Page.Title = Card.Name;
					Page.Subtitle = "User id " + Card.Uid;
					B.Line("Commander level " + std::to_string(Card.Level) + "   Squad power " + Num(Card.Power));
					B.Line("Gang: " + (Card.Gang.empty() ? std::string("none") : Card.Gang));
					B.Line("Last seen: " + SeenText(Card.LastSeen) + " UTC");
					B.Button("fr:remove", State.ConfirmId == "fr:remove" ? "Tap again to remove" : "Remove friend");
					return true;
				}

				// ------------------------------------------------------------------ gangs
				if (P == "gang")
				{
					Page.Title = "Gang";
					if (!Net.bOnline)
					{
						Page.Subtitle = OfflineText();
						B.Line("Gangs are groups of up to 30 commanders. Members earn gang points together; at the end of every month (the gang season) each");
						B.Line("member receives Gang coins for the gang shop, and season milestones pay every member along the way.");
						return true;
					}
					if (!Net.bGangLoaded)
					{
						Page.Subtitle = "Loading...";
						return true;
					}
					if (Net.PendingCoins > 0)
					{
						B.Tile("gang:claim", "Season reward", "gang_coin", Num(Net.PendingCoins) + " Gang coins (" + Net.PendingSeason + ")", true, true, true);
					}
					if (!Net.bInGang)
					{
						Page.Subtitle = "Join a gang (up to 30 members) or found your own for " + Num(Online::GangCreateGold) + " Gold";
						B.Line("Your commander level " + std::to_string(CommanderLevel(Profile)) + ", squad power " + Num(TeamBattlePower(Profile))
							+ (Net.SearchText.empty() ? "" : ".  Search: \"" + Net.SearchText + "\""));
						for (size_t Index = 0; Index < Net.Search.size(); ++Index)
						{
							const Online::FGangSummary& G = Net.Search[Index];
							const bool bMeets = CommanderLevel(Profile) >= G.MinLevel && TeamBattlePower(Profile) >= G.MinPower;
							const bool bCan = G.bOpen && G.Members < G.MaxMembers && bMeets;
							B.Tile("gang:join:" + std::to_string(Index), G.Name, "gang",
								"Lv " + std::to_string(G.Level) + " | " + std::to_string(G.Members) + "/" + std::to_string(G.MaxMembers) + " | needs Lv "
									+ std::to_string(G.MinLevel) + ", power " + Num(G.MinPower) + (G.bOpen ? "" : " | closed"),
								bCan, false);
						}
						if (Net.Search.empty())
						{
							B.Line("No gangs found. Found the first one!");
						}
						B.Button("gang:search", "Search");
						B.Button("go:gang-create", "Found a gang", Num(Online::GangCreateGold) + " Gold");
						B.Button("gang:refresh", "Refresh");
						return true;
					}
					const Online::FGang& G = Net.Gang;
					Page.Subtitle = G.Name + " - level " + std::to_string(G.Level) + " - " + std::to_string(G.Members) + " / 30 members - " + (G.MyRole == "leader" ? "you lead" : "member");
					int MyContribution = 0;
					for (const Online::FGangMember& Member : G.MemberList)
					{
						MyContribution = Member.Uid == Profile.Account.UserId ? Member.SeasonContribution : MyContribution;
					}
					B.Line("Season " + LiveOps::SeasonLabel(LiveOps::SeasonKeyFromCode(G.Season)) + ": " + Num(G.SeasonPoints) + " gang points, ends in "
						+ std::to_string(LiveOps::DaysLeftInSeason(Profile.NowUnix)) + " days. Your share: " + Num(MyContribution) + " points.");
					B.Line("Season end pays every member about " + Num(Online::GangSeasonCoins(G.SeasonPoints, MyContribution)) + " Gang coins at the current total"
						+ " (more as the gang earns). Requirements: Lv " + std::to_string(G.MinLevel) + ", power " + Num(G.MinPower) + (G.bOpen ? ", open." : ", closed."));
					B.Line("Earn points: daily check-in 50, donations 10 each (1,000 Gold, 5 a day), and 5 per daily quest / 2 per stage you clear (synced).");
					B.Tile("gang:checkin", "Daily check-in", "event", "+50 gang points");
					B.Tile("gang:donate", "Donate", "gold", Num(Online::GangDonateGold) + " Gold = +10 points", Profile.Get(ECurrency::Gold) >= Online::GangDonateGold);
					for (int Index = 0; Index < Online::GangMilestoneCount; ++Index)
					{
						const bool bReached = G.SeasonPoints >= Online::GangMilestonePoints(Index);
						const bool bClaimed = std::find(G.MilestonesClaimed.begin(), G.MilestonesClaimed.end(), Index) != G.MilestonesClaimed.end();
						B.Tile("gang:ms:" + std::to_string(Index), "Milestone " + Num(Online::GangMilestonePoints(Index)),
							bClaimed ? "check" : (bReached ? "star" : "lock"), (bClaimed ? std::string("claimed | ") : "") + RewardText(Online::GangMilestoneReward(Index)),
							bReached && !bClaimed, false, bReached && !bClaimed);
					}
					B.Tile("go:gang-members", "Members", "friends", std::to_string(G.Members) + " / 30");
					B.Tile("go:gang-shop", "Gang shop", "gang_coin", Num(Profile.Get(ECurrency::GangCoin)) + " Gang coins");
					if (G.MyRole == "leader")
					{
						B.Tile("go:gang-settings", "Gang settings", "settings", "requirements, open / closed");
					}
					B.Button("gang:refresh", "Refresh");
					B.Button("gang:leave", State.ConfirmId == "gang:leave" ? "Tap again to leave" : "Leave gang");
					return true;
				}
				if (P == "gang-members")
				{
					const Online::FGang& G = Net.Gang;
					Page.Title = G.Name + " - members";
					Page.Subtitle = std::to_string(G.MemberList.size()) + " / 30";
					std::vector<size_t> Order(G.MemberList.size());
					for (size_t Index = 0; Index < Order.size(); ++Index)
					{
						Order[Index] = Index;
					}
					std::stable_sort(Order.begin(), Order.end(), [&G](size_t A, size_t C) { return G.MemberList[A].SeasonContribution > G.MemberList[C].SeasonContribution; });
					for (size_t Index : Order)
					{
						const Online::FGangMember& Member = G.MemberList[Index];
						B.Tile("gang:member:" + std::to_string(Index), Member.Name + (Member.Role == "leader" ? " (leader)" : ""), "profile",
							"Lv " + std::to_string(Member.Level) + " | power " + Num(Member.Power) + " | season " + Num(Member.SeasonContribution) + " pts",
							true, static_cast<int>(Index) == State.SelMember);
					}
					if (State.SelMember >= 0 && State.SelMember < static_cast<int>(G.MemberList.size()))
					{
						const Online::FGangMember& Member = G.MemberList[static_cast<size_t>(State.SelMember)];
						B.Line(Member.Name + " (id " + Member.Uid + "): contribution " + Num(Member.Contribution) + " in all, last seen " + SeenText(Member.LastSeen) + " UTC.");
						if (G.MyRole == "leader" && Member.Uid != Profile.Account.UserId)
						{
							B.Button("gang:kick", State.ConfirmId == "gang:kick" ? "Tap again to remove" : "Remove from gang");
						}
					}
					return true;
				}
				if (P == "gang-shop")
				{
					const int Level = Net.bInGang ? Net.Gang.Level : 1;
					Page.Title = "Gang shop";
					Page.Subtitle = Num(Profile.Get(ECurrency::GangCoin)) + " Gang coins - stock refreshes every month (gang level " + std::to_string(Level) + ")";
					for (int Index = 0; Index < Online::GangShopCount(); ++Index)
					{
						const Online::FGangShopItem& Item = Online::GangShopItem(Index);
						const int Left = Item.MonthlyLimit - M.Gang.ShopBought[Index];
						const bool bLocked = Level < Item.MinGangLevel;
						B.Tile("gang:buy:" + std::to_string(Index), Item.Name, Item.Icon,
							bLocked ? "gang level " + std::to_string(Item.MinGangLevel) : Num(Item.Price) + " coins | " + std::to_string(std::max(0, Left)) + " left",
							!bLocked && Left > 0 && Profile.Get(ECurrency::GangCoin) >= Item.Price);
					}
					return true;
				}
				if (P == "gang-settings" || P == "gang-create")
				{
					const bool bCreate = P == "gang-create";
					Page.Title = bCreate ? "Found a gang" : "Gang settings";
					Page.Subtitle = bCreate ? "Costs " + Num(Online::GangCreateGold) + " Gold. You become its leader." : "Only the leader can change these";
					B.Tile("gang:minlv", "Minimum commander level", "profile", std::to_string(Net.NewGangMinLevel) + " (tap to change)");
					B.Tile("gang:minbp", "Minimum squad power", "star", Num(Net.NewGangMinPower) + " (tap to change)");
					B.Tile("gang:open", "New members", "gang", Net.bNewGangOpen ? "Open: anyone who meets the requirements" : "Closed");
					if (bCreate)
					{
						B.Tile("gang:new", "Choose a name and found it", "plus", "3-20 letters, digits, spaces", Profile.Get(ECurrency::Gold) >= Online::GangCreateGold, true);
					}
					else
					{
						B.Tile("gang:save", "Save settings", "check", "", true, true);
					}
					return true;
				}

				// ------------------------------------------------------------------ admin desk
				if (P == "admin")
				{
					Page.Title = "Admin desk";
					if (!IsAdmin(Profile))
					{
						B.Line("Only the admin account can use this page.");
						return true;
					}
					const Online::FAdminCompose& A = Net.Admin;
					Page.Subtitle = Net.bOnline ? "Mail and notices go to the account server" : "The account server is not reachable";
					B.Line("Mail: to " + (A.bToAll ? std::string("EVERY PLAYER") : "user " + (A.Target.empty() ? std::string("(choose an id)") : A.Target)) + " - \"" + A.Title + "\"");
					B.Line("    " + (A.Body.empty() ? std::string("(no message)") : A.Body) + "   Attachment: " + RewardText(Online::RewardPreset(A.RewardPreset)));
					B.Tile("adm:to", "Send to", "mail", A.bToAll ? "All users (tap: one user)" : "One user (tap: all users)", true, A.bToAll);
					B.Tile("adm:target", "User id", "profile", A.Target.empty() ? "tap to type" : A.Target, !A.bToAll);
					B.Tile("adm:title", "Mail title", "mail", A.Title);
					B.Tile("adm:body", "Mail message", "book", A.Body.empty() ? "tap to type" : A.Body.substr(0, 40));
					B.Tile("adm:reward", "Attachment", "gold", RewardText(Online::RewardPreset(A.RewardPreset)) + " (tap to change)");
					B.Tile("adm:send", State.ConfirmId == "adm:send" ? "Tap again to SEND" : "Send mail", "check", A.bToAll ? "to everyone" : "to " + A.Target,
						Net.bOnline && (A.bToAll || !A.Target.empty()), true);
					B.Line("Notice: " + NoticeTag(Online::NoticeCategory(A.NoticeCategory)) + " " + (A.NoticeTitle.empty() ? std::string("(title)") : A.NoticeTitle)
						+ (A.bPinned ? " - pinned" : ""));
					B.Tile("adm:ncat", "Notice type", "notice", Online::NoticeCategory(A.NoticeCategory));
					B.Tile("adm:ntitle", "Notice title", "notice", A.NoticeTitle.empty() ? "tap to type" : A.NoticeTitle);
					B.Tile("adm:nbody", "Notice text", "book", A.NoticeBody.empty() ? "tap to type" : A.NoticeBody.substr(0, 40));
					B.Tile("adm:npin", "Pinned", "star", OnOff(A.bPinned));
					B.Tile("adm:post", "Publish notice", "check", "", Net.bOnline && !A.NoticeTitle.empty() && !A.NoticeBody.empty(), true);
					for (size_t Index = 0; Index < Net.Notices.size() && Page.Grid.size() < static_cast<size_t>(MaxGrid); ++Index)
					{
						const std::string Id = "adm:ndel:" + std::to_string(Index);
						B.Tile(Id, State.ConfirmId == Id ? "Tap again to delete" : "Delete: " + Net.Notices[Index].Title, "cross", NoticeTag(Net.Notices[Index].Category));
					}
					return true;
				}

				// ------------------------------------------------------------------ arcade
				if (P == "arcade")
				{
					const FArcadeRecords& R = M.ArcadeRecords;
					Page.Title = "Arcade";
					Page.Subtitle = "1 arcade token a play. Tokens: " + Num(Profile.Get(ECurrency::ArcadeToken)) + " (topped up to 5 every day, login bonus, quests)";
					B.Line("Boss Rush: boss after boss with your squad until it falls. Every round cleared pays out. Best round: " + std::to_string(R.BossRushBest) + ".");
					B.Line("Rhythm: hit the notes as they reach the line. Three tracks, three difficulties.");
					B.Tile("arc:boss", "Boss Rush", "bossrush", "best round " + std::to_string(R.BossRushBest), Profile.Get(ECurrency::ArcadeToken) > 0 && Profile.TeamIsPlayable());
					B.Tile("arc:rhythm", "Rhythm", "rhythm", "best " + Num(*std::max_element(std::begin(R.RhythmBest), std::end(R.RhythmBest))), Profile.Get(ECurrency::ArcadeToken) > 0);
					// The 2026-10-07 mini-games (BIBMiniGames.h).
					B.Line("Halo Fall: shoot down the angels before they reach the street. Neon Memory: match the operators' portraits. Firewall Breach: stop the cursor in the open port.");
					B.Line("Ark Rally: a physics trial bike against three rivals' ghosts - lean through the jumps, then build and ride your own tracks.");
					static const char* const MiniPages[Arcade::MiniGameCount] = {"halo", "memory", "breach", "rally"};
					static const char* const MiniIcons[Arcade::MiniGameCount] = {"halofall", "memory", "breach", "rally"};
					for (int Mini = 0; Mini < Arcade::MiniGameCount; ++Mini)
					{
						int Best = 0;
						for (int D = 0; D < Arcade::DifficultyCount; ++D)
						{
							Best = std::max(Best, R.MiniBest[Mini * Arcade::DifficultyCount + D]);
						}
						B.Tile(std::string("arc:") + MiniPages[Mini], Arcade::MiniGameName(static_cast<Arcade::EMiniGame>(Mini)), MiniIcons[Mini], "best " + Num(Best),
							Profile.Get(ECurrency::ArcadeToken) > 0);
					}
					// Ultimate Custom Night (owner request 2026-10-14): unlocked by the first completion of Chapter 13.
					const bool bUcn = Arcade::Ucn::IsUnlocked(Profile);
					B.Line(bUcn ? "Ultimate Custom Night: 20 + 1 subjects, four modes. Your first win in any mode pays a one-time 3,000 Diamonds."
								: "Ultimate Custom Night: clear Chapter 13 to unlock it.");
					B.Tile("arc:ucn", "Ultimate Custom Night", "night", bUcn ? (Arcade::Ucn::IsRewardClaimed(Profile) ? "first-win reward claimed" : "first win: 3,000 Diamonds")
						: "locked - clear Chapter 13", bUcn, false, bUcn && !Arcade::Ucn::IsRewardClaimed(Profile));
					return true;
				}
				if (P == "arcade-ucn")
				{
					namespace Ucn = Arcade::Ucn;
					const FArcadeRecords& R = M.ArcadeRecords;
					Page.Title = "Ultimate Custom Night";
					Page.Subtitle = "Survive until 6 AM (" + Fixed(Arcade::NightHourSeconds * Arcade::NightHours / 60.0, 0) + " minutes). 1 token. "
						+ (Ucn::IsRewardClaimed(Profile) ? "First-win reward claimed." : "First win of any mode: 3,000 Diamonds (once per account).");
					B.Line("Tools: 2 doors (" + KeyDisplayName(KeyOf(M.Settings, NightBindFirst)) + " / " + KeyDisplayName(KeyOf(M.Settings, NightBindFirst + 1))
						+ ") and the camera system (" + KeyDisplayName(KeyOf(M.Settings, NightBindFirst + 4)) + ", cameras 1-9, 0) with a few shocks per camera (F). "
						"Front window: W (CAM 3 is the view outside it). Click things in the office; V takes the vaccine; X closes an advert.");
					B.Line("At 5 AM Plack's sacrifice summons Death in every mode. Blood at the window: shut it only in her LAST phase. Weather hides as another operator with a yellow or blue aura.");
					B.Line("Every deadly threat is announced by a sound or a sign first. Judge punishes a door shut on nobody or a wasted shock - watch for the FALSE ALARM sign.");
					static const char* const Help[Ucn::ModeCount] = {"AI 15, subjects 001-005 (+019 at 5 AM)", "AI 15, subjects 001-010 (+019 at 5 AM)",
						"AI 10, subjects 000-015 (+019 at 5 AM)", "AI 20, all of 000-020 (019 at 5 AM)"};
					for (int Mode = 0; Mode < Ucn::ModeCount; ++Mode)
					{
						B.Tile("arc:ucngo:" + std::to_string(Mode), Ucn::ModeName(static_cast<Ucn::EMode>(Mode)), "night",
							std::string(Help[Mode]) + " | best " + std::to_string(R.UcnBestHour[Mode]) + " AM | wins " + std::to_string(R.UcnWins[Mode]),
							Ucn::IsUnlocked(Profile) && Profile.Get(ECurrency::ArcadeToken) > 0);
					}
					return true;
				}
				if (P == "arcade-halo" || P == "arcade-memory" || P == "arcade-breach")
				{
					const FArcadeRecords& R = M.ArcadeRecords;
					const Arcade::EMiniGame Mini = P == "arcade-halo" ? Arcade::EMiniGame::HaloFall : (P == "arcade-memory" ? Arcade::EMiniGame::NeonMemory : Arcade::EMiniGame::FirewallBreach);
					const std::string Key = P.substr(7);
					Page.Title = Arcade::MiniGameName(Mini);
					Page.Subtitle = "1 arcade token a play. Tokens: " + Num(Profile.Get(ECurrency::ArcadeToken));
					if (Mini == Arcade::EMiniGame::HaloFall)
					{
						B.Line("Angels of light march down on Neo Tokyo in formation and rain light bolts. Hold the turret's fire and shoot every angel down.");
						B.Line("Move: " + KeyDisplayName(KeyOf(M.Settings, 2)) + " / " + KeyDisplayName(KeyOf(M.Settings, 3)) + " (or the arrow keys)   Fire (hold): "
							+ KeyDisplayName(KeyOf(M.Settings, 4)) + ". The top row and the golden Seraph are worth more. A hit costs a life; if they reach the street the run is over.");
					}
					else if (Mini == Arcade::EMiniGame::NeonMemory)
					{
						B.Line("Every card is shown for a moment, then turned over. Flip two at a time to find the operator pairs before the timer runs out.");
						B.Line("Click a card (or arrow keys + " + KeyDisplayName(KeyOf(M.Settings, 4)) + "). Pairs in a row raise the combo; fewer moves earn more stars.");
					}
					else
					{
						B.Line("Hack through the Ark's firewall: a cursor sweeps the bar - stop it inside the open port with " + KeyDisplayName(KeyOf(M.Settings, 4))
							+ " or Enter. The centre of the port is a Perfect.");
						B.Line("Every layer has a narrower port and a faster cursor. A miss (or a trace that runs out) costs one integrity point.");
					}
					for (int D = 0; D < Arcade::DifficultyCount; ++D)
					{
						const Arcade::EDifficulty Diff = static_cast<Arcade::EDifficulty>(D);
						const int Slot = Arcade::MiniRecordSlot(Mini, Diff);
						std::string Info;
						if (Mini == Arcade::EMiniGame::HaloFall)
						{
							const Arcade::FHaloRules Rules = Arcade::HaloRules(Diff);
							Info = std::to_string(Rules.Waves) + " waves, " + std::to_string(Rules.Lives) + " lives";
						}
						else if (Mini == Arcade::EMiniGame::NeonMemory)
						{
							const Arcade::FMemoryRules Rules = Arcade::MemoryRules(Diff);
							Info = std::to_string(Rules.Cols * Rules.Rows / 2) + " pairs, " + Fixed(Rules.TimeLimit, 0) + " s";
						}
						else
						{
							const Arcade::FBreachRules Rules = Arcade::BreachRules(Diff);
							Info = std::to_string(Rules.Layers) + " layers, " + std::to_string(Rules.Integrity) + " integrity";
						}
						B.Tile("arc:" + Key + "go:" + std::to_string(D), Arcade::DifficultyName(Diff), Key == "halo" ? "halofall" : Key,
							Info + " | best " + Num(R.MiniBest[Slot]) + ", " + std::to_string(R.MiniStars[Slot]) + "/5 stars | win: "
								+ RewardText(Arcade::MiniReward(Mini, Diff, true, 5)),
							Profile.Get(ECurrency::ArcadeToken) > 0);
					}
					return true;
				}
				if (P == "arcade-rally")
				{
					// Ark Rally (owner request 2026-10-15; BIBRally.h): three tracks plus the player's own track code.
					const FArcadeRecords& R = M.ArcadeRecords;
					const Arcade::EMiniGame Mini = Arcade::EMiniGame::ArkRally;
					Page.Title = "Ark Rally";
					Page.Subtitle = "1 arcade token a race (your own track is free). Tokens: " + Num(Profile.Get(ECurrency::ArcadeToken));
					const Arcade::ERallyVehicle Vehicle = R.RallyVehicle == 1 ? Arcade::ERallyVehicle::Buggy : Arcade::ERallyVehicle::DirtBike;
					B.Line("Race over hills, ramps and gaps against three operators' ghosts on the same vehicle. Land on your head (or lie on the buggy's "
						"roof) and you come back just before the mistake. Coins, bombs, springs and boost pads lie on the way.");
					B.Line("Flips fill the nitro meter: lean round a full turn in the air and land it. Dirt bike: fastest, flips easily, can crash. "
						"Buggy: slower, steady, flips only off big jumps, very hard to crash.");
					B.Line("Gas: " + KeyDisplayName(KeyOf(M.Settings, 0)) + " / Up   Brake and reverse: " + KeyDisplayName(KeyOf(M.Settings, 1)) + " / Down   Lean back: "
						+ KeyDisplayName(KeyOf(M.Settings, 2)) + " / Left   Lean forward: " + KeyDisplayName(KeyOf(M.Settings, 3)) + " / Right   Nitro: "
						+ KeyDisplayName(KeyOf(M.Settings, 4)) + " / Shift   R: back to safe ground. Beat all three ghosts without a crash for 5 stars.");
					B.Tile("arc:rallycar", std::string("Vehicle: ") + Arcade::RallyVehicleName(Vehicle), Vehicle == Arcade::ERallyVehicle::Buggy ? "rally" : "rally",
						Vehicle == Arcade::ERallyVehicle::Buggy ? "steady, hard to crash (tap: dirt bike)" : "fast, flips easily (tap: buggy)", true, true);
					for (int D = 0; D < Arcade::DifficultyCount; ++D)
					{
						const Arcade::EDifficulty Diff = static_cast<Arcade::EDifficulty>(D);
						const int Slot = Arcade::MiniRecordSlot(Mini, Diff);
						B.Tile("arc:rallygo:" + std::to_string(D), std::string(Arcade::DifficultyName(Diff)) + ": " + Arcade::RallyTrackName(Diff), "rally",
							"best " + Num(R.MiniBest[Slot]) + ", " + std::to_string(R.MiniStars[Slot]) + "/5 stars | win: " + RewardText(Arcade::MiniReward(Mini, Diff, true, 5)),
							Profile.Get(ECurrency::ArcadeToken) > 0);
					}
					// The track editor: a track is a line of text the player can write, keep and share.
					Arcade::FRallyTrack Built;
					std::string Error;
					const bool bValid = !R.RallyTrack.empty() && Arcade::BuildTrack(R.RallyTrack, "Custom track", Built, Error);
					B.Line("Your track: " + (R.RallyTrack.empty() ? std::string("none yet - start from a built-in track or write one")
						: (bValid ? Fixed(Built.FinishX - Built.StartX, 0) + " m, " + std::to_string(Built.Coins) + " coins" : "cannot be ridden: " + Error)));
					B.Line("Track code: F<len> flat, H<len>,<h> hill (negative = dip), U<len>,<d> slope up / down, R<len>,<h> ramp, G<len> gap, W<len>,<a> bumps; "
						"C<n> coins, B bomb, S spring, P boost pad, K checkpoint. Example: F10 C5 H16,3 K R12,2 G5 F10 P F20");
					B.Tile("arc:rallyedit", R.RallyTrack.empty() ? "Write a track" : "Edit your track", "book", R.RallyTrack.empty() ? "type a track code" : R.RallyTrack.substr(0, 40));
					B.Tile("arc:rallytry", "Ride your track", "rally", "free practice against Medium ghosts, no reward", bValid);
					for (int D = 0; D < Arcade::DifficultyCount; ++D)
					{
						const Arcade::EDifficulty Diff = static_cast<Arcade::EDifficulty>(D);
						const std::string Id = "arc:rallycopy:" + std::to_string(D);
						B.Tile(Id, State.ConfirmId == Id ? "Tap again to replace your track" : std::string("Start from ") + Arcade::RallyTrackName(Diff), "plus",
							"copies its code into your track");
					}
					return true;
				}
				if (P == "arcade-rhythm")
				{
					const FArcadeRecords& R = M.ArcadeRecords;
					const int D = std::clamp(State.RhythmDiff, 0, Arcade::DifficultyCount - 1);
					const Arcade::EDifficulty Diff = static_cast<Arcade::EDifficulty>(D);
					Page.Title = "Rhythm";
					Page.Subtitle = std::string(Arcade::DifficultyName(Diff)) + ": " + std::to_string(Arcade::LanesFor(Diff)) + " lanes. 1 token.";
					std::string Keys;
					for (int Lane = 0; Lane < Arcade::LanesFor(Diff); ++Lane)
					{
						Keys += (Keys.empty() ? "" : " ") + KeyDisplayName(KeyOf(M.Settings, RhythmBindFirst + Lane));
					}
					B.Line("Lane keys: " + Keys + ". Hit when a note crosses the line: Perfect / Great / Good. Every 10 in a row raises the multiplier (max x4).");
					B.Line("Misses drain the meter; an empty meter fails the song. Timing offset: " + std::to_string(M.Settings.RhythmOffsetMs) + " ms (Settings > Gameplay).");
					for (int Song = 0; Song < Arcade::SongCount(); ++Song)
					{
						const int Slot = Song * Arcade::DifficultyCount + D;
						const Arcade::FSong& S = Arcade::Song(Song);
						B.Tile("arc:song:" + std::to_string(Song), S.Title, "rhythm", std::to_string(S.Bpm) + " BPM | best " + Num(R.RhythmBest[Slot]) + " | "
							+ std::to_string(R.RhythmStars[Slot]) + "/5 stars", Profile.Get(ECurrency::ArcadeToken) > 0);
					}
					for (int Index = 0; Index < Arcade::DifficultyCount; ++Index)
					{
						B.Button("arc:rdiff:" + std::to_string(Index), Arcade::DifficultyName(static_cast<Arcade::EDifficulty>(Index)), "", true, Index == D);
					}
					return true;
				}

				// ------------------------------------------------------------------ settings
				if (P == "settings" || P == "prefs")
				{
					const FSettings& S = M.Settings;
					const int Section = std::clamp(State.SettingsTab, 0, 5);
					static const char* const Sections[6] = {"Audio", "Graphics", "Controls", "Gameplay", "Lobby", "Account"};
					Page.Title = "Settings";
					Page.Subtitle = std::string(Sections[Section]) + " - changes apply straight away and are saved with your profile";
					auto Percent = [](int Value) { return std::to_string(Value) + "%"; };
					if (Section == 0)
					{
						B.Line("Master volume drives everything. Effects covers combat and the arcade; music and voice are kept for the soundtrack and voice lines.");
						B.Tile("set:master:-1", "Master volume -", "settings", Percent(S.MasterVolume));
						B.Tile("set:master:1", "Master volume +", "settings", Percent(S.MasterVolume));
						B.Tile("set:sfx:-1", "Effects volume -", "settings", Percent(S.SfxVolume));
						B.Tile("set:sfx:1", "Effects volume +", "settings", Percent(S.SfxVolume));
						B.Tile("set:music:-1", "Music volume -", "settings", Percent(S.MusicVolume));
						B.Tile("set:music:1", "Music volume +", "settings", Percent(S.MusicVolume));
						B.Tile("set:voice:-1", "Voice volume -", "settings", Percent(S.VoiceVolume));
						B.Tile("set:voice:1", "Voice volume +", "settings", Percent(S.VoiceVolume));
						B.Tile("set:bgaudio", "Sound in the background", "settings", OnOff(S.bBackgroundAudio) + " (when the window is not focused)");
					}
					else if (Section == 1)
					{
						B.Line("Lower these if the game lags. Quality sets every group below at once; each group can then be set on its own. "
							"Render scale draws the 3D view at fewer pixels and scales it up (the biggest saving on big screens). "
							"Window mode applies in the packaged game; the editor's play window keeps its own.");
						B.Tile("set:perf", "Performance mode", "settings", "one tap: Low quality, 70% render scale, light weather, no volumetric fog");
						B.Tile("set:autodetect", "Auto-detect", "settings", "test this computer and pick settings");
						B.Tile("set:gfx", "Quality", "settings", QualityName(S.GraphicsQuality) + std::string(" (tap to cycle, resets the groups)"));
						B.Tile("set:scale:-1", "Render scale -", "settings", Percent(S.ResolutionScale));
						B.Tile("set:scale:1", "Render scale +", "settings", Percent(S.ResolutionScale));
						B.Tile("set:dynres", "Dynamic resolution", "settings", OnOff(S.bDynamicResolution) + " (lowers the render scale when a frame is slow)");
						const int Cap = FrameRateCaps[std::clamp(S.FrameRateCap, 0, FrameRateCapCount - 1)];
						B.Tile("set:fps", "Frame-rate limit", "settings", Cap > 0 ? std::to_string(Cap) + " fps" : "Unlimited");
						B.Tile("set:vsync", "V-sync", "settings", OnOff(S.bVSync));
						B.Tile("set:window", "Window mode", "settings", WindowModeName(S.WindowMode));
						B.Tile("set:showfps", "Show frame rate", "settings", OnOff(S.bShowFps));
						for (int Group = 0; Group < GraphicsGroupCount; ++Group)
						{
							const int Level = S.GraphicsDetail[Group];
							B.Tile("set:grp:" + std::to_string(Group), GraphicsGroupName(Group), "settings",
								Level < 0 ? QualityName(S.GraphicsQuality) + std::string(" (quality)") : std::string(QualityName(Level)));
						}
						B.Tile("set:weather", "Weather effects", "settings", WeatherDetailName(S.WeatherDetail));
						B.Tile("set:volfog", "Volumetric fog", "settings", OnOff(S.bVolumetricFog) + " (light shafts; needs Shadows High)");
						B.Tile("set:anime", "Anime look", "settings", OnOff(S.bAnimeLook) + " (lines, cel shading, 12 fps motion)");
					}
					else if (Section == 2)
					{
						B.Line("Camera and control preferences for battles, and key bindings for battles and the arcade.");
						B.Tile("go:keys", "Key bindings", "keys", "battle, Night Shift, Rhythm", true, true);
						B.Tile("set:sens:-1", "Mouse sensitivity -", "preferences", std::to_string(S.MouseSensitivity) + " / 10");
						B.Tile("set:sens:1", "Mouse sensitivity +", "preferences", std::to_string(S.MouseSensitivity) + " / 10");
						B.Tile("set:invert", "Invert vertical look", "preferences", OnOff(S.bInvertY));
						B.Tile("set:fov:-1", "Field of view -", "preferences", std::to_string(S.FieldOfView) + " degrees");
						B.Tile("set:fov:1", "Field of view +", "preferences", std::to_string(S.FieldOfView) + " degrees");
						B.Tile("pref:hint", "Controls line in battle", "preferences", OnOff(S.bShowControlsHint));
					}
					else if (Section == 3)
					{
						B.Line("Gameplay and accessibility.");
						B.Tile("set:dmg", "Damage numbers", "preferences", OnOff(S.bDamageNumbers));
						B.Tile("set:shake", "Camera shake", "preferences", OnOff(S.bCameraShake));
						B.Tile("set:motion", "Reduce motion", "preferences", OnOff(S.bReduceMotion) + " (no transitions, no shake, no flashes)");
						B.Tile("set:feel:-1", "Combat feel -", "preferences", Percent(S.CombatFeel) + " (hit stop, shake, flash, rings)");
						B.Tile("set:feel:1", "Combat feel +", "preferences", Percent(S.CombatFeel));
						B.Tile("set:feelstop", "Hit stop", "preferences", OnOff(S.bFeelHitStop));
						B.Tile("set:feelflash", "Hit flash", "preferences", OnOff(S.bFeelFlash));
						B.Tile("set:feelfov", "View punch", "preferences", OnOff(S.bFeelFov) + " (field of view on heavy hits)");
						B.Tile("set:feelbeat", "Ultimate beat", "preferences", OnOff(S.bFeelBeats) + " (vignette, brief slow-down)");
						B.Tile("set:feeltele", "Ground telegraphs", "preferences", OnOff(S.bFeelTelegraph) + " (your own windups)");
						B.Tile("set:captions", "Sound captions", "preferences", OnOff(S.bSoundCaptions) + " (shows where sounds come from)");
						B.Tile("set:confirm", "Confirm Diamond spending", "diamond", OnOff(S.bConfirmSpend) + " (tap twice to spend)");
						B.Tile("set:notices", "Notices at login", "notice", OnOff(S.bNoticesAtLogin) + " (open new notices)");
						B.Tile("set:hud:-1", "Text size -", "preferences", Percent(S.HudScale) + " (battle HUD, arcade)");
						B.Tile("set:hud:1", "Text size +", "preferences", Percent(S.HudScale));
						B.Tile("set:offset:-1", "Rhythm timing -10 ms", "rhythm", std::to_string(S.RhythmOffsetMs) + " ms");
						B.Tile("set:offset:1", "Rhythm timing +10 ms", "rhythm", std::to_string(S.RhythmOffsetMs) + " ms");
					}
					else if (Section == 4)
					{
						const int Shown = S.LobbyOperator >= 0 ? S.LobbyOperator : Profile.Team[0];
						B.Line("Lobby customisation: the six currencies in the top bar (tap a slot to change it), the lobby operator and your commander name.");
						for (int Slot = 0; Slot < TopBarSlots; ++Slot)
						{
							const ECurrency Currency = static_cast<ECurrency>(std::clamp(S.TopBar[Slot], 0, TopBarChoiceCount - 1));
							B.Tile("set:topbar:" + std::to_string(Slot), "Top bar slot " + std::to_string(Slot + 1), CurrencyIcon(Currency), CurrencyName(Currency));
						}
						B.Tile("pref:lobby", "Lobby operator", "profile", OpName(Shown) + " (tap to cycle)");
						B.Tile("pref:name", "Commander name", "profile", M.Name + " (tap to rename)");
						B.Tile("set:transitions", "Page transitions", "preferences", S.bPageTransitions ? "On (0.3 s fade)" : "Off");
						B.Tile("set:defaults", "Restore default settings", "back", "everything except key bindings");
					}
					else
					{
						if (Profile.Account.bSignedIn)
						{
							B.Line("Signed in as " + Profile.Account.Username + " (" + Profile.Account.Email + "), user id " + Profile.Account.UserId + ".");
						}
						else if (Profile.Account.bGuest)
						{
							B.Line("Playing as a guest: progress is saved on this device only.");
						}
						if (Profile.Account.bSignedIn || Profile.Account.bGuest)
						{
							B.Tile("acct:signout", Profile.Account.bSignedIn ? "Sign out" : "Leave guest mode", "cross",
								Profile.Account.bSignedIn ? Profile.Account.Username + " - progress is saved first" : "back to the sign-in screen");
						}
						B.Line("Demo tools (stand-ins for real play and real purchases):");
						B.Tile("demo:currency", "Grant currency", "gold", "+Gold, Bytes, hearts, tickets");
						B.Tile("demo:day", "Next day", "event", "daily reset (weekly every 7th day)");
						B.Tile("demo:xp", "+1,000 Commander XP", "xp");
						B.Tile("demo:season", "+500 Season XP", "seasonpass");
						B.Tile("demo:hours", "+12 Outer World hours", "b_shelter");
						B.Tile("demo:reset", "Reset profile", "cross", Profile.Account.bAdmin ? "back to a fresh admin profile" : "starts a new game");
					}
					for (int Index = 0; Index < 6; ++Index)
					{
						B.Button("set:tab:" + std::to_string(Index), Sections[Index], "", true, Index == Section);
					}
					return true;
				}
				if (P == "keys")
				{
					const int Group = std::clamp(State.KeyGroup, 0, 2);
					static const char* const Groups[3] = {"Battle", "Night Shift", "Rhythm"};
					Page.Title = "Key bindings";
					Page.Subtitle = std::string(Groups[Group]) + " - tap an action, then press the new key (Esc cancels). A key already used in this group swaps.";
					const int First = Group == 0 ? 0 : (Group == 1 ? NightBindFirst : RhythmBindFirst);
					const int Last = Group == 0 ? NightBindFirst : (Group == 1 ? RhythmBindFirst : KeyBindCount);
					for (int Index = First; Index < Last; ++Index)
					{
						const bool bChanged = !M.Settings.KeyBinds[Index].empty();
						B.Tile("keys:bind:" + std::to_string(Index), KeyBindName(Index), "keys", KeyDisplayName(KeyOf(M.Settings, Index)) + (bChanged ? " (changed)" : ""), true, bChanged);
					}
					for (int Index = 0; Index < 3; ++Index)
					{
						B.Button("keys:group:" + std::to_string(Index), Groups[Index], "", true, Index == Group);
					}
					B.Button("keys:reset", "Reset " + std::string(Groups[Group]));
					return true;
				}
				return false;
			}

			// =============================================================================== revolution pages
			bool BuildRevolutionPage(FProfile& Profile, const FUiState& State, FPage& Page)
			{
				FB B{Page};
				FMeta& M = Profile.Meta;
				const std::string& P = State.Page;
				if (P == "tower")
				{
					Page.Title = "Endless Tower";
					Page.Subtitle = "Two towers that never end and never reset: Operator (3-4 star operators) and Secrets (5-6 star). Rewards for every new floor.";
					for (int Mode = 0; Mode < 2; ++Mode)
					{
						const std::vector<int> Team = TowerTeam(Profile, Mode);
						std::string Names;
						for (int Index : Team)
						{
							Names += (Names.empty() ? "" : ", ") + OpName(Index);
						}
						const int Floor = TowerNextFloor(Profile, Mode);
						B.Line(std::string(Mode == 0 ? "Operator tower" : "Secrets tower") + ": best floor " + std::to_string(M.TowerBest[Mode])
							+ (M.TowerLostAt[Mode] > 0 ? ", last fell on floor " + std::to_string(M.TowerLostAt[Mode]) : "") + ". Team: " + (Names.empty() ? "none eligible" : Names) + ".");
						B.Line("    Floor " + std::to_string(Floor) + " pays: " + RewardText(TowerFloorReward(Mode, Floor)));
						const FActionResult Can = CanStartStage(Profile, TowerStageId(Mode, Floor));
						B.Tile("tower:start:" + std::to_string(Mode), Mode == 0 ? "Operator tower" : "Secrets tower", "tower", "climb floor " + std::to_string(Floor), Can.bOk);
					}
					B.Line("Every 5th floor: Diamonds and a mini-boss. Every 10th: a Regular ticket. Every 25th: a boss, 100 Diamonds and equipment. Every 50th: a Special ticket.");
					return true;
				}
				if (P == "arena")
				{
					const Online::FCache& Net = State.Net;
					const FArenaState& A = M.Arena;
					const int Rank = Arena::RankOf(A.Trophies);
					Page.Title = "Arena";
					Page.Subtitle = Arena::RankName(Rank) + " - " + Num(A.Trophies) + " trophies" + (A.bChampionFloor ? " (Champions protected)" : "") + " - season ends in "
						+ std::to_string(LiveOps::DaysLeftInSeason(Profile.NowUnix)) + " days" + (Net.bOnline ? "" : " - offline: practice rivals");
					const int Next = std::min(Arena::ChampionRank, Rank + 1);
					B.Line("Fights left today: " + std::to_string(M.ArenaAttempts) + " / " + std::to_string(Arena::DailyAttempts) + ".  Record " + std::to_string(M.ArenaWins) + " W / "
						+ std::to_string(M.ArenaLosses) + " L" + (Rank < Arena::ChampionRank ? ".  Next rank (" + Arena::RankName(Next) + ") at " + Num(Next * Arena::TrophiesPerRank) + " trophies." : "."));
					std::string Attack;
					std::string Defence;
					for (int Slot = 0; Slot < 3; ++Slot)
					{
						Attack += (Slot ? ", " : "") + OpName(A.Attack[Slot]);
						Defence += (Slot ? ", " : "") + OpName(A.Defence[Slot]);
					}
					B.Line("Attack: " + Attack + " (power " + Num(SquadPower(Profile, A.Attack)) + ").   Defence: " + Defence + " (power " + Num(SquadPower(Profile, A.Defence)) + ").");
					B.Line("Win: take trophies from the other player (more against a higher rank). Lose: they take yours. Each month everyone drops 3-4 ranks.");
					if (!M.ArenaLog.empty())
					{
						B.Line("Last fight: " + M.ArenaLog);
					}
					for (size_t Index = 0; Index < Net.ArenaServer.Log.size() && Index < 3; ++Index)
					{
						B.Line("Defence: " + Net.ArenaServer.Log[Index]);
					}
					B.Tile("arena:edit:0", "Attack team", "arena", Num(SquadPower(Profile, A.Attack)) + " power");
					B.Tile("arena:edit:1", "Defence team", "tower", Num(SquadPower(Profile, A.Defence)) + " power" + (Net.bOnline ? " (others fight it)" : ""));
					const bool bValid = Arena::TeamIsValid(Profile, A.Attack);
					if (Net.Match.bReady)
					{
						const Arena::FLineup& O = Net.Match.Opponent;
						std::string Names;
						for (const Arena::FFighter& Fighter : O.Team)
						{
							Names += (Names.empty() ? "" : ", ") + OpName(Fighter.Op) + " Lv " + std::to_string(Fighter.Level);
						}
						B.Line("Opponent: " + O.Name + (O.bBot ? " (practice rival)" : "") + " - " + Arena::RankName(Arena::RankOf(O.Trophies)) + ", " + Num(O.Trophies)
							+ " trophies, power " + Num(O.Power) + ": " + Names + ".");
						B.Tile("arena:fight", "FIGHT " + O.Name, "arena", "win +" + std::to_string(Arena::TrophyDelta(A.Trophies, O.Trophies)) + " / lose -"
							+ std::to_string(Arena::TrophyDelta(O.Trophies, A.Trophies)), bValid && M.ArenaAttempts > 0, true);
						B.Tile("arena:find", "Another opponent", "next", "", bValid && M.ArenaAttempts > 0);
					}
					else
					{
						B.Tile("arena:find", "Find opponent", "arena", bValid ? "similar strength, then rank" : "set an attack team first", bValid && M.ArenaAttempts > 0, true);
					}
					const bool bReward = Net.bOnline ? Net.ArenaServer.PendingRank >= 0 : A.PendingRewardRank >= 0;
					if (bReward)
					{
						const int RewardRank = Net.bOnline ? Net.ArenaServer.PendingRank : A.PendingRewardRank;
						B.Tile("arena:claim", "Season reward", "star", Arena::RankName(RewardRank) + ": " + RewardText(Arena::SeasonReward(RewardRank)), true, true, true);
					}
					B.Button("arena:refresh", "Refresh", "", Net.bOnline);
					return true;
				}
				if (P == "arena-team")
				{
					const bool bDef = State.bArenaDefence;
					const int* Team = bDef ? M.Arena.Defence : M.Arena.Attack;
					Page.Title = bDef ? "Defence team" : "Attack team";
					Page.Subtitle = "Tap a slot, then an operator. Power " + Num(SquadPower(Profile, Team)) + (bDef ? ". Other players fight this team while you are away." : ".");
					for (int Slot = 0; Slot < 3; ++Slot)
					{
						const int Op = Team[Slot];
						B.Tile("arena:slot:" + std::to_string(Slot), "Slot " + std::to_string(Slot + 1) + ": " + OpName(Op), Op >= 0 ? "portrait:" + LowerName(Op) : "plus",
							Op >= 0 ? Num(OperatorPower(Profile, Op)) + " power" : "empty", true, Slot == State.ArenaSlot);
					}
					for (int Index = 0; Index < static_cast<int>(Roster().size()) && Page.Grid.size() < static_cast<size_t>(MaxGrid); ++Index)
					{
						if (!Profile.Owns(Index))
						{
							continue;
						}
						const bool bIn = Team[0] == Index || Team[1] == Index || Team[2] == Index;
						B.Tile("arena:put:" + std::to_string(Index), Roster()[static_cast<size_t>(Index)].Name, "portrait:" + LowerName(Index),
							std::string(StarsLabel(Roster()[static_cast<size_t>(Index)].Stars)) + " | BP " + Num(OperatorPower(Profile, Index)), true, bIn);
					}
					B.Button("arena:auto", "Auto-fill", "strongest");
					B.Button("arena:clear", "Empty slot");
					return true;
				}
				return false;
			}

			// =============================================================================== Diamond confirmation
			long long DiamondCost(const FProfile& Profile, const FUiState& State, const std::string& Id)
			{
				const std::vector<std::string> Parts = Split(Id, ':');
				if (Parts.empty())
				{
					return 0;
				}
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				if (Parts[0] == "sum")
				{
					if (Sub == "banner" || Sub == "done" || Sub == "show" || Sub == "tut" || Sub == "mileage")
					{
						return 0;
					}
					int Count = ToInt(Parts, 2, 1);
					int Pay = ToInt(Parts, 3);
					if (Sub == "again")
					{
						Count = State.SummonCount == 10 ? 10 : 1;
						Pay = State.SummonPay;
					}
					return Pay == 1 ? static_cast<long long>(Config::DiamondsPerRoll) * (Count == 10 ? Config::TenPullPaidRolls : 1) : 0;
				}
				if (Parts[0] == "shop" && Sub == "alley")
				{
					const int Index = ToInt(Parts, 2);
					return Index >= 0 && Index < static_cast<int>(AlleyStock().size()) ? AlleyStock()[static_cast<size_t>(Index)].Diamonds : 0;
				}
				if (Parts[0] == "trade" && Sub != "from" && Sub != "to" && Sub != "amt" && Sub != "swap")
				{
					const std::vector<ECurrency>& List = TradeCurrencies();
					const int Count = static_cast<int>(List.size());
					if (Count == 0 || List[static_cast<size_t>(State.TradeFrom % Count)] != ECurrency::Diamonds)
					{
						return 0;
					}
					static const long long Amounts[5] = {1, 10, 100, 1000, 10000};
					return TradeCost(ECurrency::Diamonds, List[static_cast<size_t>(State.TradeTo % Count)], Amounts[std::clamp(State.TradeAmount, 0, 4)]);
				}
				(void)Profile;
				return 0;
			}

			// =============================================================================== actions
			bool Perform(FProfile& Profile, FUiState& State, const std::string& Id, FPerform& Out)
			{
				FMeta& M = Profile.Meta;
				Online::FCache& Net = State.Net;
				const std::vector<std::string> Parts = Split(Id, ':');
				const std::string& Head = Parts[0];
				const std::string Sub = Parts.size() > 1 ? Parts[1] : "";
				auto Done = [&Out](const FPerform& Value)
				{
					Out = Value;
					return true;
				};
				if (Head != "adm" && Head != "gang" && Head != "fr" && Id != State.ConfirmId)
				{
					// any other action cancels a pending "tap again" (the Diamond confirmation is handled in BIBPages.cpp)
				}

				// --------------------------------------------------------- navigation with a refresh
				if (Head == "go")
				{
					std::vector<Online::FCall> Calls;
					if (Sub == "admin" && !IsAdmin(Profile))
					{
						return Done(Note("Only the admin account can open the admin desk.", false));
					}
					if (Sub == "friends" && Net.bOnline)
					{
						Calls.push_back(Online::GetFriends());
					}
					else if (Sub == "gang" && Net.bOnline)
					{
						Calls.push_back(Online::MyGang());
						if (!Net.bInGang)
						{
							Calls.push_back(Online::SearchGangs(Net.SearchText));
						}
					}
					else if ((Sub == "notices" || Sub == "admin") && Net.bOnline)
					{
						Calls.push_back(Online::FetchNotices());
					}
					else if (Sub == "arena" && Net.bOnline)
					{
						Calls.push_back(Online::GetArena());
					}
					else if (Sub == "gang-settings" || Sub == "gang-create")
					{
						if (Sub == "gang-settings" && Net.bInGang)
						{
							Net.NewGangMinLevel = Net.Gang.MinLevel;
							Net.NewGangMinPower = Net.Gang.MinPower;
							Net.bNewGangOpen = Net.Gang.bOpen;
						}
					}
					else if (Sub != "return" && Sub != "keys" && Sub != "gang-members" && Sub != "gang-shop" && Sub != "friends" && Sub != "gang"
						&& Sub != "notices" && Sub != "arena")
					{
						return false;   // an older page: BIBPages.cpp handles it
					}
					State.Page = Sub;
					State.ListOffset = 0;
					State.SelMember = -1;
					FPerform Nav;
					Nav.Calls = Calls;
					return Done(Nav);
				}
				if (Head == "back")
				{
					bool bFound = false;
					const std::string Parent = ParentPage(State.Page, bFound);
					if (!bFound)
					{
						return false;
					}
					State.Page = Parent;
					return Done(Note(""));
				}

				// --------------------------------------------------------- text input answers
				if (Head == "in")
				{
					const std::string Text = Online::Trim(State.InputText);
					State.InputText.clear();
					if (Sub == "cmdname")
					{
						if (Text.empty() || Text.size() > 24)
						{
							return Done(Note("A commander name has 1-24 characters.", false));
						}
						M.Name = Text;
						std::vector<Online::FCall> Calls;
						FPerform Renamed = Note("Commander name: " + Text + ".");
						if (Net.bOnline)
						{
							Renamed.Calls.push_back(Online::PushProfile(Profile));
						}
						return Done(Renamed);
					}
					if (Sub == "rallytrack")
					{
						// Kept even when it cannot be ridden yet, so a long track is never lost to a typo.
						M.ArcadeRecords.RallyTrack = Text.substr(0, static_cast<size_t>(Arcade::RallyCodeMaxLength));
						Arcade::FRallyTrack Built;
						std::string Error;
						if (!Arcade::BuildTrack(M.ArcadeRecords.RallyTrack, "Custom track", Built, Error))
						{
							return Done(Note("Track saved, but it cannot be ridden yet: " + Error, false));
						}
						return Done(Note("Track saved: " + Fixed(Built.FinishX - Built.StartX, 0) + " m, " + std::to_string(Built.Coins) + " coins. Tap Ride your track."));
					}
					if (!Net.bOnline && Sub != "gangsearch")
					{
						return Done(Note(OfflineText(), false));
					}
					if (Sub == "friendadd")
					{
						return Done(Text.empty() ? Note("Type a user id or username.", false) : Send(Online::RequestFriend(Text)));
					}
					if (Sub == "gangsearch")
					{
						return Done(Send(Online::SearchGangs(Text)));
					}
					if (Sub == "gangname")
					{
						if (!Online::ValidGangName(Text))
						{
							return Done(Note("Gang names are 3-20 letters, digits and spaces.", false));
						}
						if (Profile.Get(ECurrency::Gold) < Online::GangCreateGold)
						{
							return Done(Note("Founding a gang costs " + Num(Online::GangCreateGold) + " Gold.", false));
						}
						State.Page = "gang";
						return Done(Send(Online::CreateGang(Text, Net.NewGangMinLevel, Net.NewGangMinPower, Net.bNewGangOpen), "Founding " + Text + "..."));
					}
					if (Sub == "mailtarget")
					{
						Net.Admin.Target = Text;
						return Done(Note("Recipient: user " + Text + "."));
					}
					if (Sub == "mailtitle")
					{
						if (!Text.empty())
						{
							Net.Admin.Title = Text.substr(0, 60);
						}
						return Done(Note(""));
					}
					if (Sub == "mailbody")
					{
						Net.Admin.Body = Text.substr(0, 600);
						return Done(Note(""));
					}
					if (Sub == "noticetitle")
					{
						Net.Admin.NoticeTitle = Text.substr(0, 60);
						return Done(Note(""));
					}
					if (Sub == "noticebody")
					{
						Net.Admin.NoticeBody = Text.substr(0, 2000);
						return Done(Note(""));
					}
					return Done(Note("", false));
				}

				// --------------------------------------------------------- key bindings
				if (Head == "keys")
				{
					if (Sub == "group")
					{
						State.KeyGroup = std::clamp(ToInt(Parts, 2), 0, 2);
						return Done(Note(""));
					}
					if (Sub == "bind")
					{
						const int Index = std::clamp(ToInt(Parts, 2), 0, KeyBindCount - 1);
						FPerform Capture = Note(std::string("Press a key for ") + KeyBindName(Index) + " (Esc cancels).");
						Capture.CaptureKey = Index;
						return Done(Capture);
					}
					if (Sub == "reset")
					{
						const int Group = std::clamp(State.KeyGroup, 0, 2);
						for (int Index = 0; Index < KeyBindCount; ++Index)
						{
							if (GroupOf(Index) == Group)
							{
								M.Settings.KeyBinds[Index].clear();
							}
						}
						return Done(Note("Key bindings reset."));
					}
					return false;
				}
				if (Head == "keyset")
				{
					const int Index = std::clamp(ToInt(Parts, 1), 0, KeyBindCount - 1);
					const std::string Key = State.InputText;
					State.InputText.clear();
					if (Key.empty() || Key == "Escape")
					{
						return Done(Note("Unchanged."));
					}
					bool bClean = Key.size() <= 40;
					for (char Character : Key)
					{
						bClean = bClean && (std::isalnum(static_cast<unsigned char>(Character)) || Character == '_');
					}
					if (!bClean)
					{
						return Done(Note("That key cannot be bound.", false));
					}
					FSettings& S = M.Settings;
					const std::string Old = KeyOf(S, Index);
					const int Clash = EffectiveKeyIndexOwner(S, GroupOf(Index), Key, Index);
					if (Clash >= 0)
					{
						S.KeyBinds[Clash] = Old == DefaultKeyName(Clash) ? std::string() : Old;
					}
					S.KeyBinds[Index] = Key == DefaultKeyName(Index) ? std::string() : Key;
					return Done(Note(std::string(KeyBindName(Index)) + ": " + KeyDisplayName(Key) + (Clash >= 0 ? std::string(" (swapped with ") + KeyBindName(Clash) + ")" : "") + "."));
				}

				// --------------------------------------------------------- settings
				if (Head == "set")
				{
					FSettings& S = M.Settings;
					const int Dir = ToInt(Parts, 2, 1) < 0 ? -1 : 1;
					auto Volume = [Dir](int& Value) { Value = std::clamp(Value + 10 * Dir, 0, 100); };
					std::string Text = "Setting changed.";
					if (Sub == "tab")
					{
						State.SettingsTab = std::clamp(ToInt(Parts, 2), 0, 5);
						return Done(Note(""));
					}
					if (Sub == "master") { Volume(S.MasterVolume); Text = "Master volume " + std::to_string(S.MasterVolume) + "%."; }
					else if (Sub == "music") { Volume(S.MusicVolume); Text = "Music volume " + std::to_string(S.MusicVolume) + "%."; }
					else if (Sub == "sfx") { Volume(S.SfxVolume); Text = "Effects volume " + std::to_string(S.SfxVolume) + "%."; }
					else if (Sub == "voice") { Volume(S.VoiceVolume); Text = "Voice volume " + std::to_string(S.VoiceVolume) + "%."; }
					else if (Sub == "bgaudio") { S.bBackgroundAudio = !S.bBackgroundAudio; Text = "Background sound " + OnOff(S.bBackgroundAudio) + "."; }
					else if (Sub == "gfx")
					{
						S.GraphicsQuality = (S.GraphicsQuality + 1) % 4;
						for (int& Level : S.GraphicsDetail)
						{
							Level = -1;   // a preset sets every group
						}
						Text = std::string("Quality: ") + QualityName(S.GraphicsQuality) + ".";
					}
					else if (Sub == "window") { S.WindowMode = (S.WindowMode + 1) % WindowModeCount; Text = std::string("Window mode: ") + WindowModeName(S.WindowMode) + "."; }
					else if (Sub == "vsync") { S.bVSync = !S.bVSync; Text = std::string("V-sync ") + (S.bVSync ? "on." : "off."); }
					else if (Sub == "fps") { S.FrameRateCap = (S.FrameRateCap + 1) % FrameRateCapCount; Text = "Frame-rate limit changed."; }
					else if (Sub == "scale") { S.ResolutionScale = std::clamp(S.ResolutionScale + 10 * Dir, ResolutionScaleMin, 100); Text = "Render scale " + std::to_string(S.ResolutionScale) + "%."; }
					else if (Sub == "grp")
					{
						// Follow the preset -> Low -> Medium -> High -> Epic -> follow the preset.
						const int Group = std::clamp(ToInt(Parts, 2), 0, GraphicsGroupCount - 1);
						int& Level = S.GraphicsDetail[Group];
						Level = Level >= 3 ? -1 : Level + 1;
						Text = std::string(GraphicsGroupName(Group)) + ": " + (Level < 0 ? std::string("follows Quality (") + QualityName(S.GraphicsQuality) + ")" : std::string(QualityName(Level))) + ".";
					}
					else if (Sub == "weather") { S.WeatherDetail = (std::clamp(S.WeatherDetail, 0, WeatherDetailCount - 1) + 1) % WeatherDetailCount; Text = std::string("Weather effects: ") + WeatherDetailName(S.WeatherDetail) + "."; }
					else if (Sub == "volfog") { S.bVolumetricFog = !S.bVolumetricFog; Text = "Volumetric fog " + OnOff(S.bVolumetricFog) + "."; }
					else if (Sub == "anime") { S.bAnimeLook = !S.bAnimeLook; Text = "Anime look " + OnOff(S.bAnimeLook) + "."; }
					else if (Sub == "dynres") { S.bDynamicResolution = !S.bDynamicResolution; Text = "Dynamic resolution " + OnOff(S.bDynamicResolution) + "."; }
					else if (Sub == "perf")
					{
						S.GraphicsQuality = 0;
						for (int& Level : S.GraphicsDetail)
						{
							Level = -1;
						}
						S.ResolutionScale = 70;
						S.WeatherDetail = 1;
						S.bVolumetricFog = false;
						Text = "Performance mode: Low quality, 70% render scale, light weather, no volumetric fog.";
					}
					else if (Sub == "autodetect") { Text = "Graphics detected for this computer."; }   // the Unreal layer runs the benchmark
					else if (Sub == "showfps") { S.bShowFps = !S.bShowFps; Text = std::string("Frame-rate counter ") + (S.bShowFps ? "on." : "off."); }
					else if (Sub == "sens") { S.MouseSensitivity = std::clamp(S.MouseSensitivity + Dir, 1, 10); Text = "Mouse sensitivity " + std::to_string(S.MouseSensitivity) + "."; }
					else if (Sub == "invert") { S.bInvertY = !S.bInvertY; Text = std::string("Vertical look ") + (S.bInvertY ? "inverted." : "normal."); }
					else if (Sub == "fov") { S.FieldOfView = std::clamp(S.FieldOfView + 5 * Dir, 70, 110); Text = "Field of view " + std::to_string(S.FieldOfView) + " degrees."; }
					else if (Sub == "transitions") { S.bPageTransitions = !S.bPageTransitions; Text = std::string("Page transitions ") + (S.bPageTransitions ? "on." : "off."); }
					else if (Sub == "dmg") { S.bDamageNumbers = !S.bDamageNumbers; Text = "Damage numbers " + OnOff(S.bDamageNumbers) + "."; }
					else if (Sub == "shake") { S.bCameraShake = !S.bCameraShake; Text = "Camera shake " + OnOff(S.bCameraShake) + "."; }
					else if (Sub == "motion")
					{
						S.bReduceMotion = !S.bReduceMotion;
						if (S.bReduceMotion)
						{
							S.bPageTransitions = false;
							S.bCameraShake = false;
						}
						Text = "Reduce motion " + OnOff(S.bReduceMotion) + ".";
					}
					else if (Sub == "feel") { S.CombatFeel = std::clamp(S.CombatFeel + 25 * Dir, 0, 150); Text = "Combat feel " + std::to_string(S.CombatFeel) + "%."; }
					else if (Sub == "feelstop") { S.bFeelHitStop = !S.bFeelHitStop; Text = "Hit stop " + OnOff(S.bFeelHitStop) + "."; }
					else if (Sub == "feelflash") { S.bFeelFlash = !S.bFeelFlash; Text = "Hit flash " + OnOff(S.bFeelFlash) + "."; }
					else if (Sub == "feelfov") { S.bFeelFov = !S.bFeelFov; Text = "View punch " + OnOff(S.bFeelFov) + "."; }
					else if (Sub == "feelbeat") { S.bFeelBeats = !S.bFeelBeats; Text = "Ultimate beat " + OnOff(S.bFeelBeats) + "."; }
					else if (Sub == "feeltele") { S.bFeelTelegraph = !S.bFeelTelegraph; Text = "Ground telegraphs " + OnOff(S.bFeelTelegraph) + "."; }
					else if (Sub == "captions") { S.bSoundCaptions = !S.bSoundCaptions; Text = "Sound captions " + OnOff(S.bSoundCaptions) + "."; }
					else if (Sub == "confirm") { S.bConfirmSpend = !S.bConfirmSpend; Text = "Confirm Diamond spending " + OnOff(S.bConfirmSpend) + "."; }
					else if (Sub == "notices") { S.bNoticesAtLogin = !S.bNoticesAtLogin; Text = "Notices at login " + OnOff(S.bNoticesAtLogin) + "."; }
					else if (Sub == "hud") { S.HudScale = std::clamp(S.HudScale + 5 * Dir, 75, 125); Text = "Text size " + std::to_string(S.HudScale) + "%."; }
					else if (Sub == "offset") { S.RhythmOffsetMs = std::clamp(S.RhythmOffsetMs + 10 * Dir, -200, 200); Text = "Rhythm timing offset " + std::to_string(S.RhythmOffsetMs) + " ms."; }
					else if (Sub == "topbar")
					{
						const int Slot = std::clamp(ToInt(Parts, 2), 0, TopBarSlots - 1);
						S.TopBar[Slot] = (std::clamp(S.TopBar[Slot], 0, TopBarChoiceCount - 1) + 1) % TopBarChoiceCount;
						Text = "Top bar slot " + std::to_string(Slot + 1) + ": " + CurrencyName(static_cast<ECurrency>(S.TopBar[Slot])) + ".";
					}
					else if (Sub == "defaults")
					{
						const int Lobby = S.LobbyOperator;
						FSettings Fresh;
						for (int Index = 0; Index < KeyBindCount; ++Index)
						{
							Fresh.KeyBinds[Index] = S.KeyBinds[Index];
						}
						S = Fresh;
						S.LobbyOperator = Lobby;
						Text = "Settings restored to their defaults.";
					}
					else
					{
						return false;
					}
					return Done(Note(Text));
				}
				if (Head == "pref")
				{
					if (Sub == "hint")
					{
						M.Settings.bShowControlsHint = !M.Settings.bShowControlsHint;
						return Done(Note("Controls line " + OnOff(M.Settings.bShowControlsHint) + "."));
					}
					if (Sub == "name")
					{
						return Done(Ask("cmdname", "Commander name", "1-24 characters", M.Name, 24));
					}
					return false;   // lobby operator cycling stays in BIBPages.cpp
				}

				// --------------------------------------------------------- notices
				if (Head == "ntc")
				{
					if (Sub == "refresh")
					{
						return Done(Net.bOnline ? Send(Online::FetchNotices(), "Refreshing...") : Note(OfflineText(), false));
					}
					if (Sub == "open")
					{
						const int Index = ToInt(Parts, 2, -1);
						if (Index < 0 || Index >= static_cast<int>(Net.Notices.size()))
						{
							return Done(Note("No such notice.", false));
						}
						State.SelNotice = Index;
						State.Page = "notice";
						const std::string& NoticeId = Net.Notices[static_cast<size_t>(Index)].Id;
						if (!NoticeRead(Profile, NoticeId))
						{
							M.ReadNotices.push_back(NoticeId);
							if (M.ReadNotices.size() > 60)
							{
								M.ReadNotices.erase(M.ReadNotices.begin());
							}
						}
						return Done(Note(""));
					}
					return false;
				}

				// --------------------------------------------------------- welcome back
				if (Head == "ret")
				{
					if (Sub == "day")
					{
						return Done(Result(LiveOps::ClaimReturnCheckIn(Profile, ToInt(Parts, 2))));
					}
					if (Sub == "q")
					{
						return Done(Result(LiveOps::ClaimReturnQuest(Profile, ToInt(Parts, 2))));
					}
					if (Sub == "all")
					{
						int Count = 0;
						for (int Pass = 0; Pass < 2; ++Pass)
						{
							for (int Day = 1; Day <= ReturnEventDays; ++Day)
							{
								Count += LiveOps::ClaimReturnCheckIn(Profile, Day).bOk ? 1 : 0;
							}
							for (int Index = 0; Index < ReturnQuestCount; ++Index)
							{
								Count += LiveOps::ClaimReturnQuest(Profile, Index).bOk ? 1 : 0;
							}
						}
						return Done(Note(Count > 0 ? "Claimed " + std::to_string(Count) + " welcome-back rewards." : "Nothing to claim.", Count > 0));
					}
					return false;
				}

				// --------------------------------------------------------- friends
				if (Head == "fr")
				{
					if (!Net.bOnline)
					{
						return Done(Note(OfflineText(), false));
					}
					const int Index = ToInt(Parts, 2, -1);
					auto At = [Index](const std::vector<Online::FCard>& List) -> const Online::FCard* {
						return Index >= 0 && Index < static_cast<int>(List.size()) ? &List[static_cast<size_t>(Index)] : nullptr;
					};
					if (Sub == "add")
					{
						return Done(Ask("friendadd", "Add a friend", "Their user id (8 digits) or username"));
					}
					if (Sub == "refresh")
					{
						return Done(Send(Online::GetFriends(), "Refreshing..."));
					}
					if (Sub == "acc" && At(Net.Incoming))
					{
						return Done(Send(Online::AcceptFriend(At(Net.Incoming)->Uid)));
					}
					if (Sub == "dec" && At(Net.Incoming))
					{
						return Done(Send(Online::DeclineFriend(At(Net.Incoming)->Uid)));
					}
					if (Sub == "cancel" && At(Net.Outgoing))
					{
						return Done(Send(Online::DeclineFriend(At(Net.Outgoing)->Uid)));
					}
					if (Sub == "sel" && At(Net.Friends))
					{
						State.SelFriend = Index;
						State.Page = "friend";
						return Done(Note(""));
					}
					if (Sub == "remove" && State.SelFriend >= 0 && State.SelFriend < static_cast<int>(Net.Friends.size()))
					{
						if (!Confirmed(State, Id))
						{
							return Done(Note("Tap again to remove " + Net.Friends[static_cast<size_t>(State.SelFriend)].Name + "."));
						}
						const std::string Uid = Net.Friends[static_cast<size_t>(State.SelFriend)].Uid;
						State.Page = "friends";
						return Done(Send(Online::RemoveFriend(Uid)));
					}
					return Done(Note("That entry is gone; refresh the list.", false));
				}

				// --------------------------------------------------------- gangs
				if (Head == "gang")
				{
					if (Sub == "buy")
					{
						return Done(Result(Online::BuyGangShop(Profile, ToInt(Parts, 2), Net.bInGang ? Net.Gang.Level : 1)));
					}
					if (Sub == "minlv" || Sub == "minbp" || Sub == "open")
					{
						if (Sub == "minlv") Net.NewGangMinLevel = NextPreset(MinLevels, Net.NewGangMinLevel);
						if (Sub == "minbp") Net.NewGangMinPower = NextPreset(MinPowers, Net.NewGangMinPower);
						if (Sub == "open") Net.bNewGangOpen = !Net.bNewGangOpen;
						return Done(Note(""));
					}
					if (!Net.bOnline)
					{
						return Done(Note(OfflineText(), false));
					}
					if (Sub == "refresh")
					{
						FPerform Refresh = Send(Online::MyGang(), "Refreshing...");
						if (!Net.bInGang)
						{
							Refresh.Calls.push_back(Online::SearchGangs(Net.SearchText));
						}
						return Done(Refresh);
					}
					if (Sub == "search")
					{
						return Done(Ask("gangsearch", "Find a gang", "Part of the name (empty = the most active gangs)", Net.SearchText));
					}
					if (Sub == "new")
					{
						return Done(Ask("gangname", "Name your gang", "3-20 letters, digits and spaces", "", 20));
					}
					if (Sub == "join")
					{
						const int Index = ToInt(Parts, 2, -1);
						if (Index < 0 || Index >= static_cast<int>(Net.Search.size()))
						{
							return Done(Note("That gang is gone; search again.", false));
						}
						const Online::FGangSummary& G = Net.Search[static_cast<size_t>(Index)];
						if (CommanderLevel(Profile) < G.MinLevel || TeamBattlePower(Profile) < G.MinPower)
						{
							return Done(Note(G.Name + " needs commander level " + std::to_string(G.MinLevel) + " and squad power " + Num(G.MinPower) + ".", false));
						}
						return Done(Send(Online::JoinGang(G.Id, CommanderLevel(Profile), TeamBattlePower(Profile)), "Joining " + G.Name + "..."));
					}
					if (Sub == "save")
					{
						State.Page = "gang";
						return Done(Send(Online::GangSettings(Net.NewGangMinLevel, Net.NewGangMinPower, Net.bNewGangOpen)));
					}
					if (Sub == "checkin")
					{
						return Done(Send(Online::Contribute("checkin", 50)));
					}
					if (Sub == "donate")
					{
						if (Profile.Get(ECurrency::Gold) < Online::GangDonateGold)
						{
							return Done(Note("A donation is " + Num(Online::GangDonateGold) + " Gold.", false));
						}
						return Done(Send(Online::Contribute("donate", 10)));
					}
					if (Sub == "ms")
					{
						const int Index = std::clamp(ToInt(Parts, 2), 0, Online::GangMilestoneCount - 1);
						return Done(Send(Online::ClaimGangMilestone(Index)));
					}
					if (Sub == "claim")
					{
						return Done(Send(Online::ClaimGangCoins()));
					}
					if (Sub == "leave")
					{
						if (!Confirmed(State, Id))
						{
							return Done(Note("Tap again to leave " + Net.Gang.Name + ". Your season points stay with the gang."));
						}
						return Done(Send(Online::LeaveGang()));
					}
					if (Sub == "member")
					{
						State.SelMember = ToInt(Parts, 2, -1);
						return Done(Note(""));
					}
					if (Sub == "kick" && State.SelMember >= 0 && State.SelMember < static_cast<int>(Net.Gang.MemberList.size()))
					{
						if (!Confirmed(State, Id))
						{
							return Done(Note("Tap again to remove " + Net.Gang.MemberList[static_cast<size_t>(State.SelMember)].Name + "."));
						}
						const std::string Uid = Net.Gang.MemberList[static_cast<size_t>(State.SelMember)].Uid;
						State.SelMember = -1;
						return Done(Send(Online::KickMember(Uid)));
					}
					return Done(Note("Unknown gang action.", false));
				}

				// --------------------------------------------------------- admin desk
				if (Head == "adm")
				{
					if (!IsAdmin(Profile))
					{
						return Done(Note("Only the admin account can do that.", false));
					}
					Online::FAdminCompose& A = Net.Admin;
					if (Sub == "to") { A.bToAll = !A.bToAll; return Done(Note(A.bToAll ? "Mail goes to every player." : "Mail goes to one player.")); }
					if (Sub == "target") return Done(Ask("mailtarget", "Recipient", "The player's user id (8 digits)", A.Target, 12));
					if (Sub == "title") return Done(Ask("mailtitle", "Mail title", "Up to 60 characters", A.Title, 60));
					if (Sub == "body") return Done(Ask("mailbody", "Mail message", "Up to 600 characters", A.Body, 600));
					if (Sub == "reward") { A.RewardPreset = (A.RewardPreset + 1) % Online::RewardPresetCount(); return Done(Note("Attachment: " + RewardText(Online::RewardPreset(A.RewardPreset)) + ".")); }
					if (Sub == "send")
					{
						if (!A.bToAll && A.Target.empty())
						{
							return Done(Note("Choose the recipient's user id first.", false));
						}
						if (!Confirmed(State, Id))
						{
							return Done(Note(std::string("Tap again to send \"") + A.Title + "\" to " + (A.bToAll ? "EVERY player" : "user " + A.Target) + "."));
						}
						return Done(Send(Online::AdminSendMail(A), "Sending..."));
					}
					if (Sub == "ncat") { A.NoticeCategory = (A.NoticeCategory + 1) % Online::NoticeCategoryCount; return Done(Note("")); }
					if (Sub == "ntitle") return Done(Ask("noticetitle", "Notice title", "Up to 60 characters", A.NoticeTitle, 60));
					if (Sub == "nbody") return Done(Ask("noticebody", "Notice text", "Up to 2,000 characters", A.NoticeBody, 2000));
					if (Sub == "npin") { A.bPinned = !A.bPinned; return Done(Note("")); }
					if (Sub == "post")
					{
						if (A.NoticeTitle.empty() || A.NoticeBody.empty())
						{
							return Done(Note("A notice needs a title and a text.", false));
						}
						FPerform Posted = Send(Online::AdminPostNotice(A), "Publishing...");
						A.NoticeTitle.clear();
						A.NoticeBody.clear();
						A.bPinned = false;
						return Done(Posted);
					}
					if (Sub == "ndel")
					{
						const int Index = ToInt(Parts, 2, -1);
						if (Index < 0 || Index >= static_cast<int>(Net.Notices.size()))
						{
							return Done(Note("That notice is gone.", false));
						}
						if (!Confirmed(State, Id))
						{
							return Done(Note("Tap again to delete \"" + Net.Notices[static_cast<size_t>(Index)].Title + "\"."));
						}
						return Done(Send(Online::AdminDeleteNotice(Net.Notices[static_cast<size_t>(Index)].Id)));
					}
					return Done(Note("Unknown admin action.", false));
				}

				// --------------------------------------------------------- arcade
				if (Head == "arc")
				{
					// Night Shift left the arcade (owner request 2026-10-15); its rules stay as the base of Ultimate Custom Night.
					if (Sub == "rhythm")
					{
						State.Page = "arcade-rhythm";
						return Done(Note(""));
					}
					if (Sub == "rally")
					{
						State.Page = "arcade-rally";
						return Done(Note(""));
					}
					if (Sub == "rallycar")
					{
						int& Vehicle = M.ArcadeRecords.RallyVehicle;
						Vehicle = Vehicle == 1 ? 0 : 1;
						return Done(Note(std::string("Ark Rally vehicle: ") + Arcade::RallyVehicleName(static_cast<Arcade::ERallyVehicle>(Vehicle)) + "."));
					}
					if (Sub == "rallygo")
					{
						const int D = std::clamp(ToInt(Parts, 2), 0, Arcade::DifficultyCount - 1);
						const FActionResult Started = Arcade::StartGame(Profile, Arcade::EGame::ArkRally);
						if (!Started.bOk)
						{
							return Done(Result(Started));
						}
						FPerform Open = Note(Started.Message);
						Open.Overlay = EOverlay::ArkRally;
						Open.OverlayA = D;
						Open.OverlayB = 0;
						return Done(Open);
					}
					if (Sub == "rallytry")
					{
						Arcade::FRallyTrack Built;
						std::string Error;
						if (!Arcade::BuildTrack(M.ArcadeRecords.RallyTrack, "Custom track", Built, Error))
						{
							return Done(Note(M.ArcadeRecords.RallyTrack.empty() ? "Write a track first." : "Your track cannot be ridden: " + Error, false));
						}
						FPerform Open = Note("Free practice on your track.");
						Open.Overlay = EOverlay::ArkRally;
						Open.OverlayA = static_cast<int>(Arcade::EDifficulty::Medium);
						Open.OverlayB = 1;
						return Done(Open);
					}
					if (Sub == "rallyedit")
					{
						const std::string& Current = M.ArcadeRecords.RallyTrack;
						return Done(Ask("rallytrack", "Your Ark Rally track",
							"F<len> flat, H<len>,<h> hill, U<len>,<d> slope, R<len>,<h> ramp, G<len> gap, W<len>,<a> bumps, C<n> coins, B bomb, S spring, P boost, K checkpoint",
							Current.empty() ? std::string(Arcade::RallyTrackCode(Arcade::EDifficulty::Easy)) : Current, Arcade::RallyCodeMaxLength));
					}
					if (Sub == "rallycopy")
					{
						const int D = std::clamp(ToInt(Parts, 2), 0, Arcade::DifficultyCount - 1);
						if (!M.ArcadeRecords.RallyTrack.empty() && State.ConfirmId != Id)
						{
							State.ConfirmId = Id;
							return Done(Note("Your track will be replaced: tap again to confirm."));
						}
						State.ConfirmId.clear();
						M.ArcadeRecords.RallyTrack = Arcade::RallyTrackCode(static_cast<Arcade::EDifficulty>(D));
						return Done(Note(std::string("Your track now starts from ") + Arcade::RallyTrackName(static_cast<Arcade::EDifficulty>(D)) + ". Edit it, then ride it."));
					}
					if (Sub == "halo" || Sub == "memory" || Sub == "breach")
					{
						State.Page = "arcade-" + Sub;
						return Done(Note(""));
					}
					if (Sub == "halogo" || Sub == "memorygo" || Sub == "breachgo")
					{
						const int D = std::clamp(ToInt(Parts, 2), 0, Arcade::DifficultyCount - 1);
						const Arcade::EGame Game = Sub == "halogo" ? Arcade::EGame::HaloFall : (Sub == "memorygo" ? Arcade::EGame::NeonMemory : Arcade::EGame::FirewallBreach);
						const FActionResult Started = Arcade::StartGame(Profile, Game);
						if (!Started.bOk)
						{
							return Done(Result(Started));
						}
						FPerform Open = Note(Started.Message);
						Open.Overlay = Sub == "halogo" ? EOverlay::HaloFall : (Sub == "memorygo" ? EOverlay::NeonMemory : EOverlay::FirewallBreach);
						Open.OverlayA = D;
						return Done(Open);
					}
					if (Sub == "ucn")
					{
						if (!Arcade::Ucn::IsUnlocked(Profile))
						{
							return Done(Note("Ultimate Custom Night unlocks when you clear Chapter 13.", false));
						}
						State.Page = "arcade-ucn";
						return Done(Note(""));
					}
					if (Sub == "ucngo")
					{
						const int Mode = std::clamp(ToInt(Parts, 2), 0, Arcade::Ucn::ModeCount - 1);
						const FActionResult Started = Arcade::Ucn::StartUcn(Profile, static_cast<Arcade::Ucn::EMode>(Mode));
						if (!Started.bOk)
						{
							return Done(Result(Started));
						}
						FPerform Open = Note(Started.Message);
						Open.Overlay = EOverlay::UltimateNight;
						Open.OverlayA = Mode;
						return Done(Open);
					}
					if (Sub == "rdiff")
					{
						State.RhythmDiff = std::clamp(ToInt(Parts, 2), 0, Arcade::DifficultyCount - 1);
						return Done(Note(""));
					}
					if (Sub == "song")
					{
						const int Song = std::clamp(ToInt(Parts, 2), 0, Arcade::SongCount() - 1);
						const FActionResult Started = Arcade::StartGame(Profile, Arcade::EGame::Rhythm);
						if (!Started.bOk)
						{
							return Done(Result(Started));
						}
						FPerform Open = Note(Started.Message);
						Open.Overlay = EOverlay::Rhythm;
						Open.OverlayA = Song;
						Open.OverlayB = std::clamp(State.RhythmDiff, 0, Arcade::DifficultyCount - 1);
						return Done(Open);
					}
					if (Sub == "boss")
					{
						const int StageId = Arcade::BossRushStageId(std::clamp(Profile.StagesCleared, 1, TotalCampaignLevels), 1);
						const FActionResult Can = CanStartStage(Profile, StageId);
						if (!Can.bOk)
						{
							return Done(Result(Can));
						}
						const FActionResult Started = Arcade::StartGame(Profile, Arcade::EGame::BossRush);
						if (!Started.bOk)
						{
							return Done(Result(Started));
						}
						FPerform Go = Note("Boss Rush: round 1. Beat the bosses one after another - every round pays.");
						Go.bStartStage = true;
						Go.StageId = StageId;
						return Done(Go);
					}
					return false;
				}

				// --------------------------------------------------------- arena
				if (Head == "arena")
				{
					FArenaState& A = M.Arena;
					if (Sub == "edit")
					{
						State.bArenaDefence = ToInt(Parts, 2) == 1;
						State.ArenaSlot = 0;
						State.Page = "arena-team";
						int* Team = State.bArenaDefence ? A.Defence : A.Attack;
						if (Team[0] < 0 && Team[1] < 0 && Team[2] < 0)
						{
							for (int Slot = 0; Slot < 3; ++Slot)
							{
								Team[Slot] = Profile.Team[Slot];
							}
						}
						return Done(Note(""));
					}
					if (Sub == "slot")
					{
						State.ArenaSlot = std::clamp(ToInt(Parts, 2), 0, 2);
						return Done(Note(""));
					}
					if (Sub == "put" || Sub == "clear" || Sub == "auto")
					{
						const FActionResult Changed = Sub == "auto" ? Arena::AutoFill(Profile, State.bArenaDefence)
							: Arena::SetSlot(Profile, State.bArenaDefence, State.ArenaSlot, Sub == "put" ? ToInt(Parts, 2, -1) : -1);
						if (Changed.bOk && Sub == "put")
						{
							State.ArenaSlot = (State.ArenaSlot + 1) % 3;
						}
						FPerform Saved = Result(Changed);
						if (Changed.bOk && State.bArenaDefence && Net.bOnline)
						{
							Saved.Calls.push_back(Online::PutDefence(Profile));
						}
						return Done(Saved);
					}
					if (Sub == "refresh")
					{
						return Done(Net.bOnline ? Send(Online::GetArena(), "Refreshing...") : Note(OfflineText(), false));
					}
					if (Sub == "claim")
					{
						if (Net.bOnline)
						{
							return Done(Net.ArenaServer.PendingRank >= 0 ? Send(Online::ClaimArenaSeason()) : Note("No arena season reward to claim.", false));
						}
						return Done(Result(Arena::ClaimSeasonReward(Profile)));
					}
					if (M.ArenaAttempts <= 0)
					{
						return Done(Note("No arena fights left today (" + std::to_string(Arena::DailyAttempts) + " a day).", false));
					}
					if (!Arena::TeamIsValid(Profile, A.Attack))
					{
						return Done(Note("Set an attack team first (Attack team).", false));
					}
					if (Sub == "find")
					{
						if (Net.bOnline)
						{
							return Done(Send(Online::FindMatch(SquadPower(Profile, A.Attack)), "Looking for an opponent of similar strength..."));
						}
						Net.Match = Online::FMatch();
						Net.Match.Opponent = Arena::MakeRival(Profile, static_cast<uint32_t>(Profile.NowUnix) + static_cast<uint32_t>(M.ArenaWins * 31 + M.ArenaLosses * 17 + A.Trophies));
						Net.Match.bReady = true;
						return Done(Note("Practice rival found: " + Net.Match.Opponent.Name + "."));
					}
					if (Sub == "fight")
					{
						if (!Net.Match.bReady)
						{
							return Done(Note("Find an opponent first.", false));
						}
						const Arena::FLineup Mine = Arena::MakeLineup(Profile, A.Attack, M.Name);
						const Arena::FLineup Theirs = Net.Match.Opponent;
						uint32_t Seed = 2166136261u;
						for (char Character : Net.Match.MatchId + Theirs.Name)
						{
							Seed = (Seed ^ static_cast<unsigned char>(Character)) * 16777619u;
						}
						Seed += static_cast<uint32_t>(M.ArenaWins + M.ArenaLosses);
						const Arena::FBattle Battle = Arena::Simulate(Mine, Theirs, Seed);
						State.LastBattle = Battle;
						State.LastAttack = Mine;
						State.LastDefence = Theirs;
						FPerform Fight;
						Fight.Overlay = EOverlay::ArenaBattle;
						if (Net.bOnline && !Net.Match.MatchId.empty())
						{
							Fight.Calls.push_back(Online::ReportResult(Net.Match.MatchId, Battle.bAttackerWon));
							Fight.Message = Battle.bAttackerWon ? "Victory! Updating trophies..." : "Defeat. Updating trophies...";
						}
						else
						{
							const int Delta = Battle.bAttackerWon ? Arena::TrophyDelta(A.Trophies, Theirs.Trophies) : -Arena::TrophyDelta(Theirs.Trophies, A.Trophies);
							bool bFloor = A.bChampionFloor;
							const int Trophies = Arena::ApplyTrophies(A.Trophies, Delta, bFloor);
							A.bChampionFloor = bFloor;
							Fight.Message = Arena::RecordFight(Profile, Theirs, Battle.bAttackerWon, Trophies, Trophies - A.Trophies);
							Net.Match = Online::FMatch();
						}
						return Done(Fight);
					}
					return false;
				}
				return false;
			}
		}

		// ===================================================================================== shared helpers
		const char* CurrencyIcon(ECurrency Currency)
		{
			switch (Currency)
			{
			case ECurrency::Gold: return "gold";
			case ECurrency::Diamonds: return "diamond";
			case ECurrency::Bytes: return "bytes";
			case ECurrency::AngelHearts: return "hearts";
			case ECurrency::RegularTicket: return "ticket_regular";
			case ECurrency::SpecialTicket: return "ticket_special";
			case ECurrency::DailyTicket: return "ticket_daily";
			case ECurrency::Mileage: return "mileage";
			case ECurrency::ArcadeToken: return "arcade_token";
			case ECurrency::GangCoin: return "gang_coin";
			case ECurrency::Charge: return "charge";
			case ECurrency::ElementCore: return "element_core";
			case ECurrency::EventToken: return "event_token";
			case ECurrency::ArkMedal: return "ark_medal";
			case ECurrency::Count: break;
			}
			return "";
		}

		int UnreadNotices(const FProfile& Profile, const FUiState& State)
		{
			int Count = 0;
			for (const Online::FNotice& Notice : State.Net.Notices)
			{
				const std::vector<std::string>& Read = Profile.Meta.ReadNotices;
				Count += std::find(Read.begin(), Read.end(), Notice.Id) == Read.end() ? 1 : 0;
			}
			return Count;
		}
	}
}
