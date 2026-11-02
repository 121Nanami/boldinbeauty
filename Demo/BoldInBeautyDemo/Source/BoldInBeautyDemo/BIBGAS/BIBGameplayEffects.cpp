#include "BIBGAS/BIBGameplayEffects.h"

#include "BIBGAS/BIBAttributeSet.h"
#include "BIBGAS/BIBGameplayTags.h"

#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "UObject/Package.h"

namespace
{
	UGameplayEffect* NewEffect(const TCHAR* BaseName, EGameplayEffectDurationType Duration)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UGameplayEffect::StaticClass(), FName(BaseName));
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), Name);
		Effect->DurationPolicy = Duration;
		Effect->AddToRoot(); // built once, lives as long as the process
		return Effect;
	}

	FGameplayEffectModifierMagnitude SetByCaller(const FGameplayTag& Tag)
	{
		FSetByCallerFloat Value;
		Value.DataTag = Tag;
		return FGameplayEffectModifierMagnitude(Value);
	}

	UGameplayEffect* BuildMeta(const TCHAR* BaseName, const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
	{
		UGameplayEffect* Effect = NewEffect(BaseName, EGameplayEffectDurationType::Instant);
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::AddBase;
		Modifier.ModifierMagnitude = SetByCaller(DataTag);
		Effect->Modifiers.Add(Modifier);
		return Effect;
	}
}

namespace BIBEffects
{
	const UGameplayEffect* Damage()
	{
		static UGameplayEffect* Effect = BuildMeta(TEXT("GE_BIB_Damage"), UBIBAttributeSet::GetIncomingDamageAttribute(), BIBTags::DataDamage());
		return Effect;
	}

	const UGameplayEffect* Heal()
	{
		static UGameplayEffect* Effect = BuildMeta(TEXT("GE_BIB_Heal"), UBIBAttributeSet::GetIncomingHealingAttribute(), BIBTags::DataHeal());
		return Effect;
	}

	const UGameplayEffect* TimedTag(const FGameplayTag& GrantedTag)
	{
		if (!GrantedTag.IsValid())
		{
			return nullptr;
		}
		static TMap<FGameplayTag, UGameplayEffect*> Effects;
		if (UGameplayEffect* const* Found = Effects.Find(GrantedTag))
		{
			return *Found;
		}
		UGameplayEffect* Effect = NewEffect(TEXT("GE_BIB_Timed"), EGameplayEffectDurationType::HasDuration);
		Effect->DurationMagnitude = SetByCaller(BIBTags::DataDuration());
		UTargetTagsGameplayEffectComponent& Grants = Effect->FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
		FInheritedTagContainer Changes;
		Changes.AddTag(GrantedTag);
		Grants.SetAndApplyTargetTagChanges(Changes);
		Effects.Add(GrantedTag, Effect);
		return Effect;
	}
}
