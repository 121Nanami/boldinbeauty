#include "BIBGame/BIBGameInstance.h"
#include "AudioDevice.h"
#include "BIBGame/BIBOnlineSubsystem.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

#include "BIBRules/BIBConfig.h"
#include "BIBRules/BIBLiveOps.h"
#include "BIBRules/BIBMetaOps.h"
#include "BIBRules/BIBSave.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Scalability.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBIBSave, Log, All);

namespace BIBSaveIO
{
	FString ToF(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	int64 NowUnix()
	{
		return FDateTime::UtcNow().ToUnixTimestamp();
	}

	FString PathFor(const FString& Key)
	{
		FString Safe;
		for (const TCHAR Character : Key)
		{
			if (FChar::IsAlnum(Character))
			{
				Safe.AppendChar(Character);
			}
		}
		if (Safe.IsEmpty())
		{
			Safe = TEXT("unknown");
		}
		return FPaths::ProjectSavedDir() / TEXT("BIB") / TEXT("Saves") / (Safe + TEXT(".json"));
	}

	std::string ReadFile(const FString& Path)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() == 0)
		{
			return std::string();
		}
		return std::string(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
	}

	// Write to a temporary file, keep the previous save as .bak, then move the new one into place: a power cut leaves the old save.
	bool WriteAtomic(const FString& Path, const std::string& Text)
	{
		IFileManager& Files = IFileManager::Get();
		Files.MakeDirectory(*FPaths::GetPath(Path), true);
		const FString Temp = Path + TEXT(".tmp");
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Text.data()), static_cast<int32>(Text.size()));
		if (!FFileHelper::SaveArrayToFile(Bytes, *Temp))
		{
			return false;
		}
		if (Files.FileExists(*Path))
		{
			Files.Copy(*(Path + TEXT(".bak")), *Path);
		}
		return Files.Move(*Path, *Temp, true, true);
	}

	// The newest readable save of this account: the file, else its backup.
	std::string ReadLocal(const FString& Key, const std::string& ExpectedUserId)
	{
		const FString Path = PathFor(Key);
		for (const FString& Candidate : {Path, Path + TEXT(".bak")})
		{
			const std::string Text = ReadFile(Candidate);
			const BIB::Save::FSaveInfo Info = BIB::Save::Peek(Text);
			if (Info.bValid && Info.UserId == ExpectedUserId)
			{
				return Text;
			}
		}
		return std::string();
	}
}

void UBIBGameInstance::Init()
{
	Super::Init();
	bDevProfile = FParse::Param(FCommandLine::Get(), TEXT("BIBDev"));
	bClassicHud = FParse::Param(FCommandLine::Get(), TEXT("BIBClassicHud"));
	GetProfile();
	if (bDevProfile)
	{
		// The developer profile the verify scripts expect: every operator, no account, nothing saved.
		Profile->GrantAllOperators();
		if (BIB::Config::TestArcadeTokenFloor > 0)
		{
			Profile->Add(BIB::ECurrency::ArcadeToken, std::max(0, BIB::Config::TestArcadeTokenFloor - Profile->Get(BIB::ECurrency::ArcadeToken)));
		}
		bProfileReady = true;
	}
	else
	{
		Account = GetSubsystem<UBIBAccountSubsystem>();
		if (UBIBAccountSubsystem* Accounts = Account.Get())
		{
			Accounts->OnSessionReady.AddUObject(this, &UBIBGameInstance::HandleSessionReady);
			Accounts->OnSignedOut.AddUObject(this, &UBIBGameInstance::HandleSignedOut);
			if (FParse::Param(FCommandLine::Get(), TEXT("BIBGuest")))
			{
				Accounts->ContinueAsGuest();
			}
			else if (!FParse::Param(FCommandLine::Get(), TEXT("BIBNoAutoLogin")))   // test flag: always show the sign-in screen
			{
				Accounts->RestoreSession();
			}
		}
	}
	// Outer World buildings produce while the game runs: 1 real minute = 1 world hour (PLACEHOLDER pace). The same one-second
	// tick keeps the featured-banner clock, writes the local save and uploads the cloud save.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UBIBGameInstance::OnSecondTick), 1.0f);
}

void UBIBGameInstance::Shutdown()
{
	SaveNow();
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Shutdown();
}

BIB::FProfile& UBIBGameInstance::GetProfile()
{
	if (!Profile.IsValid())
	{
		const uint32 Seed = static_cast<uint32>(FDateTime::Now().GetTicks() & 0xFFFFFFFF);
		Profile = MakeUnique<BIB::FProfile>(Seed);
		Profile->SetTime(BIBSaveIO::NowUnix());
	}
	return *Profile;
}

bool UBIBGameInstance::CheatsAllowed() const
{
	return bDevProfile || (Profile.IsValid() && Profile->Account.bAdmin && Profile->Account.bSignedIn);
}

// ============================================================================ the one-second tick
bool UBIBGameInstance::OnSecondTick(float DeltaSeconds)
{
	BIB::FProfile& Current = GetProfile();
	const int64 Now = BIBSaveIO::NowUnix();
	Current.SetTime(Now); // the 3-day featured-banner rotation follows the UTC clock
	// The Outer World runs on the real clock (classic AFK rewards, owner rebalance 2026-10-07; BIBConfig.h WorldHoursPerRealHour).
	BIB::Meta::AdvanceWorld(Current, static_cast<double>(DeltaSeconds) / 3600.0 * BIB::Config::WorldHoursPerRealHour);
	if (bProfileReady)
	{
		// Daily reset on the UTC day, monthly seasons (season pass, arena, gang shop), the returning-player event (2026-10-06).
		const std::vector<std::string> Lines = BIB::LiveOps::UpdateClock(Current, Now);
		if (!Lines.empty())
		{
			UiState.Status = Lines.back();
		}
		if (UBIBOnlineSubsystem* Online = GetSubsystem<UBIBOnlineSubsystem>())
		{
			Online->Tick1s();
		}
	}
	if (bProfileReady && !bDevProfile)
	{
		if (++AutoSaveTicks >= 10)
		{
			AutoSaveTicks = 0;
			WriteLocal(false);
		}
		if (bCloudDirty && !bCloudInFlight && Now - LastCloudAttempt >= 15 && CanUploadCloud())
		{
			UploadCloud(TFunction<void()>());
		}
	}
	return true;
}

// ============================================================================ loading
void UBIBGameInstance::HandleSessionReady()
{
	UBIBAccountSubsystem* Accounts = Account.Get();
	if (!Accounts)
	{
		return;
	}
	const BIB::FAccountInfo Info = Accounts->GetAccount();
	bProfileReady = false;
	bSaveEnabled = false;
	SaveKey = Info.bGuest ? FString(TEXT("guest")) : BIBSaveIO::ToF(Info.UserId);
	const std::string Expected = Info.bGuest ? std::string() : Info.UserId;
	const std::string LocalText = BIBSaveIO::ReadLocal(SaveKey, Expected);

	if (Info.bSignedIn && !Accounts->IsOffline())
	{
		SaveStatus = TEXT("Loading your save...");
		TWeakObjectPtr<UBIBGameInstance> WeakThis(this);
		Accounts->Request(TEXT("GET"), TEXT("/v1/save"), std::string(), true, [WeakThis, Info, LocalText](int32 Status, const BIB::Json::FValue& Reply, bool bNetworkError)
		{
			UBIBGameInstance* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			std::string CloudText;
			if (!bNetworkError && Status == 200)
			{
				CloudText = Reply.GetString("save");
				if (BIB::Save::Peek(CloudText).UserId != Info.UserId)
				{
					CloudText.clear(); // never load a save written for another account
				}
			}
			Self->FinishLoad(Info, LocalText, CloudText, bNetworkError || Status != 200);
		});
		return;
	}
	FinishLoad(Info, LocalText, std::string(), false);
}

void UBIBGameInstance::FinishLoad(const BIB::FAccountInfo& Info, const std::string& LocalText, const std::string& CloudText, bool bCloudUnreachable)
{
	BIB::FProfile& Current = GetProfile();
	const BIB::Save::FSaveInfo Local = BIB::Save::Peek(LocalText);
	const BIB::Save::FSaveInfo Cloud = BIB::Save::Peek(CloudText);
	// The newest save wins; on a tie the local file does.
	const bool bCloudFirst = Cloud.bValid && (!Local.bValid || Cloud.SavedAtUnix > Local.SavedAtUnix);
	const std::string* const Order[2] = {bCloudFirst ? &CloudText : &LocalText, bCloudFirst ? &LocalText : &CloudText};

	Current.Account = Info;
	Current.StartNewPlayer(); // a new account: only Luna, an empty squad, the tutorial pending
	const std::string* Loaded = nullptr;
	for (const std::string* Text : Order)
	{
		if (Text->empty())
		{
			continue;
		}
		std::string Error;
		if (BIB::Save::Deserialize(*Text, Current, Error))
		{
			Loaded = Text;
			break;
		}
		UE_LOG(LogBIBSave, Warning, TEXT("A save could not be read and was skipped: %s"), *BIBSaveIO::ToF(Error));
	}
	Current.ApplyAccount(Info); // name, id and, for the owner/admin account, every entitlement
	if (BIB::Config::TestArcadeTokenFloor > 0)
	{
		Current.Add(BIB::ECurrency::ArcadeToken, std::max(0, BIB::Config::TestArcadeTokenFloor - Current.Get(BIB::ECurrency::ArcadeToken)));
	}
	Current.SetTime(BIBSaveIO::NowUnix());
	Current.Rng().seed(static_cast<uint32>(FDateTime::UtcNow().GetTicks() & 0xFFFFFFFF));
	UiState = BIB::Pages::FUiState();
	// Time away: offline Outer World production, day and season changes, and the welcome-back event after 14 days.
	const std::vector<std::string> ClockLines = BIB::LiveOps::UpdateClock(Current, BIBSaveIO::NowUnix());
	if (!ClockLines.empty())
	{
		UiState.Status = ClockLines.back();
	}

	bProfileReady = true;
	bSaveEnabled = true;
	LastProbe.clear();
	bCloudDirty = false;
	// Queue an upload when the cloud copy is missing or older than what is on this machine.
	const bool bCloudBehind = Info.bSignedIn && !bCloudUnreachable && Loaded && (!Cloud.bValid || (Loaded == &LocalText && Local.SavedAtUnix > Cloud.SavedAtUnix));
	WriteLocal(bCloudBehind || (Info.bSignedIn && !Loaded));
	SaveStatus = Loaded ? TEXT("Save loaded.") : TEXT("New game.");
	ApplyUserSettings();
	UE_LOG(LogBIBSave, Display, TEXT("%s for %s%s"), Loaded ? (bCloudFirst && Loaded == &CloudText ? TEXT("Loaded the account save") : TEXT("Loaded the local save")) : TEXT("Started a new game"),
		*SaveKey, bCloudUnreachable ? TEXT(" (account server unreachable)") : TEXT(""));
	OnProfileReady.Broadcast();
}

// ============================================================================ saving
void UBIBGameInstance::WriteLocal(bool bQueueCloud)
{
	if (bDevProfile || !bSaveEnabled || !Profile.IsValid())
	{
		return;
	}
	const std::string Probe = BIB::Save::Serialize(*Profile, 0);
	if (Probe == LastProbe)
	{
		return; // nothing changed since the last write
	}
	const std::string Text = BIB::Save::Serialize(*Profile, BIBSaveIO::NowUnix());
	if (!BIBSaveIO::WriteAtomic(BIBSaveIO::PathFor(SaveKey), Text))
	{
		SaveStatus = TEXT("Could not write the save file.");
		UE_LOG(LogBIBSave, Error, TEXT("Could not write %s"), *BIBSaveIO::PathFor(SaveKey));
		return;
	}
	LastProbe = Probe;
	if (bQueueCloud)
	{
		PendingCloudText = Text;
		bCloudDirty = true;
	}
}

void UBIBGameInstance::SaveNow()
{
	WriteLocal(true);
}

bool UBIBGameInstance::CanUploadCloud() const
{
	const UBIBAccountSubsystem* Accounts = Account.Get();
	return Accounts && Accounts->GetAccount().bSignedIn && !Accounts->IsOffline() && Accounts->HasToken() && !PendingCloudText.empty();
}

void UBIBGameInstance::UploadCloud(TFunction<void()> After)
{
	UBIBAccountSubsystem* Accounts = Account.Get();
	if (!Accounts || !CanUploadCloud())
	{
		if (After)
		{
			After();
		}
		return;
	}
	bCloudInFlight = true;
	LastCloudAttempt = BIBSaveIO::NowUnix();
	BIB::Json::FValue Body = BIB::Json::FValue::MakeObject();
	Body.Set("save", BIB::Json::FValue::MakeString(PendingCloudText));
	const std::string Sent = PendingCloudText;
	TWeakObjectPtr<UBIBGameInstance> WeakThis(this);
	Accounts->Request(TEXT("PUT"), TEXT("/v1/save"), BIB::Json::Write(Body), true, [WeakThis, Sent, After](int32 Status, const BIB::Json::FValue& Reply, bool bNetworkError)
	{
		UBIBGameInstance* Self = WeakThis.Get();
		if (Self)
		{
			Self->bCloudInFlight = false;
			if (!bNetworkError && Status == 200)
			{
				if (Self->PendingCloudText == Sent)
				{
					Self->bCloudDirty = false;
				}
				Self->SaveStatus = TEXT("Saved to your account.");
			}
			else if (!bNetworkError && Status == 409)
			{
				// Another device saved more recently. The next change made here is newer still and will replace it.
				Self->bCloudDirty = false;
				Self->SaveStatus = TEXT("Your account has a newer save from another device; your next change here will replace it.");
				UE_LOG(LogBIBSave, Warning, TEXT("Cloud save is newer than this one (%s)."), *BIBSaveIO::ToF(Reply.GetString("error")));
			}
			else if (!bNetworkError && Status == 401)
			{
				Self->SaveStatus = TEXT("Your sign-in expired: progress is saved on this device. Sign out and log in again to sync.");
			}
			else
			{
				Self->SaveStatus = TEXT("Could not reach the account server: progress is saved on this device and will sync later.");
			}
		}
		if (After)
		{
			After();
		}
	});
}

// ============================================================================ sign-out
void UBIBGameInstance::SignOut()
{
	UBIBAccountSubsystem* Accounts = Account.Get();
	if (!Accounts)
	{
		return;
	}
	SaveNow();
	TWeakObjectPtr<UBIBGameInstance> WeakThis(this);
	auto Finish = [WeakThis]()
	{
		if (UBIBGameInstance* Self = WeakThis.Get())
		{
			if (UBIBAccountSubsystem* Subsystem = Self->Account.Get())
			{
				Subsystem->SignOut(); // calls HandleSignedOut
			}
		}
	};
	if (bCloudDirty && CanUploadCloud() && !bCloudInFlight)
	{
		UploadCloud(Finish); // push the last save first; sign out whatever the answer
	}
	else
	{
		Finish();
	}
}

void UBIBGameInstance::HandleSignedOut()
{
	bProfileReady = false;
	bSaveEnabled = false;
	bCloudDirty = false;
	PendingCloudText.clear();
	LastProbe.clear();
	SaveKey.Reset();
	BIB::FProfile& Current = GetProfile();
	Current.Account = BIB::FAccountInfo();
	Current.StartNewPlayer(); // a blank placeholder behind the sign-in screen; the next sign-in loads a profile
	Current.Account = BIB::FAccountInfo();
	UiState = BIB::Pages::FUiState();
	SaveStatus.Reset();
	OnProfileCleared.Broadcast();
}

namespace
{
	// Engine scalability for the profile: the preset, then every group the player set on its own.
	Scalability::FQualityLevels QualityLevelsFor(const BIB::FSettings& Settings)
	{
		const int32 Preset = FMath::Clamp(Settings.GraphicsQuality, 0, 3);
		auto Pick = [&](int Group)
		{
			const int Value = Settings.GraphicsDetail[Group];
			return Value < 0 ? Preset : FMath::Clamp(Value, 0, 3);
		};
		Scalability::FQualityLevels Levels;
		Levels.SetFromSingleQualityLevel(Preset);
		Levels.ResolutionQuality = static_cast<float>(FMath::Clamp(Settings.ResolutionScale, BIB::ResolutionScaleMin, 100));
		Levels.ViewDistanceQuality = Pick(BIB::GfxViewDistance);
		Levels.ShadowQuality = Pick(BIB::GfxShadows);
		Levels.GlobalIlluminationQuality = Pick(BIB::GfxLighting);
		Levels.ReflectionQuality = Pick(BIB::GfxReflections);
		Levels.PostProcessQuality = Pick(BIB::GfxPostProcess);
		Levels.TextureQuality = Pick(BIB::GfxTextures);
		Levels.EffectsQuality = Pick(BIB::GfxEffects);
		Levels.AntiAliasingQuality = Pick(BIB::GfxAntiAliasing);
		Levels.FoliageQuality = Pick(BIB::GfxFoliage);
		Levels.ShadingQuality = Pick(BIB::GfxShading);
		return Levels;
	}

	bool HasCustomGraphics(const BIB::FSettings& Settings)
	{
		const BIB::FSettings Defaults;
		for (int Group = 0; Group < BIB::GraphicsGroupCount; ++Group)
		{
			if (Settings.GraphicsDetail[Group] != Defaults.GraphicsDetail[Group])
			{
				return true;
			}
		}
		return Settings.GraphicsQuality != Defaults.GraphicsQuality || Settings.ResolutionScale != Defaults.ResolutionScale
			|| Settings.bVolumetricFog != Defaults.bVolumetricFog || Settings.bDynamicResolution != Defaults.bDynamicResolution;
	}

	void SetGameCVar(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

void UBIBGameInstance::AutoDetectGraphics()
{
	if (!Profile)
	{
		return;
	}
	// The engine's CPU / GPU benchmark (a fraction of a second): the lowest group it suggests becomes the preset, groups it
	// rates higher keep their own level, and the render scale it suggests is used as is.
	const Scalability::FQualityLevels Bench = Scalability::BenchmarkQualityLevels();
	BIB::FSettings& Settings = Profile->Meta.Settings;
	const int32 Preset = FMath::Clamp(Bench.GetMinQualityLevel(), 0, 3);
	Settings.GraphicsQuality = Preset;
	const int32 Groups[BIB::GraphicsGroupCount] = {Bench.ViewDistanceQuality, Bench.ShadowQuality, Bench.GlobalIlluminationQuality,
		Bench.ReflectionQuality, Bench.PostProcessQuality, Bench.TextureQuality, Bench.EffectsQuality, Bench.AntiAliasingQuality,
		Bench.FoliageQuality, Bench.ShadingQuality};
	for (int32 Group = 0; Group < BIB::GraphicsGroupCount; ++Group)
	{
		const int32 Level = FMath::Clamp(Groups[Group], 0, 3);
		Settings.GraphicsDetail[Group] = Level == Preset ? -1 : Level;
	}
	Settings.ResolutionScale = FMath::Clamp(FMath::RoundToInt(Bench.ResolutionQuality / 10.f) * 10, BIB::ResolutionScaleMin, 100);
	UE_LOG(LogTemp, Display, TEXT("BIB settings: auto-detect chose preset %d, render scale %d%% (shadows %d, lighting %d, reflections %d)."),
		Preset, Settings.ResolutionScale, Bench.ShadowQuality, Bench.GlobalIlluminationQuality, Bench.ReflectionQuality);
	SaveNow();
	ApplyUserSettings(false, true);
}

void UBIBGameInstance::ApplyUserSettings(bool bWindowMode, bool bGraphicsChanged)
{
	if (!Profile)
	{
		return;
	}
	const BIB::FSettings& Settings = Profile->Meta.Settings;

	// Audio: the primary volume scales everything the game plays.
	if (GEngine)
	{
		if (FAudioDevice* Audio = GEngine->GetMainAudioDeviceRaw())
		{
			Audio->SetTransientPrimaryVolume(FMath::Clamp(Settings.MasterVolume, 0, 100) / 100.f);
		}
	}

	// Graphics: engine scalability (preset + per-group overrides), render scale, v-sync, frame-rate limit, dynamic resolution
	// and window mode. The packaged game and -game runs go through UGameUserSettings (saved to GameUserSettings.ini). In the
	// editor those would rewrite the editor's own settings, so PIE sets the scalability directly, and only when the player
	// asked for something other than the defaults.
	const Scalability::FQualityLevels Levels = QualityLevelsFor(Settings);
	const int32 FrameCap = BIB::FrameRateCaps[FMath::Clamp(Settings.FrameRateCap, 0, BIB::FrameRateCapCount - 1)];
	if (GEngine && !GIsEditor)
	{
		if (UGameUserSettings* User = GEngine->GetGameUserSettings())
		{
			User->SetOverallScalabilityLevel(FMath::Clamp(Settings.GraphicsQuality, 0, 3));
			User->SetViewDistanceQuality(Levels.ViewDistanceQuality);
			User->SetShadowQuality(Levels.ShadowQuality);
			User->SetGlobalIlluminationQuality(Levels.GlobalIlluminationQuality);
			User->SetReflectionQuality(Levels.ReflectionQuality);
			User->SetPostProcessingQuality(Levels.PostProcessQuality);
			User->SetTextureQuality(Levels.TextureQuality);
			User->SetVisualEffectQuality(Levels.EffectsQuality);
			User->SetAntiAliasingQuality(Levels.AntiAliasingQuality);
			User->SetFoliageQuality(Levels.FoliageQuality);
			User->SetShadingQuality(Levels.ShadingQuality);
			User->SetResolutionScaleValueEx(Levels.ResolutionQuality);
			User->SetDynamicResolutionEnabled(Settings.bDynamicResolution);
			User->SetVSyncEnabled(Settings.bVSync);
			User->SetFrameRateLimit(static_cast<float>(FrameCap));
			if (bWindowMode)
			{
				static const EWindowMode::Type Modes[BIB::WindowModeCount] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
				User->SetFullscreenMode(Modes[FMath::Clamp(Settings.WindowMode, 0, BIB::WindowModeCount - 1)]);
				User->ApplySettings(false);
			}
			else
			{
				User->ApplyNonResolutionSettings();
			}
			User->SaveSettings();
		}
	}
	else if (GIsEditor && (bGraphicsChanged || HasCustomGraphics(Settings)))
	{
		Scalability::SetQualityLevels(Levels);
		SetGameCVar(TEXT("r.DynamicRes.OperationMode"), Settings.bDynamicResolution ? 2 : 0);
		UE_LOG(LogTemp, Log, TEXT("BIB settings: graphics applied to this editor session (quality %d, render scale %d%%)."),
			Settings.GraphicsQuality, Settings.ResolutionScale);
	}

	// Boldinbeauty's own costs (BIBWeather / BIBAnimeStyle): weather effects, volumetric fog and the anime look. Cheap to set,
	// the same everywhere; volumetric fog off overrides what the shadow quality would choose.
	SetGameCVar(TEXT("bib.Gfx.WeatherDetail"), FMath::Clamp(Settings.WeatherDetail, 0, BIB::WeatherDetailCount - 1));
	SetGameCVar(TEXT("bib.Gfx.AnimeLook"), Settings.bAnimeLook ? 1 : 0);
	if (!Settings.bVolumetricFog || bGraphicsChanged || HasCustomGraphics(Settings) || !GIsEditor)
	{
		SetGameCVar(TEXT("r.VolumetricFog"), Settings.bVolumetricFog && Levels.ShadowQuality >= 2 ? 1 : 0);
	}

	// Sound when the window is in the background (Settings > Audio).
	FApp::SetUnfocusedVolumeMultiplier(Settings.bBackgroundAudio ? 1.f : 0.f);

	// Frame-rate counter ("stat fps" toggles, so only send it when the wanted state differs).
	UWorld* World = GetWorld();
	if (GEngine && World && Settings.bShowFps != bFpsShown)
	{
		GEngine->Exec(World, TEXT("stat fps"));
		bFpsShown = Settings.bShowFps;
	}

	// The operator in play picks up sensitivity, invert and field of view at once.
	if (World)
	{
		if (ABIBPlayerCharacter* Character = Cast<ABIBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
		{
			Character->ApplyViewSettings(Settings);
		}
	}
}
