// Blueprint / Python hooks for the Ark hub (owner request 2026-10-07): Tools/verify_ark.py drives the menu, the teleports and
// the station panels through these, the same calls a click makes.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BIBArkLibrary.generated.h"

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBArkLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static bool IsArkWorld(const UObject* WorldContextObject);

	/** Walking the Ark (the hub flow), not in a stage. */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static bool IsInArk(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetStationIds(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FVector GetStationLocation(const UObject* WorldContextObject, const FString& StationId);

	/** The station whose ring the player stands in ("" = none). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetStationInReach(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static int32 GetTeleportCount(const UObject* WorldContextObject);

	/** Every node id of the flow-chart menu (BIBRules/BIBArk.cpp). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark")
	static TArray<FString> GetMenuNodeIds();

	/** The node's kind: Branch, Teleport, Page or Action ("" when unknown). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetMenuNodeKind(const UObject* WorldContextObject, const FString& NodeId);

	/** The children of a node as they are shown right now (conditional entries filtered). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetMenuChildren(const UObject* WorldContextObject, const FString& NodeId);

	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static void OpenMenu(const UObject* WorldContextObject);

	/** A click on a menu card (a branch opens the next column, a leaf teleports / opens / runs). */
	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static void MenuClick(const UObject* WorldContextObject, const FString& NodeId);

	/** "menu / the-ark / town" */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetMenuPath(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static void CloseScreens(const UObject* WorldContextObject);

	/** None, ArkMenu, ArkPanel, MainMenu, Pause, Results, SignIn ... */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetScreenName(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static bool OpenStation(const UObject* WorldContextObject, const FString& StationId, int32 Counter);

	/** F inside a ring: opens the station the player stands at. */
	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static bool Interact(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static void PanelAction(const UObject* WorldContextObject, const FString& ActionId);

	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static void PanelBack(const UObject* WorldContextObject);

	/** The title of the page the panel shows. */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetPanelTitle(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetPanelStation(const UObject* WorldContextObject);

	/** The ids of the tiles and buttons on the panel's page. */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetPanelActions(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetStatusLine(const UObject* WorldContextObject);

	/** The battle HUD is on screen (a stage, Screen None). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static bool IsBattleHudShown(const UObject* WorldContextObject);

	/** The Ark HUD (menu button) is on screen. */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static bool IsArkHudShown(const UObject* WorldContextObject);

	// ---- the page model behind every panel (what the deleted 2D lobby showed): the older verify scripts drive it through these
	/** The tile ids of the current page (the panel's cards). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetPageGridIds(const UObject* WorldContextObject);

	/** The button ids of the current page (the panel's button row). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetPageBarIds(const UObject* WorldContextObject);

	/** The page model's tab (BIBRules/BIBPages.h: 0 Operator ... 6 Summon). */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static int32 GetPageTab(const UObject* WorldContextObject);

	// ---- the saved position (owner request 2026-10-07: start in the Ark where you last stood)
	/** How the player arrived on the last Ark load: "station", "saved" or "plaza". */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static FString GetArkArrival(const UObject* WorldContextObject);

	/** The position kept in the profile (feet, cm) and its yaw; false when the profile has none. */
	UFUNCTION(BlueprintPure, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static bool GetSavedArkPosition(const UObject* WorldContextObject, FVector& Position, float& Yaw);

	/** Copies where the player stands into the profile now (and writes the save file when bWrite). */
	UFUNCTION(BlueprintCallable, Category = "BIB|Ark", meta = (WorldContext = "WorldContextObject"))
	static void SaveArkPosition(const UObject* WorldContextObject, bool bWrite);

	// ---- test hooks (Tools/verify_ui_flow.py): drive the real Slate key path, as a keyboard or pad would.
	/** Sends a key (FKey name, e.g. "Escape", "Gamepad_FaceButton_Bottom") down and up through Slate to the focused widget. */
	UFUNCTION(BlueprintCallable, Category = "BIB|Test")
	static void PressUiKey(const FString& KeyName);

	/** Slate type of the widget that has keyboard focus ("" = none). */
	UFUNCTION(BlueprintPure, Category = "BIB|Test")
	static FString GetFocusedWidgetType();

	/** Slate types from the root down to the keyboard-focused widget (which screen owns the focus). */
	UFUNCTION(BlueprintCallable, Category = "BIB|Test")
	static FString GetFocusedWidgetPath();
};
