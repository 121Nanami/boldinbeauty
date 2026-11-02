#include "BIBGame/BIBTactics.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGAS/BIBAbilitySystemComponent.h"
#include "BIBGAS/BIBGameplayTags.h"
#include "BIBRules/BIBAngelMind.h"

#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	int32 EnemyKey(const ABIBEnemy* Enemy)
	{
		return static_cast<int32>(Enemy->GetUniqueID());
	}

	// bib.Tactics 0 turns the behaviour layer off (angels fall back to the 2026-10-09 behaviour); 1 = on (default).
	TAutoConsoleVariable<int32> CVarTactics(TEXT("bib.Tactics"), 1, TEXT("Angel behaviour layer: player model, squad board, steering."));
}

UBIBTactics* UBIBTactics::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || CVarTactics.GetValueOnGameThread() == 0)
	{
		return nullptr;
	}
	return World->GetSubsystem<UBIBTactics>();
}

void UBIBTactics::Refresh(ABIBGameMode* Mode, ABIBPlayerCharacter* Target)
{
	UWorld* World = GetWorld();
	if (!World || !Mode || !Target || LastFrame == GFrameCounter)
	{
		return;
	}
	LastFrame = GFrameCounter;
	const float Now = World->GetTimeSeconds();
	const TArray<ABIBEnemy*> Live = Mode->GetLiveEnemies();
	const FVector TargetAt = Target->GetActorLocation();

	// ---- sample the commander
	BIB::Tactics::FPlayerSample Sample;
	Sample.Dt = LastSampleAt < 0.f ? 0.0 : static_cast<double>(Now - LastSampleAt);
	LastSampleAt = Now;
	const FVector Velocity = Target->GetVelocity() * FVector(1.0, 1.0, 0.0);
	Sample.Speed = Velocity.Size();
	double Nearest = -1.0;
	FVector NearestAt = FVector::ZeroVector;
	for (const ABIBEnemy* Enemy : Live)
	{
		const double Distance = FVector::Dist2D(Enemy->GetActorLocation(), TargetAt);
		if (Nearest < 0.0 || Distance < Nearest)
		{
			Nearest = Distance;
			NearestAt = Enemy->GetActorLocation();
		}
	}
	Sample.NearestDistance = Nearest;
	if (Nearest > 1.0)
	{
		const FVector Away = ((TargetAt - NearestAt) * FVector(1.0, 1.0, 0.0)).GetSafeNormal();
		Sample.RetreatSpeed = FVector::DotProduct(Velocity, Away);
	}
	if (const UBIBAbilitySystemComponent* Asc = Cast<UBIBAbilitySystemComponent>(Target->GetAbilitySystemComponent()))
	{
		Sample.bCasting = Asc->HasMatchingGameplayTag(BIBTags::StateCasting());
		Sample.bDodging = Asc->HasMatchingGameplayTag(BIBTags::StateDodging());
		Sample.bBlocking = Asc->HasMatchingGameplayTag(BIBTags::StateBlocking());
		Sample.bParrying = Asc->HasMatchingGameplayTag(BIBTags::StateParrying());
		const float Max = Asc->GetMaxHealthValue();
		Sample.HealthFraction = Max > 0.f ? FMath::Clamp(Asc->GetHealthValue() / Max, 0.f, 1.f) : 1.f;
	}
	Sample.bDiscontinuity = Target->SecondsSinceSwitch() < 0.25f;
	PlayerModel.Sample(Sample);
	CurrentAdvice = PlayerModel.Advise();

	// ---- the squad board
	double Adaptation = 0.0;
	if (UBIBGameInstance* Game = GetWorld()->GetGameInstance<UBIBGameInstance>())
	{
		Adaptation = Game->GetAngelMind().Adaptation();
	}
	BIB::Tactics::FSquadInput Input;
	Input.Now = Now;
	Input.TargetX = TargetAt.X;
	Input.TargetY = TargetAt.Y;
	const FVector Forward = Target->GetActorForwardVector();
	Input.TargetFacing = FMath::Atan2(Forward.Y, Forward.X);
	// The existing cap (BIBAngelMind MaxAttackers) is the starting point; the model can only take one away while the
	// commander is hurt, or add one against a kiter. TokenCap bounds it either way.
	Input.Capacity = BIB::Angel::MaxAttackers(Live.Num(), Adaptation) + CurrentAdvice.TokenDelta + (CurrentAdvice.GapClose > 0.3 ? 1 : 0);
	Input.Flankers = CurrentAdvice.bKiting ? 2 : 1;
	std::vector<BIB::Tactics::FAgent> Agents;
	Agents.reserve(static_cast<size_t>(Live.Num()));
	for (const ABIBEnemy* Enemy : Live)
	{
		BIB::Tactics::FAgent Agent;
		Agent.Id = EnemyKey(Enemy);
		Agent.Class = Enemy->IsBoss() ? BIB::Tactics::EClass::Boss
			: ((Enemy->GetKind() == EBIBEnemyKind::Ranged || Enemy->GetKind() == EBIBEnemyKind::Mage) ? BIB::Tactics::EClass::Ranged : BIB::Tactics::EClass::Melee);
		Agent.X = Enemy->GetActorLocation().X;
		Agent.Y = Enemy->GetActorLocation().Y;
		Agent.bCommitted = Enemy->IsCommitted();
		Agents.push_back(Agent);
	}
	Squad.Update(Input, Agents);
}

bool UBIBTactics::Knows(const ABIBEnemy* Enemy) const
{
	return Enemy && Squad.Knows(EnemyKey(Enemy));
}

bool UBIBTactics::HasToken(const ABIBEnemy* Enemy) const
{
	return Enemy && Squad.HasToken(EnemyKey(Enemy));
}

BIB::Tactics::EDuty UBIBTactics::DutyOf(const ABIBEnemy* Enemy) const
{
	return Enemy ? Squad.DutyOf(EnemyKey(Enemy)) : BIB::Tactics::EDuty::Wait;
}

double UBIBTactics::SlotAngle(const ABIBEnemy* Enemy) const
{
	return Enemy ? Squad.SlotAngle(EnemyKey(Enemy)) : 0.0;
}

FVector UBIBTactics::SteerDirection(const ABIBEnemy& Enemy, const FVector& Desired, FBIBSteerMemory& Memory, float& SpeedScale) const
{
	SpeedScale = 1.f;
	UWorld* World = GetWorld();
	const FVector Flat = Desired.GetSafeNormal2D();
	if (!World || Flat.IsNearlyZero())
	{
		return Desired;
	}
	const float Now = World->GetTimeSeconds();
	const FVector At = Enemy.GetActorLocation();
	if (Now - Memory.LastCallAt > 0.3f)
	{
		BIB::Tactics::NoteProgress(Memory.State, Now, At.X, At.Y, false);   // it had stopped on purpose: restart the window
	}
	Memory.LastCallAt = Now;
	BIB::Tactics::NoteProgress(Memory.State, Now, At.X, At.Y, true);

	// Cast the whiskers about nine times a second (or at once when the wanted heading turns by more than ~30 degrees).
	if (Now >= Memory.NextCastAt || FVector::DotProduct(Flat, Memory.Desired) < 0.87f)
	{
		Memory.NextCastAt = Now + FMath::FRandRange(0.08f, 0.14f);   // staggered, so a crowd does not cast on one frame
		Memory.Desired = Flat;
		const float Radius = FMath::Max(20.f, Enemy.GetBodyRadius() * 0.8f);
		const float Reach = static_cast<float>(BIB::Tactics::LookAheadCm) + Radius;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BIBAngelSteer), false, &Enemy);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		BIB::Tactics::FWhiskers Whiskers;
		for (int32 Index = 0; Index < BIB::Tactics::WhiskerCount; ++Index)
		{
			const float Degrees = FMath::RadiansToDegrees(static_cast<float>(BIB::Tactics::WhiskerAngle(Index)));
			const FVector Dir = Flat.RotateAngleAxis(Degrees, FVector::UpVector);
			FHitResult Hit;
			const bool bHit = World->SweepSingleByObjectType(Hit, At, At + Dir * Reach, FQuat::Identity, Objects,
				FCollisionShape::MakeSphere(Radius), Params);
			Whiskers.Clear[Index] = (bHit && !Hit.bStartPenetrating) ? FMath::Clamp(Hit.Distance / Reach, 0.f, 1.f) : 1.f;
		}
		Memory.Result = BIB::Tactics::Steer(Whiskers, Memory.State, Now);
	}
	else if (Now < Memory.State.SlideUntil)
	{
		// A slide keeps its side between casts.
		Memory.Result.Angle = BIB::Tactics::WhiskerAngle(Memory.State.SlideSide > 0 ? 4 : 0);
	}
	SpeedScale = static_cast<float>(Memory.Result.Speed);
	return Flat.RotateAngleAxis(FMath::RadiansToDegrees(static_cast<float>(Memory.Result.Angle)), FVector::UpVector);
}
