#include "BIBSteer.h"

#include <cmath>

namespace BIB
{
	namespace Tactics
	{
		namespace
		{
			constexpr double Pi = 3.14159265358979323846;
			constexpr double Whisker[WhiskerCount] = {-Pi * 0.5, -Pi * 0.25, 0.0, Pi * 0.25, Pi * 0.5};
		}

		double WhiskerAngle(int Index)
		{
			return Whisker[Index];
		}

		FSteerResult Steer(const FWhiskers& Whiskers, FSteerState& State, double Now)
		{
			FSteerResult Out;
			if (Now < State.SlideUntil)
			{
				// Sliding along an obstacle: take the slide side while it is open, otherwise the other one.
				const int Index = State.SlideSide > 0 ? 4 : 0;
				const int Other = 4 - Index;
				if (Whiskers.Clear[Index] < 0.3 && Whiskers.Clear[Other] > Whiskers.Clear[Index])
				{
					State.SlideSide = -State.SlideSide;
				}
				Out.Angle = Whisker[State.SlideSide > 0 ? 4 : 0];
				State.LastSide = State.SlideSide;
				return Out;
			}
			if (Whiskers.Clear[2] >= OpenAhead)
			{
				return Out;
			}
			int Best = 2;
			double BestScore = Whiskers.Clear[2];
			for (int Index = 0; Index < WhiskerCount; ++Index)
			{
				if (Index == 2)
				{
					continue;
				}
				const double Side = Whisker[Index] > 0.0 ? 1.0 : -1.0;
				const double Keep = (State.LastSide != 0 && static_cast<int>(Side) == State.LastSide) ? 0.15 : 0.0;
				const double Score = Whiskers.Clear[Index] * (0.5 + 0.5 * std::cos(Whisker[Index])) + Keep;
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = Index;
				}
			}
			Out.Angle = Whisker[Best];
			if (Best != 2)
			{
				State.LastSide = Whisker[Best] > 0.0 ? 1 : -1;
			}
			double Freest = 0.0;
			for (int Index = 0; Index < WhiskerCount; ++Index)
			{
				Freest = Whiskers.Clear[Index] > Freest ? Whiskers.Clear[Index] : Freest;
			}
			Out.bBlocked = Freest < 0.2;
			Out.Speed = Out.bBlocked ? 0.4 : 1.0;
			return Out;
		}

		void NoteProgress(FSteerState& State, double Now, double X, double Y, bool bWants)
		{
			if (!bWants)
			{
				State.WindowStart = -1.0;
				State.StuckCount = 0;
				return;
			}
			if (State.WindowStart < 0.0)
			{
				State.WindowStart = Now;
				State.WindowX = X;
				State.WindowY = Y;
				return;
			}
			if (Now - State.WindowStart < StuckWindowS)
			{
				return;
			}
			const double Dx = X - State.WindowX;
			const double Dy = Y - State.WindowY;
			if (std::sqrt(Dx * Dx + Dy * Dy) < StuckMinMoveCm)
			{
				++State.StuckCount;
				// Alternate sides on repeated failures so a corner is not tried the same way forever.
				State.SlideSide = (State.StuckCount % 2 == 1) ? (State.LastSide != 0 ? -State.LastSide : 1) : -State.SlideSide;
				State.SlideUntil = Now + SlideSeconds;
			}
			else
			{
				State.StuckCount = 0;
			}
			State.WindowStart = Now;
			State.WindowX = X;
			State.WindowY = Y;
		}
	}
}
