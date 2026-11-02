// Tests for the hit-feel rules (owner request 2026-10-13: every player attack should feel physical - BIBRules/BIBHitFeel.h)
// and for the hit origins the kit runtime hands the game (BIBRules/BIBKitRuntime.h FHitInfo).
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh (passes Config/DefaultGame.ini as the first argument).
#include "../Source/BoldInBeautyDemo/BIBRules/BIBHitFeel.h"
#include "kit_fake_world.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
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

	namespace F = BIB::Feel;
	namespace S = BIB::Stagger;
	using F::EReaction;
	using F::ESource;
	using F::EWeight;

	F::FHitSource Hit(ESource Source, bool bFinal = false, double Ratio = 1.0, bool bTick = false)
	{
		F::FHitSource Out;
		Out.Source = Source;
		Out.bFinal = bFinal;
		Out.Ratio = Ratio;
		Out.bTick = bTick;
		return Out;
	}

	double Frames(EWeight Weight)
	{
		return F::FeelFor(Weight).HitstopFrames;
	}

	void TestClassify()
	{
		Expect(F::Classify(Hit(ESource::Basic)) == EWeight::Light, "a basic hit is light");
		Expect(F::Classify(Hit(ESource::Basic, true)) == EWeight::Heavy, "a basic string's last hit is heavy");
		Expect(F::Classify(Hit(ESource::Move, false, 1.0)) == EWeight::Medium, "an ordinary ability hit is medium");
		Expect(F::Classify(Hit(ESource::Move, false, 3.0)) == EWeight::Heavy, "a big ability hit is heavy");
		Expect(F::Classify(Hit(ESource::Move, false, 8.0)) == EWeight::Finisher, "a huge ability hit is a finisher");
		Expect(F::Classify(Hit(ESource::Special, true, 1.0)) == EWeight::Heavy, "a Special's last hit lands heavy");
		Expect(F::Classify(Hit(ESource::Ultimate)) == EWeight::Ultimate, "ultimate hits");
		Expect(F::Classify(Hit(ESource::Ultimate, true)) == EWeight::UltimateFinisher, "the ultimate's last hit is its finisher");
		Expect(F::Classify(Hit(ESource::Ultimate, true, 1.0, true)) == EWeight::UltimateFinisher, "an ultimate zone's last tick is its finisher");
		Expect(F::Classify(Hit(ESource::Ultimate, false, 1.0, true)) == EWeight::Tick, "other ultimate ticks are ticks");
		Expect(F::Classify(Hit(ESource::Move, false, 5.0, true)) == EWeight::Tick, "zone ticks are ticks");
		Expect(F::Classify(Hit(ESource::Counter, false, 3.0)) == EWeight::Heavy, "counters are heavy");
		Expect(F::Classify(Hit(ESource::Unknown, false, 0.1)) == EWeight::Tick, "tiny unknown hits (statuses) are ticks");
		Expect(F::Classify(Hit(ESource::Unknown, false, 3.0)) == EWeight::Heavy, "big unknown hits are heavy");
		F::FHitSource Forced = Hit(ESource::Basic);
		Forced.Override = static_cast<int>(EWeight::Finisher);
		Expect(F::Classify(Forced) == EWeight::Finisher, "kit data can force a weight");
	}

	void TestTargets()
	{
		// The owner's feel targets (2026-10-13): light 2-4 frames, heavy 6-10, ultimate finisher 12+.
		Expect(Frames(EWeight::Light) >= 2.0 && Frames(EWeight::Light) <= 4.0, "light hit stop is 2-4 frames");
		Expect(Frames(EWeight::Heavy) >= 6.0 && Frames(EWeight::Heavy) <= 10.0, "heavy hit stop is 6-10 frames");
		Expect(Frames(EWeight::Finisher) >= 6.0 && Frames(EWeight::Finisher) <= 10.0, "finisher hit stop is 6-10 frames");
		Expect(Frames(EWeight::UltimateFinisher) >= 12.0, "the ultimate finisher's hit stop is 12+ frames");
		Expect(Frames(EWeight::Tick) == 0.0, "ticks never freeze");
		for (int Index = 1; Index < F::WeightCount - 1; ++Index)
		{
			if (Index == static_cast<int>(EWeight::Ultimate))
			{
				continue;
			}
			Expect(Frames(static_cast<EWeight>(Index + 1)) >= Frames(static_cast<EWeight>(Index)) || Index + 1 == static_cast<int>(EWeight::Ultimate),
				std::string("hit stop grows with weight: ") + F::WeightName(static_cast<EWeight>(Index)));
		}
		Expect(std::abs(F::HitstopSeconds(EWeight::Light, false) - 3.0 / 60.0) < 1e-9, "frames convert at 60 fps");
		Expect(F::HitstopSeconds(EWeight::UltimateFinisher, false) <= F::Tuning().MaxHitstopSeconds, "hit stop is capped");
		Expect(F::FeelFor(EWeight::Heavy).PushInCm > 0.0 && F::FeelFor(EWeight::Light).PushInCm == 0.0, "push-in on heavies only");
		Expect(F::FeelFor(EWeight::UltimateFinisher).KickDegrees > F::FeelFor(EWeight::Heavy).KickDegrees, "the ultimate finisher kicks hardest");
		Expect(F::FeelFor(EWeight::Light).RumbleIntensity > 0.0 && F::FeelFor(EWeight::UltimateFinisher).RumbleIntensity <= 1.0, "rumble in range");
		Expect(F::Tuning().InputBufferSeconds >= 0.15 && F::Tuning().InputBufferSeconds <= 0.2, "input buffer 150-200 ms");
		for (int Step = 0; Step < S::ComboLength; ++Step)
		{
			Expect(F::Tuning().ComboHitAt[Step] < F::Tuning().ComboStepSeconds[Step], "each string step hits before it ends");
		}
		Expect(F::Tuning().ComboHitAt[S::ComboLength - 1] > F::Tuning().ComboHitAt[0], "the finisher winds up longer (rhythm)");
	}

	void TestReactions()
	{
		const S::EWeight Bodies[] = {S::EWeight::Light, S::EWeight::Medium, S::EWeight::Heavy};
		// No enemy ignores a heavy hit: every body reacts to Heavy and above.
		for (const S::EWeight Body : Bodies)
		{
			for (int Index = static_cast<int>(EWeight::Heavy); Index < F::WeightCount; ++Index)
			{
				S::FMeter Meter;
				Meter.Reset(S::DazeMax(Body, false));
				const EReaction Reaction = F::ResolveReaction(Meter, Body, static_cast<EWeight>(Index), 10.0);
				Expect(Reaction != EReaction::None, std::string("every enemy reacts to ") + F::WeightName(static_cast<EWeight>(Index)));
			}
		}
		// Light enemies are interrupted by a light hit; the AI stands still while it plays.
		{
			S::FMeter Meter;
			Meter.Reset(S::DazeMaxLight);
			Expect(F::ResolveReaction(Meter, S::EWeight::Light, EWeight::Light, 1.0) == EReaction::Flinch, "a grunt flinches on a light hit");
			Expect(Meter.Flinching(1.1), "the flinch stops the grunt");
			Expect(F::ResolveReaction(Meter, S::EWeight::Light, EWeight::Finisher, 2.0) == EReaction::Launch, "a finisher launches a grunt");
			S::FMeter Down;
			Down.Reset(S::DazeMaxLight);
			Expect(F::ResolveReaction(Down, S::EWeight::Light, EWeight::UltimateFinisher, 1.0) == EReaction::Knockdown, "an ultimate finisher knocks down");
			Expect(Down.Flinching(1.0 + F::Tuning().Reaction[static_cast<int>(EReaction::Knockdown)].Seconds - 0.01), "the knockdown lasts its time");
		}
		// Light spam builds poise; heavies still land.
		{
			S::FMeter Meter;
			Meter.Reset(S::DazeMaxLight);
			int Interrupts = 0;
			for (int Index = 0; Index < 12; ++Index)
			{
				Interrupts += F::ResolveReaction(Meter, S::EWeight::Light, EWeight::Light, 1.0 + Index * 0.1) == EReaction::Flinch ? 1 : 0;
			}
			Expect(Interrupts <= S::FlinchesBeforePoise, "a crowd cannot be locked forever by light hits");
			Expect(F::ResolveReaction(Meter, S::EWeight::Light, EWeight::Heavy, 2.3) == EReaction::Knockback, "a heavy hit breaks through poise");
		}
		// Bosses: recoil on light hits, an interrupt on heavies at most every BossInterruptCooldown seconds.
		{
			S::FMeter Meter;
			Meter.Reset(S::DazeMaxBoss);
			Expect(F::ResolveReaction(Meter, S::EWeight::Heavy, EWeight::Light, 1.0) == EReaction::Recoil, "a boss recoils on a light hit");
			Expect(!Meter.Flinching(1.01), "a recoil does not stop the boss");
			Expect(F::ResolveReaction(Meter, S::EWeight::Heavy, EWeight::Heavy, 2.0) == EReaction::Flinch, "a heavy hit interrupts a boss");
			Expect(F::ResolveReaction(Meter, S::EWeight::Heavy, EWeight::Finisher, 3.0) == EReaction::Recoil, "... but not again straight away");
			Expect(F::ResolveReaction(Meter, S::EWeight::Heavy, EWeight::Finisher, 2.0 + F::Tuning().BossInterruptCooldown + 0.1) == EReaction::Stagger,
				"... until the cooldown passed");
		}
		// Brutes shrug off light hits (recoil) and stagger on heavies.
		{
			S::FMeter Meter;
			Meter.Reset(S::DazeMaxBrute);
			Expect(F::ResolveReaction(Meter, S::EWeight::Medium, EWeight::Light, 1.0) == EReaction::Recoil, "a brute recoils on a light hit");
			Expect(F::ResolveReaction(Meter, S::EWeight::Medium, EWeight::Heavy, 2.0) == EReaction::Stagger, "a brute staggers on a heavy hit");
		}
		// A broken (stunned) enemy only recoils; its break timer is not touched.
		{
			S::FMeter Meter;
			Meter.Reset(S::DazeMaxLight);
			Meter.Add(1000.0, 1.0, 3.0);
			Expect(F::ResolveReaction(Meter, S::EWeight::Light, EWeight::Finisher, 1.5) == EReaction::Recoil, "a broken enemy recoils");
		}
		Expect(F::ReactionTilt(EReaction::Knockdown, 0.5) > 60.0, "a knocked-down enemy lies on the floor");
		Expect(F::ReactionTilt(EReaction::Flinch, 10.0) == 0.0, "reactions end upright");
		Expect(F::ReactionTilt(EReaction::None, 0.1) == 0.0, "no reaction, no tilt");
	}

	void TestPoses()
	{
		const F::FPose Windup = F::EvaluatePose(0.05, 0.1, 0.2, 1.0, -1.0, 0.0);
		Expect(Windup.LeanDegrees < 0.0 && Windup.Squash < 1.0, "the wind-up leans back and squashes");
		const F::FPose Impact = F::EvaluatePose(0.1, 0.1, 0.2, 1.0, 0.0, 18.0);
		Expect(Impact.LeanDegrees > 10.0 && Impact.Forward > 0.0, "the impact lunges into the hit");
		const F::FPose Done = F::EvaluatePose(1.0, 0.1, 0.2, 1.0, 1.0, 18.0);
		Expect(std::abs(Done.LeanDegrees) < 0.5 && std::abs(Done.Squash - 1.0) < 0.01, "the recovery returns to neutral");
		const F::FPose Instant = F::EvaluatePose(0.02, 0.0, 0.2, 1.0, -1.0, 0.0);
		Expect(Instant.LeanDegrees < 0.0, "even an instant cast shows a short wind-up");
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
		F::ResetTuning();
		F::FTuning& T = F::Tuning();
		std::set<std::string> Names;
		for (const F::FConfigKey& Key : F::ConfigKeys(T))
		{
			Expect(Names.insert(Key.Name).second, "config key is unique: " + Key.Name);
			Expect(Key.Value != nullptr, "config key has a value: " + Key.Name);
		}
		for (const F::FReactionKey& Key : F::ReactionKeys(T))
		{
			Expect(Names.insert(Key.Name).second, "reaction key is unique: " + Key.Name);
		}
		F::EReaction Parsed = EReaction::None;
		Expect(F::ParseReaction("knockdown", Parsed) && Parsed == EReaction::Knockdown, "reaction names parse");
		Expect(!F::ParseReaction("Explode", Parsed), "unknown reaction names are rejected");
		if (!IniPath)
		{
			return;
		}
		// Every number lives in Config/DefaultGame.ini [BIB.CombatFeel] with the same default, and the file has no stray keys.
		const std::map<std::string, std::string> Ini = ReadSection(IniPath, "BIB.CombatFeel");
		Expect(!Ini.empty(), "DefaultGame.ini has a [BIB.CombatFeel] section");
		for (const F::FConfigKey& Key : F::ConfigKeys(T))
		{
			const auto Found = Ini.find(Key.Name);
			Expect(Found != Ini.end(), "DefaultGame.ini sets " + Key.Name);
			if (Found != Ini.end())
			{
				Expect(std::abs(std::stod(Found->second) - *Key.Value) < 1e-6, "DefaultGame.ini matches the default of " + Key.Name);
			}
		}
		for (const F::FReactionKey& Key : F::ReactionKeys(T))
		{
			const auto Found = Ini.find(Key.Name);
			Expect(Found != Ini.end(), "DefaultGame.ini sets " + Key.Name);
			F::EReaction Value = EReaction::None;
			Expect(Found != Ini.end() && F::ParseReaction(Found->second, Value) && Value == *Key.Value, "DefaultGame.ini matches the default of " + Key.Name);
		}
		for (const auto& Pair : Ini)
		{
			if (Pair.first.rfind("Sound.", 0) == 0)
			{
				continue;   // sound asset paths (strings, read by the Unreal layer)
			}
			Expect(Names.count(Pair.first) == 1, "DefaultGame.ini key is known: " + Pair.first);
		}
	}

	// ------------------------------------------------------------------ hit origins from the kit runtime
	class RecordingWorld : public KitTest::FakeWorld
	{
	public:
		std::vector<BIB::Kits::FHitInfo> Infos;
		double DamageEnemyHit(int Id, double Amount, unsigned Flags, const BIB::Kits::FHitInfo& Info) override
		{
			Infos.push_back(Info);
			return DamageEnemy(Id, Amount, Flags);
		}
	};

	void TestKitHitInfo()
	{
		using namespace BIB::Kits;
		int UltsWithFinisher = 0;
		int UltsDealingDamage = 0;
		for (const FKitDef* Kit : AllKits())
		{
			// Basic string: the last hit of the kit's own string is final, the others are not.
			{
				RecordingWorld World;
				World.Attack = Kit->BaseAttack;
				World.AddEnemy(150.0, 0.0, 1e9);
				FRuntime Runtime(*Kit, World);
				Runtime.ResetForFight();
				const int Length = std::max(1, Kit->Abilities[static_cast<int>(ESlotId::Basic)].ComboLength);
				int Finals = 0;
				int BasicHits = 0;
				for (int Press = 0; Press < Length; ++Press)
				{
					World.Infos.clear();
					World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
					Runtime.Cast(ESlotId::Basic);
					World.Run(0.4, Runtime);
					for (const FHitInfo& Info : World.Infos)
					{
						if (Info.Slot == ESlotId::Basic)
						{
							++BasicHits;
							Finals += Info.bFinal ? 1 : 0;
							Expect(Info.ComboLength == Length, Kit->Name + " basic hits carry the string length");
							Expect(Info.bFinal == (Length > 1 && Info.ComboIndex == Length - 1), Kit->Name + " basic finisher = last hit of its string");
						}
					}
				}
				Expect(BasicHits > 0, Kit->Name + " basic attack reports its hits");
				Expect(Length == 1 ? Finals == 0 : Finals >= 1, Kit->Name + " basic string has one finisher per string");
			}
			// Ultimate: if it deals damage, exactly its last damaging moment is final.
			{
				RecordingWorld World;
				World.Attack = Kit->BaseAttack;
				World.AddEnemy(200.0, 0.0, 1e9, true);
				World.AddEnemy(300.0, 120.0, 1e9);
				FRuntime Runtime(*Kit, World);
				Runtime.ResetForFight();
				World.Ult = 100.0;
				Runtime.Cast(ESlotId::Ultimate);
				World.Run(std::max(1.0, Runtime.Ability(ESlotId::Ultimate).TotalSeconds + 1.0), Runtime);
				int UltHits = 0;
				int Finals = 0;
				bool bFinalSeen = false;
				bool bHitAfterFinal = false;
				for (const FHitInfo& Info : World.Infos)
				{
					if (Info.Slot != ESlotId::Ultimate)
					{
						continue;
					}
					++UltHits;
					if (Info.bFinal)
					{
						++Finals;
						bFinalSeen = true;
					}
					else if (bFinalSeen && !Info.bTick)
					{
						bHitAfterFinal = true;
					}
				}
				if (UltHits > 0)
				{
					++UltsDealingDamage;
					UltsWithFinisher += Finals > 0 ? 1 : 0;
					Expect(Finals > 0, Kit->Name + " ultimate has a finisher hit");
					Expect(!bHitAfterFinal, Kit->Name + " ultimate's finisher is its last hit");
				}
			}
		}
		std::printf("ultimates with a finisher: %d of %d that deal damage directly\n", UltsWithFinisher, UltsDealingDamage);
	}

	void TestRecoveryCancel()
	{
		using namespace BIB::Kits;
		// Kuu's Ground Pound locks for 1.0 s and hits at 0.5 s: its recovery (after the hit) can be cancelled into a dodge.
		const FKitDef* Kuu = FindKitByName("Kuu");
		Expect(Kuu != nullptr, "Kuu exists");
		if (!Kuu)
		{
			return;
		}
		RecordingWorld World;
		World.Attack = Kuu->BaseAttack;
		World.AddEnemy(150.0, 0.0, 1e9);
		FRuntime Runtime(*Kuu, World);
		Runtime.ResetForFight();
		const FAbilityDef& Pound = Runtime.Ability(ESlotId::Move3);
		Runtime.Cast(ESlotId::Move3);
		World.Run(0.1, Runtime);
		Expect(Runtime.MovementLocked(), "Ground Pound locks the caster");
		Expect(!Runtime.InRecovery(F::Tuning().CancelGraceSeconds), "no cancel before the hit");
		double LastHit = 0.0;
		for (const FStep& Step : Pound.Steps)
		{
			if (StepDealsDamage(Step))
			{
				LastHit = std::max(LastHit, Step.At + (Step.Interval > 0.0 ? Step.Duration : 0.0));
			}
		}
		World.Run(LastHit - 0.1 + F::Tuning().CancelGraceSeconds + 0.03, Runtime);
		if (LastHit + F::Tuning().CancelGraceSeconds + 0.05 < Pound.LockSeconds)
		{
			Expect(Runtime.InRecovery(F::Tuning().CancelGraceSeconds), "after the hit the cast is in recovery");
			Runtime.CancelRecovery();
			Expect(!Runtime.MovementLocked(), "a dodge cancels the recovery");
		}
		// A buff without damage keeps its lock (only attacks have a recovery to cancel).
		const FKitDef* Myrcia = FindKitByName("Myrcia");
		if (Myrcia)
		{
			RecordingWorld Other;
			Other.AddEnemy(150.0, 0.0, 1e9);
			FRuntime MyrciaRuntime(*Myrcia, Other);
			MyrciaRuntime.ResetForFight();
			MyrciaRuntime.Cast(ESlotId::Move2);   // Cleave: hits at 0.35 s, lock 0.5 s
			Other.Run(0.1, MyrciaRuntime);
			Expect(!MyrciaRuntime.InRecovery(F::Tuning().CancelGraceSeconds), "Cleave cannot be cancelled before its hit");
		}
	}
}

int main(int Argc, char** Argv)
{
	TestClassify();
	TestTargets();
	TestReactions();
	TestPoses();
	TestConfig(Argc > 1 ? Argv[1] : nullptr);
	TestKitHitInfo();
	TestRecoveryCancel();
	std::printf("%d feel checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
