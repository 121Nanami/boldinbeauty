#include "BIBGame/BIBVideoScreen.h"

#include "BIBGame/BIBVisuals.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileMediaSource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FString MoviePath(const FString& Movie)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies") / (Movie + TEXT(".mp4")));
	}
}

ABIBVideoScreen::ABIBVideoScreen()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(RootComponent);
	Frame->SetStaticMesh(CubeFinder.Object);
	Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Frame->SetCastShadow(false);
	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetStaticMesh(PlaneFinder.Object);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panel->SetCastShadow(false);
	BackPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackPanel"));
	BackPanel->SetupAttachment(RootComponent);
	BackPanel->SetStaticMesh(PlaneFinder.Object);
	BackPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackPanel->SetCastShadow(false);
	BackPanel->SetVisibility(false);
	Sound = CreateDefaultSubobject<UMediaSoundComponent>(TEXT("Sound"));
	Sound->SetupAttachment(RootComponent);
}

bool ABIBVideoScreen::MovieExists(const FString& Movie)
{
	return FPaths::FileExists(MoviePath(Movie));
}

ABIBVideoScreen* ABIBVideoScreen::Spawn(UWorld* World, const FString& Movie, const FVector& Location, float Yaw, float WidthCm, bool bSound, bool bTwoSided)
{
	if (!World || !MovieExists(Movie))
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABIBVideoScreen* Screen = World->SpawnActor<ABIBVideoScreen>(ABIBVideoScreen::StaticClass(), Location, FRotator(0.f, Yaw, 0.f), Params);
	if (Screen)
	{
		Screen->Start(Movie, WidthCm, bSound, bTwoSided);
	}
	return Screen;
}

void ABIBVideoScreen::Start(const FString& InMovie, float WidthCm, bool bSound, bool bTwoSided)
{
	Movie = InMovie;
	const float Height = WidthCm * 9.f / 16.f;
	// A thin Neo Black frame (cube, 100 cm) and the picture just in front of it (plane faces +Z; turned to face along +X).
	Frame->SetRelativeScale3D(FVector(0.12, (WidthCm + 30.f) / 100.0, (Height + 30.f) / 100.0));
	if (UMaterialInstanceDynamic* FrameTint = BIBVisuals::MakeTint(Frame->GetMaterial(0), this))
	{
		FrameTint->SetVectorParameterValue(BIBVisuals::ColorParameter(), FLinearColor(0.02f, 0.025f, 0.04f));
		Frame->SetMaterial(0, FrameTint);
	}
	// The plane faces +Z; pitched -90 it faces the actor's forward (towards the viewer), 8 cm in front of the frame.
	Panel->SetRelativeLocation(FVector(8.0, 0.0, 0.0));
	// (and turned a quarter about its own normal so the video's rows run horizontally)
	Panel->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f).Quaternion() * FQuat(FVector::UpVector, -UE_HALF_PI));
	Panel->SetRelativeScale3D(FVector(WidthCm / 100.0, Height / 100.0, 1.0));
	if (bTwoSided)
	{
		// the back face shows the same picture, mirrored into place (turned half way round)
		BackPanel->SetVisibility(true);
		BackPanel->SetRelativeLocation(FVector(-8.0, 0.0, 0.0));
		BackPanel->SetRelativeRotation(FQuat(FVector::UpVector, UE_PI) * Panel->GetRelativeRotation().Quaternion());
		BackPanel->SetRelativeScale3D(Panel->GetRelativeScale3D());
	}

	// Another screen already playing this movie: share its player and picture (one decoder per movie).
	for (TActorIterator<ABIBVideoScreen> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->Movie == InMovie && It->Player && It->Material)
		{
			Material = It->Material;
			SharedFrom = *It;
			Panel->SetMaterial(0, Material);
			BackPanel->SetMaterial(0, Material);
			Sound->Deactivate();
			return;
		}
	}

	Player = NewObject<UMediaPlayer>(this);
	Player->SetLooping(true);
	Player->PlayOnOpen = true;
	Texture = NewObject<UMediaTexture>(this);
	Texture->SetMediaPlayer(Player);
	Texture->UpdateResource();
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BIB/Materials/M_BIBScreen.M_BIBScreen"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Base)
	{
		Material = UMaterialInstanceDynamic::Create(Base, this);
		Material->SetTextureParameterValue(TEXT("Screen"), Texture);
		Panel->SetMaterial(0, Material);
		BackPanel->SetMaterial(0, Material);
	}
	if (bSound)
	{
		Sound->SetMediaPlayer(Player);
		Sound->bOverrideAttenuation = true;
		Sound->AttenuationOverrides.bAttenuate = true;
		Sound->AttenuationOverrides.bSpatialize = true;
		Sound->AttenuationOverrides.FalloffDistance = 2200.f;
		Sound->AttenuationOverrides.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
		Sound->SetVolumeMultiplier(0.55f);   // background ads (the files are already 60% quieter: Tools/motion_movies.sh, owner 2026-10-09)
		Sound->Activate();
	}
	else
	{
		Sound->Deactivate();
	}
	UFileMediaSource* Source = NewObject<UFileMediaSource>(this);
	Source->SetFilePath(MoviePath(InMovie));
	const bool bOpened = Player->OpenSource(Source);
	UE_LOG(LogTemp, Display, TEXT("BIBVideoScreen: %s (material %s, open %s)"), *InMovie, Base ? TEXT("ok") : TEXT("MISSING"), bOpened ? TEXT("ok") : TEXT("FAILED"));
}

bool ABIBVideoScreen::IsPlaying() const
{
	if (SharedFrom.IsValid())
	{
		return SharedFrom->IsPlaying();
	}
	return Player && Player->IsPlaying();
}

void ABIBVideoScreen::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Player)
	{
		Player->Close();
	}
	Super::EndPlay(Reason);
}
