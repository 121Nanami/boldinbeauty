// Boldinbeauty demo — save system format (see BIBSave.h).
#include "BIBSave.h"
#include "BIBConfig.h"
#include "BIBJson.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace BIB
{
	namespace Save
	{
		using Json::FValue;

		namespace
		{
			constexpr const char* FormatName = "boldinbeauty-save";
			constexpr int MaxMail = 100;
			constexpr size_t MaxText = 400;
			constexpr size_t MaxName = 24;
			constexpr int MaxCurrency = 2000000000;

			const char* CurrencyKeys[static_cast<int>(ECurrency::Count)] = {
				"gold", "diamonds", "bytes", "angelHearts", "regularTicket", "specialTicket", "dailyTicket", "mileage", "arcadeToken", "gangCoin",
				"charge", "elementCore", "eventToken", "arkMedal"};

			const char* TutorialNames[static_cast<int>(ETutorialStep::Count)] = {
				"openOperators", "selectLuna", "levelUpLuna", "openTeam", "addLuna", "openSummon", "summonKuu", "openCampaign",
				"playLevel1", "complete"};

			FValue Num(double Value) { return FValue::MakeNumber(Value); }
			FValue Str(const std::string& Value) { return FValue::MakeString(Value); }
			FValue Flag(bool bValue) { return FValue::MakeBool(bValue); }

			int Clamp(int Value, int Low, int High) { return std::min(std::max(Value, Low), High); }

			std::string Limit(const std::string& Text, size_t Max)
			{
				return Text.size() <= Max ? Text : Text.substr(0, Max);
			}

			std::string OperatorKey(int RosterIndex)
			{
				return RosterIndex >= 0 && RosterIndex < static_cast<int>(Roster().size()) ? Roster()[static_cast<size_t>(RosterIndex)].Name : std::string();
			}

			std::string Bits(const bool* Values, int Count)
			{
				std::string Text;
				for (int Index = 0; Index < Count; ++Index)
				{
					Text += Values[Index] ? '1' : '0';
				}
				return Text;
			}

			void ReadBits(const std::string& Text, bool* Values, int Count)
			{
				for (int Index = 0; Index < Count; ++Index)
				{
					Values[Index] = static_cast<size_t>(Index) < Text.size() && Text[static_cast<size_t>(Index)] == '1';
				}
			}

			// ---------------------------------------------------------- rewards
			FValue RewardToJson(const FReward& R)
			{
				FValue Out = FValue::MakeObject();
				Out.Set("gold", Num(R.Gold));
				Out.Set("bytes", Num(R.Bytes));
				Out.Set("hearts", Num(R.Hearts));
				Out.Set("diamonds", Num(R.Diamonds));
				Out.Set("regular", Num(R.RegularTickets));
				Out.Set("special", Num(R.SpecialTickets));
				Out.Set("arcade", Num(R.ArcadeTokens));
				Out.Set("gangCoins", Num(R.GangCoins));
				Out.Set("seasonXp", Num(R.SeasonXp));
				Out.Set("commanderXp", Num(R.CommanderXp));
				Out.Set("equipment", Num(R.Equipment));
				Out.Set("elementCores", Num(R.ElementCores));
				Out.Set("eventTokens", Num(R.EventTokens));
				Out.Set("arkMedals", Num(R.ArkMedals));
				Out.Set("charge", Num(R.Charge));
				Out.Set("item", Num(R.Item));
				Out.Set("itemAmount", Num(R.ItemAmount));
				return Out;
			}

			FReward RewardFromJson(const FValue* Source)
			{
				FReward R;
				if (!Source || !Source->IsObject())
				{
					return R;
				}
				R.Gold = Clamp(Source->GetInt("gold"), 0, MaxCurrency);
				R.Bytes = Clamp(Source->GetInt("bytes"), 0, MaxCurrency);
				R.Hearts = Clamp(Source->GetInt("hearts"), 0, MaxCurrency);
				R.Diamonds = Clamp(Source->GetInt("diamonds"), 0, MaxCurrency);
				R.RegularTickets = Clamp(Source->GetInt("regular"), 0, MaxCurrency);
				R.SpecialTickets = Clamp(Source->GetInt("special"), 0, MaxCurrency);
				R.ArcadeTokens = Clamp(Source->GetInt("arcade"), 0, MaxCurrency);
				R.GangCoins = Clamp(Source->GetInt("gangCoins"), 0, MaxCurrency);
				R.SeasonXp = Clamp(Source->GetInt("seasonXp"), 0, MaxCurrency);
				R.CommanderXp = Clamp(Source->GetInt("commanderXp"), 0, MaxCurrency);
				R.Equipment = Clamp(Source->GetInt("equipment"), 0, 1000);
				R.ElementCores = Clamp(Source->GetInt("elementCores"), 0, MaxCurrency);
				R.EventTokens = Clamp(Source->GetInt("eventTokens"), 0, MaxCurrency);
				R.ArkMedals = Clamp(Source->GetInt("arkMedals"), 0, MaxCurrency);
				R.Charge = Clamp(Source->GetInt("charge"), 0, MaxCurrency);
				R.Item = Clamp(Source->GetInt("item", -1), -1, ItemCount - 1);
				R.ItemAmount = R.Item < 0 ? 0 : Clamp(Source->GetInt("itemAmount"), 0, 1000);
				return R;
			}

			FValue IntsToJson(const int* Values, int Count)
			{
				FValue Out = FValue::MakeArray();
				for (int Index = 0; Index < Count; ++Index)
				{
					Out.Push(Num(Values[Index]));
				}
				return Out;
			}

			void IntsFromJson(const FValue* Source, int* Values, int Count, int Low, int High)
			{
				if (!Source || !Source->IsArray())
				{
					return;
				}
				for (int Index = 0; Index < Count && static_cast<size_t>(Index) < Source->Size(); ++Index)
				{
					Values[Index] = Clamp(Source->At(static_cast<size_t>(Index)).AsInt(Low), Low, High);
				}
			}

			// Operators in a team are saved by name (like the squad), so the roster order can change.
			FValue TeamToJson(const int* Team)
			{
				FValue Out = FValue::MakeArray();
				for (int Slot = 0; Slot < 3; ++Slot)
				{
					Out.Push(Str(OperatorKey(Team[Slot])));
				}
				return Out;
			}

			void TeamFromJson(const FValue* Source, int* Team)
			{
				if (!Source || !Source->IsArray())
				{
					return;
				}
				for (int Slot = 0; Slot < 3; ++Slot)
				{
					const std::string Name = Source->At(static_cast<size_t>(Slot)).AsString();
					Team[Slot] = Name.empty() ? -1 : FindOperator(Name.c_str());
				}
			}

			FValue StringsToJson(const std::vector<std::string>& Values)
			{
				FValue Out = FValue::MakeArray();
				for (const std::string& Value : Values)
				{
					Out.Push(Str(Value));
				}
				return Out;
			}

			void StringsFromJson(const FValue* Source, std::vector<std::string>& Values, size_t Max)
			{
				Values.clear();
				if (!Source || !Source->IsArray())
				{
					return;
				}
				const size_t First = Source->Size() > Max ? Source->Size() - Max : 0;
				for (size_t Index = First; Index < Source->Size(); ++Index)
				{
					const std::string Value = Limit(Source->At(Index).AsString(), 64);
					if (!Value.empty())
					{
						Values.push_back(Value);
					}
				}
			}

			// ------------------------------------------------------------ meta
			FValue QuestsToJson(const std::vector<FQuestProgress>& Quests)
			{
				FValue Out = FValue::MakeArray();
				for (const FQuestProgress& Quest : Quests)
				{
					FValue Item = FValue::MakeObject();
					Item.Set("progress", Num(Quest.Progress));
					Item.Set("claimed", Flag(Quest.bClaimed));
					Out.Push(Item);
				}
				return Out;
			}

			void QuestsFromJson(const FValue* Source, std::vector<FQuestProgress>& Quests)
			{
				if (!Source || !Source->IsArray())
				{
					return;
				}
				for (size_t Index = 0; Index < Quests.size() && Index < Source->Size(); ++Index)
				{
					const FValue& Item = Source->At(Index);
					Quests[Index].Progress = Clamp(Item.GetInt("progress"), 0, MaxCurrency);
					Quests[Index].bClaimed = Item.GetBool("claimed");
				}
			}

			FValue MetaToJson(const FMeta& Meta)
			{
				FValue Out = FValue::MakeObject();
				Out.Set("name", Str(Meta.Name));
				Out.Set("uid", Num(Meta.Uid));
				Out.Set("commanderXp", Num(Meta.CommanderXp));

				FValue Mail = FValue::MakeArray();
				for (const FMail& Entry : Meta.Mail)
				{
					FValue Item = FValue::MakeObject();
					Item.Set("id", Num(Entry.Id));
					Item.Set("title", Str(Entry.Title));
					Item.Set("body", Str(Entry.Body));
					Item.Set("reward", RewardToJson(Entry.Reward));
					Item.Set("claimed", Flag(Entry.bClaimed));
					if (!Entry.From.empty())
					{
						Item.Set("from", Str(Entry.From));
					}
					Mail.Push(Item);
				}
				Out.Set("mail", Mail);
				Out.Set("nextMailId", Num(Meta.NextMailId));
				Out.Set("daily", QuestsToJson(Meta.Daily));
				Out.Set("weekly", QuestsToJson(Meta.Weekly));
				Out.Set("weekIndex", Num(Meta.WeekIndex));
				Out.Set("story", QuestsToJson(Meta.Story));
				Out.Set("milestones", QuestsToJson(Meta.Milestones));
				Out.Set("loginStreak", Num(Meta.LoginStreak));
				Out.Set("loginLastDay", Num(Meta.LoginLastDay));

				Out.Set("seasonXp", Num(Meta.SeasonXp));
				Out.Set("premiumPass", Flag(Meta.bPremiumPass));
				Out.Set("freeClaimed", Str(Bits(Meta.FreeClaimed, SeasonTiers + 1)));
				Out.Set("premiumClaimed", Str(Bits(Meta.PremiumClaimed, SeasonTiers + 1)));

				FValue Buildings = FValue::MakeArray();
				for (int Index = 0; Index < BuildingCount; ++Index)
				{
					const FBuildingState& Building = Meta.Buildings[Index];
					FValue Item = FValue::MakeObject();
					Item.Set("built", Flag(Building.bBuilt));
					Item.Set("level", Num(Building.Level));
					Item.Set("gold", Num(Building.StoredGold));
					Item.Set("bytes", Num(Building.StoredBytes));
					Item.Set("hearts", Num(Building.StoredHearts));
					Buildings.Push(Item);
				}
				Out.Set("buildings", Buildings);
				Out.Set("worldHours", Num(Meta.WorldHours));
				if (Meta.bArkPosValid)
				{
					FValue ArkPos = FValue::MakeObject();
					ArkPos.Set("x", Num(Meta.ArkPos[0]));
					ArkPos.Set("y", Num(Meta.ArkPos[1]));
					ArkPos.Set("z", Num(Meta.ArkPos[2]));
					ArkPos.Set("yaw", Num(Meta.ArkYaw));
					Out.Set("arkPosition", ArkPos);
				}

				const FArcadeRecords& R = Meta.ArcadeRecords;
				FValue Arcade = FValue::MakeObject();
				Arcade.Set("plays", IntsToJson(R.Plays, ArcadeGameCount));
				Arcade.Set("nightWins", IntsToJson(R.NightWins, ArcadeDifficultyCount));
				Arcade.Set("nightBestHour", IntsToJson(R.NightBestHour, ArcadeDifficultyCount));
				Arcade.Set("rhythmBest", IntsToJson(R.RhythmBest, RhythmSongCount * ArcadeDifficultyCount));
				Arcade.Set("rhythmStars", IntsToJson(R.RhythmStars, RhythmSongCount * ArcadeDifficultyCount));
				Arcade.Set("bossRushBest", Num(R.BossRushBest));
				Arcade.Set("miniBest", IntsToJson(R.MiniBest, MiniGameRecordCount * ArcadeDifficultyCount));
				Arcade.Set("miniStars", IntsToJson(R.MiniStars, MiniGameRecordCount * ArcadeDifficultyCount));
				Arcade.Set("miniWins", IntsToJson(R.MiniWins, MiniGameRecordCount * ArcadeDifficultyCount));
				Arcade.Set("rallyTrack", Str(R.RallyTrack));
				Arcade.Set("rallyVehicle", Num(R.RallyVehicle));
				Arcade.Set("ucnPlays", IntsToJson(R.UcnPlays, UcnModeCount));
				Arcade.Set("ucnWins", IntsToJson(R.UcnWins, UcnModeCount));
				Arcade.Set("ucnBestHour", IntsToJson(R.UcnBestHour, UcnModeCount));
				Arcade.Set("ucnUnlocked", FValue::MakeBool(R.bUcnUnlocked));
				Arcade.Set("ucnRewardClaimed", FValue::MakeBool(R.bUcnRewardClaimed));
				Out.Set("arcadeRecords", Arcade);

				FValue Tower = FValue::MakeArray();
				Tower.Push(Num(Meta.TowerBest[0]));
				Tower.Push(Num(Meta.TowerBest[1]));
				Out.Set("towerBest", Tower);
				Out.Set("towerLostAt", IntsToJson(Meta.TowerLostAt, 2));
				const FArenaState& A = Meta.Arena;
				FValue Arena = FValue::MakeObject();
				Arena.Set("attack", TeamToJson(A.Attack));
				Arena.Set("defence", TeamToJson(A.Defence));
				Arena.Set("trophies", Num(A.Trophies));
				Arena.Set("championFloor", Flag(A.bChampionFloor));
				Arena.Set("season", Num(A.SeasonKey));
				Arena.Set("pendingRank", Num(A.PendingRewardRank));
				Arena.Set("pendingSeason", Num(A.PendingRewardSeason));
				Arena.Set("defenceWins", Num(A.DefenceWins));
				Arena.Set("defenceLosses", Num(A.DefenceLosses));
				Out.Set("arena", Arena);
				Out.Set("arenaWins", Num(Meta.ArenaWins));
				Out.Set("arenaLosses", Num(Meta.ArenaLosses));
				Out.Set("arenaAttempts", Num(Meta.ArenaAttempts));
				Out.Set("arenaLog", Str(Meta.ArenaLog));
				FValue Stories = FValue::MakeArray();
				for (int Index = 0; Index < SubStoryCount; ++Index)
				{
					Stories.Push(Num(Meta.SubStoryCleared[Index]));
				}
				Out.Set("subStoryCleared", Stories);

				Out.Set("starterBundleBought", Flag(Meta.bStarterBundleBought));
				Out.Set("iapSpentCents", Num(Meta.IapSpentCents));
				Out.Set("narratorExchangeFrom", Num(Meta.NarratorExchangeFrom));
				Out.Set("narratorExchangeTo", Num(Meta.NarratorExchangeTo));
				Out.Set("summons", Num(Meta.Summons));

				FValue Settings = FValue::MakeObject();
				Settings.Set("music", Num(Meta.Settings.MusicVolume));
				Settings.Set("sfx", Num(Meta.Settings.SfxVolume));
				Settings.Set("voice", Num(Meta.Settings.VoiceVolume));
				Settings.Set("graphics", Num(Meta.Settings.GraphicsQuality));
				Settings.Set("controlsHint", Flag(Meta.Settings.bShowControlsHint));
				Settings.Set("mouseSensitivity", Num(Meta.Settings.MouseSensitivity));
				Settings.Set("master", Num(Meta.Settings.MasterVolume));
				Settings.Set("windowMode", Num(Meta.Settings.WindowMode));
				Settings.Set("vsync", Flag(Meta.Settings.bVSync));
				Settings.Set("frameRateCap", Num(Meta.Settings.FrameRateCap));
				Settings.Set("resolutionScale", Num(Meta.Settings.ResolutionScale));
				Settings.Set("showFps", Flag(Meta.Settings.bShowFps));
				Settings.Set("invertY", Flag(Meta.Settings.bInvertY));
				Settings.Set("fov", Num(Meta.Settings.FieldOfView));
				Settings.Set("pageTransitions", Flag(Meta.Settings.bPageTransitions));
				Settings.Set("lobbyOperator", Str(OperatorKey(Meta.Settings.LobbyOperator)));
				Settings.Set("topBar", IntsToJson(Meta.Settings.TopBar, TopBarSlots));
				FValue Keys = FValue::MakeObject();
				for (int Index = 0; Index < KeyBindCount; ++Index)
				{
					if (!Meta.Settings.KeyBinds[Index].empty())
					{
						Keys.Set(std::to_string(Index), Str(Meta.Settings.KeyBinds[Index]));
					}
				}
				Settings.Set("keys", Keys);
				Settings.Set("damageNumbers", Flag(Meta.Settings.bDamageNumbers));
				Settings.Set("cameraShake", Flag(Meta.Settings.bCameraShake));
				Settings.Set("reduceMotion", Flag(Meta.Settings.bReduceMotion));
				Settings.Set("soundCaptions", Flag(Meta.Settings.bSoundCaptions));
				Settings.Set("confirmSpend", Flag(Meta.Settings.bConfirmSpend));
				Settings.Set("noticesAtLogin", Flag(Meta.Settings.bNoticesAtLogin));
				Settings.Set("hudScale", Num(Meta.Settings.HudScale));
				Settings.Set("rhythmOffsetMs", Num(Meta.Settings.RhythmOffsetMs));
				Settings.Set("backgroundAudio", Flag(Meta.Settings.bBackgroundAudio));
				Settings.Set("combatFeel", Num(Meta.Settings.CombatFeel));
				Settings.Set("feelHitStop", Flag(Meta.Settings.bFeelHitStop));
				Settings.Set("feelFlash", Flag(Meta.Settings.bFeelFlash));
				Settings.Set("feelFov", Flag(Meta.Settings.bFeelFov));
				Settings.Set("feelBeats", Flag(Meta.Settings.bFeelBeats));
				Settings.Set("feelTelegraph", Flag(Meta.Settings.bFeelTelegraph));
				Settings.Set("graphicsDetail", IntsToJson(Meta.Settings.GraphicsDetail, GraphicsGroupCount));
				Settings.Set("weatherDetail", Num(Meta.Settings.WeatherDetail));
				Settings.Set("volumetricFog", Flag(Meta.Settings.bVolumetricFog));
				Settings.Set("animeLook", Flag(Meta.Settings.bAnimeLook));
				Settings.Set("dynamicResolution", Flag(Meta.Settings.bDynamicResolution));
				Out.Set("settings", Settings);

				Out.Set("lastActive", Num(static_cast<double>(Meta.LastActiveUnix)));
				Out.Set("lastUtcDay", Num(Meta.LastUtcDay));
				Out.Set("seasonPassKey", Num(Meta.SeasonPassKey));
				const FReturnState& Ret = Meta.Return;
				FValue Return = FValue::MakeObject();
				Return.Set("start", Num(static_cast<double>(Ret.StartUnix)));
				Return.Set("days", Num(Ret.DaysClaimed));
				Return.Set("stages", Num(Ret.StageClears));
				Return.Set("summons", Num(Ret.Summons));
				Return.Set("levelUps", Num(Ret.LevelUps));
				Return.Set("dailies", Num(Ret.DailyQuests));
				Return.Set("ark", Num(Ret.ArkFights));
				Return.Set("claimed", Str(Bits(Ret.QuestClaimed, ReturnQuestCount)));
				Out.Set("return", Return);
				FValue Gang = FValue::MakeObject();
				Gang.Set("shopSeason", Num(Meta.Gang.ShopSeason));
				Gang.Set("shopBought", IntsToJson(Meta.Gang.ShopBought, GangShopItems));
				Gang.Set("pendingActivity", Num(Meta.Gang.PendingActivity));
				Out.Set("gang", Gang);
				Out.Set("readNotices", StringsToJson(Meta.ReadNotices));
				Out.Set("importedMail", StringsToJson(Meta.ImportedMail));
				// Inventory, Charge, Monthly supply and events (2026-10-07)
				Out.Set("items", IntsToJson(Meta.Items, ItemCount));
				Out.Set("chargeUnix", Num(static_cast<double>(Meta.ChargeUnix)));
				Out.Set("chargeRefills", Num(Meta.ChargeRefillsToday));
				Out.Set("monthlyDays", Num(Meta.MonthlyDays));
				const FEventState& Ev = Meta.Events;
				FValue Events = FValue::MakeObject();
				Events.Set("week", Num(Ev.WeekKey));
				Events.Set("counters", IntsToJson(Ev.Counters, EventStatCount));
				Events.Set("claimed", Str(Bits(Ev.MissionClaimed, EventMissionCount)));
				Events.Set("shop", IntsToJson(Ev.ShopBought, EventShopCount));
				Events.Set("rookieStart", Num(Ev.RookieStartDay));
				Events.Set("rookieCounters", IntsToJson(Ev.RookieCounters, EventStatCount));
				Events.Set("rookieClaimed", Str(Bits(Ev.RookieClaimed, RookieMissionCount)));
				Events.Set("arkShopKey", Num(Ev.ArkShopKey));
				Events.Set("arkShop", IntsToJson(Ev.ArkShopBought, ArkShopCount));
				// Ticket road (2026-10-09): absent in older saves, which load as a new road.
				FValue Road = FValue::MakeObject();
				Road.Set("lastDay", Num(Ev.RoadLastDay));
				Road.Set("weekKey", Num(Ev.RoadWeekKey));
				Road.Set("weekDays", Num(Ev.RoadWeekDays));
				Road.Set("weekClaimed", Num(Ev.bRoadWeekClaimed ? 1 : 0));
				Road.Set("days", Num(Ev.RoadDays));
				Road.Set("milestones", Num(Ev.RoadMilestones));
				Road.Set("tickets", Num(Ev.RoadTickets));
				Events.Set("road", Road);
				Out.Set("events", Events);
				return Out;
			}

			void MetaFromJson(const FValue& Source, FMeta& Meta)
			{
				if (!Source.IsObject())
				{
					return;
				}
				Meta.Name = Limit(Source.GetString("name", Meta.Name), MaxName);
				Meta.Uid = Clamp(Source.GetInt("uid", Meta.Uid), 1, MaxCurrency);
				Meta.CommanderXp = Clamp(Source.GetInt("commanderXp"), 0, MaxCurrency);

				if (const FValue* Mail = Source.Find("mail"))
				{
					Meta.Mail.clear();
					for (size_t Index = 0; Index < Mail->Size() && static_cast<int>(Index) < MaxMail; ++Index)
					{
						const FValue& Item = Mail->At(Index);
						FMail Entry;
						Entry.Id = Clamp(Item.GetInt("id"), 1, MaxCurrency);
						Entry.Title = Limit(Item.GetString("title"), MaxText);
						Entry.Body = Limit(Item.GetString("body"), MaxText);
						Entry.Reward = RewardFromJson(Item.Find("reward"));
						Entry.bClaimed = Item.GetBool("claimed");
						Entry.From = Limit(Item.GetString("from"), 24);
						Meta.Mail.push_back(Entry);
					}
				}
				Meta.NextMailId = Clamp(Source.GetInt("nextMailId", Meta.NextMailId), 1, MaxCurrency);
				for (const FMail& Entry : Meta.Mail)
				{
					Meta.NextMailId = std::max(Meta.NextMailId, Entry.Id + 1);
				}
				QuestsFromJson(Source.Find("daily"), Meta.Daily);
				QuestsFromJson(Source.Find("weekly"), Meta.Weekly);
				Meta.WeekIndex = Clamp(Source.GetInt("weekIndex"), 0, 100000);
				QuestsFromJson(Source.Find("story"), Meta.Story);
				QuestsFromJson(Source.Find("milestones"), Meta.Milestones);
				Meta.LoginStreak = Clamp(Source.GetInt("loginStreak"), 0, 100000);
				Meta.LoginLastDay = Clamp(Source.GetInt("loginLastDay"), 0, 100000);

				Meta.SeasonXp = Clamp(Source.GetInt("seasonXp"), 0, MaxCurrency);
				Meta.bPremiumPass = Source.GetBool("premiumPass");
				ReadBits(Source.GetString("freeClaimed"), Meta.FreeClaimed, SeasonTiers + 1);
				ReadBits(Source.GetString("premiumClaimed"), Meta.PremiumClaimed, SeasonTiers + 1);

				if (const FValue* Buildings = Source.Find("buildings"))
				{
					for (int Index = 0; Index < BuildingCount && static_cast<size_t>(Index) < Buildings->Size(); ++Index)
					{
						const FValue& Item = Buildings->At(static_cast<size_t>(Index));
						FBuildingState& Building = Meta.Buildings[Index];
						Building.bBuilt = Item.GetBool("built");
						Building.Level = Clamp(Item.GetInt("level"), 0, MaxBuildingLevel);
						Building.StoredGold = std::max(0.0, Item.GetNumber("gold"));
						Building.StoredBytes = std::max(0.0, Item.GetNumber("bytes"));
						Building.StoredHearts = std::max(0.0, Item.GetNumber("hearts"));
					}
				}
				Meta.WorldHours = std::max(0.0, Source.GetNumber("worldHours"));
				// Older saves have no Ark position: the player starts on the plaza. Out-of-range values are dropped the same way.
				Meta.bArkPosValid = false;
				if (const FValue* ArkPos = Source.Find("arkPosition"))
				{
					const double X = ArkPos->GetNumber("x"), Y = ArkPos->GetNumber("y"), Z = ArkPos->GetNumber("z");
					const double Limit = 2000000.0;   // 20 km: far outside any map, so anything bigger is a corrupt value
					if (std::isfinite(X) && std::isfinite(Y) && std::isfinite(Z) && std::fabs(X) < Limit && std::fabs(Y) < Limit && std::fabs(Z) < Limit)
					{
						Meta.ArkPos[0] = X;
						Meta.ArkPos[1] = Y;
						Meta.ArkPos[2] = Z;
						const double Yaw = ArkPos->GetNumber("yaw");
						Meta.ArkYaw = std::isfinite(Yaw) ? std::fmod(Yaw, 360.0) : 0.0;
						Meta.bArkPosValid = true;
					}
				}

				if (const FValue* Arcade = Source.Find("arcadeRecords"))
				{
					FArcadeRecords& R = Meta.ArcadeRecords;
					IntsFromJson(Arcade->Find("plays"), R.Plays, ArcadeGameCount, 0, MaxCurrency);
					IntsFromJson(Arcade->Find("nightWins"), R.NightWins, ArcadeDifficultyCount, 0, MaxCurrency);
					IntsFromJson(Arcade->Find("nightBestHour"), R.NightBestHour, ArcadeDifficultyCount, 0, 6);
					IntsFromJson(Arcade->Find("rhythmBest"), R.RhythmBest, RhythmSongCount * ArcadeDifficultyCount, 0, MaxCurrency);
					IntsFromJson(Arcade->Find("rhythmStars"), R.RhythmStars, RhythmSongCount * ArcadeDifficultyCount, 0, 5);
					R.BossRushBest = Clamp(Arcade->GetInt("bossRushBest"), 0, 999);
					// The 2026-10-07 mini-games: older saves have no such arrays and keep the zeros.
					IntsFromJson(Arcade->Find("miniBest"), R.MiniBest, MiniGameRecordCount * ArcadeDifficultyCount, 0, MaxCurrency);
					IntsFromJson(Arcade->Find("miniStars"), R.MiniStars, MiniGameRecordCount * ArcadeDifficultyCount, 0, 5);
					IntsFromJson(Arcade->Find("miniWins"), R.MiniWins, MiniGameRecordCount * ArcadeDifficultyCount, 0, MaxCurrency);
					// 2026-10-15: Ark Rally (the fourth mini-game slot) and the player's own track code.
					if (const FValue* Track = Arcade->Find("rallyTrack"))
					{
						R.RallyTrack = Track->AsString().substr(0, 600);
					}
					R.RallyVehicle = Clamp(Arcade->GetInt("rallyVehicle"), 0, 1);
					// Ultimate Custom Night (2026-10-14): older saves have none of these and keep the defaults.
					IntsFromJson(Arcade->Find("ucnPlays"), R.UcnPlays, UcnModeCount, 0, MaxCurrency);
					IntsFromJson(Arcade->Find("ucnWins"), R.UcnWins, UcnModeCount, 0, MaxCurrency);
					IntsFromJson(Arcade->Find("ucnBestHour"), R.UcnBestHour, UcnModeCount, 0, 6);
					R.bUcnUnlocked = Arcade->GetBool("ucnUnlocked");
					R.bUcnRewardClaimed = Arcade->GetBool("ucnRewardClaimed");
				}

				if (const FValue* Tower = Source.Find("towerBest"))
				{
					Meta.TowerBest[0] = Clamp(Tower->At(0).AsInt(), 0, MaxTowerFloor);
					Meta.TowerBest[1] = Clamp(Tower->At(1).AsInt(), 0, MaxTowerFloor);
				}
				IntsFromJson(Source.Find("towerLostAt"), Meta.TowerLostAt, 2, 0, MaxTowerFloor);
				if (const FValue* Arena = Source.Find("arena"))
				{
					FArenaState& A = Meta.Arena;
					TeamFromJson(Arena->Find("attack"), A.Attack);
					TeamFromJson(Arena->Find("defence"), A.Defence);
					A.Trophies = Clamp(Arena->GetInt("trophies"), 0, MaxCurrency);
					A.bChampionFloor = Arena->GetBool("championFloor");
					A.SeasonKey = Clamp(Arena->GetInt("season"), 0, 1000000);
					A.PendingRewardRank = Clamp(Arena->GetInt("pendingRank", -1), -1, 15);
					A.PendingRewardSeason = Clamp(Arena->GetInt("pendingSeason"), 0, 1000000);
					A.DefenceWins = Clamp(Arena->GetInt("defenceWins"), 0, MaxCurrency);
					A.DefenceLosses = Clamp(Arena->GetInt("defenceLosses"), 0, MaxCurrency);
				}
				Meta.ArenaWins = Clamp(Source.GetInt("arenaWins"), 0, MaxCurrency);
				Meta.ArenaLosses = Clamp(Source.GetInt("arenaLosses"), 0, MaxCurrency);
				Meta.ArenaAttempts = Clamp(Source.GetInt("arenaAttempts", Meta.ArenaAttempts), 0, 1000);
				Meta.ArenaLog = Limit(Source.GetString("arenaLog"), MaxText);
				if (const FValue* Stories = Source.Find("subStoryCleared"))
				{
					for (int Index = 0; Index < SubStoryCount; ++Index)
					{
						Meta.SubStoryCleared[Index] = Clamp(Stories->At(static_cast<size_t>(Index)).AsInt(), 0, SubStoryLevels(Index));
					}
				}

				Meta.bStarterBundleBought = Source.GetBool("starterBundleBought");
				Meta.IapSpentCents = Clamp(Source.GetInt("iapSpentCents"), 0, MaxCurrency);
				Meta.NarratorExchangeFrom = Clamp(Source.GetInt("narratorExchangeFrom", Meta.NarratorExchangeFrom), 0, 4);
				Meta.NarratorExchangeTo = Clamp(Source.GetInt("narratorExchangeTo", Meta.NarratorExchangeTo), 0, 4);
				Meta.Summons = Clamp(Source.GetInt("summons"), 0, MaxCurrency);

				if (const FValue* Settings = Source.Find("settings"))
				{
					Meta.Settings.MusicVolume = Clamp(Settings->GetInt("music", Meta.Settings.MusicVolume), 0, 100);
					Meta.Settings.SfxVolume = Clamp(Settings->GetInt("sfx", Meta.Settings.SfxVolume), 0, 100);
					Meta.Settings.VoiceVolume = Clamp(Settings->GetInt("voice", Meta.Settings.VoiceVolume), 0, 100);
					Meta.Settings.GraphicsQuality = Clamp(Settings->GetInt("graphics", Meta.Settings.GraphicsQuality), 0, 3);
					Meta.Settings.bShowControlsHint = Settings->GetBool("controlsHint", Meta.Settings.bShowControlsHint);
					Meta.Settings.MouseSensitivity = Clamp(Settings->GetInt("mouseSensitivity", Meta.Settings.MouseSensitivity), 1, 10);
					Meta.Settings.MasterVolume = Clamp(Settings->GetInt("master", Meta.Settings.MasterVolume), 0, 100);
					Meta.Settings.WindowMode = Clamp(Settings->GetInt("windowMode", Meta.Settings.WindowMode), 0, WindowModeCount - 1);
					Meta.Settings.bVSync = Settings->GetBool("vsync", Meta.Settings.bVSync);
					Meta.Settings.FrameRateCap = Clamp(Settings->GetInt("frameRateCap", Meta.Settings.FrameRateCap), 0, FrameRateCapCount - 1);
					Meta.Settings.ResolutionScale = Clamp(Settings->GetInt("resolutionScale", Meta.Settings.ResolutionScale), ResolutionScaleMin, 100);
					Meta.Settings.bShowFps = Settings->GetBool("showFps", Meta.Settings.bShowFps);
					Meta.Settings.bInvertY = Settings->GetBool("invertY", Meta.Settings.bInvertY);
					Meta.Settings.FieldOfView = Clamp(Settings->GetInt("fov", Meta.Settings.FieldOfView), 70, 110);
					Meta.Settings.bPageTransitions = Settings->GetBool("pageTransitions", Meta.Settings.bPageTransitions);
					const std::string Lobby = Settings->GetString("lobbyOperator");
					Meta.Settings.LobbyOperator = Lobby.empty() ? -1 : FindOperator(Lobby.c_str());
					IntsFromJson(Settings->Find("topBar"), Meta.Settings.TopBar, TopBarSlots, 0, TopBarChoiceCount - 1);
					if (const FValue* Keys = Settings->Find("keys"))
					{
						for (int Index = 0; Index < KeyBindCount; ++Index)
						{
							const std::string Key = Limit(Keys->GetString(std::to_string(Index)), 40);
							bool bClean = true;
							for (char Character : Key)
							{
								bClean = bClean && (std::isalnum(static_cast<unsigned char>(Character)) || Character == '_');
							}
							Meta.Settings.KeyBinds[Index] = bClean ? Key : std::string();
						}
					}
					Meta.Settings.bDamageNumbers = Settings->GetBool("damageNumbers", Meta.Settings.bDamageNumbers);
					Meta.Settings.bCameraShake = Settings->GetBool("cameraShake", Meta.Settings.bCameraShake);
					Meta.Settings.bReduceMotion = Settings->GetBool("reduceMotion", Meta.Settings.bReduceMotion);
					Meta.Settings.bSoundCaptions = Settings->GetBool("soundCaptions", Meta.Settings.bSoundCaptions);
					Meta.Settings.bConfirmSpend = Settings->GetBool("confirmSpend", Meta.Settings.bConfirmSpend);
					Meta.Settings.bNoticesAtLogin = Settings->GetBool("noticesAtLogin", Meta.Settings.bNoticesAtLogin);
					Meta.Settings.HudScale = Clamp(Settings->GetInt("hudScale", Meta.Settings.HudScale), 75, 125);
					Meta.Settings.RhythmOffsetMs = Clamp(Settings->GetInt("rhythmOffsetMs", Meta.Settings.RhythmOffsetMs), -200, 200);
					Meta.Settings.bBackgroundAudio = Settings->GetBool("backgroundAudio", Meta.Settings.bBackgroundAudio);
					Meta.Settings.CombatFeel = Clamp(Settings->GetInt("combatFeel", Meta.Settings.CombatFeel), 0, 150);
					Meta.Settings.bFeelHitStop = Settings->GetBool("feelHitStop", Meta.Settings.bFeelHitStop);
					Meta.Settings.bFeelFlash = Settings->GetBool("feelFlash", Meta.Settings.bFeelFlash);
					Meta.Settings.bFeelFov = Settings->GetBool("feelFov", Meta.Settings.bFeelFov);
					Meta.Settings.bFeelBeats = Settings->GetBool("feelBeats", Meta.Settings.bFeelBeats);
					Meta.Settings.bFeelTelegraph = Settings->GetBool("feelTelegraph", Meta.Settings.bFeelTelegraph);
					IntsFromJson(Settings->Find("graphicsDetail"), Meta.Settings.GraphicsDetail, GraphicsGroupCount, -1, 3);
					Meta.Settings.WeatherDetail = Clamp(Settings->GetInt("weatherDetail", Meta.Settings.WeatherDetail), 0, WeatherDetailCount - 1);
					Meta.Settings.bVolumetricFog = Settings->GetBool("volumetricFog", Meta.Settings.bVolumetricFog);
					Meta.Settings.bAnimeLook = Settings->GetBool("animeLook", Meta.Settings.bAnimeLook);
					Meta.Settings.bDynamicResolution = Settings->GetBool("dynamicResolution", Meta.Settings.bDynamicResolution);
				}

				Meta.LastActiveUnix = std::max<long long>(0, Source.GetInt64("lastActive"));
				Meta.LastUtcDay = Clamp(Source.GetInt("lastUtcDay"), 0, 10000000);
				Meta.SeasonPassKey = Clamp(Source.GetInt("seasonPassKey"), 0, 1000000);
				if (const FValue* Return = Source.Find("return"))
				{
					FReturnState& Ret = Meta.Return;
					Ret.StartUnix = std::max<long long>(0, Return->GetInt64("start"));
					Ret.DaysClaimed = Clamp(Return->GetInt("days"), 0, (1 << ReturnEventDays) - 1);
					Ret.StageClears = Clamp(Return->GetInt("stages"), 0, MaxCurrency);
					Ret.Summons = Clamp(Return->GetInt("summons"), 0, MaxCurrency);
					Ret.LevelUps = Clamp(Return->GetInt("levelUps"), 0, MaxCurrency);
					Ret.DailyQuests = Clamp(Return->GetInt("dailies"), 0, MaxCurrency);
					Ret.ArkFights = Clamp(Return->GetInt("ark"), 0, MaxCurrency);
					ReadBits(Return->GetString("claimed"), Ret.QuestClaimed, ReturnQuestCount);
				}
				if (const FValue* Gang = Source.Find("gang"))
				{
					Meta.Gang.ShopSeason = Clamp(Gang->GetInt("shopSeason"), 0, 1000000);
					IntsFromJson(Gang->Find("shopBought"), Meta.Gang.ShopBought, GangShopItems, 0, 1000);
					Meta.Gang.PendingActivity = Clamp(Gang->GetInt("pendingActivity"), 0, 200);
				}
				StringsFromJson(Source.Find("readNotices"), Meta.ReadNotices, 60);
				StringsFromJson(Source.Find("importedMail"), Meta.ImportedMail, 200);
				IntsFromJson(Source.Find("items"), Meta.Items, ItemCount, 0, 99999);
				Meta.ChargeUnix = std::max<long long>(0, Source.GetInt64("chargeUnix"));
				Meta.ChargeRefillsToday = Clamp(Source.GetInt("chargeRefills"), 0, 100);
				Meta.MonthlyDays = Clamp(Source.GetInt("monthlyDays"), 0, 1000);
				if (const FValue* Events = Source.Find("events"))
				{
					FEventState& Ev = Meta.Events;
					Ev.WeekKey = Clamp(Events->GetInt("week", -1), -1, 10000000);
					IntsFromJson(Events->Find("counters"), Ev.Counters, EventStatCount, 0, 1000000);
					ReadBits(Events->GetString("claimed"), Ev.MissionClaimed, EventMissionCount);
					IntsFromJson(Events->Find("shop"), Ev.ShopBought, EventShopCount, 0, 1000);
					Ev.RookieStartDay = Clamp(Events->GetInt("rookieStart"), 0, 10000000);
					IntsFromJson(Events->Find("rookieCounters"), Ev.RookieCounters, EventStatCount, 0, 1000000);
					ReadBits(Events->GetString("rookieClaimed"), Ev.RookieClaimed, RookieMissionCount);
					Ev.ArkShopKey = Clamp(Events->GetInt("arkShopKey"), 0, 10000000);
					IntsFromJson(Events->Find("arkShop"), Ev.ArkShopBought, ArkShopCount, 0, 1000);
					if (const FValue* Road = Events->Find("road"))
					{
						Ev.RoadLastDay = Clamp(Road->GetInt("lastDay"), 0, 10000000);
						Ev.RoadWeekKey = Clamp(Road->GetInt("weekKey", -1), -1, 10000000);
						Ev.RoadWeekDays = Clamp(Road->GetInt("weekDays"), 0, 7);
						Ev.bRoadWeekClaimed = Road->GetInt("weekClaimed") != 0;
						Ev.RoadDays = Clamp(Road->GetInt("days"), 0, 10000000);
						Ev.RoadMilestones = Clamp(Road->GetInt("milestones"), 0, 10000000);
						Ev.RoadTickets = Clamp(Road->GetInt("tickets"), 0, 2000000000);
					}
				}
			}
		}

		const char* TutorialStepName(int Step)
		{
			return Step >= 0 && Step < static_cast<int>(ETutorialStep::Count) ? TutorialNames[Step] : TutorialNames[static_cast<int>(ETutorialStep::Complete)];
		}

		int TutorialStepFromName(const std::string& Name, int Fallback)
		{
			for (int Step = 0; Step < static_cast<int>(ETutorialStep::Count); ++Step)
			{
				if (Name == TutorialNames[Step])
				{
					return Step;
				}
			}
			return Fallback;
		}

		// ===================================================================== write
		std::string Serialize(const FProfile& Profile, int64_t SavedAtUnix)
		{
			FValue Root = FValue::MakeObject();
			Root.Set("format", Str(FormatName));
			Root.Set("version", Num(FormatVersion));
			Root.Set("savedAt", Num(static_cast<double>(SavedAtUnix)));
			Root.Set("userId", Str(Profile.Account.UserId));

			FValue Data = FValue::MakeObject();

			FValue Currencies = FValue::MakeObject();
			for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
			{
				Currencies.Set(CurrencyKeys[Index], Num(Profile.Raw(static_cast<ECurrency>(Index))));
			}
			Data.Set("currencies", Currencies);

			FValue Operators = FValue::MakeObject();
			for (int Index = 0; Index < static_cast<int>(Profile.Operators.size()); ++Index)
			{
				const FOwnedOperator& Owned = Profile.Operators[static_cast<size_t>(Index)];
				if (!Owned.bOwned)
				{
					continue;
				}
				FValue Item = FValue::MakeObject();
				Item.Set("level", Num(Owned.Level));
				Item.Set("stars", Num(Owned.UpgradeStars));
				Item.Set("dup", Num(Owned.DupResource));
				Item.Set("gates", Num(Owned.HeartGatesPaid));
				Item.Set("attune", Num(Owned.Attunement));
				Operators.Set(OperatorKey(Index), Item);
			}
			Data.Set("operators", Operators);

			FValue Inventory = FValue::MakeArray();
			for (const FEquipment& Item : Profile.Inventory)
			{
				FValue Entry = FValue::MakeObject();
				Entry.Set("id", Num(Item.Id));
				Entry.Set("slot", Num(static_cast<int>(Item.Slot)));
				Entry.Set("bonus", Num(static_cast<int>(Item.Bonus)));
				Entry.Set("percent", Num(Item.Percent));
				Entry.Set("upgrade", Num(Item.UpgradeLevel));
				Entry.Set("equippedBy", Str(OperatorKey(Item.EquippedBy)));
				Entry.Set("name", Str(Item.Name));
				Inventory.Push(Entry);
			}
			Data.Set("inventory", Inventory);
			Data.Set("nextEquipmentId", Num(Profile.NextEquipmentIdValue()));

			FValue Team = FValue::MakeArray();
			for (int Slot = 0; Slot < 3; ++Slot)
			{
				Team.Push(Str(OperatorKey(Profile.Team[Slot])));
			}
			Data.Set("team", Team);

			FValue Banners = FValue::MakeObject();
			for (size_t Index = 0; Index < Profile.Banners.size() && Index < Profile.Pity.size(); ++Index)
			{
				FValue Item = FValue::MakeObject();
				Item.Set("since5", Num(Profile.Pity[Index].Since5));
				Item.Set("since6", Num(Profile.Pity[Index].Since6));
				Item.Set("lostFiftyFifty", Flag(Profile.Pity[Index].bLostFiftyFifty));
				Item.Set("featured", Str(OperatorKey(Profile.Banners[Index].FeaturedOperator)));
				Banners.Set(Profile.Banners[Index].Id, Item);
			}
			Data.Set("banners", Banners);
			Data.Set("rotationIndex", Num(static_cast<double>(Profile.RotationIndex)));

			FValue Tutorial = FValue::MakeObject();
			Tutorial.Set("step", Str(TutorialStepName(Profile.TutorialStep)));
			Tutorial.Set("summonDone", Flag(Profile.bTutorialSummonDone));
			Data.Set("tutorial", Tutorial);

			Data.Set("day", Num(Profile.Day));
			Data.Set("stagesCleared", Num(Profile.StagesCleared));
			Data.Set("meta", MetaToJson(Profile.Meta));
			Root.Set("profile", Data);
			return Json::Write(Root);
		}

		// ====================================================================== read
		FSaveInfo Peek(const std::string& Text)
		{
			FSaveInfo Info;
			FValue Root;
			std::string Error;
			if (!Json::Parse(Text, Root, Error) || !Root.IsObject() || Root.GetString("format") != FormatName)
			{
				return Info;
			}
			Info.bValid = Root.Find("profile") != nullptr;
			Info.Version = Root.GetInt("version");
			Info.SavedAtUnix = Root.GetInt64("savedAt");
			Info.UserId = Root.GetString("userId");
			return Info;
		}

		bool Deserialize(const std::string& Text, FProfile& Profile, std::string& Error)
		{
			FValue Root;
			if (!Json::Parse(Text, Root, Error))
			{
				Error = "the save is not valid JSON: " + Error;
				return false;
			}
			if (!Root.IsObject() || Root.GetString("format") != FormatName)
			{
				Error = "not a Boldinbeauty save file";
				return false;
			}
			if (Root.GetInt("version") > FormatVersion)
			{
				Error = "this save was written by a newer version of the game";
				return false;
			}
			const FValue* Data = Root.Find("profile");
			if (!Data || !Data->IsObject())
			{
				Error = "the save has no profile";
				return false;
			}

			FProfile Loaded = Profile; // keeps the account, the clock and the random generator
			const std::vector<FRosterEntry>& Entries = Roster();

			if (const FValue* Currencies = Data->Find("currencies"))
			{
				for (int Index = 0; Index < static_cast<int>(ECurrency::Count); ++Index)
				{
					Loaded.SetRaw(static_cast<ECurrency>(Index), Clamp(Currencies->GetInt(CurrencyKeys[Index]), 0, MaxCurrency));
				}
			}

			Loaded.Operators.assign(Entries.size(), FOwnedOperator());
			if (const FValue* Operators = Data->Find("operators"))
			{
				for (size_t Key = 0; Key < Operators->Keys().size(); ++Key)
				{
					const int Index = FindOperator(Operators->Keys()[Key].c_str());
					if (Index < 0)
					{
						continue; // an operator this version does not have
					}
					const FValue& Item = Operators->Items()[Key];
					FOwnedOperator& Owned = Loaded.Operators[static_cast<size_t>(Index)];
					Owned.bOwned = true;
					Owned.UpgradeStars = Clamp(Item.GetInt("stars"), 0, MaxCurrency / Config::CapPerUpgradeStar - Config::BaseLevelCap);
					Owned.Level = Clamp(Item.GetInt("level", 1), 1, Config::BaseLevelCap + Config::CapPerUpgradeStar * Owned.UpgradeStars);
					Owned.DupResource = Clamp(Item.GetInt("dup"), 0, MaxCurrency);
					Owned.HeartGatesPaid = Clamp(Item.GetInt("gates"), 0, Owned.Level / Config::HeartGateEvery);
					Owned.Attunement = Clamp(Item.GetInt("attune"), 0, 10);
				}
			}

			Loaded.Inventory.clear();
			int HighestId = 0;
			if (const FValue* Inventory = Data->Find("inventory"))
			{
				for (size_t Index = 0; Index < Inventory->Size() && Index < 2000; ++Index)
				{
					const FValue& Entry = Inventory->At(Index);
					FEquipment Item;
					Item.Id = Clamp(Entry.GetInt("id"), 1, MaxCurrency);
					const bool bDuplicateId = std::any_of(Loaded.Inventory.begin(), Loaded.Inventory.end(), [&Item](const FEquipment& Other) { return Other.Id == Item.Id; });
					if (bDuplicateId)
					{
						continue;
					}
					Item.Slot = static_cast<ESlot>(Clamp(Entry.GetInt("slot"), 0, static_cast<int>(ESlot::Count) - 1));
					Item.Bonus = Item.Slot == ESlot::Weapon ? EBonus::Damage : static_cast<EBonus>(Clamp(Entry.GetInt("bonus"), 0, 1));
					Item.Percent = std::min(100.0, std::max(0.0, Entry.GetNumber("percent")));
					Item.UpgradeLevel = Clamp(Entry.GetInt("upgrade"), 0, Config::EquipMaxUpgrade);
					Item.Name = Limit(Entry.GetString("name", std::string(SlotName(Item.Slot)) + " #" + std::to_string(Item.Id)), MaxText);
					const std::string Wearer = Entry.GetString("equippedBy");
					const int WearerIndex = Wearer.empty() ? -1 : FindOperator(Wearer.c_str());
					if (WearerIndex >= 0 && Loaded.Operators[static_cast<size_t>(WearerIndex)].bOwned)
					{
						const bool bSlotTaken = std::any_of(Loaded.Inventory.begin(), Loaded.Inventory.end(),
							[&](const FEquipment& Other) { return Other.EquippedBy == WearerIndex && Other.Slot == Item.Slot; });
						Item.EquippedBy = bSlotTaken ? -1 : WearerIndex;
					}
					HighestId = std::max(HighestId, Item.Id);
					Loaded.Inventory.push_back(Item);
				}
			}
			Loaded.SetNextEquipmentId(std::max(Clamp(Data->GetInt("nextEquipmentId", 1), 1, MaxCurrency), HighestId + 1));

			for (int& Slot : Loaded.Team)
			{
				Slot = -1;
			}
			if (const FValue* Team = Data->Find("team"))
			{
				for (size_t Slot = 0; Slot < 3 && Slot < Team->Size(); ++Slot)
				{
					const std::string Name = Team->At(Slot).AsString();
					const int Index = Name.empty() ? -1 : FindOperator(Name.c_str());
					const bool bTaken = Index >= 0 && (Loaded.Team[0] == Index || Loaded.Team[1] == Index || Loaded.Team[2] == Index);
					if (Index >= 0 && !bTaken && Loaded.Owns(Index) && Entries[static_cast<size_t>(Index)].Kit != EKit::None)
					{
						Loaded.Team[Slot] = Index;
					}
				}
			}

			Loaded.RotationIndex = Data->GetInt64("rotationIndex", Loaded.RotationIndex);
			Loaded.Banners.clear();
			Loaded.Pity.clear();
			Loaded.ResetBannersToDefault();
			if (const FValue* Banners = Data->Find("banners"))
			{
				for (size_t Index = 0; Index < Loaded.Banners.size(); ++Index)
				{
					const FValue* Item = Banners->Find(Loaded.Banners[Index].Id);
					if (!Item)
					{
						continue;
					}
					// Pity belongs to a banner with a particular featured operator: if the rotation pools changed since the
					// save was written, the old counters no longer apply.
					const std::string SavedFeatured = Item->GetString("featured");
					if (!SavedFeatured.empty() && SavedFeatured != OperatorKey(Loaded.Banners[Index].FeaturedOperator))
					{
						continue;
					}
					Loaded.Pity[Index].Since5 = Clamp(Item->GetInt("since5"), 0, MaxCurrency);
					Loaded.Pity[Index].Since6 = Clamp(Item->GetInt("since6"), 0, MaxCurrency);
					Loaded.Pity[Index].bLostFiftyFifty = Item->GetBool("lostFiftyFifty");
				}
			}

			if (const FValue* Tutorial = Data->Find("tutorial"))
			{
				Loaded.TutorialStep = TutorialStepFromName(Tutorial->GetString("step"), static_cast<int>(ETutorialStep::Complete));
				Loaded.bTutorialSummonDone = Tutorial->GetBool("summonDone");
			}
			else
			{
				Loaded.TutorialStep = static_cast<int>(ETutorialStep::Complete); // a save without a tutorial block predates it
			}

			if (!Loaded.Owns(FindOperator("Luna")))
			{
				Loaded.GrantOperator(FindOperator("Luna")); // MAIN §5: Luna is always part of the collection
			}
			Loaded.Day = Clamp(Data->GetInt("day", 1), 1, 100000);
			Loaded.StagesCleared = Clamp(Data->GetInt("stagesCleared"), 0, TotalCampaignLevels);
			Loaded.Meta = MakeNewMeta();
			Loaded.Meta.Mail.clear();
			if (const FValue* Meta = Data->Find("meta"))
			{
				MetaFromJson(*Meta, Loaded.Meta);
			}

			Profile = Loaded;
			Error.clear();
			return true;
		}
	}
}
