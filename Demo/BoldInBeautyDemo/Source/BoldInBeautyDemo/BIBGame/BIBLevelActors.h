// Actors a designer places in a stage map (.umap). The game mode drives the MAIN §7 flow from them:
// talk to the Story NPC -> open the first Section's door -> clear each Section in SectionIndex order.
// When a map has none of these, the game mode spawns equivalent ones in code.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIBLevelActors.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** A door that seals a section until the previous one is cleared. Pivot is at floor level, centred in the opening. */
UCLASS()
class BOLDINBEAUTYDEMO_API ABIBDoor : public AActor
{
	GENERATED_BODY()

public:
	ABIBDoor();

	UFUNCTION(BlueprintCallable, Category = "BIB|Door")
	void SetOpen(bool bInOpen);

	UFUNCTION(BlueprintPure, Category = "BIB|Door")
	bool IsOpen() const { return bOpen; }

	/** Called after the door opens or closes (play sounds, animate a Blueprint door here). */
	UFUNCTION(BlueprintImplementableEvent, Category = "BIB|Door")
	void OnDoorStateChanged(bool bNowOpen);

	/** Code-built levels: visible door size and the height of the invisible blocker above it. */
	void SetDoorSize(const FVector& VisibleSizeCm, float BlockerHeightCm);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Door")
	TObjectPtr<USceneComponent> DoorRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Door")
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Door")
	FLinearColor DoorColour = FLinearColor(0.75f, 0.2f, 0.25f);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DoorMaterial;

	bool bOpen = false;
};

/** One combat section (MAIN §7: 10-30 enemies, all must die before the next section opens). */
UCLASS()
class BOLDINBEAUTYDEMO_API ABIBSection : public AActor
{
	GENERATED_BODY()

public:
	ABIBSection();

	/** Playable area. The team entering it starts the fight. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Section")
	TObjectPtr<UBoxComponent> Bounds;

	/** Which stage of the chapter this section belongs to (a map may hold several stages). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Section", meta = (ClampMin = "1"))
	int32 StageNumber = 1;

	/** Order within the stage, starting at 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Section", meta = (ClampMin = "0"))
	int32 SectionIndex = 0;

	/** Door the team walks through to enter; closed until this section is next. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Section")
	TObjectPtr<ABIBDoor> EntryDoor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies", meta = (ClampMin = "0"))
	int32 Grunts = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies", meta = (ClampMin = "0"))
	int32 Brutes = 0;

	/** Ranged enemies: shoot from a distance (chapter 2 on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies", meta = (ClampMin = "0"))
	int32 Ranged = 0;

	/** Mage enemies: telegraphed area blasts (chapter 3 on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies", meta = (ClampMin = "0"))
	int32 Mages = 0;

	/** Assassin enemies: fast, high damage at close range, low health (chapter 4 on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies", meta = (ClampMin = "0"))
	int32 Assassins = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies", meta = (ClampMin = "0"))
	int32 MiniBosses = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies", meta = (ClampMin = "0"))
	int32 Bosses = 0;

	/** Optional spawn markers (e.g. Target Points). Without them enemies spawn at random floor points in Bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemies")
	TArray<TObjectPtr<AActor>> SpawnPoints;

	UFUNCTION(BlueprintPure, Category = "BIB|Section")
	int32 TotalEnemies() const { return Grunts + Brutes + Ranged + Mages + Assassins + MiniBosses + Bosses; }

	bool ContainsPoint(const FVector& Location, float InsetCm) const;
	FVector FindSpawnLocation(int32 SpawnIndex) const;
	void SetAreaSize(const FVector& HalfExtentCm);
};

/** The person you talk to before each stage (MAIN §7). Dialogue is the owner's to write. */
UCLASS()
class BOLDINBEAUTYDEMO_API ABIBStoryNPC : public AActor
{
	GENERATED_BODY()

public:
	ABIBStoryNPC();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Story")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Story")
	TObjectPtr<UTextRenderComponent> NameLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Story")
	FString SpeakerName = TEXT("Story contact (placeholder)");

	/** Lines shown in order (T to continue). Empty = placeholder lines from the game mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Story", meta = (MultiLine = "true"))
	TArray<FString> DialogueLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Story")
	float TalkRadius = 500.f;

	/** Called when the conversation starts and ends. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BIB|Story")
	void OnConversation(bool bStarted);

protected:
	virtual void BeginPlay() override;
};
