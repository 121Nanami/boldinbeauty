// Boldinbeauty demo — Ultimate Custom Night: one behaviour module per subject, 000-020 (see BIBUltimateNight.h).
// The owner's text is implemented as written; every gap is a PLACEHOLDER named in FUcnTuning or marked OPEN here.
// Lethal threats are only ever opened through FUltimateNight::OpenDuty, the central scheduler.
#include "BIBUltimateNight.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace BIB
{
	namespace Arcade
	{
		namespace Ucn
		{
			namespace
			{
				constexpr double Never = 1e9;

				int AiOf(const FUltimateNight& W, const FSubject& Self)
				{
					return Self.bAdapted ? W.AiFor(AdaptId) : W.AiFor(Self.Id);
				}

				const char* NameOf(int Id)
				{
					return SubjectInfo(Id).Name;
				}

				// Owner: Luna "goes in a straight line across 5 cameras to your left door", Plack the same to the right door; owner
				// 2026-10-14: CAM 3 is outside the front window and you can see Luna or Plack there as they come over. Night Shift's
				// rooms are reused (CAM 3 = Night Shift's Backroom); the other cameras of each line are a PLACEHOLDER, never a step back.
				const ERoom LeftRoute[RouteCameras] = {ERoom::Stage, ERoom::Dining, ERoom::Backroom, ERoom::WestHall, ERoom::WestCorner};
				const ERoom RightRoute[RouteCameras] = {ERoom::Stage, ERoom::Dining, ERoom::Backroom, ERoom::EastHall, ERoom::EastCorner};

				ERoom WalkNext(FUltimateNight& W, ERoom Room, bool bLeft)
				{
					(void)W;
					const ERoom* Route = bLeft ? LeftRoute : RightRoute;
					for (int Index = 0; Index < RouteCameras - 1; ++Index)
					{
						if (Route[Index] == Room)
						{
							return Route[Index + 1];
						}
					}
					if (Room == Route[RouteCameras - 1])
					{
						return FUltimateNight::DoorRoom(bLeft ? 0 : 1);
					}
					return Route[0];
				}

				bool IsCameraRoom(ERoom Room)
				{
					return static_cast<int>(Room) >= 0 && static_cast<int>(Room) < CameraCount;
				}

				// ============================================================================ 001 Luna / 002 Plack / 005 Arita
				// Walks a route to a door. A door arrival is a scheduled lethal duty with at least DoorWindowMin of warning; at the
				// next movement opportunity a shut door sends her away and an open door kills.
				class FWalker : public FSubject
				{
				public:
					FWalker(int InId, bool bInAdapted, bool bInLeft, bool bInSteady, bool bInPickSide)
						: FSubject(InId, bInAdapted), bLeft(bInLeft), bSteady(bInSteady), bPickSide(bInPickSide)
					{
					}

					void Update(FUltimateNight& W, double Dt) override
					{
						(void)Dt;
						const FMovement M = MovementFor(AiOf(W, *this));
						const double Now = W.Time();
						if (NextOpp < 0.0)
						{
							NextOpp = (bAdapted ? Now : std::max(Now, NightGraceSeconds)) + M.IntervalSeconds * (0.15 + 0.07 * Id);
						}
						if (!bAdapted && Now < NightGraceSeconds)
						{
							return;
						}
						if (bAtDoor)
						{
							AtDoor(W, M, Now);
							return;
						}
						if (bDisabled || bRetired)
						{
							return;
						}
						if (bWantDoor)
						{
							TryArrive(W, Now);
							return;
						}
						if (Now >= NextOpp)
						{
							NextOpp = std::max(NextOpp + M.IntervalSeconds, Now + 0.01);
							if (!bSteady && !W.Roll(M.Chance))
							{
								return;
							}
							Advance(W, Now);
						}
					}

					void Disable() override { bDisabled = true; }
					bool CycleDone() const override { return bArrivedOnce && !bAtDoor && !bWantDoor; }
					bool IsIdle() const override { return !bAtDoor && !bWantDoor; }
					bool IsOnCamera(int Cam) const override { return !bAtDoor && !bDisabled && IsCameraRoom(Room) && static_cast<int>(Room) == Cam; }

				protected:
					ESide SideNow() const { return bLeft ? ESide::Left : ESide::Right; }

					void MoveTo(FUltimateNight& W, ERoom To)
					{
						if (Room != To)
						{
							Room = To;
							W.Emit(EEvent::Moved, Id, static_cast<int>(To));
						}
					}

					void Advance(FUltimateNight& W, double Now)
					{
						if (Room == ERoom::Stage && bPickSide)
						{
							bLeft = W.Roll(0.5);   // Arita: "either Plack's or Luna's path"
						}
						const ERoom Next = WalkNext(W, Room, bLeft);
						if (Next == FUltimateNight::DoorRoom(bLeft ? 0 : 1))
						{
							bWantDoor = true;
							WantSince = Now;
							TryArrive(W, Now);
							return;
						}
						MoveTo(W, Next);
					}

					void TryArrive(FUltimateNight& W, double Now)
					{
						FDutySpec Spec;
						Spec.Subject = Id;
						Spec.Kind = EDuty::CloseDoor;
						Spec.Side = bLeft ? 0 : 1;
						// Owner: "close the door and on the next movement opportunity she goes away" - the strike is the next
						// opportunity, one movement interval away (never shorter than DoorWindowMin).
						Spec.Window = std::max(W.Tuning().DoorWindowMin, MovementFor(AiOf(W, *this)).IntervalSeconds);
						Spec.bLethal = true;
						const int Duty = W.OpenDuty(Spec);
						if (Duty >= 0)
						{
							DutyId = Duty;
							bWantDoor = false;
							bAtDoor = true;
							bArrivedOnce = true;
							Room = FUltimateNight::DoorRoom(Spec.Side);
							W.Emit(EEvent::Moved, Id, static_cast<int>(Room));
							NextOpp = Now + Spec.Window;   // the first strike is never sooner than the window
						}
						else if (Now - WantSince > W.Tuning().DeferCap)
						{
							bWantDoor = false;   // bounded: give up this approach, try again at a later opportunity
							W.NoteGaveUp(Id);
						}
					}

					void AtDoor(FUltimateNight& W, const FMovement& M, double Now)
					{
						if (Now < NextOpp)
						{
							return;
						}
						NextOpp = Now + M.IntervalSeconds;
						const double Chance = bSteady ? 1.0 : M.Chance;
						if (!W.Roll(Chance))
						{
							if (FDuty* Duty = W.MutableDuty(DutyId))
							{
								Duty->Deadline = NextOpp;   // she stays; the next strike is a full interval away
							}
							return;
						}
						const int S = bLeft ? 0 : 1;
						if (W.IsDoorClosed(bLeft ? ESide::Left : ESide::Right))
						{
							W.Emit(EEvent::Knock, Id, S);
							W.Emit(EEvent::Resolved, Id, static_cast<int>(EDuty::CloseDoor));
							W.CloseDuty(DutyId);
							DutyId = -1;
							bAtDoor = false;
							Room = ERoom::Stage;   // "she goes away": back to the start of her straight line (PLACEHOLDER: where to)
							W.Emit(EEvent::Moved, Id, static_cast<int>(Room));
						}
						else
						{
							W.Kill(Id, std::string(NameOf(Id)) + " walked in through the " + (bLeft ? "left" : "right") + " door.", DutyId);
						}
					}

					bool bLeft;
					bool bSteady;       // Arita: she just walks, no roll
					bool bPickSide;
					bool bDisabled = false;
					bool bArrivedOnce = false;
					bool bAtDoor = false;
					bool bWantDoor = false;
					double WantSince = 0.0;
					double NextOpp = -1.0;
					int DutyId = -1;
					ERoom Room = ERoom::Stage;
				};

				class FLuna : public FWalker
				{
				public:
					explicit FLuna(bool bInAdapted) : FWalker(LunaId, bInAdapted, true, false, false) {}
				};

				// 002 Plack: like Chica to the right door. Owner 2026-10-14: in every mode, at 5 AM she performs the sacrifice and
				// summons 019 Death (at that mode's AI level), who kills her and takes over her role.
				class FPlack : public FWalker
				{
				public:
					explicit FPlack(bool bInAdapted) : FWalker(PlackId, bInAdapted, false, false, false) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						if (!bAdapted && !bSacrificed && W.NightTime() >= 5.0 * NightHourSeconds)
						{
							bSacrificed = true;
							// She is gone: whatever she was doing at the door ends with her.
							if (DutyId >= 0)
							{
								W.CloseDuty(DutyId);
								DutyId = -1;
							}
							bAtDoor = false;
							bWantDoor = false;
							bDisabled = true;
							FSubject* Death = W.MutableModule(DeathId);
							if (Death != nullptr)
							{
								Death->Activate(W);
							}
							W.Emit(EEvent::Sacrifice, Death != nullptr ? 1 : 0);
						}
						FWalker::Update(W, Dt);
					}

				private:
					bool bSacrificed = false;
				};

				// 005 Arita: starts on either Plack's or Luna's path and just walks. A door she reaches must be shut.
				class FArita : public FWalker
				{
				public:
					explicit FArita(bool bInAdapted) : FWalker(AritaId, bInAdapted, true, true, true) {}
				};

				// ============================================================================================= 019 Death
				class FDeath : public FWalker
				{
				public:
					explicit FDeath(bool bInAdapted)
						: FWalker(DeathId, bInAdapted, false, false, false), bDormant(!bInAdapted && SubjectInfo(DeathId).Arrival == EArrival::PlackSacrifice)
					{
					}

					// Owner 2026-10-14: Death is not active until Plack's sacrifice (in every mode).
					void Activate(FUltimateNight& W) override
					{
						if (!bDormant)
						{
							return;
						}
						bDormant = false;
						NextOpp = -1.0;
						NextRush = -1.0;
						Room = ERoom::Stage;
						W.Emit(EEvent::Summoned, DeathId);
						W.Emit(EEvent::Moved, DeathId, static_cast<int>(Room));
					}

					bool IsOnCamera(int Cam) const override { return !bDormant && FWalker::IsOnCamera(Cam); }

					void Update(FUltimateNight& W, double Dt) override
					{
						if (bDormant)
						{
							return;
						}
						FWalker::Update(W, Dt);
						const double Now = W.Time();
						const FUcnTuning& T = W.Tuning();
						if (!bAdapted && Now < NightGraceSeconds)
						{
							return;
						}
						if (NextRush < 0.0)
						{
							NextRush = Now + 5.0;
							NextOffice = Now + T.DeathOfficeEvery;
						}
						const bool bActive = !bDisabled && !bRetired;
						if (bActive && Now >= NextRush)
						{
							NextRush += 5.0;   // owner: every 5 seconds
							for (int Side = 0; Side < 2; ++Side)
							{
								if (RushDuty[Side] < 0 && !bPending[Side])
								{
									bPending[Side] = true;
									PendingSince[Side] = Now;
								}
							}
						}
						for (int Side = 0; Side < 2; ++Side)
						{
							if (bPending[Side])
							{
								FDutySpec Spec;
								Spec.Subject = DeathId;
								Spec.Kind = EDuty::CloseDoor;
								Spec.Side = Side;
								Spec.Window = T.DeathRushWindow;
								Spec.bLethal = true;
								const int Duty = W.OpenDuty(Spec);
								if (Duty >= 0)
								{
									RushDuty[Side] = Duty;
									bPending[Side] = false;
									W.Emit(EEvent::Moved, DeathId, static_cast<int>(FUltimateNight::DoorRoom(Side)));
								}
								else if (!bActive || Now - PendingSince[Side] >= 4.5)
								{
									bPending[Side] = false;   // "if available": the rush is skipped, never forced
								}
							}
							if (RushDuty[Side] >= 0)
							{
								const FDuty* Duty = W.FindDuty(RushDuty[Side]);
								if (Duty == nullptr)
								{
									RushDuty[Side] = -1;
								}
								else if (Now >= Duty->Deadline)
								{
									const int Id0 = RushDuty[Side];
									RushDuty[Side] = -1;
									++Resolved;
									if (W.IsDoorClosed(Side == 0 ? ESide::Left : ESide::Right))
									{
										W.Emit(EEvent::Knock, DeathId, Side);
										W.CloseDuty(Id0);
									}
									else
									{
										W.Kill(DeathId, std::string("Death rushed the ") + (Side == 0 ? "left" : "right") + " door.", Id0);
										return;
									}
								}
							}
						}
						if (OfficeDuty < 0 && bActive && Now >= NextOffice)
						{
							FDutySpec Spec;
							Spec.Subject = DeathId;
							Spec.Kind = EDuty::ClickOffice;
							Spec.Clicks = T.DeathOfficeClicks;
							Spec.Window = T.DeathOfficeWindow;
							Spec.bLethal = true;
							const int Duty = W.OpenDuty(Spec);
							if (Duty >= 0)
							{
								OfficeDuty = Duty;
							}
							else if (Now - NextOffice > T.DeferCap)
							{
								NextOffice = Now + T.DeathOfficeEvery;
								W.NoteGaveUp(Id);
							}
						}
						if (OfficeDuty >= 0)
						{
							const FDuty* Duty = W.FindDuty(OfficeDuty);
							if (Duty == nullptr)
							{
								OfficeDuty = -1;
							}
							else if (Now >= Duty->Deadline)
							{
								W.Kill(DeathId, "Death was in the office and you did not click her away in time.", OfficeDuty);
							}
						}
					}

					void OnClick(FUltimateNight& W, int DutyId0) override
					{
						if (DutyId0 != OfficeDuty || OfficeDuty < 0)
						{
							return;
						}
						FDuty* Duty = W.MutableDuty(OfficeDuty);
						if (Duty == nullptr)
						{
							return;
						}
						if (--Duty->Clicks <= 0)
						{
							W.Emit(EEvent::Resolved, DeathId, static_cast<int>(EDuty::ClickOffice));
							W.CloseDuty(OfficeDuty);
							OfficeDuty = -1;
							++Resolved;
							NextOffice = W.Time() + W.Tuning().DeathOfficeEvery;
						}
					}

					bool CycleDone() const override { return Resolved >= 1 && IsIdle(); }
					bool IsIdle() const override
					{
						return FWalker::IsIdle() && RushDuty[0] < 0 && RushDuty[1] < 0 && OfficeDuty < 0 && !bPending[0] && !bPending[1];
					}

				private:
					bool bDormant;
					double NextRush = -1.0;
					double NextOffice = -1.0;
					int RushDuty[2] = {-1, -1};
					bool bPending[2] = {false, false};
					double PendingSince[2] = {0.0, 0.0};
					int OfficeDuty = -1;
					int Resolved = 0;
				};

				// ============================================================================================== 003 Kuu
				class FKuu : public FSubject
				{
				public:
					explicit FKuu(bool bInAdapted) : FSubject(KuuId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						const FMovement M = MovementFor(AiOf(W, *this));
						const double Now = W.Time();
						if (NextOpp < 0.0)
						{
							NextOpp = (bAdapted ? Now : std::max(Now, NightGraceSeconds)) + M.IntervalSeconds * 0.45;
						}
						if (State == EState::Winding)
						{
							WindupLeft -= Dt;
							if (WindupLeft <= 0.0)
							{
								Punch(W, M);
							}
							return;
						}
						if (!bAdapted && Now < NightGraceSeconds)
						{
							return;
						}
						if (bRetired || Now < NextOpp)
						{
							return;
						}
						NextOpp = std::max(NextOpp + M.IntervalSeconds, Now + 0.01);
						if (!W.Roll(M.Chance))
						{
							return;
						}
						// "Copies either Luna's or Plack's route to a door."
						if (State == EState::Home)
						{
							bLeft = W.Roll(0.5);
							State = EState::Walking;
							Room = ERoom::Stage;
						}
						const ERoom Next = WalkNext(W, Room, bLeft);
						const int Side = bLeft ? 0 : 1;
						if (Next == FUltimateNight::DoorRoom(Side))
						{
							// She may only use a door nobody else needs: no lethal duty there, not broken, no flash run going on.
							if (W.IsDoorBlocked(Side) || W.HasOpenLethalOnSide(Side) || W.StargazerReserved() || W.AngelWaiting())
							{
								return;
							}
							Room = Next;
							State = EState::Winding;
							WindupLeft = W.Tuning().KuuWindup;   // owner: a loud 4-second charge
							FDutySpec Spec;
							Spec.Subject = KuuId;
							Spec.Kind = EDuty::OpenDoor;
							Spec.Side = Side;
							Spec.Window = WindupLeft;
							Spec.bLethal = false;
							DutyId = W.OpenDuty(Spec);
							W.Emit(EEvent::Moved, KuuId, static_cast<int>(Room));
							W.Emit(EEvent::KuuWindup, Side);
							return;
						}
						Room = Next;
						W.Emit(EEvent::Moved, KuuId, static_cast<int>(Room));
					}

					bool CycleDone() const override { return bPunched && State == EState::Home; }
					bool IsIdle() const override { return State != EState::Winding; }
					bool IsOnCamera(int Cam) const override
					{
						return State != EState::Winding && IsCameraRoom(Room) && static_cast<int>(Room) == Cam;
					}

				private:
					enum class EState
					{
						Home,
						Walking,
						Winding
					};

					void Punch(FUltimateNight& W, const FMovement& M)
					{
						const int Side = bLeft ? 0 : 1;
						const ESide S = bLeft ? ESide::Left : ESide::Right;
						const bool bBroke = W.IsDoorClosed(S);   // owner: if your door is shut it breaks; an open doorway is a miss
						if (bBroke)
						{
							W.BreakDoor(S);
						}
						W.Emit(EEvent::KuuPunch, Side, bBroke ? 1 : 0);
						if (DutyId >= 0)
						{
							W.CloseDuty(DutyId);
							DutyId = -1;
						}
						// owner: after the punch she teleports back to the stage
						Room = ERoom::Stage;
						State = EState::Home;
						bPunched = true;
						NextOpp = W.Time() + M.IntervalSeconds * 2.0;
						W.Emit(EEvent::Moved, KuuId, static_cast<int>(Room));
					}

					EState State = EState::Home;
					bool bLeft = true;
					bool bPunched = false;
					ERoom Room = ERoom::Stage;
					double WindupLeft = 0.0;
					double NextOpp = -1.0;
					int DutyId = -1;
				};

				// ============================================================================================ 004 Ishii
				class FIshii : public FSubject
				{
				public:
					explicit FIshii(bool bInAdapted) : FSubject(IshiiId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						const FMovement M = MovementFor(AiOf(W, *this));
						const double Now = W.Time();
						if (NextOpp < 0.0)
						{
							NextOpp = (bAdapted ? Now : std::max(Now, NightGraceSeconds)) + M.IntervalSeconds * 0.5;
							Cam = W.PickCamera(false);
						}
						if (bInfecting)
						{
							InfectLeft -= Dt;
							W.SetInfection(InfectCam, std::max(0.0, InfectLeft));
							if (W.IsCameraDisabled(InfectCam))
							{
								EndInfection(W, false);
							}
							else if (InfectLeft <= 0.0)
							{
								W.SetCameraDisabled(InfectCam);   // owner: left unchecked for 15 s the camera is permanently disabled
								EndInfection(W, false);
							}
						}
						if (Cooldown > 0.0)
						{
							Cooldown -= Dt;
						}
						if (!bAdapted && Now < NightGraceSeconds)
						{
							return;
						}
						// She goes randomly across the whole camera system (movement scales with AI).
						if (Now >= NextOpp)
						{
							NextOpp = std::max(NextOpp + M.IntervalSeconds, Now + 0.01);
							if (W.Roll(M.Chance))
							{
								const int To = W.PickCamera(false, Cam);
								if (To >= 0)
								{
									Cam = To;
								}
							}
						}
						if (Cam >= 0 && W.IsCameraDisabled(Cam))
						{
							Cam = W.PickCamera(false);
						}
						if (!bRetired && !bInfecting && Cooldown <= 0.0 && Cam >= 0 && W.ShocksLeft(Cam) > 0 && W.ShocksAvailableForNonLethal())
						{
							Cooldown = 25.0;   // owner: 25-second cooldown between attacks
							if (W.InfectionBlockedByVaccine())
							{
								bAttacked = true;   // the vaccine "disables the next infection"
								return;
							}
							bInfecting = true;
							bAttacked = true;
							InfectCam = Cam;
							InfectLeft = 15.0;   // owner: 15 seconds
							W.SetInfection(InfectCam, InfectLeft);
							FDutySpec Spec;
							Spec.Subject = IshiiId;
							Spec.Kind = EDuty::ShockInfection;
							Spec.Cam = InfectCam;
							Spec.Window = InfectLeft;
							Spec.bLethal = false;
							Spec.bLocated = false;
							DutyId = W.OpenDuty(Spec);
							W.Emit(EEvent::Infection, InfectCam);
						}
					}

					bool OnShock(FUltimateNight& W, int ShockCam) override
					{
						if (bInfecting && ShockCam == InfectCam)
						{
							W.Emit(EEvent::Resolved, IshiiId, static_cast<int>(EDuty::ShockInfection));
							EndInfection(W, true);
							return true;
						}
						return false;
					}

					bool NeedsShockAt(int ShockCam) const override { return bInfecting && ShockCam == InfectCam; }
					bool IsOnCamera(int C) const override { return Cam == C; }
					bool CycleDone() const override { return bAttacked && !bInfecting; }
					bool IsIdle() const override { return !bInfecting; }

				private:
					void EndInfection(FUltimateNight& W, bool)
					{
						bInfecting = false;
						W.SetInfection(InfectCam, 0.0);
						if (DutyId >= 0)
						{
							W.CloseDuty(DutyId);
							DutyId = -1;
						}
					}

					int Cam = -1;
					bool bInfecting = false;
					bool bAttacked = false;
					int InfectCam = -1;
					double InfectLeft = 0.0;
					double Cooldown = 0.0;
					double NextOpp = -1.0;
					int DutyId = -1;
				};

				// ========================================================================================== 006 Fujita
				class FFujita : public FSubject
				{
				public:
					explicit FFujita(bool bInAdapted) : FSubject(FujitaId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						(void)Dt;
						const double Now = W.Time();
						if (Next < 0.0)
						{
							Next = (bAdapted ? Now : std::max(Now, NightGraceSeconds)) + W.Uniform(25.0, 40.0);   // owner: 25-40 s cooldown
							Next = bAdapted ? Now + 1.0 : Next;
						}
						if (DutyId >= 0)
						{
							const FDuty* Duty = W.FindDuty(DutyId);
							if (Duty == nullptr)
							{
								DutyId = -1;
							}
							else if (Now >= Duty->Deadline)
							{
								const int Id0 = DutyId;
								DutyId = -1;
								if (W.IsDoorClosed(Side == 0 ? ESide::Left : ESide::Right))
								{
									W.Emit(EEvent::Knock, FujitaId, Side);
									W.CloseDuty(Id0);
									bDone = true;
									Next = Now + W.Uniform(25.0, 40.0);
								}
								else
								{
									W.Kill(FujitaId, "Fujita's five seconds ran out at your door.", Id0);
								}
							}
							return;
						}
						if (bRetired || Now < Next)
						{
							return;
						}
						if (!bPicked)
						{
							// owner: one of 3 attacks: mimic Luna (left), mimic Plack (right), mimic Kuu (either door)
							const int Attack = W.RandInt(0, 2);
							Side = Attack == 0 ? 0 : (Attack == 1 ? 1 : W.RandInt(0, 1));
							Mimic = Attack;
							bPicked = true;
							PickedAt = Now;
						}
						FDutySpec Spec;
						Spec.Subject = FujitaId;
						Spec.Kind = EDuty::CloseDoor;
						Spec.Side = Side;
						Spec.Window = W.Tuning().FujitaWindow;   // owner: 5-second kill timer
						Spec.bLethal = true;
						DutyId = W.OpenDuty(Spec);
						if (DutyId >= 0)
						{
							bPicked = false;
							W.Emit(EEvent::Moved, FujitaId, static_cast<int>(FUltimateNight::DoorRoom(Side)), Mimic);
						}
						else if (Now - PickedAt > W.Tuning().DeferCap)
						{
							bPicked = false;
							Next = Now + W.Uniform(25.0, 40.0);
							W.NoteGaveUp(Id);
						}
					}

					bool CycleDone() const override { return bDone && DutyId < 0; }
					bool IsIdle() const override { return DutyId < 0; }

				private:
					double Next = -1.0;
					double PickedAt = 0.0;
					bool bPicked = false;
					bool bDone = false;
					int Side = 0;
					int Mimic = 0;
					int DutyId = -1;
				};

				// ====================================================================================== 007 Shiraishi
				// Stage like Freddy, 1 camera per 20 s; camera on her 45 s (stalled, not frozen: OPEN wording); cameras down 10 s.
				class FShiraishi : public FSubject
				{
				public:
					explicit FShiraishi(bool bInAdapted) : FSubject(ShiraishiId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						const double Now = W.Time();
						if (!bAdapted && Now < NightGraceSeconds)
						{
							return;
						}
						if (bRetired)
						{
							return;
						}
						const int Door = W.Tuning().ShiraishiStepsToDoor;
						DoorSteps = Door;
						if (Step >= Door)
						{
							bAtDoor = true;
							BurnLeft -= Dt;
							if (BurnLeft <= 0.0)
							{
								W.Kill(ShiraishiId, "Shiraishi reached the door, burned it and came in.", -1, true);
							}
							return;
						}
						double Seconds = 20.0;
						if (!W.IsCameraUp())
						{
							Seconds = 10.0;   // owner: with the cameras down she moves in the background at 1 camera per 10 s
						}
						else if (W.IsViewing(CameraNow()))
						{
							Seconds = 45.0;   // owner: camera up on her, 1 camera per 45 s
						}
						Progress += Dt / Seconds;
						if (Progress >= 1.0)
						{
							Progress -= 1.0;
							++Step;
							if (Step >= Door)
							{
								BurnLeft = W.Tuning().ShiraishiBurn;
								W.Emit(EEvent::Moved, ShiraishiId, static_cast<int>(ERoom::RightDoor));
							}
							else
							{
								W.Emit(EEvent::Moved, ShiraishiId, CameraNow());
							}
						}
						if (!bRetired && DutyId < 0 && Step < Door)
						{
							FDutySpec Spec;
							Spec.Subject = ShiraishiId;
							Spec.Kind = EDuty::WatchShiraishi;
							Spec.Cam = CameraNow();
							Spec.Window = Never;
							Spec.bLethal = false;
							DutyId = W.OpenDuty(Spec);
						}
						if (FDuty* Duty = W.MutableDuty(DutyId))
						{
							Duty->Cam = CameraNow();
						}
					}

					bool IsOnCamera(int Cam) const override { return !bAtDoor && CameraNow() == Cam; }
					bool IsIdle() const override { return bRetired || Step == 0; }
					bool CycleDone() const override { return false; }   // she is the whole night; a copy is cut by Adapt's cap
					double StepsToDoor() const override { return bAtDoor ? 0.0 : static_cast<double>(DoorSteps - Step) - Progress; }

				private:
					int CameraNow() const { return Step % CameraCount; }

					int Step = 0;
					int DoorSteps = 10;
					double Progress = 0.0;
					double BurnLeft = 0.0;
					bool bAtDoor = false;
					int DutyId = -1;
				};

				// ========================================================================================== 008 Myrcia
				class FMyrcia : public FSubject
				{
				public:
					explicit FMyrcia(bool bInAdapted) : FSubject(MyrciaId, bInAdapted) {}

					void Update(FUltimateNight& W, double) override
					{
						const double Now = W.Time();
						const double Night = W.NightTime();   // bursts follow the night's clock (Time Keeper can send it back)
						if (NextBurst < 0.0)
						{
							NextBurst = bAdapted ? Night + 1.0 : NightHourSeconds;   // first burst at 1 AM, then at hour intervals (PLACEHOLDER reading)
							Last = -1;
						}
						if (DutyId >= 0)
						{
							const FDuty* Duty = W.FindDuty(DutyId);
							if (Duty == nullptr)
							{
								DutyId = -1;
							}
							else if (Now >= Duty->Deadline)
							{
								const int Id0 = DutyId;
								DutyId = -1;
								if (W.IsDoorClosed(Last == 0 ? ESide::Left : ESide::Right))
								{
									W.Emit(EEvent::Knock, MyrciaId, Last);
									W.CloseDuty(Id0);
									GapUntil = Now + 0.3;
								}
								else
								{
									W.Kill(MyrciaId, std::string("Myrcia rushed the ") + (Last == 0 ? "left" : "right") + " door.", Id0);
								}
							}
							return;
						}
						if (Remaining == 0 && !bRetired && Bursts < 3 && Night >= NextBurst)
						{
							Remaining = W.RandInt(2, 5);   // owner: rushes your doors 2-5 times
							++Bursts;
							NextBurst += NightHourSeconds;
							RushSince = Now;
						}
						if (Remaining > 0 && Now >= GapUntil)
						{
							const int Side = Last < 0 ? W.RandInt(0, 1) : (W.Roll(0.5) ? Last : 1 - Last);   // same door or the other one
							FDutySpec Spec;
							Spec.Subject = MyrciaId;
							Spec.Kind = EDuty::CloseDoor;
							Spec.Side = Side;
							Spec.Window = W.Tuning().MyrciaWindow;   // owner: 1.5 s per door rush
							Spec.bLethal = true;
							DutyId = W.OpenDuty(Spec);
							if (DutyId >= 0)
							{
								Last = Side;
								--Remaining;
								RushSince = Now;
								W.Emit(EEvent::Moved, MyrciaId, static_cast<int>(FUltimateNight::DoorRoom(Side)));
							}
							else if (Now - RushSince > W.Tuning().DeferCap)
							{
								--Remaining;
								RushSince = Now;
								W.NoteGaveUp(Id);
							}
						}
					}

					bool CycleDone() const override { return Bursts >= 1 && Remaining == 0 && DutyId < 0; }
					bool IsIdle() const override { return Remaining == 0 && DutyId < 0; }

				private:
					double NextBurst = -1.0;
					double GapUntil = 0.0;
					double RushSince = 0.0;
					int Bursts = 0;
					int Remaining = 0;
					int Last = -1;
					int DutyId = -1;
				};

				// ======================================================================================= 009 Alchemist
				// Sits on CAM 6 (OPEN: "the middle"), every 30 s disables a camera; the vaccine bottle blocks the next infection.
				class FAlchemist : public FSubject
				{
				public:
					explicit FAlchemist(bool bInAdapted) : FSubject(AlchemistId, bInAdapted) {}

					void Update(FUltimateNight& W, double) override
					{
						const double Now = W.Time();
						if (NextLeak < 0.0)
						{
							NextLeak = bAdapted ? Now + 0.5 : Now + 30.0;
							RespawnAt = Now;
						}
						// The bottle: one on a random working camera; a new one appears VaccineRespawn after one is taken or used.
						if (!bAdapted)
						{
							if (W.VaccineCamera() < 0 && !W.IsVaccineArmed())
							{
								if (RespawnAt < 0.0)
								{
									RespawnAt = Now + W.Tuning().VaccineRespawn;
								}
								if (Now >= RespawnAt)
								{
									const int Cam = W.PickCamera(false);
									if (Cam >= 0)
									{
										W.SetVaccine(Cam);
										RespawnAt = -1.0;
									}
								}
							}
							else
							{
								RespawnAt = -1.0;
							}
							SyncBottleDuty(W);
						}
						if (!bRetired && Now >= NextLeak)
						{
							NextLeak += 30.0;   // owner: every 30 seconds she disables a camera (no drift)
							bLeaked = true;
							if (!W.InfectionBlockedByVaccine())
							{
								const int Cam = W.PickCamera(false);
								if (Cam >= 0)
								{
									W.Emit(EEvent::Infection, Cam, 1);
									W.SetCameraDisabled(Cam);
								}
							}
						}
					}

					bool IsOnCamera(int Cam) const override { return Cam == 5; }   // CAM 6
					bool CycleDone() const override { return bLeaked; }

				private:
					void SyncBottleDuty(FUltimateNight& W)
					{
						if (DutyId >= 0)
						{
							const FDuty* Duty = W.FindDuty(DutyId);
							if (Duty == nullptr || W.VaccineCamera() != Duty->Cam)
							{
								W.CloseDuty(DutyId);
								DutyId = -1;
							}
						}
						if (DutyId < 0 && W.VaccineCamera() >= 0)
						{
							FDutySpec Spec;
							Spec.Subject = AlchemistId;
							Spec.Kind = EDuty::TakeVaccine;
							Spec.Cam = W.VaccineCamera();
							Spec.Window = Never;
							Spec.bLethal = false;
							Spec.bLocated = false;
							DutyId = W.OpenDuty(Spec);
						}
					}

					double NextLeak = -1.0;
					double RespawnAt = -1.0;
					bool bLeaked = false;
					int DutyId = -1;
				};

				// Fallback for an unknown id: no attack, no kill path.
				class FStub : public FSubject
				{
				public:
					FStub(int InId, bool bInAdapted) : FSubject(InId, bInAdapted) {}
					void Update(FUltimateNight&, double) override {}
				};

				// ============================================================================================== 010 Blood
				// Owner 2026-10-14: appears on your middle (front) window every 20-30 s and goes through 4 phases of 2-10 s. Do NOT
				// shut the window until the last phase, when she is closest; then shut it.
				class FBlood : public FSubject
				{
				public:
					explicit FBlood(bool bInAdapted) : FSubject(BloodId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						const double Now = W.Time();
						const FUcnTuning& T = W.Tuning();
						if (Next < 0.0)
						{
							Next = bAdapted ? Now + 1.0 : std::max(Now, NightGraceSeconds) + W.Uniform(T.BloodEveryMin, T.BloodEveryMax);
						}
						switch (Phase)
						{
						case 0:
							// She comes to an open, quiet window (the scheduler never lets her arrive behind a shut one).
							if (bRetired || Now < Next || W.IsWindowClosed() || W.BloodPhaseNow() > 0 || W.HasOpenLethalOnSide(WindowSide) || W.StargazerReserved())
							{
								return;
							}
							Phase = 1;
							PhaseLeft = W.Uniform(T.BloodPhaseMin, T.BloodPhaseMax);
							W.SetWindowLocked(true);
							W.SetBloodPhase(1);
							W.Emit(EEvent::BloodPhase, 1);
							return;
						case 1:
						case 2:
						case 3:
							PhaseLeft -= Dt;
							if (PhaseLeft > 0.0)
							{
								return;
							}
							if (Phase < 3)
							{
								++Phase;
								PhaseLeft = W.Uniform(T.BloodPhaseMin, T.BloodPhaseMax);
								W.SetBloodPhase(Phase);
								W.Emit(EEvent::BloodPhase, Phase);
								return;
							}
							{
								// The last phase is the deadly one: placed by the scheduler, she waits in phase 3 if it cannot fit yet.
								W.SetWindowLocked(false);
								FDutySpec Spec;
								Spec.Subject = BloodId;
								Spec.Kind = EDuty::CloseDoor;
								Spec.Side = WindowSide;
								Spec.Window = W.Uniform(T.BloodPhaseMin, T.BloodPhaseMax);
								Spec.bLethal = true;
								DutyId = W.OpenDuty(Spec);
								if (DutyId >= 0)
								{
									Phase = 4;
									Waited = 0.0;
									W.SetBloodPhase(4);
									W.Emit(EEvent::BloodPhase, 4);
								}
								else
								{
									W.SetWindowLocked(true);
									Waited += Dt;
									if (Waited > T.DeferCap)
									{
										W.NoteGaveUp(Id);
										Leave(W);
									}
								}
							}
							return;
						default:
						{
							const FDuty* Duty = W.FindDuty(DutyId);
							if (Duty == nullptr)
							{
								Leave(W);
							}
							else if (W.IsWindowClosed())
							{
								W.Emit(EEvent::Resolved, BloodId, static_cast<int>(EDuty::CloseDoor));
								Leave(W);
							}
							else if (Now >= Duty->Deadline)
							{
								W.Kill(BloodId, "Blood came through the window in her last phase.", DutyId);
							}
							return;
						}
						}
					}

					void OnWindowShut(FUltimateNight& W) override
					{
						if (Phase >= 1 && Phase <= 3)
						{
							// Owner: DON'T shut her window before the last phase. A punishment for a player action, cued by her phase.
							W.Kill(BloodId, "You shut the window on Blood too early - wait for her last phase.", -1, true);
						}
					}

					bool CycleDone() const override { return Visits >= 1 && Phase == 0; }
					bool IsIdle() const override { return Phase == 0; }

				private:
					void Leave(FUltimateNight& W)
					{
						if (DutyId >= 0)
						{
							W.CloseDuty(DutyId);
							DutyId = -1;
						}
						Phase = 0;
						Waited = 0.0;
						++Visits;
						W.SetWindowLocked(false);
						W.SetBloodPhase(0);
						W.Emit(EEvent::BloodPhase, 0);
						Next = W.Time() + W.Uniform(W.Tuning().BloodEveryMin, W.Tuning().BloodEveryMax);
					}

					double Next = -1.0;
					double PhaseLeft = 0.0;
					double Waited = 0.0;
					int Phase = 0;
					int Visits = 0;
					int DutyId = -1;
				};

				// ============================================================================================ 013 Weather
				// Owner 2026-10-14: takes the form of another operator that comes to CAM 3 at your window with a neon yellow or
				// light blue aura: shut the window when you spot him. How often and how long he waits are PLACEHOLDERS (OPEN).
				class FWeather : public FSubject
				{
				public:
					explicit FWeather(bool bInAdapted) : FSubject(WeatherId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						const double Now = W.Time();
						const FUcnTuning& T = W.Tuning();
						if (Next < 0.0)
						{
							Next = bAdapted ? Now + 1.0 : std::max(Now, NightGraceSeconds) + W.Uniform(T.WeatherEveryMin, T.WeatherEveryMax);
						}
						if (DutyId >= 0)
						{
							const FDuty* Duty = W.FindDuty(DutyId);
							if (Duty == nullptr)
							{
								Gone(W);
							}
							else if (Now >= Duty->Deadline)
							{
								if (W.IsWindowClosed())
								{
									W.Emit(EEvent::Knock, WeatherId, WindowSide);
									W.Emit(EEvent::Resolved, WeatherId, static_cast<int>(EDuty::CloseDoor));
									Gone(W);
								}
								else
								{
									W.Kill(WeatherId, std::string("Weather, disguised as ") + NameOf(Look) + ", came through the window.", DutyId);
								}
							}
							return;
						}
						if (bRetired || Now < Next)
						{
							return;
						}
						if (!bPicked)
						{
							Look = W.RandInt(0, SubjectCount - 2);   // "another operator": any subject but himself
							Look = Look >= WeatherId ? Look + 1 : Look;
							Aura = W.RandInt(0, 1);                  // owner: neon yellow or light blue
							bPicked = true;
							PickedAt = Now;
						}
						FDutySpec Spec;
						Spec.Subject = WeatherId;
						Spec.Kind = EDuty::CloseDoor;
						Spec.Side = WindowSide;
						Spec.Window = T.WeatherWindow;
						Spec.bLethal = true;
						DutyId = W.OpenDuty(Spec);
						if (DutyId >= 0)
						{
							bPicked = false;
							W.SetWeatherLook(Look, Aura);
							W.Emit(EEvent::Weather, Look, Aura);
							W.Emit(EEvent::Moved, WeatherId, WindowCamera);
						}
						else if (W.IsWindowLocked())
						{
							PickedAt += Dt;   // he waits Blood's visit out (bounded: her visit is) instead of giving up
						}
						else if (Now - PickedAt > T.DeferCap)
						{
							bPicked = false;
							Next = Now + W.Uniform(T.WeatherEveryMin, T.WeatherEveryMax);
							W.NoteGaveUp(Id);
						}
					}

					bool IsOnCamera(int Cam) const override { return DutyId >= 0 && Cam == WindowCamera; }
					bool CycleDone() const override { return Visits >= 1 && DutyId < 0; }
					bool IsIdle() const override { return DutyId < 0; }

				private:
					void Gone(FUltimateNight& W)
					{
						if (DutyId >= 0)
						{
							W.CloseDuty(DutyId);
							DutyId = -1;
						}
						++Visits;
						W.SetWeatherLook(-1, 0);
						W.Emit(EEvent::Weather, -1, 0);
						Next = W.Time() + W.Uniform(W.Tuning().WeatherEveryMin, W.Tuning().WeatherEveryMax);
					}

					double Next = -1.0;
					double PickedAt = 0.0;
					bool bPicked = false;
					int Look = 0;
					int Aura = 0;
					int Visits = 0;
					int DutyId = -1;
				};

				// ======================================================================================== 011 Stargazer
				// Wakes over 45-55 s (nothing slows it), then walks 1-7-9-5-3-6-4-8, 0.5 s on each camera, then CAM 2 or 3 and
				// rushes. Owner 2026-10-14: from CAM 2 or 3 he smashes your front window and kills you (a shut window does not stop him).
				// A flash (shock) on the camera he is on sends him back.
				class FStargazer : public FSubject
				{
				public:
					explicit FStargazer(bool bInAdapted) : FSubject(StargazerId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						if (State == EState::Idle)
						{
							WakeLeft = W.Uniform(45.0, 55.0);   // owner: about 45-55 s, cannot be slowed
							State = EState::Asleep;
						}
						switch (State)
						{
						case EState::Asleep:
						{
							WakeLeft -= Dt;
							const double Warn = W.Tuning().StargazerWarn;
							if (bRetired)
							{
								ReleaseHold(W);   // a dropped copy never keeps the player's hands reserved
								break;
							}
							if (WakeLeft <= Warn)
							{
								if (!bWarned)
								{
									bWarned = true;
									W.Emit(EEvent::Stargazer, 0);
								}
								if (!bHolding)
								{
									bHolding = true;
									W.HoldStargazerReserve(true);
								}
							}
							if (WakeLeft <= 0.0)
							{
								// He only starts when the player has nothing else to answer and a flash can actually work.
								if (W.NextLethalDeadline() > 1e8 && RouteHasShock(W))
								{
									BeginRoute(W);
								}
								if (State == EState::Asleep)
								{
									// The reservation keeps new threats away until he can go - bounded: after DeferCap he skips this wake.
									WakeLeft = 0.0;
									Waited += Dt;
									if (Waited > W.Tuning().DeferCap)
									{
										W.NoteGaveUp(Id);
										Reset(W);
									}
								}
							}
							break;
						}
						case EState::Route:
							StopLeft -= Dt;
							if (StopLeft <= 0.0)
							{
								NextStop(W);
							}
							break;
						case EState::Rush:
							RushLeft -= Dt;
							if (RushLeft <= 0.0)
							{
								ReleaseHold(W);
								W.Kill(StargazerId, "Stargazer smashed your front window.", -1, true);
							}
							break;
						case EState::Idle:
							break;
						}
					}

					bool NeedsShockAt(int Cam) const override { return State == EState::Route && Cam == CurrentCam && !bCurrentHidden; }

					bool OnShock(FUltimateNight& W, int Cam) override
					{
						if (State != EState::Route || Cam != CurrentCam || bCurrentHidden)
						{
							return false;
						}
						W.Emit(EEvent::Stargazer, 3, Cam);
						W.Emit(EEvent::Resolved, StargazerId, static_cast<int>(EDuty::FlashCamera));
						W.NoteShockNeed(Cam);
						Reset(W);
						++Sent;
						return true;
					}

					bool IsOnCamera(int Cam) const override { return State == EState::Route && Cam == CurrentCam; }
					bool CycleDone() const override { return Sent >= 1 && State != EState::Route && State != EState::Rush; }
					bool IsIdle() const override { return State != EState::Route && State != EState::Rush && !bWarned; }

				private:
					enum class EState
					{
						Idle,
						Asleep,
						Route,
						Rush
					};

					static constexpr int RouteLength = 9;   // 8 stops (1-7-9-5-3-6-4-8) then CAM 2 or 3

					// CAM labels are 1..9 and 0 (BIBArcadeOverlay); label n = camera index n - 1.
					int StopCamera(int Index) const
					{
						static const int Order[8] = {0, 6, 8, 4, 2, 5, 3, 7};
						return Index < 8 ? Order[Index] : FinalCam;
					}

					bool RouteHasShock(FUltimateNight& W) const
					{
						for (int Index = 0; Index < 8; ++Index)
						{
							const int Cam = StopCamera(Index);
							if (!W.IsCameraDisabled(Cam) && W.ShocksLeft(Cam) > 0)
							{
								return true;
							}
						}
						return false;
					}

					void BeginRoute(FUltimateNight& W)
					{
						FDutySpec Spec;
						Spec.Subject = StargazerId;
						Spec.Kind = EDuty::FlashCamera;
						Spec.Cam = StopCamera(0);
						Spec.Window = 0.5;
						Spec.bLethal = true;
						Spec.bLocated = true;
						DutyId = W.OpenDuty(Spec);   // exclusive: the reservation already cleared the player's hands
						if (DutyId < 0)
						{
							return;   // never a route without its flash duty: he waits (still reserved) and tries again
						}
						FinalCam = W.RandInt(1, 2);   // CAM 2 or CAM 3 (index 1 or 2)
						Waited = 0.0;
						Stop = 0;
						State = EState::Route;
						EnterStop(W);
					}

					void EnterStop(FUltimateNight& W)
					{
						CurrentCam = StopCamera(Stop);
						bCurrentHidden = W.IsCameraDisabled(CurrentCam);
						StopLeft = 0.5;   // owner: 0.5 s on each camera
						if (FDuty* Duty = W.MutableDuty(DutyId))
						{
							Duty->Cam = CurrentCam;
							Duty->Deadline = W.Time() + 0.5;
						}
						W.NoteShockNeed(CurrentCam);
						W.Emit(EEvent::Stargazer, 1, CurrentCam);
					}

					void NextStop(FUltimateNight& W)
					{
						W.NoteShockNeed(CurrentCam);
						++Stop;
						if (Stop < RouteLength)
						{
							EnterStop(W);
							return;
						}
						// Out of cameras: from CAM 2 or 3 he rushes your front window and smashes it (owner 2026-10-14).
						if (DutyId >= 0)
						{
							W.CloseDuty(DutyId);
							DutyId = -1;
						}
						State = EState::Rush;
						RushLeft = W.Tuning().StargazerRush;
						W.Emit(EEvent::Stargazer, 2, WindowSide, FinalCam);
					}

					void Reset(FUltimateNight& W)
					{
						if (DutyId >= 0)
						{
							W.CloseDuty(DutyId);
							DutyId = -1;
						}
						ReleaseHold(W);
						State = EState::Asleep;
						bWarned = false;
						Waited = 0.0;
						WakeLeft = W.Uniform(45.0, 55.0);
					}

					void ReleaseHold(FUltimateNight& W)
					{
						if (bHolding)
						{
							bHolding = false;
							W.HoldStargazerReserve(false);
						}
					}

					EState State = EState::Idle;
					double WakeLeft = 0.0;
					double StopLeft = 0.0;
					double RushLeft = 0.0;
					double Waited = 0.0;
					int Stop = 0;
					int FinalCam = 1;
					int CurrentCam = -1;
					bool bCurrentHidden = false;
					bool bWarned = false;
					bool bHolding = false;
					int Sent = 0;
					int DutyId = -1;
				};

				// ========================================================================================== 012 Gambler
				class FGambler : public FSubject
				{
				public:
					explicit FGambler(bool bInAdapted) : FSubject(GamblerId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						(void)Dt;
						const double Now = W.Time();
						if (Next < 0.0)
						{
							Next = bAdapted ? Now + 1.0 : std::max(Now, NightGraceSeconds) + 8.0;   // PLACEHOLDER first appearance
						}
						if (Cam >= 0 && W.IsCameraDisabled(Cam))
						{
							Cam = W.PickCamera(true);
							if (Cam < 0)
							{
								Leave(W, Now);
							}
							else if (FDuty* Duty = W.MutableDuty(DutyId))
							{
								Duty->Cam = Cam;
							}
						}
						if (Cam < 0 && !bRetired && Now >= Next && W.ShocksAvailableForNonLethal())
						{
							Cam = W.PickCamera(true);
							if (Cam >= 0)
							{
								FDutySpec Spec;
								Spec.Subject = GamblerId;
								Spec.Kind = EDuty::ShockGambler;
								Spec.Cam = Cam;
								Spec.Window = Never;   // OPEN: the owner gave no penalty for ignoring her
								Spec.bLethal = false;
								Spec.bLocated = false;
								DutyId = W.OpenDuty(Spec);
							}
						}
					}

					bool NeedsShockAt(int C) const override { return Cam >= 0 && C == Cam; }

					bool OnShock(FUltimateNight& W, int C) override
					{
						if (Cam < 0 || C != Cam)
						{
							return false;
						}
						const bool bHeads = W.Roll(0.5);
						W.Emit(EEvent::CoinFlip, bHeads ? 1 : 0);   // owner: high pitch heads = find her again, low pitch tails = she leaves
						W.NoteShockNeed(C);
						if (bHeads && W.ShocksAvailableForNonLethal())
						{
							const int To = W.PickCamera(true, Cam);
							if (To >= 0)
							{
								Cam = To;
								if (FDuty* Duty = W.MutableDuty(DutyId))
								{
									Duty->Cam = Cam;
								}
								return true;
							}
						}
						Leave(W, W.Time());
						return true;
					}

					bool IsOnCamera(int C) const override { return Cam >= 0 && C == Cam; }
					bool CycleDone() const override { return bLeft && Cam < 0; }
					bool IsIdle() const override { return Cam < 0; }

				private:
					void Leave(FUltimateNight& W, double Now)
					{
						Cam = -1;
						bLeft = true;
						Next = Now + 40.0;   // owner: 40-second cooldown
						if (DutyId >= 0)
						{
							W.CloseDuty(DutyId);
							DutyId = -1;
						}
					}

					double Next = -1.0;
					int Cam = -1;
					bool bLeft = false;
					int DutyId = -1;
				};

				// =========================================================================================== 014 Judge
				// The rule lives in FUltimateNight (SetDoor / Shock): this module only keeps the fail-safe switched on.
				class FJudge : public FSubject
				{
				public:
					explicit FJudge(bool bInAdapted) : FSubject(JudgeId, bInAdapted) {}
					void Update(FUltimateNight& W, double Dt) override
					{
						W.MarkJudging();
						if (bAdapted)
						{
							Held += Dt;
						}
					}
					bool CycleDone() const override { return Held >= 20.0; }   // PLACEHOLDER: a passive copy lasts 20 s

				private:
					double Held = 0.0;
				};

				// ====================================================================================== 015 Narrator / Diano
				// Even hours Narrator: a mini camera into your room every 8 s; the third at the same time kills; 3-5 clicks remove
				// one. Odd hours Diano (owner 2026-10-14, like Plushtrap): every time you put the camera down he attacks the left
				// door (left-panned audio), the right door (right-panned) or the middle window (centred): 1 second to shut it.
				class FNarrator : public FSubject
				{
				public:
					explicit FNarrator(bool bInAdapted) : FSubject(NarratorId, bInAdapted) {}

					void OnCameraDown(FUltimateNight& W, bool bForced) override
					{
						// Only the player's own camera-down (Jester's forced moves cannot cause a death), one attack at a time.
						if (!bForced && !bAdapted && !bRetired && W.Hour() % 2 == 1 && DianoDuty < 0 && !bDianoPending)
						{
							bDianoPending = true;
							DianoSince = W.Time();
							DianoFirst = W.RandInt(0, 2);
						}
					}

					void Update(FUltimateNight& W, double) override
					{
						const double Now = W.Time();
						UpdateDiano(W, Now);
						if (W.IsOver())
						{
							return;
						}
						if (Next < 0.0)
						{
							Next = bAdapted ? Now + 2.0 : std::max(Now, NightGraceSeconds) + 4.0;
						}
						// Drop duties the player finished.
						for (size_t Index = 0; Index < Minis.size();)
						{
							if (W.FindDuty(Minis[Index]) == nullptr)
							{
								Minis.erase(Minis.begin() + static_cast<std::ptrdiff_t>(Index));
							}
							else
							{
								++Index;
							}
						}
						SyncThreat(W);
						if (ThreatDuty >= 0)
						{
							const FDuty* Threat = W.FindDuty(ThreatDuty);
							if (Threat == nullptr)
							{
								ThreatDuty = -1;
							}
							else if (Now >= Threat->Deadline)
							{
								if (W.MiniCamerasInOffice() >= 2)
								{
									W.Kill(NarratorId, "Three of Narrator's cameras were in your room.", ThreatDuty);
									return;
								}
								W.CloseDuty(ThreatDuty);
								ThreatDuty = -1;
							}
						}
						const bool bNarratorHour = bAdapted || (W.Hour() % 2 == 0);   // even hours: Narrator; odd: Diano (inert)
						if (bRetired || !bNarratorHour || Now < Next || (bAdapted && Sent >= 3))
						{
							return;
						}
						if (W.MiniCamerasInOffice() >= 2)
						{
							// The third camera is the lethal one: it is announced with a window and only enters if two are still there.
							if (ThreatDuty < 0)
							{
								FDutySpec Spec;
								Spec.Subject = NarratorId;
								Spec.Kind = EDuty::ClickOffice;
								Spec.Clicks = CheapestClicks(W);
								Spec.Window = W.Tuning().MiniWindow;
								Spec.bLethal = true;
								ThreatDuty = W.OpenDuty(Spec);
								if (ThreatDuty < 0 && Now - Next > W.Tuning().DeferCap)
								{
									Next = Now + 8.0;
									W.NoteGaveUp(Id);
								}
							}
							return;
						}
						Next = Now + 8.0;   // owner: once per 8 seconds
						FDutySpec Spec;
						Spec.Subject = NarratorId;
						Spec.Kind = EDuty::ClickOffice;
						Spec.Clicks = W.RandInt(3, 5);   // owner: around 3-5 clicks
						Spec.Window = Never;
						Spec.bLethal = false;
						const int Duty = W.OpenDuty(Spec);
						if (Duty >= 0)
						{
							Minis.push_back(Duty);
							W.AddMini();
							++Sent;
							W.Emit(EEvent::MiniCamera, W.MiniCamerasInOffice());
						}
					}

					void OnClick(FUltimateNight& W, int DutyId) override
					{
						if (std::find(Minis.begin(), Minis.end(), DutyId) == Minis.end())
						{
							return;
						}
						FDuty* Duty = W.MutableDuty(DutyId);
						if (Duty != nullptr && --Duty->Clicks <= 0)
						{
							W.CloseDuty(DutyId);
							W.RemoveMini();
							W.Emit(EEvent::Resolved, NarratorId, static_cast<int>(EDuty::ClickOffice));
							Minis.erase(std::find(Minis.begin(), Minis.end(), DutyId));
							SyncThreat(W);
						}
					}

					bool CycleDone() const override { return Sent >= 3 && Minis.empty(); }   // owner: as Narrator he does 3 cameras
					bool IsIdle() const override { return Minis.empty() && ThreatDuty < 0 && DianoDuty < 0; }

				private:
					void UpdateDiano(FUltimateNight& W, double Now)
					{
						if (DianoDuty >= 0)
						{
							const FDuty* Duty = W.FindDuty(DianoDuty);
							if (Duty == nullptr)
							{
								DianoDuty = -1;
							}
							else if (Now >= Duty->Deadline)
							{
								const int Id0 = DianoDuty;
								DianoDuty = -1;
								if (W.IsClosed(DianoSide))
								{
									W.Emit(EEvent::Knock, NarratorId, DianoSide);
									W.Emit(EEvent::Resolved, NarratorId, static_cast<int>(EDuty::CloseDoor));
									W.CloseDuty(Id0);
								}
								else
								{
									static const char* Where[3] = {"the left door", "the right door", "the front window"};
									W.Kill(NarratorId, std::string("Diano got in through ") + Where[DianoSide] + ".", Id0);
								}
							}
							return;
						}
						if (!bDianoPending)
						{
							return;
						}
						// The scheduler places the attack: a side that cannot be shut (broken, Kuu, Blood's window) is skipped.
						for (int Step = 0; Step < 3; ++Step)
						{
							const int Side = (DianoFirst + Step) % 3;
							if (W.IsDoorBlocked(Side))
							{
								continue;
							}
							FDutySpec Spec;
							Spec.Subject = NarratorId;
							Spec.Kind = EDuty::CloseDoor;
							Spec.Side = Side;
							Spec.Window = W.Tuning().DianoWindow;   // owner: 1 second
							Spec.bLethal = true;
							const int Duty = W.OpenDuty(Spec);
							if (Duty >= 0)
							{
								DianoDuty = Duty;
								DianoSide = Side;
								bDianoPending = false;
								W.Emit(EEvent::Diano, Side);
								return;
							}
						}
						if (Now - DianoSince > W.Tuning().DeferCap)
						{
							bDianoPending = false;   // bounded: this camera-down's attack is skipped, never forced
							W.NoteGaveUp(Id);
						}
					}

					bool bDianoPending = false;
					double DianoSince = 0.0;
					int DianoFirst = 0;
					int DianoSide = 0;
					int DianoDuty = -1;

					int CheapestClicks(FUltimateNight& W) const
					{
						int Best = 5;
						for (const int Id0 : Minis)
						{
							if (const FDuty* Duty = W.FindDuty(Id0))
							{
								Best = std::min(Best, Duty->Clicks);
							}
						}
						return std::max(1, Best);
					}

					// The lethal duty only exists while two cameras are in the room; clearing one resolves it.
					void SyncThreat(FUltimateNight& W)
					{
						if (ThreatDuty >= 0 && W.MiniCamerasInOffice() < 2)
						{
							W.CloseDuty(ThreatDuty);
							ThreatDuty = -1;
						}
						else if (FDuty* Threat = W.MutableDuty(ThreatDuty))
						{
							Threat->Clicks = CheapestClicks(W);
						}
					}

					std::vector<int> Minis;
					double Next = -1.0;
					int Sent = 0;
					int ThreatDuty = -1;
				};

				// =========================================================================================== 016 Jester
				// Every 10-20 s: an advertisement, a 1 s air horn, and a forced door or camera move. A forced move can never be
				// lethal: it is only chosen when no lethal duty depends on what it would undo.
				class FJester : public FSubject
				{
				public:
					explicit FJester(bool bInAdapted) : FSubject(JesterId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						const double Now = W.Time();
						Held += Dt;
						if (Next < 0.0)
						{
							Next = bAdapted ? Now + 0.5 : std::max(Now, NightGraceSeconds) + W.Uniform(10.0, 20.0);
						}
						SyncAd(W);
						if (bRetired || Now < Next)
						{
							return;
						}
						if (W.StargazerReserved() || W.AngelWaiting())
						{
							return;   // not now: his flash window and a pending pull are never disturbed
						}
						Next = Now + W.Uniform(10.0, 20.0);   // owner: 10-20 s, the same interval for all three
						++Rounds;
						if (!W.IsAdOpen())
						{
							W.SetAdOpen(true);
						}
						W.SetHornUntil(Now + 1.0);
						W.Emit(EEvent::Horn);
						// 0..1 close a door, 2 camera up, 3..4 open a door, 5 camera down ("or vice versa")
						for (int Try = 0; Try < 6; ++Try)
						{
							const int Pick = W.RandInt(0, 5);
							if (TryForce(W, Pick))
							{
								break;
							}
						}
					}

					bool CycleDone() const override { return Rounds >= 1 && Held >= 20.0; }   // owner: a whole 20 s interval of all 3 attacks
					bool IsIdle() const override { return true; }

				private:
					bool TryForce(FUltimateNight& W, int Pick)
					{
						switch (Pick)
						{
						case 0:
						case 1:
						{
							const int Side = Pick;
							if (W.IsDoorBlocked(Side) || W.IsDoorClosed(Side == 0 ? ESide::Left : ESide::Right))
							{
								return false;
							}
							W.ForceDoor(Side == 0 ? ESide::Left : ESide::Right, true);
							return true;
						}
						case 3:
						case 4:
						{
							const int Side = Pick - 3;
							if (!W.IsDoorClosed(Side == 0 ? ESide::Left : ESide::Right) || W.HasOpenLethalOnSide(Side))
							{
								return false;   // never open a door a lethal duty needs shut
							}
							W.ForceDoor(Side == 0 ? ESide::Left : ESide::Right, false);
							return true;
						}
						case 2:
							if (W.IsCameraUp() || W.IsPowerOut())
							{
								return false;
							}
							W.ForceCamera(true);
							return true;
						default:
							if (!W.IsCameraUp())
							{
								return false;
							}
							W.ForceCamera(false);
							return true;
						}
					}

					void SyncAd(FUltimateNight& W)
					{
						if (AdDuty >= 0 && !W.IsAdOpen())
						{
							W.CloseDuty(AdDuty);
							AdDuty = -1;
						}
						if (AdDuty < 0 && W.IsAdOpen())
						{
							FDutySpec Spec;
							Spec.Subject = JesterId;
							Spec.Kind = EDuty::CloseAd;
							Spec.Window = Never;
							Spec.bLethal = false;
							AdDuty = W.OpenDuty(Spec);
						}
					}

					double Next = -1.0;
					double Held = 0.0;
					int Rounds = 0;
					int AdDuty = -1;
				};

				// =========================================================================================== 017 Media
				class FMedia : public FSubject
				{
				public:
					explicit FMedia(bool bInAdapted) : FSubject(MediaId, bInAdapted) {}

					void Update(FUltimateNight& W, double) override
					{
						const FMovement M = MovementFor(AiOf(W, *this));
						const double Now = W.Time();
						if (NextOpp < 0.0)
						{
							NextOpp = (bAdapted ? Now : std::max(Now, NightGraceSeconds)) + M.IntervalSeconds * 0.6;
						}
						if (DutyId >= 0)
						{
							const FDuty* Duty = W.FindDuty(DutyId);
							if (Duty == nullptr)
							{
								DutyId = -1;
							}
							else if (Now >= Duty->Deadline)
							{
								const int Id0 = DutyId;
								DutyId = -1;
								if (W.IsDoorClosed(Side == 0 ? ESide::Left : ESide::Right))
								{
									W.Emit(EEvent::Knock, MediaId, Side);
									W.Emit(EEvent::Resolved, MediaId, static_cast<int>(EDuty::CloseDoor));
									W.CloseDuty(Id0);
									bTeleported = true;
								}
								else
								{
									W.Kill(MediaId, std::string("Media got in through the ") + (Side == 0 ? "left" : "right") + " door.", Id0);
								}
							}
							return;
						}
						if (!bAdapted && Now < NightGraceSeconds)
						{
							return;
						}
						if (!bWant && !bRetired && Now >= NextOpp)
						{
							NextOpp = std::max(NextOpp + M.IntervalSeconds, Now + 0.01);
							if (W.Roll(M.Chance) && ++Progress >= (bAdapted ? 1 : W.Tuning().MediaCharge))
							{
								bWant = true;
								WantSince = Now;
								Progress = 0;
							}
						}
						if (bWant)
						{
							int Pick = W.RandInt(0, 1);
							if (W.IsDoorBlocked(Pick))
							{
								Pick = 1 - Pick;   // she appears at a door that can close
							}
							FDutySpec Spec;
							Spec.Subject = MediaId;
							Spec.Kind = EDuty::CloseDoor;
							Spec.Side = Pick;
							Spec.Window = W.Tuning().MediaWindow;   // owner: 1 second
							Spec.bLethal = true;
							DutyId = W.OpenDuty(Spec);   // the scheduler refuses a blocked door and an infeasible overlap
							if (DutyId >= 0)
							{
								bWant = false;
								Side = Pick;
								W.Emit(EEvent::Moved, MediaId, static_cast<int>(FUltimateNight::DoorRoom(Side)));
							}
							else if (Now - WantSince > W.Tuning().DeferCap)
							{
								bWant = false;
								W.NoteGaveUp(Id);
							}
						}
					}

					bool CycleDone() const override { return bTeleported && DutyId < 0; }
					bool IsIdle() const override { return DutyId < 0 && !bWant; }

				private:
					double NextOpp = -1.0;
					double WantSince = 0.0;
					int Progress = 0;
					bool bWant = false;
					bool bTeleported = false;
					int Side = 0;
					int DutyId = -1;
				};

				// ===================================================================================== 018 Time Keeper
				class FTimeKeeper : public FSubject
				{
				public:
					explicit FTimeKeeper(bool bInAdapted) : FSubject(TimeKeeperId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						// Timers count down in real seconds so sending the night's clock back never pauses her.
						SinceLast += Dt;
						if (!bStarted)
						{
							bStarted = true;
							SinceLast = bAdapted ? 8.0 : 8.0 - std::max(0.0, NightGraceSeconds - W.Time());
						}
						for (size_t Index = 0; Index < Seen.size();)
						{
							FAppearance& A = Seen[Index];
							A.Left -= Dt;
							bool bGone = false;
							if (W.IsViewing(A.Cam) || W.IsCameraDisabled(A.Cam))
							{
								W.Emit(EEvent::TimeKeeper, A.Cam, 1);   // found
								++Found;
								bGone = true;
							}
							else if (A.Left <= 0.0)
							{
								W.SendBackAnHour();   // owner: if not found she sends you back an hour
								++Found;
								bGone = true;
							}
							if (bGone)
							{
								W.CloseDuty(A.Duty);
								Seen.erase(Seen.begin() + static_cast<std::ptrdiff_t>(Index));
							}
							else
							{
								if (FDuty* Duty = W.MutableDuty(A.Duty))
								{
									Duty->Deadline = W.Time() + A.Left;
								}
								++Index;
							}
						}
						if (bRetired || SinceLast < 8.0 || (bAdapted && Appeared >= 1))
						{
							return;
						}
						if (!bAdapted && W.Time() < NightGraceSeconds)
						{
							return;
						}
						const int Cam = W.PickCamera(false);
						if (Cam < 0)
						{
							return;
						}
						SinceLast = 0.0;   // owner: every 8 seconds
						FDutySpec Spec;
						Spec.Subject = TimeKeeperId;
						Spec.Kind = EDuty::FindTimeKeeper;
						Spec.Cam = Cam;
						Spec.Window = 10.0;   // owner: found within 10 seconds
						Spec.bLethal = false;
						Spec.bLocated = false;
						FAppearance A;
						A.Duty = W.OpenDuty(Spec);
						A.Cam = Cam;
						A.Left = 10.0;
						Seen.push_back(A);
						++Appeared;
						W.Emit(EEvent::TimeKeeper, Cam, 0);
					}

					bool IsOnCamera(int Cam) const override
					{
						for (const FAppearance& A : Seen)
						{
							if (A.Cam == Cam)
							{
								return true;
							}
						}
						return false;
					}

					bool CycleDone() const override { return bAdapted && Appeared >= 1 && Seen.empty(); }
					bool IsIdle() const override { return Seen.empty(); }

				private:
					struct FAppearance
					{
						int Duty = -1;
						int Cam = 0;
						double Left = 0.0;
					};

					std::vector<FAppearance> Seen;
					double SinceLast = 0.0;
					bool bStarted = false;
					int Appeared = 0;
					int Found = 0;
				};

				// ============================================================================================ 000 Angel
				// Owner: appears in the office on every other camera pull-up; pull the cameras up again to remove her; 1.5 s timer.
				// OPEN: "does 2 simple attacks" - only this one is described.
				class FAngel : public FSubject
				{
				public:
					explicit FAngel(bool bInAdapted) : FSubject(AngelId, bInAdapted) {}

					void OnCameraUp(FUltimateNight& W, bool) override
					{
						if (bWaiting)
						{
							bWaiting = false;   // pulled up again before she arrived: nothing to remove
							W.SetPendingAngel(false);
						}
						if (bPresent)
						{
							W.Emit(EEvent::Resolved, AngelId, static_cast<int>(EDuty::PullCamera));
							W.CloseDuty(DutyId);
							DutyId = -1;
							bPresent = false;
							++Removed;
						}
					}

					void OnCameraDown(FUltimateNight& W, bool bForced) override
					{
						if (!bForced && !bRetired && !bPresent && !bWaiting && W.PullCount() > 0 && W.PullCount() % 2 == 0)
						{
							bWaiting = true;   // her arrival is queued behind anything already due: the scheduler decides
							W.SetPendingAngel(true);
							WaitingSince = W.Time();
						}
					}

					void Update(FUltimateNight& W, double) override
					{
						const double Now = W.Time();
						if (bWaiting)
						{
							FDutySpec Spec;
							Spec.Subject = AngelId;
							Spec.Kind = EDuty::PullCamera;
							Spec.Window = W.Tuning().AngelWindow;   // owner: 1.5-second kill timer
							Spec.bLethal = true;
							DutyId = W.OpenDuty(Spec);
							if (DutyId >= 0)
							{
								bWaiting = false;
								bPresent = true;
								W.SetPendingAngel(false);
							}
						}
						if (bPresent)
						{
							const FDuty* Duty = W.FindDuty(DutyId);
							if (Duty != nullptr && Now >= Duty->Deadline)
							{
								W.Kill(AngelId, "Angel was in your office and you did not pull the cameras up.", DutyId);
							}
						}
					}

					bool CycleDone() const override { return Removed >= 1 && !bPresent; }
					bool IsIdle() const override { return !bPresent && !bWaiting; }

				private:
					bool bWaiting = false;
					bool bPresent = false;
					double WaitingSince = 0.0;
					int Removed = 0;
					int DutyId = -1;
				};

				// ============================================================================================= 020 Adapt
				// Copies any of the other 20 subjects and does their associated attack for one cycle, then switches again.
				class FAdapt : public FSubject
				{
				public:
					explicit FAdapt(bool bInAdapted) : FSubject(AdaptId, bInAdapted) {}

					void Update(FUltimateNight& W, double Dt) override
					{
						Held += Dt;
						if (bRetired)
						{
							Drop();
						}
						else if (!Active || Active->CycleDone() || Held >= W.Tuning().AdaptCycleCap)
						{
							Switch(W);
						}
						if (Active)
						{
							Active->Update(W, Dt);
						}
						for (size_t Index = 0; Index < Lingering.size() && !W.IsOver();)
						{
							Lingering[Index]->Update(W, Dt);
							if (Lingering[Index]->IsIdle())
							{
								Lingering.erase(Lingering.begin() + static_cast<std::ptrdiff_t>(Index));
							}
							else
							{
								++Index;
							}
						}
					}

					void OnCameraUp(FUltimateNight& W, bool bForced) override
					{
						if (Active) { Active->OnCameraUp(W, bForced); }
						for (auto& Copy : Lingering) { Copy->OnCameraUp(W, bForced); }
					}
					void OnCameraDown(FUltimateNight& W, bool bForced) override
					{
						if (Active) { Active->OnCameraDown(W, bForced); }
						for (auto& Copy : Lingering) { Copy->OnCameraDown(W, bForced); }
					}
					bool OnShock(FUltimateNight& W, int Cam) override
					{
						bool bTook = false;
						if (Active && Active->NeedsShockAt(Cam)) { bTook = Active->OnShock(W, Cam) || bTook; }
						for (auto& Copy : Lingering)
						{
							if (Copy->NeedsShockAt(Cam)) { bTook = Copy->OnShock(W, Cam) || bTook; }
						}
						return bTook;
					}
					bool NeedsShockAt(int Cam) const override
					{
						if (Active && Active->NeedsShockAt(Cam)) { return true; }
						for (const auto& Copy : Lingering)
						{
							if (Copy->NeedsShockAt(Cam)) { return true; }
						}
						return false;
					}
					bool IsOnCamera(int Cam) const override
					{
						if (Active && Active->IsOnCamera(Cam)) { return true; }
						for (const auto& Copy : Lingering)
						{
							if (Copy->IsOnCamera(Cam)) { return true; }
						}
						return false;
					}
					void CollectOnCamera(int Cam, std::vector<int>& Out) const override
					{
						if (Active) { Active->CollectOnCamera(Cam, Out); }
						for (const auto& Copy : Lingering) { Copy->CollectOnCamera(Cam, Out); }
					}
					void OnClick(FUltimateNight& W, int DutyId) override
					{
						if (Active) { Active->OnClick(W, DutyId); }
						for (auto& Copy : Lingering) { Copy->OnClick(W, DutyId); }
					}
					void OnWindowShut(FUltimateNight& W) override
					{
						if (Active) { Active->OnWindowShut(W); }
						for (auto& Copy : Lingering) { Copy->OnWindowShut(W); }
					}
					bool IsIdle() const override { return !Active && Lingering.empty(); }
					int CopyingId() const { return Active ? Active->Id : -1; }

				private:
					void Drop()
					{
						if (Active)
						{
							Active->Retire();
							Lingering.push_back(std::move(Active));
						}
					}

					void Switch(FUltimateNight& W)
					{
						// A finished copy is idle by definition (CycleDone); a copy cut by the cap lingers until its duties are gone,
						// but starts nothing new.
						Drop();
						const int Pick = W.RandInt(0, SubjectCount - 2);   // 0..19: every subject but itself
						Active = MakeSubject(Pick, true);
						Held = 0.0;
					}

					std::unique_ptr<FSubject> Active;
					std::vector<std::unique_ptr<FSubject>> Lingering;
					double Held = 0.0;
				};
			}

			std::unique_ptr<FSubject> MakeSubject(int Id, bool bAdapted)
			{
				switch (Id)
				{
				case AngelId: return std::make_unique<FAngel>(bAdapted);
				case LunaId: return std::make_unique<FLuna>(bAdapted);
				case PlackId: return std::make_unique<FPlack>(bAdapted);
				case KuuId: return std::make_unique<FKuu>(bAdapted);
				case IshiiId: return std::make_unique<FIshii>(bAdapted);
				case AritaId: return std::make_unique<FArita>(bAdapted);
				case FujitaId: return std::make_unique<FFujita>(bAdapted);
				case ShiraishiId: return std::make_unique<FShiraishi>(bAdapted);
				case MyrciaId: return std::make_unique<FMyrcia>(bAdapted);
				case AlchemistId: return std::make_unique<FAlchemist>(bAdapted);
				case StargazerId: return std::make_unique<FStargazer>(bAdapted);
				case GamblerId: return std::make_unique<FGambler>(bAdapted);
				case JudgeId: return std::make_unique<FJudge>(bAdapted);
				case NarratorId: return std::make_unique<FNarrator>(bAdapted);
				case JesterId: return std::make_unique<FJester>(bAdapted);
				case MediaId: return std::make_unique<FMedia>(bAdapted);
				case TimeKeeperId: return std::make_unique<FTimeKeeper>(bAdapted);
				case DeathId: return std::make_unique<FDeath>(bAdapted);
				case AdaptId: return std::make_unique<FAdapt>(bAdapted);
				case BloodId: return std::make_unique<FBlood>(bAdapted);
				case WeatherId: return std::make_unique<FWeather>(bAdapted);
				default: break;
				}
				return std::make_unique<FStub>(Id, bAdapted);
			}
		}
	}
}
