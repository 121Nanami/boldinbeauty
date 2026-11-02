// Full-screen overlays on top of the hub (owner request 2026-10-06), code-built in Slate so no editor asset is needed:
//   * the arcade games Night Shift, Rhythm, Halo Fall, Neon Memory, Firewall Breach and Ark Rally, and the arena fight replay (the rules are
//     BIBRules/BIBArcade.h, BIBMiniGames.h, BIBRally.h and BIBArena.h);
//   * Ultimate Custom Night (2026-10-14; rules BIBRules/BIBUltimateNight.h): the same office / camera look as Night Shift;
//   * a one-line text prompt (friend ids, gang names, admin mail and notices, the commander name);
//   * key capture for Settings > Controls > Key bindings;
//   * the floating damage numbers layer in battle (Settings > Gameplay).
// Sounds are synthesised (BIBRules/BIBSynth.h) and played through procedural sound waves; Media's cue is panned to her side.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class UBIBGameInstance;
class UWorld;
class APlayerController;

enum class EBIBOverlayKind : uint8
{
	NightShift,
	Rhythm,
	ArenaBattle,
	HaloFall,        // the 2026-10-07 mini-games (BIBRules/BIBMiniGames.h)
	NeonMemory,
	FirewallBreach,
	ArkRally,         // 2026-10-15 (BIBRules/BIBRally.h): A = difficulty, B = 1 for the player's own track
	UltimateNight    // A = mode (BIBRules/BIBUltimateNight.h)
};

namespace BIBOverlay
{
	/** Opens a game. OnDone receives the result line (rewards are already paid and saved). */
	void OpenGame(UWorld* World, UBIBGameInstance* Instance, EBIBOverlayKind Kind, int32 A, int32 B, TFunction<void(const FString&)> OnDone);
	/** Asks for a line of text. OnDone(bOk, Text). */
	void OpenPrompt(UWorld* World, const FString& Title, const FString& Hint, const FString& Initial, int32 MaxLength, TFunction<void(bool, const FString&)> OnDone);
	/** Waits for one key (Escape cancels: OnDone gets "Escape"). */
	void OpenKeyCapture(UWorld* World, const FString& Label, TFunction<void(const FString&)> OnDone);
	bool IsOpen();
	void CloseAll();

	/** Test hooks (Tools/verify_arcade.py through UBIBBlueprintLibrary): a one-line description of the open overlay, and commands
	 *  such as "key:A", "keyup:Q", "advance:10", "night:media:left", "rhythm:autoplay", "halo:win",
	 *  "memory:solve", "breach:autoplay", "rally:autoplay", "rally:respawn", "ucn:autoplay", "close". */
	FString DebugState();
	bool DebugCommand(const FString& Command);

	/** The damage numbers layer added by the player controller (draws ABIBGameMode's recent hits). */
	TSharedRef<SWidget> MakeDamageLayer(APlayerController* Controller);
}
