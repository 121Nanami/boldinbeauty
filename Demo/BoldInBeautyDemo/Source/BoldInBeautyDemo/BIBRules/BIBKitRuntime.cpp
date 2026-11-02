// Boldinbeauty demo — operator kit runtime: casting, timelines, steps, zones, projectiles, buffs, passives.
// The named hook mechanics live in BIBKitHooks.cpp.
#include "BIBKitRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace BIB
{
	namespace Kits
	{
		namespace
		{
			constexpr double Epsilon = 1e-9;

			// Jester's rotation (deterministic, not random): act ids are 0 melee, 1 ranged, 2 defence, 3 mage, 4 speed.
			const int ActOrder[5] = {0, 3, 4, 1, 2};

			std::string Fmt(const char* Format, double Value)
			{
				char Buffer[96];
				std::snprintf(Buffer, sizeof(Buffer), Format, Value);
				return Buffer;
			}
		}

		FRuntime::FRuntime(const FKitDef& InKit, IWorld& InWorld)
			: KitPtr(&InKit)
			, World(&InWorld)
		{
			CopiedPassives.reserve(8);
			ResetForFight();
		}

		FRuntime::~FRuntime() = default;

		double FRuntime::Rand()
		{
			return bForcedRandom ? ForcedRandom : World->Random01();
		}

		// ============================================================================ lifecycle
		void FRuntime::ResetForFight()
		{
			for (int Index = 0; Index < 2; ++Index)
			{
				Res[Index] = Kit().ResourceStart[Index];
			}
			ShieldAmount = Kit().Traits.InitialShield;
			TimedShield = 0.0;
			ShieldExpiresAt = 0.0;
			PassiveDepth = 0;
			PendingTimelines.clear();
			bInTimelineTick = false;
			for (double& Timer : AutoTimer)
			{
				Timer = 0.0;
			}
			bHasLastPos = false;
			TakeoverHealthScale = 1.0;
			UltimateVariantIndex = 0;
			LastDamagedAt = -100.0;
			HealthScaleValue = 1.0;
			LockedUntil = 0.0;
			InvulnerableUntil = 0.0;
			LastDamage = 0.0;
			TotalDealt = 0.0;
			Luck = 0.0;
			ComboIndex = 0;
			LastBasicAt = -100.0;
			Timelines.clear();
			Zones.clear();
			Projectiles.clear();
			Buffs.clear();
			Reactions.clear();
			RecentCasts.clear();
			CopiedPassives.clear();
			Passives.clear();
			for (const FPassiveDef& Def : Kit().Passives)
			{
				FPassiveState State;
				State.Def = &Def;
				Passives.push_back(State);
			}
			DamageTakenWhileCasting = 0.0;
			StoredTarget = -1;
			StoredValue = 0.0;
			bStoringTaken = false;
			CountedKills.clear();
			KillCount = 0;
			SeedLastPos = FVec();
			Act = 0;
			ActCasts = 0;
			Front = 0;
			bCurse = false;
			CurseLinkCount = 0;
			TakeoverUntil = 0.0;
			AdaptStats[0] = AdaptStats[1] = AdaptStats[2] = 1.0;
			AdaptUse[0] = AdaptUse[1] = AdaptUse[2] = 0.0;
			Adapted.clear();
			AdaptSelected = 0;
			UltimateActiveUntil = 0.0;
			AdaptPassiveTimer = 0.0;
			bRecording = false;
			EchoLog.clear();
			ZeroHourUntil = 0.0;
			Infected.clear();
			InfectionClock = 0.0;
			Flowers.clear();
			SoilUntil = 0.0;
			SoilToggle = 0;
			SoilTouched.clear();
			SeedTarget = -1;
			SeedDetonateAt = 0.0;
			SeedStoredDamage = 0.0;
		}

		void FRuntime::ClearBattlefield(bool bKeepInfection)
		{
			Timelines.clear();
			Zones.clear();
			Projectiles.clear();
			Flowers.clear();
			Reactions.clear();
			bCurse = false;
			CurseLinkCount = 0;
			if (TakeoverUntil > 0.0)
			{
				EndTakeover();
			}
			SoilUntil = 0.0;
			bRecording = false;
			EchoLog.clear();
			ZeroHourUntil = 0.0;
			SeedTarget = -1;
			if (!bKeepInfection)
			{
				Infected.clear();
			}
			CountedKills.clear();
		}

		void FRuntime::OnSwitchIn()
		{
			FContext Ctx;
			FirePassives(ETrigger::OnSwitchIn, Ctx);
		}

		void FRuntime::SetTeammatePassives(const std::vector<const FPassiveDef*>& InPassives)
		{
			TeammatePassives = InPassives;
		}

		void FRuntime::SelectUltimateVariant(int Index)
		{
			const int Max = static_cast<int>(Kit().UltimateVariants.size());
			UltimateVariantIndex = std::max(0, std::min(Index, Max));
		}

		const FAbilityDef& FRuntime::Ability(ESlotId Slot) const
		{
			if (Slot == ESlotId::Ultimate && UltimateVariantIndex > 0
				&& UltimateVariantIndex <= static_cast<int>(Kit().UltimateVariants.size()))
			{
				return Kit().UltimateVariants[static_cast<size_t>(UltimateVariantIndex - 1)];
			}
			return Kit().Abilities[static_cast<int>(Slot)];
		}

		bool FRuntime::HasPassive(const char* PassiveId) const
		{
			for (const FPassiveState& State : Passives)
			{
				if (State.Def && State.Def->Id == PassiveId)
				{
					return true;
				}
			}
			return false;
		}

		// ============================================================================ modifiers
		double FRuntime::Mod(EBuff Buff) const
		{
			double Total = 0.0;
			const double T = Now();
			for (const FBuff& Entry : Buffs)
			{
				if (Entry.Stat == Buff && Entry.ExpiresAt > T)
				{
					Total += Entry.Magnitude;
				}
			}
			for (const FPassiveState& State : Passives)
			{
				if (State.Def && State.Def->Trigger == ETrigger::Always)
				{
					Total += State.Def->Mods[static_cast<int>(Buff)];
				}
			}
			if (Buff == EBuff::DamageTaken)
			{
				Total += Kit().Traits.DamageTakenMod;
			}
			else if (Buff == EBuff::StatusResist)
			{
				Total += Kit().Traits.StatusResist;
			}
			else if (Buff == EBuff::Immovable && Kit().Traits.bKnockbackImmune)
			{
				Total += 1.0;
			}
			else if (Buff == EBuff::DamageDealt)
			{
				Total += AdaptStats[0] - 1.0;
			}
			else if (Buff == EBuff::MoveSpeed)
			{
				Total += AdaptStats[1] - 1.0;
			}
			return Total;
		}

		double FRuntime::DamageDealtMult() const { return std::max(0.05, 1.0 + Mod(EBuff::DamageDealt)); }
		double FRuntime::DamageTakenMult() const { return std::max(0.05, 1.0 + Mod(EBuff::DamageTaken)); }
		double FRuntime::MoveSpeedMult() const { return std::max(0.1, 1.0 + Mod(EBuff::MoveSpeed)); }
		double FRuntime::AttackSpeedMult() const { return std::max(0.2, 1.0 + Mod(EBuff::AttackSpeed)); }

		bool FRuntime::HasBuffActive(EBuff Buff) const
		{
			const double T = Now();
			for (const FBuff& Entry : Buffs)
			{
				if (Entry.Stat == Buff && Entry.ExpiresAt > T)
				{
					return true;
				}
			}
			return false;
		}

		bool FRuntime::Invulnerable() const
		{
			return Now() < InvulnerableUntil;
		}

		void FRuntime::AddBuff(EBuff Stat, double Magnitude, double Seconds)
		{
			FBuff Entry;
			Entry.Stat = Stat;
			Entry.Magnitude = Magnitude;
			Entry.ExpiresAt = Now() + Seconds;
			for (FBuff& Existing : Buffs)
			{
				if (Existing.Stat != Stat || Existing.ExpiresAt <= Now())
				{
					continue;
				}
				if (Stat == EBuff::Evade)
				{
					// Charges add up (capped), the timer refreshes.
					Existing.Magnitude = std::min(5.0, Existing.Magnitude + Magnitude);
					Existing.ExpiresAt = std::max(Existing.ExpiresAt, Entry.ExpiresAt);
					return;
				}
				if (Stat == EBuff::Immovable || std::fabs(Existing.Magnitude - Magnitude) < 1e-9)
				{
					// The same effect applied again (a recast, a repeating passive) refreshes instead of stacking.
					Existing.Magnitude = Magnitude;
					Existing.ExpiresAt = std::max(Existing.ExpiresAt, Entry.ExpiresAt);
					return;
				}
			}
			Buffs.push_back(Entry);
		}

		void FRuntime::AddShield(double Amount, double Seconds)
		{
			ShieldAmount += Amount;
			if (Seconds > 0.0)
			{
				TimedShield += Amount;
				ShieldExpiresAt = std::max(ShieldExpiresAt, Now() + Seconds);
			}
			else if (Kit().Traits.MaxShield > 0.0)
			{
				ShieldAmount = std::min(ShieldAmount, Kit().Traits.MaxShield + TimedShield);
			}
		}

		void FRuntime::SpendResource(int Index, double Amount)
		{
			if (Index >= 0 && Index < 2)
			{
				Res[Index] = std::max(0.0, Res[Index] - Amount);
			}
		}

		void FRuntime::GainResource(int Index, double Amount)
		{
			if (Index >= 0 && Index < 2)
			{
				Res[Index] = std::max(0.0, std::min(Kit().ResourceMax[Index] > 0.0 ? Kit().ResourceMax[Index] : 1e9, Res[Index] + Amount));
			}
		}

		void FRuntime::SelfCost(double Fraction)
		{
			// Costs and sacrifices can never kill the caster.
			const double Cost = std::min(Fraction * World->CasterMaxHP(), std::max(0.0, World->CasterHP() - 1.0));
			if (Cost > 0.0)
			{
				World->CasterSelfDamage(Cost);
			}
		}

		// ============================================================================ casting
		bool FRuntime::IsCasting() const
		{
			for (const FTimeline& Timeline : Timelines)
			{
				if (Now() < Timeline.LockEndsAt)
				{
					return true;
				}
				if (Timeline.bHolding && !Timeline.bReleased)
				{
					return true;
				}
			}
			return false;
		}

		bool FRuntime::IsCasting(ESlotId Slot) const
		{
			for (const FTimeline& Timeline : Timelines)
			{
				if (Timeline.Slot == Slot)
				{
					return true;
				}
			}
			return false;
		}

		bool FRuntime::MovementLocked() const
		{
			return Now() < LockedUntil;
		}

		bool FRuntime::InRecovery(double Grace) const
		{
			const double T = Now();
			if (T >= LockedUntil)
			{
				return false;
			}
			bool bAny = false;
			for (const FTimeline& Timeline : Timelines)
			{
				if (T >= Timeline.LockEndsAt)
				{
					continue;
				}
				// Only attacks have a recovery: a cast that deals no damage, is still held, or still has hits to deal keeps its lock.
				if (Timeline.LastHitAt < 0.0 || (Timeline.bHolding && !Timeline.bReleased) || T < Timeline.LastHitAt + Grace)
				{
					return false;
				}
				for (const FScheduled& Scheduled : Timeline.Steps)
				{
					if (!Scheduled.bDone && StepDealsDamage(Scheduled.Step))
					{
						return false;
					}
				}
				bAny = true;
			}
			return bAny;
		}

		void FRuntime::CancelRecovery()
		{
			const double T = Now();
			LockedUntil = std::min(LockedUntil, T);
			for (FTimeline& Timeline : Timelines)
			{
				Timeline.LockEndsAt = std::min(Timeline.LockEndsAt, T);
			}
		}

		double FRuntime::CooldownFor(const FAbilityDef& Def) const
		{
			double Cooldown = Def.Cooldown;
			if (Def.Slot == ESlotId::Basic)
			{
				Cooldown /= AttackSpeedMult();
			}
			for (const FStep& Step : Def.Steps)
			{
				if (Step.Kind == EStep::Hook && Step.HookId == EHook::AdaptCastCopied && !Adapted.empty())
				{
					Cooldown = Adapted[static_cast<size_t>(AdaptSelected)].Cooldown;
				}
			}
			return Cooldown;
		}

		FCastResult FRuntime::CanCast(ESlotId Slot) const
		{
			FCastResult Result;
			const FAbilityDef& Def = Ability(Slot);
			if (Def.Id.empty())
			{
				Result.Reason = "no ability";
				return Result;
			}
			if (Def.bAutomatic)
			{
				Result.Reason = "automatic";
				return Result;
			}
			if (IsCasting() && Def.Cast != ECast::Toggle)
			{
				Result.Reason = "busy";
				return Result;
			}
			if (Slot == ESlotId::Ultimate && Now() < UltimateActiveUntil)
			{
				Result.Reason = "ultimate still active";
				return Result;
			}
			const double Remaining = World->CooldownRemaining(Slot);
			if (Remaining > 0.0)
			{
				Result.Reason = Fmt("cooldown %.1fs", Remaining);
				return Result;
			}
			if (Def.CostResource >= 0 && Res[Def.CostResource] + Epsilon < Def.CostAmount)
			{
				Result.Reason = "needs " + Fmt("%.0f ", Def.CostAmount) + Kit().ResourceName[Def.CostResource];
				return Result;
			}
			if (Def.RequiresResource >= 0 && Res[Def.RequiresResource] + Epsilon < Def.RequiresAmount)
			{
				Result.Reason = "needs " + Fmt("%.0f ", Def.RequiresAmount) + Kit().ResourceName[Def.RequiresResource];
				return Result;
			}
			if (Def.HealthCostFraction > 0.0 && World->CasterHP() <= World->CasterMaxHP() * Def.HealthCostFraction + 1.0)
			{
				Result.Reason = "not enough health";
				return Result;
			}
			if (Def.RequiresStatusOnEnemy != EStatus::None)
			{
				std::vector<FEnemyView> All;
				World->GetEnemies(All);
				bool bFound = false;
				for (const FEnemyView& Enemy : All)
				{
					if (World->StatusStacks(Enemy.Id, Def.RequiresStatusOnEnemy) > 0)
					{
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					Result.Reason = std::string("needs an enemy with ") + StatusRule(Def.RequiresStatusOnEnemy).Name;
					return Result;
				}
			}
			for (const FStep& Step : Def.Steps)
			{
				if (Step.Kind != EStep::Hook)
				{
					continue;
				}
				if (Step.HookId == EHook::AdaptCastCopied && Adapted.empty())
				{
					Result.Reason = "nothing adapted yet";
					return Result;
				}
				if (Step.HookId == EHook::AdaptSwitch && Adapted.size() < 2)
				{
					Result.Reason = "needs 2 adapted abilities";
					return Result;
				}
			}
			Result.bOk = true;
			return Result;
		}

		FCastResult FRuntime::Cast(ESlotId Slot)
		{
			FCastResult Result = CanCast(Slot);
			if (!Result.bOk)
			{
				return Result;
			}
			const FAbilityDef& Def = Ability(Slot);
			if (Def.CostResource >= 0)
			{
				SpendResource(Def.CostResource, Def.CostAmount);
			}
			if (Def.HealthCostFraction > 0.0)
			{
				SelfCost(Def.HealthCostFraction);
			}
			World->StartCooldown(Slot, CooldownFor(Def));
			if (Slot == ESlotId::Ultimate)
			{
				UltimateActiveUntil = Now() + std::max(0.5, Def.TotalSeconds);
			}
			if (Slot == ESlotId::Basic)
			{
				const bool bContinues = Now() - LastBasicAt <= Def.ComboWindow;
				ComboIndex = bContinues ? (ComboIndex + 1) % std::max(1, Def.ComboLength) : 0;
				LastBasicAt = Now();
			}
			else
			{
				RecentCasts.push_back(&Def);
				if (RecentCasts.size() > 3)
				{
					RecentCasts.erase(RecentCasts.begin());
				}
			}
			if (Slot != ESlotId::Basic && Def.Cast != ECast::Toggle)
			{
				LastDamage = 0.0;
			}
			BeginTimeline(Def, Slot, 0.0, false, Now());
			if (Slot != ESlotId::Basic)
			{
				if (IsKit("Jester"))
				{
					++ActCasts;
					Act = ActOrder[ActCasts % 5];
				}
				else if (IsKit("Weather"))
				{
					Front = (Front + 1) % 5;
				}
			}
			FContext Ctx;
			FirePassives(ETrigger::OnAbilityCast, Ctx, static_cast<int>(Slot));
			return Result;
		}

		void FRuntime::Release(ESlotId Slot)
		{
			for (FTimeline& Timeline : Timelines)
			{
				if (Timeline.Slot == Slot && Timeline.bHolding && !Timeline.bReleased)
				{
					Timeline.bReleased = true;
					Timeline.Held = std::min(Now() - Timeline.Start, std::max(0.0, Timeline.Def->TotalSeconds));
					double End = Now();
					for (FScheduled& Scheduled : Timeline.Steps)
					{
						Scheduled.At = Now() + Scheduled.Step.At;
						End = std::max(End, Scheduled.At + Scheduled.Step.Duration);
					}
					Timeline.EndsAt = End;
				}
			}
		}
	}
}
