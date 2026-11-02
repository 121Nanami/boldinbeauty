#include "BIBGame/BIBAnimeStyle.h"

#include "Components/BrushComponent.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "UnrealClient.h"

#include <atomic>

static TAutoConsoleVariable<float> CVarBIBVisualStepRate(
	TEXT("bib.VisualStepRate"),
	-1.f,
	TEXT("Anime style: visual updates per second. -1 = use the volume's VisualStepRate, 0 = smooth (no held frames)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBIBAnimeLook(
	TEXT("bib.Gfx.AnimeLook"),
	1,
	TEXT("Settings > Graphics > Anime look: 1 = the anime post-process and 12 fps hold where the game uses them, 0 = plain image (cheaper)."),
	ECVF_Default);

static std::atomic<int32> GBIBFreshFrames{0};
static std::atomic<int32> GBIBHeldFrames{0};
static std::atomic<int32> GBIBSkippedFrames{0};
static std::atomic<int32> GBIBAccentFrames{0};

static TAutoConsoleVariable<int32> CVarBIBImpactHold(
	TEXT("bib.Debug.ImpactHold"),
	0,
	TEXT("Debug / screenshots: 1 = keep the impact-frame look on."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBIBImpactFrames(
	TEXT("bib.ImpactFrames"),
	1,
	TEXT("Anime style: 1 = heavy moments draw one two-tone impact frame, 0 = accents only (no flash)."),
	ECVF_Default);   // held frames whose 3D scene was not rendered at all

// ================================================================================================ frame hold
// Game thread decides, once per rendered view family, whether this frame starts a new visual step. The render thread
// then either keeps a copy of the finished view (new step) or writes the kept copy back over it (held frame).
// Only real game viewports are touched: scene captures and editor views have no viewport or another world.
//
// Optimisation (2026-10-08, owner: "it runs at 3 fps"): a held frame is never shown, so it is not rendered at all.
// SetupViewFamily turns the family's Rendering show flag off (the scene renderer then skips the whole frame: no
// culling, shadows, Lumen, base pass or post-processing) and the kept copy is written straight to the viewport. With the
// default 12 steps a second the 3D scene is drawn 12 times a second instead of 60; the game, animation, physics, input and
// HUD still run every frame. bib.Gfx.SkipHeldFrames 0 renders every frame again (the old behaviour).
static TAutoConsoleVariable<int32> CVarBIBSkipHeldFrames(
	TEXT("bib.Gfx.SkipHeldFrames"),
	1,
	TEXT("Anime style: 1 = do not render frames that only show the held image (much cheaper), 0 = render every frame."),
	ECVF_Default);

class FBIBFrameStepExtension : public FWorldSceneViewExtension
{
public:
	FBIBFrameStepExtension(const FAutoRegister& AutoRegister, UWorld* InWorld, ABIBAnimeStyleVolume* InVolume)
		: FWorldSceneViewExtension(AutoRegister, InWorld)
		, Volume(InVolume)
	{
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
	{
		ABIBAnimeStyleVolume* Owner = Volume.Get();
		const int64 Step = Owner ? Owner->GetVisualStep() : -1;
		if (Owner && CVarBIBImpactHold.GetValueOnGameThread() != 0)
		{
			if (!Owner->bImpactParamOn)
			{
				Owner->SetImpactParam(true);
			}
		}
		else if (Owner && Step >= 0 && Step != LastStep && Owner->bImpactParamOn && Step != Owner->ImpactStep)
		{
			Owner->SetImpactParam(false);   // the impact frame lasted exactly one step
		}
		const FIntPoint Size = InViewFamily.RenderTarget ? InViewFamily.RenderTarget->GetSizeXY() : FIntPoint::ZeroValue;
		// A new step, the first frame after being inactive, or a resized view: take a fresh frame.
		bCapture = Step < 0 || Step != LastStep || GFrameCounter > LastFrame + 1 || Size != CapturedSize || !bHaveCapture;
		bSkip = !bCapture && CVarBIBSkipHeldFrames.GetValueOnGameThread() != 0;
		LastStep = Step;
		LastFrame = GFrameCounter;
		if (bCapture)
		{
			CapturedSize = Size;
			bHaveCapture = true;
		}
		if (bSkip)
		{
			InViewFamily.EngineShowFlags.SetRendering(false);
		}
	}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{
		TSharedRef<FBIBFrameStepExtension, ESPMode::ThreadSafe> Self = StaticCastSharedRef<FBIBFrameStepExtension>(AsShared());
		if (bSkip)
		{
			// The scene renderer will not run for this family (and so will not call PostRenderViewFamily_RenderThread):
			// put the kept image on the viewport now; the HUD and menus draw over it afterwards as usual.
			const FRenderTarget* RenderTarget = InViewFamily.RenderTarget;
			ENQUEUE_RENDER_COMMAND(BIBFrameStepRestore)([Self, RenderTarget](FRHICommandListImmediate& RHICmdList)
			{
				FRHITexture* TargetRHI = RenderTarget ? RenderTarget->GetRenderTargetTexture() : nullptr;
				if (!TargetRHI || !Self->Held.IsValid() || TargetRHI->GetDesc().NumSamples > 1 || Self->Held->GetDesc().Extent != TargetRHI->GetDesc().Extent
					|| Self->Held->GetDesc().Format != TargetRHI->GetDesc().Format)
				{
					return;
				}
				FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("BIB.FrameStep.Restore"));
				FRDGTextureRef Target = RegisterExternalTexture(GraphBuilder, TargetRHI, TEXT("BIB.FrameStep.Target"));
				AddCopyTexturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(Self->Held), Target);
				GraphBuilder.Execute();
				++GBIBHeldFrames;
				++GBIBSkippedFrames;
			});
			return;
		}
		const bool bCaptureNow = bCapture;
		ENQUEUE_RENDER_COMMAND(BIBFrameStepDecide)([Self, bCaptureNow](FRHICommandListImmediate&)
		{
			Self->bCaptureRT = bCaptureNow;
		});
	}

	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override
	{
		if (!InViewFamily.RenderTarget)
		{
			return;
		}
		FRHITexture* TargetRHI = InViewFamily.RenderTarget->GetRenderTargetTexture();
		if (!TargetRHI || TargetRHI->GetDesc().NumSamples > 1)
		{
			return;
		}
		FRDGTextureRef Target = RegisterExternalTexture(GraphBuilder, TargetRHI, TEXT("BIB.FrameStep.Target"));
		const FRDGTextureDesc& TargetDesc = Target->Desc;
		const bool bMatches = Held.IsValid() && Held->GetDesc().Extent == TargetDesc.Extent && Held->GetDesc().Format == TargetDesc.Format;
		if (bCaptureRT || !bMatches)
		{
			if (bMatches)
			{
				AddCopyTexturePass(GraphBuilder, Target, GraphBuilder.RegisterExternalTexture(Held));
			}
			else
			{
				const ETextureCreateFlags Flags = TexCreate_ShaderResource | (TargetDesc.Flags & TexCreate_SRGB);
				FRDGTextureRef HeldTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TargetDesc.Extent, TargetDesc.Format, FClearValueBinding::Black, Flags), TEXT("BIB.FrameStep.Held"));
				AddCopyTexturePass(GraphBuilder, Target, HeldTexture);
				GraphBuilder.QueueTextureExtraction(HeldTexture, &Held);
			}
			++GBIBFreshFrames;
			return;
		}
		AddCopyTexturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(Held), Target);
		++GBIBHeldFrames;
	}

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		const ABIBAnimeStyleVolume* Owner = Volume.Get();
		return Context.Viewport != nullptr && Owner && Owner->GetVisualStep() >= 0 && FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
	}

private:
	TWeakObjectPtr<ABIBAnimeStyleVolume> Volume;
	int64 LastStep = -1;            // game thread
	uint64 LastFrame = 0;           // game thread
	FIntPoint CapturedSize = FIntPoint::ZeroValue;   // game thread: viewport size of the last fresh frame
	bool bHaveCapture = false;      // game thread
	bool bCapture = true;           // game thread, this frame
	bool bSkip = false;             // game thread, this frame
	bool bCaptureRT = true;         // render thread
	TRefCountPtr<IPooledRenderTarget> Held;   // render thread
};

// ================================================================================================ volume
ABIBAnimeStyleVolume::ABIBAnimeStyleVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bUnbound = true;
	Priority = 10.f;
	StyleMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BIB/Materials/MI_BIBAnimeStyle.MI_BIBAnimeStyle")));
	if (GetBrushComponent())
	{
		GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	ApplyLookSettings();
}

void ABIBAnimeStyleVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyStyleMaterial();   // also shows the look in the editor viewport
}

// The dream filter (2026-10-11 rework): the world stays fully readable; the unease comes from muted, slightly cool colour
// and a faint lavender cast in the shadows. Native post-process settings only, every value static (no time input anywhere).
// Film grain stays 0 on purpose: Unreal's grain re-seeds every frame, which reads as flicker. Fringe is the lens model's
// static, edge-only fringe. Old values (legacy anime look) are listed in the report.
void ABIBAnimeStyleVolume::ApplyLookSettings()
{
	Settings = FPostProcessSettings();   // a version switch must not keep the other version's overrides
	FPostProcessSettings& S = Settings;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.f;
	S.bOverride_LensFlareIntensity = true;
	S.LensFlareIntensity = 0.f;
	S.bOverride_BloomIntensity = true;
	S.bOverride_BloomThreshold = true;
	S.bOverride_VignetteIntensity = true;
	S.bOverride_SceneFringeIntensity = true;
	if (bLegacyAnimeLook)
	{
		S.SceneFringeIntensity = 0.f;
		S.BloomIntensity = 0.25f;
		S.BloomThreshold = 2.5f;
		S.VignetteIntensity = 0.3f;
		return;
	}
	if (DreamLookVersion <= 1)
	{
		ApplyDreamV1();
	}
	else
	{
		ApplyDreamV2();
	}
}

// Dream filter v1 (2026-10-11, first rework): kept as the backup of the first dream look.
void ABIBAnimeStyleVolume::ApplyDreamV1()
{
	FPostProcessSettings& S = Settings;
	S.BloomIntensity = 0.25f;     // soft glow, high threshold: only real highlights bloom
	S.BloomThreshold = 2.5f;
	S.VignetteIntensity = 0.2f;   // static, light
	S.SceneFringeIntensity = 0.08f;

	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(0.85, 0.85, 0.85, 1.0);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(0.97, 0.97, 0.97, 1.0);   // a touch of haze without lifting the blacks into grey
	S.bOverride_WhiteTemp = true;
	S.WhiteTemp = 5300.f;                                 // cool of neutral (6500): takes the warm sand cast down
	S.bOverride_ColorGainShadows = true;
	S.ColorGainShadows = FVector4(0.95, 0.94, 1.0, 1.0);   // lavender in the shadows
	S.bOverride_ColorGainMidtones = true;
	S.ColorGainMidtones = FVector4(0.99, 0.985, 1.0, 1.0);
}

// Dream filter v2 (AAA grade, 2026-10-11). Raises the craft, not the intensity: every control below is a static native
// setting (no time input), the tone curve rolls highlights off like film instead of clipping them, local exposure keeps
// detail in both shadows and highlights, bloom is a wide faint glow, the grade is split-toned instead of one global cast.
void ABIBAnimeStyleVolume::ApplyDreamV2()
{
	FPostProcessSettings& S = Settings;
	// Soft glow: wide, faint, only real highlights (threshold above the diffuse white of the lit surfaces).
	S.BloomMethod = BM_SOG;
	S.bOverride_BloomMethod = true;
	S.BloomIntensity = 0.22f;
	S.BloomThreshold = 1.4f;
	S.bOverride_BloomSizeScale = true;
	S.BloomSizeScale = 5.f;
	S.bOverride_Bloom1Tint = true;   S.Bloom1Tint = FLinearColor(0.60f, 0.60f, 0.62f);
	S.bOverride_Bloom2Tint = true;   S.Bloom2Tint = FLinearColor(0.50f, 0.50f, 0.54f);
	S.bOverride_Bloom3Tint = true;   S.Bloom3Tint = FLinearColor(0.42f, 0.42f, 0.50f);
	S.bOverride_Bloom4Tint = true;   S.Bloom4Tint = FLinearColor(0.34f, 0.34f, 0.46f);
	S.bOverride_Bloom5Tint = true;   S.Bloom5Tint = FLinearColor(0.28f, 0.28f, 0.42f);
	S.bOverride_Bloom6Tint = true;   S.Bloom6Tint = FLinearColor(0.20f, 0.20f, 0.36f);
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.18f;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.06f;

	// Filmic tone curve: long soft shoulder (highlights roll off, nothing clips), gentle toe, no black crush.
	S.bOverride_FilmSlope = true;       S.FilmSlope = 0.86f;
	S.bOverride_FilmToe = true;         S.FilmToe = 0.50f;
	S.bOverride_FilmShoulder = true;    S.FilmShoulder = 0.30f;
	S.bOverride_FilmBlackClip = true;   S.FilmBlackClip = 0.f;
	S.bOverride_FilmWhiteClip = true;   S.FilmWhiteClip = 0.04f;
	S.bOverride_BlueCorrection = true;  S.BlueCorrection = 0.4f;   // keeps blue lights from turning purple-hot
	S.bOverride_ExpandGamut = true;     S.ExpandGamut = 0.8f;      // soft hue-preserving gamut handling

	// Local exposure: lifts shadow detail and holds highlight detail without a global contrast change.
	S.bOverride_LocalExposureHighlightContrastScale = true;   S.LocalExposureHighlightContrastScale = 0.85f;
	S.bOverride_LocalExposureShadowContrastScale = true;      S.LocalExposureShadowContrastScale = 0.9f;
	S.bOverride_LocalExposureDetailStrength = true;           S.LocalExposureDetailStrength = 1.25f;

	// One exposure for every lighting condition: a narrow auto-exposure range and slow adaptation (no pumping).
	S.bOverride_AutoExposureMinBrightness = true;   S.AutoExposureMinBrightness = 0.6f;
	S.bOverride_AutoExposureMaxBrightness = true;   S.AutoExposureMaxBrightness = 2.2f;
	S.bOverride_AutoExposureSpeedUp = true;         S.AutoExposureSpeedUp = 1.2f;
	S.bOverride_AutoExposureSpeedDown = true;       S.AutoExposureSpeedDown = 1.2f;

	// Depth: faint contact darkening, static.
	S.bOverride_AmbientOcclusionIntensity = true;   S.AmbientOcclusionIntensity = 0.3f;
	S.bOverride_AmbientOcclusionRadius = true;      S.AmbientOcclusionRadius = 120.f;

	// Grade: muted, slightly cool overall, split-toned. Shadows lavender and a little green-grey, highlights pale cream.
	S.bOverride_WhiteTemp = true;                   S.WhiteTemp = 5600.f;
	S.bOverride_ColorSaturation = true;             S.ColorSaturation = FVector4(0.88, 0.88, 0.88, 1.0);
	S.bOverride_ColorContrast = true;               S.ColorContrast = FVector4(0.98, 0.98, 0.98, 1.0);
	S.bOverride_ColorSaturationShadows = true;      S.ColorSaturationShadows = FVector4(0.9, 0.9, 0.9, 1.0);
	S.bOverride_ColorGainShadows = true;            S.ColorGainShadows = FVector4(0.95, 0.955, 1.0, 1.0);
	S.bOverride_ColorOffsetShadows = true;          S.ColorOffsetShadows = FVector4(0.004, 0.005, 0.010, 0.0);   // faint lifted lavender, no crush
	S.bOverride_ColorGainMidtones = true;           S.ColorGainMidtones = FVector4(0.99, 0.99, 1.0, 1.0);
	S.bOverride_ColorSaturationHighlights = true;   S.ColorSaturationHighlights = FVector4(0.85, 0.85, 0.85, 1.0);
	S.bOverride_ColorGainHighlights = true;         S.ColorGainHighlights = FVector4(1.0, 0.99, 0.965, 1.0);   // pale cream, not white
	S.bOverride_ColorCorrectionShadowsMax = true;   S.ColorCorrectionShadowsMax = 0.12f;
	S.bOverride_ColorCorrectionHighlightsMin = true; S.ColorCorrectionHighlightsMin = 0.75f;
}

void ABIBAnimeStyleVolume::ApplyStyleMaterial()
{
	ApplyLookSettings();
	if (!bLegacyAnimeLook)
	{
		// Dream filter: no material. Maps saved by the old build may carry it in the volume; take it out.
		Settings.WeightedBlendables.Array.RemoveAll([this](const FWeightedBlendable& B)
		{
			return B.Object && B.Object->GetPathName().Contains(TEXT("BIBAnimeStyle"));
		});
		return;
	}
	UMaterialInterface* Material = StyleMaterial.LoadSynchronous();
	if (!Material)
	{
		return;
	}
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		// In play the look runs through a dynamic instance, so impact frames can switch its ImpactFrame parameter.
		if (!StyleInstance)
		{
			StyleInstance = UMaterialInstanceDynamic::Create(Material, this);
		}
		Settings.WeightedBlendables.Array.RemoveAll([Material](const FWeightedBlendable& Blendable) { return Blendable.Object == Material; });
		Material = StyleInstance;
	}
	for (const FWeightedBlendable& Blendable : Settings.WeightedBlendables.Array)
	{
		if (Blendable.Object == Material)
		{
			return;
		}
	}
	Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Material));
}

void ABIBAnimeStyleVolume::BeginPlay()
{
	Super::BeginPlay();
	ApplyStyleMaterial();
	if (UWorld* World = GetWorld(); World && !IsRunningDedicatedServer() && bLegacyAnimeLook)
	{
		FrameStep = FSceneViewExtensions::NewExtension<FBIBFrameStepExtension>(World, this);
	}
}

void ABIBAnimeStyleVolume::EndPlay(const EEndPlayReason::Type Reason)
{
	FrameStep.Reset();
	Super::EndPlay(Reason);
}

void ABIBAnimeStyleVolume::SetStyleEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

void ABIBAnimeStyleVolume::SetImpactParam(bool bOn)
{
	bImpactParamOn = bOn;
	if (StyleInstance)
	{
		StyleInstance->SetScalarParameterValue(TEXT("ImpactFrame"), bOn ? 1.f : 0.f);
	}
}

void ABIBAnimeStyleVolume::RequestAccent(const UWorld* World, bool bImpact)
{
	ABIBAnimeStyleVolume* Volume = FindActive(World);
	if (!Volume || Volume->GetVisualStep() < 0)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - Volume->LastAccentReal < 1.0 / 30.0)
	{
		return;
	}
	Volume->LastAccentReal = Now;
	++Volume->AccentSteps;            // the step index changes now: the next rendered frame is a fresh one
	++GBIBAccentFrames;
	if (bImpact && CVarBIBImpactFrames.GetValueOnGameThread() != 0)
	{
		Volume->ImpactStep = Volume->GetVisualStep();
		Volume->SetImpactParam(true);
	}
}

void ABIBAnimeStyleVolume::RequestAccentFrame(const UObject* WorldContextObject, bool bImpact)
{
	RequestAccent(GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr, bImpact);
}

int32 ABIBAnimeStyleVolume::GetAccentFrames()
{
	return GBIBAccentFrames.load();
}

float ABIBAnimeStyleVolume::GetEffectiveStepRate() const
{
	const float Override = CVarBIBVisualStepRate.GetValueOnGameThread();
	if (Override >= 0.f)
	{
		return Override;
	}
	return (bLegacyAnimeLook && bStepVisuals) ? VisualStepRate : 0.f;
}

int64 ABIBAnimeStyleVolume::GetVisualStep() const
{
	const UWorld* World = GetWorld();
	const float Rate = GetEffectiveStepRate();
	if (!bEnabled || !World || Rate <= 0.f)
	{
		return -1;
	}
	return static_cast<int64>(FMath::FloorToDouble(World->GetRealTimeSeconds() * static_cast<double>(Rate))) + AccentSteps;
}

bool ABIBAnimeStyleVolume::IsLookAllowed()
{
	return CVarBIBAnimeLook.GetValueOnGameThread() != 0;
}

ABIBAnimeStyleVolume* ABIBAnimeStyleVolume::FindActive(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABIBAnimeStyleVolume> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->IsStyleEnabled())
		{
			return *It;
		}
	}
	return nullptr;
}

int64 ABIBAnimeStyleVolume::GetVisualStep(const UWorld* World)
{
	const ABIBAnimeStyleVolume* Volume = FindActive(World);
	return Volume ? Volume->GetVisualStep() : -1;
}

void ABIBAnimeStyleVolume::GetFrameStats(int32& FreshFrames, int32& HeldFrames)
{
	FlushRenderingCommands();
	FreshFrames = GBIBFreshFrames.load();
	HeldFrames = GBIBHeldFrames.load();
}

int32 ABIBAnimeStyleVolume::GetSkippedFrames()
{
	FlushRenderingCommands();
	return GBIBSkippedFrames.load();
}

void ABIBAnimeStyleVolume::ResetFrameStats()
{
	FlushRenderingCommands();
	GBIBFreshFrames = 0;
	GBIBHeldFrames = 0;
	GBIBSkippedFrames = 0;
	GBIBAccentFrames = 0;
}
