// Tests for the Zenless-style combat rules (owner request 2026-10-09, Notion "Dev notes"): Daze / stun / flinch / combo
// (BIBRules/BIBStagger.h) and the angels' adaptive mind (BIBRules/BIBAngelMind.h).
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBAngelMind.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBStagger.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
	int GFailures = 0;
	int GChecks = 0;

	void Expect(bool bCondition, const std::string& What)
	{
		++GChecks;
		if (!bCondition)
		{
			++GFailures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	namespace S = BIB::Stagger;
	namespace A = BIB::Angel;

	void TestDaze()
	{
		// Bigger hits fill more, sub-linearly; bonuses add on top.
		const double One = S::DazeForHit(100.0, 100.0, S::EBonus::None);
		const double Five = S::DazeForHit(500.0, 100.0, S::EBonus::None);
		Expect(std::abs(One - S::DazePerAttack) < 1e-9, "a hit for the attack adds DazePerAttack");
		Expect(Five > One * 2.0 && Five < One * 5.0, "a 5x hit adds more Daze, but less than 5x");
		Expect(S::DazeForHit(100.0, 100.0, S::EBonus::ParryCounter) > One + 30.0, "parry counters add a big bonus");
		Expect(S::DazeForHit(0.0, 100.0, S::EBonus::ParryCounter) == 0.0, "no damage, no Daze");
		Expect(S::DazeForHit(1.0, 100.0, S::EBonus::None) >= S::DazeMinPerHit, "tiny hits still add the minimum");
		Expect(S::DazeForHit(1e9, 100.0, S::EBonus::None) <= S::DazeMaxPerHit, "huge hits are capped");

		S::FMeter Meter;
		Meter.Reset(S::DazeMaxLight);
		double Now = 0.0;
		int Hits = 0;
		bool bStunned = false;
		while (!bStunned && Hits < 100)
		{
			bStunned = Meter.Add(One, Now, S::StunSecondsLight);
			Now += 0.4;
			++Hits;
		}
		Expect(bStunned, "basic hits eventually stun a grunt");
		Expect(Hits >= 9 && Hits <= 14, "a grunt is stunned by about a dozen basic hits");
		Expect(Meter.Stunned(Now) && Meter.ChainOpen(Now), "stun opens the chain window");
		Expect(std::abs(S::DamageTakenMult(Meter, Now) - S::StunDamageMult) < 1e-9, "stunned enemies take extra damage");
		Expect(!Meter.Add(50.0, Now, 3.0), "no Daze while stunned");
		const double StunEnd = Meter.StunnedUntil;
		Meter.Tick(StunEnd + 0.01, 0.01);
		Expect(!Meter.Stunned(StunEnd + 0.01) && Meter.Daze == 0.0, "the meter empties when the stun ends");
		Expect(!Meter.Add(50.0, StunEnd + 0.5, 3.0) && Meter.Daze == 0.0, "immune for a moment after a stun");
		Expect(S::DamageTakenMult(Meter, StunEnd + 0.5) == 1.0, "normal damage after the stun");

		// Drain after a pause.
		S::FMeter Drain;
		Drain.Reset(100.0);
		Drain.Add(50.0, 0.0, 3.0);
		Drain.Tick(1.0, 1.0);
		Expect(Drain.Daze == 50.0, "no drain right after a hit");
		for (int Step = 0; Step < 100; ++Step)
		{
			Drain.Tick(4.0 + Step * 0.1, 0.1);
		}
		Expect(Drain.Daze < 50.0 && Drain.Daze >= 0.0, "the meter drains without hits");

		// Bosses take far longer and stun longer.
		Expect(S::DazeMax(S::EWeight::Heavy, true) > S::DazeMax(S::EWeight::Heavy, false), "bosses have more Daze than mini bosses");
		Expect(S::DazeMax(S::EWeight::Heavy, false) > S::DazeMax(S::EWeight::Medium, false), "mini bosses more than brutes");
		Expect(S::StunSeconds(S::EWeight::Heavy, true) > S::StunSeconds(S::EWeight::Light, false), "bosses stay stunned longer");
	}

	void TestFlinch()
	{
		S::FMeter Meter;
		Meter.Reset(100.0);
		Expect(Meter.TryFlinch(S::EWeight::Light, 1.0, 0.0), "light enemies flinch on a basic hit");
		Expect(!Meter.TryFlinch(S::EWeight::Medium, 1.0, 0.1), "brutes ignore light hits");
		Expect(Meter.TryFlinch(S::EWeight::Medium, 3.0, 0.2), "brutes flinch on heavy hits");
		Expect(!Meter.TryFlinch(S::EWeight::Heavy, 50.0, 0.3), "bosses never flinch");
		// Poise: one button cannot lock a light enemy forever.
		S::FMeter Lock;
		Lock.Reset(100.0);
		int Flinches = 0;
		for (int Hit = 0; Hit < 12; ++Hit)
		{
			Flinches += Lock.TryFlinch(S::EWeight::Light, 1.0, Hit * 0.2) ? 1 : 0;
		}
		Expect(Flinches == S::FlinchesBeforePoise, "poise breaks a stun lock after FlinchesBeforePoise flinches");
		Expect(Flinches < 12, "not every hit of a long string flinches");
	}

	void TestCombo()
	{
		S::FCombo Combo;
		Expect(Combo.Advance(0.0) == 0, "first hit is step 0");
		Expect(Combo.Advance(0.5) == 1, "second hit continues");
		Expect(Combo.Advance(1.0) == 2, "third");
		Expect(Combo.Advance(1.5) == 3 && Combo.IsFinisher(), "fourth is the finisher");
		Expect(Combo.Advance(2.0) == 0, "after the finisher the string restarts");
		Expect(Combo.Advance(2.0 + S::ComboWindow + 0.1) == 0, "a pause restarts the string");
		double Sum = 0.0;
		for (int Step = 0; Step < S::ComboLength; ++Step)
		{
			Sum += S::ComboStepMult(Step);
		}
		Expect(Sum / S::ComboLength > 1.0 && Sum / S::ComboLength < 1.1, "a full string averages close to the old damage");
		Expect(S::ComboStepMult(3) > S::ComboStepMult(0), "the finisher hits hardest");
	}

	void Spam(A::FMind& Mind, A::ETrait Trait, double PerSecond, double Seconds, bool bBoss = false)
	{
		const double Step = 0.1;
		double Carry = 0.0;
		for (double T = 0.0; T < Seconds; T += Step)
		{
			Carry += PerSecond * Step;
			while (Carry >= 1.0)
			{
				Mind.Observe(Trait);
				Carry -= 1.0;
			}
			Mind.Tick(Step, bBoss);
		}
	}

	void TestMind()
	{
		A::FMind Mind;
		Expect(Mind.KnownCount() == 0 && Mind.Adaptation() == 0.0, "a fresh mind knows nothing");
		Spam(Mind, A::ETrait::MeleeSpam, 2.0, 30.0);
		Expect(Mind.Rate(A::ETrait::MeleeSpam) > 60.0 && Mind.Rate(A::ETrait::MeleeSpam) < 140.0, "rate tracks events per minute");
		Expect(Mind.Knows(A::ECounter::Guard), "30 s of LMB spam teaches the Guard counter");
		Expect(!Mind.Knows(A::ECounter::AntiAir) && !Mind.Knows(A::ECounter::Feint), "nothing else is learned");
		std::vector<A::ECounter> Fresh = Mind.TakeNewlyLearned();
		Expect(Fresh.size() == 1 && Fresh[0] == A::ECounter::Guard, "the new counter is announced once");
		Expect(Mind.TakeNewlyLearned().empty(), "and only once");
		Expect(Mind.Describe().find("Guard Counter") != std::string::npos, "Describe lists it");

		// Switch playstyle: dodging. The guard fades slowly, feint and mirror dash are learned.
		Spam(Mind, A::ETrait::Dodging, 0.4, 60.0);
		Expect(Mind.Knows(A::ECounter::Feint), "dodging teaches feints");
		Expect(Mind.Knows(A::ECounter::MirrorDash), "and the angels copy the dash");
		const double GuardBefore = Mind.Expertise(A::ECounter::Guard);
		Spam(Mind, A::ETrait::Dodging, 0.4, 120.0);
		Expect(Mind.Expertise(A::ECounter::Guard) < GuardBefore, "unused counters are forgotten slowly");

		// Bosses learn faster.
		A::FMind Slow;
		A::FMind Fast;
		Spam(Slow, A::ETrait::Jumping, 0.3, 15.0, false);
		Spam(Fast, A::ETrait::Jumping, 0.3, 15.0, true);
		Expect(Fast.Expertise(A::ECounter::AntiAir) > Slow.Expertise(A::ECounter::AntiAir), "a boss in the room learns faster");

		// A new stage keeps part of what was learned.
		const double Before = Mind.Expertise(A::ECounter::Feint);
		Mind.NewStage();
		Expect(std::abs(Mind.Expertise(A::ECounter::Feint) - Before * A::StageCarryOver) < 1e-9, "a new stage keeps part of the expertise");
		Expect(Mind.Rate(A::ETrait::Dodging) == 0.0, "but forgets the rates");
		Mind.Reset();
		Expect(Mind.KnownCount() == 0, "reset");
	}

	void TestDecide()
	{
		A::FMind Naive;
		A::FSituation Close;
		Close.Distance = 120.0;
		Close.AttackRange = 170.0;
		Close.bPlayerAttacking = true;
		Close.bFacingPlayer = true;
		// A naive mind only strikes (or flanks).
		for (int Roll = 0; Roll < 20; ++Roll)
		{
			const A::FDecision D = A::Decide(Naive, A::ERole::Melee, Close, Roll / 20.0);
			Expect(D.Move == A::EMove::Strike || D.Move == A::EMove::Flank, "a naive angel strikes or flanks");
		}
		A::FMind Smart;
		Smart.SetExpertise(A::ECounter::Guard, 1.0);
		int Guards = 0;
		for (int Roll = 0; Roll < 100; ++Roll)
		{
			Guards += A::Decide(Smart, A::ERole::Melee, Close, Roll / 100.0).Move == A::EMove::Guard ? 1 : 0;
		}
		Expect(Guards > 30, "a mind that learned Guard guards against a spamming commander");
		A::FSituation Back = Close;
		Back.bFacingPlayer = false;
		Guards = 0;
		for (int Roll = 0; Roll < 100; ++Roll)
		{
			Guards += A::Decide(Smart, A::ERole::Melee, Back, Roll / 100.0).Move == A::EMove::Guard ? 1 : 0;
		}
		Expect(Guards == 0, "no guard when hit from behind");

		A::FMind Jumper;
		Jumper.SetExpertise(A::ECounter::AntiAir, 1.0);
		A::FSituation Air;
		Air.Distance = 800.0;
		Air.AttackRange = 1400.0;
		Air.bPlayerAirborne = true;
		int AntiAir = 0;
		for (int Roll = 0; Roll < 100; ++Roll)
		{
			const A::FDecision D = A::Decide(Jumper, A::ERole::Ranged, Air, Roll / 100.0);
			AntiAir += D.Move == A::EMove::AntiAir ? 1 : 0;
			if (D.Move == A::EMove::AntiAir)
			{
				Expect(D.WindupMult < 1.0, "anti-air shots are quick");
			}
		}
		Expect(AntiAir > 50, "airtime is punished by a mind that learned it");

		A::FMind Blocker;
		Blocker.SetExpertise(A::ECounter::Unblockable, 1.0);
		A::FSituation Blocking = Close;
		Blocking.bPlayerBlocking = true;
		Blocking.bPlayerAttacking = false;
		bool bSawUnblockable = false;
		for (int Roll = 0; Roll < 100; ++Roll)
		{
			const A::FDecision D = A::Decide(Blocker, A::ERole::Brute, Blocking, Roll / 100.0);
			bSawUnblockable |= D.Move == A::EMove::Unblockable && D.bUnblockable;
		}
		Expect(bSawUnblockable, "blocking is answered with unblockable strikes");

		A::FMind Spread;
		Spread.SetExpertise(A::ECounter::Scatter, 1.0);
		A::FSituation Crowd = Close;
		Crowd.NearbyAllies = 3;
		Expect(A::Decide(Spread, A::ERole::Melee, Crowd, 0.5).bSpread, "area attacks teach angels to spread out");
		Expect(!A::Decide(Naive, A::ERole::Melee, Crowd, 0.5).bSpread, "naive angels bunch up");

		Expect(A::MaxAttackers(10, 0.0) == 2, "two attackers at once by default");
		Expect(A::MaxAttackers(10, 1.0) == 4, "a fully adapted mind sends four");
		Expect(A::MaxAttackers(1, 1.0) == 1, "never more than are alive");
	}
}

int main()
{
	TestDaze();
	TestFlinch();
	TestCombo();
	TestMind();
	TestDecide();
	std::printf("angel tests: %d checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
