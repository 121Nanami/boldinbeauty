// The angels' shared senses (owner request 2026-10-13, goal 2: smart, fair angels - BIBRules/BIBAngelBrain.h). One per world:
// once a frame it records what anyone could see of the commander (ABIBPlayerCharacter::GetAngelView) into a short timeline
// that every angel reads with its own reaction delay, derives the commander's habits from it, and re-plans the crowd
// (attack tokens, ring points around the commander, ranged turns). Angels call UpdateFrame before they think; the first
// call in a frame does the work.
//
// Tuning: Config/DefaultGame.ini [BIB.AngelAI] (BIB::Angel::AiConfigKeys), loaded at start.
// Console (development builds and the editor):
//   BIBAngelReload             re-reads [BIB.AngelAI] from Config/DefaultGame.ini on disk
//   BIBAngel <Key> <Value>     sets one knob now, e.g. "BIBAngel Regular.DodgeChance 0.4"; no arguments lists them
//   BIBAngelStats              logs which behaviours and moves each angel type used (Tools/verify_angels.py)
//   BIBAngelResetStats         clears those counters
//   BIBSpawnAngel <Kind> [N]   spawns N angels (Grunt, Brute, Ranged, Mage, Assassin, MiniBoss, Boss) in front of the commander
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BIBRules/BIBAngelBrain.h"
#include "BIBAngelDirector.generated.h"

class ABIBEnemy;
class ABIBGameMode;

namespace BIBAngelSenses
{
	constexpr double CallOutDelay = 0.5;    // seconds before angels nearby hear where the commander was seen
	constexpr double CallOutRange = 2500.0; // cm
}

namespace BIBAngelConfig
{
	/** Reads [BIB.AngelAI] once (later calls do nothing unless bForce). */
	void Load(bool bForce = false);
}

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBAngelDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Records the commander and re-plans the crowd once per frame (the first angel to think in a frame calls it). */
	void UpdateFrame();

	/** What an angel with this reaction delay perceives now (the snapshot ReactionSeconds old). */
	BIB::Angel::FPlayerView ViewFor(double ReactionSeconds) const;
	/** The commander healed within the window before the delayed snapshot. */
	bool HealingFor(double ReactionSeconds) const;
	/** The newest snapshot (only for movement while the commander is seen; decisions use ViewFor). */
	const BIB::Angel::FPlayerView& Latest() const { return LastView; }

	BIB::Angel::FCrowd& GetCrowd() { return Crowd; }
	const BIB::Angel::FHabits& GetHabits() const { return Habits; }
	/** An angel started a telegraph that lands at HitAt (the habits notice dodges that come too early). */
	void NoteTelegraph(double StartedAt, double HitAt) { Habits.NoteTelegraph(StartedAt, HitAt); }

	/** An angel saw the commander from From: angels nearby hear the call-out (CallOutDelay later). */
	void NoteSighting(const FVector& From, double Time);
	/** A call-out an angel at Near can use: the time the commander was seen. */
	bool HeardSighting(const FVector& Near, double Now, double& OutSeenAt) const;

	/** The campaign chapter of the current stage (other modes count as the middle of the campaign). */
	int32 GetChapter() const;

	/** The game mode's live angels. */
	TArray<ABIBEnemy*> LiveAngels() const;

private:
	BIB::Angel::FPlayerTimeline Timeline;
	BIB::Angel::FPlayerView LastView;
	BIB::Angel::FHabits Habits;
	BIB::Angel::FCrowd Crowd;
	uint64 LastFrame = 0;
	bool bHasView = false;
	double LastSightingAt = -100.0;
	FVector LastSightingFrom = FVector::ZeroVector;

	ABIBGameMode* GetMode() const;
};
