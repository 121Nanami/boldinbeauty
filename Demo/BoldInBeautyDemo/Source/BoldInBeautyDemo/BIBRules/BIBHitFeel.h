// Boldinbeauty demo — how a hit FEELS (owner request 2026-10-13: "rework every player attack so hits feel physical and real",
// reference: how Zenless Zone Zero's combat feels - snappy, heavy, stylish, readable - without copying its names or moves).
// Engine-independent; the Unreal layer is BIBGame/BIBPlayerFeel.cpp (attacker side, camera, rumble, sound, poses) and
// BIBGame/BIBEnemy.cpp (target side: freeze, reaction, Strain).
//
// Every hit of the commander's team is classified into a WEIGHT (tick, light, medium, heavy, finisher, ultimate, ultimate
// finisher). The weight decides, from one table:
//   * hit stop in frames at 60 fps - the attacker and the target freeze together (light 3, heavy 8, ultimate finisher 14),
//   * the target's reaction, scaled by how heavy the target is (flinch, stagger, knockback, launch, knockdown; bosses recoil
//     and only stagger on big hits so they stay threatening - never "ignore" a heavy hit),
//   * a directional camera kick, a camera push-in on heavies, controller rumble, spark size, a sound layer and extra Strain.
// A small pose model gives the operators' bodies (no animation clips exist yet) a wind-up, an impact and a recovery.
//
// Names used in game (owner 2026-10-13: the assistant picks them, "Perfect Dodge" stays): the enemy meter is STRAIN (was
// "Daze"); a full meter BREAKS the enemy (was "stunned"); switching in on a broken enemy is a RELAY STRIKE (was "chain
// attack"); the counter after a Perfect Dodge is the PHANTOM COUNTER (was "dodge counter"). Code identifiers keep the old
// names (BIBStagger.h "Daze") so the rules and saves stay stable.
//
// Every value is a default: Config/DefaultGame.ini [BIB.CombatFeel] overrides it (ConfigKeys() lists the keys; the Unreal
// layer loads them at start and on the console command BIBFeelReload). All numbers PROPOSED (owner may tune).
#pragma once

#include "BIBStagger.h"

#include <string>
#include <vector>

namespace BIB
{
	namespace Feel
	{
		enum class EWeight : int
		{
			Tick = 0,          // damage over time, zone / channel ticks: spark only
			Light,             // basic-attack hits
			Medium,            // ability hits
			Heavy,             // basic finishers, big ability hits, counters
			Finisher,          // the biggest non-ultimate hits (relay strike, very large ability hits)
			Ultimate,          // ultimate hits before the last one
			UltimateFinisher,  // the ultimate's last / biggest hit
			Count
		};
		constexpr int WeightCount = static_cast<int>(EWeight::Count);

		enum class EReaction : int
		{
			None = 0,
			Recoil,     // a visible jolt and a small push; the enemy keeps acting (bosses, brutes on light hits)
			Flinch,     // interrupts the attack windup
			Stagger,    // longer interrupt, pushed back
			Knockback,  // thrown back along the ground
			Launch,     // thrown into the air
			Knockdown,  // thrown down; lies on the floor before getting up
			Count
		};
		constexpr int ReactionCount = static_cast<int>(EReaction::Count);

		// Where a hit came from (filled by the kit runtime for data kits, by the hand-written kits for Luna / Judge / Narrator).
		enum class ESource : int
		{
			Unknown = -1,  // passives, statuses, reflect: classified by size
			Basic = 0,
			Move,          // Move 1-3
			Special,
			Ultimate,
			Counter        // perfect-dodge Phantom Counter, parry riposte, dash attack, switch strike
		};

		struct FHitSource
		{
			ESource Source = ESource::Unknown;
			bool bTick = false;           // zone / channel / damage-over-time tick
			bool bFinal = false;          // the cast's last damaging hit (or the string's last hit for basics)
			double Ratio = 1.0;           // damage dealt / the operator's attack
			int Override = -1;            // EWeight forced by the kit data (FAbilityDef::FeelWeight), -1 = classify
		};

		struct FWeightFeel
		{
			double HitstopFrames = 0.0;   // both attacker and target freeze this long (frames at FrameRate)
			double KickDegrees = 0.0;     // directional camera kick
			double KickSeconds = 0.0;
			double PushInCm = 0.0;        // camera arm shortened for a moment (heavies)
			double RumbleIntensity = 0.0; // 0..1 controller rumble
			double RumbleSeconds = 0.0;
			double SparkScale = 0.0;      // hit spark size (x the standard spark)
			double StrainBonus = 0.0;     // extra Strain on top of the size-based amount (BIBStagger.h DazeForHit)
			double ImpactLean = 0.0;      // degrees the operator's body leans into the hit
			bool bAccentFrame = false;    // force a fresh frame of the 12 fps look on the hit (the hit is never "between frames")
			bool bImpactFrame = false;    // two-tone impact frame
			EReaction VsLight = EReaction::None;   // grunt, ranged, mage, assassin
			EReaction VsMedium = EReaction::None;  // brute
			EReaction VsHeavy = EReaction::None;   // mini boss, boss (interrupts limited by BossInterruptCooldown)
		};

		struct FReactionFeel
		{
			double Seconds = 0.0;         // the enemy cannot act this long (Recoil: visual only)
			double PushSpeed = 0.0;       // cm/s away from the attacker
			double LiftSpeed = 0.0;       // cm/s up
			double TiltDegrees = 0.0;     // the body tilts away from the hit
			bool bInterrupts = false;     // cancels the attack windup
		};

		struct FTuning
		{
			double FrameRate = 60.0;      // hit stop is authored in frames at this rate
			FWeightFeel Weight[WeightCount];
			FReactionFeel Reaction[ReactionCount];
			// classification by size (Ratio = damage dealt / attack)
			double MediumRatio = 1.2;     // an Unknown-source hit this big is Medium ...
			double HeavyRatio = 2.5;      // ... Heavy (abilities too) ...
			double FinisherRatio = 6.0;   // ... Finisher
			double TickRatio = 0.35;      // an Unknown-source hit smaller than this is a Tick
			double BossHitstopScale = 1.0;
			double BossInterruptCooldown = 6.0;   // a boss can be interrupted at most this often
			double MaxHitstopSeconds = 0.3;       // safety cap when several hits stack
			// responsiveness
			double InputBufferSeconds = 0.18;
			double CancelGraceSeconds = 0.05;     // after a cast's last hit, its recovery can be cancelled into a dodge
			// basic string of the hand-written kits and of data kits whose basic attack is a single hit
			double ComboHitAt[Stagger::ComboLength] = {0.08, 0.08, 0.10, 0.22};
			double ComboStepSeconds[Stagger::ComboLength] = {0.26, 0.26, 0.30, 0.52};
			double ComboWindow = 0.9;
			// Perfect Dodge -> Phantom Counter
			double PerfectDodgeWindow = 0.2;
			double PerfectDodgeDilation = 0.25;
			double PerfectDodgeRealSeconds = 0.45;
			double PhantomCounterWindow = 1.2;
			double PhantomCounterMult = 3.0;
			// ultimate cinematic beat
			double UltCinematicRealSeconds = 0.9;
			double UltCinematicSkipAfter = 0.15;  // pressing attack / ultimate / dodge after this skips the rest
			double UltCinematicDilation = 0.03;   // the world nearly stops while the camera frames the operator
			double UltInvulnerableGrace = 0.35;   // game seconds of invulnerability after the beat
			double UltCameraArm = 330.0;
			double UltCameraYaw = 150.0;          // degrees around the operator (180 = straight in front)
			double UltCameraPitch = -6.0;
			double UltFinisherDilation = 0.15;    // slow motion on the ultimate's last hit ...
			double UltFinisherRealSeconds = 0.3;  // ... for this long
			// camera
			double PushInSeconds = 0.2;
			double KickOscillations = 1.5;        // the kick overshoots and settles
			// poses (procedural wind-up / impact / recovery on the body meshes)
			double PoseWindupLean = 12.0;         // degrees leaning back during the wind-up
			double PoseWindupSquash = 0.08;       // body squashes this much (scale Z)
			double PoseImpactStretch = 0.07;
			double PoseImpactForward = 18.0;      // cm the body lunges forward on impact
			double PoseImpactDecay = 0.09;        // seconds for an impact pulse to fade
			double PoseRecoverySeconds = 0.2;
			double PoseMinWindup = 0.05;          // a wind-up shorter than this still reads as a wind-up
			// gamepad look (right stick, degrees per second at full tilt)
			double PadLookYawSpeed = 220.0;
			double PadLookPitchSpeed = 150.0;
			double PadDeadZone = 0.18;
		};

		/** The live tuning (defaults until the config overrides them). */
		FTuning& Tuning();
		/** Back to the built-in defaults. */
		void ResetTuning();

		struct FConfigKey
		{
			std::string Name;   // "Weight.Heavy.HitstopFrames", "Reaction.Launch.LiftSpeed", "InputBufferSeconds" ...
			double* Value;
		};
		/** Every tunable number, with its config key. */
		std::vector<FConfigKey> ConfigKeys(FTuning& Target);
		/** The same for the enum-valued reaction table ("Weight.Heavy.VsLight" = "Knockback"). */
		struct FReactionKey
		{
			std::string Name;
			EReaction* Value;
		};
		std::vector<FReactionKey> ReactionKeys(FTuning& Target);

		const char* WeightName(EWeight Weight);
		const char* ReactionName(EReaction Reaction);
		/** Parses a ReactionName (case-insensitive); returns false for an unknown name. */
		bool ParseReaction(const std::string& Text, EReaction& Out);

		/** The weight of a hit. */
		EWeight Classify(const FHitSource& Hit);
		const FWeightFeel& FeelFor(EWeight Weight);
		/** Hit stop for a hit of this weight on this target, in seconds. */
		double HitstopSeconds(EWeight Weight, bool bBossTarget);

		/**
		 * The reaction a hit of Weight causes on a target of Body weight, after the target's poise (light enemies cannot be
		 * locked forever) and the boss interrupt cooldown. Interrupting reactions set Meter.FlinchUntil (the AI stands still).
		 */
		EReaction ResolveReaction(Stagger::FMeter& Meter, Stagger::EWeight Body, EWeight Weight, double Now);

		// ------------------------------------------------------------------ poses
		struct FPose
		{
			double LeanDegrees = 0.0;   // + leans forward (into the hit), - leans back (wind-up)
			double Squash = 1.0;        // scale on Z (volume kept on X / Y)
			double Forward = 0.0;       // cm along the facing
		};

		/**
		 * The operator's body pose Time seconds into an attack whose hit lands at ImpactAt and whose recovery lasts Recovery,
		 * plus a pulse SinceImpact seconds after the latest hit (-1 = none). Strength scales everything (weight of the attack).
		 */
		FPose EvaluatePose(double Time, double ImpactAt, double Recovery, double Strength, double SinceImpact, double PulseStrength);
		/** The enemy's tilt (degrees) Age seconds into a reaction. */
		double ReactionTilt(EReaction Reaction, double Age);

		// ------------------------------------------------------------------ what was felt (Tools/verify_feel.py reads it)
		struct FStats
		{
			int Weights[WeightCount] = {};      // hits felt per weight (ticks counted too)
			int Reactions[ReactionCount] = {};  // reactions played on enemies
			int Kicks = 0;                      // camera kicks started
			int PushIns = 0;
			int Rumbles = 0;
			int Cinematics = 0;                 // ultimate beats started ...
			int CinematicSkips = 0;             // ... and skipped
			int RecoveryCancels = 0;            // dodges that cancelled a recovery
			double MaxHitstopFrames = 0.0;
			double MinLean = 0.0;               // the operator's pose: most leaning back (wind-up) ...
			double MaxLean = 0.0;               // ... and forward (impact)
		};
		FStats& Stats();
		void ResetStats();
	}
}
