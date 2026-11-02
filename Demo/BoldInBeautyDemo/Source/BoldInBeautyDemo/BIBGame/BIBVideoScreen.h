// A video screen in the Ark (owner request 2026-10-09: use every file of the bb-Showreel pack). The six in-game ads run on the
// town's LED ad pillars (the HoloAdPlaceholder actors of L_Ark, owner 2026-10-09: "just place them on the LED signs as ads"),
// two-sided, cycling through the six; the download trailer plays over the Operator Generator Factory, the master reel in the
// arcade hall, and the UI sheets and the transitions showcase on smaller screens around the town. Videos come from Content/Movies
// (Tools/motion_movies.sh). Screens showing the same movie share one player and texture (one decoder per movie, however many
// pillars); only that first screen carries the ad's sound, quietly and only nearby. Without the movie file nothing is spawned.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIBVideoScreen.generated.h"

class UMaterialInstanceDynamic;
class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;
class UStaticMeshComponent;

UCLASS(NotPlaceable)
class BOLDINBEAUTYDEMO_API ABIBVideoScreen : public AActor
{
	GENERATED_BODY()

public:
	ABIBVideoScreen();

	/** Movie = file name in Content/Movies without ".mp4". Width in cm (16:9). bSound: the ad's audio plays near the screen. */
	static ABIBVideoScreen* Spawn(UWorld* World, const FString& Movie, const FVector& Location, float Yaw, float WidthCm, bool bSound, bool bTwoSided = false);
	static bool MovieExists(const FString& Movie);

	const FString& GetMovie() const { return Movie; }
	bool IsPlaying() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Panel;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BackPanel;

	UPROPERTY()
	TObjectPtr<UMediaSoundComponent> Sound;

	UPROPERTY()
	TObjectPtr<UMediaPlayer> Player;

	UPROPERTY()
	TObjectPtr<UMediaTexture> Texture;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FString Movie;
	TWeakObjectPtr<ABIBVideoScreen> SharedFrom;   // the screen whose player this one shows
	void Start(const FString& InMovie, float WidthCm, bool bSound, bool bTwoSided);
};
