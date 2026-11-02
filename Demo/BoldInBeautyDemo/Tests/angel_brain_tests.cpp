// Tests for the angels' tactical brain (owner request 2026-10-13, goal 2: smart, fair angels - BIBRules/BIBAngelBrain.h).
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh (passes Config/DefaultGame.ini as the first argument).
#include "../Source/BoldInBeautyDemo/BIBRules/BIBAngelBrain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
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

	namespace A = BIB::Angel;
	using A::EBehavior;
	using A::EIntent;
	using A::EMove;
	using A::ERole;
	using A::ETier;
	using A::FV2;

	constexpr int RoleCount = 7;
	const ERole Roles[RoleCount] = {ERole::Melee, ERole::Brute, ERole::Ranged, ERole::Mage, ERole::Assassin, ERole::MiniBoss, ERole::Boss};
	const char* RoleNames[RoleCount] = {"Angel", "Heavy", "Ranged", "Mage", "Assassin", "MiniBoss", "Boss"};

	// The archetypes (BIBRules/BIBCampaign.cpp): health, damage, interval, windup, speed, reach, keep band.
	struct FArchetype
	{
		double Health;
		double Interval;
		double Windup;
		double Speed;
		double Reach;
		double KeepMin;
		double KeepMax;
	};
	const FArchetype Archetypes[RoleCount] = {
		{2000.0, 1.6, 0.5, 330.0, 170.0, 0.0, 0.0},
		{5000.0, 1.6, 0.5, 260.0, 200.0, 0.0, 0.0},
		{1400.0, 2.0, 0.6, 300.0, 1500.0, 800.0, 1300.0},
		{1600.0, 4.0, 1.5, 280.0, 1300.0, 700.0, 1100.0},
		{900.0, 1.0, 0.3, 560.0, 160.0, 0.0, 0.0},
		{40000.0, 1.6, 0.5, 300.0, 220.0, 0.0, 0.0},
		{120000.0, 1.6, 0.5, 280.0, 260.0, 0.0, 0.0},
	};

	A::FThink MakeThink(int RoleIndex, int Chapter = 1)
	{
		const ERole Role = Roles[RoleIndex];
		const FArchetype& Arch = Archetypes[RoleIndex];
		A::FThink In;
		In.Id = RoleIndex + 1;
		In.Role = Role;
		In.Tier = A::TierFor(Role);
		In.Knobs = A::EffectiveKnobs(In.Tier, Chapter);
		In.Interval = Arch.Interval;
		In.BaseWindup = Arch.Windup;
		In.Reach = Arch.Reach;
		In.KeepMin = Arch.KeepMin;
		In.KeepMax = Arch.KeepMax;
		In.Now = 10.0;
		In.View.Time = 9.7;
		In.View.Pos = {0.0, 0.0};
		In.View.Facing = {1.0, 0.0};
		In.CommanderNow = In.View.Pos;
		const bool bRanged = Role == ERole::Ranged || Role == ERole::Mage;
		In.Pos = {bRanged ? 0.5 * (Arch.KeepMin + Arch.KeepMax) : Arch.Reach - 20.0, 0.0};
		In.Facing = {-1.0, 0.0};
		return In;
	}

	/** A crowd with this one angel in it, planned for its view. */
	A::FCrowd CrowdFor(const A::FThink& In, double Adaptation = 0.0)
	{
		A::FCrowd Crowd;
		A::FCrowdAgent Agent;
		Agent.Id = In.Id;
		Agent.Role = In.Role;
		Agent.Pos = In.Pos;
		Agent.Reach = In.Reach;
		Crowd.Update(In.Now, In.View, {Agent}, Adaptation, In.Knobs.Aggression);
		return Crowd;
	}

	A::FBrainState Fresh(uint32_t Seed)
	{
		A::FBrainState State;
		State.Random.State = Seed * 2654435761u + 1u;
		return State;
	}

	// ------------------------------------------------------------------ math and knobs
	void TestMath()
	{
		Expect(std::abs(A::Length({3.0, 4.0}) - 5.0) < 1e-9, "length");
		Expect(A::Length(A::Normal({0.0, 0.0})) == 0.0, "the zero vector stays zero");
		const FV2 Left = A::Perp({1.0, 0.0});
		Expect(std::abs(Left.X) < 1e-9 && std::abs(Left.Y - 1.0) < 1e-9, "perp turns left");
		const FV2 Turned = A::Rotate({1.0, 0.0}, 3.14159265358979323846 / 2.0);
		Expect(std::abs(Turned.X) < 1e-9 && std::abs(Turned.Y - 1.0) < 1e-9, "rotate");
	}

	void TestKnobs()
	{
		A::ResetAiTuning();
		const A::FAiTuning& T = A::AiTuning();
		Expect(T.Difficulty > 1.0, "owner 2026-10-13 'a bit harder': the default difficulty is above 1");
		for (int Tier = 0; Tier < A::TierCount; ++Tier)
		{
			const A::FKnobs Base = T.Tier[Tier];
			const std::string Name = A::TierName(static_cast<ETier>(Tier));
			const A::FKnobs One = A::EffectiveKnobs(static_cast<ETier>(Tier), 1);
			Expect(One.ReactionSeconds < Base.ReactionSeconds, Name + ": difficulty makes reactions faster than the base");
			Expect(One.DodgeChance > Base.DodgeChance && One.Aggression >= Base.Aggression, Name + ": difficulty raises dodge chance and aggression");
			for (int Chapter = 1; Chapter <= 13; ++Chapter)
			{
				const A::FKnobs K = A::EffectiveKnobs(static_cast<ETier>(Tier), Chapter);
				const std::string Where = Name + " chapter " + std::to_string(Chapter);
				// Fairness (brief): reactions around 150-300 ms by tier, never below the floor; nothing is perfect; telegraphs stay readable.
				Expect(K.ReactionSeconds >= 0.15 - 1e-9 && K.ReactionSeconds <= 0.3 + 1e-9, Where + ": reaction 150-300 ms");
				Expect(K.DodgeChance <= 0.85 && K.BlockChance <= 0.85 && K.PunishChance < 1.0, Where + ": never a certain dodge / block / punish");
				Expect(K.MinTelegraph >= 0.25, Where + ": telegraph floor");
				Expect(K.TurnRateDeg > 0.0 && K.TurnRateDeg <= 720.0, Where + ": turning takes time");
				if (Chapter > 1)
				{
					Expect(K.ReactionSeconds <= A::EffectiveKnobs(static_cast<ETier>(Tier), Chapter - 1).ReactionSeconds, Where + ": later chapters react no slower");
				}
			}
		}
		const A::FKnobs Regular = A::EffectiveKnobs(ETier::Regular, 1);
		const A::FKnobs Elite = A::EffectiveKnobs(ETier::Elite, 1);
		const A::FKnobs Boss = A::EffectiveKnobs(ETier::Boss, 1);
		Expect(Regular.ReactionSeconds > Elite.ReactionSeconds && Elite.ReactionSeconds > Boss.ReactionSeconds, "higher tiers react faster");
		Expect(Regular.DodgeChance < Elite.DodgeChance && Elite.DodgeChance < Boss.DodgeChance, "higher tiers dodge more");
		Expect(Regular.AdaptRate < Elite.AdaptRate && Elite.AdaptRate < Boss.AdaptRate, "higher tiers adapt faster");
		Expect(A::TierFor(ERole::Assassin) == ETier::Regular && A::TierFor(ERole::MiniBoss) == ETier::Elite && A::TierFor(ERole::Boss) == ETier::Boss,
			"tiers by role");

		// The floor holds even when the config asks for less.
		A::AiTuning().Tier[0].ReactionSeconds = 0.01;
		Expect(A::EffectiveKnobs(ETier::Regular, 13).ReactionSeconds >= A::AiTuning().ReactionFloor, "the reaction floor beats a bad config");
		A::AiTuning().Tier[0].MinTelegraph = 0.05;
		Expect(A::EffectiveKnobs(ETier::Regular, 1).MinTelegraph >= 0.25, "the telegraph floor beats a bad config");
		A::ResetAiTuning();

		// The mind learns faster with the tier's AdaptRate.
		A::FMind Slow;
		A::FMind Quick;
		Quick.SetLearnMult(2.0);
		for (int Step = 0; Step < 200; ++Step)
		{
			Slow.Observe(A::ETrait::Blocking, 0.1);
			Quick.Observe(A::ETrait::Blocking, 0.1);
			Slow.Tick(0.05, false);
			Quick.Tick(0.05, false);
		}
		Expect(Quick.Expertise(A::ECounter::Unblockable) > Slow.Expertise(A::ECounter::Unblockable), "AdaptRate speeds up learning");
		Quick.SetLearnMult(-1.0);
		Expect(Quick.GetLearnMult() == 1.0, "a bad learn multiplier falls back to 1");
	}

	// ------------------------------------------------------------------ config file
	std::map<std::string, std::string> ReadSection(const std::string& Path, const std::string& Section)
	{
		std::map<std::string, std::string> Out;
		std::ifstream File(Path);
		std::string Line;
		bool bIn = false;
		while (std::getline(File, Line))
		{
			while (!Line.empty() && (Line.back() == '\r' || Line.back() == ' '))
			{
				Line.pop_back();
			}
			if (!Line.empty() && Line[0] == '[')
			{
				bIn = Line == "[" + Section + "]";
				continue;
			}
			if (!bIn || Line.empty() || Line[0] == ';' || Line[0] == '#')
			{
				continue;
			}
			const size_t Eq = Line.find('=');
			if (Eq != std::string::npos)
			{
				Out[Line.substr(0, Eq)] = Line.substr(Eq + 1);
			}
		}
		return Out;
	}

	void TestConfig(const char* IniPath)
	{
		A::ResetAiTuning();
		A::FAiTuning& T = A::AiTuning();
		std::set<std::string> Names;
		for (const A::FAiConfigKey& Key : A::AiConfigKeys(T))
		{
			Expect(Names.insert(Key.Name).second, "config key is unique: " + Key.Name);
			Expect(Key.Value != nullptr, "config key has a value: " + Key.Name);
		}
		Expect(Names.count("Regular.ReactionSeconds") == 1 && Names.count("Boss.AdaptRate") == 1 && Names.count("Elite.DodgeChance") == 1 &&
				Names.count("Regular.Aggression") == 1,
			"the brief's knobs (reaction time, aggression, dodge chance, adaptation rate) are exposed per tier");
		if (!IniPath)
		{
			return;
		}
		const std::map<std::string, std::string> Ini = ReadSection(IniPath, "BIB.AngelAI");
		Expect(!Ini.empty(), "DefaultGame.ini has a [BIB.AngelAI] section");
		for (const A::FAiConfigKey& Key : A::AiConfigKeys(T))
		{
			const auto Found = Ini.find(Key.Name);
			Expect(Found != Ini.end(), "DefaultGame.ini sets " + Key.Name);
			if (Found != Ini.end())
			{
				Expect(std::abs(std::stod(Found->second) - *Key.Value) < 1e-6, "DefaultGame.ini matches the default of " + Key.Name);
			}
		}
		for (const auto& Pair : Ini)
		{
			Expect(Names.count(Pair.first) == 1, "DefaultGame.ini key is known: " + Pair.first);
		}
	}

	// ------------------------------------------------------------------ perception
	void TestTimeline()
	{
		A::FPlayerTimeline Timeline;
		Expect(Timeline.Empty() && Timeline.At(5.0).Time < 0.0, "an empty timeline gives a default view");
		for (int Frame = 0; Frame <= 300; ++Frame)
		{
			A::FPlayerView View;
			View.Time = Frame / 60.0;
			View.Pos = {static_cast<double>(Frame), 0.0};
			View.HealthFraction = Frame >= 240 ? 0.5 + 0.002 * (Frame - 240) : 0.5;
			Timeline.Record(View);
		}
		const A::FPlayerView Delayed = Timeline.At(5.0 - 0.25);
		Expect(Delayed.Time <= 4.75 + 1e-9 && Delayed.Time > 4.73, "At gives the newest snapshot no newer than the delayed time");
		Expect(std::abs(Delayed.Pos.X - 285.0) < 1.01, "... with that frame's position (no peeking at the present)");
		Expect(Timeline.At(0.0).Time >= 5.0 - 2.6, "old snapshots are dropped (~2.5 s kept)");
		Expect(Timeline.Healing(5.0), "a health rise is a heal");
		Expect(!Timeline.Healing(3.9), "steady health is not a heal");
		A::FPlayerView Back;
		Back.Time = 1.0;
		Timeline.Record(Back);
		Expect(Timeline.At(10.0).Time == 1.0, "time going backwards (a new stage) restarts the timeline");
	}

	A::FPlayerView ViewAt(double Time)
	{
		A::FPlayerView View;
		View.Time = Time;
		View.Facing = {1.0, 0.0};
		return View;
	}

	void TestHabits()
	{
		A::ResetAiTuning();
		A::FHabits Habits;
		double Confidence = 1.0;
		Expect(Habits.FavouriteDodge(Confidence) == A::ESide::Count && Confidence == 0.0, "no dodges, no favourite");
		// Five dodges to the left (the commander faces +X, moves +Y), one back.
		double Time = 0.0;
		for (int Index = 0; Index < 6; ++Index)
		{
			A::FPlayerView Before = ViewAt(Time);
			A::FPlayerView After = ViewAt(Time + 0.02);
			After.bDodging = true;
			After.Vel = Index == 5 ? FV2{-600.0, 0.0} : FV2{0.0, 600.0};
			Habits.Observe(Before, After);
			Time += 3.0;
		}
		const A::ESide Side = Habits.FavouriteDodge(Confidence);
		Expect(Side == A::ESide::Left && Confidence > 0.7, "the favourite dodge side is read from the dodges");
		Expect(Habits.Count(A::EAction::Dodge) == 6, "dodges are counted");
		double OpenerConfidence = 0.0;
		Expect(Habits.FavouriteOpener(OpenerConfidence) == A::EAction::Dodge && OpenerConfidence > 0.5, "the opener after calm is remembered");

		// Basic spam.
		for (int Index = 0; Index < 8; ++Index)
		{
			A::FPlayerView Before = ViewAt(Time);
			A::FPlayerView After = ViewAt(Time + 0.02);
			After.bAttacking = true;
			After.ActionStartedAt = Time + 0.01;
			Habits.Observe(Before, After);
			Time += 0.4;
		}
		Expect(Habits.SpamLevel() > 0.5, "basic-attack spam is noticed");
		Expect(Habits.Size() <= 12, "the habit window is bounded");

		// Early dodges: the commander rolls 0.5 s before each strike lands.
		A::FHabits Early;
		for (int Index = 0; Index < 5; ++Index)
		{
			const double Start = 10.0 * Index;
			Early.NoteTelegraph(Start, Start + 0.8);
			A::FPlayerView Before = ViewAt(Start + 0.28);
			A::FPlayerView After = ViewAt(Start + 0.3);
			After.bDodging = true;
			After.Vel = {0.0, -600.0};
			Early.Observe(Before, After);
		}
		Expect(Early.EarlyDodgeRate() > 0.9, "dodging before the strike is due is noticed");
		A::FHabits Timed;
		for (int Index = 0; Index < 5; ++Index)
		{
			const double Start = 10.0 * Index;
			Timed.NoteTelegraph(Start, Start + 0.8);
			A::FPlayerView Before = ViewAt(Start + 0.68);
			A::FPlayerView After = ViewAt(Start + 0.7);
			After.bDodging = true;
			Timed.Observe(Before, After);
		}
		Expect(Timed.EarlyDodgeRate() < 0.1, "well-timed dodges are not early");
		Early.Reset();
		Expect(Early.Size() == 0 && Early.EarlyDodgeRate() == 0.0, "reset");
	}

	// ------------------------------------------------------------------ the crowd
	void TestCrowd()
	{
		A::ResetAiTuning();
		A::FPlayerView Commander = ViewAt(10.0);
		std::vector<A::FCrowdAgent> Agents;
		for (int Index = 0; Index < 8; ++Index)
		{
			A::FCrowdAgent Agent;
			Agent.Id = Index + 1;
			Agent.Role = ERole::Melee;
			Agent.Reach = 170.0;
			Agent.Pos = A::Rotate({500.0, 0.0}, Index * 0.6);
			Agents.push_back(Agent);
		}
		A::FCrowd Crowd;
		Crowd.Update(10.0, Commander, Agents, 0.0, 0.6);
		const int Cap = A::MaxAttackers(8, 0.0) + 1;
		Expect(Crowd.TokensHeld() == Cap, "tokens up to MaxAttackers + ExtraAttackers");
		int Holder = -1;
		for (const A::FCrowdAgent& Agent : Agents)
		{
			if (Crowd.Slot(Agent.Id).bToken)
			{
				Holder = Agent.Id;
				break;
			}
		}
		Expect(Holder > 0, "someone holds a token");
		Crowd.Release(Holder, 10.5, 0.6);
		Crowd.Update(10.6, Commander, Agents, 0.0, 0.6);
		Expect(!Crowd.Slot(Holder).bToken, "an angel that just attacked waits its turn");
		Expect(Crowd.TokensHeld() == Cap, "... and another takes the token (pressure alternates)");
		Crowd.Update(14.0, Commander, Agents, 0.0, 0.6);
		Expect(Crowd.TokensHeld() <= Cap, "never above the cap");

		// Borrowing: one punish over the cap.
		int Waiting = -1;
		for (const A::FCrowdAgent& Agent : Agents)
		{
			if (!Crowd.Slot(Agent.Id).bToken)
			{
				Waiting = Agent.Id;
				break;
			}
		}
		Expect(Crowd.BorrowToken(Waiting, 14.0) && Crowd.TokensHeld() == Cap + 1, "a punish may borrow one token");
		int Second = -1;
		for (const A::FCrowdAgent& Agent : Agents)
		{
			if (!Crowd.Slot(Agent.Id).bToken)
			{
				Second = Agent.Id;
				break;
			}
		}
		Expect(!Crowd.BorrowToken(Second, 14.0), "... only one at a time");

		// Adapted flanking: waiting angels avoid the commander's front cone.
		A::FCrowd Smart;
		Smart.Update(20.0, Commander, Agents, 0.8, 0.6);
		const double ConeCos = std::cos(A::AiTuning().FlankFrontCone * 3.14159265358979323846 / 180.0);
		bool bAllOutside = true;
		int Flankers = 0;
		for (const A::FCrowdAgent& Agent : Agents)
		{
			const A::FCrowdSlot& Slot = Smart.Slot(Agent.Id);
			if (Slot.bToken)
			{
				continue;
			}
			bAllOutside = bAllOutside && A::Dot(A::Normal(Slot.HoldPoint - Commander.Pos), Commander.Facing) < ConeCos + 1e-6;
			Flankers += Slot.bFlanker ? 1 : 0;
			Expect(A::Length(Slot.HoldPoint - Commander.Pos) >= Commander.ThreatReach + A::AiTuning().OutrangedMargin - 1e-6,
				"waiting angels hold outside the commander's reach");
		}
		Expect(bAllOutside, "adapted waiting angels hold the sides and back, not the commander's front");
		Expect(Flankers > 0, "some angels flank");

		// Ranged turns.
		std::vector<A::FCrowdAgent> Shooters;
		for (int Index = 0; Index < 4; ++Index)
		{
			A::FCrowdAgent Agent;
			Agent.Id = 100 + Index;
			Agent.Role = Index % 2 == 0 ? ERole::Ranged : ERole::Mage;
			Agent.Pos = A::Rotate({1000.0, 0.0}, Index * 1.2);
			Shooters.push_back(Agent);
		}
		A::FCrowd Range;
		Range.Update(30.0, Commander, Shooters, 0.0, 0.6);
		int MayShoot = 0;
		int First = -1;
		for (const A::FCrowdAgent& Agent : Shooters)
		{
			if (Range.Slot(Agent.Id).bMayShoot)
			{
				++MayShoot;
				First = Agent.Id;
			}
		}
		Expect(MayShoot == 1, "one new shooter per frame");
		Range.NoteShot(First, 30.0);
		Shooters[static_cast<size_t>(First - 100)].bCommitted = true;
		Range.Update(30.1, Commander, Shooters, 0.0, 0.6);
		bool bAny = false;
		for (const A::FCrowdAgent& Agent : Shooters)
		{
			bAny = bAny || Range.Slot(Agent.Id).bMayShoot;
		}
		Expect(!bAny, "shots are staggered");
		Range.Update(30.5, Commander, Shooters, 0.0, 0.6);
		int Second2 = -1;
		for (const A::FCrowdAgent& Agent : Shooters)
		{
			Second2 = Range.Slot(Agent.Id).bMayShoot ? Agent.Id : Second2;
		}
		Expect(Second2 > 0 && Second2 != First, "a second shooter after the stagger");
		Range.NoteShot(Second2, 30.5);
		Shooters[static_cast<size_t>(Second2 - 100)].bCommitted = true;
		Range.Update(31.5, Commander, Shooters, 0.0, 0.6);
		bAny = false;
		for (const A::FCrowdAgent& Agent : Shooters)
		{
			bAny = bAny || Range.Slot(Agent.Id).bMayShoot;
		}
		Expect(!bAny, "at most MaxShooters fire at once");

		// Bosses always press; the dead are forgotten.
		A::FCrowdAgent Boss;
		Boss.Id = 900;
		Boss.Role = ERole::Boss;
		A::FCrowd Hall;
		Hall.Update(1.0, Commander, {Boss}, 0.0, 0.8);
		Expect(Hall.Slot(900).bToken, "the boss always holds a token");
		Hall.Update(2.0, Commander, {}, 0.0, 0.8);
		Expect(Hall.TokensHeld() == 0 && !Hall.Slot(900).bToken, "dead angels are forgotten");
	}

	// ------------------------------------------------------------------ decisions
	A::FAction Decide(A::FThink In, A::FBrainState& State, const A::FMind& Mind, const A::FHabits& Habits, double Adaptation = 0.0)
	{
		A::FCrowd Crowd = CrowdFor(In, Adaptation);
		return A::Think(In, Mind, Habits, Crowd, State);
	}

	void TestBehaviors()
	{
		A::ResetAiTuning();
		A::ResetAiStats();
		const A::FMind Mind;
		const A::FHabits Habits;
		for (int RoleIndex = 0; RoleIndex < RoleCount; ++RoleIndex)
		{
			const std::string Name = RoleNames[RoleIndex];
			const ERole Role = Roles[RoleIndex];
			const bool bRanged = Role == ERole::Ranged || Role == ERole::Mage;
			const bool bBoss = Role == ERole::MiniBoss || Role == ERole::Boss;

			// Defense: a telegraphed heavy aimed at it is evaded sometimes, never always, and only once per action.
			{
				int Evaded = 0;
				int Rolls = 0;
				for (uint32_t Seed = 1; Seed <= 400; ++Seed)
				{
					A::FThink In = MakeThink(RoleIndex);
					In.Pos = {bRanged ? 280.0 : 150.0, 0.0};
					In.View.bAttacking = true;
					In.View.bHeavyWindup = true;
					In.View.ThreatReach = 320.0;
					In.View.ActionStartedAt = 9.6;
					A::FBrainState State = Fresh(Seed);
					const A::FAction Action = Decide(In, State, Mind, Habits);
					++Rolls;
					if (Action.Intent == EIntent::Sidestep || Action.Intent == EIntent::Backstep)
					{
						++Evaded;
						Expect(Action.Has(EBehavior::Defense), Name + ": evasions are tagged");
						Expect(A::Length(Action.Goal) > 0.99 && A::Length(Action.Goal) < 1.01, Name + ": the evasion has a direction");
						In.Now += 0.1;
						const A::FAction Again = Decide(In, State, Mind, Habits);
						Expect(Again.Intent != EIntent::Sidestep && Again.Intent != EIntent::Backstep, Name + ": one evasion roll per commander action");
					}
				}
				const double Rate = static_cast<double>(Evaded) / Rolls;
				const double Chance = A::EffectiveKnobs(A::TierFor(Role), 1).DodgeChance;
				Expect(Rate > 0.0, Name + ": dodges telegraphed heavies");
				Expect(Rate < 0.85 && Rate <= Chance + 0.08, Name + ": never a perfect dodge (" + std::to_string(Rate) + ")");
			}

			// Spacing.
			{
				A::FThink In = MakeThink(RoleIndex);
				A::FBrainState State = Fresh(7);
				State.AttackReadyAt = 99.0;
				if (bRanged)
				{
					In.Pos = {In.KeepMin - 300.0, 0.0};
					const A::FAction Back = Decide(In, State, Mind, Habits);
					Expect(Back.Intent == EIntent::Approach && A::Length(Back.Goal) > A::Length(In.Pos) && Back.Has(EBehavior::Spacing),
						Name + ": backs off to its band when too close");
					In.Pos = {In.KeepMax + 600.0, 0.0};
					const A::FAction Close = Decide(In, State, Mind, Habits);
					Expect(Close.Intent == EIntent::Approach && A::Length(Close.Goal) < A::Length(In.Pos), Name + ": closes to its band when too far");
					In.Pos = {0.5 * (In.KeepMin + In.KeepMax), 0.0};
					Expect(Decide(In, State, Mind, Habits).Intent == EIntent::Strafe, Name + ": strafes inside its band");
					In.bLineBlockedByAlly = true;
					const A::FAction Clear = Decide(In, State, Mind, Habits);
					Expect(Clear.Intent == EIntent::Reposition && Clear.Has(EBehavior::Coordination), Name + ": repositions when an ally blocks the shot");
				}
				else
				{
					In.Pos = {900.0, 0.0};
					In.View.bRecovery = true;
					const A::FAction Rush = Decide(In, State, Mind, Habits);
					Expect(Rush.Intent == EIntent::Approach && Rush.Speed >= 1.3, Name + ": closes in on a recovering commander");
				}
			}

			// Punish: a whiffed recovery in reach is struck (melee) / shot (ranged), mostly.
			{
				int Punished = 0;
				for (uint32_t Seed = 1; Seed <= 200; ++Seed)
				{
					A::FThink In = MakeThink(RoleIndex);
					In.View.bRecovery = true;
					In.View.bWhiffed = true;
					In.View.ActionStartedAt = 9.0;
					A::FBrainState State = Fresh(Seed);
					const A::FAction Action = Decide(In, State, Mind, Habits);
					if (Action.Intent == EIntent::Attack && Action.Has(EBehavior::Punish))
					{
						++Punished;
						Expect(Action.Windup >= In.Knobs.MinTelegraph - 1e-9, Name + ": punishes keep a telegraph");
					}
				}
				Expect(Punished > 60 && Punished < 200, Name + ": punishes whiffs often but not always (" + std::to_string(Punished) + "/200)");
				int Heals = 0;
				for (uint32_t Seed = 1; Seed <= 200; ++Seed)
				{
					A::FThink In = MakeThink(RoleIndex);
					In.bHealing = true;
					A::FBrainState State = Fresh(Seed);
					const A::FAction Action = Decide(In, State, Mind, Habits);
					Heals += Action.Has(EBehavior::Punish) ? 1 : 0;
				}
				Expect(Heals > 60, Name + ": punishes heals");
				int Casts = 0;
				for (uint32_t Seed = 1; Seed <= 200; ++Seed)
				{
					A::FThink In = MakeThink(RoleIndex);
					In.View.bAttacking = true;
					In.View.bLongCast = true;
					In.View.ActionStartedAt = 9.65;
					In.View.Facing = {0.0, 1.0};   // the cast is not aimed at this angel
					A::FBrainState State = Fresh(Seed);
					const A::FAction Action = Decide(In, State, Mind, Habits);
					Casts += Action.Has(EBehavior::Punish) ? 1 : 0;
				}
				Expect(Casts > 60, Name + ": punishes ultimate / long-cast wind-ups");
			}

			// Self-preservation.
			{
				A::FThink In = MakeThink(RoleIndex);
				In.HealthFraction = 0.1;
				In.NearbyAllies = 2;
				In.AllyCentre = {900.0, 600.0};
				A::FBrainState State = Fresh(3);
				const A::FAction Action = Decide(In, State, Mind, Habits);
				if (bBoss)
				{
					Expect(Action.Intent != EIntent::Retreat, Name + ": bosses do not run; they change tactics");
					A::FThink Strained = MakeThink(RoleIndex);
					Strained.StrainFraction = 0.9;
					Strained.Pos = {500.0, 0.0};
					A::FBrainState Calm = Fresh(4);
					int Ranged = 0;
					for (int Step = 0; Step < 40; ++Step)
					{
						Strained.Now = 10.0 + Step * 3.0;
						Strained.View.Time = Strained.Now - 0.2;
						const A::FAction Pick = Decide(Strained, Calm, Mind, Habits);
						Ranged += Pick.Intent == EIntent::Attack && (Pick.Move == EMove::Nova || Pick.Move == EMove::Volley) ? 1 : 0;
					}
					Expect(Ranged > 0, Name + ": close to BREAK it plays safe (blasts and volleys)");
				}
				else
				{
					Expect(Action.Intent == EIntent::Retreat && Action.Has(EBehavior::SelfPreservation), Name + ": retreats when hurt");
					Expect(A::Length(Action.Goal - In.View.Pos) > A::Length(In.Pos - In.View.Pos), Name + ": ... away from the commander");
					In.Now += 1.0;
					Expect(Decide(In, State, Mind, Habits).Intent == EIntent::Retreat, Name + ": keeps regrouping for a while");
					In.Now += A::AiTuning().RegroupSeconds + 0.1;
					const A::FAction After = Decide(In, State, Mind, Habits);
					Expect(After.Intent != EIntent::Retreat, Name + ": then fights again (regroup has a cooldown)");
				}
			}

			// Perception: lost sight beyond memory -> no tracking; within memory -> hunts the LAST KNOWN position.
			{
				A::FThink In = MakeThink(RoleIndex);
				A::FBrainState State = Fresh(9);
				In.View.Pos = {0.0, 0.0};
				Decide(In, State, Mind, Habits);   // seen at the origin
				In.bSees = false;
				In.Now += 1.0;
				In.View.Pos = {0.0, 2000.0};       // the commander moved behind a wall
				In.CommanderNow = In.View.Pos;
				const A::FAction Hunt = Decide(In, State, Mind, Habits);
				Expect(A::Length(Hunt.Goal) < 1.0, Name + ": hunts the last known position, not the hidden one");
				Expect(bRanged ? Hunt.Intent == EIntent::Reposition : Hunt.Intent == EIntent::Approach, Name + ": hunts / looks for a line of sight");
				In.Now += In.Knobs.MemorySeconds + 1.0;
				Expect(Decide(In, State, Mind, Habits).Intent == EIntent::Hold, Name + ": forgets after MemorySeconds");
			}
		}

		const A::FAiStats& Stats = A::AiStats();
		Expect(Stats.Evasions > 0 && Stats.Punishes > 0, "stats count evasions and punishes");
	}

	/** Adaptation: the moves follow the commander's habits. */
	void TestAdaptation()
	{
		A::ResetAiTuning();
		A::FMind Mind;
		// Early dodger -> feints / delayed enders.
		A::FHabits Early;
		for (int Index = 0; Index < 6; ++Index)
		{
			const double Start = 10.0 * Index;
			Early.NoteTelegraph(Start, Start + 0.8);
			A::FPlayerView Before = ViewAt(Start + 0.28);
			A::FPlayerView After = ViewAt(Start + 0.3);
			After.bDodging = true;
			After.Vel = {0.0, 600.0};
			Early.Observe(Before, After);
		}
		A::FHabits Side;
		for (int Index = 0; Index < 6; ++Index)
		{
			A::FPlayerView Before = ViewAt(Index * 3.0);
			A::FPlayerView After = ViewAt(Index * 3.0 + 0.02);
			After.bDodging = true;
			After.Vel = {0.0, -600.0};   // always to the right
			Side.Observe(Before, After);
		}
		const A::FHabits None;
		for (int RoleIndex : {0, 4, 5, 6})
		{
			const std::string Name = RoleNames[RoleIndex];
			int FeintsEarly = 0;
			int FeintsNone = 0;
			int SweepsSide = 0;
			int SweepsNone = 0;
			for (uint32_t Seed = 1; Seed <= 300; ++Seed)
			{
				A::FThink In = MakeThink(RoleIndex, 6);
				A::FBrainState S1 = Fresh(Seed);
				A::FBrainState S2 = Fresh(Seed);
				A::FBrainState S3 = Fresh(Seed);
				const A::FAction WithEarly = Decide(In, S1, Mind, Early);
				const A::FAction WithSide = Decide(In, S2, Mind, Side);
				const A::FAction Plain = Decide(In, S3, Mind, None);
				FeintsEarly += WithEarly.Has(EBehavior::Feint) ? 1 : 0;
				FeintsNone += Plain.Has(EBehavior::Feint) ? 1 : 0;
				const bool bSweepSide = WithSide.Move == EMove::Sweep || (WithSide.Move == EMove::Combo && WithSide.Enders[WithSide.FollowUps - 1] == A::EEnder::Sweep);
				const bool bSweepNone = Plain.Move == EMove::Sweep || (Plain.Move == EMove::Combo && Plain.Enders[Plain.FollowUps - 1] == A::EEnder::Sweep);
				SweepsSide += bSweepSide ? 1 : 0;
				SweepsNone += bSweepNone ? 1 : 0;
				if (WithEarly.Has(EBehavior::Feint) && WithEarly.Move == EMove::Feint)
				{
					Expect(WithEarly.FeintDelay >= In.Knobs.MinTelegraph - 1e-9, Name + ": a feint's real strike keeps its own telegraph");
				}
			}
			Expect(FeintsEarly > FeintsNone + 30, Name + ": early dodgers get feints and delayed enders (" + std::to_string(FeintsEarly) + " vs " +
				std::to_string(FeintsNone) + ")");
			if (RoleIndex != 4)
			{
				Expect(SweepsSide > SweepsNone + 30, Name + ": side dodgers get sweeps (" + std::to_string(SweepsSide) + " vs " + std::to_string(SweepsNone) + ")");
			}
		}
		// Ranged lean their shots toward the favourite dodge side.
		{
			A::FThink In = MakeThink(2, 3);
			A::FBrainState State = Fresh(5);
			const A::FAction Shot = Decide(In, State, Mind, Side);
			Expect(Shot.Intent == EIntent::Attack && Shot.AimOffset.Y < -50.0 && Shot.Has(EBehavior::Adapt), "ranged shots lean to the commander's dodge side");
		}
		// Blockers get unblockables.
		{
			int Unblockable = 0;
			for (uint32_t Seed = 1; Seed <= 200; ++Seed)
			{
				A::FThink In = MakeThink(0, 3);
				In.View.bBlocking = true;
				A::FBrainState State = Fresh(Seed);
				const A::FAction Action = Decide(In, State, Mind, None);
				Unblockable += Action.bUnblockable || (Action.Move == EMove::Combo && Action.Enders[Action.FollowUps - 1] == A::EEnder::Unblockable) ? 1 : 0;
			}
			Expect(Unblockable > 100, "a blocking commander meets unblockables (" + std::to_string(Unblockable) + "/200)");
		}
		// Spam -> guard.
		{
			A::FHabits Spam;
			for (int Index = 0; Index < 10; ++Index)
			{
				A::FPlayerView Before = ViewAt(Index * 0.4);
				A::FPlayerView After = ViewAt(Index * 0.4 + 0.02);
				After.bAttacking = true;
				After.ActionStartedAt = Index * 0.4 + 0.01;
				Spam.Observe(Before, After);
			}
			int GuardSpam = 0;
			int GuardCalm = 0;
			for (uint32_t Seed = 1; Seed <= 300; ++Seed)
			{
				A::FThink In = MakeThink(0, 3);
				In.View.bAttacking = true;
				In.View.ActionStartedAt = 9.6;
				A::FBrainState S1 = Fresh(Seed);
				A::FBrainState S2 = Fresh(Seed);
				GuardSpam += Decide(In, S1, Mind, Spam).Intent == EIntent::Guard ? 1 : 0;
				GuardCalm += Decide(In, S2, Mind, None).Intent == EIntent::Guard ? 1 : 0;
			}
			Expect(GuardSpam > GuardCalm + 30, "basic-attack spam meets more guards (" + std::to_string(GuardSpam) + " vs " + std::to_string(GuardCalm) + ")");
		}
		// Waiting angels coordinate.
		{
			A::FThink In = MakeThink(0);
			In.Pos = {600.0, 300.0};
			A::FCrowd Crowd;
			std::vector<A::FCrowdAgent> Agents;
			for (int Index = 0; Index < 6; ++Index)
			{
				A::FCrowdAgent Agent;
				Agent.Id = Index == 0 ? In.Id : 50 + Index;
				Agent.Pos = Index == 0 ? In.Pos : A::Rotate({200.0, 0.0}, Index);
				Agent.Reach = 170.0;
				Agents.push_back(Agent);
			}
			Crowd.Update(In.Now, In.View, Agents, 0.6, 0.6);
			A::FBrainState State = Fresh(2);
			const A::FAction Wait = A::Think(In, Mind, None, Crowd, State);
			if (!Crowd.Slot(In.Id).bToken)
			{
				Expect(Wait.Has(EBehavior::Coordination) && Wait.Intent != EIntent::Attack, "an angel without a token waits at its ring point");
			}
			else
			{
				Expect(Wait.Intent == EIntent::Attack || Wait.Intent == EIntent::Approach, "a token holder presses");
			}
		}
	}

	// ------------------------------------------------------------------ fairness fuzz
	void TestFairnessFuzz()
	{
		A::ResetAiTuning();
		A::FMind Mind;
		for (int Counter = 0; Counter < A::CounterCount; ++Counter)
		{
			Mind.SetExpertise(static_cast<A::ECounter>(Counter), 1.0);   // the smartest the angels get
		}
		A::FHabits Habits;
		for (int Index = 0; Index < 12; ++Index)
		{
			A::FPlayerView Before = ViewAt(Index * 2.5);
			A::FPlayerView After = ViewAt(Index * 2.5 + 0.02);
			After.bDodging = Index % 2 == 0;
			After.bAttacking = Index % 2 == 1;
			After.ActionStartedAt = Index * 2.5 + 0.01;
			After.Vel = {0.0, 600.0};
			Habits.Observe(Before, After);
		}
		int Attacks = 0;
		bool bTelegraphs = true;
		bool bFeints = true;
		bool bEnders = true;
		std::set<int> MovesByRole[RoleCount];
		A::FRandom Random;
		Random.State = 12345u;
		for (int RoleIndex = 0; RoleIndex < RoleCount; ++RoleIndex)
		{
			for (int Chapter = 1; Chapter <= 13; Chapter += 3)
			{
				A::FBrainState State = Fresh(static_cast<uint32_t>(RoleIndex * 100 + Chapter));
				A::FCrowd Crowd;
				for (int Step = 0; Step < 4000; ++Step)
				{
					A::FThink In = MakeThink(RoleIndex, Chapter);
					In.Now = 10.0 + Step * 0.1;
					In.View.Time = In.Now - In.Knobs.ReactionSeconds;
					const double Distance = 50.0 + 1800.0 * Random.Next();
					In.Pos = A::Rotate({Distance, 0.0}, Random.Next() * 6.28);
					In.Facing = A::Normal(In.View.Pos - In.Pos);
					In.View.Facing = A::Rotate({1.0, 0.0}, Random.Next() * 6.28);
					In.View.bAttacking = Random.Next() < 0.3;
					In.View.bHeavyWindup = In.View.bAttacking && Random.Next() < 0.4;
					In.View.bRecovery = !In.View.bAttacking && Random.Next() < 0.2;
					In.View.bWhiffed = In.View.bRecovery && Random.Next() < 0.5;
					In.View.bAirborne = Random.Next() < 0.1;
					In.View.bBlocking = Random.Next() < 0.1;
					In.View.ActionStartedAt = In.Now - 0.5 - static_cast<double>(Step % 7);
					In.View.SecondsSinceSwitch = Random.Next() * 4.0;
					In.HealthFraction = Random.Next();
					In.StrainFraction = Random.Next();
					In.bPhaseTwo = Random.Next() < 0.5;
					In.NearbyAllies = static_cast<int>(Random.Next() * 3.0);
					In.AllyCentre = A::Rotate({600.0, 0.0}, Random.Next() * 6.28);
					A::FCrowdAgent Agent;
					Agent.Id = In.Id;
					Agent.Role = In.Role;
					Agent.Pos = In.Pos;
					Agent.Reach = In.Reach;
					A::FCrowdAgent Escort;   // a grunt fighting alongside (tokens, flanks)
					Escort.Id = 999;
					Escort.Pos = A::Rotate({300.0, 0.0}, Random.Next() * 6.28);
					Escort.Reach = 170.0;
					Crowd.Update(In.Now, In.View, {Agent, Escort}, 0.7, In.Knobs.Aggression);
					const A::FAction Action = A::Think(In, Mind, Habits, Crowd, State);
					if (Action.Intent == EIntent::Attack)
					{
						++Attacks;
						MovesByRole[RoleIndex].insert(static_cast<int>(Action.Move));
						bTelegraphs = bTelegraphs && Action.Windup >= In.Knobs.MinTelegraph - 1e-9 && Action.Windup >= 0.25;
						bFeints = bFeints && (Action.FeintDelay == 0.0 || Action.FeintDelay >= In.Knobs.MinTelegraph - 1e-9);
						bEnders = bEnders && Action.FollowUps >= 0 && Action.FollowUps <= 3;
						Crowd.Release(In.Id, In.Now + Action.Windup, In.Knobs.Aggression);
					}
				}
			}
		}
		Expect(Attacks > 1000, "the fuzz attacked (" + std::to_string(Attacks) + ")");
		Expect(bTelegraphs, "fairness: every attack telegraphs for at least MinTelegraph (>= 0.25 s)");
		Expect(bFeints, "fairness: a feint's real strike telegraphs again for at least MinTelegraph");
		Expect(bEnders, "combos stay short (<= 3 follow-ups)");
		// Uses the whole kit.
		const size_t MinMoves[RoleCount] = {6, 4, 3, 3, 5, 9, 9};
		for (int RoleIndex = 0; RoleIndex < RoleCount; ++RoleIndex)
		{
			Expect(MovesByRole[RoleIndex].size() >= MinMoves[RoleIndex], std::string(RoleNames[RoleIndex]) + ": uses its kit (" +
				std::to_string(MovesByRole[RoleIndex].size()) + " moves)");
		}
	}

	// ------------------------------------------------------------------ clean play beats every angel
	struct FDuelResult
	{
		bool bWon = false;
		double Seconds = 0.0;
		int HitsTaken = 0;
		int Attacks = 0;
		int Evasions = 0;
	};

	/**
	 * A scripted "clean" commander: reacts to each telegraph after a human 0.25 s, dodges so the strike lands in its 0.3 s of
	 * dodge invulnerability, attacks (300 cm reach, 0.4 s swings, every 4th a heavy) when no strike is coming. The angel sees
	 * the commander through the timeline with its reaction delay - exactly what the game does.
	 */
	FDuelResult Duel(int RoleIndex, int Chapter, double StartHealth, uint32_t Seed)
	{
		const FArchetype& Arch = Archetypes[RoleIndex];
		const ERole Role = Roles[RoleIndex];
		const bool bRanged = Role == ERole::Ranged || Role == ERole::Mage;
		A::FMind Mind;
		A::FHabits Habits;
		A::FCrowd Crowd;
		A::FPlayerTimeline Timeline;
		A::FBrainState State = Fresh(Seed);
		const A::FKnobs Knobs = A::EffectiveKnobs(A::TierFor(Role), Chapter);
		const double Dt = 1.0 / 30.0;
		const double HumanReaction = 0.25;
		const double PlayerDps = 500.0;   // one operator's basic string; long enough a fight to see the whole brain
		const double PlayerSpeed = 600.0;

		FV2 PlayerPos{0.0, 0.0};
		FV2 PlayerVel;
		FV2 AngelPos{bRanged ? 1000.0 : 600.0, 0.0};
		FV2 AngelFacing{-1.0, 0.0};
		double Health = Arch.Health * StartHealth;
		FDuelResult Result;

		// Angel action in flight.
		bool bWinding = false;
		std::vector<double> HitTimes;      // when the strikes land
		std::vector<double> FlashTimes;    // when each strike's telegraph starts
		double BusyUntil = 0.0;
		double GuardUntil = -1.0;
		double EvadeUntil = -1.0;
		FV2 EvadeDir;
		// Commander.
		double DodgeAt = -1.0;
		double InvulnerableUntil = -1.0;
		double SwingStart = -100.0;
		double SwingEnd = -100.0;
		int Swings = 0;
		bool bSwingHeavy = false;
		bool bSwingConnected = false;
		bool bSwingHit = false;   // the swing struck the angel (a swing that struck nothing is a whiff)

		for (double Now = 0.0; Now < 180.0; Now += Dt)
		{
			// The commander's view of the world: the next strike it saw coming.
			double NextHit = 1e9;
			for (size_t Index = 0; Index < HitTimes.size(); ++Index)
			{
				if (FlashTimes[Index] + HumanReaction <= Now && HitTimes[Index] >= Now)
				{
					NextHit = std::min(NextHit, HitTimes[Index]);
				}
			}
			if (NextHit < 1e8 && DodgeAt < 0.0 && InvulnerableUntil < NextHit)
			{
				DodgeAt = std::max(Now, NextHit - 0.1);
			}
			if (DodgeAt >= 0.0 && Now >= DodgeAt)
			{
				InvulnerableUntil = Now + 0.3;
				DodgeAt = -1.0;
			}
			const bool bDodging = Now < InvulnerableUntil;

			const FV2 ToAngel = AngelPos - PlayerPos;
			const double Dist = A::Length(ToAngel);
			const bool bThreatened = NextHit < 1e8;
			PlayerVel = {};
			if (!bDodging && Now >= SwingEnd)
			{
				if (bThreatened && NextHit - Now < 0.5)
				{
					// wait for the strike, ready to dodge
				}
				else if (Dist > 260.0)
				{
					PlayerVel = A::Normal(ToAngel) * PlayerSpeed;
				}
				else
				{
					++Swings;
					bSwingHeavy = Swings % 4 == 0;
					SwingStart = Now;
					SwingEnd = Now + (bSwingHeavy ? 0.7 : 0.4);
					bSwingConnected = false;
					bSwingHit = false;
				}
			}
			if (bDodging)
			{
				PlayerVel = A::Perp(A::Normal(ToAngel)) * PlayerSpeed;
			}
			PlayerPos = PlayerPos + PlayerVel * Dt;
			// The swing lands at its end (heavy) or half-way (basic) if the angel is in reach and not evading / guarding.
			const double SwingHitAt = SwingStart + (bSwingHeavy ? 0.5 : 0.2);
			if (!bSwingConnected && Now >= SwingHitAt && Now < SwingEnd)
			{
				bSwingConnected = true;
				const bool bInReach = A::Length(AngelPos - PlayerPos) <= 320.0;
				const double Damage = PlayerDps * (bSwingHeavy ? 0.7 : 0.4) * (bSwingHeavy ? 1.6 : 1.0);
				if (bInReach && Now >= EvadeUntil)
				{
					Health -= Now < GuardUntil ? Damage * 0.2 : Damage;
					bSwingHit = true;
				}
			}
			// A whiff: the swing landed on nothing (the angel was out of reach or evading). Was "!bSwingConnected", which is
			// always false once the swing resolves, so the duels never showed the angels a whiff to punish.
			const bool bWhiffNow = Now >= SwingHitAt && Now < SwingEnd && !bSwingHit;

			A::FPlayerView View;
			View.Time = Now;
			View.Pos = PlayerPos;
			View.Vel = PlayerVel;
			View.Facing = Dist > 1.0 ? A::Normal(ToAngel) : FV2{1.0, 0.0};
			View.bAttacking = Now >= SwingStart && Now < SwingHitAt;
			View.bHeavyWindup = View.bAttacking && bSwingHeavy;
			View.bRecovery = Now >= SwingHitAt && Now < SwingEnd;
			View.bWhiffed = View.bRecovery && bWhiffNow;
			View.bDodging = bDodging;
			View.ThreatReach = 320.0;
			View.ActionStartedAt = SwingStart;
			const A::FPlayerView Before = Timeline.Empty() ? View : Timeline.At(Now - Dt);
			Timeline.Record(View);
			Habits.Observe(Before, View);
			Mind.Tick(Dt, false);

			if (Health <= 0.0)
			{
				Result.bWon = true;
				Result.Seconds = Now;
				return Result;
			}

			// Strikes land.
			for (size_t Index = 0; Index < HitTimes.size();)
			{
				if (Now >= HitTimes[Index])
				{
					const double Reach = bRanged ? 1e9 : Arch.Reach + 60.0;
					if (Now >= InvulnerableUntil && A::Length(AngelPos - PlayerPos) <= Reach)
					{
						++Result.HitsTaken;
					}
					HitTimes.erase(HitTimes.begin() + static_cast<std::ptrdiff_t>(Index));
					FlashTimes.erase(FlashTimes.begin() + static_cast<std::ptrdiff_t>(Index));
					continue;
				}
				++Index;
			}
			if (bWinding && HitTimes.empty())
			{
				bWinding = false;
				Crowd.Release(1, Now, Knobs.Aggression);
			}

			// Evasion movement.
			if (Now < EvadeUntil)
			{
				AngelPos = AngelPos + EvadeDir * (A::AiTuning().SidestepDistance / 0.3 * Dt);
				continue;
			}
			if (bWinding || Now < BusyUntil)
			{
				continue;
			}

			A::FThink In;
			In.Id = 1;
			In.Now = Now;
			In.Interval = Arch.Interval;
			In.Role = Role;
			In.Tier = A::TierFor(Role);
			In.Knobs = Knobs;
			In.Pos = AngelPos;
			In.Facing = AngelFacing;
			In.Reach = Arch.Reach;
			In.KeepMin = Arch.KeepMin;
			In.KeepMax = Arch.KeepMax;
			In.BaseWindup = Arch.Windup;
			In.HealthFraction = Health / Arch.Health;
			In.View = Timeline.At(Now - Knobs.ReactionSeconds);
			In.CommanderNow = PlayerPos;
			In.bHealing = Timeline.Healing(Now - Knobs.ReactionSeconds);
			A::FCrowdAgent Agent;
			Agent.Id = 1;
			Agent.Role = Role;
			Agent.Pos = AngelPos;
			Agent.Reach = Arch.Reach;
			Agent.bCommitted = bWinding;
			Crowd.Update(Now, In.View, {Agent}, Mind.Adaptation(), Knobs.Aggression);
			const A::FAction Action = A::Think(In, Mind, Habits, Crowd, State);
			AngelFacing = A::Normal(PlayerPos - AngelPos);
			switch (Action.Intent)
			{
			case EIntent::Attack:
			{
				++Result.Attacks;
				bWinding = true;
				double At = Now + Action.Windup + Action.FeintDelay;
				FlashTimes.push_back(Action.FeintDelay > 0.0 ? Now + Action.Windup : Now);
				HitTimes.push_back(At);
				Habits.NoteTelegraph(Now, At);
				for (int Step = 0; Step < Action.FollowUps; ++Step)
				{
					const double Telegraph = Knobs.MinTelegraph + (Action.Enders[Step] == A::EEnder::Delayed ? A::AiTuning().FeintExtraDelay : 0.0);
					FlashTimes.push_back(At + 0.05);
					At += 0.05 + Telegraph;
					HitTimes.push_back(At);
				}
				break;
			}
			case EIntent::Sidestep:
			case EIntent::Backstep:
				++Result.Evasions;
				EvadeUntil = Now + 0.3;
				EvadeDir = A::Normal(Action.Goal);
				break;
			case EIntent::Guard:
				GuardUntil = Now + 0.6;
				BusyUntil = Now + 0.6;
				break;
			default:
			{
				const FV2 Goal = Action.Intent == EIntent::Strafe ? AngelPos + A::Perp(A::Normal(PlayerPos - AngelPos)) * (100.0 * Action.StrafeSign) : Action.Goal;
				const FV2 Step = Goal - AngelPos;
				const double Move = std::min(A::Length(Step), Arch.Speed * Action.Speed * Dt);
				AngelPos = AngelPos + A::Normal(Step) * Move;
				break;
			}
			}
		}
		Result.Seconds = 180.0;
		return Result;
	}

	// Bosses and mini bosses punish real whiffs in a duel (the Mac PIE run showed none: the game hid every whiff behind
	// damage-over-time ticks; BIBAngelView now ignores ticks).
	void TestDuelPunishes()
	{
		A::ResetAiTuning();
		for (const ERole Role : {ERole::MiniBoss, ERole::Boss})
		{
			const int RoleIndex = Role == ERole::MiniBoss ? 5 : 6;
			int Punishes = 0;
			for (uint32_t Seed = 1; Seed <= 10; ++Seed)
			{
				A::ResetAiStats();
				Duel(RoleIndex, 1, 1.0, Seed);
				Punishes += A::AiStats().Punishes;
			}
			Expect(Punishes > 0, std::string(RoleNames[RoleIndex]) + ": punishes whiffs in duels (" + std::to_string(Punishes) + " in 10)");
		}
	}

	void TestCleanPlayWins()
	{
		A::ResetAiTuning();
		for (int RoleIndex = 0; RoleIndex < RoleCount; ++RoleIndex)
		{
			const ERole Role = Roles[RoleIndex];
			// Bosses start pre-damaged (the test fights the last stretch; their health pools are tuned for a full team).
			const double Start = Role == ERole::Boss ? 0.04 : (Role == ERole::MiniBoss ? 0.1 : 1.0);
			for (int Chapter : {1, 13})
			{
				for (uint32_t Seed = 1; Seed <= 5; ++Seed)
				{
					const FDuelResult Result = Duel(RoleIndex, Chapter, Start, Seed);
					const std::string Where = std::string(RoleNames[RoleIndex]) + " chapter " + std::to_string(Chapter) + " seed " + std::to_string(Seed);
					Expect(Result.bWon, Where + ": clean play wins (" + std::to_string(Result.Seconds) + " s)");
					Expect(Result.HitsTaken == 0, Where + ": clean play takes no hits (" + std::to_string(Result.HitsTaken) + " taken, " +
						std::to_string(Result.Attacks) + " attacks)");
					Expect(Result.Attacks > 0, Where + ": the angel fought back");
					if (Seed == 1 && Chapter == 1)
					{
						std::printf("  duel %-8s chapter %2d: won in %5.1f s, %d attacks dodged, %d evasions by the angel\n", RoleNames[RoleIndex], Chapter,
							Result.Seconds, Result.Attacks, Result.Evasions);
					}
				}
			}
		}
	}
}

int main(int ArgCount, char** Args)
{
	const char* IniPath = ArgCount > 1 ? Args[1] : nullptr;
	TestMath();
	TestKnobs();
	TestConfig(IniPath);
	TestTimeline();
	TestHabits();
	TestCrowd();
	TestBehaviors();
	TestAdaptation();
	TestFairnessFuzz();
	// Every angel type shows every behaviour of the brief somewhere in the scenarios and the fuzz (FAiStats, as in game).
	{
		const A::FAiStats& Stats = A::AiStats();
		for (int RoleIndex = 0; RoleIndex < RoleCount; ++RoleIndex)
		{
			for (int Behavior = 0; Behavior < A::BehaviorCount; ++Behavior)
			{
				Expect(Stats.Behaviors[RoleIndex][Behavior] > 0, std::string(RoleNames[RoleIndex]) + " shows " +
					A::BehaviorName(static_cast<EBehavior>(Behavior)) + " (" + std::to_string(Stats.Behaviors[RoleIndex][Behavior]) + ")");
			}
		}
	}
	TestCleanPlayWins();
	TestDuelPunishes();
	std::printf("angel brain tests: %d checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
