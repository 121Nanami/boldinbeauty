// Judge (5*) — Judge.md, mapped for the demo as: Ability 1 Paperwork -> Move 1, Ability 2 Court setting -> Move 2,
// Ability 3 Gavel smash -> Move 3, Ability 4 Retribution -> Special, Ultimate Justice.
// Not in the demo: alternates (Forgiveness, Genecide), Special Ultimate Sentencing and Special Ignite.
#include "BIBGame/BIBPlayerCharacter.h"

#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBTuning.h"

#include "Components/CapsuleComponent.h"
#include "BIBGame/BIBFx.h"
#include "Engine/World.h"
#include "BIBGame/BIBInput.h"

namespace BIBJudgeKit
{
	void DrawGroundCircle(UWorld* World, const FVector& Centre, float Radius, const FColor& Colour, float Seconds)
	{
		// Judge's court zones as glowing ground rings (were debug circles).
		ABIBFx::Spawn(World, EBIBFxShape::Ring, Centre + FVector(0.0, 0.0, 4.0), FRotator::ZeroRotator, FVector(Radius * 2.f, Radius * 2.f, 3.f),
			FVector(Radius * 2.f, Radius * 2.f, 3.f), FLinearColor(Colour), FMath::Max(0.1f, Seconds), 3.f);
	}
}

FString ABIBPlayerCharacter::JudgeModeName(EBIBJudgeMode InMode)
{
	switch (InMode)
	{
	case EBIBJudgeMode::Witness: return TEXT("WITNESS");
	case EBIBJudgeMode::Lawyer: return TEXT("LAWYER");
	case EBIBJudgeMode::Judge: return TEXT("JUDGE");
	}
	return TEXT("?");
}

bool ABIBPlayerCharacter::JudgeAnyInputPressed() const
{
	// "Literally anything": any action press counts, including starting to move.
	for (int32 Index = 0; Index < static_cast<int32>(EBIBInput::Count); ++Index)
	{
		const EBIBInput Input = static_cast<EBIBInput>(Index);
		if (Input != EBIBInput::Talk && Input != EBIBInput::Pause && Pressed(Input))
		{
			return true;
		}
	}
	return false;
}

void ABIBPlayerCharacter::JudgeInput()
{
	const float T = Now();
	if (JudgeData.bMenuOpen)
	{
		int32 Choice = INDEX_NONE;
		if (Pressed(EBIBInput::Move1))
		{
			Choice = 0;
		}
		else if (Pressed(EBIBInput::Move2))
		{
			Choice = 1;
		}
		else if (Pressed(EBIBInput::Move3))
		{
			Choice = 2;
		}
		if (Choice != INDEX_NONE)
		{
			JudgeData.bMenuOpen = false;
			JudgeData.PendingMode = static_cast<EBIBJudgeMode>(Choice);
			StartAction(EBIBAction::JudgeCourtWindup);
			StartCooldown(EBIBSlot::Move2, BIBTuning::CourtCooldown);
		}
		else if (T - JudgeData.MenuOpenedAt > 4.f)
		{
			JudgeData.bMenuOpen = false;
			Message(TEXT("Court setting menu closed."));
		}
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
			TryActivateSlot(Slot); // the GameplayAbility calls JudgeActivate
			return;
		}
	}
}

bool ABIBPlayerCharacter::JudgeActivate(EBIBSlot Slot)
{
	const float T = Now();
	FBIBMember& Member = Active();
	const bool bJudgeMode = JudgeData.Mode == EBIBJudgeMode::Judge;
	switch (Slot)
	{
	case EBIBSlot::Move1:
		if (!IsReady(EBIBSlot::Move1))
		{
			Message(FString::Printf(TEXT("Paperwork not ready (%.1fs)."), Remaining(EBIBSlot::Move1)));
			return false;
		}
		if (JudgeData.Mode == EBIBJudgeMode::Lawyer)
		{
			Message(TEXT("Lawyer mode: Paperwork is passive - avoid damage and suspects turn guilty."));
			return false;
		}
		StartAction(EBIBAction::JudgePaperwork);
		StartCooldown(EBIBSlot::Move1, bJudgeMode ? BIBTuning::PaperworkJudgeCooldown : BIBTuning::PaperworkWitnessCooldown);
		StartPose(bJudgeMode ? BIBTuning::PaperworkJudgeWindup : BIBTuning::PaperworkWitnessWindup, 0.3f, 0.8f);
		return true;
	case EBIBSlot::Move2:
		if (T < JudgeData.CourtLockedUntil)
		{
			Message(FString::Printf(TEXT("Court setting locked (%.0fs)."), JudgeData.CourtLockedUntil - T));
			return false;
		}
		if (!IsReady(EBIBSlot::Move2))
		{
			Message(FString::Printf(TEXT("Court setting not ready (%.1fs)."), Remaining(EBIBSlot::Move2)));
			return false;
		}
		JudgeData.bMenuOpen = true;
		JudgeData.MenuOpenedAt = T;
		return true;
	case EBIBSlot::Move3:
		if (!bJudgeMode)
		{
			Message(TEXT("Gavel smash: Judge mode only."));
			return false;
		}
		if (!IsReady(EBIBSlot::Move3))
		{
			Message(FString::Printf(TEXT("Gavel smash not ready (%.1fs)."), Remaining(EBIBSlot::Move3)));
			return false;
		}
		StartAction(EBIBAction::JudgeGavel);
		StartCooldown(EBIBSlot::Move3, BIBTuning::GavelCooldown);
		StartPose(BIBTuning::GavelWindup, BIBTuning::GavelSpacing * 2.f + BIBTuning::GavelForeSwing, 1.3f);
		return true;
	case EBIBSlot::Special:
		if (!bJudgeMode)
		{
			Message(TEXT("Retribution: Judge mode only."));
			return false;
		}
		if (!IsReady(EBIBSlot::Special))
		{
			Message(FString::Printf(TEXT("Retribution not ready (%.1fs)."), Remaining(EBIBSlot::Special)));
			return false;
		}
		StartAction(EBIBAction::JudgeRetribution);
		StartCooldown(EBIBSlot::Special, BIBTuning::RetributionCooldown);
		StartPose(BIBTuning::RetributionWindup, BIBTuning::RetributionDuration + BIBTuning::RetributionForeSwing, 1.4f);
		return true;
	case EBIBSlot::Ultimate:
		if (!bJudgeMode)
		{
			Message(TEXT("Justice: Judge mode only."));
			return false;
		}
		if (Member.Ult < 100.f)
		{
			Message(FString::Printf(TEXT("Justice needs 100%% ult (%d%%)."), FMath::FloorToInt(Member.Ult)));
			return false;
		}
		SetMemberUlt(ActiveIndex, 0.f);
		StartAction(EBIBAction::JudgeJustice);
		JudgeData.JusticeRadius = BIBTuning::JusticeStartRadius;
		JudgeData.JusticeNextGrowAt = T + 1.f;
		Message(TEXT("Justice: counter stance."));
		return true;
	default:
		return false;
	}
}

void ABIBPlayerCharacter::JudgeUpdateAction(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0);
	switch (Action)
	{
	case EBIBAction::JudgePaperwork:
	{
		const bool bWitness = JudgeData.Mode == EBIBJudgeMode::Witness;
		const float Windup = bWitness ? BIBTuning::PaperworkWitnessWindup : BIBTuning::PaperworkJudgeWindup;
		if (ActionTime >= Windup)
		{
			if (bWitness)
			{
				JudgeData.RestrainingOrders += BIBTuning::PaperworkOrders;
				Message(FString::Printf(TEXT("Paperwork: +3 restraining orders (%d)."), JudgeData.RestrainingOrders));
			}
			else
			{
				int32 Stunned = 0;
				for (ABIBEnemy* Enemy : EnemiesInRadius(GetActorLocation(), 100000.f))
				{
					if (Enemy->JudgeState == EBIBJudgeState::Guilty)
					{
						Enemy->Stun(BIBTuning::PaperworkStun);
						Enemy->ReceiveImpact(this, BIB::Feel::EWeight::Medium);   // the order lands like a blow
						++Stunned;
					}
				}
				Message(FString::Printf(TEXT("Paperwork: %d guilty enemies stunned for 3 s."), Stunned));
			}
			EndAction();
		}
		break;
	}

	case EBIBAction::JudgeCourtWindup:
	{
		const bool bToJudge = JudgeData.PendingMode == EBIBJudgeMode::Judge;
		// ActionTime is 0 on the frame the windup starts; that frame's press is the menu choice itself, not contempt.
		if (bToJudge && ActionTime > 0.f && JudgeAnyInputPressed())
		{
			// "If you try to do literally anything at all while the 2.5 second windup ... disable the move for
			//  30 seconds and force change to witness; after 30 seconds you will force become judge."
			JudgeData.Mode = EBIBJudgeMode::Witness;
			JudgeData.CourtLockedUntil = Now() + BIBTuning::CourtPenaltyLock;
			JudgeData.bForceJudgeAfterLock = true;
			EndAction();
			Message(TEXT("Contempt of court! Court setting locked for 30 s, forced to Witness."));
			return;
		}
		if (ActionTime >= (bToJudge ? BIBTuning::CourtJudgeWindup : BIBTuning::CourtWindup))
		{
			JudgeData.Mode = JudgeData.PendingMode;
			EndAction();
			Message(FString::Printf(TEXT("Court setting: %s mode."), *JudgeModeName(JudgeData.Mode)));
		}
		break;
	}

	case EBIBAction::JudgeGavel:
		// 0.5 s windup, three smashes across one second, 0.75 s fore swing.
		while (JudgeData.GavelSmashesDone < 3 && ActionTime >= BIBTuning::GavelWindup + BIBTuning::GavelSpacing * JudgeData.GavelSmashesDone)
		{
			++JudgeData.GavelSmashesDone;
			BIBJudgeKit::DrawGroundCircle(World, Feet, BIBTuning::GavelRadius, FColor::Yellow, 0.3f);
			// Each smash is felt (medium, the third heavy: hit stop, reaction, camera kick); a sentence is a heavy kill.
			const bool bLastSmash = JudgeData.GavelSmashesDone >= 3;
			const BIB::Feel::EWeight SmashWeight = bLastSmash ? BIB::Feel::EWeight::Heavy : BIB::Feel::EWeight::Medium;
			PulseAge = 0.f;
			PulseLean = static_cast<float>(BIB::Feel::FeelFor(SmashWeight).ImpactLean);
			ABIBFx::ShockRing(World, Feet, BIBTuning::GavelRadius, FLinearColor(1.f, 0.85f, 0.2f), 0.3f);
			for (ABIBEnemy* Enemy : EnemiesInRadius(GetActorLocation(), BIBTuning::GavelRadius))
			{
				Enemy->Stun(BIBTuning::GavelStunPerHit * JudgeData.GavelSmashesDone); // 0.5 s per hit, max 1.5 s
				Enemy->ReceiveImpact(this, SmashWeight);
				if (Enemy->JudgeState == EBIBJudgeState::Guilty)
				{
					++Enemy->GavelStacks;
					Enemy->GavelStacksExpireAt = Now() + BIBTuning::GavelStackLifetime;
					if (Enemy->GavelStacks >= 2)
					{
						Enemy->GavelStacks = 0;
						const FBIBScopedHit Scope(this, BIB::Feel::ESource::Move, true, static_cast<int32>(BIB::Feel::EWeight::Heavy));
						KillEnemy(Enemy, true);
					}
				}
			}
		}
		if (ActionTime >= BIBTuning::GavelWindup + BIBTuning::GavelSpacing * 2.f + BIBTuning::GavelForeSwing)
		{
			EndAction();
		}
		break;

	case EBIBAction::JudgeRetribution:
		if (!JudgeData.bActionEffectDone && ActionTime >= BIBTuning::RetributionWindup)
		{
			JudgeData.bActionEffectDone = true;
			BIBJudgeKit::DrawGroundCircle(World, Feet, BIBTuning::RetributionRadius, FColor::Orange, 1.f);
			int32 Killed = 0;
			PulseAge = 0.f;
			PulseLean = static_cast<float>(BIB::Feel::FeelFor(BIB::Feel::EWeight::Finisher).ImpactLean);
			ABIBFx::ShockRing(World, Feet, BIBTuning::RetributionRadius, BIBFxColours::Gold, 0.4f);
			const FBIBScopedHit Scope(this, BIB::Feel::ESource::Special, true, static_cast<int32>(BIB::Feel::EWeight::Finisher));
			for (ABIBEnemy* Enemy : EnemiesInRadius(GetActorLocation(), BIBTuning::RetributionRadius))
			{
				if (Enemy->JudgeState == EBIBJudgeState::Guilty)
				{
					KillEnemy(Enemy, true);
					++Killed;
				}
				else
				{
					Enemy->Stun(BIBTuning::RetributionStun);
					Enemy->ReceiveImpact(this, BIB::Feel::EWeight::Heavy);
				}
			}
			Message(FString::Printf(TEXT("Retribution: %d guilty struck, the rest stunned."), Killed));
		}
		if (ActionTime >= BIBTuning::RetributionWindup + BIBTuning::RetributionDuration + BIBTuning::RetributionForeSwing)
		{
			EndAction();
		}
		break;

	case EBIBAction::JudgeJustice:
		if (Now() >= JudgeData.JusticeNextGrowAt)
		{
			JudgeData.JusticeNextGrowAt += 1.f;
			JudgeData.JusticeRadius *= 1.f + BIBTuning::JusticeGrowthPerSecond;
		}
		BIBJudgeKit::DrawGroundCircle(World, Feet, JudgeData.JusticeRadius, FColor::Yellow, -1.f);
		if (ActionTime >= BIBTuning::JusticeFailSafe)
		{
			JudgeJusticeTrigger(nullptr); // fail-safe: the counter hits nothing
		}
		break;

	default:
		EndAction();
		break;
	}
}

void ABIBPlayerCharacter::JudgeJusticeTrigger(ABIBEnemy* Attacker)
{
	// The ultimate's finisher: every sentence lands with the 14-frame hit stop, knockdown and the big impact.
	const FBIBScopedHit Scope(this, BIB::Feel::ESource::Ultimate, true);
	StartPose(0.f, 0.4f, 1.5f);
	if (Attacker)
	{
		KillEnemy(Attacker, true); // NO BOSSES: bosses take heavy damage instead
	}
	int32 Killed = 0;
	for (ABIBEnemy* Enemy : EnemiesInRadius(GetActorLocation(), JudgeData.JusticeRadius))
	{
		if (Enemy->JudgeState == EBIBJudgeState::Guilty)
		{
			KillEnemy(Enemy, true);
			++Killed;
		}
	}
	// WORKING interpretation of "you gain reg attack for 45 seconds": regular attack usable in any mode.
	JudgeData.RegAttackBuffUntil = Now() + BIBTuning::JusticeRegAttackBuff;
	EndAction();
	Message(FString::Printf(TEXT("JUSTICE: %d guilty enemies sentenced. Regular attack granted for 45 s."), Killed));
}

void ABIBPlayerCharacter::JudgeOnHit(ABIBEnemy* Attacker, float Damage)
{
	JudgeData.LastHitAt = Now();
	if (!Attacker || !Attacker->IsAlive())
	{
		return;
	}
	switch (JudgeData.Mode)
	{
	case EBIBJudgeMode::Witness:
		if (Attacker->JudgeState == EBIBJudgeState::Innocent)
		{
			if (JudgeData.RestrainingOrders > 0)
			{
				--JudgeData.RestrainingOrders;
				Attacker->JudgeState = EBIBJudgeState::Guilty;
			}
			else
			{
				Attacker->JudgeState = EBIBJudgeState::Suspect;
			}
		}
		break;
	case EBIBJudgeMode::Lawyer:
		if (Attacker->JudgeState == EBIBJudgeState::Suspect)
		{
			Attacker->JudgeState = EBIBJudgeState::Guilty;
		}
		break;
	case EBIBJudgeMode::Judge:
		break;
	}
	// Enemies that hit Judge 10 times or deal 10% of Judge's health die instantly (NO BOSSES: heavy damage instead).
	++Attacker->HitsOnJudge;
	Attacker->DamageOnJudge += Damage;
	if (Attacker->HitsOnJudge >= BIBTuning::JudgeHitsToCondemn || Attacker->DamageOnJudge >= Active().MaxHP * BIBTuning::JudgeDamageFractionToCondemn)
	{
		Attacker->HitsOnJudge = 0;
		Attacker->DamageOnJudge = 0.f;
		KillEnemy(Attacker, true);
		Message(TEXT("Condemned: an attacker paid for touching the Judge."));
	}
}

void ABIBPlayerCharacter::JudgeTick(float DeltaSeconds)
{
	const float T = Now();
	if (JudgeData.bForceJudgeAfterLock && T >= JudgeData.CourtLockedUntil)
	{
		JudgeData.bForceJudgeAfterLock = false;
		JudgeData.Mode = EBIBJudgeMode::Judge;
		if (IsJudgeActive())
		{
			Message(TEXT("Court lock over: forced into Judge mode."));
		}
	}
	// Lawyer passive: every 0.5 s without damage a random suspect becomes guilty (2 s recovery after a hit).
	if (IsJudgeActive() && JudgeData.Mode == EBIBJudgeMode::Lawyer && T - JudgeData.LastHitAt >= BIBTuning::LawyerRecovery
		&& T >= JudgeData.NextLawyerConversionAt)
	{
		JudgeData.NextLawyerConversionAt = T + BIBTuning::LawyerQuietInterval;
		TArray<ABIBEnemy*> Suspects;
		if (ABIBGameMode* GameMode = GetMode())
		{
			for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
			{
				if (Enemy->JudgeState == EBIBJudgeState::Suspect)
				{
					Suspects.Add(Enemy);
				}
			}
		}
		if (Suspects.Num() > 0)
		{
			Suspects[FMath::RandRange(0, Suspects.Num() - 1)]->JudgeState = EBIBJudgeState::Guilty;
		}
	}
}

FString ABIBPlayerCharacter::JudgeKitLine() const
{
	const float T = Now();
	FString Line = FString::Printf(TEXT("Mode: %s | Restraining orders: %d"), *JudgeModeName(JudgeData.Mode), JudgeData.RestrainingOrders);
	if (JudgeData.Mode == EBIBJudgeMode::Lawyer)
	{
		Line += T - JudgeData.LastHitAt >= BIBTuning::LawyerRecovery ? TEXT(" | Lawyer passive: converting") : TEXT(" | Lawyer passive: recovering");
	}
	if (T < JudgeData.CourtLockedUntil)
	{
		Line += FString::Printf(TEXT(" | Court locked %.0fs"), JudgeData.CourtLockedUntil - T);
	}
	if (T < JudgeData.RegAttackBuffUntil)
	{
		Line += FString::Printf(TEXT(" | Justice reg attack %.0fs"), JudgeData.RegAttackBuffUntil - T);
	}
	Line += TEXT(" | Only GUILTY enemies can be hurt");
	return Line;
}
