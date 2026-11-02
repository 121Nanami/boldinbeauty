// One attribute set serves operators and enemies. Health, Ultimate, Shield and the two kit resources are real GAS
// attributes; IncomingDamage / IncomingHealing are meta attributes that the damage and healing GameplayEffects write to
// and PostGameplayEffectExecute turns into Health changes. The modifier attributes mirror what the rules layer
// (BIBRules/BIBKitRuntime) computes so that tools and Blueprint can read them as ordinary attributes.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "BIBAttributeSet.generated.h"

#define BIB_ATTRIBUTE_ACCESSORS(PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UBIBAttributeSet, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBIBAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData Health;
	BIB_ATTRIBUTE_ACCESSORS(Health)

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData MaxHealth;
	BIB_ATTRIBUTE_ACCESSORS(MaxHealth)

	/** 0..100, charged by damage dealt (BIB::Config::UltDamageForFullBar). */
	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData Ultimate;
	BIB_ATTRIBUTE_ACCESSORS(Ultimate)

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData Shield;
	BIB_ATTRIBUTE_ACCESSORS(Shield)

	/** The two kit resources (blood, souls, starlight, luck ...); names come from the kit data. */
	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData ResourceA;
	BIB_ATTRIBUTE_ACCESSORS(ResourceA)

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData ResourceB;
	BIB_ATTRIBUTE_ACCESSORS(ResourceB)

	// Mirrors of the rules layer's modifiers (1 = unmodified).
	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData DamageDealtMult;
	BIB_ATTRIBUTE_ACCESSORS(DamageDealtMult)

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData DamageTakenMult;
	BIB_ATTRIBUTE_ACCESSORS(DamageTakenMult)

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData MoveSpeedMult;
	BIB_ATTRIBUTE_ACCESSORS(MoveSpeedMult)

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData AttackSpeedMult;
	BIB_ATTRIBUTE_ACCESSORS(AttackSpeedMult)

	// Meta attributes: written by GameplayEffects, consumed in PostGameplayEffectExecute.
	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData IncomingDamage;
	BIB_ATTRIBUTE_ACCESSORS(IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "BIB|Attributes")
	FGameplayAttributeData IncomingHealing;
	BIB_ATTRIBUTE_ACCESSORS(IncomingHealing)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
