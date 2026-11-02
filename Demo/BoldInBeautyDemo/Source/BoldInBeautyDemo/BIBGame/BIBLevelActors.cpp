#include "BIBGame/BIBLevelActors.h"
#include "BIBGame/BIBVisuals.h"

#include "BIBGame/BIBTuning.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// ============================================================================ door

ABIBDoor::ABIBDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	DoorRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = DoorRoot;

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(DoorRoot);
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorRoot);
	DoorMesh->SetStaticMesh(CubeFinder.Object);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetDoorSize(FVector(120.0, BIBTuning::DoorWidth + 10.0, BIBTuning::WallHeight), BIBTuning::CeilingHeight);
}

void ABIBDoor::SetDoorSize(const FVector& VisibleSizeCm, float BlockerHeightCm)
{
	// The pivot is on the floor: the visible door stands on it and the invisible blocker reaches the ceiling.
	Blocker->SetBoxExtent(FVector(VisibleSizeCm.X * 0.5, VisibleSizeCm.Y * 0.5, BlockerHeightCm * 0.5));
	Blocker->SetRelativeLocation(FVector(0.0, 0.0, BlockerHeightCm * 0.5));
	DoorMesh->SetRelativeScale3D(VisibleSizeCm / 100.0);
	DoorMesh->SetRelativeLocation(FVector(0.0, 0.0, VisibleSizeCm.Z * 0.5));
}

void ABIBDoor::BeginPlay()
{
	Super::BeginPlay();
	DoorMaterial = BIBVisuals::MakeTint(DoorMesh->GetMaterial(0), this);
	if (DoorMaterial)
	{
		DoorMesh->SetMaterial(0, DoorMaterial);
		DoorMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), DoorColour);
	}
}

void ABIBDoor::SetOpen(bool bInOpen)
{
	bOpen = bInOpen;
	Blocker->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	DoorMesh->SetVisibility(!bOpen);
	OnDoorStateChanged(bOpen);
}

// ============================================================================ section

ABIBSection::ABIBSection()
{
	PrimaryActorTick.bCanEverTick = false;
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetBoxExtent(FVector(BIBTuning::SectionHalfSize, BIBTuning::SectionHalfSize, BIBTuning::CeilingHeight * 0.5));
	Bounds->SetHiddenInGame(true);
	Bounds->ShapeColor = FColor(255, 160, 40);
	RootComponent = Bounds;
}

void ABIBSection::SetAreaSize(const FVector& HalfExtentCm)
{
	Bounds->SetBoxExtent(HalfExtentCm);
}

bool ABIBSection::ContainsPoint(const FVector& Location, float InsetCm) const
{
	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	const FVector Scale = Bounds->GetComponentTransform().GetScale3D().GetAbs();
	// Inset is in world centimetres; convert per axis.
	const double InX = Scale.X > 0.0 ? InsetCm / Scale.X : 0.0;
	const double InY = Scale.Y > 0.0 ? InsetCm / Scale.Y : 0.0;
	return FMath::Abs(Local.X) <= Extent.X - InX && FMath::Abs(Local.Y) <= Extent.Y - InY && FMath::Abs(Local.Z) <= Extent.Z;
}

FVector ABIBSection::FindSpawnLocation(int32 SpawnIndex) const
{
	if (SpawnPoints.Num() > 0)
	{
		const AActor* Point = SpawnPoints[SpawnIndex % SpawnPoints.Num()];
		if (Point)
		{
			return Point->GetActorLocation() + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 120.0);
		}
	}
	// Random point inside the inner 80% of the box, dropped onto the floor below it.
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	const FVector LocalTop(FMath::FRandRange(-0.8f, 0.8f) * Extent.X, FMath::FRandRange(-0.8f, 0.8f) * Extent.Y, Extent.Z - 50.0);
	const FTransform& Transform = Bounds->GetComponentTransform();
	const FVector Top = Transform.TransformPosition(LocalTop);
	const FVector Bottom = Transform.TransformPosition(FVector(LocalTop.X, LocalTop.Y, -Extent.Z - 2000.0));
	FHitResult Hit;
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		return Hit.ImpactPoint + FVector(0.0, 0.0, 300.0);
	}
	return Transform.TransformPosition(FVector(LocalTop.X, LocalTop.Y, -Extent.Z + 300.0));
}

// ============================================================================ story NPC

ABIBStoryNPC::ABIBStoryNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetStaticMesh(CylinderFinder.Object);
	BodyMesh->SetRelativeScale3D(FVector(0.9, 0.9, 2.2));
	BodyMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = BodyMesh;

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(BodyMesh);
	NameLabel->SetAbsolute(false, true, true);
	NameLabel->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetWorldSize(40.f);
	NameLabel->SetTextRenderColor(FColor(100, 255, 150));
}

void ABIBStoryNPC::BeginPlay()
{
	Super::BeginPlay();
	NameLabel->SetText(FText::FromString(SpeakerName + TEXT("\nPress T to talk")));
	if (UMaterialInstanceDynamic* Material = BIBVisuals::MakeTint(BodyMesh->GetMaterial(0), this))
	{
		BodyMesh->SetMaterial(0, Material);
		Material->SetVectorParameterValue(BIBVisuals::ColorParameter(), FLinearColor(0.1f, 0.8f, 0.4f));
	}
}

void ABIBStoryNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = Camera->GetCameraLocation() - NameLabel->GetComponentLocation();
		NameLabel->SetWorldRotation(FRotator(0.0, ToCamera.Rotation().Yaw, 0.0));
	}
}
