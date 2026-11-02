// Zenless-style combat feel and traversal for the commander's team (owner request 2026-10-09, Notion "Dev notes"; the owner
// also pointed at the Roblox game Anime Zero for the M1 / moves / ult loop and gave full creative control over movesets).
//
//  - Input buffer: attack, parry / dodge, moves, special, ultimate, switch and jump pressed up to 0.25 s early are kept and fire
//    as soon as they can, so nothing is "eaten" while an action plays ("moves feel unresponsive and lag behind inputs").
//  - Basic attack = a 4-hit string (BIBRules/BIBStagger.h FCombo): the finisher hits harder, adds Daze and knocks back. Dodge
//    and every ability cancel the string after its hit frame.
//  - Hit feel: sparks in the operator's colour, -12 degree slash streaks, hit stop on the angel and a lighter one on the
//    operator, camera jolts on heavy hits, a combo counter.
//  - Perfect dodge: dodging into a hit slows time and opens a dodge counter (LMB). Parries counter-strike.
//  - Daze stun -> CHAIN ATTACK: while an angel is stunned, switching brings the next operator in with a big strike.
//    An ordinary switch also enters with a quick strike.
//  - The angels' mind (BIBRules/BIBAngelMind.h) watches what the commander does; new counters are announced on screen.
//  - Traversal (MAIN §6: walk, run, jump, fall, climb, swim): wall climbing with a mantle at the top, swimming with surface
//    floating (Space up, Ctrl down), hard landings after long falls. Judge cannot climb or swim (Judge.md).
//  - Hit feel (owner request 2026-10-13, BIBRules/BIBHitFeel.h): every hit has a weight that sets the hit stop of the operator
//    AND the angel, the angel's reaction, a directional camera kick, a push-in on heavies, controller rumble and a sound layer;
//    the operator's body shows a wind-up, an impact and a recovery (procedural poses - no animation clips exist yet).
//    Ultimates open with a short, skippable cinematic beat (the world nearly stops, the camera frames the operator, the
//    operator cannot be hit) and end with a big impact on their last hit. A dodge cancels the recovery after a cast's last hit.
// Every number is PLACEHOLDER; the hit-feel numbers are in Config/DefaultGame.ini [BIB.CombatFeel].
#include "BIBGame/BIBPlayerCharacter.h"

#include "BIBGame/BIBAnimeStyle.h"
#include "BIBGame/BIBFeelConfig.h"
#include "BIBGame/BIBFx.h"
#include "BIBGame/BIBMotion.h"
#include "BIBGame/BIBMotionEvents.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBGame/BIBTuning.h"
#include "BIBGAS/BIBAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	// Input buffer, Perfect Dodge window, slow motion and the Phantom Counter are tuned in [BIB.CombatFeel] (BIBRules/BIBHitFeel.h).
	constexpr float ChainStrikeMult = 4.f;         // relay strike: x the incoming operator's attack
	constexpr float SwitchStrikeMult = 1.5f;
	constexpr float ClimbSpeed = 260.f;
	constexpr float SwimUpSpeed = 300.f;

	BIB::Angel::FMind* MindOf(const AActor* Actor)
	{
		UBIBGameInstance* Game = Actor ? Actor->GetGameInstance<UBIBGameInstance>() : nullptr;
		return Game ? &Game->GetAngelMind() : nullptr;
	}

	/** Settings: reduce motion turns off camera kicks, push-ins and rumble (camera shake off turns off kicks and push-ins). */
	void FeelSettings(const AActor* Actor, bool& bCameraFeel, bool& bRumble, float& SfxVolume)
	{
		bCameraFeel = true;
		bRumble = true;
		SfxVolume = 0.8f;
		UBIBGameInstance* Game = Actor ? Actor->GetGameInstance<UBIBGameInstance>() : nullptr;
		if (Game && Game->IsProfileReady())
		{
			const BIB::FSettings& Settings = Game->GetProfile().Meta.Settings;
			bCameraFeel = Settings.bCameraShake && !Settings.bReduceMotion;
			bRumble = !Settings.bReduceMotion;
			SfxVolume = FMath::Clamp(Settings.SfxVolume, 0, 100) / 100.f;
		}
	}
}

// ============================================================================ queries for the angels

float ABIBPlayerCharacter::GetActiveAttack() const
{
	const FBIBMember& Member = Active();
	return FMath::Max(1.f, Member.BaseAttack * Member.DamageMult);
}

bool ABIBPlayerCharacter::IsAirborneForAngels() const
{
	return GetCharacterMovement() && GetCharacterMovement()->IsFalling();
}

float ABIBPlayerCharacter::SecondsSinceSwitch() const
{
	return Now() - LastSwitchAt;
}

float ABIBPlayerCharacter::SecondsSinceAttack() const
{
	return Now() - LastAttackAt;
}

float ABIBPlayerCharacter::OutgoingHitMult() const
{
	// Hand-written kits swing the engine string through DoPunch, data kits with a single-hit basic attack too (their hits land
	// shortly after the cast). Data kits with their own multi-hit string keep its numbers (ComboStepNow stays -1).
	if (ComboStepNow >= 0 && Now() < PunchHitsUntil)
	{
		return static_cast<float>(BIB::Stagger::ComboStepMult(ComboStepNow));
	}
	return 1.f;
}

bool ABIBPlayerCharacter::IsEngineComboFinisher() const
{
	return ComboStepNow == BIB::Stagger::ComboLength - 1 && Now() < PunchHitsUntil;
}

BIB::Feel::EWeight ABIBPlayerCharacter::ClassifyHit(float Ratio) const
{
	// Unscoped hits (passives, kit effects without a cast) are classified by their size alone.
	BIB::Feel::FHitSource Hit = PendingHit;
	Hit.Ratio = Ratio;
	return BIB::Feel::Classify(Hit);
}

void ABIBPlayerCharacter::DebugSetAngelExpertise(float Value)
{
	if (BIB::Angel::FMind* Mind = MindOf(this))
	{
		for (int32 Index = 0; Index < BIB::Angel::CounterCount; ++Index)
		{
			Mind->SetExpertise(static_cast<BIB::Angel::ECounter>(Index), Value);
		}
	}
}

// ============================================================================ input buffer

bool ABIBPlayerCharacter::IsBufferable(EBIBInput Input) const
{
	switch (Input)
	{
	case EBIBInput::Attack:
	case EBIBInput::Parry:
	case EBIBInput::Move1:
	case EBIBInput::Move2:
	case EBIBInput::Move3:
	case EBIBInput::Special:
	case EBIBInput::UltimateE:
	case EBIBInput::UltimateR:
	case EBIBInput::SwitchOperator:
	case EBIBInput::Jump:
		return true;
	default:
		return false;
	}
}

void ABIBPlayerCharacter::ConsumeBuffer(EBIBInput Input)
{
	const int32 Index = static_cast<int32>(Input);
	if (Index >= 0 && Index < static_cast<int32>(EBIBInput::Count))
	{
		BufferedAt[Index] = -100.f;
	}
}

void ABIBPlayerCharacter::ClearBuffers()
{
	for (float& At : BufferedAt)
	{
		At = -100.f;
	}
}

void ABIBPlayerCharacter::ObserveSlot(EBIBSlot Slot)
{
	BIB::Angel::FMind* Mind = MindOf(this);
	switch (Slot)
	{
	case EBIBSlot::Punch:
		LastAttackAt = Now();
		if (Mind) { Mind->Observe(BIB::Angel::ETrait::MeleeSpam); }
		break;
	case EBIBSlot::Dodge:
		if (Mind) { Mind->Observe(BIB::Angel::ETrait::Dodging); }
		Accent();
		break;
	case EBIBSlot::Parry:
		if (Mind) { Mind->Observe(BIB::Angel::ETrait::Parrying); }
		Accent();
		break;
	default:
		Accent(Slot == EBIBSlot::Ultimate);   // every cast pops a frame; the ultimate is an impact frame
		LastAttackAt = Now();
		ComboStepNow = -1;   // an ability is not part of the basic string
		if (Mind) { Mind->Observe(BIB::Angel::ETrait::AbilityUse); }
		break;
	}
}

// ============================================================================ feedback helpers

void ABIBPlayerCharacter::ShowBanner(const FString& Text, const FString& Sub, const FLinearColor& Colour, double Seconds)
{
	BannerText = Text;
	BannerSubText = Sub;
	BannerColour = Colour;
	BannerUntilReal = FPlatformTime::Seconds() + Seconds;
}

void ABIBPlayerCharacter::SetSlowMo(float Dilation, double RealSeconds)
{
	if (bUltCinematic)
	{
		return;   // the ultimate's cinematic beat owns time until it ends
	}
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, Dilation);
		SlowMoUntilReal = FPlatformTime::Seconds() + RealSeconds;
	}
}

void ABIBPlayerCharacter::Shake(float Strength, float Seconds)
{
	if (ABIBPlayerController* PC = GetPC())
	{
		PC->AddShake(Strength, Seconds);
	}
}

bool ABIBPlayerCharacter::ScreenPoint(const FVector& World, FVector2D& Out) const
{
	ABIBPlayerController* PC = GetPC();
	return PC && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, World, Out, false);
}

void ABIBPlayerCharacter::Accent(bool bImpact)
{
	bool bCalm = false;
	if (UBIBGameInstance* Game = GetGameInstance<UBIBGameInstance>())
	{
		bCalm = Game->IsProfileReady() && Game->GetProfile().Meta.Settings.bReduceMotion;   // no flashes with reduce motion
	}
	ABIBAnimeStyleVolume::RequestAccent(GetWorld(), bImpact && !bCalm);
}

ABIBEnemy* ABIBPlayerCharacter::NearestEnemy(float MaxDistance) const
{
	ABIBEnemy* Best = nullptr;
	double BestDistance = MaxDistance;
	if (const ABIBGameMode* GameMode = GetMode())
	{
		for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			const double Distance = FVector::Dist2D(Enemy->GetActorLocation(), GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Enemy;
			}
		}
	}
	return Best;
}

// ============================================================================ hits

void ABIBPlayerCharacter::OnHitLanded(ABIBEnemy* Enemy, float Dealt, bool bGuarded, BIB::Feel::EWeight Weight)
{
	if (!Enemy || Dealt <= 0.f)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Ratio = Dealt / GetActiveAttack();
	const BIB::Feel::FWeightFeel& Feel = BIB::Feel::FeelFor(Weight);
	const FVector ToEnemy = (Enemy->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const FVector At = Enemy->GetActorLocation() - ToEnemy * Enemy->GetBodyRadius() + FVector(0.0, 0.0, 30.0);
	const FLinearColor Colour = bGuarded ? BIBFxColours::Cyan : FLinearColor::LerpUsingHSV(Hud.Colour, BIBFxColours::Rose, 0.35f);
	if (HitFrame != GFrameCounter)
	{
		HitFrame = GFrameCounter;
		HitsThisFrame = 0;
		bAreaCounted = false;
	}
	++HitsThisFrame;
	// Sparks sized by the hit's weight: one per hit, but a big area hit does not spawn 30 of them.
	if (HitsThisFrame <= 6)
	{
		ABIBFx::HitSpark(World, At, Colour, static_cast<float>(Feel.SparkScale) * FMath::Clamp(Ratio, 0.7f, 1.5f));
	}
	BIB::Angel::FMind* Mind = MindOf(this);
	if (Mind)
	{
		if (HitsThisFrame >= 3 && !bAreaCounted)
		{
			bAreaCounted = true;
			Mind->Observe(BIB::Angel::ETrait::AreaUse);
		}
		if (FVector::Dist2D(Enemy->GetActorLocation(), GetActorLocation()) > 700.0)
		{
			Mind->Observe(BIB::Angel::ETrait::RangedPoke, 0.5);
		}
	}
	// Combo counter.
	const float T = Now();
	ComboHits = T - LastHitAt < 2.5f ? ComboHits + 1 : 1;
	LastHitAt = T;
	if (Weight != BIB::Feel::EWeight::Tick)
	{
		LastStrikeHitAt = T;   // a tick of damage over time is not the swing connecting
	}
	if (ComboHits == 1)
	{
		NextComboMilestone = 10;
	}
	if (ComboHits >= NextComboMilestone)
	{
		BIBMotionEvents::ComboMilestone(ComboHits);
		NextComboMilestone = NextComboMilestone < 50 ? NextComboMilestone + (NextComboMilestone < 25 ? 15 : 25) : NextComboMilestone + 50;
	}
	if (Ratio >= 4.f || Weight == BIB::Feel::EWeight::UltimateFinisher || (Enemy->IsDazeStunned() && Ratio >= 2.f))
	{
		FVector2D Screen;
		if (ScreenPoint(Enemy->GetActorLocation() + FVector(0.0, 0.0, 140.0), Screen))
		{
			BIBMotionEvents::BigHit(Screen, Dealt);
		}
	}
	ApplyHitFeel(Enemy, Weight, At);
}

void ABIBPlayerCharacter::ApplyHitFeel(ABIBEnemy* Enemy, BIB::Feel::EWeight Weight, const FVector& At)
{
	if (!Enemy)
	{
		return;
	}
	UWorld* World = GetWorld();
	const BIB::Feel::FWeightFeel& Feel = BIB::Feel::FeelFor(Weight);
	const bool bTick = Weight == BIB::Feel::EWeight::Tick;
	const FVector ToEnemy = (Enemy->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	// Felt once per frame at the heaviest weight that landed in it (an area hit feels like one hit).
	if (FeelFrame != GFrameCounter)
	{
		FeelFrame = GFrameCounter;
		FeelWeightThisFrame = -1;
	}
	const int32 WeightIndex = static_cast<int32>(Weight);
	BIB::Feel::FStats& Stats = BIB::Feel::Stats();
	if (bTick)
	{
		++Stats.Weights[WeightIndex];
		return;
	}
	if (WeightIndex <= FeelWeightThisFrame)
	{
		return;
	}
	FeelWeightThisFrame = WeightIndex;
	++Stats.Weights[WeightIndex];
	Stats.MaxHitstopFrames = FMath::Max(Stats.MaxHitstopFrames, Feel.HitstopFrames);
	const double RealNow = FPlatformTime::Seconds();
	// Hit stop: the operator freezes with the angel (BIBEnemy.cpp freezes the angel for the same time); the weight sets it
	// (BIBRules/BIBHitFeel.h) and Settings > Combat feel scales or switches it (BIBCombatFeel, FeelHitStopMult).
	const double Stop = BIB::Feel::HitstopSeconds(Weight, Enemy->IsBoss()) * FeelHitStopMult();
	if (Stop > 0.0)
	{
		SelfHitStopUntilReal = FMath::Max(SelfHitStopUntilReal, RealNow + Stop);
		CustomTimeDilation = 0.02f;
	}
	// The hit lands on a drawn frame of the 12 fps look (never "between frames"); heavies get the two-tone impact frame.
	if (Feel.bAccentFrame)
	{
		Accent(Feel.bImpactFrame);
	}
	bool bCameraFeel = true;
	bool bRumble = true;
	float SfxVolume = 1.f;
	FeelSettings(this, bCameraFeel, bRumble, SfxVolume);
	if (bCameraFeel)
	{
		if (ABIBPlayerController* PC = GetPC())
		{
			PC->AddKick(ToEnemy, static_cast<float>(Feel.KickDegrees), static_cast<float>(Feel.KickSeconds));
		}
		if (Feel.PushInCm > 0.0)
		{
			PushInPeak = static_cast<float>(Feel.PushInCm);
			PushInStartReal = RealNow;
			++Stats.PushIns;
		}
	}
	if (bRumble)
	{
		Rumble(static_cast<float>(Feel.RumbleIntensity), static_cast<float>(Feel.RumbleSeconds));
	}
	PlayFeelSounds(BIBFeelConfig::HitSounds(Weight), At);
	// The operator's body lunges into the hit.
	PulseAge = 0.f;
	PulseLean = static_cast<float>(Feel.ImpactLean);
	if (Weight == BIB::Feel::EWeight::UltimateFinisher)
	{
		// The ultimate's big screen-space impact: impact frame, slow motion, a ring across the floor.
		const BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
		SetSlowMo(static_cast<float>(Tuning.UltFinisherDilation), Tuning.UltFinisherRealSeconds);
		ABIBFx::ShockRing(World, Enemy->GetActorLocation() - FVector(0.0, 0.0, 80.0), 900.f, Hud.Colour, 0.6f);
		ABIBFx::Spawn(World, EBIBFxShape::Sphere, Enemy->GetActorLocation(), FRotator::ZeroRotator, FVector(60.f), FVector(900.f), BIBFxColours::White, 0.35f, 9.f);
	}
}

void ABIBPlayerCharacter::OnGuardBroken(ABIBEnemy* Enemy)
{
	if (Enemy)
	{
		ABIBFx::StunBurst(GetWorld(), Enemy->GetActorLocation());
		Enemy->SetNextHitBonus(BIB::Stagger::EBonus::ComboFinisher);
	}
	ShowBanner(TEXT("GUARD BREAK"), FString(), BIBFxColours::Cyan, 0.8);
	Accent(true);
	Shake(2.f, 0.2f);
}

void ABIBPlayerCharacter::OnEnemyDazeStunned(ABIBEnemy* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	ChainTarget = Enemy;
	ChainUntil = Now() + static_cast<float>(BIB::Stagger::ChainWindow);
	SetSlowMo(0.3f, 0.35);
	Shake(2.5f, 0.25f);
	Accent(true);
	const bool bCanChain = Members[(ActiveIndex + 1) % 3].bAlive || Members[(ActiveIndex + 2) % 3].bAlive;
	ShowBanner(TEXT("BROKEN"), bCanChain ? TEXT("TAB  RELAY STRIKE") : TEXT("x1.6 DAMAGE"), BIBFxColours::Gold, BIB::Stagger::ChainWindow);
}

void ABIBPlayerCharacter::PerfectDodge(ABIBEnemy* Attacker)
{
	const float T = Now();
	if (T < PerfectDodgeReadyAt)
	{
		return;
	}
	const BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
	PerfectDodgeReadyAt = T + 0.6f;
	DodgeCounterUntil = T + static_cast<float>(Tuning.PhantomCounterWindow);
	SetSlowMo(static_cast<float>(Tuning.PerfectDodgeDilation), Tuning.PerfectDodgeRealSeconds);
	Accent(true);
	PlayFeelSounds(BIBFeelConfig::NamedSounds(TEXT("PerfectDodge")), GetActorLocation());
	Rumble(0.35f, 0.12f);
	ShowBanner(TEXT("PERFECT DODGE"), TEXT("LMB  PHANTOM COUNTER"), BIBFxColours::Violet, 1.0);
	BIBMotionEvents::Defense(2);
	BIBMotionEvents::Shared(TEXT("perfect-dodge-afterimage"), this, BIBMotionEvents::FacesLeft(this), TEXT("dodge"));
	ABIBFx::Spawn(GetWorld(), EBIBFxShape::Sphere, GetActorLocation(), FRotator::ZeroRotator, FVector(80.f), FVector(520.f), BIBFxColours::Violet, 0.35f, 5.f);
	if (BIB::Angel::FMind* Mind = MindOf(this))
	{
		Mind->Observe(BIB::Angel::ETrait::Dodging, 1.0);
	}
	(void)Attacker;
}

bool ABIBPlayerCharacter::TryDodgeCounter()
{
	if (Now() >= DodgeCounterUntil)
	{
		return false;
	}
	ABIBEnemy* Target = NearestEnemy(1100.f);
	DodgeCounterUntil = -1.f;
	if (!Target)
	{
		return false;
	}
	// Dash through to the attacker and strike.
	const FVector To = Target->GetActorLocation() - (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * (Target->GetBodyRadius() + 70.f);
	TeleportTo(FVector(To.X, To.Y, GetActorLocation().Z), (Target->GetActorLocation() - To).Rotation(), false, true);
	ABIBFx::SlashStreak(GetWorld(), Target->GetActorLocation() + FVector(0.0, 0.0, 40.0), Target->GetActorLocation() - GetActorLocation(), BIBFxColours::Violet, 420.f, 3);
	// The Phantom Counter: a heavy hit (hit stop, knockback reaction, camera kick from BIBRules/BIBHitFeel.h).
	StartPose(0.f, 0.25f, 1.2f);
	Target->SetNextHitBonus(BIB::Stagger::EBonus::DodgeCounter);
	{
		const FBIBScopedHit Scope(this, BIB::Feel::ESource::Counter);
		DealDamage(Target, Active().BaseAttack * static_cast<float>(BIB::Feel::Tuning().PhantomCounterMult));
	}
	LastAttackAt = Now();
	return true;
}

bool ABIBPlayerCharacter::TryDashAttack()
{
	// Sprinting into an attack lunges through the first angel in the way (Zenless dash attack / Anime Zero lunge).
	const float T = Now();
	if (T < DashAttackReadyAt || !Held(EBIBInput::Run) || GetVelocity().Size2D() < 520.0 || GetCharacterMovement()->IsFalling())
	{
		return false;
	}
	if (Active().Kit == EBIBKitType::Judge && JudgeData.Mode != EBIBJudgeMode::Judge)
	{
		return false;
	}
	DashAttackReadyAt = T + 1.2f;
	const FVector Dir = GetVelocity().GetSafeNormal2D();
	SetActorRotation(Dir.Rotation());
	LaunchCharacter(Dir * 1500.0, true, false);
	BIBMotionEvents::Shared(TEXT("dash-attack-trail"), this, BIBMotionEvents::FacesLeft(this), TEXT("dash-attack"));
	ABIBFx::SlashStreak(GetWorld(), GetActorLocation() + Dir * 220.0 + FVector(0.0, 0.0, 30.0), Dir, Hud.Colour, 520.f, 3);
	ABIBFx::Spawn(GetWorld(), EBIBFxShape::Slash, GetActorLocation() + Dir * 120.0, Dir.Rotation(), FVector(200.f, 30.f, 30.f), FVector(480.f, 4.f, 4.f),
		BIBFxColours::White, 0.2f, 6.f);
	const FVector Ahead = GetActorLocation() + Dir * 260.0;
	StartPose(0.f, 0.22f, 1.f);
	for (ABIBEnemy* Enemy : EnemiesInRadius(Ahead, 300.f))
	{
		Enemy->SetNextHitBonus(BIB::Stagger::EBonus::ComboFinisher);
		const FBIBScopedHit Scope(this, BIB::Feel::ESource::Counter, false, static_cast<int32>(BIB::Feel::EWeight::Medium));
		DealDamage(Enemy, Active().BaseAttack * 1.4f);
	}
	LastAttackAt = T;
	if (BIB::Angel::FMind* Mind = MindOf(this))
	{
		Mind->Observe(BIB::Angel::ETrait::MeleeSpam);
	}
	Shake(0.9f, 0.12f);
	Accent();
	return true;
}

void ABIBPlayerCharacter::SwitchStrike(bool bChain)
{
	ABIBEnemy* Target = bChain && ChainTarget.IsValid() && ChainTarget->IsAlive() ? ChainTarget.Get() : NearestEnemy(bChain ? 2000.f : 700.f);
	if (!Target)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (bChain)
	{
		// Chain attack: the incoming operator drops in on the stunned angel; everything around takes the blast.
		const FVector To = Target->GetActorLocation() - (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * (Target->GetBodyRadius() + 90.f);
		TeleportTo(FVector(To.X, To.Y, GetActorLocation().Z), (Target->GetActorLocation() - To).Rotation(), false, true);
		const FVector Ground = Target->GetActorLocation() - FVector(0.0, 0.0, 80.0);
		ABIBFx::Spawn(World, EBIBFxShape::Pillar, Target->GetActorLocation(), FRotator::ZeroRotator, FVector(60.f, 60.f, 1400.f),
			FVector(420.f, 420.f, 200.f), Hud.Colour, 0.45f, 10.f);
		ABIBFx::ShockRing(World, Ground, 420.f, BIBFxColours::Gold, 0.45f);
		ABIBFx::SlashStreak(World, Target->GetActorLocation() + FVector(0.0, 0.0, 50.0), Target->GetActorLocation() - GetActorLocation(), BIBFxColours::Rose, 600.f, 3);
		BIBMotionEvents::Shared(TEXT("chain-attack-drop-in"), Target, false, TEXT("chain"));
		// The Relay Strike lands as a finisher on the broken angel, as a heavy hit on everything around it.
		StartPose(0.f, 0.3f, 1.4f);
		Target->SetNextHitBonus(BIB::Stagger::EBonus::ChainAttack);
		{
			const FBIBScopedHit Scope(this, BIB::Feel::ESource::Counter, true, static_cast<int32>(BIB::Feel::EWeight::Finisher));
			DealDamage(Target, Active().BaseAttack * ChainStrikeMult);
		}
		for (ABIBEnemy* Other : EnemiesInRadius(Target->GetActorLocation(), 420.f))
		{
			if (Other != Target)
			{
				Other->SetNextHitBonus(BIB::Stagger::EBonus::ChainAttack);
				const FBIBScopedHit Scope(this, BIB::Feel::ESource::Counter, false, static_cast<int32>(BIB::Feel::EWeight::Heavy));
				DealDamage(Other, Active().BaseAttack * ChainStrikeMult * 0.5f);
			}
		}
		Target->CloseChain();
		ChainUntil = -1.f;
		SetSlowMo(0.2f, 0.3);
		Shake(3.f, 0.3f);
		Accent(true);
		ShowBanner(TEXT("RELAY STRIKE"), Active().Name.ToUpper(), Hud.Colour, 1.0);
		return;
	}
	// Quick entry strike in front of the incoming operator.
	Accent();
	BIBMotionEvents::Switched(false);
	FaceAim();
	ABIBFx::SlashStreak(World, GetActorLocation() + AimForward() * 180.0 + FVector(0.0, 0.0, 40.0), AimForward(), Hud.Colour, 380.f, 1);
	StartPose(0.f, 0.2f, 0.9f);
	for (ABIBEnemy* Enemy : EnemiesInFront(BIBTuning::MeleeRange + 120.f, 0.2f))
	{
		Enemy->SetNextHitBonus(BIB::Stagger::EBonus::SwitchStrike);
		const FBIBScopedHit Scope(this, BIB::Feel::ESource::Counter, false, static_cast<int32>(BIB::Feel::EWeight::Medium));
		DealDamage(Enemy, Active().BaseAttack * SwitchStrikeMult);
	}
}

// ============================================================================ per frame

void ABIBPlayerCharacter::FeelTickStart(float DeltaSeconds)
{
	(void)DeltaSeconds;
	FeelTickView();   // field-of-view punch, vignette pulse, movement trail (BIBPlayerFeelFx.cpp)
	BIBFeelConfig::Load();
	TickUltCinematic();
	const double RealNow = FPlatformTime::Seconds();
	if (!bUltCinematic && SlowMoUntilReal > 0.0 && RealNow >= SlowMoUntilReal)
	{
		SlowMoUntilReal = 0.0;
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
	}
	if (CustomTimeDilation < 1.f && RealNow >= SelfHitStopUntilReal)
	{
		CustomTimeDilation = 1.f;
	}
	// Record fresh presses into the buffer. The Judge court menu and the Exchange panel read raw keys only.
	const ABIBPlayerController* PC = GetPC();
	if (!PC || JudgeData.bMenuOpen || NarratorData.bExchangeOpen)
	{
		ClearBuffers();
		return;
	}
	const float T = Now();
	for (int32 Index = 0; Index < static_cast<int32>(EBIBInput::Count); ++Index)
	{
		const EBIBInput Input = static_cast<EBIBInput>(Index);
		if (IsBufferable(Input) && PC->WasInputPressed(Input))
		{
			BufferedAt[Index] = T;
		}
		else if (T - BufferedAt[Index] > static_cast<float>(BIB::Feel::Tuning().InputBufferSeconds))
		{
			BufferedAt[Index] = -100.f;
		}
	}
}

void ABIBPlayerCharacter::FeelTick(float DeltaSeconds)
{
	const float T = Now();
	BIB::Angel::FMind* Mind = MindOf(this);
	if (Mind && bCombatActive)
	{
		// Habits that are held rather than pressed.
		if (bBlocking)
		{
			Mind->Observe(BIB::Angel::ETrait::Blocking, DeltaSeconds);
		}
		if (GetCharacterMovement()->IsFalling() && Pressed(EBIBInput::Jump))
		{
			Mind->Observe(BIB::Angel::ETrait::Jumping);
		}
		if (ABIBEnemy* Near = NearestEnemy(900.f))
		{
			const FVector Away = (GetActorLocation() - Near->GetActorLocation()).GetSafeNormal2D();
			if (GetVelocity().Size2D() > 200.0 && FVector::DotProduct(GetVelocity().GetSafeNormal2D(), Away) > 0.6)
			{
				Mind->Observe(BIB::Angel::ETrait::Kiting, DeltaSeconds);
			}
		}
		bool bBoss = false;
		if (const ABIBGameMode* GameMode = GetMode())
		{
			for (const ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
			{
				bBoss |= Enemy->IsBoss();
			}
		}
		Mind->Tick(DeltaSeconds, bBoss);
		for (const BIB::Angel::ECounter Learned : Mind->TakeNewlyLearned())
		{
			ShowBanner(FString::Printf(TEXT("ANGELS ADAPTED  //  %s"), UTF8_TO_TCHAR(BIB::Angel::CounterName(Learned))).ToUpper(),
				UTF8_TO_TCHAR(BIB::Angel::CounterBlurb(Learned)), BIBFxColours::Rose, 3.0);
			Message(FString::Printf(TEXT("The angels adapted: %s. %s"), UTF8_TO_TCHAR(BIB::Angel::CounterName(Learned)), UTF8_TO_TCHAR(BIB::Angel::CounterBlurb(Learned))));
		}
	}
	// The 13th frame on movement: a sharp turn (60+ degrees inside 0.15 s), starting to move, and taking off.
	{
		const float Yaw = static_cast<float>(GetActorRotation().Yaw);
		const float Delta = FMath::Abs(FRotator::NormalizeAxis(Yaw - TurnYawLast));
		TurnYawLast = Yaw;
		if (T - TurnWindowStart > 0.15f)
		{
			TurnWindowStart = T;
			TurnAccum = 0.f;
		}
		TurnAccum += Delta;
		if (TurnAccum >= 60.f)
		{
			TurnAccum = 0.f;
			TurnWindowStart = T;
			Accent();
		}
		const bool bMoving = GetVelocity().Size2D() > 220.0;
		if (bMoving && !bWasMoving)
		{
			Accent();
		}
		bWasMoving = bMoving;
		if (Pressed(EBIBInput::Jump) && GetCharacterMovement()->IsFalling() && GetVelocity().Z > 200.0)
		{
			Accent();
			BIBMotionEvents::Shared(TEXT("jump-dust"), this, false, TEXT("jump"));
		}
	}
	// Motion pack moments that are states, not events: ultimate charged, low health, the boss / mini boss arriving.
	{
		const bool bUltReady = Active().Ult >= 99.99f;
		if (bUltReady && !bUltWasReady)
		{
			BIBMotionEvents::UltimateReady();
		}
		bUltWasReady = bUltReady;
		const bool bLow = Active().MaxHP > 0.f && Active().HP / Active().MaxHP < 0.25f;
		if (bLow && !bWasLowHP)
		{
			BIBMotionEvents::LowHealth();
		}
		bWasLowHP = bLow;
		bool bBoss = false;
		bool bMini = false;
		if (const ABIBGameMode* GameMode = GetMode())
		{
			for (const ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
			{
				bBoss |= Enemy->GetKind() == EBIBEnemyKind::Boss;
				bMini |= Enemy->GetKind() == EBIBEnemyKind::MiniBoss;
			}
		}
		if (bBoss && !bBossSeen)
		{
			const int32 StageId = GetMode() ? GetMode()->GetCurrentStageId() : 1;
			const int32 Chapter = FMath::Clamp((StageId - 1) / 8 + 1, 1, 13);
			BIBMotionEvents::BossIntro(FString::Printf(TEXT("CHAPTER %02d"), Chapter));
		}
		if (bMini && !bMiniBossSeen && !bBoss)
		{
			BIBMotionEvents::MiniBoss();
		}
		bBossSeen = bBoss;
		bMiniBossSeen = bMini;
	}
	if (ChainUntil > 0.f && (T >= ChainUntil || !ChainTarget.IsValid() || !ChainTarget->IsAlive()))
	{
		ChainUntil = -1.f;
	}
	TraversalTick(DeltaSeconds);
	MotionVfxTick();
}

void ABIBPlayerCharacter::MotionVfxTick()
{
	// Set 2 motion VFX that follow states rather than events (BIBMotionEvents.h).
	const float T = Now();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bMoving = GetVelocity().Size() > 120.0;
	if (bClimbing && bMoving && T >= VfxClimbAt)
	{
		VfxClimbAt = T + 0.7f;
		BIBMotionEvents::Shared(TEXT("climb-grip-dust"), this, false, TEXT("climb"));
	}
	const bool bSwimming = Movement->IsSwimming();
	if (bSwimming && (!bVfxSwimming || (bMoving && T >= VfxSwimAt)))
	{
		VfxSwimAt = T + 1.4f;
		BIBMotionEvents::Shared(TEXT("swim-splash"), this, false, TEXT("swim"));
	}
	bVfxSwimming = bSwimming;

	const FBIBMember& Member = Active();
	// Operators whose look is always on (Shiraishi's shield bubble, Media's packet loss) and Luna's idle motif.
	const bool bIdle = Action == EBIBAction::None && !bMoving && T - LastAttackAt > 2.f;
	const FString Loop = (Member.Name == TEXT("Shiraishi") || Member.Name == TEXT("Media")) ? Member.Name
		: ((Member.Kit == EBIBKitType::Luna && bIdle) ? FString(TEXT("Luna")) : FString());
	if (Loop != VfxLoopOperator)
	{
		BIBMotion::Stop(TEXT("op-loop"));
		VfxLoopOperator = Loop;
		if (!Loop.IsEmpty())
		{
			BIBMotion::FPlay Play;
			Play.Piece = BIBMotion::AbilityPiece(Loop == TEXT("Shiraishi") ? TEXT("shira") : Loop.ToLower(), Loop == TEXT("Luna") ? TEXT("IDLE") : TEXT("ALWAYS"));
			Play.Clip = TEXT("play");
			Play.Follow = this;
			Play.bLoop = true;
			Play.Opacity = 0.7f;
			Play.Tag = TEXT("op-loop");
			BIBMotion::Play(Play);
		}
	}

	// Judge: the mode wheel on every court change and on contempt; Ignite when she becomes the Judge, Condemned on contempt.
	if (Member.Kit == EBIBKitType::Judge)
	{
		const int32 Mode = static_cast<int32>(JudgeData.Mode);
		const bool bLocked = T < JudgeData.CourtLockedUntil;
		if (VfxJudgeMode >= 0 && (Mode != VfxJudgeMode || bLocked != bVfxJudgeLocked))
		{
			BIBMotionEvents::JudgeMode(Mode == 0 ? 0 : (Mode == 1 ? 1 : 2), bLocked);
			if (bLocked && !bVfxJudgeLocked)
			{
				BIBMotionEvents::Ability(this, Member.Name, TEXT("PASSIVE"));
			}
			else if (JudgeData.Mode == EBIBJudgeMode::Judge)
			{
				BIBMotionEvents::Shared(TEXT("ignite"), this, false, TEXT("judge-alt"));
			}
		}
		VfxJudgeMode = Mode;
		bVfxJudgeLocked = bLocked;
	}
	else
	{
		VfxJudgeMode = -1;
	}
	// Narrator: Burning-out when the speed nears the explosion point.
	const bool bBurning = Member.Kit == EBIBKitType::Narrator && NarratorData.Speed >= (NarratorData.bDiano ? BIBTuning::DianoExplodeSpeed : BIBTuning::NarratorExplodeSpeed) * 0.75f;
	if (bBurning && !bVfxBurningOut)
	{
		BIBMotionEvents::Shared(TEXT("burning-out"), this, false, TEXT("narr-alt"));
	}
	bVfxBurningOut = bBurning;

	// The angels' overlays (statuses, stun halo, guard stance, poise, verdict markers), ten times a second.
	if (T >= VfxEnemySyncAt)
	{
		VfxEnemySyncAt = T + 0.1f;
		if (const ABIBGameMode* GameMode = GetMode())
		{
			const bool bVerdicts = Member.Kit == EBIBKitType::Judge;
			for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
			{
				BIBMotionEvents::SyncEnemy(Enemy, Member.Name, bVerdicts);
			}
		}
	}
}

void ABIBPlayerCharacter::BuildFeelHud()
{
	const float T = Now();
	const double RealNow = FPlatformTime::Seconds();
	Hud.ComboHits = T - LastHitAt < 2.5f ? ComboHits : 0;
	Hud.ComboAge = T - LastHitAt;
	const double BannerLeft = BannerUntilReal - RealNow;
	Hud.Banner = BannerLeft > 0.0 ? BannerText : FString();
	Hud.BannerSub = BannerSubText;
	Hud.BannerColour = BannerColour;
	Hud.BannerAlpha = static_cast<float>(FMath::Clamp(BannerLeft / 0.3, 0.0, 1.0));
	Hud.bChainReady = ChainUntil > T;
	Hud.bDodgeCounterReady = DodgeCounterUntil > T;
	Hud.BossName.Reset();
	if (const ABIBGameMode* GameMode = GetMode())
	{
		const ABIBEnemy* Boss = nullptr;
		for (const ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			if (Enemy->IsBoss() && (!Boss || Enemy->GetKind() == EBIBEnemyKind::Boss))
			{
				Boss = Enemy;
			}
		}
		if (Boss)
		{
			Hud.BossName = Boss->GetKindName();
			Hud.BossHP = Boss->GetHealthFraction();
			Hud.BossDaze = Boss->GetDazeFraction();
			Hud.bBossStunned = Boss->IsDazeStunned();
		}
	}
	if (const BIB::Angel::FMind* Mind = MindOf(this))
	{
		Hud.AngelLine = UTF8_TO_TCHAR(Mind->Describe().c_str());
		Hud.AngelAdaptation = static_cast<float>(Mind->Adaptation());
	}
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	Hud.MoveState = bClimbing ? TEXT("CLIMBING") : (Movement && Movement->IsSwimming() ? TEXT("SWIMMING") : FString());
}

// ============================================================================ traversal

bool ABIBPlayerCharacter::CanClimbOrSwim() const
{
	// Judge.md: cannot sprint, climb, swim or crouch.
	return Active().Kit != EBIBKitType::Judge;
}

bool ABIBPlayerCharacter::TryStartClimb()
{
	UWorld* World = GetWorld();
	const float T = Now();
	if (bClimbing || T < ClimbCooldownUntil || !CanClimbOrSwim() || IsMovementLocked())
	{
		return false;
	}
	FVector Wish = GetLastMovementInputVector().GetSafeNormal2D();
	if (Wish.IsNearlyZero())
	{
		return false;
	}
	// Facing a steep wall at chest height that is taller than a step.
	const FVector Chest = GetActorLocation() + FVector(0.0, 0.0, 20.0);
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("BIBClimb"), false, this);
	if (!World->LineTraceSingleByChannel(Hit, Chest, Chest + Wish * 75.0, ECC_Visibility, Params))
	{
		return false;
	}
	if (FMath::Abs(Hit.ImpactNormal.Z) > 0.3 || Hit.GetActor() == nullptr || Cast<APawn>(Hit.GetActor()))
	{
		return false;
	}
	FHitResult High;
	const FVector Head = GetActorLocation() + FVector(0.0, 0.0, 110.0);
	if (!World->LineTraceSingleByChannel(High, Head, Head + Wish * 90.0, ECC_Visibility, Params))
	{
		return false;   // a low ledge: just jump up
	}
	bClimbing = true;
	ClimbNormal = Hit.ImpactNormal.GetSafeNormal2D();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Flying);
	Movement->StopMovementImmediately();
	Movement->bOrientRotationToMovement = false;
	SetActorRotation((-ClimbNormal).Rotation());
	if (BIB::Angel::FMind* Mind = MindOf(this))
	{
		Mind->Observe(BIB::Angel::ETrait::Kiting, 0.5);
	}
	return true;
}

void ABIBPlayerCharacter::StopClimb(bool bJumpOff)
{
	if (!bClimbing)
	{
		return;
	}
	bClimbing = false;
	ClimbCooldownUntil = Now() + 0.4f;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->SetMovementMode(MOVE_Falling);
	if (bJumpOff)
	{
		LaunchCharacter(ClimbNormal * 450.0 + FVector(0.0, 0.0, 420.0), true, true);
	}
}

void ABIBPlayerCharacter::HandleSwim()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement->IsSwimming())
	{
		return;
	}
	const ABIBPlayerController* PC = GetPC();
	const bool bDown = PC && (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::C));
	const bool bUp = Held(EBIBInput::Jump);
	Movement->MaxSwimSpeed = Held(EBIBInput::Run) ? 520.f : 340.f;
	Movement->Buoyancy = bDown ? 0.6f : 1.08f;   // float to the surface unless diving
	if (bUp || bDown)
	{
		FVector Velocity = Movement->Velocity;
		Velocity.Z = bUp ? SwimUpSpeed : -SwimUpSpeed;
		Movement->Velocity = Velocity;
	}
	// Jumping out at the surface: Space near the top of the water launches onto a ledge.
	if (bUp && Movement->GetPhysicsVolume() && Movement->ImmersionDepth() < 0.75f && Pressed(EBIBInput::Jump))
	{
		LaunchCharacter(FVector(0.0, 0.0, 520.0), false, true);
		ConsumeBuffer(EBIBInput::Jump);
	}
}

void ABIBPlayerCharacter::TraversalTick(float DeltaSeconds)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float T = Now();
	// Judge cannot swim: deep water is slow wading for her (she stays at walking speed, no strokes).
	Movement->GetNavAgentPropertiesRef().bCanSwim = CanClimbOrSwim();
	if (Movement->IsSwimming() && !CanClimbOrSwim())
	{
		Movement->MaxSwimSpeed = BIBTuning::JudgeWalkSpeed * 0.6f;
		Movement->Buoyancy = 1.2f;
	}
	else
	{
		HandleSwim();
	}

	// Hard landings: a long fall jolts the camera and costs a moment (and some health from very high).
	if (Movement->IsFalling())
	{
		if (!bWasFalling)
		{
			FallPeakZ = GetActorLocation().Z;
		}
		FallPeakZ = FMath::Max(FallPeakZ, static_cast<float>(GetActorLocation().Z));
		bWasFalling = true;
	}
	else if (bWasFalling)
	{
		bWasFalling = false;
		const float Drop = FallPeakZ - static_cast<float>(GetActorLocation().Z);
		if (Drop > 700.f && !Movement->IsSwimming() && !bClimbing)
		{
			LandedLockUntil = T + 0.3f;
			BIBMotionEvents::Shared(TEXT("hard-landing-crack"), this, false, TEXT("landing"));
			Accent(Drop > 1600.f);
			Shake(FMath::Clamp(Drop / 500.f, 1.f, 3.f), 0.2f);
			ABIBFx::ShockRing(GetWorld(), GetActorLocation() - FVector(0.0, 0.0, 90.0), 180.f, BIBFxColours::White, 0.3f);
			if (Drop > 1600.f && bCombatActive)
			{
				TakeSelfDamage(Active().MaxHP * FMath::Clamp((Drop - 1600.f) / 6000.f, 0.f, 0.25f));
			}
		}
	}

	if (!bClimbing)
	{
		// Walk or jump into a wall while moving towards it: climb it.
		if (!Movement->IsSwimming() && (Movement->IsFalling() || Held(EBIBInput::MoveForward) || !TouchMove.IsNearlyZero()))
		{
			if (TryStartClimb() == false)
			{
				return;
			}
		}
		else
		{
			return;
		}
	}
	// Climbing: W/S up and down, A/D sideways along the wall; Space jumps off; the top of the wall mantles over.
	if (Pressed(EBIBInput::Jump))
	{
		ConsumeBuffer(EBIBInput::Jump);
		StopClimb(true);
		return;
	}
	if (Pressed(EBIBInput::Parry) || !CanClimbOrSwim())
	{
		StopClimb(false);
		return;
	}
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(TEXT("BIBClimb"), false, this);
	FHitResult Hit;
	const FVector Chest = GetActorLocation() + FVector(0.0, 0.0, 20.0);
	if (!World->LineTraceSingleByChannel(Hit, Chest, Chest - ClimbNormal * 90.0, ECC_Visibility, Params))
	{
		// The wall ended at chest height: mantle onto the top.
		StopClimb(false);
		LaunchCharacter(-ClimbNormal * 260.0 + FVector(0.0, 0.0, 520.0), true, true);
		MantleUntil = T + 0.3f;
		return;
	}
	ClimbNormal = Hit.ImpactNormal.GetSafeNormal2D();
	SetActorRotation((-ClimbNormal).Rotation());
	float Up = (Held(EBIBInput::MoveForward) ? 1.f : 0.f) - (Held(EBIBInput::MoveBack) ? 1.f : 0.f) + static_cast<float>(TouchMove.Y);
	float Side = (Held(EBIBInput::MoveRight) ? 1.f : 0.f) - (Held(EBIBInput::MoveLeft) ? 1.f : 0.f) + static_cast<float>(TouchMove.X);
	Up = FMath::Clamp(Up, -1.f, 1.f);
	Side = FMath::Clamp(Side, -1.f, 1.f);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, -ClimbNormal) * -1.0;
	const float Speed = ClimbSpeed * (Held(EBIBInput::Run) ? 1.6f : 1.f);
	Movement->Velocity = (FVector::UpVector * Up + Right * Side).GetClampedToMaxSize(1.0) * Speed - ClimbNormal * 30.0;
	if (Up < 0.f)
	{
		// Climbing down onto the floor lets go.
		FHitResult Floor;
		const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (World->LineTraceSingleByChannel(Floor, Feet, Feet - FVector(0.0, 0.0, 15.0), ECC_Visibility, Params))
		{
			StopClimb(false);
		}
	}
}

// ============================================================================ hit feel: poses, camera, ultimate beat, rumble, sound

void ABIBPlayerCharacter::StartPose(float ImpactAt, float Recovery, float Strength)
{
	PoseTime = 0.f;
	PoseImpactAt = FMath::Max(0.f, ImpactAt);
	PoseRecovery = FMath::Max(0.f, Recovery);
	PoseStrength = FMath::Max(0.f, Strength);
}

void ABIBPlayerCharacter::UpdatePose(float DeltaSeconds)
{
	// DeltaSeconds is the operator's own time: the pose holds still during hit stop, like everything else on her.
	if (PoseTime >= 0.f)
	{
		PoseTime += DeltaSeconds;
		if (PoseTime > FMath::Max(BIB::Feel::Tuning().PoseMinWindup, static_cast<double>(PoseImpactAt)) + PoseRecovery + 0.05)
		{
			PoseTime = -1.f;
		}
	}
	if (PulseAge >= 0.f)
	{
		PulseAge += DeltaSeconds;
		if (PulseAge > 0.6f)
		{
			PulseAge = -1.f;
		}
	}
	const BIB::Feel::FPose Pose = BIB::Feel::EvaluatePose(PoseTime, PoseImpactAt, PoseRecovery, PoseStrength, PulseAge, PulseLean);
	BIB::Feel::FStats& Stats = BIB::Feel::Stats();
	Stats.MinLean = FMath::Min(Stats.MinLean, Pose.LeanDegrees);
	Stats.MaxLean = FMath::Max(Stats.MaxLean, Pose.LeanDegrees);
	// Lean around the feet: the top of the body moves along the facing (+) or away from it (- = wind-up).
	const float Lean = static_cast<float>(FMath::Clamp(Pose.LeanDegrees, -35.0, 35.0));
	const float Squash = static_cast<float>(FMath::Clamp(Pose.Squash, 0.7, 1.3));
	const FRotator Turn(-Lean, 0.f, 0.f);
	const FVector Pivot(0.0, 0.0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
	const FVector Forward(static_cast<double>(Pose.Forward), 0.0, 0.0);
	FVector BodyOffset = BodyBaseLocation - Pivot;
	BodyOffset.Z *= Squash;
	FVector HeadOffset = HeadBaseLocation - Pivot;
	HeadOffset.Z *= Squash;
	const float Thin = 1.f / FMath::Sqrt(Squash);
	BodyComp->SetRelativeLocationAndRotation(Pivot + Turn.RotateVector(BodyOffset) + Forward, Turn);
	BodyComp->SetRelativeScale3D(FVector(BodyBaseScale.X * Thin, BodyBaseScale.Y * Thin, BodyBaseScale.Z * Squash));
	HeadComp->SetRelativeLocation(Pivot + Turn.RotateVector(HeadOffset) + Forward);
}

void ABIBPlayerCharacter::UpdateCameraFeel()
{
	if (!CameraBoom || bUltCinematic)
	{
		return;
	}
	// Push-in on heavy hits: the arm shortens quickly and eases back.
	float Push = 0.f;
	const double Seconds = FMath::Max(0.05, BIB::Feel::Tuning().PushInSeconds);
	const double Age = FPlatformTime::Seconds() - PushInStartReal;
	if (PushInPeak > 0.f && Age >= 0.0 && Age < Seconds)
	{
		const double In = Seconds * 0.25;
		const double K = Age < In ? Age / In : 1.0 - (Age - In) / (Seconds - In);
		Push = PushInPeak * static_cast<float>(FMath::Clamp(K, 0.0, 1.0));
	}
	else
	{
		PushInPeak = 0.f;
	}
	CameraBoom->TargetArmLength = BaseArmLength - Push;
}

void ABIBPlayerCharacter::BeginUltCinematic(const FString& Name)
{
	const BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
	UWorld* World = GetWorld();
	if (!World || Tuning.UltCinematicRealSeconds <= 0.0 || !CameraBoom)
	{
		return;
	}
	bUltCinematic = true;
	++BIB::Feel::Stats().Cinematics;
	UltCinematicStartReal = FPlatformTime::Seconds();
	UltName = Name;
	SlowMoUntilReal = 0.0;
	CustomTimeDilation = 1.f;
	// The world nearly stops while the camera frames the operator; she cannot be hit (IsInvulnerableNow + the grant below).
	UGameplayStatics::SetGlobalTimeDilation(World, static_cast<float>(Tuning.UltCinematicDilation));
	GrantInvulnerable(static_cast<float>(Tuning.UltCinematicRealSeconds * Tuning.UltCinematicDilation + Tuning.UltInvulnerableGrace));
	UltCameraYawStart = static_cast<float>(GetActorRotation().Yaw + Tuning.UltCameraYaw);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetWorldRotation(FRotator(static_cast<float>(Tuning.UltCameraPitch), UltCameraYawStart, 0.f));
	CameraBoom->TargetArmLength = static_cast<float>(Tuning.UltCameraArm);
	ShowBanner(Name.ToUpper(), FString::Printf(TEXT("%s  //  LMB SKIP"), *Active().Name.ToUpper()), Hud.Colour, Tuning.UltCinematicRealSeconds + 0.5);
	Accent(true);
	ABIBFx::Spawn(World, EBIBFxShape::Pillar, GetActorLocation(), FRotator::ZeroRotator, FVector(200.f, 200.f, 40.f), FVector(40.f, 40.f, 1200.f),
		Hud.Colour, 0.8f, 10.f);
	ABIBFx::ShockRing(World, GetActorLocation() - FVector(0.0, 0.0, 85.0), 420.f, Hud.Colour, 0.8f);
	PlayFeelSounds(BIBFeelConfig::NamedSounds(TEXT("UltCinematic")), GetActorLocation());
	bool bCameraFeel = true;
	bool bRumble = true;
	float SfxVolume = 1.f;
	FeelSettings(this, bCameraFeel, bRumble, SfxVolume);
	if (bRumble)
	{
		Rumble(0.6f, 0.3f);
	}
	StartPose(0.25f, 0.3f, 1.5f);
}

void ABIBPlayerCharacter::TickUltCinematic()
{
	if (!bUltCinematic)
	{
		return;
	}
	const BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
	const double Age = FPlatformTime::Seconds() - UltCinematicStartReal;
	const ABIBPlayerController* PC = GetPC();
	const bool bSkip = Age >= Tuning.UltCinematicSkipAfter && PC
		&& (PC->WasInputPressed(EBIBInput::Attack) || PC->WasInputPressed(EBIBInput::UltimateE) || PC->WasInputPressed(EBIBInput::UltimateR)
			|| PC->WasInputPressed(EBIBInput::Parry) || PC->WasInputPressed(EBIBInput::Jump));
	if (bSkip)
	{
		InputSwallowFrame = GFrameCounter;
		++BIB::Feel::Stats().CinematicSkips;
	}
	if (bSkip || Age >= Tuning.UltCinematicRealSeconds || !bCombatActive || !Active().bAlive)
	{
		EndUltCinematic();
		return;
	}
	// A slow orbit around the operator while the beat plays.
	if (CameraBoom)
	{
		CameraBoom->SetWorldRotation(FRotator(static_cast<float>(Tuning.UltCameraPitch), UltCameraYawStart + static_cast<float>(Age) * 30.f, 0.f));
	}
}

void ABIBPlayerCharacter::EndUltCinematic()
{
	if (!bUltCinematic)
	{
		return;
	}
	bUltCinematic = false;
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = true;
		CameraBoom->TargetArmLength = BaseArmLength;
	}
	GrantInvulnerable(static_cast<float>(BIB::Feel::Tuning().UltInvulnerableGrace));
	Accent(true);
	ClearBuffers();
}

void ABIBPlayerCharacter::PlayFeelSounds(const TArray<USoundBase*>& Layers, const FVector& Location, float Volume) const
{
	if (Layers.Num() == 0)
	{
		return;
	}
	bool bCameraFeel = true;
	bool bRumble = true;
	float SfxVolume = 1.f;
	FeelSettings(this, bCameraFeel, bRumble, SfxVolume);
	const float Mix = BIBFeelConfig::SoundVolume() * SfxVolume * Volume;
	if (Mix <= 0.f)
	{
		return;
	}
	for (USoundBase* Sound : Layers)
	{
		if (Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Mix);
		}
	}
}

void ABIBPlayerCharacter::Rumble(float Intensity, float Seconds) const
{
	if (Intensity <= 0.f || Seconds <= 0.f)
	{
		return;
	}
	if (ABIBPlayerController* PC = GetPC())
	{
		// Native dynamic force feedback (no asset needed); does nothing without a gamepad.
		PC->PlayDynamicForceFeedback(FMath::Clamp(Intensity, 0.f, 1.f), Seconds, true, true, true, true);
		++BIB::Feel::Stats().Rumbles;
	}
}

void ABIBPlayerCharacter::KitPoseTiming(const BIB::Kits::FAbilityDef& Def, int32 ComboIndex, float& OutImpactAt, float& OutRecovery)
{
	const BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
	double First = -1.0;
	for (const BIB::Kits::FStep& Step : Def.Steps)
	{
		if ((Step.Combo < 0 || Step.Combo == ComboIndex) && BIB::Kits::StepDealsDamage(Step) && (First < 0.0 || Step.At < First))
		{
			First = Step.At;
		}
	}
	for (const BIB::Kits::FBranch& Branch : Def.Branches)
	{
		for (const BIB::Kits::FStep& Step : Branch.Steps)
		{
			if (BIB::Kits::StepDealsDamage(Step) && (First < 0.0 || Def.BranchAt + Step.At < First))
			{
				First = Def.BranchAt + Step.At;
			}
		}
	}
	// No damaging step (buffs, shields, stances): a short casting gesture.
	OutImpactAt = static_cast<float>(First >= 0.0 ? First : Tuning.PoseMinWindup);
	const double Recovery = Def.Slot == BIB::Kits::ESlotId::Basic ? Tuning.PoseRecoverySeconds
		: FMath::Clamp(Def.LockSeconds - FMath::Max(First, 0.0), Tuning.PoseRecoverySeconds, 0.6);
	OutRecovery = static_cast<float>(Recovery);
}

bool ABIBPlayerCharacter::IsActionInRecovery() const
{
	// Hand-written casts: after the last hit (plus a short grace) the rest of the action is recovery a dodge may cancel.
	const float Grace = static_cast<float>(BIB::Feel::Tuning().CancelGraceSeconds);
	switch (Action)
	{
	case EBIBAction::LunaBlackhole:
		return LunaData.OrbPhase != 0 && ActionTime >= BIBTuning::BlackholeWindup + Grace;   // the orb flies on by itself
	case EBIBAction::JudgeGavel:
		return JudgeData.GavelSmashesDone >= 3 && ActionTime >= BIBTuning::GavelWindup + BIBTuning::GavelSpacing * 2.f + Grace;
	case EBIBAction::JudgeRetribution:
		return JudgeData.bActionEffectDone && ActionTime >= BIBTuning::RetributionWindup + Grace;
	default:
		return false;
	}
}

bool ABIBPlayerCharacter::TryRecoveryCancel()
{
	// Dodge = hold dash (Shift) + parry (F); only the dodge cancels a recovery.
	if (!Pressed(EBIBInput::Parry) || !Held(EBIBInput::Run) || KitMoveLeft > 0.f || bClimbing)
	{
		return false;
	}
	BIB::Kits::FRuntime* Runtime = Active().Runtime.Get();
	const float Grace = static_cast<float>(BIB::Feel::Tuning().CancelGraceSeconds);
	if (Runtime && Action == EBIBAction::None && Runtime->MovementLocked() && Runtime->InRecovery(Grace))
	{
		Runtime->CancelRecovery();
		KitLockedUntil = FMath::Min(KitLockedUntil, Now());
		PoseTime = -1.f;
		++BIB::Feel::Stats().RecoveryCancels;
		return true;
	}
	if (Action != EBIBAction::None && Action != EBIBAction::Dodge && Action != EBIBAction::Punch && IsActionInRecovery())
	{
		EndAction();
		PoseTime = -1.f;
		++BIB::Feel::Stats().RecoveryCancels;
		return true;
	}
	return false;
}
