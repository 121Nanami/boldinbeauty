#include "BIBGAS/BIBGameplayAbility.h"

#include "BIBGAS/BIBGameplayTags.h"
#include "BIBGame/BIBPlayerCharacter.h"

#include "AbilitySystemComponent.h"

namespace
{
	ABIBPlayerCharacter* HostOf(const FGameplayAbilityActorInfo* ActorInfo)
	{
		return ActorInfo ? Cast<ABIBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	}

	int32 PackedOf(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo)
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			if (const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
			{
				return Spec->InputID;
			}
		}
		return INDEX_NONE;
	}
}

UBIBAbility_KitSlot::UBIBAbility_KitSlot()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	// A downed operator cannot act; the character mirrors BIB.State.Dead / Stunned onto its component.
	ActivationBlockedTags.AddTag(BIBTags::StateDead());
}

bool UBIBAbility_KitSlot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const int32 Packed = PackedOf(Handle, ActorInfo);
	const ABIBPlayerCharacter* Character = HostOf(ActorInfo);
	return Packed != INDEX_NONE && Character
		&& Character->CanActivateKitSlot(MemberOf(Packed), static_cast<EBIBSlot>(SlotOf(Packed)), nullptr);
}

void UBIBAbility_KitSlot::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	const ABIBPlayerCharacter* Character = HostOf(ActorInfo);
	if (Character && Spec.InputID != INDEX_NONE)
	{
		const FGameplayTag Tag = Character->AbilityTagFor(MemberOf(Spec.InputID), static_cast<EBIBSlot>(SlotOf(Spec.InputID)));
		if (Tag.IsValid())
		{
			ActivationOwnedTags.AddTag(Tag);
		}
	}
}

void UBIBAbility_KitSlot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bool bPerformed = false;
	const int32 Packed = PackedOf(Handle, ActorInfo);
	if (ABIBPlayerCharacter* Character = HostOf(ActorInfo))
	{
		if (Packed != INDEX_NONE)
		{
			bPerformed = Character->PerformKitSlot(MemberOf(Packed), static_cast<EBIBSlot>(SlotOf(Packed)));
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, false, !bPerformed);
}
