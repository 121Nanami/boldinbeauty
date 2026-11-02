// A place in the Ark the player can use (owner request 2026-10-07: the 2D lobby became the Ark). ABIBGameMode spawns one per
// station anchor that Tools/greybox_ark_hub.py put into L_Ark (a TargetPoint tagged "BIBStation" + the id from
// BIBRules/BIBArk.cpp). The actor is the station's live 3D presence on top of the greybox structure: a glowing floor ring where
// the player stands, a spinning beacon and a hologram name sign in the station's colour (none for the unmarked places), and a
// light. Walking into the ring and pressing F (or T) opens the station's counters in the Ark panel (BIBArkUI).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIBArkStation.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class BOLDINBEAUTYDEMO_API ABIBArkStation : public AActor
{
	GENERATED_BODY()

public:
	ABIBArkStation();

	virtual void Tick(float DeltaSeconds) override;

	/** Sets the station id (BIBArk.cpp) and builds its look; the actor's transform is the anchor (where the player stands, facing the station). */
	void Setup(const FString& InStationId);

	const FString& GetStationId() const { return StationId; }
	FString GetDisplayName() const { return DisplayName; }
	FLinearColor GetColour() const { return Colour; }
	/** Where a teleport puts the player's feet. */
	FVector GetStandLocation() const { return GetActorLocation(); }
	float GetStandYaw() const { return GetActorRotation().Yaw; }
	/** True when this location is close enough to use the station (inside the ring, give or take). */
	bool IsInReach(const FVector& Location) const;

	/** A short flash of the ring and beacon (arrival after a teleport, a summon in the factory). */
	void Pulse(const FLinearColor& FlashColour, float Seconds = 1.2f);

	static constexpr float ReachRadius = 650.f;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RingCore;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Sign;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> SubSign;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BeaconMaterial;

	FString StationId;
	FString DisplayName;
	FLinearColor Colour = FLinearColor::White;
	FLinearColor PulseColour = FLinearColor::White;
	float PulseLeft = 0.f;
	float PulseLength = 1.f;
	float Clock = 0.f;
	bool bUnmarked = false;
};
