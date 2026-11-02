// Gameplay tag vocabulary of the demo. Every tag is generated from the rules layer (BIBRules/BIBKitRegistry.cpp) into
// Config/DefaultGameplayTags.ini by Tools/gen_tags.py; this file only looks them up (cached) and builds the
// per-operator names:  BIB.Ability.<Operator>.<Slot>   BIB.Cooldown.<Operator>.<Slot>   BIB.State.*   BIB.Status.*
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include <string>

namespace BIBTags
{
	/** Looks a tag up by name (cached). Returns an invalid tag when it is not registered (logged once). */
	FGameplayTag Find(const FName& Name);
	FGameplayTag Find(const char* Name);
	FGameplayTag Find(const std::string& Name);

	/** BIB.Cooldown.<Kit>.<Slot> and BIB.Ability.<Kit>.<Slot>; Slot is Basic, Move1..3, Special, Ultimate, Dodge or Parry. */
	FGameplayTag Cooldown(const FString& KitName, const TCHAR* SlotName);
	FGameplayTag Ability(const FString& KitName, const TCHAR* SlotName);

	/** GameplayCue.BIB.<name> for a rules-layer cue id. */
	FGameplayTag Cue(int32 CueId);

	// Frequently used tags.
	FGameplayTag DataDamage();     // SetByCaller names of the dynamic GameplayEffects
	FGameplayTag DataHeal();
	FGameplayTag DataDuration();
	FGameplayTag StateAlive();
	FGameplayTag StateDead();
	FGameplayTag StateInvulnerable();
	FGameplayTag StateBlocking();
	FGameplayTag StateParrying();
	FGameplayTag StateDodging();
	FGameplayTag StateStunned();
	FGameplayTag StateCasting();
	FGameplayTag StateMovementLocked();
	FGameplayTag StateShielded();
	FGameplayTag StateActive();
	FGameplayTag TeamPlayer();
	FGameplayTag TeamEnemy();

	/** Writes every tag the rules layer expects but the tag manager does not know to the log (call after start-up). */
	int32 LogMissingTags();
}
