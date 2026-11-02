#include "BIBAngelMind.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace BIB
{
	namespace Angel
	{
		namespace
		{
			constexpr double Ln2 = 0.69314718055994530942;
			double DecayRate() { return Ln2 / RateHalfLife; }

			// Rate (events or seconds per minute) at which a trait counts as full pressure. PLACEHOLDER.
			constexpr double FullRate[TraitCount] = {
				70.0,  // MeleeSpam: more than one basic attack a second
				10.0,  // AbilityUse
				4.0,   // AreaUse
				25.0,  // RangedPoke
				8.0,   // Dodging
				6.0,   // Parrying
				18.0,  // Blocking (seconds held per minute)
				15.0,  // Kiting (seconds per minute)
				10.0,  // Jumping
				6.0    // Switching
			};

			struct FLink
			{
				ECounter Counter;
				ETrait Trait;
				double Weight;
			};
			// Which habits teach which counter.
			constexpr FLink Links[] = {
				{ECounter::Guard, ETrait::MeleeSpam, 1.0},
				{ECounter::Feint, ETrait::Dodging, 0.8},
				{ECounter::Feint, ETrait::Parrying, 0.6},
				{ECounter::Unblockable, ETrait::Blocking, 0.9},
				{ECounter::Unblockable, ETrait::Parrying, 0.5},
				{ECounter::Blink, ETrait::Kiting, 0.8},
				{ECounter::Blink, ETrait::RangedPoke, 0.6},
				{ECounter::Scatter, ETrait::AreaUse, 1.0},
				{ECounter::MirrorNova, ETrait::AreaUse, 0.7},
				{ECounter::MirrorNova, ETrait::AbilityUse, 0.4},
				{ECounter::MirrorVolley, ETrait::RangedPoke, 0.9},
				{ECounter::MirrorDash, ETrait::Dodging, 0.7},
				{ECounter::AntiAir, ETrait::Jumping, 1.0},
				{ECounter::Ambush, ETrait::Switching, 1.0},
			};
		}

		const char* TraitName(ETrait Trait)
		{
			static const char* Names[TraitCount] = {"Basic-attack spam", "Abilities", "Area attacks", "Ranged poking", "Dodging", "Parrying",
				"Blocking", "Kiting", "Jumping", "Switching"};
			const int Index = static_cast<int>(Trait);
			return Index >= 0 && Index < TraitCount ? Names[Index] : "?";
		}

		const char* CounterName(ECounter Counter)
		{
			static const char* Names[CounterCount] = {"Guard Counter", "Feint", "Unblockable", "Blink", "Scatter", "Mirror Nova", "Mirror Volley",
				"Mirror Dash", "Anti-Air", "Ambush"};
			const int Index = static_cast<int>(Counter);
			return Index >= 0 && Index < CounterCount ? Names[Index] : "?";
		}

		const char* CounterBlurb(ECounter Counter)
		{
			switch (Counter)
			{
			case ECounter::Guard: return "They read your basic attacks: front hits are guarded and riposted. Use moves or hit their backs.";
			case ECounter::Feint: return "They saw you dodge: strikes are delayed or doubled. Wait for the real flash.";
			case ECounter::Unblockable: return "They saw you block: rose-flashing strikes ignore block and parry. Dodge them.";
			case ECounter::Blink: return "They saw you keep distance: they blink in behind you.";
			case ECounter::Scatter: return "They saw your area attacks: they spread out.";
			case ECounter::MirrorNova: return "They copied your area attack.";
			case ECounter::MirrorVolley: return "They copied your ranged attacks.";
			case ECounter::MirrorDash: return "They copied your dash.";
			case ECounter::AntiAir: return "They saw you jump: airtime is punished.";
			case ECounter::Ambush: return "They saw you switch: whoever comes in is focused.";
			default: return "";
			}
		}

		const char* MoveName(EMove Move)
		{
			static const char* Names[static_cast<int>(EMove::Count)] = {"Strike", "Guard", "Feint", "Unblockable", "Blink", "Nova", "Volley",
				"Dash strike", "Anti-air", "Flank", "Combo", "Sweep", "Charge", "Lunge"};
			const int Index = static_cast<int>(Move);
			return Index >= 0 && Index < static_cast<int>(EMove::Count) ? Names[Index] : "?";
		}

		void FMind::Observe(ETrait Trait, double Weight)
		{
			const int Index = static_cast<int>(Trait);
			if (Index < 0 || Index >= TraitCount || Weight <= 0.0)
			{
				return;
			}
			// Steady state: an event every s seconds gives a rate of 60 / s per minute.
			Rates[Index] += Weight * 60.0 * DecayRate();
		}

		double FMind::Pressure(ECounter Counter) const
		{
			double Total = 0.0;
			for (const FLink& Link : Links)
			{
				if (Link.Counter == Counter)
				{
					const int Trait = static_cast<int>(Link.Trait);
					Total += Link.Weight * std::min(2.0, Rates[Trait] / FullRate[Trait]);
				}
			}
			return std::min(2.0, Total);
		}

		void FMind::Tick(double DeltaSeconds, bool bBossPresent)
		{
			if (DeltaSeconds <= 0.0)
			{
				return;
			}
			const double Keep = std::exp(-DecayRate() * DeltaSeconds);
			for (double& Rate : Rates)
			{
				Rate *= Keep;
			}
			const double Learn = LearnPerSecond * LearnMult * (bBossPresent ? BossLearnMult : 1.0) * DeltaSeconds;
			for (int Index = 0; Index < CounterCount; ++Index)
			{
				const double Push = Pressure(static_cast<ECounter>(Index));
				double& Value = Skill[Index];
				if (Push > 0.2)
				{
					Value = std::min(1.0, Value + Learn * (Push - 0.2));
				}
				else
				{
					Value = std::max(0.0, Value - ForgetPerSecond * DeltaSeconds);
				}
				if (Value >= KnownAt && !Announced[Index])
				{
					Announced[Index] = true;
					Fresh.push_back(static_cast<ECounter>(Index));
				}
				else if (Value < KnownAt * 0.5)
				{
					Announced[Index] = false;
				}
			}
		}

		void FMind::NewStage()
		{
			for (double& Rate : Rates)
			{
				Rate = 0.0;
			}
			for (int Index = 0; Index < CounterCount; ++Index)
			{
				Skill[Index] *= StageCarryOver;
				Announced[Index] = Skill[Index] >= KnownAt;
			}
			Fresh.clear();
		}

		void FMind::Reset()
		{
			*this = FMind();
		}

		int FMind::KnownCount() const
		{
			int Count = 0;
			for (int Index = 0; Index < CounterCount; ++Index)
			{
				Count += Skill[Index] >= KnownAt ? 1 : 0;
			}
			return Count;
		}

		double FMind::Adaptation() const
		{
			double Sorted[CounterCount];
			std::copy(std::begin(Skill), std::end(Skill), Sorted);
			std::sort(std::begin(Sorted), std::end(Sorted), [](double A, double B) { return A > B; });
			return (Sorted[0] + Sorted[1] + Sorted[2]) / 3.0;
		}

		std::vector<ECounter> FMind::TakeNewlyLearned()
		{
			std::vector<ECounter> Out;
			Out.swap(Fresh);
			return Out;
		}

		std::string FMind::Describe() const
		{
			int Order[CounterCount];
			for (int Index = 0; Index < CounterCount; ++Index)
			{
				Order[Index] = Index;
			}
			std::sort(std::begin(Order), std::end(Order), [this](int A, int B) { return Skill[A] > Skill[B]; });
			std::string Out;
			for (int Index : Order)
			{
				if (Skill[Index] < KnownAt)
				{
					break;
				}
				char Buffer[64];
				std::snprintf(Buffer, sizeof(Buffer), "%s%s %d%%", Out.empty() ? "" : " | ", CounterName(static_cast<ECounter>(Index)),
					static_cast<int>(std::lround(Skill[Index] * 100.0)));
				Out += Buffer;
			}
			return Out;
		}

		void FMind::SetExpertise(ECounter Counter, double Value)
		{
			const int Index = static_cast<int>(Counter);
			if (Index >= 0 && Index < CounterCount)
			{
				Skill[Index] = std::clamp(Value, 0.0, 1.0);
			}
		}

		FDecision Decide(const FMind& Mind, ERole Role, const FSituation& S, double Roll)
		{
			FDecision Out;
			struct FOption
			{
				EMove Move;
				double Weight;
			};
			FOption Options[static_cast<int>(EMove::Count)];
			int Count = 0;
			auto Add = [&Options, &Count](EMove Move, double Weight)
			{
				if (Weight > 0.0)
				{
					Options[Count++] = {Move, Weight};
				}
			};
			auto Skill = [&Mind](ECounter Counter) { return Mind.Knows(Counter) ? Mind.Expertise(Counter) : 0.0; };
			const bool bBoss = Role == ERole::MiniBoss || Role == ERole::Boss;
			const bool bMelee = Role == ERole::Melee || Role == ERole::Brute || Role == ERole::Assassin || bBoss;
			const bool bInReach = S.Distance <= S.AttackRange;

			// The role's own attack is always an option.
			Add(EMove::Strike, 1.0);
			// Guard: only against a commander attacking from the front, close by.
			if (bMelee && S.bPlayerAttacking && S.bFacingPlayer && S.Distance < 450.0)
			{
				Add(EMove::Guard, 2.2 * Skill(ECounter::Guard));
			}
			if (bInReach || !bMelee)
			{
				Add(EMove::Feint, 1.4 * Skill(ECounter::Feint));
				Add(EMove::Unblockable, (S.bPlayerBlocking ? 2.5 : 1.0) * Skill(ECounter::Unblockable));
			}
			if (S.Distance > 500.0)
			{
				Add(EMove::Blink, (bMelee ? 1.8 : 0.6) * Skill(ECounter::Blink));
				Add(EMove::DashStrike, (bMelee ? 1.2 : 0.3) * Skill(ECounter::MirrorDash));
			}
			Add(EMove::Nova, (S.Distance < 600.0 ? 1.4 : 0.2) * (Role == ERole::Mage || bBoss ? 1.5 : 1.0) * Skill(ECounter::MirrorNova));
			Add(EMove::Volley, (S.Distance > 400.0 ? 1.2 : 0.3) * (Role == ERole::Ranged || bBoss ? 1.6 : 0.6) * Skill(ECounter::MirrorVolley));
			if (S.bPlayerAirborne)
			{
				Add(EMove::AntiAir, 3.0 * Skill(ECounter::AntiAir));
			}
			if (S.bPlayerJustSwitched && bInReach)
			{
				// Ambush: the incoming operator is struck at once (a fast strike).
				Add(EMove::Strike, 2.0 * Skill(ECounter::Ambush));
			}
			if (bMelee && !bBoss && S.bFacingPlayer && S.Distance < 700.0)
			{
				// Smart angels do not walk into the commander's face: some circle to the side or back first.
				Add(EMove::Flank, 0.25 + 0.5 * Mind.Adaptation());
			}

			double Total = 0.0;
			for (int Index = 0; Index < Count; ++Index)
			{
				Total += Options[Index].Weight;
			}
			double Pick = std::clamp(Roll, 0.0, 0.999999) * Total;
			Out.Move = Options[0].Move;
			for (int Index = 0; Index < Count; ++Index)
			{
				if (Pick < Options[Index].Weight)
				{
					Out.Move = Options[Index].Move;
					break;
				}
				Pick -= Options[Index].Weight;
			}
			switch (Out.Move)
			{
			case EMove::Feint: Out.WindupMult = 1.8; break;
			case EMove::Unblockable: Out.WindupMult = 1.3; Out.bUnblockable = true; break;
			case EMove::Nova: Out.WindupMult = 1.6; break;
			case EMove::AntiAir: Out.WindupMult = 0.5; break;
			case EMove::Strike: Out.WindupMult = S.bPlayerJustSwitched && Mind.Knows(ECounter::Ambush) ? 0.6 : 1.0; break;
			default: break;
			}
			Out.bSpread = Mind.Knows(ECounter::Scatter) && S.NearbyAllies >= 2;
			return Out;
		}

		int MaxAttackers(int AliveAngels, double Adaptation)
		{
			// Zenless-style crowds: a few angels commit at once, the rest pressure from the edges. Smarter minds coordinate more.
			const int Cap = 2 + static_cast<int>(std::floor(Adaptation * 2.0 + 0.0001));
			return std::max(1, std::min(AliveAngels, Cap));
		}
	}
}
