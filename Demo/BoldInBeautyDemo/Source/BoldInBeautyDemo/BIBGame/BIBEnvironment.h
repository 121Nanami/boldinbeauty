// Movable lights so the code-built world is visible without baked lighting.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIBEnvironment.generated.h"

class UDirectionalLightComponent;
class USceneComponent;

UCLASS()
class BOLDINBEAUTYDEMO_API ABIBEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ABIBEnvironment();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> SunLight;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> FillLight;
};
