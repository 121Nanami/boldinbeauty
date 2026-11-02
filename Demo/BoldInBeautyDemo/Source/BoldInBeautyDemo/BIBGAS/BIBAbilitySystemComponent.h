// The ability system component used by operators and enemies. Adds the demo's helpers on top of the stock component:
// cooldowns as GameplayEffects that grant a cooldown tag, damage / healing as instant GameplayEffects, timed tag
// grants (invulnerability, statuses) and a sync helper that mirrors rules-layer state into loose gameplay tags.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BIBAbilitySystemComponent.generated.h"

/** Dealt = health actually removed, Overkill = damage beyond zero health. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FBIBDamageTakenSignature, float /*Dealt*/, float /*Overkill*/, AActor* /*Instigator*/);

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UBIBAbilitySystemComponent();

	FBIBDamageTakenSignature OnDamageTaken;
	void NotifyDamageTaken(float Dealt, float Overkill, AActor* Instigator);

	// ---- attributes
	float GetHealthValue() const;
	float GetMaxHealthValue() const;
	float GetUltimateValue() const;
	void SetHealthValue(float Value);
	void SetMaxHealthValue(float Value);
	void SetUltimateValue(float Value);

	/** Applies the damage GameplayEffect to this component. Returns the health actually removed. */
	float ApplyDamage(float Amount, AActor* Instigator);
	/** Applies the healing GameplayEffect. Returns the health actually restored. */
	float ApplyHealing(float Amount);

	// ---- cooldowns (GameplayEffect granting the cooldown tag)
	void StartCooldown(const FGameplayTag& CooldownTag, float Seconds);
	float GetCooldownRemaining(const FGameplayTag& CooldownTag) const;
	bool IsOnCooldown(const FGameplayTag& CooldownTag) const { return CooldownTag.IsValid() && HasMatchingGameplayTag(CooldownTag); }
	void ReduceCooldown(const FGameplayTag& CooldownTag, float Seconds);
	void ClearCooldown(const FGameplayTag& CooldownTag);

	/** Grants Tag for Seconds through a timed GameplayEffect (re-applying replaces the running one). */
	void GrantTimedTag(const FGameplayTag& Tag, float Seconds);
	float GetTimedTagRemaining(const FGameplayTag& Tag) const { return GetCooldownRemaining(Tag); }
	void RemoveTimedTag(const FGameplayTag& Tag) { ClearCooldown(Tag); }

	// ---- mirrored loose tags
	/** Makes the loose tags of Group exactly Desired (adds the new ones, removes the ones no longer wanted). */
	void SyncMirroredTags(const FName& Group, const TArray<FGameplayTag>& Desired);
	void ClearMirroredTags(const FName& Group);

	// ---- visuals
	/** Fires GameplayCue.BIB.* at a location (Magnitude is the radius). The demo draws placeholder shapes elsewhere. */
	void ExecuteBIBCue(const FGameplayTag& CueTag, const FVector& Location, float Magnitude);

private:
	TMap<FName, TArray<FGameplayTag>> Mirrored;

	FGameplayEffectQuery CooldownQuery(const FGameplayTag& Tag) const;
	void ApplyTimedTag(const FGameplayTag& Tag, float Seconds);
};
