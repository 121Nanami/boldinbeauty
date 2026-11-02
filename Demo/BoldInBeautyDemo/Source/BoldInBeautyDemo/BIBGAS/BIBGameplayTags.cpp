#include "BIBGAS/BIBGameplayTags.h"

#include "BIBRules/BIBKitTypes.h"
#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBIBTags, Log, All);

namespace BIBTags
{
	FGameplayTag Find(const FName& Name)
	{
		static TMap<FName, FGameplayTag> Cache;
		if (const FGameplayTag* Found = Cache.Find(Name))
		{
			return *Found;
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Name, false);
		if (!Tag.IsValid())
		{
			// Not cached: the tag may be registered later (editor tag reload); log once per name.
			static TSet<FName> Reported;
			if (!Reported.Contains(Name))
			{
				Reported.Add(Name);
				UE_LOG(LogBIBTags, Warning, TEXT("Gameplay tag '%s' is not registered. Run Tools/gen_tags.py."), *Name.ToString());
			}
			return Tag;
		}
		Cache.Add(Name, Tag);
		return Tag;
	}

	FGameplayTag Find(const char* Name)
	{
		return Find(FName(UTF8_TO_TCHAR(Name)));
	}

	FGameplayTag Find(const std::string& Name)
	{
		return Find(FName(UTF8_TO_TCHAR(Name.c_str())));
	}

	FGameplayTag Cooldown(const FString& KitName, const TCHAR* SlotName)
	{
		return Find(FName(*FString::Printf(TEXT("BIB.Cooldown.%s.%s"), *KitName, SlotName)));
	}

	FGameplayTag Ability(const FString& KitName, const TCHAR* SlotName)
	{
		return Find(FName(*FString::Printf(TEXT("BIB.Ability.%s.%s"), *KitName, SlotName)));
	}

	FGameplayTag Cue(int32 CueId)
	{
		return Find(BIB::Kits::CueName(CueId));
	}

	FGameplayTag DataDamage() { return Find("BIB.Data.Damage"); }
	FGameplayTag DataHeal() { return Find("BIB.Data.Heal"); }
	FGameplayTag DataDuration() { return Find("BIB.Data.Duration"); }
	FGameplayTag StateAlive() { return Find("BIB.State.Alive"); }
	FGameplayTag StateDead() { return Find("BIB.State.Dead"); }
	FGameplayTag StateInvulnerable() { return Find("BIB.State.Invulnerable"); }
	FGameplayTag StateBlocking() { return Find("BIB.State.Blocking"); }
	FGameplayTag StateParrying() { return Find("BIB.State.Parrying"); }
	FGameplayTag StateDodging() { return Find("BIB.State.Dodging"); }
	FGameplayTag StateStunned() { return Find("BIB.State.Stunned"); }
	FGameplayTag StateCasting() { return Find("BIB.State.Casting"); }
	FGameplayTag StateMovementLocked() { return Find("BIB.State.MovementLocked"); }
	FGameplayTag StateShielded() { return Find("BIB.State.Shielded"); }
	FGameplayTag StateActive() { return Find("BIB.State.Active"); }
	FGameplayTag TeamPlayer() { return Find("BIB.Team.Player"); }
	FGameplayTag TeamEnemy() { return Find("BIB.Team.Enemy"); }

	int32 LogMissingTags()
	{
		int32 Missing = 0;
		for (const std::string& Name : BIB::Kits::AllGameplayTags())
		{
			const FName TagName(UTF8_TO_TCHAR(Name.c_str()));
			if (!UGameplayTagsManager::Get().RequestGameplayTag(TagName, false).IsValid())
			{
				UE_LOG(LogBIBTags, Warning, TEXT("Missing gameplay tag: %s"), *TagName.ToString());
				++Missing;
			}
		}
		if (Missing == 0)
		{
			UE_LOG(LogBIBTags, Log, TEXT("All BIB gameplay tags are registered."));
		}
		return Missing;
	}
}
