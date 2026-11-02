// One GameplayAbility class serves every operator slot (regular attack, Move 1-3, Special, Ultimate, Dodge, Parry).
// The ability is granted per team member with FGameplayAbilitySpec::InputID = PackSlot(member, slot), so the same class
// carries 8 abilities x 3 operators. Activation is the gate: it checks the operator's cooldown tag and the kit's own
// conditions through ABIBPlayerCharacter::CanActivateKitSlot, then runs ABIBPlayerCharacter::PerformKitSlot (the kit logic).
// While an ability is active it grants the matching tag BIB.Ability.<Operator>.<Slot>.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BIBGameplayAbility.generated.h"

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBAbility_KitSlot : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBIBAbility_KitSlot();

	static int32 PackSlot(int32 MemberIndex, int32 Slot) { return MemberIndex * 16 + Slot; }
	static int32 MemberOf(int32 Packed) { return Packed / 16; }
	static int32 SlotOf(int32 Packed) { return Packed % 16; }

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
