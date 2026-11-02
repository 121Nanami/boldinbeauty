#include "BIBGame/BIBPlayerController.h"

#include "BIBGAS/BIBGameplayTags.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBArcadeOverlay.h"
#include "BIBGame/BIBArkStation.h"
#include "BIBGame/BIBArkUI.h"
#include "BIBGame/BIBTouch.h"
#include "BIBGame/BIBMotion.h"
#include "BIBGame/BIBMotionEvents.h"
#include "BIBGame/BIBOnlineSubsystem.h"
#include "BIBRules/BIBArk.h"
#include "BIBRules/BIBHitFeel.h"
#include "BIBRules/BIBPages.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBTuning.h"
#include "BIBUI/BIBUI.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SWidget.h"

ABIBPlayerController::ABIBPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

ABIBGameMode* ABIBPlayerController::GetBIBGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ABIBGameMode>() : nullptr;
}

ABIBPlayerCharacter* ABIBPlayerController::GetBIBCharacter() const
{
	return Cast<ABIBPlayerCharacter>(GetPawn());
}

// ============================================================================ input

FKey ABIBPlayerController::PadKey(EBIBInput Input, int32 Slot)
{
	// Gamepad layout (2026-10-13, owner OK'd gamepad support for rumble): left stick move, right stick look, X attack, A jump,
	// B dodge (Run + Parry), LB parry, Y special, RB switch, LT block, RT move 1, D-pad left / right moves 2 / 3, D-pad up
	// ultimate, D-pad down the R ultimate / special, L3 sprint, View talk, Menu pause.
	if (Slot == 1)
	{
		return Input == EBIBInput::Run || Input == EBIBInput::Parry ? EKeys::Gamepad_FaceButton_Right : EKeys::Invalid;
	}
	switch (Input)
	{
	case EBIBInput::MoveForward: return EKeys::Gamepad_LeftStick_Up;
	case EBIBInput::MoveBack: return EKeys::Gamepad_LeftStick_Down;
	case EBIBInput::MoveLeft: return EKeys::Gamepad_LeftStick_Left;
	case EBIBInput::MoveRight: return EKeys::Gamepad_LeftStick_Right;
	case EBIBInput::Jump: return EKeys::Gamepad_FaceButton_Bottom;
	case EBIBInput::Run: return EKeys::Gamepad_LeftThumbstick;
	case EBIBInput::Attack: return EKeys::Gamepad_FaceButton_Left;
	case EBIBInput::Block: return EKeys::Gamepad_LeftTrigger;
	case EBIBInput::Parry: return EKeys::Gamepad_LeftShoulder;
	case EBIBInput::Move1: return EKeys::Gamepad_RightTrigger;
	case EBIBInput::Move2: return EKeys::Gamepad_DPad_Left;
	case EBIBInput::Move3: return EKeys::Gamepad_DPad_Right;
	case EBIBInput::Special: return EKeys::Gamepad_FaceButton_Top;
	case EBIBInput::UltimateE: return EKeys::Gamepad_DPad_Up;
	case EBIBInput::UltimateR: return EKeys::Gamepad_DPad_Down;
	case EBIBInput::SwitchOperator: return EKeys::Gamepad_RightShoulder;
	case EBIBInput::Talk: return EKeys::Gamepad_Special_Left;
	case EBIBInput::Pause: return EKeys::Gamepad_Special_Right;
	default: return EKeys::Invalid;
	}
}

FKey ABIBPlayerController::DefaultKey(EBIBInput Input)
{
	switch (Input)
	{
	case EBIBInput::MoveForward: return EKeys::W;
	case EBIBInput::MoveBack: return EKeys::S;
	case EBIBInput::MoveLeft: return EKeys::A;
	case EBIBInput::MoveRight: return EKeys::D;
	case EBIBInput::Jump: return EKeys::SpaceBar;
	case EBIBInput::Run: return EKeys::LeftShift;
	case EBIBInput::Attack: return EKeys::LeftMouseButton;
	case EBIBInput::Block: return EKeys::RightMouseButton;
	case EBIBInput::Parry: return EKeys::F;
	case EBIBInput::Move1: return EKeys::One;
	case EBIBInput::Move2: return EKeys::Two;
	case EBIBInput::Move3: return EKeys::Three;
	case EBIBInput::Special: return EKeys::Four;
	case EBIBInput::UltimateE: return EKeys::E;
	case EBIBInput::UltimateR: return EKeys::R;
	case EBIBInput::SwitchOperator: return EKeys::Tab;
	case EBIBInput::Talk: return EKeys::T;
	case EBIBInput::Pause: return EKeys::P;
	default: return EKeys::Invalid;
	}
}

void ABIBPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Value bindings let us poll each action like a key, so the gameplay code works with or without assets.
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		for (const TPair<EBIBInput, TObjectPtr<UInputAction>>& Pair : ButtonActions)
		{
			if (Pair.Value)
			{
				EnhancedInput->BindActionValue(Pair.Value);
			}
		}
		if (LookAction)
		{
			EnhancedInput->BindActionValue(LookAction);
		}
	}
}

void ABIBPlayerController::UpdateInputState()
{
	const UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	const UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	const BIB::FSettings* Settings = Instance && Instance->IsProfileReady() ? &const_cast<UBIBGameInstance*>(Instance)->GetProfile().Meta.Settings : nullptr;
	for (int32 Index = 0; Index < static_cast<int32>(EBIBInput::Count); ++Index)
	{
		const EBIBInput Input = static_cast<EBIBInput>(Index);
		const TObjectPtr<UInputAction>* Action = ButtonActions.Find(Input);
		bool bNowDown = false;
		// Settings > Controls > Key bindings (owner request 2026-10-06): a rebound action reads its key directly.
		const bool bRebound = Settings && Index < BIB::BattleBindCount && !Settings->KeyBinds[Index].empty();
		if (bRebound)
		{
			const FKey Key(FName(UTF8_TO_TCHAR(Settings->KeyBinds[Index].c_str())));
			bNowDown = Key.IsValid() && IsInputKeyDown(Key);
			bPressed[Index] = Key.IsValid() && WasInputKeyJustPressed(Key);
		}
		else if (EnhancedInput && Action && *Action)
		{
			bNowDown = EnhancedInput->GetBoundActionValue(*Action).Get<bool>();
			bPressed[Index] = bNowDown && !bDown[Index];
		}
		else
		{
			const FKey Key = DefaultKey(Input);
			bNowDown = IsInputKeyDown(Key);
			bPressed[Index] = WasInputKeyJustPressed(Key);
		}
		for (int32 PadSlot = 0; PadSlot < 2; ++PadSlot)
		{
			const FKey Pad = PadKey(Input, PadSlot);
			if (Pad.IsValid())
			{
				bPressed[Index] = bPressed[Index] || WasInputKeyJustPressed(Pad);
				bNowDown = bNowDown || IsInputKeyDown(Pad);
			}
		}
		if (VirtualDown[Index] || VirtualLatch[Index])
		{
			// Touch buttons (BIBTouch): held = down; a tap that started and ended between two frames is still one press.
			bPressed[Index] = bPressed[Index] || !bDown[Index] || VirtualLatch[Index];
			bNowDown = true;
			VirtualLatch[Index] = false;
		}
		bDown[Index] = bNowDown;
	}
	if (EnhancedInput && LookAction)
	{
		LookDelta = EnhancedInput->GetBoundActionValue(LookAction).Get<FVector2D>();
	}
	else
	{
		float DeltaX = 0.f;
		float DeltaY = 0.f;
		GetInputMouseDelta(DeltaX, DeltaY);
		LookDelta = FVector2D(DeltaX, DeltaY);
	}
	{
		// Gamepad right stick: degrees per second at full tilt ([BIB.CombatFeel] PadLook*), in mouse-delta units.
		const BIB::Feel::FTuning& Feel = BIB::Feel::Tuning();
		const float DeadZone = static_cast<float>(Feel.PadDeadZone);
		float StickX = GetInputAnalogKeyState(EKeys::Gamepad_RightX);
		float StickY = GetInputAnalogKeyState(EKeys::Gamepad_RightY);
		StickX = FMath::Abs(StickX) < DeadZone ? 0.f : StickX;
		StickY = FMath::Abs(StickY) < DeadZone ? 0.f : StickY;
		if (StickX != 0.f || StickY != 0.f)
		{
			const float RealDelta = static_cast<float>(FApp::GetDeltaTime());
			const float DegreesPerUnit = BIBTuning::MouseSensitivity * 4.f;   // what HandleLookAndMove turns one mouse unit into
			LookDelta += FVector2D(StickX * static_cast<float>(Feel.PadLookYawSpeed), StickY * static_cast<float>(Feel.PadLookPitchSpeed))
				* (RealDelta / DegreesPerUnit);
		}
	}
	if (!TouchLook.IsNearlyZero())
	{
		// Drag right = turn right, drag up = look up (pixels at 1080p -> mouse-like units).
		LookDelta += FVector2D(TouchLook.X, -TouchLook.Y) * 0.12;
		TouchLook = FVector2D::ZeroVector;
	}
}

bool ABIBPlayerController::IsInputDown(EBIBInput Input) const
{
	const int32 Index = static_cast<int32>(Input);
	return Index >= 0 && Index < static_cast<int32>(EBIBInput::Count) && bDown[Index];
}

bool ABIBPlayerController::WasInputPressed(EBIBInput Input) const
{
	const int32 Index = static_cast<int32>(Input);
	return Index >= 0 && Index < static_cast<int32>(EBIBInput::Count) && bPressed[Index];
}

void ABIBPlayerController::BIBDamageEnemies(float Amount)
{
	if (ABIBPlayerCharacter* PlayerChar = GetBIBCharacter())
	{
		PlayerChar->DebugDamageEnemies(Amount);
	}
}

bool ABIBPlayerController::BIBMotion(const FString& Piece, const FString& Clip)
{
	return BIBMotion::Play(Piece, Clip, 1.f, TEXT("console"));
}

void ABIBPlayerController::BIBAngels(float Expertise)
{
	if (ABIBPlayerCharacter* PlayerChar = GetBIBCharacter())
	{
		PlayerChar->DebugSetAngelExpertise(FMath::Clamp(Expertise, 0.f, 1.f));
	}
}

void ABIBPlayerController::BIBFillUlt()
{
	if (ABIBPlayerCharacter* PlayerChar = GetBIBCharacter())
	{
		PlayerChar->DebugFillUlt();
	}
}

bool ABIBPlayerController::CheatsAllowed() const
{
	const UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (Instance && Instance->CheatsAllowed())
	{
		return true;
	}
	if (ABIBGameMode* GameMode = GetBIBGameMode())
	{
		GameMode->AddMessage(TEXT("Test hooks that change the profile are for the admin account (or -BIBDev) only."));
	}
	return false;
}

void ABIBPlayerController::BIBSetCampaignProgress(int32 Levels)
{
	if (!CheatsAllowed())
	{
		return;
	}
	if (ABIBGameMode* GameMode = GetBIBGameMode())
	{
		GameMode->GetProfile().StagesCleared = FMath::Clamp(Levels, 0, BIB::TotalCampaignLevels);
	}
}

void ABIBPlayerController::BIBTeam(const FString& First, const FString& Second, const FString& Third)
{
	ABIBGameMode* GameMode = GetBIBGameMode();
	if (!GameMode || !CheatsAllowed())
	{
		return;
	}
	BIB::FProfile& Profile = GameMode->GetProfile();
	const FString Names[3] = {First, Second, Third};
	int32 Indices[3] = {-1, -1, -1};
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		Indices[Slot] = BIB::FindOperator(TCHAR_TO_UTF8(*Names[Slot]));
		if (Indices[Slot] < 0)
		{
			GameMode->AddMessage(FString::Printf(TEXT("BIBTeam: unknown operator '%s'."), *Names[Slot]));
			return;
		}
	}
	Profile.GrantAllOperators();
	// Place slot 3 first, then 2, then 1, so that swaps never undo an earlier placement.
	for (int32 Slot = 2; Slot >= 0; --Slot)
	{
		Profile.SetTeamSlot(Slot, Indices[Slot]);
	}
	GameMode->AddMessage(FString::Printf(TEXT("Team: %s, %s, %s (start the stage to play them)."), *First, *Second, *Third));
}

void ABIBPlayerController::BIBGrantAll()
{
	if (!CheatsAllowed())
	{
		return;
	}
	if (ABIBGameMode* GameMode = GetBIBGameMode())
	{
		GameMode->GetProfile().GrantAllOperators();
		GameMode->AddMessage(TEXT("All operators owned."));
	}
}

void ABIBPlayerController::BIBUltVariant(int32 Variant)
{
	if (ABIBPlayerCharacter* PlayerChar = GetBIBCharacter())
	{
		PlayerChar->DebugSelectUltimate(Variant);
	}
}

void ABIBPlayerController::BIBTags()
{
	if (ABIBPlayerCharacter* PlayerChar = GetBIBCharacter())
	{
		const FString Tags = PlayerChar->DescribeActiveTags();
		UE_LOG(LogTemp, Display, TEXT("BIB tags: %s"), *Tags);
		if (ABIBGameMode* GameMode = GetBIBGameMode())
		{
			GameMode->AddMessage(FString::Printf(TEXT("Tags: %s"), *Tags));
		}
	}
}

void ABIBPlayerController::BIBCheckTags()
{
	const int32 Missing = BIBTags::LogMissingTags();
	if (ABIBGameMode* GameMode = GetBIBGameMode())
	{
		GameMode->AddMessage(FString::Printf(TEXT("Gameplay tags: %d missing."), Missing));
	}
}

void ABIBPlayerController::BIBSetNarratorSpeed(float Speed)
{
	if (ABIBPlayerCharacter* PlayerChar = GetBIBCharacter())
	{
		PlayerChar->DebugSetNarratorSpeed(Speed);
	}
}

// ============================================================================ lifecycle

void ABIBPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	if (DefaultMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		DamageLayer = BIBOverlay::MakeDamageLayer(this);
		Viewport->AddViewportWidgetContent(DamageLayer.ToSharedRef(), 5);
	}
	// Battle HUD (owner request 2026-10-07: only what the fight needs): BIBArkUI's, unless -BIBClassicHud asks for the old one.
	const UBIBGameInstance* HudOwner = Cast<UBIBGameInstance>(GetGameInstance());
	const bool bClassicHud = HudOwner && HudOwner->UseClassicHud();
	TSharedPtr<SBox> Host;
	RootWidget = BIBUI::BuildRoot(this, Host, bClassicHud && HudWidgetClass == nullptr);
	MenuHost = Host;
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		ArkLayer = BIBArkUI::BuildLayer(this, !bClassicHud);
		Viewport->AddViewportWidgetContent(ArkLayer.ToSharedRef(), 8);
		if (BIBTouch::Enabled())
		{
			// Mobile: joystick, camera drag and the button arc under the HUD (its buttons stay tappable above it).
			TouchLayer = BIBTouch::Build(this);
			Viewport->AddViewportWidgetContent(TouchLayer.ToSharedRef(), 7);
		}
		Viewport->AddViewportWidgetContent(RootWidget.ToSharedRef(), 10);
		// The bb-Showreel motion pack (BIBMotion.h): transitions, banners and flourishes play above everything.
		MotionLayer = BIBMotion::MakeLayer();
		BIBMotion::SetController(this);
		Viewport->AddViewportWidgetContent(MotionLayer.ToSharedRef(), 60);
	}
	if (HudWidgetClass && bClassicHud)
	{
		HudWidget = CreateWidget<UUserWidget>(this, HudWidgetClass);
		if (HudWidget)
		{
			HudWidget->AddToViewport(0);
			HudWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	// Until a profile is loaded (signed in, guest, or -BIBDev) the sign-in screen stays up; it hands over to the hub.
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (Instance)
	{
		Instance->OnProfileReady.AddUObject(this, &ABIBPlayerController::OnProfileReady);
		Instance->OnProfileCleared.AddUObject(this, &ABIBPlayerController::OnProfileCleared);
	}
	// The boot map (L_Hub) hands straight over to the Ark (the old 2D main screen was deleted on 2026-10-07), even before
	// sign-in, so the sign-in screen sits over the living Ark. A stage map loaded through level travel starts straight into
	// gameplay; a level map opened directly in the editor is simply walked.
	ABIBGameMode* GameMode = GetBIBGameMode();
	if (GameMode && GameMode->IsHubWorld() && !GameMode->IsArkWorld() && !GameMode->HasPendingStart() && GameMode->ShouldUseArk())
	{
		SetScreen(EBIBScreen::None);
		GameMode->TravelToArk();
	}
	else if (Instance && Instance->NeedsSignIn())
	{
		SetScreen(EBIBScreen::SignIn);
	}
	else if (GameMode && (GameMode->HasPendingStart() || !GameMode->IsHubWorld()))
	{
		SetScreen(EBIBScreen::None);   // in the Ark, the game mode begins the walk once the pawn exists (Tick)
	}
	else
	{
		ShowHub();
	}
}

void ABIBPlayerController::OnProfileReady()
{
	if (Screen == EBIBScreen::SignIn)
	{
		ShowHub();
	}
}

void ABIBPlayerController::OnProfileCleared()
{
	SetScreen(EBIBScreen::SignIn);
}

void ABIBPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
	{
		Instance->OnProfileReady.RemoveAll(this);
		Instance->OnProfileCleared.RemoveAll(this);
	}
	if (RootWidget.IsValid() && GetWorld())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(RootWidget.ToSharedRef());
		}
	}
	if (DamageLayer.IsValid() && GetWorld())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(DamageLayer.ToSharedRef());
		}
	}
	DamageLayer.Reset();
	if (ArkLayer.IsValid() && GetWorld())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(ArkLayer.ToSharedRef());
		}
	}
	ArkLayer.Reset();
	ScreenFocus.Reset();
	if (ScreenKeys.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(ScreenKeys);
	}
	ScreenKeys.Reset();
	BIBOverlay::CloseAll();
	RootWidget.Reset();
	MenuHost.Reset();
	Super::EndPlay(EndPlayReason);
}

// ============================================================================ feedback (owner request 2026-10-06: preferences)
void ABIBPlayerController::UpdateFeedback(float DeltaTime)
{
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	const ABIBPlayerCharacter* PlayerChar = GetBIBCharacter();
	if (!Instance || !Instance->IsProfileReady())
	{
		return;
	}
	const BIB::FSettings& Settings = Instance->GetProfile().Meta.Settings;
	// Text size: the battle HUD scales from its top-left corner.
	if (HudWidget)
	{
		const float Scale = FMath::Clamp(Settings.HudScale, 75, 125) / 100.f;
		HudWidget->SetRenderTransformPivot(FVector2D::ZeroVector);
		HudWidget->SetRenderScale(FVector2D(Scale, Scale));
	}
	// Camera shake: a heavy hit (8% of the operator's health or more) jolts the view for a quarter of a second.
	const ABIBGameMode* GameMode = GetBIBGameMode();
	if (PlayerChar && GameMode && GameMode->IsInStage() && Screen == EBIBScreen::None)
	{
		const FBIBHud& Hud = PlayerChar->GetHud();
		if (Hud.bValid)
		{
			if (Hud.ActiveName == LastActive && LastHp >= 0.f && LastHp - Hud.HP >= Hud.MaxHP * 0.08f)
			{
				AddShake(1.6f, 0.25f);
			}
			LastHp = Hud.HP;
			LastActive = Hud.ActiveName;
		}
	}
	else
	{
		LastHp = -1.f;
	}
	FRotator Target = FRotator::ZeroRotator;
	if (ShakeTime > 0.f)
	{
		ShakeTime = FMath::Max(0.f, ShakeTime - DeltaTime);
		const float Strength = ShakeStrength * (ShakeTime / FMath::Max(0.01f, ShakeDuration));
		Target = FRotator(FMath::FRandRange(-Strength, Strength), FMath::FRandRange(-Strength, Strength), 0.f);
	}
	// Directional hit kick (real time, so hit stop and slow motion do not stretch it): snaps along the hit, overshoots, settles.
	const double KickAge = FPlatformTime::Seconds() - KickStartReal;
	if (KickStrength > 0.f && KickAge >= 0.0 && KickAge < KickDuration)
	{
		const double Progress = KickAge / FMath::Max(0.01f, KickDuration);
		const double Amplitude = KickStrength * (1.0 - Progress) * (1.0 - Progress)
			* FMath::Cos(UE_DOUBLE_PI * BIB::Feel::Tuning().KickOscillations * Progress);
		Target.Pitch += KickDir.Y * Amplitude;
		Target.Yaw += KickDir.X * Amplitude;
	}
	if (!Target.Equals(ShakeOffset))
	{
		SetControlRotation(GetControlRotation() - ShakeOffset + Target);
		ShakeOffset = Target;
	}
}

void ABIBPlayerController::SetVirtualInput(EBIBInput Input, bool bInDown)
{
	const int32 Index = static_cast<int32>(Input);
	if (Index < 0 || Index >= static_cast<int32>(EBIBInput::Count))
	{
		return;
	}
	if (bInDown && !VirtualDown[Index])
	{
		VirtualLatch[Index] = true;
	}
	VirtualDown[Index] = bInDown;
}

void ABIBPlayerController::AddKick(const FVector& WorldDirection, float Degrees, float Seconds)
{
	if (Degrees <= 0.f || Seconds <= 0.f)
	{
		return;
	}
	const UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (Instance && Instance->IsProfileReady())
	{
		const BIB::FSettings& Settings = const_cast<UBIBGameInstance*>(Instance)->GetProfile().Meta.Settings;
		if (!Settings.bCameraShake || Settings.bReduceMotion)
		{
			return;
		}
	}
	// The stronger kick still playing wins.
	const double Now = FPlatformTime::Seconds();
	const double Left = KickStrength > 0.f ? FMath::Max(0.0, KickDuration - (Now - KickStartReal)) : 0.0;
	if (Degrees * Seconds < KickStrength * Left)
	{
		return;
	}
	// Sideways part of the hit turns the view a little; every hit also punches the view down (weight).
	const FVector Right = FRotator(0.0, GetControlRotation().Yaw + 90.0, 0.0).Vector();
	const float Lateral = static_cast<float>(FVector::DotProduct(WorldDirection.GetSafeNormal2D(), Right));
	KickDir = FVector2D(Lateral * 0.7f, -1.f).GetSafeNormal();
	++BIB::Feel::Stats().Kicks;
	KickStrength = Degrees;
	KickDuration = Seconds;
	KickStartReal = Now;
}

void ABIBPlayerController::AddShake(float Strength, float Seconds)
{
	const UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (Instance && Instance->IsProfileReady())
	{
		const BIB::FSettings& Settings = const_cast<UBIBGameInstance*>(Instance)->GetProfile().Meta.Settings;
		if (!Settings.bCameraShake || Settings.bReduceMotion)
		{
			return;
		}
		// Combat feel: the one slider that scales every presentation effect also scales the shake (100 % = as designed).
		Strength *= static_cast<float>(FMath::Clamp(Settings.CombatFeel, 0, 150)) / 100.f;
		if (Strength <= 0.f)
		{
			return;
		}
	}
	// The stronger jolt wins.
	if (ShakeTime <= 0.f || Strength * Seconds >= ShakeStrength * ShakeTime)
	{
		ShakeStrength = Strength;
		ShakeDuration = Seconds;
		ShakeTime = Seconds;
	}
}

// ============================================================================ UI

void ABIBPlayerController::SetStatus(const FString& Text)
{
	StatusText = Text;
}

void ABIBPlayerController::ShowResults(const TArray<FString>& Lines, bool bSuccess)
{
	if (const ABIBGameMode* GameMode = GetBIBGameMode(); GameMode && GameMode->IsInStage())
	{
		if (bSuccess)
		{
			// VICTORY with the real rewards (the result lines that name a currency).
			FString Rewards;
			for (const FString& Line : Lines)
			{
				if (Line.StartsWith(TEXT("+")))
				{
					Rewards += (Rewards.IsEmpty() ? TEXT("STAGE CLEAR // ") : TEXT(" // ")) + Line;
				}
			}
			BIBMotionEvents::Victory(Rewards.IsEmpty() ? FString(TEXT("STAGE CLEAR")) : Rewards.Left(90));
		}
		else
		{
			BIBMotionEvents::Defeat();
		}
	}
	ResultLines = Lines;
	bResultSuccess = bSuccess;
	SetScreen(EBIBScreen::Results);
}

namespace
{
	// The Pause and Results screens run in UI-only input mode, where the player controller sees no keys (P / Esc / Start did
	// nothing there, and the UMG buttons are mouse-only). While one of them is up this processor sees every key first.
	class FBIBScreenKeys : public IInputProcessor
	{
	public:
		explicit FBIBScreenKeys(ABIBPlayerController* InOwner) : Owner(InOwner) {}

		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			ABIBPlayerController* PC = Owner.Get();
			return PC && !Event.IsRepeat() && PC->HandleScreenKey(Event.GetKey());
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("BIBScreenKeys"); }

	private:
		TWeakObjectPtr<ABIBPlayerController> Owner;
	};
}

void ABIBPlayerController::UpdateScreenKeys()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	const bool bWanted = Screen == EBIBScreen::Pause || Screen == EBIBScreen::Results;
	if (bWanted && !ScreenKeys.IsValid())
	{
		ScreenKeys = MakeShared<FBIBScreenKeys>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(ScreenKeys, 0);
	}
	else if (!bWanted && ScreenKeys.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(ScreenKeys);
		ScreenKeys.Reset();
	}
}

void ABIBPlayerController::ContinueFromResults()
{
	SetStatus(FString());
	SetScreen(EBIBScreen::None);
	if (ABIBGameMode* GameMode = GetBIBGameMode())
	{
		GameMode->ReturnToHub();   // back to the Ark (travels when the stage was in another map)
		if (!GameMode->ShouldUseArk())
		{
			ShowHub();   // no Ark on this machine: explains how to build it
		}
	}
}

bool ABIBPlayerController::HandleScreenKey(const FKey& Key)
{
	const bool bBack = Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
	const bool bConfirm = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
	if (Screen == EBIBScreen::Pause)
	{
		// The pause key (P unless rebound) and the back keys resume; Enter / A press Resume, the highlighted default.
		FKey PauseKey = DefaultKey(EBIBInput::Pause);
		const UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
		if (Instance && Instance->IsProfileReady() && static_cast<int32>(EBIBInput::Pause) < BIB::BattleBindCount)
		{
			const std::string& Bound = const_cast<UBIBGameInstance*>(Instance)->GetProfile().Meta.Settings.KeyBinds[static_cast<int32>(EBIBInput::Pause)];
			if (!Bound.empty())
			{
				PauseKey = FKey(FName(UTF8_TO_TCHAR(Bound.c_str())));
			}
		}
		if (bBack || bConfirm || Key == PauseKey)
		{
			SetScreen(EBIBScreen::None);
			return true;
		}
	}
	else if (Screen == EBIBScreen::Results && (bBack || bConfirm))
	{
		ContinueFromResults();
		return true;
	}
	return false;
}

void ABIBPlayerController::SetScreen(EBIBScreen NewScreen)
{
	if (NewScreen == EBIBScreen::MainMenu)
	{
		ShowHub();   // the 2D main screen was deleted (2026-10-07): the Ark is the hub
		return;
	}
	const bool bWasPaused = Screen == EBIBScreen::Pause;
	Screen = NewScreen;
	UpdateScreenKeys();
	bMenuDirty = true;
	if (NewScreen == EBIBScreen::Pause)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
	else if (bWasPaused)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}
	ApplyInputMode();
}

void ABIBPlayerController::ApplyInputMode()
{
	if (Screen == EBIBScreen::None)
	{
		if (bPanelCursor)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			SetInputMode(InputMode);
			bShowMouseCursor = true;
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
			bShowMouseCursor = false;
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().SetAllUserFocusToGameViewport();
			}
		}
		return;
	}
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ABIBPlayerController::RebuildMenu()
{
	if (ScreenWidget)
	{
		ScreenWidget->RemoveFromParent();
		ScreenWidget = nullptr;
	}
	const TSubclassOf<UUserWidget>* WidgetClass = ScreenWidgetClasses.Find(Screen);
	const bool bUsable = WidgetClass && *WidgetClass;
	if (Screen != EBIBScreen::None && bUsable)
	{
		ScreenWidget = CreateWidget<UUserWidget>(this, *WidgetClass);
		if (ScreenWidget)
		{
			ScreenWidget->AddToViewport(20);
		}
		if (MenuHost.IsValid())
		{
			MenuHost->SetContent(SNew(SSpacer));
		}
		return;
	}
	ScreenFocus.Reset();
	if (Screen == EBIBScreen::ArkMenu || Screen == EBIBScreen::ArkPanel)
	{
		const TSharedRef<SWidget> Built = Screen == EBIBScreen::ArkMenu ? BIBArkUI::BuildMenu(this) : BIBArkUI::BuildPanel(this);
		ScreenFocus = Built;
		if (MenuHost.IsValid())
		{
			MenuHost->SetContent(Built);
		}
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(Built, EFocusCause::SetDirectly);
		}
		return;
	}
	if (MenuHost.IsValid())
	{
		MenuHost->SetContent(BIBUI::BuildScreen(this, Screen));
	}
}

void ABIBPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateInputState();

	if (bMenuDirty)
	{
		bMenuDirty = false;
		RebuildMenu();
	}

	const ABIBPlayerCharacter* PlayerChar = GetBIBCharacter();
	if (HudWidget)
	{
		const bool bShowHud = Screen == EBIBScreen::None && PlayerChar && PlayerChar->GetHud().bValid;
		HudWidget->SetVisibility(bShowHud ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	UpdateFeedback(DeltaTime);
	{
		// Entering the Ark: the pack's loading spinner loops while the travel note shows.
		const ABIBGameMode* TravelMode = GetBIBGameMode();
		const UBIBGameInstance* TravelOwner = Cast<UBIBGameInstance>(GetGameInstance());
		const bool bTravelling = TravelMode && TravelOwner && Screen == EBIBScreen::None && !TravelOwner->NeedsSignIn() && TravelMode->ShouldUseArk()
			&& !TravelMode->IsArkWorld() && TravelMode->GetFlow() == EBIBFlow::Hub && !TravelMode->HasPendingStart();
		BIBMotionEvents::Travel(bTravelling, TEXT("THE ARK"));
		// Story dialogue and Narrator's Global Exchange use the pack's panels (Set 2) with the real speaker, line and state.
		const bool bDialogue = TravelMode && TravelMode->IsDialogueOpen();
		if (bDialogue || bMotionDialogue)
		{
			BIBMotionEvents::Dialogue(bDialogue, bDialogue ? TravelMode->GetDialogueSpeaker() : FString(), bDialogue ? TravelMode->GetDialogueLine() : FString());
		}
		bMotionDialogue = bDialogue;
		const bool bExchange = PlayerChar && PlayerChar->GetHud().bExchangeOpen;
		if (bExchange && !bMotionExchange)
		{
			BIBMotionEvents::Exchange(true);
		}
		bMotionExchange = bExchange;
	}

	ABIBGameMode* GameMode = GetBIBGameMode();
	// P, Esc or the gamepad's Start pause; the pause screen takes the same keys to resume.
	if (Screen == EBIBScreen::None && GameMode && GameMode->IsInStage()
		&& (WasInputPressed(EBIBInput::Pause) || WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::Gamepad_Special_Right)))
	{
		SetScreen(EBIBScreen::Pause);
		return;
	}
	const bool bInArk = GameMode && GameMode->IsInArk();
	if (bInArk && Screen == EBIBScreen::None)
	{
		// Walking the Ark: M / P / Esc / the gamepad's Start open the menu; F or T inside a station's ring opens the station.
		if (WasInputPressed(EBIBInput::Pause) || WasInputKeyJustPressed(EKeys::M) || WasInputKeyJustPressed(EKeys::Escape)
			|| WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
		{
			OpenArkMenu();
			return;
		}
		if (WasInputPressed(EBIBInput::Parry) || WasInputPressed(EBIBInput::Talk))
		{
			if (const ABIBArkStation* Station = GameMode->GetStationInReach())
			{
				ArkOpenStation(Station->GetStationId());
				return;
			}
		}
	}

	// Narrator's Global Exchange panel needs a mouse cursor while gameplay continues; in the Ark, holding Alt shows it (to click
	// the menu button).
	const bool bWantCursor = Screen == EBIBScreen::None && PlayerChar
		&& (PlayerChar->GetHud().bExchangeOpen || (bInArk && (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))));
	if (bWantCursor != bPanelCursor)
	{
		bPanelCursor = bWantCursor;
		ApplyInputMode();
	}
}

// ============================================================================ the Ark (owner request 2026-10-07)

namespace
{
	FString ArkF(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	std::string ArkS(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}
}

void ABIBPlayerController::ShowHub()
{
	// The Ark is the only hub (owner request 2026-10-07: the 2D lobby / main screen was deleted).
	ABIBGameMode* GameMode = GetBIBGameMode();
	if (GameMode && GameMode->IsArkWorld())
	{
		SetScreen(EBIBScreen::None);
		if (GetPawn())
		{
			GameMode->BeginArk();   // a (new) sign-in in the Ark: this profile's team, at this profile's saved position
		}
		return;
	}
	if (GameMode && GameMode->ShouldUseArk())
	{
		if (!GameMode->HasPendingStart())
		{
			SetScreen(EBIBScreen::None);
			GameMode->TravelToArk();
		}
		return;
	}
	// L_Ark has not been generated on this machine: there is no other hub any more, so say how to build it.
	ShowResults({TEXT("The Ark (L_Ark) is not built on this machine."), TEXT("Run Demo/BoldInBeautyDemo/Tools/build_greybox.sh, then start the game again.")}, false);
}

void ABIBPlayerController::ArkToast(const FString& Text)
{
	if (!Text.IsEmpty())
	{
		ArkUi.Toast = Text;
		ArkUi.ToastAt = FPlatformTime::Seconds();
		// The notice-toast piece slides the newest messages in (up to three).
		RecentToasts.Add(Text);
		if (RecentToasts.Num() > 3)
		{
			RecentToasts.RemoveAt(0);
		}
		BIBMotionEvents::ArkToasts(RecentToasts);
	}
}

void ABIBPlayerController::OpenArkMenu()
{
	const ABIBGameMode* GameMode = GetBIBGameMode();
	if (!GameMode || !GameMode->IsInArk())
	{
		return;
	}
	// Reopen where the player was, but start fresh when the tutorial wants a route.
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (Instance)
	{
		const std::vector<std::string> Route = BIB::Ark::TutorialPath(Instance->GetProfile());
		if (!Route.empty())
		{
			ArkUi.MenuPath.Reset();
		}
	}
	ArkUi.MenuOpenedAt = FPlatformTime::Seconds();
	if (Screen != EBIBScreen::ArkMenu)
	{
		ArkUi.MenuColumnsShown = 0;   // every column animates in on a fresh open
		BIBMotionEvents::MenuOpened();
	}
	SetScreen(EBIBScreen::ArkMenu);
}

void ABIBPlayerController::CloseArkScreens()
{
	if (Screen == EBIBScreen::ArkMenu || Screen == EBIBScreen::ArkPanel)
	{
		BIBMotionEvents::ClosePressed();
		SetScreen(EBIBScreen::None);
	}
}

void ABIBPlayerController::ArkMenuUp()
{
	if (ArkUi.MenuPath.Num() > 0)
	{
		ArkUi.MenuPath.Pop();
		MarkMenuDirty();
		return;
	}
	CloseArkScreens();
}

void ABIBPlayerController::ArkMenuClick(const FString& NodeId)
{
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (!Instance)
	{
		return;
	}
	const std::string Id = ArkS(NodeId);
	if (Id == BIB::Ark::RootId)
	{
		ArkUi.MenuPath.Reset();
		MarkMenuDirty();
		return;
	}
	const BIB::Ark::FNode Node = BIB::Ark::Node(Instance->GetProfile(), Instance->UiState, Id);
	if (Node.Id != Id)
	{
		return;
	}
	switch (Node.Kind)
	{
	case BIB::Ark::ENodeKind::Branch:
	{
		ArkUi.MenuPath.Reset();
		const std::vector<std::string> Path = BIB::Ark::PathTo(Id);
		for (size_t Index = 1; Index < Path.size(); ++Index)
		{
			ArkUi.MenuPath.Add(ArkF(Path[Index]));
		}
		MarkMenuDirty();
		break;
	}
	case BIB::Ark::ENodeKind::Teleport:
		if (!ArkTeleport(ArkF(Node.Station)))
		{
			// Not built in this Ark (an old map): open the place's counters right here instead.
			ArkOpenStation(ArkF(Node.Station));
		}
		break;
	case BIB::Ark::ENodeKind::Page:
		ArkOpenPage(Node.Tab, ArkF(Node.Page), ArkF(Node.Label), FLinearColor(Node.Colour[0], Node.Colour[1], Node.Colour[2]), ArkF(Node.Icon));
		break;
	case BIB::Ark::ENodeKind::Action:
		ArkRunAction(ArkF(Node.Action));
		break;
	}
}

bool ABIBPlayerController::ArkTeleport(const FString& StationId)
{
	ABIBGameMode* GameMode = GetBIBGameMode();
	FString Error;
	if (!GameMode || !GameMode->TeleportToStation(StationId, Error))
	{
		ArkToast(Error);
		return false;
	}
	if (const BIB::Ark::FStation* Station = BIB::Ark::FindStation(ArkS(StationId)))
	{
		ArkUi.ArrivalName = ArkF(Station->Name);
		ArkUi.ArrivalSub = Station->bUnmarked ? FString(TEXT("Press F to look inside")) : ArkF(Station->Blurb) + TEXT("  -  press F");
		ArkUi.ArrivalColour = FLinearColor(Station->Colour[0], Station->Colour[1], Station->Colour[2]);
		ArkUi.ArrivalAt = FPlatformTime::Seconds();
	}
	SetScreen(EBIBScreen::None);
	return true;
}

bool ABIBPlayerController::ArkOpenStation(const FString& StationId, int32 Counter)
{
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	const BIB::Ark::FStation* Station = BIB::Ark::FindStation(ArkS(StationId));
	if (!Instance || !Station || Station->Counters.empty())
	{
		return false;
	}
	int32 Wanted = Counter;
	if (Wanted < 0)
	{
		// Reopen the tab last used here (not while the tutorial walks a route: it names the counter it wants).
		const int32* Last = ArkUi.LastCounter.Find(StationId);
		Wanted = Last && BIB::Ark::TutorialPath(Instance->GetProfile()).empty() ? *Last : 0;
	}
	const int32 Index = FMath::Clamp(Wanted, 0, static_cast<int32>(Station->Counters.size()) - 1);
	const BIB::Ark::FCounter& Entry = Station->Counters[static_cast<size_t>(Index)];
	const BIB::Pages::FPerform Done = BIB::Ark::OpenCounter(Instance->GetProfile(), Instance->UiState, Entry);
	if (!Done.bOk)
	{
		ArkToast(ArkF(Done.Message));   // e.g. the tutorial asks for something else first
		return false;
	}
	Instance->UiState.SummonOps.clear();
	ArkUi.PanelStation = StationId;
	ArkUi.PanelCounter = Index;
	ArkUi.LastCounter.Add(StationId, Index);
	ArkUi.PanelTitle = ArkF(Station->Name);
	ArkUi.PanelIcon = ArkF(Station->Icon);
	ArkUi.PanelColour = FLinearColor(Station->Colour[0], Station->Colour[1], Station->Colour[2]);
	ArkUi.RootTab = Entry.Tab;
	ArkUi.RootPage = ArkF(Entry.Page);
	ArkUi.PanelOpenedAt = FPlatformTime::Seconds();
	Instance->ArkStation = StationId;
	Instance->SaveNow();
	BIBMotionEvents::StationOpened(ArkF(Station->Name), TEXT("THE ARK // ") + ArkF(Station->Blurb), StationId);
	SetScreen(EBIBScreen::ArkPanel);
	return true;
}

bool ABIBPlayerController::ArkOpenPage(int32 Tab, const FString& Page, const FString& Title, const FLinearColor& Colour, const FString& Icon)
{
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (!Instance)
	{
		return false;
	}
	BIB::Ark::FCounter Entry;
	Entry.Tab = Tab;
	Entry.Page = ArkS(Page);
	const BIB::Pages::FPerform Done = BIB::Ark::OpenCounter(Instance->GetProfile(), Instance->UiState, Entry);
	if (!Done.bOk)
	{
		ArkToast(ArkF(Done.Message));
		return false;
	}
	ArkUi.PanelStation.Reset();
	ArkUi.PanelCounter = 0;
	ArkUi.PanelTitle = Title;
	ArkUi.PanelIcon = Icon;
	ArkUi.PanelColour = Colour;
	ArkUi.RootTab = Tab;
	ArkUi.RootPage = Page;
	ArkUi.PanelOpenedAt = FPlatformTime::Seconds();
	BIBMotionEvents::PageOpened(Title);
	SetScreen(EBIBScreen::ArkPanel);
	return true;
}

void ABIBPlayerController::ArkPanelBack()
{
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (!Instance)
	{
		CloseArkScreens();
		return;
	}
	BIB::Pages::FUiState& State = Instance->UiState;
	const bool bResults = State.Tab == BIB::Pages::TabSummon && !State.SummonOps.empty();
	if (!bResults && State.Tab == ArkUi.RootTab && State.Page == ArkS(ArkUi.RootPage))
	{
		CloseArkScreens();
		return;
	}
	BIBMotionEvents::BackPressed();
	ArkRunAction(TEXT("back"));
	// The old tab roots of Lobby, Shop and Revolution are gone (their entries live in the menu and the Ark): close instead.
	const bool bNoRoot = State.Page.empty() && (State.Tab == BIB::Pages::TabLobby || State.Tab == BIB::Pages::TabShop || State.Tab == BIB::Pages::TabRevolution);
	if (bNoRoot && !(State.Tab == ArkUi.RootTab && ArkUi.RootPage.IsEmpty()))
	{
		CloseArkScreens();
	}
}

void ABIBPlayerController::ArkRunAction(const FString& ActionId)
{
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	ABIBGameMode* GameMode = GetBIBGameMode();
	if (!Instance)
	{
		return;
	}
	const BIBMotionEvents::FSnapshot Before = BIBMotionEvents::Snap(Instance->GetProfile(), Instance->UiState);
	const BIB::Pages::FPerform Done = BIB::Pages::Perform(Instance->GetProfile(), Instance->UiState, ArkS(ActionId));
	Instance->SaveNow(); // a summon, a level-up, a purchase: kept on disk straight away
	if (Done.bOk)
	{
		BIBMotionEvents::AfterAction(Before, Instance->GetProfile(), Instance->UiState, ActionId, ArkF(Done.Message));
	}
	if (ActionId == TEXT("set:autodetect"))
	{
		Instance->AutoDetectGraphics();   // the engine benchmark lives here, not in the rules core
	}
	else if (ActionId.StartsWith(TEXT("set:")) || ActionId.StartsWith(TEXT("pref:")))
	{
		static const TCHAR* const GraphicsActions[] = {TEXT("set:gfx"), TEXT("set:scale"), TEXT("set:grp"), TEXT("set:weather"), TEXT("set:volfog"),
			TEXT("set:anime"), TEXT("set:dynres"), TEXT("set:perf"), TEXT("set:defaults")};
		bool bGraphics = false;
		for (const TCHAR* Prefix : GraphicsActions)
		{
			bGraphics |= ActionId.StartsWith(Prefix);
		}
		Instance->ApplyUserSettings(ActionId.StartsWith(TEXT("set:window")) || ActionId == TEXT("set:defaults"), bGraphics);
	}
	if (Done.bSignOut)
	{
		if (GameMode)
		{
			GameMode->SaveArkPosition(true);   // the next sign-in with this account starts here
		}
		Instance->SignOut(); // OnProfileCleared shows the sign-in screen when the sign-out completes
		return;
	}
	if (!Done.Calls.empty())
	{
		if (UBIBOnlineSubsystem* Online = Instance->GetSubsystem<UBIBOnlineSubsystem>())
		{
			Online->Send(Done.Calls);
		}
	}
	TWeakObjectPtr<ABIBPlayerController> WeakThis(this);
	if (!Done.Prompt.Key.empty())
	{
		const FString Key = ArkF(Done.Prompt.Key);
		BIBOverlay::OpenPrompt(GetWorld(), ArkF(Done.Prompt.Title), ArkF(Done.Prompt.Hint), ArkF(Done.Prompt.Initial), Done.Prompt.MaxLength,
			[WeakThis, Key](bool bOk, const FString& Text)
			{
				ABIBPlayerController* Self = WeakThis.Get();
				UBIBGameInstance* Owner = Self ? Cast<UBIBGameInstance>(Self->GetGameInstance()) : nullptr;
				if (bOk && Owner)
				{
					Owner->UiState.InputText = ArkS(Text);
					Self->ArkRunAction(TEXT("in:") + Key);
				}
			});
	}
	if (Done.CaptureKey >= 0)
	{
		const int32 Index = Done.CaptureKey;
		BIBOverlay::OpenKeyCapture(GetWorld(), FString::Printf(TEXT("%s - press the new key or mouse button (Esc cancels)."), UTF8_TO_TCHAR(BIB::KeyBindName(Index))),
			[WeakThis, Index](const FString& KeyName)
			{
				ABIBPlayerController* Self = WeakThis.Get();
				UBIBGameInstance* Owner = Self ? Cast<UBIBGameInstance>(Self->GetGameInstance()) : nullptr;
				if (Owner)
				{
					Owner->UiState.InputText = ArkS(KeyName);
					Self->ArkRunAction(FString::Printf(TEXT("keyset:%d"), Index));
				}
			});
	}
	if (Done.Overlay != BIB::Pages::EOverlay::None)
	{
		EBIBOverlayKind Kind = EBIBOverlayKind::ArenaBattle;
		switch (Done.Overlay)
		{
		case BIB::Pages::EOverlay::NightShift: Kind = EBIBOverlayKind::NightShift; BIBMotionEvents::ArcadeStart(0); break;
		case BIB::Pages::EOverlay::Rhythm: Kind = EBIBOverlayKind::Rhythm; BIBMotionEvents::ArcadeStart(1); break;
		case BIB::Pages::EOverlay::HaloFall: Kind = EBIBOverlayKind::HaloFall; break;
		case BIB::Pages::EOverlay::NeonMemory: Kind = EBIBOverlayKind::NeonMemory; break;
		case BIB::Pages::EOverlay::FirewallBreach: Kind = EBIBOverlayKind::FirewallBreach; break;
		case BIB::Pages::EOverlay::ArkRally: Kind = EBIBOverlayKind::ArkRally; break;
		case BIB::Pages::EOverlay::UltimateNight: Kind = EBIBOverlayKind::UltimateNight; BIBMotionEvents::ArcadeStart(0); break;
		default: break;
		}
		BIBOverlay::OpenGame(GetWorld(), Instance, Kind, Done.OverlayA, Done.OverlayB, [WeakThis](const FString& Result)
		{
			ABIBPlayerController* Self = WeakThis.Get();
			UBIBGameInstance* Owner = Self ? Cast<UBIBGameInstance>(Self->GetGameInstance()) : nullptr;
			if (Owner)
			{
				if (!Result.IsEmpty())
				{
					Owner->UiState.Status = ArkS(Result);
				}
				Owner->SaveNow();
				Self->MarkMenuDirty();
			}
		});
	}
	if (!Done.FreeRoamMap.empty())
	{
		FString Error;
		if (!GameMode || !GameMode->StartFreeRoam(ArkF(Done.FreeRoamMap), Error))
		{
			Instance->UiState.Status = ArkS(Error);
			ArkToast(Error);
		}
		else
		{
			SetScreen(EBIBScreen::None);
			return; // the level load ends the Ark screens
		}
	}
	if (Done.bStartStage)
	{
		FString Error;
		if (!GameMode || !GameMode->StartStage(Done.StageId, Error))
		{
			Instance->UiState.Status = ArkS(Error);
		}
		else
		{
			SetScreen(EBIBScreen::None);
			return;
		}
	}
	if (GameMode && GameMode->IsInArk())
	{
		GameMode->RefreshArkTeam();
		// A summon lights the factory's ring in the colour of the best pull (3* blue, 4* violet, 5* gold, 6* red).
		if ((ActionId.StartsWith(TEXT("sum:go")) || ActionId.StartsWith(TEXT("sum:tut")) || ActionId == TEXT("sum:again")) && !Instance->UiState.SummonStars.empty())
		{
			int32 Best = 3;
			for (const int Stars : Instance->UiState.SummonStars)
			{
				Best = FMath::Max(Best, Stars);
			}
			static const FLinearColor Colours[4] = {FLinearColor(0.3f, 0.6f, 1.f), FLinearColor(0.75f, 0.35f, 1.f), FLinearColor(1.f, 0.8f, 0.2f), FLinearColor(1.f, 0.2f, 0.35f)};
			if (ABIBArkStation* Factory = GameMode->FindStation(TEXT("factory")))
			{
				Factory->Pulse(Colours[FMath::Clamp(Best - 3, 0, 3)] * 3.f, 3.f);
			}
		}
	}
	MarkMenuDirty();
}
