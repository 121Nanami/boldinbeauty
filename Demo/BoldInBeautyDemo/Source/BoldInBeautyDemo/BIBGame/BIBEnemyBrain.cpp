// The angels' tactical brain in the world (owner request 2026-10-13, goal 2: "every Angel AI should fight at or above a skilled
// player's level while staying fair"; "make the angels overall a bit harder to face"). The decisions are BIBRules/BIBAngelBrain.h
// (engine-free, tested in Tests/angel_brain_tests.cpp); this file perceives, moves and executes them:
//   * perception: a line-of-sight trace every 0.2 s within the tier's sight range (walls block it, angels do not), memory of
//     the last known position, call-outs from angels nearby, and the commander as it was ReactionSeconds ago (UBIBAngelDirector);
//   * turning at the tier's TurnRateDeg; melee strikes only land inside their swing cone, so a sidestep during the windup works;
//   * moves: sidestep / backstep, guard, retreat, strafing and ring points, flank, telegraphed lunge / dash strike / charge
//     (super armour), blink, combos with quick / delayed / sweep / unblockable enders, feints that flash twice, ranged shots that
//     fly (aim follows, then locks and flashes), mage blasts aimed where the commander was seen, novas and the boss's slam.
// Every attack telegraphs for at least the tier's MinTelegraph (FAction.Windup is already clamped; follow-ups are clamped here).
#include "BIBGame/BIBEnemy.h"

#include "BIBGame/BIBAngelDirector.h"
#include "BIBGame/BIBFx.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBMotionEvents.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBTuning.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

// Named (not anonymous) so unity builds cannot mix these helpers up with another file's.
namespace BIBEnemyBrainImpl
{
	constexpr float SightCheckSeconds = 0.2f;
	constexpr float EvadeSeconds = 0.25f;
	constexpr float MeleeConeDegrees = 70.f;    // half angle of a strike
	constexpr float SweepConeDegrees = 150.f;   // half angle of a sweep (catches a sidestep)
	constexpr float SweepExtraReach = 80.f;
	constexpr float NovaRadius = 420.f;
	constexpr float ShotHitHeight = 200.f;

	FVector Flat(const BIB::Angel::FV2& Point, double Z) { return FVector(Point.X, Point.Y, Z); }

	/** Damage of a move relative to the archetype's attack. */
	float MoveDamage(BIB::Angel::EMove Move)
	{
		namespace A = BIB::Angel;
		switch (Move)
		{
		case A::EMove::Combo: return 0.9f;
		case A::EMove::Unblockable: return 1.25f;
		case A::EMove::Nova: return 1.3f;
		case A::EMove::Volley: return 0.5f;   // per shot, three shots
		case A::EMove::AntiAir: return 0.9f;
		case A::EMove::DashStrike: return 1.1f;
		case A::EMove::Lunge: return 1.2f;
		default: return 1.f;
		}
	}

	BIB::Angel::FMind& FallbackMind()
	{
		static BIB::Angel::FMind Mind;
		return Mind;
	}
}

UBIBAngelDirector* ABIBEnemy::GetDirector() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UBIBAngelDirector>() : nullptr;
}

bool ABIBEnemy::IsCommittedForCrowd() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	return IsCommitted() || (bDashStrikes && Now < DashUntil);
}

float ABIBEnemy::GetSecondsToStrike() const
{
	float Best = -1.f;
	auto Take = [&Best](float Seconds)
	{
		if (Seconds >= 0.f && (Best < 0.f || Seconds < Best))
		{
			Best = Seconds;
		}
	};
	if (AttackWindup > 0.f)
	{
		Take(AttackWindup);
	}
	if (SlamWindup > 0.f)
	{
		Take(SlamWindup);
	}
	if (const ABIBPlayerCharacter* Target = GetTarget())
	{
		const FVector At = Target->GetActorLocation();
		for (const FBIBAngelShot& Shot : Shots)
		{
			const float Speed = static_cast<float>(Shot.Vel.Size2D());
			const FVector To = At - Shot.Pos;
			if (Speed > 1.f && FVector::DotProduct(To.GetSafeNormal2D(), Shot.Vel.GetSafeNormal2D()) > 0.f)
			{
				Take(static_cast<float>(To.Size2D()) / Speed);
			}
		}
	}
	return Best;
}

void ABIBEnemy::ReleaseToken()
{
	if (UBIBAngelDirector* Director = GetDirector())
	{
		Director->GetCrowd().Release(GetBrainId(), GetWorld()->GetTimeSeconds(), Knobs.Aggression);
	}
}

void ABIBEnemy::CancelAttack()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bWasCommitted = AttackWindup > 0.f || SlamWindup > 0.f || PendingFollowUps > 0 || (bDashStrikes && Now < DashUntil);
	AttackWindup = -1.f;
	SlamWindup = -1.f;
	PendingFollowUps = 0;
	SecondFlashAt = -1.f;
	bSuperArmor = false;
	if (bDashStrikes)
	{
		DashUntil = -1.f;
		bDashStrikes = false;
	}
	if (bWasCommitted)
	{
		ReleaseToken();
	}
}

void ABIBEnemy::FinishAttack()
{
	const float Now = GetWorld()->GetTimeSeconds();
	RecoverUntil = Now + (Kind == EBIBEnemyKind::Assassin ? 0.25f : 0.35f);
	bSuperArmor = false;
	SecondFlashAt = -1.f;
	bCurrentUnblockable = false;
	ReleaseToken();
}

// ============================================================================ perception and turning

void ABIBEnemy::Perceive(ABIBPlayerCharacter* Target, float Now)
{
	if (Now < NextSightCheckAt)
	{
		return;
	}
	// Staggered so a room of angels does not trace on the same frame.
	NextSightCheckAt = Now + BIBEnemyBrainImpl::SightCheckSeconds + 0.01f * static_cast<float>(GetBrainId() % 5);
	UWorld* World = GetWorld();
	UBIBAngelDirector* Director = GetDirector();
	const FVector Eye = GetActorLocation() + FVector(0.0, 0.0, 50.0);
	const FVector Aim = Target->GetActorLocation() + FVector(0.0, 0.0, 30.0);
	const TArray<ABIBEnemy*> Live = Director ? Director->LiveAngels() : TArray<ABIBEnemy*>();
	bool bSees = FVector::Dist2D(Eye, Aim) <= Knobs.SightRange;
	if (bSees)
	{
		// Walls and level geometry block the view; other angels and the commander's own body do not.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BIBAngelSight), false, this);
		Params.AddIgnoredActor(Target);
		for (ABIBEnemy* Other : Live)
		{
			Params.AddIgnoredActor(Other);
		}
		bSees = !World->LineTraceTestByChannel(Eye, Aim, ECC_Visibility, Params);
	}
	bSeesCommander = bSees;
	if (bSees && Director)
	{
		Director->NoteSighting(GetActorLocation(), Now);
	}
	// Ranged: is another angel standing in the line of fire?
	bAllyInLine = false;
	if (IsRangedRole() && bSees)
	{
		const FVector From = GetActorLocation();
		const FVector Line = Aim - From;
		const double LengthSq = FMath::Max(1.0, Line.SizeSquared2D());
		for (const ABIBEnemy* Other : Live)
		{
			if (Other == this)
			{
				continue;
			}
			const FVector To = Other->GetActorLocation() - From;
			const double Along = (To.X * Line.X + To.Y * Line.Y) / LengthSq;
			if (Along > 0.05 && Along < 0.95)
			{
				const FVector Closest = From + Line * Along;
				if (FVector::Dist2D(Closest, Other->GetActorLocation()) < Other->GetBodyRadius() + 35.f)
				{
					bAllyInLine = true;
					break;
				}
			}
		}
	}
}

FVector ABIBEnemy::LookPoint(const ABIBPlayerCharacter* Target) const
{
	if (bSeesCommander && Target)
	{
		return Target->GetActorLocation();
	}
	return BIBEnemyBrainImpl::Flat(Brain.LastKnown, GetActorLocation().Z);
}

void ABIBEnemy::TurnToward(const FVector& Point, float DeltaSeconds)
{
	FVector To = Point - GetActorLocation();
	To.Z = 0.0;
	if (To.SizeSquared() < 1.0)
	{
		return;
	}
	const FRotator Want(0.0, To.Rotation().Yaw, 0.0);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Want, DeltaSeconds, static_cast<float>(Knobs.TurnRateDeg)));
}

bool ABIBEnemy::InStrikeCone(const AActor* Target, float Range, float HalfAngleDegrees) const
{
	if (!Target)
	{
		return false;
	}
	FVector To = Target->GetActorLocation() - GetActorLocation();
	if (FMath::Abs(To.Z) > 250.0)
	{
		return false;
	}
	To.Z = 0.0;
	if (To.Size() > Range)
	{
		return false;
	}
	const double Cos = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), To.GetSafeNormal());
	return To.Size() < BodyRadius + 30.0 || Cos >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

// ============================================================================ the brain's frame

void ABIBEnemy::TickBrain(float DeltaSeconds, ABIBPlayerCharacter* Target, float Distance, const FVector& Direction, float MoveMult, float DealtScale)
{
	namespace A = BIB::Angel;
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	UBIBAngelDirector* Director = GetDirector();
	if (!Director || !Target)
	{
		return;
	}
	Director->UpdateFrame();
	Knobs = A::EffectiveKnobs(A::TierFor(GetRole()), Director->GetChapter());
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	Perceive(Target, Now);
	const FVector Look = LookPoint(Target);

	if (DashUntil > Now)
	{
		TickDash(DeltaSeconds, Target, DealtScale);
		return;
	}
	if (IsGuarding())
	{
		TurnToward(Look, DeltaSeconds);
		if (GuardHits >= 3)
		{
			// Soaked three hits on the guard: a quick riposte (still telegraphed).
			GuardUntil = -1.f;
			A::FAction Riposte;
			Riposte.Intent = A::EIntent::Attack;
			Riposte.Move = A::EMove::Strike;
			Riposte.Windup = Knobs.MinTelegraph;
			StartAttack(Riposte, Target);
		}
		return;
	}
	if (AttackWindup > 0.f)
	{
		// Melee swings lock their direction for the last part of the windup (a sidestep then escapes the cone); shots aim on.
		if (IsRangedRole() || AttackWindup > static_cast<float>(Knobs.MinTelegraph) * 0.6f)
		{
			TurnToward(Look, DeltaSeconds);
		}
		AttackWindup -= DeltaSeconds;
		if (SecondFlashAt > 0.f && Now >= SecondFlashAt)
		{
			// The real strike of a feint / delayed ender: its own flash, at least MinTelegraph before it lands.
			SecondFlashAt = -1.f;
			ABIBFx::Telegraph(World, GetActorLocation() + FVector(0.0, 0.0, 60.0), bCurrentUnblockable);
			OnAttackTelegraph();
		}
		if (IsRangedRole())
		{
			UpdateAim(Target);
		}
		else if (Distance > AttackRange * 0.8f && CurrentMove != A::EMove::Charge && CurrentMove != A::EMove::Lunge && CurrentMove != A::EMove::DashStrike)
		{
			AddMovementInput(Direction, MoveMult * 0.35f);   // creeps in during the windup
		}
		if (AttackWindup <= 0.f)
		{
			AttackWindup = -1.f;
			ResolveAttack(Target, nullptr, Distance, DealtScale);
		}
		return;
	}
	if (Now < RecoverUntil)
	{
		TurnToward(Look, DeltaSeconds);
		if (Kind == EBIBEnemyKind::Assassin)
		{
			AddMovementInput(-Direction, MoveMult);   // hit and back off
		}
		return;
	}

	// Call-outs: an angel that cannot see the commander hears where its group saw it (a little later).
	if (!bSeesCommander)
	{
		double SeenAt = 0.0;
		if (Director->HeardSighting(GetActorLocation(), Now, SeenAt) && SeenAt > Brain.LastSeenAt)
		{
			Brain.LastSeenAt = SeenAt;
			Brain.LastKnown = Director->ViewFor(Knobs.ReactionSeconds + BIBAngelSenses::CallOutDelay).Pos;
		}
	}

	A::FThink In;
	In.Id = GetBrainId();
	In.Now = Now;
	In.Interval = AttackIntervalS;
	In.Role = GetRole();
	In.Tier = A::TierFor(In.Role);
	In.Knobs = Knobs;
	In.Pos = {GetActorLocation().X, GetActorLocation().Y};
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	In.Facing = {Forward.X, Forward.Y};
	In.Reach = AttackRange;
	In.KeepMin = KeepMinCm;
	In.KeepMax = KeepMaxCm;
	In.BaseWindup = AttackWindupS;
	In.HealthFraction = GetHealthFraction();
	In.StrainFraction = GetDazeFraction();
	In.bPhaseTwo = bPhaseTwo;
	In.bSees = bSeesCommander;
	In.bLineBlockedByAlly = bAllyInLine;
	FVector Centre = FVector::ZeroVector;
	for (const ABIBEnemy* Other : Director->LiveAngels())
	{
		if (Other != this && FVector::Dist2D(Other->GetActorLocation(), GetActorLocation()) < 900.0)
		{
			Centre += Other->GetActorLocation();
			++In.NearbyAllies;
		}
	}
	if (In.NearbyAllies > 0)
	{
		Centre /= In.NearbyAllies;
		In.AllyCentre = {Centre.X, Centre.Y};
	}
	In.View = Director->ViewFor(Knobs.ReactionSeconds);
	In.CommanderNow = bSeesCommander ? A::FV2{Target->GetActorLocation().X, Target->GetActorLocation().Y} : Brain.LastKnown;
	In.bHealing = Director->HealingFor(Knobs.ReactionSeconds);
	const A::FMind* Mind = GetMind();
	const A::FAction Action = A::Think(In, Mind ? *Mind : BIBEnemyBrainImpl::FallbackMind(), Director->GetHabits(), Director->GetCrowd(), Brain);
	ExecuteAction(Action, Target, Direction, MoveMult);
}

void ABIBEnemy::ExecuteAction(const BIB::Angel::FAction& Action, ABIBPlayerCharacter* Target, const FVector& Direction, float MoveMult)
{
	namespace A = BIB::Angel;
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	const float DeltaSeconds = World->GetDeltaSeconds() * CustomTimeDilation;
	const FVector Look = LookPoint(Target);
	const double Z = GetActorLocation().Z;
	switch (Action.Intent)
	{
	case A::EIntent::Attack:
		TurnToward(Look, DeltaSeconds);
		StartAttack(Action, Target);
		return;
	case A::EIntent::Sidestep:
	case A::EIntent::Backstep:
	{
		// A quick evasion (no invulnerability: it simply is not where the swing goes).
		const FVector Dir = FVector(Action.Goal.X, Action.Goal.Y, 0.0).GetSafeNormal();
		StartDash(Dir, static_cast<float>(A::AiTuning().SidestepDistance) / BIBEnemyBrainImpl::EvadeSeconds, BIBEnemyBrainImpl::EvadeSeconds, false);
		ABIBFx::Spawn(World, EBIBFxShape::Slash, GetActorLocation(), Dir.Rotation(), FVector(BodyRadius * 2.f, 6.f, 60.f),
			FVector(static_cast<float>(A::AiTuning().SidestepDistance), 2.f, 20.f), BIBFxColours::Cyan, 0.2f, 4.f);
		return;
	}
	case A::EIntent::Guard:
		TurnToward(Look, DeltaSeconds);
		GuardUntil = Now + 0.9f;
		GuardHits = 0;
		ABIBFx::Spawn(World, EBIBFxShape::Sphere, GetActorLocation(), FRotator::ZeroRotator, FVector(BodyRadius * 2.f), FVector(BodyRadius * 3.2f),
			BIBFxColours::Cyan, 0.3f, 3.f);
		return;
	case A::EIntent::Hold:
		TurnToward(Look, DeltaSeconds);
		return;
	default:
		break;
	}

	// Movement intents.
	const FVector Goal = BIBEnemyBrainImpl::Flat(Action.Goal, Z);
	FVector ToGoal = Goal - GetActorLocation();
	ToGoal.Z = 0.0;
	FVector ToLook = Look - GetActorLocation();
	ToLook.Z = 0.0;
	const FVector Side = FVector::CrossProduct(ToLook.GetSafeNormal().IsNearlyZero() ? Direction : ToLook.GetSafeNormal(), FVector::UpVector)
		* static_cast<float>(Action.StrafeSign);
	FVector Move = FVector::ZeroVector;
	switch (Action.Intent)
	{
	case A::EIntent::Approach:
	case A::EIntent::Retreat:
		Move = ToGoal.Size() > 30.0 ? ToGoal.GetSafeNormal() : FVector::ZeroVector;
		break;
	case A::EIntent::Strafe:
		Move = Side;
		break;
	case A::EIntent::Reposition:
		Move = Side * 0.8f + (ToGoal.Size() > 30.0 ? ToGoal.GetSafeNormal() * 0.6f : FVector::ZeroVector);
		break;
	default:
		break;
	}
	Move += Separation() * 0.6f;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * FMath::Clamp(static_cast<float>(Action.Speed), 0.3f, 1.6f);
	if (!Move.IsNearlyZero())
	{
		AddMovementInput(Move.GetSafeNormal2D(), MoveMult);
	}
	TurnToward(Look, DeltaSeconds);
}

// ============================================================================ attacks

void ABIBEnemy::StartAttack(const BIB::Angel::FAction& Action, ABIBPlayerCharacter* Target)
{
	namespace A = BIB::Angel;
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	UBIBAngelDirector* Director = GetDirector();
	CurrentAction = Action;
	CurrentMove = Action.Move;
	bCurrentUnblockable = Action.bUnblockable;
	CurrentDamageMult = static_cast<float>(Action.DamageMult) * BIBEnemyBrainImpl::MoveDamage(Action.Move);
	PendingFollowUps = FMath::Clamp(Action.FollowUps, 0, 3);
	FollowUpIndex = 0;
	bSuperArmor = Action.bSuperArmor;
	SecondFlashAt = -1.f;
	bAimLocked = false;
	const float Windup = FMath::Max(static_cast<float>(Action.Windup), static_cast<float>(Knobs.MinTelegraph));
	const FVector Ground = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.0);
	switch (Action.Move)
	{
	case A::EMove::Feint:
		// Flashes now, holds, flashes again: the real strike comes FeintDelay (>= MinTelegraph) after the second flash.
		AttackWindup = Windup + FMath::Max(static_cast<float>(Action.FeintDelay), static_cast<float>(Knobs.MinTelegraph));
		SecondFlashAt = Now + Windup;
		BIBMotionEvents::Shared(TEXT("feint-fake-out-flicker"), this);
		break;
	case A::EMove::Blink:
		if (Target)
		{
			// Blink in behind the commander (seen: a pillar out, a pillar in), then a telegraphed strike.
			BIBMotionEvents::Shared(TEXT("blink-out-in"), this);
			const FVector Behind = Target->GetActorLocation() - Target->GetActorForwardVector().GetSafeNormal2D() * (AttackRange * 0.8f);
			ABIBFx::Spawn(World, EBIBFxShape::Pillar, GetActorLocation(), FRotator::ZeroRotator, FVector(BodyRadius * 2.f, BodyRadius * 2.f, 300.f),
				FVector(10.f, 10.f, 500.f), BIBFxColours::Violet, 0.3f, 8.f);
			TeleportTo(FVector(Behind.X, Behind.Y, GetActorLocation().Z), (Target->GetActorLocation() - Behind).Rotation(), false, true);
			ABIBFx::Spawn(World, EBIBFxShape::Pillar, GetActorLocation(), FRotator::ZeroRotator, FVector(10.f, 10.f, 500.f),
				FVector(BodyRadius * 2.f, BodyRadius * 2.f, 300.f), BIBFxColours::Violet, 0.25f, 8.f);
		}
		AttackWindup = Windup;
		break;
	case A::EMove::Nova:
		if (Kind == EBIBEnemyKind::Boss)
		{
			// The chapter boss's slam: the big rose circle (Tick resolves it).
			SlamWindup = FMath::Max(static_cast<float>(Knobs.MinTelegraph), BIBTuning::BossSlamWindup * (bPhaseTwo ? 0.8f : 1.f));
			bSuperArmor = true;
			ABIBFx::AreaWarning(World, Ground, BIBTuning::BossSlamRadius, SlamWindup, BIBFxColours::Rose);
			ABIBFx::Telegraph(World, GetActorLocation() + FVector(0.0, 0.0, 80.0), false);
			OnAttackTelegraph();
			if (Director)
			{
				Director->NoteTelegraph(Now, Now + SlamWindup);
			}
			return;
		}
		AttackWindup = Windup;
		BIBMotionEvents::Shared(TEXT("mirror-tint-copied"), this);
		ABIBFx::AreaWarning(World, Ground, BIBEnemyBrainImpl::NovaRadius + BodyRadius, AttackWindup, BIBFxColours::Violet);
		break;
	case A::EMove::Volley:
	case A::EMove::DashStrike:
		BIBMotionEvents::Shared(TEXT("mirror-tint-copied"), this);
		AttackWindup = Windup;
		break;
	case A::EMove::Charge:
	case A::EMove::Lunge:
		BIBMotionEvents::Shared(TEXT("assassin-dash-streak"), this, BIBMotionEvents::FacesLeft(this));
		AttackWindup = Windup;
		break;
	default:
		AttackWindup = Windup;
		break;
	}
	if (Action.FeintDelay > 0.0 && Action.Move != A::EMove::Feint)
	{
		// A held attack (ranged mixup): the glow stays on, a second flash comes FeintDelay (>= MinTelegraph) before it lands.
		const float Hold = FMath::Max(static_cast<float>(Action.FeintDelay), static_cast<float>(Knobs.MinTelegraph));
		SecondFlashAt = Now + AttackWindup;
		AttackWindup += Hold;
	}
	if (IsRangedRole())
	{
		// The aim follows what the angel sees until the last moments of the windup, then locks (a second flash).
		AimLockAt = FMath::Min(AttackWindup * 0.5f, 0.35f);
		UpdateAim(Target);
		if (Kind == EBIBEnemyKind::Mage && CurrentMove == A::EMove::Strike)
		{
			// A blast is placed where the commander was seen (plus the dodge habit) and stays there: step out of the circle.
			AoeCenter = FVector(AimPoint.X, AimPoint.Y, Ground.Z);
			bAimLocked = true;
			ABIBFx::AreaWarning(World, AoeCenter, AoeRadiusCm, AttackWindup, bCurrentUnblockable ? BIBFxColours::Rose : BIBFxColours::Violet);
		}
	}
	ABIBFx::Telegraph(World, GetActorLocation() + FVector(0.0, 0.0, 60.0), bCurrentUnblockable);
	OnAttackTelegraph();
	if (Director)
	{
		Director->NoteTelegraph(Now, Now + AttackWindup);
	}
}

void ABIBEnemy::UpdateAim(ABIBPlayerCharacter* Target)
{
	UBIBAngelDirector* Director = GetDirector();
	if (bAimLocked || !Director || !Target)
	{
		return;
	}
	// Where the angel saw the commander (its reaction delay), led by the movement it saw and leaning to the dodge habit.
	const BIB::Angel::FPlayerView View = Director->ViewFor(Knobs.ReactionSeconds);
	const double Z = Target->GetActorLocation().Z;
	const FVector Seen = BIBEnemyBrainImpl::Flat(View.Pos, Z);
	const double Flight = FVector::Dist2D(GetActorLocation(), Seen) / FMath::Max(1.0, BIB::Angel::AiTuning().ShotSpeed);
	const double LeadTime = (Kind == EBIBEnemyKind::Mage ? 0.0 : Flight + FMath::Max(0.f, AttackWindup)) * BIB::Angel::AiTuning().ShotLead;
	AimPoint = Seen + FVector(View.Vel.X, View.Vel.Y, 0.0) * LeadTime + BIBEnemyBrainImpl::Flat(CurrentAction.AimOffset, 0.0);
	if (AttackWindup > 0.f && AttackWindup <= AimLockAt)
	{
		bAimLocked = true;
		if (Kind == EBIBEnemyKind::Ranged || CurrentMove != BIB::Angel::EMove::Strike)
		{
			// The aim locks: a thin line shows where the shot will fly.
			const FVector From = GetActorLocation();
			const float Length = static_cast<float>(FVector::Dist(From, AimPoint));
			ABIBFx::Spawn(GetWorld(), EBIBFxShape::Slash, (From + AimPoint) * 0.5, (AimPoint - From).Rotation() + FRotator(0.f, 90.f, 0.f),
				FVector(Length, 3.f, 3.f), FVector(Length, 1.f, 1.f), bCurrentUnblockable ? BIBFxColours::Rose : BIBFxColours::Gold, AttackWindup, 2.f);
		}
	}
}

void ABIBEnemy::ResolveAttack(ABIBPlayerCharacter* Target, ABIBEnemy* Victim, float Distance, float DealtScale)
{
	namespace A = BIB::Angel;
	UWorld* World = GetWorld();
	const float Damage = AttackDamage * DealtScale * CurrentDamageMult;
	const FVector Ground = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.0);
	AActor* Struck = Victim ? static_cast<AActor*>(Victim) : static_cast<AActor*>(Target);
	if (Victim)
	{
		// Illusion: a plain hit on the other angel.
		if (Distance <= AttackRange + 80.f)
		{
			HitTarget(Target, Victim, Damage, true);
		}
		RecoverUntil = World->GetTimeSeconds() + 0.35f;
		bCurrentUnblockable = false;
		return;
	}
	switch (CurrentMove)
	{
	case A::EMove::Nova:
		ABIBFx::ShockRing(World, Ground, BIBEnemyBrainImpl::NovaRadius + BodyRadius, BIBFxColours::Violet, 0.4f);
		if (Distance <= BIBEnemyBrainImpl::NovaRadius + BodyRadius)
		{
			HitTarget(Target, nullptr, Damage, false);
		}
		break;
	case A::EMove::Volley:
		for (int32 Shot = -1; Shot <= 1; ++Shot)
		{
			const FVector From = GetActorLocation();
			const FVector To = From + (AimPoint - From).RotateAngleAxis(Shot * 10.0, FVector::UpVector);
			FireShot(To, Damage, 1.f);
		}
		break;
	case A::EMove::AntiAir:
		if (IsRangedRole())
		{
			FireShot(AimPoint, Damage, 1.4f);
		}
		else if (Target && (Target->IsAirborneForAngels() || InStrikeCone(Target, AttackRange + 80.f, BIBEnemyBrainImpl::MeleeConeDegrees)))
		{
			ABIBFx::SlashStreak(World, (GetActorLocation() + Target->GetActorLocation()) * 0.5, Target->GetActorLocation() - GetActorLocation(), BIBFxColours::Gold, Distance, 1);
			HitTarget(Target, nullptr, Damage, false);
		}
		break;
	case A::EMove::DashStrike:
	case A::EMove::Lunge:
	case A::EMove::Charge:
	{
		// The telegraph is over: rush along the line to the commander (locked now), hitting once on contact.
		FVector Line = (Target ? Target->GetActorLocation() : GetActorLocation() + GetActorForwardVector()) - GetActorLocation();
		Line.Z = 0.0;
		const float Speed = CurrentMove == A::EMove::Lunge ? BIBTuning::AssassinDashSpeed : (CurrentMove == A::EMove::Charge ? BaseWalkSpeed * 3.2f : 1900.f);
		const float Seconds = CurrentMove == A::EMove::Charge ? 0.75f : FMath::Clamp(static_cast<float>(Line.Size()) / Speed + 0.1f, 0.15f, 0.45f);
		StartDash(Line.GetSafeNormal(), Speed, Seconds, true);
		return;   // FinishAttack when the rush ends
	}
	default:
		if (IsRangedRole())
		{
			if (Kind == EBIBEnemyKind::Mage)
			{
				ABIBFx::ShockRing(World, AoeCenter, AoeRadiusCm, BIBFxColours::Violet, 0.4f);
				ABIBFx::Spawn(World, EBIBFxShape::Pillar, AoeCenter, FRotator::ZeroRotator, FVector(AoeRadiusCm * 1.6f, AoeRadiusCm * 1.6f, 40.f),
					FVector(AoeRadiusCm * 0.4f, AoeRadiusCm * 0.4f, 900.f), BIBFxColours::Violet, 0.3f, 6.f);
				if (Target && FVector::Dist2D(Target->GetActorLocation(), AoeCenter) <= AoeRadiusCm)
				{
					HitTarget(Target, nullptr, Damage, false);
				}
			}
			else
			{
				FireShot(AimPoint, Damage, 1.f);
			}
			break;
		}
		{
			// A melee strike lands only inside its swing cone: a sidestep during the windup beats a strike, a sweep catches it.
			const bool bSweep = CurrentMove == A::EMove::Sweep;
			const float Reach = AttackRange + 80.f + (bSweep ? BIBEnemyBrainImpl::SweepExtraReach : 0.f);
			const float Cone = bSweep ? BIBEnemyBrainImpl::SweepConeDegrees : BIBEnemyBrainImpl::MeleeConeDegrees;
			const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
			ABIBFx::SlashStreak(World, GetActorLocation() + Facing * (AttackRange * 0.6f) + FVector(0.0, 0.0, 40.0), Facing,
				bCurrentUnblockable ? BIBFxColours::Rose : (bSweep ? BIBFxColours::Violet : BIBFxColours::White), Reach * (bSweep ? 1.6f : 1.f), FollowUpIndex % 2);
			if (InStrikeCone(Struck, Reach, Cone))
			{
				HitTarget(Target, nullptr, Damage, true);
			}
		}
		break;
	}
	bCurrentUnblockable = false;
	if (PendingFollowUps > 0)
	{
		StartFollowUp();
		return;
	}
	FinishAttack();
}

void ABIBEnemy::StartFollowUp()
{
	namespace A = BIB::Angel;
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	--PendingFollowUps;
	const A::EEnder Ender = CurrentAction.Enders[FMath::Clamp(FollowUpIndex, 0, 2)];
	++FollowUpIndex;
	CurrentMove = A::EMove::Strike;
	CurrentDamageMult = 0.85f;
	float Windup = FMath::Max(static_cast<float>(Knobs.MinTelegraph), AttackWindupS * 0.55f);
	switch (Ender)
	{
	case A::EEnder::Delayed:
	{
		// Held: the glow stays on, the flash comes MinTelegraph before the strike (catches a dodge thrown on rhythm).
		const float Hold = static_cast<float>(A::AiTuning().FeintExtraDelay);
		SecondFlashAt = Now + Hold;
		Windup += Hold;
		break;
	}
	case A::EEnder::Sweep:
		CurrentMove = A::EMove::Sweep;
		Windup *= 1.2f;
		CurrentDamageMult = 0.95f;
		break;
	case A::EEnder::Unblockable:
		bCurrentUnblockable = true;
		Windup = FMath::Max(Windup, static_cast<float>(Knobs.MinTelegraph) + 0.1f);
		CurrentDamageMult = 1.1f;
		break;
	default:
		break;
	}
	AttackWindup = Windup;
	ABIBFx::Telegraph(World, GetActorLocation() + FVector(0.0, 0.0, 60.0), bCurrentUnblockable);
	OnAttackTelegraph();
	if (UBIBAngelDirector* Director = GetDirector())
	{
		Director->NoteTelegraph(Now, Now + AttackWindup);
	}
}

// ============================================================================ dashes and shots

void ABIBEnemy::StartDash(const FVector& Direction, float Speed, float Seconds, bool bStrikes)
{
	DashDir = Direction.GetSafeNormal2D();
	DashSpeed = Speed;
	DashUntil = GetWorld()->GetTimeSeconds() + Seconds;
	bDashStrikes = bStrikes;
	bDashHit = false;
}

void ABIBEnemy::TickDash(float DeltaSeconds, ABIBPlayerCharacter* Target, float DealtScale)
{
	const float Now = GetWorld()->GetTimeSeconds();
	FHitResult Hit;
	AddActorWorldOffset(DashDir * DashSpeed * DeltaSeconds, true, &Hit);
	bool bEnd = Now >= DashUntil || Hit.bBlockingHit;
	if (bDashStrikes && !bDashHit && Target)
	{
		FVector To = Target->GetActorLocation() - GetActorLocation();
		if (FMath::Abs(To.Z) < 200.0)
		{
			To.Z = 0.0;
			if (To.Size() <= BodyRadius + 120.f)
			{
				bDashHit = true;
				ABIBFx::SlashStreak(GetWorld(), (GetActorLocation() + Target->GetActorLocation()) * 0.5 + FVector(0.0, 0.0, 40.0), DashDir,
					bCurrentUnblockable ? BIBFxColours::Rose : BIBFxColours::White, AttackRange + 60.f, 1);
				HitTarget(Target, nullptr, AttackDamage * DealtScale * CurrentDamageMult, true);
				bEnd = bEnd || CurrentMove != BIB::Angel::EMove::Charge;   // a charge runs on through
			}
		}
	}
	if (bEnd)
	{
		DashUntil = -1.f;
		if (bDashStrikes)
		{
			bDashStrikes = false;
			bCurrentUnblockable = false;
			FinishAttack();
		}
	}
}

void ABIBEnemy::FireShot(const FVector& To, float Damage, float SpeedMult)
{
	FBIBAngelShot Shot;
	Shot.Pos = GetActorLocation() + GetActorForwardVector() * BodyRadius + FVector(0.0, 0.0, 30.0);
	FVector Line = To - Shot.Pos;
	Line.Z = 0.0;
	const float Speed = static_cast<float>(BIB::Angel::AiTuning().ShotSpeed) * SpeedMult;
	Shot.Vel = Line.GetSafeNormal() * Speed;
	Shot.Life = (AttackRange + BIBTuning::RangedHitSlack + 400.f) / FMath::Max(1.f, Speed);
	Shot.Damage = Damage;
	Shot.bUnblockable = bCurrentUnblockable;
	Shot.Visual = ABIBFx::Spawn(GetWorld(), EBIBFxShape::Sphere, Shot.Pos, FRotator::ZeroRotator, FVector(34.f), FVector(26.f),
		bCurrentUnblockable ? BIBFxColours::Rose : BIBFxColours::Gold, Shot.Life, 14.f);
	Shots.Add(Shot);
}

void ABIBEnemy::TickShots(float DeltaSeconds, ABIBPlayerCharacter* Target)
{
	if (Shots.Num() == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float HitRadius = static_cast<float>(BIB::Angel::AiTuning().ShotLaneHalfWidth);
	for (int32 Index = Shots.Num() - 1; Index >= 0; --Index)
	{
		FBIBAngelShot& Shot = Shots[Index];
		const FVector From = Shot.Pos;
		const FVector To = From + Shot.Vel * DeltaSeconds;
		Shot.Life -= DeltaSeconds;
		bool bDone = Shot.Life <= 0.f;
		// Walls stop shots.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BIBAngelShot), false, this);
		if (Target)
		{
			Params.AddIgnoredActor(Target);
		}
		FHitResult Wall;
		if (World->LineTraceSingleByChannel(Wall, From, To, ECC_WorldStatic, Params))
		{
			bDone = true;
		}
		if (!bDone && Target && Target->IsCombatActive())
		{
			const FVector At = Target->GetActorLocation();
			if (FMath::Abs(At.Z - To.Z) < BIBEnemyBrainImpl::ShotHitHeight)
			{
				const FVector Closest = FMath::ClosestPointOnSegment(At, From, To);
				if (FVector::Dist2D(Closest, At) <= HitRadius)
				{
					const bool bWasUnblockable = bCurrentUnblockable;
					bCurrentUnblockable = Shot.bUnblockable;
					const bool bShotUnblockable = Shot.bUnblockable;
					HitTarget(Target, nullptr, Shot.Damage, false);
					bCurrentUnblockable = bWasUnblockable;
					// The hit can end this angel (the commander's counter / parry): dying clears Shots, so stop here (Shot dangles).
					if (!IsAlive() || !Shots.IsValidIndex(Index))
					{
						return;
					}
					ABIBFx::HitSpark(World, At + FVector(0.0, 0.0, 40.0), bShotUnblockable ? BIBFxColours::Rose : BIBFxColours::Gold, 1.f);
					bDone = true;
				}
			}
		}
		FBIBAngelShot& Live = Shots[Index];   // re-read: the hit above may have changed the array
		Live.Pos = To;
		if (AActor* Visual = Live.Visual.Get())
		{
			if (bDone)
			{
				Visual->Destroy();
			}
			else
			{
				Visual->SetActorLocation(To);
			}
		}
		if (bDone)
		{
			Shots.RemoveAtSwap(Index);
		}
	}
}

// ============================================================================ illusion (fighting another angel)

void ABIBEnemy::TickIllusion(float DeltaSeconds, ABIBEnemy* Victim, float Distance, const FVector& Direction, float MoveMult, float DealtScale)
{
	if (AttackWindup > 0.f)
	{
		AttackWindup -= DeltaSeconds;
		if (AttackWindup <= 0.f)
		{
			AttackWindup = -1.f;
			ResolveAttack(nullptr, Victim, Distance, DealtScale);
		}
		return;
	}
	if (GetWorld()->GetTimeSeconds() < RecoverUntil)
	{
		return;
	}
	if (Distance > AttackRange)
	{
		AddMovementInput(Direction, MoveMult);
	}
	else if (AttackCooldown <= 0.f)
	{
		CurrentMove = BIB::Angel::EMove::Strike;
		CurrentDamageMult = 1.f;
		bCurrentUnblockable = false;
		PendingFollowUps = 0;
		AttackWindup = AttackWindupS;
		AttackCooldown = AttackIntervalS;
		ABIBFx::Telegraph(GetWorld(), GetActorLocation() + FVector(0.0, 0.0, 60.0), false);
	}
}
