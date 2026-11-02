#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBVideoScreen.h"
#include "BIBGame/BIBMotionEvents.h"
#include "BIBGame/BIBTouch.h"
#include "BIBGame/BIBAnimeStyle.h"
#include "BIBGame/BIBArkStation.h"
#include "BIBGame/BIBWeather.h"

#include "BIBGame/BIBBlock.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBEnvironment.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBLevelActors.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBGame/BIBTuning.h"
#include "BIBRules/BIBArcade.h"
#include "BIBRules/BIBArk.h"
#include "BIBRules/BIBCampaign.h"
#include "BIBRules/BIBCore.h"
#include "BIBRules/BIBElement.h"
#include "BIBRules/BIBMetaOps.h"

#include "Algo/Sort.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace BIBGameModeLayout
{
	constexpr double StartHalf = BIBTuning::StartRoomHalfSize;
	constexpr double SectionHalf = BIBTuning::SectionHalfSize;
	constexpr double VisibleWallHeight = BIBTuning::WallHeight;
	constexpr double CeilingHeight = BIBTuning::CeilingHeight;
	const FLinearColor WallColour(0.32f, 0.33f, 0.38f);
	// Where a code-built stage goes when the current map was designed in the editor (keeps clear of its geometry).
	const FVector DesignedLevelCodeOrigin(0.0, 120000.0, 0.0);
}

ABIBGameMode::ABIBGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ABIBPlayerCharacter::StaticClass();
	PlayerControllerClass = ABIBPlayerController::StaticClass();
}

BIB::FProfile& ABIBGameMode::GetProfile() const
{
	if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
	{
		return Instance->GetProfile();
	}
	// Fallback when the project's GameInstanceClass setting was not applied.
	static BIB::FProfile FallbackProfile(1234u);
	return FallbackProfile;
}

BIB::Pages::FUiState& ABIBGameMode::GetUiState() const
{
	if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
	{
		return Instance->UiState;
	}
	static BIB::Pages::FUiState FallbackState;
	return FallbackState;
}

ABIBPlayerController* ABIBGameMode::GetBIBController() const
{
	return GetWorld() ? Cast<ABIBPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
}

ABIBPlayerCharacter* ABIBGameMode::GetPlayerCharacter() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ABIBPlayerCharacter>(PC->GetPawn()) : nullptr;
}

void ABIBGameMode::AddMessage(const FString& Text)
{
	Messages.Add(Text);
	while (Messages.Num() > 8)
	{
		Messages.RemoveAt(0);
	}
}

// ============================================================================ world setup

bool ABIBGameMode::LevelHasDesignedContent() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (TActorIterator<APlayerStart>(World))
	{
		return true;
	}
	if (TActorIterator<ABIBSection>(World))
	{
		return true;
	}
	if (TActorIterator<ABIBStoryNPC>(World))
	{
		return true;
	}
	return false;
}

void ABIBGameMode::EnsureWorld()
{
	if (bWorldChecked || !GetWorld())
	{
		return;
	}
	bWorldChecked = true;
	bDesignedLevel = LevelHasDesignedContent();
	if (!bDesignedLevel)
	{
		// Empty map (e.g. /Engine/Maps/Entry): build the blockout hub in code.
		CodeOrigin = FVector::ZeroVector;
		BuildCodeHub(true);
		BuildHubEastWall();
	}
}

AActor* ABIBGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	EnsureWorld();
	if (!bDesignedLevel && SpawnMarker)
	{
		return SpawnMarker.Get();
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void ABIBGameMode::StartPlay()
{
	EnsureWorld();
	if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
	{
		if (Instance->PendingStage > 0)
		{
			// This map was opened by StartStage: begin that stage as soon as the player exists.
			PendingBeginStage = Instance->PendingStage;
			Instance->PendingStage = 0;
			bStageFromMap = true;
		}
		if (!Instance->PendingFreeRoam.IsEmpty())
		{
			// Opened by StartFreeRoam: the player walks the map with their team once the pawn exists (see Tick).
			FreeRoamMap = Instance->PendingFreeRoam;
			Instance->PendingFreeRoam.Reset();
			WalkStage = Instance->PendingWalkStage;
			Instance->PendingWalkStage = 0;
			bPlainWorld = WalkStage > 0;
			bStageFromMap = true;
		}
	}
	// Test / profiling hook: `<map>?game=/Script/BoldInBeautyDemo.BIBGameMode -BIBFreeRoam` walks any greybox map
	// (Neo Tokyo, the chapter maps) with the team, like the Campaign tab's free roam (Tools/perf_capture.sh).
	if (FreeRoamMap.IsEmpty() && PendingBeginStage == 0 && !IsArkWorld() && FParse::Param(FCommandLine::Get(), TEXT("BIBFreeRoam"))
		&& UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()).StartsWith(TEXT("/Game/Greybox/Maps/")))
	{
		FreeRoamMap = FPackageName::GetShortName(GetWorld()->GetOutermost()->GetName());
		bStageFromMap = true;
	}
	// The Ark (the hub since 2026-10-07): walk it once the pawn exists (see Tick).
	bArkPending = IsArkWorld() && PendingBeginStage == 0 && FreeRoamMap.IsEmpty();
	Super::StartPlay();
}

// ============================================================================ code-built blockout

ABIBBlock* ABIBGameMode::AddBlock(TArray<TObjectPtr<AActor>>& Into, const FVector& Centre, const FVector& Size, const FLinearColor& Colour, bool bVisible)
{
	ABIBBlock* Block = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Cube, CodeOrigin + Centre, Size, Colour, true, bVisible);
	if (Block)
	{
		Into.Add(Block);
	}
	return Block;
}

void ABIBGameMode::AddWallAlongY(TArray<TObjectPtr<AActor>>& Into, double X, double HalfWidth, bool bDoorGap)
{
	using namespace BIBGameModeLayout;
	const double UpperHeight = CeilingHeight - VisibleWallHeight;
	auto AddSegment = [&](double CentreY, double Length)
	{
		if (ABIBBlock* Lower = AddBlock(Into, FVector(X, CentreY, VisibleWallHeight * 0.5), FVector(100.0, Length, VisibleWallHeight), WallColour))
		{
			Lower->MarkAsWall();
		}
		// Invisible upper part so enemies under inverted gravity stay inside.
		if (ABIBBlock* Upper = AddBlock(Into, FVector(X, CentreY, VisibleWallHeight + UpperHeight * 0.5), FVector(100.0, Length, UpperHeight), WallColour, false))
		{
			Upper->MarkAsWall();
		}
	};
	if (!bDoorGap)
	{
		AddSegment(0.0, HalfWidth * 2.0 + 100.0);
		return;
	}
	const double HalfDoor = BIBTuning::DoorWidth * 0.5;
	const double Length = HalfWidth + 50.0 - HalfDoor;
	AddSegment(-(HalfDoor + Length * 0.5), Length);
	AddSegment(HalfDoor + Length * 0.5, Length);
}

void ABIBGameMode::AddWallAlongX(TArray<TObjectPtr<AActor>>& Into, double Y, double MinX, double MaxX)
{
	using namespace BIBGameModeLayout;
	const double UpperHeight = CeilingHeight - VisibleWallHeight;
	const double CentreX = (MinX + MaxX) * 0.5;
	const double Length = MaxX - MinX + 100.0;
	if (ABIBBlock* Lower = AddBlock(Into, FVector(CentreX, Y, VisibleWallHeight * 0.5), FVector(Length, 100.0, VisibleWallHeight), WallColour))
	{
		Lower->MarkAsWall();
	}
	if (ABIBBlock* Upper = AddBlock(Into, FVector(CentreX, Y, VisibleWallHeight + UpperHeight * 0.5), FVector(Length, 100.0, UpperHeight), WallColour, false))
	{
		Upper->MarkAsWall();
	}
}

void ABIBGameMode::AddRoom(TArray<TObjectPtr<AActor>>& Into, double MinX, double MaxX, double HalfWidth, const FLinearColor& FloorColour)
{
	using namespace BIBGameModeLayout;
	const double CentreX = (MinX + MaxX) * 0.5;
	AddBlock(Into, FVector(CentreX, 0.0, -50.0), FVector(MaxX - MinX + 100.0, HalfWidth * 2.0 + 100.0, 100.0), FloorColour);
	AddBlock(Into, FVector(CentreX, 0.0, CeilingHeight + 50.0), FVector(MaxX - MinX + 100.0, HalfWidth * 2.0 + 100.0, 100.0), FloorColour, false);
	AddWallAlongX(Into, HalfWidth, MinX, MaxX);
	AddWallAlongX(Into, -HalfWidth, MinX, MaxX);
}

void ABIBGameMode::BuildCodeHub(bool bWithLights)
{
	using namespace BIBGameModeLayout;
	if (bCodeHubBuilt)
	{
		return;
	}
	bCodeHubBuilt = true;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (bWithLights)
	{
		if (AActor* Lights = GetWorld()->SpawnActor<ABIBEnvironment>(ABIBEnvironment::StaticClass(), CodeOrigin, FRotator::ZeroRotator, Params))
		{
			HubActors.Add(Lights);
		}
	}
	// Start room: the story contact stands here.
	AddRoom(HubActors, -StartHalf, StartHalf, StartHalf, FLinearColor(0.18f, 0.2f, 0.26f));
	AddWallAlongY(HubActors, -StartHalf, StartHalf, false);

	CodeNpc = GetWorld()->SpawnActor<ABIBStoryNPC>(ABIBStoryNPC::StaticClass(), CodeOrigin + FVector(700.0, 0.0, 110.0), FRotator(0.0, 180.0, 0.0), Params);
	if (CodeNpc)
	{
		HubActors.Add(CodeNpc);
	}
	SpawnMarker = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Cube, CodeOrigin + FVector(-900.0, 0.0, 120.0), FVector(10.0), FLinearColor::Black, false, false);
	if (SpawnMarker)
	{
		HubActors.Add(SpawnMarker);
	}
}

void ABIBGameMode::BuildHubEastWall()
{
	AddWallAlongY(StageActors, BIBGameModeLayout::StartHalf, BIBGameModeLayout::StartHalf, false);
}

double ABIBGameMode::SectionMinX(int32 SectionIndex) const
{
	return BIBGameModeLayout::StartHalf + BIBGameModeLayout::SectionHalf * 2.0 * SectionIndex;
}

TArray<FBIBSectionPlan> ABIBGameMode::MakePlan(int32 InStage) const
{
	// MAIN §7: 2-5 sections per stage, 10-30 enemies per section, a mini-boss every stage, a boss only at the
	// chapter end. The compositions live in BIBRules/BIBMeta.cpp (Chapter 1) and BIBRules/BIBCampaign.cpp (chapters 2-13)
	// and are PLACEHOLDERS.
	TArray<FBIBSectionPlan> Result;
	for (const BIB::FSectionPlanData& Data : BIB::StagePlan(InStage))
	{
		FBIBSectionPlan Section;
		Section.Grunts = Data.Grunts;
		Section.Brutes = Data.Brutes;
		Section.Ranged = Data.Ranged;
		Section.Mages = Data.Mages;
		Section.Assassins = Data.Assassins;
		Section.MiniBosses = Data.MiniBosses;
		Section.Bosses = Data.Bosses;
		Result.Add(Section);
	}
	return Result;
}

void ABIBGameMode::BuildCodeStage()
{
	using namespace BIBGameModeLayout;
	const TArray<FBIBSectionPlan> Plan = MakePlan(CurrentStage);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < Plan.Num(); ++Index)
	{
		const double MinX = SectionMinX(Index);
		const double MaxX = MinX + SectionHalf * 2.0;
		const FLinearColor Floor = (Index % 2 == 0) ? FLinearColor(0.12f, 0.12f, 0.16f) : FLinearColor(0.16f, 0.12f, 0.14f);
		AddRoom(StageActors, MinX, MaxX, SectionHalf, Floor);
		AddWallAlongY(StageActors, MinX, SectionHalf, true);

		ABIBDoor* Door = GetWorld()->SpawnActor<ABIBDoor>(ABIBDoor::StaticClass(), CodeOrigin + FVector(MinX, 0.0, 0.0), FRotator::ZeroRotator, Params);
		if (Door)
		{
			Door->SetDoorSize(FVector(120.0, BIBTuning::DoorWidth + 10.0, VisibleWallHeight), static_cast<float>(CeilingHeight));
			StageActors.Add(Door);
		}
		ABIBSection* Section = GetWorld()->SpawnActor<ABIBSection>(ABIBSection::StaticClass(),
			CodeOrigin + FVector(MinX + SectionHalf, 0.0, CeilingHeight * 0.5), FRotator::ZeroRotator, Params);
		if (Section)
		{
			Section->SetAreaSize(FVector(SectionHalf, SectionHalf, CeilingHeight * 0.5));
			Section->StageNumber = CurrentStage;
			Section->SectionIndex = Index;
			Section->EntryDoor = Door;
			Section->Grunts = Plan[Index].Grunts;
			Section->Brutes = Plan[Index].Brutes;
			Section->Ranged = Plan[Index].Ranged;
			Section->Mages = Plan[Index].Mages;
			Section->Assassins = Plan[Index].Assassins;
			Section->MiniBosses = Plan[Index].MiniBosses;
			Section->Bosses = Plan[Index].Bosses;
			StageActors.Add(Section);
			Sections.Add(Section);
		}
		if (ABIBBlock* Sign = ABIBBlock::SpawnBlock(GetWorld(), EBIBShape::Cube, CodeOrigin + FVector(MinX - 150.0, BIBTuning::DoorWidth * 0.5 + 120.0, 300.0),
			FVector(40.0), FLinearColor(1.f, 1.f, 1.f), false))
		{
			Sign->SetLabel(FString::Printf(TEXT("Section %d"), Index + 1), FLinearColor::White, 80.f, 70.f);
			StageActors.Add(Sign);
		}
	}
	AddWallAlongY(StageActors, SectionMinX(Plan.Num()), SectionHalf, false); // far east wall
}

void ABIBGameMode::CollectDesignedSections()
{
	for (TActorIterator<ABIBSection> It(GetWorld()); It; ++It)
	{
		if (It->StageNumber == CurrentStage)
		{
			Sections.Add(*It);
		}
	}
	Algo::Sort(Sections, [](const TObjectPtr<ABIBSection>& A, const TObjectPtr<ABIBSection>& B) { return A->SectionIndex < B->SectionIndex; });
}

void ABIBGameMode::ClearStage()
{
	for (const TWeakObjectPtr<ABIBEnemy>& Enemy : Enemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->Destroy();
		}
	}
	Enemies.Reset();
	for (AActor* Actor : StageActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	StageActors.Reset();
	Sections.Reset();
}

void ABIBGameMode::SetSectionDoor(int32 SectionIndex, bool bOpen)
{
	if (Sections.IsValidIndex(SectionIndex) && Sections[SectionIndex] && Sections[SectionIndex]->EntryDoor)
	{
		Sections[SectionIndex]->EntryDoor->SetOpen(bOpen);
		if (bOpen)
		{
			BIBMotionEvents::Shared(TEXT("section-door-unlock-glow"), Sections[SectionIndex]->EntryDoor, false, TEXT("door"));
		}
	}
}

// ============================================================================ stage flow

FString ABIBGameMode::GetStageName(int32 InStage)
{
	return FString(UTF8_TO_TCHAR(BIB::StageName(InStage).c_str()));
}

bool ABIBGameMode::StartStage(int32 InStage, FString& OutError)
{
	if (!GetPlayerCharacter())
	{
		OutError = TEXT("No player character.");
		return false;
	}
	if (!BIB::IsValidStage(InStage))
	{
		OutError = TEXT("That stage does not exist.");
		return false;
	}
	const BIB::FActionResult Can = BIB::Meta::CanStartStage(GetProfile(), InStage);
	if (!Can.bOk)
	{
		OutError = FString(UTF8_TO_TCHAR(Can.Message.c_str()));
		return false;
	}
	const BIB::FStageRef Ref = BIB::DecodeStage(InStage);
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (BIB::Campaign::IsWalkStage(InStage))
	{
		// Return to Nowhere (owner level idea, final): no levels, no enemies, no bosses - walk the Chapter 11 map to its exit.
		if (!StartFreeRoam(TEXT("Nowhere"), OutError))
		{
			return false;
		}
		if (Instance)
		{
			Instance->PendingWalkStage = BIB::Campaign::WalkEntryStage();
		}
		return true;
	}
	SaveArkPosition(true);   // coming back from the stage puts the player where they left the Ark
	// The editor maps only exist for Chapter 1 (its eight levels). Maps built for an older plan (a different number of
	// levels) are ignored, and every other chapter is built in code from the same plans.
	const bool bMapsMatchPlan = StageMaps.Num() == BIB::CampaignLevels;
	if (Instance && bMapsMatchPlan && Ref.Kind == BIB::EStageKind::Campaign && Ref.A == 1 && StageMaps.IsValidIndex(Ref.B - 1) && !StageMaps[Ref.B - 1].IsNull())
	{
		// Editor-built level map: travel there; its game mode begins the stage on load.
		Instance->PendingStage = InStage;
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, StageMaps[Ref.B - 1]);
		return true;
	}
	if (IsArkWorld())
	{
		// Code-built stages (chapters 2-13, tower, arena, Boss Rush, sub-stories) are built in the old hub map, which is
		// out of the way and has the day / night and weather; finishing one travels back to the Ark (ReturnToHub).
		if (!Instance || HubMap.IsNull())
		{
			OutError = TEXT("No stage map to build this level in.");
			return false;
		}
		Instance->PendingStage = InStage;
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, HubMap);
		return true;
	}
	bStageFromMap = false;
	BeginStage(InStage);
	return true;
}

namespace
{
	// Return to Nowhere: how close (cm, top-down) to the exit marker the player must walk to leave. The path is 14 m wide.
	constexpr float WalkExitRadius = 700.f;

	// Free-roam maps (Campaign tab). The greybox maps are generated by Tools/build_greybox.sh (git-ignored content).
	FString FreeRoamPackage(const FString& MapId)
	{
		if (MapId == TEXT("NeoTokyo"))
		{
			return TEXT("/Game/Greybox/Maps/L_NeoTokyo");
		}
		if (MapId == TEXT("Nowhere"))
		{
			return TEXT("/Game/Greybox/Maps/L_Ch11_ReturnToNowhere");   // Chapter 11, a walk (BIB::Campaign::WalkChapter)
		}
		return FString();
	}
}

bool ABIBGameMode::StartFreeRoam(const FString& MapId, FString& OutError)
{
	const FString Package = FreeRoamPackage(MapId);
	if (Package.IsEmpty())
	{
		OutError = TEXT("That level does not exist.");
		return false;
	}
	if (!FPackageName::DoesPackageExist(Package))
	{
		OutError = TEXT("This map is not built on this machine yet (run Tools/build_greybox.sh).");
		return false;
	}
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (!Instance)
	{
		OutError = TEXT("No game instance.");
		return false;
	}
	SaveArkPosition(true);
	Instance->PendingFreeRoam = MapId;
	// The greybox map's own World Settings use the engine's free-fly game mode; ?game= runs this one instead.
	UGameplayStatics::OpenLevel(this, FName(*Package), true, TEXT("game=") + GetClass()->GetPathName());
	return true;
}

void ABIBGameMode::BeginStage(int32 InStage)
{
	ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	if (!PlayerChar)
	{
		PendingBeginStage = InStage;
		return;
	}
	EnsureWorld();
	ClearStage();
	CurrentStage = InStage;
	CurrentSection = 0;
	BossRushNextRoundAt = -1.f;
	DamagePopups.Reset();

	CollectDesignedSections();
	const bool bCodeStage = Sections.Num() == 0;
	if (bCodeStage)
	{
		if (bDesignedLevel)
		{
			// The map has no sections for this stage: build the blockout well away from the map's own geometry.
			CodeOrigin = BIBGameModeLayout::DesignedLevelCodeOrigin;
			BuildCodeHub(false);
		}
		BuildCodeStage();
		Npc = CodeNpc;
	}
	else
	{
		Npc = nullptr;
		for (TActorIterator<ABIBStoryNPC> It(GetWorld()); It; ++It)
		{
			if (*It != CodeNpc)
			{
				Npc = *It;
				break;
			}
		}
	}
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		SetSectionDoor(Index, false);
	}

	int32 StageTeam[3];
	BIB::Meta::EffectiveTeam(GetProfile(), CurrentStage, StageTeam); // tower modes only admit eligible operators
	PlayerChar->SetupTeam(GetProfile(), StageTeam);
	PlayerChar->SetCombatActive(true);
	{
		// The motion pack: transition 04 into a stage (05 into a chapter's boss level), the skill intro; the tower's floor
		// counter and the boss rush round as they start.
		const BIB::FStageRef StageRef = BIB::DecodeStage(CurrentStage);
		const bool bBossLevel = StageRef.Kind == BIB::EStageKind::Campaign && CurrentStage > 0 && CurrentStage % BIB::CampaignLevels == 0;
		BIBMotionEvents::StageStarted(bBossLevel, BIBTouch::Enabled(), FString());
		if (StageRef.Kind == BIB::EStageKind::Tower)
		{
			BIBMotionEvents::TowerFloor(StageRef.B);
		}
		else if (StageRef.Kind == BIB::EStageKind::BossRush)
		{
			BIBMotionEvents::BossRushRound(FMath::Max(1, StageRef.B));
		}
	}
	TeleportPlayerToStart();

	DialogueLines.Reset();
	if (Npc && Npc->DialogueLines.Num() > 0)
	{
		DialogueLines = Npc->DialogueLines;
	}
	else
	{
		DialogueLines.Add(FString::Printf(TEXT("[Story conversation for %s goes here - the owner writes this dialogue.]"), *GetStageName(CurrentStage)));
		DialogueLines.Add(TEXT("MAIN 7: you progress to each stage by talking to someone to advance the story, which leads to battle."));
		DialogueLines.Add(FString::Printf(TEXT("This stage has %d sections. Defeat every enemy in a section to open the next door."), Sections.Num()));
	}
	DialogueIndex = -1;
	Messages.Reset();
	if (Npc && BIB::DecodeStage(CurrentStage).Kind != BIB::EStageKind::BossRush)   // Boss Rush goes straight to the arena
	{
		Flow = EBIBFlow::Talk;
		AddMessage(FString::Printf(TEXT("%s started. Talk to the story contact (T)."), *GetStageName(CurrentStage)));
	}
	else
	{
		// A designed map without a Story NPC goes straight to the first section.
		SetSectionDoor(0, true);
		Flow = EBIBFlow::Explore;
		AddMessage(FString::Printf(TEXT("%s started."), *GetStageName(CurrentStage)));
	}
	// Boss Rush starts inside the arena with the first boss already there.
	if (BIB::DecodeStage(CurrentStage).Kind == BIB::EStageKind::BossRush && Sections.IsValidIndex(0) && Sections[0])
	{
		const FVector Centre = Sections[0]->GetActorLocation();
		PlayerChar->SetActorLocation(FVector(Centre.X - BIBTuning::SectionHalfSize * 0.5, Centre.Y, PlayerChar->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		SetSectionDoor(0, false);
		SpawnSection(0);
		Flow = EBIBFlow::Fight;
		AddMessage(TEXT("Boss Rush - round 1! Beat every boss; each round pays."));
	}
	if (ABIBPlayerController* PC = GetBIBController())
	{
		PC->SetScreen(EBIBScreen::None);
	}
}

void ABIBGameMode::TeleportPlayerToStart()
{
	ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	if (!PlayerChar)
	{
		return;
	}
	FVector Location = PlayerChar->GetActorLocation();
	double Yaw = 0.0;
	if (SpawnMarker && (!bDesignedLevel || CodeOrigin != FVector::ZeroVector))
	{
		Location = SpawnMarker->GetActorLocation();
	}
	else
	{
		if (TActorIterator<APlayerStart> It(GetWorld()); It)
		{
			Location = It->GetActorLocation();
			Yaw = It->GetActorRotation().Yaw;
		}
	}
	PlayerChar->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	PlayerChar->GetCharacterMovement()->StopMovementImmediately();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetControlRotation(FRotator(-12.0, Yaw, 0.0));
	}
}

void ABIBGameMode::SpawnEnemy(EBIBEnemyKind Kind, const FVector& Location)
{
	TSubclassOf<ABIBEnemy> EnemyClass = EnemyClasses.FindRef(Kind);
	if (!EnemyClass)
	{
		EnemyClass = ABIBEnemy::StaticClass();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABIBEnemy* Enemy = GetWorld()->SpawnActor<ABIBEnemy>(EnemyClass, Location, FRotator(0.0, 180.0, 0.0), Params);
	if (Enemy)
	{
		Enemy->InitEnemy(Kind, this);
		const BIB::FEnemyScale Scale = BIB::StageScales(CurrentStage);
		// The chapter boss's slam was never scaled in Chapter 1 and stays that way; from Chapter 2 it follows the damage scale.
		const bool bScaleSlam = BIB::DecodeStage(CurrentStage).Kind == BIB::EStageKind::Campaign && CurrentStage > BIB::CampaignLevels;
		Enemy->ApplyStatScales(static_cast<float>(Scale.Hp), static_cast<float>(Scale.Damage), bScaleSlam ? static_cast<float>(Scale.Damage) : 1.f);
		// Element of the stage / section (owner request 2026-10-07, BIBRules/BIBElement.h).
		Enemy->SetElement(static_cast<int32>(BIB::Element::OfStage(CurrentStage, CurrentSection)));
		Enemies.Add(Enemy);
	}
}

void ABIBGameMode::SpawnSection(int32 SectionIndex)
{
	if (!Sections.IsValidIndex(SectionIndex) || !Sections[SectionIndex])
	{
		return;
	}
	const ABIBSection* Section = Sections[SectionIndex];
	BIBMotionEvents::Section(false, SectionIndex + 1, Sections.Num(), TEXT("Defeat every angel"));
	int32 SpawnIndex = 0;
	auto SpawnKind = [&](EBIBEnemyKind Kind, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			SpawnEnemy(Kind, Section->FindSpawnLocation(SpawnIndex++));
		}
	};
	SpawnKind(EBIBEnemyKind::Grunt, Section->Grunts);
	SpawnKind(EBIBEnemyKind::Brute, Section->Brutes);
	SpawnKind(EBIBEnemyKind::Ranged, Section->Ranged);
	SpawnKind(EBIBEnemyKind::Mage, Section->Mages);
	SpawnKind(EBIBEnemyKind::Assassin, Section->Assassins);
	SpawnKind(EBIBEnemyKind::MiniBoss, Section->MiniBosses);
	SpawnKind(EBIBEnemyKind::Boss, Section->Bosses);
	AddMessage(FString::Printf(TEXT("Section %d: %d enemies%s%s."), SectionIndex + 1, Section->TotalEnemies(),
		Section->MiniBosses > 0 ? TEXT(" incl. MINI-BOSS") : TEXT(""), Section->Bosses > 0 ? TEXT(" and CHAPTER BOSS") : TEXT("")));
}

TArray<ABIBEnemy*> ABIBGameMode::GetLiveEnemies() const
{
	TArray<ABIBEnemy*> Result;
	for (const TWeakObjectPtr<ABIBEnemy>& Enemy : Enemies)
	{
		if (Enemy.IsValid() && Enemy->IsAlive())
		{
			Result.Add(Enemy.Get());
		}
	}
	return Result;
}

int32 ABIBGameMode::GetEnemiesRemaining() const
{
	return GetLiveEnemies().Num();
}

void ABIBGameMode::OnEnemyKilled(ABIBEnemy* Enemy)
{
	if (Enemy && Enemy->IsBoss())
	{
		AddMessage(FString::Printf(TEXT("%s defeated!"), *Enemy->GetKindName()));
	}
}

void ABIBGameMode::AddDamagePopup(const FVector& Location, float Amount, bool bBig)
{
	if (Amount <= 0.f || !GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	DamagePopups.RemoveAll([Now](const FBIBDamagePopup& Popup) { return Now - Popup.Time > 1.f; });
	if (DamagePopups.Num() < 64)
	{
		FBIBDamagePopup Popup;
		Popup.Location = Location + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 0.f);
		Popup.Amount = Amount;
		Popup.Time = Now;
		Popup.bBig = bBig;
		DamagePopups.Add(Popup);
	}
}

int32 ABIBGameMode::GetBossRushRound() const
{
	const BIB::FStageRef Ref = BIB::DecodeStage(CurrentStage);
	return IsInStage() && Ref.Kind == BIB::EStageKind::BossRush ? Ref.B : 0;
}

void ABIBGameMode::StartNextBossRushRound()
{
	BossRushNextRoundAt = -1.f;
	const BIB::FStageRef Ref = BIB::DecodeStage(CurrentStage);
	const int32 Round = FMath::Min(Ref.B + 1, BIB::Arcade::BossRushMaxRound);
	CurrentStage = BIB::Arcade::BossRushStageId(Ref.A, Round);
	if (Sections.IsValidIndex(CurrentSection) && Sections[CurrentSection])
	{
		const BIB::FSectionPlanData Plan = BIB::Arcade::BossRushRound(Round);
		ABIBSection* Section = Sections[CurrentSection];
		Section->Grunts = Plan.Grunts;
		Section->Brutes = Plan.Brutes;
		Section->Ranged = Plan.Ranged;
		Section->Mages = Plan.Mages;
		Section->Assassins = Plan.Assassins;
		Section->MiniBosses = Plan.MiniBosses;
		Section->Bosses = Plan.Bosses;
		SpawnSection(CurrentSection);
	}
	AddMessage(FString::Printf(TEXT("Boss Rush - round %d!"), Round));
	BIBMotionEvents::BossRushRound(Round);
}

void ABIBGameMode::OnTeamWiped()
{
	FinishStage(false, TEXT("All three operators are down."));
}

void ABIBGameMode::AbandonStage()
{
	if (Flow == EBIBFlow::Roam)
	{
		ReturnToHub();   // nothing to win or lose in free roam
		return;
	}
	if (IsInStage() && Flow != EBIBFlow::Finished)
	{
		FinishStage(false, TEXT("Stage abandoned."));
	}
}

void ABIBGameMode::FinishStage(bool bSuccess, const FString& Reason)
{
	if (Flow == EBIBFlow::Finished)
	{
		return;
	}
	Flow = EBIBFlow::Finished;
	DialogueIndex = -1;
	if (ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter())
	{
		PlayerChar->SetCombatActive(false);
	}
	TArray<FString> Lines;
	Lines.Add(Reason);
	if (bSuccess)
	{
		const std::vector<std::string> Rewards = BIB::Meta::ClearStage(GetProfile(), CurrentStage);
		for (const std::string& Line : Rewards)
		{
			Lines.Add(FString(UTF8_TO_TCHAR(Line.c_str())));
		}
		if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
		{
			Instance->SaveNow(); // a cleared stage (and the tutorial's Plack) must never be lost
		}
	}
	else
	{
		const BIB::FStageRef Ref = BIB::DecodeStage(CurrentStage);
		std::vector<std::string> Extra;
		if (Ref.Kind == BIB::EStageKind::BossRush)
		{
			// Boss Rush ends when the team falls (or the player leaves): every cleared round already paid.
			Extra = BIB::Arcade::BossRushOver(GetProfile(), Ref.B);
		}
		else
		{
			Extra = BIB::Meta::FailStage(GetProfile(), CurrentStage);   // the endless tower saves the floor you fell on
			Lines.Add(TEXT("No rewards. (Failure/retry rules are OPEN in MAIN.)"));
		}
		for (const std::string& Line : Extra)
		{
			Lines.Add(FString(UTF8_TO_TCHAR(Line.c_str())));
		}
		if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
		{
			Instance->SaveNow();
		}
	}
	BossRushNextRoundAt = -1.f;
	if (ABIBPlayerController* PC = GetBIBController())
	{
		PC->ShowResults(Lines, bSuccess);
	}
}

void ABIBGameMode::ReturnToHub()
{
	if (ShouldUseArk() && !IsArkWorld())
	{
		// The 2D lobby is gone: every stage, free roam and arcade fight ends back in the Ark.
		bStageFromMap = false;
		TravelToArk();
		return;
	}
	if (bStageFromMap && !HubMap.IsNull())
	{
		bStageFromMap = false;
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, HubMap);
		return;
	}
	ClearStage();
	if (!bDesignedLevel)
	{
		BuildHubEastWall();
	}
	CodeOrigin = FVector::ZeroVector;
	Flow = EBIBFlow::Hub;
	DialogueIndex = -1;
	if (ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter())
	{
		PlayerChar->SetCombatActive(false);
	}
	TeleportPlayerToStart();
}

bool ABIBGameMode::IsHubWorld() const
{
	if (HubMap.IsNull())
	{
		return true;   // code-built fallback: the menus live in whatever map was opened
	}
	return UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == HubMap.GetLongPackageName();
}

void ABIBGameMode::UpdateAnimeStyle()
{
	// The Ark is a gameplay level now (anime look on) but it is underground: no sky, so no day / night or weather there.
	const bool bArk = IsArkWorld();
	const bool bWeatherWorld = !bPlainWorld && (bArk || IsInStage() || HasPendingStage() || !IsHubWorld());
	// Owner request 2026-10-16: the anime post-process volume is global - every level and every menu page (the hub / main menu
	// included), not only gameplay maps. Only a plain world (Chapter 11, "no post-processing") stays without it.
	const bool bWant = !bPlainWorld;
	UpdateWeather(bWeatherWorld && !bArk);
	ABIBAnimeStyleVolume* Volume = AnimeStyle.Get();
	if (!Volume)
	{
		TActorIterator<ABIBAnimeStyleVolume> It(GetWorld());
		Volume = It ? *It : nullptr;
	}
	if (!Volume && bWant)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Volume = GetWorld()->SpawnActor<ABIBAnimeStyleVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	if (Volume)
	{
		AnimeStyle = Volume;
		const bool bLook = bWant && ABIBAnimeStyleVolume::IsLookAllowed();   // Settings > Graphics > Anime look
		if (Volume->IsStyleEnabled() != bLook)
		{
			Volume->SetStyleEnabled(bLook);
		}
	}
}

void ABIBGameMode::UpdateWeather(bool bWant)
{
	ABIBWeatherSystem* System = Weather.Get();
	if (!System)
	{
		TActorIterator<ABIBWeatherSystem> It(GetWorld());
		System = It ? *It : nullptr;
	}
	if (!System && bWant)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		System = GetWorld()->SpawnActor<ABIBWeatherSystem>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	if (System)
	{
		Weather = System;
		if (System->IsWeatherEnabled() != bWant)
		{
			System->SetWeatherEnabled(bWant);
		}
	}
}

void ABIBGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAnimeStyle();
	ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	if (!FreeRoamMap.IsEmpty() && PlayerChar)
	{
		// Free roam: the team walks the map from its PlayerStart; no story, sections or enemies.
		const FString FreeRoamName = FreeRoamMap;
		FreeRoamMap.Reset();
		EnsureWorld();
		int32 Team[3];
		BIB::Meta::EffectiveTeam(GetProfile(), 1, Team);
		PlayerChar->SetupTeam(GetProfile(), Team);
		PlayerChar->SetCombatActive(true);
		TeleportPlayerToStart();
		CurrentStage = 0;
		Messages.Reset();
		Flow = EBIBFlow::Roam;
		if (WalkStage > 0)
		{
			// Return to Nowhere: nothing on screen, nothing happens; reaching the exit frame clears Chapter 11.
			bWalkExitFound = false;
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (It->ActorHasTag(TEXT("NowhereExit")))
				{
					WalkExit = It->GetActorLocation();
					bWalkExitFound = true;
					break;
				}
			}
			if (!bWalkExitFound)
			{
				UE_LOG(LogTemp, Warning, TEXT("BIB: Return to Nowhere has no NowhereExit marker (rebuild with Tools/build_greybox.sh)."));
			}
		}
		else
		{
			AddMessage(FString::Printf(TEXT("Free roam: %s. Pause > Abandon stage returns to the menu."),
				FreeRoamName == TEXT("NeoTokyo") || FreeRoamName.IsEmpty() ? TEXT("Neo Tokyo") : *FreeRoamName));
		}
		if (ABIBPlayerController* PC = GetBIBController())
		{
			PC->SetScreen(EBIBScreen::None);
		}
	}
	if (WalkStage > 0 && bWalkExitFound && Flow == EBIBFlow::Roam && PlayerChar
		&& FVector::DistSquared2D(PlayerChar->GetActorLocation(), WalkExit) < FMath::Square(WalkExitRadius))
	{
		// Walked out. Nothing happens: the chapter is cleared, the game saves, and the player is back in the Ark.
		const int32 Stage = WalkStage;
		WalkStage = 0;
		BIB::Meta::ClearStage(GetProfile(), Stage);
		if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
		{
			Instance->SaveNow();
		}
		ReturnToHub();
		return;
	}
	if (PendingBeginStage > 0 && PlayerChar)
	{
		const int32 Stage = PendingBeginStage;
		PendingBeginStage = 0;
		BeginStage(Stage);
	}
	if (bArkPending && PlayerChar)
	{
		const UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
		if (Instance && !Instance->NeedsSignIn())
		{
			BeginArk();
		}
	}
	if (IsArkWorld())
	{
		const UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
		if (Instance && Instance->NeedsSignIn())
		{
			TickIntroCamera(DeltaSeconds);
		}
		else if (Flow == EBIBFlow::Ark)
		{
			// The autosave (every 10 s, UBIBGameInstance) writes whatever is in the profile; keep it current.
			ArkSaveTimer += DeltaSeconds;
			if (ArkSaveTimer >= 0.5f)
			{
				ArkSaveTimer = 0.f;
				SaveArkPosition(false);
			}
		}
	}
	ABIBPlayerController* PC = GetBIBController();
	if (!PC || !PlayerChar)
	{
		return;
	}
	const FVector PlayerLocation = PlayerChar->GetActorLocation();
	switch (Flow)
	{
	case EBIBFlow::Talk:
		if (DialogueIndex >= 0)
		{
			if (PC->WasInputPressed(EBIBInput::Talk))
			{
				++DialogueIndex;
				if (DialogueIndex >= DialogueLines.Num())
				{
					DialogueIndex = -1;
					if (Npc)
					{
						Npc->OnConversation(false);
					}
					CurrentSection = 0;
					SetSectionDoor(0, true);
					Flow = EBIBFlow::Explore;
					AddMessage(TEXT("The door to Section 1 is open."));
				}
			}
		}
		else if (Npc && FVector::Dist2D(PlayerLocation, Npc->GetActorLocation()) < Npc->TalkRadius && PC->WasInputPressed(EBIBInput::Talk))
		{
			DialogueIndex = 0;
			Npc->OnConversation(true);
		}
		break;

	case EBIBFlow::Explore:
		if (Sections.IsValidIndex(CurrentSection) && Sections[CurrentSection] && Sections[CurrentSection]->ContainsPoint(PlayerLocation, 400.f))
		{
			SetSectionDoor(CurrentSection, false); // seal the section behind the team
			SpawnSection(CurrentSection);
			Flow = EBIBFlow::Fight;
		}
		break;

	case EBIBFlow::Fight:
		if (GetBossRushRound() > 0)
		{
			// Boss Rush: boss after boss in the same arena until the team falls (owner request 2026-10-06).
			if (BossRushNextRoundAt >= 0.f)
			{
				if (GetWorld()->GetTimeSeconds() >= BossRushNextRoundAt)
				{
					StartNextBossRushRound();
				}
			}
			else if (GetEnemiesRemaining() == 0)
			{
				const int32 Round = GetBossRushRound();
				for (const std::string& Line : BIB::Arcade::BossRushRoundCleared(GetProfile(), Round))
				{
					AddMessage(FString(UTF8_TO_TCHAR(Line.c_str())));
				}
				if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
				{
					Instance->SaveNow();
				}
				PlayerChar->HealTeamFraction(static_cast<float>(BIB::Arcade::BossRushHealBetweenRounds));
				AddMessage(FString::Printf(TEXT("Next boss in 4 seconds (round %d). Pause > Abandon keeps your rewards."), Round + 1));
				BossRushNextRoundAt = GetWorld()->GetTimeSeconds() + 4.f;
			}
			break;
		}
		if (GetEnemiesRemaining() == 0)
		{
			if (CurrentSection + 1 < Sections.Num())
			{
				BIBMotionEvents::Section(true, CurrentSection + 1, Sections.Num(), TEXT("Section cleared"));
				++CurrentSection;
				SetSectionDoor(CurrentSection, true);
				Flow = EBIBFlow::Explore;
				AddMessage(FString::Printf(TEXT("Section cleared! Door to Section %d open."), CurrentSection + 1));
			}
			else
			{
				FinishStage(true, FString::Printf(TEXT("%s cleared!"), *GetStageName(CurrentStage)));
			}
		}
		break;

	default:
		break;
	}
}

FString ABIBGameMode::GetStageTitle() const
{
	if (!IsInStage())
	{
		return TEXT("Hub");
	}
	if (Flow == EBIBFlow::Roam)
	{
		return WalkStage > 0 ? FString() : FString(TEXT("Neo Tokyo - free roam"));
	}
	if (GetBossRushRound() > 0)
	{
		return FString::Printf(TEXT("Boss Rush - round %d (best %d)"), GetBossRushRound(), GetProfile().Meta.ArcadeRecords.BossRushBest);
	}
	return FString::Printf(TEXT("%s - Section %d/%d"), *GetStageName(CurrentStage), CurrentSection + 1, Sections.Num());
}

FString ABIBGameMode::GetObjective() const
{
	switch (Flow)
	{
	case EBIBFlow::Talk: return DialogueIndex >= 0 ? TEXT("Press T to continue the conversation") : TEXT("Walk to the green story contact and press T");
	case EBIBFlow::Explore: return FString::Printf(TEXT("Go through the door into Section %d"), CurrentSection + 1);
	case EBIBFlow::Fight: return FString::Printf(TEXT("Defeat every enemy (%d left)"), GetEnemiesRemaining());
	case EBIBFlow::Finished: return TEXT("Stage over");
	case EBIBFlow::Roam: return TEXT("Explore the city");
	default: return TEXT("Choose an option from the menu");
	}
}

FString ABIBGameMode::GetDialogueLine() const
{
	return DialogueLines.IsValidIndex(DialogueIndex) ? DialogueLines[DialogueIndex] : FString();
}

FString ABIBGameMode::GetDialogueSpeaker() const
{
	return Npc ? Npc->SpeakerName : FString(TEXT("Story contact (placeholder)"));
}

// ============================================================================ the Ark (owner request 2026-10-07)

FString ABIBGameMode::ArkPackage()
{
	return TEXT("/Game/Greybox/Maps/L_Ark");
}

bool ABIBGameMode::ShouldUseArk() const
{
	return FPackageName::DoesPackageExist(ArkPackage());
}

bool ABIBGameMode::IsArkWorld() const
{
	return GetWorld() && UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == ArkPackage();
}

bool ABIBGameMode::TravelToArk(const FString& InStation)
{
	if (!FPackageName::DoesPackageExist(ArkPackage()))
	{
		return false;
	}
	if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
	{
		if (!InStation.IsEmpty())
		{
			Instance->ArkStation = InStation;
			Instance->PendingArkStation = InStation;
		}
	}
	// The greybox map's own World Settings use the engine's free-fly game mode; ?game= runs this one instead.
	UGameplayStatics::OpenLevel(this, FName(*ArkPackage()), true, TEXT("game=") + GetClass()->GetPathName());
	return true;
}

void ABIBGameMode::SpawnScreens()
{
	for (ABIBVideoScreen* Screen : Screens)
	{
		if (Screen)
		{
			Screen->Destroy();
		}
	}
	Screens.Reset();
	// The six ads run on the town's LED ad pillars (owner 2026-10-09: "just place them on the LED signs as ads"): the HoloAd
	// placeholder cylinders are hidden and each carries a two-sided screen, the ads cycling pillar by pillar, facing the town.
	static const TCHAR* const Ads[] = {TEXT("BB_Ad01_Government_Broadcast"), TEXT("BB_Ad02_ASCEND_Pills"), TEXT("BB_Ad03_Uniform_Sponsor_Slots_2010"),
		TEXT("BB_Ad04_Merchant_Cheap_Deals"), TEXT("BB_Ad05_Broken_Black_Board_Loop"), TEXT("BB_Ad06_Media_Private_Apartments")};
	{
		TArray<AActor*> Pillars;
		FVector Centre = FVector::ZeroVector;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->Tags.Contains(FName(TEXT("HoloAdPlaceholder"))))
			{
				Pillars.Add(*It);
				Centre += It->GetActorLocation();
			}
		}
		Pillars.Sort([](const AActor& A, const AActor& B) { return A.GetActorLocation().X + A.GetActorLocation().Y * 0.37 < B.GetActorLocation().X + B.GetActorLocation().Y * 0.37; });
		Centre /= FMath::Max(1, Pillars.Num());
		for (int32 Index = 0; Index < Pillars.Num(); ++Index)
		{
			AActor* Pillar = Pillars[Index];
			// on top of the pillar: the screen's lower edge just above the placeholder (420 x 236 cm)
			FVector Origin, Extent;
			Pillar->GetActorBounds(false, Origin, Extent);
			const FVector At(Origin.X, Origin.Y, Origin.Z + Extent.Z + 118.0 + 60.0);
			const float Yaw = static_cast<float>((Centre - At).GetSafeNormal2D().Rotation().Yaw);
			if (ABIBVideoScreen* Screen = ABIBVideoScreen::Spawn(GetWorld(), Ads[Index % 6], At, Yaw, 420.f, true, true))
			{
				Pillar->SetActorHiddenInGame(true);
				Screens.Add(Screen);
			}
		}
	}
	// Station -> video: the trailer over the factory; the reel in the arcade hall; the UI sheets and the transitions showcase on
	// smaller screens (one per station, so every file of the pack plays somewhere).
	// Set 2 (the VFX bible sheets 08-18 and its master reel) fills the stations the ads left, then the left side of others.
	struct FPlacement { const TCHAR* Station; const TCHAR* Movie; float Width; bool bSound; float Side = 1.f; };
	static const FPlacement Placements[] = {
		{TEXT("factory"), TEXT("BB_Download_Trailer"), 1100.f, false},
		{TEXT("arcade"), TEXT("BB_Showreel_Master"), 900.f, false},
		{TEXT("rollcall"), TEXT("BB_UI_Sheet_04_Combat_HUD"), 480.f, false},
		{TEXT("armoury"), TEXT("BB_UI_Sheet_02_Economy_Rewards"), 480.f, false},
		{TEXT("taskroom"), TEXT("BB_UI_Sheet_05_Progression"), 480.f, false},
		{TEXT("tower"), TEXT("BB_UI_Sheet_06_Arcade"), 480.f, false},
		{TEXT("ganghall"), TEXT("BB_UI_Sheet_01_Navigation_Core"), 480.f, false},
		{TEXT("eventshop"), TEXT("BB_UI_Sheet_03_Summon"), 480.f, false},
		{TEXT("gangshop"), TEXT("BB_UI_Sheet_07_Mobile_Controls"), 480.f, false},
		{TEXT("arkshop"), TEXT("BB_Transitions_Showcase"), 480.f, false},
		{TEXT("arena"), TEXT("BB_Showreel_Set2_Master"), 900.f, false},
		{TEXT("government"), TEXT("BB_Set2_Sheet_08_Missing_Screens"), 560.f, false},
		{TEXT("premium"), TEXT("BB_Set2_VFX_Sheet_09_Shared_Combat"), 480.f, false},
		{TEXT("trade"), TEXT("BB_Set2_VFX_Sheet_10_Status_Effects"), 480.f, false},
		{TEXT("alley"), TEXT("BB_Set2_VFX_Sheet_11_Angels"), 480.f, false},
		{TEXT("library"), TEXT("BB_Set2_VFX_Sheet_12_Luna"), 480.f, false},
		{TEXT("resistance"), TEXT("BB_Set2_VFX_Sheet_13_Judge"), 480.f, false},
		{TEXT("rollcall"), TEXT("BB_Set2_VFX_Sheet_14_Narrator_Diano"), 480.f, false, -1.f},
		{TEXT("armoury"), TEXT("BB_Set2_VFX_Sheet_15_Alchemist_Arita_Adapt"), 480.f, false, -1.f},
		{TEXT("taskroom"), TEXT("BB_Set2_VFX_Sheet_16_Fujita_Ishii_Myrcia_Shiraishi_Kuu_Plack"), 480.f, false, -1.f},
		{TEXT("tower"), TEXT("BB_Set2_VFX_Sheet_17_Blood_Gambler_Stargazer_Weather"), 480.f, false, -1.f},
		{TEXT("ganghall"), TEXT("BB_Set2_VFX_Sheet_18_Death_Jester_Media_Timekeeper"), 480.f, false, -1.f},
	};
	for (const FPlacement& Place : Placements)
	{
		const ABIBArkStation* Station = FindStation(Place.Station);
		if (!Station)
		{
			continue;
		}
		// The station anchor is where the player stands facing the building: the screen hangs above and in front of the ring,
		// turned towards that spot.
		const FVector Forward = Station->GetActorForwardVector().GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		// Beside the station's own sign (which sits straight ahead), a little forward, at billboard height.
		const FVector At = Station->GetActorLocation() + Forward * 380.0 + Right * (Place.Side * (Place.Width * 0.5 + 420.0)) + FVector(0.0, 0.0, 260.0 + Place.Width * 0.28);
		if (ABIBVideoScreen* Screen = ABIBVideoScreen::Spawn(GetWorld(), Place.Movie, At, Station->GetActorRotation().Yaw + 180.f, Place.Width, Place.bSound))
		{
			Screens.Add(Screen);
		}
	}
}

void ABIBGameMode::SpawnStations()
{
	for (ABIBArkStation* Station : Stations)
	{
		if (Station)
		{
			Station->Destroy();
		}
	}
	Stations.Reset();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* Anchor = *It;
		if (!Anchor->Tags.Contains(FName(TEXT("BIBStation"))))
		{
			continue;
		}
		FString Id;
		for (const FName& Tag : Anchor->Tags)
		{
			if (Tag != FName(TEXT("BIBStation")))
			{
				Id = Tag.ToString();
			}
		}
		if (Id.IsEmpty() || !BIB::Ark::FindStation(TCHAR_TO_UTF8(*Id)) || FindStation(Id))
		{
			continue;
		}
		ABIBArkStation* Station = GetWorld()->SpawnActor<ABIBArkStation>(Anchor->GetActorLocation(), FRotator(0.0, Anchor->GetActorRotation().Yaw, 0.0), Params);
		if (Station)
		{
			Station->Setup(Id);
			Stations.Add(Station);
		}
	}
}

ABIBArkStation* ABIBGameMode::FindStation(const FString& Id) const
{
	for (ABIBArkStation* Station : Stations)
	{
		if (Station && Station->GetStationId() == Id)
		{
			return Station;
		}
	}
	return nullptr;
}

ABIBArkStation* ABIBGameMode::GetStationInReach() const
{
	const ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	if (!PlayerChar || Flow != EBIBFlow::Ark)
	{
		return nullptr;
	}
	const FVector Here = PlayerChar->GetActorLocation() - FVector(0.0, 0.0, PlayerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 100.0);
	ABIBArkStation* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (ABIBArkStation* Station : Stations)
	{
		if (Station && Station->IsInReach(Here))
		{
			const double Distance = FVector::Dist2D(Here, Station->GetActorLocation());
			if (Distance < BestDistance)
			{
				Best = Station;
				BestDistance = Distance;
			}
		}
	}
	return Best;
}

void ABIBGameMode::PlacePlayer(const FVector& Anchor, double Yaw)
{
	ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	if (!PlayerChar)
	{
		return;
	}
	// Anchors sit 1 m above the floor; the capsule's centre goes half its height above the floor.
	const double HalfHeight = PlayerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	PlayerChar->SetActorLocation(Anchor + FVector(0.0, 0.0, HalfHeight - 100.0 + 5.0), false, nullptr, ETeleportType::TeleportPhysics);
	PlayerChar->SetActorRotation(FRotator(0.0, Yaw, 0.0));
	PlayerChar->GetCharacterMovement()->StopMovementImmediately();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetControlRotation(FRotator(-10.0, Yaw, 0.0));
	}
}

bool ABIBGameMode::TeleportToStation(const FString& Id, FString& OutError)
{
	if (Flow != EBIBFlow::Ark)
	{
		OutError = TEXT("Teleports only work while walking the Ark.");
		return false;
	}
	ABIBArkStation* Station = FindStation(Id);
	if (!Station)
	{
		OutError = FString::Printf(TEXT("%s is not built in this Ark yet (run Tools/build_greybox.sh)."), *Id);
		return false;
	}
	PlacePlayer(Station->GetStandLocation(), Station->GetStandYaw());
	Station->Pulse(FLinearColor::White, 1.4f);
	if (UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance()))
	{
		Instance->ArkStation = Id;
	}
	++TeleportCount;
	return true;
}

void ABIBGameMode::RefreshArkTeam()
{
	ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	if (!PlayerChar || Flow != EBIBFlow::Ark)
	{
		return;
	}
	int32 Team[3];
	BIB::Meta::EffectiveTeam(GetProfile(), 1, Team);
	if (Team[0] == ArkTeam[0] && Team[1] == ArkTeam[1] && Team[2] == ArkTeam[2])
	{
		return;
	}
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		ArkTeam[Slot] = Team[Slot];
	}
	PlayerChar->SetupTeam(GetProfile(), Team);
	PlayerChar->SetCombatActive(false);
	PlayerChar->SetWalkOnly(true);
}

void ABIBGameMode::BeginArk()
{
	bArkPending = false;
	ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	if (!PlayerChar)
	{
		return;
	}
	SpawnStations();
	SpawnScreens();
	Flow = EBIBFlow::Ark;
	CurrentStage = 0;
	DialogueIndex = -1;
	Messages.Reset();
	ArkTeam[0] = ArkTeam[1] = ArkTeam[2] = -2;
	RefreshArkTeam();
	// Arrival (owner request 2026-10-07): an explicit trip to a station first, else where the player last stood (the saved
	// position, also how a stage hands back: the position is saved when it starts), else the plaza in front of the government.
	EndIntroCamera();
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	ABIBArkStation* Arrival = nullptr;
	if (Instance)
	{
		Arrival = FindStation(Instance->PendingArkStation);
		Instance->PendingArkStation.Reset();
	}
	const BIB::FMeta& Meta = GetProfile().Meta;
	FVector Floor;
	if (Arrival)
	{
		PlacePlayer(Arrival->GetStandLocation(), Arrival->GetStandYaw());
		Arrival->Pulse(FLinearColor::White, 1.4f);
		ArkArrival = TEXT("station");
	}
	else if (Meta.bArkPosValid && FindArkFloor(FVector(Meta.ArkPos[0], Meta.ArkPos[1], Meta.ArkPos[2]), Floor))
	{
		PlacePlayer(Floor + FVector(0.0, 0.0, 100.0), Meta.ArkYaw);
		ArkArrival = TEXT("saved");
	}
	else
	{
		ArkArrival = TEXT("plaza");
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->Tags.Contains(FName(TEXT("BIBArkSpawn"))))
			{
				PlacePlayer(It->GetActorLocation(), It->GetActorRotation().Yaw);
				break;
			}
		}
	}
	ArkSaveTimer = 0.f;
	UE_LOG(LogTemp, Display, TEXT("BIB Ark: %d stations, arrived at %s (%s)"), Stations.Num(), Arrival ? *Arrival->GetStationId() : TEXT("-"), *ArkArrival);
	if (ABIBPlayerController* PC = GetBIBController())
	{
		PC->SetScreen(EBIBScreen::None);
	}
}

bool ABIBGameMode::FindArkFloor(const FVector& Location, FVector& OutFloor) const
{
	// A saved spot must still stand on something (the map is regenerated by Tools/build_greybox.sh and may have changed).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BIBArkFloor), false);
	if (const ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter())
	{
		Params.AddIgnoredActor(PlayerChar);
	}
	FHitResult Hit;
	const FVector Start = Location + FVector(0.0, 0.0, 60.0);
	const FVector End = Location - FVector(0.0, 0.0, 600.0);
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || Hit.bStartPenetrating)
	{
		return false;
	}
	OutFloor = Hit.ImpactPoint;
	return Hit.ImpactNormal.Z > 0.6;   // floors, not walls
}

void ABIBGameMode::SaveArkPosition(bool bWriteNow)
{
	ABIBPlayerCharacter* PlayerChar = GetPlayerCharacter();
	UBIBGameInstance* Instance = Cast<UBIBGameInstance>(GetGameInstance());
	if (Flow != EBIBFlow::Ark || !PlayerChar || !Instance || !Instance->IsProfileReady())
	{
		return;
	}
	const UCharacterMovementComponent* Movement = PlayerChar->GetCharacterMovement();
	if (Movement && Movement->IsMovingOnGround())
	{
		// The feet, rounded to whole centimetres so standing still does not rewrite the save.
		const FVector Feet = PlayerChar->GetActorLocation() - FVector(0.0, 0.0, PlayerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		BIB::FMeta& Meta = GetProfile().Meta;
		Meta.ArkPos[0] = FMath::RoundToDouble(Feet.X);
		Meta.ArkPos[1] = FMath::RoundToDouble(Feet.Y);
		Meta.ArkPos[2] = FMath::RoundToDouble(Feet.Z);
		Meta.ArkYaw = FMath::RoundToDouble(FRotator::NormalizeAxis(PlayerChar->GetActorRotation().Yaw));
		Meta.bArkPosValid = true;
	}
	if (bWriteNow)
	{
		Instance->SaveNow();
	}
}

void ABIBGameMode::TickIntroCamera(float DeltaSeconds)
{
	// The sign-in screen sits over the living Ark: a slow circle around the government spire on the town square.
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	FVector Centre(43500.0, 0.0, 0.0);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(FName(TEXT("BIBArkSpawn"))))
		{
			Centre = It->GetActorLocation() + FVector(9500.0, 0.0, 0.0);
			break;
		}
	}
	if (!IntroCamera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		IntroCamera = GetWorld()->SpawnActor<ACameraActor>(Centre, FRotator::ZeroRotator, Params);
	}
	if (!IntroCamera)
	{
		return;
	}
	IntroAngle += DeltaSeconds * 2.5;
	const double Radians = FMath::DegreesToRadians(IntroAngle);
	const FVector Eye = Centre + FVector(FMath::Cos(Radians) * 9000.0, FMath::Sin(Radians) * 9000.0, 2600.0);
	const FVector Look = Centre + FVector(0.0, 0.0, 4200.0);
	IntroCamera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
	if (PC->GetViewTarget() != IntroCamera)
	{
		PC->SetViewTarget(IntroCamera);
	}
}

void ABIBGameMode::EndIntroCamera()
{
	if (!IntroCamera)
	{
		return;
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTarget(Pawn);
		}
	}
	IntroCamera->Destroy();
	IntroCamera = nullptr;
}
