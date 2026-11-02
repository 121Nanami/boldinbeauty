// Boldinbeauty demo — the new-account tutorial (owner request 2026-10-04). A small state machine over the hub pages:
//
//   1  open the Operator tab          2  tap Luna              3  level Luna up once
//   4  open the Team tab (squad empty) 5  tap an empty slot: only Luna can be added
//   6  open the Summon tab            7  forced summon: Kuu    (Kuu joins the squad)
//   8  open the Campaign tab          9  play Chapter 1 level 1 -> Plack is unlocked for free (FProfile::ApplyStageRewards)
//
// While a step is active the hub only allows the actions that step asks for (everything else is greyed out and refused with
// a hint), highlights the target tile or tab, and shows the instruction as the first line of every page. The step lives in
// the profile (saved), so quitting half-way resumes at the same step. Engine-independent.
#pragma once

#include <string>

#include "BIBCore.h"
#include "BIBPages.h"

namespace BIB
{
	namespace Tutorial
	{
		constexpr int StepCount = static_cast<int>(ETutorialStep::Complete); // steps the player actually sees (9)

		struct FCoach
		{
			int Number = 0;          // 1..StepCount
			std::string Text;        // the instruction
			int TargetTab = -1;      // bottom tab to highlight, -1 = none
		};

		bool Active(const FProfile& Profile);
		FCoach Coach(const FProfile& Profile);

		// Is this page action (the same ids as Pages::Perform) allowed at the current step? Always true once the tutorial is over.
		bool Allows(const FProfile& Profile, const std::string& ActionId);

		// Moves to the next step when the player has done what the current one asks (reads the profile and the hub state).
		// Returns true when the step changed. Safe to call repeatedly.
		bool Advance(FProfile& Profile, const Pages::FUiState& State);

		// Greys out the actions the step does not allow, highlights the ones it asks for, and puts the instruction first.
		void Decorate(const FProfile& Profile, Pages::FPage& Page);
	}
}
