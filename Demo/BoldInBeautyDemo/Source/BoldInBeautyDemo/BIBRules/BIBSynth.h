// Boldinbeauty demo — a tiny synthesiser for the arcade sounds (owner request 2026-10-06: "Media teleports ... with a panning
// audio"). The demo has no sound assets, so the cues are generated: 16-bit stereo PCM the Unreal layer plays through a
// procedural sound wave (BIBGame/BIBArcadeOverlay). Engine-independent and unit-tested (Tests/arcade_tests.cpp checks that a
// left cue is louder on the left). Placeholder sounds: the owner's audio replaces them later.
#pragma once

#include <cstdint>
#include <vector>

namespace BIB
{
	namespace Synth
	{
		constexpr int SampleRate = 32000;

		struct FBuffer
		{
			std::vector<int16_t> Samples;   // interleaved stereo: left, right, left, right...
			int Channels = 2;
			int Rate = SampleRate;
			double Seconds() const { return Samples.empty() ? 0.0 : static_cast<double>(Samples.size()) / (Channels * Rate); }
		};

		enum class EWave : int
		{
			Sine = 0,
			Square,
			Saw,
			Triangle,
			Noise
		};

		// Pan: -1 = only the left speaker, 0 = centre, +1 = only the right speaker (equal-power).
		FBuffer Tone(double Hz, double Seconds, double Pan, double Volume, EWave Wave = EWave::Sine);

		// Night Shift
		FBuffer MediaCue(double Pan);        // a rising, wavering glide from one side: "close that door now"
		FBuffer DoorSlam(double Pan);
		FBuffer Knock(double Pan);           // someone hit a closed door and left
		FBuffer KuuThump(double Pan);        // the wind-up: heavy thumps from that side
		FBuffer Punch(double Pan, bool bBroke);
		FBuffer CameraBlip();
		FBuffer PowerDown();
		FBuffer Jumpscare();
		FBuffer Chime();                     // 6 AM

		// Rhythm
		FBuffer RhythmBacking(int Song);     // drums and bass for the whole song (lead-in included), see Arcade::Song
		FBuffer LaneTone(double Hz, double Pan);
		FBuffer MissBuzz();

		// Arena replay
		FBuffer HitSound(double Pan, bool bBig);

		// For tests: the mean absolute amplitude of one channel (0 left, 1 right).
		double Energy(const FBuffer& Buffer, int Channel);
	}
}
