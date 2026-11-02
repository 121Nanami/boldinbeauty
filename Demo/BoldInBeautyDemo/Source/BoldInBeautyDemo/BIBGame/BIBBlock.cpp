#include "BIBGame/BIBBlock.h"
#include "BIBGame/BIBVisuals.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABIBBlock::ABIBBlock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetMobility(EComponentMobility::Movable);
	RootComponent = MeshComp;

	LabelComp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComp->SetupAttachment(MeshComp);
	LabelComp->SetHorizontalAlignment(EHTA_Center);
	LabelComp->SetVerticalAlignment(EVRTA_TextCenter);
	LabelComp->SetHiddenInGame(true);
	LabelComp->SetVisibility(false);
	LabelComp->SetAbsolute(false, false, true); // keep text size independent of the block's scale

	// Engine content shipped with every Unreal install; no project assets required.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	ConeMesh = ConeFinder.Object;
	ShapeMaterial = MaterialFinder.Object;

	MeshComp->SetStaticMesh(CubeMesh);
}

ABIBBlock* ABIBBlock::SpawnBlock(UWorld* World, EBIBShape InShape, const FVector& Location, const FVector& SizeCm,
	const FLinearColor& InColor, bool bInCollides, bool bInVisible)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABIBBlock* Block = World->SpawnActor<ABIBBlock>(ABIBBlock::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!Block)
	{
		return nullptr;
	}
	Block->ApplyShape(InShape);
	Block->SetSize(SizeCm);
	Block->SetColor(InColor);
	if (bInCollides)
	{
		Block->MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		Block->MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Block->MeshComp->SetHiddenInGame(!bInVisible);
	Block->MeshComp->SetCastShadow(bInVisible);
	return Block;
}

void ABIBBlock::ApplyShape(EBIBShape InShape)
{
	UStaticMesh* Mesh = CubeMesh;
	switch (InShape)
	{
	case EBIBShape::Sphere: Mesh = SphereMesh; break;
	case EBIBShape::Cylinder: Mesh = CylinderMesh; break;
	case EBIBShape::Cone: Mesh = ConeMesh; break;
	default: break;
	}
	MeshComp->SetStaticMesh(Mesh);
}

void ABIBBlock::SetSize(const FVector& SizeCm)
{
	// Basic shapes are 100 cm across.
	MeshComp->SetWorldScale3D(SizeCm / 100.0);
}

void ABIBBlock::SetColor(const FLinearColor& InColor)
{
	if (!DynMaterial && ShapeMaterial)
	{
		DynMaterial = BIBVisuals::MakeTint(ShapeMaterial, this);
		MeshComp->SetMaterial(0, DynMaterial);
	}
	if (DynMaterial)
	{
		DynMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), InColor);
	}
}

void ABIBBlock::SetLabel(const FString& Text, const FLinearColor& InColor, float HeightAboveCentre, float WorldSize)
{
	bHasLabel = !Text.IsEmpty();
	LabelComp->SetText(FText::FromString(Text));
	LabelComp->SetTextRenderColor(InColor.ToFColor(true));
	LabelComp->SetWorldSize(WorldSize);
	LabelComp->SetWorldLocation(GetActorLocation() + FVector(0.0, 0.0, HeightAboveCentre));
	LabelComp->SetHiddenInGame(!bHasLabel);
	LabelComp->SetVisibility(bHasLabel);
	SetActorTickEnabled(bHasLabel);
}

void ABIBBlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bHasLabel)
	{
		return;
	}
	if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = Camera->GetCameraLocation() - LabelComp->GetComponentLocation();
		LabelComp->SetWorldRotation(FRotator(0.0, ToCamera.Rotation().Yaw, 0.0));
	}
}
