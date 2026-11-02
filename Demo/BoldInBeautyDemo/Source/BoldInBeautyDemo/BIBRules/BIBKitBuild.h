// Boldinbeauty demo — small helpers that keep the kit data tables readable (internal to the kit data files).
#pragma once

#include "BIBKitTypes.h"

namespace BIB
{
	namespace Kits
	{
		namespace B
		{
			// ---- steps
			inline FStep Melee(double At, double Range, double ArcDot, double Damage, double AtkMult = 0.0)
			{
				FStep S;
				S.Kind = EStep::Melee;
				S.At = At;
				S.Range = Range;
				S.ArcDot = ArcDot;
				S.Damage = Damage;
				S.AtkMult = AtkMult;
				S.CueId = CueSlash;
				return S;
			}

			inline FStep Burst(double At, double Radius, double Damage, double AtkMult = 0.0)
			{
				FStep S;
				S.Kind = EStep::Burst;
				S.At = At;
				S.Radius = Radius;
				S.Damage = Damage;
				S.AtkMult = AtkMult;
				S.CueId = CueBurst;
				return S;
			}

			inline FStep BurstAhead(double At, double Ahead, double Radius, double Damage)
			{
				FStep S = Burst(At, Radius, Damage);
				S.Range = Ahead;
				S.Flags |= FlagFromAimPoint;
				return S;
			}

			inline FStep Beam(double At, double Length, double Width, double Damage, double AtkMult = 0.0)
			{
				FStep S;
				S.Kind = EStep::Beam;
				S.At = At;
				S.Range = Length;
				S.Radius = Width;
				S.Damage = Damage;
				S.AtkMult = AtkMult;
				S.CueId = CueBeam;
				return S;
			}

			inline FStep Proj(double At, int Count, double Speed, double Range, double Radius, double Damage, double AtkMult = 0.0, double SpreadDegrees = 0.0)
			{
				FStep S;
				S.Kind = EStep::Projectile;
				S.At = At;
				S.Hits = Count;
				S.Speed = Speed;
				S.Range = Range;
				S.Radius = Radius;
				S.Damage = Damage;
				S.AtkMult = AtkMult;
				S.Param = SpreadDegrees;
				return S;
			}

			inline FStep ZoneAt(double At, double Ahead, double Radius, double Seconds, double Interval, double Damage)
			{
				FStep S;
				S.Kind = EStep::Zone;
				S.At = At;
				S.Range = Ahead;
				S.Radius = Radius;
				S.Duration = Seconds;
				S.Interval = Interval;
				S.Damage = Damage;
				S.Flags |= FlagFromAimPoint;
				S.CueId = CueZone;
				return S;
			}

			inline FStep ZoneFollow(double At, double Radius, double Seconds, double Interval, double Damage)
			{
				FStep S;
				S.Kind = EStep::Zone;
				S.At = At;
				S.Radius = Radius;
				S.Duration = Seconds;
				S.Interval = Interval;
				S.Damage = Damage;
				S.Flags |= FlagFollowCaster;
				S.CueId = CueZone;
				return S;
			}

			inline FStep DashStep(double At, double Distance, double Seconds, double Damage, double HitRadius = 120.0)
			{
				FStep S;
				S.Kind = EStep::Dash;
				S.At = At;
				S.Range = Distance;
				S.Duration = Seconds;
				S.Damage = Damage;
				S.Radius = HitRadius;
				S.CueId = CueDash;
				return S;
			}

			inline FStep Blink(double At, double Range, unsigned Flags, double InvulnSeconds = 0.0)
			{
				FStep S;
				S.Kind = EStep::Teleport;
				S.At = At;
				S.Range = Range;
				S.Flags = Flags;
				S.Duration = InvulnSeconds;
				S.CueId = CueBlink;
				return S;
			}

			inline FStep BuffStep(double At, EBuff Stat, double Magnitude, double Seconds)
			{
				FStep S;
				S.Kind = EStep::Buff;
				S.At = At;
				S.Stat = static_cast<int>(Stat);
				S.Param = Magnitude;
				S.Duration = Seconds;
				S.CueId = CueBuff;
				return S;
			}

			inline FStep HealStep(double At, double Fraction)
			{
				FStep S;
				S.Kind = EStep::Heal;
				S.At = At;
				S.Param = Fraction;
				S.CueId = CueHeal;
				return S;
			}

			inline FStep ShieldStep(double At, double Amount, double Seconds)
			{
				FStep S;
				S.Kind = EStep::Shield;
				S.At = At;
				S.Damage = Amount;
				S.Duration = Seconds;
				S.CueId = CueShield;
				return S;
			}

			inline FStep ResStep(double At, int Resource, double Amount)
			{
				FStep S;
				S.Kind = EStep::Resource;
				S.At = At;
				S.Stat = Resource;
				S.Param = Amount;
				return S;
			}

			inline FStep SelfCostStep(double At, double Fraction)
			{
				FStep S;
				S.Kind = EStep::SelfDamage;
				S.At = At;
				S.Param = Fraction;
				return S;
			}

			inline FStep ExecStep(double At, unsigned Flags, double Below, double TollFraction, double MinHpFraction, double BossFrac = 0.15)
			{
				FStep S;
				S.Kind = EStep::Execute;
				S.At = At;
				S.Flags = Flags;
				S.ExecuteBelow = Below;
				S.Param = TollFraction;
				S.Param2 = MinHpFraction;
				S.BossFraction = BossFrac;
				S.CueId = CueExecute;
				return S;
			}

			inline FStep DetonateStep(double At, double Radius, EStatus Status, double PerStack, bool bConsume, int Resource = 0, double GainPerStack = 0.0)
			{
				FStep S;
				S.Kind = EStep::Detonate;
				S.At = At;
				S.Radius = Radius;
				S.Status = Status;
				S.Damage = PerStack;
				S.Param2 = bConsume ? 1.0 : 0.0;
				S.Stat = Resource;
				S.Param = GainPerStack;
				S.CueId = CueExplosion;
				return S;
			}

			inline FStep ChainStep(double At, int Targets, double Reach, double Damage)
			{
				FStep S;
				S.Kind = EStep::Chain;
				S.At = At;
				S.Hits = Targets;
				S.Radius = Reach;
				S.Damage = Damage;
				S.CueId = CueLightning;
				return S;
			}

			inline FStep HookStep(double At, EHook Hook, double Range = 0.0)
			{
				FStep S;
				S.Kind = EStep::Hook;
				S.At = At;
				S.HookId = Hook;
				S.Range = Range;
				return S;
			}

			inline FStep CueStep(double At, int Cue, double Radius = 300.0, double Seconds = 0.6)
			{
				FStep S;
				S.Kind = EStep::Cue;
				S.At = At;
				S.CueId = Cue;
				S.Radius = Radius;
				S.Duration = Seconds;
				return S;
			}

			// ---- abilities
			inline FAbilityDef Def(const char* Id, const char* Name, ESlotId Slot, double Cooldown, double Lock, double Total, const char* Note, int Source = 2)
			{
				FAbilityDef D;
				D.Id = Id;
				D.Name = Name;
				D.Slot = Slot;
				D.Cooldown = Cooldown;
				D.LockSeconds = Lock;
				D.TotalSeconds = Total;
				D.Note = Note;
				D.Source = Source;
				return D;
			}

			inline FAbilityDef Basic(const char* Id, const char* Name, double Interval, int ComboLength, const char* Note, int Source = 2)
			{
				FAbilityDef D = Def(Id, Name, ESlotId::Basic, Interval, 0.0, 0.0, Note, Source);
				D.ComboLength = ComboLength;
				return D;
			}

			inline FPassiveDef Passive(const char* Id, const char* Name, ETrigger Trigger, const char* Note, int Source = 2)
			{
				FPassiveDef P;
				P.Id = Id;
				P.Name = Name;
				P.Trigger = Trigger;
				P.Note = Note;
				P.Source = Source;
				return P;
			}
		}
	}
}
