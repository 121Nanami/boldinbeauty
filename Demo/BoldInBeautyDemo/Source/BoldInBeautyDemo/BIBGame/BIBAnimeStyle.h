// The Boldinbeauty post-process filter. Two looks, switched per volume by bLegacyAnimeLook:
//   off (default since 2026-10-11): the dream-core filter — native volume settings only (muted, slightly cool grade with a
//      faint lavender cast in the shadows, soft late bloom, a light static vignette, minimal static edge fringe, no grain:
//      Unreal's film grain re-seeds every frame and reads as flicker). Nothing time-based: no camera shake, no UV wobble,
//      no animated distortion, no pulsing, and the frame hold is off so the presentation is smooth. Detail preservation
//      first: no stylisation material, no lines, no cel bands, no depth of field, no heavy bloom.
//   on: the legacy anime filter, kept as the switchable backup (owner 2026-10-07): the M_BIBAnimeStyle / MI_BIBAnimeStyle
//      material (thick ink lines, cel bands, cool shadows); the frame hold at VisualStepRate (12 visual updates a second
//      while the game runs at full rate) also needs bStepVisuals on. HUD / menus stay smooth in both looks: only the 3D
//      view is ever held.
//
// The legacy frame hold keeps whole frames: a scene view extension copies the finished view (after post-processing, so
// lines, bands and every particle are in it) on each step and shows that copy until the next step. The volume is placed in
// every gameplay map by the build scripts, and ABIBGameMode spawns one in any other world (the hub / main menu, code-built
// stages) so the look is global (owner 2026-10-16); only a plain world (Chapter 11) has none.
#pragma once

#include "CoreMinimal.h"
#include "Engine/PostProcessVolume.h"
#include "BIBAnimeStyle.generated.h"

class FBIBFrameStepExtension;
class UMaterialInterface;

UCLASS(meta = (DisplayName = "BIB Anime Style Volume"))
class BOLDINBEAUTYDEMO_API ABIBAnimeStyleVolume : public APostProcessVolume
{
	GENERATED_BODY()

public:
	ABIBAnimeStyleVolume();

	/** Visual updates per second. The game still simulates and animates at the full frame rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Anime Style", meta = (ClampMin = "1", ClampMax = "60"))
	float VisualStepRate = 12.f;

	/** Hold frames at VisualStepRate. Off (default since the 2026-10-11 dream rework) = smooth presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Anime Style")
	bool bStepVisuals = false;

	/**
	 * The old 2026-10-07 anime look (ink lines, cel bands, object boil, M_BIBAnimeStyle + 12 fps stepping when bStepVisuals).
	 * Off (default) = the dream filter: native post-process settings only, no material, nothing animated.
	 * Kept as the backup of the old filter; turn it on per volume.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Anime Style")
	bool bLegacyAnimeLook = false;

	/**
	 * Dream filter generation. 2 = the AAA grade (2026-10-11: filmic shoulder, local exposure, tuned bloom, split-toned
	 * grade). 1 = the first dream filter (plain saturation / white temp / shadow gain), kept as the backup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Anime Style", meta = (ClampMin = "1", ClampMax = "2"))
	int32 DreamLookVersion = 2;

	/** The stylisation material (lines, cel bands, shadow tint). Tune it through its material instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Anime Style")
	TSoftObjectPtr<UMaterialInterface> StyleMaterial;

	UFUNCTION(BlueprintCallable, Category = "BIB|Anime Style")
	void SetStyleEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category = "BIB|Anime Style")
	bool IsStyleEnabled() const { return bEnabled != 0; }

	/** Effective step rate after the bib.VisualStepRate console override; 0 = not stepping. */
	float GetEffectiveStepRate() const;

	/** Index of the visual step shown now (changes VisualStepRate times a second); -1 when nothing is held. */
	int64 GetVisualStep() const;

	/** The enabled anime volume of a world, if any. */
	static ABIBAnimeStyleVolume* FindActive(const UWorld* World);

	/** Step index for UI drawn over the world (damage numbers) so it moves with the held frames; -1 = draw every frame. */
	static int64 GetVisualStep(const UWorld* World);

	/** Test hook (Tools/verify_anime_style.py): frames shown fresh vs held since the last reset, all worlds. */
	UFUNCTION(BlueprintCallable, Category = "BIB|Anime Style")
	static void GetFrameStats(int32& FreshFrames, int32& HeldFrames);

	UFUNCTION(BlueprintCallable, Category = "BIB|Anime Style")
	static void ResetFrameStats();

	/** Test hook: held frames since the last reset whose 3D scene was not rendered at all (bib.Gfx.SkipHeldFrames). */
	UFUNCTION(BlueprintCallable, Category = "BIB|Anime Style")
	static int32 GetSkippedFrames();

	/** The "13th frame" (owner 2026-10-09): an action moment (a hit, a cast, a dodge, a sharp turn, an enemy wind-up) inserts
	 *  one extra fresh frame right away instead of waiting for the next 12 fps step, so actions read instantly and vividly
	 *  while the held-frame look stays. bImpact also draws that frame as an anime impact frame (two-tone ink flash) for heavy
	 *  moments. At most one accent every 1/30 s; impact frames are off with Settings > reduce motion (bib.ImpactFrames 0). */
	static void RequestAccent(const UWorld* World, bool bImpact = false);

	/** Test hook / Blueprint: request an accent (and impact) frame in the world of the context object. */
	UFUNCTION(BlueprintCallable, Category = "BIB|Anime Style", meta = (WorldContext = "WorldContextObject"))
	static void RequestAccentFrame(const UObject* WorldContextObject, bool bImpact);

	/** Test hook: accent frames inserted since the last ResetFrameStats. */
	UFUNCTION(BlueprintCallable, Category = "BIB|Anime Style")
	static int32 GetAccentFrames();

	/** Settings > Graphics > Anime look (bib.Gfx.AnimeLook): false = the player turned the look off for speed. */
	static bool IsLookAllowed();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void ApplyStyleMaterial();
	void ApplyLookSettings();
	void ApplyDreamV1();
	void ApplyDreamV2();

	TSharedPtr<FBIBFrameStepExtension, ESPMode::ThreadSafe> FrameStep;

	friend class FBIBFrameStepExtension;
	int64 AccentSteps = 0;          // extra steps inserted by accents (added to the clock's step index)
	double LastAccentReal = -1.0;
	int64 ImpactStep = -100;        // the step drawn as an impact frame
	bool bImpactParamOn = false;
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> StyleInstance;
	void SetImpactParam(bool bOn);
};
