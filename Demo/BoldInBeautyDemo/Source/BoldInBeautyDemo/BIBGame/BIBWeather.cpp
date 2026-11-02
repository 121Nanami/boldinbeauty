#include "BIBGame/BIBWeather.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"
#include "TextureResource.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

// ================================================================================================ shared clock
// One clock and one weather for the whole game session: it keeps running across level loads and on the menus.
namespace BIBWeatherState
{
	struct FState
	{
		double Hour = 9.0;              // a new session starts on a bright morning
		double TimeScale = 1.0;
		double LastRealSeconds = -1.0;
		bool bAuto = true;
		EBIBRain Rain = EBIBRain::None;
		bool bThunder = false;
		EBIBWind Wind = EBIBWind::Light;
		double HoursUntilChange = 3.0;
		bool bSnap = false;             // jump the blended values to the targets on the next tick
		// Blended values (0..1)
		float RainAmount = 0.f;
		float WindAmount = 0.2f;
		float Cloud = 0.3f;
		float Storm = 0.f;
		float Wetness = 0.f;
		float Puddles = 0.f;
		bool bShowClock = true;
	};
	static FState G;
	static FRandomStream Rng(20261007);

	float RainTarget(EBIBRain Rain)
	{
		switch (Rain)
		{
		case EBIBRain::Light: return 0.3f;
		case EBIBRain::Medium: return 0.62f;
		case EBIBRain::Heavy: return 1.f;
		default: return 0.f;
		}
	}

	float WindTarget(EBIBWind Wind)
	{
		switch (Wind)
		{
		case EBIBWind::Light: return 0.3f;
		case EBIBWind::Medium: return 0.62f;
		case EBIBWind::Heavy: return 1.f;
		default: return 0.f;
		}
	}

	float CloudTarget()
	{
		if (G.bThunder)
		{
			return 1.f;
		}
		switch (G.Rain)
		{
		case EBIBRain::Light: return 0.72f;
		case EBIBRain::Medium: return 0.86f;
		case EBIBRain::Heavy: return 0.96f;
		default: return 0.32f;
		}
	}

	/** The scheduler (PROPOSED weights): a new weather every 2-5 game hours; rainy maps (Neo Tokyo) rain far more often. */
	void PickNext(bool bRainyMap)
	{
		const float Clear = bRainyMap ? 12.f : 46.f;
		const float Weights[5] = {Clear, 20.f, bRainyMap ? 26.f : 15.f, bRainyMap ? 26.f : 11.f, bRainyMap ? 16.f : 8.f};
		float Total = 0.f;
		for (float W : Weights)
		{
			Total += W;
		}
		float Pick = Rng.FRand() * Total;
		int32 Kind = 0;
		for (; Kind < 4; ++Kind)
		{
			if (Pick < Weights[Kind])
			{
				break;
			}
			Pick -= Weights[Kind];
		}
		G.bThunder = Kind == 4;
		G.Rain = Kind == 4 ? EBIBRain::Heavy : static_cast<EBIBRain>(Kind);
		const float WindPick = Rng.FRand() * 100.f;
		G.Wind = WindPick < 30.f ? EBIBWind::Calm : (WindPick < 65.f ? EBIBWind::Light : (WindPick < 88.f ? EBIBWind::Medium : EBIBWind::Heavy));
		if (G.bThunder && G.Wind < EBIBWind::Medium)
		{
			G.Wind = EBIBWind::Medium;   // storms are always windy
		}
		G.HoursUntilChange = Rng.FRandRange(2.f, 5.f);
	}

	/** Advances the clock and every blended value by real seconds. */
	void Advance(double RealSeconds, bool bRainyMap)
	{
		const double Hours = RealSeconds * G.TimeScale / ABIBWeatherSystem::RealSecondsPerGameHour;
		G.Hour = FMath::Fmod(G.Hour + Hours, 24.0);
		if (G.Hour < 0.0)
		{
			G.Hour += 24.0;
		}
		if (G.bAuto && G.TimeScale > 0.0)
		{
			G.HoursUntilChange -= Hours;
			if (G.HoursUntilChange <= 0.0)
			{
				PickNext(bRainyMap);
			}
		}
		const float Dt = static_cast<float>(RealSeconds);
		const float Blend = G.bSnap ? 1.f : FMath::Clamp(Dt / 25.f, 0.f, 1.f);   // ~25 s weather transitions
		G.bSnap = false;
		G.RainAmount = FMath::Lerp(G.RainAmount, RainTarget(G.Rain), Blend);
		G.WindAmount = FMath::Lerp(G.WindAmount, WindTarget(G.Wind), Blend);
		G.Cloud = FMath::Lerp(G.Cloud, CloudTarget(), Blend * 0.8f);
		G.Storm = FMath::Lerp(G.Storm, G.bThunder ? 1.f : 0.f, Blend);
		// Surfaces: wet in ~40 s of heavy rain; dry over ~3 game hours (faster in sun and wind). Puddles fill over ~2.5
		// game hours of heavy rain and stay for ~6-10 hours after it stops (owner: "water trails staying").
		const float GameHours = static_cast<float>(Hours);
		const bool bDaylight = G.Hour > 7.0 && G.Hour < 18.0;
		if (G.RainAmount > 0.05f)
		{
			G.Wetness = FMath::Min(1.f, G.Wetness + Dt * G.RainAmount / 40.f);
			G.Puddles = FMath::Min(1.f, G.Puddles + GameHours * FMath::Max(0.f, G.RainAmount - 0.15f) / 2.2f);
		}
		else
		{
			const float DryRate = (bDaylight ? 1.4f : 0.8f) * (1.f + 0.5f * G.WindAmount);
			G.Wetness = FMath::Max(0.f, G.Wetness - GameHours * DryRate / 3.f);
			G.Puddles = FMath::Max(0.f, G.Puddles - GameHours * DryRate / 8.f);
			G.Wetness = FMath::Max(G.Wetness, G.Puddles * 0.6f);   // puddle edges keep the ground damp
		}
	}

	FString RainName()
	{
		if (G.bThunder)
		{
			return TEXT("Thunderstorm");
		}
		switch (G.Rain)
		{
		case EBIBRain::Light: return TEXT("Light rain");
		case EBIBRain::Medium: return TEXT("Rain");
		case EBIBRain::Heavy: return TEXT("Heavy rain");
		default: return G.Cloud > 0.6f ? TEXT("Cloudy") : TEXT("Clear");
		}
	}

	FString WindName()
	{
		switch (G.Wind)
		{
		case EBIBWind::Light: return TEXT("light wind");
		case EBIBWind::Medium: return TEXT("windy");
		case EBIBWind::Heavy: return TEXT("strong wind");
		default: return TEXT("calm");
		}
	}
}

using BIBWeatherState::G;

// ================================================================================================ time-of-day palette
// Stylised anime palette (PROPOSED look numbers). Keys are game hours; values blend smoothly between keys.
namespace BIBWeatherPalette
{
	struct FPaletteKey
	{
		float Hour;
		FLinearColor Zenith;
		FLinearColor Horizon;
		FLinearColor Glow;       // sun / moon glow and the dawn / dusk band
		FLinearColor Light;      // key light colour
		float LightLux;          // key light intensity (sun by day, moon by night)
		float Sky;               // sky light intensity
		FLinearColor FogColour;
		float Exposure;          // EV bias: + brighter
		float Saturation;
		float Contrast;
		float Stars;
		FLinearColor CloudLit;
		FLinearColor CloudShade;
	};

	static const FPaletteKey Keys[] = {
		//  hour   zenith                          horizon                         glow                           light                        lux   sky   fog                             exp   sat   con   stars cloud lit                       cloud shade
		{0.0f,  {0.008f, 0.012f, 0.06f},  {0.04f, 0.05f, 0.16f}  , {0.10f, 0.12f, 0.30f}, {0.55f, 0.66f, 1.0f}, 0.45f, 0.55f, {0.012f, 0.016f, 0.045f}, -1.4f, 1.15f, 1.05f, 1.0f, {0.10f, 0.12f, 0.24f}, {0.020f, 0.024f, 0.06f}},
		{4.3f,  {0.006f, 0.010f, 0.050f}, {0.07f, 0.05f, 0.14f},   {0.25f, 0.12f, 0.22f}, {0.55f, 0.66f, 1.0f}, 0.35f, 0.55f, {0.025f, 0.022f, 0.06f},  -1.5f, 1.15f, 1.05f, 0.85f, {0.16f, 0.12f, 0.24f}, {0.035f, 0.03f, 0.08f}},
		{5.4f,  {0.04f, 0.06f, 0.20f},    {0.55f, 0.22f, 0.30f},   {1.10f, 0.40f, 0.35f}, {1.0f, 0.50f, 0.40f}, 0.6f,  0.7f,  {0.20f, 0.12f, 0.20f},   -1.0f, 1.25f, 1.08f, 0.35f, {1.0f, 0.55f, 0.55f},  {0.20f, 0.12f, 0.30f}},
		{6.3f,  {0.10f, 0.20f, 0.50f},    {1.20f, 0.55f, 0.30f},   {2.00f, 0.80f, 0.35f}, {1.0f, 0.62f, 0.38f}, 3.5f,  0.9f,  {0.55f, 0.38f, 0.32f},   -0.9f, 1.30f, 1.10f, 0.0f, {1.6f, 0.95f, 0.60f},  {0.38f, 0.30f, 0.50f}},
		{8.0f,  {0.07f, 0.30f, 1.00f},    {0.70f, 0.85f, 1.05f},   {1.20f, 1.00f, 0.75f}, {1.0f, 0.90f, 0.75f}, 8.0f,  1.45f,  {0.55f, 0.68f, 0.95f},   -1.3f, 1.35f, 1.10f, 0.0f, {1.9f, 1.85f, 1.80f},  {0.52f, 0.62f, 0.95f}},
		{12.0f, {0.03f, 0.26f, 1.05f},    {0.55f, 0.80f, 1.10f},   {1.10f, 1.05f, 0.95f}, {1.0f, 0.97f, 0.92f}, 10.f,  1.55f,  {0.52f, 0.70f, 1.00f},   -1.6f, 1.35f, 1.12f, 0.0f, {2.0f, 2.0f, 2.0f},    {0.55f, 0.66f, 1.00f}},
		{16.0f, {0.05f, 0.24f, 0.95f},    {0.75f, 0.80f, 0.95f},   {1.30f, 1.05f, 0.75f}, {1.0f, 0.86f, 0.62f}, 8.0f,  1.45f,  {0.62f, 0.66f, 0.88f},   -1.4f, 1.35f, 1.10f, 0.0f, {2.0f, 1.80f, 1.55f},  {0.55f, 0.58f, 0.92f}},
		{17.6f, {0.10f, 0.14f, 0.50f},    {1.35f, 0.45f, 0.22f},   {2.20f, 0.65f, 0.25f}, {1.0f, 0.50f, 0.25f}, 3.2f,  0.9f,  {0.60f, 0.32f, 0.30f},   -1.0f, 1.35f, 1.12f, 0.0f, {2.0f, 0.85f, 0.45f},  {0.42f, 0.24f, 0.48f}},
		{18.6f, {0.05f, 0.05f, 0.22f},    {0.70f, 0.20f, 0.30f},   {1.20f, 0.30f, 0.35f}, {0.9f, 0.40f, 0.45f}, 0.5f,  0.65f, {0.22f, 0.10f, 0.20f},   -1.0f, 1.30f, 1.08f, 0.25f, {1.0f, 0.40f, 0.50f},  {0.16f, 0.08f, 0.24f}},
		{19.8f, {0.010f, 0.012f, 0.07f},  {0.10f, 0.06f, 0.17f},   {0.25f, 0.15f, 0.35f}, {0.55f, 0.66f, 1.0f}, 0.35f, 0.55f, {0.04f, 0.03f, 0.08f},   -1.5f, 1.15f, 1.05f, 0.8f, {0.18f, 0.14f, 0.28f}, {0.03f, 0.03f, 0.08f}},
		{24.0f, {0.008f, 0.012f, 0.06f},  {0.04f, 0.05f, 0.16f}  , {0.10f, 0.12f, 0.30f}, {0.55f, 0.66f, 1.0f}, 0.45f, 0.55f, {0.012f, 0.016f, 0.045f}, -1.4f, 1.15f, 1.05f, 1.0f, {0.10f, 0.12f, 0.24f}, {0.020f, 0.024f, 0.06f}},
	};

	FPaletteKey Sample(float Hour)
	{
		const int32 Count = UE_ARRAY_COUNT(Keys);
		int32 Index = 0;
		while (Index < Count - 2 && Hour >= Keys[Index + 1].Hour)
		{
			++Index;
		}
		const FPaletteKey& A = Keys[Index];
		const FPaletteKey& B = Keys[Index + 1];
		const float T = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Hour - A.Hour) / FMath::Max(0.01f, B.Hour - A.Hour), 0.f, 1.f));
		auto L = [T](const FLinearColor& X, const FLinearColor& Y) { return FMath::Lerp(X, Y, T); };
		auto F = [T](float X, float Y) { return FMath::Lerp(X, Y, T); };
		FPaletteKey Out;
		Out.Hour = Hour;
		Out.Zenith = L(A.Zenith, B.Zenith);
		Out.Horizon = L(A.Horizon, B.Horizon);
		Out.Glow = L(A.Glow, B.Glow);
		Out.Light = L(A.Light, B.Light);
		Out.LightLux = F(A.LightLux, B.LightLux);
		Out.Sky = F(A.Sky, B.Sky);
		Out.FogColour = L(A.FogColour, B.FogColour);
		Out.Exposure = F(A.Exposure, B.Exposure);
		Out.Saturation = F(A.Saturation, B.Saturation);
		Out.Contrast = F(A.Contrast, B.Contrast);
		Out.Stars = F(A.Stars, B.Stars);
		Out.CloudLit = L(A.CloudLit, B.CloudLit);
		Out.CloudShade = L(A.CloudShade, B.CloudShade);
		return Out;
	}

	/** Direction towards the sun: rises in the east (+Y) at 06:00, highest (55 degrees) at noon in the south, sets at 18:00. */
	FVector SunDirection(double Hour)
	{
		const double Angle = (Hour - 6.0) / 12.0 * PI;
		const double Tilt = FMath::DegreesToRadians(35.0);
		const FVector East(0.0, 1.0, 0.0);
		const FVector Arc = FVector(-FMath::Sin(Tilt), 0.0, FMath::Cos(Tilt));
		return (East * FMath::Cos(Angle) + Arc * FMath::Sin(Angle)).GetSafeNormal();
	}

	/** The moon runs roughly opposite the sun, a little higher in the sky, so nights always have a moon. */
	FVector MoonDirection(double Hour)
	{
		const double Angle = (Hour - 18.5) / 12.0 * PI;
		const double Tilt = FMath::DegreesToRadians(25.0);
		const FVector East(0.0, 0.92, 0.39);
		const FVector Arc = FVector(-FMath::Sin(Tilt), -0.2, FMath::Cos(Tilt)).GetSafeNormal();
		return (East * FMath::Cos(Angle) + Arc * FMath::Sin(Angle)).GetSafeNormal();
	}
}

// ================================================================================================ console
namespace
{
	ABIBWeatherSystem* AnyActive()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->IsGameWorld())
			{
				if (ABIBWeatherSystem* Found = ABIBWeatherSystem::FindActive(World))
				{
					return Found;
				}
			}
		}
		return nullptr;
	}

	int32 ArgInt(const TArray<FString>& Args, int32 Default)
	{
		return Args.Num() > 0 ? FCString::Atoi(*Args[0]) : Default;
	}

	FAutoConsoleCommand CmdHour(TEXT("bib.Weather.Hour"), TEXT("Sets the game clock (0-24, e.g. 18.5)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				ABIBWeatherSystem::SetHour(FCString::Atof(*Args[0]));
			}
		}));
	FAutoConsoleCommand CmdRain(TEXT("bib.Weather.Rain"), TEXT("Rain 0 none, 1 light, 2 medium, 3 heavy (stops the automatic weather)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ABIBWeatherSystem::SetWeather(static_cast<EBIBRain>(FMath::Clamp(ArgInt(Args, 3), 0, 3)), G.bThunder && ArgInt(Args, 3) > 0, G.Wind, false);
		}));
	FAutoConsoleCommand CmdThunder(TEXT("bib.Weather.Thunder"), TEXT("1 = thunderstorm (heavy rain, lightning), 0 = no thunder."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const bool bOn = ArgInt(Args, 1) != 0;
			ABIBWeatherSystem::SetWeather(bOn ? EBIBRain::Heavy : G.Rain, bOn, bOn ? FMath::Max(G.Wind, EBIBWind::Medium) : G.Wind, false);
		}));
	FAutoConsoleCommand CmdWind(TEXT("bib.Weather.Wind"), TEXT("Wind 0 calm, 1 light, 2 medium, 3 heavy."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ABIBWeatherSystem::SetWeather(G.Rain, G.bThunder, static_cast<EBIBWind>(FMath::Clamp(ArgInt(Args, 2), 0, 3)), false);
		}));
	FAutoConsoleCommand CmdAuto(TEXT("bib.Weather.Auto"), TEXT("1 = weather changes by itself (default), 0 = keep the current weather."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { ABIBWeatherSystem::SetAutoWeather(ArgInt(Args, 1) != 0); }));
	FAutoConsoleCommand CmdScale(TEXT("bib.Weather.TimeScale"), TEXT("Clock speed (1 = 24-minute day, 0 = frozen, 60 = an hour a second)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ABIBWeatherSystem::SetTimeScale(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.f);
		}));
	FAutoConsoleCommand CmdStrike(TEXT("bib.Weather.Strike"), TEXT("A lightning strike now."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (ABIBWeatherSystem* Weather = AnyActive())
			{
				Weather->StrikeLightning();
			}
		}));
	FAutoConsoleCommand CmdClock(TEXT("bib.Weather.Clock"), TEXT("1 = show the clock / weather readout, 0 = hide it."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { G.bShowClock = ArgInt(Args, 1) != 0; }));
	FAutoConsoleCommand CmdReport(TEXT("bib.Weather.Report"), TEXT("Logs the weather state."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const ABIBWeatherSystem* Weather = AnyActive();
			UE_LOG(LogTemp, Display, TEXT("[BIBWeather] %s"), Weather ? *Weather->GetReport() : *ABIBWeatherSystem::GetClockText());
		}));

	TAutoConsoleVariable<float> CVarExposureOffset(TEXT("bib.Weather.ExposureOffset"), 0.f, TEXT("Look tuning: added to the time-of-day exposure bias (EV, + brighter)."));
	TAutoConsoleVariable<int32> CVarWeatherDetail(TEXT("bib.Gfx.WeatherDetail"), 2,
		TEXT("Settings > Graphics > Weather effects: 0 = off (sky, light, clock and sound only), 1 = low (rain sheets, no splashes, ")
		TEXT("wet decals or puddles), 2 = full."));
	TAutoConsoleVariable<float> CVarLightScale(TEXT("bib.Weather.LightScale"), 1.f, TEXT("Look tuning: multiplies the sun / moon and sky light."));

	const TCHAR* const MaterialRoot = TEXT("/Game/Weather/");
	constexpr float SkyRadiusScale = 30000.f;      // engine sphere (50 cm) -> 15 km: beyond the anime lines' sky depth (10 km)
	constexpr float ShelterWidth = 16000.f;        // the overhead rain-shelter capture covers 160 m around the camera
	constexpr float ShelterAbove = 60000.f;        // captured from 600 m above the camera
	constexpr int32 ShelterResolution = 512;
	constexpr float DecalHalf = 6000.f;            // wetness decals cover 120 m around the camera
}

// ================================================================================================ construction
ABIBWeatherSystem::ABIBWeatherSystem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera moved this frame
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetCastShadows(true);
	KeyLight->SetIntensity(8.f);

	FlashLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("LightningFlash"));
	FlashLight->SetupAttachment(Root);
	FlashLight->SetMobility(EComponentMobility::Movable);
	FlashLight->SetCastShadows(false);
	FlashLight->SetIntensity(0.f);
	FlashLight->SetLightColor(FLinearColor(0.75f, 0.82f, 1.f));

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = SLS_CapturedScene;
	SkyLight->bRealTimeCapture = true;
	SkyLight->bLowerHemisphereIsBlack = false;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetVolumetricFog(true);
	Fog->SetFogCutoffDistance(1200000.f);   // never fog the sky dome
	Fog->SetVolumetricFogScatteringDistribution(0.55f);

	SkyDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyDome"));
	SkyDome->SetupAttachment(Root);
	SkyDome->SetUsingAbsoluteScale(true);
	SkyDome->SetWorldScale3D(FVector(SkyRadiusScale));
	SkyDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyDome->SetCastShadow(false);
	SkyDome->bReceivesDecals = false;
	SkyDome->bAffectDistanceFieldLighting = false;
	SkyDome->bAffectDynamicIndirectLighting = false;
	SkyDome->bVisibleInRayTracing = false;
	SkyDome->SetGenerateOverlapEvents(false);

	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(Root);
	Grade->bUnbound = true;
	Grade->Priority = 5.f;   // below the anime volume (10), which only sets its own fields

	FollowRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FollowCamera"));
	FollowRoot->SetupAttachment(Root);
	FollowRoot->SetUsingAbsoluteLocation(true);
	FollowRoot->SetUsingAbsoluteRotation(true);

	RainRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RainRoot"));
	RainRoot->SetupAttachment(FollowRoot);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UProceduralMeshComponent* Layer = CreateDefaultSubobject<UProceduralMeshComponent>(*FString::Printf(TEXT("RainLayer%d"), Index));
		Layer->SetupAttachment(RainRoot);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->bReceivesDecals = false;
		Layer->bAffectDistanceFieldLighting = false;
		Layer->bVisibleInRayTracing = false;
		RainLayers.Add(Layer);
	}

	Particles = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Particles"));
	Particles->SetupAttachment(Root);
	Particles->SetUsingAbsoluteLocation(true);
	Particles->SetUsingAbsoluteRotation(true);
	Particles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Particles->SetCastShadow(false);
	Particles->bReceivesDecals = false;
	Particles->bVisibleInRayTracing = false;

	auto MakeDecal = [this](const TCHAR* Name, int32 Sort)
	{
		UDecalComponent* Decal = CreateDefaultSubobject<UDecalComponent>(Name);
		Decal->SetupAttachment(FollowRoot);
		Decal->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));   // project straight down
		Decal->DecalSize = FVector(4000.0, DecalHalf, DecalHalf);  // 80 m tall, 120 m square
		Decal->SetFadeScreenSize(0.f);
		Decal->SortOrder = Sort;
		return Decal;
	};
	WetDarken = MakeDecal(TEXT("WetDarken"), 0);
	WetGloss = MakeDecal(TEXT("WetGloss"), 1);
	Puddles = MakeDecal(TEXT("Puddles"), 2);

	ShelterCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ShelterCapture"));
	ShelterCapture->SetupAttachment(Root);
	ShelterCapture->SetUsingAbsoluteLocation(true);
	ShelterCapture->SetUsingAbsoluteRotation(true);
	ShelterCapture->SetWorldRotation(FRotator(-90.0, 0.0, 0.0));
	ShelterCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	ShelterCapture->OrthoWidth = ShelterWidth;
	ShelterCapture->bAutoCalculateOrthoPlanes = false;
	ShelterCapture->bUpdateOrthoPlanes = false;
	ShelterCapture->CaptureSource = SCS_SceneDepth;
	ShelterCapture->bCaptureEveryFrame = false;
	ShelterCapture->bCaptureOnMovement = false;
	ShelterCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	// Only solid, static-ish geometry shelters from rain: no characters, particles, translucency or lighting work.
	FEngineShowFlags& Flags = ShelterCapture->ShowFlags;
	Flags.SetSkeletalMeshes(false);
	Flags.SetParticles(false);
	Flags.SetTranslucency(false);
	Flags.SetFog(false);
	Flags.SetVolumetricFog(false);
	Flags.SetAtmosphere(false);
	Flags.SetDecals(false);
	Flags.SetDynamicShadows(false);
	Flags.SetGlobalIllumination(false);
	Flags.SetLumenGlobalIllumination(false);
	Flags.SetLumenReflections(false);
	Flags.SetPostProcessing(false);
	Flags.SetTextRender(false);
	Flags.SetBloom(false);
	Flags.SetAmbientOcclusion(false);
	Flags.SetScreenSpaceReflections(false);
	Flags.SetLighting(false);

	auto MakeAudio = [this](const TCHAR* Name)
	{
		UAudioComponent* Audio = CreateDefaultSubobject<UAudioComponent>(Name);
		Audio->SetupAttachment(Root);
		Audio->bAutoActivate = false;
		Audio->bAllowSpatialization = false;
		Audio->bIsUISound = false;
		return Audio;
	};
	RainAudio = MakeAudio(TEXT("RainAudio"));
	WindAudio = MakeAudio(TEXT("WindAudio"));
	ThunderAudio = MakeAudio(TEXT("ThunderAudio"));

	Root->SetVisibility(false, true);   // off until the game mode enables it
}

// ================================================================================================ lifecycle
UMaterialInstanceDynamic* ABIBWeatherSystem::MakeMid(const TCHAR* Name, bool bUsesShelter)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s%s.%s"), MaterialRoot, Name, Name));
	if (!Base)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BIBWeather] missing %s%s - run Tools/build_weather.sh"), MaterialRoot, Name);
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
	if (bUsesShelter)
	{
		ShelterUsers.Add(Mid);
	}
	return Mid;
}

void ABIBWeatherSystem::LoadAssets()
{
	if (bAssetsLoaded)
	{
		return;
	}
	bAssetsLoaded = true;
	Collection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Weather/MPC_BIBWeather.MPC_BIBWeather"));
	if (!Collection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BIBWeather] /Game/Weather is not built - lighting and clock only (run Tools/build_weather.sh)"));
	}
	if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		SkyDome->SetStaticMesh(Sphere);
	}
	if (UMaterialInstanceDynamic* Sky = MakeMid(TEXT("M_BIBSky"), false))
	{
		SkyDome->SetMaterial(0, Sky);
	}
	else
	{
		SkyDome->SetVisibility(false);
	}

	ShelterTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, ShelterResolution, ShelterResolution, RTF_R32f, FLinearColor(1e8f, 0, 0, 0), false);
	ShelterCapture->TextureTarget = ShelterTarget;
	ShelterCapture->HiddenActors.Add(this);

	BuildRainLayers();
	if (UMaterialInstanceDynamic* Gloss = MakeMid(TEXT("M_BIBWetGloss"), true))
	{
		WetGloss->SetDecalMaterial(Gloss);
	}
	if (UMaterialInstanceDynamic* Darken = MakeMid(TEXT("M_BIBWetDarken"), true))
	{
		WetDarken->SetDecalMaterial(Darken);
	}
	if (UMaterialInstanceDynamic* Puddle = MakeMid(TEXT("M_BIBPuddle"), true))
	{
		Puddles->SetDecalMaterial(Puddle);
	}
	for (UMaterialInstanceDynamic* Mid : ShelterUsers)
	{
		Mid->SetTextureParameterValue(TEXT("ShelterDepth"), ShelterTarget);
	}

	// Particle sections: 0 splashes, 1 wind streaks, 2 lightning.
	const TCHAR* const ParticleMaterials[3] = {TEXT("M_BIBSplash"), TEXT("M_BIBWindStreak"), TEXT("M_BIBLightning")};
	for (int32 Section = 0; Section < 3; ++Section)
	{
		if (UMaterialInstanceDynamic* Mid = MakeMid(ParticleMaterials[Section], false))
		{
			Particles->SetMaterial(Section, Mid);
		}
	}

	auto LoadSound = [](const TCHAR* Name) { return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Weather/Audio/%s.%s"), Name, Name)); };
	RainAudio->SetSound(LoadSound(TEXT("S_RainLoop")));
	WindAudio->SetSound(LoadSound(TEXT("S_WindLoop")));
	for (const TCHAR* Name : {TEXT("S_Thunder1"), TEXT("S_Thunder2"), TEXT("S_Thunder3")})
	{
		if (USoundBase* Sound = LoadSound(Name))
		{
			ThunderSounds.Add(Sound);
		}
	}
}

void ABIBWeatherSystem::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	// Maps designed for rain (Neo Tokyo: the greybox's rain placeholder) get a rainier forecast.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("RainPlaceholder")))
		{
			bRainyMap = true;
			break;
		}
	}
	LoadAssets();
	if (G.LastRealSeconds < 0.0)
	{
		G.LastRealSeconds = FPlatformTime::Seconds();
		if (bRainyMap)
		{
			G.Rain = EBIBRain::Heavy;   // Neo Tokyo's look: heavy rain and fog (owner, 2026-10-07)
			G.Wind = EBIBWind::Light;
			G.HoursUntilChange = 4.0;
			G.bSnap = true;
		}
	}
}

void ABIBWeatherSystem::EndPlay(const EEndPlayReason::Type Reason)
{
	ShowClock(false);
	RestoreLevelLighting();
	Super::EndPlay(Reason);
}

ABIBWeatherSystem* ABIBWeatherSystem::FindActive(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABIBWeatherSystem> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->IsWeatherEnabled())
		{
			return *It;
		}
	}
	return nullptr;
}

void ABIBWeatherSystem::TakeOverLevelLighting()
{
	RestoreLevelLighting();
	float LevelFog = -1.f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It == this)
		{
			continue;
		}
		TInlineComponentArray<USceneComponent*> Components(*It);
		for (USceneComponent* Component : Components)
		{
			const bool bLighting = Component->IsA<UDirectionalLightComponent>() || Component->IsA<USkyLightComponent>() || Component->IsA<USkyAtmosphereComponent>()
				|| Component->IsA<UExponentialHeightFogComponent>() || Component->IsA<UVolumetricCloudComponent>();
			if (!bLighting || !Component->IsVisible())
			{
				continue;
			}
			if (const UExponentialHeightFogComponent* LevelFogComponent = Cast<UExponentialHeightFogComponent>(Component))
			{
				LevelFog = FMath::Max(LevelFog, LevelFogComponent->FogDensity);
			}
			Component->SetVisibility(false);
			HiddenLevelLighting.Add(Component);
		}
	}
	// Default fog density here is 0.02; a map built with heavy fog (Neo Tokyo 0.12) keeps up to 4x of it.
	MapFogScale = LevelFog > 0.f ? FMath::Clamp(LevelFog / 0.02f, 1.f, 4.f) : 1.f;
}

void ABIBWeatherSystem::RestoreLevelLighting()
{
	for (const TWeakObjectPtr<USceneComponent>& Component : HiddenLevelLighting)
	{
		if (Component.IsValid())
		{
			Component->SetVisibility(true);
		}
	}
	HiddenLevelLighting.Reset();
}

void ABIBWeatherSystem::SetWeatherEnabled(bool bInEnabled)
{
	if (bActive == bInEnabled)
	{
		return;
	}
	bActive = bInEnabled;
	Root->SetVisibility(bActive, true);
	if (bActive)
	{
		LoadAssets();
		TakeOverLevelLighting();
		if (!SkyDome->GetMaterial(0))
		{
			SkyDome->SetVisibility(false);
		}
		G.LastRealSeconds = G.LastRealSeconds < 0.0 ? FPlatformTime::Seconds() : G.LastRealSeconds;
		UpdateCamera();
		UpdateShelter(true);
		SkyLight->RecaptureSky();
		if (RainAudio->Sound)
		{
			RainAudio->Play();
		}
		if (WindAudio->Sound)
		{
			WindAudio->Play();
		}
	}
	else
	{
		RestoreLevelLighting();
		RainAudio->Stop();
		WindAudio->Stop();
		ThunderAudio->Stop();
		Splashes.Reset();
		Streaks.Reset();
		Bolts.Reset();
		Particles->ClearAllMeshSections();
	}
	ShowClock(bActive && G.bShowClock);
}

// ================================================================================================ static API
float ABIBWeatherSystem::GetHour()
{
	return static_cast<float>(G.Hour);
}

void ABIBWeatherSystem::SetHour(float Hour)
{
	G.Hour = FMath::Fmod(FMath::Max(0.f, Hour), 24.f);
}

void ABIBWeatherSystem::SetWeather(EBIBRain Rain, bool bThunder, EBIBWind Wind, bool bInstant)
{
	G.Rain = bThunder ? EBIBRain::Heavy : Rain;
	G.bThunder = bThunder;
	G.Wind = Wind;
	G.bAuto = false;
	G.bSnap = G.bSnap || bInstant;
}

void ABIBWeatherSystem::SetAutoWeather(bool bAuto)
{
	G.bAuto = bAuto;
}

void ABIBWeatherSystem::SetTimeScale(float Scale)
{
	G.TimeScale = FMath::Max(0.0, static_cast<double>(Scale));
}

void ABIBWeatherSystem::SetSurfaceWater(float Wetness, float InPuddles)
{
	G.Wetness = FMath::Clamp(Wetness, 0.f, 1.f);
	G.Puddles = FMath::Clamp(InPuddles, 0.f, 1.f);
}

FString ABIBWeatherSystem::GetPhaseName()
{
	const double H = G.Hour;
	if (H >= 4.5 && H < 7.0)
	{
		return TEXT("Dawn");
	}
	if (H >= 7.0 && H < 17.3)
	{
		return TEXT("Day");
	}
	if (H >= 17.3 && H < 19.8)
	{
		return TEXT("Dusk");
	}
	return TEXT("Night");
}

FString ABIBWeatherSystem::GetClockText()
{
	const int32 Minutes = static_cast<int32>(G.Hour * 60.0) % (24 * 60);
	return FString::Printf(TEXT("%02d:%02d  %s  |  %s, %s"), Minutes / 60, Minutes % 60, *GetPhaseName(), *BIBWeatherState::RainName(), *BIBWeatherState::WindName());
}

FString ABIBWeatherSystem::GetReport() const
{
	const BIBWeatherPalette::FPaletteKey Key = BIBWeatherPalette::Sample(static_cast<float>(G.Hour));
	int32 Hidden = 0;
	for (const TWeakObjectPtr<USceneComponent>& Component : HiddenLevelLighting)
	{
		Hidden += Component.IsValid() ? 1 : 0;
	}
	return FString::Printf(
		TEXT("enabled=%d hour=%.3f phase=%s rain=%d thunder=%d wind=%d auto=%d rainAmount=%.3f windAmount=%.3f cloud=%.3f wetness=%.3f puddles=%.3f ")
		TEXT("keyLux=%.3f keyPitch=%.1f skyLight=%.3f exposure=%.2f fog=%.4f hiddenLevelLighting=%d shelterValid=%d splashes=%d splashesSpawned=%d streaks=%d bolts=%d strikes=%d ")
		TEXT("flash=%.3f assets=%d rainyMap=%d rainAudio=%d windAudio=%d skyMaterial=%d"),
		bActive ? 1 : 0, G.Hour, *GetPhaseName(), static_cast<int32>(G.Rain), G.bThunder ? 1 : 0, static_cast<int32>(G.Wind), G.bAuto ? 1 : 0,
		G.RainAmount, G.WindAmount, G.Cloud, G.Wetness, G.Puddles,
		KeyLight->Intensity, KeyLight->GetComponentRotation().Pitch, SkyLight->Intensity, Key.Exposure, Fog->FogDensity, Hidden, bShelterValid ? 1 : 0,
		Splashes.Num(), SplashesSpawned, Streaks.Num(), Bolts.Num(), StrikesSeen, Flash, Collection ? 1 : 0, bRainyMap ? 1 : 0,
		RainAudio->IsPlaying() ? 1 : 0, WindAudio->IsPlaying() ? 1 : 0, SkyDome->GetMaterial(0) ? 1 : 0);
}

// ================================================================================================ parameters
void ABIBWeatherSystem::SetScalar(FName Name, float Value) const
{
	if (Collection && GetWorld())
	{
		if (UMaterialParameterCollectionInstance* Instance = GetWorld()->GetParameterCollectionInstance(Collection))
		{
			Instance->SetScalarParameterValue(Name, Value);
		}
	}
}

void ABIBWeatherSystem::SetVector(FName Name, const FLinearColor& Value) const
{
	if (Collection && GetWorld())
	{
		if (UMaterialParameterCollectionInstance* Instance = GetWorld()->GetParameterCollectionInstance(Collection))
		{
			Instance->SetVectorParameterValue(Name, Value);
		}
	}
}

// ================================================================================================ tick
void ABIBWeatherSystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}
	// The clock follows real time (so it also ran while the menus or a loading screen were up), not world time.
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = G.LastRealSeconds < 0.0 ? DeltaSeconds : FMath::Clamp(Now - G.LastRealSeconds, 0.0, 6.0 * 3600.0);
	G.LastRealSeconds = Now;
	if (Elapsed > 1.0)
	{
		// Catch up after a pause in steps so wetness / puddles integrate sensibly.
		double Left = Elapsed;
		while (Left > 0.0)
		{
			const double Step = FMath::Min(Left, 1.0);
			BIBWeatherState::Advance(Step, bRainyMap);
			Left -= Step;
		}
	}
	else
	{
		BIBWeatherState::Advance(Elapsed, bRainyMap);
	}

	UpdateDetail();
	UpdateCamera();
	UpdateLighting(DeltaSeconds);
	UpdateShelter(false);
	UpdateRain(DeltaSeconds);
	UpdateLightning(DeltaSeconds);
	UpdateParticles(DeltaSeconds);
	UpdateAudio(DeltaSeconds);
	if ((ClockWidget.IsValid()) != G.bShowClock)
	{
		ShowClock(G.bShowClock);
	}
}

void ABIBWeatherSystem::UpdateDetail()
{
	Detail = FMath::Clamp(CVarWeatherDetail.GetValueOnGameThread(), 0, 2);
	// Wet / gloss / puddle decals: three 120 m screen-covering decals. Only on Full, and only while there is water to show.
	const bool bDecals = Detail >= 2 && (G.Wetness > 0.005f || G.Puddles > 0.005f || G.RainAmount > 0.01f);
	for (UDecalComponent* Decal : {WetDarken.Get(), WetGloss.Get(), Puddles.Get()})
	{
		if (Decal && Decal->GetDecalMaterial() && Decal->IsVisible() != bDecals)
		{
			Decal->SetVisibility(bDecals);
		}
	}
}

void ABIBWeatherSystem::UpdateCamera()
{
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
		}
	}
	FollowRoot->SetWorldLocation(CameraLocation);
	SkyDome->SetWorldLocation(CameraLocation);
}

void ABIBWeatherSystem::UpdateLighting(float DeltaSeconds)
{
	using namespace BIBWeatherPalette;
	const float Hour = static_cast<float>(G.Hour);
	const FPaletteKey Key = Sample(Hour);
	const FVector Sun = SunDirection(G.Hour);
	const FVector Moon = MoonDirection(G.Hour);
	const float Cloud = G.Cloud;
	const float Rain = G.RainAmount;
	const float Storm = G.Storm;
	const float Overcast = FMath::SmoothStep(0.55f, 1.f, Cloud);

	// One key light: the sun while it is up, the moon otherwise. Its strength fades to zero at the swap.
	const float SunUp = FMath::SmoothStep(-0.04f, 0.06f, static_cast<float>(Sun.Z));
	const float MoonUp = FMath::SmoothStep(-0.04f, 0.10f, static_cast<float>(Moon.Z));
	const bool bSunKey = Sun.Z > -0.02;
	const FVector KeyDir = bSunKey ? Sun : Moon;
	const float KeyFade = bSunKey ? SunUp : MoonUp * (1.f - SunUp);
	const float CloudDim = FMath::Lerp(1.f, 0.22f, Overcast) * FMath::Lerp(1.f, 0.6f, Storm);
	KeyLight->SetWorldRotation((-KeyDir).Rotation());
	const float LightScale = CVarLightScale.GetValueOnGameThread();
	KeyLight->SetIntensity(Key.LightLux * KeyFade * CloudDim * LightScale);
	// Overcast light turns soft and slightly cool.
	KeyLight->SetLightColor(FMath::Lerp(Key.Light, FLinearColor(0.78f, 0.84f, 0.95f), Overcast * 0.6f));

	// Sky light: real-time capture of the sky dome; raise the floor under heavy cloud so storms are moody, not black.
	const float NightAmount = 1.f - FMath::SmoothStep(-0.18f, 0.08f, static_cast<float>(Sun.Z));
	SkyLight->SetIntensity(Key.Sky * FMath::Lerp(1.f, 1.35f, Overcast) * LightScale);
	SkyLight->SetLightColor(FLinearColor::White);

	// Fog: thicker in rain and storms, coloured by the hour; the level's own heavy fog (Neo Tokyo) is kept.
	const float FogDensity = 0.02f * MapFogScale * (1.f + Rain * 1.4f + Storm * 0.6f) * FMath::Lerp(1.f, 0.75f, G.WindAmount);
	Fog->SetFogDensity(FogDensity);
	Fog->SetFogHeightFalloff(0.05f);
	const FLinearColor RainFog = FLinearColor::LerpUsingHSV(Key.FogColour, Key.FogColour * FLinearColor(0.7f, 0.78f, 0.9f), 0.5f);
	Fog->SetFogInscatteringColor(FMath::Lerp(Key.FogColour, RainFog, FMath::Max(Rain, Overcast)));
	Fog->SetDirectionalInscatteringColor(Key.Glow * 0.35f * (1.f - Overcast) * KeyFade);
	Fog->SetDirectionalInscatteringExponent(6.f);
	Fog->SetFogMaxOpacity(FMath::Lerp(0.85f, 0.97f, Rain));
	Fog->SetVolumetricFogExtinctionScale(FMath::Lerp(1.f, 2.2f, Rain));
	Fog->SetVolumetricFogAlbedo(FColor(230, 235, 255));

	// Post-process grade (vivid by day; rainy days desaturate a little; nights lift a little for readability).
	FPostProcessSettings& P = Grade->Settings;
	P.bOverride_AutoExposureMethod = true;
	P.AutoExposureMethod = AEM_Manual;
	P.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	P.AutoExposureApplyPhysicalCameraExposure = 0;
	P.bOverride_AutoExposureBias = true;
	// Owner 2026-10-07 (no extreme white light): overcast days lift the exposure less than before (0.9 EV -> 0.45 EV).
	P.AutoExposureBias = CVarExposureOffset.GetValueOnGameThread() + Key.Exposure + Overcast * 0.45f * (1.f - NightAmount) + Storm * 0.1f;
	// Anime frames keep their shadows readable: lift them with local exposure instead of raising the whole image.
	P.bOverride_LocalExposureShadowContrastScale = true;
	P.LocalExposureShadowContrastScale = 0.6f;
	P.bOverride_LocalExposureHighlightContrastScale = true;
	P.LocalExposureHighlightContrastScale = 0.7f;   // pulls bright areas (wet streets under a bright sky) down a little
	P.bOverride_ColorSaturation = true;
	P.ColorSaturation = FVector4(1.0, 1.0, 1.0, FMath::Lerp(Key.Saturation, Key.Saturation * 0.82f, FMath::Max(Rain, Overcast * 0.7f)));
	P.bOverride_ColorContrast = true;
	P.ColorContrast = FVector4(1.0, 1.0, 1.0, Key.Contrast);
	P.bOverride_WhiteTemp = true;
	P.WhiteTemp = FMath::Lerp(6500.f, 7600.f, FMath::Max(Rain * 0.8f, NightAmount * 0.5f));
	P.bOverride_ColorGain = true;
	P.ColorGain = FVector4(1.0, 1.0, 1.0, 1.0 + Flash * 0.6);

	// Sky and effect parameters (MPC_BIBWeather).
	const FLinearColor Grey(0.42f, 0.46f, 0.55f);
	const float GreyAmount = Overcast * (1.f - NightAmount * 0.5f);
	auto Overcasted = [&](const FLinearColor& C, float Bright) { return FMath::Lerp(C, Grey * Bright, GreyAmount * 0.75f); };
	const float DayBright = FMath::Max(0.02f, Key.Horizon.GetLuminance());
	SetVector(TEXT("SunDir"), FLinearColor(Sun.X, Sun.Y, Sun.Z, SunUp));
	SetVector(TEXT("MoonDir"), FLinearColor(Moon.X, Moon.Y, Moon.Z, MoonUp));
	SetVector(TEXT("ZenithColor"), Overcasted(Key.Zenith, DayBright * 0.8f));
	SetVector(TEXT("HorizonColor"), Overcasted(Key.Horizon, DayBright));
	SetVector(TEXT("GlowColor"), Key.Glow * (1.f - Overcast * 0.85f));
	SetVector(TEXT("CloudLit"), FMath::Lerp(Key.CloudLit, Key.CloudLit * FLinearColor(0.55f, 0.58f, 0.66f), Overcast) * (1.f - Storm * 0.45f));
	SetVector(TEXT("CloudShade"), FMath::Lerp(Key.CloudShade, Key.CloudShade * FLinearColor(0.5f, 0.52f, 0.6f), Overcast) * (1.f - Storm * 0.4f));
	SetScalar(TEXT("CloudCover"), Cloud);
	SetScalar(TEXT("StarAmount"), Key.Stars * (1.f - Overcast));
	SetScalar(TEXT("NightAmount"), NightAmount);
	SetScalar(TEXT("DayHour"), Hour);
	SetScalar(TEXT("RainAmount"), Rain);
	SetScalar(TEXT("Wetness"), G.Wetness);
	SetScalar(TEXT("Puddles"), G.Puddles);
	SetScalar(TEXT("WindAmount"), G.WindAmount);
	SetScalar(TEXT("Storm"), Storm);
	// Rain catches the light: brighter by day, dim blue at night (neon still lights it: the rain material is lit).
	SetScalar(TEXT("RainBrightness"), FMath::Lerp(0.05f, 1.f, 1.f - NightAmount) * FMath::Lerp(1.f, 0.7f, Overcast));
	SetVector(TEXT("AmbientColor"), FMath::Lerp(Key.Horizon, Key.Zenith, 0.5f));

	// Wind: from the west-south-west, gusting; clouds drift with it.
	const float Gust = 0.75f + 0.25f * FMath::Sin(GetWorld()->GetTimeSeconds() * 0.7f) * FMath::Sin(GetWorld()->GetTimeSeconds() * 1.9f + 1.3f);
	const FVector2D WindDir = FVector2D(0.85, 0.53).GetSafeNormal();
	const float WindSpeed = (1.5f + G.WindAmount * 16.f) * Gust;   // m/s
	CloudOffset += WindDir * (0.004f + G.WindAmount * 0.02f) * DeltaSeconds;
	SetVector(TEXT("WindVector"), FLinearColor(WindDir.X, WindDir.Y, WindSpeed, G.WindAmount * Gust));
	SetVector(TEXT("CloudOffset"), FLinearColor(CloudOffset.X, CloudOffset.Y, 0.f, 0.f));
	SetVector(TEXT("CameraPos"), FLinearColor(CameraLocation.X, CameraLocation.Y, CameraLocation.Z, 0.f));
	SetVector(TEXT("LightningDir"), FLinearColor(FlashDir.X, FlashDir.Y, FlashDir.Z, Flash));
	SetScalar(TEXT("Lightning"), Flash);
}

// ================================================================================================ rain shelter
void ABIBWeatherSystem::UpdateShelter(bool bForce)
{
	if (!ShelterTarget)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// Snap to a 2 m grid so the captured texels stay put while the camera moves.
	const FVector Centre(FMath::GridSnap(CameraLocation.X, 200.0), FMath::GridSnap(CameraLocation.Y, 200.0), CameraLocation.Z);
	const bool bMoved = FVector::Dist2D(Centre, LastCaptureCentre) > 800.0 || FMath::Abs(Centre.Z - LastCaptureCentre.Z) > 3000.0;
	const bool bNeeded = (Detail >= 1 && (G.RainAmount > 0.01f || G.Wetness > 0.01f || G.Puddles > 0.01f)) || bForce;
	const double Refresh = Detail >= 2 ? 2.0 : 6.0;   // overhead depth render of the whole area around the camera
	if (bNeeded && (bForce || bMoved || Now - LastCaptureTime > Refresh))
	{
		const FVector CapturePos(Centre.X, Centre.Y, Centre.Z + ShelterAbove);
		ShelterCapture->SetWorldLocation(CapturePos);
		ShelterCapture->CaptureScene();
		LastCaptureCentre = Centre;
		LastCaptureTime = Now;
		bShelterValid = true;
		SetVector(TEXT("ShelterInfo"), FLinearColor(Centre.X, Centre.Y, CapturePos.Z, ShelterWidth));
	}
}

float ABIBWeatherSystem::SampleShelterTop(FVector Location)
{
	if (!ShelterTarget || !bShelterValid)
	{
		return -1e9f;
	}
	// Image axes of a capture looking straight down with yaw 0: +u = +Y (right), +v = -X (down the image).
	const float U = 0.5f + static_cast<float>((Location.Y - LastCaptureCentre.Y) / ShelterWidth);
	const float V = 0.5f - static_cast<float>((Location.X - LastCaptureCentre.X) / ShelterWidth);
	if (U < 0.f || U > 1.f || V < 0.f || V > 1.f)
	{
		return -1e9f;
	}
	// Raw float read (the Kismet helpers convert through 16-bit floats and lose these depths).
	FTextureRenderTargetResource* Resource = ShelterTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		return -1e9f;
	}
	const int32 X = FMath::Clamp(FMath::FloorToInt(U * ShelterResolution), 0, ShelterResolution - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt(V * ShelterResolution), 0, ShelterResolution - 1);
	TArray<FLinearColor> Pixels;
	FReadSurfaceDataFlags Flags(RCM_MinMax);
	Flags.SetLinearToGamma(false);
	if (!Resource->ReadLinearColorPixels(Pixels, Flags, FIntRect(X, Y, X + 1, Y + 1)) || Pixels.Num() == 0)
	{
		return -1e9f;
	}
	const FLinearColor Depth = Pixels[0];
	if (Depth.R > 5e7f)
	{
		return -1e9f;
	}
	return static_cast<float>(LastCaptureCentre.Z + ShelterAbove) - Depth.R;
}

// ================================================================================================ rain
void ABIBWeatherSystem::BuildRainLayers()
{
	// Open cylinders around the camera; UVs in metres so the rain material draws world-sized drops.
	const float Radii[3] = {350.f, 900.f, 2200.f};
	const float Bottom[3] = {-800.f, -1500.f, -2500.f};
	const float Top[3] = {1200.f, 2500.f, 4000.f};
	const TCHAR* const Names[3] = {TEXT("M_BIBRain"), TEXT("M_BIBRain"), TEXT("M_BIBRain")};
	const float Columns[3] = {5.f, 7.f, 9.f};         // streak columns per metre of circumference
	const float Lengths[3] = {0.55f, 0.75f, 1.1f};    // streak length (m)
	const float Widths[3] = {0.10f, 0.08f, 0.07f};    // streak width (fraction of a column)
	const float Opacities[3] = {0.55f, 0.45f, 0.4f};
	constexpr int32 Segments = 48;
	for (int32 Layer = 0; Layer < 3; ++Layer)
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colours;
		TArray<FProcMeshTangent> Tangents;
		const float Circumference = 2.f * PI * Radii[Layer] / 100.f;
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float A = 2.f * PI * Index / Segments;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			for (const float Z : {Bottom[Layer], Top[Layer]})
			{
				Verts.Add(Dir * Radii[Layer] + FVector(0, 0, Z));
				Normals.Add(-Dir);
				UVs.Add(FVector2D(Circumference * Index / Segments, Z / 100.f));
				Colours.Add(FLinearColor::White);
				Tangents.Add(FProcMeshTangent(FVector(-Dir.Y, Dir.X, 0.f), false));
			}
		}
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const int32 A = Index * 2;
			Tris.Append({A, A + 1, A + 2, A + 1, A + 3, A + 2});
		}
		RainLayers[Layer]->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colours, Tangents, false);
		if (UMaterialInstanceDynamic* Mid = MakeMid(Names[Layer], true))
		{
			Mid->SetScalarParameterValue(TEXT("Columns"), Columns[Layer]);
			Mid->SetScalarParameterValue(TEXT("StreakLength"), Lengths[Layer]);
			Mid->SetScalarParameterValue(TEXT("StreakWidth"), Widths[Layer]);
			Mid->SetScalarParameterValue(TEXT("LayerOpacity"), Opacities[Layer]);
			Mid->SetScalarParameterValue(TEXT("LayerSeed"), 17.f * (Layer + 1));
			RainLayers[Layer]->SetMaterial(0, Mid);
		}
		else
		{
			RainLayers[Layer]->SetVisibility(false);
		}
	}
}

void ABIBWeatherSystem::UpdateRain(float DeltaSeconds)
{
	const bool bRaining = G.RainAmount > 0.01f && Detail >= 1;
	for (UProceduralMeshComponent* Layer : RainLayers)
	{
		if (Layer->GetMaterial(0))
		{
			Layer->SetVisibility(bRaining);
		}
	}
	// Lean the rain into the wind: drops fall at ~9 m/s, so the tilt is atan(wind / 9).
	const FVector Wind = FVector(0.85, 0.53, 0.0).GetSafeNormal() * (1.5 + G.WindAmount * 16.0);
	const FVector Fall = (FVector(0, 0, -900.0) + Wind * 100.0 * 0.8).GetSafeNormal();
	RainRoot->SetWorldRotation(FRotationMatrix::MakeFromZ(-Fall).Rotator());
}

// ================================================================================================ particles (splashes, wind streaks)
void ABIBWeatherSystem::UpdateParticles(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	// Splashes where drops land: trace down from the sky around the camera, so roofs, awnings and characters get them too.
	if (Detail == 0)
	{
		// Weather effects off: no splashes, streaks or bolt geometry (the lightning flash and thunder stay).
		Splashes.Reset();
		Streaks.Reset();
		Bolts.Reset();
		if (bParticlesShown)
		{
			Particles->ClearAllMeshSections();
			bParticlesShown = false;
		}
		return;
	}
	const float Rain = Detail >= 2 ? G.RainAmount : 0.f;   // Low: no splashes (a line trace each)
	SplashBudget += DeltaSeconds * Rain * 520.f;
	const FVector Focus = CameraLocation + CameraRotation.Vector() * 500.0;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BIBRainSplash), false, this);
	int32 Spawned = 0;
	while (SplashBudget >= 1.f && Spawned < 64)
	{
		SplashBudget -= 1.f;
		++Spawned;
		const float Radius = 1400.f * FMath::Sqrt(Rng.FRand());
		const float Angle = Rng.FRand() * 2.f * PI;
		const FVector Top = FVector(Focus.X + Radius * FMath::Cos(Angle), Focus.Y + Radius * FMath::Sin(Angle), CameraLocation.Z + 3000.0);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 9000.0), ECC_Visibility, Params))
		{
			FSplash Splash;
			Splash.Location = Hit.ImpactPoint + Hit.ImpactNormal * 2.0;
			Splash.Life = Rng.FRandRange(0.18f, 0.32f);
			Splash.Size = Rng.FRandRange(6.f, 14.f) * (0.7f + Rain * 0.5f);
			Splash.Seed = Rng.FRand();
			Splashes.Add(Splash);
			++SplashesSpawned;
		}
	}
	for (int32 Index = Splashes.Num() - 1; Index >= 0; --Index)
	{
		Splashes[Index].Age += DeltaSeconds;
		if (Splashes[Index].Age >= Splashes[Index].Life)
		{
			Splashes.RemoveAtSwap(Index);
		}
	}
	if (Splashes.Num() > 600)
	{
		Splashes.RemoveAt(0, Splashes.Num() - 600);
	}

	// Anime wind streaks: white speed-line swooshes drifting with the wind, more of them the stronger it blows.
	const float Wind = G.WindAmount;
	const FVector WindDir = FVector(0.85, 0.53, 0.0).GetSafeNormal();
	const int32 Wanted = Wind < 0.15f ? 0 : FMath::RoundToInt(Wind * Wind * 26.f + 2.f);
	StreakBudget += DeltaSeconds * Wanted / 1.3f;
	while (StreakBudget >= 1.f)
	{
		StreakBudget -= 1.f;
		if (Streaks.Num() >= Wanted + 4)
		{
			continue;
		}
		FStreak Streak;
		const float Ahead = Rng.FRandRange(400.f, 2600.f);
		const float Side = Rng.FRandRange(-1800.f, 1800.f);
		const FVector Forward = FVector(CameraRotation.Vector().X, CameraRotation.Vector().Y, 0.0).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		Streak.Start = CameraLocation + Forward * Ahead + Right * Side + FVector(0, 0, Rng.FRandRange(-150.f, 500.f)) - WindDir * 500.0;
		Streak.Dir = (WindDir + FVector(0, 0, Rng.FRandRange(-0.08f, 0.12f))).GetSafeNormal();
		Streak.Side = FVector::CrossProduct(FVector::UpVector, Streak.Dir).GetSafeNormal();
		Streak.Life = Rng.FRandRange(0.9f, 1.5f) * FMath::Lerp(1.3f, 0.75f, Wind);
		Streak.Length = Rng.FRandRange(500.f, 1100.f) * (0.7f + Wind * 0.6f);
		Streak.Width = Rng.FRandRange(4.f, 9.f);
		Streak.Wave = Rng.FRandRange(20.f, 70.f);
		Streak.Seed = Rng.FRand();
		Streaks.Add(Streak);
	}
	for (int32 Index = Streaks.Num() - 1; Index >= 0; --Index)
	{
		FStreak& Streak = Streaks[Index];
		Streak.Age += DeltaSeconds;
		Streak.Start += Streak.Dir * (300.0 + Wind * 1200.0) * DeltaSeconds;
		if (Streak.Age >= Streak.Life)
		{
			Streaks.RemoveAtSwap(Index);
		}
	}
	for (int32 Index = Bolts.Num() - 1; Index >= 0; --Index)
	{
		Bolts[Index].Age += DeltaSeconds;
		if (Bolts[Index].Age >= Bolts[Index].Life)
		{
			Bolts.RemoveAtSwap(Index);
		}
	}
	// Rebuilding the procedural mesh re-creates its render data: skip it while there is nothing to draw.
	const bool bAny = Splashes.Num() > 0 || Streaks.Num() > 0 || Bolts.Num() > 0;
	if (bAny || bParticlesShown)
	{
		BuildParticleMesh();
	}
	bParticlesShown = bAny;
}

void ABIBWeatherSystem::BuildParticleMesh()
{
	const FVector CamUp = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);
	auto Emit = [this](int32 Section, const TArray<FVector>& Verts, const TArray<int32>& Tris, const TArray<FVector2D>& UVs, const TArray<FLinearColor>& Colours)
	{
		if (Tris.Num() == 0)
		{
			if (Particles->GetNumSections() > Section)
			{
				Particles->ClearMeshSection(Section);
			}
			return;
		}
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Verts.Num());
		Particles->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, UVs, Colours, TArray<FProcMeshTangent>(), false);
	};

	// Section 0: splashes (upright quads turned to the camera; the material draws the crown and droplets).
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colours;
		for (const FSplash& Splash : Splashes)
		{
			FVector ToCam = CameraLocation - Splash.Location;
			ToCam.Z = 0.0;
			const FVector Right = FVector::CrossProduct(FVector::UpVector, ToCam.GetSafeNormal()) * Splash.Size;
			const FVector Up = FVector::UpVector * Splash.Size * 1.6f;
			const int32 Base = Verts.Num();
			Verts.Append({Splash.Location - Right, Splash.Location + Right, Splash.Location + Right + Up, Splash.Location - Right + Up});
			UVs.Append({FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0)});
			const FLinearColor C(Splash.Age / Splash.Life, Splash.Seed, 0.f, 1.f);
			Colours.Append({C, C, C, C});
			Tris.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
		}
		Emit(0, Verts, Tris, UVs, Colours);
	}
	// Section 1: wind streaks (camera-facing ribbons along a gently waving path).
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colours;
		constexpr int32 Steps = 16;
		for (const FStreak& Streak : Streaks)
		{
			const int32 Base = Verts.Num();
			const FLinearColor C(Streak.Age / Streak.Life, Streak.Seed, 0.f, 1.f);
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float S = static_cast<float>(Step) / Steps;
				const float Phase = Streak.Seed * 6.28f;
				const FVector P = Streak.Start + Streak.Dir * (S * Streak.Length) + Streak.Side * (FMath::Sin(S * 4.2f + Phase) * Streak.Wave)
					+ FVector::UpVector * (FMath::Sin(S * 2.7f + Phase * 1.7f) * Streak.Wave * 0.6f);
				const FVector Tangent = (Streak.Dir + Streak.Side * (FMath::Cos(S * 4.2f + Phase) * 4.2f * Streak.Wave / Streak.Length)).GetSafeNormal();
				const FVector Across = FVector::CrossProduct(Tangent, (CameraLocation - P).GetSafeNormal()).GetSafeNormal() * Streak.Width;
				Verts.Append({P - Across, P + Across});
				UVs.Append({FVector2D(S, 0), FVector2D(S, 1)});
				Colours.Append({C, C});
				if (Step > 0)
				{
					const int32 A = Base + (Step - 1) * 2;
					Tris.Append({A, A + 1, A + 2, A + 1, A + 3, A + 2});
				}
			}
		}
		Emit(1, Verts, Tris, UVs, Colours);
	}
	// Section 2: lightning bolts (camera-facing ribbons; R = branch brightness, G = life).
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colours;
		for (const FBolt& Bolt : Bolts)
		{
			for (int32 Branch = 0; Branch < Bolt.Branches.Num(); ++Branch)
			{
				const TArray<FVector>& Points = Bolt.Branches[Branch];
				const float Width = Bolt.Width * (Branch == 0 ? 1.f : 0.45f);
				const FLinearColor C(Branch == 0 ? 1.f : 0.55f, Bolt.Age / Bolt.Life, 0.f, 1.f);
				const int32 Base = Verts.Num();
				for (int32 Index = 0; Index < Points.Num(); ++Index)
				{
					const FVector Next = Points[FMath::Min(Index + 1, Points.Num() - 1)];
					const FVector Prev = Points[FMath::Max(Index - 1, 0)];
					const FVector Tangent = (Next - Prev).GetSafeNormal();
					const float Taper = Branch == 0 ? 1.f : 1.f - static_cast<float>(Index) / Points.Num();
					const FVector Across = FVector::CrossProduct(Tangent, (CameraLocation - Points[Index]).GetSafeNormal()).GetSafeNormal() * Width * Taper;
					Verts.Append({Points[Index] - Across, Points[Index] + Across});
					UVs.Append({FVector2D(static_cast<float>(Index) / Points.Num(), 0), FVector2D(static_cast<float>(Index) / Points.Num(), 1)});
					Colours.Append({C, C});
					if (Index > 0)
					{
						const int32 A = Base + (Index - 1) * 2;
						Tris.Append({A, A + 1, A + 2, A + 1, A + 3, A + 2});
					}
				}
			}
		}
		Emit(2, Verts, Tris, UVs, Colours);
	}
	(void)CamUp;
}

// ================================================================================================ lightning
void ABIBWeatherSystem::StrikeLightning()
{
	// A bolt 0.5 - 3 km away in a random direction; thunder follows at the speed of sound.
	const float Distance = Rng.FRandRange(50000.f, 300000.f);
	// Mostly somewhere in front of the camera, so strikes are seen, not only heard.
	const float Yaw = FMath::DegreesToRadians(CameraRotation.Yaw);
	const float Angle = Rng.FRand() < 0.7f ? Yaw + Rng.FRandRange(-1.1f, 1.1f) : Rng.FRand() * 2.f * PI;
	const FVector Ground = FVector(CameraLocation.X + Distance * FMath::Cos(Angle), CameraLocation.Y + Distance * FMath::Sin(Angle), CameraLocation.Z - 2000.0);
	const FVector Cloud = Ground + FVector(Rng.FRandRange(-15000.f, 15000.f), Rng.FRandRange(-15000.f, 15000.f), 120000.0);
	FBolt Bolt;
	Bolt.Width = FMath::Clamp(Distance * 0.004f, 150.f, 900.f);
	Bolt.Life = 0.75f;
	// Main channel: midpoint displacement for a jagged, branching bolt.
	TArray<FVector> Main = {Cloud, Ground};
	for (int32 Level = 0; Level < 6; ++Level)
	{
		TArray<FVector> Finer;
		const float Jitter = FVector::Dist(Cloud, Ground) * 0.18f / FMath::Pow(1.9f, static_cast<float>(Level));
		for (int32 Index = 0; Index < Main.Num() - 1; ++Index)
		{
			Finer.Add(Main[Index]);
			Finer.Add((Main[Index] + Main[Index + 1]) * 0.5 + FVector(Rng.FRandRange(-Jitter, Jitter), Rng.FRandRange(-Jitter, Jitter), Rng.FRandRange(-Jitter, Jitter) * 0.3f));
		}
		Finer.Add(Main.Last());
		Main = MoveTemp(Finer);
	}
	Bolt.Branches.Add(Main);
	const int32 BranchCount = Rng.RandRange(2, 4);
	for (int32 Branch = 0; Branch < BranchCount; ++Branch)
	{
		const int32 From = Rng.RandRange(Main.Num() / 6, Main.Num() * 2 / 3);
		TArray<FVector> Points = {Main[From]};
		FVector Dir = (Main[FMath::Min(From + 4, Main.Num() - 1)] - Main[From]).GetSafeNormal() + FVector(Rng.FRandRange(-0.8f, 0.8f), Rng.FRandRange(-0.8f, 0.8f), 0.0);
		Dir.Normalize();
		const float Step = FVector::Dist(Cloud, Ground) / Main.Num();
		const int32 Count = Rng.RandRange(6, 14);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Dir = (Dir + FVector(Rng.FRandRange(-0.5f, 0.5f), Rng.FRandRange(-0.5f, 0.5f), Rng.FRandRange(-0.3f, 0.1f))).GetSafeNormal();
			Points.Add(Points.Last() + Dir * Step);
		}
		Bolt.Branches.Add(Points);
	}
	Bolts.Add(Bolt);
	FlashDir = (Cloud - CameraLocation).GetSafeNormal();
	Flash = 1.f;
	++StrikesSeen;
	ThunderDelay = Distance / 34300.f;
	ThunderVolume = FMath::Clamp(1.25f - Distance / 300000.f, 0.25f, 1.f);
}

void ABIBWeatherSystem::UpdateLightning(float DeltaSeconds)
{
	if (G.Storm > 0.5f)
	{
		NextStrikeIn -= DeltaSeconds;
		if (NextStrikeIn <= 0.f)
		{
			StrikeLightning();
			NextStrikeIn = Rng.FRandRange(5.f, 18.f);
		}
	}
	// Flicker: three pulses over ~0.6 s, each held long enough to survive the 12 fps anime frame hold.
	float Pulse = 0.f;
	for (const FBolt& Bolt : Bolts)
	{
		const float T = Bolt.Age;
		Pulse = FMath::Max(Pulse, T < 0.12f ? 1.f : (T < 0.2f ? 0.25f : (T < 0.32f ? 0.8f : (T < 0.42f ? 0.2f : (T < 0.55f ? 0.55f : 0.f)))));
	}
	Flash = Pulse;
	FlashLight->SetWorldRotation((-FlashDir).Rotation());
	FlashLight->SetIntensity(Flash * 7.f);
	if (ThunderDelay >= 0.f)
	{
		ThunderDelay -= DeltaSeconds;
		if (ThunderDelay < 0.f && ThunderSounds.Num() > 0)
		{
			ThunderAudio->SetSound(ThunderSounds[Rng.RandRange(0, ThunderSounds.Num() - 1)]);
			ThunderAudio->SetVolumeMultiplier(ThunderVolume);
			ThunderAudio->SetPitchMultiplier(Rng.FRandRange(0.85f, 1.1f));
			ThunderAudio->Play();
		}
	}
}

// ================================================================================================ audio
void ABIBWeatherSystem::UpdateAudio(float DeltaSeconds)
{
	// Under a roof the rain is quieter and muffled.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BIBRainShelter), false, this);
	if (APawn* Pawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr)
	{
		Params.AddIgnoredActor(Pawn);
	}
	const bool bSheltered = GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, CameraLocation + FVector(0, 0, 20000.0), ECC_Visibility, Params);
	RainAudio->SetVolumeMultiplier(FMath::Max(0.001f, G.RainAmount * (bSheltered ? 0.45f : 1.f)));
	RainAudio->SetPitchMultiplier(0.9f + G.RainAmount * 0.15f);
	RainAudio->SetLowPassFilterEnabled(bSheltered);
	RainAudio->SetLowPassFilterFrequency(1800.f);
	const float Gust = 0.8f + 0.2f * FMath::Sin(GetWorld()->GetTimeSeconds() * 0.9f);
	WindAudio->SetVolumeMultiplier(FMath::Max(0.001f, G.WindAmount * G.WindAmount * Gust));
	WindAudio->SetPitchMultiplier(0.8f + G.WindAmount * 0.4f);
}

// ================================================================================================ clock readout
void ABIBWeatherSystem::ShowClock(bool bShow)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!bShow || !Viewport)
	{
		if (ClockWidget.IsValid() && Viewport)
		{
			Viewport->RemoveViewportWidgetContent(ClockWidget.ToSharedRef());
		}
		ClockWidget.Reset();
		return;
	}
	if (ClockWidget.IsValid())
	{
		return;
	}
	ClockWidget = SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 24.f, 18.f))
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(STextBlock)
			.Text_Lambda([]() { return FText::FromString(ABIBWeatherSystem::GetClockText()); })
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.85f))
			.ShadowOffset(FVector2D(1.5f, 1.5f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		];
	Viewport->AddViewportWidgetContent(ClockWidget.ToSharedRef(), 5);
}
