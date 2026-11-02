// Tests for the angels' behaviour layer (2026-10-12): the rolling model of the commander (BIBRules/BIBPlayerModel.h), the squad
// board with its attack tokens, roles and slots (BIBRules/BIBSquad.h) and obstacle-aware steering (BIBRules/BIBSteer.h).
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPlayerModel.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSquad.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSteer.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

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

	namespace T = BIB::Tactics;
	constexpr double Pi = 3.14159265358979323846;

	// Small deterministic generator so the randomised checks are repeatable.
	struct FRng
	{
		uint32_t State = 12345u;
		double Next()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<double>(State >> 8) / 16777216.0;
		}
	};

	// Plays Seconds of a fight in 0.1 s steps; DodgeEvery > 0 presses dodge on that period (0.15 s long).
	void Play(T::FPlayerModel& Model, double Seconds, double DodgeEvery, double Speed, double Retreat, double Distance, bool bCast = false)
	{
		double Next = DodgeEvery;
		for (double Time = 0.0; Time < Seconds; Time += 0.1)
		{
			T::FPlayerSample Sample;
			Sample.Dt = 0.1;
			Sample.Speed = Speed;
			Sample.RetreatSpeed = Retreat;
			Sample.NearestDistance = Distance;
			Sample.bCasting = bCast;
			if (DodgeEvery > 0.0 && Time >= Next)
			{
				Sample.bDodging = true;
				if (Time >= Next + 0.15)
				{
					Next += DodgeEvery;
				}
			}
			Model.Sample(Sample);
		}
	}

	void TestModelHabits()
	{
		T::FPlayerModel Model;
		Expect(Model.Strength() == 0.0, "a fresh model has no adaptation");
		Expect(Model.DodgePerMinute() == 0.0 && Model.EngagementDistance() < 0.0, "nothing observed yet");

		// Level versus edge: holding dodge for a long time is one dodge.
		T::FPlayerSample Hold;
		Hold.Dt = 0.1;
		Hold.bDodging = true;
		Hold.NearestDistance = 500.0;
		for (int Step = 0; Step < 20; ++Step)
		{
			Model.Sample(Hold);
		}
		const double OneDodge = Model.DodgePerMinute();
		Expect(OneDodge > 0.0 && OneDodge < 6.0, "a held dodge counts once");

		T::FPlayerModel Dodger;
		Play(Dodger, 40.0, 1.5, 100.0, 0.0, 600.0);
		Expect(Dodger.DodgePerMinute() > 20.0, "a dodge every 1.5 s reads as a dodge-heavy commander");
		Expect(Dodger.BlockPerMinute() == 0.0 && Dodger.ParryPerMinute() == 0.0, "no block or parry seen");
		const double Peak = Dodger.DodgePerMinute();
		Play(Dodger, 60.0, 0.0, 100.0, 0.0, 600.0);
		Expect(Dodger.DodgePerMinute() < Peak * 0.2, "a forgotten habit decays");

		T::FPlayerModel Blocker;
		for (int Press = 0; Press < 10; ++Press)
		{
			T::FPlayerSample Sample;
			Sample.Dt = 0.5;
			Sample.NearestDistance = 400.0;
			Sample.bBlocking = Press % 2 == 0;
			Sample.bParrying = Press % 4 == 0;
			Blocker.Sample(Sample);
		}
		Expect(Blocker.BlockPerMinute() > 0.0 && Blocker.ParryPerMinute() > 0.0, "block and parry presses are counted");
		Expect(Blocker.BlockPerMinute() > Blocker.ParryPerMinute(), "blocks outnumber parries here");
	}

	void TestModelRhythm()
	{
		T::FPlayerModel Steady;
		Play(Steady, 14.0, 1.6, 100.0, 0.0, 500.0);
		Expect(Steady.DodgeRhythm() > 0.7, "evenly spaced dodges read as a rhythm");

		T::FPlayerModel Ragged;
		FRng Rng;
		for (int Dodge = 0; Dodge < 8; ++Dodge)
		{
			const double Gap = 0.4 + Rng.Next() * 3.2;   // 0.4 .. 3.6 s
			for (double Time = 0.0; Time < Gap; Time += 0.1)
			{
				T::FPlayerSample Sample;
				Sample.Dt = 0.1;
				Sample.NearestDistance = 500.0;
				Sample.bDodging = Time < 0.15;
				Ragged.Sample(Sample);
			}
		}
		Expect(Ragged.DodgeRhythm() < Steady.DodgeRhythm(), "irregular dodges have a weaker rhythm than regular ones");
		Expect(Ragged.DodgeRhythm() < 0.5, "irregular dodges are not a rhythm");

		const double Before = Steady.DodgeRhythm();
		Play(Steady, 12.0, 0.0, 100.0, 0.0, 500.0);
		Expect(Before > 0.0 && Steady.DodgeRhythm() == 0.0, "the rhythm ends when the dodging stops");

		T::FPlayerModel Few;
		Play(Few, 3.0, 1.5, 100.0, 0.0, 500.0);
		Expect(Few.DodgeRhythm() == 0.0, "two dodges are not a rhythm");
	}

	void TestModelKiteStillDistance()
	{
		T::FPlayerModel Kiter;
		Play(Kiter, 40.0, 0.0, 500.0, 400.0, 700.0);
		Expect(Kiter.KiteFraction() > 0.7 && Kiter.StillFraction() < 0.1, "backing away from angels is kiting");
		Expect(std::abs(Kiter.EngagementDistance() - 700.0) < 1.0, "engagement distance follows the angels' distance");
		Expect(Kiter.Advise().bKiting, "advice flags a kiter");

		T::FPlayerModel Statue;
		Play(Statue, 40.0, 0.0, 0.0, 0.0, 200.0);
		Expect(Statue.StillFraction() > 0.7 && Statue.KiteFraction() < 0.1, "not moving is standing still");

		T::FPlayerModel Brawler;
		Play(Brawler, 40.0, 0.0, 400.0, -300.0, 150.0);
		Expect(Brawler.KiteFraction() < 0.05, "closing in is not kiting");

		T::FPlayerModel Far;
		Play(Far, 40.0, 0.0, 500.0, 400.0, 6000.0);
		Expect(Far.KiteFraction() < 0.05 && Far.EngagementDistance() < 0.0, "an angel far outside the fight is ignored");

		// Nobody alive: nothing is learned and time does not count as fighting.
		T::FPlayerModel Empty;
		Play(Empty, 30.0, 0.0, 500.0, 400.0, -1.0);
		Expect(Empty.ObservedSeconds() == 0.0 && Empty.Strength() == 0.0, "no angels, no observation");
	}

	void TestModelDamageAndChannel()
	{
		T::FPlayerModel Model;
		T::FPlayerSample Sample;
		Sample.Dt = 0.1;
		Sample.NearestDistance = 300.0;
		Model.Sample(Sample);
		Sample.HealthFraction = 0.8;
		Model.Sample(Sample);
		Expect(std::abs(Model.RecentDamage() - 0.2) < 1e-9, "a drop in health is damage");
		Sample.HealthFraction = 0.95;   // healing
		Model.Sample(Sample);
		Expect(std::abs(Model.RecentDamage() - 0.2) < 0.01, "healing is not damage");
		Sample.HealthFraction = 0.2;
		Sample.bDiscontinuity = true;   // switched to a hurt operator
		Model.Sample(Sample);
		Expect(Model.RecentDamage() < 0.25, "a health jump from switching operators is not damage");
		Sample.bDiscontinuity = false;
		for (int Step = 0; Step < 400; ++Step)
		{
			Model.Sample(Sample);
		}
		Expect(Model.RecentDamage() < 0.01, "recent damage fades");

		// Channel: a basic attack is short, a held cast is a channel; dodging is not casting.
		T::FPlayerModel Caster;
		Play(Caster, 0.5, 0.0, 0.0, 0.0, 400.0, true);
		Expect(!Caster.ChannelActive(), "a short cast is a basic attack");
		Play(Caster, 1.0, 0.0, 0.0, 0.0, 400.0, true);
		Expect(Caster.ChannelActive() && Caster.Advise().bChannel, "a cast held past the threshold is a channel");
		const double Early = Caster.Advise().BackOff;
		Expect(Early > 0.0 && Early <= 1.0, "angels give way during a channel");
		Play(Caster, 6.0, 0.0, 0.0, 0.0, 400.0, true);
		Expect(Caster.Advise().BackOff == 0.0, "giving way is bounded in time");
		Play(Caster, 0.2, 0.0, 0.0, 0.0, 400.0, false);
		Expect(!Caster.ChannelActive() && Caster.Advise().BackOff == 0.0, "the crowd resumes when the channel ends");
	}

	void TestModelBounds()
	{
		// Whatever the commander does, advice stays inside its documented bounds and Strength never passes the cap.
		FRng Rng;
		for (int Run = 0; Run < 40; ++Run)
		{
			T::FPlayerModel Model;
			for (int Step = 0; Step < 3000; ++Step)
			{
				T::FPlayerSample Sample;
				Sample.Dt = 0.016 + Rng.Next() * 0.05;
				Sample.Speed = Rng.Next() * 900.0;
				Sample.RetreatSpeed = (Rng.Next() - 0.5) * 900.0;
				Sample.NearestDistance = Rng.Next() < 0.05 ? -1.0 : Rng.Next() * 3000.0;
				Sample.bCasting = Rng.Next() < 0.3;
				Sample.bDodging = Rng.Next() < 0.2;
				Sample.bBlocking = Rng.Next() < 0.2;
				Sample.bParrying = Rng.Next() < 0.1;
				Sample.HealthFraction = Rng.Next();
				Sample.bDiscontinuity = Rng.Next() < 0.01;
				Model.Sample(Sample);
				const T::FAdvice Advice = Model.Advise();
				const bool bOk = Advice.Strength >= 0.0 && Advice.Strength <= T::StrengthCap + 1e-9
					&& Advice.TimingJitterS >= 0.0 && Advice.TimingJitterS <= 0.5 + 1e-9
					&& Advice.FeintBias >= 0.0 && Advice.FeintBias <= T::StrengthCap + 1e-9
					&& Advice.Intercept >= 0.0 && Advice.Intercept <= 1.0
					&& Advice.GapClose >= 0.0 && Advice.GapClose <= 1.0
					&& Advice.MageLead >= 0.0 && Advice.MageLead <= T::StrengthCap + 1e-9
					&& Advice.MageCutOff >= 0.0 && Advice.MageCutOff <= T::StrengthCap + 1e-9
					&& Advice.StrafeBias >= 0.0 && Advice.StrafeBias <= T::StrengthCap + 1e-9
					&& Advice.BackOff >= 0.0 && Advice.BackOff <= 1.0
					&& (Advice.TokenDelta == 0 || Advice.TokenDelta == -1);
				if (!bOk)
				{
					Expect(false, "advice stays inside its bounds (run " + std::to_string(Run) + " step " + std::to_string(Step) + ")");
					return;
				}
			}
		}
		Expect(true, "advice stays inside its bounds under 40 random fights");

		// Gentle ramp: nothing at the start, partial after 10 s, the cap after the ramp (and not beyond).
		T::FPlayerModel Ramp;
		Play(Ramp, 0.1, 1.0, 100.0, 0.0, 500.0);
		const double Start = Ramp.Strength();
		Play(Ramp, 10.0, 1.0, 100.0, 0.0, 500.0);
		const double Mid = Ramp.Strength();
		Play(Ramp, 120.0, 1.0, 100.0, 0.0, 500.0);
		Expect(Start < 0.01, "adaptation starts at zero");
		Expect(Mid > Start && Mid < T::StrengthCap * 0.5, "adaptation grows gently");
		Expect(std::abs(Ramp.Strength() - T::StrengthCap) < 1e-6, "adaptation stops at the cap");

		// Fairness: a commander who is being hurt is pressed less and meets fewer attackers.
		T::FPlayerModel Hurt = Ramp;
		T::FPlayerSample Hit;
		Hit.Dt = 0.1;
		Hit.NearestDistance = 300.0;
		Hit.HealthFraction = 1.0;
		Hurt.Sample(Hit);
		Hit.HealthFraction = 0.5;
		Hurt.Sample(Hit);
		Expect(Hurt.Strength() < Ramp.Strength(), "recent damage lowers adaptation");
		Expect(Hurt.Advise().TokenDelta == -1 && Ramp.Advise().TokenDelta == 0, "recent damage takes one attacker away");
	}

	void TestModelCounters()
	{
		// A rhythmic dodger gets jitter and feints; a kiter gets intercepts; a statue gets a repositioning crowd.
		T::FPlayerModel Dodger;
		Play(Dodger, 70.0, 1.6, 120.0, 0.0, 500.0);
		const T::FAdvice D = Dodger.Advise();
		Expect(D.TimingJitterS > 0.1 && D.FeintBias > 0.1, "a rhythmic dodger gets varied timing and feints");
		Expect(D.Intercept < 0.05, "a dodger who does not kite gets no intercept");

		T::FPlayerModel Kiter;
		Play(Kiter, 70.0, 0.0, 600.0, 450.0, 800.0);
		const T::FAdvice K = Kiter.Advise();
		Expect(K.Intercept > 0.2 && K.GapClose > 0.2 && K.MageCutOff > 0.2, "a kiter gets intercepts, a pressing crowd and cut-off area attacks");
		Expect(K.FeintBias < 0.05, "a kiter that never dodges gets no feints");

		T::FPlayerModel Statue;
		Play(Statue, 70.0, 0.0, 0.0, 0.0, 300.0);
		const T::FAdvice S = Statue.Advise();
		Expect(S.StrafeBias > D.StrafeBias * 0.5 && S.MageLead < K.MageLead, "a statue gets strafing archers and little lead");
	}

	// ---- squad

	std::vector<T::FAgent> Ring(int Count, double Radius, int FirstId = 1)
	{
		std::vector<T::FAgent> Agents;
		for (int Index = 0; Index < Count; ++Index)
		{
			T::FAgent Agent;
			Agent.Id = FirstId + Index;
			const double Angle = 2.0 * Pi * Index / Count;
			Agent.X = std::cos(Angle) * Radius;
			Agent.Y = std::sin(Angle) * Radius;
			Agents.push_back(Agent);
		}
		return Agents;
	}

	int Pressing(const T::FSquad& Squad, const std::vector<T::FAgent>& Agents)
	{
		int Count = 0;
		for (const T::FAgent& Agent : Agents)
		{
			Count += (Agent.Class == T::EClass::Melee && Squad.HasToken(Agent.Id)) ? 1 : 0;
		}
		return Count;
	}

	void TestTokens()
	{
		T::FSquad Squad;
		T::FSquadInput Input;
		Input.Capacity = 2;
		std::vector<T::FAgent> Agents = Ring(8, 600.0);
		Squad.Update(Input, Agents);
		Expect(Pressing(Squad, Agents) == 2, "two tokens for eight melee angels at capacity 2");
		Expect(Squad.TokensHeld() == 2, "the board agrees");

		// Capacity is clamped to TokenCap, and to the angels available.
		Input.Capacity = 9;
		Squad.Update(Input, Agents);
		Expect(Pressing(Squad, Agents) == T::TokenCap, "never more than TokenCap attackers");
		std::vector<T::FAgent> Pair = Ring(2, 400.0);
		T::FSquad Small;
		Input.Capacity = 3;
		Small.Update(Input, Pair);
		Expect(Pressing(Small, Pair) == 2, "two angels, two tokens");
		T::FSquad Zero;
		Input.Capacity = 0;
		Zero.Update(Input, Agents);
		Expect(Pressing(Zero, Agents) == 1, "capacity is at least one so someone always presses");

		// Nearest angels get the first tokens.
		T::FSquad Near;
		std::vector<T::FAgent> Mixed = Ring(5, 900.0);
		Mixed[3].X *= 0.3;
		Mixed[3].Y *= 0.3;
		Input.Capacity = 1;
		Near.Update(Input, Mixed);
		Expect(Near.HasToken(Mixed[3].Id), "the nearest angel takes the only token");

		// Committed angels keep their tokens, and nobody beyond capacity joins them.
		T::FSquad Busy;
		std::vector<T::FAgent> Fight = Ring(6, 500.0);
		Fight[4].bCommitted = true;
		Fight[5].bCommitted = true;
		Input.Capacity = 2;
		Busy.Update(Input, Fight);
		Expect(Busy.HasToken(Fight[4].Id) && Busy.HasToken(Fight[5].Id), "committed angels hold tokens");
		Expect(Pressing(Busy, Fight) == 2, "committed angels use up the capacity");
		Expect(Busy.CommittedMelee() == 2, "the board counts committed melee angels");

		// Bosses and ranged angels never take or need a melee token.
		T::FSquad Mixed2;
		std::vector<T::FAgent> Group = Ring(4, 500.0);
		Group[0].Class = T::EClass::Boss;
		Group[1].Class = T::EClass::Ranged;
		Input.Capacity = 1;
		Mixed2.Update(Input, Group);
		Expect(Mixed2.HasToken(Group[0].Id), "a boss always may attack");
		Expect(Mixed2.TokensHeld() == 1 && Mixed2.DutyOf(Group[1].Id) == T::EDuty::Support, "ranged angels support, melee tokens stay at capacity");
		Expect(Mixed2.DutyOf(Group[0].Id) == T::EDuty::Pressure, "a boss is pressure");

		// A lower capacity takes tokens from idle holders, never from a committed one.
		T::FSquad Hurt;
		std::vector<T::FAgent> Six = Ring(6, 500.0);
		Input.Capacity = 3;
		Hurt.Update(Input, Six);
		Six[0].bCommitted = true;
		const bool bFirstHeld = Hurt.HasToken(Six[0].Id);
		Input.Capacity = 1;
		Hurt.Update(Input, Six);
		Expect(Pressing(Hurt, Six) == 1, "capacity drops to one");
		Expect(!bFirstHeld || Hurt.HasToken(Six[0].Id), "the committed angel kept its token");
	}

	void TestTokenRotationAndInvariants()
	{
		// An angel that holds a token but never attacks (stuck behind a wall) loses it to the one that waited longest.
		T::FSquad Squad;
		T::FSquadInput Input;
		Input.Capacity = 1;
		std::vector<T::FAgent> Agents = Ring(3, 500.0);
		Agents[0].X = 200.0;   // nearest
		Agents[0].Y = 0.0;
		Squad.Update(Input, Agents);
		Expect(Squad.HasToken(Agents[0].Id), "the nearest angel starts with the token");
		bool bRotated = false;
		for (double Time = 0.1; Time < 8.0 && !bRotated; Time += 0.1)
		{
			Input.Now = Time;
			Squad.Update(Input, Agents);
			bRotated = !Squad.HasToken(Agents[0].Id);
		}
		Expect(bRotated, "an unused token rotates after TokenHoldSeconds");
		Expect(Squad.TokensHeld() == 1, "exactly one token is out after rotating");

		// Everyone gets a turn.
		T::FSquad Fair;
		std::vector<bool> Had(6, false);
		std::vector<T::FAgent> Six = Ring(6, 500.0);
		Input.Capacity = 2;
		for (double Time = 0.0; Time < 40.0; Time += 0.1)
		{
			Input.Now = Time;
			Fair.Update(Input, Six);
			for (size_t Index = 0; Index < Six.size(); ++Index)
			{
				Had[Index] = Had[Index] || Fair.HasToken(Six[Index].Id);
			}
		}
		bool bAll = true;
		for (bool Value : Had)
		{
			bAll = bAll && Value;
		}
		Expect(bAll, "every melee angel gets a token over time");

		// Invariant under churn: with nobody committed there are never more tokens than the cap; committed angels are always holders.
		FRng Rng;
		T::FSquad Churn;
		bool bOk = true;
		for (int Frame = 0; Frame < 4000 && bOk; ++Frame)
		{
			Input.Now = Frame * 0.05;
			Input.Capacity = 1 + static_cast<int>(Rng.Next() * 4.0);
			Input.TargetFacing = Rng.Next() * 6.0;
			std::vector<T::FAgent> Live;
			const int Count = 1 + static_cast<int>(Rng.Next() * 12.0);
			for (int Index = 0; Index < Count; ++Index)
			{
				T::FAgent Agent;
				Agent.Id = 100 + static_cast<int>(Rng.Next() * 20.0);   // ids come and go
				bool bDuplicate = false;
				for (const T::FAgent& Other : Live)
				{
					bDuplicate = bDuplicate || Other.Id == Agent.Id;
				}
				if (bDuplicate)
				{
					continue;
				}
				Agent.Class = Rng.Next() < 0.2 ? T::EClass::Ranged : (Rng.Next() < 0.08 ? T::EClass::Boss : T::EClass::Melee);
				Agent.X = (Rng.Next() - 0.5) * 2000.0;
				Agent.Y = (Rng.Next() - 0.5) * 2000.0;
				Agent.bCommitted = Rng.Next() < 0.15;
				Live.push_back(Agent);
			}
			Churn.Update(Input, Live);
			int Tokens = 0;
			int Committed = 0;
			for (const T::FAgent& Agent : Live)
			{
				if (Agent.Class == T::EClass::Melee)
				{
					Tokens += Churn.HasToken(Agent.Id) ? 1 : 0;
					Committed += Agent.bCommitted ? 1 : 0;
					bOk = bOk && (!Agent.bCommitted || Churn.HasToken(Agent.Id));
				}
			}
			const int Allowed = std::max(std::min(Input.Capacity, T::TokenCap), Committed);
			bOk = bOk && Tokens <= Allowed;
		}
		Expect(bOk, "tokens never exceed max(capacity, committed) and committed angels always hold one");
	}

	void TestRolesAndSlots()
	{
		T::FSquad Squad;
		T::FSquadInput Input;
		Input.Capacity = 2;
		Input.Flankers = 2;
		Input.TargetFacing = 0.0;
		std::vector<T::FAgent> Agents = Ring(9, 700.0);
		Agents[8].Class = T::EClass::Ranged;
		Agents[7].Class = T::EClass::Ranged;
		Squad.Update(Input, Agents);
		int Pressure = 0;
		int Flank = 0;
		int Wait = 0;
		int Support = 0;
		for (const T::FAgent& Agent : Agents)
		{
			switch (Squad.DutyOf(Agent.Id))
			{
			case T::EDuty::Pressure: ++Pressure; break;
			case T::EDuty::Flank: ++Flank; break;
			case T::EDuty::Wait: ++Wait; break;
			case T::EDuty::Support: ++Support; break;
			}
		}
		Expect(Pressure == 2 && Flank == 2 && Support == 2 && Wait == 3, "roles split into pressure, flank, support and wait");

		// Slots: flankers sit around the commander's back, and no two angels of a duty share a slot.
		double Back = Input.TargetFacing + Pi;
		for (const T::FAgent& Agent : Agents)
		{
			if (Squad.DutyOf(Agent.Id) == T::EDuty::Flank)
			{
				Expect(std::abs(T::WrapAngle(Squad.SlotAngle(Agent.Id) - Back)) < 1.0, "flank slots are behind the commander");
			}
		}
		bool bSpread = true;
		for (const T::FAgent& A : Agents)
		{
			for (const T::FAgent& B : Agents)
			{
				if (A.Id < B.Id && Squad.DutyOf(A.Id) == Squad.DutyOf(B.Id) && Squad.DutyOf(A.Id) != T::EDuty::Pressure)
				{
					bSpread = bSpread && std::abs(T::WrapAngle(Squad.SlotAngle(A.Id) - Squad.SlotAngle(B.Id))) > 0.3;
				}
			}
		}
		Expect(bSpread, "angels of one duty never share a slot (no stacking)");

		// Waiters spread over the whole ring and stay near where they already stand, so the crowd does not bunch up behind the commander.
		std::vector<T::FAgent> Wait5 = Ring(5, 800.0);
		T::FSquad Order;
		Input.Capacity = 1;
		Input.Flankers = 0;
		Order.Update(Input, Wait5);
		int Waiting = 0;
		bool bNear = true;
		for (const T::FAgent& Agent : Wait5)
		{
			if (Order.DutyOf(Agent.Id) == T::EDuty::Wait)
			{
				++Waiting;
				const double Own = std::atan2(Agent.Y, Agent.X);
				bNear = bNear && std::abs(T::WrapAngle(Order.SlotAngle(Agent.Id) - Own)) < 1.0;
			}
		}
		Expect(Waiting == 4, "four angels wait behind the one that presses");
		Expect(bNear, "waiting angels keep slots near where they already stand");
	}

	// ---- steering

	void TestSteering()
	{
		T::FSteerState State;
		T::FWhiskers Open;
		T::FSteerResult Result = T::Steer(Open, State, 0.0);
		Expect(Result.Angle == 0.0 && Result.Speed == 1.0 && !Result.bBlocked, "an open path is followed straight");

		T::FWhiskers Wall;
		Wall.Clear[2] = 0.1;
		Wall.Clear[1] = 0.2;
		Wall.Clear[3] = 0.9;
		Wall.Clear[0] = 0.3;
		Wall.Clear[4] = 0.8;
		Result = T::Steer(Wall, State, 0.1);
		Expect(Result.Angle > 0.0 && Result.Angle < Pi * 0.5 + 1e-9, "a wall ahead turns toward the freer side");
		Expect(State.LastSide == 1, "the detour side is remembered");

		// Hysteresis: nearly equal sides keep the side already chosen.
		T::FWhiskers Even = Wall;
		Even.Clear[1] = 0.62;
		Even.Clear[3] = 0.6;
		Result = T::Steer(Even, State, 0.2);
		T::FWhiskers EvenLeftBetter = Even;
		T::FSteerState Fresh;
		const T::FSteerResult Fresh1 = T::Steer(EvenLeftBetter, Fresh, 0.2);
		Expect(Fresh1.Angle < 0.0 && Result.Angle > 0.0, "a remembered side wins a near tie, a fresh angel takes the freer one");

		T::FWhiskers Boxed;
		for (double& Clear : Boxed.Clear)
		{
			Clear = 0.05;
		}
		Result = T::Steer(Boxed, State, 0.3);
		Expect(Result.bBlocked && Result.Speed < 1.0, "boxed in: creep and report blocked");

		// Stuck detection and the slide.
		T::FSteerState Stuck;
		for (double Time = 0.0; Time < 2.0; Time += 0.1)
		{
			T::NoteProgress(Stuck, Time, 100.0, 50.0, true);
		}
		Expect(Stuck.StuckCount >= 1, "no progress for a window counts as stuck");
		Expect(Stuck.SlideUntil > 0.0, "a stuck angel starts to slide");
		Result = T::Steer(Open, Stuck, Stuck.SlideUntil - 0.1);
		Expect(std::abs(std::abs(Result.Angle) - Pi * 0.5) < 1e-9, "while sliding the angel moves sideways along the obstacle");
		Result = T::Steer(Open, Stuck, Stuck.SlideUntil + 0.1);
		Expect(Result.Angle == 0.0, "the slide ends");

		T::FSteerState Moving;
		for (double Time = 0.0; Time < 3.0; Time += 0.1)
		{
			T::NoteProgress(Moving, Time, Time * 300.0, 0.0, true);
		}
		Expect(Moving.StuckCount == 0 && Moving.SlideUntil < 0.0, "an angel that makes progress is never stuck");

		T::FSteerState Idle;
		for (double Time = 0.0; Time < 3.0; Time += 0.1)
		{
			T::NoteProgress(Idle, Time, 0.0, 0.0, false);
		}
		Expect(Idle.StuckCount == 0 && Idle.SlideUntil < 0.0, "standing still on purpose is not stuck");

		// Corner escape simulation: a wall to the north (desired direction), the angel keeps trying; it must leave the pocket.
		double X = 0.0;
		double Y = 0.0;
		T::FSteerState Pocket;
		bool bEscaped = false;
		for (double Time = 0.0; Time < 12.0 && !bEscaped; Time += 0.1)
		{
			// Desired heading is +Y. A wall occupies y >= 100 for |x| < 300. Whiskers see it when within the look-ahead.
			T::FWhiskers Seen;
			for (int Index = 0; Index < T::WhiskerCount; ++Index)
			{
				const double Angle = Pi * 0.5 + (Index - 2) * Pi * 0.25;   // heading +Y rotated by the whisker angle (left = +)
				double Free = 1.0;
				for (double Step = 20.0; Step <= T::LookAheadCm; Step += 20.0)
				{
					const double Px = X + std::cos(Angle) * Step;
					const double Py = Y + std::sin(Angle) * Step;
					if (Py >= 100.0 && std::abs(Px) < 300.0)
					{
						Free = (Step - 20.0) / T::LookAheadCm;
						break;
					}
				}
				Seen.Clear[Index] = Free;
			}
			const T::FSteerResult Move = T::Steer(Seen, Pocket, Time);
			const double Heading = Pi * 0.5 + Move.Angle;
			const double NextX = X + std::cos(Heading) * 300.0 * 0.1 * Move.Speed;
			const double NextY = Y + std::sin(Heading) * 300.0 * 0.1 * Move.Speed;
			if (!(NextY >= 100.0 && std::abs(NextX) < 300.0))
			{
				X = NextX;
				Y = NextY;
			}
			T::NoteProgress(Pocket, Time, X, Y, true);
			bEscaped = Y > 150.0;
		}
		Expect(bEscaped, "steering walks around a wall to reach what is behind it");
	}
}

int main()
{
	TestModelHabits();
	TestModelRhythm();
	TestModelKiteStillDistance();
	TestModelDamageAndChannel();
	TestModelBounds();
	TestModelCounters();
	TestTokens();
	TestTokenRotationAndInvariants();
	TestRolesAndSlots();
	TestSteering();
	std::printf("tactics tests: %d checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
