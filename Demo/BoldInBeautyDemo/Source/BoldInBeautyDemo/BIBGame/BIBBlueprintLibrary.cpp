#include "BIBGame/BIBBlueprintLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "BIBGame/BIBArcadeOverlay.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBMotion.h"
#include "BIBGame/BIBVideoScreen.h"
#include "EngineUtils.h"

#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBRules/BIBConfig.h"
#include "BIBRules/BIBCore.h"
#include "BIBRules/BIBMeta.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

namespace BIBLibraryInternal
{
	UWorld* WorldOf(const UObject* Context)
	{
		return (GEngine && Context) ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	ABIBGameMode* ModeOf(const UObject* Context)
	{
		UWorld* World = WorldOf(Context);
		return World ? World->GetAuthGameMode<ABIBGameMode>() : nullptr;
	}

	ABIBPlayerController* ControllerOf(const UObject* Context)
	{
		UWorld* World = WorldOf(Context);
		return World ? Cast<ABIBPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	BIB::FProfile* ProfileOf(const UObject* Context)
	{
		ABIBGameMode* GameMode = ModeOf(Context);
		return GameMode ? &GameMode->GetProfile() : nullptr;
	}

	FString ToF(const std::string& Value)
	{
		return FString(UTF8_TO_TCHAR(Value.c_str()));
	}

	FBIBResult Finish(const UObject* Context, const BIB::FActionResult& Action)
	{
		FBIBResult Result;
		Result.bOk = Action.bOk;
		Result.Message = ToF(Action.Message);
		if (ABIBPlayerController* PC = ControllerOf(Context))
		{
			PC->SetStatus(Result.Message);
			PC->MarkMenuDirty();
		}
		return Result;
	}

	FBIBResult Fail(const FString& Message)
	{
		FBIBResult Result;
		Result.Message = Message;
		return Result;
	}

	FString OperatorLabel(int32 RosterIndex)
	{
		if (RosterIndex < 0 || RosterIndex >= static_cast<int32>(BIB::Roster().size()))
		{
			return FString(TEXT("-"));
		}
		const BIB::FRosterEntry& Entry = BIB::Roster()[static_cast<size_t>(RosterIndex)];
		return FString::Printf(TEXT("%s (%s)"), *ToF(Entry.Name), *ToF(BIB::StarsLabel(Entry.Stars)));
	}

	TArray<FBIBSummonInfo> ConvertSummons(const UObject* Context, const BIB::FSummonOutcome& Outcome, FString& OutMessage)
	{
		TArray<FBIBSummonInfo> Results;
		ABIBPlayerController* PC = ControllerOf(Context);
		if (PC)
		{
			PC->SummonLines.Reset();
		}
		for (const BIB::FSummonResult& One : Outcome.Results)
		{
			FBIBSummonInfo Info;
			Info.Name = ToF(BIB::Roster()[static_cast<size_t>(One.Operator)].Name);
			Info.Stars = One.Stars;
			Info.bNew = One.bNew;
			Info.bFeatured = One.bFeatured;
			Info.bPity = One.bPity;
			Info.bTenPullGuarantee = One.bTenPullGuarantee;
			Info.bFiftyFiftyGuarantee = One.bFiftyFiftyGuarantee;
			Info.Label = OperatorLabel(One.Operator) + (One.bNew ? TEXT(" NEW!") : TEXT(" duplicate (+20 upgrade units)"))
				+ (One.bFeatured ? TEXT(" [FEATURED]") : TEXT("")) + (One.bPity ? TEXT(" [pity]") : TEXT(""))
				+ (One.bTenPullGuarantee ? TEXT(" [10x guarantee]") : TEXT("")) + (One.bFiftyFiftyGuarantee ? TEXT(" [50/50 guarantee]") : TEXT(""));
			if (PC)
			{
				PC->SummonLines.Add(Info.Label);
			}
			Results.Add(Info);
		}
		OutMessage = ToF(Outcome.Message);
		if (PC)
		{
			PC->SetStatus(OutMessage);
			PC->MarkMenuDirty();
		}
		return Results;
	}
}


// ============================================================================ profile

int32 UBIBBlueprintLibrary::GetCurrency(const UObject* WorldContextObject, EBIBCurrency Currency)
{
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? Profile->Get(static_cast<BIB::ECurrency>(static_cast<int32>(Currency))) : 0;
}

FString UBIBBlueprintLibrary::GetCurrencySummary(const UObject* WorldContextObject)
{
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile)
	{
		return FString();
	}
	return FString::Printf(TEXT("Gold %d   Diamonds %d   Bytes %d   Angel hearts %d   |   Regular %d   Special %d   Daily %d   Mileage %d   |   Day %d"),
		Profile->Get(BIB::ECurrency::Gold), Profile->Get(BIB::ECurrency::Diamonds), Profile->Get(BIB::ECurrency::Bytes),
		Profile->Get(BIB::ECurrency::AngelHearts), Profile->Get(BIB::ECurrency::RegularTicket), Profile->Get(BIB::ECurrency::SpecialTicket),
		Profile->Get(BIB::ECurrency::DailyTicket), Profile->Get(BIB::ECurrency::Mileage), Profile->Day);
}

int32 UBIBBlueprintLibrary::GetDay(const UObject* WorldContextObject)
{
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? Profile->Day : 0;
}

void UBIBBlueprintLibrary::GrantDemoCurrency(const UObject* WorldContextObject)
{
	if (BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject))
	{
		Profile->GrantDemoCurrency();
		BIB::FActionResult Action;
		Action.bOk = true;
		Action.Message = "Demo currency granted.";
		BIBLibraryInternal::Finish(WorldContextObject, Action);
	}
}

int32 UBIBBlueprintLibrary::GetMetaValue(const UObject* WorldContextObject, const FString& Key)
{
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile)
	{
		return -1;
	}
	const BIB::FMeta& M = Profile->Meta;
	if (Key == TEXT("cleared")) return Profile->StagesCleared;
	if (Key == TEXT("trophies")) return M.Arena.Trophies;
	if (Key == TEXT("arenaFights")) return M.ArenaWins + M.ArenaLosses;
	if (Key == TEXT("towerLost0")) return M.TowerLostAt[0];
	if (Key == TEXT("towerLost1")) return M.TowerLostAt[1];
	if (Key == TEXT("towerBest0")) return M.TowerBest[0];
	if (Key == TEXT("towerBest1")) return M.TowerBest[1];
	if (Key == TEXT("bossBest")) return M.ArcadeRecords.BossRushBest;
	if (Key.StartsWith(TEXT("nightWins"))) return M.ArcadeRecords.NightWins[FMath::Clamp(FCString::Atoi(*Key.RightChop(9)), 0, 2)];
	if (Key == TEXT("rhythmBest"))
	{
		int32 Best = 0;
		for (int32 Score : M.ArcadeRecords.RhythmBest)
		{
			Best = FMath::Max(Best, Score);
		}
		return Best;
	}
	if (Key.StartsWith(TEXT("miniBest")) || Key.StartsWith(TEXT("miniWins")))
	{
		// "miniBest0" .. "miniBest3" (Halo Fall, Neon Memory, Firewall Breach, Ark Rally): best over the three difficulties; "miniWins<g>" adds the wins.
		const int32 Game = FMath::Clamp(FCString::Atoi(*Key.RightChop(8)), 0, BIB::MiniGameRecordCount - 1);
		int32 Value = 0;
		for (int32 D = 0; D < BIB::ArcadeDifficultyCount; ++D)
		{
			const int32 Slot = Game * BIB::ArcadeDifficultyCount + D;
			Value = Key.StartsWith(TEXT("miniBest")) ? FMath::Max(Value, M.ArcadeRecords.MiniBest[Slot]) : Value + M.ArcadeRecords.MiniWins[Slot];
		}
		return Value;
	}
	if (Key == TEXT("gangCoins")) return Profile->Get(BIB::ECurrency::GangCoin);
	if (Key == TEXT("arcadeTokens")) return Profile->Get(BIB::ECurrency::ArcadeToken);
	// Ultimate Custom Night (2026-10-14): the unlock, the one-time reward and the wins; diamondsRaw is the stored amount (not the dev "unlimited" view).
	if (Key == TEXT("ucnUnlocked")) return M.ArcadeRecords.bUcnUnlocked ? 1 : 0;
	if (Key == TEXT("ucnRewardClaimed")) return M.ArcadeRecords.bUcnRewardClaimed ? 1 : 0;
	if (Key.StartsWith(TEXT("ucnWins"))) return M.ArcadeRecords.UcnWins[FMath::Clamp(FCString::Atoi(*Key.RightChop(7)), 0, BIB::UcnModeCount - 1)];
	if (Key == TEXT("diamondsRaw")) return Profile->Raw(BIB::ECurrency::Diamonds);
	if (Key == TEXT("seasonXp")) return M.SeasonXp;
	if (Key == TEXT("returnActive")) return M.Return.StartUnix > 0 ? 1 : 0;
	if (Key == TEXT("mail")) return static_cast<int32>(M.Mail.size());
	// 2026-10-07: inventory, Charge, elements and events
	if (Key == TEXT("charge")) return Profile->Get(BIB::ECurrency::Charge);
	if (Key == TEXT("elementCores")) return Profile->Get(BIB::ECurrency::ElementCore);
	if (Key == TEXT("eventTokens")) return Profile->Get(BIB::ECurrency::EventToken);
	if (Key == TEXT("arkMedals")) return Profile->Get(BIB::ECurrency::ArkMedal);
	if (Key == TEXT("gold")) return Profile->Get(BIB::ECurrency::Gold);
	if (Key == TEXT("items"))
	{
		int32 Items = 0;
		for (int32 Count : M.Items)
		{
			Items += Count;
		}
		return Items;
	}
	if (Key.StartsWith(TEXT("attune")))   // "attune<roster index>"
	{
		const int32 Op = FCString::Atoi(*Key.RightChop(6));
		return Op >= 0 && Op < static_cast<int32>(Profile->Operators.size()) ? Profile->Operators[static_cast<size_t>(Op)].Attunement : -1;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const ABIBGameMode* Mode = World ? World->GetAuthGameMode<ABIBGameMode>() : nullptr;
	if (Key == TEXT("popups")) return Mode ? Mode->GetDamagePopups().Num() : -1;
	if (Key == TEXT("bossRound")) return Mode ? Mode->GetBossRushRound() : -1;
	if (Key == TEXT("lastPopup")) return Mode && Mode->GetDamagePopups().Num() > 0 ? FMath::RoundToInt(Mode->GetDamagePopups().Last().Amount) : -1;
	// 2026-10-09: the bb-Showreel motion pack (Tools/verify_motion.py)
	if (Key == TEXT("motion:pieces")) return BIBMotion::PieceCount();
	if (Key == TEXT("motion:played")) return BIBMotion::PlayedPieces().Num();
	if (Key.StartsWith(TEXT("motion:played:"))) return BIBMotion::PlayedPieces().Contains(Key.RightChop(14)) ? 1 : 0;
	if (Key == TEXT("screens") || Key == TEXT("screens:playing") || Key == TEXT("screens:ads"))
	{
		int32 Count = 0;
		for (TActorIterator<ABIBVideoScreen> It(World); It; ++It)
		{
			Count += Key == TEXT("screens") || (Key == TEXT("screens:ads") ? It->GetMovie().StartsWith(TEXT("BB_Ad")) : It->IsPlaying()) ? 1 : 0;
		}
		return Count;
	}
	// 2026-10-13 (goal 2): the angels' tactical brain (BIBRules/BIBAngelBrain.h FAiStats, Tools/verify_angels.py). angel:b:<Role>:<Behavior>
	// and angel:m:<Role>:<Move> count decisions (roles Angel, Heavy, Ranged, Mage, Assassin, MiniBoss, Boss; behaviours Spacing, Punish,
	// Defense, Feint, Adapt, Coordination, SelfPreservation, Kit; moves as BIB::Angel::MoveName), angel:evasions, angel:punishes,
	// angel:threatMs (ms until the next angel strike can land, -1 none - it is on screen as a telegraph), angel:seeing, angel:tokens.
	// Console BIBAngelResetStats clears the counters.
	if (Key.StartsWith(TEXT("angel:")))
	{
		static const TCHAR* Roles[7] = {TEXT("Angel"), TEXT("Heavy"), TEXT("Ranged"), TEXT("Mage"), TEXT("Assassin"), TEXT("MiniBoss"), TEXT("Boss")};
		const BIB::Angel::FAiStats& Stats = BIB::Angel::AiStats();
		const FString AngelKey = Key.RightChop(6);
		for (int32 Role = 0; Role < 7; ++Role)
		{
			const FString Prefix = FString::Printf(TEXT("%s:"), Roles[Role]);
			for (int32 Behavior = 0; Behavior < BIB::Angel::BehaviorCount; ++Behavior)
			{
				if (AngelKey == TEXT("b:") + Prefix + UTF8_TO_TCHAR(BIB::Angel::BehaviorName(static_cast<BIB::Angel::EBehavior>(Behavior))))
				{
					return Stats.Behaviors[Role][Behavior];
				}
			}
			for (int32 Move = 0; Move < static_cast<int32>(BIB::Angel::EMove::Count); ++Move)
			{
				if (AngelKey == TEXT("m:") + Prefix + UTF8_TO_TCHAR(BIB::Angel::MoveName(static_cast<BIB::Angel::EMove>(Move))))
				{
					return Stats.Moves[Role][Move];
				}
			}
		}
		if (AngelKey == TEXT("evasions")) return Stats.Evasions;
		if (AngelKey == TEXT("punishes")) return Stats.Punishes;
		const TArray<ABIBEnemy*> Live = Mode ? Mode->GetLiveEnemies() : TArray<ABIBEnemy*>();
		if (AngelKey == TEXT("threatMs"))
		{
			float Best = -1.f;
			for (const ABIBEnemy* Enemy : Live)
			{
				const float Seconds = Enemy->GetSecondsToStrike();
				if (Seconds >= 0.f && (Best < 0.f || Seconds < Best))
				{
					Best = Seconds;
				}
			}
			return Best < 0.f ? -1 : FMath::RoundToInt(Best * 1000.f);
		}
		if (AngelKey == TEXT("seeing") || AngelKey == TEXT("tokens"))
		{
			int32 Count = 0;
			for (const ABIBEnemy* Enemy : Live)
			{
				Count += (AngelKey == TEXT("seeing") ? Enemy->SeesCommander() : Enemy->IsCommittedForCrowd()) ? 1 : 0;
			}
			return Count;
		}
		return -1;
	}
	// 2026-10-09: Zenless-style combat feel, the angels' mind and traversal (Tools/verify_combat.py)
	if (Key.StartsWith(TEXT("feel:")))
	{
		// Hit feel counters (BIBRules/BIBHitFeel.h FStats, 2026-10-13): feel:w:<Weight>, feel:r:<Reaction>, feel:kicks, feel:pushIns,
		// feel:rumbles, feel:cinematics, feel:skips, feel:cancels, feel:maxHitstop (frames), feel:minLean / feel:maxLean (degrees x10).
		// Console BIBFeelResetStats clears them.
		{
			const BIB::Feel::FStats& Stats = BIB::Feel::Stats();
			const FString FeelKey = Key.RightChop(5);
			for (int32 Index = 0; Index < BIB::Feel::WeightCount; ++Index)
			{
				if (FeelKey == FString(TEXT("w:")) + UTF8_TO_TCHAR(BIB::Feel::WeightName(static_cast<BIB::Feel::EWeight>(Index))))
				{
					return Stats.Weights[Index];
				}
			}
			for (int32 Index = 0; Index < BIB::Feel::ReactionCount; ++Index)
			{
				if (FeelKey == FString(TEXT("r:")) + UTF8_TO_TCHAR(BIB::Feel::ReactionName(static_cast<BIB::Feel::EReaction>(Index))))
				{
					return Stats.Reactions[Index];
				}
			}
			if (FeelKey == TEXT("kicks")) return Stats.Kicks;
			if (FeelKey == TEXT("pushIns")) return Stats.PushIns;
			if (FeelKey == TEXT("rumbles")) return Stats.Rumbles;
			if (FeelKey == TEXT("cinematics")) return Stats.Cinematics;
			if (FeelKey == TEXT("skips")) return Stats.CinematicSkips;
			if (FeelKey == TEXT("cancels")) return Stats.RecoveryCancels;
			if (FeelKey == TEXT("maxHitstop")) return FMath::RoundToInt(Stats.MaxHitstopFrames);
			if (FeelKey == TEXT("minLean")) return FMath::RoundToInt(Stats.MinLean * 10.0);
			if (FeelKey == TEXT("maxLean")) return FMath::RoundToInt(Stats.MaxLean * 10.0);
		}
		const APlayerController* PCx = World ? World->GetFirstPlayerController() : nullptr;
		const ABIBPlayerCharacter* Char = PCx ? Cast<ABIBPlayerCharacter>(PCx->GetPawn()) : nullptr;
		if (!Char)
		{
			return -1;
		}
		const FBIBHud& Hud = Char->GetHud();
		const FString What = Key.RightChop(5);
		if (What == TEXT("combo")) return Hud.ComboHits;
		if (What == TEXT("chain")) return Hud.bChainReady ? 1 : 0;
		if (What == TEXT("dodgeCounter")) return Hud.bDodgeCounterReady ? 1 : 0;
		if (What == TEXT("banner")) return Hud.Banner.IsEmpty() ? 0 : static_cast<int32>(GetTypeHash(Hud.Banner) & 0x7fffffff);
		if (What == TEXT("adapt")) return FMath::RoundToInt(Hud.AngelAdaptation * 100.f);
		if (What == TEXT("climbing")) return Hud.MoveState == TEXT("CLIMBING") ? 1 : 0;
		if (What == TEXT("swimming")) return Hud.MoveState == TEXT("SWIMMING") ? 1 : 0;
		if (What == TEXT("jumpZ")) return FMath::RoundToInt(Char->GetCharacterMovement()->JumpZVelocity);
		if (What == TEXT("gravity")) return FMath::RoundToInt(Char->GetCharacterMovement()->GravityScale * 100.f);
		if (UBIBGameInstance* Game = World->GetGameInstance<UBIBGameInstance>())
		{
			if (What == TEXT("known")) return Game->GetAngelMind().KnownCount();
			if (What == TEXT("guard")) return FMath::RoundToInt(Game->GetAngelMind().Expertise(BIB::Angel::ECounter::Guard) * 100.0);
		}
		int32 MaxDaze = 0;
		int32 Stunned = 0;
		int32 Guarding = 0;
		for (const ABIBEnemy* Enemy : Mode ? Mode->GetLiveEnemies() : TArray<ABIBEnemy*>())
		{
			MaxDaze = FMath::Max(MaxDaze, FMath::RoundToInt(Enemy->GetDazeFraction() * 100.f));
			Stunned += Enemy->IsDazeStunned() ? 1 : 0;
			Guarding += Enemy->IsGuarding() ? 1 : 0;
		}
		if (What == TEXT("maxDaze")) return MaxDaze;
		if (What == TEXT("stunned")) return Stunned;
		if (What == TEXT("guarding")) return Guarding;
		return -1;
	}
	if (Key == TEXT("enemyElements"))   // living enemies that carry an element (BIBRules/BIBElement.h)
	{
		int32 Count = 0;
		for (const ABIBEnemy* Enemy : Mode ? Mode->GetLiveEnemies() : TArray<ABIBEnemy*>())
		{
			Count += Enemy && Enemy->GetElement() >= 0 ? 1 : 0;
		}
		return Mode ? Count : -1;
	}
	return -1;
}

bool UBIBBlueprintLibrary::IsArcadeOverlayOpen()
{
	return BIBOverlay::IsOpen();
}

FString UBIBBlueprintLibrary::GetArcadeOverlayState()
{
	return BIBOverlay::DebugState();
}

bool UBIBBlueprintLibrary::ArcadeOverlayCommand(const FString& Command)
{
	return BIBOverlay::DebugCommand(Command);
}

void UBIBBlueprintLibrary::AdvanceDay(const UObject* WorldContextObject)
{
	if (BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject))
	{
		Profile->AdvanceDay();
		BIB::FActionResult Action;
		Action.bOk = true;
		Action.Message = "Day " + std::to_string(Profile->Day) + ": Daily ticket refreshed.";
		BIBLibraryInternal::Finish(WorldContextObject, Action);
	}
}

void UBIBBlueprintLibrary::ResetProfile(const UObject* WorldContextObject)
{
	if (BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject))
	{
		Profile->ResetForAccount();
		BIB::FActionResult Action;
		Action.bOk = true;
		Action.Message = "Profile reset.";
		BIBLibraryInternal::Finish(WorldContextObject, Action);
	}
}

// ============================================================================ operators

FBIBOperatorInfo UBIBBlueprintLibrary::GetOperatorInfo(const UObject* WorldContextObject, int32 RosterIndex)
{
	FBIBOperatorInfo Info;
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile || RosterIndex < 0 || RosterIndex >= static_cast<int32>(BIB::Roster().size()))
	{
		return Info;
	}
	const BIB::FRosterEntry& Entry = BIB::Roster()[static_cast<size_t>(RosterIndex)];
	Info.RosterIndex = RosterIndex;
	Info.Name = BIBLibraryInternal::ToF(Entry.Name);
	Info.Stars = Entry.Stars;
	Info.RarityLabel = BIBLibraryInternal::ToF(BIB::StarsLabel(Entry.Stars));
	Info.bOwned = Profile->Owns(RosterIndex);
	Info.bPlayable = Entry.Kit != BIB::EKit::None;
	Info.Label = BIBLibraryInternal::OperatorLabel(RosterIndex);
	if (Info.bOwned)
	{
		const BIB::FOwnedOperator& Owned = Profile->Operators[static_cast<size_t>(RosterIndex)];
		const BIB::FCost Cost = BIB::FProfile::LevelUpCost(Owned.Level);
		const BIB::FCombatStats Stats = Profile->GetCombatStats(RosterIndex);
		Info.Level = Owned.Level;
		Info.LevelCap = Profile->LevelCap(RosterIndex);
		Info.UpgradeStars = Owned.UpgradeStars;
		Info.DuplicateUnits = Owned.DupResource;
		Info.NextStarCost = BIB::FProfile::UpgradeStarCost(Owned.UpgradeStars + 1);
		Info.NextLevelGold = Cost.Gold;
		Info.NextLevelBytes = Cost.Bytes;
		Info.bNeedsHeartGate = Profile->NeedsHeartGate(RosterIndex);
		Info.HeartGateCost = Info.bNeedsHeartGate ? BIB::FProfile::HeartGateCost(Owned.Level / BIB::Config::HeartGateEvery) : 0;
		Info.MaxHP = static_cast<float>(Stats.MaxHP);
		Info.DamageMultiplier = static_cast<float>(Stats.DamageMult);
		Info.DefencePercent = static_cast<float>(Stats.DefencePercent);
	}
	return Info;
}

TArray<FBIBOperatorInfo> UBIBBlueprintLibrary::GetRoster(const UObject* WorldContextObject)
{
	TArray<FBIBOperatorInfo> Result;
	for (int32 Index = 0; Index < static_cast<int32>(BIB::Roster().size()); ++Index)
	{
		Result.Add(GetOperatorInfo(WorldContextObject, Index));
	}
	return Result;
}

FBIBResult UBIBBlueprintLibrary::LevelUpOperator(const UObject* WorldContextObject, int32 RosterIndex, bool bSupplementWithDiamonds)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->TryLevelUp(RosterIndex, bSupplementWithDiamonds)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

FBIBResult UBIBBlueprintLibrary::PayHeartGate(const UObject* WorldContextObject, int32 RosterIndex)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->TryPayHeartGate(RosterIndex)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

FBIBResult UBIBBlueprintLibrary::UpgradeStar(const UObject* WorldContextObject, int32 RosterIndex)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->TryUpgradeStar(RosterIndex)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

// ============================================================================ summoning

TArray<FBIBBannerInfo> UBIBBlueprintLibrary::GetBanners(const UObject* WorldContextObject)
{
	TArray<FBIBBannerInfo> Result;
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile)
	{
		return Result;
	}
	for (int32 Index = 0; Index < static_cast<int32>(Profile->Banners.size()); ++Index)
	{
		const BIB::FBanner& Banner = Profile->Banners[static_cast<size_t>(Index)];
		const BIB::FPityState& Pity = Profile->Pity[static_cast<size_t>(Index)];
		FBIBBannerInfo Info;
		Info.Index = Index;
		Info.Title = BIBLibraryInternal::ToF(Banner.Title);
		Info.Kind = static_cast<EBIBBannerKind>(static_cast<int32>(Banner.Type));
		Info.RollsSince5Star = Pity.Since5;
		Info.RollsSince6Star = Pity.Since6;
		Info.bLostFiftyFifty = Pity.bLostFiftyFifty;
		if (Banner.FeaturedOperator >= 0)
		{
			const BIB::FRosterEntry& Featured = BIB::Roster()[static_cast<size_t>(Banner.FeaturedOperator)];
			Info.FeaturedName = BIBLibraryInternal::ToF(Featured.Name);
			Info.FeaturedStars = Featured.Stars;
			Info.MileageRedeemCost = Featured.Stars == 6 ? BIB::Config::MileageFeatured6Cost : BIB::Config::MileageFeatured5Cost;
		}
		switch (Banner.Type)
		{
		case BIB::EBannerType::Standard:
			Info.Description = TEXT("Regular tickets or 300 Diamonds per roll; 10x = 9 rolls (2,700 Diamonds). Equal split within each rarity.");
			break;
		case BIB::EBannerType::Featured:
			Info.Description = FString::Printf(TEXT("Featured %s: 50%% of that rarity; lose the 50/50 and the next one is guaranteed. Special tickets or 300 Diamonds per roll."), *Info.FeaturedName);
			break;
		case BIB::EBannerType::Daily:
			Info.Description = TEXT("One Daily ticket per day (expires). No Diamonds, no 10x, no Mileage, no pity credit.");
			break;
		}
		Result.Add(Info);
	}
	return Result;
}

TArray<FBIBSummonInfo> UBIBBlueprintLibrary::Summon(const UObject* WorldContextObject, int32 BannerIndex, int32 Count, bool bUseDiamonds, FString& OutMessage)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile)
	{
		OutMessage = TEXT("No profile.");
		return TArray<FBIBSummonInfo>();
	}
	return BIBLibraryInternal::ConvertSummons(WorldContextObject, Profile->Summon(BannerIndex, Count, bUseDiamonds ? BIB::EPay::Diamonds : BIB::EPay::Ticket), OutMessage);
}

bool UBIBBlueprintLibrary::IsTutorialSummonAvailable(const UObject* WorldContextObject)
{
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile && !Profile->bTutorialSummonDone;
}

TArray<FBIBSummonInfo> UBIBBlueprintLibrary::TutorialSummon(const UObject* WorldContextObject, FString& OutMessage)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile)
	{
		OutMessage = TEXT("No profile.");
		return TArray<FBIBSummonInfo>();
	}
	return BIBLibraryInternal::ConvertSummons(WorldContextObject, Profile->TutorialSummon(), OutMessage);
}

FBIBResult UBIBBlueprintLibrary::RedeemMileage(const UObject* WorldContextObject, int32 BannerIndex)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile)
	{
		return BIBLibraryInternal::Fail(TEXT("No profile."));
	}
	FString Message;
	const TArray<FBIBSummonInfo> Results = BIBLibraryInternal::ConvertSummons(WorldContextObject, Profile->RedeemMileage(BannerIndex), Message);
	FBIBResult Result;
	Result.bOk = Results.Num() > 0;
	Result.Message = Message;
	return Result;
}

TArray<FString> UBIBBlueprintLibrary::GetLastSummonLines(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	return PC ? PC->SummonLines : TArray<FString>();
}

// ============================================================================ equipment

TArray<FBIBEquipmentInfo> UBIBBlueprintLibrary::GetInventory(const UObject* WorldContextObject)
{
	TArray<FBIBEquipmentInfo> Result;
	const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!Profile)
	{
		return Result;
	}
	for (const BIB::FEquipment& Item : Profile->Inventory)
	{
		FBIBEquipmentInfo Info;
		Info.Id = Item.Id;
		Info.Name = BIBLibraryInternal::ToF(Item.Name);
		Info.SlotIndex = static_cast<int32>(Item.Slot);
		Info.SlotName = BIBLibraryInternal::ToF(BIB::SlotName(Item.Slot));
		Info.bDamageBonus = Item.Bonus == BIB::EBonus::Damage;
		Info.TotalPercent = static_cast<float>(Item.Percent + Item.UpgradeLevel * BIB::Config::EquipUpgradePercent);
		Info.UpgradeLevel = Item.UpgradeLevel;
		Info.UpgradeCostBytes = BIB::FProfile::EquipmentUpgradeCost(Item.UpgradeLevel);
		Info.EquippedBy = Item.EquippedBy;
		Info.EquippedByName = Item.EquippedBy >= 0 ? BIBLibraryInternal::ToF(BIB::Roster()[static_cast<size_t>(Item.EquippedBy)].Name) : FString();
		Info.Label = FString::Printf(TEXT("%s: +%.1f%% %s (upgrade +%d)%s"), *Info.Name, Info.TotalPercent,
			Info.bDamageBonus ? TEXT("damage") : TEXT("defence"), Info.UpgradeLevel,
			Info.EquippedBy >= 0 ? *FString::Printf(TEXT(" [%s]"), *Info.EquippedByName) : TEXT(""));
		Result.Add(Info);
	}
	return Result;
}

FBIBResult UBIBBlueprintLibrary::EquipItem(const UObject* WorldContextObject, int32 ItemId, int32 RosterIndex)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->Equip(ItemId, RosterIndex)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

FBIBResult UBIBBlueprintLibrary::UnequipItem(const UObject* WorldContextObject, int32 ItemId)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->Unequip(ItemId)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

FBIBResult UBIBBlueprintLibrary::UpgradeItem(const UObject* WorldContextObject, int32 ItemId)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->UpgradeEquipment(ItemId)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

// ============================================================================ team & merchant

TArray<int32> UBIBBlueprintLibrary::GetTeam(const UObject* WorldContextObject)
{
	TArray<int32> Result;
	if (const BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject))
	{
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			Result.Add(Profile->Team[Slot]);
		}
	}
	return Result;
}

FBIBResult UBIBBlueprintLibrary::SetTeamSlot(const UObject* WorldContextObject, int32 Slot, int32 RosterIndex)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->SetTeamSlot(Slot, RosterIndex)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

TArray<FString> UBIBBlueprintLibrary::GetMerchantOffers()
{
	TArray<FString> Result;
	for (int32 Offer = 0; Offer < BIB::FProfile::MerchantOfferCount(); ++Offer)
	{
		Result.Add(BIBLibraryInternal::ToF(BIB::FProfile::MerchantOfferLabel(Offer)));
	}
	return Result;
}

FBIBResult UBIBBlueprintLibrary::BuyMerchantOffer(const UObject* WorldContextObject, int32 OfferIndex)
{
	BIB::FProfile* Profile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	return Profile ? BIBLibraryInternal::Finish(WorldContextObject, Profile->BuyFromMerchant(OfferIndex)) : BIBLibraryInternal::Fail(TEXT("No profile."));
}

// ============================================================================ stages

int32 UBIBBlueprintLibrary::GetStageCount()
{
	return ABIBGameMode::GetStageCount();
}

FString UBIBBlueprintLibrary::GetStageDisplayName(int32 Stage)
{
	return ABIBGameMode::GetStageName(Stage);
}

int32 UBIBBlueprintLibrary::GetStageSectionCount(int32 Stage)
{
	return static_cast<int32>(BIB::StagePlan(Stage).size());
}

TArray<int32> UBIBBlueprintLibrary::GetStageSectionCounts(int32 Stage, int32 SectionIndex)
{
	TArray<int32> Result;
	const std::vector<BIB::FSectionPlanData> Plan = BIB::StagePlan(Stage);
	if (SectionIndex >= 0 && SectionIndex < static_cast<int32>(Plan.size()))
	{
		const BIB::FSectionPlanData& Section = Plan[static_cast<size_t>(SectionIndex)];
		Result.Add(Section.Grunts);
		Result.Add(Section.Brutes);
		Result.Add(Section.Ranged);
		Result.Add(Section.Mages);
		Result.Add(Section.Assassins);
		Result.Add(Section.MiniBosses);
		Result.Add(Section.Bosses);
	}
	return Result;
}

FVector2D UBIBBlueprintLibrary::GetStageScales(int32 Stage)
{
	const BIB::FEnemyScale Scale = BIB::StageScales(Stage);
	return FVector2D(Scale.Hp, Scale.Damage);
}

bool UBIBBlueprintLibrary::StartStage(const UObject* WorldContextObject, int32 Stage, FString& OutError)
{
	ABIBGameMode* GameMode = BIBLibraryInternal::ModeOf(WorldContextObject);
	if (!GameMode)
	{
		OutError = TEXT("No BIB game mode in this map.");
		return false;
	}
	const bool bStarted = GameMode->StartStage(Stage, OutError);
	if (ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject))
	{
		PC->SetStatus(bStarted ? FString() : OutError);
		if (bStarted)
		{
			PC->SetScreen(EBIBScreen::None);
		}
		else
		{
			PC->MarkMenuDirty();
		}
	}
	return bStarted;
}

void UBIBBlueprintLibrary::ReturnToHub(const UObject* WorldContextObject)
{
	if (ABIBGameMode* GameMode = BIBLibraryInternal::ModeOf(WorldContextObject))
	{
		GameMode->ReturnToHub();   // the Ark is the hub: travels there when the stage ran in another map
	}
	if (ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject))
	{
		PC->SetStatus(FString());
		PC->SetScreen(EBIBScreen::None);
	}
}

void UBIBBlueprintLibrary::AbandonStage(const UObject* WorldContextObject)
{
	if (ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject))
	{
		PC->SetScreen(EBIBScreen::None); // unpause first
	}
	if (ABIBGameMode* GameMode = BIBLibraryInternal::ModeOf(WorldContextObject))
	{
		GameMode->AbandonStage();
	}
}

TArray<FString> UBIBBlueprintLibrary::GetResultLines(const UObject* WorldContextObject, bool& bSuccess)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	bSuccess = PC && PC->bResultSuccess;
	return PC ? PC->ResultLines : TArray<FString>();
}

// ============================================================================ HUD

FString UBIBBlueprintLibrary::GetOperatorTags(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	const ABIBPlayerCharacter* PlayerChar = PC ? PC->GetBIBCharacter() : nullptr;
	return PlayerChar ? PlayerChar->DescribeActiveTags() : FString();
}

FString UBIBBlueprintLibrary::GetEnemyTags(const AActor* Enemy)
{
	const ABIBEnemy* BIBEnemy = Cast<ABIBEnemy>(Enemy);
	const UAbilitySystemComponent* ASC = BIBEnemy ? BIBEnemy->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return FString();
	}
	FGameplayTagContainer Tags;
	ASC->GetOwnedGameplayTags(Tags);
	return Tags.ToStringSimple();
}

float UBIBBlueprintLibrary::GetEnemyHP(const AActor* Enemy)
{
	const ABIBEnemy* BIBEnemy = Cast<ABIBEnemy>(Enemy);
	return BIBEnemy ? BIBEnemy->GetHP() : 0.f;
}


FBIBHudInfo UBIBBlueprintLibrary::GetHudInfo(const UObject* WorldContextObject)
{
	FBIBHudInfo Info;
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	const ABIBPlayerCharacter* PlayerChar = PC ? PC->GetBIBCharacter() : nullptr;
	const ABIBGameMode* GameMode = BIBLibraryInternal::ModeOf(WorldContextObject);
	if (PlayerChar)
	{
		const FBIBHud& Hud = PlayerChar->GetHud();
		Info.bValid = Hud.bValid;
		Info.ActiveName = Hud.ActiveName;
		Info.HP = Hud.HP;
		Info.MaxHP = Hud.MaxHP;
		Info.HPFraction = Hud.MaxHP > 0.f ? Hud.HP / Hud.MaxHP : 0.f;
		Info.Ult = Hud.Ult;
		Info.UltFraction = Hud.Ult / 100.f;
		Info.Colour = Hud.Colour;
		for (const FBIBAbilityHud& Ability : Hud.Abilities)
		{
			FBIBAbilityInfo Entry;
			Entry.Key = Ability.Key;
			Entry.Name = Ability.Name;
			Entry.CooldownRemaining = Ability.Remaining;
			Entry.Note = Ability.Note;
			Info.Abilities.Add(Entry);
		}
		for (const FBIBTeamHud& Member : Hud.Team)
		{
			FBIBTeamMemberInfo Entry;
			Entry.Name = Member.Name;
			Entry.HPFraction = Member.HPFraction;
			Entry.bAlive = Member.bAlive;
			Entry.bActive = Member.bActive;
			Info.Team.Add(Entry);
		}
		Info.KitLine = Hud.KitLine;
		Info.ActionLine = Hud.ActionLine;
		Info.bJudgeModeMenu = Hud.bJudgeModeMenu;
		Info.bExchangeOpen = Hud.bExchangeOpen;
		for (int32 Stat = 0; Stat < static_cast<int32>(EBIBStat::Count); ++Stat)
		{
			Info.ExchangeValues.Add(Hud.ExchangeValues[Stat]);
		}
		Info.ExchangeFrom = Hud.ExchangeFrom;
		Info.ExchangeTo = Hud.ExchangeTo;
	}
	if (GameMode)
	{
		Info.StageTitle = GameMode->GetStageTitle();
		Info.Objective = GameMode->GetObjective();
		Info.Messages = GameMode->GetMessages();
		Info.bDialogueOpen = GameMode->IsDialogueOpen();
		Info.DialogueSpeaker = GameMode->GetDialogueSpeaker();
		Info.DialogueLine = GameMode->GetDialogueLine();
	}
	const BIB::FProfile* HintProfile = BIBLibraryInternal::ProfileOf(WorldContextObject);
	if (!HintProfile || HintProfile->Meta.Settings.bShowControlsHint)
	{
		Info.ControlsHint = TEXT("WASD move | Shift run | Space jump | LMB attack | RMB block | F parry | Shift+F dodge | 1-3 moves | 4 special | E/R ult | Tab switch | T talk | P pause");
	}
	return Info;
}

void UBIBBlueprintLibrary::ExchangeSelect(const UObject* WorldContextObject, bool bFrom, int32 StatIndex)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	if (ABIBPlayerCharacter* PlayerChar = PC ? PC->GetBIBCharacter() : nullptr)
	{
		PlayerChar->ExchangeSelect(bFrom, StatIndex);
	}
}

void UBIBBlueprintLibrary::ExchangeApply(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	if (ABIBPlayerCharacter* PlayerChar = PC ? PC->GetBIBCharacter() : nullptr)
	{
		PlayerChar->ExchangeApply();
	}
}

void UBIBBlueprintLibrary::CloseExchange(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	if (ABIBPlayerCharacter* PlayerChar = PC ? PC->GetBIBCharacter() : nullptr)
	{
		PlayerChar->CloseExchange();
	}
}

FString UBIBBlueprintLibrary::GetExchangeStatName(int32 StatIndex)
{
	return ABIBPlayerCharacter::StatName(StatIndex);
}

// ============================================================================ screens

void UBIBBlueprintLibrary::GoToScreen(const UObject* WorldContextObject, EBIBScreen Screen)
{
	if (ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject))
	{
		PC->SetStatus(FString());
		PC->SetScreen(Screen);
	}
}

EBIBScreen UBIBBlueprintLibrary::GetCurrentScreen(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	return PC ? PC->GetScreen() : EBIBScreen::None;
}

FString UBIBBlueprintLibrary::GetStatusText(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject);
	return PC ? PC->GetStatus() : FString();
}

void UBIBBlueprintLibrary::QuitDemo(const UObject* WorldContextObject)
{
	if (ABIBPlayerController* PC = BIBLibraryInternal::ControllerOf(WorldContextObject))
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}

// ============================================================================ widget base

ABIBPlayerController* UBIBUserWidget::GetBIBController() const
{
	return Cast<ABIBPlayerController>(GetOwningPlayer());
}

FBIBHudInfo UBIBUserWidget::GetHud() const
{
	return UBIBBlueprintLibrary::GetHudInfo(this);
}

void UBIBUserWidget::GoToScreen(EBIBScreen NewScreen)
{
	UBIBBlueprintLibrary::GoToScreen(this, NewScreen);
}

void UBIBUserWidget::RefreshScreen()
{
	if (ABIBPlayerController* PC = GetBIBController())
	{
		PC->MarkMenuDirty();
	}
}
