#include "BIBGame/BIBFx.h"
#include "BIBGame/BIBVisuals.h"
#include "BIBGame/BIBAnimeStyle.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMesh* GSphere = nullptr;
	UStaticMesh* GCylinder = nullptr;
	UStaticMesh* GCube = nullptr;

	UMaterialInterface* FxBase()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BIB/Materials/M_BIBFx.M_BIBFx"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return Cached.Get();
	}

	// The engine shapes are 100 cm across.
	FVector ToScale(const FVector& SizeCm) { return SizeCm / 100.0; }

	// Recycled effects and the live count (game thread only). Weak pointers: a level change destroys the actors and the entries
	// simply go stale.
	TArray<TWeakObjectPtr<ABIBFx>> GPool;
	int32 GLive = 0;
	int32 GCap = 22;
	constexpr int32 PoolLimit = 48;
}

void ABIBFx::SetCap(int32 Cap)
{
	GCap = FMath::Clamp(Cap, 4, 64);
}

int32 ABIBFx::LiveCount()
{
	return GLive;
}

void ABIBFx::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bLive)
	{
		bLive = false;
		GLive = FMath::Max(0, GLive - 1);
	}
	Super::EndPlay(Reason);
}

void ABIBFx::Retire()
{
	if (bLive)
	{
		bLive = false;
		GLive = FMath::Max(0, GLive - 1);
	}
	if (GPool.Num() >= PoolLimit)
	{
		Destroy();
		return;
	}
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	GPool.Add(this);
}

ABIBFx::ABIBFx()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCastShadow(false);
	MeshComp->SetGenerateOverlapEvents(false);
	RootComponent = MeshComp;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	GSphere = SphereFinder.Object;
	GCylinder = CylinderFinder.Object;
	GCube = CubeFinder.Object;
}

ABIBFx* ABIBFx::Spawn(UWorld* World, EBIBFxShape Shape, const FVector& Location, const FRotator& Rotation, const FVector& StartSize,
	const FVector& EndSize, const FLinearColor& InColour, float InLife, float InIntensity, bool bEssential)
{
	if (!World || World->IsNetMode(NM_DedicatedServer))
	{
		return nullptr;
	}
	if (!bEssential && GLive >= GCap)
	{
		return nullptr;   // over the cap: the effect is dropped (an enemy's tell is essential and never is)
	}
	// Every effect is an action moment: it gets its own fresh frame at once (the 13th frame) so the burst appears together
	// with the damage number / HUD / motion layer instead of up to one 12 fps step later. Throttled inside RequestAccent.
	ABIBAnimeStyleVolume::RequestAccent(World, false);
	// A recycled effect of this world first; a new actor only when the pool is empty.
	ABIBFx* Fx = nullptr;
	while (GPool.Num() > 0 && !Fx)
	{
		ABIBFx* Pooled = GPool.Pop(EAllowShrinking::No).Get();
		if (IsValid(Pooled) && Pooled->GetWorld() == World)
		{
			Fx = Pooled;
			Fx->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			Fx->SetActorHiddenInGame(false);
			Fx->SetActorTickEnabled(true);
		}
	}
	if (!Fx)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Fx = World->SpawnActor<ABIBFx>(ABIBFx::StaticClass(), Location, Rotation, Params);
		if (!Fx)
		{
			return nullptr;
		}
	}
	UStaticMesh* Mesh = Shape == EBIBFxShape::Sphere ? GSphere : (Shape == EBIBFxShape::Slash ? GCube : GCylinder);
	if (Fx->MeshComp->GetStaticMesh() != Mesh)
	{
		Fx->MeshComp->SetStaticMesh(Mesh);
	}
	if (!Fx->Material)
	{
		// Created once per actor; a recycled effect keeps it and only changes its parameters.
		UMaterialInterface* Base = FxBase();
		Fx->bAdditive = Base != nullptr;
		Fx->Material = Base ? UMaterialInstanceDynamic::Create(Base, Fx) : BIBVisuals::MakeTint(Fx->MeshComp->GetMaterial(0), Fx);
		if (Fx->Material)
		{
			Fx->MeshComp->SetMaterial(0, Fx->Material);
		}
	}
	Fx->bLive = true;
	++GLive;
	Fx->StartScale = ToScale(StartSize);
	Fx->EndScale = ToScale(EndSize);
	Fx->Colour = InColour;
	Fx->Intensity = InIntensity;
	// At least two anime steps (2 / 12 s): a shorter burst would be drawn on one held frame only and read as a flicker.
	Fx->Life = FMath::Max(0.17f, InLife);
	Fx->BornAt = FPlatformTime::Seconds();
	Fx->SetActorScale3D(Fx->StartScale);
	Fx->Tick(0.f);
	return Fx;
}

void ABIBFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Age = static_cast<float>(FPlatformTime::Seconds() - BornAt);
	const float Alpha = FMath::Clamp(Age / Life, 0.f, 1.f);
	if (Alpha >= 1.f)
	{
		Retire();
		return;
	}
	// Fast ease-out growth ("slams with overshoot"), linear fade.
	const float Grow = 1.f - FMath::Pow(1.f - Alpha, 3.f);
	SetActorScale3D(FMath::Lerp(StartScale, EndScale, Grow));
	if (Material)
	{
		const float Fade = 1.f - Alpha;
		if (bAdditive)
		{
			Material->SetVectorParameterValue(BIBVisuals::ColorParameter(), Colour);
			Material->SetScalarParameterValue(TEXT("Intensity"), Intensity * Fade);
			Material->SetScalarParameterValue(TEXT("Opacity"), Fade);
		}
		else
		{
			Material->SetVectorParameterValue(BIBVisuals::ColorParameter(), Colour * (0.4f + Fade));
		}
	}
}

void ABIBFx::HitSpark(UWorld* World, const FVector& Location, const FLinearColor& Colour, float Power)
{
	const float Size = FMath::Clamp(40.f + 30.f * Power, 40.f, 220.f);
	Spawn(World, EBIBFxShape::Sphere, Location, FRotator::ZeroRotator, FVector(Size * 0.3f), FVector(Size), Colour, 0.16f + 0.04f * FMath::Min(Power, 3.f), 9.f);
	Spawn(World, EBIBFxShape::Sphere, Location, FRotator::ZeroRotator, FVector(Size * 0.15f), FVector(Size * 0.55f), BIBFxColours::White, 0.09f, 14.f);
}

void ABIBFx::SlashStreak(UWorld* World, const FVector& Location, const FVector& Direction, const FLinearColor& Colour, float Length, int32 ComboStep)
{
	// Alternating diagonal cuts at the trailer's -12 degrees, a bigger cross on the finisher.
	const FRotator Facing = Direction.GetSafeNormal2D().Rotation();
	const float Roll = (ComboStep % 2 == 0 ? -12.f : 12.f) + (ComboStep == 3 ? 0.f : 0.f);
	const FRotator Cut(0.f, Facing.Yaw + 90.f, Roll + (ComboStep % 2 == 0 ? 30.f : -30.f));
	Spawn(World, EBIBFxShape::Slash, Location, Cut, FVector(Length * 0.4f, 6.f, 6.f), FVector(Length, 14.f, 3.f), Colour, 0.14f, 10.f);
	if (ComboStep >= 3)
	{
		const FRotator Cross(0.f, Facing.Yaw + 90.f, -Roll - 30.f);
		Spawn(World, EBIBFxShape::Slash, Location, Cross, FVector(Length * 0.5f, 8.f, 8.f), FVector(Length * 1.4f, 22.f, 4.f), BIBFxColours::White, 0.2f, 12.f);
	}
}

void ABIBFx::ShockRing(UWorld* World, const FVector& Ground, float Radius, const FLinearColor& Colour, float InLife)
{
	const float D = Radius * 2.f;
	Spawn(World, EBIBFxShape::Ring, Ground + FVector(0.0, 0.0, 6.0), FRotator::ZeroRotator, FVector(D * 0.2f, D * 0.2f, 4.f), FVector(D, D, 2.f), Colour, InLife, 5.f);
}

void ABIBFx::Telegraph(UWorld* World, const FVector& Location, bool bUnblockable)
{
	// Zenless-style warning flash on the attacker: gold = dodge or parry it, rose = it cannot be blocked or parried.
	// The warning gets its own fresh frame at once (the 13th frame), so a 12 fps hold never hides an enemy's tell.
	ABIBAnimeStyleVolume::RequestAccent(World, false);
	const FLinearColor Colour = bUnblockable ? BIBFxColours::Rose : BIBFxColours::Gold;
	Spawn(World, EBIBFxShape::Sphere, Location, FRotator::ZeroRotator, FVector(160.f), FVector(20.f), Colour, 0.22f, 16.f, true);
	Spawn(World, EBIBFxShape::Slash, Location, FRotator(0.f, 0.f, -12.f), FVector(10.f, 10.f, 10.f), FVector(10.f, 260.f, 8.f), Colour, 0.18f, 14.f, true);
}

void ABIBFx::StunBurst(UWorld* World, const FVector& Location)
{
	Spawn(World, EBIBFxShape::Sphere, Location, FRotator::ZeroRotator, FVector(60.f), FVector(420.f), BIBFxColours::Cyan, 0.35f, 10.f);
	Spawn(World, EBIBFxShape::Ring, Location, FRotator(0.f, 0.f, 90.f), FVector(80.f, 80.f, 4.f), FVector(600.f, 600.f, 2.f), BIBFxColours::Rose, 0.45f, 8.f);
}

void ABIBFx::AreaWarning(UWorld* World, const FVector& Ground, float Radius, float Seconds, const FLinearColor& Colour)
{
	const float D = Radius * 2.f;
	// Grows to the full danger zone exactly when it goes off.
	Spawn(World, EBIBFxShape::Ring, Ground + FVector(0.0, 0.0, 4.0), FRotator::ZeroRotator, FVector(10.f, 10.f, 3.f), FVector(D, D, 3.f), Colour, Seconds, 3.f, true);
	Spawn(World, EBIBFxShape::Ring, Ground + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(D, D, 1.f), FVector(D, D, 1.f), Colour * 0.5f, Seconds, 1.5f, true);
}
