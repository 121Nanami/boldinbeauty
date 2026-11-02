#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBMotionEvents.h"
#include "BIBGame/BIBVisuals.h"
#include "BIBGame/BIBFx.h"

#include "BIBGAS/BIBAbilitySystemComponent.h"
#include "BIBGAS/BIBAttributeSet.h"
#include "BIBGAS/BIBGameplayTags.h"

#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBTactics.h"
#include "BIBGame/BIBTuning.h"
#include "BIBRules/BIBCampaign.h"
#include "BIBRules/BIBElement.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABIBEnemy::ABIBEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// No AI controller: movement runs without one.
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	BodyComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyComp->SetupAttachment(GetCapsuleComponent());
	BodyComp->SetStaticMesh(CylinderFinder.Object);
	BodyComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HeadComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	HeadComp->SetupAttachment(GetCapsuleComponent());
	HeadComp->SetStaticMesh(SphereFinder.Object);
	HeadComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LabelComp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComp->SetupAttachment(GetCapsuleComponent());
	LabelComp->SetHorizontalAlignment(EHTA_Center);
	LabelComp->SetVerticalAlignment(EVRTA_TextCenter);
	LabelComp->SetWorldSize(26.f);

	// Zenless-style floating bars: health (rose) over Daze (cyan), turned to the camera with the label.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeFinder.Object;
	auto MakeBar = [this, CubeMesh](const TCHAR* Name)
	{
		UStaticMeshComponent* Bar = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Bar->SetupAttachment(LabelComp);
		Bar->SetStaticMesh(CubeMesh);
		Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bar->SetCastShadow(false);
		return Bar;
	};
	BarBackComp = MakeBar(TEXT("BarBack"));
	BarFillComp = MakeBar(TEXT("BarFill"));
	DazeFillComp = MakeBar(TEXT("DazeFill"));

	AbilitySystem = CreateDefaultSubobject<UBIBAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UBIBAttributeSet>(TEXT("Attributes"));
}

void ABIBEnemy::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
		if (Attributes && !AbilitySystem->GetSet<UBIBAttributeSet>())
		{
			AbilitySystem->AddAttributeSetSubobject(Attributes.Get());
		}
		AbilitySystem->AddLooseGameplayTag(BIBTags::TeamEnemy());
	}
}

UAbilitySystemComponent* ABIBEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

float ABIBEnemy::GetHP() const
{
	return AbilitySystem ? AbilitySystem->GetHealthValue() : 0.f;
}

float ABIBEnemy::GetMaxHP() const
{
	return AbilitySystem ? AbilitySystem->GetMaxHealthValue() : 1.f;
}

void ABIBEnemy::ApplyStatScale(float Scale)
{
	ApplyStatScales(Scale, Scale, 1.f);
}

void ABIBEnemy::ApplyStatScales(float HpScale, float DamageScale, float SlamScale)
{
	if (HpScale > 0.f && !FMath::IsNearlyEqual(HpScale, 1.f))
	{
		const float ScaledMax = GetMaxHP() * HpScale;
		AbilitySystem->SetMaxHealthValue(ScaledMax);
		AbilitySystem->SetHealthValue(ScaledMax);
	}
	if (DamageScale > 0.f)
	{
		AttackDamage *= DamageScale;
	}
	if (SlamScale > 0.f)
	{
		SlamDamageMult = SlamScale;
	}
	UpdateLabelAndColour();
}

namespace
{
	BIB::Campaign::EArchetype ArchetypeFor(EBIBEnemyKind Kind)
	{
		switch (Kind)
		{
		case EBIBEnemyKind::Brute: return BIB::Campaign::EArchetype::Brute;
		case EBIBEnemyKind::MiniBoss: return BIB::Campaign::EArchetype::MiniBoss;
		case EBIBEnemyKind::Boss: return BIB::Campaign::EArchetype::Boss;
		case EBIBEnemyKind::Ranged: return BIB::Campaign::EArchetype::Ranged;
		case EBIBEnemyKind::Mage: return BIB::Campaign::EArchetype::Mage;
		case EBIBEnemyKind::Assassin: return BIB::Campaign::EArchetype::Assassin;
		case EBIBEnemyKind::Grunt:
		default: return BIB::Campaign::EArchetype::Melee;
		}
	}
}

void ABIBEnemy::InitEnemy(EBIBEnemyKind InKind, ABIBGameMode* InMode)
{
	Kind = InKind;
	Mode = InMode;

	// Every kind reads its numbers from the archetype table (BIBRules/BIBCampaign.cpp): the original grunt, brute, mini-boss
	// and boss rows are the demo's first placeholders, unchanged; ranged, mage and assassin are the campaign's additions.
	const BIB::Campaign::FArchetype& Archetype = BIB::Campaign::Archetype(ArchetypeFor(Kind));
	float Speed = static_cast<float>(Archetype.Speed);
	float Radius = static_cast<float>(Archetype.Radius);
	float HalfHeight = static_cast<float>(Archetype.HalfHeight);
	float NewMaxHP = static_cast<float>(Archetype.Hp);
	AttackDamage = static_cast<float>(Archetype.Damage);
	AttackAbilityId = Archetype.AbilityId;
	AttackIntervalS = static_cast<float>(Archetype.Interval);
	AttackWindupS = static_cast<float>(Archetype.Windup);
	KeepMinCm = static_cast<float>(Archetype.KeepMin);
	KeepMaxCm = static_cast<float>(Archetype.KeepMax);
	AoeRadiusCm = static_cast<float>(Archetype.AoeRadius);
	if (!bUseTuningDefaults)
	{
		NewMaxHP = CustomMaxHP;
		AttackDamage = CustomAttackDamage;
		Speed = CustomMoveSpeed;
		Radius = CustomRadius;
		HalfHeight = CustomHalfHeight;
	}
	Weight = IsBoss() ? BIB::Stagger::EWeight::Heavy : (Kind == EBIBEnemyKind::Brute ? BIB::Stagger::EWeight::Medium : BIB::Stagger::EWeight::Light);
	Daze.Reset(BIB::Stagger::DazeMax(Weight, Kind == EBIBEnemyKind::Boss));
	bPhaseTwo = false;
	GuardUntil = -1.f;
	PendingFollowUps = 0;
	AbilitySystem->SetMaxHealthValue(NewMaxHP);
	AbilitySystem->SetHealthValue(NewMaxHP);
	StatusBook.ClearAll();
	RefreshStatusTags();
	BodyRadius = Radius;
	// Melee enemies (Archetype.Range = 0) reach from the edge of their body; ranged and mage enemies shoot from afar.
	AttackRange = Archetype.Range > 0.0 ? static_cast<float>(Archetype.Range) : Radius + 130.f;
	AttackCooldown = FMath::FRandRange(0.2f, AttackIntervalS);
	// The tactical brain starts fresh; each angel has its own random stream (feints, strafing) and a staggered first attack.
	Brain = BIB::Angel::FBrainState();
	Brain.Random.State = GetUniqueID() * 2654435761u + 1u;
	Brain.AttackReadyAt = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + AttackCooldown;
	CurrentAction = BIB::Angel::FAction();
	BaseWalkSpeed = Speed;
	bSeesCommander = false;
	bAllyInLine = false;
	NextSightCheckAt = 0.f;
	DashUntil = -1.f;
	SecondFlashAt = -1.f;
	bSuperArmor = false;
	Shots.Reset();

	GetCapsuleComponent()->SetCapsuleSize(Radius, HalfHeight);
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	const double Diameter = Radius * 2.0;
	BodyComp->SetRelativeScale3D(FVector(Diameter / 100.0, Diameter / 100.0, HalfHeight * 1.5 / 100.0));
	BodyComp->SetRelativeLocation(FVector(0.0, 0.0, -HalfHeight * 0.25));
	BodyComp->SetRelativeRotation(FRotator::ZeroRotator);
	HeadComp->SetRelativeScale3D(FVector(Diameter * 0.8 / 100.0));
	HeadComp->SetRelativeLocation(FVector(0.0, 0.0, HalfHeight * 0.75));
	BodyBaseLocation = FVector(0.0, 0.0, -HalfHeight * 0.25);
	HeadBaseLocation = FVector(0.0, 0.0, HalfHeight * 0.75);
	PivotZ = -HalfHeight;
	ReactionKind = BIB::Feel::EReaction::None;
	LastTilt = 0.f;
	LabelComp->SetRelativeLocation(FVector(0.0, 0.0, HalfHeight + 60.0));

	if (!BarBackMaterial)
	{
		BarBackMaterial = BIBVisuals::MakeTint(BarBackComp->GetMaterial(0), this);
		BarFillMaterial = BIBVisuals::MakeTint(BarFillComp->GetMaterial(0), this);
		DazeFillMaterial = BIBVisuals::MakeTint(DazeFillComp->GetMaterial(0), this);
		BarBackComp->SetMaterial(0, BarBackMaterial);
		BarFillComp->SetMaterial(0, BarFillMaterial);
		DazeFillComp->SetMaterial(0, DazeFillMaterial);
		if (BarBackMaterial) { BarBackMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), FLinearColor(0.035f, 0.04f, 0.07f)); }
		if (BarFillMaterial) { BarFillMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), BIBFxColours::Rose); }
		if (DazeFillMaterial) { DazeFillMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), BIBFxColours::Cyan); }
	}
	if (bTintByState && !BodyMaterial)
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
	UpdateLabelAndColour();
}

FString ABIBEnemy::GetKindName() const
{
	return FString(UTF8_TO_TCHAR(BIB::Campaign::ArchetypeName(ArchetypeFor(Kind))));
}

ABIBPlayerCharacter* ABIBEnemy::GetTarget() const
{
	return Mode.IsValid() ? Mode->GetPlayerCharacter() : nullptr;
}

float ABIBEnemy::ReceiveDamage(float Amount, AActor* Instigator)
{
	if (!IsAlive() || Amount <= 0.f)
	{
		return 0.f;
	}
	// Element advantage (NIKKE-style code weakness): only operator hits, only when their element beats this enemy's.
	bool bAdvantage = false;
	if (ElementIndex >= 0)
	{
		if (const ABIBPlayerCharacter* Player = Cast<ABIBPlayerCharacter>(Instigator))
		{
			if (UBIBGameInstance* Game = GetGameInstance<UBIBGameInstance>())
			{
				const double Mult = BIB::Element::DamageMultiplier(Game->GetProfile(), Player->GetActiveRosterIndex(), BIB::Element::FromIndex(ElementIndex));
				bAdvantage = Mult > 1.0;
				Amount *= static_cast<float>(Mult);
			}
		}
	}
	// Zenless-style reactions to the commander's hits: guard, stun bonus, Daze, flinch, hit stop and a white flash.
	ABIBPlayerCharacter* Commander = Cast<ABIBPlayerCharacter>(Instigator);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const BIB::Stagger::EBonus Bonus = NextHitBonus;
	NextHitBonus = BIB::Stagger::EBonus::None;
	const float Attack = Commander ? FMath::Max(1.f, Commander->GetActiveAttack()) : 1.f;
	bool bGuarded = false;
	if (Commander)
	{
		Amount *= Commander->OutgoingHitMult();   // basic-attack string step
		const float Ratio = Amount / Attack;
		if (IsGuarding())
		{
			const FVector ToCommander = (Commander->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			const bool bFront = FVector::DotProduct(GetActorForwardVector(), ToCommander) > 0.2;
			if (bFront && Ratio < BIB::Stagger::HeavyHitRatio && Bonus == BIB::Stagger::EBonus::None)
			{
				bGuarded = true;
				Amount *= 0.2f;
				++GuardHits;
				ABIBFx::HitSpark(GetWorld(), GetActorLocation() + ToCommander * BodyRadius + FVector(0.0, 0.0, 40.0), BIBFxColours::Cyan, 0.6f);
			}
			else if (bFront)
			{
				GuardUntil = -1.f;
				GuardHits = 0;
				NextHitBonus = BIB::Stagger::EBonus::None;
				Commander->OnGuardBroken(this);
			}
		}
		Amount *= static_cast<float>(BIB::Stagger::DamageTakenMult(Daze, Now));
	}
	const float Dealt = AbilitySystem->ApplyDamage(Amount, Instigator);
	if (Commander)
	{
		// A blow tells the angel where the commander is (it turns to face it even from behind a wall).
		Brain.LastSeenAt = Now;
		Brain.LastKnown = {Commander->GetActorLocation().X, Commander->GetActorLocation().Y};
	}
	if (Mode.IsValid())
	{
		Mode->AddDamagePopup(GetActorLocation() + FVector(0.0, 0.0, 120.0), Dealt, IsBoss() || bAdvantage || IsDazeStunned());
	}
	// The hit's weight (BIBRules/BIBHitFeel.h) drives everything below and the commander's side (hit stop, camera, rumble).
	const BIB::Feel::EWeight HitWeight = Commander && Dealt > 0.f ? Commander->ClassifyHit(Dealt / Attack) : BIB::Feel::EWeight::Tick;
	if (Commander && Dealt > 0.f)
	{
		Commander->OnHitLanded(this, Dealt, bGuarded, HitWeight);
	}
	if (Commander && Dealt > 0.f && GetHP() > 0.f)
	{
		const BIB::Feel::FWeightFeel& Feel = BIB::Feel::FeelFor(HitWeight);
		const double Points = (BIB::Stagger::DazeForHit(Dealt, Attack, Bonus) + (HitWeight == BIB::Feel::EWeight::Tick ? 0.0 : Feel.StrainBonus))
			* (bGuarded ? 0.15 : 1.0);
		if (Daze.Add(Points, Now, BIB::Stagger::StunSeconds(Weight, Kind == EBIBEnemyKind::Boss)))
		{
			// Strain full: the angel BREAKS (the punish window); the hit shows as a stagger.
			CancelAttack();
			GuardUntil = -1.f;
			ABIBFx::StunBurst(GetWorld(), GetActorLocation());
			ApplyReaction(BIB::Feel::EReaction::Stagger, Commander->GetActorLocation());
			Commander->OnEnemyDazeStunned(this);
		}
		else if (bGuarded)
		{
			ApplyReaction(BIB::Feel::EReaction::Recoil, Commander->GetActorLocation(), false);
		}
		else
		{
			// Reaction scaled to the hit and to this angel: recoil, flinch, stagger, knockback, launch or knockdown. A charge or a
			// slam has super armour: hits lighter than heavy only jolt it (heavy hits still break through).
			if (bSuperArmor && IsCommittedForCrowd() && HitWeight < BIB::Feel::EWeight::Heavy)
			{
				ApplyReaction(BIB::Feel::EReaction::Recoil, Commander->GetActorLocation(), false);
			}
			else
			{
				const BIB::Feel::EReaction Reaction = BIB::Feel::ResolveReaction(Daze, Weight, HitWeight, Now);
				if (Reaction != BIB::Feel::EReaction::None)
				{
					ApplyReaction(Reaction, Commander->GetActorLocation());
				}
			}
		}
		// Hit stop: the angel freezes with the commander for the hit's weight (ticks never freeze).
		const double RealNow = FPlatformTime::Seconds();
		// Combat feel: the hit's weight sets the stop (BIBRules/BIBHitFeel.h, goal 1), scaled by Settings > Combat feel (BIBCombatFeel).
		const BIBCombatFeel::FConfig& SettingsFeel = Commander->GetFeelConfig();
		const double Stop = BIB::Feel::HitstopSeconds(HitWeight, IsBoss()) * Commander->FeelHitStopMult();
		if (Stop > 0.0)
		{
			HitStopUntilReal = FMath::Max(HitStopUntilReal, RealNow + Stop);
			CustomTimeDilation = 0.02f;
		}
		if (SettingsFeel.FlashScale() > 0.f)
		{
			FlashUntilReal = RealNow + (HitWeight == BIB::Feel::EWeight::Tick ? 0.03 : 0.07) * SettingsFeel.FlashScale();
			FlashBrightness = 1.f + 2.f * FMath::Min(1.f, SettingsFeel.FlashScale());   // 3 at 100 %
		}
	}
	if (GetHP() <= 0.f)
	{
		Die();
	}
	else
	{
		UpdateLabelAndColour();
	}
	return Dealt;
}

void ABIBEnemy::SetElement(int32 InElement)
{
	ElementIndex = InElement >= 0 && InElement < BIB::ElementCount ? InElement : -1;
	UpdateLabelAndColour();
}

void ABIBEnemy::Die()
{
	BIBMotionEvents::EnemyDied(this);
	BIBMotionEvents::Shared(TEXT("angel-death-dissolve"), this);
	TArray<FGameplayTag> Dead;
	Dead.Add(BIBTags::StateDead());
	AbilitySystem->SyncMirroredTags(TEXT("Life"), Dead);
	AbilitySystem->ClearMirroredTags(TEXT("Status"));
	StatusBook.ClearAll();
	if (ABIBPlayerCharacter* Player = GetTarget())
	{
		Player->OnEnemyDied(this);
	}
	for (FBIBAngelShot& Shot : Shots)
	{
		if (AActor* Visual = Shot.Visual.Get())
		{
			Visual->Destroy();
		}
	}
	Shots.Reset();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetLifeSpan(0.5f);
	OnEnemyDied();
	if (Mode.IsValid())
	{
		Mode->OnEnemyKilled(this);
	}
}

void ABIBEnemy::Stun(float Seconds)
{
	StunTime = FMath::Max(StunTime, Seconds);
	CancelAttack();
	RefreshStatusTags();
}

bool ABIBEnemy::ApplyKitStatus(BIB::Kits::EStatus Status, double Seconds, double Power, int32 Stacks, int32 SourceMember)
{
	if (!IsAlive())
	{
		return false;
	}
	const bool bApplied = StatusBook.Apply(Status, Seconds, Power, Stacks, SourceMember);
	if (bApplied)
	{
		if (StatusBook.Disabled())
		{
			CancelAttack();
		}
		RefreshStatusTags();
	}
	return bApplied;
}

void ABIBEnemy::RefreshStatusTags()
{
	TArray<FGameplayTag> Desired;
	for (int32 Index = 1; Index < BIB::Kits::StatusCount; ++Index)
	{
		const BIB::Kits::EStatus Status = static_cast<BIB::Kits::EStatus>(Index);
		if (StatusBook.Has(Status))
		{
			Desired.AddUnique(BIBTags::Find(BIB::Kits::StatusRule(Status).Tag));
		}
	}
	if (StunTime > 0.f || IsDazeStunned())
	{
		Desired.AddUnique(BIBTags::Find(BIB::Kits::StatusRule(BIB::Kits::EStatus::Stun).Tag));
	}
	bStatusTagsActive = Desired.Num() > 0;
	AbilitySystem->SyncMirroredTags(TEXT("Status"), Desired);
}

void ABIBEnemy::ApplyKitImpulse(const FVector& Velocity)
{
	if (!IsAlive() || Velocity.IsNearlyZero())
	{
		return;
	}
	const double Scale = IsBoss() ? 0.25 : 1.0;
	LaunchCharacter(FVector(Velocity.X * Scale, Velocity.Y * Scale, Velocity.Z * Scale + Velocity.Size2D() * 0.1 * Scale), true, true);
}

void ABIBEnemy::TickStatuses(float DeltaSeconds, ABIBPlayerCharacter* Target)
{
	if (StatusBook.TotalStacks() == 0 && StunTime <= 0.f && !bStatusTagsActive)
	{
		return;
	}
	std::vector<BIB::Kits::FStatusEvent> Events;
	StatusBook.Tick(DeltaSeconds, Events);
	for (const BIB::Kits::FStatusEvent& Event : Events)
	{
		if (Event.Type == BIB::Kits::FStatusEvent::EType::DotDamage || Event.Type == BIB::Kits::FStatusEvent::EType::ShockDischarge)
		{
			if (Target)
			{
				Target->OnKitStatusDamage(this, Event.Amount, Event.Source);
			}
			else
			{
				ReceiveDamage(static_cast<float>(Event.Amount * StatusBook.DamageTakenMult()));
			}
			if (!IsAlive())
			{
				return;
			}
		}
	}
	RefreshStatusTags();
}

ABIBEnemy* ABIBEnemy::FindNearestOtherEnemy() const
{
	ABIBEnemy* Nearest = nullptr;
	double Best = TNumericLimits<double>::Max();
	if (Mode.IsValid())
	{
		for (ABIBEnemy* Other : Mode->GetLiveEnemies())
		{
			if (Other == this)
			{
				continue;
			}
			const double Distance = FVector::DistSquared2D(GetActorLocation(), Other->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = Other;
			}
		}
	}
	return Nearest;
}

void ABIBEnemy::ReceiveImpact(ABIBPlayerCharacter* Commander, BIB::Feel::EWeight HitWeight)
{
	if (!Commander || !IsAlive() || HitWeight == BIB::Feel::EWeight::Tick)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bArmoured = bSuperArmor && IsCommittedForCrowd() && HitWeight < BIB::Feel::EWeight::Heavy;
	const BIB::Feel::EReaction Reaction = bArmoured ? BIB::Feel::EReaction::Recoil : BIB::Feel::ResolveReaction(Daze, Weight, HitWeight, Now);
	ApplyReaction(Reaction, Commander->GetActorLocation(), !bArmoured);
	const double RealNow = FPlatformTime::Seconds();
	const double Stop = BIB::Feel::HitstopSeconds(HitWeight, IsBoss());
	if (Stop > 0.0)
	{
		HitStopUntilReal = FMath::Max(HitStopUntilReal, RealNow + Stop);
		CustomTimeDilation = 0.02f;
	}
	FlashUntilReal = RealNow + 0.07;
	const FVector ToEnemy = (GetActorLocation() - Commander->GetActorLocation()).GetSafeNormal2D();
	const FVector At = GetActorLocation() - ToEnemy * BodyRadius + FVector(0.0, 0.0, 30.0);
	ABIBFx::HitSpark(GetWorld(), At, BIBFxColours::Gold, static_cast<float>(BIB::Feel::FeelFor(HitWeight).SparkScale));
	Commander->ApplyHitFeel(this, HitWeight, At);
}

void ABIBEnemy::ApplyReaction(BIB::Feel::EReaction Reaction, const FVector& From, bool bMove)
{
	if (Reaction == BIB::Feel::EReaction::None)
	{
		return;
	}
	const BIB::Feel::FReactionFeel& Feel = BIB::Feel::Tuning().Reaction[static_cast<int32>(Reaction)];
	++BIB::Feel::Stats().Reactions[static_cast<int32>(Reaction)];
	FVector Away = GetActorLocation() - From;
	Away.Z = 0.0;
	Away = Away.GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		Away = -GetActorForwardVector();
	}
	// A bigger reaction replaces a smaller one that is still playing; the same or smaller restarts only when it ended.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bPlaying = Now - ReactionStart < static_cast<float>(BIB::Feel::Tuning().Reaction[static_cast<int32>(ReactionKind)].Seconds);
	if (!bPlaying || Reaction >= ReactionKind)
	{
		ReactionKind = Reaction;
		ReactionStart = Now;
		ReactionDir = Away;
	}
	if (Feel.bInterrupts)
	{
		CancelAttack();
		BIBMotionEvents::Shared(TEXT("flinch-pop"), this);
	}
	if (bMove && (Feel.PushSpeed > 0.0 || Feel.LiftSpeed > 0.0))
	{
		// Bosses and brutes are heavy: they move less and are never thrown into the air.
		const double Scale = IsBoss() ? 0.3 : (Weight == BIB::Stagger::EWeight::Medium ? 0.7 : 1.0);
		const double Lift = IsBoss() ? 0.0 : Feel.LiftSpeed * Scale;
		LaunchCharacter(Away * Feel.PushSpeed * Scale + FVector(0.0, 0.0, Lift), true, Lift > 0.0);
	}
}

void ABIBEnemy::UpdateReactionPose()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Tilt = static_cast<float>(BIB::Feel::ReactionTilt(ReactionKind, Now - ReactionStart));
	if (FMath::IsNearlyEqual(Tilt, LastTilt, 0.05f))
	{
		return;
	}
	LastTilt = Tilt;
	// Tilt the body away from the hit around the feet (the angels have no animations yet).
	const FVector LocalAway = GetActorRotation().UnrotateVector(ReactionDir).GetSafeNormal2D();
	FVector Axis = FVector::CrossProduct(FVector::UpVector, LocalAway);
	if (Axis.IsNearlyZero())
	{
		Axis = FVector::RightVector;
	}
	const FQuat Turn(Axis.GetSafeNormal(), FMath::DegreesToRadians(Tilt));
	const FVector Pivot(0.0, 0.0, PivotZ);
	BodyComp->SetRelativeLocationAndRotation(Pivot + Turn.RotateVector(BodyBaseLocation - Pivot), Turn);
	HeadComp->SetRelativeLocation(Pivot + Turn.RotateVector(HeadBaseLocation - Pivot));
}

void ABIBEnemy::Knockback(const FVector& FromLocation, float Strength)
{
	FVector Direction = GetActorLocation() - FromLocation;
	Direction.Z = 0.0;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}
	const float Scale = IsBoss() ? 0.25f : 1.f;
	LaunchCharacter(Direction * Strength * Scale + FVector(0.0, 0.0, Strength * 0.25f * Scale), true, true);
}

void ABIBEnemy::AddPull(const FVector& VelocityCmPerSecond)
{
	PendingPull += VelocityCmPerSecond * (IsBoss() ? 0.3 : 1.0);
}

void ABIBEnemy::SetGravityInverted(bool bInverted)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->GravityScale = bInverted ? -1.f : 1.f;
	Movement->SetMovementMode(MOVE_Falling);
	LaunchCharacter(FVector(0.0, 0.0, bInverted ? 250.0 : -50.0), false, true);
}

void ABIBEnemy::UpdateLabelAndColour()
{
	ABIBPlayerCharacter* Target = GetTarget();
	const bool bJudgeActive = Target && Target->IsJudgeActive();
	FString StateText;
	FLinearColor Colour(0.85f, 0.85f, 0.9f);
	if (bJudgeActive)
	{
		switch (JudgeState)
		{
		case EBIBJudgeState::Innocent: StateText = TEXT(" [INNOCENT]"); Colour = FLinearColor(0.2f, 0.45f, 1.f); break;
		case EBIBJudgeState::Suspect: StateText = TEXT(" [SUSPECT]"); Colour = FLinearColor(1.f, 0.6f, 0.1f); break;
		case EBIBJudgeState::Guilty: StateText = TEXT(" [GUILTY]"); Colour = FLinearColor(0.9f, 0.05f, 0.05f); break;
		}
		if (GavelStacks > 0)
		{
			StateText += FString::Printf(TEXT(" gavel x%d"), GavelStacks);
		}
	}
	else if (IsBoss())
	{
		Colour = Kind == EBIBEnemyKind::Boss ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(1.f, 0.95f, 0.6f);
	}
	else if (Kind == EBIBEnemyKind::Ranged)
	{
		Colour = FLinearColor(0.45f, 0.95f, 0.45f);
	}
	else if (Kind == EBIBEnemyKind::Mage)
	{
		Colour = FLinearColor(0.75f, 0.5f, 1.f);
	}
	else if (Kind == EBIBEnemyKind::Assassin)
	{
		Colour = FLinearColor(1.f, 0.4f, 0.4f);
	}
	if (StunTime > 0.f)
	{
		StateText += TEXT(" (stunned)");
	}
	if (AttackWindup > 0.f || SlamWindup > 0.f)
	{
		Colour = bCurrentUnblockable ? BIBFxColours::Rose * 1.6f : FLinearColor(1.f, 0.75f, 0.2f); // attack telegraph: gold, rose = unblockable
	}
	if (IsGuarding())
	{
		Colour = BIBFxColours::Cyan;
	}
	if (IsDazeStunned())
	{
		Colour = FLinearColor::LerpUsingHSV(Colour, BIBFxColours::Gold, 0.5f);
		StateText += TEXT(" BROKEN");
	}
	FLinearColor BodyColour = Colour;
	if (FPlatformTime::Seconds() < FlashUntilReal)
	{
		BodyColour = FLinearColor(FlashBrightness, FlashBrightness, FlashBrightness);   // hit flash
	}
	if (BodyMaterial && bTintByState)
	{
		BodyMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), BodyColour);
	}
	if (HeadMaterial && bTintByState)
	{
		HeadMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), Colour * 0.8f);
	}
	if (StatusBook.TotalStacks() > 0)
	{
		StateText += FString::Printf(TEXT(" [%s]"), UTF8_TO_TCHAR(StatusBook.Describe().c_str()));
	}
	const FString ElementText = ElementIndex >= 0
		? FString::Printf(TEXT("[%s] "), UTF8_TO_TCHAR(BIB::Element::Name(BIB::Element::FromIndex(ElementIndex)))) : FString();
	// Name and state only; health and Daze are the bars under it (UpdateBars).
	LabelComp->SetText(FText::FromString(FString::Printf(TEXT("%s%s%s"), *ElementText, *GetKindName().ToUpper(), *StateText)));
	UpdateBars();
	LabelComp->SetTextRenderColor(Colour.ToFColor(true));
}

// ============================================================================ Zenless-style state

bool ABIBEnemy::IsDazeStunned() const
{
	return GetWorld() && Daze.Stunned(GetWorld()->GetTimeSeconds());
}

bool ABIBEnemy::IsChainOpen() const
{
	return GetWorld() && IsAlive() && Daze.ChainOpen(GetWorld()->GetTimeSeconds());
}

float ABIBEnemy::GetDazeFraction() const
{
	return GetWorld() ? static_cast<float>(Daze.Fraction(GetWorld()->GetTimeSeconds())) : 0.f;
}

bool ABIBEnemy::IsGuarding() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < GuardUntil;
}

BIB::Angel::FMind* ABIBEnemy::GetMind() const
{
	UBIBGameInstance* Game = GetGameInstance<UBIBGameInstance>();
	return Game ? &Game->GetAngelMind() : nullptr;
}

BIB::Angel::ERole ABIBEnemy::GetRole() const
{
	switch (Kind)
	{
	case EBIBEnemyKind::Brute: return BIB::Angel::ERole::Brute;
	case EBIBEnemyKind::Ranged: return BIB::Angel::ERole::Ranged;
	case EBIBEnemyKind::Mage: return BIB::Angel::ERole::Mage;
	case EBIBEnemyKind::Assassin: return BIB::Angel::ERole::Assassin;
	case EBIBEnemyKind::MiniBoss: return BIB::Angel::ERole::MiniBoss;
	case EBIBEnemyKind::Boss: return BIB::Angel::ERole::Boss;
	default: return BIB::Angel::ERole::Melee;
	}
}

FVector ABIBEnemy::Separation() const
{
	FVector Push = FVector::ZeroVector;
	if (!Mode.IsValid())
	{
		return Push;
	}
	for (const ABIBEnemy* Other : Mode->GetLiveEnemies())
	{
		if (Other == this)
		{
			continue;
		}
		FVector Away = GetActorLocation() - Other->GetActorLocation();
		Away.Z = 0.0;
		const double Distance = Away.Size();
		if (Distance > 1.0 && Distance < 420.0)
		{
			Push += Away / Distance * (1.0 - Distance / 420.0);
		}
	}
	return Push;
}

void ABIBEnemy::UpdateBars()
{
	const float Width = FMath::Clamp(BodyRadius * 3.2f, 110.f, 360.f);
	const float Health = FMath::Clamp(GetHealthFraction(), 0.f, 1.f);
	const float DazeShare = GetDazeFraction();
	// The cube is 100 cm; the bars sit just under the label, drawn in the label's plane (the label faces the camera).
	BarBackComp->SetRelativeScale3D(FVector(0.02, Width / 100.0, 0.16));
	BarBackComp->SetRelativeLocation(FVector(-1.0, 0.0, -26.0));
	BarFillComp->SetRelativeScale3D(FVector(0.03, FMath::Max(0.001f, Width * Health) / 100.0, 0.09));
	BarFillComp->SetRelativeLocation(FVector(0.0, (Width * (1.f - Health)) * 0.5f, -23.5));
	DazeFillComp->SetRelativeScale3D(FVector(0.03, FMath::Max(0.001f, Width * DazeShare) / 100.0, 0.04));
	DazeFillComp->SetRelativeLocation(FVector(0.0, (Width * (1.f - DazeShare)) * 0.5f, -30.0));
	if (DazeFillMaterial)
	{
		const bool bStunned = IsDazeStunned();
		const float Pulse = bStunned ? 0.6f + 0.4f * FMath::Sin(static_cast<float>(FPlatformTime::Seconds()) * 14.f) : 1.f;
		DazeFillMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), (bStunned ? BIBFxColours::Gold : BIBFxColours::Cyan) * Pulse);
	}
}

void ABIBEnemy::HitTarget(ABIBPlayerCharacter* Target, ABIBEnemy* Victim, float Damage, bool bMelee)
{
	if (Victim)
	{
		Victim->ReceiveDamage(Damage, this);
		return;
	}
	if (Target)
	{
		Target->ReceiveEnemyHit(this, Damage, AttackAbilityId, bMelee, bCurrentUnblockable);
	}
}

void ABIBEnemy::Tick(float DeltaSeconds)
{
	// Hit stop runs on real time: the angel freezes for a few hundredths of a second after a hit.
	if (CustomTimeDilation < 1.f && FPlatformTime::Seconds() >= HitStopUntilReal)
	{
		CustomTimeDilation = 1.f;
	}
	Super::Tick(DeltaSeconds);
	if (!IsAlive())
	{
		return;
	}
	UpdateReactionPose();
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	if (GavelStacks > 0 && Now >= GavelStacksExpireAt)
	{
		GavelStacks = 0;
	}

	if (!PendingPull.IsNearlyZero())
	{
		AddActorWorldOffset(PendingPull * DeltaSeconds, true);
		PendingPull = FVector::ZeroVector;
	}

	if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = Camera->GetCameraLocation() - LabelComp->GetComponentLocation();
		LabelComp->SetWorldRotation(FRotator(0.0, ToCamera.Rotation().Yaw, 0.0));
	}

	ABIBPlayerCharacter* Target = GetTarget();
	if (UBIBTactics* Tactics = UBIBTactics::Get(this))
	{
		Tactics->Refresh(Mode.Get(), Target);   // player model + squad board, once per frame
	}
	TickStatuses(DeltaSeconds, Target);
	if (!IsAlive())
	{
		return;
	}
	TickShots(DeltaSeconds, Target);   // shots already fired keep flying whatever happens to the shooter
	Daze.Tick(Now, DeltaSeconds);
	const bool bDazed = Daze.Stunned(Now);
	if (bWasDazeStunned && !bDazed)
	{
		RefreshStatusTags();
	}
	bWasDazeStunned = bDazed;
	if (!bPhaseTwo && IsBoss() && GetHealthFraction() < 0.5f)
	{
		// Second phase: faster, chains more.
		bPhaseTwo = true;
		ABIBFx::StunBurst(World, GetActorLocation());
		BIBMotionEvents::Shared(TEXT("phase-2-transition-burst"), this);
	}
	if (StunTime > 0.f || StatusBook.Disabled() || bDazed || Daze.Flinching(Now))
	{
		// Stunned (Judge, parry, Daze) or Stun / Freeze from a kit, or flinching from a hit: no movement, no attacks.
		StunTime = FMath::Max(0.f, StunTime - DeltaSeconds);
		CancelAttack();
		GuardUntil = -1.f;
		UpdateLabelAndColour();
		return;
	}
	if (!Target || !Target->IsCombatActive())
	{
		UpdateLabelAndColour();
		return;
	}

	// An enemy under Illusion (Alchemist's Ring leader) turns on its nearest neighbour instead of the operator.
	ABIBEnemy* Victim = StatusBook.Illusory() ? FindNearestOtherEnemy() : nullptr;
	const FVector Goal = Victim ? Victim->GetActorLocation() : Target->GetActorLocation();
	FVector ToTarget = Goal - GetActorLocation();
	ToTarget.Z = 0.0;
	const float Distance = static_cast<float>(ToTarget.Size());
	const FVector Direction = ToTarget.GetSafeNormal();
	const float MoveMult = static_cast<float>(StatusBook.MoveMult());
	const float DealtScale = static_cast<float>(StatusBook.DamageDealtMult());
	if (Victim && !Direction.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0, Direction.Rotation().Yaw, 0.0));   // against the commander, turning is rate-limited (TickBrain)
	}
	if (StatusBook.Feared())
	{
		// Frightened enemies run away and do not attack.
		CancelAttack();
		AddMovementInput(-Direction, MoveMult);
		UpdateLabelAndColour();
		return;
	}

	// The chapter boss's slam (its Nova, chosen by the brain): a telegraphed circle, then damage inside it. Super armour.
	if (SlamWindup > 0.f)
	{
		SlamWindup -= DeltaSeconds;
		if (SlamWindup <= 0.f)
		{
			SlamWindup = -1.f;
			ABIBFx::ShockRing(World, GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.0),
				BIBTuning::BossSlamRadius, BIBFxColours::Rose, 0.5f);
			if (!Victim && Distance <= BIBTuning::BossSlamRadius)
			{
				Target->ReceiveEnemyHit(this, BIBTuning::BossSlamDamage * SlamDamageMult * DealtScale, 4, false);
			}
			FinishAttack();
		}
		UpdateLabelAndColour();
		return;
	}

	AttackCooldown -= DeltaSeconds;
	if (Victim)
	{
		TickIllusion(DeltaSeconds, Victim, Distance, Direction, MoveMult, DealtScale);
	}
	else
	{
		TickBrain(DeltaSeconds, Target, Distance, Direction, MoveMult, DealtScale);
	}
	UpdateLabelAndColour();
}
