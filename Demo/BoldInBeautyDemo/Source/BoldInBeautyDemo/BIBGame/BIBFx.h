// Combat effects (owner request 2026-10-09, Notion "Dev notes": attacks "all feel identical, incredibly boring and lack
// artistic feel ... it should flow together and be high energy like zzz"). Short-lived glowing shapes that grow and fade:
// hit sparks, slash streaks cut at the trailer's -12 degree angle, shock rings, attack telegraph flashes (gold = dodge it,
// rose = unblockable) and stun bursts. Colours follow the BoldInBeauty style guide (Bold Rose, Electric Violet, Angel Cyan)
// mixed with each operator's own colour so every kit reads differently.
//
// Material: /Game/BIB/Materials/M_BIBFx (unlit, additive; "Color", "Intensity", "Opacity"), built by Tools/build_assets.py;
// falls back to the tint material (opaque) when it does not exist yet.
//
// Performance (combat-feel pass 2026-10-12): finished effects are recycled instead of destroyed (an actor, its mesh component and
// its material instance are created once and reused, so a fight allocates nothing per hit), and the number of simultaneous
// effects is capped (SetCap, from the Effects graphics group; BIBCombatFeel.h). An effect over the cap is dropped unless it is
// essential: an enemy's tell (telegraph, area warning) is never dropped.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIBFx.generated.h"

class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

namespace BIBFxColours
{
	inline const FLinearColor Rose(1.f, 0.24f, 0.55f);       // #FF3D8D
	inline const FLinearColor Violet(0.46f, 0.34f, 1.f);     // #7657FF
	inline const FLinearColor Cyan(0.39f, 0.91f, 1.f);       // #64E8FF
	inline const FLinearColor White(0.97f, 0.96f, 0.98f);    // #F7F4FA
	inline const FLinearColor Gold(1.f, 0.78f, 0.25f);
}

UENUM()
enum class EBIBFxShape : uint8
{
	Sphere,
	Ring,      // a flat, thin cylinder
	Slash,     // a long thin box
	Pillar     // a tall thin cylinder
};

UCLASS(NotPlaceable)
class BOLDINBEAUTYDEMO_API ABIBFx : public AActor
{
	GENERATED_BODY()

public:
	ABIBFx();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Spawns an effect that scales from StartSize to EndSize (cm) and fades out over Life seconds (real time, so hit stop
	 *  and slow motion do not freeze it). Returns nullptr when the effect was dropped by the cap; bEssential effects are never dropped. */
	static ABIBFx* Spawn(UWorld* World, EBIBFxShape Shape, const FVector& Location, const FRotator& Rotation, const FVector& StartSize,
		const FVector& EndSize, const FLinearColor& Colour, float Life, float Intensity = 6.f, bool bEssential = false);

	/** Maximum simultaneous non-essential effects. */
	static void SetCap(int32 Cap);
	/** Effects alive now (not counting recycled ones). */
	static int32 LiveCount();

	// ---- presets
	static void HitSpark(UWorld* World, const FVector& Location, const FLinearColor& Colour, float Power);
	static void SlashStreak(UWorld* World, const FVector& Location, const FVector& Direction, const FLinearColor& Colour, float Length, int32 ComboStep);
	static void ShockRing(UWorld* World, const FVector& Ground, float Radius, const FLinearColor& Colour, float Life = 0.35f);
	static void Telegraph(UWorld* World, const FVector& Location, bool bUnblockable);
	static void StunBurst(UWorld* World, const FVector& Location);
	static void AreaWarning(UWorld* World, const FVector& Ground, float Radius, float Seconds, const FLinearColor& Colour);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FVector StartScale = FVector::OneVector;
	FVector EndScale = FVector::OneVector;
	FLinearColor Colour = FLinearColor::White;
	float Intensity = 6.f;
	double BornAt = 0.0;
	float Life = 0.3f;
	bool bAdditive = false;
	bool bLive = false;

	/** Hides the effect and parks it in the pool (or destroys it when the pool is full). */
	void Retire();
};
