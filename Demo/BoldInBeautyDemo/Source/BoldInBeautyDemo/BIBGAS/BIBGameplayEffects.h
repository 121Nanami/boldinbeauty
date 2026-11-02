// GameplayEffects the demo builds in code (no assets needed): damage, healing, timed tag grants (cooldowns, i-frames,
// statuses). Each effect is created once and rooted; the magnitude and duration are passed as SetByCaller values with
// the tags BIB.Data.Damage / BIB.Data.Heal / BIB.Data.Duration.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayEffect;

namespace BIBEffects
{
	/** Instant: adds SetByCaller BIB.Data.Damage to the IncomingDamage meta attribute. */
	const UGameplayEffect* Damage();
	/** Instant: adds SetByCaller BIB.Data.Heal to the IncomingHealing meta attribute. */
	const UGameplayEffect* Heal();
	/** HasDuration (SetByCaller BIB.Data.Duration): grants GrantedTag to the target while it runs. One effect per tag. */
	const UGameplayEffect* TimedTag(const FGameplayTag& GrantedTag);
}
