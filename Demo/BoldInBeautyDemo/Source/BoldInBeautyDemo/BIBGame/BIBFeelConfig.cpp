#include "BIBGame/BIBFeelConfig.h"

#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogBIBFeel, Log, All);

// Named (not anonymous) so unity builds cannot mix these helpers up with another file's.
namespace BIBFeelConfigImpl
{
	const TCHAR* const FeelSection = TEXT("BIB.CombatFeel");
	bool GLoaded = false;
	float GSoundVolume = 1.f;
	TArray<USoundBase*> GHitSounds[BIB::Feel::WeightCount];
	TMap<FString, TArray<USoundBase*>> GNamedSounds;
	TMap<FString, FString> GSoundPaths;   // Sound.<Name> -> comma-separated asset paths

	void ReleaseSounds()
	{
		for (TArray<USoundBase*>& Layers : GHitSounds)
		{
			for (USoundBase* Sound : Layers)
			{
				if (Sound)
				{
					Sound->RemoveFromRoot();
				}
			}
			Layers.Reset();
		}
		for (TPair<FString, TArray<USoundBase*>>& Pair : GNamedSounds)
		{
			for (USoundBase* Sound : Pair.Value)
			{
				if (Sound)
				{
					Sound->RemoveFromRoot();
				}
			}
		}
		GNamedSounds.Reset();
	}

	TArray<USoundBase*> LoadLayers(const FString& Paths)
	{
		TArray<USoundBase*> Out;
		TArray<FString> Parts;
		Paths.ParseIntoArray(Parts, TEXT(","), true);
		for (FString Part : Parts)
		{
			Part.TrimStartAndEndInline();
			if (Part.IsEmpty())
			{
				continue;
			}
			if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Part, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Sound->AddToRoot();   // a handful of short hit sounds, kept loaded for the session
				Out.Add(Sound);
			}
			else
			{
				UE_LOG(LogBIBFeel, Warning, TEXT("Combat feel: sound '%s' not found."), *Part);
			}
		}
		return Out;
	}

	bool SetValue(const FString& Key, const FString& Value)
	{
		BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
		if (Key.StartsWith(TEXT("Sound.")))
		{
			GSoundPaths.Add(Key, Value);
			if (Key == TEXT("Sound.Volume"))
			{
				GSoundVolume = FMath::Clamp(static_cast<float>(FCString::Atod(*Value)), 0.f, 4.f);
			}
			return true;
		}
		for (const BIB::Feel::FConfigKey& Entry : BIB::Feel::ConfigKeys(Tuning))
		{
			if (Key.Equals(UTF8_TO_TCHAR(Entry.Name.c_str()), ESearchCase::IgnoreCase))
			{
				*Entry.Value = FCString::Atod(*Value);
				return true;
			}
		}
		for (const BIB::Feel::FReactionKey& Entry : BIB::Feel::ReactionKeys(Tuning))
		{
			if (Key.Equals(UTF8_TO_TCHAR(Entry.Name.c_str()), ESearchCase::IgnoreCase))
			{
				BIB::Feel::EReaction Parsed = BIB::Feel::EReaction::None;
				if (BIB::Feel::ParseReaction(TCHAR_TO_UTF8(*Value), Parsed))
				{
					*Entry.Value = Parsed;
					return true;
				}
				UE_LOG(LogBIBFeel, Warning, TEXT("Combat feel: '%s' is not a reaction (None, Recoil, Flinch, Stagger, Knockback, Launch, Knockdown)."), *Value);
				return false;
			}
		}
		UE_LOG(LogBIBFeel, Warning, TEXT("Combat feel: unknown key '%s'."), *Key);
		return false;
	}

	void ApplySounds()
	{
		ReleaseSounds();
		for (int32 Index = 0; Index < BIB::Feel::WeightCount; ++Index)
		{
			const FString Key = FString(TEXT("Sound.")) + UTF8_TO_TCHAR(BIB::Feel::WeightName(static_cast<BIB::Feel::EWeight>(Index)));
			if (const FString* Paths = GSoundPaths.Find(Key))
			{
				GHitSounds[Index] = LoadLayers(*Paths);
			}
		}
	}

	/** Every known key of the section from the merged game config (works in packaged builds). */
	void LoadFromGConfig()
	{
		if (!GConfig)
		{
			return;
		}
		TArray<FString> Keys;
		BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
		for (const BIB::Feel::FConfigKey& Entry : BIB::Feel::ConfigKeys(Tuning))
		{
			Keys.Add(UTF8_TO_TCHAR(Entry.Name.c_str()));
		}
		for (const BIB::Feel::FReactionKey& Entry : BIB::Feel::ReactionKeys(Tuning))
		{
			Keys.Add(UTF8_TO_TCHAR(Entry.Name.c_str()));
		}
		Keys.Add(TEXT("Sound.Volume"));
		Keys.Add(TEXT("Sound.UltCinematic"));
		Keys.Add(TEXT("Sound.PerfectDodge"));
		for (int32 Index = 0; Index < BIB::Feel::WeightCount; ++Index)
		{
			Keys.Add(FString(TEXT("Sound.")) + UTF8_TO_TCHAR(BIB::Feel::WeightName(static_cast<BIB::Feel::EWeight>(Index))));
		}
		for (const FString& Key : Keys)
		{
			FString Value;
			if (GConfig->GetString(FeelSection, *Key, Value, GGameIni))
			{
				SetValue(Key, Value.TrimStartAndEnd());
			}
		}
	}

	/** Re-reads Config/DefaultGame.ini from disk (the editor keeps the merged config in memory). */
	void LoadFromFile()
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini"));
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
		{
			UE_LOG(LogBIBFeel, Warning, TEXT("Combat feel: cannot read %s."), *Path);
			return;
		}
		const FString Header = FString::Printf(TEXT("[%s]"), FeelSection);
		bool bInSection = false;
		int32 Count = 0;
		for (FString Line : Lines)
		{
			Line.TrimStartAndEndInline();
			if (Line.StartsWith(TEXT("[")))
			{
				bInSection = Line == Header;
				continue;
			}
			if (!bInSection || Line.IsEmpty() || Line.StartsWith(TEXT(";")))
			{
				continue;
			}
			FString Key;
			FString Value;
			if (Line.Split(TEXT("="), &Key, &Value) && SetValue(Key.TrimStartAndEnd(), Value.TrimStartAndEnd()))
			{
				++Count;
			}
		}
		UE_LOG(LogBIBFeel, Log, TEXT("Combat feel: %d values read from %s."), Count, *Path);
	}

	void ReloadCommand()
	{
		BIB::Feel::ResetTuning();
		LoadFromFile();
		ApplySounds();
		GLoaded = true;
	}

	void SetCommand(const TArray<FString>& Args)
	{
		BIBFeelConfig::Load();
		if (Args.Num() >= 2)
		{
			if (SetValue(Args[0], Args[1]))
			{
				if (Args[0].StartsWith(TEXT("Sound.")))
				{
					ApplySounds();
				}
				UE_LOG(LogBIBFeel, Log, TEXT("Combat feel: %s = %s"), *Args[0], *Args[1]);
			}
			return;
		}
		BIB::Feel::FTuning& Tuning = BIB::Feel::Tuning();
		for (const BIB::Feel::FConfigKey& Entry : BIB::Feel::ConfigKeys(Tuning))
		{
			UE_LOG(LogBIBFeel, Log, TEXT("%s=%g"), UTF8_TO_TCHAR(Entry.Name.c_str()), *Entry.Value);
		}
		for (const BIB::Feel::FReactionKey& Entry : BIB::Feel::ReactionKeys(Tuning))
		{
			UE_LOG(LogBIBFeel, Log, TEXT("%s=%s"), UTF8_TO_TCHAR(Entry.Name.c_str()), UTF8_TO_TCHAR(BIB::Feel::ReactionName(*Entry.Value)));
		}
	}

	void ResetStatsCommand()
	{
		BIB::Feel::ResetStats();
		UE_LOG(LogBIBFeel, Log, TEXT("Combat feel: stats cleared."));
	}

	FAutoConsoleCommand GBIBFeelResetStats(TEXT("BIBFeelResetStats"), TEXT("Clears the hit-feel counters (Tools/verify_feel.py)."),
		FConsoleCommandDelegate::CreateStatic(&ResetStatsCommand));
	FAutoConsoleCommand GBIBFeelReload(TEXT("BIBFeelReload"), TEXT("Re-reads [BIB.CombatFeel] from Config/DefaultGame.ini."),
		FConsoleCommandDelegate::CreateStatic(&ReloadCommand));
	FAutoConsoleCommand GBIBFeelSet(TEXT("BIBFeel"), TEXT("BIBFeel <Key> <Value> sets a combat feel value; no arguments lists them."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetCommand));
}

namespace BIBFeelConfig
{
	void Load(bool bForce)
	{
		if (BIBFeelConfigImpl::GLoaded && !bForce)
		{
			return;
		}
		BIBFeelConfigImpl::GLoaded = true;
		BIB::Feel::ResetTuning();
		BIBFeelConfigImpl::LoadFromGConfig();
		BIBFeelConfigImpl::ApplySounds();
	}

	const TArray<USoundBase*>& HitSounds(BIB::Feel::EWeight Weight)
	{
		Load();
		const int32 Index = FMath::Clamp(static_cast<int32>(Weight), 0, BIB::Feel::WeightCount - 1);
		return BIBFeelConfigImpl::GHitSounds[Index];
	}

	const TArray<USoundBase*>& NamedSounds(const FString& Name)
	{
		Load();
		if (const TArray<USoundBase*>* Found = BIBFeelConfigImpl::GNamedSounds.Find(Name))
		{
			return *Found;
		}
		const FString* Paths = BIBFeelConfigImpl::GSoundPaths.Find(TEXT("Sound.") + Name);
		return BIBFeelConfigImpl::GNamedSounds.Add(Name, Paths ? BIBFeelConfigImpl::LoadLayers(*Paths) : TArray<USoundBase*>());
	}

	float SoundVolume()
	{
		Load();
		return BIBFeelConfigImpl::GSoundVolume;
	}
}
