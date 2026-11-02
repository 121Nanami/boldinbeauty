// Owns input state (Enhanced Input assets or key fallback) and the UI: UMG widget classes when assigned in the
// Blueprint, otherwise the code-built Slate HUD and menus.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BIBGame/BIBInput.h"
#include "BIBPlayerController.generated.h"

class ABIBGameMode;
class ABIBPlayerCharacter;
class SBox;
class SWidget;
class UInputAction;
class UInputMappingContext;
class UUserWidget;

UENUM(BlueprintType)
enum class EBIBScreen : uint8
{
	None, // gameplay HUD only
	MainMenu, // retired 2026-10-07 (the 2D lobby was deleted; the Ark is the hub): SetScreen(MainMenu) goes to the Ark
	Summon,
	Operators,
	Equipment,
	Team,
	Merchant,
	DemoTools,
	Results,
	Pause,
	SignIn, // log in / create an account / guest (code-built Slate screen; no UMG class needed)
	ArkMenu, // the Ark's flow-chart menu (BIBArkUI; owner request 2026-10-07)
	ArkPanel // a station's counters or a personal page, over the 3D Ark (BIBArkUI)
};

// What the Ark interface shows (BIBArkUI.h). Times are FPlatformTime::Seconds(), so they also run while the game is paused.
struct FBIBArkUi
{
	TArray<FString> MenuPath;            // the node picked in each column (MenuPath[0] is a child of the root)
	double MenuOpenedAt = 0.0;
	int32 MenuColumnsShown = 0;          // columns on screen at the last build: only a new one animates in
	FString PanelStation;                // "" = a personal page opened from the menu
	int32 PanelCounter = 0;
	TMap<FString, int32> LastCounter;    // station id -> the counter tab last used there (reopened on it; session only, not saved)
	FString PanelTitle;                  // a personal page: the menu entry's name
	FString PanelIcon;
	FLinearColor PanelColour = FLinearColor(0.65f, 0.45f, 1.f);
	int32 RootTab = 0;                   // the page the panel opened on: Back from here closes the panel
	FString RootPage;
	double PanelOpenedAt = 0.0;
	FString ArrivalName;                 // banner after a teleport
	FString ArrivalSub;
	FLinearColor ArrivalColour = FLinearColor::White;
	double ArrivalAt = -100.0;
	FString Toast;
	double ToastAt = -100.0;
};

UCLASS()
class BOLDINBEAUTYDEMO_API ABIBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABIBPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	// ------------------------------------------------------------------ input
	/** Mapping context added at BeginPlay (e.g. IMC_BIB_Default). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BIB|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Digital actions (bool). Any input without an asset falls back to its default key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BIB|Input")
	TMap<EBIBInput, TObjectPtr<UInputAction>> ButtonActions;

	/** Axis2D action for camera look (Mouse XY 2D-Axis). Falls back to raw mouse delta. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BIB|Input")
	TObjectPtr<UInputAction> LookAction;

	UFUNCTION(BlueprintPure, Category = "BIB|Input")
	bool IsInputDown(EBIBInput Input) const;

	UFUNCTION(BlueprintPure, Category = "BIB|Input")
	bool WasInputPressed(EBIBInput Input) const;

	FVector2D GetLookDelta() const { return LookDelta; }
	/** Mobile controls (BIBTouch): a touch button holds a logical input down; a short tap still counts as one press. */
	void SetVirtualInput(EBIBInput Input, bool bDown);
	/** Mobile controls: camera drag in pixels (1080p). */
	void AddTouchLook(const FVector2D& Delta) { TouchLook += Delta; }
	/** Combat feel: a short camera jolt (Settings > camera shake / reduce motion respected). */
	void AddShake(float Strength, float Seconds);
	/** Combat feel (2026-10-13): a directional kick of the view along a hit (WorldDirection = attacker -> target); it snaps, then
	 *  overshoots and settles (BIBRules/BIBHitFeel.h KickOscillations). Settings > camera shake / reduce motion respected. */
	void AddKick(const FVector& WorldDirection, float Degrees, float Seconds);
	static FKey DefaultKey(EBIBInput Input);
	/** Gamepad buttons for an input (Slot 0 / 1; EKeys::Invalid when none). B = dodge: it holds Run and presses Parry. */
	static FKey PadKey(EBIBInput Input, int32 Slot);

	// --------------------------------------------------------------------- UI
	/** UMG HUD (parent class BIBUserWidget recommended). Empty = Slate HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BIB|UI")
	TSubclassOf<UUserWidget> HudWidgetClass;

	/** UMG widget per menu screen. Screens without an entry use the Slate menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BIB|UI")
	TMap<EBIBScreen, TSubclassOf<UUserWidget>> ScreenWidgetClasses;

	UFUNCTION(BlueprintCallable, Category = "BIB|UI")
	void SetScreen(EBIBScreen NewScreen);

	UFUNCTION(BlueprintPure, Category = "BIB|UI")
	EBIBScreen GetScreen() const { return Screen; }

	/** Rebuilds the current menu (UMG screens are re-created) on the next tick. */
	UFUNCTION(BlueprintCallable, Category = "BIB|UI")
	void MarkMenuDirty() { bMenuDirty = true; }

	UFUNCTION(BlueprintCallable, Category = "BIB|UI")
	void SetStatus(const FString& Text);

	UFUNCTION(BlueprintPure, Category = "BIB|UI")
	FString GetStatus() const { return StatusText; }

	void ShowResults(const TArray<FString>& Lines, bool bSuccess);

	// --------------------------------------------------------------- the Ark (owner request 2026-10-07: the Ark is the hub)
	FBIBArkUi ArkUi;
	/** After sign-in or on a fresh map: the Ark when it replaces the lobby (travels there), else the 2D hub. */
	void ShowHub();
	void OpenArkMenu();
	void CloseArkScreens();
	/** A click on a menu node: a branch opens the next column; a leaf teleports, opens its page or runs its action. */
	void ArkMenuClick(const FString& NodeId);
	/** One column back (Backspace); closes the menu from the first column. */
	void ArkMenuUp();
	/** Counter < 0 = the tab last used at this station (the first one the first time, and while the tutorial is running). */
	bool ArkOpenStation(const FString& StationId, int32 Counter = -1);
	bool ArkOpenPage(int32 Tab, const FString& Page, const FString& Title, const FLinearColor& Colour, const FString& Icon);
	bool ArkTeleport(const FString& StationId);
	void ArkPanelBack();
	/** Runs one page action in the panel, with everything the 2D hub did around it (stages, overlays, prompts, key capture,
	 *  sign-out, account-server calls). */
	void ArkRunAction(const FString& ActionId);
	void ArkToast(const FString& Text);
	/** The Results screen's way out (Continue): back to the Ark. */
	void ContinueFromResults();
	/** Keys for the Pause / Results screens (called by the screen key processor before any widget sees them). True = used. */
	bool HandleScreenKey(const FKey& Key);
	ABIBGameMode* GetBIBGameMode() const;
	ABIBPlayerCharacter* GetBIBCharacter() const;

	// Menu selection state shared by the Slate menus and UMG screens.
	UPROPERTY(BlueprintReadWrite, Category = "BIB|UI")
	int32 SelectedBanner = 0;

	UPROPERTY(BlueprintReadWrite, Category = "BIB|UI")
	int32 SelectedOperator = 5;

	UPROPERTY(BlueprintReadWrite, Category = "BIB|UI")
	int32 EquipOperator = 5;

	UPROPERTY(BlueprintReadOnly, Category = "BIB|UI")
	TArray<FString> SummonLines;

	UPROPERTY(BlueprintReadOnly, Category = "BIB|UI")
	TArray<FString> ResultLines;

	UPROPERTY(BlueprintReadOnly, Category = "BIB|UI")
	bool bResultSuccess = false;

	/** Equipment screen selection; kept here so it survives the screen being re-created after an action. */
	UPROPERTY(BlueprintReadWrite, Category = "BIB|UI")
	int32 SelectedItemId = -1;

	// ------------------------------------------------------------------ test hooks (console / Python)
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBDamageEnemies(float Amount);

	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBFillUlt();

	/** Console: BIBAngels <0..1> sets every counter the angels can learn (1 = fully adapted, 0 = forget everything). */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBAngels(float Expertise);

	/** Console: BIBMotion <piece> [clip] plays a piece of the bb-Showreel motion pack (BIBMotion.h). */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	bool BIBMotion(const FString& Piece, const FString& Clip);

	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBSetNarratorSpeed(float Speed);

	/** BIBTeam Blood Death Gambler  (hub only): sets the three team slots by operator name; start the stage afterwards. */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBTeam(const FString& First, const FString& Second, const FString& Third);

	/** Owns every operator (already the demo default; useful after loading an old profile). */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBGrantAll();

	/** BIBUltVariant <n>: picks the active operator's alternative ultimate (unused: no operator has one since the owner's 2026-10-04 decisions). */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBUltVariant(int32 Variant);

	/** Prints the active operator's gameplay tags to the log and the message feed. */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBTags();

	/** Logs every BIB gameplay tag the rules expect but the project does not register. */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBCheckTags();

	/** Marks campaign levels 1..N as cleared (0-104, counted through all 13 chapters: 7 = up to Chapter 1 level 7, 8 = Chapter 1 done) so a test can jump ahead; levels still unlock in order.
	 *  This hook and BIBTeam / BIBGrantAll change the profile, so they only work for the admin account or with -BIBDev. */
	UFUNCTION(Exec, BlueprintCallable, Category = "BIB|Debug")
	void BIBSetCampaignProgress(int32 Levels);

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> HudWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> ScreenWidget;

	TSharedPtr<SWidget> RootWidget;
	TSharedPtr<SBox> MenuHost;
	EBIBScreen Screen = EBIBScreen::None;
	bool bMenuDirty = true;
	bool bPanelCursor = false;
	FString StatusText;

	TSharedPtr<SWidget> DamageLayer;     // floating damage numbers (Settings > Gameplay)
	TSharedPtr<SWidget> ArkLayer;        // Ark HUD + battle HUD (BIBArkUI)
	TSharedPtr<SWidget> ScreenFocus;     // the Ark menu / panel: takes keyboard focus (Esc, M, Backspace)
	TSharedPtr<class IInputProcessor> ScreenKeys;   // registered with Slate while Pause / Results is up (UI-only input mode)
	void UpdateScreenKeys();
	float LastHp = -1.f;                 // camera shake on a heavy hit (Settings > Gameplay)
	FString LastActive;
	float ShakeTime = 0.f;
	float ShakeDuration = 0.25f;
	float ShakeStrength = 1.6f;
	FRotator ShakeOffset = FRotator::ZeroRotator;
	FVector2D KickDir = FVector2D::ZeroVector;   // x = yaw, y = pitch (unit)
	float KickStrength = 0.f;
	float KickDuration = 0.f;
	double KickStartReal = 0.0;
	void UpdateFeedback(float DeltaTime);

	bool bDown[static_cast<int32>(EBIBInput::Count)] = {};
	bool bPressed[static_cast<int32>(EBIBInput::Count)] = {};
	FVector2D LookDelta = FVector2D::ZeroVector;
	bool VirtualDown[static_cast<int32>(EBIBInput::Count)] = {};
	bool VirtualLatch[static_cast<int32>(EBIBInput::Count)] = {};
	FVector2D TouchLook = FVector2D::ZeroVector;
	TSharedPtr<SWidget> TouchLayer;
	TSharedPtr<SWidget> MotionLayer;
	bool bMotionDialogue = false;
	bool bMotionExchange = false;
	TArray<FString> RecentToasts;

	void UpdateInputState();
	void ApplyInputMode();
	void RebuildMenu();
	bool CheatsAllowed() const;
	void OnProfileReady();
	void OnProfileCleared();
};
