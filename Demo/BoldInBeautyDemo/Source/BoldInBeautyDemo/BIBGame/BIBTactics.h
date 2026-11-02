// The angels' behaviour layer glue (2026-10-12). One per world: it samples the commander once per frame into the rolling
// player model (BIBRules/BIBPlayerModel.h), hands the live angels to the squad board (BIBRules/BIBSquad.h: attack tokens,
// roles, slots) and casts the steering whiskers (BIBRules/BIBSteer.h) against the stage's static geometry. ABIBEnemy reads
// its assignment and the bounded advice from here; nothing in this file changes a hit point, damage, speed or timing number.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BIBRules/BIBPlayerModel.h"
#include "BIBRules/BIBSquad.h"
#include "BIBRules/BIBSteer.h"
#include "BIBTactics.generated.h"

class ABIBEnemy;
class ABIBGameMode;
class ABIBPlayerCharacter;

/** Per-angel steering memory (stuck detection, last detour side, cached whiskers). */
struct FBIBSteerMemory
{
	BIB::Tactics::FSteerState State;
	BIB::Tactics::FSteerResult Result;
	FVector Desired = FVector::ZeroVector;
	float NextCastAt = 0.f;
	float LastCallAt = -10.f;
};

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBTactics : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBIBTactics* Get(const UObject* WorldContext);

	/** Updates the model and the board once per frame (later calls in the same frame return at once). */
	void Refresh(ABIBGameMode* Mode, ABIBPlayerCharacter* Target);

	const BIB::Tactics::FAdvice& Advice() const { return CurrentAdvice; }
	const BIB::Tactics::FPlayerModel& Model() const { return PlayerModel; }
	bool Knows(const ABIBEnemy* Enemy) const;
	bool HasToken(const ABIBEnemy* Enemy) const;
	BIB::Tactics::EDuty DutyOf(const ABIBEnemy* Enemy) const;
	/** World-space bearing (radians) of this angel's slot around the commander. */
	double SlotAngle(const ABIBEnemy* Enemy) const;
	int32 TokensHeld() const { return Squad.TokensHeld(); }

	/** Bends a desired horizontal direction around static geometry. SpeedScale is 1 unless the angel is boxed in. */
	FVector SteerDirection(const ABIBEnemy& Enemy, const FVector& Desired, FBIBSteerMemory& Memory, float& SpeedScale) const;

private:
	BIB::Tactics::FPlayerModel PlayerModel;
	BIB::Tactics::FSquad Squad;
	BIB::Tactics::FAdvice CurrentAdvice;
	uint64 LastFrame = MAX_uint64;
	float LastSampleAt = -1.f;
};
