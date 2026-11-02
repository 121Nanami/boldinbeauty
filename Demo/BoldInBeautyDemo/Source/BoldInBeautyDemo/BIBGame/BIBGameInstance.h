// Holds the player's profile (currencies, roster, equipment, banners, meta-game state) and the hub's navigation state
// for the whole session; both survive level travel.
//
// Accounts and saving (owner request 2026-10-04): the profile belongs to whoever is signed in (UBIBAccountSubsystem). When a
// session starts, the newest of the local save (Saved/BIB/Saves/<user id>.json) and the account's cloud save is loaded; a new
// account starts with only Luna and the tutorial. The profile is written locally after every hub action and every 10 seconds,
// and pushed to the account server when something changed. Launch with -BIBDev for the old developer profile (every operator,
// no sign-in, no saving) that the verify_*.py scripts expect, or -BIBGuest to skip the sign-in screen as a local guest.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Templates/UniquePtr.h"
#include "BIBGame/BIBAccount.h"
#include "BIBRules/BIBAngelMind.h"
#include "BIBRules/BIBCore.h"
#include "BIBRules/BIBPages.h"
#include "BIBGameInstance.generated.h"

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	BIB::FProfile& GetProfile();

	/** Stage to begin after the next level load (set by ABIBGameMode::StartStage when using stage maps). */
	int32 PendingStage = 0;

	/** Free-roam map id to start after the next level load (set by ABIBGameMode::StartFreeRoam). */
	FString PendingFreeRoam;

	/** Campaign stage of the Chapter 11 walk (Return to Nowhere) being opened with that free roam, 0 = a plain free roam. */
	int32 PendingWalkStage = 0;

	/** The Ark is the hub (owner request 2026-10-07): the 2D lobby was deleted; the game starts in L_Ark at the saved position. */
	/** -BIBClassicHud: the old UMG battle HUD (WBP_HUD) instead of the battle HUD that only shows what the fight needs. */
	bool UseClassicHud() const { return bClassicHud; }
	/** The Ark station the player last used or travelled to (BIBRules/BIBArk.cpp id). */
	FString ArkStation;
	/** A station the next Ark load must arrive at (explicit travel); otherwise the player arrives at the saved position. */
	FString PendingArkStation;

	/** Which hub tab and page is open, and what is selected in it. Kept here so it survives stage maps. */
	BIB::Pages::FUiState UiState;

	/** -BIBDev: the developer profile (every operator, no sign-in, nothing saved). */
	bool IsDevProfile() const { return bDevProfile; }
	bool IsProfileReady() const { return bProfileReady; }
	/** The angels' shared adaptive mind (BIBRules/BIBAngelMind.h): lives for the play session, carried between stages. */
	BIB::Angel::FMind& GetAngelMind() { return AngelMind; }
	/** True while the hub must stay behind the sign-in screen: no profile has been loaded yet. */
	bool NeedsSignIn() const { return !bDevProfile && !bProfileReady; }
	/** The owner/admin account may use the test hooks; so may the developer profile. */
	bool CheatsAllowed() const;

	/** Writes the profile to disk now and queues the cloud upload. Cheap; call after anything the player would hate to lose. */
	void SaveNow();
	/** Saves, pushes the last save to the account, then signs out (the sign-in screen comes back). */
	void SignOut();
	FString GetSaveStatus() const { return SaveStatus; }

	/** Applies the profile's Settings page (owner request 2026-10-06): the primary volume to the audio device, graphics to
	 *  UGameUserSettings (packaged / -game runs only: the editor keeps its own), the frame-rate counter, and the camera and
	 *  look options to the operator in play. bWindowMode also applies the window mode (only when the player changed it).
	 *  Graphics (owner request 2026-10-08, lag): the quality preset, per-group overrides, render scale, dynamic resolution,
	 *  weather effects, volumetric fog and the anime look. In PIE the engine-wide ones apply only once the player has
	 *  changed a graphics option (bGraphicsChanged) or the profile differs from the defaults, so the editor keeps its own. */
	void ApplyUserSettings(bool bWindowMode = false, bool bGraphicsChanged = false);

	/** Settings > Graphics > Auto-detect: runs the engine's hardware benchmark and stores the quality it suggests. */
	void AutoDetectGraphics();

	FBIBAccountEvent OnProfileReady;    // a profile was loaded: show the hub
	FBIBAccountEvent OnProfileCleared;  // signed out: show the sign-in screen

private:
	BIB::Angel::FMind AngelMind;
	bool OnSecondTick(float DeltaSeconds);
	void HandleSessionReady();
	void HandleSignedOut();
	void FinishLoad(const BIB::FAccountInfo& Info, const std::string& LocalText, const std::string& CloudText, bool bCloudUnreachable);
	void WriteLocal(bool bQueueCloud);
	bool CanUploadCloud() const;
	void UploadCloud(TFunction<void()> After);

	TUniquePtr<BIB::FProfile> Profile;
	FTSTicker::FDelegateHandle TickHandle;
	TWeakObjectPtr<UBIBAccountSubsystem> Account;

	bool bDevProfile = false;
	bool bProfileReady = false;
	bool bClassicHud = false;
	bool bSaveEnabled = false;
	FString SaveKey;                 // file name stem: the user id, or "guest"
	std::string LastProbe;           // the last saved profile (without its time stamp), to skip writes when nothing changed
	std::string PendingCloudText;
	bool bCloudDirty = false;
	bool bCloudInFlight = false;
	int64 LastCloudAttempt = 0;
	int32 AutoSaveTicks = 0;
	FString SaveStatus;
	bool bFpsShown = false;          // "stat fps" is a toggle: remember what the viewport shows
};
