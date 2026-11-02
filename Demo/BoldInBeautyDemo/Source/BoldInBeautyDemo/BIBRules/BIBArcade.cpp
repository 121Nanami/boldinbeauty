// Boldinbeauty demo — the arcade games (see BIBArcade.h). Engine-independent; every number not given by the owner is PLACEHOLDER.
#include "BIBArcade.h"

#include "BIBCampaign.h"
#include "BIBCore.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Arcade
	{
		namespace
		{
			FActionResult Ok(const std::string& Message)
			{
				FActionResult Result;
				Result.bOk = true;
				Result.Message = Message;
				return Result;
			}

			FActionResult Fail(const std::string& Message)
			{
				FActionResult Result;
				Result.Message = Message;
				return Result;
			}

			int DiffIndex(EDifficulty Difficulty)
			{
				return std::clamp(static_cast<int>(Difficulty), 0, DifficultyCount - 1);
			}
		}

		const char* DifficultyName(EDifficulty Difficulty)
		{
			static const char* Names[DifficultyCount] = {"Easy", "Medium", "Hard"};
			return Names[DiffIndex(Difficulty)];
		}

		const char* GameName(EGame Game)
		{
			switch (Game)
			{
			case EGame::NightShift: return "Night Shift";
			case EGame::BossRush: return "Boss Rush";
			case EGame::Rhythm: return "Rhythm";
			case EGame::HaloFall: return "Halo Fall";
			case EGame::NeonMemory: return "Neon Memory";
			case EGame::FirewallBreach: return "Firewall Breach";
			case EGame::ArkRally: return "Ark Rally";
			case EGame::Count: break;
			}
			return "?";
		}

		// ================================================================== Night Shift
		FNightRules NightRules(EDifficulty Difficulty)
		{
			FNightRules Rules;
			switch (Difficulty)
			{
			case EDifficulty::Easy:
				Rules = {7.0, 0.50, 2.0, 15.0, 6, 3};
				break;
			case EDifficulty::Medium:
				Rules = {6.0, 0.75, 1.5, 20.0, 6, 3};
				break;
			case EDifficulty::Hard:
			case EDifficulty::Count:
				Rules = {5.0, 1.00, 1.0, 25.0, 9, 3};
				break;
			}
			return Rules;
		}

		const char* NightCharName(ENightChar Char)
		{
			static const char* Names[NightCharCount] = {"Luna", "Plack", "Kuu", "Media", "Narrator"};
			const int Index = static_cast<int>(Char);
			return Index >= 0 && Index < NightCharCount ? Names[Index] : "?";
		}

		const char* NightCharPortrait(ENightChar Char)
		{
			static const char* Names[NightCharCount] = {"luna", "plack", "kuu", "media", "narrator"};
			const int Index = static_cast<int>(Char);
			return Index >= 0 && Index < NightCharCount ? Names[Index] : "";
		}

		const char* RoomName(ERoom Room)
		{
			static const char* Names[static_cast<int>(ERoom::Count)] = {"Stage", "Dining Hall", "Back Room", "Storage", "West Hall", "West Corner",
				"Kitchen", "Restrooms", "East Hall", "East Corner", "Left Door", "Right Door", "Office"};
			const int Index = static_cast<int>(Room);
			return Index >= 0 && Index < static_cast<int>(ERoom::Count) ? Names[Index] : "?";
		}

		void RoomMapPosition(ERoom Room, double& OutX, double& OutY)
		{
			// A small floor plan: the stage at the top, the office at the bottom between the two door spots.
			static const double Positions[static_cast<int>(ERoom::Count)][2] = {
				{0.50, 0.08}, // Stage
				{0.50, 0.30}, // Dining
				{0.14, 0.18}, // Backroom
				{0.12, 0.46}, // Storage
				{0.30, 0.56}, // WestHall
				{0.30, 0.78}, // WestCorner
				{0.88, 0.40}, // Kitchen
				{0.86, 0.16}, // Restrooms
				{0.70, 0.56}, // EastHall
				{0.70, 0.78}, // EastCorner
				{0.40, 0.92}, // LeftDoor
				{0.60, 0.92}, // RightDoor
				{0.50, 0.95}, // Inside
			};
			const int Index = std::clamp(static_cast<int>(Room), 0, static_cast<int>(ERoom::Count) - 1);
			OutX = Positions[Index][0];
			OutY = Positions[Index][1];
		}

		void FNightShift::Start(EDifficulty InDifficulty, uint32_t Seed)
		{
			*this = FNightShift();
			Difficulty = InDifficulty;
			R = NightRules(InDifficulty);
			Rng.seed(Seed);
			for (int Index = 0; Index < NightCharCount; ++Index)
			{
				// Staggered so the five never roll on the same frame (FNAF's opportunities are a few hundredths apart too).
				Chars[Index].NextOpportunity = NightGraceSeconds + R.OpportunitySeconds * (0.15 * Index);
			}
			Chars[CharIndex(ENightChar::Kuu)].Room = ERoom::Backroom;
		}

		int FNightShift::Hour() const
		{
			return std::min(NightHours, static_cast<int>(Clock / NightHourSeconds));
		}

		std::string FNightShift::ClockText() const
		{
			const int H = Hour();
			return (H == 0 ? std::string("12") : std::to_string(H)) + " AM";
		}

		int FNightShift::Usage() const
		{
			if (bPowerOut)
			{
				return 0;
			}
			int Use = 1;
			for (int Side = 0; Side < 2; ++Side)
			{
				Use += Doors[Side].bClosed ? 1 : 0;
				Use += Lights[Side] ? 1 : 0;
			}
			Use += bCameraUp ? 1 : 0;
			return Use;
		}

		bool FNightShift::SetDoor(ESide Side, bool bClosed)
		{
			if (bOver || bPowerOut)
			{
				return false;
			}
			FDoor& Door = Doors[SideIndex(Side)];
			if (bClosed && Door.RepairLeft > 0.0)
			{
				Emit(ENightEventKind::DoorBlocked, SideIndex(Side));
				return false;
			}
			Door.bClosed = bClosed;
			return true;
		}

		bool FNightShift::ToggleDoor(ESide Side)
		{
			return SetDoor(Side, !Doors[SideIndex(Side)].bClosed);
		}

		void FNightShift::SetLight(ESide Side, bool bOn)
		{
			Lights[SideIndex(Side)] = bOn && !bPowerOut && !bOver;
		}

		void FNightShift::SetCamera(bool bUp)
		{
			bCameraUp = bUp && !bPowerOut && !bOver;
		}

		void FNightShift::SelectCamera(int InCamera)
		{
			Camera = std::clamp(InCamera, 0, CameraCount - 1);
		}

		std::vector<ENightChar> FNightShift::VisibleInRoom(ERoom Room) const
		{
			std::vector<ENightChar> Seen;
			for (int Index = 0; Index < NightCharCount; ++Index)
			{
				if (Chars[Index].Room == Room)
				{
					Seen.push_back(static_cast<ENightChar>(Index));
				}
			}
			return Seen;
		}

		std::vector<ENightChar> FNightShift::AtDoor(ESide Side) const
		{
			return VisibleInRoom(DoorRoom(Side));
		}

		std::vector<ENightChar> FNightShift::Visible() const
		{
			std::vector<ENightChar> Seen;
			if (bCameraUp)
			{
				Seen = VisibleInRoom(static_cast<ERoom>(Camera));
			}
			for (int Side = 0; Side < 2; ++Side)
			{
				if (Lights[Side])
				{
					for (ENightChar Char : AtDoor(static_cast<ESide>(Side)))
					{
						Seen.push_back(Char);
					}
				}
			}
			return Seen;
		}

		bool FNightShift::IsKuuWindingUp(ESide Side) const
		{
			const FChar& Kuu = Chars[CharIndex(ENightChar::Kuu)];
			return KuuWindup > 0.0 && Kuu.Room == DoorRoom(Side);
		}

		bool FNightShift::IsMediaAtDoor(ESide Side) const
		{
			return bMediaAtDoor && Chars[CharIndex(ENightChar::Media)].Room == DoorRoom(Side);
		}

		bool FNightShift::IsNarratorWatched() const
		{
			return bCameraUp && static_cast<ERoom>(Camera) == Chars[CharIndex(ENightChar::Narrator)].Room;
		}

		std::vector<FNightEvent> FNightShift::PopEvents()
		{
			std::vector<FNightEvent> Out;
			Out.swap(Events);
			return Out;
		}

		void FNightShift::Emit(ENightEventKind Kind, int A, int B)
		{
			FNightEvent Event;
			Event.Kind = Kind;
			Event.A = A;
			Event.B = B;
			Event.Time = Clock;
			Events.push_back(Event);
		}

		void FNightShift::ForceRoom(ENightChar Char, ERoom Room)
		{
			MoveTo(Char, Room);
		}

		void FNightShift::ForceMediaTeleport(ESide Side)
		{
			FChar& Media = Chars[CharIndex(ENightChar::Media)];
			Media.Side = Side;
			MoveTo(ENightChar::Media, DoorRoom(Side));
			bMediaAtDoor = true;
			MediaTimer = R.MediaWindow;
			Media.Progress = 0;
			Emit(ENightEventKind::MediaArrived, SideIndex(Side));
		}

		bool FNightShift::Roll(double Chance)
		{
			return std::uniform_real_distribution<double>(0.0, 1.0)(Rng) < Chance;
		}

		ERoom FNightShift::Pick(const std::vector<std::pair<ERoom, double>>& Options)
		{
			double Total = 0.0;
			for (const auto& Option : Options)
			{
				Total += Option.second;
			}
			double Value = std::uniform_real_distribution<double>(0.0, Total)(Rng);
			for (const auto& Option : Options)
			{
				Value -= Option.second;
				if (Value <= 0.0)
				{
					return Option.first;
				}
			}
			return Options.back().first;
		}

		void FNightShift::MoveTo(ENightChar Char, ERoom Room)
		{
			FChar& C = Chars[CharIndex(Char)];
			if (C.Room != Room)
			{
				C.Room = Room;
				Emit(ENightEventKind::Moved, CharIndex(Char), static_cast<int>(Room));
			}
		}

		void FNightShift::Lose(ENightChar Char, const std::string& Why)
		{
			if (bOver)
			{
				return;
			}
			bOver = true;
			bWon = false;
			KilledBy = Char;
			Reason = Why;
			Chars[CharIndex(Char)].Room = ERoom::Inside;
			bCameraUp = false;
			Emit(ENightEventKind::Jumpscare, CharIndex(Char));
		}

		void FNightShift::Tick(double DeltaSeconds)
		{
			double Left = std::max(0.0, DeltaSeconds);
			while (Left > 0.0 && !bOver)
			{
				const double Dt = std::min(0.05, Left);
				Step(Dt);
				Left -= Dt;
			}
		}

		void FNightShift::Step(double Dt)
		{
			Clock += Dt;
			const int H = Hour();
			if (H != LastHour)
			{
				LastHour = H;
				Emit(ENightEventKind::HourChanged, H);
				if (H >= NightHours)
				{
					bOver = true;
					bWon = true;
					bCameraUp = false;
					Emit(ENightEventKind::Survived);
					return;
				}
			}

			// Power: FNAF-style, every closed door, lit light and the raised camera adds one usage bar.
			if (!bPowerOut)
			{
				PowerLeft -= PowerDrainPerSecond * Usage() * Dt;
				if (PowerLeft <= 0.0)
				{
					PowerLeft = 0.0;
					bPowerOut = true;
					Doors[0].bClosed = Doors[1].bClosed = false;
					Lights[0] = Lights[1] = false;
					bCameraUp = false;
					// Narrator comes for you a while after the lights go (unless 6 AM arrives first).
					PowerOutDeath = std::uniform_real_distribution<double>(5.0, 18.0)(Rng);
					Emit(ENightEventKind::PowerOut);
				}
			}
			else
			{
				PowerOutDeath -= Dt;
				if (PowerOutDeath <= 0.0)
				{
					Lose(ENightChar::Narrator, "The power ran out and Narrator found you in the dark.");
					return;
				}
			}

			for (int Side = 0; Side < 2; ++Side)
			{
				if (Doors[Side].RepairLeft > 0.0)
				{
					Doors[Side].RepairLeft -= Dt;
					if (Doors[Side].RepairLeft <= 0.0)
					{
						Doors[Side].RepairLeft = 0.0;
						Emit(ENightEventKind::DoorRepaired, Side);
					}
				}
			}

			// Media: close the door she appeared at before the window runs out.
			if (bMediaAtDoor)
			{
				FChar& Media = Chars[CharIndex(ENightChar::Media)];
				if (Doors[SideIndex(Media.Side)].bClosed)
				{
					bMediaAtDoor = false;
					Media.Progress = 0;
					Emit(ENightEventKind::MediaLeft, SideIndex(Media.Side));
					MoveTo(ENightChar::Media, ERoom::Stage);
				}
				else
				{
					MediaTimer -= Dt;
					if (MediaTimer <= 0.0)
					{
						Lose(ENightChar::Media, std::string("Media got in through the ") + (Media.Side == ESide::Left ? "left" : "right") + " door.");
						return;
					}
				}
			}

			// Kuu: a five-second wind-up, then the punch. A closed door breaks; an open doorway is a miss.
			FChar& Kuu = Chars[CharIndex(ENightChar::Kuu)];
			if (KuuWindup > 0.0)
			{
				KuuWindup -= Dt;
				if (KuuWindup <= 0.0)
				{
					KuuWindup = 0.0;
					FDoor& Door = Doors[SideIndex(Kuu.Side)];
					const bool bBroke = Door.bClosed;
					if (bBroke)
					{
						Door.bClosed = false;
						Door.RepairLeft = R.DoorRepairSeconds;
						// Balance (PLACEHOLDER): the punch also throws Luna or Plack back from that doorway, so a broken door is a
						// danger for the next seconds, not a certain loss. Narrator never leaves a door, so Kuu avoids hers.
						for (ENightChar Other : {ENightChar::Luna, ENightChar::Plack})
						{
							if (Chars[CharIndex(Other)].Room == DoorRoom(Kuu.Side))
							{
								MoveTo(Other, ERoom::Dining);
							}
						}
					}
					Emit(ENightEventKind::KuuPunch, SideIndex(Kuu.Side), bBroke ? 1 : 0);
					MoveTo(ENightChar::Kuu, ERoom::Backroom);
					KuuRest = R.OpportunitySeconds * 2.0;
				}
			}
			else if (KuuRest > 0.0)
			{
				KuuRest -= Dt;
			}

			if (Clock < NightGraceSeconds || bPowerOut)
			{
				return;
			}
			for (int Index2 = 0; Index2 < NightCharCount && !bOver; ++Index2)
			{
				FChar& C = Chars[Index2];
				if (Clock >= C.NextOpportunity)
				{
					C.NextOpportunity += R.OpportunitySeconds;
					++OpportunityCount;
					Opportunity(static_cast<ENightChar>(Index2));
				}
			}
		}

		void FNightShift::Opportunity(ENightChar Char)
		{
			FChar& C = Chars[CharIndex(Char)];
			switch (Char)
			{
			case ENightChar::Luna:
			case ENightChar::Plack:
			{
				const bool bLeft = Char == ENightChar::Luna;
				const ESide Side = bLeft ? ESide::Left : ESide::Right;
				if (!Roll(R.MoveChance))
				{
					return;
				}
				if (C.Room == DoorRoom(Side))
				{
					if (Doors[SideIndex(Side)].bClosed)
					{
						Emit(ENightEventKind::Knock, SideIndex(Side), CharIndex(Char));
						MoveTo(Char, ERoom::Dining);   // like Bonnie and Chica: back to the dining hall
					}
					else
					{
						Lose(Char, std::string(NightCharName(Char)) + " walked in through the " + (bLeft ? "left" : "right") + " door.");
					}
					return;
				}
				std::vector<std::pair<ERoom, double>> Next;
				switch (C.Room)
				{
				case ERoom::Stage: Next = {{ERoom::Dining, 1.0}}; break;
				case ERoom::Dining:
					Next = bLeft ? std::vector<std::pair<ERoom, double>>{{ERoom::WestHall, 0.45}, {ERoom::Storage, 0.35}, {ERoom::Backroom, 0.20}}
						: std::vector<std::pair<ERoom, double>>{{ERoom::EastHall, 0.45}, {ERoom::Kitchen, 0.35}, {ERoom::Restrooms, 0.20}};
					break;
				case ERoom::Storage: Next = {{ERoom::WestHall, 0.6}, {ERoom::Dining, 0.4}}; break;
				case ERoom::Backroom: Next = {{ERoom::WestHall, 0.5}, {ERoom::Dining, 0.5}}; break;
				case ERoom::Kitchen: Next = {{ERoom::EastHall, 0.6}, {ERoom::Dining, 0.4}}; break;
				case ERoom::Restrooms: Next = {{ERoom::EastHall, 0.5}, {ERoom::Dining, 0.5}}; break;
				case ERoom::WestHall: Next = {{ERoom::WestCorner, 0.6}, {ERoom::Storage, 0.4}}; break;
				case ERoom::EastHall: Next = {{ERoom::EastCorner, 0.6}, {ERoom::Kitchen, 0.4}}; break;
				case ERoom::WestCorner: Next = {{ERoom::LeftDoor, 0.75}, {ERoom::WestHall, 0.25}}; break;
				case ERoom::EastCorner: Next = {{ERoom::RightDoor, 0.75}, {ERoom::EastHall, 0.25}}; break;
				default: Next = {{ERoom::Dining, 1.0}}; break;
				}
				MoveTo(Char, Pick(Next));
				return;
			}
			case ENightChar::Narrator:
			{
				// Like Freddy: she cannot move while her camera is watched; leaving the monitor parked on her camera slows her.
				if (IsNarratorWatched())
				{
					return;
				}
				const bool bParked = !bCameraUp && static_cast<ERoom>(Camera) == C.Room;
				if (!Roll(R.MoveChance * (bParked ? 0.5 : 1.0)))
				{
					return;
				}
				if (C.Room == ERoom::RightDoor)
				{
					// She never leaves the door. The moment it is open, she is in.
					if (!Doors[1].bClosed)
					{
						Lose(Char, "Narrator was waiting at the right door.");
					}
					return;
				}
				if (++C.Progress < R.NarratorStepsPerRoom)
				{
					return;
				}
				C.Progress = 0;
				static const ERoom Path[] = {ERoom::Stage, ERoom::Dining, ERoom::Restrooms, ERoom::Kitchen, ERoom::EastHall, ERoom::EastCorner, ERoom::RightDoor};
				for (size_t Step = 0; Step + 1 < sizeof(Path) / sizeof(Path[0]); ++Step)
				{
					if (Path[Step] == C.Room)
					{
						MoveTo(Char, Path[Step + 1]);
						return;
					}
				}
				MoveTo(Char, ERoom::Dining);
				return;
			}
			case ENightChar::Kuu:
			{
				if (KuuWindup > 0.0 || KuuRest > 0.0 || !Roll(R.MoveChance))
				{
					return;
				}
				switch (C.Room)
				{
				case ERoom::Backroom:
					C.Side = Roll(0.5) ? ESide::Left : ESide::Right;
					if (Chars[CharIndex(ENightChar::Narrator)].Room == ERoom::RightDoor)
					{
						C.Side = ESide::Left;   // balance: she never punches the door Narrator waits at
					}
					MoveTo(Char, C.Side == ESide::Left ? ERoom::WestHall : ERoom::EastHall);
					break;
				case ERoom::WestHall:
				case ERoom::EastHall:
					MoveTo(Char, C.Side == ESide::Left ? ERoom::WestCorner : ERoom::EastCorner);
					break;
				case ERoom::WestCorner:
				case ERoom::EastCorner:
					MoveTo(Char, DoorRoom(C.Side));
					KuuWindup = KuuWindupSeconds;
					Emit(ENightEventKind::KuuWindup, SideIndex(C.Side));
					break;
				default:
					MoveTo(Char, ERoom::Backroom);
					break;
				}
				return;
			}
			case ENightChar::Media:
			{
				if (bMediaAtDoor || !Roll(R.MoveChance))
				{
					return;
				}
				if (++C.Progress >= R.MediaChargeToTeleport)
				{
					// She appears at a door that can close (balance: a broken door would be a certain loss).
					ESide Side = Roll(0.5) ? ESide::Left : ESide::Right;
					if (Doors[SideIndex(Side)].RepairLeft > 0.0)
					{
						Side = Side == ESide::Left ? ESide::Right : ESide::Left;
					}
					if (Doors[SideIndex(Side)].RepairLeft <= 0.0)
					{
						ForceMediaTeleport(Side);
					}
				}
				return;
			}
			case ENightChar::Count:
				break;
			}
		}

		FReward NightReward(EDifficulty Difficulty, bool bWon, int HoursSurvived)
		{
			FReward Reward;
			const int D = DiffIndex(Difficulty);
			if (bWon)
			{
				static const int Gold[DifficultyCount] = {3000, 6000, 10000};
				static const int Bytes[DifficultyCount] = {1000, 2000, 3500};
				static const int Hearts[DifficultyCount] = {4, 8, 15};
				static const int Diamonds[DifficultyCount] = {0, 20, 50};
				Reward.Gold = Gold[D];
				Reward.Bytes = Bytes[D];
				Reward.Hearts = Hearts[D];
				Reward.Diamonds = Diamonds[D];
			}
			else
			{
				Reward.Gold = std::clamp(HoursSurvived, 0, NightHours) * (200 + 150 * D);
			}
			return Reward;
		}

		// ======================================================================= Rhythm
		int SongCount()
		{
			return 3;
		}

		const FSong& Song(int Index)
		{
			// Placeholder tracks: the beat and the lane tones are synthesised (BIBSynth.h); the owner's music replaces them.
			static const FSong Songs[3] = {
				{"Track 1 - Steady", 100, 30, {261.63, 293.66, 329.63, 392.00, 440.00}},
				{"Track 2 - Pulse", 128, 34, {293.66, 349.23, 392.00, 440.00, 523.25}},
				{"Track 3 - Overdrive", 150, 40, {329.63, 392.00, 440.00, 523.25, 587.33}},
			};
			return Songs[std::clamp(Index, 0, 2)];
		}

		int LanesFor(EDifficulty Difficulty)
		{
			static const int Lanes[DifficultyCount] = {3, 4, 5};
			return Lanes[DiffIndex(Difficulty)];
		}

		double SongLength(int SongIndex)
		{
			const FSong& S = Song(SongIndex);
			return RhythmLeadIn + S.Bars * 4.0 * 60.0 / S.Bpm + 2.0;
		}

		std::vector<FNote> MakeChart(int SongIndex, EDifficulty Difficulty)
		{
			const FSong& S = Song(SongIndex);
			const int D = DiffIndex(Difficulty);
			const int Lanes = LanesFor(Difficulty);
			std::mt19937 Gen(static_cast<uint32_t>(1000 + SongIndex * 31 + D * 7));
			std::uniform_real_distribution<double> U(0.0, 1.0);
			const double Beat = 60.0 / S.Bpm;
			// Easy: quarter notes; Medium: eighths; Hard: eighths with sixteenth runs on every fourth bar. Rests thin them out.
			static const double RestChance[DifficultyCount] = {0.30, 0.45, 0.30};
			std::vector<FNote> Notes;
			int Lane = 0;
			for (int Bar = 0; Bar < S.Bars; ++Bar)
			{
				const bool bRun = D == 2 && Bar % 4 == 3;
				const int Steps = D == 0 ? 4 : (bRun ? 16 : 8);
				for (int Step = 0; Step < Steps; ++Step)
				{
					const bool bDownbeat = (Step * 4) % Steps == 0;
					if (Bar == 0 && Step == 0)
					{
						// always start on the first beat
					}
					else if (!bRun && U(Gen) < (bDownbeat ? RestChance[D] * 0.5 : RestChance[D]))
					{
						continue;
					}
					// Melodic walk: mostly neighbouring lanes, sometimes a jump.
					const double Move = U(Gen);
					if (Move < 0.35)
					{
						Lane = std::min(Lanes - 1, Lane + 1);
					}
					else if (Move < 0.70)
					{
						Lane = std::max(0, Lane - 1);
					}
					else if (Move < 0.85)
					{
						Lane = static_cast<int>(U(Gen) * Lanes) % Lanes;
					}
					FNote Note;
					Note.Time = RhythmLeadIn + (Bar * 4.0 + Step * 4.0 / Steps) * Beat;
					Note.Lane = Lane;
					Notes.push_back(Note);
				}
			}
			return Notes;
		}

		const char* JudgeName(EJudge Judge)
		{
			static const char* Names[4] = {"PERFECT", "GREAT", "GOOD", "MISS"};
			const int Index = static_cast<int>(Judge);
			return Index >= 0 && Index < 4 ? Names[Index] : "";
		}

		void FRhythm::Start(int InSong, EDifficulty InDifficulty)
		{
			*this = FRhythm();
			SongIdx = std::clamp(InSong, 0, SongCount() - 1);
			Difficulty = InDifficulty;
			LaneCount = LanesFor(InDifficulty);
			Chart = MakeChart(SongIdx, InDifficulty);
			State.assign(Chart.size(), 0);
			Length = SongLength(SongIdx);
		}

		int FRhythm::Multiplier() const
		{
			return std::min(4, 1 + Streak / 10);
		}

		double FRhythm::Accuracy() const
		{
			if (Chart.empty())
			{
				return 0.0;
			}
			const double Weighted = Counts[0] * 1.0 + Counts[1] * 0.7 + Counts[2] * 0.4;
			return Weighted / static_cast<double>(Chart.size());
		}

		int FRhythm::Stars() const
		{
			if (bFailed)
			{
				return 0;
			}
			const double A = Accuracy();
			return A >= 0.95 ? 5 : A >= 0.85 ? 4 : A >= 0.70 ? 3 : A >= 0.50 ? 2 : 1;
		}

		void FRhythm::Judge(EJudge Result)
		{
			++Counts[static_cast<int>(Result)];
			Last = Result;
			LastTime = Clock;
			if (Result == EJudge::Miss)
			{
				Streak = 0;
				Health -= 0.06;
			}
			else
			{
				static const int Points0[3] = {100, 70, 40};
				Points += static_cast<long long>(Points0[static_cast<int>(Result)]) * Multiplier();
				++Streak;
				BestStreak = std::max(BestStreak, Streak);
				Health += Result == EJudge::Perfect ? 0.03 : 0.02;
			}
			Health = std::clamp(Health, 0.0, 1.0);
			if (Health <= 0.0)
			{
				bOver = true;
				bFailed = true;
			}
		}

		void FRhythm::Tick(double DeltaSeconds)
		{
			if (bOver)
			{
				return;
			}
			Clock += std::max(0.0, DeltaSeconds);
			while (FirstPending < Chart.size() && (State[FirstPending] != 0 || Chart[FirstPending].Time < Clock - RhythmGood))
			{
				if (State[FirstPending] == 0)
				{
					State[FirstPending] = 2;
					Judge(EJudge::Miss);
					if (bOver)
					{
						return;
					}
				}
				++FirstPending;
			}
			if (Clock >= Length)
			{
				bOver = true;
			}
		}

		EJudge FRhythm::Press(int Lane)
		{
			if (bOver || Lane < 0 || Lane >= LaneCount)
			{
				return EJudge::Miss;
			}
			size_t Best = Chart.size();
			double BestGap = RhythmGood + 1.0;
			for (size_t Index = FirstPending; Index < Chart.size() && Chart[Index].Time <= Clock + RhythmGood; ++Index)
			{
				if (State[Index] != 0 || Chart[Index].Lane != Lane)
				{
					continue;
				}
				const double Gap = std::fabs(Chart[Index].Time - Clock);
				if (Gap <= RhythmGood && Gap < BestGap)
				{
					Best = Index;
					BestGap = Gap;
				}
			}
			if (Best == Chart.size())
			{
				// A strum with nothing to hit breaks the combo (no note is counted as missed).
				Streak = 0;
				Health = std::max(0.0, Health - 0.02);
				Last = EJudge::Miss;
				LastTime = Clock;
				if (Health <= 0.0)
				{
					bOver = true;
					bFailed = true;
				}
				return EJudge::Miss;
			}
			State[Best] = 1;
			const EJudge Result = BestGap <= RhythmPerfect ? EJudge::Perfect : (BestGap <= RhythmGreat ? EJudge::Great : EJudge::Good);
			Judge(Result);
			return Result;
		}

		FReward RhythmReward(EDifficulty Difficulty, bool bPassed, int Stars)
		{
			FReward Reward;
			if (!bPassed)
			{
				Reward.Gold = 300;
				return Reward;
			}
			const int D = DiffIndex(Difficulty);
			static const int Gold[DifficultyCount] = {1500, 3000, 5000};
			static const int Bytes[DifficultyCount] = {500, 1000, 1700};
			static const int Hearts[DifficultyCount] = {0, 5, 10};
			const double Share = 0.4 + 0.12 * std::clamp(Stars, 0, 5);   // 1 star 52%, 5 stars 100%
			Reward.Gold = static_cast<int>(Gold[D] * Share);
			Reward.Bytes = static_cast<int>(Bytes[D] * Share);
			Reward.Hearts = Stars >= 3 ? Hearts[D] : 0;
			Reward.Diamonds = Stars >= 5 ? 10 + 10 * D : 0;
			return Reward;
		}

		// ===================================================================== Boss Rush
		int BossRushStageId(int BaseStage, int Round)
		{
			return BossRushStageBase + std::clamp(BaseStage, 1, TotalCampaignLevels) * 1000 + std::clamp(Round, 1, BossRushMaxRound);
		}

		int BossRushBaseOf(int StageId)
		{
			return (StageId - BossRushStageBase) / 1000;
		}

		int BossRushRoundOf(int StageId)
		{
			return (StageId - BossRushStageBase) % 1000;
		}

		FSectionPlanData BossRushRound(int Round)
		{
			// Every round has a boss; escorts join as it goes on, and every fifth round brings two bosses.
			FSectionPlanData Plan;
			Plan.Bosses = Round % 5 == 0 ? 2 : 1;
			Plan.MiniBosses = Round >= 3 ? (Round % 2 == 0 ? 1 : 0) + (Round >= 8 ? 1 : 0) : 0;
			Plan.Brutes = Round >= 6 ? std::min(4, Round / 3) : 0;
			return Plan;
		}

		FEnemyScale BossRushScales(int StageId)
		{
			const int Base = std::clamp(BossRushBaseOf(StageId), 1, TotalCampaignLevels);
			const int Round = std::max(1, BossRushRoundOf(StageId));
			FEnemyScale Scale = Campaign::CampaignScales(Base);
			const double Growth = 1.0 + 0.12 * (Round - 1);
			Scale.Hp *= Growth;
			Scale.Damage *= 1.0 + 0.06 * (Round - 1);
			return Scale;
		}

		FReward BossRushRoundReward(int Round)
		{
			FReward Reward;
			Reward.Gold = 600 + 250 * Round;
			Reward.Bytes = 200 + 80 * Round;
			Reward.Hearts = 1 + Round / 2;
			Reward.Diamonds = Round % 5 == 0 ? 20 : 0;
			Reward.RegularTickets = Round % 10 == 0 ? 1 : 0;
			return Reward;
		}

		// =================================================================== profile side
		FActionResult StartGame(FProfile& Profile, EGame Game)
		{
			if (Game == EGame::BossRush && !Profile.TeamIsPlayable())
			{
				return Fail("Boss Rush uses your squad: fill it first (Team tab).");
			}
			if (!Profile.Spend(ECurrency::ArcadeToken, TokenCost))
			{
				return Fail("You need an arcade token (daily top-up, login bonus, quests).");
			}
			FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
			++Records.Plays[std::clamp(static_cast<int>(Game), 0, GameCount - 1)];
			Meta::NotifyQuest(Profile, Meta::EQuestEvent::ArcadePlay);
			return Ok(std::string(GameName(Game)) + " started (1 token).");
		}

		std::string FinishNight(FProfile& Profile, const FNightShift& Night)
		{
			FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
			const int D = DiffIndex(Night.GetDifficulty());
			const int Hours = Night.IsWon() ? NightHours : Night.Hour();
			Records.NightBestHour[D] = std::max(Records.NightBestHour[D], Hours);
			if (Night.IsWon())
			{
				++Records.NightWins[D];
			}
			const std::string Text = Meta::ApplyReward(Profile, NightReward(Night.GetDifficulty(), Night.IsWon(), Hours));
			return (Night.IsWon() ? std::string("6 AM - you survived the night (") + DifficultyName(Night.GetDifficulty()) + ")! "
				: Night.LostReason() + " ") + "Reward: " + Text;
		}

		std::string FinishRhythm(FProfile& Profile, const FRhythm& Song0)
		{
			FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
			const int Slot = std::clamp(Song0.SongIndex(), 0, SongCount() - 1) * DifficultyCount + DiffIndex(Song0.GetDifficulty());
			const bool bBest = Song0.Score() > Records.RhythmBest[Slot];
			Records.RhythmBest[Slot] = std::max(Records.RhythmBest[Slot], static_cast<int>(std::min<long long>(Song0.Score(), 2000000000LL)));
			Records.RhythmStars[Slot] = std::max(Records.RhythmStars[Slot], Song0.Stars());
			const std::string Text = Meta::ApplyReward(Profile, RhythmReward(Song0.GetDifficulty(), Song0.IsPassed(), Song0.Stars()));
			return std::string(Song0.IsPassed() ? "Song cleared" : "Song failed") + " - score " + Meta::Num(Song0.Score()) + ", " + std::to_string(Song0.Stars())
				+ " stars, max combo " + std::to_string(Song0.MaxCombo()) + (bBest ? " (new best)" : "") + ". Reward: " + Text;
		}

		std::vector<std::string> BossRushRoundCleared(FProfile& Profile, int Round)
		{
			std::vector<std::string> Lines;
			FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
			// Balancing pass 2026-10-09 (PLACEHOLDER): Gold, Bytes and hearts pay on every run; the Diamonds and Regular tickets only the
			// first time a round is reached (above the best). Paid every run, five free tokens a day to round 10 were about five
			// tickets and 200 Diamonds a day, several times the ~55 free pulls a month.
			FReward Reward = BossRushRoundReward(Round);
			if (Round <= Records.BossRushBest)
			{
				Reward.Diamonds = 0;
				Reward.RegularTickets = 0;
			}
			Lines.push_back("Boss Rush round " + std::to_string(Round) + " cleared! Reward: " + Meta::ApplyReward(Profile, Reward));
			if (Round > Records.BossRushBest)
			{
				Records.BossRushBest = Round;
				Lines.push_back("New best: round " + std::to_string(Round) + ".");
			}
			return Lines;
		}

		std::vector<std::string> BossRushOver(FProfile& Profile, int RoundReached)
		{
			std::vector<std::string> Lines;
			const int Cleared = std::max(0, RoundReached - 1);
			const FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
			Lines.push_back("Boss Rush over: " + std::to_string(Cleared) + " boss round" + (Cleared == 1 ? "" : "s") + " cleared. Best ever: round "
				+ std::to_string(Records.BossRushBest) + ".");
			Lines.push_back("Every cleared round already paid its reward (Diamonds and tickets only on a new best round).");
			return Lines;
		}
	}
}
