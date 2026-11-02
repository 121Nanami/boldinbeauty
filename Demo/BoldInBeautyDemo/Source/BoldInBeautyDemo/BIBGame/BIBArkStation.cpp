#include "BIBGame/BIBArkStation.h"
#include "BIBGame/BIBVisuals.h"
#include "BIBRules/BIBArk.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The sign floats over the station, a few metres past where the player stands.
	constexpr float SignForward = 520.f;
	constexpr float SignHeight = 760.f;

	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->SetMobility(EComponentMobility::Movable);
		return Comp;
	}

	UTextRenderComponent* MakeText(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UTextRenderComponent* Text = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
		Text->SetupAttachment(Parent);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetCastShadow(false);
		return Text;
	}
}

ABIBArkStation::ABIBArkStation()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Floor ring: a thin glowing disc with a darker centre, so it reads as a ring from above.
	Ring = MakeMesh(this, Root, TEXT("Ring"), CylinderFinder.Object);
	Ring->SetRelativeLocation(FVector(0.0, 0.0, -96.0));
	Ring->SetRelativeScale3D(FVector(4.2, 4.2, 0.04));
	RingCore = MakeMesh(this, Root, TEXT("RingCore"), CylinderFinder.Object);
	RingCore->SetRelativeLocation(FVector(0.0, 0.0, -95.0));
	RingCore->SetRelativeScale3D(FVector(3.6, 3.6, 0.04));

	// Beacon: a turning diamond (a cube on its corner) over the station.
	Beacon = MakeMesh(this, Root, TEXT("Beacon"), CubeFinder.Object);
	Beacon->SetRelativeLocation(FVector(SignForward, 0.0, SignHeight + 260.0));
	Beacon->SetRelativeScale3D(FVector(0.55, 0.55, 0.55));

	Sign = MakeText(this, Root, TEXT("Sign"));
	Sign->SetRelativeLocation(FVector(SignForward, 0.0, SignHeight));
	Sign->SetWorldSize(95.f);
	SubSign = MakeText(this, Root, TEXT("SubSign"));
	SubSign->SetRelativeLocation(FVector(SignForward, 0.0, SignHeight - 85.0));
	SubSign->SetWorldSize(42.f);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(SignForward * 0.5, 0.0, 420.0));
	Light->SetIntensity(9000.f);
	Light->SetAttenuationRadius(1400.f);
	Light->SetCastShadows(false);

	if (MaterialFinder.Object)
	{
		RingMaterial = BIBVisuals::MakeTint(MaterialFinder.Object, this);
		BeaconMaterial = BIBVisuals::MakeTint(MaterialFinder.Object, this);
		Ring->SetMaterial(0, RingMaterial);
		Beacon->SetMaterial(0, BeaconMaterial);
		RingCore->SetMaterial(0, BIBVisuals::MakeTint(MaterialFinder.Object, this));
	}
}

void ABIBArkStation::Setup(const FString& InStationId)
{
	StationId = InStationId;
	const BIB::Ark::FStation* Station = BIB::Ark::FindStation(TCHAR_TO_UTF8(*InStationId));
	if (!Station)
	{
		DisplayName = InStationId;
		return;
	}
	DisplayName = UTF8_TO_TCHAR(Station->Name.c_str());
	Colour = FLinearColor(Station->Colour[0], Station->Colour[1], Station->Colour[2]);
	bUnmarked = Station->bUnmarked;
	// Unmarked places (the task room, the resistance's building) get no sign, beacon or coloured light: only a faint ring.
	Sign->SetText(FText::FromString(DisplayName.ToUpper()));
	Sign->SetTextRenderColor(Colour.ToFColor(true));
	SubSign->SetText(FText::FromString(UTF8_TO_TCHAR(Station->Blurb.c_str())));
	SubSign->SetTextRenderColor(FLinearColor(0.92f, 0.94f, 1.f).ToFColor(true));
	Sign->SetVisibility(!bUnmarked);
	SubSign->SetVisibility(!bUnmarked);
	Beacon->SetVisibility(!bUnmarked);
	Light->SetLightColor(bUnmarked ? FLinearColor(0.6f, 0.6f, 0.65f) : Colour);
	Light->SetIntensity(bUnmarked ? 1500.f : 9000.f);
	if (RingMaterial)
	{
		RingMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), bUnmarked ? FLinearColor(0.25f, 0.25f, 0.28f) : Colour);
	}
	if (BeaconMaterial)
	{
		BeaconMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), Colour);
	}
	if (UMaterialInstanceDynamic* CoreMaterial = Cast<UMaterialInstanceDynamic>(RingCore->GetMaterial(0)))
	{
		CoreMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), FLinearColor(0.02f, 0.02f, 0.03f));
	}
}

bool ABIBArkStation::IsInReach(const FVector& Location) const
{
	const FVector Here = GetActorLocation();
	return FVector::Dist2D(Location, Here) <= ReachRadius && FMath::Abs(Location.Z - Here.Z) < 400.f;
}

void ABIBArkStation::Pulse(const FLinearColor& FlashColour, float Seconds)
{
	PulseColour = FlashColour;
	PulseLength = FMath::Max(Seconds, 0.05f);
	PulseLeft = PulseLength;
}

void ABIBArkStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	// The beacon turns and bobs; the signs face the camera (yaw only) so they read from anywhere.
	Beacon->SetRelativeRotation(FRotator(45.0, Clock * 70.0, 45.0));
	Beacon->SetRelativeLocation(FVector(SignForward, 0.0, SignHeight + 260.0 + FMath::Sin(Clock * 2.0) * 25.0));
	if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		for (UTextRenderComponent* Text : {Sign.Get(), SubSign.Get()})
		{
			const FVector ToCamera = Camera->GetCameraLocation() - Text->GetComponentLocation();
			Text->SetWorldRotation(FRotator(0.0, ToCamera.Rotation().Yaw, 0.0));
		}
	}
	// Breathing glow, plus the pulse (arrival flash, summon flash).
	float Glow = 0.75f + 0.25f * FMath::Sin(Clock * 2.6f);
	FLinearColor Tint = bUnmarked ? FLinearColor(0.25f, 0.25f, 0.28f) : Colour;
	if (PulseLeft > 0.f)
	{
		PulseLeft = FMath::Max(0.f, PulseLeft - DeltaSeconds);
		const float Alpha = PulseLeft / PulseLength;
		Tint = FMath::Lerp(Tint, PulseColour, Alpha);
		Glow += 2.5f * Alpha;
	}
	if (RingMaterial)
	{
		RingMaterial->SetVectorParameterValue(BIBVisuals::ColorParameter(), Tint * Glow);
	}
	if (!bUnmarked)
	{
		Light->SetIntensity(9000.f * Glow);
		Light->SetLightColor(Tint);
	}
}
