// Boldinbeauty demo — the game's side of the online features (owner request 2026-10-06): friends, gangs (clans of up to
// 30 with join requirements, season rewards and a gang shop), admin mail to one user or every user, Nikke-style notices and
// the semi-online arena. Engine-independent: the pages build FCall requests, the Unreal layer (UBIBOnlineSubsystem) sends
// them through the account client and hands the reply back to ApplyReply, which updates the cache the pages draw from.
// The server side is Demo/BoldInBeautyDemo/Server; the routes are specified in docs/online-api.md.
#pragma once

#include <string>
#include <vector>

#include "BIBArena.h"
#include "BIBJson.h"
#include "BIBMeta.h"

namespace BIB
{
	class FProfile;
	struct FActionResult;

	namespace Online
	{
		enum class ECall : int
		{
			Profile = 0,
			Friends,
			FriendRequest,
			FriendAccept,
			FriendDecline,
			FriendRemove,
			GangSearch,
			GangMine,
			GangCreate,
			GangJoin,
			GangLeave,
			GangKick,
			GangSettings,
			GangContribute,
			GangClaim,
			GangMilestone,
			Mail,
			MailAck,
			AdminMail,
			Notices,
			AdminNotice,
			AdminNoticeDelete,
			ArenaDefence,
			ArenaMe,
			ArenaMatch,
			ArenaResult,
			ArenaClaim,
			Count
		};
		const char* CallName(ECall Kind);

		struct FCall
		{
			ECall Kind = ECall::Profile;
			std::string Verb = "GET";
			std::string Path;
			std::string Body;      // JSON ("" for GET)
			int Arg = 0;           // context for the reply (milestone index, donation count, ...)
			std::string Text;      // context for the reply (gang name, opponent uid, ...)
			bool bQuiet = false;   // background refresh: no status message on success
		};

		struct FCard
		{
			std::string Uid;
			std::string Name;
			int Level = 0;
			int Power = 0;
			std::string LastSeen;
			std::string Gang;
		};

		struct FGangMember
		{
			std::string Uid;
			std::string Name;
			std::string Role;
			int Level = 0;
			int Power = 0;
			int Contribution = 0;
			int SeasonContribution = 0;
			std::string LastSeen;
		};

		struct FGangSummary
		{
			std::string Id;
			std::string Name;
			std::string LeaderName;
			int Level = 1;
			int Members = 0;
			int MaxMembers = 30;
			int MinLevel = 0;
			int MinPower = 0;
			bool bOpen = true;
		};

		struct FGang : FGangSummary
		{
			int Xp = 0;
			int SeasonPoints = 0;
			std::string Season;
			std::string MyRole;
			std::vector<int> MilestonesClaimed;
			std::vector<FGangMember> MemberList;
		};

		struct FNotice
		{
			std::string Id;
			std::string Category;   // notice / event / update / maintenance
			std::string Title;
			std::string Body;
			std::string CreatedAt;
			bool bPinned = false;
		};

		struct FArenaServer
		{
			bool bLoaded = false;
			int Trophies = 0;
			int Rank = 0;
			std::string Season;
			bool bChampion = false;
			int Wins = 0;
			int Losses = 0;
			int DefenceWins = 0;
			int DefenceLosses = 0;
			std::vector<std::string> Log;
			int PendingRank = -1;
			std::string PendingSeason;
		};

		struct FMatch
		{
			bool bReady = false;
			std::string MatchId;    // "" = a generated rival (no server, or nobody else has a defence yet)
			Arena::FLineup Opponent;
		};

		struct FAdminCompose
		{
			bool bToAll = true;          // owner: "a toggle ... to send mail to other users or all users"
			std::string Target;          // user id when not to all
			std::string Title = "A message from the Boldinbeauty team";
			std::string Body;
			int RewardPreset = 0;
			int NoticeCategory = 0;
			bool bPinned = false;
			std::string NoticeTitle;
			std::string NoticeBody;
		};

		struct FCache
		{
			bool bOnline = false;        // signed in with a reachable account server
			bool bAdmin = false;
			int Busy = 0;                // requests in flight
			std::string LastError;

			bool bFriendsLoaded = false;
			std::vector<FCard> Friends;
			std::vector<FCard> Incoming;
			std::vector<FCard> Outgoing;

			bool bGangLoaded = false;
			bool bInGang = false;
			FGang Gang;
			int PendingCoins = 0;
			std::string PendingSeason;
			std::vector<FGangSummary> Search;
			std::string SearchText;
			int NewGangMinLevel = 0;     // gang creation / settings form
			int NewGangMinPower = 0;
			bool bNewGangOpen = true;

			bool bNoticesLoaded = false;
			std::vector<FNotice> Notices;
			int OpenNotice = -1;
			bool bLoginNoticesShown = false;

			FArenaServer ArenaServer;
			FMatch Match;

			FAdminCompose Admin;
		};

		// ---------------------------------------------------------------- request builders
		FCall PushProfile(const FProfile& Profile);
		FCall GetFriends();
		FCall RequestFriend(const std::string& To);
		FCall AcceptFriend(const std::string& Uid);
		FCall DeclineFriend(const std::string& Uid);
		FCall RemoveFriend(const std::string& Uid);
		FCall SearchGangs(const std::string& Query);
		FCall MyGang();
		FCall CreateGang(const std::string& Name, int MinLevel, int MinPower, bool bOpen);
		FCall JoinGang(const std::string& Id, int Level, int Power);
		FCall LeaveGang();
		FCall KickMember(const std::string& Uid);
		FCall GangSettings(int MinLevel, int MinPower, bool bOpen);
		FCall Contribute(const std::string& Kind, int Amount);
		FCall ClaimGangCoins();
		FCall ClaimGangMilestone(int Index);
		FCall FetchMail();
		FCall AckMail(const std::vector<std::string>& Ids);
		FCall AdminSendMail(const FAdminCompose& Compose);
		FCall FetchNotices();
		FCall AdminPostNotice(const FAdminCompose& Compose);
		FCall AdminDeleteNotice(const std::string& Id);
		FCall PutDefence(const FProfile& Profile);
		FCall GetArena();
		FCall FindMatch(int Power);
		FCall ReportResult(const std::string& MatchId, bool bWon);
		FCall ClaimArenaSeason();

		// ---------------------------------------------------------------- replies
		struct FReplyResult
		{
			bool bOk = false;
			std::string Message;
			std::vector<FCall> FollowUps;
			bool bOpenNotices = false;   // unread notices arrived and the player wants them at login
		};
		FReplyResult ApplyReply(FCache& Cache, FProfile& Profile, const FCall& Call, int Status, const Json::FValue& Reply, bool bNetworkError);
		std::string FriendlyError(const std::string& Code, int Status, bool bNetworkError);

		// ---------------------------------------------------------------- rewards in JSON
		Json::FValue RewardToJson(const FReward& Reward);
		FReward RewardFromJson(const Json::FValue& Value);
		int RewardPresetCount();
		FReward RewardPreset(int Index);             // the admin mail attachments
		const char* NoticeCategory(int Index);       // notice, event, update, maintenance
		constexpr int NoticeCategoryCount = 4;

		// ---------------------------------------------------------------- gangs (rules the game knows)
		constexpr int GangMaxMembers = 30;           // owner: gangs of 30 members
		constexpr int GangCreateGold = 20000;        // PLACEHOLDER
		constexpr int GangDonateGold = 1000;         // per donation (10 contribution points)
		constexpr int GangMilestoneCount = 5;
		int GangLevelFromXp(int Xp);
		int GangMilestonePoints(int Index);          // season points: 1,000 / 3,000 / 6,000 / 10,000 / 15,000
		FReward GangMilestoneReward(int Index);      // every member claims it once a season
		int GangSeasonCoins(int SeasonPoints, int SeasonContribution);   // what the server pays at the season end (shown in the UI)

		struct FGangShopItem
		{
			const char* Name;
			const char* Icon;
			int Price;           // Gang coins
			int MonthlyLimit;    // per player per season
			int MinGangLevel;
			FReward Reward;
		};
		int GangShopCount();
		const FGangShopItem& GangShopItem(int Index);
		FActionResult BuyGangShop(FProfile& Profile, int Index, int GangLevel);

		// Text helpers for the pages.
		std::string UrlEncode(const std::string& Text);
		std::string Trim(const std::string& Text);
		bool ValidGangName(const std::string& Name);
	}
}
