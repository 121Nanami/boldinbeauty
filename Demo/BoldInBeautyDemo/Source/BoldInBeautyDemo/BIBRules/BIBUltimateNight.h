// Boldinbeauty demo — Ultimate Custom Night (owner request 2026-10-14): the 20 + 1 subject arcade night (000-020), four
// modes, built ON TOP of Night Shift (BIBArcade.h): the same 10 cameras, rooms, 5-minute night, 12 AM-6 AM hours, power drain
// and door repair idea. Engine-independent and unit-tested headless (Tests/ucn_tests.cpp); the overlay (BIBGame) only draws it.
//
// Layout of the code:
//   BIBUltimateNight.*  the world: tools (2 doors, camera system with a very limited shock per camera), power, clock, the
//                       roster table, the CENTRAL THREAT SCHEDULER and the reward / save side.
//   BIBUcnSubjects.cpp  one behaviour module per subject (000-020), each a class of its own; Adapt (020) hosts copies.
//   BIBUcnBots.*        perfect / budget / human-model / do-nothing bots and the beatability suite.
//
// Fairness contract (owner request: "incredibly hard, never impossible"):
//   1. Every lethal threat is opened through the scheduler, which records a telegraph (audio + visual cue) and a response
//      window of at least the stated time (FUcnStats checks it).
//   2. The scheduler runs an earliest-deadline-first feasibility test over every unsatisfied lethal duty, charging each one a
//      reaction budget plus the UI travel / click time of its response; a new lethal threat that would make the set
//      infeasible is DELAYED (bounded retries), never forced.
//   3. Shocks, power and vaccine spawns are verified numerically by the beatability suite.
// Every number the owner did not state is a PLACEHOLDER in FUcnTuning, the single place to tune fairness.
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "BIBArcade.h"

namespace BIB
{
	class FProfile;
	struct FActionResult;

	namespace Arcade
	{
		namespace Ucn
		{
			// ========================================================================= modes and the roster table
			enum class EMode : int
			{
				Easy = 0,    // AI 15, subjects 001-005
				Medium,      // AI 15, subjects 001-010
				Hard,        // AI 10, subjects 001-015 plus 000
				Angel,       // AI 20, all of 000-020
				Count
			};
			constexpr int ModeCount = static_cast<int>(EMode::Count);
			const char* ModeName(EMode Mode);

			constexpr int SubjectCount = 21;   // 000..020
			constexpr int RouteCameras = 5;    // owner: Luna / Plack go "in a straight line across 5 cameras" to a door
			constexpr int AngelId = 0;
			constexpr int LunaId = 1;
			constexpr int PlackId = 2;
			constexpr int KuuId = 3;
			constexpr int IshiiId = 4;
			constexpr int AritaId = 5;
			constexpr int FujitaId = 6;
			constexpr int ShiraishiId = 7;
			constexpr int MyrciaId = 8;
			constexpr int AlchemistId = 9;
			constexpr int BloodId = 10;
			constexpr int StargazerId = 11;
			constexpr int GamblerId = 12;
			constexpr int WeatherId = 13;
			constexpr int JudgeId = 14;
			constexpr int NarratorId = 15;
			constexpr int JesterId = 16;
			constexpr int MediaId = 17;
			constexpr int TimeKeeperId = 18;
			constexpr int DeathId = 19;
			constexpr int AdaptId = 20;

			constexpr int WindowSide = 2;      // owner 2026-10-14: the front window is the third barrier (0 left door, 1 right door)
			constexpr int WindowCamera = 2;    // owner: CAM 3 is the view outside the front window

			// When a subject of the mode becomes active.
			enum class EArrival : int
			{
				Start = 0,         // from 12 AM
				PlackSacrifice     // owner 2026-10-14: Death (019) is summoned by Plack's 5 AM sacrifice in every mode
			};

			// The ONE roster table: subject id, name, mode membership and AI level per mode (0 = not in that mode), arrival.
			struct FSubjectInfo
			{
				int Id;
				const char* Name;
				int Ai[ModeCount];   // Easy, Medium, Hard, Angel
				EArrival Arrival;
			};
			const FSubjectInfo& SubjectInfo(int Id);
			int AiLevel(int Id, EMode Mode);
			bool InMode(int Id, EMode Mode);
			std::vector<int> Roster(EMode Mode);   // ascending ids

			// AI level N out of 20 -> how often and how surely the movement-based subjects act. The ONE place this is defined.
			// Chosen so AI 10 / 15 / 20 reproduces Night Shift's Easy 7 s x 50%, Medium 6 s x 75%, Hard 5 s x 100%.
			struct FMovement
			{
				double IntervalSeconds = 7.0;
				double Chance = 0.5;
			};
			FMovement MovementFor(int Ai);

			// ========================================================================= tuning (all PLACEHOLDER unless noted)
			struct FUcnTuning
			{
				double ReactionBudget = 0.35;      // owner-given default of the scheduler; configurable
				double ActionSeconds = 0.25;       // PLACEHOLDER: one UI action incl. travel (door, camera key, camera select)
				double ClickSeconds = 0.20;        // PLACEHOLDER: one extra click on an office target
				double DoorWindowMin = 2.0;        // PLACEHOLDER: warning at a door before a walker may strike
				double DoorRepairSeconds = 20.0;   // PLACEHOLDER: Night Shift Medium value
				int ShocksPerCamera = 5;           // PLACEHOLDER: "a very limited shock per camera" (tuned 4 -> 5: 4 left the perfect bot 0 spare in Angel)
				int ShockReserve = 2;              // PLACEHOLDER: charges only the lethal Stargazer flash may use
				double VaccineRespawn = 12.0;      // PLACEHOLDER: a new bottle appears this long after one is taken
				double JudgeGrace = 0.6;           // PLACEHOLDER: a late honest action just after a threat ended is not a misplay
				double DeathRushWindow = 1.5;      // PLACEHOLDER (owner gave none): same as one Myrcia rush
				double DeathOfficeEvery = 25.0;    // PLACEHOLDER: how often Death can appear in the office
				double DeathOfficeWindow = 2.0;    // owner: 4 clicks in 2 s
				int DeathOfficeClicks = 4;         // owner
				double AngelWindow = 1.5;          // owner: 1.5 s kill timer
				double MediaWindow = 1.0;          // owner: 1 s
				double MyrciaWindow = 1.5;         // owner: 1.5 s per rush
				double FujitaWindow = 5.0;         // owner: 5 s kill timer
				double KuuWindup = 4.0;            // owner: 4 s charge noise
				int MediaCharge = 6;               // PLACEHOLDER: Night Shift's value
				double StargazerWarn = 3.0;        // PLACEHOLDER: audio / visual warning before the route starts
				double StargazerRush = 1.0;        // PLACEHOLDER: his rush at the door after the route
				int ShiraishiStepsToDoor = 12;     // PLACEHOLDER: owner gave no route length (OPEN); tuned 10 -> 12 (10: perfect bot 85% in Angel)
				double ShiraishiBurn = 4.0;        // PLACEHOLDER: "burns it, then kills you"
				double PowerOutDeath = 10.0;       // PLACEHOLDER: seconds from power out until the dark kills
				double PowerDrainScale = 0.6;      // PLACEHOLDER: multiplier on Night Shift's drain; tuned 1.0 -> 0.6 (Shiraishi needs the camera up most of the night: at 1.0 the perfect bot ran out of power in Medium+)
				double AdaptCycleCap = 60.0;       // PLACEHOLDER: a copied behaviour never lasts longer
				double PassiveCycle = 20.0;        // PLACEHOLDER: how long Adapt keeps a passive copy (Judge, Alchemist...)
				double DeferCap = 12.0;            // a subject that cannot be scheduled this long skips that attack
				double MiniWindow = 4.0;           // PLACEHOLDER: warning before a third mini camera would enter
				// Owner 2026-10-14: the front window, Diano, Blood and Weather.
				double DianoWindow = 1.0;          // owner: 1 second to react
				double BloodEveryMin = 20.0;       // owner: Blood appears every 20-30 seconds
				double BloodEveryMax = 30.0;
				double BloodPhaseMin = 2.0;        // owner: 4 phases that take between 2-10 seconds (read: each phase)
				double BloodPhaseMax = 10.0;
				double WeatherEveryMin = 25.0;     // PLACEHOLDER: owner gave no frequency for Weather
				double WeatherEveryMax = 40.0;
				double WeatherWindow = 5.0;        // PLACEHOLDER: owner gave no kill timer for Weather
				bool bWindowUsesPower = true;      // PLACEHOLDER: a shut window costs a power bar like a door
			};

			// ========================================================================= events
			enum class EEvent : int
			{
				Telegraph = 0,   // a lethal threat opened (A = subject, B = duty kind, C = side / camera, D = window ms)
				Resolved,        // a threat was answered (A = subject, B = duty kind)
				Moved,           // a subject changed room (A = subject, B = room)
				DoorRepaired,    // (A = side)
				DoorBroken,      // (A = side)
				DoorForced,      // Jester moved a door (A = side, B = 1 closed)
				CameraForced,    // Jester moved the camera (A = 1 up)
				Knock,           // a subject hit a closed door and left (A = subject, B = side)
				KuuWindup,       // (A = side)
				KuuPunch,        // (A = side, B = 1 broke the door)
				Infection,       // Ishii / Alchemist infection started (A = camera)
				CameraLost,      // a camera was permanently disabled (A = camera)
				Vaccine,         // the vaccine was taken / armed (A = camera) or used (B = 1)
				CoinFlip,        // Gambler (A = 1 heads: find her again, 0 tails: she leaves)
				Stargazer,       // A = 0 stirring warning, 1 stop on camera B, 2 rush: smashing the front window (B = 2, C = from CAM index), 3 sent back
				HourLost,        // Time Keeper sent you back an hour
				TimeKeeper,      // appeared (A = camera) or found (B = 1)
				MiniCamera,      // Narrator mini camera entered (A = number now in the office)
				Ad,              // Jester advertisement opened (A = 1) or closed (A = 0)
				Horn,            // Jester air horn (1 s)
				Judge,           // false alarm kill (A = 0 door, 1 shock, 2 window)
				Sacrifice,       // 5 AM: Plack's sacrifice (A = 1 when Death was disabled)
				Hour,            // (A = hour 0..6)
				PowerOut,
				Jumpscare,       // lost (A = subject or -1)
				Survived,
				Window,          // the front window moved (A = 1 shut)
				BloodPhase,      // A = phase 1..4, 0 = she left
				Weather,         // Weather at the window: A = the operator he looks like, B = aura (0 neon yellow, 1 light blue); A = -1 he left
				Diano,           // Diano attacks: A = 0 left, 1 right, 2 the middle window
				Summoned         // Death arrives (A = subject)
			};

			struct FEvent
			{
				EEvent Kind = EEvent::Hour;
				int A = 0;
				int B = 0;
				int C = 0;
				int D = 0;
				double Time = 0.0;
			};

			// ========================================================================= duties (what the player can answer)
			enum class EDuty : int
			{
				CloseDoor = 0,    // lethal: be shut at the deadline (Side: 0 left door, 1 right door, 2 the front window)
				OpenDoor,         // non-lethal: Kuu's punch breaks a shut door (Side)
				ClickOffice,      // lethal: click an object in the office Clicks more times (Death, mini cameras, Angel uses PullCamera)
				PullCamera,       // lethal: raise the camera system (Angel)
				FlashCamera,      // lethal: Stargazer on a camera right now (Cam)
				ShockInfection,   // non-lethal: Ishii infects Cam, shock it before the deadline
				ShockGambler,     // non-lethal: Gambler is on Cam
				FindTimeKeeper,   // non-lethal: look at Cam before the deadline
				TakeVaccine,      // non-lethal: the bottle is on Cam
				CloseAd,          // non-lethal: click the advertisement
				WatchShiraishi,   // pacing: keep the camera on her room (Cam) to slow her
				Count
			};

			struct FDuty
			{
				int Id = 0;
				int Subject = 0;
				EDuty Kind = EDuty::CloseDoor;
				int Side = 0;          // 0 left, 1 right
				int Cam = -1;          // camera index 0..9 (cameras are labelled CAM 1..9, 0)
				int Clicks = 1;        // clicks still needed
				double Opened = 0.0;
				double Deadline = 0.0;
				double Window = 0.0;
				bool bLethal = false;
				bool bLocated = true;  // visible from the office; false = the camera has to be found by looking
				bool bOpen = false;
			};

			// What a subject asks the scheduler for.
			struct FDutySpec
			{
				int Subject = 0;
				EDuty Kind = EDuty::CloseDoor;
				int Side = 0;
				int Cam = -1;
				int Clicks = 1;
				double Window = 1.0;
				bool bLethal = true;
				bool bLocated = true;
			};

			// Counters the fairness tests read.
			struct FUcnStats
			{
				int LethalOpened = 0;
				int UntelegraphedKills = 0;      // a kill with no recorded telegraph: must stay 0
				int ShortWindowKills = 0;        // a kill whose window was below the subject's stated time: must stay 0
				double MinLethalWindow = 1e9;    // shortest response window granted to any lethal duty
				int Deferrals = 0;               // lethal threats the scheduler held back
				int GaveUp = 0;                  // attacks skipped after DeferCap
				int GaveUpBy[SubjectCount] = {}; // ... per subject
				int ShocksSpent = 0;
				int CamerasLost = 0;
				int HoursLost = 0;
				int VaccinesTaken = 0;
				int JudgeKills = 0;
				double MinPower = 100.0;
			};

			class FUltimateNight;

			// ========================================================================= one behaviour module per subject
			class FSubject
			{
			public:
				FSubject(int InId, bool bInAdapted) : Id(InId), bAdapted(bInAdapted) {}
				virtual ~FSubject() = default;
				virtual void Update(FUltimateNight& W, double Dt) = 0;
				// Camera system events (player only; Jester's forced moves are passed with bForced).
				virtual void OnCameraUp(FUltimateNight&, bool /*bForced*/) {}
				virtual void OnCameraDown(FUltimateNight&, bool /*bForced*/) {}
				// A shock was fired at Cam. Return true when this subject needed it (it was consumed).
				virtual bool OnShock(FUltimateNight&, int /*Cam*/) { return false; }
				virtual bool NeedsShockAt(int /*Cam*/) const { return false; }
				virtual bool IsOnCamera(int /*Cam*/) const { return false; }
				// Adds the subject ids this module shows on that camera (Adapt reports the copy it hosts).
				virtual void CollectOnCamera(int Cam, std::vector<int>& Out) const { if (IsOnCamera(Cam)) { Out.push_back(Id); } }
				// One attack finished and nothing of it is left in the office: Adapt may switch to another subject.
				virtual bool CycleDone() const { return true; }
				// Adapt drops this copy: it starts no new attack (a leftover duty is still honoured until idle).
				void Retire() { bRetired = true; }
				virtual bool IsIdle() const { return true; }
				// Shiraishi: camera steps still to go before she reaches the door (fraction included); < 0 for everyone else.
				virtual double StepsToDoor() const { return -1.0; }
				// Death's 5 AM disabling by Plack's sacrifice.
				virtual void Disable() {}
				// The duty was answered by the player (a click, a flash).
				virtual void OnClick(FUltimateNight&, int /*DutyId*/) {}
				// The player shut the front window (Blood: too early is deadly).
				virtual void OnWindowShut(FUltimateNight&) {}
				// Summoned by Plack's sacrifice (EArrival::PlackSacrifice).
				virtual void Activate(FUltimateNight&) {}

				const int Id;
				const bool bAdapted;

			protected:
				bool bRetired = false;
			};

			std::unique_ptr<FSubject> MakeSubject(int Id, bool bAdapted);

			// ========================================================================= the world
			class FUltimateNight
			{
			public:
				FUltimateNight();
				~FUltimateNight();
				FUltimateNight(const FUltimateNight&) = delete;
				FUltimateNight& operator=(const FUltimateNight&) = delete;

				void Start(EMode InMode, uint32_t Seed, const FUcnTuning& InTuning = FUcnTuning());
				// Same, with an explicit roster (unit tests run one subject at a time); AI levels still come from the table for InMode.
				void StartWith(EMode InMode, uint32_t Seed, const std::vector<int>& Subjects, const FUcnTuning& InTuning = FUcnTuning());
				void Tick(double DeltaSeconds);   // steps at most 0.05 s at a time

				// ---- player controls (all return false when they did nothing)
				bool SetDoor(ESide Side, bool bClosed);
				bool ToggleDoor(ESide Side) { return SetDoor(Side, !IsDoorClosed(Side)); }
				bool SetCamera(bool bUp);
				bool ToggleCamera() { return SetCamera(!bCameraUp); }
				bool SetWindow(bool bClosed);          // the front window (owner 2026-10-14)
				bool ToggleWindow() { return SetWindow(!IsWindowClosed()); }
				void SelectCamera(int Cam);
				bool Shock();                          // fire at the selected camera
				bool ClickVaccine();                   // the bottle on the selected camera
				bool ClickOffice(int DutyId);          // a Death / mini camera click
				bool CloseAd();

				// ---- state for the screen and for bots
				EMode GetMode() const { return Mode; }
				const FUcnTuning& Tuning() const { return T; }
				double Time() const { return Clock; }                  // real seconds since the night began (never goes back)
				double NightTime() const { return std::max(0.0, Clock - HourOffset); }   // the night's clock: Time Keeper sends it back
				int Hour() const;
				double Power() const { return PowerLeft; }
				int Usage() const;
				bool IsPowerOut() const { return bPowerOut; }
				bool IsOver() const { return bOver; }
				bool IsWon() const { return bWon; }
				int Killer() const { return KilledBy; }   // subject id, or -1 for the dark / none
				const std::string& LostReason() const { return Reason; }
				bool IsDoorClosed(ESide Side) const { return Doors[SideIdx(Side)].bClosed; }
				double DoorRepairLeft(ESide Side) const { return Doors[SideIdx(Side)].RepairLeft; }
				bool IsWindowClosed() const { return Doors[WindowSide].bClosed; }
				bool IsClosed(int Side) const { return Side >= 0 && Side <= WindowSide && Doors[Side].bClosed; }
				// Blood in phases 1-3: the window must stay open (the scheduler sends no window threat meanwhile).
				bool IsWindowLocked() const { return bWindowLocked; }
				int BloodPhaseNow() const { return BloodPhase; }
				// Weather's disguise at the window (-1 when he is not there) and his aura (0 neon yellow, 1 light blue).
				int WeatherLooksLike() const { return WeatherLook; }
				int WeatherAura() const { return WeatherAuraColour; }
				bool IsCameraUp() const { return bCameraUp; }
				int SelectedCamera() const { return Camera; }
				bool IsCameraDisabled(int Cam) const { return InCameras(Cam) && Cams[Cam].bDisabled; }
				bool IsCameraInfected(int Cam) const { return InCameras(Cam) && Cams[Cam].InfectionLeft > 0.0; }
				double InfectionLeft(int Cam) const { return InCameras(Cam) ? Cams[Cam].InfectionLeft : 0.0; }
				int ShocksLeft(int Cam) const { return InCameras(Cam) ? Cams[Cam].Shocks : 0; }
				int ShocksTotal() const;
				int CamerasWorking() const;
				bool IsAdOpen() const { return bAdOpen; }
				int PullCount() const { return Pulls; }
				int VaccineCamera() const { return VaccineCam; }   // -1 = no bottle now
				bool IsVaccineArmed() const { return bVaccineArmed; }
				int MiniCamerasInOffice() const { return Minis; }
				bool IsHorn() const { return Clock < HornUntil; }
				const std::vector<FDuty>& Duties() const { return DutyList; }
				const FDuty* FindDuty(int DutyId) const;
				std::vector<FEvent> PopEvents();
				const FUcnStats& Stats() const { return StatsData; }
				// Who the camera of that room shows right now (ignores whether the camera is up; Alchemist sits on CAM 6).
				std::vector<int> SubjectsOnCamera(int Cam) const;
				bool HasSubject(int Id) const;
				const FSubject* Module(int Id) const;
				FSubject* MutableModule(int Id);
				void NoteGaveUp(int Subject) { ++StatsData.GaveUp; if (Subject >= 0 && Subject < SubjectCount) { ++StatsData.GaveUpBy[Subject]; } }
				// Judge's false alarm cue: true when closing that door / firing the selected camera would be a misplay.
				bool IsDoorFalseAlarm(ESide Side) const;
				bool IsShockFalseAlarm() const;
				bool IsWindowFalseAlarm() const;
				bool JudgeActive() const;

				// ---- the central scheduler
				// True when a lethal duty like this could open right now and the player would still have a feasible response
				// sequence for every open duty (earliest-deadline-first, reaction budget + UI travel per response).
				bool CanOpenLethal(const FDutySpec& Spec) const;
				// Opens a duty: lethal ones only if CanOpenLethal; returns the duty id or -1 when deferred.
				int OpenDuty(const FDutySpec& Spec);
				void CloseDuty(int DutyId);
				FDuty* MutableDuty(int DutyId);
				// Work (seconds of player time) one response to a duty needs.
				double WorkFor(EDuty Kind, int Clicks) const;
				// Would the open lethal duties still be feasible if the player also had to answer Extra (a hypothetical Angel)?
				bool FeasibleWith(EDuty Kind, int Clicks, double Window, int Side) const;
				// Longest time until any open lethal duty is due (for bots: nothing urgent when large).
				double NextLethalDeadline() const;
				bool HasOpenLethalOnSide(int Side) const;
				bool IsDoorBlocked(int Side) const;     // broken, or Kuu is using it

				// ---- services for the subject modules
				std::mt19937& Rng() { return RngState; }
				double Uniform(double Lo, double Hi);
				int RandInt(int Lo, int Hi);   // inclusive
				bool Roll(double Chance);
				int AiFor(int SubjectId) const { return AiLevel(SubjectId, Mode); }
				void Emit(EEvent Kind, int A = 0, int B = 0, int C = 0, int D = 0);
				// bPacing: a kill that is not a missed duty (Shiraishi arriving, the dark, Judge's false alarm): its telegraph is a state, not a duty.
				void Kill(int Subject, const std::string& Why, int DutyId = -1, bool bPacing = false);
				void ForceDoor(ESide Side, bool bClosed);       // Jester
				void ForceCamera(bool bUp);                     // Jester
				void BreakDoor(ESide Side);                     // Kuu's punch
				// Kuu at a doorway is read from her open duties, so the real Kuu and Adapt's copy can never clear each other.
				int KuuSideInUse() const;
				// Stargazer's flash window is exclusive. A counter, so a copy hosted by Adapt cannot cancel the real one's hold.
				void HoldStargazerReserve(bool bOn) { StargazerHolds = std::max(0, StargazerHolds + (bOn ? 1 : -1)); }
				bool StargazerReserved() const { return StargazerHolds > 0; }
				void SetCameraDisabled(int Cam);
				void SetInfection(int Cam, double SecondsLeft) { if (InCameras(Cam)) { Cams[Cam].InfectionLeft = SecondsLeft; } }
				void MarkJudging() { bJudgeMark = true; }
				// Picks a working camera; bNeedShock restricts to cameras with charges (and respects the shock reserve).
				int PickCamera(bool bNeedShock, int Avoid = -1);
				bool ShocksAvailableForNonLethal() const;       // above the reserve
				bool IsViewing(int Cam) const { return bCameraUp && !bAdOpen && Camera == Cam; }
				void SetAdOpen(bool bOpen);
				void AddMini() { ++Minis; }
				void RemoveMini() { if (Minis > 0) { --Minis; } }
				void SetVaccine(int Cam) { VaccineCam = Cam; }
				void ArmVaccine(bool bArmed) { bVaccineArmed = bArmed; }
				void SendBackAnHour();
				void SetHornUntil(double T0) { HornUntil = T0; }
				bool InfectionBlockedByVaccine();               // consumes an armed vaccine when it blocks an infection
				void NoteThreatSeen(int Side) { if (Side >= 0 && Side <= WindowSide) { LastDoorThreat[Side] = Clock; } }
				void SetWindowLocked(bool bOn) { bWindowLocked = bOn; }
				void SetBloodPhase(int Phase) { BloodPhase = Phase; }
				void SetWeatherLook(int Id, int Aura) { WeatherLook = Id; WeatherAuraColour = Aura; }
				void NoteShockNeed(int Cam) { if (InCameras(Cam)) { LastShockNeed[Cam] = Clock; } }
				void NotePlayerPull() { ++Pulls; }
				void SetPendingAngel(bool bOn) { bAngelWaiting = bOn; }
				bool AngelWaiting() const { return bAngelWaiting; }
				// A new walker may not hold lethal duties while another is still allowed to start: see CanOpenLethal.
				static ERoom DoorRoom(int Side) { return Side == 0 ? ERoom::LeftDoor : (Side == 1 ? ERoom::RightDoor : ERoom::Inside); }
				static int SideIdx(ESide Side) { return Side == ESide::Left ? 0 : 1; }
				static bool InCameras(int Cam) { return Cam >= 0 && Cam < CameraCount; }

			private:
				struct FDoorState
				{
					bool bClosed = false;
					double RepairLeft = 0.0;
				};
				struct FCamState
				{
					bool bDisabled = false;
					int Shocks = 0;
					double InfectionLeft = 0.0;
				};
				struct FJob
				{
					double Deadline = 0.0;
					double Work = 0.0;
					int Key = 0;
				};

				void Step(double Dt);
				std::vector<FJob> CollectJobs(const FDuty* Extra, int ExtraId) const;
				static bool Feasible(std::vector<FJob>& Jobs, double Now);
				bool DutySatisfied(const FDuty& Duty) const;
				void NoteDoorClosed(int Side);

				EMode Mode = EMode::Easy;
				FUcnTuning T;
				std::mt19937 RngState;
				double Clock = 0.0;
				double PowerLeft = 100.0;
				bool bPowerOut = false;
				double PowerOutLeft = 0.0;
				FDoorState Doors[WindowSide + 1];   // left door, right door, front window
				bool bCameraUp = false;
				int Camera = 0;
				FCamState Cams[CameraCount];
				int Pulls = 0;
				bool bAdOpen = false;
				int VaccineCam = -1;
				bool bVaccineArmed = false;
				int Minis = 0;
				double HornUntil = -1.0;
				bool bOver = false;
				bool bWon = false;
				int KilledBy = -1;
				std::string Reason;
				int LastHour = 0;
				int StargazerHolds = 0;
				double HourOffset = 0.0;
				bool bAngelWaiting = false;
				double LastDoorThreat[WindowSide + 1] = {-100.0, -100.0, -100.0};
				bool bWindowLocked = false;
				int BloodPhase = 0;
				int WeatherLook = -1;
				int WeatherAuraColour = 0;
				bool SetBarrier(int Index, bool bClosed);
				double LastShockNeed[CameraCount];
				int NextDutyId = 1;
				std::vector<FDuty> DutyList;
				std::vector<FEvent> Events;
				std::vector<std::unique_ptr<FSubject>> Modules;
				FUcnStats StatsData;
				bool bJudgeMark = false;
				bool bJudgeOn = false;
				bool bForcedMove = false;   // set while Jester moves something: the Judge ignores it, the Angel does not count it
			};

			// ========================================================================= profile side (account save)
			constexpr int RewardDiamonds = 3000;   // owner: one-time reward, once per account, on the first win of any mode
			constexpr int ChapterToUnlock = CampaignChapters;   // owner: the first completion of Chapter 13 unlocks the mode

			bool IsUnlocked(const FProfile& Profile);                 // Chapter 13 cleared (or already latched in the save)
			bool IsRewardClaimed(const FProfile& Profile);
			FActionResult StartUcn(FProfile& Profile, EMode Mode);    // needs the unlock; spends one arcade token like the other games
			// Records the night; pays the 3000 Diamonds exactly once, only after the unlock and only for a win.
			std::string FinishUcn(FProfile& Profile, const FUltimateNight& Night);
			// For tests and the overlay result: same bookkeeping from plain facts.
			std::string FinishUcn(FProfile& Profile, EMode Mode, bool bWon, int HoursSurvived);
		}
	}
}
