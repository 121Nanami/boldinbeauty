// Narrator / Diano (6*) — Narrator:Diamon.md, mapped for the demo as:
//   Narrator: Ability 1 Explosion -> Move 1, Ability 2 Global Exchange -> Move 2, Ability 3 I AM STILL ALIVE -> Move 3,
//             Ability 4 Close enough -> Special, Ultimate Switch (becomes Diano).
//   Diano:    Ability 1 Acceleration -> Move 1, Ability 2 Endless god -> Move 2, Ability 3 Embodiment of imperfection -> Move 3,
//             Ability 4 The TRUE STRONGEST -> Special (not in demo), Ultimate THE SOUL ONE (needs Ult^2, not in demo).
// Not in the demo: alternates/specials (A past life, A life of serenity, Mastery, Burning-out, Brain-damage,
// Haemorrhage, Supression, Alter ego, The three souls, Last stand), pills, swim/climb/teleport speed perks, speed shield.
#include "BIBGame/BIBPlayerCharacter.h"

#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBTuning.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BIBGame/BIBInput.h"
#include "BIBGame/BIBMotionEvents.h"

namespace BIBNarratorKit
{
	float GetExchangeValue(const FBIBNarratorState& State, int32 Stat)
	{
		switch (static_cast<EBIBStat>(Stat))
		{
		case EBIBStat::Damage: return State.DamageX;
		case EBIBStat::Health: return State.HealthX;
		case EBIBStat::Speed: return State.Speed / BIBTuning::SpeedPerX;
		case EBIBStat::Ultimate: return State.UltX;
		case EBIBStat::Cooldown: return State.CooldownX;
		default: return 0.f;
		}
	}

	void SetExchangeValue(FBIBNarratorState& State, int32 Stat, float Value)
	{
		switch (static_cast<EBIBStat>(Stat))
		{
		case EBIBStat::Damage: State.DamageX = Value; break;
		case EBIBStat::Health: State.HealthX = Value; break;
		case EBIBStat::Speed: State.Speed = Value * BIBTuning::SpeedPerX; break;
		case EBIBStat::Ultimate: State.UltX = Value; break;
		case EBIBStat::Cooldown: State.CooldownX = Value; break;
		default: break;
		}
	}
}

FString ABIBPlayerCharacter::StatName(int32 StatIndex)
{
	switch (static_cast<EBIBStat>(StatIndex))
	{
	case EBIBStat::Damage: return TEXT("Damage");
	case EBIBStat::Health: return TEXT("Health");
	case EBIBStat::Speed: return TEXT("Speed");
	case EBIBStat::Ultimate: return TEXT("Ultimate");
	case EBIBStat::Cooldown: return TEXT("Cooldown");
	default: return TEXT("?");
	}
}

float ABIBPlayerCharacter::NarratorForcedSpeed() const
{
	const float BaseSpeed = NarratorData.bDiano ? BIBTuning::DianoStartSpeed : BIBTuning::NarratorStartSpeed;
	return FMath::Min(BIBTuning::SpeedMoveCap, FMath::Max(0.f, NarratorData.Speed - BaseSpeed) * BIBTuning::SpeedToCmPerSecond);
}

void ABIBPlayerCharacter::DebugSetNarratorSpeed(float Speed)
{
	NarratorData.Speed = FMath::Max(0.f, Speed);
	Message(FString::Printf(TEXT("Debug: Narrator speed set to %d."), FMath::RoundToInt(NarratorData.Speed)));
}

float ABIBPlayerCharacter::NarratorAttack() const
{
	return FMath::Max(0.f, Active().BaseAttack + NarratorData.BonusAttack);
}

float ABIBPlayerCharacter::NarratorCooldown(float Seconds) const
{
	return Seconds / FMath::Max(0.1f, NarratorData.CooldownX);
}

void ABIBPlayerCharacter::NarratorRecalcHealth()
{
	const int32 Index = FindMember(EBIBKitType::Narrator);
	if (Index == INDEX_NONE)
	{
		return;
	}
	const FBIBMember& Member = Members[Index];
	const float Fraction = Member.MaxHP > 0.f ? Member.HP / Member.MaxHP : 1.f;
	const float NewMax = FMath::Max(1.f, Member.BaseMaxHP * NarratorData.HealthX + NarratorData.BonusHP);
	SetMemberMaxHP(Index, NewMax);
	if (Member.bAlive)
	{
		SetMemberHP(Index, FMath::Clamp(Fraction * NewMax, 1.f, NewMax));
	}
}

void ABIBPlayerCharacter::ExchangeSelect(bool bFrom, int32 StatIndex)
{
	const int32 Clamped = FMath::Clamp(StatIndex, 0, static_cast<int32>(EBIBStat::Count) - 1);
	if (bFrom)
	{
		NarratorData.ExchangeFrom = Clamped;
	}
	else
	{
		NarratorData.ExchangeTo = Clamped;
	}
}

void ABIBPlayerCharacter::ExchangeApply()
{
	FBIBNarratorState& State = NarratorData;
	if (State.ExchangeFrom == State.ExchangeTo)
	{
		Message(TEXT("Global Exchange: pick two different stats."));
		return;
	}
	// Demo simplification: each click moves 10% of the source stat to the target; a stat can drop to 10% at most.
	const float FromValue = BIBNarratorKit::GetExchangeValue(State, State.ExchangeFrom);
	const float Amount = FromValue * BIBTuning::ExchangeStep;
	if (FromValue - Amount < BIBTuning::ExchangeFloor)
	{
		Message(TEXT("Global Exchange: that stat cannot go any lower."));
		return;
	}
	BIBNarratorKit::SetExchangeValue(State, State.ExchangeFrom, FromValue - Amount);
	BIBNarratorKit::SetExchangeValue(State, State.ExchangeTo, BIBNarratorKit::GetExchangeValue(State, State.ExchangeTo) + Amount);
	NarratorRecalcHealth();
	Message(FString::Printf(TEXT("Exchanged %.2fx %s -> %s."), Amount, *StatName(State.ExchangeFrom), *StatName(State.ExchangeTo)));
}

void ABIBPlayerCharacter::CloseExchange()
{
	NarratorData.bExchangeOpen = false;
}

void ABIBPlayerCharacter::NarratorStartAcceleration()
{
	FaceAim();
	StartAction(EBIBAction::DianoAcceleration);
	NarratorData.bAccelHit = false;
	LaunchCharacter(ActionDirection * BIBTuning::AccelerationDashSpeed, true, false);
}

void ABIBPlayerCharacter::NarratorEndDiano(const FString& Reason)
{
	FBIBNarratorState& State = NarratorData;
	if (State.bDiano && Reason != TEXT("exploded"))
	{
		BIBMotionEvents::Shared(TEXT("life-of-serenity"), this, false, TEXT("narr-alt"));   // the form ends calmly
	}
	State.bDiano = false;
	State.bDianoPermanent = false;
	State.bAccelChainPending = false;
	State.Speed = State.SavedNarratorSpeed; // WORKING: Narrator resumes the speed she had
	Message(FString::Printf(TEXT("Diano fades (%s). Narrator is back."), *Reason));
}

void ABIBPlayerCharacter::NarratorInput()
{
	FBIBNarratorState& State = NarratorData;
	if (State.bDiano)
	{
		if (State.bAccelChainPending && Pressed(EBIBInput::Move1) && Action == EBIBAction::None)
		{
			State.bAccelChainPending = false;
			State.bAccelMimic = false;
			NarratorStartAcceleration();
			return;
		}
	}
	else if (Pressed(EBIBInput::Move2))
	{
		State.bExchangeOpen = !State.bExchangeOpen; // Global Exchange is a panel, not a cast
		return;
	}
	if (Action != EBIBAction::None)
	{
		return;
	}
	static const EBIBSlot Slots[] = {EBIBSlot::Move1, EBIBSlot::Move2, EBIBSlot::Move3, EBIBSlot::Special, EBIBSlot::Ultimate};
	for (const EBIBSlot Slot : Slots)
	{
		if (Pressed(InputForSlot(Slot)))
		{
			TryActivateSlot(Slot); // the GameplayAbility calls NarratorActivate
			return;
		}
	}
}

bool ABIBPlayerCharacter::NarratorActivate(EBIBSlot Slot)
{
	FBIBNarratorState& State = NarratorData;
	FBIBMember& Member = Active();
	const float T = Now();

	if (State.bDiano)
	{
		switch (Slot)
		{
		case EBIBSlot::Move1:
			if (!IsReady(EBIBSlot::Move1))
			{
				Message(FString::Printf(TEXT("Acceleration not ready (%.1fs)."), Remaining(EBIBSlot::Move1)));
				return false;
			}
			State.bAccelMimic = false;
			NarratorStartAcceleration();
			return true;
		case EBIBSlot::Move2:
			if (Member.HP <= BIBTuning::EndlessGodHealth || NarratorAttack() <= BIBTuning::EndlessGodDamage)
			{
				Message(TEXT("Endless god needs more than 80 health and 20 damage."));
				return false;
			}
			StartAction(EBIBAction::DianoEndlessGod);
			State.EndlessNextTickAt = T + BIBTuning::EndlessGodTick;
			return true;
		case EBIBSlot::Move3:
			if (!IsReady(EBIBSlot::Move3))
			{
				Message(FString::Printf(TEXT("Embodiment of imperfection not ready (%.1fs)."), Remaining(EBIBSlot::Move3)));
				return false;
			}
			if (State.Speed < BIBTuning::ImperfectionBurn)
			{
				Message(TEXT("Embodiment of imperfection burns 999 speed - not enough speed."));
				return false;
			}
			State.Speed -= BIBTuning::ImperfectionBurn;
			StartCooldown(EBIBSlot::Move3, NarratorCooldown(BIBTuning::ImperfectionCooldown));
			Message(TEXT("Embodiment of imperfection: burned 999 speed."));
			return true;
		case EBIBSlot::Special:
			Message(TEXT("The TRUE STRONGEST summons secret mini-bosses - not included in this demo."));
			return false;
		case EBIBSlot::Ultimate:
			Message(TEXT("THE SOUL ONE needs Ult^2 - not included in this demo."));
			return false;
		default:
			return false;
		}
	}

	switch (Slot)
	{
	case EBIBSlot::Move1:
		if (!IsReady(EBIBSlot::Move1))
		{
			return false;
		}
		if (State.Speed < BIBTuning::ExplosionSpeedCost)
		{
			Message(TEXT("Explosion needs 100 speed."));
			return false;
		}
		State.Speed -= BIBTuning::ExplosionSpeedCost;
		StartPose(0.f, 0.3f, 1.2f);
		{
			const FBIBScopedHit Scope(this, BIB::Feel::ESource::Move, true);
			Explode(GetActorLocation(), BIBTuning::ExplosionRadius, BIBTuning::ExplosionDamage, 700.f, FColor::Orange);
		}
		StartCooldown(EBIBSlot::Move1, NarratorCooldown(BIBTuning::ExplosionCooldown));
		return true;
	case EBIBSlot::Move3:
	{
		if (!IsReady(EBIBSlot::Move3))
		{
			Message(FString::Printf(TEXT("I AM STILL ALIVE not ready (%.1fs)."), Remaining(EBIBSlot::Move3)));
			return false;
		}
		if (Member.Ult < 50.f)
		{
			State.ParryUntil = T + BIBTuning::StillAliveParry;
			StartCooldown(EBIBSlot::Move3, NarratorCooldown(BIBTuning::StillAliveCooldown));
			return true;
		}
		// Over 50% ult: randomly mimic one of Diano's abilities 1-4.
		const int32 Roll = FMath::RandRange(1, 4);
		float Cooldown = BIBTuning::StillAliveCooldown;
		switch (Roll)
		{
		case 1:
			State.bAccelMimic = true;
			NarratorStartAcceleration();
			Message(TEXT("I AM STILL ALIVE mimics Acceleration."));
			break;
		case 2:
			State.BonusAttack += 100.f;
			State.BonusHP += 400.f;
			NarratorRecalcHealth();
			SetMemberHP(ActiveIndex, Member.HP + 400.f);
			Message(TEXT("I AM STILL ALIVE mimics Endless god: +100 damage, +400 health."));
			break;
		case 3:
			State.Speed += 99.f;
			Message(TEXT("I AM STILL ALIVE mimics Embodiment: +99 speed."));
			break;
		default:
			Cooldown = BIBTuning::StillAliveLongCooldown;
			Message(TEXT("I AM STILL ALIVE mimics The TRUE STRONGEST: 45 s cooldown."));
			break;
		}
		StartCooldown(EBIBSlot::Move3, NarratorCooldown(Cooldown));
		return true;
	}
	case EBIBSlot::Special:
		if (State.bCloseEnoughUsed)
		{
			Message(TEXT("Close enough: once per fight."));
			return false;
		}
		if (State.CloseEnoughAt >= 0.f)
		{
			return false;
		}
		State.CloseEnoughAt = T + BIBTuning::CloseEnoughWindup;
		StartPose(BIBTuning::CloseEnoughWindup, 0.3f, 1.f);   // a long, readable wind-up
		Message(TEXT("Close enough: 3 s windup - don't get hit."));
		return true;
	case EBIBSlot::Ultimate:
		if (Member.Ult < 100.f)
		{
			Message(FString::Printf(TEXT("Switch needs 100%% ult (%d%%)."), FMath::FloorToInt(Member.Ult)));
			return false;
		}
		SetMemberUlt(ActiveIndex, 0.f);
		State.bExchangeOpen = false;
		StartAction(EBIBAction::NarratorSwitch);
		GrantInvulnerable(BIBTuning::SwitchTransform);
		return true;
	default:
		return false;
	}
}

void ABIBPlayerCharacter::NarratorUpdateAction(float DeltaSeconds)
{
	FBIBNarratorState& State = NarratorData;
	switch (Action)
	{
	case EBIBAction::NarratorSwitch:
		if (ActionTime >= BIBTuning::SwitchTransform)
		{
			State.bDiano = true;
			State.SavedNarratorSpeed = State.Speed;
			State.Speed = BIBTuning::DianoStartSpeed;
			State.DianoUntil = Now() + BIBTuning::SwitchDuration;
			State.DianoBlockUntil = Now() + BIBTuning::SwitchBlockWindow;
			EndAction();
			Message(TEXT("Switch: Diano takes over for 25 s (blocks the next hit for 3 s)."));
		}
		break;

	case EBIBAction::DianoAcceleration:
	{
		const TArray<ABIBEnemy*> Touched = EnemiesInRadius(GetActorLocation(), 90.f);
		if (!State.bAccelHit && Touched.Num() > 0)
		{
			ABIBEnemy* Enemy = Touched[0];
			GetCharacterMovement()->StopMovementImmediately();
			EndAction();
			if (Now() - Enemy->LastAccelerationHitAt < BIBTuning::AccelerationRepeatLock)
			{
				// Same enemy within 8 s counts as a miss.
				if (!State.bAccelMimic)
				{
					StartCooldown(EBIBSlot::Move1, NarratorCooldown(BIBTuning::AccelerationCooldown));
				}
				Message(TEXT("Acceleration: that enemy was hit in the last 8 s - cooldown."));
				return;
			}
			State.bAccelHit = true;
			Enemy->LastAccelerationHitAt = Now();
			StartPose(0.f, 0.2f, 1.f);
			{
				const FBIBScopedHit Scope(this, BIB::Feel::ESource::Move, true);
				DealDamage(Enemy, BIBTuning::AccelerationDamage);
			}
			State.Speed += BIBTuning::AccelerationSpeedGain;
			if (!State.bAccelMimic)
			{
				// Refresh: press again within 0.5 s to chain, otherwise 10 s cooldown.
				StartCooldown(EBIBSlot::Move1, 0.f);
				State.AccelChainUntil = Now() + BIBTuning::AccelerationChain;
				State.bAccelChainPending = true;
			}
			return;
		}
		if (ActionTime >= BIBTuning::AccelerationWindow)
		{
			if (!State.bAccelHit && !State.bAccelMimic)
			{
				StartCooldown(EBIBSlot::Move1, NarratorCooldown(BIBTuning::AccelerationCooldown));
			}
			EndAction();
		}
		break;
	}

	case EBIBAction::DianoEndlessGod:
	{
		if (!Held(EBIBInput::Move2))
		{
			EndAction();
			return;
		}
		FBIBMember& Member = Active();
		while (Now() >= State.EndlessNextTickAt)
		{
			State.EndlessNextTickAt += BIBTuning::EndlessGodTick;
			if (Member.HP <= BIBTuning::EndlessGodHealth || NarratorAttack() <= BIBTuning::EndlessGodDamage)
			{
				EndAction();
				Message(TEXT("Endless god: nothing left to give."));
				return;
			}
			SetMemberHP(ActiveIndex, Member.HP - BIBTuning::EndlessGodHealth);
			State.BonusAttack -= BIBTuning::EndlessGodDamage;
			State.EndlessHealthGiven += BIBTuning::EndlessGodHealth;
			State.EndlessDamageGiven += BIBTuning::EndlessGodDamage;
			if (!State.bDianoPermanent)
			{
				State.DianoUntil += BIBTuning::EndlessGodSeconds;
				if (State.EndlessHealthGiven >= BIBTuning::EndlessGodPermanentHealth && State.EndlessDamageGiven >= BIBTuning::EndlessGodPermanentDamage)
				{
					State.bDianoPermanent = true;
					const float Doubled = Member.HP * 2.f;
					SetMemberMaxHP(ActiveIndex, FMath::Max(Member.MaxHP, Doubled));
					SetMemberHP(ActiveIndex, Doubled);
					State.BonusAttack += NarratorAttack();
					Message(TEXT("Endless god: Diano stays for the rest of the fight - health and damage doubled."));
				}
			}
		}
		break;
	}

	default:
		EndAction();
		break;
	}
}

void ABIBPlayerCharacter::NarratorTick(float DeltaSeconds)
{
	FBIBNarratorState& State = NarratorData;
	const int32 Index = FindMember(EBIBKitType::Narrator);
	if (Index == INDEX_NONE)
	{
		return;
	}
	const float T = Now();
	if (State.bAccelChainPending && T > State.AccelChainUntil)
	{
		State.bAccelChainPending = false;
		StartCooldown(Index, EBIBSlot::Move1, NarratorCooldown(BIBTuning::AccelerationCooldown));
	}
	if (!bCombatActive || ActiveIndex != Index || !Members[Index].bAlive)
	{
		return;
	}
	FBIBMember& Member = Members[Index];

	// Speed: +25 (Diano +125) for every second without being hit.
	State.SecondTimer += DeltaSeconds;
	while (State.SecondTimer >= 1.f)
	{
		State.SecondTimer -= 1.f;
		if (!State.bHitThisSecond)
		{
			State.Speed += State.bDiano ? BIBTuning::DianoSpeedGain : BIBTuning::NarratorSpeedGain;
		}
		State.bHitThisSecond = false;
	}

	if (State.bDiano && !State.bDianoPermanent && T >= State.DianoUntil)
	{
		NarratorEndDiano(TEXT("time up"));
	}

	// Diano: Embodiment of imperfection force-activates at 4999 speed if health and attack allow it.
	if (State.bDiano && State.Speed >= BIBTuning::ImperfectionAutoAt && NarratorAttack() > BIBTuning::ImperfectionAutoAttack
		&& Member.HP > BIBTuning::ImperfectionAutoHealth)
	{
		State.BonusAttack -= BIBTuning::ImperfectionAutoAttack;
		SetMemberHP(Index, Member.HP - BIBTuning::ImperfectionAutoHealth);
		State.Speed -= BIBTuning::ImperfectionAutoSpeed;
		Message(TEXT("Embodiment of imperfection auto-fired: -19 attack, -77 health, -125 speed."));
	}

	// Too much speed: explode, damaging everything nearby and killing the operator.
	const float Limit = State.bDiano ? BIBTuning::DianoExplodeSpeed : BIBTuning::NarratorExplodeSpeed;
	if (State.Speed >= Limit)
	{
		const float Damage = State.bDiano ? BIBTuning::DianoExplodeDamage : BIBTuning::NarratorExplodeDamage;
		Message(FString::Printf(TEXT("%s reached %d speed and exploded!"), State.bDiano ? TEXT("Diano") : TEXT("Narrator"), FMath::RoundToInt(Limit)));
		{
			const FBIBScopedHit Scope(this, BIB::Feel::ESource::Unknown, true, static_cast<int32>(BIB::Feel::EWeight::Finisher));
			Explode(GetActorLocation(), BIBTuning::SelfExplosionRadius, Damage, 1500.f, FColor::Red, Index);
		}
		BIBMotionEvents::Shared(TEXT("brain-damage-haemorrhage"), this, false, TEXT("narr-alt"));
		if (State.bDiano)
		{
			NarratorEndDiano(TEXT("exploded"));
		}
		State.Speed = BIBTuning::NarratorStartSpeed;
		TakeSelfDamage(Member.HP + 1.f);
		return;
	}

	// Close enough: after the 3 s windup kill everything under 20% health (bosses must be under 5%).
	if (State.CloseEnoughAt > 0.f && T >= State.CloseEnoughAt)
	{
		State.CloseEnoughAt = -1.f;
		State.bCloseEnoughUsed = true;
		int32 Killed = 0;
		const FBIBScopedHit Scope(this, BIB::Feel::ESource::Special, true);
		StartPose(0.f, 0.35f, 1.3f);
		for (ABIBEnemy* Enemy : EnemiesInRadius(GetActorLocation(), 100000.f))
		{
			const float Threshold = Enemy->IsBoss() ? BIBTuning::CloseEnoughBossThreshold : BIBTuning::CloseEnoughThreshold;
			if (Enemy->GetHealthFraction() < Threshold)
			{
				KillEnemy(Enemy, false);
				++Killed;
			}
		}
		Message(FString::Printf(TEXT("Close enough: %d enemies finished."), Killed));
	}

	// Running into an enemy head-on deals 1% of max health per 10 speed (Diano: per 50 speed). WORKING reading.
	if (GetVelocity().Size2D() > 300.0 && Action == EBIBAction::None)
	{
		for (ABIBEnemy* Enemy : EnemiesInRadius(GetActorLocation(), 70.f))
		{
			if (T - Enemy->LastBodyCheckAt >= BIBTuning::BodyCheckCooldown)
			{
				Enemy->LastBodyCheckAt = T;
				const float PerSpeed = State.bDiano ? 50.f : 10.f;
				DealDamage(Enemy, Member.MaxHP * 0.01f * (State.Speed / PerSpeed), Index);
			}
		}
	}
}

FString ABIBPlayerCharacter::NarratorKitLine() const
{
	const FBIBNarratorState& State = NarratorData;
	FString Line = FString::Printf(TEXT("%s speed %d / explodes at %d | DMG %.2fx HP %.2fx SPD %.2fx ULT %.2fx CD %.2fx"),
		State.bDiano ? TEXT("DIANO") : TEXT("Narrator"), FMath::RoundToInt(State.Speed),
		State.bDiano ? FMath::RoundToInt(BIBTuning::DianoExplodeSpeed) : FMath::RoundToInt(BIBTuning::NarratorExplodeSpeed),
		State.DamageX, State.HealthX, State.Speed / BIBTuning::SpeedPerX, State.UltX, State.CooldownX);
	if (State.bDiano)
	{
		Line += State.bDianoPermanent ? FString(TEXT(" | Diano permanent")) : FString::Printf(TEXT(" | Diano %.0fs"), State.DianoUntil - Now());
		if (Now() < State.DianoBlockUntil)
		{
			Line += TEXT(" | BLOCKING");
		}
	}
	return Line;
}
