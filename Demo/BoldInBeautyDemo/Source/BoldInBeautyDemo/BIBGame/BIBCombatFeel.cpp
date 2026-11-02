#include "BIBGame/BIBCombatFeel.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace BIBCombatFeel
{
	FConfig FromSettings(const BIB::FSettings& Settings)
	{
		FConfig Config;
		Config.Scale = static_cast<float>(FMath::Clamp(Settings.CombatFeel, 0, 150)) / 100.f;
		Config.bHitStop = Settings.bFeelHitStop;
		Config.bFlash = Settings.bFeelFlash;
		Config.bFov = Settings.bFeelFov;
		Config.bBeats = Settings.bFeelBeats;
		Config.bTelegraph = Settings.bFeelTelegraph;
		Config.bCalm = Settings.bReduceMotion;
		// The simultaneous-effect cap follows the Effects graphics group (or the overall quality preset when it follows it).
		static constexpr int32 CapByLevel[4] = {8, 14, 22, 32};
		const int32 Level = Settings.GraphicsDetail[BIB::GfxEffects] >= 0 ? Settings.GraphicsDetail[BIB::GfxEffects] : Settings.GraphicsQuality;
		Config.FxCap = CapByLevel[FMath::Clamp(Level, 0, 3)];
		return Config;
	}

	const FSlotFeel& ForSlot(int32 SlotIndex)
	{
		// {hit stop x, shake x, FOV punch deg, ground ring}. The basic attack keeps today's feel; the heavier the slot, the more weight.
		static const FSlotFeel Table[] = {
			{1.0f, 1.0f, 0.0f, false},   // Punch
			{1.1f, 1.2f, 0.8f, true},    // Move 1
			{1.1f, 1.2f, 0.8f, true},    // Move 2
			{1.1f, 1.2f, 0.8f, true},    // Move 3
			{1.3f, 1.4f, 1.4f, true},    // Special
			{1.6f, 1.8f, 2.4f, true},    // Ultimate
			{1.0f, 1.0f, 0.0f, false},   // Dodge
			{1.0f, 1.0f, 0.0f, false},   // Parry
		};
		return Table[FMath::Clamp(SlotIndex, 0, 7)];
	}

	void PlayCue(UWorld* World, const TCHAR* Name, const FVector& At, float Volume)
	{
		if (!World || World->IsNetMode(NM_DedicatedServer) || !Name)
		{
			return;
		}
		static TMap<FName, TWeakObjectPtr<USoundBase>> Loaded;
		static TSet<FName> Missing;
		const FName Key(Name);
		if (Missing.Contains(Key))
		{
			return;
		}
		TWeakObjectPtr<USoundBase>& Slot = Loaded.FindOrAdd(Key);
		if (!Slot.IsValid())
		{
			const FString Path = FString::Printf(TEXT("/Game/BIB/Audio/Combat/%s.%s"), Name, Name);
			Slot = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Slot.IsValid())
			{
				Missing.Add(Key);   // not authored yet: the hook stays silent and is not retried
				return;
			}
		}
		UGameplayStatics::PlaySoundAtLocation(World, Slot.Get(), At, Volume);
	}
}
