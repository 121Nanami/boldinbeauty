// Boldinbeauty demo — operator kit runtime: timelines, step execution, targeting, zones and projectiles.
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
			constexpr double ProjectileDefaultSpeed = 1800.0;
			constexpr double ProjectileDefaultRange = 1500.0;
			constexpr double ProjectileDefaultRadius = 60.0;
			constexpr double MeleeClosePad = 120.0;
			constexpr double DefaultReach = 1500.0;
			constexpr size_t MaxZones = 24;
			constexpr double TakeoverBasicDamageScale = 5.0;   // SOURCE (Arita): custom M1s do 400% more damage ...
			constexpr double TakeoverBasicSizeScale = 3.5;     // ... and have a 250% larger hitbox
			constexpr double LuckPityStep = 0.05;              // PROPOSED (Gambler passive "Lady Luck")

			// Marks the code that deals ultimate damage so it does not recharge the ultimate bar.
			struct FUltGuard
			{
				bool& Flag;
				bool Previous;
				FUltGuard(bool& InFlag, bool bSet) : Flag(InFlag), Previous(InFlag)
				{
					if (bSet)
					{
						Flag = true;
					}
				}
				~FUltGuard() { Flag = Previous; }
			};

			bool KindRepeats(EStep Kind)
			{
				return Kind != EStep::Zone && Kind != EStep::Buff && Kind != EStep::Shield;
			}

			bool IsMarked(const IWorld& World, int Id)
			{
				return World.StatusStacks(Id, EStatus::Doom) > 0 || World.StatusStacks(Id, EStatus::Mark) > 0;
			}
		}

		bool StepDealsDamage(const FStep& Step)
		{
			if (Step.Kind == EStep::Execute || Step.Kind == EStep::Detonate)
			{
				return true;
			}
			if (Step.Kind == EStep::Hook)
			{
				return Step.HookId == EHook::MutateNearest || Step.HookId == EHook::AdaptCastCopied || Step.HookId == EHook::TimeRelease
					|| Step.HookId == EHook::JesterEncore;
			}
			return Step.Damage > 0.0 || Step.AtkMult > 0.0;
		}

		// ============================================================================ enemy queries
		void FRuntime::CollectEnemies(std::vector<FEnemyView>& Out) const
		{
			Out.clear();
			World->GetEnemies(Out);
		}

		bool FRuntime::PassesFilter(const FEnemyView& Enemy, unsigned Flags) const
		{
			if ((Flags & FlagIgnoreBoss) && Enemy.bBoss)
			{
				return false;
			}
			if ((Flags & FlagMarkedOnly) && !IsMarked(*World, Enemy.Id))
			{
				return false;
			}
			return true;
		}

		bool FRuntime::FindNearest(const std::vector<FEnemyView>& All, const FVec& From, double MaxDist, unsigned Flags, FEnemyView& Out) const
		{
			bool bFound = false;
			double Best = 1e18;
			for (const FEnemyView& Enemy : All)
			{
				if (!PassesFilter(Enemy, Flags))
				{
					continue;
				}
				const double Distance = Dist2(From, Enemy.Pos);
				if (Distance <= MaxDist + Enemy.Radius && Distance < Best)
				{
					Best = Distance;
					Out = Enemy;
					bFound = true;
				}
			}
			return bFound;
		}

		void FRuntime::ResolveTargets(const FStep& Step, const FContext& Ctx, std::vector<FEnemyView>& Targets, FVec& Centre, FVec& Facing)
		{
			Targets.clear();
			const FVec Origin = World->CasterPos();
			Facing = Norm2(World->CasterAim());
			Centre = Origin + Facing * Step.Offset;
			if (Step.Flags & FlagFromAimPoint)
			{
				Centre = Origin + Facing * Step.Range;
			}
			std::vector<FEnemyView> All;
			CollectEnemies(All);
			const double Reach = Step.Range > 0.0 ? Step.Range : (Step.Radius > 0.0 ? Step.Radius : DefaultReach);

			auto SortByDistance = [&Origin](std::vector<FEnemyView>& List)
			{
				std::stable_sort(List.begin(), List.end(), [&Origin](const FEnemyView& A, const FEnemyView& B)
				{
					return Dist2(Origin, A.Pos) < Dist2(Origin, B.Pos);
				});
			};

			if (Step.Flags & (FlagHitTarget | FlagStoredTarget))
			{
				const int WantedId = (Step.Flags & FlagHitTarget) ? Ctx.TriggerEnemy : StoredTarget;
				for (const FEnemyView& Enemy : All)
				{
					if (Enemy.Id == WantedId && PassesFilter(Enemy, Step.Flags))
					{
						Targets.push_back(Enemy);
						Centre = Enemy.Pos;
					}
				}
				return;
			}
			if (Step.Flags & FlagAllEnemies)
			{
				for (const FEnemyView& Enemy : All)
				{
					if (PassesFilter(Enemy, Step.Flags))
					{
						Targets.push_back(Enemy);
					}
				}
				SortByDistance(Targets);
				return;
			}
			if (Step.Kind == EStep::Chain)
			{
				std::vector<FEnemyView> Candidates;
				const double ChainReach = Step.Radius > 0.0 ? Step.Radius : Reach;
				for (const FEnemyView& Enemy : All)
				{
					if (PassesFilter(Enemy, Step.Flags) && Dist2(Origin, Enemy.Pos) <= ChainReach + Enemy.Radius)
					{
						Candidates.push_back(Enemy);
					}
				}
				SortByDistance(Candidates);
				const size_t Wanted = static_cast<size_t>(std::max(1, Step.Hits));
				if (Step.Flags & FlagRandomEnemy)
				{
					// Deterministic through IWorld::Random01; the same enemy may be struck more than once.
					for (size_t Strike = 0; Strike < Wanted && !Candidates.empty(); ++Strike)
					{
						const size_t Pick = std::min(Candidates.size() - 1, static_cast<size_t>(Rand() * static_cast<double>(Candidates.size())));
						Targets.push_back(Candidates[Pick]);
					}
				}
				else
				{
					for (size_t Index = 0; Index < Candidates.size() && Index < Wanted; ++Index)
					{
						Targets.push_back(Candidates[Index]);
					}
				}
				if (!Targets.empty())
				{
					Centre = Targets.front().Pos;
				}
				return;
			}
			if (Step.Flags & (FlagAtTarget | FlagNearestEnemy | FlagRandomEnemy))
			{
				FEnemyView Chosen;
				bool bFound = false;
				if (Step.Flags & FlagRandomEnemy)
				{
					std::vector<FEnemyView> Candidates;
					for (const FEnemyView& Enemy : All)
					{
						if (PassesFilter(Enemy, Step.Flags) && Dist2(Origin, Enemy.Pos) <= Reach + Enemy.Radius)
						{
							Candidates.push_back(Enemy);
						}
					}
					if (!Candidates.empty())
					{
						SortByDistance(Candidates);
						Chosen = Candidates[std::min(Candidates.size() - 1, static_cast<size_t>(Rand() * static_cast<double>(Candidates.size())))];
						bFound = true;
					}
				}
				else
				{
					bFound = FindNearest(All, Origin, Reach, Step.Flags, Chosen);
				}
				if (!bFound)
				{
					return;
				}
				Centre = Chosen.Pos;
				if (Step.Radius <= 0.0 && Step.Kind != EStep::Beam)
				{
					Targets.push_back(Chosen);
					if (Step.Flags & FlagStoreTarget)
					{
						StoredTarget = Chosen.Id;
					}
					return;
				}
				if (Step.Flags & FlagStoreTarget)
				{
					StoredTarget = Chosen.Id;
				}
			}

			for (const FEnemyView& Enemy : All)
			{
				if (!PassesFilter(Enemy, Step.Flags))
				{
					continue;
				}
				bool bInside = false;
				const FVec ToEnemy = Enemy.Pos - Origin;
				const double Distance = Len2(ToEnemy);
				switch (Step.Kind)
				{
				case EStep::Melee:
					bInside = Distance <= Step.Range + Enemy.Radius
						&& (Dot2(Norm2(ToEnemy), Facing) >= Step.ArcDot || Distance < MeleeClosePad);
					break;
				case EStep::Beam:
				{
					const double Along = Dot2(ToEnemy, Facing);
					const FVec Closest = Origin + Facing * std::max(0.0, std::min(Along, Step.Range));
					bInside = Along >= -Enemy.Radius && Along <= Step.Range + Enemy.Radius
						&& Dist2(Closest, Enemy.Pos) <= Step.Radius + Enemy.Radius;
					break;
				}
				case EStep::Execute:
					if (Step.ArcDot > -1.0 && Step.Range > 0.0)
					{
						bInside = Distance <= Step.Range + Enemy.Radius
							&& (Dot2(Norm2(ToEnemy), Facing) >= Step.ArcDot || Distance < MeleeClosePad);
					}
					else
					{
						bInside = Dist2(Centre, Enemy.Pos) <= std::max(Step.Radius, 1.0) + Enemy.Radius;
					}
					break;
				default:
					bInside = Dist2(Centre, Enemy.Pos) <= Step.Radius + Enemy.Radius;
					break;
				}
				if (bInside)
				{
					Targets.push_back(Enemy);
				}
			}
			SortByDistance(Targets);
		}

		// ============================================================================ damage
		double FRuntime::StepDamage(const FStep& Step, bool) const
		{
			double Base = Step.Damage + Step.AtkMult * World->CasterAttack();
			if ((Step.Flags & FlagUseStored) && Step.Param2 != 0.0)
			{
				Base += Step.Param2 * StoredValue;
			}
			if ((Step.Flags & FlagHitAmount) && Step.Param2 != 0.0)
			{
				Base += Step.Param2 * HitAmount;
			}
			if ((Step.Flags & FlagUseResource) && Step.Param2 != 0.0 && Step.Stat >= 0 && Step.Stat < 2)
			{
				Base += Step.Param2 * Res[Step.Stat];
			}
			return Base * World->CasterDamageMult() * DamageDealtMult();
		}

		void FRuntime::RecordDamage(int EnemyId, double Dealt, bool bBasic)
		{
			if (Dealt <= 0.0)
			{
				return;
			}
			LastDamage += Dealt;
			TotalDealt += Dealt;
			if (!bSuppressUlt)
			{
				World->AddUltimate(Dealt);
			}
			AdaptUse[0] += Dealt;
			const double Steal = Mod(EBuff::LifeSteal);
			if (Steal > 0.0)
			{
				World->CasterHeal(Dealt * Steal);
			}
			if (bRecording)
			{
				FEcho Echo;
				Echo.Id = EnemyId;
				Echo.Amount = Dealt;
				EchoLog.push_back(Echo);
			}
			if (SeedTarget == EnemyId)
			{
				SeedStoredDamage += Dealt * 0.25;
			}
			FContext Ctx;
			Ctx.TriggerEnemy = EnemyId;
			Ctx.Amount = Dealt;
			FirePassives(ETrigger::OnHit, Ctx);
			if (bBasic)
			{
				FirePassives(ETrigger::OnBasicHit, Ctx);
			}
		}

		void FRuntime::HandleKill(int EnemyId)
		{
			if (!CountedKills.insert(EnemyId).second)
			{
				return;
			}
			++KillCount;
			Infected.erase(EnemyId);
			FContext Ctx;
			Ctx.TriggerEnemy = EnemyId;
			FirePassives(ETrigger::OnKill, Ctx);
		}

		void FRuntime::NotifyEnemyKilled(int EnemyId)
		{
			HandleKill(EnemyId);
		}

		void FRuntime::NotifyDamageDealt(int EnemyId, double Amount, bool bBasic)
		{
			RecordDamage(EnemyId, Amount, bBasic);
		}

		void FRuntime::HitEnemy(const FStep& Step, const FEnemyView& Enemy, const FVec& Centre, bool bBasic, double DamageScale)
		{
			double Amount = StepDamage(Step, bBasic) * DamageScale;
			if (Step.WetBonus > 0.0 && World->StatusStacks(Enemy.Id, EStatus::Wet) > 0)
			{
				Amount *= 1.0 + Step.WetBonus;
			}
			if (Step.BonusVs > 0.0 && Step.BonusStatus != EStatus::None && World->StatusStacks(Enemy.Id, Step.BonusStatus) > 0)
			{
				Amount *= 1.0 + Step.BonusVs;   // set-up -> pay-off: extra damage against an enemy that carries the status
			}
			if (Amount > 0.0)
			{
				unsigned Flags = bBasic ? DmgBasic : DmgNone;
				if (Step.Flags & FlagTrueDamage)
				{
					Flags |= DmgTrue;
				}
				const double Dealt = World->DamageEnemyHit(Enemy.Id, Amount, Flags, CurrentHit);
				RecordDamage(Enemy.Id, Dealt, bBasic);
			}
			if (Step.Status != EStatus::None)
			{
				double Power = Step.StatusPower;
				if (StatusRule(Step.Status).bDamageOverTime)
				{
					Power *= World->CasterDamageMult() * DamageDealtMult();
				}
				if (World->ApplyStatus(Enemy.Id, Step.Status, Step.StatusSeconds, Power, Step.StatusStacks))
				{
					FContext Ctx;
					Ctx.TriggerEnemy = Enemy.Id;
					FirePassives(ETrigger::OnStatusApplied, Ctx, static_cast<int>(Step.Status));
				}
			}
			if (Step.Knockback != 0.0)
			{
				World->Impulse(Enemy.Id, Norm2(Enemy.Pos - Centre) * Step.Knockback);   // negative = pull toward Centre
			}
			if (!World->EnemyAlive(Enemy.Id))
			{
				HandleKill(Enemy.Id);
			}
		}

		// ============================================================================ timelines
		int FRuntime::ChooseBranch(const FAbilityDef& Def)
		{
			const int Count = static_cast<int>(Def.Branches.size());
			if (Count == 0)
			{
				return -1;
			}
			switch (Def.BranchMode)
			{
			case EBranchMode::ByAct:
				return std::min(Count - 1, Act);
			case EBranchMode::ByFront:
				return Front % Count;
			case EBranchMode::Weighted:
			{
				double Total = 0.0;
				for (const FBranch& Branch : Def.Branches)
				{
					Total += Branch.Weight * (Branch.bGood ? 1.0 + Luck : 1.0);
				}
				double Roll = Rand() * Total;
				int Picked = Count - 1;
				for (int Index = 0; Index < Count; ++Index)
				{
					const FBranch& Branch = Def.Branches[static_cast<size_t>(Index)];
					Roll -= Branch.Weight * (Branch.bGood ? 1.0 + Luck : 1.0);
					if (Roll <= 0.0)
					{
						Picked = Index;
						break;
					}
				}
				if (HasPassive("Gambler.LadyLuck"))
				{
					Luck = Def.Branches[static_cast<size_t>(Picked)].bGood ? 0.0 : Luck + LuckPityStep;
				}
				if (Def.Slot != ESlotId::Basic)
				{
					World->Message(Def.Name + ": " + Def.Branches[static_cast<size_t>(Picked)].Label);
				}
				return Picked;
			}
			default:
				return -1;
			}
		}

		void FRuntime::BeginTimeline(const FAbilityDef& Def, ESlotId Slot, double Held, bool bFree, double StartTime)
		{
			FTimeline Timeline;
			Timeline.Def = &Def;
			Timeline.Slot = Slot;
			Timeline.Start = StartTime;
			Timeline.Held = Held;
			Timeline.bFree = bFree;
			Timeline.bHolding = Def.Cast == ECast::Hold;
			double End = StartTime + std::max(0.0, Def.TotalSeconds);
			for (const FStep& Step : Def.Steps)
			{
				if (Step.Combo >= 0 && Step.Combo != ComboIndex)
				{
					continue;
				}
				FScheduled Scheduled;
				Scheduled.Step = Step;
				Scheduled.At = StartTime + Step.At;
				Timeline.Steps.push_back(Scheduled);
				End = std::max(End, Scheduled.At + Step.Duration);
			}
			const int Branch = ChooseBranch(Def);
			if (Branch >= 0)
			{
				for (const FStep& Step : Def.Branches[static_cast<size_t>(Branch)].Steps)
				{
					FScheduled Scheduled;
					Scheduled.Step = Step;
					Scheduled.At = StartTime + Def.BranchAt + Step.At;
					Timeline.Steps.push_back(Scheduled);
					End = std::max(End, Scheduled.At + Step.Duration);
				}
			}
			Timeline.EndsAt = Timeline.bHolding ? StartTime + 1e9 : End;
			for (const FScheduled& Scheduled : Timeline.Steps)
			{
				if (StepDealsDamage(Scheduled.Step))
				{
					const bool bRepeats = Scheduled.Step.Duration > 0.0 && Scheduled.Step.Interval > 0.0 && KindRepeats(Scheduled.Step.Kind);
					Timeline.LastHitAt = std::max(Timeline.LastHitAt, Scheduled.At + (bRepeats ? Scheduled.Step.Duration : 0.0));
				}
			}
			if (Def.LockSeconds > 0.0 && !bFree)
			{
				Timeline.LockEndsAt = StartTime + Def.LockSeconds;
				LockedUntil = std::max(LockedUntil, StartTime + Def.LockSeconds);
				World->CasterLock(Def.LockSeconds);
			}
			if (Def.Reaction.Trigger != EReaction::None)
			{
				FReactionState State;
				State.Def = &Def.Reaction;
				State.ExpiresAt = StartTime + Def.Reaction.Window;
				State.TriggersLeft = std::max(1, Def.Reaction.MaxTriggers);
				Reactions.push_back(State);
			}
			if (Def.bStoresDamageTaken)
			{
				StoredValue = 0.0;
				bStoringTaken = true;
			}
			if (bInTimelineTick)
			{
				PendingTimelines.push_back(Timeline);
			}
			else
			{
				Timelines.push_back(Timeline);
			}
		}

		void FRuntime::TickTimelines(double)
		{
			const double T = Now();
			bInTimelineTick = true;
			for (size_t Index = 0; Index < Timelines.size(); ++Index)
			{
				if (Timelines[Index].bHolding && !Timelines[Index].bReleased)
				{
					if (T - Timelines[Index].Start >= Timelines[Index].Def->TotalSeconds)
					{
						Release(Timelines[Index].Slot);
					}
					continue;
				}
				FContext Ctx;
				Ctx.Held = Timelines[Index].Held;
				Ctx.bBasic = Timelines[Index].Slot == ESlotId::Basic;
				Ctx.bUlt = Timelines[Index].Slot == ESlotId::Ultimate;
				// Hit feel (BIBHitFeel.h): every hit of this step carries where it came from.
				FHitInfo Info;
				Info.Slot = Timelines[Index].Slot;
				Info.FeelWeight = Timelines[Index].Def ? Timelines[Index].Def->FeelWeight : -1;
				if (Ctx.bBasic && Timelines[Index].Def)
				{
					Info.ComboIndex = ComboIndex;
					Info.ComboLength = std::max(1, Timelines[Index].Def->ComboLength);
				}
				const double LastHitAt = Timelines[Index].LastHitAt;
				for (size_t StepIndex = 0; StepIndex < Timelines[Index].Steps.size(); ++StepIndex)
				{
					FScheduled& Scheduled = Timelines[Index].Steps[StepIndex];
					if (Scheduled.bDone || T + 1e-9 < Scheduled.At)
					{
						continue;
					}
					const FStep Step = Scheduled.Step;   // copy: executing may touch the timeline list
					const bool bLastStep = StepDealsDamage(Step) && LastHitAt >= 0.0;
					if (Step.Duration > 0.0 && Step.Interval > 0.0 && KindRepeats(Step.Kind))
					{
						if (!Scheduled.bStarted)
						{
							Scheduled.bStarted = true;
							Scheduled.EndAt = Scheduled.At + Step.Duration;
							Scheduled.NextTick = Scheduled.At;
						}
						while (T + 1e-9 >= Timelines[Index].Steps[StepIndex].NextTick && Timelines[Index].Steps[StepIndex].NextTick <= Scheduled.EndAt + 1e-9)
						{
							Timelines[Index].Steps[StepIndex].NextTick += Step.Interval;
							const bool bLastTick = Timelines[Index].Steps[StepIndex].NextTick > Scheduled.EndAt + 1e-9;
							CurrentHit = Info;
							CurrentHit.bTick = true;
							CurrentHit.bFinal = Ctx.bBasic ? (Info.ComboLength > 1 && Info.ComboIndex == Info.ComboLength - 1)
								: (bLastStep && bLastTick && Scheduled.EndAt >= LastHitAt - 1e-9);
							ExecuteStep(Step, &Timelines[Index], Ctx);
						}
						if (T >= Timelines[Index].Steps[StepIndex].EndAt)
						{
							Timelines[Index].Steps[StepIndex].bDone = true;
						}
					}
					else
					{
						Timelines[Index].Steps[StepIndex].bDone = true;
						CurrentHit = Info;
						CurrentHit.bFinal = Ctx.bBasic ? (Info.ComboLength > 1 && Info.ComboIndex == Info.ComboLength - 1)
							: (bLastStep && Scheduled.At >= LastHitAt - 1e-9);
						ExecuteStep(Step, &Timelines[Index], Ctx);
					}
					CurrentHit = FHitInfo();
				}
			}
			bInTimelineTick = false;
			for (FTimeline& Pending : PendingTimelines)
			{
				Timelines.push_back(Pending);
			}
			PendingTimelines.clear();
			for (size_t Index = 0; Index < Timelines.size();)
			{
				bool bAllDone = true;
				for (const FScheduled& Scheduled : Timelines[Index].Steps)
				{
					bAllDone = bAllDone && Scheduled.bDone;
				}
				if (bAllDone && T >= Timelines[Index].EndsAt && !(Timelines[Index].bHolding && !Timelines[Index].bReleased))
				{
					if (Timelines[Index].Def && Timelines[Index].Def->bStoresDamageTaken)
					{
						bStoringTaken = false;
					}
					Timelines.erase(Timelines.begin() + static_cast<std::ptrdiff_t>(Index));
					continue;
				}
				++Index;
			}
		}

		void FRuntime::CancelAll()
		{
			Timelines.clear();
			PendingTimelines.clear();
			Reactions.clear();
			bStoringTaken = false;
		}

		void FRuntime::RunStepsNow(const std::vector<FStep>& Steps, const FContext& Ctx)
		{
			// Passives and reactions are not part of the cast that triggered them (hit feel: classified by size).
			const FHitInfo Saved = CurrentHit;
			CurrentHit = FHitInfo();
			for (const FStep& Step : Steps)
			{
				ExecuteStep(Step, nullptr, Ctx);
			}
			CurrentHit = Saved;
		}

		// ============================================================================ steps
		void FRuntime::ExecuteStep(const FStep& RawStep, FTimeline*, const FContext& Ctx)
		{
			FStep Step = RawStep;
			const FUltGuard UltGuard(bSuppressUlt, Ctx.bUlt);
			HitAmount = Ctx.Amount;
			if (Ctx.bBasic && IsBasicBoostedByTakeover())
			{
				Step.Damage *= TakeoverBasicDamageScale;
				Step.AtkMult *= TakeoverBasicDamageScale;
				Step.Range *= TakeoverBasicSizeScale;
				Step.Radius *= TakeoverBasicSizeScale;
			}
			const FVec Origin = World->CasterPos();
			const FVec Facing = Norm2(World->CasterAim());
			std::vector<FEnemyView> Targets;
			FVec Centre = Origin;
			FVec Aim = Facing;

			switch (Step.Kind)
			{
			case EStep::Melee:
			case EStep::Burst:
			case EStep::Beam:
			case EStep::Chain:
				ResolveTargets(Step, Ctx, Targets, Centre, Aim);
				for (const FEnemyView& Enemy : Targets)
				{
					HitEnemy(Step, Enemy, Centre, Ctx.bBasic);
					if (Step.Kind == EStep::Beam && Step.Param != 0.0 && World->EnemyAlive(Enemy.Id))
					{
						World->Impulse(Enemy.Id, Aim * Step.Param);   // carries what is stuck on the beam (Alchemist's saw)
					}
				}
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Centre, std::max(Step.Radius, Step.Range * 0.5), 0.5);
				}
				break;

			case EStep::Projectile:
			{
				const int Count = std::max(1, Step.Hits);
				FVec BaseDir = Facing;
				if (Step.Flags & FlagNearestEnemy)
				{
					std::vector<FEnemyView> All;
					CollectEnemies(All);
					FEnemyView Nearest;
					if (FindNearest(All, Origin, Step.Range > 0.0 ? Step.Range : ProjectileDefaultRange, Step.Flags, Nearest))
					{
						BaseDir = Norm2(Nearest.Pos - Origin);
					}
				}
				for (int Index = 0; Index < Count; ++Index)
				{
					const double SpreadRadians = Step.Param * Pi / 180.0;
					const double Angle = Count > 1 ? -SpreadRadians * 0.5 + SpreadRadians * static_cast<double>(Index) / static_cast<double>(Count - 1) : 0.0;
					FProjectile Shot;
					Shot.Step = Step;
					Shot.Dir = Rotate2(BaseDir, Angle);
					Shot.Pos = Origin + Shot.Dir * 60.0;
					Shot.PierceLeft = std::max(1, Step.Pierce);
					Shot.bUlt = Ctx.bUlt;
					Shot.Source = CurrentHit;
					Shot.Source.bTick = false;
					Projectiles.push_back(Shot);
				}
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Origin, 100.0, 0.3);
				}
				break;
			}

			case EStep::Zone:
			{
				FZone Zone;
				Zone.Step = Step;
				Zone.Centre = Origin + Facing * Step.Offset;
				if (Step.Flags & FlagFromAimPoint)
				{
					Zone.Centre = Origin + Facing * Step.Range;
				}
				if (Step.Flags & FlagAtTarget)
				{
					std::vector<FEnemyView> All;
					CollectEnemies(All);
					FEnemyView Nearest;
					if (FindNearest(All, Origin, Step.Range > 0.0 ? Step.Range : DefaultReach, Step.Flags, Nearest))
					{
						Zone.Centre = Nearest.Pos;
					}
				}
				Zone.bFollow = (Step.Flags & FlagFollowCaster) != 0;
				Zone.bUlt = Ctx.bUlt;
				Zone.Hit = CurrentHit;
				Zone.EndsAt = Now() + std::max(0.1, Step.Duration);
				Zone.NextTick = Now();
				if (Zones.size() >= MaxZones)
				{
					Zones.erase(Zones.begin());
				}
				Zones.push_back(Zone);
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Zone.Centre, Step.Radius, Step.Duration);
				}
				break;
			}

			case EStep::Dash:
			{
				const double Seconds = Step.Duration > 0.0 ? Step.Duration : 0.18;
				const FVec To = Origin + Facing * Step.Range;
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				for (const FEnemyView& Enemy : All)
				{
					if (!PassesFilter(Enemy, Step.Flags))
					{
						continue;
					}
					const double Along = std::max(0.0, std::min(Dot2(Enemy.Pos - Origin, Facing), Step.Range));
					const FVec Closest = Origin + Facing * Along;
					if (Dist2(Closest, Enemy.Pos) <= std::max(Step.Radius, 100.0) + Enemy.Radius)
					{
						HitEnemy(Step, Enemy, Origin, Ctx.bBasic);
					}
				}
				if (Step.Flags & FlagInvulnerable)
				{
					InvulnerableUntil = std::max(InvulnerableUntil, Now() + Seconds + 0.05);
					World->CasterInvulnerable(Seconds + 0.05);
				}
				World->CasterMove(To, Seconds);
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Origin, 150.0, Seconds);
				}
				break;
			}

			case EStep::Teleport:
			{
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				FEnemyView Chosen;
				bool bFound = false;
				const double Reach = Step.Range > 0.0 ? Step.Range : DefaultReach;
				if (Step.Flags & FlagRandomEnemy)
				{
					std::vector<FEnemyView> Candidates;
					for (const FEnemyView& Enemy : All)
					{
						if (PassesFilter(Enemy, Step.Flags) && Dist2(Origin, Enemy.Pos) <= Reach + Enemy.Radius)
						{
							Candidates.push_back(Enemy);
						}
					}
					if (!Candidates.empty())
					{
						Chosen = Candidates[std::min(Candidates.size() - 1, static_cast<size_t>(Rand() * static_cast<double>(Candidates.size())))];
						bFound = true;
					}
				}
				else if (Step.Flags & (FlagNearestEnemy | FlagMarkedOnly))
				{
					bFound = FindNearest(All, Origin, Reach, Step.Flags, Chosen);
				}
				FVec Destination = Origin + Facing * Step.Range;
				if (bFound)
				{
					const FVec Away = Norm2(Chosen.Pos - Origin);
					Destination = (Step.Flags & FlagBehindTarget) ? Chosen.Pos + Away * (Chosen.Radius + 90.0) : Chosen.Pos - Away * (Chosen.Radius + 90.0);
					Destination.Z = Origin.Z;
					if (Step.Flags & FlagStoreTarget)
					{
						StoredTarget = Chosen.Id;
					}
				}
				else if (Step.Flags & (FlagNearestEnemy | FlagMarkedOnly | FlagRandomEnemy))
				{
					break;   // nobody to blink to
				}
				if (Step.Flags & FlagInvulnerable)
				{
					const double Seconds = std::max(0.2, Step.Duration);
					InvulnerableUntil = std::max(InvulnerableUntil, Now() + Seconds);
					World->CasterInvulnerable(Seconds);
				}
				World->CasterMove(Destination, 0.0);
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Destination, 160.0, 0.4);
				}
				break;
			}

			case EStep::Buff:
				AddBuff(static_cast<EBuff>(Step.Stat), Step.Param, Step.Duration > 0.0 ? Step.Duration : 1.0);
				if (Step.Flags & FlagInvulnerable)
				{
					InvulnerableUntil = std::max(InvulnerableUntil, Now() + Step.Duration);
					World->CasterInvulnerable(Step.Duration);
				}
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Origin, 200.0, Step.Duration);
				}
				break;

			case EStep::Heal:
				World->CasterHeal(Step.Param * World->CasterMaxHP() + Step.Damage);
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Origin, 150.0, 0.6);
				}
				break;

			case EStep::Shield:
				AddShield(Step.Damage, Step.Duration);
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Origin, 150.0, Step.Duration);
				}
				break;

			case EStep::Resource:
				if (Step.Param >= 0.0)
				{
					GainResource(Step.Stat, Step.Param);
				}
				else
				{
					SpendResource(Step.Stat, -Step.Param);
				}
				break;

			case EStep::SelfDamage:
				SelfCost(Step.Param);
				break;

			case EStep::Execute:
			{
				ResolveTargets(Step, Ctx, Targets, Centre, Aim);
				int Killed = 0;
				for (const FEnemyView& Enemy : Targets)
				{
					if (!World->EnemyAlive(Enemy.Id))
					{
						continue;
					}
					const double HpFraction = Enemy.MaxHP > 0.0 ? Enemy.HP / Enemy.MaxHP : 1.0;
					const bool bMarkedRule = (Step.Flags & FlagMarkedOnly) != 0;
					if (!bMarkedRule && HpFraction > Step.ExecuteBelow)
					{
						continue;
					}
					if (Step.Param2 > 0.0 && World->CasterHP() < World->CasterMaxHP() * Step.Param2)
					{
						continue;   // limitation: too weak to pay the toll
					}
					World->KillEnemyHit(Enemy.Id, (Step.Flags & FlagRespectBoss) != 0, Step.BossFraction, CurrentHit);
					++Killed;
					if (Step.Param > 0.0)
					{
						SelfCost(Step.Param);
					}
					if (!World->EnemyAlive(Enemy.Id))
					{
						HandleKill(Enemy.Id);
					}
					else
					{
						FContext Hit;
						Hit.TriggerEnemy = Enemy.Id;
					}
				}
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Centre, std::max(Step.Radius, Step.Range * 0.5), 0.6);
				}
				(void)Killed;
				break;
			}

			case EStep::Detonate:
			{
				ResolveTargets(Step, Ctx, Targets, Centre, Aim);
				for (const FEnemyView& Enemy : Targets)
				{
					const int Stacks = World->StatusStacks(Enemy.Id, Step.Status);
					if (Stacks <= 0)
					{
						continue;
					}
					const double Amount = StepDamage(Step, false) * static_cast<double>(Stacks);
					const double Dealt = World->DamageEnemy(Enemy.Id, Amount, DmgNone);
					RecordDamage(Enemy.Id, Dealt, false);
					if (Step.Param2 > 0.5)
					{
						World->ClearStatus(Enemy.Id, Step.Status);
					}
					if (Step.Param > 0.0)
					{
						GainResource(Step.Stat, Step.Param * static_cast<double>(Stacks));
					}
					if (!World->EnemyAlive(Enemy.Id))
					{
						HandleKill(Enemy.Id);
					}
				}
				if (Step.CueId > 0)
				{
					World->Cue(Step.CueId, Centre, Step.Radius, 0.5);
				}
				break;
			}

			case EStep::Hook:
				RunHook(Step, nullptr, Ctx);
				break;

			case EStep::Cue:
				World->Cue(Step.CueId, Origin + Facing * Step.Offset, Step.Radius, Step.Duration > 0.0 ? Step.Duration : 0.6);
				break;
			}
		}

		// ============================================================================ zones & projectiles
		void FRuntime::TickZones(double)
		{
			const double T = Now();
			for (size_t Index = 0; Index < Zones.size();)
			{
				FZone& Zone = Zones[Index];
				if (Zone.bFollow)
				{
					Zone.Centre = World->CasterPos();
				}
				while (T + 1e-9 >= Zone.NextTick && Zone.NextTick <= Zone.EndsAt + 1e-9)
				{
					Zone.NextTick += Zone.Step.Interval > 0.0 ? Zone.Step.Interval : 1.0;
					// Hit feel: zone ticks are ticks; the last tick of a zone placed by a cast's final step is that cast's finisher.
					CurrentHit = Zone.Hit;
					CurrentHit.bTick = true;
					CurrentHit.bFinal = Zone.Hit.bFinal && Zone.NextTick > Zone.EndsAt + 1e-9;
					std::vector<FEnemyView> All;
					CollectEnemies(All);
					std::vector<FEnemyView> Inside;
					for (const FEnemyView& Enemy : All)
					{
						if (PassesFilter(Enemy, Zone.Step.Flags) && Dist2(Zone.Centre, Enemy.Pos) <= Zone.Step.Radius + Enemy.Radius)
						{
							Inside.push_back(Enemy);
						}
					}
					if (Zone.Step.Flags & FlagTurret)
					{
						const FVec Centre = Zone.Centre;
						std::stable_sort(Inside.begin(), Inside.end(), [&Centre](const FEnemyView& A, const FEnemyView& B)
						{
							return Dist2(Centre, A.Pos) < Dist2(Centre, B.Pos);
						});
						const size_t Shots = static_cast<size_t>(std::max(1, Zone.Step.Hits));
						if (Inside.size() > Shots)
						{
							Inside.resize(Shots);
						}
					}
					const FStep ZoneStep = Zone.Step;
					const FVec ZoneCentre = Zone.Centre;
					const FUltGuard ZoneGuard(bSuppressUlt, Zone.bUlt);
					for (const FEnemyView& Enemy : Inside)
					{
						HitEnemy(ZoneStep, Enemy, ZoneCentre, false);
						if (ZoneStep.Param > 0.0)
						{
							GainResource(ZoneStep.Stat, ZoneStep.Param);
						}
					}
					if (&Zone != &Zones[Index])
					{
						break;   // the list changed while hitting
					}
				}
				if (Index < Zones.size() && T >= Zones[Index].EndsAt)
				{
					Zones.erase(Zones.begin() + static_cast<std::ptrdiff_t>(Index));
					continue;
				}
				++Index;
			}
			CurrentHit = FHitInfo();
		}

		void FRuntime::TickProjectiles(double Dt)
		{
			for (size_t Index = 0; Index < Projectiles.size();)
			{
				FProjectile& Shot = Projectiles[Index];
				const double Speed = Shot.Step.Speed > 0.0 ? Shot.Step.Speed : ProjectileDefaultSpeed;
				const double Range = Shot.Step.Range > 0.0 ? Shot.Step.Range : ProjectileDefaultRange;
				const double Radius = Shot.Step.Radius > 0.0 ? Shot.Step.Radius : ProjectileDefaultRadius;
				const double Travel = std::min(Speed * Dt, Range - Shot.Travelled);
				Shot.Pos = Shot.Pos + Shot.Dir * Travel;
				Shot.Travelled += Travel;
				const bool bExplodes = Shot.Step.Param2 > 0.0;
				bool bRemove = Shot.Travelled >= Range - 1e-6;
				std::vector<FEnemyView> All;
				CollectEnemies(All);
				const FStep ShotStep = Shot.Step;
				const FVec ShotPos = Shot.Pos;
				const FUltGuard ShotGuard(bSuppressUlt, Shot.bUlt);
				CurrentHit = Shot.Source;   // hit feel: the cast that fired it
				bool bContact = false;
				std::vector<int> NewHits;
				for (const FEnemyView& Enemy : All)
				{
					if (!PassesFilter(Enemy, ShotStep.Flags) || Dist2(ShotPos, Enemy.Pos) > Radius + Enemy.Radius)
					{
						continue;
					}
					if (std::find(Shot.Hit.begin(), Shot.Hit.end(), Enemy.Id) != Shot.Hit.end())
					{
						continue;
					}
					bContact = true;
					if (!bExplodes)
					{
						NewHits.push_back(Enemy.Id);
						HitEnemy(ShotStep, Enemy, ShotPos, false);
						--Shot.PierceLeft;
						Shot.Hit.push_back(Enemy.Id);
						if (Shot.PierceLeft <= 0)
						{
							bRemove = true;
							break;
						}
					}
				}
				if (bExplodes && (bContact || bRemove))
				{
					for (const FEnemyView& Enemy : All)
					{
						if (PassesFilter(Enemy, ShotStep.Flags) && Dist2(ShotPos, Enemy.Pos) <= ShotStep.Param2 + Enemy.Radius)
						{
							HitEnemy(ShotStep, Enemy, ShotPos, false);
						}
					}
					if (ShotStep.CueId > 0)
					{
						World->Cue(ShotStep.CueId, ShotPos, ShotStep.Param2, 0.5);
					}
					bRemove = true;
				}
				if (bRemove)
				{
					Projectiles.erase(Projectiles.begin() + static_cast<std::ptrdiff_t>(Index));
					continue;
				}
				++Index;
			}
			CurrentHit = FHitInfo();
		}
	}
}
