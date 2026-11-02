#include "BIBStagger.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Stagger
	{
		double BonusDaze(EBonus Bonus)
		{
			switch (Bonus)
			{
			case EBonus::ComboFinisher: return 14.0;
			case EBonus::DodgeCounter: return 28.0;
			case EBonus::ParryCounter: return 40.0;
			case EBonus::ChainAttack: return 30.0;
			case EBonus::SwitchStrike: return 12.0;
			default: return 0.0;
			}
		}

		double DazeMax(EWeight Weight, bool bBoss)
		{
			if (Weight == EWeight::Heavy)
			{
				return bBoss ? DazeMaxBoss : DazeMaxMiniBoss;
			}
			return Weight == EWeight::Medium ? DazeMaxBrute : DazeMaxLight;
		}

		double StunSeconds(EWeight Weight, bool bBoss)
		{
			(void)bBoss;
			switch (Weight)
			{
			case EWeight::Heavy: return StunSecondsBoss;
			case EWeight::Medium: return StunSecondsBrute;
			default: return StunSecondsLight;
			}
		}

		double DazeForHit(double Dealt, double Attack, EBonus Bonus)
		{
			if (Dealt <= 0.0)
			{
				return 0.0;
			}
			const double Ratio = Dealt / std::max(1.0, Attack);
			const double Base = std::clamp(DazePerAttack * std::pow(Ratio, DazeExponent), DazeMinPerHit, DazeMaxPerHit);
			return Base + BonusDaze(Bonus);
		}

		void FMeter::Reset(double InMax)
		{
			*this = FMeter();
			Max = InMax;
		}

		bool FMeter::Add(double Points, double Now, double StunFor)
		{
			LastHitAt = Now;
			if (Points <= 0.0 || Stunned(Now) || Now < ImmuneUntil)
			{
				return false;
			}
			Daze = std::min(Max, Daze + Points);
			if (Daze < Max)
			{
				return false;
			}
			StunnedUntil = Now + StunFor;
			ChainUntil = Now + ChainWindow;
			ImmuneUntil = StunnedUntil + ImmuneAfterStun;
			++StunCount;
			return true;
		}

		void FMeter::Tick(double Now, double DeltaSeconds)
		{
			if (Daze >= Max && !Stunned(Now))
			{
				// The stun just ended: the meter empties.
				Daze = 0.0;
			}
			if (!Stunned(Now) && Now - LastHitAt > DrainDelay && Daze > 0.0)
			{
				Daze = std::max(0.0, Daze - Max * DrainPerSecond * DeltaSeconds);
			}
		}

		double FMeter::Fraction(double Now) const
		{
			if (Stunned(Now))
			{
				return 1.0;
			}
			return Max > 0.0 ? std::clamp(Daze / Max, 0.0, 1.0) : 0.0;
		}

		bool FMeter::TryFlinch(EWeight Weight, double Ratio, double Now)
		{
			if (Weight == EWeight::Heavy || Stunned(Now) || Now < PoiseUntil)
			{
				return false;
			}
			if (Weight == EWeight::Medium && Ratio < HeavyHitRatio)
			{
				return false;
			}
			if (Now - FlinchWindowStart > PoiseWindow)
			{
				FlinchWindowStart = Now;
				FlinchesInWindow = 0;
			}
			++FlinchesInWindow;
			if (FlinchesInWindow > FlinchesBeforePoise)
			{
				PoiseUntil = Now + PoiseSeconds;
				FlinchesInWindow = 0;
				FlinchWindowStart = Now + PoiseSeconds;
				return false;
			}
			FlinchUntil = Now + FlinchSeconds;
			return true;
		}

		double DamageTakenMult(const FMeter& Meter, double Now)
		{
			return Meter.Stunned(Now) ? StunDamageMult : 1.0;
		}

		double ComboStepMult(int Step)
		{
			static constexpr double Mult[ComboLength] = {0.85, 0.95, 1.0, 1.45};
			return Step >= 0 && Step < ComboLength ? Mult[Step] : 1.0;
		}

		int FCombo::Advance(double Now, double Window)
		{
			if (Step < 0 || Step >= ComboLength - 1 || Now - LastAt > Window)
			{
				Step = 0;
			}
			else
			{
				++Step;
			}
			LastAt = Now;
			return Step;
		}
	}
}
