#include "BIBGreyboxMover.h"

#include "Components/StaticMeshComponent.h"

ABIBGreyboxMover::ABIBGreyboxMover()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Mesh;
}

void ABIBGreyboxMover::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetActorLocation();
	StartRotation = GetActorRotation();
	Elapsed = Phase * Period;
}

void ABIBGreyboxMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	const float Cycle = 2.f * PI * Elapsed / FMath::Max(Period, 0.1f);
	if (!Offset.IsNearlyZero())
	{
		SetActorLocation(StartLocation + Offset * (0.5f - 0.5f * FMath::Cos(Cycle)));
	}
	if (!RotationRate.IsNearlyZero())
	{
		if (bSwing)
		{
			SetActorRotation(StartRotation + RotationRate * FMath::Sin(Cycle));
		}
		else
		{
			AddActorLocalRotation(RotationRate * DeltaSeconds);
		}
	}
}
