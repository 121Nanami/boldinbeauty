// Luna (4*) — Luna.md, mapped for the demo as: Ability 1 Blackhole -> Move 1, Ability 2 Orbiting -> Move 2,
// Ability 3 Event horizon -> Move 3, Ability 4 Expel -> Special, Ultimate Reality Destabilisation.
#include "BIBGame/BIBPlayerCharacter.h"

#include "BIBGame/BIBBlock.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBTuning.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "BIBGame/BIBFx.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "BIBGame/BIBInput.h"

namespace BIBLunaKit
{
	bool BlockedByWall(UWorld* World, const FVector& From, const FVector& To)
	{
		FHitResult Hit;
		return World && World->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic));
	}
}

void ABIBPlayerCharacter::LunaInput()
{
	if (Action != EBIBAction::None)
	{
		return; // hold abilities are driven from LunaUpdateAction
	}
	static const EBIBSlot Slots[] = {EBIBSlot::Move1, EBIBSlot::Move2, EBIBSlot::Move3, EBIBSlot::Special, EBIBSlot::Ultimate};
	for (const EBIBSlot Slot : Slots)
	{
		if (Pressed(InputForSlot(Slot)))
		{
			TryActivateSlot(Slot); // the GameplayAbility calls LunaActivate
			return;
		}
	}
}

bool ABIBPlayerCharacter::LunaActivate(EBIBSlot Slot)
{
	FBIBMember& Member = Active();
	switch (Slot)
	{
	case EBIBSlot::Move1:
		if (!IsReady(EBIBSlot::Move1) || LunaData.OrbPhase != 0)
		{
			Message(FString::Printf(TEXT("Blackhole not ready (%.1fs)."), Remaining(EBIBSlot::Move1)));
			return false;
		}
		FaceAim();
		StartAction(EBIBAction::LunaBlackhole);
		StartCooldown(EBIBSlot::Move1, BIBTuning::BlackholeCooldown);
		StartPose(BIBTuning::BlackholeWindup, BIBTuning::BlackholeRecovery, 1.1f);
		return true;
	case EBIBSlot::Move2:
	{
		const bool bBlack = Now() < LunaData.BlackUntil;
		const bool bWhite = Now() < LunaData.WhiteUntil;
		if (bBlack && bWhite)
		{
			Message(TEXT("Orbiting: max 1 black and 1 white hole."));
			return false;
		}
		if (!IsReady(EBIBSlot::Move2))
		{
			Message(FString::Printf(TEXT("Orbiting not ready (%.1fs)."), Remaining(EBIBSlot::Move2)));
			return false;
		}
		StartAction(EBIBAction::LunaOrbitingWindup);
		StartPose(BIBTuning::OrbitingWindup, 0.3f, 0.8f);
		return true;
	}
	case EBIBSlot::Move3:
	{
		if (!IsReady(EBIBSlot::Move3))
		{
			Message(FString::Printf(TEXT("Event horizon not ready (%.1fs)."), Remaining(EBIBSlot::Move3)));
			return false;
		}
		StartAction(EBIBAction::LunaEventHorizon);
		StartCooldown(EBIBSlot::Move3, BIBTuning::EventHorizonCooldown); // "50 second cd no matter time held"
		StartPose(0.3f, 0.3f, 1.f);
		const float Diameter = BIBTuning::EventHorizonRadius * 2.f;
		const FVector Floor = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 3.0);
		LunaData.HorizonVisual = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Cylinder, Floor, FVector(Diameter, Diameter, 4.0),
			FLinearColor(0.02f, 0.0f, 0.05f), false);
		return true;
	}
	case EBIBSlot::Special:
		if (!IsReady(EBIBSlot::Special))
		{
			Message(FString::Printf(TEXT("Expel not ready (%.1fs)."), Remaining(EBIBSlot::Special)));
			return false;
		}
		StartAction(EBIBAction::LunaExpel);
		StartPose(BIBTuning::ExpelMaxCharge, 0.3f, 0.9f);   // a long wind-up while she charges; the release lunges (LunaReleaseExpel)
		LunaData.ExpelVisual = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Sphere, GetActorLocation() + FVector(0.0, 0.0, 260.0),
			FVector(80.0), FLinearColor(1.f, 1.f, 1.f), false);
		return true;
	case EBIBSlot::Ultimate:
		if (Member.Ult < 100.f)
		{
			Message(FString::Printf(TEXT("Reality Destabilisation needs 100%% ult (%d%%)."), FMath::FloorToInt(Member.Ult)));
			return false;
		}
		if (LunaData.DestabiliseStart >= 0.f || Action == EBIBAction::LunaCutscene)
		{
			Message(TEXT("Reality Destabilisation is already tearing the room apart."));
			return false;
		}
		SetMemberUlt(ActiveIndex, 0.f);
		StartAction(EBIBAction::LunaCutscene);
		GrantInvulnerable(BIBTuning::DestabilisationCutscene); // WORKING: no damage during the cutscene
		Message(TEXT("Luna: Reality Destabilisation!"));
		return true;
	default:
		return false;
	}
}

void ABIBPlayerCharacter::LunaFireOrb()
{
	LunaData.OrbPhase = 1;
	LunaData.OrbPhaseTime = 0.f;
	LunaData.OrbDir = ActionDirection;
	LunaData.OrbPos = GetActorLocation() + ActionDirection * 120.0 + FVector(0.0, 0.0, 40.0);
	const float Size = BIBTuning::BlackholeOrbRadius * 2.f;
	LunaData.OrbVisual = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Sphere, LunaData.OrbPos, FVector(Size), FLinearColor(0.01f, 0.f, 0.02f), false);
	if (ABIBGameMode* GameMode = GetMode())
	{
		for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			Enemy->bHitByBlackholeShot = false;
		}
	}
}

void ABIBPlayerCharacter::LunaExplodeOrb()
{
	const FBIBScopedHit Scope(this, BIB::Feel::ESource::Move, true);
	Explode(LunaData.OrbPos, BIBTuning::BlackholeExplosionRadius, BIBTuning::BlackholeExplosionDamage, 900.f, FColor::Purple, FindMember(EBIBKitType::Luna));
	if (LunaData.OrbVisual.IsValid())
	{
		LunaData.OrbVisual->Destroy();
	}
	LunaData.OrbPhase = 0;
}

void ABIBPlayerCharacter::LunaReleaseExpel()
{
	const float ChargeSeconds = FMath::Min(ActionTime, BIBTuning::ExpelMaxCharge);
	const float Damage = BIBTuning::ExpelBaseDamage + BIBTuning::ExpelDamagePerSecond * ChargeSeconds;
	float Radius = BIBTuning::ExpelLargeRadius;
	float Multiplier = 1.f;
	FString SizeName = TEXT("large");
	if (ChargeSeconds >= BIBTuning::ExpelMaxCharge)
	{
		Radius = 100000.f; // all range
		Multiplier = 1.5f;
		SizeName = TEXT("ALL-RANGE");
	}
	else if (ChargeSeconds < 1.f)
	{
		Radius = BIBTuning::ExpelSmallRadius;
		Multiplier = 2.f;
		SizeName = TEXT("small");
	}
	else if (ChargeSeconds < 4.f)
	{
		Radius = BIBTuning::ExpelMediumRadius;
		Multiplier = 1.5f;
		SizeName = TEXT("medium");
	}
	StartPose(0.f, 0.35f, 1.3f);
	{
		const FBIBScopedHit Scope(this, BIB::Feel::ESource::Special, true);
		Explode(GetActorLocation(), Radius, Damage * Multiplier, 1400.f, FColor::White);
	}
	if (LunaData.ExpelVisual.IsValid())
	{
		LunaData.ExpelVisual->Destroy();
	}
	StartCooldown(EBIBSlot::Special, BIBTuning::ExpelCooldown);
	EndAction();
	Message(FString::Printf(TEXT("Expel: %s explosion, %d base damage x%.1f."), *SizeName, FMath::RoundToInt(Damage), Multiplier));
}

void ABIBPlayerCharacter::LunaClearOrbits()
{
	if (LunaData.BlackVisual.IsValid())
	{
		LunaData.BlackVisual->Destroy();
	}
	if (LunaData.WhiteVisual.IsValid())
	{
		LunaData.WhiteVisual->Destroy();
	}
	LunaData.BlackUntil = 0.f;
	LunaData.WhiteUntil = 0.f;
}

void ABIBPlayerCharacter::LunaUpdateAction(float DeltaSeconds)
{
	const FBIBMember& Member = Active();
	switch (Action)
	{
	case EBIBAction::LunaBlackhole:
		if (DamageTakenThisAction >= Member.MaxHP * BIBTuning::LunaCancelHealthFraction)
		{
			if (LunaData.OrbVisual.IsValid())
			{
				LunaData.OrbVisual->Destroy();
			}
			LunaData.OrbPhase = 0;
			EndAction();
			Message(TEXT("Blackhole cancelled (15% max health lost)."));
			return;
		}
		if (LunaData.OrbPhase == 0 && ActionTime >= BIBTuning::BlackholeWindup)
		{
			LunaFireOrb();
		}
		// 2026-10-13: locked for the windup and a short recovery (a dodge cancels it, BIBPlayerFeel.cpp IsActionInRecovery); the
		// orb flies, orbits and explodes on its own (LunaTick). Was locked through windup, shot and orbit (4.5 s).
		if (ActionTime >= BIBTuning::BlackholeWindup + BIBTuning::BlackholeRecovery)
		{
			EndAction();
		}
		break;

	case EBIBAction::LunaOrbitingWindup:
		if (DamageTakenThisAction >= Member.MaxHP * BIBTuning::LunaCancelHealthFraction)
		{
			EndAction();
			Message(TEXT("Orbiting cancelled - the hole did not summon."));
			return;
		}
		if (ActionTime >= BIBTuning::OrbitingWindup)
		{
			// Summon a black hole; if one already exists, summon a white hole.
			if (Now() >= LunaData.BlackUntil)
			{
				LunaData.BlackUntil = Now() + BIBTuning::OrbitingDuration;
				LunaData.BlackVisual = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Sphere, GetActorLocation(), FVector(110.0), FLinearColor(0.01f, 0.f, 0.02f), false);
				Message(TEXT("Black hole orbiting (90 s)."));
			}
			else
			{
				LunaData.WhiteUntil = Now() + BIBTuning::OrbitingDuration;
				LunaData.WhiteVisual = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Sphere, GetActorLocation(), FVector(110.0), FLinearColor(1.f, 1.f, 1.f), false);
				Message(TEXT("White hole orbiting (90 s) - pull and push now cancel out."));
			}
			StartCooldown(EBIBSlot::Move2, BIBTuning::OrbitingCooldown);
			EndAction();
		}
		break;

	case EBIBAction::LunaEventHorizon:
	{
		if (!Held(EBIBInput::Move3))
		{
			CancelAction();
			Message(TEXT("Event horizon released."));
			return;
		}
		const FVector Centre = GetActorLocation();
		if (LunaData.HorizonVisual.IsValid())
		{
			LunaData.HorizonVisual->SetActorLocation(Centre - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 3.0));
		}
		bool bReachedMiddle = false;
		// Only affects enemies on the floor (not walls or enemies in the air). Its damage is a tick (no hit stop).
		BIB::Feel::FHitSource HorizonTick;
		HorizonTick.Source = BIB::Feel::ESource::Move;
		HorizonTick.bTick = true;
		const FBIBScopedHit HorizonScope(this, HorizonTick);
		for (ABIBEnemy* Enemy : EnemiesInRadius(Centre, BIBTuning::EventHorizonRadius, false))
		{
			const FVector ToCentre = (Centre - Enemy->GetActorLocation()).GetSafeNormal2D();
			Enemy->AddPull(ToCentre * BIBTuning::EventHorizonPullSpeed);
			DealDamage(Enemy, BIBTuning::EventHorizonDps * DeltaSeconds);
			if (FVector::Dist2D(Centre, Enemy->GetActorLocation()) <= BIBTuning::EventHorizonCentre + Enemy->GetBodyRadius() + 40.f)
			{
				bReachedMiddle = true;
			}
		}
		if (bReachedMiddle)
		{
			// Swap to the white hole: violent close-range repel nuke.
			const FBIBScopedHit Nuke(this, BIB::Feel::ESource::Move, true);
			StartPose(0.f, 0.35f, 1.3f);
			Explode(Centre, BIBTuning::EventHorizonNukeRadius, BIBTuning::EventHorizonNukeDamage, 1800.f, FColor::White);
			CancelAction();
			Message(TEXT("Event horizon: white-hole nuke!"));
		}
		break;
	}

	case EBIBAction::LunaExpel:
	{
		if (LunaData.ExpelVisual.IsValid())
		{
			LunaData.ExpelVisual->SetActorLocation(GetActorLocation() + FVector(0.0, 0.0, 260.0));
			LunaData.ExpelVisual->SetSize(FVector(80.0 + ActionTime * 45.0));
		}
		// Activates instantly upon 10% max-health damage taken.
		const bool bForced = DamageTakenThisAction >= Member.MaxHP * BIBTuning::ExpelInterruptHealthFraction;
		if (!Held(EBIBInput::Special) || ActionTime >= BIBTuning::ExpelMaxCharge || bForced)
		{
			LunaReleaseExpel();
		}
		break;
	}

	case EBIBAction::LunaCutscene:
		if (ActionTime >= BIBTuning::DestabilisationCutscene)
		{
			LunaData.DestabiliseStart = Now();
			LunaData.DestabiliseLocation = GetActorLocation() + FVector(0.0, 0.0, 300.0);
			LunaData.NextFlipAt = Now() + BIBTuning::DestabilisationFlip;
			LunaData.bGravityInverted = false;
			LunaData.DestabiliseVisual = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Sphere, LunaData.DestabiliseLocation, FVector(260.0),
				FLinearColor(0.5f, 0.5f, 0.5f), false);
			EndAction();
			Message(FString::Printf(TEXT("Destabilisation placed: gravity flips every %.1f s, explosion in %d s."),
				BIBTuning::DestabilisationFlip, FMath::RoundToInt(BIBTuning::DestabilisationDuration)));
		}
		break;

	default:
		EndAction();
		break;
	}
}

void ABIBPlayerCharacter::LunaTick(float DeltaSeconds)
{
	const int32 LunaIndex = FindMember(EBIBKitType::Luna);
	if (LunaIndex == INDEX_NONE)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float T = Now();

	// ---------------------------------------------------------------- Move 1: Blackhole orb
	if (LunaData.OrbPhase != 0)
	{
		LunaData.OrbPhaseTime += DeltaSeconds;
		if (LunaData.OrbPhase == 1 || LunaData.OrbPhase == 3)
		{
			const float Speed = BIBTuning::BlackholeTravelSpeed * (LunaData.OrbPhase == 3 ? 1.4f : 1.f);
			const FVector Next = LunaData.OrbPos + LunaData.OrbDir * Speed * DeltaSeconds;
			const bool bWall = BIBLunaKit::BlockedByWall(World, LunaData.OrbPos, Next);
			if (!bWall)
			{
				LunaData.OrbPos = Next;
			}
			if (LunaData.OrbPhase == 1)
			{
				for (ABIBEnemy* Enemy : EnemiesInRadius(LunaData.OrbPos, BIBTuning::BlackholeOrbRadius))
				{
					if (!Enemy->bHitByBlackholeShot)
					{
						Enemy->bHitByBlackholeShot = true;
						const FBIBScopedHit Scope(this, BIB::Feel::ESource::Move);
						DealDamage(Enemy, BIBTuning::BlackholeHitDamage, LunaIndex);
					}
				}
				if (bWall || LunaData.OrbPhaseTime >= BIBTuning::BlackholeShoot)
				{
					LunaData.OrbPhase = 2;
					LunaData.OrbPhaseTime = 0.f;
					const FVector Offset = LunaData.OrbPos - GetActorLocation();
					LunaData.OrbAngle = static_cast<float>(FMath::Atan2(Offset.Y, Offset.X));
				}
			}
			else
			{
				for (ABIBEnemy* Enemy : EnemiesInRadius(LunaData.OrbPos, BIBTuning::BlackholeSuckRadius))
				{
					Enemy->AddPull((LunaData.OrbPos - Enemy->GetActorLocation()).GetSafeNormal2D() * 350.0);
				}
				if (bWall || LunaData.OrbPhaseTime >= BIBTuning::BlackholeFinal)
				{
					LunaExplodeOrb();
				}
			}
		}
		else if (LunaData.OrbPhase == 2)
		{
			// Orbits around Luna for 3 s, sucking enemies in and ticking 1250 per second inside.
			LunaData.OrbAngle += 2.f * UE_PI / BIBTuning::BlackholeOrbit * DeltaSeconds;
			LunaData.OrbPos = GetActorLocation() + FVector(FMath::Cos(LunaData.OrbAngle) * 420.0, FMath::Sin(LunaData.OrbAngle) * 420.0, 40.0);
			for (ABIBEnemy* Enemy : EnemiesInRadius(LunaData.OrbPos, BIBTuning::BlackholeSuckRadius))
			{
				Enemy->AddPull((LunaData.OrbPos - Enemy->GetActorLocation()).GetSafeNormal2D() * 500.0);
				if (FVector::Dist2D(LunaData.OrbPos, Enemy->GetActorLocation()) <= BIBTuning::BlackholeOrbRadius + Enemy->GetBodyRadius()
					&& T >= Enemy->BlackholeTickAt)
				{
					Enemy->BlackholeTickAt = T + 1.f;
					BIB::Feel::FHitSource OrbTick;
					OrbTick.Source = BIB::Feel::ESource::Move;
					OrbTick.bTick = true;
					const FBIBScopedHit Scope(this, OrbTick);
					DealDamage(Enemy, BIBTuning::BlackholeTickDamage, LunaIndex);
				}
			}
			if (LunaData.OrbPhaseTime >= BIBTuning::BlackholeOrbit)
			{
				LunaData.OrbPhase = 3;
				LunaData.OrbPhaseTime = 0.f;
				LunaData.OrbDir = AimForward();
			}
		}
		if (LunaData.OrbVisual.IsValid())
		{
			LunaData.OrbVisual->SetActorLocation(LunaData.OrbPos);
		}
		// Follow-through: a violet wake behind the orb on its flights (presentation only; one puff per 70 ms).
		const double RealNow = FPlatformTime::Seconds();
		if (FeelCfg.Scale > 0.f && RealNow >= TrailNextReal && LunaData.OrbPhase != 2)
		{
			TrailNextReal = RealNow + 0.07;
			ABIBFx::Spawn(World, EBIBFxShape::Sphere, LunaData.OrbPos, FRotator::ZeroRotator, FVector(BIBTuning::BlackholeOrbRadius * 1.6f), FVector(20.f),
				BIBFxColours::Violet, 0.22f, 6.f);
		}
	}

	// ---------------------------------------------------------------- Move 2: Orbiting holes
	if (LunaData.BlackVisual.IsValid() && T >= LunaData.BlackUntil)
	{
		ABIBFx::Spawn(World, EBIBFxShape::Sphere, LunaData.BlackVisual->GetActorLocation(), FRotator::ZeroRotator, FVector(60.f), FVector(400.f), BIBFxColours::Violet, 0.4f, 6.f); // mini supernova
		LunaData.BlackVisual->Destroy();
	}
	if (LunaData.WhiteVisual.IsValid() && T >= LunaData.WhiteUntil)
	{
		ABIBFx::Spawn(World, EBIBFxShape::Sphere, LunaData.WhiteVisual->GetActorLocation(), FRotator::ZeroRotator, FVector(60.f), FVector(400.f), BIBFxColours::White, 0.4f, 6.f);
		LunaData.WhiteVisual->Destroy();
	}
	const bool bBlack = T < LunaData.BlackUntil;
	const bool bWhite = T < LunaData.WhiteUntil;
	if ((bBlack || bWhite) && Active().Kit == EBIBKitType::Luna && bCombatActive)
	{
		LunaData.OrbitAngle += 1.6f * DeltaSeconds;
		const FVector Centre = GetActorLocation();
		const FVector BlackPos = Centre + FVector(FMath::Cos(LunaData.OrbitAngle) * BIBTuning::OrbitingRadius, FMath::Sin(LunaData.OrbitAngle) * BIBTuning::OrbitingRadius, 30.0);
		const FVector WhitePos = Centre - FVector(FMath::Cos(LunaData.OrbitAngle) * BIBTuning::OrbitingRadius, FMath::Sin(LunaData.OrbitAngle) * BIBTuning::OrbitingRadius, -30.0);
		if (LunaData.BlackVisual.IsValid())
		{
			LunaData.BlackVisual->SetActorLocation(BlackPos);
		}
		if (LunaData.WhiteVisual.IsValid())
		{
			LunaData.WhiteVisual->SetActorLocation(WhitePos);
		}
		for (ABIBEnemy* Enemy : EnemiesInRadius(Centre, BIBTuning::OrbitingPullRadius))
		{
			const FVector ToLuna = (Centre - Enemy->GetActorLocation()).GetSafeNormal2D();
			// Black pulls towards Luna, white repels; together they nullify each other.
			if (bBlack && !bWhite)
			{
				Enemy->AddPull(ToLuna * BIBTuning::OrbitingPullSpeed);
			}
			else if (bWhite && !bBlack)
			{
				Enemy->AddPull(-ToLuna * BIBTuning::OrbitingPullSpeed);
			}
			if (bBlack && FVector::Dist2D(BlackPos, Enemy->GetActorLocation()) <= 70.f + Enemy->GetBodyRadius())
			{
				Enemy->OrbitContactTime += DeltaSeconds;
				if (Enemy->OrbitContactTime >= BIBTuning::OrbitingContactDelay && T >= Enemy->OrbitNextTickAt)
				{
					Enemy->OrbitNextTickAt = T + BIBTuning::OrbitingTickInterval;
					BIB::Feel::FHitSource OrbitTick;
					OrbitTick.Source = BIB::Feel::ESource::Move;
					OrbitTick.bTick = true;
					const FBIBScopedHit Scope(this, OrbitTick);
					DealDamage(Enemy, BIBTuning::OrbitingDamage, LunaIndex);
				}
			}
			else
			{
				Enemy->OrbitContactTime = 0.f;
			}
		}
	}

	// ---------------------------------------------------------------- Ultimate: Reality Destabilisation
	if (LunaData.DestabiliseStart >= 0.f)
	{
		if (T >= LunaData.NextFlipAt)
		{
			LunaData.bGravityInverted = !LunaData.bGravityInverted;
			LunaData.NextFlipAt += BIBTuning::DestabilisationFlip;
			if (ABIBGameMode* GameMode = GetMode())
			{
				for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
				{
					Enemy->SetGravityInverted(LunaData.bGravityInverted);
				}
			}
			if (LunaData.DestabiliseVisual.IsValid())
			{
				LunaData.DestabiliseVisual->SetColor(LunaData.bGravityInverted ? FLinearColor(1.f, 1.f, 1.f) : FLinearColor(0.02f, 0.02f, 0.02f));
			}
		}
		if (T - LunaData.DestabiliseStart >= BIBTuning::DestabilisationDuration)
		{
			if (ABIBGameMode* GameMode = GetMode())
			{
				for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
				{
					Enemy->SetGravityInverted(false);
				}
			}
			LunaData.bGravityInverted = false;
			FeelSlot = EBIBSlot::Ultimate;   // presentation: this blast's hits get the ultimate's weight (not 25 s of "basic attack")
			FeelSlotAt = T;
			{
				// The ultimate's finisher (BIBRules/BIBHitFeel.h): 14-frame hit stop, knockdown, slow motion, the big impact.
				const FBIBScopedHit Scope(this, BIB::Feel::ESource::Ultimate, true);
				Explode(LunaData.DestabiliseLocation, BIBTuning::DestabilisationRadius, BIBTuning::DestabilisationDamage, 0.f, FColor(128, 128, 128), LunaIndex);
			}
			if (LunaData.DestabiliseVisual.IsValid())
			{
				LunaData.DestabiliseVisual->Destroy();
			}
			LunaData.DestabiliseStart = -1.f;
			Message(FString::Printf(TEXT("Reality Destabilisation exploded: %d to everything within %d m."),
				FMath::RoundToInt(BIBTuning::DestabilisationDamage), FMath::RoundToInt(BIBTuning::DestabilisationRadius / 100.f)));
		}
	}
}

FString ABIBPlayerCharacter::LunaKitLine() const
{
	const float T = Now();
	FString Line = FString::Printf(TEXT("Black hole: %s | White hole: %s"),
		T < LunaData.BlackUntil ? *FString::Printf(TEXT("%ds"), FMath::CeilToInt(LunaData.BlackUntil - T)) : TEXT("-"),
		T < LunaData.WhiteUntil ? *FString::Printf(TEXT("%ds"), FMath::CeilToInt(LunaData.WhiteUntil - T)) : TEXT("-"));
	if (LunaData.OrbPhase != 0)
	{
		Line += LunaData.OrbPhase == 1 ? TEXT(" | Orb: shot") : (LunaData.OrbPhase == 2 ? TEXT(" | Orb: orbiting") : TEXT(" | Orb: firing out"));
	}
	if (LunaData.DestabiliseStart >= 0.f)
	{
		Line += FString::Printf(TEXT(" | Destabilisation %ds (gravity %s)"),
			FMath::CeilToInt(BIBTuning::DestabilisationDuration - (T - LunaData.DestabiliseStart)),
			LunaData.bGravityInverted ? TEXT("INVERTED") : TEXT("normal"));
	}
	return Line;
}
