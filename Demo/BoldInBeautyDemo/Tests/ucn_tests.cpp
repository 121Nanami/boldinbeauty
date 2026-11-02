// Tests for the 2026-10-14 owner request: Ultimate Custom Night (BIBUltimateNight.h): the roster table and AI formula, one
// test per subject module (000-020), the central threat scheduler, the fairness contract and the one-time 3000 Diamond reward
// in the account save. The 200-seed solvability / beatability suite is Tests/ucn_beatability.cpp.
// Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBUcnBots.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBUltimateNight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace
{
	int GFailures = 0;
	int GChecks = 0;

	void Expect(bool bCondition, const std::string& What)
	{
		++GChecks;
		if (!bCondition)
		{
			++GFailures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	using namespace BIB;
	using namespace BIB::Arcade;
	using namespace BIB::Arcade::Ucn;

	constexpr double Step = 0.05;

	// Runs the night for Seconds (or until it ends), calling OnTick before every step and logging every event.
	template <typename F>
	void RunFor(FUltimateNight& W, double Seconds, std::vector<FEvent>& Log, F&& OnTick)
	{
		for (double T = 0.0; T < Seconds && !W.IsOver(); T += Step)
		{
			OnTick(W);
			W.Tick(Step);
			for (const FEvent& Event : W.PopEvents())
			{
				Log.push_back(Event);
			}
		}
	}

	void RunIdle(FUltimateNight& W, double Seconds, std::vector<FEvent>& Log)
	{
		RunFor(W, Seconds, Log, [](FUltimateNight&) {});
	}

	// The perfect bot plays (zero reaction, full information); the bot pops the events itself, so log them through a copy.
	void RunPerfect(FUltimateNight& W, double Seconds, std::vector<FEvent>& Log, uint32_t Seed)
	{
		FBot Bot(EBotKind::Perfect, Seed, W.Tuning());
		for (double T = 0.0; T < Seconds && !W.IsOver(); T += Step)
		{
			W.Tick(Step);
			const std::vector<FEvent> Events = W.PopEvents();
			Log.insert(Log.end(), Events.begin(), Events.end());
			// Hand the events back so the bot sees them too (Stargazer's route announcements).
			for (const FEvent& Event : Events)
			{
				W.Emit(Event.Kind, Event.A, Event.B, Event.C, Event.D);
			}
			Bot.Act(W);
			for (const FEvent& Event : W.PopEvents())
			{
				Log.push_back(Event);   // caused by the bot's own actions
			}
		}
	}

	// Clicks every office object away (Narrator's mini cameras, Death).
	void ClearOffice(FUltimateNight& W)
	{
		for (int Guard = 0; Guard < 20; ++Guard)
		{
			int Target = -1;
			for (const FDuty& Duty : W.Duties())
			{
				Target = Duty.bOpen && Duty.Kind == EDuty::ClickOffice && Target < 0 ? Duty.Id : Target;
			}
			if (Target < 0)
			{
				return;
			}
			W.ClickOffice(Target);
		}
	}

	std::vector<const FEvent*> Of(const std::vector<FEvent>& Log, EEvent Kind, int A = -999)
	{
		std::vector<const FEvent*> Out;
		for (const FEvent& Event : Log)
		{
			if (Event.Kind == Kind && (A == -999 || Event.A == A))
			{
				Out.push_back(&Event);
			}
		}
		return Out;
	}

	std::vector<const FEvent*> Telegraphs(const std::vector<FEvent>& Log, int Subject, EDuty Kind)
	{
		std::vector<const FEvent*> Out;
		for (const FEvent* Event : Of(Log, EEvent::Telegraph, Subject))
		{
			if (Event->B == static_cast<int>(Kind))
			{
				Out.push_back(Event);
			}
		}
		return Out;
	}

	std::vector<int> MovesOf(const std::vector<FEvent>& Log, int Subject)
	{
		std::vector<int> Rooms;
		for (const FEvent* Event : Of(Log, EEvent::Moved, Subject))
		{
			Rooms.push_back(Event->B);
		}
		return Rooms;
	}

	namespace P = BIB::Pages;

	const P::FPageAction* Find(const P::FPage& Page, const std::string& Id)
	{
		for (const P::FPageAction& Action : Page.Grid)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		return nullptr;
	}

	std::string Str(int Value)
	{
		return std::to_string(Value);
	}

	// ======================================================================================== roster table and AI
	void TestRoster()
	{
		const int Expected[ModeCount] = {6, 11, 17, 21};   // + Death (019), summoned by Plack's 5 AM sacrifice in every mode
		for (int M = 0; M < ModeCount; ++M)
		{
			Expect(static_cast<int>(Roster(static_cast<EMode>(M)).size()) == Expected[M], std::string("roster size ") + ModeName(static_cast<EMode>(M)));
		}
		// Easy: AI 15, subjects 001-005.
		for (int Id = 0; Id < SubjectCount; ++Id)
		{
			Expect(InMode(Id, EMode::Easy) == ((Id >= 1 && Id <= 5) || Id == DeathId), "Easy membership " + Str(Id));
			Expect(InMode(Id, EMode::Medium) == ((Id >= 1 && Id <= 10) || Id == DeathId), "Medium membership " + Str(Id));
			Expect(InMode(Id, EMode::Hard) == (Id <= 15 || Id == DeathId), "Hard membership (001-015 plus 000, Death summoned) " + Str(Id));
			Expect((SubjectInfo(Id).Arrival == EArrival::PlackSacrifice) == (Id == DeathId), "only Death waits for the sacrifice " + Str(Id));
			Expect(InMode(Id, EMode::Angel), "Angel has every subject " + Str(Id));
			if (InMode(Id, EMode::Easy)) { Expect(AiLevel(Id, EMode::Easy) == 15, "Easy AI 15"); }
			if (InMode(Id, EMode::Medium)) { Expect(AiLevel(Id, EMode::Medium) == 15, "Medium AI 15"); }
			if (InMode(Id, EMode::Hard)) { Expect(AiLevel(Id, EMode::Hard) == 10, "Hard AI 10"); }
			Expect(AiLevel(Id, EMode::Angel) == 20, "Angel AI 20");
			Expect(SubjectInfo(Id).Id == Id, "table row id " + Str(Id));
		}
		Expect(std::string(SubjectInfo(LunaId).Name) == "Luna" && std::string(SubjectInfo(PlackId).Name) == "Plack"
			&& std::string(SubjectInfo(KuuId).Name) == "Kuu" && std::string(SubjectInfo(MediaId).Name) == "Media"
			&& std::string(SubjectInfo(AngelId).Name) == "Angel" && std::string(SubjectInfo(AdaptId).Name) == "Adapt", "owner names kept");
		Expect(AiLevel(DeathId, EMode::Easy) == 15 && AiLevel(DeathId, EMode::Medium) == 15 && AiLevel(DeathId, EMode::Hard) == 10
			&& AiLevel(DeathId, EMode::Angel) == 20, "Death is summoned at the mode's AI level (15 / 15 / 10 / 20)");
		// Owner 2026-10-14: Angel (000) is always active in Hard and Angel mode, from the start.
		Expect(InMode(AngelId, EMode::Hard) && InMode(AngelId, EMode::Angel) && SubjectInfo(AngelId).Arrival == EArrival::Start, "Angel is active from the start in Hard and Angel");

		// AI N: one formula. AI 10 / 15 / 20 = Night Shift's Easy / Medium / Hard movement.
		Expect(std::fabs(MovementFor(10).IntervalSeconds - 7.0) < 1e-9 && std::fabs(MovementFor(10).Chance - 0.5) < 1e-9, "AI 10 = 7 s x 50%");
		Expect(std::fabs(MovementFor(15).IntervalSeconds - 6.0) < 1e-9 && std::fabs(MovementFor(15).Chance - 0.75) < 1e-9, "AI 15 = 6 s x 75%");
		Expect(std::fabs(MovementFor(20).IntervalSeconds - 5.0) < 1e-9 && std::fabs(MovementFor(20).Chance - 1.0) < 1e-9, "AI 20 = 5 s x 100%");
		Expect(MovementFor(0).Chance == 0.0, "AI 0 never moves");

		// Every mode's world builds its modules from the table.
		for (int M = 0; M < ModeCount; ++M)
		{
			FUltimateNight W;
			W.Start(static_cast<EMode>(M), 7u);
			for (int Id = 0; Id < SubjectCount; ++Id)
			{
				Expect(W.HasSubject(Id) == InMode(Id, static_cast<EMode>(M)), std::string("module built from the table ") + ModeName(static_cast<EMode>(M)));
			}
		}
	}

	// ======================================================================================== the night frame
	void TestNightFrame()
	{
		FUltimateNight W;
		W.StartWith(EMode::Angel, 1u, {});
		Expect(W.Hour() == 0 && W.Power() == 100.0 && W.ShocksTotal() == CameraCount * W.Tuning().ShocksPerCamera, "starts at 12 AM, full power and shocks");
		Expect(W.CamerasWorking() == CameraCount, "Night Shift's 10 cameras");
		std::vector<FEvent> Log;
		RunIdle(W, NightHours * NightHourSeconds + 1.0, Log);
		Expect(W.IsOver() && W.IsWon(), "an empty night is survived at 6 AM (5-minute night)");
		Expect(Of(Log, EEvent::Hour).size() == static_cast<size_t>(NightHours), "6 hour chimes");

		FUltimateNight P;
		P.StartWith(EMode::Angel, 1u, {});
		P.SetDoor(ESide::Left, true);
		P.SetDoor(ESide::Right, true);
		P.SetCamera(true);
		Expect(P.Usage() == 4, "usage: base + 2 doors + camera");
		RunIdle(P, 10.0, Log);
		const double Expected = 100.0 - PowerDrainPerSecond * P.Tuning().PowerDrainScale * 4.0 * 10.0;
		Expect(std::fabs(P.Power() - Expected) < 0.05, "Night Shift drain x PowerDrainScale");
	}

	// ======================================================================================== 001 Luna / 002 Plack
	void TestWalkers()
	{
		// Luna: a straight line across 5 cameras to the left door.
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 3u, {LunaId});
			std::vector<FEvent> Log;
			RunFor(W, 120.0, Log, [](FUltimateNight& Night)
			{
				for (const FDuty& Duty : Night.Duties())
				{
					if (Duty.bOpen && Duty.Kind == EDuty::CloseDoor)
					{
						Night.SetDoor(ESide::Left, true);
					}
				}
			});
			const std::vector<int> Rooms = MovesOf(Log, LunaId);
			const std::vector<int> Want = {static_cast<int>(ERoom::Dining), WindowCamera, static_cast<int>(ERoom::WestHall),
				static_cast<int>(ERoom::WestCorner), static_cast<int>(ERoom::LeftDoor), static_cast<int>(ERoom::Stage)};
			Expect(Rooms.size() >= Want.size() && std::equal(Want.begin(), Want.end(), Rooms.begin()), "Luna: Stage-Dining-CAM 3 (outside the front window)-West Hall-West Corner-left door");
			const auto Door = Telegraphs(Log, LunaId, EDuty::CloseDoor);
			Expect(!Door.empty() && Door[0]->C == 0, "Luna telegraphs at the LEFT door (glowing eyes)");
			Expect(!Door.empty() && Door[0]->D >= 4999, "Luna strikes on the next movement opportunity (AI 20: 5 s)");
			Expect(!Of(Log, EEvent::Knock, LunaId).empty() && !W.IsOver(), "Luna: a closed door sends her away");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 3u, {LunaId});
			std::vector<FEvent> Log;
			RunIdle(W, 120.0, Log);
			Expect(W.IsOver() && W.Killer() == LunaId, "Luna: an open door kills");
			Expect(W.Stats().UntelegraphedKills == 0 && W.Stats().ShortWindowKills == 0, "Luna's kill was telegraphed");
		}
		// Plack: right door; Death replaces her from the start when both exist; 5 AM sacrifice disables Death.
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 4u, {PlackId});
			std::vector<FEvent> Log;
			RunIdle(W, 120.0, Log);
			const std::vector<int> Rooms = MovesOf(Log, PlackId);
			Expect(Rooms.size() >= 5 && Rooms[0] == static_cast<int>(ERoom::Dining) && Rooms[1] == WindowCamera
				&& Rooms[2] == static_cast<int>(ERoom::EastHall) && Rooms[3] == static_cast<int>(ERoom::EastCorner) && Rooms[4] == static_cast<int>(ERoom::RightDoor),
				"Plack: straight line across 5 cameras to the right door");
			Expect(W.Killer() == PlackId, "Plack: an open right door kills");
		}
		// Owner 2026-10-14: in every mode Plack's 5 AM sacrifice summons Death (019), who is not active before it.
		for (int M = 0; M < ModeCount; ++M)
		{
			FUltimateNight W;
			W.Start(static_cast<EMode>(M), 5u);
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 5u);
			const double FiveAm = 5.0 * NightHourSeconds;
			const auto Sacrifice = Of(Log, EEvent::Sacrifice);
			const std::string Tag = std::string(" - ") + ModeName(static_cast<EMode>(M));
			Expect(Sacrifice.size() == 1 && Sacrifice[0]->A == 1 && std::fabs(Sacrifice[0]->Time - FiveAm) < 0.1, "5 AM: Plack's sacrifice summons Death" + Tag);
			bool bDeathEarly = false;
			bool bDeathLate = false;
			bool bPlackLate = false;
			for (const FEvent& Event : Log)
			{
				if ((Event.Kind == EEvent::Telegraph || Event.Kind == EEvent::Moved) && Event.A == DeathId)
				{
					bDeathEarly = bDeathEarly || Event.Time < FiveAm - 0.1;
					bDeathLate = bDeathLate || (Event.Kind == EEvent::Telegraph && Event.Time > FiveAm);
				}
				bPlackLate = bPlackLate || ((Event.Kind == EEvent::Telegraph || Event.Kind == EEvent::Moved) && Event.A == PlackId && Event.Time > FiveAm + 0.1);
			}
			Expect(!bDeathEarly, "Death is not active before the sacrifice" + Tag);
			Expect(bDeathLate, "Death attacks after the sacrifice" + Tag);
			Expect(!bPlackLate, "Plack is gone after her sacrifice" + Tag);
			Expect(!MovesOf(Log, PlackId).empty(), "Plack walks until 5 AM" + Tag);
			Expect(W.IsWon(), "perfect play survives the last hour with Death" + Tag);
		}
		// Arita: either path, no roll; every door she reaches must be shut.
		{
			std::set<int> Sides;
			for (uint32_t Seed = 1; Seed <= 12; ++Seed)
			{
				FUltimateNight W;
				W.StartWith(EMode::Angel, Seed, {AritaId});
				std::vector<FEvent> Log;
				RunIdle(W, 120.0, Log);
				const auto Door = Telegraphs(Log, AritaId, EDuty::CloseDoor);
				if (!Door.empty())
				{
					Sides.insert(Door[0]->C);
				}
				Expect(W.Killer() == AritaId, "Arita: an open door she reaches kills");
			}
			Expect(Sides.size() == 2, "Arita takes either Luna's or Plack's path");
		}
	}

	// ======================================================================================== 003 Kuu
	void TestKuu()
	{
		for (int Shut = 0; Shut < 2; ++Shut)
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 8u, {KuuId});
			std::vector<FEvent> Log;
			int Side = -1;
			RunFor(W, 90.0, Log, [&](FUltimateNight& Night)
			{
				if (Shut != 0 && Night.KuuSideInUse() >= 0 && Side < 0)
				{
					Side = Night.KuuSideInUse();
					Night.SetDoor(Side == 0 ? ESide::Left : ESide::Right, true);
				}
			});
			const auto Windup = Of(Log, EEvent::KuuWindup);
			const auto Punch = Of(Log, EEvent::KuuPunch);
			Expect(!Windup.empty() && !Punch.empty(), "Kuu winds up and punches");
			if (!Windup.empty() && !Punch.empty())
			{
				Expect(std::fabs(Punch[0]->Time - Windup[0]->Time - 4.0) < 0.11, "Kuu: a loud 4-second charge, then the punch");
				Expect((Punch[0]->B == 1) == (Shut != 0), Shut != 0 ? "Kuu breaks a shut door" : "Kuu misses an open doorway");
				// After the punch she teleports back to the stage.
				bool bStage = false;
				for (const FEvent* Move : Of(Log, EEvent::Moved, KuuId))
				{
					bStage = bStage || (Move->Time >= Punch[0]->Time && Move->B == static_cast<int>(ERoom::Stage));
				}
				Expect(bStage, "Kuu teleports back to the stage");
			}
			if (Shut != 0)
			{
				Expect(!Of(Log, EEvent::DoorBroken).empty(), "a broken door is reported");
			}
			Expect(!W.IsOver(), "Kuu's punch is not a kill");
		}
	}

	// ======================================================================================== 004 Ishii
	void TestIshii()
	{
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 9u, {IshiiId});
			std::vector<FEvent> Log;
			RunIdle(W, 60.0, Log);
			const auto Start = Of(Log, EEvent::Infection);
			const auto Lost = Of(Log, EEvent::CameraLost);
			Expect(!Start.empty() && !Lost.empty() && Lost[0]->A == Start[0]->A && std::fabs(Lost[0]->Time - Start[0]->Time - 15.0) < 0.11,
				"Ishii: a camera left unchecked for 15 s is permanently disabled");
			Expect(W.CamerasWorking() < CameraCount && !W.IsOver(), "losing a camera is not a kill");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 9u, {IshiiId});
			std::vector<FEvent> Log;
			RunPerfect(W, 120.0, Log, 9u);
			const auto Start = Of(Log, EEvent::Infection);
			Expect(Start.size() >= 3 && W.CamerasWorking() == CameraCount, "a shock stops each infection");
			for (size_t Index = 1; Index < Start.size(); ++Index)
			{
				Expect(Start[Index]->Time - Start[Index - 1]->Time >= 25.0 - 1e-6, "Ishii: 25-second cooldown between attacks");
			}
		}
	}

	// ======================================================================================== 006 Fujita
	void TestFujita()
	{
		std::set<int> Sides;
		for (uint32_t Seed = 1; Seed <= 10; ++Seed)
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, Seed, {FujitaId});
			std::vector<FEvent> Log;
			RunPerfect(W, 200.0, Log, Seed);
			const auto Door = Telegraphs(Log, FujitaId, EDuty::CloseDoor);
			Expect(!Door.empty(), "Fujita appears at a door");
			for (size_t Index = 0; Index < Door.size(); ++Index)
			{
				Sides.insert(Door[Index]->C);
				Expect(Door[Index]->D == 5000, "Fujita: a 5-second kill timer");
				if (Index > 0)
				{
					const double Gap = Door[Index]->Time - Door[Index - 1]->Time - 5.0;
					Expect(Gap >= 25.0 - 0.06 && Gap <= 40.0 + 0.06, "Fujita: 25-40 s cooldown");
				}
			}
			Expect(MovesOf(Log, FujitaId).size() == Door.size(), "Fujita does not follow a path: she just appears at the door");
		}
		Expect(Sides.size() == 2, "Fujita mimics both sides");
		FUltimateNight W;
		W.StartWith(EMode::Angel, 2u, {FujitaId});
		std::vector<FEvent> Log;
		RunIdle(W, 80.0, Log);
		const auto Door = Telegraphs(Log, FujitaId, EDuty::CloseDoor);
		Expect(W.Killer() == FujitaId && !Door.empty() && std::fabs(Log.back().Time - Door[0]->Time - 5.0) < 0.06, "Fujita kills 5 s after appearing");
	}

	// ======================================================================================== 007 Shiraishi
	double ShiraishiStep(int Mode)   // 0 cameras down, 1 cameras up elsewhere, 2 camera on her
	{
		FUltimateNight W;
		W.StartWith(EMode::Angel, 1u, {ShiraishiId});
		std::vector<FEvent> Log;
		RunFor(W, 200.0, Log, [Mode](FUltimateNight& Night)
		{
			if (Mode == 0)
			{
				return;
			}
			Night.SetCamera(true);
			int Her = 0;
			for (int Cam = 0; Cam < CameraCount; ++Cam)
			{
				for (const int Id : Night.SubjectsOnCamera(Cam))
				{
					Her = Id == ShiraishiId ? Cam : Her;
				}
			}
			Night.SelectCamera(Mode == 2 ? Her : (Her + 5) % CameraCount);
		});
		const auto Moves = Of(Log, EEvent::Moved, ShiraishiId);
		return Moves.size() >= 2 ? Moves[1]->Time - Moves[0]->Time : -1.0;
	}

	void TestShiraishi()
	{
		Expect(std::fabs(ShiraishiStep(0) - 10.0) < 0.11, "Shiraishi: cameras down, 1 camera per 10 s");
		Expect(std::fabs(ShiraishiStep(1) - 20.0) < 0.11, "Shiraishi: default 1 camera per 20 s");
		Expect(std::fabs(ShiraishiStep(2) - 45.0) < 0.11, "Shiraishi: camera on her, 1 camera per 45 s (stalled, not frozen)");
		FUltimateNight W;
		W.StartWith(EMode::Angel, 1u, {ShiraishiId});
		Expect(!W.SubjectsOnCamera(0).empty() && W.SubjectsOnCamera(0)[0] == ShiraishiId, "Shiraishi starts on the show stage");
		std::vector<FEvent> Log;
		RunIdle(W, 300.0, Log);
		const double Expected = NightGraceSeconds + W.Tuning().ShiraishiStepsToDoor * 10.0 + W.Tuning().ShiraishiBurn;
		Expect(W.Killer() == ShiraishiId && std::fabs(Log.back().Time - Expected) < 0.2, "Shiraishi reaches the door, burns it, then kills");
	}

	// ======================================================================================== 008 Myrcia
	void TestMyrcia()
	{
		FUltimateNight W;
		W.StartWith(EMode::Angel, 11u, {MyrciaId});
		std::vector<FEvent> Log;
		RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 11u);
		const auto Rush = Telegraphs(Log, MyrciaId, EDuty::CloseDoor);
		int Bursts = 0;
		int InBurst = 0;
		bool bCountsOk = true;
		for (size_t Index = 0; Index < Rush.size(); ++Index)
		{
			Expect(Rush[Index]->D == 1500, "Myrcia: 1.5 s per door rush");
			if (Index == 0 || Rush[Index]->Time - Rush[Index - 1]->Time > 10.0)
			{
				if (Index > 0)
				{
					bCountsOk = bCountsOk && InBurst >= 2 && InBurst <= 5;
				}
				++Bursts;
				InBurst = 0;
			}
			++InBurst;
		}
		bCountsOk = bCountsOk && InBurst >= 2 && InBurst <= 5;
		Expect(Bursts == 3, "Myrcia: three times, at hour intervals");
		Expect(bCountsOk, "Myrcia: 2-5 rushes each time");
		Expect(W.IsWon(), "perfect play survives Myrcia");
	}

	// ======================================================================================== 009 Alchemist
	void TestAlchemist()
	{
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 12u, {AlchemistId});
			Expect(W.SubjectsOnCamera(5).size() == 1 && W.SubjectsOnCamera(5)[0] == AlchemistId, "Alchemist sits on CAM 6 (the middle)");
			std::vector<FEvent> Log;
			RunIdle(W, 95.0, Log);
			const auto Lost = Of(Log, EEvent::CameraLost);
			Expect(Lost.size() == 3 && std::fabs(Lost[0]->Time - 30.0) < 0.11 && std::fabs(Lost[1]->Time - 60.0) < 0.11, "Alchemist: every 30 s she disables a camera");
			Expect(W.VaccineCamera() >= 0, "a vaccine bottle exists on a camera");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 12u, {AlchemistId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 12u);
			Expect(W.IsWon() && W.CamerasWorking() == CameraCount && W.Stats().VaccinesTaken >= 8, "the vaccine disables the next infection: all cameras kept");
		}
	}

	// ======================================================================================== 010 Blood / 013 Weather
	void TestBlood()
	{
		{
			// Ignored: four phases of 2-10 s at the window, then she comes through.
			FUltimateNight W;
			W.StartWith(EMode::Angel, 13u, {BloodId});
			std::vector<FEvent> Log;
			RunIdle(W, 120.0, Log);
			const auto Phases = Of(Log, EEvent::BloodPhase);
			Expect(Phases.size() >= 4 && Phases[0]->A == 1 && Phases[3]->A == 4, "Blood goes through 4 phases");
			bool bLengths = true;
			for (size_t Index = 1; Index < Phases.size() && Index < 4; ++Index)
			{
				const double Took = Phases[Index]->Time - Phases[Index - 1]->Time;
				bLengths = bLengths && Took >= 2.0 - 0.06 && Took <= 10.0 + 0.06;
			}
			Expect(bLengths, "each phase takes 2-10 s");
			Expect(!Phases.empty() && Phases[0]->Time >= NightGraceSeconds + 20.0 - 0.06 && Phases[0]->Time <= NightGraceSeconds + 30.0 + 0.06, "she appears after 20-30 s");
			Expect(W.Killer() == BloodId && W.Stats().UntelegraphedKills == 0, "her last phase kills if the window stays open");
		}
		{
			// Shut early: deadly (a punishment for the player's action, cued by her phase).
			FUltimateNight W;
			W.StartWith(EMode::Angel, 13u, {BloodId});
			std::vector<FEvent> Log;
			RunFor(W, 120.0, Log, [](FUltimateNight& Night)
			{
				if (Night.BloodPhaseNow() == 2)
				{
					Night.SetWindow(true);
				}
			});
			Expect(W.Killer() == BloodId, "shutting her window before the last phase kills");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 13u, {BloodId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 13u);
			Expect(W.IsWon() && Of(Log, EEvent::BloodPhase, 4).size() >= 4, "shut in her last phase: she leaves, every visit");
		}
	}

	void TestWeather()
	{
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 14u, {WeatherId});
			std::vector<FEvent> Log;
			RunIdle(W, 120.0, Log);
			const auto Seen = Of(Log, EEvent::Weather);
			Expect(!Seen.empty() && Seen[0]->A >= 0 && Seen[0]->A != WeatherId && (Seen[0]->B == 0 || Seen[0]->B == 1), "Weather takes the form of another operator with a yellow or blue aura");
			Expect(W.Killer() == WeatherId, "Weather comes through an open window");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 14u, {WeatherId});
			std::vector<FEvent> Log;
			bool bOnCam3 = false;
			RunFor(W, 60.0, Log, [&bOnCam3](FUltimateNight& Night)
			{
				for (const int Id : Night.SubjectsOnCamera(WindowCamera))
				{
					bOnCam3 = bOnCam3 || Id == WeatherId;
				}
			});
			Expect(bOnCam3, "Weather is seen on CAM 3, outside the front window");
		}
		std::set<int> Auras;
		for (uint32_t Seed = 1; Seed <= 6; ++Seed)
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, Seed, {WeatherId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, Seed);
			for (const FEvent* Event : Of(Log, EEvent::Weather))
			{
				if (Event->A >= 0)
				{
					Auras.insert(Event->B);
				}
			}
			Expect(W.IsWon(), "shutting the window sends Weather away");
		}
		Expect(Auras.size() == 2, "both auras appear");
	}

	// ======================================================================================== 011 Stargazer
	void TestStargazer()
	{
		FUltimateNight W;
		W.StartWith(EMode::Angel, 14u, {StargazerId});
		std::vector<FEvent> Log;
		RunIdle(W, 90.0, Log);
		const auto Star = Of(Log, EEvent::Stargazer);
		std::vector<int> Cams;
		double First = -1.0;
		bool bHalf = true;
		double Last = -1.0;
		for (const FEvent* Event : Star)
		{
			if (Event->A == 1)
			{
				First = First < 0.0 ? Event->Time : First;
				if (Last >= 0.0)
				{
					bHalf = bHalf && std::fabs(Event->Time - Last - 0.5) < 0.06;
				}
				Last = Event->Time;
				Cams.push_back(Event->B);
			}
		}
		Expect(First >= 45.0 - 0.06 && First <= 55.0 + 0.06, "Stargazer wakes over 45-55 s");
		const std::vector<int> Order = {0, 6, 8, 4, 2, 5, 3, 7};   // CAM 1-7-9-5-3-6-4-8
		Expect(Cams.size() == 9 && std::equal(Order.begin(), Order.end(), Cams.begin()) && (Cams[8] == 1 || Cams[8] == 2), "Stargazer: 1-7-9-5-3-6-4-8, then 2 or 3");
		Expect(bHalf, "Stargazer stays 0.5 s on each camera");
		Expect(!Of(Log, EEvent::Stargazer, 0).empty(), "Stargazer's wake-up is telegraphed");
		const auto Smash = Of(Log, EEvent::Stargazer, 2);
		Expect(Smash.size() == 1 && Smash[0]->B == WindowSide && (Smash[0]->C == 1 || Smash[0]->C == 2), "from CAM 2 or 3 (index 1 or 2) he goes for the front window");
		Expect(W.Killer() == StargazerId && W.LostReason().find("front window") != std::string::npos, "Stargazer unflashed smashes your front window and kills");
		{
			// A shut window does not stop him.
			FUltimateNight Shut;
			Shut.StartWith(EMode::Angel, 14u, {StargazerId});
			std::vector<FEvent> ShutLog;
			RunFor(Shut, 90.0, ShutLog, [](FUltimateNight& Night) { Night.SetWindow(true); });
			Expect(Shut.Killer() == StargazerId, "a shut window does not stop Stargazer");
		}

		FUltimateNight P;
		P.StartWith(EMode::Angel, 14u, {StargazerId});
		std::vector<FEvent> Played;
		RunPerfect(P, NightHours * NightHourSeconds + 1.0, Played, 14u);
		Expect(P.IsWon() && Of(Played, EEvent::Stargazer, 3).size() >= 4, "a flash on his camera sends him back");
	}

	// ======================================================================================== 012 Gambler
	void TestGambler()
	{
		int Heads = 0;
		int Tails = 0;
		for (uint32_t Seed = 1; Seed <= 6; ++Seed)
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, Seed, {GamblerId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, Seed);
			const auto Flips = Of(Log, EEvent::CoinFlip);
			const auto Appear = Telegraphs(Log, GamblerId, EDuty::ShockGambler);
			for (const FEvent* Flip : Flips)
			{
				(Flip->A == 1 ? Heads : Tails) += 1;
				if (Flip->A == 0)
				{
					for (const FEvent* Next : Appear)
					{
						if (Next->Time > Flip->Time)
						{
							Expect(Next->Time - Flip->Time >= 40.0 - 0.06, "Gambler: tails, she leaves (40-second cooldown)");
							break;
						}
					}
				}
			}
			Expect(!Flips.empty() && W.IsWon(), "Gambler: a shock plays the coin flip");
		}
		Expect(Heads > 0 && Tails > 0, "both heads (find her again) and tails happen");
	}

	// ======================================================================================== 014 Judge
	void TestJudge()
	{
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 15u, {JudgeId});
			std::vector<FEvent> Log;
			RunIdle(W, 2.0, Log);
			Expect(W.IsDoorFalseAlarm(ESide::Left), "the false alarm cue is shown before the misplay");
			W.SetDoor(ESide::Left, true);
			Expect(W.IsOver() && W.Killer() == JudgeId, "Judge: a door shut with nobody there kills");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 15u, {JudgeId});
			std::vector<FEvent> Log;
			RunIdle(W, 2.0, Log);
			W.SetCamera(true);
			Expect(W.IsShockFalseAlarm(), "shock false alarm cue");
			W.Shock();
			Expect(W.IsOver() && W.Killer() == JudgeId, "Judge: a shock with nothing to shock kills");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 15u, {JudgeId});
			std::vector<FEvent> Log;
			RunIdle(W, 2.0, Log);
			Expect(W.IsWindowFalseAlarm(), "window false alarm cue");
			W.SetWindow(true);
			Expect(W.IsOver() && W.Killer() == JudgeId, "Judge: the window shut with nobody there kills");
		}
		{
			// Only ever after a player action: Jester's forced doors never wake him.
			FUltimateNight W;
			W.StartWith(EMode::Angel, 16u, {JudgeId, JesterId});
			std::vector<FEvent> Log;
			RunFor(W, NightHours * NightHourSeconds + 1.0, Log, [](FUltimateNight& Night) { Night.CloseAd(); });
			Expect(!Of(Log, EEvent::DoorForced).empty() && W.Killer() != JudgeId && Of(Log, EEvent::Judge).empty(), "Judge ignores Jester's forced doors");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 17u, {JudgeId, MediaId, LunaId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 17u);
			Expect(W.IsWon() && W.Stats().JudgeKills == 0, "closing a door on a real threat is never a misplay");
		}
	}

	// ======================================================================================== 015 Narrator / Diano
	void TestNarrator()
	{
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 18u, {NarratorId});
			std::vector<FEvent> Log;
			RunIdle(W, 60.0, Log);
			const auto Minis = Of(Log, EEvent::MiniCamera);
			Expect(Minis.size() == 2 && std::fabs(Minis[1]->Time - Minis[0]->Time - 8.0) < 0.06, "Narrator: a mini camera once per 8 s");
			Expect(W.Killer() == NarratorId && W.Stats().UntelegraphedKills == 0, "Narrator: the third camera kills (announced)");
			for (const FDuty& Duty : W.Duties())
			{
				if (Duty.Subject == NarratorId && !Duty.bLethal)
				{
					Expect(Duty.Clicks >= 3 && Duty.Clicks <= 5, "around 3-5 clicks remove a camera");
				}
			}
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 18u, {NarratorId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 18u);
			bool bOdd = false;
			for (const FEvent* Mini : Of(Log, EEvent::MiniCamera))
			{
				bOdd = bOdd || (static_cast<int>(Mini->Time / NightHourSeconds) % 2) == 1;
			}
			Expect(W.IsWon() && !bOdd, "Narrator's mini cameras only come in even hours");
		}
		{
			// The perfect bot, plus a camera check every 3 s (Diano's cue is the camera going down).
			FUltimateNight W;
			W.StartWith(EMode::Angel, 18u, {NarratorId});
			std::vector<FEvent> Log;
			FBot Bot(EBotKind::Perfect, 18u, W.Tuning());
			double NextLook = 3.0;
			for (double T = 0.0; T < NightHours * NightHourSeconds + 1.0 && !W.IsOver(); T += Step)
			{
				if (W.Time() >= NextLook && W.NextLethalDeadline() > 1e8)
				{
					NextLook += 3.0;
					W.SetCamera(true);
					W.SetCamera(false);
				}
				W.Tick(Step);
				const std::vector<FEvent> Events = W.PopEvents();
				Log.insert(Log.end(), Events.begin(), Events.end());
				Bot.Act(W);
				for (const FEvent& Event : W.PopEvents())
				{
					Log.push_back(Event);
				}
			}
			Expect(W.IsWon(), "perfect answers to Diano survive the night");
			std::set<int> Sides;
			bool bOnlyOdd = true;
			for (const FEvent* Attack : Of(Log, EEvent::Diano))
			{
				Sides.insert(Attack->A);
				bOnlyOdd = bOnlyOdd && (static_cast<int>(Attack->Time / NightHourSeconds) % 2) == 1;
			}
			Expect(Sides.size() == 3 && bOnlyOdd, "Diano attacks the left door, the right door and the middle window, only in odd hours");
			for (const FEvent* Event : Telegraphs(Log, NarratorId, EDuty::CloseDoor))
			{
				Expect(Event->D == 1000, "Diano: 1 second to react");
			}
		}
		{
			// Like Plushtrap: putting the camera down in an odd hour starts his attack; ignored, he kills.
			FUltimateNight W;
			W.StartWith(EMode::Angel, 18u, {NarratorId});
			std::vector<FEvent> Log;
			RunFor(W, NightHourSeconds + 1.0, Log, [](FUltimateNight& Night) { ClearOffice(Night); });
			W.SetCamera(true);
			W.SetCamera(false);
			RunIdle(W, 2.0, Log);
			Expect(Of(Log, EEvent::Diano).size() == 1 && W.Killer() == NarratorId, "1 AM: camera down, Diano attacks and kills after 1 s");
		}
		{
			// Jester's forced camera never starts Diano.
			FUltimateNight W;
			W.StartWith(EMode::Angel, 18u, {NarratorId});
			std::vector<FEvent> Log;
			RunFor(W, NightHourSeconds + 1.0, Log, [](FUltimateNight& Night) { ClearOffice(Night); });
			W.ForceCamera(true);
			W.ForceCamera(false);
			RunIdle(W, 2.0, Log);
			Expect(Of(Log, EEvent::Diano).empty(), "a forced camera-down is not Diano's cue");
		}
	}

	// ======================================================================================== 016 Jester
	void TestJester()
	{
		FUltimateNight W;
		W.StartWith(EMode::Angel, 19u, {JesterId});
		std::vector<FEvent> Log;
		RunFor(W, NightHours * NightHourSeconds + 1.0, Log, [](FUltimateNight& Night)
		{
			Night.CloseAd();
			Night.SetDoor(ESide::Left, false);
			Night.SetDoor(ESide::Right, false);
			Night.SetCamera(false);
		});
		const auto Horns = Of(Log, EEvent::Horn);
		Expect(Horns.size() >= 14, "Jester acts all night");
		bool bGaps = true;
		for (size_t Index = 1; Index < Horns.size(); ++Index)
		{
			const double Gap = Horns[Index]->Time - Horns[Index - 1]->Time;
			bGaps = bGaps && Gap >= 10.0 - 0.06 && Gap <= 20.0 + 0.06;
		}
		Expect(bGaps, "Jester: every 10-20 s");
		Expect(Of(Log, EEvent::Ad, 1).size() == Horns.size(), "an advertisement each time");
		Expect(!Of(Log, EEvent::DoorForced).empty() && !Of(Log, EEvent::CameraForced).empty(), "forced doors and cameras");
		Expect(W.IsWon(), "Jester cannot cause a death");
		// Even ignored completely.
		FUltimateNight Idle;
		Idle.StartWith(EMode::Angel, 19u, {JesterId});
		std::vector<FEvent> IdleLog;
		RunIdle(Idle, NightHours * NightHourSeconds + 1.0, IdleLog);
		Expect(Idle.IsWon(), "Jester ignored all night still cannot kill (not even through the power)");
	}

	// ======================================================================================== 017 Media
	void TestMedia()
	{
		std::set<int> Sides;
		for (uint32_t Seed = 1; Seed <= 8; ++Seed)
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, Seed, {MediaId});
			std::vector<FEvent> Log;
			RunPerfect(W, 200.0, Log, Seed);
			for (const FEvent* Event : Telegraphs(Log, MediaId, EDuty::CloseDoor))
			{
				Sides.insert(Event->C);
				Expect(Event->D == 1000, "Media: 1 second to close it");
			}
			Expect(!W.IsOver(), "perfect play closes on Media");
		}
		Expect(Sides.size() == 2, "Media teleports to the left or the right door");
		FUltimateNight W;
		W.StartWith(EMode::Angel, 1u, {MediaId});
		std::vector<FEvent> Log;
		RunIdle(W, 200.0, Log);
		Expect(W.Killer() == MediaId, "Media kills when the door stays open");
	}

	// ======================================================================================== 018 Time Keeper
	void TestTimeKeeper()
	{
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 20u, {TimeKeeperId});
			std::vector<FEvent> Log;
			RunIdle(W, 60.0, Log);
			const auto Appear = Of(Log, EEvent::TimeKeeper);
			Expect(Appear.size() >= 2 && std::fabs(Appear[1]->Time - Appear[0]->Time - 8.0) < 0.06, "Time Keeper appears every 8 s");
			const auto Lost = Of(Log, EEvent::HourLost);
			Expect(!Lost.empty() && std::fabs(Lost[0]->Time - Appear[0]->Time - 10.0) < 0.06, "not found within 10 s: sent back an hour");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 20u, {TimeKeeperId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 20u);
			Expect(W.IsWon() && W.Stats().HoursLost == 0, "finding her each time keeps the clock");
		}
	}

	// ======================================================================================== 019 Death
	void TestDeath()
	{
		// Death needs Plack's sacrifice: run Plack + Death; she only acts in the last hour.
		FUltimateNight W;
		W.StartWith(EMode::Angel, 21u, {PlackId, DeathId});
		std::vector<FEvent> Log;
		RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, 21u);
		const auto Rush = Telegraphs(Log, DeathId, EDuty::CloseDoor);
		std::set<int> Sides;
		bool bEveryFive = true;
		double LastLeft = -1.0;
		for (const FEvent* Event : Rush)
		{
			Sides.insert(Event->C);
			Expect(Event->D == 1500 || Event->D >= 5000, "Death's rush window (PLACEHOLDER 1.5 s) or her walk to Plack's door (next movement opportunity)");
			if (Event->C == 0 && Event->Time > 5.0 * NightHourSeconds + 2.0)
			{
				bEveryFive = bEveryFive && (LastLeft < 0.0 || Event->Time - LastLeft >= 5.0 - 0.06);
				LastLeft = Event->Time;
			}
		}
		Expect(Rush.size() >= 10 && Sides.size() == 2, "Death rushes the left and right doors");
		Expect(bEveryFive, "Death rushes a door at most every 5 s");
		const auto Office = Telegraphs(Log, DeathId, EDuty::ClickOffice);
		Expect(!Office.empty() && Office[0]->D == 2000, "Death in the office: 2 s");
		Expect(W.Tuning().DeathOfficeClicks == 4, "click her 4 times");
		Expect(W.IsWon(), "perfect play answers Death");
		FUltimateNight Idle;
		Idle.StartWith(EMode::Angel, 21u, {PlackId, DeathId});
		std::vector<FEvent> IdleLog;
		RunPerfect(Idle, 5.0 * NightHourSeconds + 1.0, IdleLog, 21u);
		RunIdle(Idle, 60.0, IdleLog);
		Expect(Idle.Killer() == DeathId, "Death kills when ignored");
	}

	// ======================================================================================== 020 Adapt
	void TestAdapt()
	{
		std::set<int> Copied;
		for (uint32_t Seed = 1; Seed <= 4; ++Seed)
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, Seed, {AdaptId});
			std::vector<FEvent> Log;
			RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, Seed);
			for (const FEvent& Event : Log)
			{
				if (Event.Kind == EEvent::Telegraph || Event.Kind == EEvent::Moved)
				{
					Copied.insert(Event.A);
				}
			}
			Expect(W.IsWon(), "perfect play survives Adapt (seed " + Str(static_cast<int>(Seed)) + ")");
		}
		Expect(Copied.size() >= 6, "Adapt copies many subjects (" + Str(static_cast<int>(Copied.size())) + ")");
		Expect(Copied.count(AdaptId) == 0, "Adapt never copies himself");
	}

	// ======================================================================================== 000 Angel
	void TestAngel()
	{
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 22u, {AngelId});
			std::vector<FEvent> Log;
			W.SetCamera(true);   // pull 1
			W.SetCamera(false);
			RunIdle(W, 1.0, Log);
			Expect(Of(Log, EEvent::Telegraph, AngelId).empty(), "Angel: not on the first pull");
			W.SetCamera(true);   // pull 2
			W.SetCamera(false);
			RunIdle(W, 0.2, Log);
			const auto In = Of(Log, EEvent::Telegraph, AngelId);
			Expect(In.size() == 1 && In[0]->D == 1500, "Angel: in the office every other pull, 1.5 s");
			W.SetCamera(true);   // pull 3 removes her
			RunIdle(W, 3.0, Log);
			Expect(!W.IsOver() && !Of(Log, EEvent::Resolved, AngelId).empty(), "pulling the cameras up again removes her");
		}
		{
			FUltimateNight W;
			W.StartWith(EMode::Angel, 22u, {AngelId});
			std::vector<FEvent> Log;
			W.SetCamera(true);
			W.SetCamera(false);
			W.SetCamera(true);
			W.SetCamera(false);
			RunIdle(W, 3.0, Log);
			Expect(W.Killer() == AngelId && std::fabs(Log.back().Time - 1.5) < 0.11, "Angel kills after 1.5 s");
		}
	}

	// ======================================================================================== scheduler
	void TestScheduler()
	{
		FUltimateNight W;
		W.StartWith(EMode::Angel, 30u, {});
		FDutySpec Media;
		Media.Subject = MediaId;
		Media.Window = 1.0;
		Media.Side = 0;
		FDutySpec Myrcia;
		Myrcia.Subject = MyrciaId;
		Myrcia.Window = 1.5;
		Myrcia.Side = 1;
		FDutySpec Death;
		Death.Subject = DeathId;
		Death.Kind = EDuty::ClickOffice;
		Death.Clicks = 4;
		Death.Window = 2.0;
		Expect(W.OpenDuty(Media) >= 0 && W.OpenDuty(Myrcia) >= 0, "two door threats that fit are both allowed");
		const int Deferred = W.Stats().Deferrals;
		Expect(W.OpenDuty(Death) < 0 && W.Stats().Deferrals == Deferred + 1, "a third that the player could not also answer is delayed");
		// Reaction budget is configurable: a faster budget makes the same overlap feasible.
		FUcnTuning Fast;
		Fast.ReactionBudget = 0.1;
		FUltimateNight F;
		F.StartWith(EMode::Angel, 30u, {}, Fast);
		Expect(F.OpenDuty(Media) >= 0 && F.OpenDuty(Myrcia) >= 0 && F.OpenDuty(Death) >= 0, "ReactionBudget 0.1 s: all three fit");
		// Work model.
		Expect(std::fabs(W.WorkFor(EDuty::CloseDoor, 1) - (W.Tuning().ReactionBudget + W.Tuning().ActionSeconds)) < 1e-9, "door = reaction + one action");
		Expect(std::fabs(W.Tuning().ReactionBudget - 0.35) < 1e-9, "default reaction budget 0.35 s");
		// A door that cannot close is never the answer to a lethal threat.
		FUltimateNight B;
		B.StartWith(EMode::Angel, 31u, {});
		B.BreakDoor(ESide::Left);
		Expect(B.OpenDuty(Media) < 0, "no lethal door threat on a broken door");
		// Bounded retries: an attack that cannot be placed is skipped after DeferCap, never forced; the night never hangs.
		const FNightResult Capped = RunNight(EMode::Angel, 1u, EBotKind::DoNothing, FUcnTuning(), 30.0);
		Expect(Capped.Seconds <= 30.0 + 1e-6, "RunNight has a hard time cap");
	}

	// ======================================================================================== fairness contract
	void TestFairness()
	{
		// Stated windows per subject; every lethal telegraph must give at least that.
		auto Stated = [](int Subject, int Kind, const FUcnTuning& T) -> double
		{
			switch (Subject)
			{
			case MediaId: return 1.0;
			case MyrciaId: return 1.5;
			case AngelId: return 1.5;
			case FujitaId: return 5.0;
			case DeathId: return Kind == static_cast<int>(EDuty::ClickOffice) ? 2.0 : T.DeathRushWindow;
			case StargazerId: return 0.5;
			case NarratorId: return Kind == static_cast<int>(EDuty::ClickOffice) ? T.MiniWindow : T.DianoWindow;
			case BloodId: return T.BloodPhaseMin;
			case WeatherId: return T.WeatherWindow;
			case LunaId:
			case PlackId:
			case AritaId: return T.DoorWindowMin;
			default: break;
			}
			return 0.0;
		};
		const FUcnTuning T;
		for (int M = 0; M < ModeCount; ++M)
		{
			int Lethal = 0;
			bool bOk = true;
			for (uint32_t Seed = 1; Seed <= 10; ++Seed)
			{
				FUltimateNight W;
				W.Start(static_cast<EMode>(M), Seed);
				std::vector<FEvent> Log;
				RunPerfect(W, NightHours * NightHourSeconds + 1.0, Log, Seed);
				for (const FEvent* Event : Of(Log, EEvent::Telegraph))
				{
					const double Need = Stated(Event->A, Event->B, T);
					if (Need > 0.0)
					{
						++Lethal;
						bOk = bOk && Event->D + 1 >= static_cast<int>(Need * 1000.0);
					}
				}
				Expect(W.Stats().UntelegraphedKills == 0 && W.Stats().ShortWindowKills == 0, "no hidden or short-window kill");
			}
			Expect(bOk && Lethal > 0, std::string("every lethal telegraph gives at least the stated window - ") + ModeName(static_cast<EMode>(M)));
		}
	}

	// ======================================================================================== the account side
	void TestReward()
	{
		FProfile Profile(1);
		Profile.Add(ECurrency::ArcadeToken, 10);
		const int Start = Profile.Get(ECurrency::Diamonds);
		// Pre-unlock attempt: cannot start, and a win reported anyway pays nothing.
		Expect(!IsUnlocked(Profile) && !StartUcn(Profile, EMode::Easy).bOk, "locked until Chapter 13 is cleared");
		FinishUcn(Profile, EMode::Easy, true, NightHours);
		Expect(Profile.Get(ECurrency::Diamonds) == Start && !IsRewardClaimed(Profile), "no reward before the unlock");

		Profile.StagesCleared = TotalCampaignLevels;   // the first completion of Chapter 13
		Expect(IsUnlocked(Profile) && StartUcn(Profile, EMode::Angel).bOk, "Chapter 13 cleared unlocks it");
		// A loss pays nothing.
		FinishUcn(Profile, EMode::Angel, false, 3);
		Expect(Profile.Get(ECurrency::Diamonds) == Start && !IsRewardClaimed(Profile), "a loss pays nothing");
		Expect(Profile.Meta.ArcadeRecords.UcnBestHour[static_cast<int>(EMode::Angel)] == 3, "best hour recorded");
		// First win of any mode: 3000 Diamonds, once.
		StartUcn(Profile, EMode::Medium);
		FinishUcn(Profile, EMode::Medium, true, NightHours);
		Expect(Profile.Get(ECurrency::Diamonds) == Start + RewardDiamonds && IsRewardClaimed(Profile), "first win pays 3000 Diamonds");
		// Double claim: same mode, another mode, and the same call again.
		FinishUcn(Profile, EMode::Medium, true, NightHours);
		FinishUcn(Profile, EMode::Angel, true, NightHours);
		Expect(Profile.Get(ECurrency::Diamonds) == Start + RewardDiamonds, "never paid twice (idempotent)");
		Expect(Profile.Meta.ArcadeRecords.UcnWins[static_cast<int>(EMode::Medium)] == 2, "wins still counted");

		// The claim lives in the account save.
		const std::string Text = Save::Serialize(Profile, 1760400000);
		FProfile Loaded(2);
		std::string Error;
		Expect(Save::Deserialize(Text, Loaded, Error), "save loads: " + Error);
		Expect(IsRewardClaimed(Loaded) && Loaded.Meta.ArcadeRecords.bUcnUnlocked, "claim and unlock survive a save round trip");
		Loaded.Add(ECurrency::ArcadeToken, 1);
		const int Before = Loaded.Get(ECurrency::Diamonds);
		FinishUcn(Loaded, EMode::Easy, true, NightHours);
		Expect(Loaded.Get(ECurrency::Diamonds) == Before, "a loaded account that already claimed is not paid again");
		// The unlock is latched even if the campaign counter is lower later (e.g. a reset tester path).
		Loaded.StagesCleared = 0;
		Expect(IsUnlocked(Loaded), "the unlock is latched in the save");

		// A real night through the world: the perfect bot's Easy win pays on a fresh unlocked account.
		FProfile Fresh(3);
		Fresh.SetRaw(ECurrency::ArcadeToken, 1);
		Fresh.StagesCleared = TotalCampaignLevels;
		const int FreshStart = Fresh.Get(ECurrency::Diamonds);
		Expect(StartUcn(Fresh, EMode::Easy).bOk, "start Easy");
		FUltimateNight W;
		W.Start(EMode::Easy, 1u);
		std::vector<FEvent> Log;
		RunPerfect(W, NightHours * NightHourSeconds + 2.0, Log, 1u);
		const std::string Text2 = FinishUcn(Fresh, W);
		Expect(W.IsWon() && Fresh.Get(ECurrency::Diamonds) == FreshStart + RewardDiamonds, "a won night pays the first-win reward: " + Text2);
		Expect(!StartUcn(Fresh, EMode::Easy).bOk, "one arcade token per play");
	}

	// ======================================================================================== hub pages
	void TestPages()
	{
		FProfile Profile(4);
		Profile.SetRaw(ECurrency::ArcadeToken, 3);
		P::FUiState State;
		State.Tab = P::TabLobby;
		State.Page = "arcade";
		const P::FPage Before = P::Build(Profile, State);
		const P::FPageAction* Locked = Find(Before, "arc:ucn");
		Expect(Locked != nullptr && !Locked->bEnabled, "the arcade lists Ultimate Custom Night, locked before Chapter 13");
		P::Perform(Profile, State, "arc:ucn");
		Expect(State.Page == "arcade", "a locked tile does not open the page");
		Profile.StagesCleared = TotalCampaignLevels;
		const P::FPage After = P::Build(Profile, State);
		const P::FPageAction* Open = Find(After, "arc:ucn");
		Expect(Open != nullptr && Open->bEnabled && Open->bBadge, "unlocked: the first-win reward is badged");
		P::Perform(Profile, State, "arc:ucn");
		Expect(State.Page == "arcade-ucn", "the mode page opens");
		const P::FPage Modes = P::Build(Profile, State);
		Expect(Find(Modes, "arc:ucngo:0") && Find(Modes, "arc:ucngo:1") && Find(Modes, "arc:ucngo:2") && Find(Modes, "arc:ucngo:3") && Modes.bCanBack, "four modes");
		const P::FPerform Go = P::Perform(Profile, State, "arc:ucngo:3");
		Expect(Go.Overlay == P::EOverlay::UltimateNight && Go.OverlayA == 3 && Profile.Get(ECurrency::ArcadeToken) == 2, "Angel opens the overlay for a token");
		P::Perform(Profile, State, "back");
		Expect(State.Page == "arcade", "back returns to the arcade");
	}
}

int main()
{
	TestRoster();
	TestNightFrame();
	TestWalkers();
	TestKuu();
	TestIshii();
	TestFujita();
	TestShiraishi();
	TestMyrcia();
	TestAlchemist();
	TestBlood();
	TestWeather();
	TestStargazer();
	TestGambler();
	TestJudge();
	TestNarrator();
	TestJester();
	TestMedia();
	TestTimeKeeper();
	TestDeath();
	TestAdapt();
	TestAngel();
	TestScheduler();
	TestFairness();
	TestReward();
	TestPages();
	std::printf("%d Ultimate Custom Night checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
