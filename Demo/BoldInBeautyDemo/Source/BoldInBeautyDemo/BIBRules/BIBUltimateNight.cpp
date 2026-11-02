// Boldinbeauty demo — Ultimate Custom Night, the world, the scheduler and the account side (see BIBUltimateNight.h).
#include "BIBUltimateNight.h"

#include "BIBCore.h"
#include "BIBMetaOps.h"

#include <algorithm>
#include <cmath>

namespace BIB
{
	namespace Arcade
	{
		namespace Ucn
		{
			namespace
			{
				// id, name, AI per mode {Easy, Medium, Hard, Angel}, arrival.
				// Easy: 001-005 at AI 15. Medium: 001-010 at AI 15. Hard: 000 and 001-015 at AI 10. Angel: 000-020 at AI 20.
				// Owner 2026-10-14: in every mode Plack's 5 AM sacrifice summons Death (019) at that mode's AI level.
				const FSubjectInfo Table[SubjectCount] = {
					{0, "Angel", {0, 0, 10, 20}, EArrival::Start},
					{1, "Luna", {15, 15, 10, 20}, EArrival::Start},
					{2, "Plack", {15, 15, 10, 20}, EArrival::Start},
					{3, "Kuu", {15, 15, 10, 20}, EArrival::Start},
					{4, "Ishii", {15, 15, 10, 20}, EArrival::Start},
					{5, "Arita", {15, 15, 10, 20}, EArrival::Start},
					{6, "Fujita", {0, 15, 10, 20}, EArrival::Start},
					{7, "Shiraishi", {0, 15, 10, 20}, EArrival::Start},
					{8, "Myrcia", {0, 15, 10, 20}, EArrival::Start},
					{9, "Alchemist", {0, 15, 10, 20}, EArrival::Start},
					{10, "Blood", {0, 15, 10, 20}, EArrival::Start},
					{11, "Stargazer", {0, 0, 10, 20}, EArrival::Start},
					{12, "Gambler", {0, 0, 10, 20}, EArrival::Start},
					{13, "Weather", {0, 0, 10, 20}, EArrival::Start},
					{14, "Judge", {0, 0, 10, 20}, EArrival::Start},
					{15, "Narrator / Diano", {0, 0, 10, 20}, EArrival::Start},
					{16, "Jester", {0, 0, 0, 20}, EArrival::Start},
					{17, "Media", {0, 0, 0, 20}, EArrival::Start},
					{18, "Time Keeper", {0, 0, 0, 20}, EArrival::Start},
					{19, "Death", {15, 15, 10, 20}, EArrival::PlackSacrifice},
					{20, "Adapt", {0, 0, 0, 20}, EArrival::Start},
				};
			}

			const char* ModeName(EMode Mode)
			{
				static const char* Names[ModeCount] = {"Easy", "Medium", "Hard", "Angel"};
				const int Index = static_cast<int>(Mode);
				return Index >= 0 && Index < ModeCount ? Names[Index] : "?";
			}

			const FSubjectInfo& SubjectInfo(int Id)
			{
				return Table[std::clamp(Id, 0, SubjectCount - 1)];
			}

			int AiLevel(int Id, EMode Mode)
			{
				const int M = static_cast<int>(Mode);
				if (Id < 0 || Id >= SubjectCount || M < 0 || M >= ModeCount)
				{
					return 0;
				}
				return Table[Id].Ai[M];
			}

			bool InMode(int Id, EMode Mode)
			{
				return AiLevel(Id, Mode) > 0;
			}

			std::vector<int> Roster(EMode Mode)
			{
				std::vector<int> Ids;
				for (int Id = 0; Id < SubjectCount; ++Id)
				{
					if (InMode(Id, Mode))
					{
						Ids.push_back(Id);
					}
				}
				return Ids;
			}

			FMovement MovementFor(int Ai)
			{
				const int Level = std::clamp(Ai, 0, 20);
				FMovement Result;
				Result.IntervalSeconds = 9.0 - 0.2 * Level;   // AI 10 = 7 s, 15 = 6 s, 20 = 5 s
				Result.Chance = static_cast<double>(Level) / 20.0;
				return Result;
			}

			// ================================================================================================ the world
			FUltimateNight::FUltimateNight()
			{
				for (double& Value : LastShockNeed)
				{
					Value = -100.0;
				}
			}

			FUltimateNight::~FUltimateNight() = default;

			void FUltimateNight::Start(EMode InMode, uint32_t Seed, const FUcnTuning& InTuning)
			{
				StartWith(InMode, Seed, Roster(InMode), InTuning);
			}

			void FUltimateNight::StartWith(EMode InMode, uint32_t Seed, const std::vector<int>& Subjects, const FUcnTuning& InTuning)
			{
				Modules.clear();
				DutyList.clear();
				Events.clear();
				for (FDoorState& Door : Doors)
				{
					Door = FDoorState();
				}
				bWindowLocked = false;
				BloodPhase = 0;
				WeatherLook = -1;
				WeatherAuraColour = 0;
				Mode = InMode;
				T = InTuning;
				RngState.seed(Seed);
				Clock = 0.0;
				PowerLeft = 100.0;
				bPowerOut = false;
				PowerOutLeft = 0.0;
				bCameraUp = false;
				Camera = 0;
				for (int Cam = 0; Cam < CameraCount; ++Cam)
				{
					Cams[Cam] = FCamState();
					Cams[Cam].Shocks = T.ShocksPerCamera;
					LastShockNeed[Cam] = -100.0;
				}
				Pulls = 0;
				bAdOpen = false;
				VaccineCam = -1;
				bVaccineArmed = false;
				Minis = 0;
				HornUntil = -1.0;
				bOver = false;
				bWon = false;
				KilledBy = -1;
				Reason.clear();
				LastHour = 0;
				StargazerHolds = 0;
				HourOffset = 0.0;
				bAngelWaiting = false;
				LastDoorThreat[0] = LastDoorThreat[1] = LastDoorThreat[2] = -100.0;
				NextDutyId = 1;
				StatsData = FUcnStats();
				bJudgeMark = false;
				bJudgeOn = false;
				bForcedMove = false;
				for (const int Id : Subjects)
				{
					if (Id >= 0 && Id < SubjectCount && !HasSubject(Id))
					{
						Modules.push_back(MakeSubject(Id, false));
					}
				}
				bJudgeOn = HasSubject(JudgeId);
			}

			int FUltimateNight::Hour() const
			{
				return std::clamp(static_cast<int>(NightTime() / NightHourSeconds), 0, NightHours);
			}

			int FUltimateNight::Usage() const
			{
				if (bPowerOut)
				{
					return 0;
				}
				return 1 + (Doors[0].bClosed ? 1 : 0) + (Doors[1].bClosed ? 1 : 0) + (bCameraUp ? 1 : 0) + (Doors[WindowSide].bClosed && T.bWindowUsesPower ? 1 : 0);
			}

			int FUltimateNight::ShocksTotal() const
			{
				int Sum = 0;
				for (int Cam = 0; Cam < CameraCount; ++Cam)
				{
					Sum += Cams[Cam].bDisabled ? 0 : Cams[Cam].Shocks;
				}
				return Sum;
			}

			int FUltimateNight::CamerasWorking() const
			{
				int Count = 0;
				for (int Cam = 0; Cam < CameraCount; ++Cam)
				{
					Count += Cams[Cam].bDisabled ? 0 : 1;
				}
				return Count;
			}

			const FDuty* FUltimateNight::FindDuty(int DutyId) const
			{
				for (const FDuty& Duty : DutyList)
				{
					if (Duty.Id == DutyId && Duty.bOpen)
					{
						return &Duty;
					}
				}
				return nullptr;
			}

			FDuty* FUltimateNight::MutableDuty(int DutyId)
			{
				for (FDuty& Duty : DutyList)
				{
					if (Duty.Id == DutyId && Duty.bOpen)
					{
						return &Duty;
					}
				}
				return nullptr;
			}

			std::vector<FEvent> FUltimateNight::PopEvents()
			{
				std::vector<FEvent> Out;
				Out.swap(Events);
				return Out;
			}

			void FUltimateNight::Emit(EEvent Kind, int A, int B, int C, int D)
			{
				FEvent Event;
				Event.Kind = Kind;
				Event.A = A;
				Event.B = B;
				Event.C = C;
				Event.D = D;
				Event.Time = Clock;
				Events.push_back(Event);
			}

			double FUltimateNight::Uniform(double Lo, double Hi)
			{
				if (Hi <= Lo)
				{
					return Lo;
				}
				return std::uniform_real_distribution<double>(Lo, Hi)(RngState);
			}

			int FUltimateNight::RandInt(int Lo, int Hi)
			{
				if (Hi <= Lo)
				{
					return Lo;
				}
				return std::uniform_int_distribution<int>(Lo, Hi)(RngState);
			}

			bool FUltimateNight::Roll(double Chance)
			{
				return std::uniform_real_distribution<double>(0.0, 1.0)(RngState) < Chance;
			}

			bool FUltimateNight::HasSubject(int Id) const
			{
				return Module(Id) != nullptr;
			}

			const FSubject* FUltimateNight::Module(int Id) const
			{
				for (const std::unique_ptr<FSubject>& Subject : Modules)
				{
					if (Subject->Id == Id)
					{
						return Subject.get();
					}
				}
				return nullptr;
			}

			FSubject* FUltimateNight::MutableModule(int Id)
			{
				for (std::unique_ptr<FSubject>& Subject : Modules)
				{
					if (Subject->Id == Id)
					{
						return Subject.get();
					}
				}
				return nullptr;
			}

			std::vector<int> FUltimateNight::SubjectsOnCamera(int Cam) const
			{
				std::vector<int> Seen;
				for (const std::unique_ptr<FSubject>& Subject : Modules)
				{
					Subject->CollectOnCamera(Cam, Seen);
				}
				return Seen;
			}

			// ---------------------------------------------------------------------------------------------- judge
			bool FUltimateNight::JudgeActive() const
			{
				return bJudgeOn;
			}

			bool FUltimateNight::IsDoorFalseAlarm(ESide Side) const
			{
				if (!bJudgeOn)
				{
					return false;
				}
				const int S = SideIdx(Side);
				for (const FDuty& Duty : DutyList)
				{
					// A character is there: one the door must stop, or Kuu winding up her punch at that doorway.
					if (Duty.bOpen && (Duty.Kind == EDuty::CloseDoor || Duty.Kind == EDuty::OpenDoor) && Duty.Side == S)
					{
						return false;
					}
				}
				return Clock - LastDoorThreat[S] > T.JudgeGrace;
			}

			bool FUltimateNight::IsWindowFalseAlarm() const
			{
				if (!bJudgeOn)
				{
					return false;
				}
				for (const FDuty& Duty : DutyList)
				{
					if (Duty.bOpen && Duty.Kind == EDuty::CloseDoor && Duty.Side == WindowSide)
					{
						return false;
					}
				}
				// Someone at the window (Blood) or right outside it on CAM 3 counts as "a character is there".
				if (BloodPhase > 0 || !SubjectsOnCamera(WindowCamera).empty())
				{
					return false;
				}
				return Clock - LastDoorThreat[WindowSide] > T.JudgeGrace;
			}

			bool FUltimateNight::IsShockFalseAlarm() const
			{
				if (!bJudgeOn || !bCameraUp)
				{
					return false;
				}
				for (const std::unique_ptr<FSubject>& Subject : Modules)
				{
					if (Subject->NeedsShockAt(Camera))
					{
						return false;
					}
				}
				return Clock - LastShockNeed[Camera] > T.JudgeGrace;
			}

			// --------------------------------------------------------------------------------------------- scheduler
			double FUltimateNight::WorkFor(EDuty Kind, int Clicks) const
			{
				switch (Kind)
				{
				case EDuty::CloseDoor:
				case EDuty::OpenDoor:
				case EDuty::PullCamera:
					return T.ReactionBudget + T.ActionSeconds;
				case EDuty::ClickOffice:
					return T.ReactionBudget + T.ActionSeconds + std::max(1, Clicks) * T.ClickSeconds;
				default:
					break;
				}
				return T.ReactionBudget + T.ActionSeconds;
			}

			bool FUltimateNight::DutySatisfied(const FDuty& Duty) const
			{
				switch (Duty.Kind)
				{
				case EDuty::CloseDoor: return Doors[Duty.Side].bClosed;
				case EDuty::OpenDoor: return !Doors[Duty.Side].bClosed;
				default: break;
				}
				return false;
			}

			std::vector<FUltimateNight::FJob> FUltimateNight::CollectJobs(const FDuty* Extra, int ExtraId) const
			{
				std::vector<FJob> Jobs;
				auto Add = [&Jobs, this](const FDuty& Duty)
				{
					if (!Duty.bLethal || DutySatisfied(Duty))
					{
						return;
					}
					int Key = 0;
					switch (Duty.Kind)
					{
					case EDuty::CloseDoor: Key = 100 + Duty.Side; break;       // one shut answers every duty on that side
					case EDuty::FlashCamera: return;                           // exclusive: handled by the Stargazer reservation
					default: Key = 1000 + Duty.Id; break;
					}
					const double Work = WorkFor(Duty.Kind, Duty.Clicks);
					for (FJob& Job : Jobs)
					{
						if (Job.Key == Key)
						{
							Job.Deadline = std::min(Job.Deadline, Duty.Deadline);
							return;
						}
					}
					FJob Job;
					Job.Deadline = Duty.Deadline;
					Job.Work = Work;
					Job.Key = Key;
					Jobs.push_back(Job);
				};
				for (const FDuty& Duty : DutyList)
				{
					if (Duty.bOpen && Duty.Id != ExtraId)
					{
						Add(Duty);
					}
				}
				if (Extra != nullptr)
				{
					Add(*Extra);
				}
				return Jobs;
			}

			bool FUltimateNight::Feasible(std::vector<FJob>& Jobs, double Now)
			{
				std::sort(Jobs.begin(), Jobs.end(), [](const FJob& A, const FJob& B) { return A.Deadline < B.Deadline; });
				double At = Now;
				for (const FJob& Job : Jobs)
				{
					At += Job.Work;
					if (At > Job.Deadline + 1e-9)
					{
						return false;
					}
				}
				return true;
			}

			bool FUltimateNight::IsDoorBlocked(int Side) const
			{
				if (Side == WindowSide)
				{
					return bWindowLocked;   // Blood's early phases: the window must not be shut
				}
				if (Doors[Side].RepairLeft > 0.0)
				{
					return true;
				}
				for (const FDuty& Duty : DutyList)
				{
					if (Duty.bOpen && Duty.Kind == EDuty::OpenDoor && Duty.Side == Side)
					{
						return true;   // a Kuu is charging there
					}
				}
				return false;
			}

			int FUltimateNight::KuuSideInUse() const
			{
				for (const FDuty& Duty : DutyList)
				{
					if (Duty.bOpen && Duty.Kind == EDuty::OpenDoor)
					{
						return Duty.Side;
					}
				}
				return -1;
			}

			bool FUltimateNight::HasOpenLethalOnSide(int Side) const
			{
				for (const FDuty& Duty : DutyList)
				{
					if (Duty.bOpen && Duty.Kind == EDuty::CloseDoor && Duty.Side == Side)
					{
						return true;
					}
				}
				return false;
			}

			bool FUltimateNight::CanOpenLethal(const FDutySpec& Spec) const
			{
				if (bOver)
				{
					return false;
				}
				if (StargazerReserved() && Spec.Subject != StargazerId)
				{
					return false;   // his flash window is exclusive: nobody else may ask for the player's hands
				}
				if (bAngelWaiting && Spec.Subject != AngelId && !(Spec.Subject == StargazerId && StargazerReserved()))
				{
					return false;   // a pull the player already made has priority over new attacks
				}
				if (Spec.Kind == EDuty::CloseDoor)
				{
					if (bPowerOut || IsDoorBlocked(Spec.Side))
					{
						return false;   // a door that cannot close must never be the answer to a lethal threat
					}
				}
				FDuty Extra;
				Extra.Id = -1;
				Extra.Subject = Spec.Subject;
				Extra.Kind = Spec.Kind;
				Extra.Side = Spec.Side;
				Extra.Cam = Spec.Cam;
				Extra.Clicks = Spec.Clicks;
				Extra.Opened = Clock;
				Extra.Deadline = Clock + Spec.Window;
				Extra.Window = Spec.Window;
				Extra.bLethal = Spec.bLethal;
				Extra.bOpen = true;
				std::vector<FJob> Jobs = CollectJobs(&Extra, -1);
				return Feasible(Jobs, Clock);
			}

			bool FUltimateNight::FeasibleWith(EDuty Kind, int Clicks, double Window, int Side) const
			{
				FDuty Extra;
				Extra.Id = -1;
				Extra.Kind = Kind;
				Extra.Side = Side;
				Extra.Clicks = Clicks;
				Extra.Deadline = Clock + Window;
				Extra.Window = Window;
				Extra.bLethal = true;
				Extra.bOpen = true;
				std::vector<FJob> Jobs = CollectJobs(&Extra, -1);
				return Feasible(Jobs, Clock);
			}

			double FUltimateNight::NextLethalDeadline() const
			{
				double Best = 1e9;
				for (const FDuty& Duty : DutyList)
				{
					if (Duty.bOpen && Duty.bLethal && !DutySatisfied(Duty))
					{
						Best = std::min(Best, Duty.Deadline - Clock);
					}
				}
				return Best;
			}

			int FUltimateNight::OpenDuty(const FDutySpec& Spec)
			{
				if (bOver)
				{
					return -1;
				}
				if (Spec.bLethal && !CanOpenLethal(Spec))
				{
					++StatsData.Deferrals;
					return -1;
				}
				FDuty Duty;
				Duty.Id = NextDutyId++;
				Duty.Subject = Spec.Subject;
				Duty.Kind = Spec.Kind;
				Duty.Side = Spec.Side;
				Duty.Cam = Spec.Cam;
				Duty.Clicks = Spec.Clicks;
				Duty.Opened = Clock;
				Duty.Deadline = Clock + Spec.Window;
				Duty.Window = Spec.Window;
				Duty.bLethal = Spec.bLethal;
				Duty.bLocated = Spec.bLocated;
				Duty.bOpen = true;
				DutyList.push_back(Duty);
				if (Spec.bLethal)
				{
					++StatsData.LethalOpened;
					StatsData.MinLethalWindow = std::min(StatsData.MinLethalWindow, Spec.Window);
				}
				if (Spec.Kind == EDuty::CloseDoor)
				{
					NoteDoorClosed(Spec.Side);
				}
				if (Spec.Kind == EDuty::ShockInfection || Spec.Kind == EDuty::ShockGambler || Spec.Kind == EDuty::FlashCamera)
				{
					NoteShockNeed(Spec.Cam);
				}
				Emit(EEvent::Telegraph, Spec.Subject, static_cast<int>(Spec.Kind), Spec.Kind == EDuty::CloseDoor ? Spec.Side : Spec.Cam,
					static_cast<int>(std::min(Spec.Window, 1.0e6) * 1000.0));   // ms; "no deadline" duties are clamped, never overflow
				return Duty.Id;
			}

			void FUltimateNight::NoteDoorClosed(int Side)
			{
				LastDoorThreat[Side] = Clock;
			}

			void FUltimateNight::CloseDuty(int DutyId)
			{
				for (FDuty& Duty : DutyList)
				{
					if (Duty.Id == DutyId && Duty.bOpen)
					{
						Duty.bOpen = false;
						if (Duty.Kind == EDuty::CloseDoor)
						{
							NoteDoorClosed(Duty.Side);   // the Judge's grace starts when the threat ends
						}
						else if (Duty.Kind == EDuty::ShockInfection || Duty.Kind == EDuty::ShockGambler || Duty.Kind == EDuty::FlashCamera)
						{
							NoteShockNeed(Duty.Cam);
						}
						return;
					}
				}
			}

			// ------------------------------------------------------------------------------------------ player tools
			bool FUltimateNight::SetDoor(ESide Side, bool bClosed)
			{
				return SetBarrier(SideIdx(Side), bClosed);
			}

			bool FUltimateNight::SetWindow(bool bClosed)
			{
				return SetBarrier(WindowSide, bClosed);
			}

			bool FUltimateNight::SetBarrier(int S, bool bClosed)
			{
				if (bOver || bPowerOut || S < 0 || S > WindowSide)
				{
					return false;
				}
				FDoorState& Door = Doors[S];
				if (bClosed && Door.RepairLeft > 0.0)
				{
					return false;
				}
				if (Door.bClosed == bClosed)
				{
					return false;
				}
				const bool bMisplay = bClosed && !bForcedMove && (S == WindowSide ? IsWindowFalseAlarm() : IsDoorFalseAlarm(S == 0 ? ESide::Left : ESide::Right));
				Door.bClosed = bClosed;
				if (S == WindowSide)
				{
					Emit(EEvent::Window, bClosed ? 1 : 0);
					if (bClosed)
					{
						for (size_t Index = 0; Index < Modules.size() && !bOver; ++Index)
						{
							Modules[Index]->OnWindowShut(*this);   // Blood: shut too early and she gets you
						}
					}
				}
				if (bMisplay && !bOver)
				{
					++StatsData.JudgeKills;
					Emit(EEvent::Judge, S == WindowSide ? 2 : 0, S);
					Kill(JudgeId, S == WindowSide ? "Judge: the window shut with nothing there - false alarm." : "Judge: a door shut with nothing there - false alarm.", -1, true);
				}
				return true;
			}

			bool FUltimateNight::SetCamera(bool bUp)
			{
				if (bOver || bPowerOut || bCameraUp == bUp)
				{
					return false;
				}
				bCameraUp = bUp;
				if (bUp)
				{
					if (!bForcedMove)
					{
						++Pulls;
					}
					for (const std::unique_ptr<FSubject>& Subject : Modules)
					{
						Subject->OnCameraUp(*this, bForcedMove);
					}
				}
				else
				{
					for (const std::unique_ptr<FSubject>& Subject : Modules)
					{
						Subject->OnCameraDown(*this, bForcedMove);
					}
				}
				return true;
			}

			void FUltimateNight::SelectCamera(int Cam)
			{
				if (!bOver && !bAdOpen)
				{
					Camera = std::clamp(Cam, 0, CameraCount - 1);
				}
			}

			bool FUltimateNight::Shock()
			{
				if (bOver || bPowerOut || !bCameraUp || bAdOpen)
				{
					return false;
				}
				FCamState& Cam = Cams[Camera];
				if (Cam.bDisabled || Cam.Shocks <= 0)
				{
					return false;
				}
				--Cam.Shocks;
				++StatsData.ShocksSpent;
				bool bNeeded = false;
				for (const std::unique_ptr<FSubject>& Subject : Modules)
				{
					if (Subject->NeedsShockAt(Camera) && Subject->OnShock(*this, Camera))
					{
						bNeeded = true;
					}
				}
				if (!bNeeded && bJudgeOn && Clock - LastShockNeed[Camera] > T.JudgeGrace)
				{
					++StatsData.JudgeKills;
					Emit(EEvent::Judge, 1, Camera);
					Kill(JudgeId, "Judge: a shock with nothing to shock - false alarm.", -1, true);
				}
				return true;
			}

			bool FUltimateNight::ClickVaccine()
			{
				if (bOver || bPowerOut || !bCameraUp || bAdOpen || VaccineCam < 0 || Camera != VaccineCam || Cams[Camera].bDisabled)
				{
					return false;
				}
				bVaccineArmed = true;
				Emit(EEvent::Vaccine, VaccineCam, 0);
				VaccineCam = -1;
				return true;
			}

			bool FUltimateNight::ClickOffice(int DutyId)
			{
				FDuty* Duty = MutableDuty(DutyId);
				if (bOver || Duty == nullptr || Duty->Kind != EDuty::ClickOffice)
				{
					return false;
				}
				for (const std::unique_ptr<FSubject>& Subject : Modules)
				{
					Subject->OnClick(*this, DutyId);
				}
				return true;
			}

			bool FUltimateNight::CloseAd()
			{
				if (bOver || !bAdOpen)
				{
					return false;
				}
				SetAdOpen(false);
				return true;
			}

			void FUltimateNight::SetAdOpen(bool bOpen)
			{
				if (bAdOpen == bOpen)
				{
					return;
				}
				bAdOpen = bOpen;
				Emit(EEvent::Ad, bOpen ? 1 : 0);
			}

			// ------------------------------------------------------------------------------------ module services
			void FUltimateNight::Kill(int Subject, const std::string& Why, int DutyId, bool bPacing)
			{
				if (bOver)
				{
					return;
				}
				if (!bPacing)
				{
					const FDuty* Duty = nullptr;
					for (const FDuty& Candidate : DutyList)
					{
						if (Candidate.Id == DutyId)
						{
							Duty = &Candidate;
							break;
						}
					}
					if (Duty == nullptr || !Duty->bLethal)
					{
						++StatsData.UntelegraphedKills;
					}
					else if (Duty->Window + 1e-9 < WorkFor(Duty->Kind, 1))
					{
						++StatsData.ShortWindowKills;
					}
				}
				bOver = true;
				bWon = false;
				KilledBy = Subject;
				Reason = Why;
				bCameraUp = false;
				Emit(EEvent::Jumpscare, Subject);
			}

			void FUltimateNight::ForceDoor(ESide Side, bool bClosed)
			{
				bForcedMove = true;
				if (SetDoor(Side, bClosed))
				{
					Emit(EEvent::DoorForced, SideIdx(Side), bClosed ? 1 : 0);
				}
				bForcedMove = false;
			}

			void FUltimateNight::ForceCamera(bool bUp)
			{
				bForcedMove = true;
				if (SetCamera(bUp))
				{
					Emit(EEvent::CameraForced, bUp ? 1 : 0);
				}
				bForcedMove = false;
			}

			void FUltimateNight::BreakDoor(ESide Side)
			{
				FDoorState& Door = Doors[SideIdx(Side)];
				Door.bClosed = false;
				Door.RepairLeft = T.DoorRepairSeconds;
				Emit(EEvent::DoorBroken, SideIdx(Side));
			}

			void FUltimateNight::SetCameraDisabled(int Cam)
			{
				if (InCameras(Cam) && !Cams[Cam].bDisabled)
				{
					Cams[Cam].bDisabled = true;
					Cams[Cam].InfectionLeft = 0.0;
					if (VaccineCam == Cam)
					{
						VaccineCam = -1;
					}
					++StatsData.CamerasLost;
					Emit(EEvent::CameraLost, Cam);
				}
			}

			bool FUltimateNight::ShocksAvailableForNonLethal() const
			{
				return ShocksTotal() > T.ShockReserve;
			}

			int FUltimateNight::PickCamera(bool bNeedShock, int Avoid)
			{
				// Bounded: a fixed scan, never a random retry loop.
				int Candidates[CameraCount];
				int Count = 0;
				for (int Cam = 0; Cam < CameraCount; ++Cam)
				{
					if (Cams[Cam].bDisabled || Cam == Avoid || (bNeedShock && Cams[Cam].Shocks <= 0))
					{
						continue;
					}
					Candidates[Count++] = Cam;
				}
				if (Count == 0)
				{
					return -1;
				}
				return Candidates[RandInt(0, Count - 1)];
			}

			void FUltimateNight::SendBackAnHour()
			{
				HourOffset = std::min(Clock, HourOffset + NightHourSeconds);   // the night's clock goes back; real time does not
				++StatsData.HoursLost;
				LastHour = Hour();
				Emit(EEvent::HourLost, LastHour);
			}

			bool FUltimateNight::InfectionBlockedByVaccine()
			{
				if (!bVaccineArmed)
				{
					return false;
				}
				bVaccineArmed = false;
				++StatsData.VaccinesTaken;
				Emit(EEvent::Vaccine, -1, 1);
				return true;
			}

			// ----------------------------------------------------------------------------------------------- the night
			void FUltimateNight::Tick(double DeltaSeconds)
			{
				double Left = std::max(0.0, DeltaSeconds);
				while (Left > 0.0 && !bOver)
				{
					const double Dt = std::min(0.05, Left);
					Step(Dt);
					Left -= Dt;
				}
			}

			void FUltimateNight::Step(double Dt)
			{
				Clock += Dt;
				const int H = Hour();
				if (H != LastHour)
				{
					LastHour = H;
					Emit(EEvent::Hour, H);
					if (H >= NightHours)
					{
						bOver = true;
						bWon = true;
						bCameraUp = false;
						Emit(EEvent::Survived);
						return;
					}
				}

				// Power: Night Shift's FNAF-style drain (each shut door and the raised camera add a bar).
				if (!bPowerOut)
				{
					PowerLeft -= PowerDrainPerSecond * T.PowerDrainScale * Usage() * Dt;
					StatsData.MinPower = std::min(StatsData.MinPower, PowerLeft);
					if (PowerLeft <= 0.0)
					{
						PowerLeft = 0.0;
						bPowerOut = true;
						Doors[0].bClosed = Doors[1].bClosed = Doors[WindowSide].bClosed = false;
						bCameraUp = false;
						PowerOutLeft = T.PowerOutDeath;
						Emit(EEvent::PowerOut);
					}
				}
				else
				{
					PowerOutLeft -= Dt;
					if (PowerOutLeft <= 0.0)
					{
						Kill(-1, "The power ran out and the dark found you.", -1, true);
						return;
					}
				}

				for (int Side = 0; Side < WindowSide; ++Side)
				{
					if (Doors[Side].RepairLeft > 0.0)
					{
						Doors[Side].RepairLeft -= Dt;
						if (Doors[Side].RepairLeft <= 0.0)
						{
							Doors[Side].RepairLeft = 0.0;
							Emit(EEvent::DoorRepaired, Side);
						}
					}
				}

				bJudgeMark = false;
				// Index loop: Adapt may add or drop hosted copies, never top-level modules, but keep the iteration stable.
				for (size_t Index = 0; Index < Modules.size() && !bOver; ++Index)
				{
					Modules[Index]->Update(*this, Dt);
				}
				bJudgeOn = bJudgeMark;

				// Housekeeping: drop closed duties (keeps the list short for the bots and the scheduler).
				if (DutyList.size() > 24)
				{
					DutyList.erase(std::remove_if(DutyList.begin(), DutyList.end(), [](const FDuty& Duty) { return !Duty.bOpen; }), DutyList.end());
				}
			}

			// ===================================================================================== account side
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

				int ModeIdx(EMode Mode)
				{
					return std::clamp(static_cast<int>(Mode), 0, ModeCount - 1);
				}
			}

			bool IsUnlocked(const FProfile& Profile)
			{
				return Profile.Meta.ArcadeRecords.bUcnUnlocked || Profile.StagesCleared >= TotalCampaignLevels;
			}

			bool IsRewardClaimed(const FProfile& Profile)
			{
				return Profile.Meta.ArcadeRecords.bUcnRewardClaimed;
			}

			FActionResult StartUcn(FProfile& Profile, EMode Mode)
			{
				if (!IsUnlocked(Profile))
				{
					return Fail("Ultimate Custom Night unlocks when you clear Chapter 13.");
				}
				if (!Profile.Spend(ECurrency::ArcadeToken, TokenCost))
				{
					return Fail("You need an arcade token (daily top-up, login bonus, quests).");
				}
				FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
				Records.bUcnUnlocked = true;   // latched: the unlock survives whatever happens to the campaign counters
				++Records.UcnPlays[ModeIdx(Mode)];
				return Ok(std::string("Ultimate Custom Night - ") + ModeName(Mode) + " started (1 token).");
			}

			std::string FinishUcn(FProfile& Profile, EMode Mode, bool bWon, int HoursSurvived)
			{
				FArcadeRecords& Records = Profile.Meta.ArcadeRecords;
				const int M = ModeIdx(Mode);
				const int Hours = bWon ? NightHours : std::clamp(HoursSurvived, 0, NightHours);
				Records.UcnBestHour[M] = std::max(Records.UcnBestHour[M], Hours);
				std::string Text;
				if (bWon)
				{
					++Records.UcnWins[M];
					Text = std::string("6 AM - you survived Ultimate Custom Night (") + ModeName(Mode) + ")!";
					if (IsUnlocked(Profile) && !Records.bUcnRewardClaimed)
					{
						// Once per account, first win of any mode: flagged BEFORE paying so a second call can never pay twice.
						Records.bUcnUnlocked = true;
						Records.bUcnRewardClaimed = true;
						FReward Reward;
						Reward.Diamonds = RewardDiamonds;
						Text += " First win reward: " + Meta::ApplyReward(Profile, Reward);
					}
				}
				else
				{
					Text = std::string("You did not survive Ultimate Custom Night (") + ModeName(Mode) + ") - reached " + std::to_string(Hours == 0 ? 12 : Hours) + " AM.";
				}
				return Text;
			}

			std::string FinishUcn(FProfile& Profile, const FUltimateNight& Night)
			{
				const int Hours = Night.IsWon() ? NightHours : Night.Hour();
				std::string Text = FinishUcn(Profile, Night.GetMode(), Night.IsWon(), Hours);
				if (!Night.IsWon() && !Night.LostReason().empty())
				{
					Text = Night.LostReason() + " " + Text;
				}
				return Text;
			}
		}
	}
}
