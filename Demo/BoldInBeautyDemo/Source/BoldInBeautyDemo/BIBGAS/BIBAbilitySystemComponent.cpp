#include "BIBGAS/BIBAbilitySystemComponent.h"

#include "BIBGAS/BIBAttributeSet.h"
#include "BIBGAS/BIBGameplayEffects.h"
#include "BIBGAS/BIBGameplayTags.h"

#include "GameplayCueManager.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

UBIBAbilitySystemComponent::UBIBAbilitySystemComponent()
{
	SetIsReplicatedByDefault(false);
}

void UBIBAbilitySystemComponent::NotifyDamageTaken(float Dealt, float Overkill, AActor* Instigator)
{
	OnDamageTaken.Broadcast(Dealt, Overkill, Instigator);
}

float UBIBAbilitySystemComponent::GetHealthValue() const
{
	return GetNumericAttribute(UBIBAttributeSet::GetHealthAttribute());
}

float UBIBAbilitySystemComponent::GetMaxHealthValue() const
{
	return GetNumericAttribute(UBIBAttributeSet::GetMaxHealthAttribute());
}

float UBIBAbilitySystemComponent::GetUltimateValue() const
{
	return GetNumericAttribute(UBIBAttributeSet::GetUltimateAttribute());
}

void UBIBAbilitySystemComponent::SetHealthValue(float Value)
{
	SetNumericAttributeBase(UBIBAttributeSet::GetHealthAttribute(), Value);
}

void UBIBAbilitySystemComponent::SetMaxHealthValue(float Value)
{
	SetNumericAttributeBase(UBIBAttributeSet::GetMaxHealthAttribute(), Value);
	// Health never stays above the new maximum.
	if (GetHealthValue() > GetMaxHealthValue())
	{
		SetHealthValue(GetMaxHealthValue());
	}
}

void UBIBAbilitySystemComponent::SetUltimateValue(float Value)
{
	SetNumericAttributeBase(UBIBAttributeSet::GetUltimateAttribute(), Value);
}

float UBIBAbilitySystemComponent::ApplyDamage(float Amount, AActor* Instigator)
{
	if (Amount <= 0.f)
	{
		return 0.f;
	}
	const float Before = GetHealthValue();
	FGameplayEffectContextHandle Context = MakeEffectContext();
	if (Instigator)
	{
		Context.AddInstigator(Instigator, Instigator);
	}
	FGameplayEffectSpec Spec(BIBEffects::Damage(), Context, 1.f);
	Spec.SetSetByCallerMagnitude(BIBTags::DataDamage(), Amount);
	ApplyGameplayEffectSpecToSelf(Spec);
	return FMath::Max(0.f, Before - GetHealthValue());
}

float UBIBAbilitySystemComponent::ApplyHealing(float Amount)
{
	if (Amount <= 0.f)
	{
		return 0.f;
	}
	const float Before = GetHealthValue();
	FGameplayEffectContextHandle Context = MakeEffectContext();
	FGameplayEffectSpec Spec(BIBEffects::Heal(), Context, 1.f);
	Spec.SetSetByCallerMagnitude(BIBTags::DataHeal(), Amount);
	ApplyGameplayEffectSpecToSelf(Spec);
	return FMath::Max(0.f, GetHealthValue() - Before);
}

FGameplayEffectQuery UBIBAbilitySystemComponent::CooldownQuery(const FGameplayTag& Tag) const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(Tag);
	return FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags);
}

void UBIBAbilitySystemComponent::ApplyTimedTag(const FGameplayTag& Tag, float Seconds)
{
	const UGameplayEffect* Effect = BIBEffects::TimedTag(Tag);
	if (!Effect || Seconds <= 0.f)
	{
		return;
	}
	FGameplayEffectContextHandle Context = MakeEffectContext();
	FGameplayEffectSpec Spec(Effect, Context, 1.f);
	Spec.SetSetByCallerMagnitude(BIBTags::DataDuration(), Seconds);
	ApplyGameplayEffectSpecToSelf(Spec);
}

void UBIBAbilitySystemComponent::StartCooldown(const FGameplayTag& CooldownTag, float Seconds)
{
	if (!CooldownTag.IsValid())
	{
		return;
	}
	ClearCooldown(CooldownTag);
	ApplyTimedTag(CooldownTag, Seconds);
}

float UBIBAbilitySystemComponent::GetCooldownRemaining(const FGameplayTag& CooldownTag) const
{
	if (!CooldownTag.IsValid() || !HasMatchingGameplayTag(CooldownTag))
	{
		return 0.f;
	}
	float Remaining = 0.f;
	for (const float Time : GetActiveEffectsTimeRemaining(CooldownQuery(CooldownTag)))
	{
		Remaining = FMath::Max(Remaining, Time);
	}
	return Remaining;
}

void UBIBAbilitySystemComponent::ReduceCooldown(const FGameplayTag& CooldownTag, float Seconds)
{
	const float Remaining = GetCooldownRemaining(CooldownTag);
	if (Remaining <= 0.f || Seconds <= 0.f)
	{
		return;
	}
	ClearCooldown(CooldownTag);
	if (Remaining - Seconds > 0.01f)
	{
		ApplyTimedTag(CooldownTag, Remaining - Seconds);
	}
}

void UBIBAbilitySystemComponent::ClearCooldown(const FGameplayTag& CooldownTag)
{
	if (!CooldownTag.IsValid())
	{
		return;
	}
	for (const FActiveGameplayEffectHandle& Handle : GetActiveEffects(CooldownQuery(CooldownTag)))
	{
		RemoveActiveGameplayEffect(Handle);
	}
}

void UBIBAbilitySystemComponent::GrantTimedTag(const FGameplayTag& Tag, float Seconds)
{
	if (!Tag.IsValid() || Seconds <= 0.f)
	{
		return;
	}
	// Keep the longer of the running grant and the new one.
	const float Running = GetCooldownRemaining(Tag);
	if (Running >= Seconds)
	{
		return;
	}
	ClearCooldown(Tag);
	ApplyTimedTag(Tag, Seconds);
}

void UBIBAbilitySystemComponent::SyncMirroredTags(const FName& Group, const TArray<FGameplayTag>& Desired)
{
	TArray<FGameplayTag>& Current = Mirrored.FindOrAdd(Group);
	for (int32 Index = Current.Num() - 1; Index >= 0; --Index)
	{
		if (!Desired.Contains(Current[Index]))
		{
			RemoveLooseGameplayTag(Current[Index]);
			Current.RemoveAtSwap(Index);
		}
	}
	for (const FGameplayTag& Tag : Desired)
	{
		if (Tag.IsValid() && !Current.Contains(Tag))
		{
			AddLooseGameplayTag(Tag);
			Current.Add(Tag);
		}
	}
}

void UBIBAbilitySystemComponent::ClearMirroredTags(const FName& Group)
{
	SyncMirroredTags(Group, TArray<FGameplayTag>());
}

void UBIBAbilitySystemComponent::ExecuteBIBCue(const FGameplayTag& CueTag, const FVector& Location, float Magnitude)
{
	if (!CueTag.IsValid())
	{
		return;
	}
	FGameplayCueParameters Parameters;
	Parameters.Location = Location;
	Parameters.RawMagnitude = Magnitude;
	ExecuteGameplayCue(CueTag, Parameters);
}
