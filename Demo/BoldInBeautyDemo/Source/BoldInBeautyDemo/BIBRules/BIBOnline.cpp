// Boldinbeauty demo — online features, game side (see BIBOnline.h and docs/online-api.md). Amounts are PLACEHOLDER.
#include "BIBOnline.h"

#include "BIBCore.h"
#include "BIBLiveOps.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace BIB
{
	namespace Online
	{
		using Json::FValue;

		namespace
		{
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

			FCall Make(ECall Kind, const char* Verb, const std::string& Path, const FValue* Body = nullptr)
			{
				FCall Call;
				Call.Kind = Kind;
				Call.Verb = Verb;
				Call.Path = Path;
				if (Body)
				{
					Call.Body = Json::Write(*Body);
				}
				else if (Call.Verb != "GET")
				{
					Call.Body = "{}";
				}
				return Call;
			}

			FValue Str(const std::string& Text)
			{
				return FValue::MakeString(Text);
			}

			FValue Num(double Value)
			{
				return FValue::MakeNumber(Value);
			}

			std::string IdString(const FValue& Value, const char* Key)
			{
				const FValue* Field = Value.Find(Key);
				if (!Field)
				{
					return "";
				}
				if (Field->IsString())
				{
					return Field->AsString();
				}
				if (Field->IsNumber())
				{
					return std::to_string(Field->AsInt64());
				}
				return "";
			}

			FCard ParseCard(const FValue& V)
			{
				FCard Card;
				Card.Uid = IdString(V, "uid");
				Card.Name = V.GetString("name");
				Card.Level = V.GetInt("level");
				Card.Power = V.GetInt("power");
				Card.LastSeen = V.GetString("lastSeen");
				Card.Gang = V.GetString("gang");
				return Card;
			}

			std::vector<FCard> ParseCards(const FValue& Reply, const char* Key)
			{
				std::vector<FCard> Cards;
				if (const FValue* List = Reply.Find(Key))
				{
					for (const FValue& Item : List->Items())
					{
						Cards.push_back(ParseCard(Item));
					}
				}
				return Cards;
			}

			void ParseSummary(const FValue& V, FGangSummary& Out)
			{
				Out.Id = IdString(V, "id");
				Out.Name = V.GetString("name");
				Out.LeaderName = V.GetString("leaderName");
				Out.Level = std::max(1, V.GetInt("level", 1));
				const FValue* Members = V.Find("members");
				Out.Members = Members && Members->IsNumber() ? Members->AsInt() : (Members && Members->IsArray() ? static_cast<int>(Members->Size()) : 0);
				Out.MaxMembers = V.GetInt("maxMembers", GangMaxMembers);
				Out.MinLevel = V.GetInt("minLevel");
				Out.MinPower = V.GetInt("minPower");
				Out.bOpen = V.GetBool("open", true);
			}

			FGang ParseGang(const FValue& V)
			{
				FGang Gang;
				ParseSummary(V, Gang);
				Gang.Xp = V.GetInt("xp");
				Gang.SeasonPoints = V.GetInt("seasonPoints");
				Gang.Season = V.GetString("season");
				Gang.MyRole = V.GetString("myRole", "member");
				if (const FValue* Claimed = V.Find("milestonesClaimed"))
				{
					for (const FValue& Item : Claimed->Items())
					{
						Gang.MilestonesClaimed.push_back(Item.AsInt());
					}
				}
				if (const FValue* Members = V.Find("members"))
				{
					if (Members->IsArray())
					{
						for (const FValue& Item : Members->Items())
						{
							FGangMember Member;
							Member.Uid = IdString(Item, "uid");
							Member.Name = Item.GetString("name");
							Member.Role = Item.GetString("role", "member");
							Member.Level = Item.GetInt("level");
							Member.Power = Item.GetInt("power");
							Member.Contribution = Item.GetInt("contribution");
							Member.SeasonContribution = Item.GetInt("seasonContribution");
							Member.LastSeen = Item.GetString("lastSeen");
							Gang.MemberList.push_back(Member);
						}
						Gang.Members = static_cast<int>(Gang.MemberList.size());
					}
				}
				if (Gang.Level <= 1)
				{
					Gang.Level = GangLevelFromXp(Gang.Xp);
				}
				return Gang;
			}

			FValue FighterJson(const Arena::FFighter& Fighter)
			{
				FValue V = FValue::MakeObject();
				V.Set("op", Num(Fighter.Op));
				V.Set("level", Num(Fighter.Level));
				V.Set("stars", Num(Fighter.Stars));
				V.Set("hp", Num(std::round(Fighter.Hp)));
				V.Set("attack", Num(std::round(Fighter.Attack)));
				V.Set("defence", Num(Fighter.Defence));
				V.Set("power", Num(Fighter.Power));
				return V;
			}

			Arena::FLineup ParseLineup(const FValue& V)
			{
				Arena::FLineup Lineup;
				Lineup.Uid = IdString(V, "uid");
				Lineup.Name = V.GetString("name", "Commander");
				Lineup.Trophies = V.GetInt("trophies");
				Lineup.Power = V.GetInt("power");
				if (const FValue* Team = V.Find("team"))
				{
					for (const FValue& Item : Team->Items())
					{
						Arena::FFighter Fighter;
						Fighter.Op = std::clamp(Item.GetInt("op"), 0, static_cast<int>(Roster().size()) - 1);
						Fighter.Level = std::max(1, Item.GetInt("level", 1));
						Fighter.Stars = std::max(0, Item.GetInt("stars"));
						Fighter.Hp = std::max(1.0, Item.GetNumber("hp", 1.0));
						Fighter.Attack = std::max(1.0, Item.GetNumber("attack", 1.0));
						Fighter.Defence = std::clamp(Item.GetNumber("defence"), 0.0, 75.0);
						Fighter.Power = std::max(0, Item.GetInt("power"));
						if (Lineup.Team.size() < 3)
						{
							Lineup.Team.push_back(Fighter);
						}
					}
				}
				if (Lineup.Power <= 0)
				{
					for (const Arena::FFighter& Fighter : Lineup.Team)
					{
						Lineup.Power += Fighter.Power;
					}
				}
				return Lineup;
			}

			const int* DefenceTeam(const FProfile& Profile)
			{
				const int* Defence = Profile.Meta.Arena.Defence;
				const bool bEmpty = Defence[0] < 0 && Defence[1] < 0 && Defence[2] < 0;
				return bEmpty ? Profile.Team : Defence;
			}
		}

		const char* CallName(ECall Kind)
		{
			static const char* Names[static_cast<int>(ECall::Count)] = {"profile", "friends", "friend request", "friend accept", "friend decline",
				"friend remove", "gang search", "my gang", "gang create", "gang join", "gang leave", "gang kick", "gang settings", "gang contribute",
				"gang claim", "gang milestone", "mail", "mail ack", "admin mail", "notices", "admin notice", "admin notice delete", "arena defence",
				"arena", "arena match", "arena result", "arena claim"};
			const int Index = static_cast<int>(Kind);
			return Index >= 0 && Index < static_cast<int>(ECall::Count) ? Names[Index] : "?";
		}

		// ============================================================================ builders
		FCall PushProfile(const FProfile& Profile)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("level", Num(Meta::CommanderLevel(Profile)));
			Body.Set("power", Num(TeamBattlePower(Profile)));
			const int Lobby = Profile.Meta.Settings.LobbyOperator >= 0 ? Profile.Meta.Settings.LobbyOperator : std::max(0, Profile.Team[0]);
			Body.Set("lobbyOp", Num(std::clamp(Lobby, 0, static_cast<int>(Roster().size()) - 1)));
			FValue Team = FValue::MakeArray();
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				const int Op = Profile.Team[Slot];
				if (Op >= 0 && Profile.Owns(Op))
				{
					FValue Item = FValue::MakeObject();
					Item.Set("op", Num(Op));
					Item.Set("level", Num(Profile.Operators[static_cast<size_t>(Op)].Level));
					Item.Set("stars", Num(Profile.Operators[static_cast<size_t>(Op)].UpgradeStars));
					Item.Set("power", Num(OperatorPower(Profile, Op)));
					Team.Push(Item);
				}
			}
			Body.Set("team", Team);
			FCall Call = Make(ECall::Profile, "PUT", "/v1/profile", &Body);
			Call.bQuiet = true;
			return Call;
		}

		FCall GetFriends()
		{
			FCall Call = Make(ECall::Friends, "GET", "/v1/friends");
			Call.bQuiet = true;
			return Call;
		}

		FCall RequestFriend(const std::string& To)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("to", Str(Trim(To)));
			FCall Call = Make(ECall::FriendRequest, "POST", "/v1/friends/request", &Body);
			Call.Text = Trim(To);
			return Call;
		}

		static FCall UidCall(ECall Kind, const char* Path, const std::string& Uid)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("uid", Str(Uid));
			FCall Call = Make(Kind, "POST", Path, &Body);
			Call.Text = Uid;
			return Call;
		}

		FCall AcceptFriend(const std::string& Uid) { return UidCall(ECall::FriendAccept, "/v1/friends/accept", Uid); }
		FCall DeclineFriend(const std::string& Uid) { return UidCall(ECall::FriendDecline, "/v1/friends/decline", Uid); }
		FCall RemoveFriend(const std::string& Uid) { return UidCall(ECall::FriendRemove, "/v1/friends/remove", Uid); }

		FCall SearchGangs(const std::string& Query)
		{
			FCall Call = Make(ECall::GangSearch, "GET", "/v1/gangs?q=" + UrlEncode(Trim(Query)));
			Call.Text = Trim(Query);
			Call.bQuiet = true;
			return Call;
		}

		FCall MyGang()
		{
			FCall Call = Make(ECall::GangMine, "GET", "/v1/gangs/mine");
			Call.bQuiet = true;
			return Call;
		}

		FCall CreateGang(const std::string& Name, int MinLevel, int MinPower, bool bOpen)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("name", Str(Trim(Name)));
			Body.Set("minLevel", Num(std::max(0, MinLevel)));
			Body.Set("minPower", Num(std::max(0, MinPower)));
			Body.Set("open", FValue::MakeBool(bOpen));
			FCall Call = Make(ECall::GangCreate, "POST", "/v1/gangs", &Body);
			Call.Text = Trim(Name);
			return Call;
		}

		FCall JoinGang(const std::string& Id, int Level, int Power)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("id", Str(Id));
			Body.Set("level", Num(Level));
			Body.Set("power", Num(Power));
			FCall Call = Make(ECall::GangJoin, "POST", "/v1/gangs/join", &Body);
			Call.Text = Id;
			return Call;
		}

		FCall LeaveGang() { return Make(ECall::GangLeave, "POST", "/v1/gangs/leave"); }
		FCall KickMember(const std::string& Uid) { return UidCall(ECall::GangKick, "/v1/gangs/kick", Uid); }

		FCall GangSettings(int MinLevel, int MinPower, bool bOpen)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("minLevel", Num(std::max(0, MinLevel)));
			Body.Set("minPower", Num(std::max(0, MinPower)));
			Body.Set("open", FValue::MakeBool(bOpen));
			return Make(ECall::GangSettings, "POST", "/v1/gangs/settings", &Body);
		}

		FCall Contribute(const std::string& Kind, int Amount)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("kind", Str(Kind));
			Body.Set("amount", Num(Amount));
			FCall Call = Make(ECall::GangContribute, "POST", "/v1/gangs/contribute", &Body);
			Call.Text = Kind;
			Call.Arg = Amount;
			Call.bQuiet = Kind == "activity";
			return Call;
		}

		FCall ClaimGangCoins() { return Make(ECall::GangClaim, "POST", "/v1/gangs/claim"); }

		FCall ClaimGangMilestone(int Index)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("index", Num(Index));
			FCall Call = Make(ECall::GangMilestone, "POST", "/v1/gangs/milestone", &Body);
			Call.Arg = Index;
			return Call;
		}

		FCall FetchMail()
		{
			FCall Call = Make(ECall::Mail, "GET", "/v1/mail");
			Call.bQuiet = true;
			return Call;
		}

		FCall AckMail(const std::vector<std::string>& Ids)
		{
			FValue Body = FValue::MakeObject();
			FValue List = FValue::MakeArray();
			for (const std::string& Id : Ids)
			{
				List.Push(Str(Id));
			}
			Body.Set("ids", List);
			FCall Call = Make(ECall::MailAck, "POST", "/v1/mail/ack", &Body);
			Call.bQuiet = true;
			return Call;
		}

		FCall AdminSendMail(const FAdminCompose& Compose)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("to", Str(Compose.bToAll ? std::string("all") : Trim(Compose.Target)));
			Body.Set("title", Str(Compose.Title.substr(0, 60)));
			Body.Set("body", Str(Compose.Body.substr(0, 600)));
			Body.Set("reward", RewardToJson(RewardPreset(Compose.RewardPreset)));
			FCall Call = Make(ECall::AdminMail, "POST", "/v1/admin/mail", &Body);
			Call.Text = Compose.bToAll ? "every player" : "user " + Trim(Compose.Target);
			return Call;
		}

		FCall FetchNotices()
		{
			FCall Call = Make(ECall::Notices, "GET", "/v1/notices");
			Call.bQuiet = true;
			return Call;
		}

		FCall AdminPostNotice(const FAdminCompose& Compose)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("category", Str(NoticeCategory(Compose.NoticeCategory)));
			Body.Set("title", Str(Compose.NoticeTitle.substr(0, 60)));
			Body.Set("body", Str(Compose.NoticeBody.substr(0, 2000)));
			Body.Set("pinned", FValue::MakeBool(Compose.bPinned));
			return Make(ECall::AdminNotice, "POST", "/v1/admin/notices", &Body);
		}

		FCall AdminDeleteNotice(const std::string& Id)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("id", Str(Id));
			FCall Call = Make(ECall::AdminNoticeDelete, "POST", "/v1/admin/notices/delete", &Body);
			Call.Text = Id;
			return Call;
		}

		FCall PutDefence(const FProfile& Profile)
		{
			const Arena::FLineup Lineup = Arena::MakeLineup(Profile, DefenceTeam(Profile), Profile.Meta.Name);
			FValue Body = FValue::MakeObject();
			FValue Team = FValue::MakeArray();
			for (const Arena::FFighter& Fighter : Lineup.Team)
			{
				Team.Push(FighterJson(Fighter));
			}
			Body.Set("team", Team);
			Body.Set("power", Num(Lineup.Power));
			FCall Call = Make(ECall::ArenaDefence, "PUT", "/v1/arena/defence", &Body);
			Call.bQuiet = true;
			return Call;
		}

		FCall GetArena()
		{
			FCall Call = Make(ECall::ArenaMe, "GET", "/v1/arena/me");
			Call.bQuiet = true;
			return Call;
		}

		FCall FindMatch(int Power)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("power", Num(Power));
			return Make(ECall::ArenaMatch, "POST", "/v1/arena/match", &Body);
		}

		FCall ReportResult(const std::string& MatchId, bool bWon)
		{
			FValue Body = FValue::MakeObject();
			Body.Set("matchId", Str(MatchId));
			Body.Set("won", FValue::MakeBool(bWon));
			FCall Call = Make(ECall::ArenaResult, "POST", "/v1/arena/result", &Body);
			Call.Arg = bWon ? 1 : 0;
			Call.Text = MatchId;
			return Call;
		}

		FCall ClaimArenaSeason() { return Make(ECall::ArenaClaim, "POST", "/v1/arena/claim"); }

		// ============================================================================ replies
		std::string FriendlyError(const std::string& Code, int Status, bool bNetworkError)
		{
			if (bNetworkError)
			{
				return "The account server cannot be reached. Online features need it running (see Server/README.md).";
			}
			static const std::pair<const char*, const char*> Messages[] = {
				{"unauthorized", "Your session ended. Sign in again."},
				{"forbidden", "Only the admin account can do that."},
				{"not_found", "Not found: check the user id, username or gang."},
				{"self", "That is you."},
				{"already_friends", "You are already friends."},
				{"already_requested", "A request is already waiting."},
				{"friends_full", "A friends list is full (50)."},
				{"in_gang", "Leave your current gang first."},
				{"name_taken", "That gang name is taken."},
				{"invalid_name", "Gang names are 3-20 letters, digits and spaces."},
				{"gang_full", "That gang is full (30 members)."},
				{"gang_closed", "That gang is not taking new members."},
				{"requirements", "You do not meet that gang's requirements yet."},
				{"not_leader", "Only the gang leader can do that."},
				{"nothing_to_claim", "Nothing to claim."},
				{"not_reached", "The gang has not reached that milestone yet."},
				{"already_claimed", "Already claimed this season."},
				{"limit_reached", "Today's limit is reached."},
				{"match_expired", "That match expired. Find a new opponent."},
				{"rate_limited", "Too many requests: wait a moment."},
				{"invalid_request", "The server refused the request."},
				{"invalid_input", "The server refused that input."},
				{"not_in_gang", "You are not in a gang."},
				{"already_reported", "That fight was already counted."},
			};
			for (const auto& Entry : Messages)
			{
				if (Code == Entry.first)
				{
					return Entry.second;
				}
			}
			return "The server answered " + std::to_string(Status) + (Code.empty() ? "" : " (" + Code + ")") + ".";
		}

		FReplyResult ApplyReply(FCache& Cache, FProfile& Profile, const FCall& Call, int Status, const FValue& Reply, bool bNetworkError)
		{
			FReplyResult Out;
			Cache.Busy = std::max(0, Cache.Busy - 1);
			if (bNetworkError || Status != 200 || !Reply.GetBool("ok", Status == 200))
			{
				Out.Message = FriendlyError(Reply.GetString("error"), Status, bNetworkError);
				Cache.LastError = Out.Message;
				if (Call.Kind == ECall::ArenaMatch)
				{
					// No server answer: fight a generated rival so the arena still works.
					Cache.Match = FMatch();
					Cache.Match.Opponent = Arena::MakeRival(Profile, static_cast<uint32_t>(Profile.NowUnix + Profile.Meta.ArenaWins * 7 + Profile.Meta.ArenaLosses));
					Cache.Match.bReady = true;
					Out.Message += " Fighting a practice rival instead.";
				}
				return Out;
			}
			Out.bOk = true;
			Cache.LastError.clear();
			switch (Call.Kind)
			{
			case ECall::Profile:
			case ECall::MailAck:
			case ECall::ArenaDefence:
			case ECall::Count:
				break;
			case ECall::Friends:
				Cache.Friends = ParseCards(Reply, "friends");
				Cache.Incoming = ParseCards(Reply, "incoming");
				Cache.Outgoing = ParseCards(Reply, "outgoing");
				Cache.bFriendsLoaded = true;
				break;
			case ECall::FriendRequest:
				Out.Message = Reply.GetString("status") == "accepted" ? "You are now friends with " + Call.Text + "." : "Friend request sent to " + Call.Text + ".";
				Out.FollowUps.push_back(GetFriends());
				break;
			case ECall::FriendAccept:
				Out.Message = "Friend request accepted.";
				Out.FollowUps.push_back(GetFriends());
				break;
			case ECall::FriendDecline:
				Out.Message = "Request removed.";
				Out.FollowUps.push_back(GetFriends());
				break;
			case ECall::FriendRemove:
				Out.Message = "Friend removed.";
				Out.FollowUps.push_back(GetFriends());
				break;
			case ECall::GangSearch:
				Cache.Search.clear();
				if (const FValue* List = Reply.Find("gangs"))
				{
					for (const FValue& Item : List->Items())
					{
						FGangSummary Summary;
						ParseSummary(Item, Summary);
						Cache.Search.push_back(Summary);
					}
				}
				Cache.SearchText = Call.Text;
				break;
			case ECall::GangMine:
			{
				const FValue* Gang = Reply.Find("gang");
				Cache.bInGang = Gang && Gang->IsObject();
				Cache.Gang = Cache.bInGang ? ParseGang(*Gang) : FGang();
				Cache.PendingCoins = Reply.GetInt("pendingCoins");
				Cache.PendingSeason = Reply.GetString("pendingSeason");
				Cache.bGangLoaded = true;
				const int Pending = Profile.Meta.Gang.PendingActivity;
				if (Cache.bInGang && Pending > 0)
				{
					Out.FollowUps.push_back(Contribute("activity", std::min(50, Pending)));
				}
				break;
			}
			case ECall::GangCreate:
			case ECall::GangJoin:
			case ECall::GangKick:
			case ECall::GangSettings:
				if (const FValue* Gang = Reply.Find("gang"))
				{
					Cache.Gang = ParseGang(*Gang);
					Cache.bInGang = true;
					Cache.bGangLoaded = true;
				}
				if (Call.Kind == ECall::GangCreate)
				{
					Profile.Spend(ECurrency::Gold, GangCreateGold);
					Out.Message = "Gang " + Cache.Gang.Name + " founded. You lead it.";
				}
				else if (Call.Kind == ECall::GangJoin)
				{
					Out.Message = "You joined " + Cache.Gang.Name + ".";
				}
				else
				{
					Out.Message = Call.Kind == ECall::GangKick ? "Member removed." : "Gang settings saved.";
				}
				break;
			case ECall::GangLeave:
				Cache.bInGang = false;
				Cache.Gang = FGang();
				Out.Message = "You left the gang.";
				Out.FollowUps.push_back(MyGang());
				break;
			case ECall::GangContribute:
			{
				if (const FValue* Gang = Reply.Find("gang"))
				{
					Cache.Gang = ParseGang(*Gang);
				}
				const int Added = Reply.GetInt("added");
				if (Call.Text == "donate" && Added > 0)
				{
					Profile.Spend(ECurrency::Gold, GangDonateGold);
				}
				if (Call.Text == "activity")
				{
					Profile.Meta.Gang.PendingActivity = std::max(0, Profile.Meta.Gang.PendingActivity - Call.Arg);
				}
				Out.Message = Added > 0 ? "+" + std::to_string(Added) + " gang points." : "Today's limit for that is reached.";
				break;
			}
			case ECall::GangClaim:
			{
				const int Coins = std::max(0, Reply.GetInt("coins"));
				Profile.Add(ECurrency::GangCoin, Coins);
				Cache.PendingCoins = 0;
				Out.Message = "Gang season " + Reply.GetString("season") + " reward: " + Meta::Num(Coins) + " Gang coins.";
				break;
			}
			case ECall::GangMilestone:
				Cache.Gang.MilestonesClaimed.push_back(Call.Arg);
				Out.Message = "Gang milestone " + std::to_string(Call.Arg + 1) + ": " + Meta::ApplyReward(Profile, GangMilestoneReward(Call.Arg));
				break;
			case ECall::Mail:
			{
				std::vector<std::string> Ids;
				int Added = 0;
				if (const FValue* List = Reply.Find("mail"))
				{
					for (const FValue& Item : List->Items())
					{
						const std::string Id = IdString(Item, "id");
						if (Id.empty())
						{
							continue;
						}
						Ids.push_back(Id);
						std::vector<std::string>& Seen = Profile.Meta.ImportedMail;
						if (std::find(Seen.begin(), Seen.end(), Id) != Seen.end())
						{
							continue;
						}
						FMail Mail;
						Mail.Id = Profile.Meta.NextMailId++;
						Mail.Title = Item.GetString("title").substr(0, 60);
						Mail.Body = Item.GetString("body").substr(0, 600);
						Mail.From = Item.GetString("from", "Admin").substr(0, 24);
						if (const FValue* Reward = Item.Find("reward"))
						{
							Mail.Reward = RewardFromJson(*Reward);
						}
						const bool bNothing = Meta::RewardText(Mail.Reward) == "nothing";
						Mail.bClaimed = bNothing;   // a letter without an attachment has nothing to claim
						Profile.Meta.Mail.push_back(Mail);
						Seen.push_back(Id);
						if (Seen.size() > 200)
						{
							Seen.erase(Seen.begin(), Seen.begin() + static_cast<long>(Seen.size() - 200));
						}
						++Added;
					}
				}
				if (!Ids.empty())
				{
					Out.FollowUps.push_back(AckMail(Ids));
				}
				if (Added > 0)
				{
					Out.Message = std::to_string(Added) + " new mail" + (Added == 1 ? "" : "s") + " arrived.";
				}
				break;
			}
			case ECall::AdminMail:
				Out.Message = "Mail sent to " + Call.Text + ".";
				break;
			case ECall::Notices:
			{
				Cache.Notices.clear();
				if (const FValue* List = Reply.Find("notices"))
				{
					for (const FValue& Item : List->Items())
					{
						FNotice Notice;
						Notice.Id = IdString(Item, "id");
						Notice.Category = Item.GetString("category", "notice");
						Notice.Title = Item.GetString("title");
						Notice.Body = Item.GetString("body");
						Notice.CreatedAt = Item.GetString("createdAt");
						Notice.bPinned = Item.GetBool("pinned");
						Cache.Notices.push_back(Notice);
					}
				}
				Cache.bNoticesLoaded = true;
				bool bUnread = false;
				for (const FNotice& Notice : Cache.Notices)
				{
					const std::vector<std::string>& Read = Profile.Meta.ReadNotices;
					bUnread = bUnread || std::find(Read.begin(), Read.end(), Notice.Id) == Read.end();
				}
				if (bUnread && !Cache.bLoginNoticesShown && Profile.Meta.Settings.bNoticesAtLogin)
				{
					Out.bOpenNotices = true;
				}
				Cache.bLoginNoticesShown = true;
				break;
			}
			case ECall::AdminNotice:
				Out.Message = "Notice published.";
				Out.FollowUps.push_back(FetchNotices());
				break;
			case ECall::AdminNoticeDelete:
				Out.Message = "Notice deleted.";
				Out.FollowUps.push_back(FetchNotices());
				break;
			case ECall::ArenaMe:
			{
				FArenaServer& A = Cache.ArenaServer;
				A.bLoaded = true;
				A.Trophies = Reply.GetInt("trophies");
				A.Rank = Reply.GetInt("rank", Arena::RankOf(A.Trophies));
				A.Season = Reply.GetString("season");
				A.bChampion = Reply.GetBool("champion");
				A.Wins = Reply.GetInt("wins");
				A.Losses = Reply.GetInt("losses");
				A.DefenceWins = Reply.GetInt("defenceWins");
				A.DefenceLosses = Reply.GetInt("defenceLosses");
				A.Log.clear();
				if (const FValue* Log = Reply.Find("log"))
				{
					for (const FValue& Item : Log->Items())
					{
						const bool bWon = Item.GetBool("won");
						const int Delta = Item.GetInt("delta");
						A.Log.push_back(Item.GetString("attacker", "Someone") + " attacked your defence: " + (bWon ? "you held (+" : "they won (-")
							+ std::to_string(std::abs(Delta)) + ")");
					}
				}
				A.PendingRank = -1;
				A.PendingSeason.clear();
				if (const FValue* Pending = Reply.Find("pendingReward"))
				{
					if (Pending->IsObject())
					{
						A.PendingRank = Pending->GetInt("rank", -1);
						A.PendingSeason = Pending->GetString("season");
					}
				}
				// The server is the authority for signed-in players: mirror it locally.
				Profile.Meta.Arena.Trophies = A.Trophies;
				Profile.Meta.Arena.bChampionFloor = A.bChampion;
				Profile.Meta.Arena.SeasonKey = LiveOps::SeasonKeyFromCode(A.Season);
				Profile.Meta.Arena.DefenceWins = A.DefenceWins;
				Profile.Meta.Arena.DefenceLosses = A.DefenceLosses;
				break;
			}
			case ECall::ArenaMatch:
			{
				Cache.Match = FMatch();
				const FValue* Opponent = Reply.Find("opponent");
				if (Opponent && Opponent->IsObject())
				{
					Cache.Match.MatchId = Reply.GetString("matchId");
					Cache.Match.Opponent = ParseLineup(*Opponent);
					Out.Message = "Opponent found: " + Cache.Match.Opponent.Name + " (" + Arena::RankName(Arena::RankOf(Cache.Match.Opponent.Trophies)) + ").";
				}
				else
				{
					Cache.Match.Opponent = Arena::MakeRival(Profile, static_cast<uint32_t>(Profile.NowUnix + Profile.Meta.ArenaWins * 7 + Profile.Meta.ArenaLosses));
					Out.Message = "Nobody else has a defence team yet: here is a practice rival.";
				}
				Cache.Match.bReady = !Cache.Match.Opponent.Team.empty();
				break;
			}
			case ECall::ArenaResult:
			{
				const bool bWon = Call.Arg == 1;
				const int Trophies = Reply.GetInt("trophies", Profile.Meta.Arena.Trophies);
				const int Delta = Reply.GetInt("delta");
				Out.Message = Arena::RecordFight(Profile, Cache.Match.Opponent, bWon, Trophies, bWon ? Delta : -Delta);
				Cache.ArenaServer.Trophies = Trophies;
				Cache.Match = FMatch();
				Out.FollowUps.push_back(GetArena());
				break;
			}
			case ECall::ArenaClaim:
			{
				const int Rank = Reply.GetInt("rank");
				Out.Message = "Arena season reward (" + Arena::RankName(Rank) + "): " + Meta::ApplyReward(Profile, Arena::SeasonReward(Rank));
				Cache.ArenaServer.PendingRank = -1;
				break;
			}
			}
			return Out;
		}

		// ============================================================================ rewards in JSON
		FValue RewardToJson(const FReward& Reward)
		{
			FValue V = FValue::MakeObject();
			auto Put = [&V](const char* Key, int Amount)
			{
				if (Amount > 0)
				{
					V.Set(Key, FValue::MakeNumber(Amount));
				}
			};
			Put("gold", Reward.Gold);
			Put("bytes", Reward.Bytes);
			Put("hearts", Reward.Hearts);
			Put("diamonds", Reward.Diamonds);
			Put("regular", Reward.RegularTickets);
			Put("special", Reward.SpecialTickets);
			Put("arcade", Reward.ArcadeTokens);
			Put("gangCoins", Reward.GangCoins);
			return V;
		}

		FReward RewardFromJson(const FValue& V)
		{
			auto Get = [&V](const char* Key) { return std::clamp(V.GetInt(Key), 0, 1000000); };
			FReward Reward;
			Reward.Gold = Get("gold");
			Reward.Bytes = Get("bytes");
			Reward.Hearts = Get("hearts");
			Reward.Diamonds = Get("diamonds");
			Reward.RegularTickets = Get("regular");
			Reward.SpecialTickets = Get("special");
			Reward.ArcadeTokens = Get("arcade");
			Reward.GangCoins = Get("gangCoins");
			return Reward;
		}

		int RewardPresetCount()
		{
			return 9;
		}

		FReward RewardPreset(int Index)
		{
			static const FReward Presets[9] = {
				{},
				{.Diamonds = 300},
				{.Diamonds = 1000},
				{.RegularTickets = 5},
				{.SpecialTickets = 2},
				{.Gold = 50000, .Bytes = 20000},
				{.Hearts = 100},
				{.ArcadeTokens = 10},
				{.GangCoins = 500},
			};
			return Presets[std::clamp(Index, 0, 8)];
		}

		const char* NoticeCategory(int Index)
		{
			static const char* Names[NoticeCategoryCount] = {"notice", "event", "update", "maintenance"};
			return Names[std::clamp(Index, 0, NoticeCategoryCount - 1)];
		}

		// ============================================================================ gangs
		int GangLevelFromXp(int Xp)
		{
			return std::clamp(1 + static_cast<int>(std::sqrt(std::max(0, Xp) / 500.0)), 1, 30);
		}

		int GangMilestonePoints(int Index)
		{
			static const int Points[GangMilestoneCount] = {1000, 3000, 6000, 10000, 15000};
			return Points[std::clamp(Index, 0, GangMilestoneCount - 1)];
		}

		FReward GangMilestoneReward(int Index)
		{
			static const FReward Rewards[GangMilestoneCount] = {
				{.Gold = 5000, .Bytes = 1500},
				{.Hearts = 20, .ArcadeTokens = 2},
				{.Gold = 10000, .RegularTickets = 1},
				{.Bytes = 5000, .Diamonds = 100},
				{.Diamonds = 150, .SpecialTickets = 1},
			};
			return Rewards[std::clamp(Index, 0, GangMilestoneCount - 1)];
		}

		int GangSeasonCoins(int SeasonPoints, int SeasonContribution)
		{
			static const int Thresholds[6] = {0, 1000, 3000, 6000, 10000, 15000};
			static const int Coins[6] = {50, 150, 300, 450, 650, 900};
			int Tier = 0;
			for (int Index = 0; Index < 6; ++Index)
			{
				Tier = SeasonPoints >= Thresholds[Index] ? Index : Tier;
			}
			return Coins[Tier] + std::min(300, std::max(0, SeasonContribution) / 10);
		}

		int GangShopCount()
		{
			return GangShopItems;
		}

		const FGangShopItem& GangShopItem(int Index)
		{
			static const FGangShopItem Items[GangShopItems] = {
				{"Regular ticket", "ticket_regular", 150, 5, 1, {.RegularTickets = 1}},
				{"Special ticket", "ticket_special", 450, 2, 3, {.SpecialTickets = 1}},
				{"20 Angel hearts", "hearts", 100, 10, 1, {.Hearts = 20}},
				{"5,000 Bytes", "bytes", 80, 10, 1, {.Bytes = 5000}},
				{"15,000 Gold", "gold", 80, 10, 1, {.Gold = 15000}},
				{"Equipment crate", "equipment", 120, 5, 2, {.Equipment = 1}},
				{"3 Arcade tokens", "arcade", 50, 5, 1, {.ArcadeTokens = 3}},
				{"100 Diamonds", "diamond", 250, 3, 5, {.Diamonds = 100}},
			};
			return Items[std::clamp(Index, 0, GangShopItems - 1)];
		}

		FActionResult BuyGangShop(FProfile& Profile, int Index, int GangLevel)
		{
			if (Index < 0 || Index >= GangShopItems)
			{
				return Fail("No such item.");
			}
			const FGangShopItem& Item = GangShopItem(Index);
			int& Bought = Profile.Meta.Gang.ShopBought[Index];
			if (GangLevel < Item.MinGangLevel)
			{
				return Fail(std::string(Item.Name) + " unlocks at gang level " + std::to_string(Item.MinGangLevel) + ".");
			}
			if (Bought >= Item.MonthlyLimit)
			{
				return Fail("Sold out for this season (" + std::to_string(Item.MonthlyLimit) + " a month).");
			}
			if (!Profile.Spend(ECurrency::GangCoin, Item.Price))
			{
				return Fail("Not enough Gang coins (" + Meta::Num(Item.Price) + " needed). Gang coins are paid at the end of each gang season.");
			}
			++Bought;
			Meta::NotifyQuest(Profile, Meta::EQuestEvent::ShopBuy);
			return Ok("Bought " + std::string(Item.Name) + ": " + Meta::ApplyReward(Profile, Item.Reward));
		}

		// ============================================================================ text helpers
		std::string UrlEncode(const std::string& Text)
		{
			std::string Out;
			for (unsigned char Character : Text)
			{
				if (std::isalnum(Character) || Character == '-' || Character == '_' || Character == '.' || Character == '~')
				{
					Out += static_cast<char>(Character);
				}
				else
				{
					char Buffer[4];
					std::snprintf(Buffer, sizeof(Buffer), "%%%02X", Character);
					Out += Buffer;
				}
			}
			return Out;
		}

		std::string Trim(const std::string& Text)
		{
			size_t Begin = 0;
			size_t End = Text.size();
			while (Begin < End && std::isspace(static_cast<unsigned char>(Text[Begin])))
			{
				++Begin;
			}
			while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1])))
			{
				--End;
			}
			return Text.substr(Begin, End - Begin);
		}

		bool ValidGangName(const std::string& Name)
		{
			const std::string Clean = Trim(Name);
			if (Clean.size() < 3 || Clean.size() > 20)
			{
				return false;
			}
			for (unsigned char Character : Clean)
			{
				if (!std::isalnum(Character) && Character != ' ')
				{
					return false;
				}
			}
			return true;
		}
	}
}
