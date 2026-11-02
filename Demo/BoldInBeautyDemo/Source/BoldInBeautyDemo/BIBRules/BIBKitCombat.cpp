// Boldinbeauty demo — operator kit runtime: passives, reactions, incoming damage, per-frame update, HUD text.
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
			const char* const ActNames[5] = {"Melee", "Ranged", "Defence", "Mage", "Speed"};
			const char* const FrontNames[5] = {"Rain", "Snow", "Wind", "Storm", "Heat"};
			const int ActOrder[5] = {0, 3, 4, 1, 2};
			constexpr double RunSpeedReference = 700.0;   // cm/s used to normalise Adapt's movement usage
			constexpr double AdaptRatePerSecond = 0.05;   // SOURCE (Adapt): 5% per second
			constexpr double AdaptMin = 0.5;              // PLACEHOLDER bounds for the drifting stats
			constexpr double AdaptMax = 2.5;
			constexpr double CurseLinkDrain = 0.01;       // SOURCE (Arita): 1% health per second

			std::string Fmt(const char* Format, double A, double B = 0.0)
			{
				char Buffer[128];
				std::snprintf(Buffer, sizeof(Buffer), Format, A, B);
				return Buffer;
			}
		}

		const char* FRuntime::ActName() const { return ActNames[Act % 5]; }
		const char* FRuntime::FrontName() const { return FrontNames[Front % 5]; }

		// ============================================================================ passives
		bool FRuntime::PassiveConditionMet(const FPassiveDef& Def) const
		{
			if (Def.CondResource >= 0 && Def.CondResource < 2 && Res[Def.CondResource] + 1e-9 < Def.CondMin)
			{
				return false;
			}
			if (Def.CondEnemyStatus != EStatus::None)
			{
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				for (const FEnemyView& Enemy : All)
				{
					if (World->StatusStacks(Enemy.Id, Def.CondEnemyStatus) > 0)
					{
						return true;
					}
				}
				return false;
			}
			return true;
		}

		void FRuntime::FirePassives(ETrigger Trigger, const FContext& Ctx, int Filter)
		{
			if (PassiveDepth > 0)
			{
				return;
			}
			++PassiveDepth;
			const double T = Now();
			for (size_t Index = 0; Index < Passives.size(); ++Index)
			{
				const FPassiveDef* Def = Passives[Index].Def;
				if (!Def || Def->Trigger != Trigger)
				{
					continue;
				}
				if (Def->Param >= 0.0 && Filter >= 0 && static_cast<int>(Def->Param) != Filter)
				{
					continue;
				}
				if (T < Passives[Index].IcdUntil || !PassiveConditionMet(*Def))
				{
					continue;
				}
				++Passives[Index].Counter;
				if (Def->EveryN > 1 && Passives[Index].Counter % Def->EveryN != 0)
				{
					continue;
				}
				if (Def->Chance < 1.0 && Rand() > Def->Chance)
				{
					continue;
				}
				Passives[Index].IcdUntil = T + Def->Interval;
				RunStepsNow(Def->Steps, Ctx);
			}
			--PassiveDepth;
		}

		void FRuntime::TickPassives(double Dt)
		{
			const double HpFraction = World->CasterMaxHP() > 0.0 ? World->CasterHP() / World->CasterMaxHP() : 1.0;
			for (size_t Index = 0; Index < Passives.size(); ++Index)
			{
				const FPassiveDef* Def = Passives[Index].Def;
				if (!Def)
				{
					continue;
				}
				if (Def->Trigger == ETrigger::Periodic && Def->Interval > 0.0)
				{
					Passives[Index].Timer += Dt;
					if (Passives[Index].Timer >= Def->Interval)
					{
						Passives[Index].Timer -= Def->Interval;
						if (PassiveConditionMet(*Def))
						{
							FContext Ctx;
							RunStepsNow(Def->Steps, Ctx);
						}
					}
				}
				else if (Def->Trigger == ETrigger::OnLowHealth)
				{
					if (HpFraction < Def->Threshold && !Passives[Index].bBelowLow && Now() >= Passives[Index].IcdUntil)
					{
						Passives[Index].bBelowLow = true;
						Passives[Index].IcdUntil = Now() + Def->Interval;
						FContext Ctx;
						RunStepsNow(Def->Steps, Ctx);
					}
					else if (HpFraction >= Def->Threshold + 0.05)
					{
						Passives[Index].bBelowLow = false;
					}
				}
			}
		}

		// ============================================================================ buffs & shields
		void FRuntime::TickBuffs(double Dt)
		{
			const double T = Now();
			Buffs.erase(std::remove_if(Buffs.begin(), Buffs.end(), [T](const FBuff& Entry) { return Entry.ExpiresAt <= T; }), Buffs.end());
			if (TimedShield > 0.0 && ShieldExpiresAt > 0.0 && T >= ShieldExpiresAt)
			{
				ShieldAmount = std::max(0.0, ShieldAmount - TimedShield);
				TimedShield = 0.0;
				ShieldExpiresAt = 0.0;
			}
			const double Regen = Mod(EBuff::Regen);
			if (Regen > 0.0)
			{
				World->CasterHeal(Regen * World->CasterMaxHP() * Dt);
			}
			const FKitTraits& Traits = Kit().Traits;
			const double BaseShield = ShieldAmount - TimedShield;
			if (Traits.ShieldRegenPerSecond > 0.0 && T - LastDamagedAt >= Traits.ShieldRegenDelay && BaseShield < Traits.MaxShield)
			{
				ShieldAmount = std::min(Traits.MaxShield + TimedShield, ShieldAmount + Traits.ShieldRegenPerSecond * Dt);
			}
		}

		void FRuntime::TickReactions()
		{
			const double T = Now();
			Reactions.erase(std::remove_if(Reactions.begin(), Reactions.end(), [T](const FReactionState& State)
			{
				return State.ExpiresAt <= T || State.TriggersLeft <= 0;
			}), Reactions.end());
		}

		// ============================================================================ incoming damage
		void FRuntime::NotifyDodge()
		{
			FContext Ctx;
			FirePassives(ETrigger::OnDodge, Ctx);
		}

		void FRuntime::NotifyParry()
		{
			FContext Ctx;
			FirePassives(ETrigger::OnParry, Ctx);
		}

		double FRuntime::OnIncomingDamage(double Amount, int AttackerId, bool)
		{
			const double T = Now();
			LastDamagedAt = T;
			FContext Ctx;
			Ctx.TriggerEnemy = AttackerId;
			Ctx.Amount = Amount;

			// 1. evade charges / evade chance
			for (FBuff& Entry : Buffs)
			{
				if (Entry.Stat == EBuff::Evade && Entry.ExpiresAt > T && Entry.Magnitude >= 0.5)
				{
					Entry.Magnitude -= 1.0;
					FirePassives(ETrigger::OnDodge, Ctx);
					return 0.0;
				}
			}
			const double EvadeChance = Mod(EBuff::EvadeChance);
			if (EvadeChance > 0.0 && Rand() < EvadeChance)
			{
				FirePassives(ETrigger::OnDodge, Ctx);
				return 0.0;
			}

			// 2. armed reactions
			for (size_t Index = 0; Index < Reactions.size(); ++Index)
			{
				if (Reactions[Index].Def->Trigger != EReaction::OnDamaged || Reactions[Index].ExpiresAt <= T || Reactions[Index].TriggersLeft <= 0)
				{
					continue;
				}
				--Reactions[Index].TriggersLeft;
				const FReaction* Def = Reactions[Index].Def;
				RunStepsNow(Def->Steps, Ctx);
				if (Def->bNegate)
				{
					return 0.0;
				}
			}

			// 3. modifiers
			double Result = Amount * DamageTakenMult();

			// 4. Nature's Curse: the damage is split 50/50 with the attacker, who is soul-linked
			if (bCurse && AttackerId >= 0)
			{
				const double Half = Result * 0.5;
				World->DamageEnemy(AttackerId, Half, DmgTrue | DmgReflect);
				Result -= Half;
				bool bLinked = false;
				for (int Index = 0; Index < CurseLinkCount; ++Index)
				{
					bLinked = bLinked || CurseLink[Index] == AttackerId;
				}
				if (!bLinked && CurseLinkCount < 8)
				{
					CurseLink[CurseLinkCount++] = AttackerId;
				}
			}

			// 5. reflect
			const double Reflect = Mod(EBuff::Reflect);
			if (Reflect > 0.0 && AttackerId >= 0)
			{
				World->DamageEnemy(AttackerId, Result * Reflect, DmgTrue | DmgReflect);
			}

			// 6. shield
			if (bStoringTaken)
			{
				StoredValue += Result;
			}
			if (ShieldAmount > 0.0 && Result > 0.0)
			{
				const double Absorbed = std::min(ShieldAmount, Result);
				ShieldAmount -= Absorbed;
				TimedShield = std::max(0.0, TimedShield - Absorbed);
				Result -= Absorbed;
				if (ShieldAmount <= 1e-9)
				{
					ShieldAmount = 0.0;
					TimedShield = 0.0;
					for (size_t Index = 0; Index < Reactions.size(); ++Index)
					{
						if (Reactions[Index].Def->Trigger == EReaction::OnShieldBreak && Reactions[Index].ExpiresAt > T && Reactions[Index].TriggersLeft > 0)
						{
							--Reactions[Index].TriggersLeft;
							RunStepsNow(Reactions[Index].Def->Steps, Ctx);
						}
					}
				}
			}
			AdaptUse[2] += Result / std::max(1.0, World->CasterMaxHP() * 0.05);
			Ctx.Amount = Result;
			FirePassives(ETrigger::OnDamaged, Ctx);
			return std::max(0.0, Result);
		}

		// ============================================================================ per-frame update
		void FRuntime::Tick(double Dt)
		{
			TickBuffs(Dt);
			TickTimelines(Dt);
			TickZones(Dt);
			TickProjectiles(Dt);
			TickPassives(Dt);
			TickReactions();
			TickKitState(Dt);
			TickInfection(Dt);
			TickFlowers(Dt);
			const double Flow = Mod(EBuff::CooldownFlow);
			if (Flow > 0.0)
			{
				for (int Index = 0; Index < SlotCount; ++Index)
				{
					World->ReduceCooldown(static_cast<ESlotId>(Index), Dt * Flow);
				}
			}
			for (int Index = 0; Index < 2; ++Index)
			{
				if (Kit().ResourceRegen[Index] != 0.0)
				{
					GainResource(Index, Kit().ResourceRegen[Index] * Dt);
				}
			}
		}

		void FRuntime::TickKitState(double Dt)
		{
			const double T = Now();
			// Automatic abilities (Adapt's Move 2): the timeline repeats by itself.
			for (int Index = 0; Index < SlotCount; ++Index)
			{
				const FAbilityDef& Def = Ability(static_cast<ESlotId>(Index));
				if (Def.AutoInterval > 0.0)
				{
					AutoTimer[Index] += Dt;
					if (AutoTimer[Index] >= Def.AutoInterval)
					{
						AutoTimer[Index] -= Def.AutoInterval;
						BeginTimeline(Def, static_cast<ESlotId>(Index), 0.0, true, T);
					}
				}
			}
			// Nature's Curse soul link: 1% of the attacker's health per second.
			if (bCurse && CurseLinkCount > 0)
			{
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				for (int Index = 0; Index < CurseLinkCount; ++Index)
				{
					for (const FEnemyView& Enemy : All)
					{
						if (Enemy.Id == CurseLink[Index])
						{
							World->DamageEnemy(Enemy.Id, CurseLinkDrain * Enemy.MaxHP * Dt, DmgTrue | DmgDot | DmgReflect);
						}
					}
				}
			}
			if (TakeoverUntil > 0.0 && T >= TakeoverUntil)
			{
				EndTakeover();
			}
			if (SeedTarget >= 0)
			{
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				for (const FEnemyView& Enemy : All)
				{
					if (Enemy.Id == SeedTarget)
					{
						SeedLastPos = Enemy.Pos;
					}
				}
				if (T >= SeedDetonateAt)
				{
					DetonateSeed();
				}
			}
			if (IsKit("Adapt"))
			{
				const double Decay = std::exp(-Dt / 2.0);
				for (double& Use : AdaptUse)
				{
					Use *= Decay;
				}
				const FVec Pos = World->CasterPos();
				if (bHasLastPos)
				{
					AdaptUse[1] += Len2(Pos - LastCasterPos) / (RunSpeedReference * 1.0) * 1.0;
				}
				LastCasterPos = Pos;
				bHasLastPos = true;
				int Favoured = -1;
				double Best = 0.05;
				for (int Index = 0; Index < 3; ++Index)
				{
					if (AdaptUse[Index] > Best)
					{
						Best = AdaptUse[Index];
						Favoured = Index;
					}
				}
				const double Rate = AdaptRatePerSecond * Dt;
				const double OldHealth = AdaptStats[2];
				if (Favoured >= 0)
				{
					int Least = -1;
					double LeastUse = 1e18;
					for (int Index = 0; Index < 3; ++Index)
					{
						if (Index != Favoured && AdaptUse[Index] < LeastUse)
						{
							LeastUse = AdaptUse[Index];
							Least = Index;
						}
					}
					const double Gain = std::min(Rate, AdaptMax - AdaptStats[Favoured]);
					const double Loss = std::min(Gain, AdaptStats[Least] - AdaptMin);
					AdaptStats[Favoured] += Loss > 0.0 ? Loss : 0.0;   // 1-1: what is lost elsewhere is gained here
					AdaptStats[Least] -= Loss > 0.0 ? Loss : 0.0;
				}
				else
				{
					for (double& Stat : AdaptStats)
					{
						const double Step = std::min(Rate, std::fabs(Stat - 1.0));
						Stat += Stat < 1.0 ? Step : -Step;
					}
				}
				if (std::fabs(AdaptStats[2] - OldHealth) > 1e-6)
				{
					ApplyHealthScale(false);
				}
			}
		}

		// ============================================================================ HUD text
		bool FRuntime::HasState(const std::string& Name) const
		{
			for (const std::string& State : ActiveStateTags())
			{
				if (State == Name)
				{
					return true;
				}
			}
			return false;
		}

		std::vector<std::string> FRuntime::ActiveStateTags() const
		{
			std::vector<std::string> Tags;
			const double T = Now();
			if (bCurse)
			{
				Tags.push_back("Curse");
			}
			if (TakeoverUntil > T)
			{
				Tags.push_back("Takeover");
			}
			if (bRecording)
			{
				Tags.push_back("Recording");
			}
			if (ZeroHourUntil > T)
			{
				Tags.push_back("ZeroHour");
			}
			if (SoilUntil > T)
			{
				Tags.push_back("Puddle");
			}
			if (ShieldAmount > 0.0)
			{
				Tags.push_back("Shielded");
			}
			if (!Infected.empty())
			{
				Tags.push_back("Infecting");
			}
			if (SeedTarget >= 0)
			{
				Tags.push_back("Seeded");
			}
			return Tags;
		}

		std::string FRuntime::AdaptedName(int Index) const
		{
			return (Index >= 0 && Index < static_cast<int>(Adapted.size())) ? Adapted[static_cast<size_t>(Index)].Name : std::string();
		}

		std::string FRuntime::AbilityNote(ESlotId Slot) const
		{
			const FAbilityDef& Def = Ability(Slot);
			if (Def.Id.empty())
			{
				return std::string();
			}
			if (Def.bAutomatic)
			{
				return "automatic";
			}
			const FCastResult Check = CanCast(Slot);
			if (!Check.bOk && Check.Reason != "busy" && Check.Reason.rfind("cooldown", 0) != 0)
			{
				return Check.Reason;
			}
			for (const FStep& Step : Def.Steps)
			{
				if (Step.Kind == EStep::Hook && Step.HookId == EHook::AdaptCastCopied && !Adapted.empty())
				{
					return Adapted[static_cast<size_t>(AdaptSelected)].Name;
				}
			}
			if (Def.BranchMode == EBranchMode::ByAct)
			{
				return std::string("act: ") + ActName();
			}
			if (Def.BranchMode == EBranchMode::ByFront)
			{
				return std::string("front: ") + FrontName();
			}
			if (Def.Cast == ECast::Toggle && bCurse)
			{
				return "ON";
			}
			return std::string();
		}

		std::string FRuntime::KitLine() const
		{
			std::string Line;
			auto Add = [&Line](const std::string& Text)
			{
				if (!Line.empty())
				{
					Line += " | ";
				}
				Line += Text;
			};
			for (int Index = 0; Index < 2; ++Index)
			{
				if (!Kit().ResourceName[Index].empty())
				{
					Add(Kit().ResourceName[Index] + Fmt(" %.0f/%.0f", Res[Index], Kit().ResourceMax[Index]));
				}
			}
			if (ShieldAmount > 0.5)
			{
				Add(Fmt("Shield %.0f", ShieldAmount));
			}
			if (IsKit("Jester"))
			{
				Add(std::string("Act: ") + ActNames[Act % 5] + " -> " + ActNames[ActOrder[(ActCasts + 1) % 5]]);
			}
			if (IsKit("Weather"))
			{
				Add(std::string("Front: ") + FrontNames[Front % 5] + " -> " + FrontNames[(Front + 1) % 5]);
			}
			if (IsKit("Gambler") && Luck > 0.0)
			{
				Add(Fmt("Luck +%.0f%%", Luck * 100.0));
			}
			if (IsKit("Death"))
			{
				Add(Fmt("Kills %.0f", static_cast<double>(KillCount)));
			}
			if (IsKit("Alchemist") || IsKit("Arita"))
			{
				int Mutated = 0;
				for (const auto& Pair : Infected)
				{
					Mutated += Pair.second.bMutated ? 1 : 0;
				}
				Add(Fmt("Infected %.0f", static_cast<double>(Infected.size())) + Fmt(" (mutated %.0f)", static_cast<double>(Mutated)));
			}
			if (SoilUntil > Now())
			{
				Add(Fmt("Puddle %.0fs", SoilUntil - Now()));
			}
			if (bCurse)
			{
				Add("Nature's Curse ON");
			}
			if (TakeoverUntil > Now())
			{
				Add(Fmt("Flower creature %.0fs", TakeoverUntil - Now()));
			}
			if (IsKit("Adapt"))
			{
				std::string Names;
				for (size_t Index = 0; Index < Adapted.size(); ++Index)
				{
					Names += (Index == static_cast<size_t>(AdaptSelected) ? "[" : "") + Adapted[Index].Name + (Index == static_cast<size_t>(AdaptSelected) ? "]" : "") + " ";
				}
				Add(Fmt("Adapted %.0f/8 ", static_cast<double>(Adapted.size())) + Names);
				Add(Fmt("Dmg x%.2f", AdaptStats[0], 0.0) + Fmt(" Spd x%.2f", AdaptStats[1]) + Fmt(" HP x%.2f", AdaptStats[2]));
			}
			if (bRecording)
			{
				double Sum = 0.0;
				for (const FEcho& Echo : EchoLog)
				{
					Sum += Echo.Amount;
				}
				Add(Fmt("Echo recorded %.0f", Sum));
			}
			if (ZeroHourUntil > Now())
			{
				Add(Fmt("Zero Hour %.0fs", ZeroHourUntil - Now()));
			}
			if (SeedTarget >= 0)
			{
				Add(Fmt("Seed detonates %.0fs", std::max(0.0, SeedDetonateAt - Now())));
			}
			return Line;
		}
	}
}
