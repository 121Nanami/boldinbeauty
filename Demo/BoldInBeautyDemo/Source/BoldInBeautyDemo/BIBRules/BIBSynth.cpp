// Boldinbeauty demo — the arcade synthesiser (see BIBSynth.h). Simple oscillators, envelopes and noise; nothing clever.
#include "BIBSynth.h"

#include "BIBArcade.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace BIB
{
	namespace Synth
	{
		namespace
		{
			constexpr double Pi = 3.14159265358979323846;

			// A float stereo mix that becomes 16-bit at the end (soft-clipped).
			struct FMix
			{
				std::vector<float> L;
				std::vector<float> R;

				explicit FMix(double Seconds)
				{
					const size_t Frames = static_cast<size_t>(std::max(0.0, Seconds) * SampleRate) + 1;
					L.assign(Frames, 0.f);
					R.assign(Frames, 0.f);
				}

				size_t Frames() const { return L.size(); }

				void Add(size_t Frame, double Value, double Pan)
				{
					if (Frame >= L.size())
					{
						return;
					}
					const double Angle = (std::clamp(Pan, -1.0, 1.0) + 1.0) * Pi / 4.0;   // equal-power pan
					L[Frame] += static_cast<float>(Value * std::cos(Angle));
					R[Frame] += static_cast<float>(Value * std::sin(Angle));
				}

				FBuffer Finish(double Gain = 1.0) const
				{
					FBuffer Out;
					Out.Samples.resize(L.size() * 2);
					for (size_t Frame = 0; Frame < L.size(); ++Frame)
					{
						const double Left = std::tanh(L[Frame] * Gain);
						const double Right = std::tanh(R[Frame] * Gain);
						Out.Samples[Frame * 2] = static_cast<int16_t>(std::lround(Left * 30000.0));
						Out.Samples[Frame * 2 + 1] = static_cast<int16_t>(std::lround(Right * 30000.0));
					}
					return Out;
				}
			};

			double Osc(EWave Wave, double Phase, std::mt19937& Rng)
			{
				const double Frac = Phase - std::floor(Phase);
				switch (Wave)
				{
				case EWave::Sine: return std::sin(2.0 * Pi * Frac);
				case EWave::Square: return Frac < 0.5 ? 0.7 : -0.7;
				case EWave::Saw: return 2.0 * Frac - 1.0;
				case EWave::Triangle: return 1.0 - 4.0 * std::fabs(Frac - 0.5);
				case EWave::Noise: return std::uniform_real_distribution<double>(-1.0, 1.0)(Rng);
				}
				return 0.0;
			}

			// Attack / decay envelope: rises over Attack seconds, falls exponentially with time constant Decay.
			double Env(double T, double Attack, double Decay)
			{
				if (T < 0.0)
				{
					return 0.0;
				}
				const double Rise = Attack > 0.0 ? std::min(1.0, T / Attack) : 1.0;
				return Rise * std::exp(-std::max(0.0, T - Attack) / std::max(1e-4, Decay));
			}

			// One voice into the mix: frequency may glide from Hz to HzEnd.
			void Voice(FMix& Mix, double Start, double Seconds, double Hz, double HzEnd, double Volume, double Pan, EWave Wave, double Attack, double Decay,
				std::mt19937& Rng, double Vibrato = 0.0)
			{
				const size_t First = static_cast<size_t>(std::max(0.0, Start) * SampleRate);
				const size_t Count = static_cast<size_t>(Seconds * SampleRate);
				double Phase = 0.0;
				for (size_t Index = 0; Index < Count; ++Index)
				{
					const double T = static_cast<double>(Index) / SampleRate;
					const double Alpha = Seconds > 0.0 ? T / Seconds : 0.0;
					double Freq = Hz + (HzEnd - Hz) * Alpha;
					if (Vibrato > 0.0)
					{
						Freq *= 1.0 + Vibrato * std::sin(2.0 * Pi * 6.0 * T);
					}
					Phase += Freq / SampleRate;
					Mix.Add(First + Index, Volume * Env(T, Attack, Decay) * Osc(Wave, Phase, Rng), Pan);
				}
			}
		}

		FBuffer Tone(double Hz, double Seconds, double Pan, double Volume, EWave Wave)
		{
			std::mt19937 Rng(1);
			FMix Mix(Seconds);
			Voice(Mix, 0.0, Seconds, Hz, Hz, Volume, Pan, Wave, 0.005, Seconds * 0.6, Rng);
			return Mix.Finish();
		}

		FBuffer MediaCue(double Pan)
		{
			// Hard-panned so the side is unmistakable, with a wavering glide that rises like a warning.
			std::mt19937 Rng(2);
			FMix Mix(0.9);
			Voice(Mix, 0.0, 0.9, 220.0, 660.0, 0.55, Pan, EWave::Saw, 0.02, 0.6, Rng, 0.03);
			Voice(Mix, 0.0, 0.9, 330.0, 990.0, 0.30, Pan, EWave::Square, 0.02, 0.5, Rng, 0.05);
			Voice(Mix, 0.0, 0.25, 0.0, 0.0, 0.25, Pan, EWave::Noise, 0.001, 0.08, Rng);
			return Mix.Finish(1.2);
		}

		FBuffer DoorSlam(double Pan)
		{
			std::mt19937 Rng(3);
			FMix Mix(0.4);
			Voice(Mix, 0.0, 0.4, 90.0, 45.0, 0.9, Pan, EWave::Sine, 0.001, 0.12, Rng);
			Voice(Mix, 0.0, 0.15, 0.0, 0.0, 0.5, Pan, EWave::Noise, 0.001, 0.04, Rng);
			return Mix.Finish();
		}

		FBuffer Knock(double Pan)
		{
			std::mt19937 Rng(4);
			FMix Mix(0.6);
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				Voice(Mix, Hit * 0.17, 0.15, 160.0, 120.0, 0.7, Pan, EWave::Sine, 0.001, 0.05, Rng);
				Voice(Mix, Hit * 0.17, 0.05, 0.0, 0.0, 0.3, Pan, EWave::Noise, 0.001, 0.02, Rng);
			}
			return Mix.Finish();
		}

		FBuffer KuuThump(double Pan)
		{
			std::mt19937 Rng(5);
			FMix Mix(1.0);
			for (int Hit = 0; Hit < 2; ++Hit)
			{
				Voice(Mix, Hit * 0.5, 0.35, 70.0, 40.0, 1.0, Pan, EWave::Sine, 0.002, 0.15, Rng);
			}
			return Mix.Finish(1.3);
		}

		FBuffer Punch(double Pan, bool bBroke)
		{
			std::mt19937 Rng(6);
			FMix Mix(bBroke ? 0.9 : 0.4);
			Voice(Mix, 0.0, 0.3, 110.0, 40.0, 1.0, Pan, EWave::Sine, 0.001, 0.1, Rng);
			Voice(Mix, 0.0, bBroke ? 0.8 : 0.2, 0.0, 0.0, bBroke ? 0.7 : 0.35, Pan, EWave::Noise, 0.001, bBroke ? 0.3 : 0.05, Rng);
			return Mix.Finish(1.2);
		}

		FBuffer CameraBlip()
		{
			std::mt19937 Rng(7);
			FMix Mix(0.12);
			Voice(Mix, 0.0, 0.12, 0.0, 0.0, 0.18, 0.0, EWave::Noise, 0.001, 0.05, Rng);
			Voice(Mix, 0.0, 0.06, 1800.0, 1800.0, 0.12, 0.0, EWave::Square, 0.001, 0.03, Rng);
			return Mix.Finish();
		}

		FBuffer PowerDown()
		{
			std::mt19937 Rng(8);
			FMix Mix(1.6);
			Voice(Mix, 0.0, 1.6, 440.0, 40.0, 0.6, 0.0, EWave::Saw, 0.01, 1.0, Rng);
			return Mix.Finish();
		}

		FBuffer Jumpscare()
		{
			std::mt19937 Rng(9);
			FMix Mix(1.2);
			Voice(Mix, 0.0, 1.2, 0.0, 0.0, 0.9, -0.2, EWave::Noise, 0.001, 0.6, Rng);
			Voice(Mix, 0.0, 1.2, 0.0, 0.0, 0.9, 0.2, EWave::Noise, 0.001, 0.6, Rng);
			Voice(Mix, 0.0, 1.2, 180.0, 120.0, 0.7, 0.0, EWave::Square, 0.001, 0.7, Rng, 0.08);
			return Mix.Finish(1.5);
		}

		FBuffer Chime()
		{
			std::mt19937 Rng(10);
			FMix Mix(2.4);
			const double Notes[4] = {523.25, 659.25, 783.99, 1046.5};
			for (int Index = 0; Index < 4; ++Index)
			{
				Voice(Mix, Index * 0.35, 1.2, Notes[Index], Notes[Index], 0.4, 0.0, EWave::Triangle, 0.01, 0.6, Rng);
			}
			return Mix.Finish();
		}

		FBuffer RhythmBacking(int SongIndex)
		{
			const Arcade::FSong& Song = Arcade::Song(SongIndex);
			const double Beat = 60.0 / Song.Bpm;
			const double Length = Arcade::SongLength(SongIndex);
			std::mt19937 Rng(static_cast<uint32_t>(100 + SongIndex));
			FMix Mix(Length);
			// Count-in: four clicks during the lead-in so the player feels the tempo.
			const int CountIn = static_cast<int>(std::floor(Arcade::RhythmLeadIn / Beat));
			for (int Click = 0; Click < CountIn; ++Click)
			{
				Voice(Mix, Arcade::RhythmLeadIn - (CountIn - Click) * Beat, 0.05, 2000.0, 2000.0, 0.15, 0.0, EWave::Square, 0.001, 0.02, Rng);
			}
			// Chord roots I - V - vi - IV under the lanes' scale.
			const double Root = Song.LaneHz[0] / 2.0;
			const double Progression[4] = {1.0, 1.4983, 1.6818, 1.3348};
			for (int Bar = 0; Bar < Song.Bars; ++Bar)
			{
				const double BarStart = Arcade::RhythmLeadIn + Bar * 4.0 * Beat;
				const double Bass = Root * Progression[Bar % 4] / 2.0;
				for (int Step = 0; Step < 8; ++Step)
				{
					const double T = BarStart + Step * Beat / 2.0;
					// Hats on eighths, kick on 1 and 3, snare on 2 and 4.
					Voice(Mix, T, 0.04, 0.0, 0.0, 0.08, 0.15, EWave::Noise, 0.001, 0.015, Rng);
					if (Step % 4 == 0)
					{
						Voice(Mix, T, 0.18, 130.0, 45.0, 0.9, 0.0, EWave::Sine, 0.001, 0.08, Rng);
					}
					if (Step % 4 == 2)
					{
						Voice(Mix, T, 0.14, 0.0, 0.0, 0.35, -0.1, EWave::Noise, 0.001, 0.05, Rng);
						Voice(Mix, T, 0.10, 220.0, 180.0, 0.2, -0.1, EWave::Triangle, 0.001, 0.04, Rng);
					}
					Voice(Mix, T, Beat / 2.0 * 0.9, Bass, Bass, 0.28, 0.0, EWave::Saw, 0.005, Beat * 0.4, Rng);
				}
			}
			return Mix.Finish(0.9);
		}

		FBuffer LaneTone(double Hz, double Pan)
		{
			std::mt19937 Rng(11);
			FMix Mix(0.35);
			Voice(Mix, 0.0, 0.35, Hz, Hz, 0.45, Pan, EWave::Triangle, 0.003, 0.18, Rng);
			Voice(Mix, 0.0, 0.35, Hz * 2.0, Hz * 2.0, 0.12, Pan, EWave::Sine, 0.003, 0.12, Rng);
			return Mix.Finish();
		}

		FBuffer MissBuzz()
		{
			std::mt19937 Rng(12);
			FMix Mix(0.15);
			Voice(Mix, 0.0, 0.15, 110.0, 90.0, 0.25, 0.0, EWave::Square, 0.001, 0.06, Rng);
			return Mix.Finish();
		}

		FBuffer HitSound(double Pan, bool bBig)
		{
			std::mt19937 Rng(13);
			FMix Mix(bBig ? 0.5 : 0.18);
			Voice(Mix, 0.0, bBig ? 0.5 : 0.18, bBig ? 300.0 : 600.0, bBig ? 80.0 : 300.0, bBig ? 0.7 : 0.35, Pan, EWave::Saw, 0.001, bBig ? 0.2 : 0.06, Rng);
			Voice(Mix, 0.0, 0.08, 0.0, 0.0, 0.25, Pan, EWave::Noise, 0.001, 0.03, Rng);
			return Mix.Finish();
		}

		double Energy(const FBuffer& Buffer, int Channel)
		{
			if (Buffer.Samples.empty())
			{
				return 0.0;
			}
			double Sum = 0.0;
			size_t Count = 0;
			for (size_t Index = static_cast<size_t>(std::clamp(Channel, 0, 1)); Index < Buffer.Samples.size(); Index += 2)
			{
				Sum += std::abs(static_cast<int>(Buffer.Samples[Index]));
				++Count;
			}
			return Count > 0 ? Sum / static_cast<double>(Count) : 0.0;
		}
	}
}
