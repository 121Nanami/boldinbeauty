#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBVisuals.h"

#include "BIBGAS/BIBAbilitySystemComponent.h"
#include "BIBGAS/BIBAttributeSet.h"
#include "BIBGAS/BIBGameplayTags.h"

#include "BIBGame/BIBBlock.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBGame/BIBTuning.h"
#include "BIBRules/BIBConfig.h"
#include "BIBRules/BIBCore.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "BIBGame/BIBInput.h"
#include "BIBGame/BIBFx.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBMotionEvents.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABIBPlayerCharacter::ABIBPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0, 720.0, 0.0);
	GetCharacterMovement()->MaxWalkSpeed = BIBTuning::WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = BIBTuning::BaseJumpZ;
	GetCharacterMovement()->AirControl = 0.45f;
	// Snappier, lower jumps (owner 2026-10-09: "jump height feels way to high"): heavier gravity, faster fall.
	GetCharacterMovement()->GravityScale = BIBTuning::PlayerGravityScale;
	GetCharacterMovement()->BrakingDecelerationWalking = 2600.f;
	GetCharacterMovement()->MaxAcceleration = 3200.f;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanSwim = true;
	for (float& At : BufferedAt)
	{
		At = -100.f;
	}

	AbilitySystem = CreateDefaultSubobject<UBIBAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UBIBAttributeSet>(TEXT("Attributes"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 650.f;
	CameraBoom->SocketOffset = FVector(0.0, 60.0, 120.0);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	BodyComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyComp->SetupAttachment(GetCapsuleComponent());
	BodyComp->SetStaticMesh(CylinderFinder.Object);
	BodyComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyComp->SetRelativeScale3D(FVector(0.8, 0.8, 1.35));
	BodyComp->SetRelativeLocation(FVector(0.0, 0.0, -20.0));

	HeadComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	HeadComp->SetupAttachment(GetCapsuleComponent());
	HeadComp->SetStaticMesh(SphereFinder.Object);
	HeadComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadComp->SetRelativeScale3D(FVector(0.55, 0.55, 0.55));
	HeadComp->SetRelativeLocation(FVector(25.0, 0.0, 70.0)); // offset forward so facing is readable
}

// ============================================================================ helpers

float ABIBPlayerCharacter::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

ABIBPlayerController* ABIBPlayerCharacter::GetPC() const
{
	return Cast<ABIBPlayerController>(GetController());
}

ABIBGameMode* ABIBPlayerCharacter::GetMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ABIBGameMode>() : nullptr;
}

bool ABIBPlayerCharacter::Pressed(EBIBInput Input) const
{
	if (bUltCinematic || InputSwallowFrame == GFrameCounter)
	{
		return false;   // the ultimate's cinematic beat (and the press that skipped it)
	}
	const ABIBPlayerController* PC = GetPC();
	if (PC && PC->WasInputPressed(Input))
	{
		return true;
	}
	// Input buffer (BIBPlayerFeel.cpp): a press up to InputBufferSeconds ago ([BIB.CombatFeel], 0.18 s) still counts until
	// something consumes it.
	const int32 Index = static_cast<int32>(Input);
	return Index >= 0 && Index < static_cast<int32>(EBIBInput::Count)
		&& Now() - BufferedAt[Index] <= static_cast<float>(BIB::Feel::Tuning().InputBufferSeconds);
}

bool ABIBPlayerCharacter::Held(EBIBInput Input) const
{
	const ABIBPlayerController* PC = GetPC();
	return PC && PC->IsInputDown(Input);
}

void ABIBPlayerCharacter::NotifyOperatorChanged()
{
	OnActiveOperatorChanged(Active().Name, ActiveIndex);
}

void ABIBPlayerCharacter::DebugDamageEnemies(float Amount)
{
	if (ABIBGameMode* GameMode = GetMode())
	{
		for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			GainUlt(ActiveIndex, Enemy->ReceiveDamage(Amount, this));
		}
	}
	Message(FString::Printf(TEXT("Debug: %d damage to every enemy."), FMath::RoundToInt(Amount)));
}

void ABIBPlayerCharacter::DebugFillUlt()
{
	SetMemberUlt(ActiveIndex, 100.f);
	Message(TEXT("Debug: ultimate filled."));
}

void ABIBPlayerCharacter::Message(const FString& Text) const
{
	if (ABIBGameMode* GameMode = GetMode())
	{
		GameMode->AddMessage(Text);
	}
}

int32 ABIBPlayerCharacter::FindMember(EBIBKitType Kit) const
{
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Members[Index].Kit == Kit)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void ABIBPlayerCharacter::StartAction(EBIBAction NewAction)
{
	Action = NewAction;
	ActionTime = 0.f;
	DamageTakenThisAction = 0.f;
	JudgeData.GavelSmashesDone = 0;
	JudgeData.bActionEffectDone = false;
	ActionDirection = AimForward();
	FeelOnActionStart(NewAction);   // windup telegraph (presentation only)
}

void ABIBPlayerCharacter::EndAction()
{
	Action = EBIBAction::None;
	ActionTime = 0.f;
}

void ABIBPlayerCharacter::CancelAction()
{
	if (LunaData.HorizonVisual.IsValid())
	{
		LunaData.HorizonVisual->Destroy();
	}
	if (LunaData.ExpelVisual.IsValid())
	{
		LunaData.ExpelVisual->Destroy();
	}
	EndAction();
}

bool ABIBPlayerCharacter::IsMovementLocked() const
{
	switch (Action)
	{
	case EBIBAction::LunaBlackhole:
	case EBIBAction::LunaEventHorizon:
	case EBIBAction::LunaCutscene:
	case EBIBAction::JudgePaperwork:
	case EBIBAction::JudgeCourtWindup:
	case EBIBAction::JudgeGavel:
	case EBIBAction::JudgeRetribution:
	case EBIBAction::JudgeJustice:
	case EBIBAction::NarratorSwitch:
	case EBIBAction::DianoAcceleration:
	case EBIBAction::Dodge:
		return true;
	default:
		break;
	}
	// Data kits: timed dashes / blinks, locks requested by the runtime, or a running cast with a movement lock.
	if (KitMoveLeft > 0.f || Now() < KitLockedUntil)
	{
		return true;
	}
	const BIB::Kits::FRuntime* Runtime = Active().Runtime.Get();
	return Runtime && Runtime->MovementLocked();
}

FVector ABIBPlayerCharacter::AimForward() const
{
	const ABIBPlayerController* PC = GetPC();
	const double Yaw = PC ? PC->GetControlRotation().Yaw : GetActorRotation().Yaw;
	return FRotator(0.0, Yaw, 0.0).Vector();
}

void ABIBPlayerCharacter::FaceAim()
{
	SetActorRotation(AimForward().Rotation());
}

int32 ABIBPlayerCharacter::GetActiveRosterIndex() const
{
	return ActiveIndex >= 0 && ActiveIndex < UE_ARRAY_COUNT(Members) ? Members[ActiveIndex].RosterIndex : -1;
}

bool ABIBPlayerCharacter::IsJudgeActive() const
{
	return bCombatActive && Active().Kit == EBIBKitType::Judge && Active().bAlive;
}

// ============================================================================ team setup

void ABIBPlayerCharacter::SetupTeam(const BIB::FProfile& Profile, const int32* TeamOverride)
{
	const std::vector<BIB::FRosterEntry>& Roster = BIB::Roster();
	// A new stage starts clean: no cooldowns, no timed states, abilities are granted again below.
	ClearAllMemberCooldowns();
	AbilitySystem->ClearAllAbilities();
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		FBIBMember& Member = Members[Slot];
		Member = FBIBMember();
		const int32 RosterIndex = TeamOverride ? TeamOverride[Slot] : Profile.Team[Slot];
		if (RosterIndex < 0 || RosterIndex >= static_cast<int32>(Roster.size()))
		{
			continue;
		}
		const BIB::FRosterEntry& Entry = Roster[static_cast<size_t>(RosterIndex)];
		const BIB::FCombatStats Stats = Profile.GetCombatStats(RosterIndex);
		Member.RosterIndex = RosterIndex;
		Member.Name = UTF8_TO_TCHAR(Entry.Name);
		switch (Entry.Kit)
		{
		case BIB::EKit::Luna: Member.Kit = EBIBKitType::Luna; Member.KitName = TEXT("Luna"); break;
		case BIB::EKit::Judge: Member.Kit = EBIBKitType::Judge; Member.KitName = TEXT("Judge"); break;
		case BIB::EKit::Narrator: Member.Kit = EBIBKitType::Narrator; Member.KitName = TEXT("Narrator"); break;
		case BIB::EKit::Placeholder: Member.Kit = EBIBKitType::Placeholder; Member.KitName = TEXT("Common"); break;
		default:
			Member.KitDef = BIB::Kits::FindKit(static_cast<int>(Entry.Kit));
			Member.Kit = Member.KitDef ? EBIBKitType::Data : EBIBKitType::None;
			if (Member.KitDef)
			{
				Member.KitName = UTF8_TO_TCHAR(Member.KitDef->Name.c_str());
			}
			break;
		}
		CacheMemberTags(Member);
		Member.BaseMaxHP = static_cast<float>(Stats.MaxHP);
		Member.MaxHP = Member.BaseMaxHP;
		Member.HP = Member.MaxHP;
		Member.BaseAttack = static_cast<float>(Entry.BaseAttack);
		Member.DamageMult = static_cast<float>(Stats.DamageMult);
		Member.DefencePercent = static_cast<float>(Stats.DefencePercent);
		Member.bAlive = Member.Kit != EBIBKitType::None && Member.MaxHP > 0.f;
	}
	ActiveIndex = 0;
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		if (Members[Slot].bAlive)
		{
			ActiveIndex = Slot;
			break;
		}
	}
	ClearKitEffects();
	LunaData = FBIBLunaState();
	JudgeData = FBIBJudgeState();
	NarratorData = FBIBNarratorState();
	NarratorData.Speed = BIBTuning::NarratorStartSpeed;
	// Gameplay Ability System: every operator slot becomes a GameplayAbility, data kits get their runtime, and the
	// active operator's health / ultimate move into the attribute set.
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		GrantMemberAbilities(Slot);
		if (Members[Slot].KitDef)
		{
			SetupDataKit(Members[Slot], Slot);
		}
	}
	ShareTeammatePassives();
	KitMoveLeft = 0.f;
	KitLockedUntil = 0.f;
	PendingKills.Reset();
	LoadMemberIntoAttributes(ActiveIndex);
	// Team page options: Global Exchange starts on the chosen stats; Preferences: mouse sensitivity.
	NarratorData.ExchangeFrom = FMath::Clamp(Profile.Meta.NarratorExchangeFrom, 0, static_cast<int32>(EBIBStat::Count) - 1);
	NarratorData.ExchangeTo = FMath::Clamp(Profile.Meta.NarratorExchangeTo, 0, static_cast<int32>(EBIBStat::Count) - 1);
	ApplyViewSettings(Profile.Meta.Settings);
	Action = EBIBAction::None;
	SwitchReadyAt = 0.f;
	Combo.Reset();
	ComboStepNow = -1;
	ChainUntil = -1.f;
	DodgeCounterUntil = -1.f;
	ClearBuffers();
	StopClimb(false);
	if (UBIBGameInstance* Game = GetGameInstance<UBIBGameInstance>())
	{
		Game->GetAngelMind().NewStage();   // the angels remember part of what they learned last stage
	}
	UpdateMovementStats();
	UpdateVisuals();
	BuildHud();
	NotifyOperatorChanged();
}

void ABIBPlayerCharacter::SetCombatActive(bool bActive)
{
	bCombatActive = bActive;
	if (bActive)
	{
		bWalkOnly = false;
	}
	if (!bActive)
	{
		ClearKitEffects();
		EndAction();
		bBlocking = false;
		AbilitySystem->ClearMirroredTags(TEXT("State"));
	}
}

void ABIBPlayerCharacter::ClearKitEffects()
{
	CancelAction();
	if (LunaData.OrbVisual.IsValid())
	{
		LunaData.OrbVisual->Destroy();
	}
	LunaData.OrbPhase = 0;
	LunaClearOrbits();
	if (LunaData.DestabiliseVisual.IsValid())
	{
		LunaData.DestabiliseVisual->Destroy();
	}
	LunaData.DestabiliseStart = -1.f;
	if (ABIBGameMode* GameMode = GetMode())
	{
		for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			Enemy->SetGravityInverted(false);
		}
	}
	LunaData.bGravityInverted = false;
	CloseExchange();
	JudgeData.bMenuOpen = false;
	for (FBIBMember& Member : Members)
	{
		if (Member.Runtime)
		{
			Member.Runtime->CancelAll();
			Member.Runtime->ClearBattlefield();
		}
	}
	for (bool& bHeld : bKitHeld)
	{
		bHeld = false;
	}
	KitMoveLeft = 0.f;
}

// ============================================================================ tick

void ABIBPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FeelTickStart(DeltaSeconds);
	if (!bCombatActive)
	{
		if (bWalkOnly && Active().bAlive)
		{
			HandleLookAndMove();
			UpdateMovementStats();
			TraversalTick(DeltaSeconds);
		}
		UpdatePose(DeltaSeconds);
		UpdateCameraFeel();
		BuildHud();
		return;
	}
	if (Active().bAlive)
	{
		ActionTime += DeltaSeconds;
		if (!bUltCinematic)
		{
			// During the ultimate's cinematic beat the operator takes no input (any attack / ultimate / dodge key skips it).
			HandleLookAndMove();
			HandleCommonInput();
			HandleKitInput();
		}
		UpdateAction(DeltaSeconds);
		DataKitTick(DeltaSeconds);
		FeelTick(DeltaSeconds);
	}
	// Effects that keep running regardless of who is active.
	LunaTick(DeltaSeconds);
	JudgeTick(DeltaSeconds);
	NarratorTick(DeltaSeconds);
	UpdateMovementStats();
	UpdateVisuals();
	UpdatePose(DeltaSeconds);
	UpdateCameraFeel();
	MirrorKitAttributes();
	UpdateStateTags();
	BuildHud();
}

void ABIBPlayerCharacter::ApplyViewSettings(const BIB::FSettings& Settings)
{
	LookSensitivityScale = static_cast<float>(FMath::Clamp(Settings.MouseSensitivity, 1, 10)) / 5.f;
	LookPitchSign = Settings.bInvertY ? -1.f : 1.f;
	FeelApplySettings(Settings);
	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(static_cast<float>(FMath::Clamp(Settings.FieldOfView, 70, 110)));
	}
}

void ABIBPlayerCharacter::HandleLookAndMove()
{
	ABIBPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	if (!NarratorData.bExchangeOpen)
	{
		const FVector2D LookDelta = PC->GetLookDelta();
		FRotator Look = PC->GetControlRotation();
		const double Sensitivity = BIBTuning::MouseSensitivity * LookSensitivityScale * 4.0;
		Look.Yaw += LookDelta.X * Sensitivity;
		Look.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch + LookDelta.Y * Sensitivity * LookPitchSign), -70.0, 45.0);
		Look.Roll = 0.0;
		PC->SetControlRotation(Look);
	}

	bBlocking = Held(EBIBInput::Block) && Action == EBIBAction::None && !bClimbing;
	if (IsMovementLocked() || bClimbing || Now() < LandedLockUntil)
	{
		return;
	}
	const FRotator YawRotation(0.0, PC->GetControlRotation().Yaw, 0.0);
	const FVector Forward = YawRotation.Vector();
	const FVector Right = FRotator(0.0, YawRotation.Yaw + 90.0, 0.0).Vector();
	float Axis = (Held(EBIBInput::MoveForward) ? 1.f : 0.f) - (Held(EBIBInput::MoveBack) ? 1.f : 0.f) + static_cast<float>(TouchMove.Y);
	AddMovementInput(Forward, FMath::Clamp(Axis, -1.f, 1.f));
	Axis = (Held(EBIBInput::MoveRight) ? 1.f : 0.f) - (Held(EBIBInput::MoveLeft) ? 1.f : 0.f) + static_cast<float>(TouchMove.X);
	AddMovementInput(Right, FMath::Clamp(Axis, -1.f, 1.f));
	if (Active().Kit == EBIBKitType::Narrator)
	{
		// Above the base speed Narrator is pushed along the way she faces at (Speed - base speed); input steers and brakes.
		UpdateMovementStats();
		const float Forced = NarratorForcedSpeed();
		if (Forced > 0.f)
		{
			const float MaxSpeed = FMath::Max(GetCharacterMovement()->MaxWalkSpeed, 1.f);
			AddMovementInput(GetActorForwardVector(), FMath::Clamp(Forced / MaxSpeed, 0.f, 1.f));
		}
	}

	if (Pressed(EBIBInput::Jump))
	{
		bool bCanJump = true;
		if (Active().Kit == EBIBKitType::Narrator)
		{
			// Narrator jumps from 100 speed; Diano needs 5x each requirement.
			bCanJump = NarratorData.Speed >= BIBTuning::SpeedJumpThreshold * (NarratorData.bDiano ? 5.f : 1.f);
			if (!bCanJump)
			{
				Message(TEXT("Narrator needs more speed to jump."));
			}
		}
		if (bCanJump && !GetCharacterMovement()->IsSwimming())
		{
			Jump();
			ConsumeBuffer(EBIBInput::Jump);
			Accent();
		}
	}
}

void ABIBPlayerCharacter::HandleCommonInput()
{
	// Switch operator (Tab) — owner choice for the demo.
	if (Pressed(EBIBInput::SwitchOperator))
	{
		// A stunned angel opens a chain attack: the switch ignores its cooldown and the incoming operator drops in on it.
		const bool bChain = ChainUntil > Now() && ChainTarget.IsValid() && ChainTarget->IsAlive();
		for (int32 Step = 1; Step < 3; ++Step)
		{
			const int32 Candidate = (ActiveIndex + Step) % 3;
			if (Members[Candidate].bAlive)
			{
				const int32 Before = ActiveIndex;
				TrySwitch(Candidate, bChain);
				if (ActiveIndex != Before)
				{
					ConsumeBuffer(EBIBInput::SwitchOperator);
					SwitchStrike(bChain);
				}
				break;
			}
		}
		return;
	}
	// Recovery cancel (2026-10-13): after a cast's last hit, a dodge cuts its recovery short (data kits and hand-written casts).
	TryRecoveryCancel();
	// Animation cancel (Zenless / Anime Zero feel): once the basic attack's hit has landed, dodge, parry and every ability
	// cut the recovery short instead of waiting for it.
	if (Action == EBIBAction::Punch && JudgeData.bActionEffectDone
		&& (Pressed(EBIBInput::Parry) || Pressed(EBIBInput::Move1) || Pressed(EBIBInput::Move2) || Pressed(EBIBInput::Move3)
			|| Pressed(EBIBInput::Special) || Pressed(EBIBInput::UltimateE) || Pressed(EBIBInput::UltimateR)))
	{
		EndAction();
	}
	if (Action != EBIBAction::None || JudgeData.bMenuOpen || (Active().Runtime && IsMovementLocked()))
	{
		return;
	}
	if (bClimbing)
	{
		return;
	}
	// After a perfect dodge the next attack is the dodge counter.
	if (Pressed(EBIBInput::Attack) && TryDodgeCounter())
	{
		ConsumeBuffer(EBIBInput::Attack);
		return;
	}
	if (Pressed(EBIBInput::Attack) && TryDashAttack())
	{
		ConsumeBuffer(EBIBInput::Attack);
		return;
	}
	// Dodge = hold dash (Shift) + parry (F); parry = F. Every action is a GameplayAbility (UBIBAbility_KitSlot).
	if (Pressed(EBIBInput::Parry))
	{
		TryActivateSlot(Held(EBIBInput::Run) ? EBIBSlot::Dodge : EBIBSlot::Parry);
		return;
	}
	// Data kits repeat their regular attack while the button is held (the attack interval is the cooldown).
	const bool bAttackKey = Active().Kit == EBIBKitType::Data ? Held(EBIBInput::Attack) : Pressed(EBIBInput::Attack);
	if (bAttackKey && !bBlocking)
	{
		TryActivateSlot(EBIBSlot::Punch);
	}
}

bool ABIBPlayerCharacter::DodgeActivate()
{
	if (Action != EBIBAction::None)
	{
		return false;
	}
	const FBIBMember& Member = Active();
	const float Cooldown = Member.KitDef ? static_cast<float>(Member.KitDef->DodgeCooldown) : BIBTuning::DodgeCooldown;
	const float Speed = BIBTuning::DodgeSpeed * (Member.KitDef ? static_cast<float>(Member.KitDef->DodgeDistanceMult) : 1.f);
	StartAction(EBIBAction::Dodge);
	StartCooldown(EBIBSlot::Dodge, Cooldown);
	GrantInvulnerable(BIBTuning::DodgeDuration);
	FVector Direction = GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = AimForward();
	}
	LaunchCharacter(Direction * Speed, true, false);
	BIBMotionEvents::Shared(TEXT("dodge-dash-smear"), this, BIBMotionEvents::FacesLeft(this), TEXT("dodge"));
	return true;
}

bool ABIBPlayerCharacter::ParryActivate()
{
	const FBIBMember& Member = Active();
	StartCooldown(EBIBSlot::Parry, BIBTuning::ParryCooldown);
	GrantParry(Member.KitDef ? static_cast<float>(Member.KitDef->ParryWindow) : BIBTuning::ParryWindow);
	return true;
}

void ABIBPlayerCharacter::HandleKitInput()
{
	switch (Active().Kit)
	{
	case EBIBKitType::Luna: LunaInput(); break;
	case EBIBKitType::Judge: JudgeInput(); break;
	case EBIBKitType::Narrator: NarratorInput(); break;
	case EBIBKitType::Data: DataKitInput(); break;
	default: break;
	}
}

void ABIBPlayerCharacter::UpdateAction(float DeltaSeconds)
{
	switch (Action)
	{
	case EBIBAction::None:
		return;
	case EBIBAction::Dodge:
		if (ActionTime >= BIBTuning::DodgeDuration)
		{
			EndAction();
		}
		return;
	case EBIBAction::Punch:
	{
		// One step of the 4-hit string (BIBRules/BIBStagger.h FCombo). Luna's string drags angels in with the black-hole
		// fist and the finisher pushes them out with the white hole (Luna.md flavour); everyone's finisher knocks back.
		const bool bLuna = Active().Kit == EBIBKitType::Luna;
		const bool bFinisher = ComboStepNow == BIB::Stagger::ComboLength - 1;
		const int32 Step = FMath::Clamp(ComboStepNow, 0, BIB::Stagger::ComboLength - 1);
		const float HitAt = static_cast<float>(BIB::Feel::Tuning().ComboHitAt[Step]);         // [BIB.CombatFeel] Combo<n>.HitAt
		const float Duration = static_cast<float>(BIB::Feel::Tuning().ComboStepSeconds[Step]); // [BIB.CombatFeel] Combo<n>.Seconds
		if (bLuna && ActionTime < HitAt)
		{
			for (ABIBEnemy* Enemy : EnemiesInFront(BIBTuning::MeleeRange + 350.f))
			{
				Enemy->AddPull((GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D() * (bFinisher ? 700.0 : 300.0));
			}
		}
		if (ActionTime < HitAt && !bFinisher)
		{
			// A small step into the swing keeps the string moving forward.
			AddMovementInput(ActionDirection, 0.6f);
		}
		if (!JudgeData.bActionEffectDone && ActionTime >= HitAt)
		{
			JudgeData.bActionEffectDone = true;
			float Factor = BIBTuning::PlaceholderComboFactor;
			switch (Active().Kit)
			{
			case EBIBKitType::Luna: Factor = BIBTuning::LunaComboFactor; break;
			case EBIBKitType::Judge: Factor = BIBTuning::JudgeComboFactor; break;
			case EBIBKitType::Narrator: Factor = BIBTuning::NarratorComboFactor; break;
			default: break;
			}
			const float Damage = (Active().Kit == EBIBKitType::Narrator ? NarratorAttack() : Active().BaseAttack) * Factor;
			const float Range = BIBTuning::MeleeRange + (bFinisher ? 80.f : 0.f);
			PunchHitsUntil = Now() + 0.05f;
			// Light hits, the finisher heavy (BIBRules/BIBHitFeel.h): the reaction (flinch / knockback) comes from the weight.
			const FBIBScopedHit Scope(this, BIB::Feel::ESource::Basic, bFinisher);
			for (ABIBEnemy* Enemy : EnemiesInFront(Range, bFinisher ? 0.1f : 0.35f))
			{
				if (bFinisher)
				{
					Enemy->SetNextHitBonus(BIB::Stagger::EBonus::ComboFinisher);
				}
				DealDamage(Enemy, Damage);
				if (Enemy->IsAlive() && bLuna && !bFinisher)
				{
					Enemy->Knockback(GetActorLocation(), 120.f);   // Luna's black-hole fist drags them along
				}
			}
			const FLinearColor Colour = bLuna ? (bFinisher ? BIBFxColours::White : BIBFxColours::Violet) : Hud.Colour;
			ABIBFx::SlashStreak(GetWorld(), GetActorLocation() + ActionDirection * 150.0 + FVector(0.0, 0.0, 30.0), ActionDirection, Colour,
				bFinisher ? 420.f : 300.f, ComboStepNow);
			if (bFinisher)
			{
				ABIBFx::ShockRing(GetWorld(), GetActorLocation() + ActionDirection * 160.0 - FVector(0.0, 0.0, 85.0), 260.f, Colour, 0.3f);
				Shake(1.2f, 0.15f);
			}
		}
		if (ActionTime >= Duration)
		{
			EndAction();
		}
		return;
	}
	default:
		break;
	}
	switch (Active().Kit)
	{
	case EBIBKitType::Luna: LunaUpdateAction(DeltaSeconds); break;
	case EBIBKitType::Judge: JudgeUpdateAction(DeltaSeconds); break;
	case EBIBKitType::Narrator: NarratorUpdateAction(DeltaSeconds); break;
	default: EndAction(); break;
	}
}

bool ABIBPlayerCharacter::DoPunch()
{
	if (!IsReady(EBIBSlot::Punch))
	{
		return false;
	}
	float Cooldown = BIBTuning::NarratorPunchCooldown;
	switch (Active().Kit)
	{
	case EBIBKitType::Luna:
		Cooldown = BIBTuning::LunaPunchCooldown;
		break;
	case EBIBKitType::Judge:
		// Witness and Lawyer modes disable the regular attack; Justice grants it for 45 s.
		if (JudgeData.Mode != EBIBJudgeMode::Judge && Now() >= JudgeData.RegAttackBuffUntil)
		{
			Message(TEXT("Judge: regular attack only in Judge mode."));
			return false;
		}
		Cooldown = BIBTuning::JudgePunchCooldown;
		break;
	case EBIBKitType::Narrator:
		Cooldown = NarratorCooldown(BIBTuning::NarratorPunchCooldown);
		break;
	default:
		break;
	}
	// The string's own timing paces the basic attack now; the old per-kit cooldown only follows the finisher.
	(void)Cooldown;
	ComboStepNow = Combo.Advance(Now(), BIB::Feel::Tuning().ComboWindow);
	if (ABIBEnemy* Target = NearestEnemy(BIBTuning::MeleeRange + 260.f))
	{
		if (LastLockTarget.Get() != Target)
		{
			// A new target: the pack's target lock closes on it.
			LastLockTarget = Target;
			FVector2D Screen;
			if (ScreenPoint(Target->GetActorLocation(), Screen))
			{
				BIBMotionEvents::TargetLock(Screen, static_cast<float>(FVector::Dist(Target->GetActorLocation(), GetActorLocation()) / 100.0));
			}
		}
		// Soft lock-on: turn to the closest angel in reach (Anime Zero / Zenless style), otherwise swing where the camera looks.
		SetActorRotation((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation());
	}
	else
	{
		FaceAim();
	}
	StartAction(EBIBAction::Punch);
	ActionDirection = GetActorForwardVector().GetSafeNormal2D();
	{
		const int32 Step = FMath::Clamp(ComboStepNow, 0, BIB::Stagger::ComboLength - 1);
		const float HitAt = static_cast<float>(BIB::Feel::Tuning().ComboHitAt[Step]);
		const float Duration = static_cast<float>(BIB::Feel::Tuning().ComboStepSeconds[Step]);
		PunchHitsUntil = Now() + Duration;
		// Wind-up until the hit frame, then the recovery (the finisher winds up longer and swings harder).
		StartPose(HitAt, FMath::Max(0.05f, Duration - HitAt), Combo.IsFinisher() ? 1.15f : 0.75f);
	}
	StartCooldown(EBIBSlot::Punch, Combo.IsFinisher() ? 0.25f : 0.02f);
	return true;
}

void ABIBPlayerCharacter::UpdateMovementStats()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bRunning = Held(EBIBInput::Run);
	switch (Active().Kit)
	{
	case EBIBKitType::Judge:
		// Cannot sprint, climb, swim or crouch; walks slowly; can slightly jump.
		Movement->MaxWalkSpeed = BIBTuning::JudgeWalkSpeed;
		Movement->JumpZVelocity = BIBTuning::JudgeJumpZ;
		break;
	case EBIBKitType::Narrator:
		// Speed is the resource; it cannot sprint.
		Movement->MaxWalkSpeed = FMath::Min(BIBTuning::SpeedMoveCap, BIBTuning::WalkSpeed + NarratorForcedSpeed());
		Movement->JumpZVelocity = BIBTuning::BaseJumpZ;
		break;
	case EBIBKitType::Luna:
		Movement->MaxWalkSpeed = bRunning ? BIBTuning::RunSpeed : BIBTuning::WalkSpeed;
		Movement->JumpZVelocity = BIBTuning::BaseJumpZ; // Luna.md: "All base except jump is doubled" (was doubled by mistake)
		break;
	case EBIBKitType::Data:
		if (const BIB::Kits::FKitDef* Kit = Active().KitDef)
		{
			const float Modifier = Active().Runtime ? static_cast<float>(Active().Runtime->MoveSpeedMult()) : 1.f;
			const bool bSprint = bRunning && Kit->bCanSprint;
			Movement->MaxWalkSpeed = (bSprint ? BIBTuning::RunSpeed * static_cast<float>(Kit->RunMult)
				: BIBTuning::WalkSpeed * static_cast<float>(Kit->WalkMult)) * Modifier;
			Movement->JumpZVelocity = BIBTuning::BaseJumpZ * FMath::Min(BIBTuning::MaxJumpMult, static_cast<float>(Kit->JumpMult));
		}
		break;
	default:
		Movement->MaxWalkSpeed = bRunning ? BIBTuning::RunSpeed : BIBTuning::WalkSpeed;
		Movement->JumpZVelocity = BIBTuning::BaseJumpZ;
		break;
	}
	if (bBlocking)
	{
		Movement->MaxWalkSpeed *= 0.5f;
	}
}

void ABIBPlayerCharacter::UpdateVisuals()
{
	if (!BodyMaterial && bTintByOperator)
	{
		BodyMaterial = BIBVisuals::MakeTint(BodyComp->GetMaterial(0), this);
		HeadMaterial = BIBVisuals::MakeTint(HeadComp->GetMaterial(0), this);
		if (BodyMaterial)
		{
			BodyComp->SetMaterial(0, BodyMaterial);
		}
		if (HeadMaterial)
		{
			HeadComp->SetMaterial(0, HeadMaterial);
		}
	}
	FLinearColor Colour(0.6f, 0.6f, 0.6f);
	switch (Active().Kit)
	{
	case EBIBKitType::Luna:
		Colour = FLinearColor(0.22f, 0.05f, 0.35f); // Luna.md: 60% dark purple
		break;
	case EBIBKitType::Judge:
		// Judge.md: gavel glows blue (Witness), red (Lawyer), yellow (Judge).
		Colour = JudgeData.Mode == EBIBJudgeMode::Witness ? FLinearColor(0.15f, 0.35f, 1.f)
			: (JudgeData.Mode == EBIBJudgeMode::Lawyer ? FLinearColor(0.9f, 0.1f, 0.1f) : FLinearColor(1.f, 0.85f, 0.1f));
		break;
	case EBIBKitType::Narrator:
		Colour = NarratorData.bDiano ? FLinearColor(0.9f, 0.3f, 0.05f) : FLinearColor(0.45f, 0.1f, 0.6f);
		break;
	case EBIBKitType::Data:
		if (const BIB::Kits::FKitDef* Kit = Active().KitDef)
		{
			Colour = FLinearColor(Kit->Colour[0], Kit->Colour[1], Kit->Colour[2]);
		}
		break;
	case EBIBKitType::Placeholder:
		// Placeholder fighter: a stable colour per operator so the team is told apart.
		Colour = FLinearColor::MakeFromHSV8(static_cast<uint8>((Active().RosterIndex * 37) % 256), 170, 215);
		break;
	default:
		break;
	}
	if (IsParryingNow() || Now() < NarratorData.ParryUntil)
	{
		Colour = FLinearColor::White;
	}
	if (bBlocking)
	{
		Colour *= 0.5f;
	}
	if (BodyMaterial && bTintByOperator)
	{
		BodyMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), Colour);
	}
	if (HeadMaterial && bTintByOperator)
	{
		HeadMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), FLinearColor(0.95f, 0.85f, 0.75f));
	}
}

// ============================================================================ switching & death

void ABIBPlayerCharacter::TrySwitch(int32 NewIndex, bool bForced)
{
	if (NewIndex == ActiveIndex || NewIndex < 0 || NewIndex > 2 || !Members[NewIndex].bAlive)
	{
		return;
	}
	if (!bForced && Now() < SwitchReadyAt)
	{
		return;
	}
	const EBIBKitType OldKit = Active().Kit;
	CancelAction();
	BIBMotionEvents::Shared(TEXT("switch-out-fade"), this, BIBMotionEvents::FacesLeft(this), TEXT("switch-out"));
	if (OldKit == EBIBKitType::Judge)
	{
		BIBMotionEvents::Shared(TEXT("forgiveness"), this, false, TEXT("judge-alt"));   // every angel becomes innocent again
	}
	if (OldKit == EBIBKitType::Luna)
	{
		LunaClearOrbits(); // the holes orbit Luna herself
	}
	if (OldKit == EBIBKitType::Narrator)
	{
		CloseExchange();
		if (NarratorData.bDiano)
		{
			NarratorEndDiano(TEXT("switched out"));
		}
	}
	// Judge.md: on any switch every enemy becomes innocent and restraining orders are lost.
	ResetJudgeWorld();

	// The outgoing operator keeps its live health / ultimate; the incoming one's values move into the attribute set.
	SyncActiveFromAttributes();
	DataKitSwitchedOut(ActiveIndex);
	ActiveIndex = NewIndex;
	LoadMemberIntoAttributes(ActiveIndex);
	DataKitSwitchedIn(ActiveIndex);
	SwitchReadyAt = Now() + BIBTuning::SwitchCooldown;
	bBlocking = false;
	LastSwitchAt = Now();
	Combo.Reset();
	ComboStepNow = -1;
	StopClimb(false);
	if (UBIBGameInstance* Game = GetGameInstance<UBIBGameInstance>())
	{
		if (!bForced)
		{
			Game->GetAngelMind().Observe(BIB::Angel::ETrait::Switching);
		}
	}
	ABIBFx::Spawn(GetWorld(), EBIBFxShape::Pillar, GetActorLocation(), FRotator::ZeroRotator, FVector(160.f, 160.f, 40.f), FVector(30.f, 30.f, 700.f),
		BIBFxColours::Cyan, 0.3f, 8.f);
	switch (Active().Kit)
	{
	case EBIBKitType::Judge:
		JudgeData.Mode = EBIBJudgeMode::Witness; // "You automatically start on Witness mode"
		JudgeData.bMenuOpen = false;
		break;
	case EBIBKitType::Narrator:
		NarratorData.Speed = BIBTuning::NarratorStartSpeed;
		NarratorData.SecondTimer = 0.f;
		NarratorData.bCloseEnoughUsed = false;
		break;
	default:
		break;
	}
	Message(FString::Printf(TEXT("%s is now active."), *Active().Name));
	BIBMotionEvents::Shared(TEXT("switch-in-drop"), this, BIBMotionEvents::FacesLeft(this), TEXT("switch-in"));
	BIBMotionEvents::Gauges(Active().Name);
	if (Active().Kit == EBIBKitType::Narrator)
	{
		BIBMotionEvents::Shared(TEXT("past-life"), this, false, TEXT("narr-alt"));
	}
	else
	{
		// passives shown as the operator arrives
		BIBMotionEvents::Ability(this, Active().Name, TEXT("PASSIVES")) || BIBMotionEvents::Ability(this, Active().Name, TEXT("PASSIVE"));
	}
	NotifyOperatorChanged();
}

void ABIBPlayerCharacter::ResetJudgeWorld()
{
	JudgeData.RestrainingOrders = 0;
	JudgeData.bMenuOpen = false;
	if (ABIBGameMode* GameMode = GetMode())
	{
		for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			Enemy->JudgeState = EBIBJudgeState::Innocent;
			Enemy->HitsOnJudge = 0;
			Enemy->DamageOnJudge = 0.f;
		}
	}
}

void ABIBPlayerCharacter::OnActiveDown()
{
	FBIBMember& Down = Active();
	SetMemberHP(ActiveIndex, 0.f);
	Down.bAlive = false;
	DataKitSwitchedOut(ActiveIndex);
	CancelAction();
	Message(FString::Printf(TEXT("%s is down!"), *Down.Name));
	// MAIN §6: the next operator in the lineup enters (wrap-around after slot 3 is OPEN; the demo wraps).
	for (int32 Step = 1; Step < 3; ++Step)
	{
		const int32 Candidate = (ActiveIndex + Step) % 3;
		if (Members[Candidate].bAlive)
		{
			TrySwitch(Candidate, true);
			return;
		}
	}
	if (ABIBGameMode* GameMode = GetMode())
	{
		GameMode->OnTeamWiped();
	}
}

// ============================================================================ damage

void ABIBPlayerCharacter::ReceiveEnemyHit(ABIBEnemy* Attacker, float Damage, int32 EnemyAbilityId, bool bMelee, bool bUnblockable)
{
	if (!bCombatActive || !Active().bAlive)
	{
		return;
	}
	const float T = Now();
	FBIBMember& Member = Active();
	if (bUltCinematic)
	{
		return;   // the ultimate's cinematic beat: nothing reaches the operator
	}
	if (IsInvulnerableNow())
	{
		// A dodge (i-frames) that negates a hit is what dodge passives react to.
		if (Action == EBIBAction::Dodge && Member.Runtime)
		{
			Member.Runtime->NotifyDodge();
		}
		if (Action == EBIBAction::Dodge && ActionTime <= static_cast<float>(BIB::Feel::Tuning().PerfectDodgeWindow))
		{
			PerfectDodge(Attacker);
		}
		return;
	}

	// Kit counters that negate the hit entirely.
	if (Member.Kit == EBIBKitType::Judge && Action == EBIBAction::JudgeJustice)
	{
		JudgeJusticeTrigger(Attacker);
		return;
	}
	if (Member.Kit == EBIBKitType::Narrator)
	{
		if (NarratorData.bDiano && T < NarratorData.DianoBlockUntil)
		{
			if (Attacker)
			{
				const FBIBScopedHit Scope(this, BIB::Feel::ESource::Counter);
				DealDamage(Attacker, BIBTuning::SwitchCounterDamage);
			}
			NarratorData.DianoUntil += BIBTuning::SwitchBlockBonus;
			Message(TEXT("Diano blocked the attack: 10000 damage back, +5 s."));
			return;
		}
		if (T < NarratorData.ParryUntil)
		{
			Message(TEXT("I AM STILL ALIVE: parried."));
			return;
		}
	}
	if (IsParryingNow() && !bUnblockable)
	{
		// Judge cannot receive status effects, but parrying the attacker is fine.
		if (Attacker)
		{
			Attacker->Stun(BIBTuning::ParryStun);
		}
		if (Member.Runtime)
		{
			Member.Runtime->NotifyParry();
		}
		Message(TEXT("Parry!"));
		if (Attacker && Attacker->IsAlive())
		{
			// Parry counter: a riposte that piles on Strain (a heavy hit: hit stop, knockback, camera kick).
			Attacker->SetNextHitBonus(BIB::Stagger::EBonus::ParryCounter);
			StartPose(0.f, 0.22f, 1.1f);
			const FBIBScopedHit Scope(this, BIB::Feel::ESource::Counter);
			DealDamage(Attacker, Member.BaseAttack * 2.f);
			ABIBFx::SlashStreak(GetWorld(), Attacker->GetActorLocation() + FVector(0.0, 0.0, 40.0), Attacker->GetActorLocation() - GetActorLocation(), BIBFxColours::Gold, 360.f, 3);
		}
		ShowBanner(TEXT("PARRY"), FString(), BIBFxColours::Gold, 0.7);
		FeelOnDefence(1);
		BIBMotionEvents::Defense(1);
		BIBMotionEvents::Shared(TEXT("parry-spark-riposte"), this, BIBMotionEvents::FacesLeft(this), TEXT("parry"));
		if (Member.Kit == EBIBKitType::Narrator)
		{
			BIBMotionEvents::Shared(TEXT("suppression"), this, false, TEXT("narr-alt"));
		}
		Accent(true);
		SetSlowMo(0.4f, 0.2);
		Shake(1.5f, 0.15f);
		return;
	}

	float Amount = Damage;
	if (bBlocking && !bUnblockable)
	{
		FeelOnDefence(0);
		BIBMotionEvents::Defense(0);
		BIBMotionEvents::Shared(TEXT("block-shield-flare"), this, BIBMotionEvents::FacesLeft(this), TEXT("block"));
		if (Member.Kit == EBIBKitType::Narrator && NarratorData.bDiano)
		{
			BIBMotionEvents::Shared(TEXT("ultimate-parry-punch"), this, BIBMotionEvents::FacesLeft(this), TEXT("narr-alt"));
		}
		Amount *= BIBTuning::BlockDamageFactor;
	}
	Amount *= 1.f - FMath::Clamp(Member.DefencePercent, 0.f, 90.f) / 100.f;
	if (Member.Runtime)
	{
		// Data kits: shields, evasion, damage-taken modifiers, reactions and reflect are decided by the rules layer.
		bool bNegated = false;
		Amount = DataKitReceiveHit(Amount, Attacker, EnemyAbilityId, bMelee, bNegated);
	}
	if (Amount > 0.f)
	{
		AbilitySystem->ApplyDamage(Amount, Attacker);
		SyncActiveFromAttributes();
		DamageTakenThisAction += Amount;

		if (Member.Kit == EBIBKitType::Judge)
		{
			JudgeOnHit(Attacker, Amount);
		}
		else if (Member.Kit == EBIBKitType::Narrator)
		{
			NarratorData.bHitThisSecond = true;
			if (NarratorData.CloseEnoughAt > 0.f)
			{
				NarratorData.CloseEnoughAt = -1.f;
				Message(TEXT("Close enough interrupted by damage."));
			}
		}
	}
	if (Member.HP <= 0.f)
	{
		OnActiveDown();
	}
}

void ABIBPlayerCharacter::TakeSelfDamage(float Amount)
{
	AbilitySystem->ApplyDamage(Amount, this);
	SyncActiveFromAttributes();
	if (Active().HP <= 0.f)
	{
		OnActiveDown();
	}
}

TArray<ABIBEnemy*> ABIBPlayerCharacter::EnemiesInRadius(const FVector& Centre, float Radius, bool bIncludeAirborne) const
{
	TArray<ABIBEnemy*> Result;
	if (ABIBGameMode* GameMode = GetMode())
	{
		for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			if (!bIncludeAirborne && Enemy->GetCharacterMovement()->IsFalling())
			{
				continue;
			}
			const double Reach = Radius + Enemy->GetBodyRadius();
			if (FVector::DistSquared2D(Centre, Enemy->GetActorLocation()) <= Reach * Reach)
			{
				Result.Add(Enemy);
			}
		}
	}
	return Result;
}

TArray<ABIBEnemy*> ABIBPlayerCharacter::EnemiesInFront(float Range, float MinDot) const
{
	TArray<ABIBEnemy*> Result;
	const FVector Origin = GetActorLocation();
	const FVector Forward = Action != EBIBAction::None ? ActionDirection : AimForward();
	for (ABIBEnemy* Enemy : EnemiesInRadius(Origin, Range))
	{
		const FVector ToEnemy = (Enemy->GetActorLocation() - Origin).GetSafeNormal2D();
		if (FVector::DotProduct(ToEnemy, Forward) >= MinDot || FVector::Dist2D(Origin, Enemy->GetActorLocation()) < 120.0)
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}

float ABIBPlayerCharacter::DealDamage(ABIBEnemy* Enemy, float Amount, int32 SourceIndex)
{
	if (!Enemy || !Enemy->IsAlive() || Amount <= 0.f)
	{
		return 0.f;
	}
	const int32 Source = SourceIndex >= 0 ? SourceIndex : ActiveIndex;
	const FBIBMember& Member = Members[Source];
	if (Member.Kit == EBIBKitType::Judge && Source == ActiveIndex)
	{
		// Judge can only hurt Guilty enemies; attacking an Innocent hurts Judge; Suspects are unaffected.
		if (Enemy->JudgeState == EBIBJudgeState::Suspect)
		{
			Enemy->ReceiveImpact(this, BIB::Feel::EWeight::Light);   // no damage, but the blow still lands
			return 0.f;
		}
		if (Enemy->JudgeState == EBIBJudgeState::Innocent)
		{
			Message(TEXT("Judge attacked an innocent and was hurt."));
			Enemy->ReceiveImpact(this, BIB::Feel::EWeight::Light);
			TakeSelfDamage(Active().MaxHP * BIBTuning::JudgeInnocentSelfDamageFraction);
			return 0.f;
		}
	}
	float Scaled = Amount * Member.DamageMult;
	if (Member.Kit == EBIBKitType::Narrator)
	{
		Scaled *= NarratorData.DamageX;
	}
	const float Dealt = Enemy->ReceiveDamage(Scaled, this);
	// An ultimate's own damage never recharges the ultimate (data kits already work this way; Luna's 55000 to everything
	// used to refill her bar at once - 2026-10-13).
	if (PendingHit.Source != BIB::Feel::ESource::Ultimate)
	{
		GainUlt(Source, Dealt);
	}
	return Dealt;
}

void ABIBPlayerCharacter::KillEnemy(ABIBEnemy* Enemy, bool bRespectBosses, int32 SourceIndex)
{
	if (!Enemy || !Enemy->IsAlive())
	{
		return;
	}
	const int32 Source = SourceIndex >= 0 ? SourceIndex : ActiveIndex;
	const float Amount = (bRespectBosses && Enemy->IsBoss()) ? Enemy->GetMaxHP() * BIBTuning::BossInstantKillFraction : Enemy->GetHP();
	const float Dealt = Enemy->ReceiveDamage(Amount, this);
	if (PendingHit.Source != BIB::Feel::ESource::Ultimate)
	{
		GainUlt(Source, Dealt);   // an ultimate's own kills never recharge it (as in DealDamage)
	}
}

void ABIBPlayerCharacter::GainUlt(int32 SourceIndex, float DamageDealt)
{
	if (SourceIndex < 0 || SourceIndex > 2 || DamageDealt <= 0.f)
	{
		return;
	}
	const FBIBMember& Member = Members[SourceIndex];
	// Owner choice: the ultimate charges from damage dealt (rate is a PLACEHOLDER in BIBConfig.h).
	float Gain = DamageDealt / static_cast<float>(BIB::Config::UltDamageForFullBar) * 100.f;
	if (Member.Kit == EBIBKitType::Narrator)
	{
		Gain *= NarratorData.UltX;
	}
	else if (Member.Kit == EBIBKitType::Luna)
	{
		Gain *= BIBTuning::LunaUltGainMult;   // 2026-10-13 rebalance: Reality Destabilisation takes two bars' worth of damage
	}
	SetMemberUlt(SourceIndex, Member.Ult + Gain);
}

void ABIBPlayerCharacter::Explode(const FVector& Centre, float Radius, float Damage, float KnockbackStrength, const FColor& Colour, int32 SourceIndex)
{
	ABIBFx::Spawn(GetWorld(), EBIBFxShape::Sphere, Centre, FRotator::ZeroRotator, FVector(Radius * 0.3f), FVector(Radius * 2.f), FLinearColor(Colour), 0.4f, 5.f);
	ABIBFx::ShockRing(GetWorld(), Centre - FVector(0.0, 0.0, 80.0), Radius, FLinearColor::LerpUsingHSV(FLinearColor(Colour), BIBFxColours::Rose, 0.3f), 0.45f);
	if (Radius >= 500.f)
	{
		Shake(FMath::Clamp(Radius / 400.f, 1.f, 3.f), 0.25f);
	}
	for (ABIBEnemy* Enemy : EnemiesInRadius(Centre, Radius))
	{
		DealDamage(Enemy, Damage, SourceIndex);
		if (KnockbackStrength > 0.f && Enemy->IsAlive())
		{
			Enemy->Knockback(Centre, KnockbackStrength);
		}
	}
}

void ABIBPlayerCharacter::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
	FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	if (!bCombatActive || Active().Kit != EBIBKitType::Narrator || NarratorData.bDiano)
	{
		return;
	}
	const ABIBBlock* Block = Cast<ABIBBlock>(Other);
	if (Block && Block->IsWall() && FMath::Abs(HitNormal.Z) < 0.3 && Now() - NarratorData.LastWallHitAt > BIBTuning::WallHitCooldown)
	{
		// "Hitting a wall removes 50% of your current speed."
		NarratorData.LastWallHitAt = Now();
		NarratorData.Speed *= 0.5f;
		Message(FString::Printf(TEXT("Narrator hit a wall: speed halved to %d."), FMath::RoundToInt(NarratorData.Speed)));
	}
}

// ============================================================================ HUD

void ABIBPlayerCharacter::AddAbilityHud(const FString& Key, const FString& Name, EBIBSlot Slot, const FString& Note)
{
	FBIBAbilityHud Entry;
	Entry.Key = Key;
	Entry.Name = Name;
	Entry.Remaining = Remaining(Slot);
	Entry.Note = Note;
	Hud.Abilities.Add(Entry);
}

void ABIBPlayerCharacter::BuildHud()
{
	Hud.bValid = bCombatActive;
	BuildFeelHud();
	Hud.Abilities.Reset();
	Hud.Team.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FBIBTeamHud Entry;
		Entry.Name = Members[Index].Name.IsEmpty() ? FString(TEXT("(empty)")) : Members[Index].Name;
		Entry.HPFraction = Members[Index].MaxHP > 0.f ? Members[Index].HP / Members[Index].MaxHP : 0.f;
		Entry.bAlive = Members[Index].bAlive;
		Entry.bActive = Index == ActiveIndex;
		Hud.Team.Add(Entry);
	}
	const FBIBMember& Member = Active();
	Hud.ActiveName = Member.Name;
	Hud.HP = Member.HP;
	Hud.MaxHP = FMath::Max(1.f, Member.MaxHP);
	Hud.Ult = Member.Ult;
	Hud.bJudgeModeMenu = Member.Kit == EBIBKitType::Judge && JudgeData.bMenuOpen;
	Hud.bExchangeOpen = Member.Kit == EBIBKitType::Narrator && NarratorData.bExchangeOpen;
	Hud.ExchangeValues[static_cast<int32>(EBIBStat::Damage)] = NarratorData.DamageX;
	Hud.ExchangeValues[static_cast<int32>(EBIBStat::Health)] = NarratorData.HealthX;
	Hud.ExchangeValues[static_cast<int32>(EBIBStat::Speed)] = NarratorData.Speed / BIBTuning::SpeedPerX;
	Hud.ExchangeValues[static_cast<int32>(EBIBStat::Ultimate)] = NarratorData.UltX;
	Hud.ExchangeValues[static_cast<int32>(EBIBStat::Cooldown)] = NarratorData.CooldownX;
	Hud.ExchangeFrom = NarratorData.ExchangeFrom;
	Hud.ExchangeTo = NarratorData.ExchangeTo;
	Hud.ActionLine.Reset();
	Hud.KitLine.Reset();

	const FString UltNote = Member.Ult >= 100.f ? FString(TEXT("READY")) : FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Member.Ult));
	switch (Member.Kit)
	{
	case EBIBKitType::Luna:
		Hud.Colour = FLinearColor(0.6f, 0.3f, 0.9f);
		AddAbilityHud(TEXT("LMB"), TEXT("Reg attack"), EBIBSlot::Punch);
		AddAbilityHud(TEXT("1"), TEXT("Blackhole"), EBIBSlot::Move1);
		AddAbilityHud(TEXT("2"), TEXT("Orbiting"), EBIBSlot::Move2);
		AddAbilityHud(TEXT("3"), TEXT("Event horizon (hold)"), EBIBSlot::Move3);
		AddAbilityHud(TEXT("4"), TEXT("Special: Expel (hold)"), EBIBSlot::Special);
		AddAbilityHud(TEXT("E"), TEXT("Ult: Reality Destabilisation"), EBIBSlot::Ultimate, UltNote);
		Hud.KitLine = LunaKitLine();
		break;
	case EBIBKitType::Judge:
	{
		Hud.Colour = FLinearColor(1.f, 0.85f, 0.2f);
		const bool bJudgeMode = JudgeData.Mode == EBIBJudgeMode::Judge;
		const FString JudgeOnly = bJudgeMode ? FString() : FString(TEXT("Judge mode only"));
		AddAbilityHud(TEXT("LMB"), TEXT("Reg attack"), EBIBSlot::Punch, (bJudgeMode || Now() < JudgeData.RegAttackBuffUntil) ? FString() : JudgeOnly);
		AddAbilityHud(TEXT("1"), TEXT("Paperwork"), EBIBSlot::Move1, JudgeModeName(JudgeData.Mode));
		AddAbilityHud(TEXT("2"), TEXT("Court setting"), EBIBSlot::Move2, Now() < JudgeData.CourtLockedUntil ? FString(TEXT("LOCKED")) : FString());
		AddAbilityHud(TEXT("3"), TEXT("Gavel smash"), EBIBSlot::Move3, JudgeOnly);
		AddAbilityHud(TEXT("4"), TEXT("Special: Retribution"), EBIBSlot::Special, JudgeOnly);
		AddAbilityHud(TEXT("R"), TEXT("Ult: Justice"), EBIBSlot::Ultimate, bJudgeMode ? UltNote : JudgeOnly);
		Hud.KitLine = JudgeKitLine();
		break;
	}
	case EBIBKitType::Narrator:
		Hud.Colour = NarratorData.bDiano ? FLinearColor(1.f, 0.45f, 0.1f) : FLinearColor(0.75f, 0.4f, 1.f);
		AddAbilityHud(TEXT("LMB"), TEXT("Reg attack"), EBIBSlot::Punch);
		if (NarratorData.bDiano)
		{
			AddAbilityHud(TEXT("1"), TEXT("Acceleration"), EBIBSlot::Move1, NarratorData.bAccelChainPending ? FString(TEXT("CHAIN!")) : FString());
			AddAbilityHud(TEXT("2"), TEXT("Endless god (hold)"), EBIBSlot::Move2);
			AddAbilityHud(TEXT("3"), TEXT("Embodiment of imperfection"), EBIBSlot::Move3);
			AddAbilityHud(TEXT("4"), TEXT("Special: The TRUE STRONGEST"), EBIBSlot::Special, TEXT("not in demo"));
			AddAbilityHud(TEXT("E"), TEXT("Ult: THE SOUL ONE"), EBIBSlot::Ultimate, TEXT("needs Ult^2 - not in demo"));
		}
		else
		{
			AddAbilityHud(TEXT("1"), TEXT("Explosion"), EBIBSlot::Move1, NarratorData.Speed < BIBTuning::ExplosionSpeedCost ? FString(TEXT("needs 100 speed")) : FString());
			AddAbilityHud(TEXT("2"), TEXT("Global Exchange"), EBIBSlot::Move2, NarratorData.bExchangeOpen ? FString(TEXT("OPEN")) : FString());
			AddAbilityHud(TEXT("3"), TEXT("I AM STILL ALIVE"), EBIBSlot::Move3, Member.Ult < 50.f ? FString(TEXT("parry")) : FString(TEXT("random mimic")));
			AddAbilityHud(TEXT("4"), TEXT("Special: Close enough"), EBIBSlot::Special, NarratorData.bCloseEnoughUsed ? FString(TEXT("used this fight")) : FString());
			AddAbilityHud(TEXT("E"), TEXT("Ult: Switch"), EBIBSlot::Ultimate, UltNote);
		}
		Hud.KitLine = NarratorKitLine();
		break;
	case EBIBKitType::Data:
		DataKitBuildHud(Member);
		break;
	case EBIBKitType::Placeholder:
		Hud.Colour = FLinearColor(0.8f, 0.8f, 0.85f);
		AddAbilityHud(TEXT("LMB"), TEXT("Reg attack"), EBIBSlot::Punch);
		Hud.KitLine = TEXT("Placeholder fighter: no moves written yet (regular attack, block, parry, dodge).");
		break;
	default:
		Hud.Colour = FLinearColor::White;
		break;
	}

	switch (Action)
	{
	case EBIBAction::LunaBlackhole: Hud.ActionLine = FString::Printf(TEXT("Blackhole %.1fs (cancels at 15%% max HP lost)"), ActionTime); break;
	case EBIBAction::LunaOrbitingWindup: Hud.ActionLine = FString::Printf(TEXT("Summoning hole %.1f / %.1fs"), ActionTime, BIBTuning::OrbitingWindup); break;
	case EBIBAction::LunaEventHorizon: Hud.ActionLine = TEXT("Event horizon held - release 3 to cancel"); break;
	case EBIBAction::LunaExpel: Hud.ActionLine = FString::Printf(TEXT("Expel charging %.1fs - damage %d"), ActionTime, FMath::RoundToInt(BIBTuning::ExpelBaseDamage + BIBTuning::ExpelDamagePerSecond * ActionTime)); break;
	case EBIBAction::LunaCutscene: Hud.ActionLine = TEXT("[Cutscene] Luna forcibly smashes the black and white hole together"); break;
	case EBIBAction::JudgePaperwork: Hud.ActionLine = TEXT("Paperwork windup..."); break;
	case EBIBAction::JudgeCourtWindup:
		Hud.ActionLine = FString::Printf(TEXT("Changing to %s %.1fs%s"), *JudgeModeName(JudgeData.PendingMode), ActionTime,
			JudgeData.PendingMode == EBIBJudgeMode::Judge ? TEXT(" - DO NOTHING or the court locks for 30 s") : TEXT(""));
		break;
	case EBIBAction::JudgeGavel: Hud.ActionLine = TEXT("Gavel smash!"); break;
	case EBIBAction::JudgeRetribution: Hud.ActionLine = TEXT("Retribution windup..."); break;
	case EBIBAction::JudgeJustice: Hud.ActionLine = FString::Printf(TEXT("Justice counter stance %.1fs, radius %dcm"), ActionTime, FMath::RoundToInt(JudgeData.JusticeRadius)); break;
	case EBIBAction::NarratorSwitch: Hud.ActionLine = TEXT("Switching to Diano..."); break;
	case EBIBAction::DianoEndlessGod: Hud.ActionLine = TEXT("Endless god: sacrificing health and damage for time"); break;
	default: break;
	}
	if (NarratorData.CloseEnoughAt > 0.f && Member.Kit == EBIBKitType::Narrator)
	{
		Hud.ActionLine = FString::Printf(TEXT("Close enough in %.1fs (damage interrupts)"), NarratorData.CloseEnoughAt - Now());
	}
	if (Hud.bJudgeModeMenu)
	{
		Hud.ActionLine = TEXT("COURT SETTING: press 1 Witness, 2 Lawyer, 3 Judge");
	}
}
