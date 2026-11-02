// Boldinbeauty demo — Ark Rally, a side-view physics trial racer for the arcade (owner request 2026-10-15: a clean-room,
// copyright-safe arcade game that plays and feels like Traplight's Big Bang Racing, inside Boldinbeauty).
//
// Clean room: only the publicly observable game design is followed (a 2D side-view dirt bike on a physics track; gas and
// reverse on one hand, lean back / forward on the other; coins, checkpoints and hazards; racing against other riders'
// ghosts; tracks players build and share). No code, art, sound, level, name or text of Big Bang Racing was used or
// decompiled: the physics, tracks, track code format, names and numbers below are original and PLACEHOLDER (the owner decides).
//
// Engine-independent and deterministic (fixed 1/240 s steps), so it is unit-tested without Unreal (Tests/minigame_tests.cpp);
// the Unreal layer (BIBGame/BIBArcadeOverlay) draws it and reads the keys, like the other arcade games.
//
// Owner feedback 2026-10-15: flips fill a nitro meter; a second vehicle (the buggy: slower, steadier, hard to flip, hard to
// crash) next to the dirt bike (faster, flips easily, can crash); the dirt bike must not feel like a rage game (crashes only
// when it is really upside down, a wheelie that balances instead of looping at once, respawn on the last safe ground with
// its speed instead of far back at a standstill).
//
// Units are metres and seconds, x to the right, y up. Angles are radians, counter-clockwise (positive = nose up).
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "BIBArcade.h"

namespace BIB
{
	namespace Arcade
	{
		struct FVec2
		{
			double X = 0.0;
			double Y = 0.0;
		};

		// ================================================================ tracks
		// A track is a line of text (the "track code"), so players can write, share and edit tracks without an editor asset:
		//   F<len>          flat ground
		//   H<len>,<h>      a smooth hill (negative h = a dip)
		//   U<len>,<d>      a slope that raises (or with negative d lowers) the ground for good
		//   R<len>,<h>      a kicker ramp that ends in a drop back to the ground level
		//   G<len>          a gap (a pit: falling in is a crash)
		//   W<len>,<a>      washboard bumps (at most 0.4 m)
		//   C<n>            a row of n coins from here on      B  a bomb      S  a jump spring
		//   P               a boost pad                        K  a checkpoint
		// Tokens are separated by spaces. The finish line follows the last segment. Lengths 1-200 m, at most 2,000 m in total.
		enum class ERallyThing : int
		{
			Coin = 0,
			Bomb,
			Spring,
			Boost,
			Checkpoint
		};

		struct FRallyThing
		{
			ERallyThing Kind = ERallyThing::Coin;
			double X = 0.0;
			double Y = 0.0;
			bool bUsed = false;   // a coin taken, a bomb gone off
		};

		struct FRallyPit
		{
			double X0 = 0.0;
			double X1 = 0.0;
			double LipY = 0.0;   // the lower of the two edges
		};

		struct FRallyTrack
		{
			std::string Name;
			std::string Code;
			std::vector<FVec2> Ground;     // the surface, left to right (x never decreases; drops and walls are vertical pieces)
			std::vector<FRallyThing> Things;
			std::vector<FRallyPit> Pits;
			double StartX = 4.0;
			double FinishX = 0.0;
			double EndX = 0.0;
			int Coins = 0;

			double HeightAt(double X) const;   // the surface height under X (the top of a vertical piece)
		};

		// Builds a track from a track code. False (with the reason in Error) when the code cannot be read.
		bool BuildTrack(const std::string& Code, const std::string& Name, FRallyTrack& Out, std::string& Error);
		// The three built-in tracks (one per difficulty).
		const char* RallyTrackName(EDifficulty Difficulty);
		const char* RallyTrackCode(EDifficulty Difficulty);
		constexpr int RallyCodeMaxLength = 600;

		// ================================================================ the race
		constexpr double RallyStep = 1.0 / 240.0;   // fixed simulation step
		constexpr double RallyCrashPause = 0.9;     // seconds a crash shows before the vehicle respawns on safe ground
		constexpr int RallyRivals = 3;

		// The two vehicles (owner request 2026-10-15). Every number is PLACEHOLDER.
		enum class ERallyVehicle : int
		{
			DirtBike = 0,   // faster, light, flips easily (more nitro), crashes when it lands on the rider
			Buggy,          // slower, heavy and steady, slow to rotate (fewer flips), roll cage: hard to crash
			Count
		};
		constexpr int RallyVehicleCount = static_cast<int>(ERallyVehicle::Count);
		const char* RallyVehicleName(ERallyVehicle Vehicle);

		struct FRallyVehicleSpec
		{
			double ChassisMass = 1.0;
			double ChassisInertia = 0.35;
			double WheelMass = 0.18;
			double WheelRadius = 0.38;
			FVec2 Anchors[2] = {{-0.8, -0.35}, {0.8, -0.35}};    // rear, front wheel rest positions (chassis space)
			FVec2 FramePoints[3] = {{-0.75, 0.05}, {0.75, 0.05}, {0.0, -0.05}};
			double FrameRadius = 0.15;
			FVec2 HeadLocal = {-0.05, 0.85};   // the rider's head (bike) or the roof (buggy)
			double HeadRadius = 0.22;
			double SuspStiffness = 140.0;
			double SuspDamping = 5.0;
			double SuspTravel = 0.18;
			double Grip = 1.15;
			double MotorTorque = 5.5;         // at a standstill, falling to half at top speed
			double MotorKick = 0.35;          // share of the motor torque that lifts the nose
			double MaxSpeed = 21.0;
			double LeanTorque = 4.0;          // in the air (and leaning forward on the ground)
			double WheelieTorque = 16.0;      // leaning back on the gas with a wheel down; fades out as the nose rises
			double SpinDamping = 0.8;
			double CrashTilt = 1.35;          // the head / roof on the ground is a crash only when tilted further than this from the ground
			double RoofTime = 0.0;            // ...and only after it stays there this long (s)
			double LandingSpinCap = 2.0;      // for a moment after touching down, the spin is capped to this (rad/s): a landing settles
			double AutoLevel = 0.0;           // in the air without lean input, a pull (torque, at most this) back towards the ground's angle: steadier landings
			double RideHeight() const { return -Anchors[0].Y + WheelRadius + 0.02; }
		};
		const FRallyVehicleSpec& RallyVehicleSpec(ERallyVehicle Vehicle);

		// Nitro (owner request 2026-10-15: flips give nitro). A clean landing after a full flip fills the meter.
		constexpr double RallyNitroPerFlip = 0.34;   // three flips fill it
		constexpr double RallyNitroBurn = 0.5;       // meter per second while it burns
		constexpr double RallyNitroAccel = 14.0;     // m/s per second along the vehicle
		constexpr double RallyNitroTopBonus = 7.0;   // m/s above the vehicle's top speed

		struct FRallyRules
		{
			double RivalPace[RallyRivals] = {1.15, 1.30, 1.50};   // rivals' finish times as multiples of the reference run
			double TimeLimitFactor = 3.0;                          // the race is lost after this many reference times
		};
		FRallyRules RallyRules(EDifficulty Difficulty);

		struct FRallyBody
		{
			FVec2 Pos;
			FVec2 Vel;
			double Angle = 0.0;
			double Spin = 0.0;   // chassis: angular velocity; wheels: rolling rate (rad/s, positive = rolling forward)
			double Turn = 0.0;   // wheels: the drawn rotation
		};

		struct FRallyGhostFrame
		{
			double X = 0.0;
			double Y = 0.0;
			double Angle = 0.0;
		};

		enum class ERallyEventKind : int
		{
			Coin = 0,
			Checkpoint,
			Crash,
			Respawn,
			Bomb,
			Spring,
			Boost,
			Land,       // A = impact speed x 10
			Finished,   // A = place (1-4)
			TimeUp,
			Flip,       // a landed flip: A = flips in that jump (negative = front flips)
			Nitro       // the nitro was lit
		};

		struct FRallyEvent
		{
			ERallyEventKind Kind = ERallyEventKind::Coin;
			int A = 0;
		};

		class FArkRally
		{
		public:
			// Starts a built-in track (Seed picks the rival riders). The reference run (the rivals' ghosts) is driven here.
			// The rivals ride the same vehicle as the player.
			void Start(EDifficulty InDifficulty, uint32_t Seed, ERallyVehicle InVehicle = ERallyVehicle::DirtBike);
			// Starts a player-made track: Medium rivals, no reward. False when the code cannot be read (Error says why).
			bool StartCustom(const std::string& Code, uint32_t Seed, std::string& Error, ERallyVehicle InVehicle = ERallyVehicle::DirtBike);
			void Tick(double DeltaSeconds);

			void SetThrottle(int Value);   // +1 gas, -1 brake / reverse, 0 coast
			void SetLean(int Value);       // +1 lean back (nose up), -1 lean forward, 0 none
			void SetNitro(bool bHold);     // hold to burn the nitro meter
			void Respawn();                // back to safe ground now (counts as a crash), for a vehicle that is stuck
			void Autopilot();              // sets the inputs like the reference rider (call it every step; the verify script's autoplay)

			EDifficulty GetDifficulty() const { return Difficulty; }
			ERallyVehicle Vehicle() const { return VehicleKind; }
			const FRallyVehicleSpec& Spec() const { return RallyVehicleSpec(VehicleKind); }
			double Nitro() const { return NitroLevel; }        // 0..1
			bool IsNitroOn() const { return bNitroOn; }
			int Flips() const { return FlipCount; }
			double AirSpin() const { return AirRotation; }      // rotation so far in this jump (rad)
			bool IsCustom() const { return bCustom; }
			const FRallyRules& Rules() const { return R; }
			const FRallyTrack& Track() const { return T; }
			const FRallyBody& Chassis() const { return Body; }
			const FRallyBody& Wheel(int Index) const { return Wheels[Index == 0 ? 0 : 1]; }
			FVec2 HeadPos() const;
			double Speed() const;
			double Time() const { return Clock; }
			double TimeLimit() const { return Limit; }
			double ReferenceTime() const { return Reference; }
			bool HasGhosts() const { return !Trace.empty(); }
			int Coins() const { return CoinCount; }
			int Crashes() const { return CrashCount; }
			bool IsCrashed() const { return CrashTimer > 0.0; }
			bool IsGrounded() const { return bGrounded[0] || bGrounded[1]; }
			double Progress() const;           // 0..1 of the way to the finish line
			int RivalFace(int Index) const;    // roster index of the rival rider
			double RivalTime(int Index) const; // when the rival crosses the line
			FRallyGhostFrame RivalAt(int Index, double AtTime) const;
			int Place() const;                 // 1-4: the current place (by distance), the final place once finished
			bool IsOver() const { return bOver; }
			bool IsWon() const { return bWon; }
			int Stars() const;                 // 0..5 at the end
			long long Score() const;
			std::vector<FRallyEvent> PopEvents();

		private:
			void Begin(EDifficulty InDifficulty, uint32_t Seed);
			void Spawn(double X, double Speed = 0.0);
			double TiltFromGround() const;
			void Step(double Dt);
			void Physics(double Dt);
			void Things();
			void Crash();
			void Emit(ERallyEventKind Kind, int A = 0) { Events.push_back({Kind, A}); }
			void DriveReference();

			EDifficulty Difficulty = EDifficulty::Easy;
			bool bCustom = false;
			FRallyRules R;
			FRallyTrack T;
			std::mt19937 Rng;
			double Clock = 0.0;
			double Pending = 0.0;
			double Limit = 120.0;
			double Reference = 60.0;
			FRallyBody Body;
			FRallyBody Wheels[2];
			bool bGrounded[2] = {false, false};
			double AirTime = 0.0;
			double StuckSince = -1.0;   // the autopilot's stuck timer
			double SpringCooldown = 0.0;
			int Throttle = 0;
			int Lean = 0;
			double CrashTimer = 0.0;
			double LastCheckpoint = 4.0;
			ERallyVehicle VehicleKind = ERallyVehicle::DirtBike;
			double NitroLevel = 0.0;
			bool bNitroHeld = false;
			bool bNitroOn = false;
			double AirRotation = 0.0;    // rotation while airborne
			int PendingFlips = 0;        // landed flips waiting for 0.3 s of riding without a crash
			double PendingTimer = 0.0;
			int FlipCount = 0;
			double RoofTimer = 0.0;      // how long the head / roof has been on the ground
			double LandingAssist = 0.0;  // seconds left of the landing assist
			struct FSafeSpot
			{
				double Time = 0.0;
				double X = 0.0;
				double Speed = 0.0;
			};
			std::vector<FSafeSpot> SafeSpots;   // the last few seconds of stable riding (respawn points)
			double NextSafeSpot = 0.0;
			double LastCrashTime = -100.0;
			int CrashStreak = 0;         // crashes in quick succession go one safe spot further back each time
			double RespawnX = 4.0;       // where the current crash comes back
			double RespawnSpeed = 0.0;
			int CoinCount = 0;
			int CrashCount = 0;
			double FinishTime = 0.0;
			int FinalPlace = 4;
			bool bOver = false;
			bool bWon = false;
			int Faces[RallyRivals] = {0, 1, 2};
			std::vector<FRallyGhostFrame> Trace;   // the reference run, every 0.1 s
			std::vector<FRallyEvent> Events;
		};

		// ============================================================ the profile side
		std::string FinishRally(FProfile& Profile, const FArkRally& Game);
	}
}
