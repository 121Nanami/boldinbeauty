// A coloured engine basic shape. Used for floors, walls, doors, the NPC and ability visuals.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIBBlock.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UENUM()
enum class EBIBShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone
};

UCLASS()
class BOLDINBEAUTYDEMO_API ABIBBlock : public AActor
{
	GENERATED_BODY()

public:
	ABIBBlock();

	virtual void Tick(float DeltaSeconds) override;

	// Spawns a block whose bounding box is SizeCm, centred on Location.
	static ABIBBlock* SpawnBlock(UWorld* World, EBIBShape InShape, const FVector& Location, const FVector& SizeCm,
		const FLinearColor& InColor, bool bInCollides, bool bInVisible = true);

	void SetColor(const FLinearColor& InColor);
	void SetSize(const FVector& SizeCm);
	void SetLabel(const FString& Text, const FLinearColor& InColor, float HeightAboveCentre, float WorldSize = 60.f);
	bool IsWall() const { return bIsWall; }
	void MarkAsWall() { bIsWall = true; }

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> LabelComp;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	bool bIsWall = false;
	bool bHasLabel = false;

	void ApplyShape(EBIBShape InShape);
};
