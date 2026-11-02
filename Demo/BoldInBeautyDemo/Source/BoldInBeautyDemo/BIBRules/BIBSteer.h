// Boldinbeauty demo — obstacle-aware steering for the angels (behaviour layer, 2026-10-12). Engine-independent decision; the
// Unreal glue (BIBGame/BIBTactics.*) casts the five whiskers with sphere sweeps and moves the character.
//
// Five whiskers fan out from the desired heading (-90, -45, 0, +45, +90 degrees; the caller decides which way positive turns). Each reports how much of the look-ahead
// is free (1 = open, 0 = wall at once). Steer() keeps the heading when it is open, otherwise turns toward the freest side
// that still points roughly the right way, and remembers the side so the angel does not flicker between two walls. Stuck
// detection watches real progress: an angel that wants to move but has not moved slides sideways along the obstacle for a
// moment. No navmesh and no plugin are needed for the greybox stage rooms.
// Every number here is PLACEHOLDER.
#pragma once

namespace BIB
{
	namespace Tactics
	{
		constexpr int WhiskerCount = 5;
		constexpr double LookAheadCm = 260.0;
		constexpr double OpenAhead = 0.85;      // forward whisker at least this free = keep going straight
		constexpr double StuckWindowS = 0.6;
		constexpr double StuckMinMoveCm = 40.0;
		constexpr double SlideSeconds = 1.0;

		struct FWhiskers
		{
			// Index 0..4 = WhiskerAngle(0..4) = -90, -45, 0, +45, +90 degrees from the desired heading.
			double Clear[WhiskerCount] = {1.0, 1.0, 1.0, 1.0, 1.0};
		};

		struct FSteerState
		{
			int LastSide = 0;            // -1 / +1: the way the last detour went
			double SlideUntil = -1.0;
			int SlideSide = 1;
			double WindowStart = -1.0;
			double WindowX = 0.0;
			double WindowY = 0.0;
			int StuckCount = 0;
		};

		struct FSteerResult
		{
			double Angle = 0.0;          // radians from the desired heading, same sign convention as WhiskerAngle
			double Speed = 1.0;          // 0..1 of the intended speed (an angel boxed in creeps)
			bool bBlocked = false;       // every whisker is nearly closed
		};

		/** Radians of whisker Index (0..WhiskerCount-1) from the desired heading. */
		double WhiskerAngle(int Index);

		/** Where to head, given the whiskers cast along the desired heading. */
		FSteerResult Steer(const FWhiskers& Whiskers, FSteerState& State, double Now);

		/** Call each frame the angel is asked to move (bWants); a window without progress starts a sideways slide. */
		void NoteProgress(FSteerState& State, double Now, double X, double Y, bool bWants);
	}
}
