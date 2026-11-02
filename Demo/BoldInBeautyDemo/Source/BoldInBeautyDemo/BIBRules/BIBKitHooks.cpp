// Boldinbeauty demo — operator kit runtime: named hook mechanics (infection, flowers, Nature's Curse, takeover,
// Adapt, Jester's encore, Timekeeper's echo, Stargazer's seed).
#include "BIBKitRuntime.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Kits
	{
		namespace
		{
			constexpr double Pi = 3.14159265358979323846;
			constexpr double DefaultTouchReach = 320.0;      // PLACEHOLDER: "simply touch a single enemy"
			constexpr double TakeoverSeconds = 40.0;         // SOURCE (Arita): lasts 40 seconds
			constexpr double TakeoverStatScale = 1.75;       // SOURCE (Arita): 75% more health and damage
			constexpr double FlowerLife = 25.0;              // PLACEHOLDER
			constexpr double FlowerTouchRadius = 110.0;      // PLACEHOLDER
			constexpr double ThornDamage = 100.0;            // SOURCE (Arita): thorns do 100 damage when trod on
			constexpr double ThornInterval = 0.5;
			constexpr double PoisonFlowerTotal = 200.0;      // SOURCE: a poisonous flower does 200 damage over 10 seconds
			constexpr double PoisonFlowerSeconds = 10.0;
			constexpr double PheromoneStun = 0.5;            // SOURCE: stuns enemies for 0.5 seconds
			constexpr double SpringBaseRadius = 500.0;       // PLACEHOLDER: grows with every second held (SOURCE: "for every second held increase aoe")
			constexpr double SpringRadiusPerSecond = 350.0;
			constexpr double SpringMaxRadius = 2600.0;
			constexpr double SoilRadius = 220.0;             // SOURCE: "a very small area"
			constexpr double SoilRetouchSeconds = 5.0;       // PLACEHOLDER
			constexpr double AdaptCopyDamageScale = 10.0;    // PLACEHOLDER: demo enemies hit for 40-300; set 1.0 for strictly identical damage
			constexpr double AdaptMaxAbilities = 8.0;        // SOURCE: can store 8 moves at once
			constexpr double AdaptPassiveHeal = 0.25;        // SOURCE: gain 25% hp when nothing can be copied
			constexpr size_t AdaptMaxCopiedPassives = 4;
		}

		// ============================================================================ hook dispatch
		void FRuntime::RunHook(const FStep& Step, FTimeline*, const FContext& Ctx)
		{
			const FVec Origin = World->CasterPos();
			const FVec Facing = Norm2(World->CasterAim());
			switch (Step.HookId)
			{
			case EHook::InfectTouched:
			case EHook::MutateNearest:
			{
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				FEnemyView Target;
				const double Reach = Step.Range > 0.0 ? Step.Range : DefaultTouchReach;
				bool bFound = false;
				double Best = 1e18;
				for (const FEnemyView& Enemy : All)
				{
					const FVec ToEnemy = Enemy.Pos - Origin;
					const double Distance = Len2(ToEnemy);
					if (Distance <= Reach + Enemy.Radius && (Dot2(Norm2(ToEnemy), Facing) >= 0.2 || Distance < 140.0) && Distance < Best)
					{
						Best = Distance;
						Target = Enemy;
						bFound = true;
					}
				}
				if (!bFound)
				{
					World->Message(Step.HookId == EHook::InfectTouched ? "Infection: nobody in reach." : "Mutation: nobody in reach.");
					break;
				}
				if (Step.HookId == EHook::InfectTouched)
				{
					InfectEnemy(Target.Id);
					World->Message("Infected an enemy.");
				}
				else if (Infected.count(Target.Id) != 0)
				{
					MutateEnemy(Target.Id);
					World->Message("The infection mutated.");
				}
				else
				{
					// SOURCE (Alchemist): a non-infected enemy hit with mutation just explodes for 5000 (WORKING: neighbours are hurt too).
					std::vector<FEnemyView> Blast;
					for (const FEnemyView& Enemy : All)
					{
						if (Enemy.Id == Target.Id || Dist2(Target.Pos, Enemy.Pos) <= Infection::MutateExplosionRadius + Enemy.Radius)
						{
							Blast.push_back(Enemy);
						}
					}
					for (const FEnemyView& Enemy : Blast)
					{
						const double Amount = Infection::MutateExplosionDamage * World->CasterDamageMult() * DamageDealtMult();
						const double Dealt = World->DamageEnemyHit(Enemy.Id, Amount, DmgNone, CurrentHit);
						RecordDamage(Enemy.Id, Dealt, false);
						if (!World->EnemyAlive(Enemy.Id))
						{
							HandleKill(Enemy.Id);
						}
					}
					World->Cue(Step.CueId, Target.Pos, Infection::MutateExplosionRadius, 0.6);
				}
				break;
			}

			case EHook::RingLeader:
			{
				int Count = 0;
				for (const auto& Pair : Infected)
				{
					if (World->EnemyAlive(Pair.first) && World->ApplyStatus(Pair.first, EStatus::Illusion, Infection::IllusionSeconds, 0.0, 1))
					{
						++Count;
					}
				}
				World->Message("Ring leader: " + std::to_string(Count) + " infected enemies see illusions.");
				break;
			}

			case EHook::SoilPuddle:
				SoilUntil = Now() + (Step.Duration > 0.0 ? Step.Duration : 20.0);
				SoilTouched.clear();
				break;

			case EHook::SpringTime:
				SpawnFlowers(Ctx.Held);
				break;

			case EHook::NaturesCurse:
				ToggleCurse();
				break;

			case EHook::FullTakeover:
				StartTakeover(Step.Duration > 0.0 ? Step.Duration : TakeoverSeconds);
				break;

			case EHook::AdaptCastCopied:
				AdaptCast();
				break;

			case EHook::AdaptSwitch:
				AdaptSwitchNext();
				break;

			case EHook::AdaptPassiveCopy:
				AdaptCopyPassive();
				break;

			case EHook::JesterEncore:
				StartEncore();
				break;

			case EHook::TimeRecord:
				bRecording = true;
				EchoLog.clear();
				if (Step.Duration > 0.0)
				{
					ZeroHourUntil = Now() + Step.Duration;
				}
				break;

			case EHook::TimeRelease:
				ReleaseEcho(Step.Param > 0.0 ? Step.Param : 1.0, Step.Param2);
				ZeroHourUntil = 0.0;
				break;

			case EHook::StarSeed:
			{
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				FEnemyView Target;
				if (!FindNearest(All, Origin, Step.Range > 0.0 ? Step.Range : 1800.0, Step.Flags, Target))
				{
					World->Message("Supernova seed: no target.");
					break;
				}
				SeedTarget = Target.Id;
				SeedLastPos = Target.Pos;
				SeedDetonateAt = Now() + (Step.Duration > 0.0 ? Step.Duration : 4.0);
				SeedStoredDamage = 0.0;
				SeedStep = Step;
				SeedStep.Kind = EStep::Burst;
				SeedStep.HookId = EHook::None;
				SeedStep.Flags &= ~static_cast<unsigned>(FlagNearestEnemy);
				World->ApplyStatus(Target.Id, EStatus::Mark, Step.Duration > 0.0 ? Step.Duration : 4.0, 0.0, 1);
				break;
			}

			default:
				break;
			}
		}

		// ============================================================================ Alchemist / Arita: infection
		void FRuntime::InfectEnemy(int Id)
		{
			if (Infected.count(Id) != 0 || World->StatusStacks(Id, EStatus::Immune) > 0 || !World->EnemyAlive(Id))
			{
				return;
			}
			Infected[Id] = FInfect();
			if (World->ApplyStatus(Id, EStatus::Infection, 100000.0, 0.0, 1))
			{
				FContext Ctx;
				Ctx.TriggerEnemy = Id;
				FirePassives(ETrigger::OnStatusApplied, Ctx, static_cast<int>(EStatus::Infection));
			}
		}

		void FRuntime::MutateEnemy(int Id)
		{
			const auto Found = Infected.find(Id);
			if (Found == Infected.end() || Found->second.bMutated)
			{
				return;
			}
			Found->second.bMutated = true;
			World->ApplyStatus(Id, EStatus::Mutated, 100000.0, 0.0, 1);
		}

		bool FRuntime::IsMutated(int EnemyId) const
		{
			const auto Found = Infected.find(EnemyId);
			return Found != Infected.end() && Found->second.bMutated;
		}

		void FRuntime::TickBackground(double Dt)
		{
			// The operator is benched (2026-10-09): what she left on the enemies keeps going - infection spreads, the goo grows and
			// hosts explode, so Alchemist can infect, mutate and swap out. Her soil puddle leaks around her and waits for her.
			TickInfection(Dt, false);
		}

		void FRuntime::TickInfection(double Dt, bool bOnField)
		{
			if (Infected.empty() && (!bOnField || SoilUntil <= Now()))
			{
				return;
			}
			std::vector<FEnemyView> All;
			CollectEnemies(All);
			const double T = Now();
			if (!Infected.empty())
			{
				InfectionClock += Dt;
			}
			const bool bWindow = std::fmod(InfectionClock, Infection::SpreadPeriod) < Infection::SpreadWindow;

			// Soil puddle (Alchemist ultimate): small area around her; each enemy that touches it rolls an effect.
			if (bOnField && SoilUntil > T)
			{
				const FVec Centre = World->CasterPos();
				for (const FEnemyView& Enemy : All)
				{
					if (Dist2(Centre, Enemy.Pos) > SoilRadius + Enemy.Radius)
					{
						continue;
					}
					const auto Touched = SoilTouched.find(Enemy.Id);
					if (Touched != SoilTouched.end() && T < Touched->second)
					{
						continue;
					}
					SoilTouched[Enemy.Id] = T + SoilRetouchSeconds;
					const bool bInfected = Infected.count(Enemy.Id) != 0;
					const bool bImmune = World->StatusStacks(Enemy.Id, EStatus::Immune) > 0;
					if (bImmune)
					{
						World->ClearStatus(Enemy.Id, EStatus::Immune);                         // effect 4: remove immunity
					}
					else if (bInfected)
					{
						World->ApplyStatus(Enemy.Id, EStatus::Vulnerable, 8.0, 1.0, 1);        // effect 3: double the damage taken (WORKING)
					}
					else if (SoilToggle++ % 2 == 0)
					{
						InfectEnemy(Enemy.Id);                                                 // effect 1: gain infection
					}
					else
					{
						InfectEnemy(Enemy.Id);                                                 // effect 2: gain mutation
						MutateEnemy(Enemy.Id);
					}
				}
			}

			std::vector<int> Exploding;
			for (auto& Pair : Infected)
			{
				const int Id = Pair.first;
				FInfect& State = Pair.second;
				const FEnemyView* Self = nullptr;
				for (const FEnemyView& Enemy : All)
				{
					if (Enemy.Id == Id)
					{
						Self = &Enemy;
						break;
					}
				}
				if (!Self)
				{
					Exploding.push_back(-Id - 1);   // gone: forget it
					continue;
				}
				State.Age += Dt;
				// During the spreading window an infected enemy that touches another passes it on.
				if (bWindow)
				{
					for (const FEnemyView& Other : All)
					{
						if (Other.Id == Id || Dist2(Self->Pos, Other.Pos) > Self->Radius + Other.Radius + Infection::ContactPad)
						{
							continue;
						}
						const auto OtherState = Infected.find(Other.Id);
						if (OtherState == Infected.end())
						{
							if (!State.bMutated)
							{
								Exploding.push_back(1000000 + Other.Id);   // infect after the loop (the map must not change here)
							}
						}
						else if (State.bMutated && !OtherState->second.bMutated && World->StatusStacks(Other.Id, EStatus::Immune) == 0)
						{
							Exploding.push_back(2000000 + Other.Id);       // mutation spreads to infected enemies
						}
					}
				}
				if (State.Age >= Kit().InfectionTuning.GooStartsAt)
				{
					State.GooTimer += Dt;
					while (State.GooTimer >= Kit().InfectionTuning.GooInterval)
					{
						State.GooTimer -= Kit().InfectionTuning.GooInterval;
						const double Bonus = State.bMutated ? Infection::MutationDamageBonus : 1.0;
						const double Dealt = World->DamageEnemy(Id, Kit().InfectionTuning.GooDamage * Bonus * World->CasterDamageMult(), DmgDot);
						RecordDamage(Id, Dealt, false);
					}
				}
				if (State.Age >= Kit().InfectionTuning.ExplodeAt)
				{
					Exploding.push_back(Id);
				}
			}
			for (const int Code : Exploding)
			{
				if (Code < 0)
				{
					Infected.erase(-Code - 1);
				}
				else if (Code >= 2000000)
				{
					MutateEnemy(Code - 2000000);
				}
				else if (Code >= 1000000)
				{
					InfectEnemy(Code - 1000000);
				}
				else
				{
					const auto Found = Infected.find(Code);
					if (Found == Infected.end())
					{
						continue;
					}
					const double Bonus = Found->second.bMutated ? Infection::MutationDamageBonus : 1.0;
					FVec Centre;
					for (const FEnemyView& Enemy : All)
					{
						if (Enemy.Id == Code)
						{
							Centre = Enemy.Pos;
						}
					}
					for (const FEnemyView& Enemy : All)
					{
						if (Dist2(Centre, Enemy.Pos) <= Infection::ExplodeRadius + Enemy.Radius)
						{
							const double Dealt = World->DamageEnemy(Enemy.Id, Kit().InfectionTuning.ExplodeDamage * Bonus * World->CasterDamageMult(), DmgNone);
							RecordDamage(Enemy.Id, Dealt, false);
							if (!World->EnemyAlive(Enemy.Id))
							{
								HandleKill(Enemy.Id);
							}
						}
					}
					World->Cue(0, Centre, Infection::ExplodeRadius, 0.8);
					// SOURCE: the host loses the infection and gains immunity.
					Infected.erase(Code);
					World->ClearStatus(Code, EStatus::Infection);
					World->ClearStatus(Code, EStatus::Mutated);
					World->ApplyStatus(Code, EStatus::Immune, 100000.0, 0.0, 1);
				}
			}
		}

		// ============================================================================ Arita: flowers, curse, takeover
		void FRuntime::SpawnFlowers(double Held)
		{
			const double Radius = std::min(SpringMaxRadius, SpringBaseRadius + SpringRadiusPerSecond * Held);
			const int Count = std::min(60, 8 + 6 * static_cast<int>(Held));
			const FVec Centre = World->CasterPos();
			for (int Index = 0; Index < Count; ++Index)
			{
				FFlower Flower;
				const double Distance = Radius * std::sqrt(Rand());
				const double Angle = 2.0 * Pi * Rand();
				Flower.Pos = Centre + FVec{std::cos(Angle) * Distance, std::sin(Angle) * Distance, 0.0};
				const double Roll = Rand();
				Flower.Type = Roll < 0.4 ? 0 : (Roll < 0.75 ? 1 : 2);
				Flower.EndsAt = Now() + FlowerLife;
				Flower.NextTrigger = Now();
				Flowers.push_back(Flower);
			}
			if (Flowers.size() > 180)
			{
				Flowers.erase(Flowers.begin(), Flowers.begin() + static_cast<std::ptrdiff_t>(Flowers.size() - 180));
			}
			World->Cue(0, Centre, Radius, 1.0);
			World->Message("Spring time: " + std::to_string(Count) + " flowers over " + std::to_string(static_cast<int>(Radius)) + " cm.");
		}

		void FRuntime::TickFlowers(double)
		{
			if (Flowers.empty())
			{
				return;
			}
			const double T = Now();
			std::vector<FEnemyView> All;
			CollectEnemies(All);
			for (FFlower& Flower : Flowers)
			{
				if (T < Flower.NextTrigger)
				{
					continue;
				}
				for (const FEnemyView& Enemy : All)
				{
					if (Dist2(Flower.Pos, Enemy.Pos) > FlowerTouchRadius + Enemy.Radius || !World->EnemyAlive(Enemy.Id))
					{
						continue;
					}
					if (Flower.Type == 0)
					{
						const double Dealt = World->DamageEnemy(Enemy.Id, ThornDamage * World->CasterDamageMult() * DamageDealtMult(), DmgNone);
						RecordDamage(Enemy.Id, Dealt, false);
						Flower.NextTrigger = T + ThornInterval;
					}
					else if (Flower.Type == 1)
					{
						World->ApplyStatus(Enemy.Id, EStatus::Poison, PoisonFlowerSeconds, PoisonFlowerTotal / PoisonFlowerSeconds * World->CasterDamageMult(), 1);
						Flower.NextTrigger = T + 2.5;
					}
					else
					{
						World->ApplyStatus(Enemy.Id, EStatus::Stun, PheromoneStun, 0.0, 1);
						Flower.NextTrigger = T + 3.0;
					}
					if (!World->EnemyAlive(Enemy.Id))
					{
						HandleKill(Enemy.Id);
					}
					break;
				}
			}
			Flowers.erase(std::remove_if(Flowers.begin(), Flowers.end(), [T](const FFlower& Flower) { return Flower.EndsAt <= T; }), Flowers.end());
		}

		void FRuntime::ToggleCurse()
		{
			bCurse = !bCurse;
			if (!bCurse)
			{
				CurseLinkCount = 0;
			}
			World->Message(bCurse ? "Nature's Curse: ON (damage splits 50/50 with the attacker)." : "Nature's Curse: OFF.");
		}

		void FRuntime::ApplyHealthScale(bool bFill)
		{
			HealthScaleValue = TakeoverHealthScale * AdaptStats[2];
			World->CasterHealthScale(HealthScaleValue, bFill);
		}

		void FRuntime::StartTakeover(double Seconds)
		{
			TakeoverUntil = Now() + Seconds;
			TakeoverHealthScale = TakeoverStatScale;
			AddBuff(EBuff::DamageDealt, TakeoverStatScale - 1.0, Seconds);   // SOURCE: 75% more damage on all moves
			AddBuff(EBuff::AttackSpeed, 0.5, Seconds);                       // SOURCE: custom M1s are 50% faster
			ApplyHealthScale(true);                                          // SOURCE: 75% more health (WORKING: she does not die)
			World->Message("Full takeover: a giant flower creature for " + std::to_string(static_cast<int>(Seconds)) + " s.");
		}

		void FRuntime::EndTakeover()
		{
			TakeoverUntil = 0.0;
			TakeoverHealthScale = 1.0;
			ApplyHealthScale(false);
			World->Message("The flower creature withers: back to base form.");
		}

		// ============================================================================ Adapt
		void FRuntime::NotifyEnemyAbilityHit(int EnemyAbilityId, double EnemyDamage)
		{
			if (!IsKit("Adapt"))
			{
				return;
			}
			const FEnemyAbility* Known = nullptr;
			for (const FEnemyAbility& Candidate : EnemyAbilities())
			{
				if (Candidate.Id == EnemyAbilityId)
				{
					Known = &Candidate;
				}
			}
			if (!Known)
			{
				return;
			}
			const double Scaled = EnemyDamage * AdaptCopyDamageScale;
			for (FCopied& Existing : Adapted)
			{
				if (Existing.EnemyAbilityId == EnemyAbilityId)
				{
					Existing.Damage = std::max(Existing.Damage, Scaled);
					return;
				}
			}
			if (static_cast<double>(Adapted.size()) >= AdaptMaxAbilities)
			{
				return;
			}
			FCopied Copy;
			Copy.EnemyAbilityId = EnemyAbilityId;
			Copy.Damage = Scaled;
			Copy.Cooldown = Known->Cooldown;
			Copy.Radius = Known->Radius;
			Copy.Name = Known->Name;
			Adapted.push_back(Copy);
			World->Cue(0, World->CasterPos(), 200.0, 0.4);   // "does a slight glitch of the screen"
			World->Message(std::string("Adapted: ") + Known->Name);
		}

		void FRuntime::AdaptCast()
		{
			if (Adapted.empty())
			{
				return;
			}
			const FCopied Copy = Adapted[static_cast<size_t>(AdaptSelected)];
			FStep Step;
			if (Copy.Radius > 0.0)
			{
				Step.Kind = EStep::Burst;
				Step.Radius = Copy.Radius;
			}
			else
			{
				Step.Kind = EStep::Melee;
				Step.Range = 280.0;
				Step.ArcDot = 0.2;
			}
			Step.Damage = Copy.Damage;
			Step.CueId = 0;
			FContext Ctx;
			ExecuteStep(Step, nullptr, Ctx);
			World->Message("Adapt casts " + Copy.Name + ".");
		}

		void FRuntime::AdaptSwitchNext()
		{
			if (Adapted.size() < 2)
			{
				return;
			}
			AdaptSelected = (AdaptSelected + 1) % static_cast<int>(Adapted.size());
			World->Message("Adapt: selected " + Adapted[static_cast<size_t>(AdaptSelected)].Name + " (" + std::to_string(AdaptSelected + 1) + ").");
		}

		void FRuntime::AdaptCopyPassive()
		{
			std::vector<const FPassiveDef*> Candidates;
			for (const FPassiveDef* Passive : TeammatePassives)
			{
				bool bHave = false;
				for (const FPassiveState& State : Passives)
				{
					bHave = bHave || (State.Def && State.Def->Id == Passive->Id);
				}
				if (Passive && !bHave)
				{
					Candidates.push_back(Passive);
				}
			}
			if (Candidates.empty() || CopiedPassives.size() >= AdaptMaxCopiedPassives)
			{
				World->CasterHeal(AdaptPassiveHeal * World->CasterMaxHP());   // SOURCE: gain 25% hp instead
				World->Message("Adapt: nothing to copy, healed 25%.");
				return;
			}
			const size_t Pick = std::min(Candidates.size() - 1, static_cast<size_t>(Rand() * static_cast<double>(Candidates.size())));
			CopiedPassives.push_back(*Candidates[Pick]);
			FPassiveState State;
			State.Def = &CopiedPassives.back();
			Passives.push_back(State);
			World->Message("Adapt copied the passive " + Candidates[Pick]->Name + ".");
		}

		// ============================================================================ Jester, Timekeeper, Stargazer
		void FRuntime::StartEncore()
		{
			const std::vector<const FAbilityDef*> History = RecentCasts;   // oldest first
			double Offset = 0.3;
			for (auto It = History.rbegin(); It != History.rend(); ++It)
			{
				const FAbilityDef* Def = *It;
				if (Def->Slot == ESlotId::Special || Def->Slot == ESlotId::Basic)
				{
					continue;
				}
				BeginTimeline(*Def, Def->Slot, 0.0, true, Now() + Offset);
				Offset += 0.5;
			}
			World->Message("Encore!");
		}

		void FRuntime::ReleaseEcho(double Multiplier, double MinimumPerEnemy)
		{
			bRecording = false;
			std::map<int, double> Totals;
			for (const FEcho& Echo : EchoLog)
			{
				Totals[Echo.Id] += Echo.Amount;
			}
			EchoLog.clear();
			std::vector<FEnemyView> All;
			CollectEnemies(All);
			double Sum = 0.0;
			for (const FEnemyView& Enemy : All)
			{
				const auto Found = Totals.find(Enemy.Id);
				const double Recorded = Found == Totals.end() ? 0.0 : Found->second;
				const double Amount = std::max(MinimumPerEnemy * (Recorded > 0.0 || MinimumPerEnemy > 0.0 ? 1.0 : 0.0), Recorded * Multiplier);
				if (Amount <= 0.0 || !World->EnemyAlive(Enemy.Id))
				{
					continue;
				}
				const double Dealt = World->DamageEnemyHit(Enemy.Id, Amount, DmgTrue, CurrentHit);
				Sum += Dealt;
				RecordDamage(Enemy.Id, Dealt, false);
				if (!World->EnemyAlive(Enemy.Id))
				{
					HandleKill(Enemy.Id);
				}
				World->Cue(0, Enemy.Pos, Enemy.Radius * 2.0, 0.4);
			}
			World->Message("Time echo released: " + std::to_string(static_cast<int>(Sum)) + " damage.");
		}

		void FRuntime::DetonateSeed()
		{
			const int Id = SeedTarget;
			SeedTarget = -1;
			std::vector<FEnemyView> All;
			CollectEnemies(All);
			FVec Centre = SeedLastPos;
			for (const FEnemyView& Enemy : All)
			{
				if (Enemy.Id == Id)
				{
					Centre = Enemy.Pos;
				}
			}
			const double Radius = SeedStep.Radius > 0.0 ? SeedStep.Radius : 700.0;
			const double Base = StepDamage(SeedStep, false);
			const double Scale = Base > 0.0 ? (Base + SeedStoredDamage) / Base : 1.0;
			for (const FEnemyView& Enemy : All)
			{
				if (Dist2(Centre, Enemy.Pos) <= Radius + Enemy.Radius)
				{
					HitEnemy(SeedStep, Enemy, Centre, false, Scale);
				}
			}
			World->Cue(SeedStep.CueId, Centre, Radius, 0.8);
			World->Message("Supernova!");
			SeedStoredDamage = 0.0;
		}
	}
}
