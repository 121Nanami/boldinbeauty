// Combat-feel presentation layer of the commander's team (owner task 2026-10-12: "some operator abilities feel flat").
//
// PRESENTATION ONLY. Every function here is called from an event that already exists (a cast succeeded, an action started, a hit
// landed, a block / parry / dodge happened) and draws or nudges something: glow, ring, trail, camera field of view, vignette,
// sound hook. None of it reads or writes damage, cooldowns, radii, durations, hit detection or GAS state. The radii and windup
// lengths below are READ from BIBTuning.h so a telegraph is exactly as big and as long as the thing it announces.
//
// What each stage gives an ability (BIBCombatFeel.h has the slot table):
//   anticipation   a ring converging on the operator when a move is cast; a windup telegraph on the ground (Judge's gavel and
//                  retribution, Luna's orbiting hole and Blackhole shot lane) for exactly the windup's length
//   impact         a ground ring under the struck angel and a field-of-view punch on heavy hits, both heavier for heavier slots
//                  (the existing spark, hit stop, camera shake and body flash are scaled by the same table)
//   follow-through a wake behind a dash / dodge and behind Luna's orb
//   ultimate beat  converging charge ring + pillar, a vignette pulse, a field-of-view punch and a very short slow-down
//   defence        block / parry / dodge each get a distinct world flash, not just the HUD prompt
//   audio          named hooks (BIBCombatFeel::PlayCue); silent until the owner adds the sound
//
// Hit stop and slow-down: see BIBPlayerFeel.cpp (OnHitLanded) and BIBEnemy.cpp (ReceiveDamage) for the hit stop, which is the
// engine's per-actor time dilation and therefore also holds that actor's own Tick-delta timers for its few hundredths of a second
// (Combat-feel report, "Hit stop interaction"). Nothing in this file adds a new time dilation except the ultimate beat's global one,
// which scales every game-time clock equally for at most 0.18 s of real time and never overrides a slow-down already running.
#include "BIBGame/BIBPlayerCharacter.h"

#include "BIBGame/BIBCombatFeel.h"
#include "BIBGame/BIBFx.h"
#include "BIBGame/BIBTuning.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

namespace
{
	constexpr float HeavyRatio = static_cast<float>(BIB::Stagger::HeavyHitRatio);

	// How long after a cast its hits still count as that slot's (a meteor lands a second after the cast; the Special's blast and the
	// ultimate's hits come later still).
	float SlotWindow(EBIBSlot Slot)
	{
		switch (Slot)
		{
		case EBIBSlot::Ultimate: return 4.f;
		case EBIBSlot::Special: return 2.f;
		case EBIBSlot::Move1:
		case EBIBSlot::Move2:
		case EBIBSlot::Move3: return 1.5f;
		default: return 0.f;
		}
	}
}

void ABIBPlayerCharacter::FeelApplySettings(const BIB::FSettings& Settings)
{
	FeelCfg = BIBCombatFeel::FromSettings(Settings);
	FovBase = static_cast<float>(FMath::Clamp(Settings.FieldOfView, 70, 110));
	ABIBFx::SetCap(FeelCfg.FxCap);
}

int32 ABIBPlayerCharacter::FeelSlotForHit() const
{
	if (FeelSlot != EBIBSlot::Punch && Now() - FeelSlotAt <= SlotWindow(FeelSlot))
	{
		return static_cast<int32>(FeelSlot);
	}
	return static_cast<int32>(EBIBSlot::Punch);
}

float ABIBPlayerCharacter::FeelHitStopMult() const
{
	return FeelCfg.HitStopScale() * BIBCombatFeel::ForSlot(FeelSlotForHit()).HitStop;
}

void ABIBPlayerCharacter::FovPunch(float Degrees, float Seconds)
{
	const float Scaled = Degrees * FeelCfg.FovScale();
	if (Scaled <= 0.05f || !FollowCamera)
	{
		return;
	}
	const double RealNow = FPlatformTime::Seconds();
	// A stronger punch replaces a weaker one that is still running; a weaker one is ignored.
	const float Running = RealNow < FovPunchEndReal ? FovPunchPeak * static_cast<float>((FovPunchEndReal - RealNow) / FMath::Max(0.05, FovPunchEndReal - FovPunchStartReal)) : 0.f;
	if (Scaled >= Running)
	{
		FovPunchPeak = FMath::Min(Scaled, 6.f);
		FovPunchStartReal = RealNow;
		FovPunchEndReal = RealNow + Seconds;
	}
}

// Called every frame from FeelTickStart (also while dead or out of combat, so the camera always returns to its field of view).
void ABIBPlayerCharacter::FeelTickView()
{
	const double RealNow = FPlatformTime::Seconds();
	if (FollowCamera)
	{
		if (FovPunchPeak > 0.f)
		{
			if (RealNow >= FovPunchEndReal)
			{
				FovPunchPeak = 0.f;
				FollowCamera->SetFieldOfView(FovBase);
			}
			else
			{
				// Fast rise, then an ease-out back: the view "kicks" outward and settles.
				const float T = static_cast<float>((RealNow - FovPunchStartReal) / FMath::Max(0.05, FovPunchEndReal - FovPunchStartReal));
				const float Envelope = T < 0.15f ? T / 0.15f : FMath::Square(1.f - (T - 0.15f) / 0.85f);
				FollowCamera->SetFieldOfView(FovBase + FovPunchPeak * Envelope);
			}
		}
		if (bBeatActive)
		{
			if (RealNow >= BeatEndReal)
			{
				bBeatActive = false;
				FollowCamera->PostProcessBlendWeight = 0.f;
				FollowCamera->PostProcessSettings.bOverride_VignetteIntensity = false;
			}
			else
			{
				// The camera's post-process blends over the anime volume's vignette: weight 0 = untouched, 1 = fully closed in.
				const float T = static_cast<float>((RealNow - BeatStartReal) / FMath::Max(0.05, BeatEndReal - BeatStartReal));
				FollowCamera->PostProcessBlendWeight = FMath::Square(1.f - T) * FMath::Min(1.f, FeelCfg.BeatScale());
			}
		}
	}
	// Trail behind a dodge, a dash and a kit's movement step: one short streak every 60 ms, aligned with the motion.
	if (bCombatActive && FeelCfg.Scale > 0.f && RealNow >= TrailNextReal)
	{
		const bool bMoving = Action == EBIBAction::Dodge || Action == EBIBAction::DianoAcceleration || KitMoveLeft > 0.f;
		if (bMoving && GetVelocity().Size2D() > 500.0)
		{
			TrailNextReal = RealNow + 0.06;
			FeelTrail(GetActorLocation(), -GetVelocity().GetSafeNormal2D(), Action == EBIBAction::Dodge ? BIBFxColours::Violet : Hud.Colour);
		}
	}
}

void ABIBPlayerCharacter::FeelTrail(const FVector& At, const FVector& Direction, const FLinearColor& Colour)
{
	// A thin streak laid along the direction of travel that fades in 0.2 s: the afterimage of the movement.
	ABIBFx::Spawn(GetWorld(), EBIBFxShape::Slash, At + Direction * 60.0, Direction.Rotation(), FVector(180.f, 18.f, 18.f), FVector(260.f, 3.f, 3.f), Colour, 0.2f, 7.f);
}

// ---------------------------------------------------------------------------- cast

void ABIBPlayerCharacter::FeelOnCast(EBIBSlot Slot)
{
	if (Slot == EBIBSlot::Punch || Slot == EBIBSlot::Dodge || Slot == EBIBSlot::Parry)
	{
		return;   // a dodge or a basic attack does not take over the attribution of an ability's late hits
	}
	FeelSlot = Slot;
	FeelSlotAt = Now();
	if (FeelCfg.Scale <= 0.f)
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0);
	if (Slot == EBIBSlot::Ultimate)
	{
		FeelUltimateBeat();
		return;
	}
	// A cast whose windup already has its own telegraph (FeelOnActionStart) does not get a second ring.
	const bool bWindupShown = Action != EBIBAction::None && Action != EBIBAction::Punch && Action != EBIBAction::Dodge;
	if (!bWindupShown)
	{
		// Anticipation: a ring that closes onto the operator in operator colour, heavier for the Special. Narrator's Close enough
		// has a 3 s windup that is a timer rather than an action, so its ring lasts the whole windup ("don't get hit").
		const bool bCloseEnough = Active().Kit == EBIBKitType::Narrator && !NarratorData.bDiano && Slot == EBIBSlot::Special;
		const float Radius = bCloseEnough ? 520.f : (Slot == EBIBSlot::Special ? 300.f : 210.f);
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Feet + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(Radius * 2.f, Radius * 2.f, 3.f),
			FVector(60.f, 60.f, 2.f), Hud.Colour, bCloseEnough ? BIBTuning::CloseEnoughWindup : 0.18f, 6.f);
	}
	FeelDataAreaTelegraphs(Slot);
	BIBCombatFeel::PlayCue(World, Slot == EBIBSlot::Special ? TEXT("Cast_Special") : TEXT("Cast_Move"), GetActorLocation());
}

// Data kits: a Burst the kit data fires later than 0.3 s after the cast, while the caster is locked in place until it does, goes
// off exactly where its ring is drawn, so the ring is an honest telegraph. (Bursts that follow a target, a moving caster or a
// random enemy are not drawn: the place is not known at cast time.)
void ABIBPlayerCharacter::FeelDataAreaTelegraphs(EBIBSlot Slot)
{
	const FBIBMember& Member = Active();
	if (!Member.KitDef || FeelCfg.TelegraphScale() <= 0.f || Slot > EBIBSlot::Ultimate)
	{
		return;
	}
	// EBIBSlot Move1..Ultimate and the kit's slot ids run in the same order after the basic attack.
	const BIB::Kits::FAbilityDef& Def = Member.KitDef->Abilities[static_cast<int32>(Slot)];
	constexpr unsigned Unknown = BIB::Kits::FlagAtTarget | BIB::Kits::FlagFollowCaster | BIB::Kits::FlagRandomEnemy | BIB::Kits::FlagNearestEnemy
		| BIB::Kits::FlagBehindTarget | BIB::Kits::FlagStoredTarget | BIB::Kits::FlagHitTarget | BIB::Kits::FlagAllEnemies;
	const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0);
	int32 Drawn = 0;
	for (const BIB::Kits::FStep& Step : Def.Steps)
	{
		if (Step.Kind != BIB::Kits::EStep::Burst || Step.At < 0.3 || Step.Radius < 100.0 || (Step.Flags & Unknown) || Def.LockSeconds < Step.At || Drawn >= 2)
		{
			continue;
		}
		const double Reach = (Step.Flags & BIB::Kits::FlagFromAimPoint) ? Step.Range : Step.Offset;
		ABIBFx::AreaWarning(GetWorld(), Feet + AimForward() * Reach, static_cast<float>(Step.Radius), static_cast<float>(Step.At), Hud.Colour);
		++Drawn;
	}
}

// ---------------------------------------------------------------------------- windups that already exist as actions

void ABIBPlayerCharacter::FeelOnActionStart(EBIBAction NewAction)
{
	if (FeelCfg.Scale <= 0.f || !bCombatActive)
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0);
	const FVector Hand = GetActorLocation() + AimForward() * 120.0 + FVector(0.0, 0.0, 40.0);
	const bool bGround = FeelCfg.TelegraphScale() > 0.f;
	switch (NewAction)
	{
	case EBIBAction::LunaBlackhole:
		// The orb gathers at the hand for the windup, and the lane it will travel is drawn so it can be aimed.
		ABIBFx::Spawn(World, EBIBFxShape::Sphere, Hand, FRotator::ZeroRotator, FVector(260.f), FVector(30.f), BIBFxColours::Violet, BIBTuning::BlackholeWindup, 8.f);
		if (bGround)
		{
			const float Lane = BIBTuning::BlackholeTravelSpeed * BIBTuning::BlackholeShoot;
			ABIBFx::Spawn(World, EBIBFxShape::Slash, Feet + AimForward() * (Lane * 0.5f) + FVector(0.0, 0.0, 3.0), AimForward().Rotation(),
				FVector(Lane, 90.f, 2.f), FVector(Lane, 90.f, 2.f), BIBFxColours::Violet * 0.6f, BIBTuning::BlackholeWindup + 0.15f, 2.f);
		}
		break;
	case EBIBAction::LunaOrbitingWindup:
		// The hole being summoned: a ring at its orbit radius closes onto the operator over the whole 2 s windup.
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Feet + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator,
			FVector(BIBTuning::OrbitingRadius * 4.f, BIBTuning::OrbitingRadius * 4.f, 3.f), FVector(60.f, 60.f, 2.f), BIBFxColours::Violet, BIBTuning::OrbitingWindup, 5.f);
		break;
	case EBIBAction::LunaEventHorizon:
		ABIBFx::ShockRing(World, Feet, 600.f, BIBFxColours::Violet, 0.45f);   // the field's reach is the existing ground visual
		break;
	case EBIBAction::LunaExpel:
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Feet + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(520.f, 520.f, 3.f), FVector(60.f, 60.f, 2.f),
			BIBFxColours::White, 0.3f, 6.f);
		break;
	case EBIBAction::JudgePaperwork:
	{
		const bool bWitness = JudgeData.Mode == EBIBJudgeMode::Witness;
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Feet + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(440.f, 440.f, 3.f), FVector(60.f, 60.f, 2.f),
			BIBFxColours::Gold, bWitness ? BIBTuning::PaperworkWitnessWindup : BIBTuning::PaperworkJudgeWindup, 5.f);
		break;
	}
	case EBIBAction::JudgeCourtWindup:
	{
		const bool bToJudge = JudgeData.PendingMode == EBIBJudgeMode::Judge;
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Feet + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(600.f, 600.f, 3.f), FVector(60.f, 60.f, 2.f),
			bToJudge ? BIBFxColours::Gold : BIBFxColours::White, bToJudge ? BIBTuning::CourtJudgeWindup : BIBTuning::CourtWindup, 5.f);
		break;
	}
	case EBIBAction::JudgeGavel:
		// Where the three smashes will land, for exactly the windup before the first one.
		if (bGround)
		{
			ABIBFx::AreaWarning(World, Feet, BIBTuning::GavelRadius, BIBTuning::GavelWindup, BIBFxColours::Gold);
		}
		break;
	case EBIBAction::JudgeRetribution:
		if (bGround)
		{
			ABIBFx::AreaWarning(World, Feet, BIBTuning::RetributionRadius, BIBTuning::RetributionWindup, FLinearColor(1.f, 0.55f, 0.15f));
		}
		break;
	case EBIBAction::DianoAcceleration:
		ABIBFx::Spawn(World, EBIBFxShape::Pillar, GetActorLocation(), FRotator::ZeroRotator, FVector(160.f, 160.f, 40.f), FVector(20.f, 20.f, 500.f), Hud.Colour, 0.25f, 8.f);
		break;
	case EBIBAction::Dodge:
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Feet + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(80.f, 80.f, 2.f), FVector(360.f, 360.f, 2.f),
			BIBFxColours::Violet, 0.22f, 5.f);
		BIBCombatFeel::PlayCue(World, TEXT("Dodge"), GetActorLocation());
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------- impact

// Called once per frame at most, from OnHitLanded, after the existing spark / hit stop / shake. Ratio = damage dealt / attack.
void ABIBPlayerCharacter::FeelOnHit(ABIBEnemy* Enemy, float Ratio, const FVector& At, const FLinearColor& Colour)
{
	if (!Enemy || FeelCfg.Scale <= 0.f)
	{
		return;
	}
	UWorld* World = GetWorld();
	const BIBCombatFeel::FSlotFeel& Feel = BIBCombatFeel::ForSlot(FeelSlotForHit());
	const bool bHeavy = Ratio >= HeavyRatio;
	if (Feel.bGroundRing && Ratio >= 1.f)
	{
		// The heavier the slot and the hit, the wider the ring that leaves the struck angel's feet.
		const float Radius = FMath::Clamp(110.f + 55.f * Ratio, 130.f, 420.f) * FeelCfg.Scale;
		ABIBFx::ShockRing(World, Enemy->GetActorLocation() - FVector(0.0, 0.0, 80.0), Radius, Colour, 0.3f);
	}
	if (bHeavy)
	{
		float Punch = Feel.FovPunch;
		if (Punch <= 0.f && ComboStepNow == BIB::Stagger::ComboLength - 1)
		{
			Punch = 0.7f;   // the basic string's finisher
		}
		FovPunch(Punch * (0.7f + 0.1f * FMath::Min(Ratio, 4.f)), 0.28f);
	}
	BIBCombatFeel::PlayCue(World, bHeavy ? TEXT("Hit_Heavy") : TEXT("Hit_Light"), At, FMath::Min(1.f, 0.6f + 0.15f * Ratio));
}

// ---------------------------------------------------------------------------- defence

void ABIBPlayerCharacter::FeelOnDefence(int32 Kind)
{
	if (FeelCfg.Scale <= 0.f)
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Front = GetActorLocation() + AimForward() * 110.0 + FVector(0.0, 0.0, 20.0);
	switch (Kind)
	{
	case 0:   // block: a cool flare in front of the operator, no camera movement
		ABIBFx::HitSpark(World, Front, BIBFxColours::Cyan, 0.8f);
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Front, FRotator(0.f, AimForward().Rotation().Yaw, 90.f), FVector(60.f, 60.f, 3.f), FVector(240.f, 240.f, 2.f),
			BIBFxColours::Cyan, 0.2f, 7.f);
		BIBCombatFeel::PlayCue(World, TEXT("Block"), GetActorLocation());
		break;
	case 1:   // parry: a gold cross and a punch of the view
		ABIBFx::HitSpark(World, Front, BIBFxColours::Gold, 1.6f);
		ABIBFx::SlashStreak(World, Front, AimForward(), BIBFxColours::Gold, 340.f, 3);
		FovPunch(1.2f, 0.25f);
		BIBCombatFeel::PlayCue(World, TEXT("Parry"), GetActorLocation());
		break;
	default:  // perfect dodge: the violet bloom already exists; the view widens
		FovPunch(1.6f, 0.4f);
		BIBCombatFeel::PlayCue(World, TEXT("Dodge_Perfect"), GetActorLocation());
		break;
	}
}

// ---------------------------------------------------------------------------- ultimate beat

void ABIBPlayerCharacter::FeelUltimateBeat()
{
	UWorld* World = GetWorld();
	const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0);
	const float Beat = FeelCfg.BeatScale();
	// Charge-up: a wide ring closes onto the operator while a pillar of their colour rises.
	ABIBFx::Spawn(World, EBIBFxShape::Ring, Feet + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(1100.f, 1100.f, 3.f), FVector(60.f, 60.f, 2.f),
		Hud.Colour, 0.4f, 8.f);
	ABIBFx::Spawn(World, EBIBFxShape::Pillar, GetActorLocation(), FRotator::ZeroRotator, FVector(120.f, 120.f, 40.f), FVector(40.f, 40.f, 1400.f),
		BIBFxColours::White, 0.45f, 10.f);
	BIBCombatFeel::PlayCue(World, TEXT("Ultimate_Charge"), GetActorLocation());
	if (Beat <= 0.f || !FollowCamera)
	{
		return;
	}
	FovPunch(BIBCombatFeel::ForSlot(static_cast<int32>(EBIBSlot::Ultimate)).FovPunch, 0.5f);
	// Vignette pulse: the picture closes in for 0.6 s and opens again (the camera's post-process blends over the anime volume).
	const double RealNow = FPlatformTime::Seconds();
	BeatStartReal = RealNow;
	BeatEndReal = RealNow + 0.6;
	bBeatActive = true;
	FollowCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
	FollowCamera->PostProcessSettings.VignetteIntensity = 0.9f;
	FollowCamera->PostProcessBlendWeight = FMath::Min(1.f, Beat);
	// A brief slow-down, only when none is running (the dodge / parry / stun slow-downs keep their own length). The global
	// dilation scales every game-time clock equally, so cooldowns, windups and damage ticks keep their relative timing.
	if (SlowMoUntilReal <= RealNow)
	{
		SetSlowMo(FMath::Lerp(1.f, 0.35f, FMath::Min(1.f, Beat)), 0.18 * FMath::Min(1.f, Beat));
	}
}
