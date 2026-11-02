#include "BIBGame/BIBAngelDirector.h"

#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBRules/BIBMeta.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBIBAngelAI, Log, All);

// Named (not anonymous) so unity builds cannot mix these helpers up with another file's.
namespace BIBAngelConfigImpl
{
	const TCHAR* const AiSection = TEXT("BIB.AngelAI");
	bool GLoaded = false;

	bool SetValue(const FString& Key, const FString& Value)
	{
		for (const BIB::Angel::FAiConfigKey& Entry : BIB::Angel::AiConfigKeys(BIB::Angel::AiTuning()))
		{
			if (Key.Equals(UTF8_TO_TCHAR(Entry.Name.c_str()), ESearchCase::IgnoreCase))
			{
				*Entry.Value = FCString::Atod(*Value);
				return true;
			}
		}
		UE_LOG(LogBIBAngelAI, Warning, TEXT("Angel AI: unknown key '%s'."), *Key);
		return false;
	}

	void LoadFromGConfig()
	{
		if (!GConfig)
		{
			return;
		}
		for (const BIB::Angel::FAiConfigKey& Entry : BIB::Angel::AiConfigKeys(BIB::Angel::AiTuning()))
		{
			FString Value;
			if (GConfig->GetString(AiSection, UTF8_TO_TCHAR(Entry.Name.c_str()), Value, GGameIni))
			{
				*Entry.Value = FCString::Atod(*Value.TrimStartAndEnd());
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
			UE_LOG(LogBIBAngelAI, Warning, TEXT("Angel AI: cannot read %s."), *Path);
			return;
		}
		const FString Header = FString::Printf(TEXT("[%s]"), AiSection);
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
		UE_LOG(LogBIBAngelAI, Log, TEXT("Angel AI: %d values read from %s."), Count, *Path);
	}

	void ReloadCommand()
	{
		BIB::Angel::ResetAiTuning();
		LoadFromFile();
		GLoaded = true;
	}

	void SetCommand(const TArray<FString>& Args)
	{
		BIBAngelConfig::Load();
		if (Args.Num() >= 2)
		{
			if (SetValue(Args[0], Args[1]))
			{
				UE_LOG(LogBIBAngelAI, Log, TEXT("Angel AI: %s = %s"), *Args[0], *Args[1]);
			}
			return;
		}
		for (const BIB::Angel::FAiConfigKey& Entry : BIB::Angel::AiConfigKeys(BIB::Angel::AiTuning()))
		{
			UE_LOG(LogBIBAngelAI, Log, TEXT("%s=%g"), UTF8_TO_TCHAR(Entry.Name.c_str()), *Entry.Value);
		}
	}

	const TCHAR* RoleLabel(int32 Role)
	{
		static const TCHAR* Names[] = {TEXT("Angel"), TEXT("Heavy"), TEXT("Ranged"), TEXT("Mage"), TEXT("Assassin"), TEXT("MiniBoss"), TEXT("Boss")};
		return Role >= 0 && Role < 7 ? Names[Role] : TEXT("?");
	}

	void StatsCommand()
	{
		const BIB::Angel::FAiStats& Stats = BIB::Angel::AiStats();
		for (int32 Role = 0; Role < 7; ++Role)
		{
			FString Line = FString::Printf(TEXT("%-9s"), RoleLabel(Role));
			for (int32 Behavior = 0; Behavior < BIB::Angel::BehaviorCount; ++Behavior)
			{
				Line += FString::Printf(TEXT(" %s=%d"), UTF8_TO_TCHAR(BIB::Angel::BehaviorName(static_cast<BIB::Angel::EBehavior>(Behavior))),
					Stats.Behaviors[Role][Behavior]);
			}
			Line += TEXT(" |");
			for (int32 Move = 0; Move < static_cast<int32>(BIB::Angel::EMove::Count); ++Move)
			{
				if (Stats.Moves[Role][Move] > 0)
				{
					Line += FString::Printf(TEXT(" %s=%d"), UTF8_TO_TCHAR(BIB::Angel::MoveName(static_cast<BIB::Angel::EMove>(Move))), Stats.Moves[Role][Move]);
				}
			}
			UE_LOG(LogBIBAngelAI, Log, TEXT("%s"), *Line);
		}
		UE_LOG(LogBIBAngelAI, Log, TEXT("evasions=%d punishes=%d"), Stats.Evasions, Stats.Punishes);
	}

	void ResetStatsCommand()
	{
		BIB::Angel::ResetAiStats();
		UE_LOG(LogBIBAngelAI, Log, TEXT("Angel AI: stats cleared."));
	}

	bool ParseKind(const FString& Text, EBIBEnemyKind& Out)
	{
		// In EBIBEnemyKind order.
		static const TCHAR* Names[7] = {TEXT("Grunt"), TEXT("Brute"), TEXT("MiniBoss"), TEXT("Boss"), TEXT("Ranged"), TEXT("Mage"), TEXT("Assassin")};
		for (int32 Index = 0; Index < 7; ++Index)
		{
			if (Text.Equals(Names[Index], ESearchCase::IgnoreCase))
			{
				Out = static_cast<EBIBEnemyKind>(Index);
				return true;
			}
		}
		if (Text.Equals(TEXT("Angel"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("Melee"), ESearchCase::IgnoreCase))
		{
			Out = EBIBEnemyKind::Grunt;
			return true;
		}
		if (Text.Equals(TEXT("Heavy"), ESearchCase::IgnoreCase))
		{
			Out = EBIBEnemyKind::Brute;
			return true;
		}
		return false;
	}

	void SpawnCommand(const TArray<FString>& Args, UWorld* World)
	{
		EBIBEnemyKind Kind = EBIBEnemyKind::Grunt;
		if (!World || Args.Num() < 1 || !ParseKind(Args[0], Kind))
		{
			UE_LOG(LogBIBAngelAI, Warning, TEXT("BIBSpawnAngel <Grunt|Brute|Ranged|Mage|Assassin|MiniBoss|Boss> [Count]"));
			return;
		}
		ABIBGameMode* Mode = World->GetAuthGameMode<ABIBGameMode>();
		ABIBPlayerCharacter* Commander = Mode ? Mode->GetPlayerCharacter() : nullptr;
		if (!Mode || !Commander)
		{
			UE_LOG(LogBIBAngelAI, Warning, TEXT("BIBSpawnAngel: no stage in play."));
			return;
		}
		const int32 Count = Args.Num() >= 2 ? FMath::Clamp(FCString::Atoi(*Args[1]), 1, 12) : 1;
		const FVector Forward = Commander->GetActorForwardVector().GetSafeNormal2D();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Offset = Forward.RotateAngleAxis(-40.0 + 80.0 * (Count > 1 ? static_cast<double>(Index) / (Count - 1) : 0.5), FVector::UpVector) * 900.0;
			Mode->DebugSpawnEnemy(Kind, Commander->GetActorLocation() + Offset + FVector(0.0, 0.0, 60.0));
		}
		UE_LOG(LogBIBAngelAI, Log, TEXT("BIBSpawnAngel: %d x %s."), Count, *Args[0]);
	}

	FAutoConsoleCommand GBIBAngelReload(TEXT("BIBAngelReload"), TEXT("Re-reads [BIB.AngelAI] from Config/DefaultGame.ini."),
		FConsoleCommandDelegate::CreateStatic(&ReloadCommand));
	FAutoConsoleCommand GBIBAngelSet(TEXT("BIBAngel"), TEXT("BIBAngel <Key> <Value> sets an angel AI knob; no arguments lists them."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetCommand));
	FAutoConsoleCommand GBIBAngelStats(TEXT("BIBAngelStats"), TEXT("Logs which behaviours and moves each angel type used."),
		FConsoleCommandDelegate::CreateStatic(&StatsCommand));
	FAutoConsoleCommand GBIBAngelResetStats(TEXT("BIBAngelResetStats"), TEXT("Clears the angel AI counters (Tools/verify_angels.py)."),
		FConsoleCommandDelegate::CreateStatic(&ResetStatsCommand));
	FAutoConsoleCommandWithWorldAndArgs GBIBSpawnAngel(TEXT("BIBSpawnAngel"), TEXT("BIBSpawnAngel <Kind> [Count]: spawns angels in front of the commander (tests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnCommand));
}

namespace BIBAngelConfig
{
	void Load(bool bForce)
	{
		if (BIBAngelConfigImpl::GLoaded && !bForce)
		{
			return;
		}
		BIBAngelConfigImpl::GLoaded = true;
		BIB::Angel::ResetAiTuning();
		BIBAngelConfigImpl::LoadFromGConfig();
	}
}

ABIBGameMode* UBIBAngelDirector::GetMode() const
{
	UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ABIBGameMode>() : nullptr;
}

TArray<ABIBEnemy*> UBIBAngelDirector::LiveAngels() const
{
	const ABIBGameMode* Mode = GetMode();
	return Mode ? Mode->GetLiveEnemies() : TArray<ABIBEnemy*>();
}

int32 UBIBAngelDirector::GetChapter() const
{
	const ABIBGameMode* Mode = GetMode();
	if (!Mode)
	{
		return 1;
	}
	const BIB::FStageRef Stage = BIB::DecodeStage(Mode->GetCurrentStageId());
	return Stage.Kind == BIB::EStageKind::Campaign ? FMath::Clamp(Stage.A, 1, 13) : 7;
}

void UBIBAngelDirector::UpdateFrame()
{
	if (LastFrame == GFrameCounter)
	{
		return;
	}
	LastFrame = GFrameCounter;
	BIBAngelConfig::Load();
	ABIBGameMode* Mode = GetMode();
	ABIBPlayerCharacter* Commander = Mode ? Mode->GetPlayerCharacter() : nullptr;
	if (!Commander)
	{
		return;
	}
	const BIB::Angel::FPlayerView View = Commander->GetAngelView();
	if (bHasView && View.Time < LastView.Time)
	{
		// A new stage / world time restarted.
		Timeline.Clear();
		Crowd.Reset();
		bHasView = false;
	}
	Timeline.Record(View);
	if (bHasView)
	{
		Habits.Observe(LastView, View);
	}
	LastView = View;
	bHasView = true;

	// The crowd: tokens, ring points and ranged turns from what the angels see. The mind learns at the fastest tier present.
	const TArray<ABIBEnemy*> Live = Mode->GetLiveEnemies();
	std::vector<BIB::Angel::FCrowdAgent> Agents;
	Agents.reserve(Live.Num());
	double LearnMult = 0.0;
	double Aggression = 0.0;
	const int32 Chapter = GetChapter();
	for (const ABIBEnemy* Angel : Live)
	{
		BIB::Angel::FCrowdAgent Agent;
		Agent.Id = Angel->GetBrainId();
		Agent.Role = Angel->GetRole();
		Agent.Pos = {Angel->GetActorLocation().X, Angel->GetActorLocation().Y};
		Agent.Reach = Angel->GetAttackRange();
		Agent.bCommitted = Angel->IsCommittedForCrowd();
		Agent.bSees = Angel->SeesCommander();
		Agents.push_back(Agent);
		const BIB::Angel::FKnobs Knobs = BIB::Angel::EffectiveKnobs(BIB::Angel::TierFor(Agent.Role), Chapter);
		LearnMult = FMath::Max(LearnMult, Knobs.AdaptRate);
		Aggression = FMath::Max(Aggression, Knobs.Aggression);
	}
	UBIBGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance<UBIBGameInstance>() : nullptr;
	BIB::Angel::FMind* Mind = Game ? &Game->GetAngelMind() : nullptr;
	if (Mind && LearnMult > 0.0)
	{
		Mind->SetLearnMult(LearnMult);
	}
	Crowd.Update(View.Time, View, Agents, Mind ? Mind->Adaptation() : 0.0, Aggression);
}

BIB::Angel::FPlayerView UBIBAngelDirector::ViewFor(double ReactionSeconds) const
{
	return Timeline.At(LastView.Time - FMath::Max(0.0, ReactionSeconds));
}

bool UBIBAngelDirector::HealingFor(double ReactionSeconds) const
{
	return Timeline.Healing(LastView.Time - FMath::Max(0.0, ReactionSeconds));
}

void UBIBAngelDirector::NoteSighting(const FVector& From, double Time)
{
	LastSightingAt = Time;
	LastSightingFrom = From;
}

bool UBIBAngelDirector::HeardSighting(const FVector& Near, double Now, double& OutSeenAt) const
{
	if (Now - LastSightingAt > 1.0 || FVector::Dist2D(Near, LastSightingFrom) > BIBAngelSenses::CallOutRange)
	{
		return false;
	}
	OutSeenAt = LastSightingAt - BIBAngelSenses::CallOutDelay;
	return true;
}
