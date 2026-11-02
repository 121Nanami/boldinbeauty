#include "BIBHitFeel.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace BIB
{
	namespace Feel
	{
		namespace
		{
			FWeightFeel MakeWeight(double Frames, double Kick, double KickSeconds, double PushIn, double Rumble, double RumbleSeconds,
				double Spark, double Strain, double Lean, bool bAccent, bool bImpact, EReaction VsLight, EReaction VsMedium, EReaction VsHeavy)
			{
				FWeightFeel Feel;
				Feel.HitstopFrames = Frames;
				Feel.KickDegrees = Kick;
				Feel.KickSeconds = KickSeconds;
				Feel.PushInCm = PushIn;
				Feel.RumbleIntensity = Rumble;
				Feel.RumbleSeconds = RumbleSeconds;
				Feel.SparkScale = Spark;
				Feel.StrainBonus = Strain;
				Feel.ImpactLean = Lean;
				Feel.bAccentFrame = bAccent;
				Feel.bImpactFrame = bImpact;
				Feel.VsLight = VsLight;
				Feel.VsMedium = VsMedium;
				Feel.VsHeavy = VsHeavy;
				return Feel;
			}

			FReactionFeel MakeReaction(double Seconds, double Push, double Lift, double Tilt, bool bInterrupts)
			{
				FReactionFeel Feel;
				Feel.Seconds = Seconds;
				Feel.PushSpeed = Push;
				Feel.LiftSpeed = Lift;
				Feel.TiltDegrees = Tilt;
				Feel.bInterrupts = bInterrupts;
				return Feel;
			}

			FTuning MakeDefaults()
			{
				using R = EReaction;
				FTuning T;
				//                                                       frames kick  kick s push  rumble  s     spark strain lean  accent impact  vs light      vs brute      vs boss
				T.Weight[static_cast<int>(EWeight::Tick)] =             MakeWeight(0.0,  0.0, 0.0,   0.0, 0.0,  0.0,  0.45, 0.0,  0.0,  false, false, R::None,      R::None,      R::None);
				T.Weight[static_cast<int>(EWeight::Light)] =            MakeWeight(3.0,  0.6, 0.08,  0.0, 0.15, 0.06, 1.0,  0.0,  10.0, true,  false, R::Flinch,    R::Recoil,    R::Recoil);
				T.Weight[static_cast<int>(EWeight::Medium)] =           MakeWeight(5.0,  1.0, 0.10, 12.0, 0.3,  0.09, 1.4,  2.0,  13.0, true,  false, R::Stagger,   R::Flinch,    R::Recoil);
				T.Weight[static_cast<int>(EWeight::Heavy)] =            MakeWeight(8.0,  1.8, 0.14, 30.0, 0.55, 0.14, 2.0,  6.0,  18.0, true,  true,  R::Knockback, R::Stagger,   R::Flinch);
				T.Weight[static_cast<int>(EWeight::Finisher)] =         MakeWeight(10.0, 2.6, 0.18, 45.0, 0.75, 0.18, 2.6,  10.0, 22.0, true,  true,  R::Launch,    R::Knockback, R::Stagger);
				T.Weight[static_cast<int>(EWeight::Ultimate)] =         MakeWeight(6.0,  1.4, 0.12, 20.0, 0.5,  0.12, 2.0,  4.0,  16.0, true,  false, R::Stagger,   R::Flinch,    R::Recoil);
				T.Weight[static_cast<int>(EWeight::UltimateFinisher)] = MakeWeight(14.0, 4.0, 0.30, 70.0, 1.0,  0.35, 4.0,  20.0, 26.0, true,  true,  R::Knockdown, R::Knockdown, R::Stagger);
				//                                                         seconds push   lift   tilt  interrupts
				T.Reaction[static_cast<int>(R::None)] =      MakeReaction(0.0,  0.0,   0.0,   0.0,  false);
				T.Reaction[static_cast<int>(R::Recoil)] =    MakeReaction(0.14, 90.0,  0.0,   9.0,  false);
				T.Reaction[static_cast<int>(R::Flinch)] =    MakeReaction(0.32, 260.0, 0.0,   14.0, true);
				T.Reaction[static_cast<int>(R::Stagger)] =   MakeReaction(0.55, 380.0, 0.0,   20.0, true);
				T.Reaction[static_cast<int>(R::Knockback)] = MakeReaction(0.75, 820.0, 120.0, 26.0, true);
				T.Reaction[static_cast<int>(R::Launch)] =    MakeReaction(1.0,  260.0, 760.0, 35.0, true);
				T.Reaction[static_cast<int>(R::Knockdown)] = MakeReaction(1.25, 520.0, 220.0, 80.0, true);
				return T;
			}

			FTuning& Live()
			{
				static FTuning Value = MakeDefaults();
				return Value;
			}

			std::string Lower(const std::string& Text)
			{
				std::string Out = Text;
				for (char& C : Out)
				{
					C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
				}
				return Out;
			}

			double Clamp01(double V)
			{
				return std::max(0.0, std::min(1.0, V));
			}

			double EaseOut(double V)
			{
				const double X = Clamp01(V);
				return 1.0 - (1.0 - X) * (1.0 - X);
			}
		}

		FTuning& Tuning()
		{
			return Live();
		}

		FStats& Stats()
		{
			static FStats Value;
			return Value;
		}

		void ResetStats()
		{
			Stats() = FStats();
		}

		void ResetTuning()
		{
			Live() = MakeDefaults();
		}

		std::vector<FConfigKey> ConfigKeys(FTuning& T)
		{
			std::vector<FConfigKey> Keys;
			for (int Index = 0; Index < WeightCount; ++Index)
			{
				const std::string P = std::string("Weight.") + WeightName(static_cast<EWeight>(Index)) + ".";
				FWeightFeel& W = T.Weight[Index];
				Keys.push_back({P + "HitstopFrames", &W.HitstopFrames});
				Keys.push_back({P + "KickDegrees", &W.KickDegrees});
				Keys.push_back({P + "KickSeconds", &W.KickSeconds});
				Keys.push_back({P + "PushInCm", &W.PushInCm});
				Keys.push_back({P + "RumbleIntensity", &W.RumbleIntensity});
				Keys.push_back({P + "RumbleSeconds", &W.RumbleSeconds});
				Keys.push_back({P + "SparkScale", &W.SparkScale});
				Keys.push_back({P + "StrainBonus", &W.StrainBonus});
				Keys.push_back({P + "ImpactLean", &W.ImpactLean});
			}
			for (int Index = 1; Index < ReactionCount; ++Index)
			{
				const std::string P = std::string("Reaction.") + ReactionName(static_cast<EReaction>(Index)) + ".";
				FReactionFeel& R = T.Reaction[Index];
				Keys.push_back({P + "Seconds", &R.Seconds});
				Keys.push_back({P + "PushSpeed", &R.PushSpeed});
				Keys.push_back({P + "LiftSpeed", &R.LiftSpeed});
				Keys.push_back({P + "TiltDegrees", &R.TiltDegrees});
			}
			Keys.push_back({"FrameRate", &T.FrameRate});
			Keys.push_back({"MediumRatio", &T.MediumRatio});
			Keys.push_back({"HeavyRatio", &T.HeavyRatio});
			Keys.push_back({"FinisherRatio", &T.FinisherRatio});
			Keys.push_back({"TickRatio", &T.TickRatio});
			Keys.push_back({"BossHitstopScale", &T.BossHitstopScale});
			Keys.push_back({"BossInterruptCooldown", &T.BossInterruptCooldown});
			Keys.push_back({"MaxHitstopSeconds", &T.MaxHitstopSeconds});
			Keys.push_back({"InputBufferSeconds", &T.InputBufferSeconds});
			Keys.push_back({"CancelGraceSeconds", &T.CancelGraceSeconds});
			for (int Step = 0; Step < Stagger::ComboLength; ++Step)
			{
				Keys.push_back({"Combo" + std::to_string(Step + 1) + ".HitAt", &T.ComboHitAt[Step]});
				Keys.push_back({"Combo" + std::to_string(Step + 1) + ".Seconds", &T.ComboStepSeconds[Step]});
			}
			Keys.push_back({"ComboWindow", &T.ComboWindow});
			Keys.push_back({"PerfectDodgeWindow", &T.PerfectDodgeWindow});
			Keys.push_back({"PerfectDodgeDilation", &T.PerfectDodgeDilation});
			Keys.push_back({"PerfectDodgeRealSeconds", &T.PerfectDodgeRealSeconds});
			Keys.push_back({"PhantomCounterWindow", &T.PhantomCounterWindow});
			Keys.push_back({"PhantomCounterMult", &T.PhantomCounterMult});
			Keys.push_back({"UltCinematicRealSeconds", &T.UltCinematicRealSeconds});
			Keys.push_back({"UltCinematicSkipAfter", &T.UltCinematicSkipAfter});
			Keys.push_back({"UltCinematicDilation", &T.UltCinematicDilation});
			Keys.push_back({"UltInvulnerableGrace", &T.UltInvulnerableGrace});
			Keys.push_back({"UltCameraArm", &T.UltCameraArm});
			Keys.push_back({"UltCameraYaw", &T.UltCameraYaw});
			Keys.push_back({"UltCameraPitch", &T.UltCameraPitch});
			Keys.push_back({"UltFinisherDilation", &T.UltFinisherDilation});
			Keys.push_back({"UltFinisherRealSeconds", &T.UltFinisherRealSeconds});
			Keys.push_back({"PushInSeconds", &T.PushInSeconds});
			Keys.push_back({"KickOscillations", &T.KickOscillations});
			Keys.push_back({"PoseWindupLean", &T.PoseWindupLean});
			Keys.push_back({"PoseWindupSquash", &T.PoseWindupSquash});
			Keys.push_back({"PoseImpactStretch", &T.PoseImpactStretch});
			Keys.push_back({"PoseImpactForward", &T.PoseImpactForward});
			Keys.push_back({"PoseImpactDecay", &T.PoseImpactDecay});
			Keys.push_back({"PoseRecoverySeconds", &T.PoseRecoverySeconds});
			Keys.push_back({"PoseMinWindup", &T.PoseMinWindup});
			Keys.push_back({"PadLookYawSpeed", &T.PadLookYawSpeed});
			Keys.push_back({"PadLookPitchSpeed", &T.PadLookPitchSpeed});
			Keys.push_back({"PadDeadZone", &T.PadDeadZone});
			return Keys;
		}

		std::vector<FReactionKey> ReactionKeys(FTuning& T)
		{
			std::vector<FReactionKey> Keys;
			for (int Index = 0; Index < WeightCount; ++Index)
			{
				const std::string P = std::string("Weight.") + WeightName(static_cast<EWeight>(Index)) + ".";
				Keys.push_back({P + "VsLight", &T.Weight[Index].VsLight});
				Keys.push_back({P + "VsMedium", &T.Weight[Index].VsMedium});
				Keys.push_back({P + "VsHeavy", &T.Weight[Index].VsHeavy});
			}
			return Keys;
		}

		const char* WeightName(EWeight Weight)
		{
			switch (Weight)
			{
			case EWeight::Tick: return "Tick";
			case EWeight::Light: return "Light";
			case EWeight::Medium: return "Medium";
			case EWeight::Heavy: return "Heavy";
			case EWeight::Finisher: return "Finisher";
			case EWeight::Ultimate: return "Ultimate";
			case EWeight::UltimateFinisher: return "UltimateFinisher";
			default: return "?";
			}
		}

		const char* ReactionName(EReaction Reaction)
		{
			switch (Reaction)
			{
			case EReaction::None: return "None";
			case EReaction::Recoil: return "Recoil";
			case EReaction::Flinch: return "Flinch";
			case EReaction::Stagger: return "Stagger";
			case EReaction::Knockback: return "Knockback";
			case EReaction::Launch: return "Launch";
			case EReaction::Knockdown: return "Knockdown";
			default: return "?";
			}
		}

		bool ParseReaction(const std::string& Text, EReaction& Out)
		{
			const std::string Wanted = Lower(Text);
			for (int Index = 0; Index < ReactionCount; ++Index)
			{
				if (Lower(ReactionName(static_cast<EReaction>(Index))) == Wanted)
				{
					Out = static_cast<EReaction>(Index);
					return true;
				}
			}
			return false;
		}

		EWeight Classify(const FHitSource& Hit)
		{
			if (Hit.Override >= 0 && Hit.Override < WeightCount)
			{
				return static_cast<EWeight>(Hit.Override);
			}
			if (Hit.Source == ESource::Ultimate && Hit.bFinal)
			{
				return EWeight::UltimateFinisher;   // also the last tick of an ultimate's zone or channel
			}
			if (Hit.bTick)
			{
				return EWeight::Tick;
			}
			const FTuning& T = Tuning();
			switch (Hit.Source)
			{
			case ESource::Ultimate:
				return Hit.bFinal ? EWeight::UltimateFinisher : EWeight::Ultimate;
			case ESource::Basic:
				return Hit.bFinal ? EWeight::Heavy : EWeight::Light;
			case ESource::Counter:
				return Hit.Ratio >= T.FinisherRatio ? EWeight::Finisher : EWeight::Heavy;
			case ESource::Special:
				// A Special's last hit always lands heavy; earlier hits by size.
				if (Hit.Ratio >= T.FinisherRatio)
				{
					return EWeight::Finisher;
				}
				return Hit.bFinal || Hit.Ratio >= T.HeavyRatio ? EWeight::Heavy : EWeight::Medium;
			case ESource::Move:
				if (Hit.Ratio >= T.FinisherRatio)
				{
					return EWeight::Finisher;
				}
				return Hit.Ratio >= T.HeavyRatio ? EWeight::Heavy : EWeight::Medium;
			default:
				if (Hit.Ratio < T.TickRatio)
				{
					return EWeight::Tick;
				}
				if (Hit.Ratio >= T.FinisherRatio)
				{
					return EWeight::Finisher;
				}
				if (Hit.Ratio >= T.HeavyRatio)
				{
					return EWeight::Heavy;
				}
				return Hit.Ratio >= T.MediumRatio ? EWeight::Medium : EWeight::Light;
			}
		}

		const FWeightFeel& FeelFor(EWeight Weight)
		{
			const int Index = std::max(0, std::min(WeightCount - 1, static_cast<int>(Weight)));
			return Tuning().Weight[Index];
		}

		double HitstopSeconds(EWeight Weight, bool bBossTarget)
		{
			const FTuning& T = Tuning();
			const double Seconds = FeelFor(Weight).HitstopFrames / std::max(1.0, T.FrameRate) * (bBossTarget ? T.BossHitstopScale : 1.0);
			return std::max(0.0, std::min(T.MaxHitstopSeconds, Seconds));
		}

		EReaction ResolveReaction(Stagger::FMeter& Meter, Stagger::EWeight Body, EWeight Weight, double Now)
		{
			const FWeightFeel& Feel = FeelFor(Weight);
			EReaction Reaction = Body == Stagger::EWeight::Heavy ? Feel.VsHeavy : (Body == Stagger::EWeight::Medium ? Feel.VsMedium : Feel.VsLight);
			if (Reaction == EReaction::None)
			{
				return Reaction;
			}
			const FTuning& T = Tuning();
			const FReactionFeel& React = T.Reaction[static_cast<int>(Reaction)];
			if (!React.bInterrupts)
			{
				return Reaction;
			}
			if (Meter.Stunned(Now))
			{
				// A broken enemy is already helpless: the hit still shows (recoil), the break timer is not touched.
				return EReaction::Recoil;
			}
			if (Body == Stagger::EWeight::Heavy)
			{
				// Bosses stay threatening: an interrupt at most every BossInterruptCooldown seconds, otherwise a recoil.
				if (Now - Meter.BossInterruptAt < T.BossInterruptCooldown)
				{
					return EReaction::Recoil;
				}
				Meter.BossInterruptAt = Now;
			}
			else
			{
				// Poise (BIBStagger.h): too many interrupts in a short time and the enemy shrugs the next light / medium ones off.
				if (Now < Meter.PoiseUntil && Weight < EWeight::Heavy)
				{
					return EReaction::Recoil;
				}
				if (Now - Meter.FlinchWindowStart > Stagger::PoiseWindow)
				{
					Meter.FlinchWindowStart = Now;
					Meter.FlinchesInWindow = 0;
				}
				++Meter.FlinchesInWindow;
				if (Meter.FlinchesInWindow > Stagger::FlinchesBeforePoise && Weight < EWeight::Heavy)
				{
					// Light and medium spam builds poise; heavies, finishers and ultimates always land.
					Meter.PoiseUntil = Now + Stagger::PoiseSeconds;
					Meter.FlinchesInWindow = 0;
					Meter.FlinchWindowStart = Now + Stagger::PoiseSeconds;
					return EReaction::Recoil;
				}
			}
			Meter.FlinchUntil = std::max(Meter.FlinchUntil, Now + React.Seconds);
			return Reaction;
		}

		FPose EvaluatePose(double Time, double ImpactAt, double Recovery, double Strength, double SinceImpact, double PulseStrength)
		{
			const FTuning& T = Tuning();
			FPose Pose;
			if (Time >= 0.0 && Strength > 0.0)
			{
				const double Windup = std::max(T.PoseMinWindup, ImpactAt);
				if (Time < Windup)
				{
					// Anticipation: lean back and squash, easing in.
					const double K = EaseOut(Time / Windup);
					Pose.LeanDegrees = -T.PoseWindupLean * Strength * K;
					Pose.Squash = 1.0 - T.PoseWindupSquash * Strength * K;
					Pose.Forward = -6.0 * Strength * K;
				}
				else if (Recovery > 0.0 && Time < Windup + Recovery)
				{
					// Recovery: from the lunge back to neutral.
					const double K = 1.0 - EaseOut((Time - Windup) / Recovery);
					Pose.LeanDegrees = T.PoseWindupLean * 0.5 * Strength * K;
					Pose.Squash = 1.0 + T.PoseImpactStretch * 0.5 * Strength * K;
					Pose.Forward = T.PoseImpactForward * 0.5 * Strength * K;
				}
			}
			if (SinceImpact >= 0.0 && PulseStrength > 0.0)
			{
				// Impact: a sharp lunge into the hit, decaying quickly (held while the hit stop freezes the operator).
				const double K = std::exp(-SinceImpact / std::max(0.01, T.PoseImpactDecay));
				Pose.LeanDegrees = std::max(Pose.LeanDegrees, PulseStrength * K);
				Pose.Squash = std::max(Pose.Squash, 1.0 + T.PoseImpactStretch * K * std::min(1.5, PulseStrength / 15.0));
				Pose.Forward = std::max(Pose.Forward, T.PoseImpactForward * K * std::min(1.5, PulseStrength / 15.0));
			}
			return Pose;
		}

		double ReactionTilt(EReaction Reaction, double Age)
		{
			const FTuning& T = Tuning();
			const int Index = static_cast<int>(Reaction);
			if (Index <= 0 || Index >= ReactionCount || Age < 0.0)
			{
				return 0.0;
			}
			const FReactionFeel& R = T.Reaction[Index];
			const double Length = std::max(0.08, R.Seconds);
			if (Age >= Length)
			{
				return 0.0;
			}
			const double Rise = Reaction == EReaction::Knockdown ? 0.18 : 0.05;
			if (Age < Rise)
			{
				return R.TiltDegrees * EaseOut(Age / Rise);
			}
			// Knockdown: stays down, then gets up in the last quarter; the others ease back.
			const double HoldUntil = Reaction == EReaction::Knockdown ? Length * 0.75 : Rise;
			if (Age < HoldUntil)
			{
				return R.TiltDegrees;
			}
			return R.TiltDegrees * (1.0 - EaseOut((Age - HoldUntil) / std::max(0.01, Length - HoldUntil)));
		}
	}
}
