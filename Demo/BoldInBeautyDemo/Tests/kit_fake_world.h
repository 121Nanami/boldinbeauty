// A fake battlefield for the kit tests and the balance simulator: enemies stand still, statuses tick through the real
// FStatusBook, the runtime is driven with fixed time steps.
#pragma once

#include "../Source/BoldInBeautyDemo/BIBRules/BIBKitRuntime.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace KitTest
{
	using namespace BIB::Kits;

	struct FakeEnemy
	{
		int Id = 0;
		FVec Pos;
		double Radius = 40.0;
		double HP = 2000.0;
		double MaxHP = 2000.0;
		bool bBoss = false;
		bool bAlive = true;
		bool bRespawn = false;      // balance simulator: dummies come back at full health after a moment
		double RespawnAt = 0.0;
		double Overkill = 0.0;
		FStatusBook Book;
		FVec LastImpulse;
		FVec Velocity;
		double VelocityLeft = 0.0;
		double DamageTaken = 0.0;
	};

	class FakeWorld : public IWorld
	{
	public:
		double Time = 0.0;
		FVec Pos;
		FVec Aim{1.0, 0.0, 0.0};
		double Attack = 450.0;
		double DamageMult = 1.0;
		double BaseMaxHP = 3500.0;
		double HP = 3500.0;
		double MaxHP = 3500.0;
		double Cooldowns[SlotCount] = {};
		double Ult = 0.0;
		double Healed = 0.0;
		double SelfDamage = 0.0;
		double LockedFor = 0.0;
		double InvulnerableFor = 0.0;
		double TotalDamage = 0.0;
		int CueCalls = 0;
		std::vector<std::string> Log;
		std::vector<FakeEnemy> Enemies;
		std::mt19937 Rng{12345u};
		int NextId = 1;
		FVec MoveFrom;
		FVec MoveTo;
		double MoveLeft = 0.0;
		double MoveTotal = 0.0;

		int AddEnemy(double X, double Y, double InHP = 2000.0, bool bBossEnemy = false)
		{
			FakeEnemy Enemy;
			Enemy.Id = NextId++;
			Enemy.Pos = FVec{X, Y, 0.0};
			Enemy.HP = Enemy.MaxHP = InHP;
			Enemy.bBoss = bBossEnemy;
			Enemy.Radius = bBossEnemy ? 140.0 : 40.0;
			Enemies.push_back(Enemy);
			return Enemy.Id;
		}

		FakeEnemy* Find(int Id)
		{
			for (FakeEnemy& Enemy : Enemies)
			{
				if (Enemy.Id == Id)
				{
					return &Enemy;
				}
			}
			return nullptr;
		}

		int AliveCount() const
		{
			int Count = 0;
			for (const FakeEnemy& Enemy : Enemies)
			{
				Count += Enemy.bAlive ? 1 : 0;
			}
			return Count;
		}

		// Advances the world and the runtime by Dt seconds.
		void Step(double Dt, FRuntime& Runtime)
		{
			Time += Dt;
			for (double& Cd : Cooldowns)
			{
				Cd = std::max(0.0, Cd - Dt);
			}
			LockedFor = std::max(0.0, LockedFor - Dt);
			InvulnerableFor = std::max(0.0, InvulnerableFor - Dt);
			if (MoveLeft > 0.0)
			{
				MoveLeft = std::max(0.0, MoveLeft - Dt);
				const double Progress = MoveTotal > 0.0 ? 1.0 - MoveLeft / MoveTotal : 1.0;
				Pos = MoveFrom + (MoveTo - MoveFrom) * Progress;
			}
			for (FakeEnemy& Enemy : Enemies)
			{
				if (!Enemy.bAlive && Enemy.bRespawn && Time >= Enemy.RespawnAt)
				{
					Enemy.bAlive = true;
					Enemy.HP = Enemy.MaxHP;
					Enemy.Book.ClearAll();
				}
				if (!Enemy.bAlive)
				{
					continue;
				}
				if (Enemy.VelocityLeft > 0.0)
				{
					Enemy.VelocityLeft = std::max(0.0, Enemy.VelocityLeft - Dt);
					Enemy.Pos = Enemy.Pos + Enemy.Velocity * Dt;
				}
				std::vector<FStatusEvent> Events;
				Enemy.Book.Tick(Dt, Events);
				for (const FStatusEvent& Event : Events)
				{
					if (Event.Type == FStatusEvent::EType::DotDamage || Event.Type == FStatusEvent::EType::ShockDischarge)
					{
						const double Dealt = DamageEnemy(Enemy.Id, Event.Amount, DmgDot);
						Runtime.NotifyDamageDealt(Enemy.Id, Dealt, false);
					}
				}
				if (!Enemy.bAlive)
				{
					Runtime.NotifyEnemyKilled(Enemy.Id);
				}
			}
			if (bBenched)
			{
				Runtime.TickBackground(Dt);   // swapped out: only what she left on the enemies runs
			}
			else
			{
				Runtime.Tick(Dt);
			}
		}

		bool bBenched = false;   // the operator is swapped out (Step calls FRuntime::TickBackground)

		void Run(double Seconds, FRuntime& Runtime, double Dt = 1.0 / 60.0)
		{
			const int Steps = static_cast<int>(Seconds / Dt + 0.5);
			for (int Index = 0; Index < Steps; ++Index)
			{
				Step(Dt, Runtime);
			}
		}

		// ---- IWorld
		double Now() const override { return Time; }
		FVec CasterPos() const override { return Pos; }
		FVec CasterAim() const override { return Aim; }
		double CasterAttack() const override { return Attack; }
		double CasterDamageMult() const override { return DamageMult; }
		double CasterHP() const override { return HP; }
		double CasterMaxHP() const override { return MaxHP; }
		void CasterMove(const FVec& To, double Seconds) override
		{
			if (Seconds <= 0.0)
			{
				Pos = To;
				MoveLeft = 0.0;
				return;
			}
			MoveFrom = Pos;
			MoveTo = To;
			MoveLeft = MoveTotal = Seconds;
		}
		void CasterInvulnerable(double Seconds) override { InvulnerableFor = std::max(InvulnerableFor, Seconds); }
		void CasterHeal(double Amount) override
		{
			const double Before = HP;
			HP = std::min(MaxHP, HP + Amount);
			Healed += HP - Before;
		}
		void CasterSelfDamage(double Amount) override
		{
			HP -= Amount;
			SelfDamage += Amount;
		}
		void CasterLock(double Seconds) override { LockedFor = std::max(LockedFor, Seconds); }
		void CasterHealthScale(double Mult, bool bFill) override
		{
			const double Ratio = MaxHP > 0.0 ? HP / MaxHP : 1.0;
			MaxHP = BaseMaxHP * Mult;
			HP = bFill ? MaxHP : MaxHP * Ratio;
		}
		void GetEnemies(std::vector<FEnemyView>& Out) const override
		{
			for (const FakeEnemy& Enemy : Enemies)
			{
				if (Enemy.bAlive)
				{
					FEnemyView View;
					View.Id = Enemy.Id;
					View.Pos = Enemy.Pos;
					View.Radius = Enemy.Radius;
					View.HP = Enemy.HP;
					View.MaxHP = Enemy.MaxHP;
					View.bBoss = Enemy.bBoss;
					Out.push_back(View);
				}
			}
		}
		bool EnemyAlive(int Id) const override
		{
			for (const FakeEnemy& Enemy : Enemies)
			{
				if (Enemy.Id == Id)
				{
					return Enemy.bAlive;
				}
			}
			return false;
		}
		double DamageEnemy(int Id, double Amount, unsigned Flags) override
		{
			FakeEnemy* Enemy = Find(Id);
			if (!Enemy || !Enemy->bAlive || Amount <= 0.0)
			{
				return 0.0;
			}
			const double Scaled = (Flags & DmgTrue) ? Amount : Amount * Enemy->Book.DamageTakenMult();
			const double Dealt = std::min(Enemy->HP, Scaled);
			Enemy->HP -= Dealt;
			Enemy->DamageTaken += Dealt;
			TotalDamage += Dealt;
			if (Enemy->HP <= 1e-9)
			{
				Enemy->HP = 0.0;
				Enemy->bAlive = false;
				Enemy->RespawnAt = Time + 0.05;
			}
			return Dealt;
		}
		void KillEnemy(int Id, bool bRespectBoss, double BossFraction) override
		{
			FakeEnemy* Enemy = Find(Id);
			if (!Enemy || !Enemy->bAlive)
			{
				return;
			}
			if (Enemy->bBoss && bRespectBoss)
			{
				DamageEnemy(Id, Enemy->MaxHP * BossFraction, DmgTrue | DmgExecute);
			}
			else
			{
				DamageEnemy(Id, Enemy->HP, DmgTrue | DmgExecute);
			}
		}
		void Impulse(int Id, const FVec& Velocity) override
		{
			if (FakeEnemy* Enemy = Find(Id))
			{
				Enemy->LastImpulse = Velocity;
				Enemy->Velocity = Velocity;
				Enemy->VelocityLeft = 0.15;
			}
		}
		bool ApplyStatus(int Id, EStatus Status, double Seconds, double Power, int Stacks) override
		{
			FakeEnemy* Enemy = Find(Id);
			return Enemy && Enemy->bAlive && Enemy->Book.Apply(Status, Seconds, Power, Stacks, 0);
		}
		int StatusStacks(int Id, EStatus Status) const override
		{
			for (const FakeEnemy& Enemy : Enemies)
			{
				if (Enemy.Id == Id)
				{
					return Enemy.Book.Stacks(Status);
				}
			}
			return 0;
		}
		void ClearStatus(int Id, EStatus Status) override
		{
			if (FakeEnemy* Enemy = Find(Id))
			{
				Enemy->Book.Clear(Status);
			}
		}
		void RemoveStatusStacks(int Id, EStatus Status, int Count) override
		{
			if (FakeEnemy* Enemy = Find(Id))
			{
				Enemy->Book.RemoveStacks(Status, Count);
			}
		}
		double CooldownRemaining(ESlotId Slot) const override { return Cooldowns[static_cast<int>(Slot)]; }
		void StartCooldown(ESlotId Slot, double Seconds) override { Cooldowns[static_cast<int>(Slot)] = Seconds; }
		void ReduceCooldown(ESlotId Slot, double Seconds) override
		{
			double& Cd = Cooldowns[static_cast<int>(Slot)];
			Cd = std::max(0.0, Cd - Seconds);
		}
		void AddUltimate(double DamageDealt) override { Ult += DamageDealt; }
		void Cue(int, const FVec&, double, double) override { ++CueCalls; }
		void Message(const std::string& Text) override { Log.push_back(Text); }
		double Random01() override
		{
			return std::uniform_real_distribution<double>(0.0, 1.0)(Rng);
		}
	};
}
