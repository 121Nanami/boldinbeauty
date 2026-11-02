// The bb-Showreel motion pack in the game (owner request 2026-10-09: "a file filled with stylised transitions, ui etc to use in game
// ... they are all animated, to be extra stylised. Make sure to use them all").
//
// Tools/motion_export.py renders each of the pack's 49 UI pieces and 5 page transitions at 12 fps (the game's anime frame rate)
// with its sample values blanked, packs the frames into atlas pages (/Game/BIBMotion/<id>/T_<id>_NN, imported by
// Tools/motion_import.py) and writes Content/BIBMotion/motion.json: per piece its atlas layout, its timeline, named clips (the
// pack's sub-moments: BLOCK / PARRY / DODGE, the button presses, a transition) and value slots (where the real number or text is
// drawn: the combo count, the floor, the reward amounts, a toast's message ...).
//
// BIBMotion plays them on a full-screen layer above the HUD and menus. A play is a piece + clip, either laid out like the pack
// (its 1920x1080 frame fitted to the screen) or anchored: the piece's anchor point follows a screen position (a target lock on
// an enemy, a button press under the cursor). When the generated assets are missing (a fresh clone before
// Tools/import_motion_pack.sh) nothing is drawn and the game plays as before.
#pragma once

#include "CoreMinimal.h"

class SWidget;
class FPaintArgs;
struct FGeometry;
class FSlateWindowElementList;

namespace BIBMotion
{
	struct FPlay
	{
		FString Piece;
		FString Clip;
		float Speed = 1.f;
		float Opacity = 1.f;
		float Scale = 1.f;
		bool bLoop = false;
		bool bAnchored = false;
		FVector2D Anchor = FVector2D::ZeroVector;   // viewport-local pixels (the layer's space) when bAnchored
		TMap<FString, FString> Values;              // slot name -> text
		FString Tag;                                // a new play with the same tag replaces the old one
		float Delay = 0.f;                          // seconds before it starts (sequences: ticket -> charge -> reveal)
		// World VFX (Set 2, sheets 09-18): the piece's anchor follows this actor's feet and is scaled so the pack's 300 px body
		// reference matches the actor's height on screen; bFlip mirrors it (a caster facing left).
		TWeakObjectPtr<AActor> Follow;
		float BodyCm = 185.f;
		bool bFlip = false;
	};

	/** Starts a play; returns false when the piece or clip does not exist (assets not imported). */
	BOLDINBEAUTYDEMO_API bool Play(const FPlay& Params);
	/** Shorthand: a piece's clip, laid out like the pack. */
	BOLDINBEAUTYDEMO_API bool Play(const FString& Piece, const FString& Clip, float Speed = 1.f, const FString& Tag = FString());
	BOLDINBEAUTYDEMO_API void Stop(const FString& Tag);
	BOLDINBEAUTYDEMO_API bool IsPlaying(const FString& Tag);
	BOLDINBEAUTYDEMO_API void StopAll();
	/** Stops every play whose tag starts with Prefix (an angel's status loops when it dies). */
	BOLDINBEAUTYDEMO_API void StopPrefix(const FString& Prefix);
	/** The player controller used to project followed actors to the screen. */
	BOLDINBEAUTYDEMO_API void SetController(APlayerController* Controller);
	/** A Set 2 VFX piece for an operator's slot: "M1", "1", "2", "3", "S", "U", "D1".."D3", "IDLE", "ALWAYS" ... ("" when none). */
	BOLDINBEAUTYDEMO_API FString AbilityPiece(const FString& Who, const FString& SlotKey);
	/** A Set 2 VFX piece by its slug ("dash-attack-trail" -> "fx09-01-dash-attack-trail"); "" when missing. */
	BOLDINBEAUTYDEMO_API FString FindFx(const FString& Slug);
	/** The manifest's anchor mode of a piece ("caster", "enemy", "centre", "" for UI). */
	BOLDINBEAUTYDEMO_API FString PieceMode(const FString& Piece);
	BOLDINBEAUTYDEMO_API bool HasPiece(const FString& Piece);
	/** Pieces with assets, and every play started since launch (test hooks). */
	BOLDINBEAUTYDEMO_API int32 PieceCount();
	BOLDINBEAUTYDEMO_API const TSet<FString>& PlayedPieces();
	/** The viewport position of the mouse / last touch, in the layer's space (button presses play there). */
	BOLDINBEAUTYDEMO_API FVector2D CursorPosition();

	/** The full-screen layer that draws every active play (added to the game viewport by the player controller). */
	TSharedRef<SWidget> MakeLayer();

	/** Draws one frame of a piece's clip at Seconds into Dest (layer pixels) - for widgets that own a piece (mobile joysticks). */
	BOLDINBEAUTYDEMO_API bool PaintFrame(const FString& Piece, const FString& Clip, double Seconds, bool bLoop, const FGeometry& Geometry,
		FSlateWindowElementList& Out, int32 Layer, const FVector2D& Centre, float PixelScale, float Opacity = 1.f);
}
