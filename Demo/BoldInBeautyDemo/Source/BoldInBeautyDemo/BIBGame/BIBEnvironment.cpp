#include "BIBGame/BIBEnvironment.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"

ABIBEnvironment::ABIBEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	SunLight->SetupAttachment(Root);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->SetRelativeRotation(FRotator(-50.0, 35.0, 0.0));
	SunLight->SetIntensity(6.f);
	SunLight->SetCastShadows(true);

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Fill"));
	FillLight->SetupAttachment(Root);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetRelativeRotation(FRotator(-35.0, 215.0, 0.0));
	FillLight->SetIntensity(2.5f);
	FillLight->SetCastShadows(false);
}
