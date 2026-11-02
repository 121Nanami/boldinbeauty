// What the angels can perceive of the commander (owner request 2026-10-13, goal 2: smart angels that do not cheat). A read-only
// snapshot of what anyone watching the fight could see this frame - never a key press, never anything that has not started.
// The angels read it through UBIBAngelDirector's timeline with their reaction delay (BIBRules/BIBAngelBrain.h).
#include "BIBGame/BIBPlayerCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

BIB::Angel::FPlayerView ABIBPlayerCharacter::GetAngelView() const
{
	BIB::Angel::FPlayerView View;
	const float T = Now();
	View.Time = T;
	const FVector Location = GetActorLocation();
	const FVector Velocity = GetVelocity();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	View.Pos = {Location.X, Location.Y};
	View.Vel = {Velocity.X, Velocity.Y};
	View.Facing = Forward.IsNearlyZero() ? BIB::Angel::FV2{1.0, 0.0} : BIB::Angel::FV2{Forward.X, Forward.Y};
	const FBIBMember& Member = Members[ActiveIndex];
	View.HealthFraction = Member.MaxHP > 0.f ? FMath::Clamp(Member.HP / Member.MaxHP, 0.f, 1.f) : 0.f;
	View.Operator = ActiveIndex;
	View.SecondsSinceSwitch = SecondsSinceSwitch();
	View.bBlocking = bBlocking;
	View.bAirborne = IsAirborneForAngels();
	View.bDodging = Action == EBIBAction::Dodge || T < InvulnerableUntil;

	// Long casts and charges: the wind-ups everyone can see coming (and punish).
	const bool bLongAction = Action == EBIBAction::LunaCutscene || Action == EBIBAction::LunaExpel || Action == EBIBAction::JudgeCourtWindup
		|| Action == EBIBAction::NarratorSwitch || Action == EBIBAction::LunaEventHorizon;
	View.bLongCast = bUltCinematic || bLongAction;

	// The attack being swung (the procedural pose is the body everyone sees: wind-up -> impact -> recovery).
	const float Settle = 0.05f;
	const bool bPose = PoseTime >= 0.f;
	const bool bWindingUp = bPose && PoseTime <= PoseImpactAt + Settle;
	const bool bPoseRecovery = bPose && !bWindingUp && PoseTime < PoseImpactAt + PoseRecovery;
	View.bAttacking = bWindingUp || View.bLongCast;
	View.bHeavyWindup = (bWindingUp && PoseStrength >= 1.05f) || View.bLongCast;
	View.bRecovery = bPoseRecovery || IsActionInRecovery();
	if (bPose)
	{
		View.ActionStartedAt = T - PoseTime;
	}
	else if (Action != EBIBAction::None)
	{
		View.ActionStartedAt = T - ActionTime;
	}
	else
	{
		View.ActionStartedAt = LastAttackAt;
	}
	// A whiff: recovering from an attack that has not hit anything since it started. Damage-over-time ticks do not count (they
	// land all the time on a long fight's boss and its escorts, which hid every whiff from the angels).
	View.bWhiffed = View.bRecovery && LastStrikeHitAt < View.ActionStartedAt - 0.01f;
	// How far the current attack reaches (big swings and casts reach further).
	View.ThreatReach = View.bLongCast ? 650.0 : (View.bHeavyWindup ? 450.0 : 320.0);
	return View;
}
