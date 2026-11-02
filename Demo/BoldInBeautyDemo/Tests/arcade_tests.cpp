// Tests for the 2026-10-06 owner request: the arcade (Night Shift, Rhythm, Boss Rush), the arena and battle power, the endless
// tower, live operations (monthly seasons, the season pass pace, returning players), the online client (friends, gangs, mail,
// notices, arena) and the new hub pages. Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
//   arcade_tests --balance   also prints the Night Shift survival table of the balance bot.
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArcade.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBArena.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBJson.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBLiveOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBMetaOps.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBOnline.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBPages.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSave.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBSynth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

namespace
{
	int GFailures = 0;
	int GChecks = 0;
	bool GPrintBalance = false;

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
	namespace P = BIB::Pages;

	bool Has(const std::string& Text, const std::string& Part)
	{
		return Text.find(Part) != std::string::npos;
	}

	const P::FPageAction* Find(const P::FPage& Page, const std::string& Id)
	{
		for (const P::FPageAction& Action : Page.Grid)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		for (const P::FPageAction& Action : Page.Bar)
		{
			if (Action.Id == Id)
			{
				return &Action;
			}
		}
		return nullptr;
	}

	bool PageHas(const P::FPage& Page, const std::string& Part)
	{
		if (Has(Page.Title, Part) || Has(Page.Subtitle, Part))
		{
			return true;
		}
		for (const std::string& Line : Page.Lines)
		{
			if (Has(Line, Part))
			{
				return true;
			}
		}
		return false;
	}

	Json::FValue Parse(const std::string& Text)
	{
		Json::FValue Value;
		std::string Error;
		const bool bOk = Json::Parse(Text, Value, Error);
		Expect(bOk, "test JSON parses: " + Error);
		return Value;
	}

	constexpr int64_t Oct6 = 1791244800;   // 2026-10-06 00:00:00 UTC
	constexpr int64_t Day = 86400;

	// ============================================================================ Night Shift: owner rules
	void TestNightRules()
	{
		const FNightRules Easy = NightRules(EDifficulty::Easy);
		const FNightRules Medium = NightRules(EDifficulty::Medium);
		const FNightRules Hard = NightRules(EDifficulty::Hard);
		Expect(Easy.MoveChance == 0.5 && Easy.OpportunitySeconds == 7.0, "owner: Easy = 50% every 7 s");
		Expect(Medium.MoveChance == 0.75 && Medium.OpportunitySeconds == 6.0, "owner: Medium = 75% every 6 s");
		Expect(Hard.MoveChance == 1.0 && Hard.OpportunitySeconds == 5.0, "owner: Hard = 100% every 5 s");
		Expect(Easy.MediaWindow == 2.0 && Medium.MediaWindow == 1.5 && Hard.MediaWindow == 1.0, "owner: Media window 2 / 1.5 / 1 s");
		Expect(KuuWindupSeconds == 5.0, "owner: Kuu winds up for 5 seconds");

		FNightShift Night;
		Night.Start(EDifficulty::Hard, 1);
		Expect(Night.Power() == 100.0 && !Night.IsDoorClosed(ESide::Left) && !Night.IsDoorClosed(ESide::Right), "100% power, two open doors");
		Expect(Night.ClockText() == "12 AM" && Night.Hour() == 0, "the night starts at 12 AM");
		Night.Tick(NightGraceSeconds - 1.0);
		Expect(Night.Opportunities() == 0 && Night.RoomOf(ENightChar::Luna) == ERoom::Stage, "nobody moves during the grace period");
		const double Before = Night.Power();
		Night.Tick(6.0);
		Expect(std::fabs(Before - Night.Power() - 6.0 * PowerDrainPerSecond) < 1e-6, "usage 1 drains the base rate");
		Expect(Night.Opportunities() >= NightCharCount, "opportunities start after the grace period");

		// Doors and usage.
		FNightShift Usage;
		Usage.Start(EDifficulty::Easy, 2);
		Usage.SetDoor(ESide::Left, true);
		Usage.SetDoor(ESide::Right, true);
		Usage.SetLight(ESide::Left, true);
		Usage.SetCamera(true);
		Expect(Usage.Usage() == 5, "two doors, a light and the camera = usage 5");
		const double P0 = Usage.Power();
		Usage.Tick(1.0);
		Expect(std::fabs(P0 - Usage.Power() - 5.0 * PowerDrainPerSecond) < 1e-6, "usage multiplies the drain");

		// Luna at the left door: an open door is fatal on her next successful roll; a closed one sends her back.
		FNightShift Luna;
		Luna.Start(EDifficulty::Hard, 3);
		Luna.Tick(NightGraceSeconds);
		Luna.ForceRoom(ENightChar::Luna, ERoom::LeftDoor);
		Luna.SetDoor(ESide::Left, true);
		Luna.SetLight(ESide::Left, true);
		const std::vector<ENightChar> Seen = Luna.Visible();
		Expect(std::find(Seen.begin(), Seen.end(), ENightChar::Luna) != Seen.end(), "the light shows Luna at the left door");
		bool bKnock = false;
		for (int Step = 0; Step < 200 && !bKnock && !Luna.IsOver(); ++Step)
		{
			Luna.Tick(0.05);
			for (const FNightEvent& Event : Luna.PopEvents())
			{
				bKnock = bKnock || (Event.Kind == ENightEventKind::Knock && Event.B == static_cast<int>(ENightChar::Luna));
			}
		}
		Expect(bKnock && !Luna.IsOver() && Luna.RoomOf(ENightChar::Luna) == ERoom::Dining, "a closed door sends Luna back to the dining hall");
		FNightShift Open;
		Open.Start(EDifficulty::Hard, 4);
		Open.Tick(NightGraceSeconds);
		Open.ForceRoom(ENightChar::Plack, ERoom::RightDoor);
		Open.Tick(Hard.OpportunitySeconds + 0.1);
		Expect(Open.IsOver() && !Open.IsWon() && Open.Killer() == ENightChar::Plack, "Plack walks in through an open right door");

		// Kuu: a closed door breaks, an open doorway is a miss.
		FNightShift Kuu;
		Kuu.Start(EDifficulty::Hard, 5);
		Kuu.Tick(NightGraceSeconds);
		Kuu.ForceRoom(ENightChar::Kuu, ERoom::WestCorner);
		bool bWound = false;
		for (int Step = 0; Step < 400 && !bWound; ++Step)
		{
			Kuu.Tick(0.05);
			bWound = Kuu.IsKuuWindingUp(ESide::Left) || Kuu.IsKuuWindingUp(ESide::Right);
		}
		const ESide KuuSide = Kuu.IsKuuWindingUp(ESide::Left) ? ESide::Left : ESide::Right;
		Expect(bWound, "Kuu reaches a door and winds up");
		Kuu.SetDoor(KuuSide, true);
		bool bBroke = false;
		for (int Step = 0; Step < 120 && !bBroke; ++Step)
		{
			Kuu.Tick(0.05);
			for (const FNightEvent& Event : Kuu.PopEvents())
			{
				bBroke = bBroke || (Event.Kind == ENightEventKind::KuuPunch && Event.B == 1);
			}
		}
		Expect(bBroke && Kuu.IsDoorBroken(KuuSide) && !Kuu.IsDoorClosed(KuuSide), "Kuu's punch breaks a closed door");
		Expect(!Kuu.SetDoor(KuuSide, true) && !Kuu.IsDoorClosed(KuuSide), "a broken door cannot close");
		Expect(std::fabs(Kuu.DoorRepairLeft(KuuSide) - NightRules(EDifficulty::Hard).DoorRepairSeconds) < 0.1, "the repair takes the mode's repair time");
		for (int Step = 0; Step < 600 && Kuu.IsDoorBroken(KuuSide); ++Step)
		{
			for (ENightChar Char : {ENightChar::Luna, ENightChar::Plack, ENightChar::Narrator})
			{
				Kuu.ForceRoom(Char, ERoom::Stage);   // keep the others away while the door is repaired
			}
			Kuu.ForceRoom(ENightChar::Kuu, ERoom::Backroom);
			Kuu.Tick(0.05);
		}
		Expect(!Kuu.IsOver() && !Kuu.IsDoorBroken(KuuSide) && Kuu.SetDoor(KuuSide, true), "the door works again after the repair time");

		FNightShift Miss;
		Miss.Start(EDifficulty::Hard, 6);
		Miss.Tick(NightGraceSeconds);
		Miss.ForceRoom(ENightChar::Kuu, ERoom::EastCorner);
		bool bMissed = false;
		for (int Step = 0; Step < 600 && !bMissed && !Miss.IsOver(); ++Step)
		{
			Miss.Tick(0.05);
			for (const FNightEvent& Event : Miss.PopEvents())
			{
				bMissed = bMissed || (Event.Kind == ENightEventKind::KuuPunch && Event.B == 0);
			}
		}
		Expect(bMissed && !Miss.IsDoorBroken(ESide::Left) && !Miss.IsDoorBroken(ESide::Right), "Kuu hits nothing through an open doorway");

		// Media: close the door within the window.
		for (int D = 0; D < DifficultyCount; ++D)
		{
			const EDifficulty Diff = static_cast<EDifficulty>(D);
			FNightShift Safe;
			Safe.Start(Diff, 7);
			Safe.ForceMediaTeleport(ESide::Right);
			Safe.Tick(NightRules(Diff).MediaWindow - 0.2);
			Safe.SetDoor(ESide::Right, true);
			Safe.Tick(0.1);
			Expect(!Safe.IsOver() && !Safe.IsMediaAtDoor(ESide::Right), std::string("Media leaves when the door closes in time: ") + DifficultyName(Diff));
			FNightShift Late;
			Late.Start(Diff, 8);
			Late.ForceMediaTeleport(ESide::Left);
			Late.Tick(NightRules(Diff).MediaWindow + 0.06);
			Expect(Late.IsOver() && Late.Killer() == ENightChar::Media, std::string("Media wins after the window: ") + DifficultyName(Diff));
		}
		FNightShift Arrived;
		Arrived.Start(EDifficulty::Easy, 9);
		Arrived.ForceMediaTeleport(ESide::Left);
		bool bCue = false;
		for (const FNightEvent& Event : Arrived.PopEvents())
		{
			bCue = bCue || (Event.Kind == ENightEventKind::MediaArrived && Event.A == 0);
		}
		Expect(bCue, "Media's arrival is an event with its side (the overlay pans the sound)");

		// Narrator: watching her camera stops her; at the right door she never leaves.
		FNightShift Watched;
		Watched.Start(EDifficulty::Hard, 10);
		Watched.SelectCamera(static_cast<int>(ERoom::Stage));
		Watched.SetCamera(true);
		Watched.Tick(120.0);
		Expect(Watched.RoomOf(ENightChar::Narrator) == ERoom::Stage, "Narrator never moves while her camera is watched");
		FNightShift Free;
		Free.Start(EDifficulty::Hard, 10);
		Free.SelectCamera(static_cast<int>(ERoom::Kitchen));
		for (int Step = 0; Step < 400 && Free.RoomOf(ENightChar::Narrator) == ERoom::Stage && !Free.IsOver(); ++Step)
		{
			Free.Tick(0.25);
		}
		Expect(Free.RoomOf(ENightChar::Narrator) != ERoom::Stage || Free.IsOver(), "Narrator moves when nobody watches");
		FNightShift Door;
		Door.Start(EDifficulty::Hard, 11);
		Door.Tick(NightGraceSeconds);
		Door.ForceRoom(ENightChar::Narrator, ERoom::RightDoor);
		Door.SetDoor(ESide::Right, true);
		Door.Tick(60.0);
		Expect(Door.IsOver() || Door.RoomOf(ENightChar::Narrator) == ERoom::RightDoor, "Narrator stays at the closed right door");
		FNightShift Opened;
		Opened.Start(EDifficulty::Hard, 12);
		Opened.Tick(NightGraceSeconds);
		Opened.ForceRoom(ENightChar::Narrator, ERoom::RightDoor);
		Opened.Tick(Hard.OpportunitySeconds + 0.1);
		Expect(Opened.IsOver() && Opened.Killer() == ENightChar::Narrator, "opening the door on Narrator is fatal");

		// Power out: Narrator strikes a little later unless 6 AM comes first.
		FNightShift Dark;
		Dark.Start(EDifficulty::Easy, 13);
		Dark.SetDoor(ESide::Left, true);
		Dark.SetDoor(ESide::Right, true);
		Dark.SetLight(ESide::Left, true);
		Dark.SetLight(ESide::Right, true);
		Dark.SelectCamera(static_cast<int>(ERoom::Stage));
		Dark.SetCamera(true);
		for (int Step = 0; Step < 4000 && !Dark.IsPowerOut() && !Dark.IsOver(); ++Step)
		{
			Dark.Tick(0.05);
		}
		Expect(Dark.IsPowerOut() || Dark.IsOver(), "everything on drains the power");
		if (Dark.IsPowerOut() && !Dark.IsOver())
		{
			Expect(!Dark.IsDoorClosed(ESide::Left) && !Dark.IsCameraUp() && !Dark.SetDoor(ESide::Left, true), "power out: doors open, camera off, nothing works");
			Dark.Tick(20.0);
			Expect(Dark.IsOver() && Dark.Killer() == ENightChar::Narrator, "Narrator strikes in the dark");
		}

		// Surviving: an empty night (everyone forced away) reaches 6 AM.
		FNightShift Win;
		Win.Start(EDifficulty::Easy, 14);
		Win.SelectCamera(static_cast<int>(ERoom::Stage));
		for (int Step = 0; Step < 100000 && !Win.IsOver(); ++Step)
		{
			Win.ForceRoom(ENightChar::Luna, ERoom::Stage);
			Win.ForceRoom(ENightChar::Plack, ERoom::Stage);
			Win.ForceRoom(ENightChar::Kuu, ERoom::Backroom);
			Win.ForceRoom(ENightChar::Narrator, ERoom::Stage);
			if (Win.IsMediaAtDoor(ESide::Left) || Win.IsMediaAtDoor(ESide::Right))
			{
				Win.SetDoor(Win.IsMediaAtDoor(ESide::Left) ? ESide::Left : ESide::Right, true);
			}
			Win.Tick(0.5);
			Win.SetDoor(ESide::Left, false);
			Win.SetDoor(ESide::Right, false);
		}
		Expect(Win.IsWon() && Win.Hour() == NightHours, "6 AM wins the night");

		// Profile side: a won night pays and records.
		FProfile Profile(1);
		const int Gold = Profile.Get(ECurrency::Gold);
		const std::string Text = FinishNight(Profile, Win);
		Expect(Profile.Get(ECurrency::Gold) == Gold + NightReward(EDifficulty::Easy, true, 6).Gold && Profile.Meta.ArcadeRecords.NightWins[0] == 1
			&& Profile.Meta.ArcadeRecords.NightBestHour[0] == 6 && Has(Text, "survived"), "a won night pays and is recorded");
		Expect(NightReward(EDifficulty::Hard, true, 6).Gold > NightReward(EDifficulty::Medium, true, 6).Gold
			&& NightReward(EDifficulty::Medium, true, 6).Gold > NightReward(EDifficulty::Easy, true, 6).Gold, "harder nights pay more");
	}

	// ============================================================================ Night Shift: balance bot
	// A careful player with a human reaction time: checks a light now and then, closes doors when someone is there, keeps a
	// door open for Kuu when nothing else threatens it, reacts to Media's sound, and keeps an eye on Narrator.
	struct FBotResult
	{
		bool bWon = false;
		ENightChar Killer = ENightChar::Count;
		double Power = 0.0;
	};

	FBotResult PlayNight(EDifficulty Diff, uint32_t Seed)
	{
		FNightShift Night;
		Night.Start(Diff, Seed);
		std::mt19937 Rng(Seed * 7919u + 13u);
		std::uniform_real_distribution<double> U(0.0, 1.0);
		const double Dt = 0.05;
		double Reaction[2] = {-1.0, -1.0};      // when a decision on that door becomes due
		double MediaReact = -1.0;
		double NextGlance = 0.0;
		double LookUntil = 0.0;
		while (!Night.IsOver())
		{
			const double T = Night.Time();
			// Glance at Narrator's camera regularly (a short look), otherwise keep the camera parked on her.
			const ERoom NarratorRoom = Night.RoomOf(ENightChar::Narrator);
			Night.SelectCamera(std::clamp(static_cast<int>(NarratorRoom), 0, CameraCount - 1));
			if (T >= NextGlance)
			{
				LookUntil = T + 1.5;
				NextGlance = T + 6.0 + 4.0 * U(Rng);
			}
			Night.SetCamera(T < LookUntil && NarratorRoom < ERoom::LeftDoor);
			for (int Side = 0; Side < 2; ++Side)
			{
				const ESide S = static_cast<ESide>(Side);
				std::vector<ENightChar> AtDoor = Night.AtDoor(S);
				bool bThreat = false;
				bool bKuuOnly = false;
				for (ENightChar Char : AtDoor)
				{
					bThreat = bThreat || Char == ENightChar::Luna || Char == ENightChar::Plack || Char == ENightChar::Narrator;
					bKuuOnly = bKuuOnly || Char == ENightChar::Kuu;
				}
				const bool bWantClosed = bThreat;   // with only Kuu there, leave it open so her punch misses
				if (bWantClosed != Night.IsDoorClosed(S))
				{
					if (Reaction[Side] < 0.0)
					{
						Reaction[Side] = T + 0.6 + 0.9 * U(Rng);   // the time to notice (light check) and act
					}
					if (T >= Reaction[Side])
					{
						Night.SetDoor(S, bWantClosed);
						Reaction[Side] = -1.0;
					}
				}
				else
				{
					Reaction[Side] = -1.0;
				}
				(void)bKuuOnly;
			}
			if (Night.IsMediaAtDoor(ESide::Left) || Night.IsMediaAtDoor(ESide::Right))
			{
				const ESide S = Night.IsMediaAtDoor(ESide::Left) ? ESide::Left : ESide::Right;
				if (MediaReact < 0.0)
				{
					MediaReact = T + 0.45 + 0.6 * U(Rng);   // hearing which side the sound comes from
				}
				if (T >= MediaReact)
				{
					Night.SetDoor(S, true);
				}
			}
			else
			{
				MediaReact = -1.0;
			}
			Night.Tick(Dt);
		}
		FBotResult Result;
		Result.bWon = Night.IsWon();
		Result.Killer = Night.Killer();
		Result.Power = Night.Power();
		return Result;
	}

	void TestNightBalance()
	{
		const int Nights = 300;
		double Rates[DifficultyCount] = {};
		for (int D = 0; D < DifficultyCount; ++D)
		{
			int Won = 0;
			int Killers[NightCharCount + 1] = {};
			double Power = 0.0;
			for (int Seed = 1; Seed <= Nights; ++Seed)
			{
				const FBotResult Result = PlayNight(static_cast<EDifficulty>(D), static_cast<uint32_t>(Seed + 1000 * D));
				Won += Result.bWon ? 1 : 0;
				Power += Result.bWon ? Result.Power : 0.0;
				++Killers[std::clamp(static_cast<int>(Result.Killer), 0, NightCharCount)];
			}
			Rates[D] = static_cast<double>(Won) / Nights;
			if (GPrintBalance)
			{
				std::printf("Night Shift %-6s: bot survives %5.1f%% (avg power left %4.1f%%) | lost to Luna %d, Plack %d, Kuu %d, Media %d, Narrator %d\n",
					DifficultyName(static_cast<EDifficulty>(D)), Rates[D] * 100.0, Won > 0 ? Power / Won : 0.0, Killers[0], Killers[1], Killers[2], Killers[3], Killers[4]);
			}
		}
		// Targets (PLACEHOLDER, reviewed with the owner): a careful player nearly always survives Easy, usually Medium, and
		// Hard is a real challenge that is still possible.
		Expect(Rates[0] >= 0.85, "Easy: the careful bot survives at least 85% of nights (" + std::to_string(Rates[0]) + ")");
		Expect(Rates[1] >= 0.45 && Rates[1] <= 0.95, "Medium: the bot survives 45-95% (" + std::to_string(Rates[1]) + ")");
		Expect(Rates[2] >= 0.08 && Rates[2] <= 0.65, "Hard: the bot survives 8-65% (" + std::to_string(Rates[2]) + ")");
		Expect(Rates[0] > Rates[1] && Rates[1] > Rates[2], "each mode is harder than the last");
	}

	// ============================================================================ Rhythm
	void TestRhythm()
	{
		for (int Song = 0; Song < SongCount(); ++Song)
		{
			for (int D = 0; D < DifficultyCount; ++D)
			{
				const EDifficulty Diff = static_cast<EDifficulty>(D);
				const std::vector<FNote> Chart = MakeChart(Song, Diff);
				const std::vector<FNote> Again = MakeChart(Song, Diff);
				Expect(!Chart.empty() && Chart.size() == Again.size() && Chart.back().Time == Again.back().Time, "charts are deterministic");
				bool bSorted = true;
				bool bLanes = true;
				for (size_t Index = 0; Index < Chart.size(); ++Index)
				{
					bSorted = bSorted && (Index == 0 || Chart[Index].Time > Chart[Index - 1].Time);
					bLanes = bLanes && Chart[Index].Lane >= 0 && Chart[Index].Lane < LanesFor(Diff);
				}
				Expect(bSorted && bLanes && Chart.front().Time >= RhythmLeadIn && Chart.back().Time < SongLength(Song), "notes are in order, in range, inside the song");
				// A perfect player.
				FRhythm Perfect;
				Perfect.Start(Song, Diff);
				size_t Next = 0;
				while (!Perfect.IsOver())
				{
					while (Next < Chart.size() && Chart[Next].Time <= Perfect.Time() + 0.0001)
					{
						Perfect.Press(Chart[Next].Lane);
						++Next;
					}
					Perfect.Tick(0.004);
				}
				Expect(Perfect.IsPassed() && Perfect.Stars() == 5 && Perfect.Count(EJudge::Miss) == 0 && Perfect.MaxCombo() == static_cast<int>(Chart.size()),
					"a perfect run passes with 5 stars and a full combo");
				// Nobody playing.
				FRhythm Idle;
				Idle.Start(Song, Diff);
				while (!Idle.IsOver())
				{
					Idle.Tick(0.05);
				}
				Expect(Idle.IsFailed() && Idle.Stars() == 0 && Idle.Score() == 0, "ignoring the notes fails the song");
			}
			Expect(MakeChart(Song, EDifficulty::Hard).size() > MakeChart(Song, EDifficulty::Easy).size(), "Hard has more notes than Easy");
		}
		FRhythm Judge;
		Judge.Start(0, EDifficulty::Easy);
		const FNote First = Judge.Notes()[0];
		Judge.Tick(First.Time - 0.12);
		Expect(Judge.Press(First.Lane) == EJudge::Good, "120 ms early is Good");
		FRhythm Great;
		Great.Start(0, EDifficulty::Easy);
		Great.Tick(First.Time + 0.08);
		Expect(Great.Press(First.Lane) == EJudge::Great, "80 ms late is Great");
		FRhythm Empty;
		Empty.Start(0, EDifficulty::Easy);
		Empty.Tick(0.5);
		Expect(Empty.Press(0) == EJudge::Miss && Empty.Count(EJudge::Miss) == 0 && Empty.Combo() == 0, "an empty strum breaks the combo but is not a missed note");
		FRhythm Combo;
		Combo.Start(1, EDifficulty::Medium);
		size_t Hit = 0;
		while (Combo.Combo() < 12 && Hit < Combo.Notes().size())
		{
			const FNote Note = Combo.Notes()[Hit++];
			Combo.Tick(Note.Time - Combo.Time());
			Combo.Press(Note.Lane);
		}
		Expect(Combo.Multiplier() == 2, "10 in a row doubles the score");
		FProfile Profile(2);
		const int Gold = Profile.Get(ECurrency::Gold);
		FRhythm Done;
		Done.Start(2, EDifficulty::Hard);
		for (const FNote& Note : MakeChart(2, EDifficulty::Hard))
		{
			Done.Tick(Note.Time - Done.Time());
			Done.Press(Note.Lane);
		}
		Done.Tick(100.0);
		const std::string Text = FinishRhythm(Profile, Done);
		Expect(Done.IsPassed() && Profile.Get(ECurrency::Gold) == Gold + RhythmReward(EDifficulty::Hard, true, Done.Stars()).Gold
			&& Profile.Meta.ArcadeRecords.RhythmBest[2 * DifficultyCount + 2] == static_cast<int>(Done.Score()) && Has(Text, "new best"), "a cleared song pays and records the best score");
	}

	// ============================================================================ Boss Rush
	void TestBossRush()
	{
		const int Id = BossRushStageId(40, 3);
		const FStageRef Ref = DecodeStage(Id);
		Expect(Ref.Kind == EStageKind::BossRush && Ref.A == 40 && Ref.B == 3 && IsValidStage(Id), "boss rush ids decode");
		Expect(!IsValidStage(BossRushStageId(40, 3) - 3) && Has(StageName(Id), "Boss Rush"), "round 0 is invalid; the name says Boss Rush");
		double LastHp = 0.0;
		for (int Round = 1; Round <= 30; ++Round)
		{
			const FSectionPlanData Plan = BossRushRound(Round);
			Expect(Plan.Bosses >= 1 && Plan.Total() <= 30, "every round has a boss " + std::to_string(Round));
			const FEnemyScale Scale = StageScales(BossRushStageId(10, Round));
			Expect(Scale.Hp > LastHp, "each round is tougher " + std::to_string(Round));
			LastHp = Scale.Hp;
			Expect(BossRushRoundReward(Round).Gold > BossRushRoundReward(std::max(1, Round - 1)).Gold || Round == 1, "later rounds pay more");
			Expect(StagePlan(BossRushStageId(10, Round)).size() == 1, "one arena per round");
		}
		Expect(BossRushRound(5).Bosses == 2 && BossRushRound(1).MiniBosses == 0, "two bosses every fifth round");
		Expect(StageScales(BossRushStageId(80, 1)).Hp > StageScales(BossRushStageId(5, 1)).Hp, "bosses scale with the player's campaign progress");
		FProfile Profile(3);
		const int Gold = Profile.Get(ECurrency::Gold);
		std::vector<std::string> Lines = BossRushRoundCleared(Profile, 1);
		Lines = BossRushRoundCleared(Profile, 2);
		Expect(Profile.Get(ECurrency::Gold) == Gold + BossRushRoundReward(1).Gold + BossRushRoundReward(2).Gold && Profile.Meta.ArcadeRecords.BossRushBest == 2, "every round pays; the best round is kept");
		{
			// 2026-10-09: Diamonds and tickets only on a new best round
			FProfile Rush(77);
			const int Tickets = Rush.Get(ECurrency::RegularTicket);
			for (int Round = 1; Round <= 10; ++Round)
			{
				BossRushRoundCleared(Rush, Round);
			}
			Expect(Rush.Get(ECurrency::RegularTicket) == Tickets + 1, "a first run to round 10 pays its ticket");
			const int Diamonds = Rush.Get(ECurrency::Diamonds);
			const int GoldBefore = Rush.Get(ECurrency::Gold);
			for (int Round = 1; Round <= 10; ++Round)
			{
				BossRushRoundCleared(Rush, Round);
			}
			Expect(Rush.Get(ECurrency::RegularTicket) == Tickets + 1 && Rush.Get(ECurrency::Diamonds) == Diamonds, "a repeat run pays no Diamonds or tickets");
			Expect(Rush.Get(ECurrency::Gold) > GoldBefore, "...but still pays Gold");
		}
		Expect(Has(BossRushOver(Profile, 3)[0], "2 boss rounds"), "the summary counts the rounds cleared");
		Expect(Meta::CanStartStage(Profile, BossRushStageId(1, 1)).bOk, "a playable squad can start Boss Rush");
		FProfile Empty(4);
		Empty.Team[0] = Empty.Team[1] = Empty.Team[2] = -1;
		Expect(!Meta::CanStartStage(Empty, BossRushStageId(1, 1)).bOk && !StartGame(Empty, EGame::BossRush).bOk, "Boss Rush needs a squad");
	}

	// ============================================================================ tower
	void TestTower()
	{
		Expect(IsValidStage(TowerStageId(0, 600)) && IsValidStage(TowerStageId(1, 250000)) && DecodeStage(TowerStageId(1, 250000)).B == 250000, "the tower goes far past floor 499");
		Expect(StageScales(TowerStageId(0, 101)).Hp > 6.9 && StageScales(TowerStageId(1, 1)).Hp > StageScales(TowerStageId(0, 1)).Hp, "+6% a floor; Secrets tougher");
		FProfile Profile(5);
		Profile.Meta.TowerBest[0] = 24;
		const std::vector<std::string> Lines = Meta::FailStage(Profile, TowerStageId(0, 25));
		Expect(Profile.Meta.TowerLostAt[0] == 25 && !Lines.empty() && Meta::TowerNextFloor(Profile, 0) == 25 && Profile.Meta.TowerBest[0] == 24, "a fall is saved and the climb resumes there");
		const FReward R25 = Meta::TowerFloorReward(0, 25);
		const FReward R50 = Meta::TowerFloorReward(1, 50);
		Expect(R25.Equipment == 1 && R25.Diamonds == 130 && R50.SpecialTickets == 1 && R50.RegularTickets == 1, "milestone floors pay extra");
		Expect(Meta::TowerFloorReward(1, 7).Gold > Meta::TowerFloorReward(0, 7).Gold, "Secrets mode pays 25% more");
		const int Gold = Profile.Get(ECurrency::Gold);
		Meta::ClearStage(Profile, TowerStageId(0, 25));
		Expect(Profile.Meta.TowerBest[0] == 25 && Profile.Get(ECurrency::Gold) == Gold + R25.Gold, "a new floor pays once");
	}

	// ============================================================================ battle power and arena
	void TestPowerAndArena()
	{
		FProfile Profile(6);
		const int Luna = FindOperator("Luna");
		const int Before = OperatorPower(Profile, Luna);
		Profile.Operators[static_cast<size_t>(Luna)].Level = 50;
		const int Level50 = OperatorPower(Profile, Luna);
		Profile.Operators[static_cast<size_t>(Luna)].UpgradeStars = 2;
		const int Starred = OperatorPower(Profile, Luna);
		Expect(Before > 1000 && Level50 > Before * 3 && Starred > Level50, "battle power grows with levels and upgrade stars");
		Expect(OperatorPower(Profile, FindOperator("Adapt")) == 0, "an operator not owned has no battle power");
		Expect(TeamBattlePower(Profile) == SquadPower(Profile, Profile.Team), "squad power sums the team");

		Expect(Arena::RankName(0) == "Bronze 1" && Arena::RankName(2) == "Bronze 3" && Arena::RankName(3) == "Silver 1" && Arena::RankName(9) == "Diamond 1"
			&& Arena::RankName(12) == "Platinum 1" && Arena::RankName(14) == "Platinum 3" && Arena::RankName(15) == "Champions", "the owner's rank ladder");
		Expect(Arena::RankOf(0) == 0 && Arena::RankOf(99) == 0 && Arena::RankOf(100) == 1 && Arena::RankOf(1499) == 14 && Arena::RankOf(1500) == 15 && Arena::RankOf(99999) == 15, "100 trophies a rank");
		Expect(Arena::TrophyDelta(1000, 1000) == 30 && Arena::TrophyDelta(500, 1500) == 50 && Arena::TrophyDelta(1500, 500) == 10, "beating a stronger player pays more");
		bool bFloor = false;
		Expect(Arena::ApplyTrophies(20, -30, bFloor) == 0 && !bFloor, "trophies never go below zero");
		Expect(Arena::ApplyTrophies(1490, 30, bFloor) == 1520 && bFloor, "reaching Champions sets the floor");
		Expect(Arena::ApplyTrophies(1520, -50, bFloor) == 1500, "a Champions player cannot fall below Champions in the season");
		Expect(Arena::RankAfterSeason(15) == 11 && Arena::RankAfterSeason(9) == 5 && Arena::RankAfterSeason(8) == 5 && Arena::RankAfterSeason(2) == 0, "seasons drop 3 ranks, 4 from Diamond up");
		for (int Rank = 1; Rank < Arena::RankCount; ++Rank)
		{
			Expect(Arena::SeasonReward(Rank).Diamonds > Arena::SeasonReward(Rank - 1).Diamonds, "higher ranks earn more");
		}

		FProfile Fighter(7);
		Arena::AutoFill(Fighter, false);
		Arena::AutoFill(Fighter, true);
		Expect(Arena::TeamIsValid(Fighter, Fighter.Meta.Arena.Attack) && Arena::TeamIsValid(Fighter, Fighter.Meta.Arena.Defence), "auto-filled teams are valid");
		const Arena::FLineup Mine = Arena::MakeLineup(Fighter, Fighter.Meta.Arena.Attack, "Me");
		for (uint32_t Seed = 1; Seed <= 30; ++Seed)
		{
			const Arena::FLineup Rival = Arena::MakeRival(Fighter, Seed);
			Expect(Rival.Team.size() == 3 && Rival.bBot && Rival.Power >= Mine.Power * 0.8 && Rival.Power <= Mine.Power * 1.2, "rivals have similar strength");
			const Arena::FBattle A = Arena::Simulate(Mine, Rival, Seed);
			const Arena::FBattle B = Arena::Simulate(Mine, Rival, Seed);
			Expect(A.bAttackerWon == B.bAttackerWon && A.Events.size() == B.Events.size() && !A.Events.empty() && A.Duration <= Arena::BattleTimeLimit, "battles are deterministic and end");
		}
		// A much stronger team wins almost always.
		Arena::FLineup Strong = Mine;
		for (Arena::FFighter& Unit : Strong.Team)
		{
			Unit.Hp *= 4.0;
			Unit.Attack *= 4.0;
		}
		int Wins = 0;
		for (uint32_t Seed = 1; Seed <= 100; ++Seed)
		{
			Wins += Arena::Simulate(Strong, Arena::MakeRival(Fighter, Seed), Seed).bAttackerWon ? 1 : 0;
		}
		Expect(Wins >= 95, "a team four times stronger wins at least 95 of 100");
		int Even = 0;
		for (uint32_t Seed = 1; Seed <= 200; ++Seed)
		{
			Even += Arena::Simulate(Mine, Mine, Seed).bAttackerWon ? 1 : 0;
		}
		Expect(Even > 60 && Even < 140, "a mirror match is close to even (" + std::to_string(Even) + "/200)");

		// Local season (guests).
		FProfile Guest(8);
		Guest.Meta.Arena.SeasonKey = 2026 * 12 + 8;
		Guest.Meta.Arena.Trophies = 1550;
		Guest.Meta.Arena.bChampionFloor = true;
		Arena::UpdateSeason(Guest, 2026 * 12 + 9);
		Expect(Guest.Meta.Arena.Trophies == 1100 && !Guest.Meta.Arena.bChampionFloor && Guest.Meta.Arena.PendingRewardRank == 15, "season end: Champions drops to Diamond 3, the reward waits");
		const int Diamonds = Guest.Get(ECurrency::Diamonds);
		Expect(Arena::ClaimSeasonReward(Guest).bOk && Guest.Get(ECurrency::Diamonds) == Diamonds + Arena::SeasonReward(15).Diamonds && !Arena::ClaimSeasonReward(Guest).bOk, "the season reward is claimed once");
	}

	// ============================================================================ live ops
	void TestLiveOps()
	{
		Expect(LiveOps::SeasonKeyOf(Oct6) == 2026 * 12 + 9 && LiveOps::SeasonCode(2026 * 12 + 9) == "2026-10" && LiveOps::SeasonLabel(2026 * 12 + 9) == "October 2026", "October 2026");
		Expect(LiveOps::SeasonKeyFromCode("2026-10") == 2026 * 12 + 9 && LiveOps::SeasonKeyFromCode("2026-13") == 0 && LiveOps::SeasonKeyFromCode("bad") == 0, "season codes parse");
		Expect(LiveOps::SeasonKeyOf(LiveOps::SeasonStartUnix(2027 * 12 + 1)) == 2027 * 12 + 1 && LiveOps::SeasonKeyOf(LiveOps::SeasonEndUnix(2027 * 12 + 1) - 1) == 2027 * 12 + 1,
			"a season runs from the 1st to the end of the month (February 2027)");
		Expect(LiveOps::DaysLeftInSeason(Oct6) == 26, "26 days left on 6 October");

		// Daily reset on the UTC day.
		FProfile Profile(9);
		LiveOps::UpdateClock(Profile, Oct6 + 3600);
		const int DayBefore = Profile.Day;
		Meta::NotifyQuest(Profile, Meta::EQuestEvent::Summon);
		LiveOps::UpdateClock(Profile, Oct6 + 7200);
		Expect(Profile.Day == DayBefore && Meta::DailyQuest(Profile, 2).bComplete, "same day: nothing resets");
		LiveOps::UpdateClock(Profile, Oct6 + Day + 60);
		Expect(Profile.Day == DayBefore + 1 && !Meta::DailyQuest(Profile, 2).bComplete && Profile.Get(ECurrency::DailyTicket) == 1, "a new UTC day resets the dailies");
		LiveOps::UpdateClock(Profile, Oct6 + 10 * Day);
		Expect(Profile.Day == DayBefore + 10, "days away still count for the weekly reset");

		// Season pass: one month; unclaimed tiers are mailed.
		FProfile Pass(10);
		LiveOps::UpdateClock(Pass, Oct6);
		Pass.Meta.SeasonXp = 1000;
		Pass.Meta.bPremiumPass = true;
		Meta::ClaimSeasonTier(Pass, 1);
		const size_t Mails = Pass.Meta.Mail.size();
		LiveOps::UpdateClock(Pass, LiveOps::SeasonStartUnix(2026 * 12 + 10) + 10);
		Expect(Pass.Meta.SeasonXp == 0 && !Pass.Meta.bPremiumPass && !Pass.Meta.FreeClaimed[1] && Pass.Meta.SeasonPassKey == 2026 * 12 + 10, "a new month starts a new season pass");
		bool bMailed = false;
		for (size_t Index = Mails; Index < Pass.Meta.Mail.size(); ++Index)
		{
			bMailed = bMailed || (Has(Pass.Meta.Mail[Index].Title, "unclaimed") && Pass.Meta.Mail[Index].Reward.Gold > 0);
		}
		Expect(bMailed, "unclaimed tiers 2-10 are mailed at the season end");

		// Pace: finishing every daily and weekly quest reaches tier 50 on day 15 (owner: "around 15 days").
		FProfile Pace(11);
		Pace.Meta.ArenaAttempts = 0;
		int DayReached = 0;
		for (int DayNumber = 1; DayNumber <= 30 && DayReached == 0; ++DayNumber)
		{
			for (int Event = 0; Event < static_cast<int>(Meta::EQuestEvent::DailyDone); ++Event)
			{
				Meta::NotifyQuest(Pace, static_cast<Meta::EQuestEvent>(Event), 30);
			}
			Meta::ClaimAllQuests(Pace);
			if (Meta::SeasonTier(Pace) >= SeasonTiers)
			{
				DayReached = DayNumber;
			}
			Pace.AdvanceDay();
		}
		Expect(DayReached == 15, "doing every daily and weekly quest reaches tier 50 on day 15 (got day " + std::to_string(DayReached) + ")");

		// Returning players.
		FProfile Away(12);
		Away.TutorialStep = static_cast<int>(ETutorialStep::Complete);
		LiveOps::UpdateClock(Away, Oct6);
		Away.SetTime(Oct6 + 13 * Day);
		LiveOps::UpdateClock(Away, Oct6 + 13 * Day);
		Expect(!LiveOps::ReturnActive(Away), "13 days away is not a return");
		const int64_t Back = Oct6 + 13 * Day + 14 * Day + 5;
		Away.SetTime(Back);
		const std::vector<std::string> Lines = LiveOps::UpdateClock(Away, Back);
		Expect(LiveOps::ReturnActive(Away) && LiveOps::ReturnDay(Away) == 1 && !Lines.empty(), "14 days away starts the welcome-back event");
		const FReward Total = LiveOps::ReturnQuestTotal();
		Expect(Total.RegularTickets == 5 && Total.Diamonds == 600 && Total.Gold > 0 && Total.Equipment > 0, "owner: Returned quests total 5 tickets and 600 Diamonds plus loot");
		for (int D = 1; D <= ReturnEventDays; ++D)
		{
			const FReward Reward = LiveOps::ReturnCheckInReward(D);
			const int Parts = (Reward.Gold > 0) + (Reward.Bytes > 0) + (Reward.Hearts > 0) + (Reward.ArcadeTokens > 0) + (Reward.Equipment > 0);
			Expect(Parts == 3, "owner: three rewards on check-in day " + std::to_string(D));
		}
		Expect(LiveOps::ClaimReturnCheckIn(Away, 1).bOk && !LiveOps::ClaimReturnCheckIn(Away, 1).bOk && !LiveOps::ClaimReturnCheckIn(Away, 2).bOk, "one check-in a day");
		Expect(LiveOps::ClaimReturnQuest(Away, 0).bOk, "the first Returned quest completes with the check-in");
		Meta::NotifyQuest(Away, Meta::EQuestEvent::StageClear, 3);
		Expect(LiveOps::ReturnQuest(Away, 1).bComplete && LiveOps::ClaimReturnQuest(Away, 1).bOk, "stage clears count for the Returned quests");
		Away.SetTime(Back + 3 * Day);
		Expect(LiveOps::ReturnDay(Away) == 4 && LiveOps::ClaimReturnCheckIn(Away, 2).bOk && LiveOps::ClaimReturnCheckIn(Away, 4).bOk, "missed days can be caught up");
		Away.SetTime(Back + 7 * Day + 1);
		LiveOps::UpdateClock(Away, Back + 7 * Day + 1);
		Expect(!LiveOps::ReturnActive(Away) && Away.Meta.Return.StartUnix == 0, "the event ends after 7 days");

		// Offline Outer World production.
		FProfile World(13);
		World.Add(ECurrency::Gold, 100000);
		World.Add(ECurrency::Bytes, 100000);
		Meta::BuildOrUpgrade(World, 0);
		LiveOps::UpdateClock(World, Oct6);
		LiveOps::UpdateClock(World, Oct6 + 3 * 3600);
		double Stored = 0.0;
		for (const FBuildingState& Building : World.Meta.Buildings)
		{
			Stored += Building.StoredGold + Building.StoredBytes + Building.StoredHearts;
		}
		Expect(Stored > 0.0, "buildings produce while the game is closed");
	}

	// ============================================================================ online client
	void TestOnline()
	{
		FProfile Profile(14);
		Online::FCache Net;
		Net.bOnline = true;
		const Online::FCall Push = Online::PushProfile(Profile);
		Expect(Push.Verb == "PUT" && Push.Path == "/v1/profile" && Has(Push.Body, "\"power\"") && Has(Push.Body, "\"team\""), "profile push");
		Expect(Online::SearchGangs("Night Owls").Path == "/v1/gangs?q=Night%20Owls", "search text is URL-encoded");
		Expect(Online::AdminSendMail(Net.Admin).Path == "/v1/admin/mail" && Has(Online::AdminSendMail(Net.Admin).Body, "\"to\":\"all\""), "admin mail to all by default");
		Online::FAdminCompose One;
		One.bToAll = false;
		One.Target = "10000002";
		One.RewardPreset = 1;
		Expect(Has(Online::AdminSendMail(One).Body, "\"to\":\"10000002\"") && Has(Online::AdminSendMail(One).Body, "\"diamonds\":300"), "admin mail to one user with an attachment");

		// Friends.
		Online::FReplyResult R = Online::ApplyReply(Net, Profile, Online::GetFriends(), 200,
			Parse("{\"ok\":true,\"friends\":[{\"uid\":\"10000002\",\"name\":\"Nova\",\"level\":7,\"power\":12000,\"lastSeen\":\"2026-10-06T10:00:00.000Z\",\"gang\":\"Owls\"}],"
				"\"incoming\":[{\"uid\":\"10000003\",\"name\":\"Rook\"}],\"outgoing\":[]}"), false);
		Expect(R.bOk && Net.Friends.size() == 1 && Net.Friends[0].Name == "Nova" && Net.Incoming.size() == 1 && Net.bFriendsLoaded, "friends list parsed");
		R = Online::ApplyReply(Net, Profile, Online::RequestFriend("Rook"), 200, Parse("{\"ok\":true,\"status\":\"accepted\"}"), false);
		Expect(Has(R.Message, "now friends") && R.FollowUps.size() == 1 && R.FollowUps[0].Kind == Online::ECall::Friends, "a crossing request accepts and refreshes");
		R = Online::ApplyReply(Net, Profile, Online::RequestFriend("Ghost"), 404, Parse("{\"ok\":false,\"error\":\"not_found\"}"), false);
		Expect(!R.bOk && Has(R.Message, "Not found"), "server errors become friendly text");
		R = Online::ApplyReply(Net, Profile, Online::GetFriends(), 0, Json::FValue(), true);
		Expect(!R.bOk && Has(R.Message, "cannot be reached"), "a network error says the server is unreachable");

		// Gangs.
		Profile.Add(ECurrency::Gold, 50000);
		const int Gold = Profile.Get(ECurrency::Gold);
		const std::string GangJson = "{\"ok\":true,\"gang\":{\"id\":\"g1\",\"name\":\"Owls\",\"leaderName\":\"Me\",\"level\":2,\"maxMembers\":30,\"minLevel\":5,\"minPower\":0,\"open\":true,"
			"\"xp\":2500,\"season\":\"2026-10\",\"seasonPoints\":3200,\"milestonesClaimed\":[0],\"myRole\":\"leader\",\"members\":[{\"uid\":\"10000001\",\"name\":\"Me\",\"role\":\"leader\","
			"\"level\":9,\"power\":20000,\"contribution\":300,\"seasonContribution\":120,\"lastSeen\":\"2026-10-06T10:00:00.000Z\"}]}}";
		R = Online::ApplyReply(Net, Profile, Online::CreateGang("Owls", 5, 0, true), 200, Parse(GangJson), false);
		Expect(R.bOk && Net.bInGang && Net.Gang.Name == "Owls" && Net.Gang.Members == 1 && Net.Gang.MyRole == "leader" && Profile.Get(ECurrency::Gold) == Gold - Online::GangCreateGold,
			"founding a gang costs Gold and fills the cache");
		Profile.Meta.Gang.PendingActivity = 37;
		R = Online::ApplyReply(Net, Profile, Online::MyGang(), 200, Parse("{\"ok\":true,\"gang\":" + GangJson.substr(GangJson.find("\"gang\":") + 7, GangJson.size() - GangJson.find("\"gang\":") - 8)
			+ ",\"pendingCoins\":450,\"pendingSeason\":\"2026-09\"}"), false);
		Expect(Net.PendingCoins == 450 && R.FollowUps.size() == 1 && R.FollowUps[0].Kind == Online::ECall::GangContribute && R.FollowUps[0].Arg == 37, "offline activity is sent with the next gang refresh");
		R = Online::ApplyReply(Net, Profile, R.FollowUps[0], 200, Parse("{\"ok\":true,\"added\":37}"), false);
		Expect(Profile.Meta.Gang.PendingActivity == 0, "sent activity is cleared");
		R = Online::ApplyReply(Net, Profile, Online::ClaimGangCoins(), 200, Parse("{\"ok\":true,\"coins\":450,\"season\":\"2026-09\"}"), false);
		Expect(Profile.Get(ECurrency::GangCoin) == 450 && Net.PendingCoins == 0, "season coins arrive");
		Expect(Online::BuyGangShop(Profile, 0, 1).bOk && Profile.Get(ECurrency::GangCoin) == 300 && Profile.Get(ECurrency::RegularTicket) >= 1, "the gang shop sells a ticket");
		Expect(!Online::BuyGangShop(Profile, 7, 1).bOk, "Diamonds unlock at gang level 5");
		for (int Buy = 0; Buy < 5; ++Buy)
		{
			Profile.Add(ECurrency::GangCoin, 1000);
			Online::BuyGangShop(Profile, 0, 1);
		}
		Expect(Profile.Meta.Gang.ShopBought[0] == 5 && !Online::BuyGangShop(Profile, 0, 1).bOk, "monthly stock limits");
		const int Hearts = Profile.Get(ECurrency::AngelHearts);
		R = Online::ApplyReply(Net, Profile, Online::ClaimGangMilestone(1), 200, Parse("{\"ok\":true}"), false);
		Expect(Profile.Get(ECurrency::AngelHearts) == Hearts + Online::GangMilestoneReward(1).Hearts, "a gang milestone pays every member");
		Expect(Online::GangSeasonCoins(0, 0) == 50 && Online::GangSeasonCoins(15000, 5000) == 1200 && Online::GangLevelFromXp(0) == 1 && Online::GangLevelFromXp(2000) == 3, "gang season and level formulas");
		Expect(Online::ValidGangName("Night Owls") && !Online::ValidGangName("ab") && !Online::ValidGangName("Owls!"), "gang names");

		// Mail import, de-duplicated, acknowledged.
		const std::string Mail = "{\"ok\":true,\"mail\":[{\"id\":\"m1\",\"title\":\"Gift\",\"body\":\"Thanks\",\"reward\":{\"diamonds\":300,\"bogus\":5},\"from\":\"Admin\"},"
			"{\"id\":\"m2\",\"title\":\"Note\",\"body\":\"No attachment\",\"reward\":{}}]}";
		const size_t Before = Profile.Meta.Mail.size();
		R = Online::ApplyReply(Net, Profile, Online::FetchMail(), 200, Parse(Mail), false);
		Expect(Profile.Meta.Mail.size() == Before + 2 && Profile.Meta.Mail.back().bClaimed && !Profile.Meta.Mail[Before].bClaimed && Profile.Meta.Mail[Before].From == "Admin"
			&& Profile.Meta.Mail[Before].Reward.Diamonds == 300, "server mail is imported (a letter without attachment is already 'claimed')");
		Expect(R.FollowUps.size() == 1 && R.FollowUps[0].Kind == Online::ECall::MailAck && Has(R.FollowUps[0].Body, "m1") && Has(R.FollowUps[0].Body, "m2"), "imported mail is acknowledged");
		Online::ApplyReply(Net, Profile, Online::FetchMail(), 200, Parse(Mail), false);
		Expect(Profile.Meta.Mail.size() == Before + 2, "the same mail is never imported twice");

		// Notices: unread ones open at login once.
		R = Online::ApplyReply(Net, Profile, Online::FetchNotices(), 200, Parse("{\"ok\":true,\"notices\":[{\"id\":\"n1\",\"category\":\"maintenance\",\"title\":\"Downtime\",\"body\":\"Soon\","
			"\"createdAt\":\"2026-10-06T10:00:00.000Z\",\"pinned\":true}]}"), false);
		Expect(R.bOpenNotices && Net.Notices.size() == 1 && Net.Notices[0].bPinned, "a new notice opens the board at login");
		R = Online::ApplyReply(Net, Profile, Online::FetchNotices(), 200, Parse("{\"ok\":true,\"notices\":[]}"), false);
		Expect(!R.bOpenNotices, "only once a session");

		// Arena.
		Arena::AutoFill(Profile, false);
		R = Online::ApplyReply(Net, Profile, Online::GetArena(), 200, Parse("{\"ok\":true,\"trophies\":720,\"rank\":7,\"season\":\"2026-10\",\"champion\":false,\"wins\":3,\"losses\":1,"
			"\"defenceWins\":2,\"defenceLosses\":1,\"log\":[{\"attacker\":\"Nova\",\"won\":true,\"delta\":28}],\"pendingReward\":{\"season\":\"2026-09\",\"rank\":12}}"), false);
		Expect(Profile.Meta.Arena.Trophies == 720 && Net.ArenaServer.PendingRank == 12 && Net.ArenaServer.Log.size() == 1, "the server's arena state is mirrored");
		R = Online::ApplyReply(Net, Profile, Online::FindMatch(10000), 200, Parse("{\"ok\":true,\"matchId\":\"x1\",\"opponent\":{\"uid\":\"10000002\",\"name\":\"Nova\",\"trophies\":760,"
			"\"power\":11000,\"team\":[{\"op\":5,\"level\":20,\"stars\":0,\"hp\":9000,\"attack\":1600,\"defence\":0,\"power\":5000},{\"op\":7,\"level\":20,\"hp\":11000,\"attack\":700,\"power\":4000}]}}"), false);
		Expect(Net.Match.bReady && Net.Match.MatchId == "x1" && Net.Match.Opponent.Team.size() == 2 && !Net.Match.Opponent.bBot, "an opponent from the server");
		const int Attempts = Profile.Meta.ArenaAttempts;
		R = Online::ApplyReply(Net, Profile, Online::ReportResult("x1", true), 200, Parse("{\"ok\":true,\"trophies\":751,\"rank\":7,\"delta\":31}"), false);
		Expect(Profile.Meta.Arena.Trophies == 751 && Profile.Meta.ArenaAttempts == Attempts - 1 && Has(R.Message, "+31") && !Net.Match.bReady, "a reported win updates trophies");
		R = Online::ApplyReply(Net, Profile, Online::FindMatch(10000), 200, Parse("{\"ok\":true,\"matchId\":\"\",\"opponent\":null}"), false);
		Expect(Net.Match.bReady && Net.Match.Opponent.bBot && Net.Match.MatchId.empty(), "nobody else yet: a practice rival");
		R = Online::ApplyReply(Net, Profile, Online::FindMatch(10000), 0, Json::FValue(), true);
		Expect(Net.Match.bReady && Net.Match.Opponent.bBot, "no server: a practice rival");
		const int Diamonds = Profile.Get(ECurrency::Diamonds);
		Online::ApplyReply(Net, Profile, Online::ClaimArenaSeason(), 200, Parse("{\"ok\":true,\"season\":\"2026-09\",\"rank\":12}"), false);
		Expect(Profile.Get(ECurrency::Diamonds) == Diamonds + Arena::SeasonReward(12).Diamonds, "the server's season reward is paid");
	}

	// ============================================================================ pages
	void TestPages()
	{
		FProfile Profile(15);
		P::FUiState State;
		auto Do = [&](const std::string& Id) { return P::Perform(Profile, State, Id); };
		Do("tab:2");
		P::FPage Root = P::Build(Profile, State);
		Expect(Find(Root, "go:friends") && Find(Root, "go:gang") && Find(Root, "go:arcade") && !Find(Root, "go:admin"), "lobby: friends and gang; no admin desk for players");
		Expect(PageHas(Root, "Squad power"), "the lobby shows squad power");

		// Arcade.
		Do("go:arcade");
		P::FPage Arcade = P::Build(Profile, State);
		Expect(!Find(Arcade, "arc:night") && Find(Arcade, "arc:boss") && Find(Arcade, "arc:rhythm"), "Boss Rush and Rhythm are listed; Night Shift is gone (2026-10-15)");
		Do("arc:rhythm");
		Expect(State.Page == "arcade-rhythm" && Find(P::Build(Profile, State), "arc:song:0") != nullptr, "song list");
		Do("arc:rdiff:2");
		const int Tokens = Profile.Get(ECurrency::ArcadeToken);
		const P::FPerform Song = Do("arc:song:1");
		Expect(Song.Overlay == P::EOverlay::Rhythm && Song.OverlayA == 1 && Song.OverlayB == 2 && Profile.Get(ECurrency::ArcadeToken) == Tokens - 1, "a song opens on Hard and costs a token");
		Do("back");
		Do("back");
		const P::FPerform Boss = Do("arc:boss");
		Expect(Boss.bStartStage && DecodeStage(Boss.StageId).Kind == EStageKind::BossRush && DecodeStage(Boss.StageId).B == 1, "Boss Rush starts round 1 as a stage");

		// Settings: six sections, key bindings, top bar, confirm spending.
		Do("tab:2");
		Do("go:settings");
		P::FPage Settings = P::Build(Profile, State);
		Expect(Settings.Bar.size() == 6, "six settings sections");
		Do("set:tab:2");
		Do("go:keys");
		Expect(State.Page == "keys" && P::Build(Profile, State).Grid.size() == static_cast<size_t>(BattleBindCount), "battle key bindings");
		const P::FPerform Capture = Do("keys:bind:4");
		Expect(Capture.CaptureKey == 4, "binding waits for a key");
		State.InputText = "LeftControl";
		Do("keyset:4");
		Expect(Profile.Meta.Settings.KeyBinds[4] == "LeftControl", "Jump rebound");
		State.InputText = "W";
		Do("keyset:4");
		Expect(Profile.Meta.Settings.KeyBinds[4] == "W" && Profile.Meta.Settings.KeyBinds[0] == "LeftControl", "a clash swaps the two keys");
		State.InputText = "Escape";
		Do("keyset:0");
		Expect(Profile.Meta.Settings.KeyBinds[0] == "LeftControl", "Escape cancels");
		Do("keys:reset");
		Expect(Profile.Meta.Settings.KeyBinds[0].empty() && Profile.Meta.Settings.KeyBinds[4].empty(), "reset the group");
		Do("keys:group:1");
		Expect(P::Build(Profile, State).Grid.size() == static_cast<size_t>(NightBindCount), "Night Shift keys");
		Do("back");
		Expect(State.Page == "settings", "keys go back to settings");
		Do("set:tab:4");
		Do("set:topbar:0");
		Expect(P::Resources(Profile)[0].Label == CurrencyName(ECurrency::Diamonds), "the first top-bar slot cycles to Diamonds");
		Do("set:tab:3");
		Do("set:confirm");
		Expect(Profile.Meta.Settings.bConfirmSpend, "confirm Diamond spending on");
		Do("tab:6");
		const int Diamonds = Profile.Get(ECurrency::Diamonds);
		Profile.Add(ECurrency::Diamonds, 5000);
		const P::FPerform First = Do("sum:go:1:1");
		Expect(Has(First.Message, "Tap again") && Profile.Get(ECurrency::Diamonds) == Diamonds + 5000, "the first tap only asks");
		Do("sum:go:1:1");
		Expect(Profile.Get(ECurrency::Diamonds) == Diamonds + 5000 - Config::DiamondsPerRoll, "the second tap spends");
		Do("sum:done");
		Do("tab:2");
		Do("go:settings");
		Do("set:tab:4");
		const P::FPerform Rename = Do("pref:name");
		Expect(Rename.Prompt.Key == "cmdname", "renaming asks for a name");
		State.InputText = "Captain Nova";
		Do("in:cmdname");
		Expect(Profile.Meta.Name == "Captain Nova", "commander renamed");

		// Offline: friends and gangs explain themselves.
		Do("tab:2");
		Do("go:friends");
		Expect(PageHas(P::Build(Profile, State), "signed-in account"), "offline friends page explains");
		Expect(!Do("fr:add").bOk, "no friend requests offline");
		Do("back");
		State.Net.bOnline = true;
		Profile.Account.bSignedIn = true;
		Profile.Account.UserId = "10000001";
		const P::FPerform Add = Do("go:friends");
		Expect(Add.Calls.size() == 1 && Add.Calls[0].Kind == Online::ECall::Friends, "opening Friends refreshes the list");
		const P::FPerform Ask = Do("fr:add");
		Expect(Ask.Prompt.Key == "friendadd", "Add friend asks for an id");
		State.InputText = "10000002";
		const P::FPerform Sent = Do("in:friendadd");
		Expect(Sent.Calls.size() == 1 && Sent.Calls[0].Kind == Online::ECall::FriendRequest && Has(Sent.Calls[0].Body, "10000002"), "the request goes to the server");
		Do("back");
		const P::FPerform Gang = Do("go:gang");
		Expect(Gang.Calls.size() == 2 && Gang.Calls[0].Kind == Online::ECall::GangMine && Gang.Calls[1].Kind == Online::ECall::GangSearch, "opening Gang loads mine and the list");
		State.Net.bGangLoaded = true;
		State.Net.Search.push_back({"g9", "Owls", "Rook", 3, 30, 30, 0, 0, true});
		State.Net.Search.push_back({"g8", "Elite", "Ace", 9, 10, 30, 50, 9000000, true});
		P::FPage GangPage = P::Build(Profile, State);
		Expect(Find(GangPage, "gang:join:0") && !Find(GangPage, "gang:join:0")->bEnabled && !Find(GangPage, "gang:join:1")->bEnabled, "a full gang and an out-of-reach gang cannot be joined");
		Expect(!Do("gang:join:1").bOk, "requirements are checked before asking the server");
		Do("go:gang-create");
		Do("gang:minlv");
		Expect(State.Net.NewGangMinLevel == 5, "the minimum level cycles");
		const P::FPerform Name = Do("gang:new");
		Expect(Name.Prompt.Key == "gangname", "founding asks for a name");
		Profile.Spend(ECurrency::Gold, Profile.Get(ECurrency::Gold));
		State.InputText = "Owls";
		Expect(!Do("in:gangname").bOk, "founding needs 20,000 Gold");
		Profile.Add(ECurrency::Gold, 30000);
		State.InputText = "Owls";
		const P::FPerform Found = Do("in:gangname");
		Expect(Found.Calls.size() == 1 && Found.Calls[0].Kind == Online::ECall::GangCreate && Has(Found.Calls[0].Body, "\"minLevel\":5"), "the gang is founded on the server");

		// Admin desk.
		Expect(!Do("go:admin").bOk, "players cannot open the admin desk");
		Profile.Account.bAdmin = true;
		Root = P::Build(Profile, State);
		Do("back");
		Root = P::Build(Profile, State);
		Expect(Find(Root, "go:admin") != nullptr, "the admin sees the admin desk");
		Do("go:admin");
		Do("adm:to");
		Expect(!State.Net.Admin.bToAll && !Do("adm:send").bOk, "one user needs an id");
		Do("adm:target");
		State.InputText = "10000002";
		Do("in:mailtarget");
		Do("adm:reward");
		const P::FPerform Ask1 = Do("adm:send");
		Expect(Ask1.Calls.empty() && Has(Ask1.Message, "Tap again"), "sending mail asks for a second tap");
		const P::FPerform Send = Do("adm:send");
		Expect(Send.Calls.size() == 1 && Send.Calls[0].Kind == Online::ECall::AdminMail && Has(Send.Calls[0].Body, "10000002") && Has(Send.Calls[0].Body, "\"diamonds\":300"), "admin mail goes out");
		Do("adm:to");
		Do("adm:ntitle");
		State.InputText = "Maintenance";
		Do("in:noticetitle");
		State.InputText = "Tonight at 22:00 UTC.";
		Do("in:noticebody");
		const P::FPerform Post = Do("adm:post");
		Expect(Post.Calls.size() == 1 && Post.Calls[0].Kind == Online::ECall::AdminNotice && State.Net.Admin.NoticeTitle.empty(), "a notice is published");

		// Notices page: open marks read.
		State.Net.Notices.push_back({"n7", "event", "Halloween", "Spooky", "2026-10-06T10:00:00.000Z", false});
		Expect(P::UnreadNotices(Profile, State) == 1, "one unread notice");
		Do("back");
		Do("go:notices");
		Do("ntc:open:0");
		Expect(State.Page == "notice" && P::UnreadNotices(Profile, State) == 0 && PageHas(P::Build(Profile, State), "Spooky"), "opening a notice reads it");

		// Arena (offline / guest): find, fight, trophies move, the overlay replays.
		FProfile Guest(16);
		P::FUiState GuestState;
		auto G = [&](const std::string& Id) { return P::Perform(Guest, GuestState, Id); };
		G("tab:4");
		G("go:ark");
		G("go:arena");
		Expect(!G("arena:find").bOk, "an attack team is needed first");
		G("arena:edit:0");
		G("arena:auto");
		Expect(Arena::TeamIsValid(Guest, Guest.Meta.Arena.Attack), "auto-fill");
		G("back");
		Expect(GuestState.Page == "arena", "back to the arena");
		Expect(G("arena:find").bOk && GuestState.Net.Match.bReady && GuestState.Net.Match.Opponent.bBot, "a practice rival offline");
		const int Trophies = Guest.Meta.Arena.Trophies;
		const P::FPerform Fight = G("arena:fight");
		Expect(Fight.Overlay == P::EOverlay::ArenaBattle && !GuestState.LastBattle.Events.empty() && (Guest.Meta.Arena.Trophies != Trophies || Trophies == 0),
			"the fight is simulated, replayed and moves trophies");
		Expect(Guest.Meta.ArenaWins + Guest.Meta.ArenaLosses == 1 && !GuestState.Net.Match.bReady, "one fight recorded");

		// Tower page and welcome back page.
		G("back");
		G("go:tower");
		Expect(PageHas(P::Build(Guest, GuestState), "never end"), "the tower page explains it never resets");
		LiveOps::StartReturn(Guest, Oct6);
		Guest.SetTime(Oct6 + 10);
		G("tab:2");
		Expect(Find(P::Build(Guest, GuestState), "go:return") != nullptr, "the welcome-back tile appears");
		G("go:return");
		Expect(G("ret:all").bOk && Guest.Meta.Return.DaysClaimed == 1, "claim all on the welcome-back page");
	}

	// ============================================================================ synthesised sounds
	void TestSynth()
	{
		const Synth::FBuffer Left = Synth::MediaCue(-1.0);
		const Synth::FBuffer Right = Synth::MediaCue(1.0);
		Expect(Left.Seconds() > 0.5 && Synth::Energy(Left, 0) > 20.0 * Synth::Energy(Left, 1), "Media's left cue plays on the left speaker");
		Expect(Synth::Energy(Right, 1) > 20.0 * Synth::Energy(Right, 0), "Media's right cue plays on the right speaker");
		for (int Song = 0; Song < SongCount(); ++Song)
		{
			const Synth::FBuffer Backing = Synth::RhythmBacking(Song);
			Expect(std::fabs(Backing.Seconds() - SongLength(Song)) < 0.05 && Synth::Energy(Backing, 0) > 100.0, "the backing track lasts the song and is not silent");
		}
		Expect(Synth::Energy(Synth::Jumpscare(), 0) > 1000.0 && Synth::Energy(Synth::KuuThump(-0.8), 0) > Synth::Energy(Synth::KuuThump(-0.8), 1), "cues are loud and panned");
	}

	// ============================================================================ save round trip of a fresh account
	void TestFreshAccount()
	{
		FProfile Fresh(17);
		Fresh.StartNewPlayer();
		Expect(Fresh.Meta.Mail.empty(), "owner: no beginning mail for a new account");
		const std::string Text = Save::Serialize(Fresh, Oct6);
		FProfile Loaded(18);
		std::string Error;
		Expect(Save::Deserialize(Text, Loaded, Error) && Loaded.Meta.Mail.empty(), "and none after a reload");
	}
}

int main(int argc, char** argv)
{
	for (int Index = 1; Index < argc; ++Index)
	{
		GPrintBalance = GPrintBalance || std::strcmp(argv[Index], "--balance") == 0;
	}
	TestNightRules();
	TestNightBalance();
	TestRhythm();
	TestBossRush();
	TestTower();
	TestPowerAndArena();
	TestLiveOps();
	TestOnline();
	TestPages();
	TestFreshAccount();
	TestSynth();
	std::printf("%d arcade/arena/live-ops/online checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
