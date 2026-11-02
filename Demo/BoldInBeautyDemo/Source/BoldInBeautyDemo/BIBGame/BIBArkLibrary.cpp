#include "BIBGame/BIBArkLibrary.h"

#include "BIBGame/BIBArkStation.h"
#include "BIBGame/BIBGameInstance.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBRules/BIBArk.h"
#include "BIBRules/BIBPages.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	ABIBPlayerController* PCOf(const UObject* Context)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		return World ? Cast<ABIBPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
	}

	ABIBGameMode* ModeOf(const UObject* Context)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		return World ? World->GetAuthGameMode<ABIBGameMode>() : nullptr;
	}

	UBIBGameInstance* InstanceOf(const UObject* Context)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		return World ? Cast<UBIBGameInstance>(World->GetGameInstance()) : nullptr;
	}
}

bool UBIBArkLibrary::IsArkWorld(const UObject* WorldContextObject)
{
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	return GameMode && GameMode->IsArkWorld();
}

bool UBIBArkLibrary::IsInArk(const UObject* WorldContextObject)
{
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	return GameMode && GameMode->IsInArk();
}

TArray<FString> UBIBArkLibrary::GetStationIds(const UObject* WorldContextObject)
{
	TArray<FString> Ids;
	if (const ABIBGameMode* GameMode = ModeOf(WorldContextObject))
	{
		for (const ABIBArkStation* Station : GameMode->GetStations())
		{
			if (Station)
			{
				Ids.Add(Station->GetStationId());
			}
		}
	}
	return Ids;
}

FVector UBIBArkLibrary::GetStationLocation(const UObject* WorldContextObject, const FString& StationId)
{
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	const ABIBArkStation* Station = GameMode ? GameMode->FindStation(StationId) : nullptr;
	return Station ? Station->GetActorLocation() : FVector::ZeroVector;
}

FString UBIBArkLibrary::GetStationInReach(const UObject* WorldContextObject)
{
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	const ABIBArkStation* Station = GameMode ? GameMode->GetStationInReach() : nullptr;
	return Station ? Station->GetStationId() : FString();
}

int32 UBIBArkLibrary::GetTeleportCount(const UObject* WorldContextObject)
{
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	return GameMode ? GameMode->GetTeleportCount() : 0;
}

TArray<FString> UBIBArkLibrary::GetMenuNodeIds()
{
	TArray<FString> Ids;
	for (const std::string& Id : BIB::Ark::AllNodeIds())
	{
		Ids.Add(UTF8_TO_TCHAR(Id.c_str()));
	}
	return Ids;
}

FString UBIBArkLibrary::GetMenuNodeKind(const UObject* WorldContextObject, const FString& NodeId)
{
	UBIBGameInstance* Instance = InstanceOf(WorldContextObject);
	if (!Instance)
	{
		return FString();
	}
	const BIB::Ark::FNode Node = BIB::Ark::Node(Instance->GetProfile(), Instance->UiState, TCHAR_TO_UTF8(*NodeId));
	if (Node.Id != TCHAR_TO_UTF8(*NodeId))
	{
		return FString();
	}
	switch (Node.Kind)
	{
	case BIB::Ark::ENodeKind::Branch: return TEXT("Branch");
	case BIB::Ark::ENodeKind::Teleport: return TEXT("Teleport");
	case BIB::Ark::ENodeKind::Page: return TEXT("Page");
	case BIB::Ark::ENodeKind::Action: return TEXT("Action");
	}
	return FString();
}

TArray<FString> UBIBArkLibrary::GetMenuChildren(const UObject* WorldContextObject, const FString& NodeId)
{
	TArray<FString> Ids;
	if (UBIBGameInstance* Instance = InstanceOf(WorldContextObject))
	{
		for (const BIB::Ark::FNode& Node : BIB::Ark::Children(Instance->GetProfile(), Instance->UiState, TCHAR_TO_UTF8(*NodeId)))
		{
			Ids.Add(UTF8_TO_TCHAR(Node.Id.c_str()));
		}
	}
	return Ids;
}

void UBIBArkLibrary::OpenMenu(const UObject* WorldContextObject)
{
	if (ABIBPlayerController* PC = PCOf(WorldContextObject))
	{
		PC->OpenArkMenu();
	}
}

void UBIBArkLibrary::MenuClick(const UObject* WorldContextObject, const FString& NodeId)
{
	if (ABIBPlayerController* PC = PCOf(WorldContextObject))
	{
		PC->ArkMenuClick(NodeId);
	}
}

FString UBIBArkLibrary::GetMenuPath(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = PCOf(WorldContextObject);
	if (!PC)
	{
		return FString();
	}
	FString Path = TEXT("menu");
	for (const FString& Id : PC->ArkUi.MenuPath)
	{
		Path += TEXT(" / ") + Id;
	}
	return Path;
}

void UBIBArkLibrary::CloseScreens(const UObject* WorldContextObject)
{
	if (ABIBPlayerController* PC = PCOf(WorldContextObject))
	{
		PC->CloseArkScreens();
	}
}

FString UBIBArkLibrary::GetScreenName(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = PCOf(WorldContextObject);
	if (!PC)
	{
		return FString();
	}
	const UEnum* Enum = StaticEnum<EBIBScreen>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(PC->GetScreen())) : FString();
}

bool UBIBArkLibrary::OpenStation(const UObject* WorldContextObject, const FString& StationId, int32 Counter)
{
	ABIBPlayerController* PC = PCOf(WorldContextObject);
	return PC && PC->ArkOpenStation(StationId, Counter);
}

bool UBIBArkLibrary::Interact(const UObject* WorldContextObject)
{
	ABIBPlayerController* PC = PCOf(WorldContextObject);
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	const ABIBArkStation* Station = GameMode ? GameMode->GetStationInReach() : nullptr;
	return PC && Station && PC->ArkOpenStation(Station->GetStationId());
}

void UBIBArkLibrary::PanelAction(const UObject* WorldContextObject, const FString& ActionId)
{
	if (ABIBPlayerController* PC = PCOf(WorldContextObject))
	{
		PC->ArkRunAction(ActionId);
	}
}

void UBIBArkLibrary::PanelBack(const UObject* WorldContextObject)
{
	if (ABIBPlayerController* PC = PCOf(WorldContextObject))
	{
		PC->ArkPanelBack();
	}
}

FString UBIBArkLibrary::GetPanelTitle(const UObject* WorldContextObject)
{
	UBIBGameInstance* Instance = InstanceOf(WorldContextObject);
	return Instance ? FString(UTF8_TO_TCHAR(BIB::Pages::Build(Instance->GetProfile(), Instance->UiState).Title.c_str())) : FString();
}

FString UBIBArkLibrary::GetPanelStation(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = PCOf(WorldContextObject);
	return PC ? PC->ArkUi.PanelStation : FString();
}

TArray<FString> UBIBArkLibrary::GetPanelActions(const UObject* WorldContextObject)
{
	TArray<FString> Ids;
	if (UBIBGameInstance* Instance = InstanceOf(WorldContextObject))
	{
		const BIB::Pages::FPage Page = BIB::Pages::Build(Instance->GetProfile(), Instance->UiState);
		for (const BIB::Pages::FPageAction& Action : Page.Grid)
		{
			Ids.Add(UTF8_TO_TCHAR(Action.Id.c_str()));
		}
		for (const BIB::Pages::FPageAction& Action : Page.Bar)
		{
			Ids.Add(UTF8_TO_TCHAR(Action.Id.c_str()));
		}
	}
	return Ids;
}

FString UBIBArkLibrary::GetStatusLine(const UObject* WorldContextObject)
{
	const UBIBGameInstance* Instance = InstanceOf(WorldContextObject);
	return Instance ? FString(UTF8_TO_TCHAR(Instance->UiState.Status.c_str())) : FString();
}

bool UBIBArkLibrary::IsBattleHudShown(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = PCOf(WorldContextObject);
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	const ABIBPlayerCharacter* PlayerChar = PC ? PC->GetBIBCharacter() : nullptr;
	return PC && GameMode && GameMode->IsInStage() && PC->GetScreen() == EBIBScreen::None && PlayerChar && PlayerChar->GetHud().bValid;
}

bool UBIBArkLibrary::IsArkHudShown(const UObject* WorldContextObject)
{
	const ABIBPlayerController* PC = PCOf(WorldContextObject);
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	return PC && GameMode && GameMode->IsInArk() && PC->GetScreen() == EBIBScreen::None;
}

namespace
{
	TArray<FString> PageIds(const UObject* Context, bool bGrid)
	{
		TArray<FString> Ids;
		if (UBIBGameInstance* Instance = InstanceOf(Context))
		{
			const BIB::Pages::FPage Page = BIB::Pages::Build(Instance->GetProfile(), Instance->UiState);
			for (const BIB::Pages::FPageAction& Action : bGrid ? Page.Grid : Page.Bar)
			{
				Ids.Add(UTF8_TO_TCHAR(Action.Id.c_str()));
			}
		}
		return Ids;
	}
}

TArray<FString> UBIBArkLibrary::GetPageGridIds(const UObject* WorldContextObject)
{
	return PageIds(WorldContextObject, true);
}

TArray<FString> UBIBArkLibrary::GetPageBarIds(const UObject* WorldContextObject)
{
	return PageIds(WorldContextObject, false);
}

int32 UBIBArkLibrary::GetPageTab(const UObject* WorldContextObject)
{
	const UBIBGameInstance* Instance = InstanceOf(WorldContextObject);
	return Instance ? Instance->UiState.Tab : -1;
}

FString UBIBArkLibrary::GetArkArrival(const UObject* WorldContextObject)
{
	const ABIBGameMode* GameMode = ModeOf(WorldContextObject);
	return GameMode ? GameMode->GetArkArrival() : FString();
}

bool UBIBArkLibrary::GetSavedArkPosition(const UObject* WorldContextObject, FVector& Position, float& Yaw)
{
	UBIBGameInstance* Instance = InstanceOf(WorldContextObject);
	if (!Instance || !Instance->GetProfile().Meta.bArkPosValid)
	{
		return false;
	}
	const BIB::FMeta& Meta = Instance->GetProfile().Meta;
	Position = FVector(Meta.ArkPos[0], Meta.ArkPos[1], Meta.ArkPos[2]);
	Yaw = static_cast<float>(Meta.ArkYaw);
	return true;
}

void UBIBArkLibrary::SaveArkPosition(const UObject* WorldContextObject, bool bWrite)
{
	if (ABIBGameMode* GameMode = ModeOf(WorldContextObject))
	{
		GameMode->SaveArkPosition(bWrite);
	}
}

void UBIBArkLibrary::PressUiKey(const FString& KeyName)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		return;
	}
	const FKeyEvent Event(Key, FModifierKeysState(), FSlateApplicationBase::SlateAppPrimaryPlatformUser, false, 0, 0);
	FSlateApplication::Get().ProcessKeyDownEvent(Event);
	FSlateApplication::Get().ProcessKeyUpEvent(Event);
}

FString UBIBArkLibrary::GetFocusedWidgetType()
{
	if (!FSlateApplication::IsInitialized())
	{
		return FString();
	}
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	return Focused.IsValid() ? Focused->GetType().ToString() : FString();
}

FString UBIBArkLibrary::GetFocusedWidgetPath()
{
	if (!FSlateApplication::IsInitialized())
	{
		return FString();
	}
	TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FString Path;
	for (int32 Depth = 0; Focused.IsValid() && Depth < 40; ++Depth)
	{
		Path = Focused->GetType().ToString() + (Path.IsEmpty() ? FString() : TEXT(" > ") + Path);
		Focused = Focused->GetParentWidget();
	}
	return Path;
}
