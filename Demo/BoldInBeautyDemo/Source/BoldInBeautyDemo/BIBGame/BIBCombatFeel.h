// Shared combat-feel layer (owner task 2026-10-12, "some operator abilities feel flat"): PRESENTATION ONLY. Nothing here reads or
// writes damage, cooldowns, radii, durations or scaling; it only decides how strong the glow, shake, field-of-view punch, flash,
// ground telegraph and sound hooks of an event are, and how many of them may exist at once.
//
//   FConfig     the player's Combat feel settings (BIB::FSettings: CombatFeel %, per-effect switches) + Reduce motion + a cap on
//               simultaneous effects that follows the Effects graphics group (Low 8 / Medium 14 / High 22 / Epic 32).
//   FSlotFeel   how heavy each ability slot feels: multipliers on the existing hit stop, shake and ring, and a field-of-view punch.
//   PlayCue     an audio hook: plays /Game/BIB/Audio/Combat/<Name> when the owner has added that sound, silently does nothing when
//               it has not (the list of hook names is in reports/demo/2026-10-12-combat-feel-pass.md).
//
// Defaults (100 %, every switch on) reproduce the pre-existing hit stop, shake and flash exactly.
#pragma once

#include "CoreMinimal.h"
#include "BIBRules/BIBMeta.h"

class UWorld;

namespace BIBCombatFeel
{
	struct FConfig
	{
		float Scale = 1.f;          // 0 = every effect off, 1 = as designed, 1.5 = exaggerated
		bool bHitStop = true;
		bool bFlash = true;
		bool bFov = true;
		bool bBeats = true;         // ultimate beat: charge ring, vignette pulse, brief slow-down
		bool bTelegraph = true;     // ground rings under the operator's own windups
		bool bCalm = false;         // Reduce motion: no flashes, no field-of-view punch, no vignette pulse, no slow-down
		int32 FxCap = 22;           // simultaneous glowing effects (ABIBFx), essential ones (enemy tells) ignore it

		float HitStopScale() const { return bHitStop ? Scale : 0.f; }
		float FlashScale() const { return (bFlash && !bCalm) ? Scale : 0.f; }
		float FovScale() const { return (bFov && !bCalm) ? Scale : 0.f; }
		float BeatScale() const { return (bBeats && !bCalm) ? Scale : 0.f; }
		float TelegraphScale() const { return (bTelegraph && Scale > 0.f) ? 1.f : 0.f; }
	};

	FConfig FromSettings(const BIB::FSettings& Settings);

	/** Heaviness of an ability slot. SlotIndex is an EBIBSlot value: 0 Punch, 1-3 Move 1-3, 4 Special, 5 Ultimate, 6 Dodge, 7 Parry. */
	struct FSlotFeel
	{
		float HitStop;     // x the existing hit stop of a hit
		float Shake;       // x the existing camera shake of a heavy hit
		float FovPunch;    // degrees of field-of-view punch on a heavy hit (0 = none)
		bool bGroundRing;  // a ring on the ground under the struck angel
	};
	const FSlotFeel& ForSlot(int32 SlotIndex);

	/** Plays the optional sound /Game/BIB/Audio/Combat/<Name>. Cheap when the sound is missing (one failed load per name). */
	void PlayCue(UWorld* World, const TCHAR* Name, const FVector& At, float Volume = 1.f);
}
