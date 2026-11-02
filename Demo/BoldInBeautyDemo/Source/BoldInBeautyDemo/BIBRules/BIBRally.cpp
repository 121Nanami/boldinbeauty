// Boldinbeauty demo — Ark Rally (see BIBRally.h). Engine-independent; every number is PLACEHOLDER.
#include "BIBRally.h"

#include "BIBCore.h"
#include "BIBMetaOps.h"
#include "BIBMiniGames.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace BIB
{
	namespace Arcade
	{
		namespace
		{
			constexpr double Pi = 3.14159265358979323846;
			constexpr double Sample = 0.25;        // ground points every 25 cm
			constexpr double RunUp = 8.0;          // flat ground before the first segment
			constexpr double RunOut = 40.0;        // flat ground after the finish line
			constexpr double PitDepth = 30.0;
			constexpr double MaxTrackLength = 2000.0;
			constexpr int MaxTokens = 200;
			constexpr double MaxSlope = 0.9;        // steepest hill (about 42 degrees), so every hill can be climbed from a standstill

			// Shared physics (PLACEHOLDER). The vehicles' own numbers are in RallyVehicleSpec().
			constexpr double Gravity = 12.0;
			constexpr double AxleStiffness = 900.0;   // the wheels along the chassis
			constexpr double AxleDamping = 20.0;
			constexpr double BumpStop = 1500.0;
			constexpr double ReverseSpeed = 6.0;
			constexpr double BrakeRate = 260.0;       // rad/s per second (grip is the real limit)
			constexpr double BoostAccel = 18.0;
			constexpr double BoostTop = 24.0;
			constexpr double BoostLength = 3.5;
			constexpr double SpringLaunch = 11.0;
			constexpr double BombKick = 10.0;
			constexpr double SafeSpotEvery = 0.5;     // seconds between respawn points while riding steadily
			constexpr size_t SafeSpotsKept = 16;

			double WrapAngle(double Angle)
			{
				return std::remainder(Angle, 2.0 * Pi);
			}

			FVec2 Add(FVec2 A, FVec2 B) { return {A.X + B.X, A.Y + B.Y}; }
			FVec2 Sub(FVec2 A, FVec2 B) { return {A.X - B.X, A.Y - B.Y}; }
			FVec2 Mul(FVec2 A, double S) { return {A.X * S, A.Y * S}; }
			double Dot(FVec2 A, FVec2 B) { return A.X * B.X + A.Y * B.Y; }
			double Cross(FVec2 A, FVec2 B) { return A.X * B.Y - A.Y * B.X; }
			double Length(FVec2 A) { return std::sqrt(Dot(A, A)); }

			FVec2 Rotate(FVec2 Local, double Angle)
			{
				const double C = std::cos(Angle);
				const double S = std::sin(Angle);
				return {C * Local.X - S * Local.Y, S * Local.X + C * Local.Y};
			}

			// Velocity of a point R away from the centre of a body turning at Spin.
			FVec2 PointVelocity(FVec2 Vel, double Spin, FVec2 R)
			{
				return {Vel.X - Spin * R.Y, Vel.Y + Spin * R.X};
			}

			struct FContact
			{
				bool bHit = false;
				FVec2 Normal;
				double Depth = 0.0;
			};

			// A circle against the ground line: the closest piece decides; a centre under the surface is pushed out along that
			// piece's outward normal (so walls and drops work, not only a height field).
			FContact Collide(const FRallyTrack& Track, FVec2 Centre, double Radius)
			{
				FContact Out;
				const std::vector<FVec2>& G = Track.Ground;
				if (G.size() < 2)
				{
					return Out;
				}
				const double Low = Centre.X - Radius - 0.05;
				const double High = Centre.X + Radius + 0.05;
				auto First = std::lower_bound(G.begin(), G.end(), Low, [](const FVec2& P, double X) { return P.X < X; });
				size_t Index = static_cast<size_t>(std::max<std::ptrdiff_t>(0, (First - G.begin()) - 1));
				double Best = 1e18;
				FVec2 BestDiff;
				FVec2 BestNormal{0.0, 1.0};
				double BestSide = 1.0;
				for (; Index + 1 < G.size() && G[Index].X <= High; ++Index)
				{
					const FVec2 A = G[Index];
					const FVec2 D = Sub(G[Index + 1], A);
					const double Len2 = Dot(D, D);
					if (Len2 < 1e-12)
					{
						continue;
					}
					const double T = std::clamp(Dot(Sub(Centre, A), D) / Len2, 0.0, 1.0);
					const FVec2 Diff = Sub(Centre, Add(A, Mul(D, T)));
					const double Dist = Length(Diff);
					if (Dist < Best)
					{
						const double Len = std::sqrt(Len2);
						Best = Dist;
						BestDiff = Diff;
						BestNormal = {-D.Y / Len, D.X / Len};
						BestSide = Dot(Diff, BestNormal);
					}
				}
				if (Best > 1e17)
				{
					return Out;
				}
				if (BestSide < 0.0)
				{
					Out.bHit = true;
					Out.Normal = BestNormal;
					Out.Depth = Radius + Best;
				}
				else if (Best < Radius)
				{
					Out.bHit = true;
					Out.Normal = Best > 1e-6 ? Mul(BestDiff, 1.0 / Best) : BestNormal;
					Out.Depth = Radius - Best;
				}
				return Out;
			}

			double SlopeAngle(const FRallyTrack& Track, double X)
			{
				return std::clamp(std::atan2(Track.HeightAt(X + 0.8) - Track.HeightAt(X - 0.8), 1.6), -0.9, 0.9);
			}

			std::string Seconds(double Value)
			{
				char Text[32];
				std::snprintf(Text, sizeof(Text), "%.1f", Value);
				return Text;
			}

			bool ReadNumber(const std::string& Text, double& Out)
			{
				if (Text.empty())
				{
					return false;
				}
				char* End = nullptr;
				Out = std::strtod(Text.c_str(), &End);
				return End && *End == '\0' && std::isfinite(Out);
			}
		}

		// ======================================================================== tracks
		double FRallyTrack::HeightAt(double X) const
		{
			if (Ground.empty())
			{
				return 0.0;
			}
			if (X <= Ground.front().X)
			{
				return Ground.front().Y;
			}
			if (X >= Ground.back().X)
			{
				return Ground.back().Y;
			}
			auto Next = std::upper_bound(Ground.begin(), Ground.end(), X, [](double Value, const FVec2& P) { return Value < P.X; });
			const size_t I = static_cast<size_t>(Next - Ground.begin());   // Ground[I - 1].X <= X < Ground[I].X
			const FVec2 A = Ground[I - 1];
			const FVec2 B = Ground[I];
			double Y = A.Y + (B.Y - A.Y) * (X - A.X) / std::max(1e-9, B.X - A.X);
			if (I >= 2 && X == A.X && Ground[I - 2].X == A.X)
			{
				Y = std::max(Y, Ground[I - 2].Y);   // exactly on a vertical piece: its top
			}
			return Y;
		}

		bool BuildTrack(const std::string& Code, const std::string& Name, FRallyTrack& Out, std::string& Error)
		{
			Out = FRallyTrack();
			Out.Name = Name;
			Out.Code = Code;
			if (Code.size() > static_cast<size_t>(RallyCodeMaxLength))
			{
				Error = "The track code is too long (at most " + std::to_string(RallyCodeMaxLength) + " characters).";
				return false;
			}
			std::vector<std::string> Tokens;
			{
				std::string Word;
				for (char Ch : Code + " ")
				{
					if (std::isspace(static_cast<unsigned char>(Ch)))
					{
						if (!Word.empty())
						{
							Tokens.push_back(Word);
						}
						Word.clear();
					}
					else
					{
						Word += Ch;
					}
				}
			}
			if (Tokens.empty())
			{
				Error = "The track is empty: start with a segment such as F20 or H15,3.";
				return false;
			}
			if (Tokens.size() > static_cast<size_t>(MaxTokens))
			{
				Error = "Too many pieces (at most " + std::to_string(MaxTokens) + ").";
				return false;
			}
			double X = 0.0;
			double Base = 0.0;
			std::vector<FVec2>& G = Out.Ground;
			G.push_back({-30.0, 0.0});
			G.push_back({0.0, 0.0});
			auto Profile = [&G, &X](double Len, auto&& HeightOf)
			{
				const int Count = std::max(1, static_cast<int>(std::ceil(Len / Sample)));
				for (int K = 1; K <= Count; ++K)
				{
					const double T = static_cast<double>(K) / Count;
					G.push_back({X + Len * T, HeightOf(T)});
				}
				X += Len;
			};
			Profile(RunUp, [&Base](double) { return Base; });
			Out.StartX = 4.0;
			int Segments = 0;
			for (const std::string& Token : Tokens)
			{
				const char Kind = static_cast<char>(std::toupper(static_cast<unsigned char>(Token[0])));
				std::vector<double> Args;
				{
					std::string Rest = Token.substr(1);
					size_t Begin = 0;
					while (!Rest.empty() && Begin <= Rest.size())
					{
						const size_t Comma = Rest.find(',', Begin);
						const std::string Part = Rest.substr(Begin, Comma == std::string::npos ? std::string::npos : Comma - Begin);
						double Value = 0.0;
						if (!ReadNumber(Part, Value))
						{
							Error = "\"" + Token + "\" has a number that cannot be read.";
							return false;
						}
						Args.push_back(Value);
						if (Comma == std::string::npos)
						{
							break;
						}
						Begin = Comma + 1;
					}
				}
				auto Need = [&Args, &Token, &Error](size_t Count) -> bool
				{
					if (Args.size() != Count)
					{
						Error = "\"" + Token + "\" needs " + std::to_string(Count) + (Count == 1 ? " number." : " numbers separated by a comma.");
						return false;
					}
					return true;
				};
				auto Fail = [&Error, &Token](const std::string& Why)
				{
					Error = "\"" + Token + "\": " + Why;
					return false;
				};
				if (std::string("FHURGW").find(Kind) != std::string::npos)
				{
					if (!Need(Kind == 'F' || Kind == 'G' ? 1 : 2))
					{
						return false;
					}
					const double Len = Args[0];
					if (Len < 1.0 || Len > 200.0)
					{
						return Fail("a length is 1 to 200 m.");
					}
					if (X + Len > MaxTrackLength)
					{
						return Fail("the track would be longer than 2,000 m.");
					}
					const double V = Args.size() > 1 ? Args[1] : 0.0;
					if (std::abs(V) > 15.0)
					{
						return Fail("a height is -15 to 15 m.");
					}
					++Segments;
					switch (Kind)
					{
					case 'F':
						Profile(Len, [&Base](double) { return Base; });
						break;
					case 'H':
						if (Pi * std::abs(V) / Len > MaxSlope)
						{
							return Fail("too steep - make it longer or lower.");
						}
						Profile(Len, [&Base, V](double T) { return Base + V * 0.5 * (1.0 - std::cos(2.0 * Pi * T)); });
						break;
					case 'U':
						if (Pi * std::abs(V) / (2.0 * Len) > MaxSlope)
						{
							return Fail("too steep - make it longer or lower.");
						}
						Profile(Len, [&Base, V](double T) { return Base + V * 0.5 * (1.0 - std::cos(Pi * T)); });
						Base += V;
						break;
					case 'R':
						if (V <= 0.0 || V > Len)
						{
							return Fail("a ramp is 0 to its length high.");
						}
						Profile(Len, [&Base, V](double T) { return Base + V * std::pow(T, 1.6); });
						G.push_back({X, Base});
						break;
					case 'G':
						if (Len > 25.0)
						{
							return Fail("a gap is at most 25 m.");
						}
						G.push_back({X, Base - PitDepth});
						G.push_back({X + Len, Base - PitDepth});
						G.push_back({X + Len, Base});
						Out.Pits.push_back({X, X + Len, Base});
						X += Len;
						break;
					case 'W':
					{
						if (V < 0.0 || V > 0.4)
						{
							return Fail("bumps are 0 to 0.4 m high.");
						}
						const double Bumps = std::max(1.0, std::round(Len / 1.6));
						Profile(Len, [&Base, V, Bumps](double T) { return Base + V * 0.5 * (1.0 - std::cos(2.0 * Pi * T * Bumps)); });
						break;
					}
					default:
						break;
					}
					continue;
				}
				if (Kind == 'C')
				{
					if (!Need(1) || Args[0] < 1.0 || Args[0] > 30.0)
					{
						return Error.empty() ? Fail("a coin row has 1 to 30 coins.") : false;
					}
					const int Count = static_cast<int>(Args[0]);
					for (int K = 0; K < Count; ++K)
					{
						Out.Things.push_back({ERallyThing::Coin, X + 1.6 * K, Base, false});
					}
					continue;
				}
				const std::string Single = "BSPK";
				if (Single.find(Kind) != std::string::npos && Token.size() == 1)
				{
					const ERallyThing Thing = Kind == 'B' ? ERallyThing::Bomb : (Kind == 'S' ? ERallyThing::Spring : (Kind == 'P' ? ERallyThing::Boost : ERallyThing::Checkpoint));
					Out.Things.push_back({Thing, X, Base, false});
					continue;
				}
				return Fail("unknown piece. Use F, H, U, R, G, W (ground) or C, B, S, P, K (things).");
			}
			if (Segments == 0)
			{
				Error = "The track needs at least one ground piece (F, H, U, R, G or W).";
				return false;
			}
			Out.FinishX = X + 2.0;
			Profile(RunOut, [&Base](double) { return Base; });
			Out.EndX = X;
			G.push_back({X + 30.0, Base});
			for (FRallyThing& Thing : Out.Things)
			{
				const double Ground = Out.HeightAt(Thing.X);
				Thing.Y = Thing.Kind == ERallyThing::Coin ? std::max(Ground, Thing.Y) + 1.1 : Ground;
				Out.Coins += Thing.Kind == ERallyThing::Coin ? 1 : 0;
			}
			Error.clear();
			return true;
		}

		const char* RallyTrackName(EDifficulty Difficulty)
		{
			switch (Difficulty)
			{
			case EDifficulty::Easy: return "Ark Outskirts";
			case EDifficulty::Medium: return "Neo Tokyo Rooftops";
			case EDifficulty::Hard:
			case EDifficulty::Count: break;
			}
			return "Halo Ruins";
		}

		const char* RallyTrackCode(EDifficulty Difficulty)
		{
			// Each one is driven clean by the autopilot (Tests/minigame_tests.cpp checks it), so the rivals' ghosts never crash.
			switch (Difficulty)
			{
			case EDifficulty::Easy:
				return "F6 C4 H14,1.5 F6 H18,2.5 F5 C5 U20,4 F8 K W12,0.25 F6 C4 R10,1.6 F14 U20,-4 H16,2 S F14 C6 F4 K H20,3 F8 P F20 H12,-1.5 F10 C5 "
					"H24,3 F6 U18,3 F10 K R10,1.5 F16 C6 W14,0.3 F8 U18,-3 H16,2 F12";
			case EDifficulty::Medium:
				return "F6 H16,2 C4 F5 R12,2 G4 F12 K U18,5 H14,2 F5 B F10 R10,2.5 G6 F14 C6 W14,0.35 K F6 S F14 U16,-5 P F16 R12,3 G7 F14 C5 H18,-2.5 F8 K "
					"H14,3 H14,3 F8 C6 R12,2.5 G6 F16 U16,4 W12,0.4 F8 K B F6 R10,2 G5 F12 U18,-4 C5 H16,2 F12";
			case EDifficulty::Hard:
			case EDifficulty::Count:
				break;
			}
			return "F6 R12,2.5 G6 F12 H14,3 F4 B F6 K U16,6 R10,2 G8 F14 C6 H12,2.5 H12,2.5 K F10 U20,-6 W16,0.4 F10 R14,3.5 G10 F8 B F8 K S F16 H14,-3 P "
				"F20 R14,3 G9 F18 K C6 H12,3 H12,3 H12,3 F8 U16,5 R12,3 G10 F14 B F6 K W18,0.35 F8 U20,-5 R14,3 G11 F26 C5 H16,-2.5 F10";
		}

		FRallyRules RallyRules(EDifficulty Difficulty)
		{
			FRallyRules Rules;
			switch (Difficulty)
			{
			case EDifficulty::Easy:
				Rules = {{1.15, 1.30, 1.50}, 3.0};
				break;
			case EDifficulty::Medium:
				Rules = {{1.02, 1.12, 1.25}, 2.5};
				break;
			case EDifficulty::Hard:
			case EDifficulty::Count:
				Rules = {{0.94, 1.02, 1.10}, 2.2};
				break;
			}
			return Rules;
		}

		// ======================================================================== the vehicles
		const char* RallyVehicleName(ERallyVehicle Vehicle)
		{
			return Vehicle == ERallyVehicle::Buggy ? "Buggy" : "Dirt bike";
		}

		const FRallyVehicleSpec& RallyVehicleSpec(ERallyVehicle Vehicle)
		{
			// Dirt bike: light, quick to rotate (easy flips, lots of nitro), the fastest; it crashes when it lands on the rider.
			static const FRallyVehicleSpec Bike = []
			{
				FRallyVehicleSpec Spec;
				Spec.MaxSpeed = 22.0;
				return Spec;
			}();
			// Buggy: heavy, wide and low, big wheels; slow to rotate (flips are hard), no wheelies, a roll cage that only gives
			// way when it lies on its roof for a while.
			static const FRallyVehicleSpec Buggy = []
			{
				FRallyVehicleSpec Spec;
				Spec.ChassisMass = 1.6;
				Spec.ChassisInertia = 1.5;
				Spec.WheelMass = 0.26;
				Spec.WheelRadius = 0.46;
				Spec.Anchors[0] = {-1.0, -0.36};
				Spec.Anchors[1] = {1.0, -0.36};
				Spec.FramePoints[0] = {-1.1, 0.0};
				Spec.FramePoints[1] = {1.1, 0.0};
				Spec.FramePoints[2] = {0.0, -0.1};
				Spec.FrameRadius = 0.18;
				Spec.HeadLocal = {0.0, 0.72};
				Spec.HeadRadius = 0.32;
				Spec.SuspStiffness = 230.0;
				Spec.SuspDamping = 10.0;
				Spec.SuspTravel = 0.22;
				Spec.Grip = 1.3;
				Spec.MotorTorque = 9.0;
				Spec.MotorKick = 0.1;
				Spec.MaxSpeed = 17.5;
				Spec.LeanTorque = 9.0;
				Spec.WheelieTorque = 0.0;
				Spec.SpinDamping = 0.9;
				Spec.CrashTilt = 2.2;
				Spec.RoofTime = 1.2;
				Spec.AutoLevel = 2.5;
				Spec.LandingSpinCap = 1.2;
				return Spec;
			}();
			return Vehicle == ERallyVehicle::Buggy ? Buggy : Bike;
		}

		// ======================================================================== the race
		void FArkRally::Start(EDifficulty InDifficulty, uint32_t Seed, ERallyVehicle InVehicle)
		{
			*this = FArkRally();
			VehicleKind = InVehicle == ERallyVehicle::Buggy ? ERallyVehicle::Buggy : ERallyVehicle::DirtBike;
			std::string Error;
			BuildTrack(RallyTrackCode(InDifficulty), RallyTrackName(InDifficulty), T, Error);
			Begin(InDifficulty, Seed);
		}

		bool FArkRally::StartCustom(const std::string& Code, uint32_t Seed, std::string& Error, ERallyVehicle InVehicle)
		{
			FRallyTrack Built;
			if (!BuildTrack(Code, "Custom track", Built, Error))
			{
				return false;
			}
			*this = FArkRally();
			VehicleKind = InVehicle == ERallyVehicle::Buggy ? ERallyVehicle::Buggy : ERallyVehicle::DirtBike;
			T = std::move(Built);
			bCustom = true;
			Begin(EDifficulty::Medium, Seed);
			return true;
		}

		void FArkRally::Begin(EDifficulty InDifficulty, uint32_t Seed)
		{
			Difficulty = InDifficulty;
			R = RallyRules(InDifficulty);
			Rng.seed(Seed);
			std::vector<int> Pool;
			for (int Index = 0; Index < static_cast<int>(Roster().size()); ++Index)
			{
				if (Roster()[static_cast<size_t>(Index)].Kit != EKit::Placeholder)
				{
					Pool.push_back(Index);
				}
			}
			std::shuffle(Pool.begin(), Pool.end(), Rng);
			for (int Index = 0; Index < RallyRivals; ++Index)
			{
				Faces[Index] = Pool.empty() ? Index : Pool[static_cast<size_t>(Index) % Pool.size()];
			}
			LastCheckpoint = T.StartX;
			Spawn(T.StartX);
			DriveReference();
		}

		void FArkRally::DriveReference()
		{
			// The rivals' ghosts replay one reference run by the autopilot, each at its own pace.
			Reference = 1e6;
			Limit = 1e6;
			FArkRally Bot = *this;
			std::vector<FRallyGhostFrame> Frames;
			int Steps = 0;
			while (!Bot.bOver && Bot.Clock < 400.0 && Bot.CrashCount <= 6)
			{
				if (Steps % 24 == 0)
				{
					Frames.push_back({Bot.Body.Pos.X, Bot.Body.Pos.Y, Bot.Body.Angle});
				}
				Bot.Autopilot();
				Bot.Step(RallyStep);
				++Steps;
			}
			if (Bot.bOver && Bot.bWon)
			{
				Reference = Bot.FinishTime;
				Frames.push_back({Bot.Body.Pos.X, Bot.Body.Pos.Y, Bot.Body.Angle});
				Trace = std::move(Frames);
			}
			else
			{
				Reference = (T.FinishX - T.StartX) / 9.0;   // the autopilot could not finish: the ghosts glide along the ground
				Trace.clear();
			}
			Limit = std::max(60.0, Reference * R.TimeLimitFactor);
		}

		void FArkRally::Spawn(double X, double Speed)
		{
			const FRallyVehicleSpec& V = Spec();
			const double Angle = SlopeAngle(T, X);
			const FVec2 Along = Rotate({1.0, 0.0}, Angle);
			Body = FRallyBody();
			Body.Pos = {X, T.HeightAt(X) + V.RideHeight()};
			Body.Angle = Angle;
			Body.Vel = Mul(Along, Speed);
			for (int Index = 0; Index < 2; ++Index)
			{
				Wheels[Index] = FRallyBody();
				Wheels[Index].Pos = Add(Body.Pos, Rotate(V.Anchors[Index], Angle));
				Wheels[Index].Vel = Body.Vel;
				Wheels[Index].Spin = Speed / V.WheelRadius;
				bGrounded[Index] = false;
			}
			AirTime = 0.0;
			AirRotation = 0.0;
			RoofTimer = 0.0;
			LandingAssist = 0.0;
			PendingFlips = 0;
		}

		double FArkRally::TiltFromGround() const
		{
			return std::abs(WrapAngle(Body.Angle - SlopeAngle(T, Body.Pos.X)));
		}

		void FArkRally::SetNitro(bool bHold)
		{
			bNitroHeld = bHold;
		}

		void FArkRally::SetThrottle(int Value)
		{
			Throttle = std::clamp(Value, -1, 1);
		}

		void FArkRally::SetLean(int Value)
		{
			Lean = std::clamp(Value, -1, 1);
		}

		void FArkRally::Respawn()
		{
			if (!bOver && CrashTimer <= 0.0)
			{
				Crash();
			}
		}

		void FArkRally::Autopilot()
		{
			Throttle = 1;
			if (Speed() >= 0.5 || CrashTimer > 0.0)
			{
				StuckSince = -1.0;
			}
			else if (StuckSince < 0.0)
			{
				StuckSince = Clock;
			}
			else if (Clock - StuckSince > 3.0)
			{
				StuckSince = -1.0;
				Respawn();   // wedged somewhere: back to the checkpoint
				return;
			}
			double Target = SlopeAngle(T, Body.Pos.X);
			if (!IsGrounded())
			{
				// Line up with the ground where the bike will land.
				double Ahead = Body.Pos.X + std::max(0.0, Body.Vel.X) * 0.45;
				for (const FRallyPit& Pit : T.Pits)
				{
					if (Ahead > Pit.X0 && Ahead < Pit.X1)
					{
						Ahead = Pit.X1 + 1.0;
					}
				}
				Target = SlopeAngle(T, Ahead);
			}
			const double Error = Target - Body.Angle - 0.45 * Body.Spin;
			if (IsGrounded())
			{
				Lean = Error < -0.3 ? -1 : 0;   // on the ground only catch a wheelie (leaning back there would lift the front)
				return;
			}
			Lean = Error > 0.06 ? 1 : (Error < -0.06 ? -1 : 0);
		}

		FVec2 FArkRally::HeadPos() const
		{
			return Add(Body.Pos, Rotate(Spec().HeadLocal, Body.Angle));
		}

		double FArkRally::Speed() const
		{
			return Length(Body.Vel);
		}

		double FArkRally::Progress() const
		{
			return std::clamp((Body.Pos.X - T.StartX) / std::max(1.0, T.FinishX - T.StartX), 0.0, 1.0);
		}

		int FArkRally::RivalFace(int Index) const
		{
			return Faces[std::clamp(Index, 0, RallyRivals - 1)];
		}

		double FArkRally::RivalTime(int Index) const
		{
			return Reference * R.RivalPace[std::clamp(Index, 0, RallyRivals - 1)];
		}

		FRallyGhostFrame FArkRally::RivalAt(int Index, double AtTime) const
		{
			const double Pace = R.RivalPace[std::clamp(Index, 0, RallyRivals - 1)];
			const double Into = std::max(0.0, AtTime) / std::max(0.1, Pace);   // seconds into the reference run
			if (Trace.empty())
			{
				const double X = T.StartX + (T.FinishX + 3.0 - T.StartX) * std::min(1.0, Into / std::max(1.0, Reference));
				return {X, T.HeightAt(X) + Spec().RideHeight(), SlopeAngle(T, X)};
			}
			const double Slot = Into / 0.1;
			const size_t I = static_cast<size_t>(std::min(Slot, static_cast<double>(Trace.size() - 1)));
			if (I + 1 >= Trace.size())
			{
				return Trace.back();
			}
			const double F = Slot - static_cast<double>(I);
			const FRallyGhostFrame& A = Trace[I];
			const FRallyGhostFrame& B = Trace[I + 1];
			return {A.X + (B.X - A.X) * F, A.Y + (B.Y - A.Y) * F, A.Angle + (B.Angle - A.Angle) * F};
		}

		int FArkRally::Place() const
		{
			if (bOver && bWon)
			{
				return FinalPlace;
			}
			int Ahead = 0;
			for (int Index = 0; Index < RallyRivals; ++Index)
			{
				Ahead += RivalAt(Index, Clock).X > Body.Pos.X ? 1 : 0;
			}
			return 1 + Ahead;
		}

		int FArkRally::Stars() const
		{
			if (!bOver || !bWon)
			{
				return 0;
			}
			return std::clamp(1 + (4 - FinalPlace) + (CrashCount == 0 ? 1 : 0), 0, 5);
		}

		long long FArkRally::Score() const
		{
			if (!bWon)
			{
				return 50LL * CoinCount;
			}
			const long long TimeBonus = static_cast<long long>(std::max(0.0, Limit - FinishTime) * 20.0);
			return 1000LL * (4 - FinalPlace) + 100LL * CoinCount + 150LL * FlipCount + (CrashCount == 0 ? 500LL : 0LL) + TimeBonus;
		}

		std::vector<FRallyEvent> FArkRally::PopEvents()
		{
			std::vector<FRallyEvent> Out;
			Out.swap(Events);
			return Out;
		}

		void FArkRally::Tick(double DeltaSeconds)
		{
			Pending += std::max(0.0, DeltaSeconds);
			while (Pending >= RallyStep && !bOver)
			{
				Pending -= RallyStep;
				Step(RallyStep);
			}
			if (bOver)
			{
				Pending = 0.0;
			}
		}

		void FArkRally::Step(double Dt)
		{
			if (bOver)
			{
				return;
			}
			Clock += Dt;
			if (CrashTimer > 0.0)
			{
				CrashTimer -= Dt;
				if (CrashTimer <= 0.0)
				{
					CrashTimer = 0.0;
					Spawn(RespawnX, RespawnSpeed);
					Emit(ERallyEventKind::Respawn);
				}
			}
			else
			{
				Physics(Dt);
				if (CrashTimer <= 0.0)
				{
					Things();
					// A landed flip counts once the vehicle has ridden on for a moment without crashing: then it pays nitro.
					if (PendingFlips != 0 && (PendingTimer -= Dt) <= 0.0)
					{
						const int Count = std::abs(PendingFlips);
						FlipCount += Count;
						NitroLevel = std::min(1.0, NitroLevel + RallyNitroPerFlip * Count);
						Emit(ERallyEventKind::Flip, PendingFlips);
						PendingFlips = 0;
					}
					// Respawn points: steady riding on the ground, away from gaps.
					if (Clock >= NextSafeSpot && bGrounded[0] && bGrounded[1] && TiltFromGround() < 0.35 && Body.Vel.X > 2.0)
					{
						bool bNearPit = false;
						for (const FRallyPit& Pit : T.Pits)
						{
							bNearPit = bNearPit || (Body.Pos.X > Pit.X0 - 2.0 && Body.Pos.X < Pit.X1 + 2.0);
						}
						if (!bNearPit)
						{
							SafeSpots.push_back({Clock, Body.Pos.X, Dot(Body.Vel, Rotate({1.0, 0.0}, Body.Angle))});
							if (SafeSpots.size() > SafeSpotsKept)
							{
								SafeSpots.erase(SafeSpots.begin());
							}
							NextSafeSpot = Clock + SafeSpotEvery;
						}
					}
				}
				if (CrashTimer <= 0.0 && Body.Pos.X >= T.FinishX)
				{
					FinishTime = Clock;
					int Beaten = 0;
					for (int Index = 0; Index < RallyRivals; ++Index)
					{
						Beaten += FinishTime < RivalTime(Index) ? 1 : 0;
					}
					FinalPlace = 1 + RallyRivals - Beaten;
					bOver = true;
					bWon = true;
					Emit(ERallyEventKind::Finished, FinalPlace);
					return;
				}
			}
			if (Clock >= Limit)
			{
				bOver = true;
				bWon = false;
				Emit(ERallyEventKind::TimeUp);
			}
		}

		void FArkRally::Physics(double Dt)
		{
			const FRallyVehicleSpec& V = Spec();
			const double WheelInertia = 0.5 * V.WheelMass * V.WheelRadius * V.WheelRadius;
			const FVec2 AxisX = Rotate({1.0, 0.0}, Body.Angle);
			const FVec2 AxisY = Rotate({0.0, 1.0}, Body.Angle);
			const bool bWasAir = !bGrounded[0] && !bGrounded[1];
			FVec2 Force{0.0, -Gravity * V.ChassisMass};
			double Torque = Lean * V.LeanTorque;
			if (Lean > 0 && Throttle > 0 && !bWasAir)
			{
				// A wheelie: strong at first, fading out as the nose rises, so holding it balances instead of looping over.
				const double Rise = WrapAngle(Body.Angle - SlopeAngle(T, Body.Pos.X));
				Torque = std::max(V.LeanTorque * 0.5 * std::clamp(1.0 - Rise / 0.8, 0.0, 1.0), V.WheelieTorque * std::clamp(1.0 - Rise / 0.8, 0.0, 1.0))
					- 4.0 * std::max(0.0, Body.Spin);   // the rider balances it: a held wheelie settles instead of looping over
			}
			if (bWasAir && V.AutoLevel > 0.0 && Lean == 0)   // only while the player is not leaning: steady, yet flips stay possible
			{
				const double Off = WrapAngle(SlopeAngle(T, Body.Pos.X + std::max(0.0, Body.Vel.X) * 0.4) - Body.Angle);
				Torque += std::clamp(Off * 3.0 - Body.Spin, -1.0, 1.0) * V.AutoLevel;
			}
			FVec2 WheelForce[2] = {{0.0, -Gravity * V.WheelMass}, {0.0, -Gravity * V.WheelMass}};

			// Only the rear wheel down and the nose past about 45 degrees: the rider pulls it back down (no loop-outs on the ground).
			if (bGrounded[0] && !bGrounded[1])
			{
				const double Rise = WrapAngle(Body.Angle - SlopeAngle(T, Body.Pos.X));
				if (Rise > 0.8)
				{
					Torque -= 12.0 * (Rise - 0.8) + 3.0 * std::max(0.0, Body.Spin);
				}
			}

			// The motor drives the rear wheel; the brake works on both; reverse when slow.
			const double Forward = Dot(Body.Vel, AxisX);
			FRallyBody& Rear = Wheels[0];
			if (Throttle > 0)
			{
				const double Rev = std::clamp(Rear.Spin * V.WheelRadius / V.MaxSpeed, 0.0, 1.0);
				if (Rev < 1.0)
				{
					const double Drive = V.MotorTorque * (1.0 - 0.5 * Rev);
					Rear.Spin += Drive / WheelInertia * Dt;
					Torque += Drive * V.MotorKick;
				}
			}
			else if (Throttle < 0 && Forward > 1.0)
			{
				for (FRallyBody& Wheel : Wheels)
				{
					Wheel.Spin = Wheel.Spin > 0.0 ? std::max(0.0, Wheel.Spin - BrakeRate * Dt) : std::min(0.0, Wheel.Spin + BrakeRate * Dt);
				}
			}
			else if (Throttle < 0)
			{
				if (Rear.Spin > -ReverseSpeed / V.WheelRadius)
				{
					Rear.Spin -= 0.4 * V.MotorTorque / WheelInertia * Dt;
				}
			}
			else
			{
				for (FRallyBody& Wheel : Wheels)
				{
					Wheel.Spin *= std::exp(-0.3 * Dt);
				}
			}

			// Nitro: a push along the vehicle while the meter lasts, past the normal top speed.
			const bool bWasNitro = bNitroOn;
			bNitroOn = bNitroHeld && NitroLevel > 0.0;
			if (bNitroOn)
			{
				NitroLevel = std::max(0.0, NitroLevel - RallyNitroBurn * Dt);
				if (!bWasNitro)
				{
					Emit(ERallyEventKind::Nitro);
				}
				if (Forward < V.MaxSpeed + RallyNitroTopBonus)
				{
					const FVec2 Push = Mul(AxisX, RallyNitroAccel * Dt);
					Body.Vel = Add(Body.Vel, Push);
					for (FRallyBody& Wheel : Wheels)
					{
						Wheel.Vel = Add(Wheel.Vel, Push);
						Wheel.Spin = std::max(Wheel.Spin, Dot(Wheel.Vel, AxisX) / V.WheelRadius);
					}
				}
			}

			// The suspension: each wheel hangs on a spring at its anchor (stiff along the chassis, soft across it).
			for (int Index = 0; Index < 2; ++Index)
			{
				const FVec2 Arm = Rotate(V.Anchors[Index], Body.Angle);
				const FVec2 Offset = Sub(Wheels[Index].Pos, Add(Body.Pos, Arm));
				const FVec2 RelVel = Sub(Wheels[Index].Vel, PointVelocity(Body.Vel, Body.Spin, Arm));
				const double Ex = Dot(Offset, AxisX);
				const double Ey = Dot(Offset, AxisY);
				double Fx = -AxleStiffness * Ex - AxleDamping * Dot(RelVel, AxisX);
				double Fy = -V.SuspStiffness * Ey - V.SuspDamping * Dot(RelVel, AxisY);
				if (std::abs(Ey) > V.SuspTravel)
				{
					Fy -= BumpStop * (Ey - std::copysign(V.SuspTravel, Ey));
				}
				const FVec2 F = Add(Mul(AxisX, Fx), Mul(AxisY, Fy));
				WheelForce[Index] = Add(WheelForce[Index], F);
				Force = Sub(Force, F);
				Torque += Cross(Arm, Mul(F, -1.0));
			}

			// Integrate (semi-implicit Euler).
			Body.Vel = Add(Body.Vel, Mul(Force, Dt / V.ChassisMass));
			Body.Vel = Mul(Body.Vel, std::exp(-0.02 * Dt));
			Body.Spin += Torque / V.ChassisInertia * Dt;
			Body.Spin *= std::exp(-V.SpinDamping * Dt);
			Body.Pos = Add(Body.Pos, Mul(Body.Vel, Dt));
			Body.Angle += Body.Spin * Dt;
			for (int Index = 0; Index < 2; ++Index)
			{
				FRallyBody& Wheel = Wheels[Index];
				Wheel.Vel = Add(Wheel.Vel, Mul(WheelForce[Index], Dt / V.WheelMass));
				Wheel.Pos = Add(Wheel.Pos, Mul(Wheel.Vel, Dt));
				Wheel.Turn -= Wheel.Spin * Dt;
			}

			// Wheels on the ground: push out, stop the fall, grip.
			bool bAnyGround = false;
			for (int Index = 0; Index < 2; ++Index)
			{
				FRallyBody& Wheel = Wheels[Index];
				const bool bWas = bGrounded[Index];
				const FContact Contact = Collide(T, Wheel.Pos, V.WheelRadius);
				bGrounded[Index] = Contact.bHit;
				if (!Contact.bHit)
				{
					continue;
				}
				bAnyGround = true;
				const FVec2 N = Contact.Normal;
				Wheel.Pos = Add(Wheel.Pos, Mul(N, Contact.Depth));
				const double Vn = Dot(Wheel.Vel, N);
				double Jn = 0.0;
				if (Vn < 0.0)
				{
					Jn = -Vn * 1.05 * V.WheelMass;
					Wheel.Vel = Add(Wheel.Vel, Mul(N, Jn / V.WheelMass));
					if (!bWas && AirTime > 0.25 && -Vn > 3.0)
					{
						Emit(ERallyEventKind::Land, static_cast<int>(-Vn * 10.0));
					}
				}
				const FVec2 Tangent{N.Y, -N.X};
				const double Slip = Dot(Wheel.Vel, Tangent) - Wheel.Spin * V.WheelRadius;
				const double Effective = 1.0 / (1.0 / V.WheelMass + V.WheelRadius * V.WheelRadius / WheelInertia);
				const double Jt = std::clamp(-Slip * Effective, -V.Grip * Jn, V.Grip * Jn);
				Wheel.Vel = Add(Wheel.Vel, Mul(Tangent, Jt / V.WheelMass));
				Wheel.Spin -= Jt * V.WheelRadius / WheelInertia;
			}
			if (bAnyGround)
			{
				if (bWasAir && AirTime > 0.2)
				{
					// Landing: whole turns in the air, landed within about 35 degrees of upright, are flips. A landing close to the
					// ground's angle soaks up the spin so the vehicle settles instead of bouncing over.
					const int Turns = static_cast<int>(std::lround(std::abs(AirRotation) / (2.0 * Pi)));
					if (Turns > 0 && std::abs(std::abs(AirRotation) - Turns * 2.0 * Pi) < 0.6)
					{
						PendingFlips = AirRotation > 0.0 ? Turns : -Turns;
						PendingTimer = 0.3;
					}
					if (TiltFromGround() < 0.7)
					{
						Body.Spin *= 0.4;
						LandingAssist = 0.35;
					}
				}
				AirRotation = 0.0;
				AirTime = 0.0;
			}
			else
			{
				AirRotation += Body.Spin * Dt;
				AirTime += Dt;
			}

			if (LandingAssist > 0.0)
			{
				LandingAssist -= Dt;
				Body.Spin = std::clamp(Body.Spin, -V.LandingSpinCap, V.LandingSpinCap);
			}

			// The frame scrapes along the ground (no grip). The head / roof does too, unless the vehicle is upside down.
			auto Scrape = [this, &V](const FVec2& Arm, double Radius)
			{
				const FContact Contact = Collide(T, Add(Body.Pos, Arm), Radius);
				if (!Contact.bHit)
				{
					return false;
				}
				const FVec2 N = Contact.Normal;
				Body.Pos = Add(Body.Pos, Mul(N, Contact.Depth));
				const FVec2 Vp = PointVelocity(Body.Vel, Body.Spin, Arm);
				const double Vn = Dot(Vp, N);
				if (Vn < 0.0)
				{
					const double Rn = Cross(Arm, N);
					const double J = -1.1 * Vn / (1.0 / V.ChassisMass + Rn * Rn / V.ChassisInertia);
					Body.Vel = Add(Body.Vel, Mul(N, J / V.ChassisMass));
					Body.Spin += Rn * J / V.ChassisInertia;
					const FVec2 Tangent{N.Y, -N.X};
					const double Rt = Cross(Arm, Tangent);
					const double Jt = std::clamp(-Dot(PointVelocity(Body.Vel, Body.Spin, Arm), Tangent) / (1.0 / V.ChassisMass + Rt * Rt / V.ChassisInertia), -0.4 * J, 0.4 * J);
					Body.Vel = Add(Body.Vel, Mul(Tangent, Jt / V.ChassisMass));
					Body.Spin += Rt * Jt / V.ChassisInertia;
				}
				return true;
			};
			bool bBodyDown = false;
			for (const FVec2& Local : V.FramePoints)
			{
				bBodyDown = Scrape(Rotate(Local, Body.Angle), V.FrameRadius) || bBodyDown;
			}
			const FVec2 HeadArm = Rotate(V.HeadLocal, Body.Angle);
			const bool bUpsideDown = TiltFromGround() > V.CrashTilt;
			// The rider's head on the ground; for the buggy, lying on its roof or side (no wheel down) for RoofTime.
			const bool bHeadDown = Collide(T, Add(Body.Pos, HeadArm), V.HeadRadius).bHit;
			if (bUpsideDown && (bHeadDown || (V.RoofTime > 0.0 && bBodyDown && !bAnyGround)))
			{
				RoofTimer += Dt;
				if (RoofTimer >= V.RoofTime)
				{
					Crash();
					return;
				}
			}
			else
			{
				RoofTimer = 0.0;
			}
			Scrape(HeadArm, V.HeadRadius);
			for (const FRallyPit& Pit : T.Pits)
			{
				if (Body.Pos.X > Pit.X0 && Body.Pos.X < Pit.X1 && Body.Pos.Y < Pit.LipY - 4.0)
				{
					Crash();
					return;
				}
			}
			if (Body.Pos.Y < -500.0 || !std::isfinite(Body.Pos.X) || !std::isfinite(Body.Pos.Y))
			{
				Crash();
			}
		}

		void FArkRally::Things()
		{
			SpringCooldown = std::max(0.0, SpringCooldown - RallyStep);
			const FVec2 AxisX = Rotate({1.0, 0.0}, Body.Angle);
			for (FRallyThing& Thing : T.Things)
			{
				switch (Thing.Kind)
				{
				case ERallyThing::Coin:
					if (!Thing.bUsed && (Length(Sub(Body.Pos, {Thing.X, Thing.Y})) < 1.1 || Length(Sub(HeadPos(), {Thing.X, Thing.Y})) < 0.8))
					{
						Thing.bUsed = true;
						++CoinCount;
						Emit(ERallyEventKind::Coin, CoinCount);
					}
					break;
				case ERallyThing::Checkpoint:
					if (!Thing.bUsed && Body.Pos.X >= Thing.X)
					{
						Thing.bUsed = true;
						LastCheckpoint = std::max(LastCheckpoint, Thing.X);
						Emit(ERallyEventKind::Checkpoint);
					}
					break;
				case ERallyThing::Bomb:
					if (!Thing.bUsed && Length(Sub(Body.Pos, {Thing.X, Thing.Y + 0.3})) < 1.4)
					{
						Thing.bUsed = true;
						FVec2 Away = Sub(Body.Pos, {Thing.X, Thing.Y - 0.6});
						Away = Mul(Away, 1.0 / std::max(0.1, Length(Away)));
						const FVec2 Kick = Mul(Away, BombKick);
						Body.Vel = Add(Body.Vel, Kick);
						Body.Spin += Away.X >= 0.0 ? 2.5 : -2.5;
						for (FRallyBody& Wheel : Wheels)
						{
							Wheel.Vel = Add(Wheel.Vel, Kick);
						}
						Emit(ERallyEventKind::Bomb);
					}
					break;
				case ERallyThing::Spring:
					if (SpringCooldown <= 0.0)
					{
						for (int Index = 0; Index < 2; ++Index)
						{
							if (bGrounded[Index] && std::abs(Wheels[Index].Pos.X - Thing.X) < 0.8)
							{
								SpringCooldown = 0.6;
								Body.Vel.Y = std::max(Body.Vel.Y, SpringLaunch);
								for (FRallyBody& Wheel : Wheels)
								{
									Wheel.Vel.Y = std::max(Wheel.Vel.Y, SpringLaunch);
								}
								Emit(ERallyEventKind::Spring);
								break;
							}
						}
					}
					break;
				case ERallyThing::Boost:
				{
					bool bOn = false;
					for (int Index = 0; Index < 2; ++Index)
					{
						bOn = bOn || (bGrounded[Index] && Wheels[Index].Pos.X >= Thing.X && Wheels[Index].Pos.X <= Thing.X + BoostLength);
					}
					if (bOn && !Thing.bUsed)
					{
						Emit(ERallyEventKind::Boost);
					}
					Thing.bUsed = bOn;
					if (bOn && Dot(Body.Vel, AxisX) < BoostTop)
					{
						const FVec2 Push = Mul(AxisX, BoostAccel * RallyStep);
						Body.Vel = Add(Body.Vel, Push);
						for (FRallyBody& Wheel : Wheels)
						{
							Wheel.Vel = Add(Wheel.Vel, Push);
						}
					}
					break;
				}
				}
			}
		}

		void FArkRally::Crash()
		{
			++CrashCount;
			CrashTimer = RallyCrashPause;
			PendingFlips = 0;   // a flip that ends in a crash pays nothing
			bNitroOn = false;
			// Come back on the newest safe spot from at least 1.5 s before the crash (further back for each quick repeat),
			// with most of the speed it had there; the last checkpoint when that is further on.
			CrashStreak = Clock - LastCrashTime < 6.0 ? CrashStreak + 1 : 0;
			LastCrashTime = Clock;
			const double Before = Clock - 1.5 - 1.5 * CrashStreak;
			const double PreviousX = CrashStreak > 0 ? RespawnX : 1e9;   // a quick repeat never comes back at the same spot
			RespawnX = LastCheckpoint;
			RespawnSpeed = 0.0;
			for (size_t Index = SafeSpots.size(); Index-- > 0;)
			{
				if (SafeSpots[Index].Time <= Before && SafeSpots[Index].X < PreviousX - 1.0)
				{
					if (SafeSpots[Index].X > LastCheckpoint)
					{
						RespawnX = SafeSpots[Index].X;
						RespawnSpeed = std::clamp(SafeSpots[Index].Speed * 0.85, 0.0, 14.0);
					}
					SafeSpots.resize(Index + 1);
					break;
				}
			}
			NextSafeSpot = Clock + RallyCrashPause + 1.0;
			Emit(ERallyEventKind::Crash, CrashCount);
		}

		// ============================================================ the profile side
		std::string FinishRally(FProfile& Profile, const FArkRally& Game)
		{
			if (Game.IsCustom())
			{
				return std::string("Custom track (") + RallyVehicleName(Game.Vehicle()) + ") " + (Game.IsWon() ? "finished in " + Seconds(Game.Time()) + " s, place "
					+ std::to_string(Game.Place()) + " of 4" : "not finished") + ", " + std::to_string(Game.Coins()) + " coins, " + std::to_string(Game.Flips()) + " flips, "
					+ std::to_string(Game.Crashes()) + " crashes. Custom tracks pay no reward.";
			}
			return std::string(RallyVehicleName(Game.Vehicle())) + ", " + std::to_string(Game.Flips()) + " flips: "
				+ FinishMini(Profile, EMiniGame::ArkRally, Game.GetDifficulty(), Game.IsWon(), Game.Score(), Game.Stars());
		}
	}
}
