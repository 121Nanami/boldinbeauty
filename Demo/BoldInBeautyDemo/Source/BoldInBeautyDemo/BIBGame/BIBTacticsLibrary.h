// Crowd measurements for the angels' behaviour layer (2026-10-12), for Tools/verify_tactics.py and the console. It reads the
// live angels directly, so it measures the same things with the layer on (bib.Tactics 1) and off (bib.Tactics 0).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BIBTacticsLibrary.generated.h"

UCLASS()
class BOLDINBEAUTYDEMO_API UBIBTacticsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BIB|Tactics")
	static void ResetCrowdStats();

	/** Call once per frame while measuring: counts committed melee angels, stacked pairs and distances. */
	UFUNCTION(BlueprintCallable, Category = "BIB|Tactics", meta = (WorldContext = "WorldContextObject"))
	static void SampleCrowd(const UObject* WorldContextObject);

	/** "samples=.. maxCommitted=.. ..." (key=value pairs) since the last reset. */
	UFUNCTION(BlueprintPure, Category = "BIB|Tactics")
	static FString GetCrowdStats();
};
