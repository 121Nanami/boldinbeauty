// Combat feel tuning from the game config (owner request 2026-10-13: tuning values in data / config, not hard-coded).
// The numbers live in Config/DefaultGame.ini [BIB.CombatFeel] (BIBRules/BIBHitFeel.h lists every key); this file copies
// them into BIB::Feel::Tuning() and holds the hit sounds named there.
//
// Console (development builds and the editor):
//   BIBFeelReload            re-reads Config/DefaultGame.ini from disk (tune while playing)
//   BIBFeel <Key> <Value>    sets one value now, e.g. "BIBFeel Weight.Heavy.HitstopFrames 9"
//   BIBFeel                  prints every key and its value to the log
#pragma once

#include "CoreMinimal.h"
#include "BIBRules/BIBHitFeel.h"

class USoundBase;

namespace BIBFeelConfig
{
	/** Reads the config once (later calls do nothing unless bForce). */
	void Load(bool bForce = false);

	/** Sound layers played on a hit of this weight (Sound.<Weight>); empty while no asset is set. */
	const TArray<USoundBase*>& HitSounds(BIB::Feel::EWeight Weight);
	/** Named one-off sounds: "UltCinematic", "PerfectDodge". */
	const TArray<USoundBase*>& NamedSounds(const FString& Name);
	/** Sound.Volume (multiplied with the player's SFX and master volume). */
	float SoundVolume();
}
