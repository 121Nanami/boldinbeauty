// Boldinbeauty demo — new-account tutorial (see BIBTutorial.h).
#include "BIBTutorial.h"
#include "BIBConfig.h"
#include "BIBTester.h"

namespace BIB
{
	namespace Tutorial
	{
		namespace
		{
			ETutorialStep StepOf(const FProfile& Profile)
			{
				const int Value = Profile.TutorialStep;
				return Value >= 0 && Value < static_cast<int>(ETutorialStep::Count) ? static_cast<ETutorialStep>(Value) : ETutorialStep::Complete;
			}

			void SetStep(FProfile& Profile, ETutorialStep Step)
			{
				Profile.TutorialStep = static_cast<int>(Step);
			}

			std::string TabAction(int Tab)
			{
				return "tab:" + std::to_string(Tab);
			}

			// Moves the squad's members to the front, keeping their order: [-, Luna, -] becomes [Luna, -, -].
			void CompactSquad(FProfile& Profile)
			{
				int Members[3] = {-1, -1, -1};
				int Count = 0;
				for (int Slot = 0; Slot < 3; ++Slot)
				{
					if (Profile.Team[Slot] >= 0)
					{
						Members[Count++] = Profile.Team[Slot];
					}
				}
				for (int Slot = 0; Slot < 3; ++Slot)
				{
					Profile.Team[Slot] = Members[Slot];
				}
			}

			// "Navigation" actions are allowed but never highlighted as the thing to do.
			bool IsNavigation(const std::string& ActionId)
			{
				return ActionId == "back" || ActionId == "sum:done" || ActionId == "tst:tut:skip" || ActionId.rfind("tab:", 0) == 0;
			}
		}

		bool Active(const FProfile& Profile)
		{
			return StepOf(Profile) != ETutorialStep::Complete;
		}

		FCoach Coach(const FProfile& Profile)
		{
			FCoach Coach;
			const ETutorialStep Step = StepOf(Profile);
			Coach.Number = static_cast<int>(Step) + 1;
			switch (Step)
			{
			case ETutorialStep::OpenOperators:
				Coach.Text = "Welcome, Commander. Luna is your first operator. Open the Operator tab.";
				Coach.TargetTab = Pages::TabOperator;
				break;
			case ETutorialStep::SelectLuna:
				Coach.Text = "Tap Luna to see her details.";
				break;
			case ETutorialStep::LevelUpLuna:
				Coach.Text = "Levelling up makes an operator stronger and costs Gold and Bytes - this first one is free. Tap Level up.";
				break;
			case ETutorialStep::OpenTeam:
				Coach.Text = "Next, build your squad. Open the Team tab.";
				Coach.TargetTab = Pages::TabTeam;
				break;
			case ETutorialStep::AddLuna:
				Coach.Text = "Your squad is empty. Tap an empty slot to add an operator - Luna is the only one you own, so she joins.";
				break;
			case ETutorialStep::OpenSummon:
				Coach.Text = "More operators come from summoning. Open the Summon tab.";
				Coach.TargetTab = Pages::TabSummon;
				break;
			case ETutorialStep::SummonKuu:
				Coach.Text = "Your first summon is guaranteed: Kuu. Tap Tutorial x1.";
				break;
			case ETutorialStep::OpenCampaign:
				Coach.Text = "Kuu joined your squad. Now play your first level. Open the Campaign tab.";
				Coach.TargetTab = Pages::TabCampaign;
				break;
			case ETutorialStep::PlayLevel1:
				Coach.Text = "Open Chapter 1 and start Level 1. Clear it to unlock Plack for free.";
				break;
			default:
				break;
			}
			return Coach;
		}

		bool Allows(const FProfile& Profile, const std::string& ActionId)
		{
			const ETutorialStep Step = StepOf(Profile);
			if (Step == ETutorialStep::Complete)
			{
				return true;
			}
			if (ActionId == "back" || (ActionId == "tst:tut:skip" && Tester::Enabled()))
			{
				return true; // a tester can always leave the tutorial
			}
			switch (Step)
			{
			case ETutorialStep::OpenOperators:
				return ActionId == TabAction(Pages::TabOperator);
			case ETutorialStep::SelectLuna:
				return ActionId == TabAction(Pages::TabOperator) || ActionId == "op:sel:" + std::to_string(FindOperator("Luna"));
			case ETutorialStep::LevelUpLuna:
				return ActionId == TabAction(Pages::TabOperator) || ActionId == "op:lvl" || ActionId == "op:sel:" + std::to_string(FindOperator("Luna"));
			case ETutorialStep::OpenTeam:
				return ActionId == TabAction(Pages::TabTeam);
			case ETutorialStep::AddLuna:
				return ActionId == TabAction(Pages::TabTeam) || ActionId.rfind("team:cycle:", 0) == 0;
			case ETutorialStep::OpenSummon:
				return ActionId == TabAction(Pages::TabSummon);
			case ETutorialStep::SummonKuu:
				return ActionId == TabAction(Pages::TabSummon) || ActionId == "sum:tut";
			case ETutorialStep::OpenCampaign:
				return ActionId == TabAction(Pages::TabCampaign) || ActionId == "sum:done";
			case ETutorialStep::PlayLevel1:
				return ActionId == TabAction(Pages::TabCampaign) || ActionId == "camp:ch:1" ||
					ActionId == "camp:start:" + std::to_string(Config::TutorialPlayLevel);
			default:
				return true;
			}
		}

		bool Advance(FProfile& Profile, const Pages::FUiState& State)
		{
			bool bChanged = false;
			const int Luna = FindOperator("Luna");
			const int Kuu = FindOperator("Kuu");
			for (int Guard = 0; Guard < static_cast<int>(ETutorialStep::Count); ++Guard)
			{
				const ETutorialStep Step = StepOf(Profile);
				ETutorialStep Next = Step;
				switch (Step)
				{
				case ETutorialStep::OpenOperators:
					if (State.Tab == Pages::TabOperator)
					{
						Next = ETutorialStep::SelectLuna;
					}
					break;
				case ETutorialStep::SelectLuna:
					if (State.Tab == Pages::TabOperator && State.Page == "op" && State.SelOperator == Luna)
					{
						Next = ETutorialStep::LevelUpLuna;
					}
					break;
				case ETutorialStep::LevelUpLuna:
					if (Profile.Owns(Luna) && Profile.Operators[static_cast<size_t>(Luna)].Level >= 2)
					{
						Next = ETutorialStep::OpenTeam;
					}
					break;
				case ETutorialStep::OpenTeam:
					if (State.Tab == Pages::TabTeam)
					{
						Next = ETutorialStep::AddLuna;
					}
					break;
				case ETutorialStep::AddLuna:
					if (Profile.Team[0] == Luna || Profile.Team[1] == Luna || Profile.Team[2] == Luna)
					{
						CompactSquad(Profile); // whichever empty slot was tapped, the first operator leads
						Next = ETutorialStep::OpenSummon;
					}
					break;
				case ETutorialStep::OpenSummon:
					if (State.Tab == Pages::TabSummon)
					{
						Next = ETutorialStep::SummonKuu;
					}
					break;
				case ETutorialStep::SummonKuu:
					if (Profile.bTutorialSummonDone && Profile.Owns(Kuu))
					{
						Profile.AutoPlaceInTeam(Kuu); // the starter squad: Luna and Kuu
						Next = ETutorialStep::OpenCampaign;
					}
					break;
				case ETutorialStep::OpenCampaign:
					if (State.Tab == Pages::TabCampaign)
					{
						Next = ETutorialStep::PlayLevel1;
					}
					break;
				case ETutorialStep::PlayLevel1:
					// Normally finished by FProfile::ApplyStageRewards when the level is cleared. A profile that already
					// cleared the level some other way (an old save) finishes here, with Plack granted.
					if (Profile.StagesCleared >= Config::TutorialPlayLevel)
					{
						const int Plack = FindOperator("Plack");
						Profile.GrantOperator(Plack);
						Profile.AutoPlaceInTeam(Plack);
						Next = ETutorialStep::Complete;
					}
					break;
				default:
					break;
				}
				if (Next == Step)
				{
					break;
				}
				SetStep(Profile, Next);
				bChanged = true;
			}
			return bChanged;
		}

		void Decorate(const FProfile& Profile, Pages::FPage& Page)
		{
			if (!Active(Profile))
			{
				return;
			}
			const FCoach Hint = Coach(Profile);
			Page.Coach = "TUTORIAL " + std::to_string(Hint.Number) + "/" + std::to_string(StepCount) + " - " + Hint.Text;
			Page.Lines.insert(Page.Lines.begin(), Page.Coach);
			if (Tester::Enabled() && static_cast<int>(Page.Bar.size()) < Pages::MaxBar)
			{
				Page.Bar.push_back({"tst:tut:skip", "Skip tutorial", "tester", "next", true, false, false});
			}
			for (Pages::FPageAction& Action : Page.Grid)
			{
				const bool bAllowed = Allows(Profile, Action.Id);
				Action.bHighlight = bAllowed && !IsNavigation(Action.Id);
				Action.bEnabled = Action.bEnabled && bAllowed;
			}
			for (Pages::FPageAction& Action : Page.Bar)
			{
				const bool bAllowed = Allows(Profile, Action.Id);
				Action.bHighlight = bAllowed && !IsNavigation(Action.Id);
				Action.bEnabled = Action.bEnabled && bAllowed;
			}
			for (int Tab = 0; Tab < Pages::TabCount; ++Tab)
			{
				Page.NavLocked[Tab] = !Allows(Profile, TabAction(Tab));
			}
			Page.NavHighlight = Hint.TargetTab;
		}
	}
}
