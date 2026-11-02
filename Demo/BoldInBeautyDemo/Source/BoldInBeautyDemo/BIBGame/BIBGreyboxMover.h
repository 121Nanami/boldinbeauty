// A greybox "dynamic" piece for the chapter maps (Tools/greybox_chapters.py): a static mesh that moves back and forth
// along Offset (eased, one full cycle every Period seconds) and/or spins at RotationRate. Used for swinging and
// sliding platforms, crashing waves, conveyor loads, presses and the lighthouse beam. All numbers are greybox
// placeholders set by the build script; the player can stand on it (the mesh blocks and drags the character along).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIBGreyboxMover.generated.h"

class UStaticMeshComponent;

UCLASS(meta = (DisplayName = "BIB Greybox Mover"))
class BOLDINBEAUTYDEMO_API ABIBGreyboxMover : public AActor
{
	GENERATED_BODY()

public:
	ABIBGreyboxMover();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Greybox")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** World-space travel from the placed location to the far end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Greybox")
	FVector Offset = FVector::ZeroVector;

	/** Seconds for one there-and-back cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Greybox", meta = (ClampMin = "0.1"))
	float Period = 6.f;

	/** 0..1, shifts the cycle so neighbouring movers do not move in step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Greybox")
	float Phase = 0.f;

	/** Degrees per second, added in local space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Greybox")
	FRotator RotationRate = FRotator::ZeroRotator;

	/** Swing instead of spin: RotationRate becomes the swing amplitude in degrees, on the same Period. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Greybox")
	bool bSwing = false;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;
	float Elapsed = 0.f;
};
