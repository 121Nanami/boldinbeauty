// Boldinbeauty demo — Ultimate Custom Night bots and the beatability suite (see BIBUcnBots.h).
#include "BIBUcnBots.h"

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
				// Stargazer's public route as camera indices (CAM labels 1-7-9-5-3-6-4-8, label n = index n - 1).
				const int StarOrder[8] = {0, 6, 8, 4, 2, 5, 3, 7};
			}

			const char* BotName(EBotKind Kind)
			{
				static const char* Names[4] = {"perfect", "budget", "human", "do-nothing"};
				const int Index = static_cast<int>(Kind);
				return Index >= 0 && Index < 4 ? Names[Index] : "?";
			}

			FBotProfile ProfileFor(EBotKind Kind, const FUcnTuning& Tuning)
			{
				FBotProfile P;
				switch (Kind)
				{
				case EBotKind::Perfect:
					break;
				case EBotKind::Budget:
					P.ReactionMean = Tuning.ReactionBudget;
					P.ActionGap = Tuning.ActionSeconds;
					break;
				case EBotKind::Human:
					P.ReactionMean = 0.35;     // owner: 0.35 s +- 0.1 s
					P.ReactionSigma = 0.10;
					P.ActionGap = std::max(0.2, Tuning.ActionSeconds);   // cannot do two actions in the same 0.2 s
					P.MissChance = 0.05;       // owner: 5% missed inputs
					P.bFullInfo = false;
					break;
				case EBotKind::DoNothing:
				case EBotKind::Count:
					P.bActs = false;
					break;
				}
				return P;
			}

			FBot::FBot(EBotKind InKind, uint32_t Seed, const FUcnTuning& Tuning)
				: Kind(InKind), Profile(ProfileFor(InKind, Tuning)), T(Tuning), RngState(Seed * 2654435761u + 0x9E3779B9u)
			{
				if (RngState == 0)
				{
					RngState = 0xA5A5A5A5u;
				}
			}

			double FBot::Uniform01()
			{
				RngState ^= RngState << 13;
				RngState ^= RngState >> 17;
				RngState ^= RngState << 5;
				return (static_cast<double>(RngState) + 0.5) / 4294967296.0;
			}

			double FBot::Sample(double Mean, double Sigma)
			{
				if (Sigma <= 0.0)
				{
					return Mean;
				}
				const double U1 = Uniform01();
				const double U2 = Uniform01();
				const double Z = std::sqrt(-2.0 * std::log(U1)) * std::cos(6.283185307179586 * U2);
				return std::max(0.1, Mean + Sigma * Z);
			}

			bool FBot::Noticed(FUltimateNight& W, const FDuty& Duty)
			{
				for (const auto& Entry : NoticeAt)
				{
					if (Entry.first == Duty.Id)
					{
						return W.Time() >= Entry.second;
					}
				}
				const double At = W.Time() + (Profile.ReactionMean > 0.0 ? Sample(Profile.ReactionMean, Profile.ReactionSigma) : 0.0);
				NoticeAt.emplace_back(Duty.Id, At);
				if (NoticeAt.size() > 64)
				{
					NoticeAt.erase(NoticeAt.begin(), NoticeAt.begin() + 32);
				}
				return W.Time() >= At;
			}

			int FBot::KnownCam(const FUltimateNight& W, const FDuty& Duty) const
			{
				(void)W;
				if (Profile.bFullInfo || Duty.bLocated)
				{
					return Duty.Cam;
				}
				for (const auto& Entry : Seen)
				{
					// Only while it is still where it was seen: Gambler's heads moves her, and the player has to find her again.
					if (Entry.first == Duty.Id && Entry.second == Duty.Cam)
					{
						return Entry.second;
					}
				}
				return -1;
			}

			// Shiraishi: must the camera be kept on her? She needs (steps left) x 10 s at the fastest (cameras down); if that
			// is still longer than what is left of the night (plus a margin) she cannot arrive and nothing needs watching.
			bool FBot::WatchNeeded(const FUltimateNight& W) const
			{
				const FSubject* Shiraishi = W.Module(ShiraishiId);
				if (Shiraishi == nullptr)
				{
					return false;
				}
				const double Steps = Shiraishi->StepsToDoor();
				if (Steps <= 0.0)
				{
					return false;
				}
				const double NightLeft = NightHours * NightHourSeconds - W.NightTime();
				return Steps * 10.0 < NightLeft + 6.0;
			}

			void FBot::Act(FUltimateNight& W)
			{
				if (!Profile.bActs || W.IsOver())
				{
					return;
				}
				for (const FEvent& Event : W.PopEvents())
				{
					if (Event.Kind == EEvent::Stargazer)
					{
						if (Event.A == 0)
						{
							bStarWarn = true;
							StarStop = 0;
						}
						else if (Event.A == 1)
						{
							++StarStop;
							// He is on a new camera: the player reacts to seeing him there (the stated 0.5 s stop).
							StarSeenAt = Event.Time + (Profile.ReactionMean > 0.0 ? Sample(Profile.ReactionMean, Profile.ReactionSigma) : 0.0);
						}
						else if (Event.A == 3 || Event.A == 2)
						{
							bStarWarn = false;
							StarStop = 0;
						}
					}
				}
				const int MaxActions = Kind == EBotKind::Perfect ? 64 : 1;
				for (int Index = 0; Index < MaxActions; ++Index)
				{
					if (W.Time() + 1e-9 < NextAct || W.IsOver())
					{
						return;
					}
					const FAct Act = Decide(W);
					if (Act.Kind == FAct::None)
					{
						return;
					}
					if (!Execute(W, Act))
					{
						return;
					}
				}
			}

			bool FBot::Execute(FUltimateNight& W, const FAct& Act)
			{
				const bool bMiss = Profile.MissChance > 0.0 && Uniform01() < Profile.MissChance;
				const double Gap = Act.Kind == FAct::Click && Profile.ActionGap > 0.0 ? std::max(0.2, T.ClickSeconds) : Profile.ActionGap;
				NextAct = W.Time() + Gap;
				++Taken;
				if (bMiss)
				{
					return true;   // the input did nothing; the bot will see that and try again
				}
				switch (Act.Kind)
				{
				case FAct::Door: return W.SetDoor(Act.A == 0 ? ESide::Left : ESide::Right, Act.B != 0);
				case FAct::Camera: return W.SetCamera(Act.A != 0);
				case FAct::Select:
					if (W.SelectedCamera() == Act.A)
					{
						return false;
					}
					W.SelectCamera(Act.A);
					return true;
				case FAct::Shock: return W.Shock();
				case FAct::Vaccine: return W.ClickVaccine();
				case FAct::Click: return W.ClickOffice(Act.A);
				case FAct::Ad: return W.CloseAd();
				case FAct::Window: return W.SetWindow(Act.B != 0);
				case FAct::None: break;
				}
				return false;
			}

			FBot::FAct FBot::Decide(FUltimateNight& W)
			{
				FAct Out;
				const std::vector<FDuty>& All = W.Duties();
				const bool bViewing = W.IsCameraUp() && !W.IsAdOpen();
				if (bViewing)
				{
					// What is on the screen right now becomes known (the human finds things by looking).
					for (const FDuty& Duty : All)
					{
						if (Duty.bOpen && Duty.Cam == W.SelectedCamera() && !Duty.bLocated)
						{
							bool bKnown = false;
							for (const auto& Entry : Seen)
							{
								bKnown = bKnown || Entry.first == Duty.Id;
							}
							if (!bKnown)
							{
								Seen.emplace_back(Duty.Id, Duty.Cam);
								if (Seen.size() > 64)
								{
									Seen.erase(Seen.begin(), Seen.begin() + 32);
								}
							}
						}
					}
				}

				// ---- 1. lethal duties, earliest deadline first
				std::vector<const FDuty*> Lethal;
				const FDuty* Flash = nullptr;
				for (const FDuty& Duty : All)
				{
					if (!Duty.bOpen || !Duty.bLethal)
					{
						continue;
					}
					if (Duty.Kind == EDuty::FlashCamera)
					{
						Flash = &Duty;
					}
					else
					{
						Lethal.push_back(&Duty);
					}
				}
				std::sort(Lethal.begin(), Lethal.end(), [](const FDuty* A, const FDuty* B) { return A->Deadline < B->Deadline; });
				for (const FDuty* Duty : Lethal)
				{
					if (!Noticed(W, *Duty))
					{
						continue;
					}
					switch (Duty->Kind)
					{
					case EDuty::CloseDoor:
					{
						if (Duty->Side == WindowSide)
						{
							if (!W.IsWindowClosed())
							{
								Out.Kind = FAct::Window;
								Out.B = 1;
								return Out;
							}
							break;
						}
						const ESide Side = Duty->Side == 0 ? ESide::Left : ESide::Right;
						if (!W.IsDoorClosed(Side) && W.DoorRepairLeft(Side) <= 0.0)
						{
							Out.Kind = FAct::Door;
							Out.A = Duty->Side;
							Out.B = 1;
							return Out;
						}
						break;
					}
					case EDuty::ClickOffice:
					{
						if (Duty->Subject == NarratorId)
						{
							// The lethal third camera: clear the cheapest of the two in the room.
							const FDuty* Cheapest = nullptr;
							for (const FDuty& Mini : All)
							{
								if (Mini.bOpen && !Mini.bLethal && Mini.Subject == NarratorId && Mini.Kind == EDuty::ClickOffice
									&& (Cheapest == nullptr || Mini.Clicks < Cheapest->Clicks))
								{
									Cheapest = &Mini;
								}
							}
							if (Cheapest != nullptr)
							{
								Out.Kind = FAct::Click;
								Out.A = Cheapest->Id;
								return Out;
							}
						}
						else
						{
							Out.Kind = FAct::Click;
							Out.A = Duty->Id;
							return Out;
						}
						break;
					}
					case EDuty::PullCamera:
						if (!W.IsCameraUp())
						{
							Out.Kind = FAct::Camera;
							Out.A = 1;
							return Out;
						}
						break;
					default:
						break;
					}
				}

				// ---- 2. Stargazer: park on his next stop, flash him when he is on the camera we are watching. The perfect bot
				// (zero reaction) does not need to park: it answers his first stop from wherever it is.
				const bool bPark = bStarWarn && Profile.ReactionMean > 0.0;
				if (Flash != nullptr || bPark)
				{
					if (W.IsAdOpen())
					{
						Out.Kind = FAct::Ad;
						return Out;
					}
					if (!W.IsCameraUp())
					{
						Out.Kind = FAct::Camera;
						Out.A = 1;
						return Out;
					}
					if (Flash != nullptr && Flash->Cam == W.SelectedCamera() && W.ShocksLeft(Flash->Cam) > 0 && !W.IsCameraDisabled(Flash->Cam))
					{
						if (W.Time() + 1e-9 >= StarSeenAt)
						{
							Out.Kind = FAct::Shock;
						}
						return Out;   // he is on the watched camera: hold still until the flash is fired
					}
					int Target = -1;
					// The camera he is on right now, if a flash can still work there (the only chance when later stops have no charge).
					if (Flash != nullptr && !W.IsCameraDisabled(Flash->Cam) && W.ShocksLeft(Flash->Cam) > 0 && W.Time() + 1e-9 >= StarSeenAt)
					{
						Target = Flash->Cam;
					}
					for (int Stop = Flash != nullptr ? StarStop : 0; Target < 0 && Stop < 8; ++Stop)
					{
						const int Cam = StarOrder[Stop];
						if (!W.IsCameraDisabled(Cam) && W.ShocksLeft(Cam) > 0)
						{
							Target = Cam;
							break;
						}
					}
					if (Target >= 0 && W.SelectedCamera() != Target)
					{
						Out.Kind = FAct::Select;
						Out.A = Target;
						return Out;
					}
					return Out;   // parked: wait for him
				}

				// ---- 3. an advertisement over the monitor
				if (W.IsAdOpen())
				{
					for (const FDuty& Duty : All)
					{
						if (Duty.bOpen && Duty.Kind == EDuty::CloseAd && Noticed(W, Duty))
						{
							Out.Kind = FAct::Ad;
							return Out;
						}
					}
				}

				// ---- 4. Kuu's punch breaks a shut door: open it; and the mini cameras
				const FDuty* MiniPick = nullptr;
				for (const FDuty& Duty : All)
				{
					if (!Duty.bOpen)
					{
						continue;
					}
					if (Duty.Kind == EDuty::OpenDoor && Noticed(W, Duty))
					{
						const ESide Side = Duty.Side == 0 ? ESide::Left : ESide::Right;
						if (W.IsDoorClosed(Side))
						{
							Out.Kind = FAct::Door;
							Out.A = Duty.Side;
							Out.B = 0;
							return Out;
						}
					}
					if (Duty.Kind == EDuty::ClickOffice && !Duty.bLethal && Duty.Subject == NarratorId
						&& (MiniPick == nullptr || Duty.Clicks < MiniPick->Clicks))
					{
						MiniPick = &Duty;
					}
				}
				if (MiniPick != nullptr && Noticed(W, *MiniPick))
				{
					Out.Kind = FAct::Click;
					Out.A = MiniPick->Id;
					return Out;
				}

				// ---- 5. doors nobody needs shut any more
				for (int Side = 0; Side < 2; ++Side)
				{
					const ESide S = Side == 0 ? ESide::Left : ESide::Right;
					if (!W.IsDoorClosed(S))
					{
						continue;
					}
					bool bNeeded = false;
					for (const FDuty& Duty : All)
					{
						bNeeded = bNeeded || (Duty.bOpen && Duty.Kind == EDuty::CloseDoor && Duty.Side == Side);
					}
					if (!bNeeded)
					{
						Out.Kind = FAct::Door;
						Out.A = Side;
						Out.B = 0;
						return Out;
					}
				}

				// ---- 5b. the front window when nothing needs it shut (Blood's early phases need it OPEN)
				if (W.IsWindowClosed())
				{
					bool bNeeded = false;
					for (const FDuty& Duty : All)
					{
						bNeeded = bNeeded || (Duty.bOpen && Duty.Kind == EDuty::CloseDoor && Duty.Side == WindowSide);
					}
					if (!bNeeded)
					{
						Out.Kind = FAct::Window;
						Out.B = 0;
						return Out;
					}
				}

				// ---- 6. the camera system
				const FDuty* Task = nullptr;
				bool bUnknownTask = false;
				for (const FDuty& Duty : All)
				{
					if (!Duty.bOpen)
					{
						continue;
					}
					const bool bCamTask = Duty.Kind == EDuty::ShockInfection || Duty.Kind == EDuty::ShockGambler
						|| Duty.Kind == EDuty::FindTimeKeeper || Duty.Kind == EDuty::TakeVaccine;
					if (!bCamTask)
					{
						continue;
					}
					if (KnownCam(W, Duty) < 0)
					{
						bUnknownTask = true;
						continue;
					}
					if (Duty.Kind == EDuty::ShockInfection || Duty.Kind == EDuty::ShockGambler)
					{
						if (W.ShocksLeft(Duty.Cam) <= 0 || W.IsCameraDisabled(Duty.Cam))
						{
							continue;
						}
					}
					if (Task == nullptr || Duty.Deadline < Task->Deadline)
					{
						Task = &Duty;
					}
				}
				const bool bWatch = WatchNeeded(W);
				const bool bCamSubjects = W.HasSubject(IshiiId) || W.HasSubject(GamblerId) || W.HasSubject(TimeKeeperId) || W.HasSubject(AlchemistId);
				if (!Profile.bFullInfo && bCamSubjects && SweepLeft == 0 && W.Time() - LastSweep >= Profile.PatrolEvery)
				{
					SweepLeft = CameraCount;
				}
				if (!Profile.bFullInfo && bUnknownTask && SweepLeft == 0 && W.Time() - LastSweep >= 1.0)
				{
					SweepLeft = CameraCount;
				}
				const bool bNeedCamera = Task != nullptr || bWatch || SweepLeft > 0;
				if (!bNeedCamera)
				{
					if (W.IsCameraUp())
					{
						Out.Kind = FAct::Camera;
						Out.A = 0;
						return Out;
					}
					return Out;
				}
				if (W.IsAdOpen())
				{
					Out.Kind = FAct::Ad;
					return Out;
				}
				if (!W.IsCameraUp())
				{
					Out.Kind = FAct::Camera;
					Out.A = 1;
					return Out;
				}
				if (Task != nullptr && Noticed(W, *Task))
				{
					const int Cam = KnownCam(W, *Task);
					if (W.SelectedCamera() != Cam)
					{
						Out.Kind = FAct::Select;
						Out.A = Cam;
						return Out;
					}
					if (Task->Kind == EDuty::ShockInfection || Task->Kind == EDuty::ShockGambler)
					{
						Out.Kind = FAct::Shock;
						return Out;
					}
					if (Task->Kind == EDuty::TakeVaccine)
					{
						Out.Kind = FAct::Vaccine;
						return Out;
					}
					// FindTimeKeeper: looking at her camera is finding her; nothing more to do.
				}
				if (SweepLeft > 0)
				{
					int Guard = 0;
					while (Guard < CameraCount && W.IsCameraDisabled(SweepNext % CameraCount))
					{
						++SweepNext;
						++Guard;
					}
					Out.Kind = FAct::Select;
					Out.A = SweepNext % CameraCount;
					++SweepNext;
					--SweepLeft;
					if (SweepLeft == 0)
					{
						LastSweep = W.Time();
					}
					return Out;
				}
				if (bWatch && Task == nullptr)
				{
					for (const FDuty& Duty : All)
					{
						if (Duty.bOpen && Duty.Kind == EDuty::WatchShiraishi && W.SelectedCamera() != Duty.Cam)
						{
							Out.Kind = FAct::Select;
							Out.A = Duty.Cam;
							return Out;
						}
					}
				}
				return Out;
			}

			// ============================================================================================ the suite
			FNightResult RunNight(EMode Mode, uint32_t Seed, EBotKind BotKind, const FUcnTuning& Tuning, double MaxSeconds)
			{
				FUltimateNight Night;
				Night.Start(Mode, Seed, Tuning);
				FBot Bot(BotKind, Seed, Tuning);
				constexpr double Dt = 0.05;
				double Elapsed = 0.0;
				while (!Night.IsOver() && Elapsed < MaxSeconds)
				{
					Bot.Act(Night);
					Night.Tick(Dt);
					Elapsed += Dt;
				}
				FNightResult Result;
				Result.Seed = Seed;
				Result.bWon = Night.IsOver() && Night.IsWon();
				Result.bTimedOut = !Night.IsOver();
				Result.Seconds = Elapsed;
				Result.Hour = Night.Hour();
				Result.Killer = Night.Killer();
				Result.Reason = Result.bTimedOut ? std::string("timed out") : Night.LostReason();
				Result.Stats = Night.Stats();
				Result.ShocksLeft = Night.ShocksTotal();
				Result.CamerasWorking = Night.CamerasWorking();
				Result.PowerLeft = Night.Power();
				Result.Actions = Bot.ActionsTaken();
				return Result;
			}

			FSuiteResult RunSuite(EMode Mode, EBotKind BotKind, int Seeds, uint32_t FirstSeed, const FUcnTuning& Tuning)
			{
				FSuiteResult Suite;
				Suite.Mode = Mode;
				Suite.Bot = BotKind;
				double TotalSeconds = 0.0;
				for (int Index = 0; Index < Seeds; ++Index)
				{
					const uint32_t Seed = FirstSeed + static_cast<uint32_t>(Index);
					const FNightResult Night = RunNight(Mode, Seed, BotKind, Tuning);
					++Suite.Nights;
					TotalSeconds += Night.Seconds;
					Suite.UntelegraphedKills += Night.Stats.UntelegraphedKills;
					Suite.ShortWindowKills += Night.Stats.ShortWindowKills;
					Suite.MinLethalWindow = std::min(Suite.MinLethalWindow, Night.Stats.MinLethalWindow);
					Suite.MinPower = std::min(Suite.MinPower, Night.Stats.MinPower);
					Suite.Deferrals += Night.Stats.Deferrals;
					Suite.GaveUp += Night.Stats.GaveUp;
					Suite.HoursLost += Night.Stats.HoursLost;
					if (Night.bWon)
					{
						++Suite.Wins;
						Suite.MinShocksLeft = std::min(Suite.MinShocksLeft, Night.ShocksLeft);
						Suite.MinCamerasWorking = std::min(Suite.MinCamerasWorking, Night.CamerasWorking);
					}
					else
					{
						const int Slot = Night.Killer >= 0 && Night.Killer < SubjectCount ? Night.Killer : SubjectCount;
						++Suite.Killers[Slot];
						if (Suite.LossSeeds.size() < 8)
						{
							Suite.LossSeeds.push_back(Seed);
							Suite.LossReasons.push_back(Night.Reason + " (" + std::to_string(Night.Hour) + " AM)");
						}
					}
				}
				Suite.AverageSeconds = Suite.Nights > 0 ? TotalSeconds / Suite.Nights : 0.0;
				return Suite;
			}
		}
	}
}
