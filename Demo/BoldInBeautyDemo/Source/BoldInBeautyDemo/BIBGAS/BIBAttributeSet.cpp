#include "BIBGAS/BIBAttributeSet.h"

#include "BIBGAS/BIBAbilitySystemComponent.h"

#include "GameplayEffectExtension.h"

UBIBAttributeSet::UBIBAttributeSet()
{
	InitHealth(1.f);
	InitMaxHealth(1.f);
	InitUltimate(0.f);
	InitShield(0.f);
	InitResourceA(0.f);
	InitResourceB(0.f);
	InitDamageDealtMult(1.f);
	InitDamageTakenMult(1.f);
	InitMoveSpeedMult(1.f);
	InitAttackSpeedMult(1.f);
	InitIncomingDamage(0.f);
	InitIncomingHealing(0.f);
}

void UBIBAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(GetMaxHealth(), 0.f));
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetUltimateAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	}
	else if (Attribute == GetShieldAttribute() || Attribute == GetResourceAAttribute() || Attribute == GetResourceBAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UBIBAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	UBIBAbilitySystemComponent* Component = Cast<UBIBAbilitySystemComponent>(&Data.Target);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage > 0.f)
		{
			const float Before = GetHealth();
			const float After = FMath::Clamp(Before - Damage, 0.f, FMath::Max(GetMaxHealth(), 0.f));
			SetHealth(After);
			if (Component)
			{
				const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
				Component->NotifyDamageTaken(Before - After, Damage - (Before - After), Context.GetOriginalInstigator());
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float Healing = GetIncomingHealing();
		SetIncomingHealing(0.f);
		if (Healing > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() + Healing, 0.f, FMath::Max(GetMaxHealth(), 0.f)));
		}
	}
}
