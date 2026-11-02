// Blueprint/UMG access to the demo: profile, summoning, progression, equipment, team, stages and HUD data.
// Widget Blueprints call these instead of reaching into C++ objects. Every mutating call updates the
// controller's status text and refreshes the current menu screen.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBBlueprintLibrary.generated.h"

UENUM(BlueprintType)
enum class EBIBCurrency : uint8
{
	Gold,
	Diamonds,
	Bytes,
	AngelHearts,
	RegularTicket,
	SpecialTicket,
	DailyTicket,
	Mileage
};

UENUM(BlueprintType)
enum class EBIBBannerKind : uint8
{
	Standard,
	Featured,
	Daily
};

USTRUCT(BlueprintType)
struct FBIBResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB")
	bool bOk = false;

	UPROPERTY(BlueprintReadOnly, Category = "BIB")
	FString Message;
};

USTRUCT(BlueprintType)
struct FBIBOperatorInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 RosterIndex = -1;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 Stars = 3;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString RarityLabel;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bOwned = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bPlayable = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 Level = 1;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 LevelCap = 100;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 UpgradeStars = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 DuplicateUnits = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 NextStarCost = 20;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 NextLevelGold = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 NextLevelBytes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bNeedsHeartGate = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 HeartGateCost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float MaxHP = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float DamageMultiplier = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float DefencePercent = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Label;
};

USTRUCT(BlueprintType)
struct FBIBBannerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 Index = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") EBIBBannerKind Kind = EBIBBannerKind::Standard;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString FeaturedName;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 FeaturedStars = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 MileageRedeemCost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 RollsSince5Star = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 RollsSince6Star = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bLostFiftyFifty = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Description;
};

USTRUCT(BlueprintType)
struct FBIBSummonInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 Stars = 3;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bNew = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bFeatured = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bPity = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bTenPullGuarantee = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bFiftyFiftyGuarantee = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Label;
};

USTRUCT(BlueprintType)
struct FBIBEquipmentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 Id = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 SlotIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString SlotName;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bDamageBonus = true;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float TotalPercent = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 UpgradeLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 UpgradeCostBytes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 EquippedBy = -1;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString EquippedByName;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Label;
};

USTRUCT(BlueprintType)
struct FBIBAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Key;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float CooldownRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Note;
};

USTRUCT(BlueprintType)
struct FBIBTeamMemberInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float HPFraction = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bAlive = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bActive = false;
};

USTRUCT(BlueprintType)
struct FBIBHudInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString ActiveName;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float HP = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float MaxHP = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float HPFraction = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float Ult = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") float UltFraction = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FLinearColor Colour = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") TArray<FBIBAbilityInfo> Abilities;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") TArray<FBIBTeamMemberInfo> Team;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString KitLine;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString ActionLine;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bJudgeModeMenu = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bExchangeOpen = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") TArray<float> ExchangeValues;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 ExchangeFrom = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") int32 ExchangeTo = 1;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString StageTitle;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString Objective;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") TArray<FString> Messages;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") bool bDialogueOpen = false;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString DialogueSpeaker;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString DialogueLine;
	UPROPERTY(BlueprintReadOnly, Category = "BIB") FString ControlsHint;
};

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------ profile
	UFUNCTION(BlueprintPure, Category = "BIB|Profile", meta = (WorldContext = "WorldContextObject"))
	static int32 GetCurrency(const UObject* WorldContextObject, EBIBCurrency Currency);

	UFUNCTION(BlueprintPure, Category = "BIB|Profile", meta = (WorldContext = "WorldContextObject"))
	static FString GetCurrencySummary(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Profile", meta = (WorldContext = "WorldContextObject"))
	static int32 GetDay(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Profile", meta = (WorldContext = "WorldContextObject"))
	static void GrantDemoCurrency(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Profile", meta = (WorldContext = "WorldContextObject"))
	static void AdvanceDay(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Profile", meta = (WorldContext = "WorldContextObject"))
	static void ResetProfile(const UObject* WorldContextObject);

	// ------------------------------------------------- arcade, arena, live ops (owner request 2026-10-06; Tools/verify_arcade.py)
	/** A number from the profile for tests: trophies, arenaFights, towerLost0/1, towerBest0/1, bossBest, nightWins0..2, rhythmBest,
	 *  gangCoins, arcadeTokens, seasonXp, returnActive, mail, popups (damage numbers alive), bossRound. -1 when unknown. */
	UFUNCTION(BlueprintPure, Category = "BIB|Test", meta = (WorldContext = "WorldContextObject"))
	static int32 GetMetaValue(const UObject* WorldContextObject, const FString& Key);

	UFUNCTION(BlueprintPure, Category = "BIB|Test")
	static bool IsArcadeOverlayOpen();

	/** One line describing the open overlay ("none", "prompt", "night ...", "rhythm ...", "arena ..."). */
	UFUNCTION(BlueprintPure, Category = "BIB|Test")
	static FString GetArcadeOverlayState();

	/** Drives the open overlay: key:A, keyup:Q, advance:10, night:media:left, night:win, rhythm:autoplay, close, prompt:text:..., prompt:key:... */
	UFUNCTION(BlueprintCallable, Category = "BIB|Test")
	static bool ArcadeOverlayCommand(const FString& Command);

	// ---------------------------------------------------------------- operators
	UFUNCTION(BlueprintPure, Category = "BIB|Operators", meta = (WorldContext = "WorldContextObject"))
	static TArray<FBIBOperatorInfo> GetRoster(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Operators", meta = (WorldContext = "WorldContextObject"))
	static FBIBOperatorInfo GetOperatorInfo(const UObject* WorldContextObject, int32 RosterIndex);

	UFUNCTION(BlueprintCallable, Category = "BIB|Operators", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult LevelUpOperator(const UObject* WorldContextObject, int32 RosterIndex, bool bSupplementWithDiamonds);

	UFUNCTION(BlueprintCallable, Category = "BIB|Operators", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult PayHeartGate(const UObject* WorldContextObject, int32 RosterIndex);

	UFUNCTION(BlueprintCallable, Category = "BIB|Operators", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult UpgradeStar(const UObject* WorldContextObject, int32 RosterIndex);

	// ---------------------------------------------------------------- summoning
	UFUNCTION(BlueprintPure, Category = "BIB|Summon", meta = (WorldContext = "WorldContextObject"))
	static TArray<FBIBBannerInfo> GetBanners(const UObject* WorldContextObject);

	/** Count must be 1 or 10. Daily banners only allow 1 with a ticket. */
	UFUNCTION(BlueprintCallable, Category = "BIB|Summon", meta = (WorldContext = "WorldContextObject"))
	static TArray<FBIBSummonInfo> Summon(const UObject* WorldContextObject, int32 BannerIndex, int32 Count, bool bUseDiamonds, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "BIB|Summon", meta = (WorldContext = "WorldContextObject"))
	static bool IsTutorialSummonAvailable(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Summon", meta = (WorldContext = "WorldContextObject"))
	static TArray<FBIBSummonInfo> TutorialSummon(const UObject* WorldContextObject, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "BIB|Summon", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult RedeemMileage(const UObject* WorldContextObject, int32 BannerIndex);

	UFUNCTION(BlueprintPure, Category = "BIB|Summon", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetLastSummonLines(const UObject* WorldContextObject);

	// ---------------------------------------------------------------- equipment
	UFUNCTION(BlueprintPure, Category = "BIB|Equipment", meta = (WorldContext = "WorldContextObject"))
	static TArray<FBIBEquipmentInfo> GetInventory(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Equipment", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult EquipItem(const UObject* WorldContextObject, int32 ItemId, int32 RosterIndex);

	UFUNCTION(BlueprintCallable, Category = "BIB|Equipment", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult UnequipItem(const UObject* WorldContextObject, int32 ItemId);

	UFUNCTION(BlueprintCallable, Category = "BIB|Equipment", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult UpgradeItem(const UObject* WorldContextObject, int32 ItemId);

	// --------------------------------------------------------------------- team
	UFUNCTION(BlueprintPure, Category = "BIB|Team", meta = (WorldContext = "WorldContextObject"))
	static TArray<int32> GetTeam(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Team", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult SetTeamSlot(const UObject* WorldContextObject, int32 Slot, int32 RosterIndex);

	// ----------------------------------------------------------------- merchant
	UFUNCTION(BlueprintPure, Category = "BIB|Merchant")
	static TArray<FString> GetMerchantOffers();

	UFUNCTION(BlueprintCallable, Category = "BIB|Merchant", meta = (WorldContext = "WorldContextObject"))
	static FBIBResult BuyMerchantOffer(const UObject* WorldContextObject, int32 OfferIndex);

	// ------------------------------------------------------------------- stages
	UFUNCTION(BlueprintPure, Category = "BIB|Stage")
	static int32 GetStageCount();

	UFUNCTION(BlueprintPure, Category = "BIB|Stage")
	static FString GetStageDisplayName(int32 Stage);

	/** Sections in the stage's plan (campaign stage ids 1..104, tower and sub-story ids too); 0 for an invalid id. */
	UFUNCTION(BlueprintPure, Category = "BIB|Stage")
	static int32 GetStageSectionCount(int32 Stage);

	/** What one section spawns: Basic, Brutes, Ranged, Mages, Assassins, Mini-bosses, Bosses (empty when the id or section is invalid). */
	UFUNCTION(BlueprintPure, Category = "BIB|Stage")
	static TArray<int32> GetStageSectionCounts(int32 Stage, int32 SectionIndex);

	/** The enemy health and damage multipliers of a stage (X = health, Y = damage). */
	UFUNCTION(BlueprintPure, Category = "BIB|Stage")
	static FVector2D GetStageScales(int32 Stage);

	UFUNCTION(BlueprintCallable, Category = "BIB|Stage", meta = (WorldContext = "WorldContextObject"))
	static bool StartStage(const UObject* WorldContextObject, int32 Stage, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "BIB|Stage", meta = (WorldContext = "WorldContextObject"))
	static void ReturnToHub(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Stage", meta = (WorldContext = "WorldContextObject"))
	static void AbandonStage(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Stage", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetResultLines(const UObject* WorldContextObject, bool& bSuccess);

	// ---------------------------------------------------------------------- HUD
	UFUNCTION(BlueprintPure, Category = "BIB|HUD", meta = (WorldContext = "WorldContextObject"))
	static FBIBHudInfo GetHudInfo(const UObject* WorldContextObject);

	// Read-only test hooks (Tools/verify_kits.py): the active operator's and an enemy's GameplayTags, and an enemy's HP.
	UFUNCTION(BlueprintPure, Category = "BIB|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetOperatorTags(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Debug")
	static FString GetEnemyTags(const AActor* Enemy);

	UFUNCTION(BlueprintPure, Category = "BIB|Debug")
	static float GetEnemyHP(const AActor* Enemy);

	UFUNCTION(BlueprintCallable, Category = "BIB|HUD", meta = (WorldContext = "WorldContextObject"))
	static void ExchangeSelect(const UObject* WorldContextObject, bool bFrom, int32 StatIndex);

	UFUNCTION(BlueprintCallable, Category = "BIB|HUD", meta = (WorldContext = "WorldContextObject"))
	static void ExchangeApply(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|HUD", meta = (WorldContext = "WorldContextObject"))
	static void CloseExchange(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|HUD")
	static FString GetExchangeStatName(int32 StatIndex);

	// ------------------------------------------------------------------ screens
	UFUNCTION(BlueprintCallable, Category = "BIB|UI", meta = (WorldContext = "WorldContextObject"))
	static void GoToScreen(const UObject* WorldContextObject, EBIBScreen Screen);

	UFUNCTION(BlueprintPure, Category = "BIB|UI", meta = (WorldContext = "WorldContextObject"))
	static EBIBScreen GetCurrentScreen(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|UI", meta = (WorldContext = "WorldContextObject"))
	static FString GetStatusText(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|UI", meta = (WorldContext = "WorldContextObject"))
	static void QuitDemo(const UObject* WorldContextObject);
};

/** Optional parent class for the demo's Widget Blueprints. */
UCLASS(Blueprintable)
class BOLDINBEAUTYDEMO_API UBIBUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "BIB|UI")
	ABIBPlayerController* GetBIBController() const;

	UFUNCTION(BlueprintPure, Category = "BIB|UI")
	FBIBHudInfo GetHud() const;

	UFUNCTION(BlueprintCallable, Category = "BIB|UI")
	void GoToScreen(EBIBScreen NewScreen);

	/** Re-creates this screen next tick (use after changing selections). */
	UFUNCTION(BlueprintCallable, Category = "BIB|UI")
	void RefreshScreen();
};
