#include "BIBPlayerModel.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Tactics
	{
		namespace
		{
			constexpr double MaxJitterS = 0.5;
			constexpr double DamageEaseAt = 0.35;      // recent damage (health fractions) that takes one attacker token away

			// Fraction kept after Dt seconds of an exponential decay with the given half-life.
			double Keep(double Dt, double HalfLife)
			{
				return std::pow(0.5, std::max(0.0, Dt) / HalfLife);
			}

			double Clamp01(double Value)
			{
				return std::max(0.0, std::min(1.0, Value));
			}

			// Counter -> events per minute (a decayed count with half-life H holds r * H / ln 2 at a steady r events per second).
			double PerMinute(double Count)
			{
				return Count * 0.6931471805599453 / HabitHalfLife * 60.0;
			}
		}

		void FPlayerModel::Reset()
		{
			*this = FPlayerModel();
		}

		void FPlayerModel::Sample(const FPlayerSample& In)
		{
			const double Dt = std::max(0.0, std::min(In.Dt, 0.5));
			ClockSeconds += Dt;

			// Habits: count rising edges, decay everything.
			const double HabitKeep = Keep(Dt, HabitHalfLife);
			DodgeCount *= HabitKeep;
			BlockCount *= HabitKeep;
			ParryCount *= HabitKeep;
			if (In.bDodging && !bWasDodging)
			{
				DodgeCount += 1.0;
				if (DodgeStored == DodgeHistory)
				{
					for (int Index = 1; Index < DodgeHistory; ++Index)
					{
						DodgeTimes[Index - 1] = DodgeTimes[Index];
					}
					--DodgeStored;
				}
				DodgeTimes[DodgeStored++] = ClockSeconds;
			}
			if (In.bBlocking && !bWasBlocking)
			{
				BlockCount += 1.0;
			}
			if (In.bParrying && !bWasParrying)
			{
				ParryCount += 1.0;
			}
			bWasDodging = In.bDodging;
			bWasBlocking = In.bBlocking;
			bWasParrying = In.bParrying;

			// Damage taken: only drops count, and not a jump caused by switching operators.
			Damage *= Keep(Dt, DamageHalfLife);
			if (bHaveHealth && !In.bDiscontinuity && In.HealthFraction < LastHealth)
			{
				Damage += LastHealth - In.HealthFraction;
			}
			LastHealth = In.HealthFraction;
			bHaveHealth = true;

			// Casting: a basic attack or dodge is short; a cast held past ChannelMinSeconds is an ultimate or a channel.
			CastSeconds = (In.bCasting && !In.bDodging) ? CastSeconds + Dt : 0.0;

			// Movement and distance only mean something while an angel is alive.
			if (In.NearestDistance < 0.0)
			{
				return;
			}
			Observed += Dt;
			const double Blend = 1.0 - Keep(Dt, HabitHalfLife);
			const bool bNear = In.NearestDistance <= EngageRangeCm;
			const double RetreatNow = (bNear && In.RetreatSpeed > RetreatSpeedCmS) ? 1.0 : 0.0;
			const double StillNow = In.Speed < StillSpeedCmS ? 1.0 : 0.0;
			Kite += (RetreatNow - Kite) * Blend;
			Still += (StillNow - Still) * Blend;
			if (bNear)
			{
				Engagement = Engagement < 0.0 ? In.NearestDistance : Engagement + (In.NearestDistance - Engagement) * Blend;
			}
		}

		double FPlayerModel::DodgePerMinute() const { return PerMinute(DodgeCount); }
		double FPlayerModel::BlockPerMinute() const { return PerMinute(BlockCount); }
		double FPlayerModel::ParryPerMinute() const { return PerMinute(ParryCount); }

		double FPlayerModel::DodgeRhythm() const
		{
			if (DodgeStored < 4)
			{
				return 0.0;
			}
			const int Intervals = DodgeStored - 1;
			double Mean = 0.0;
			for (int Index = 0; Index < Intervals; ++Index)
			{
				Mean += DodgeTimes[Index + 1] - DodgeTimes[Index];
			}
			Mean /= Intervals;
			if (Mean < 0.35 || Mean > 4.0)
			{
				return 0.0;
			}
			double Variance = 0.0;
			for (int Index = 0; Index < Intervals; ++Index)
			{
				const double Delta = (DodgeTimes[Index + 1] - DodgeTimes[Index]) - Mean;
				Variance += Delta * Delta;
			}
			const double Deviation = std::sqrt(Variance / Intervals);
			if (ClockSeconds - DodgeTimes[DodgeStored - 1] > Mean * 2.5 + 1.0)
			{
				return 0.0;   // the rhythm stopped
			}
			return Clamp01(1.0 - (Deviation / Mean) / 0.4);
		}

		double FPlayerModel::Strength() const
		{
			const double Ramp = Clamp01(Observed / StrengthRampSeconds);
			const double Ease = 1.0 - std::min(0.8, Damage * 1.5);   // a commander who is being hurt is not pressed harder
			return StrengthCap * Ramp * Ease;
		}

		FAdvice FPlayerModel::Advise() const
		{
			FAdvice Out;
			const double S = Strength();
			const double Rhythm = DodgeRhythm();
			const double DodgeHabit = Clamp01(DodgePerMinute() / 12.0);
			const double BlockHabit = Clamp01((BlockPerMinute() + ParryPerMinute()) / 12.0);
			Out.Strength = S;
			Out.TimingJitterS = std::min(MaxJitterS, S * (0.25 + 0.75 * Rhythm) * 1.2);
			Out.FeintBias = S * DodgeHabit * (0.4 + 0.6 * Rhythm);
			Out.Intercept = S * Kite;
			Out.GapClose = S * Kite;
			Out.MageLead = S * (1.0 - Still);
			Out.MageCutOff = S * std::max(DodgeHabit, Kite);
			Out.StrafeBias = S * (0.5 + 0.5 * std::max(Still, BlockHabit));
			Out.bKiting = Kite > 0.4;
			Out.bChannel = ChannelActive();
			if (Out.bChannel)
			{
				// Give way, but only for a few seconds of one channel; a long channel does not freeze the crowd.
				const double Fade = Clamp01(1.0 - (CastSeconds - ChannelMinSeconds) / BackOffMaxSeconds);
				Out.BackOff = Fade * (0.3 + 0.7 * S / StrengthCap);
			}
			Out.TokenDelta = Damage > DamageEaseAt ? -1 : 0;
			return Out;
		}
	}
}
