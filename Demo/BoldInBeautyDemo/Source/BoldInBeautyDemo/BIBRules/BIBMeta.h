// Boldinbeauty demo — meta-game data (mail, quests, season pass, Outer World, arcade, tower, arena...) and stage
// definitions. Pure data: the operations live in BIBMetaOps.h. No Unreal includes.
//
// Everything here is PLACEHOLDER content for the demo UI (owner request 2026-10-04): names, rewards and rates are
// stand-ins to be replaced when the owner decides. Nothing here is a game-design decision.
#pragma once

#include <string>
#include <vector>

namespace BIB
{
	// ------------------------------------------------------------------ rewards
	struct FReward
	{
		int Gold = 0;
		int Bytes = 0;
		int Hearts = 0;
		int Diamonds = 0;
		int RegularTickets = 0;
		int SpecialTickets = 0;
		int ArcadeTokens = 0;
		int GangCoins = 0;      // gang shop currency (owner request 2026-10-06)
		int SeasonXp = 0;
		int CommanderXp = 0;
		int Equipment = 0;      // random equipment drops
		// Owner request 2026-10-07 (inventory, elements, events): new currencies and consumable items.
		int ElementCores = 0;   // Element Attunement (BIBElement.h)
		int EventTokens = 0;    // event shop (BIBEvents.h)
		int ArkMedals = 0;      // Ark shop: arena fights and tower floors (BIBEvents.h)
		int Charge = 0;         // stamina for campaign replays (BIBInventory.h)
		int Item = -1;          // EItem (BIBInventory.h) granted ItemAmount times; -1 = none
		int ItemAmount = 0;
	};

	// -------------------------------------------------------------------- stages
	// A stage id packs what is being played. Campaign: 1..104, the level counted through all 13 chapters (1..8 is Chapter 1,
	// 9..16 Chapter 2, ...), so a campaign stage id doubles as the "levels cleared" count that unlocks it. Tower: 1000 + mode*500
	// + floor. Sub-story: 3000 + story*10 + level.
	enum class EStageKind : int
	{
		Campaign = 0,
		Tower,
		SubStory,
		BossRush
	};

	struct FStageRef
	{
		EStageKind Kind = EStageKind::Campaign;
		int A = 1;   // campaign: chapter, tower: mode (0 Operator, 1 Secrets), sub-story: story index, boss rush: base stage
		int B = 1;   // campaign: level inside the chapter, tower: floor, sub-story: level, boss rush: round
	};

	// What one section spawns. "Grunts" are the basic melee enemies; the other fields are the standard archetypes of
	// BIBCampaign.h (ranged, mage = area damage, brute = tank, assassin = high damage at close range, mini-boss, boss).
	struct FSectionPlanData
	{
		int Grunts = 0;
		int Brutes = 0;
		int MiniBosses = 0;
		int Bosses = 0;
		int Ranged = 0;
		int Mages = 0;
		int Assassins = 0;
		int Total() const { return Grunts + Brutes + MiniBosses + Bosses + Ranged + Mages + Assassins; }
	};

	// Stat multipliers on the base enemy numbers of the archetype table (BIBCampaign.h).
	struct FEnemyScale
	{
		double Hp = 1.0;
		double Damage = 1.0;
	};

	constexpr int CampaignLevels = 8;       // levels per chapter: Chapter 1 has 8 (owner request); chapters 2-13 follow it (PLACEHOLDER). The last has the boss.
	constexpr int CampaignChapters = 13;    // owner request: 13 chapters
	constexpr int TotalCampaignLevels = CampaignLevels * CampaignChapters;
	constexpr int SubStoryCount = 4;
	// The Ark Tower is endless and never resets (owner request 2026-10-06); this bound only keeps stage ids inside an int.
	constexpr int MaxTowerFloor = 999999;

	int CampaignStageId(int Level);                // Chapter 1, level 1..8 (the original single-chapter form)
	int CampaignStageId(int Chapter, int Level);   // any chapter 1..13, level 1..8
	int ChapterOfStage(int CampaignStage);         // 1..13 for a campaign stage id, 0 otherwise
	int LevelOfStage(int CampaignStage);           // 1..8 inside its chapter
	int ChapterFirstStage(int Chapter);            // stage id of level 1 of a chapter
	int TowerStageId(int Mode, int Floor);
	int SubStoryStageId(int Story, int Level);
	FStageRef DecodeStage(int StageId);
	bool IsValidStage(int StageId);
	int SubStoryLevels(int Story);
	std::string SubStoryTitle(int Story);
	std::string StageName(int StageId);
	bool IsChapterFinal(int StageId);
	std::vector<FSectionPlanData> StagePlan(int StageId);
	FEnemyScale StageScales(int StageId);   // health and damage multipliers on the base enemy numbers
	double StageEnemyScale(int StageId);    // = StageScales(...).Hp (kept for older callers)

	// ----------------------------------------------------------------- meta state
	enum class EBuilding : int
	{
		School = 0,
		PoliceStation,
		FireStation,
		Hospital,
		Gym,
		Market,
		Workshop,
		Shelter,
		Count
	};
	constexpr int BuildingCount = static_cast<int>(EBuilding::Count);
	constexpr int MaxBuildingLevel = 10;
	constexpr int SeasonTiers = 50;
	constexpr int DailyQuestCount = 8;
	constexpr int WeeklyQuestCount = 7;
	constexpr int StoryQuestsPerChapter = 3;   // levels 3, 6 and 8 (the chapter end) of every chapter
	constexpr int StoryQuestCount = StoryQuestsPerChapter * CampaignChapters;
	constexpr int MilestoneCount = 6;

	struct FMail
	{
		int Id = 0;
		std::string Title;
		std::string Body;
		FReward Reward;
		bool bClaimed = false;
		std::string From;       // "" = the game, "Admin" = sent by the admin account
	};

	struct FQuestProgress
	{
		int Progress = 0;
		bool bClaimed = false;
	};

	struct FBuildingState
	{
		bool bBuilt = false;
		int Level = 0;
		double StoredGold = 0.0;
		double StoredBytes = 0.0;
		double StoredHearts = 0.0;
	};

	// Arcade records (owner request 2026-10-06: Night Shift, Boss Rush, Rhythm; BIBArcade.h). The old placeholder games are gone.
	constexpr int ArcadeGameCount = 7;         // Night Shift, Boss Rush, Rhythm + the 2026-10-07 mini-games + Ark Rally (Arcade::EGame)
	constexpr int ArcadeDifficultyCount = 3;
	constexpr int RhythmSongCount = 3;
	constexpr int MiniGameRecordCount = 4;     // Halo Fall, Neon Memory, Firewall Breach, Ark Rally (Arcade::EMiniGame)
	constexpr int UcnModeCount = 4;            // Ultimate Custom Night: Easy, Medium, Hard, Angel (Arcade::Ucn::EMode)
	struct FArcadeRecords
	{
		int Plays[ArcadeGameCount] = {};
		int NightWins[ArcadeDifficultyCount] = {};
		int NightBestHour[ArcadeDifficultyCount] = {};
		int RhythmBest[RhythmSongCount * ArcadeDifficultyCount] = {};    // best score per song and difficulty
		int RhythmStars[RhythmSongCount * ArcadeDifficultyCount] = {};
		int BossRushBest = 0;                                            // highest boss round cleared
		int MiniBest[MiniGameRecordCount * ArcadeDifficultyCount] = {};  // best score per mini-game and difficulty (BIBMiniGames.h)
		int MiniStars[MiniGameRecordCount * ArcadeDifficultyCount] = {};
		int MiniWins[MiniGameRecordCount * ArcadeDifficultyCount] = {};
		std::string RallyTrack;   // the player's own Ark Rally track code (BIBRally.h); empty = none yet
		int RallyVehicle = 0;     // Ark Rally: 0 dirt bike, 1 buggy (BIBRally.h ERallyVehicle)
		// Ultimate Custom Night (owner request 2026-10-14; BIBUltimateNight.h): four modes Easy / Medium / Hard / Angel.
		int UcnPlays[UcnModeCount] = {};
		int UcnWins[UcnModeCount] = {};
		int UcnBestHour[UcnModeCount] = {};
		bool bUcnUnlocked = false;        // latched the first time Chapter 13 is found cleared
		bool bUcnRewardClaimed = false;   // the one-time 3000 Diamonds were paid (once per account, first win of any mode)
	};

	// Arena (owner request 2026-10-06; BIBArena.h). Trophies and rank live on the account server for signed-in players; this
	// is the local copy (and the whole arena for guests, who fight generated rivals).
	struct FArenaState
	{
		int Attack[3] = {-1, -1, -1};    // roster indices; -1 = empty
		int Defence[3] = {-1, -1, -1};
		int Trophies = 0;
		bool bChampionFloor = false;     // reached Champions this season: cannot fall below it until the season ends
		int SeasonKey = 0;               // year * 12 + month - 1 of the season the trophies belong to (0 = not started)
		int PendingRewardRank = -1;      // season-end reward waiting to be claimed (-1 = none)
		int PendingRewardSeason = 0;
		int DefenceWins = 0;
		int DefenceLosses = 0;
	};

	// Returning players (owner request 2026-10-06): away 14 days or more -> 7 days of a 3-reward check-in plus "Returned quests".
	constexpr int ReturnAwayDays = 14;
	constexpr int ReturnEventDays = 7;
	constexpr int ReturnQuestCount = 6;
	struct FReturnState
	{
		long long StartUnix = 0;         // 0 = no welcome-back event
		int DaysClaimed = 0;             // bit d = check-in day d+1 claimed
		int StageClears = 0;             // progress counters for the Returned quests (counted while the event runs)
		int Summons = 0;
		int LevelUps = 0;
		int DailyQuests = 0;
		int ArkFights = 0;
		bool QuestClaimed[ReturnQuestCount] = {};
	};

	// Gangs (owner request 2026-10-06; BIBOnline.h holds the server side). Only the local bits are saved here.
	constexpr int GangShopItems = 8;
	struct FGangLocal
	{
		int ShopSeason = 0;                    // season key the purchase counts below belong to
		int ShopBought[GangShopItems] = {};
		int PendingActivity = 0;               // contribution points earned offline, sent with the next gang call
	};

	// Key bindings (owner request 2026-10-06: "preferences like key binds"). Battle actions first (same order as EBIBInput in
	// BIBGame/BIBInput.h), then the arcade keys. A binding is an Unreal key name ("W", "SpaceBar", "LeftMouseButton"); "" = default.
	constexpr int BattleBindCount = 18;
	constexpr int NightBindCount = 5;    // left door, right door, left light, right light, camera
	constexpr int RhythmBindCount = 5;   // lanes 1..5
	constexpr int KeyBindCount = BattleBindCount + NightBindCount + RhythmBindCount;
	constexpr int NightBindFirst = BattleBindCount;
	constexpr int RhythmBindFirst = BattleBindCount + NightBindCount;
	const char* KeyBindName(int Index);       // "Jump", "Night Shift: left door", ...
	const char* DefaultKeyName(int Index);    // Unreal key name
	const char* KeyBindGroup(int Index);      // "Battle", "Night Shift", "Rhythm"
	std::string KeyDisplayName(const std::string& KeyName);   // "Space", "Left mouse", "1"...

	// Top-bar currencies: the six resources the lobby shows (owner request 2026-10-06: "lobby customisation like what currencies show").
	constexpr int TopBarSlots = 6;
	constexpr int TopBarChoiceCount = 14;     // every ECurrency (BIBCore.h): Gang coins, Charge, Element cores, Event tokens, Ark medals included

	// Settings (owner request 2026-10-06: "settings with relevant preferences that should work"). The Unreal layer applies
	// them (UBIBGameInstance::ApplyUserSettings): audio to the audio device, graphics to UGameUserSettings, controls and the
	// camera to the operator, interface options to the hub. Saved with the profile.
	constexpr int FrameRateCapCount = 4;
	constexpr int FrameRateCaps[FrameRateCapCount] = {30, 60, 120, 0};   // 0 = unlimited
	constexpr int WindowModeCount = 3;                                  // 0 full screen, 1 borderless window, 2 window

	// Graphics detail (owner request 2026-10-08: "customisations that allow to lower graphics and other stuff that causes lag").
	// Each group can follow the Quality preset (-1) or be set on its own (0 Low .. 3 Epic); the Unreal layer maps them to the
	// engine's scalability groups (UBIBGameInstance::ApplyUserSettings).
	enum EGraphicsGroup
	{
		GfxViewDistance,
		GfxShadows,
		GfxLighting,         // global illumination (Lumen on High / Epic)
		GfxReflections,      // Lumen reflections on High / Epic, screen-space below
		GfxPostProcess,
		GfxTextures,
		GfxEffects,
		GfxAntiAliasing,
		GfxFoliage,
		GfxShading,
		GraphicsGroupCount
	};
	const char* GraphicsGroupName(int Group);
	constexpr int WeatherDetailCount = 3;   // 0 Off (sky, light and sound only), 1 Low (rain, no splashes / puddles), 2 Full
	const char* WeatherDetailName(int Detail);
	constexpr int ResolutionScaleMin = 30;  // % of the screen resolution the 3D view renders at (upscaled back)

	struct FSettings
	{
		int MasterVolume = 100;        // 0..100, the audio device's primary volume
		int MusicVolume = 70;          // 0..100: kept for the music and voice the demo does not have yet
		int SfxVolume = 80;
		int VoiceVolume = 80;
		int GraphicsQuality = 2;       // 0 Low .. 3 Epic (engine scalability)
		int WindowMode = 1;            // see WindowModeCount (applied when changed; the editor's play window ignores it)
		bool bVSync = false;
		int FrameRateCap = 1;          // index into FrameRateCaps
		int ResolutionScale = 100;     // ResolutionScaleMin..100 % of the screen resolution (3D only)
		int GraphicsDetail[GraphicsGroupCount] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   // -1 = follow GraphicsQuality
		int WeatherDetail = 2;         // WeatherDetailCount
		bool bVolumetricFog = true;    // light shafts through fog and rain (costly)
		bool bAnimeLook = true;        // the anime post-process (lines, bands, 12 fps hold); off = plain image, cheaper
		bool bDynamicResolution = false; // lower the render scale on its own when a frame takes too long
		bool bShowFps = false;         // frame-rate counter
		bool bShowControlsHint = true; // HUD controls line
		int MouseSensitivity = 5;      // 1..10
		bool bInvertY = false;         // invert the camera's vertical look
		int FieldOfView = 90;          // camera field of view, 70..110 degrees
		bool bPageTransitions = true;  // 0.3 s fade + slide between hub pages
		int LobbyOperator = -1;        // roster index shown in the lobby, -1 = first team member
		// Owner request 2026-10-06 ("whatever preferences the game could possibly need"):
		int TopBar[TopBarSlots] = {0, 2, 3, 1, 4, 5};   // ECurrency values: Gold, Bytes, Angel hearts, Diamonds, Regular, Special
		std::string KeyBinds[KeyBindCount];             // "" = DefaultKeyName
		bool bDamageNumbers = true;    // floating damage numbers in battle
		bool bCameraShake = true;      // camera shake on heavy hits
		bool bReduceMotion = false;    // no page transitions, no camera shake, calmer minigame effects
		bool bSoundCaptions = false;   // on-screen captions for game sounds (Media's direction in Night Shift, Kuu's thumping)
		bool bConfirmSpend = false;    // ask again (second tap) before spending Diamonds - opt-in
		bool bNoticesAtLogin = true;   // open the notice board when there is an unread notice
		int HudScale = 100;            // 75..125 % battle HUD and arcade text size
		int RhythmOffsetMs = 0;        // -200..200 audio/visual timing offset for Rhythm
		bool bBackgroundAudio = false; // keep sound playing when the window loses focus
		// Combat feel (owner task 2026-10-12; BIBGame/BIBCombatFeel.h). Presentation only: none of these changes damage, cooldowns,
		// radii or durations. 100 % with every switch on is the game as it was.
		int CombatFeel = 100;          // 0..150 % (steps of 25): strength of hit stop, shake, flash, field-of-view punch, rings; 0 = all off
		bool bFeelHitStop = true;      // brief hold of the struck angel and the operator on a hit
		bool bFeelFlash = true;        // body flash on a hit
		bool bFeelFov = true;          // field-of-view punch on heavy hits, parries and ultimates
		bool bFeelBeats = true;        // ultimate beat: vignette pulse and a very short slow-down
		bool bFeelTelegraph = true;    // ground rings for the operator's own windups and delayed area attacks
	};

	// Inventory and events (owner request 2026-10-07; BIBInventory.h, BIBEvents.h).
	constexpr int ItemCount = 13;            // EItem: 12 Outer World boosts (Gold / Bytes / hearts x 3, 6, 12, 24 h) + Charge pack
	constexpr int EventStatCount = 10;       // Events::EStat
	constexpr int EventMissionCount = 8;     // weekly operation missions
	constexpr int EventShopCount = 10;
	constexpr int RookieMissionCount = 21;   // Rookie week: 3 missions a day for 7 days
	constexpr int ArkShopCount = 8;
	struct FEventState
	{
		int WeekKey = -1;                              // week (Profile.Day - 1) / 7 the weekly counters belong to
		int Counters[EventStatCount] = {};
		bool MissionClaimed[EventMissionCount] = {};
		int ShopBought[EventShopCount] = {};
		int RookieStartDay = 0;                        // Profile.Day the account started on (0 = no Rookie week: older accounts)
		int RookieCounters[EventStatCount] = {};
		bool RookieClaimed[RookieMissionCount] = {};
		int ArkShopKey = 0;                            // season key (month) the Ark shop limits belong to
		int ArkShopBought[ArkShopCount] = {};
		// Ticket road (owner request 2026-10-09; BIBEvents.h). Saved under "events"/"road"; missing in older saves = a new road.
		int RoadLastDay = 0;                           // Profile.Day of the last stipend (0 = never)
		int RoadWeekKey = -1;                          // week the two counters below belong to
		int RoadWeekDays = 0;
		bool bRoadWeekClaimed = false;
		int RoadDays = 0;                              // stipends ever
		int RoadMilestones = 0;                        // milestones claimed
		int RoadTickets = 0;                           // tickets the road has paid
	};

	const char* WindowModeName(int Mode);
	const char* QualityName(int Quality);

	struct FMeta
	{
		std::string Name = "Commander";
		int Uid = 10000001;
		int CommanderXp = 0;

		std::vector<FMail> Mail;
		int NextMailId = 1;

		std::vector<FQuestProgress> Daily;
		std::vector<FQuestProgress> Weekly;
		int WeekIndex = 0;                     // (Day - 1) / 7 when the weekly quests were last reset
		std::vector<FQuestProgress> Story;     // only bClaimed is used: progress is read from the campaign
		std::vector<FQuestProgress> Milestones;

		int LoginStreak = 0;
		int LoginLastDay = 0;

		int SeasonXp = 0;
		bool bPremiumPass = false;
		bool FreeClaimed[SeasonTiers + 1] = {};
		bool PremiumClaimed[SeasonTiers + 1] = {};

		FBuildingState Buildings[BuildingCount];
		double WorldHours = 0.0;

		FArcadeRecords ArcadeRecords;

		int TowerBest[2] = {0, 0};
		int TowerLostAt[2] = {0, 0};   // the floor of the last failed climb per mode (0 = never failed)
		FArenaState Arena;
		int ArenaWins = 0;
		int ArenaLosses = 0;
		int ArenaAttempts = 5;
		std::string ArenaLog;
		int SubStoryCleared[SubStoryCount] = {0, 0, 0, 0};

		bool bStarterBundleBought = false;
		int IapSpentCents = 0; // simulated: no real payment is ever taken in the demo

		int NarratorExchangeFrom = 0; // team option: Global Exchange presets (stat index)
		int NarratorExchangeTo = 1;

		int Summons = 0;
		FSettings Settings;

		// Live operations (owner request 2026-10-06; BIBLiveOps.h)
		long long LastActiveUnix = 0;          // last time the game ran with this profile (returning-player check)
		int LastUtcDay = 0;                    // UTC day index of the last daily reset driven by the clock (0 = never)
		int SeasonPassKey = 0;                 // year * 12 + month - 1 of the season pass the XP belongs to (it lasts one month)
		FReturnState Return;
		FGangLocal Gang;
		std::vector<std::string> ReadNotices;  // server notice ids already opened (newest 60)
		std::vector<std::string> ImportedMail; // server mail ids already in the mailbox (newest 200)

		// Inventory, Charge, Monthly supply and events (owner request 2026-10-07; BIBInventory.h, BIBEvents.h)
		int Items[ItemCount] = {};
		long long ChargeUnix = 0;              // real time the Charge regeneration has been counted up to (0 = no clock yet)
		int ChargeRefillsToday = 0;
		int MonthlyDays = 0;                   // Monthly supply days still to be delivered (one mail a day)
		FEventState Events;

		// Where the player last stood in the Ark (owner request 2026-10-07: the game starts in the Ark at the saved position).
		// Unreal units (cm) in L_Ark; bArkPosValid = false for a new account (the plaza spawn is used).
		bool bArkPosValid = false;
		double ArkPos[3] = {0.0, 0.0, 0.0};
		double ArkYaw = 0.0;
	};

	FMeta MakeNewMeta();
	void ResetDaily(FMeta& Meta, int NewDay);
}
