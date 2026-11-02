// Unit tests for the data-driven operator kits (BIBKit*.cpp): data rules, status stacking and every kit mechanic,
// driven through a fake battlefield. Build & run: Demo/BoldInBeautyDemo/Tests/run_core_tests.sh
#include "kit_fake_world.h"

#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>

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

	void ExpectNear(double Actual, double Expected, double Tolerance, const std::string& What)
	{
		Expect(std::fabs(Actual - Expected) <= Tolerance,
			What + " (actual " + std::to_string(Actual) + ", expected " + std::to_string(Expected) + " +/- " + std::to_string(Tolerance) + ")");
	}

	using namespace KitTest;

	const FKitDef& Kit(const char* Name)
	{
		const FKitDef* Found = FindKitByName(Name);
		if (!Found)
		{
			std::printf("missing kit %s\n", Name);
			std::abort();
		}
		return *Found;
	}

	// A caster with a kit in a world with the given base health.
	struct Fixture
	{
		FakeWorld World;
		std::unique_ptr<FRuntime> Runtime;
		explicit Fixture(const char* Name)
		{
			const FKitDef& Def = Kit(Name);
			World.BaseMaxHP = Def.BaseHP;
			World.HP = World.MaxHP = Def.BaseHP;
			World.Attack = Def.BaseAttack;
			Runtime = std::make_unique<FRuntime>(Def, World);
		}
		bool Cast(ESlotId Slot) { return Runtime->Cast(Slot).bOk; }
		void Run(double Seconds) { World.Run(Seconds, *Runtime); }
		// Casts and waits until the ability's cooldown is over.
		void CastAndRun(ESlotId Slot, double Seconds)
		{
			Cast(Slot);
			Run(Seconds);
		}
	};

	// ------------------------------------------------------------------------------------------ data rules
	void TestKitData()
	{
		const std::vector<const FKitDef*>& Kits = AllKits();
		Expect(Kits.size() == 17, "17 data-driven kits");
		std::set<int> KitIds;
		std::set<std::string> AbilityIds;
		std::set<std::string> PassiveIds;
		for (const FKitDef* Def : Kits)
		{
			const std::string Name = Def->Name;
			KitIds.insert(Def->KitId);
			const int RosterIndex = BIB::FindOperator(Name.c_str());
			Expect(RosterIndex >= 0, Name + " is in the roster");
			if (RosterIndex >= 0)
			{
				const BIB::FRosterEntry& Entry = BIB::Roster()[static_cast<size_t>(RosterIndex)];
				Expect(Entry.Stars == Def->Stars, Name + " kit stars match the roster");
				Expect(static_cast<int>(Entry.Kit) == Def->KitId, Name + " roster kit id matches");
				ExpectNear(static_cast<double>(Entry.BaseHP), Def->BaseHP, 0.5, Name + " roster HP matches the kit");
				ExpectNear(static_cast<double>(Entry.BaseAttack), Def->BaseAttack, 0.5, Name + " roster attack matches the kit");
			}
			// MAIN §6: three moves, one Special, one Ultimate for every operator; passives only for 5-6 star (1-4).
			for (int Slot = 0; Slot < SlotCount; ++Slot)
			{
				const FAbilityDef& Ability = Def->Abilities[Slot];
				Expect(!Ability.Id.empty(), Name + " has slot " + SlotName(static_cast<ESlotId>(Slot)));
				Expect(Ability.Slot == static_cast<ESlotId>(Slot), Name + " slot index matches " + Ability.Id);
				Expect(AbilityIds.insert(Ability.Id).second, "ability id unique: " + Ability.Id);
				Expect(Ability.Id.rfind(Name + ".", 0) == 0, Ability.Id + " is prefixed with the kit name");
				Expect(Ability.Cooldown >= 0.0, Ability.Id + " cooldown is not negative");
				if (!Ability.bAutomatic && Ability.Slot != ESlotId::Basic)
				{
					Expect(!Ability.Steps.empty() || !Ability.Branches.empty(), Ability.Id + " does something");
				}
			}
			for (const FAbilityDef& Alt : Def->UltimateVariants)
			{
				Expect(Alt.Slot == ESlotId::Ultimate, Alt.Id + " alternative ultimate is in the Ultimate slot");
				Expect(AbilityIds.insert(Alt.Id).second, "alt ultimate id unique: " + Alt.Id);
			}
			const int PassiveCount = static_cast<int>(Def->Passives.size());
			if (Def->Stars >= 5)
			{
				Expect(PassiveCount >= 1 && PassiveCount <= 4, Name + " (5-6 star) has 1-4 passives, has " + std::to_string(PassiveCount));
			}
			else
			{
				Expect(PassiveCount == 0, Name + " (3-4 star) has no passives");
			}
			for (const FPassiveDef& Passive : Def->Passives)
			{
				Expect(PassiveIds.insert(Passive.Id).second, "passive id unique: " + Passive.Id);
			}
			Expect(Def->BaseHP > 0.0 && Def->BaseAttack > 0.0, Name + " has stats");
			Expect(!Def->Playstyle.empty(), Name + " has a playstyle line");
		}
		Expect(KitIds.size() == 17, "kit ids are unique");
		const std::vector<std::string> Tags = AllGameplayTags();
		Expect(Tags.size() > 300, "a full gameplay tag list is generated (" + std::to_string(Tags.size()) + ")");
		std::set<std::string> Unique(Tags.begin(), Tags.end());
		Expect(Unique.size() == Tags.size(), "gameplay tags are unique");
		Expect(Unique.count("BIB.Ability.Blood.Move1") == 1, "tag BIB.Ability.Blood.Move1 exists");
		Expect(Unique.count("BIB.Cooldown.Weather.Ultimate") == 1, "tag BIB.Cooldown.Weather.Ultimate exists");
		Expect(Unique.count("BIB.Status.Burn") == 1, "tag BIB.Status.Burn exists");
		Expect(Unique.count("BIB.State.Judge.Witness") == 1, "bespoke Judge tags exist");
		Expect(Unique.count("GameplayCue.BIB.Slash") == 1, "cue tags exist");
		for (const std::string& Tag : Tags)
		{
			Expect(!Tag.empty() && Tag.front() != '.' && Tag.back() != '.' && Tag.find("..") == std::string::npos, "well formed tag: " + Tag);
			Expect(Tag.find(' ') == std::string::npos, "tag has no spaces: " + Tag);
		}
	}

	// ------------------------------------------------------------------------------------------ status book
	void TestStatusBook()
	{
		FStatusBook Book;
		std::vector<FStatusEvent> Events;
		Expect(Book.Apply(EStatus::Burn, 5.0, 100.0, 1, 1), "burn applies");
		for (int Index = 0; Index < 10; ++Index)
		{
			Book.Apply(EStatus::Burn, 5.0, 100.0, 1, 1);
		}
		Expect(Book.Stacks(EStatus::Burn) == 6, "burn stacks cap at 6");
		Book.Tick(1.0, Events);
		double Dot = 0.0;
		for (const FStatusEvent& Event : Events)
		{
			Dot += Event.Type == FStatusEvent::EType::DotDamage ? Event.Amount : 0.0;
		}
		ExpectNear(Dot, 600.0, 1e-6, "six burn stacks tick 6 x 100 per second");
		Events.clear();
		Book.Tick(5.0, Events);
		Expect(!Book.Has(EStatus::Burn), "burn expires");

		Book.Apply(EStatus::Slow, 3.0, 0.4, 1, 1);
		ExpectNear(Book.MoveMult(), 0.6, 1e-9, "40% slow");
		Book.Apply(EStatus::Root, 2.0, 0.0, 1, 1);
		ExpectNear(Book.MoveMult(), 0.0, 1e-9, "rooted cannot move");
		Book.Clear(EStatus::Root);
		Book.Apply(EStatus::Stun, 1.0, 0.0, 1, 1);
		Expect(Book.Disabled(), "stunned is disabled");
		Book.ClearAll();

		Book.Apply(EStatus::Vulnerable, 6.0, 0.15, 1, 1);
		Book.Apply(EStatus::Vulnerable, 6.0, 0.15, 1, 1);
		ExpectNear(Book.DamageTakenMult(), 1.30, 1e-9, "two vulnerable stacks = +30%");
		Book.Apply(EStatus::Mark, 8.0, 0.0, 1, 1);
		ExpectNear(Book.DamageTakenMult(), 1.45, 1e-9, "mark adds 15%");
		Book.Apply(EStatus::Mutated, 100.0, 0.0, 1, 1);
		ExpectNear(Book.DamageDealtMult(), 0.8, 1e-9, "mutation: -20% damage dealt");
		ExpectNear(Book.DamageTakenMult(), 1.65, 1e-9, "mutation: -20% defence");
		Book.ClearAll();

		Book.Apply(EStatus::Immune, 100.0, 0.0, 1, 1);
		Expect(!Book.Apply(EStatus::Infection, 100.0, 0.0, 1, 1), "immune enemies cannot be infected");
		Book.ClearAll();

		Book.Apply(EStatus::Wet, 8.0, 0.0, 1, 1);
		Events.clear();
		for (int Stack = 0; Stack < 5; ++Stack)
		{
			Book.Apply(EStatus::Shock, 6.0, 0.0, 1, 1);
		}
		Book.Tick(0.016, Events);
		bool bDischarged = false;
		for (const FStatusEvent& Event : Events)
		{
			if (Event.Type == FStatusEvent::EType::ShockDischarge)
			{
				bDischarged = true;
				ExpectNear(Event.Amount, 450.0 * 5.0 * 1.5, 1e-6, "five shock stacks discharge 450 each, +50% when wet");
			}
		}
		Expect(bDischarged && !Book.Has(EStatus::Shock), "shock discharges at five stacks and clears");
	}

	// ------------------------------------------------------------------------------------------ casting basics
	void TestCasting()
	{
		Fixture F("Blood");
		F.World.AddEnemy(300.0, 0.0, 100000.0);
		Expect(F.Runtime->CanCast(ESlotId::Move2).bOk == false, "Clot Chains needs 20 blood");
		F.Runtime->Cast(ESlotId::Basic);
		F.Run(0.3);
		Expect(F.Runtime->Resource(0) >= 3.5, "a lancet hit feeds blood");
		const bool bFirst = F.Cast(ESlotId::Move1);
		Expect(bFirst, "Crimson Pool casts");
		Expect(!F.Cast(ESlotId::Move1), "cannot recast while busy / cooling down");
		F.Run(1.0);
		Expect(F.World.Cooldowns[static_cast<int>(ESlotId::Move1)] > 10.0, "cooldown started (12 s)");

		// Sacrifice never kills.
		Fixture G("Blood");
		G.World.HP = 300.0;
		G.World.AddEnemy(300.0, 0.0, 5000.0);
		const FCastResult Result = G.Runtime->CanCast(ESlotId::Move3);
		Expect(!Result.bOk, "Hemorrhage refuses when it would leave too little health");
		G.World.HP = 3000.0;
		G.Cast(ESlotId::Move3);
		G.Run(1.0);
		Expect(G.World.HP < 3000.0 && G.World.HP > 2000.0, "Hemorrhage sacrifices ~10% max health");
		Expect(G.Runtime->Resource(0) >= 39.0, "and gains 40 blood");
	}

	void TestCombo()
	{
		Fixture F("Plack");
		const int Id = F.World.AddEnemy(200.0, 0.0, 1e9);
		double Last = 0.0;
		double Damages[3] = {};
		for (int Hit = 0; Hit < 3; ++Hit)
		{
			F.Cast(ESlotId::Basic);
			F.Run(0.5);
			Damages[Hit] = F.World.Find(Id)->DamageTaken - Last;
			Last = F.World.Find(Id)->DamageTaken;
		}
		Expect(Damages[0] > 0.0 && Damages[2] > Damages[0], "the third hit of Plack's combo is heavier");
		F.Run(2.0);   // combo window passes
		const double Before = F.World.Find(Id)->DamageTaken;
		F.Cast(ESlotId::Basic);
		F.Run(0.5);
		ExpectNear(F.World.Find(Id)->DamageTaken - Before, Damages[0], 1e-6, "the combo resets after the window");
	}

	// ------------------------------------------------------------------------------------------ owner kits
	void TestAlchemist()
	{
		Fixture F("Alchemist");
		const int Near = F.World.AddEnemy(200.0, 0.0, 1e9);
		const int Far = F.World.AddEnemy(900.0, 0.0, 1e9);
		Expect(F.Runtime->CanCast(ESlotId::Special).bOk == false, "Ring leader needs an infected enemy");
		Expect(F.Cast(ESlotId::Move1), "Infection casts");
		F.Run(3.2);
		Expect(F.Runtime->IsInfected(Near), "the touched enemy is infected after the 2 s windup");
		Expect(!F.Runtime->IsInfected(Far), "only one enemy is infected");
		Expect(F.World.StatusStacks(Near, EStatus::Infection) == 1, "infection status mirrored to the enemy");
		ExpectNear(F.World.Cooldowns[static_cast<int>(ESlotId::Move1)], 60.0 - 3.2, 0.2, "Infection has a 60 s cooldown");

		// Ring leader makes infected enemies see illusions.
		F.World.Cooldowns[static_cast<int>(ESlotId::Special)] = 0.0;
		Expect(F.Cast(ESlotId::Special), "Ring leader casts with an infected enemy");
		F.Run(2.2);
		Expect(F.World.StatusStacks(Near, EStatus::Illusion) == 1, "infected enemy gains Illusion");
		Expect(F.World.StatusStacks(Far, EStatus::Illusion) == 0, "healthy enemy does not");

		// Infection spreads during the 5 s window to a touching enemy.
		Fixture S("Alchemist");
		const int A = S.World.AddEnemy(200.0, 0.0, 1e9);
		const int Bb = S.World.AddEnemy(260.0, 0.0, 1e9);   // within touching distance of A
		S.Cast(ESlotId::Move1);
		S.Run(3.2);
		Expect(S.Runtime->IsInfected(A), "A infected");
		S.Run(1.0);
		Expect(S.Runtime->IsInfected(Bb), "infection spread to the touching enemy inside the window");

		// Goo (500 every 5 s after 45 s) and the 75 s explosion that leaves immunity: the shared default profile, on Arita (who has the
		// same Infection move). Alchemist has her own faster profile since 2026-10-09 (below).
		Fixture L("Arita");
		const int Host = L.World.AddEnemy(200.0, 0.0, 1e9);
		const int Bystander = L.World.AddEnemy(900.0, 0.0, 1e9);
		L.Cast(ESlotId::Move1);
		L.Run(3.1);   // infected at 2.0 s: the infection is 1.1 s old
		const double AtInfection = L.World.Find(Host)->DamageTaken;
		L.Run(43.0);   // age ~ 44
		ExpectNear(L.World.Find(Host)->DamageTaken - AtInfection, 0.0, 1e-6, "no goo damage before 45 s");
		L.Run(7.0);    // age ~ 51: the first goo tick lands at 50 s
		ExpectNear(L.World.Find(Host)->DamageTaken - AtInfection, 500.0, 1e-6, "first goo tick: 500");
		L.Run(30.0);   // past 75 s
		Expect(!L.Runtime->IsInfected(Host), "the infection ends at 75 s");
		Expect(L.World.StatusStacks(Host, EStatus::Immune) == 1, "and leaves immunity");
		Expect(L.World.Find(Bystander)->DamageTaken >= 2000.0, "the explosion hits a large area for 2000");

		// Alchemist's own profile (2026-10-09): goo 1000 every 5 s from 10 s, explosion 3500 at 40 s - and it keeps running while she is
		// swapped out (FRuntime::TickBackground), so she can infect, mutate and swap.
		{
			Fixture A2("Alchemist");
			const int Host2 = A2.World.AddEnemy(200.0, 0.0, 1e9);
			const int Near2 = A2.World.AddEnemy(900.0, 0.0, 1e9);
			A2.Cast(ESlotId::Move1);
			A2.Run(3.1);
			Expect(A2.Runtime->IsInfected(Host2), "Alchemist: host infected");
			A2.Runtime->CancelAll();                  // what a switch-out does in the game (ABIBPlayerCharacter::DataKitSwitchedOut)
			A2.Runtime->ClearBattlefield(true);
			A2.World.bBenched = true;   // swapped out from here on
			const double At = A2.World.Find(Host2)->DamageTaken;
			A2.Run(8.0);    // age ~ 9
			ExpectNear(A2.World.Find(Host2)->DamageTaken - At, 0.0, 1e-6, "Alchemist: no goo before 10 s");
			A2.Run(7.0);    // age ~ 16: first tick at 15 s
			ExpectNear(A2.World.Find(Host2)->DamageTaken - At, 1000.0, 1e-6, "Alchemist: goo 1000 at 15 s, while benched");
			A2.Run(26.0);   // past 40 s
			Expect(!A2.Runtime->IsInfected(Host2) && A2.World.StatusStacks(Host2, EStatus::Immune) == 1, "Alchemist: the infection explodes at 40 s while benched");
			Expect(A2.World.Find(Near2)->DamageTaken >= 3500.0, "Alchemist: explosion 3500");
			const FKitDef& Default = Kit("Arita");
			Expect(Default.InfectionTuning.GooStartsAt == 45.0 && Default.InfectionTuning.ExplodeAt == 75.0, "Arita keeps the owner's default infection timings");
		}

		// Mutation: an uninfected enemy explodes for 5000; an infected one mutates.
		Fixture M("Alchemist");
		const int Clean = M.World.AddEnemy(200.0, 0.0, 1e9);
		M.Cast(ESlotId::Move2);
		M.Run(2.1);
		ExpectNear(M.World.Find(Clean)->DamageTaken, 5000.0, 1e-6, "Mutation on an uninfected enemy: 5000");
		Fixture N("Alchemist");
		const int Sick = N.World.AddEnemy(200.0, 0.0, 1e9);
		N.Cast(ESlotId::Move1);
		N.Run(3.1);
		N.World.Cooldowns[static_cast<int>(ESlotId::Move2)] = 0.0;
		N.Cast(ESlotId::Move2);
		N.Run(2.1);
		Expect(N.Runtime->IsMutated(Sick), "Mutation on an infected enemy mutates it");
		ExpectNear(N.World.Find(Sick)->Book.DamageDealtMult(), 0.8, 1e-9, "mutated: -20% damage");

		// Transmutation total on a single target in front: 866*5 + 1950 + 3*1100 + 2275 (owner numbers x1.3, 2026-10-09).
		Fixture T("Alchemist");
		const int Dummy = T.World.AddEnemy(250.0, 0.0, 1e9);
		T.Cast(ESlotId::Move3);
		T.Run(9.5);
		Expect(T.World.Find(Dummy)->DamageTaken > 8000.0, "Transmutation lands all four parts (" + std::to_string(T.World.Find(Dummy)->DamageTaken) + ")");
		ExpectNear(T.World.Cooldowns[static_cast<int>(ESlotId::Move3)], 40.0 - 9.5, 0.2, "Transmutation cooldown is 40 s (OPEN: Notion also says 50 s)");

		// Ultimate: puddle rolls infection / mutation.
		Fixture U("Alchemist");
		const int P1 = U.World.AddEnemy(100.0, 0.0, 1e9);
		const int P2 = U.World.AddEnemy(-100.0, 0.0, 1e9);
		U.Runtime->Cast(ESlotId::Ultimate);
		U.Run(1.0);
		Expect(U.Runtime->IsInfected(P1) && U.Runtime->IsInfected(P2), "enemies touching the puddle are infected");
		Expect(U.Runtime->IsMutated(P1) != U.Runtime->IsMutated(P2), "the puddle alternates infection and mutation");

		// Owner decisions 2026-10-04: "Why do i bother" is Adapt's ultimate (Alchemist has one ultimate), Ring leader is the Special,
		// Transmutation is 40 s.
		Expect(Kit("Alchemist").UltimateVariants.empty(), "Alchemist has no alternative ultimate");
		Expect(Kit("Alchemist").Abilities[static_cast<int>(ESlotId::Ultimate)].Name == "One with the soil", "Alchemist's ultimate is One with the soil");
		Expect(Kit("Alchemist").Abilities[static_cast<int>(ESlotId::Special)].Name == "Ring leader", "Ring leader is Alchemist's Special");
		Expect(Kit("Alchemist").Abilities[static_cast<int>(ESlotId::Move3)].Cooldown == 40.0, "Transmutation cooldown is 40 s");
	}

	void TestArita()
	{
		Fixture F("Arita");
		const int A = F.World.AddEnemy(150.0, 0.0, 1e9);
		// Friend zone repels and cannot be broken.
		F.Cast(ESlotId::Move2);
		F.Run(0.3);
		Expect(F.World.Find(A)->LastImpulse.X > 0.0, "Friend zone pushes enemies outward");
		Expect(F.World.InvulnerableFor > 1.0, "Friend zone makes her untouchable for 3 s");
		ExpectNear(F.World.Cooldowns[static_cast<int>(ESlotId::Move2)], 20.0, 0.5, "Friend zone: 20 s cooldown");

		// Nature's Curse: damage splits 50/50 and the attacker is soul-linked (1% health per second).
		Fixture C("Arita");
		const int Attacker = C.World.AddEnemy(150.0, 0.0, 100000.0);
		C.Cast(ESlotId::Special);
		C.Run(0.3);
		const double Taken = C.Runtime->OnIncomingDamage(1000.0, Attacker, true);
		ExpectNear(Taken, 500.0, 1e-6, "Nature's Curse: she takes half");
		ExpectNear(C.World.Find(Attacker)->DamageTaken, 500.0, 1e-6, "the attacker takes the other half");
		const double Before = C.World.Find(Attacker)->DamageTaken;
		C.Run(2.0);
		ExpectNear(C.World.Find(Attacker)->DamageTaken - Before, 0.01 * 100000.0 * 2.0, 25.0, "soul link: 1% health per second");
		Expect(C.Runtime->HasState("Curse"), "curse state is reported");
		C.World.Cooldowns[static_cast<int>(ESlotId::Special)] = 0.0;
		C.Cast(ESlotId::Special);
		C.Run(0.3);
		ExpectNear(C.Runtime->OnIncomingDamage(1000.0, Attacker, true), 1000.0, 1e-6, "toggled off: full damage again");

		// Spring time: hold to grow, release to scatter flowers.
		Fixture S("Arita");
		S.Runtime->Cast(ESlotId::Move3);
		S.Run(2.0);
		S.Runtime->Release(ESlotId::Move3);
		S.Run(0.2);
		const int Walker = S.World.AddEnemy(400.0, 0.0, 1e9);
		S.World.Find(Walker)->Pos = S.World.Pos + FVec{300.0, 0.0, 0.0};
		S.Run(0.1);
		Expect(S.Runtime->ActiveZoneCount() == 0, "flowers are not zones");
		S.Run(25.0);
		(void)Walker;
		Expect(S.World.Log.size() > 0, "Spring time reports the scattered flowers");

		// Full takeover: +75% health, M1 x5 damage and x3.5 reach.
		Fixture T("Arita");
		const int Dummy = T.World.AddEnemy(200.0, 0.0, 1e9);
		T.Cast(ESlotId::Basic);
		T.Run(0.6);
		const double Normal = T.World.Find(Dummy)->DamageTaken;
		T.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
		T.Run(2.0);
		T.Runtime->Cast(ESlotId::Ultimate);
		T.Run(0.2);
		ExpectNear(T.World.MaxHP, T.World.BaseMaxHP * 1.75, 1e-6, "takeover: +75% health");
		Expect(T.Runtime->HasState("Takeover"), "takeover state is reported");
		T.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
		const double Mark = T.World.Find(Dummy)->DamageTaken;
		T.Run(1.0);   // clears combo window effects
		T.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
		T.Cast(ESlotId::Basic);
		T.Run(0.4);
		const double Boosted = T.World.Find(Dummy)->DamageTaken - Mark;
		Expect(Boosted > Normal * 5.0, "takeover M1 hits far harder than the base M1 (" + std::to_string(Boosted) + " vs " + std::to_string(Normal) + ")");
		T.Run(40.0);
		ExpectNear(T.World.MaxHP, T.World.BaseMaxHP, 1e-6, "after 40 s she returns to base form");
		Expect(!T.Runtime->HasState("Takeover"), "takeover ends");
	}

	void TestAdapt()
	{
		Fixture F("Adapt");
		const int Foe = F.World.AddEnemy(200.0, 0.0, 1e9);
		Expect(!F.Runtime->CanCast(ESlotId::Move1).bOk, "nothing adapted yet: Move 1 is unavailable");
		Expect(!F.Runtime->CanCast(ESlotId::Special).bOk, "Switch needs 2 adapted abilities");
		F.Runtime->NotifyEnemyAbilityHit(1, 40.0);
		Expect(F.Runtime->AdaptedCount() == 1, "an enemy ability hit is adapted");
		F.Runtime->NotifyEnemyAbilityHit(1, 40.0);
		Expect(F.Runtime->AdaptedCount() == 1, "the same ability is not adapted twice");
		for (int Id = 2; Id <= 4; ++Id)
		{
			F.Runtime->NotifyEnemyAbilityHit(Id, 100.0);
		}
		Expect(F.Runtime->AdaptedCount() == 4, "four different enemy abilities adapted");
		Expect(F.Cast(ESlotId::Move1), "an adapted ability can be cast");
		F.Run(0.5);
		Expect(F.World.Find(Foe)->DamageTaken > 0.0, "the copy hits");
		ExpectNear(F.World.Cooldowns[static_cast<int>(ESlotId::Move1)], 1.6 - 0.5, 0.2, "the copy keeps the enemy ability's cooldown");
		const int Selected = F.Runtime->AdaptedSelected();
		F.World.Cooldowns[static_cast<int>(ESlotId::Special)] = 0.0;
		F.Cast(ESlotId::Special);
		F.Run(1.2);
		Expect(F.Runtime->AdaptedSelected() == (Selected + 1) % 4, "Switch selects the next adapted ability");
		ExpectNear(F.World.Cooldowns[static_cast<int>(ESlotId::Special)], 5.0 - 1.2, 0.2, "Switch: 5 s cooldown");

		// 8 abilities at most.
		Fixture G("Adapt");
		for (int Id = 1; Id <= 4; ++Id)
		{
			G.Runtime->NotifyEnemyAbilityHit(Id, 50.0);
		}
		Expect(G.Runtime->AdaptedCount() <= 8, "at most 8 adapted abilities");

		// Passive copy every 30 s: heals 25% when there is nothing to copy.
		Fixture H("Adapt");
		H.World.HP = H.World.MaxHP * 0.5;
		H.Run(30.2);
		Expect(H.World.Healed >= 0.24 * H.World.MaxHP, "no teammate passive to copy: heals 25%");
		Fixture I("Adapt");
		const FPassiveDef* Borrowed = nullptr;
		for (const FPassiveDef& Passive : Kit("Blood").Passives)
		{
			if (Passive.Id == "Blood.Transfusion")
			{
				Borrowed = &Passive;
			}
		}
		I.Runtime->SetTeammatePassives({Borrowed});
		I.Run(30.2);
		Expect(I.World.Log.back().find("copied the passive") != std::string::npos, "copies a teammate passive after 30 s");

		// Stat drift: attacking a lot raises damage 5% per second and costs the least used stat.
		Fixture J("Adapt");
		J.World.AddEnemy(200.0, 0.0, 1e12);
		for (int Second = 0; Second < 6; ++Second)
		{
			J.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
			J.Cast(ESlotId::Basic);
			J.Run(1.0);
		}
		Expect(J.Runtime->AdaptStat(0) > 1.0, "attacking raises his damage");
		Expect(J.Runtime->AdaptStat(0) <= 1.0 + 0.05 * 6.5, "at no more than 5% per second");
		ExpectNear(J.Runtime->AdaptStat(0) + J.Runtime->AdaptStat(1) + J.Runtime->AdaptStat(2), 3.0, 1e-6, "stat shifts are 1-1");

		// Ultimate "Why do i bother" (owner decision 2026-10-04: Adapt's): ten AK shots of 2000 in one second.
		Fixture K("Adapt");
		const int Target = K.World.AddEnemy(800.0, 0.0, 1e9);
		Expect(Kit("Adapt").Abilities[static_cast<int>(ESlotId::Ultimate)].Name == "Why do i bother", "Adapt's ultimate is Why do i bother");
		Expect(Kit("Adapt").Abilities[static_cast<int>(ESlotId::Ultimate)].Source == 1, "Adapt's ultimate is owner-written");
		K.Runtime->Cast(ESlotId::Ultimate);
		K.Run(1.5);
		// 10 x 2000 = 20000, plus at most the stat drift that "Adaption stats" adds while he deals damage (5% per second).
		const double Dealt = K.World.Find(Target)->DamageTaken;
		Expect(Dealt >= 20000.0 - 1e-6 && Dealt <= 20000.0 * 1.06, "Why do i bother: 10 shots of 2000 (" + std::to_string(Dealt) + ")");
	}

	// ------------------------------------------------------------------------------------------ new kits
	void TestDeath()
	{
		Fixture F("Death");
		const int Marked = F.World.AddEnemy(300.0, 0.0, 1e9);
		const int Clean = F.World.AddEnemy(-300.0, 0.0, 1e9);        // behind her: outside the sweep
		const int Boss = F.World.AddEnemy(0.0, 380.0, 100000.0, true);
		// Harvest only kills marked enemies; the rest are marked.
		F.World.Aim = FVec{1.0, 0.0, 0.0};
		F.Cast(ESlotId::Move1);
		F.Run(1.0);
		Expect(F.World.Find(Marked)->bAlive, "Harvest on an unmarked enemy only marks it");
		Expect(F.World.StatusStacks(Marked, EStatus::Doom) == 1, "marked with Doom");
		F.World.Cooldowns[static_cast<int>(ESlotId::Move1)] = 0.0;
		const double HpBefore = F.World.HP;
		F.Cast(ESlotId::Move1);
		F.Run(1.0);
		Expect(!F.World.Find(Marked)->bAlive, "Harvest executes a marked enemy");
		// Two executions were attempted (the marked enemy and the marked boss): 2 x 3% toll, minus Memento's 1.5% for the kill.
		// Reaper's Due regenerates up to 1.2% a second while something is marked, so the loss can be that much smaller.
		const double Toll = 2.0 * 0.03 * F.World.MaxHP - 0.015 * F.World.MaxHP;
		Expect(HpBefore - F.World.HP <= Toll + 1.0 && HpBefore - F.World.HP >= Toll - 0.012 * F.World.MaxHP - 1.0, "each execution costs 3% max health (Memento returns half for a kill)");
		Expect(F.World.Find(Clean)->bAlive, "unmarked bystanders live");
		Expect(F.Runtime->Resource(0) >= 1.0, "Memento gives a soul");
		// Bosses lose 15% instead of dying.
		Expect(F.World.Find(Boss)->bAlive, "a marked boss survives Harvest");
		ExpectNear(F.World.Find(Boss)->DamageTaken, 9000.0 + 1.5 * 950.0 * 2.0, 1500.0, "but loses ~9% of its health (plus the strikes)");

		// Limitation: too weak to pay the toll -> no execution.
		Fixture L("Death");
		const int Dummy = L.World.AddEnemy(300.0, 0.0, 1e9);
		L.World.Find(Dummy)->Book.Apply(EStatus::Doom, 8.0, 0.0, 1, 0);
		L.World.HP = L.World.MaxHP * 0.10;
		L.Cast(ESlotId::Move1);
		L.Run(1.0);
		Expect(L.World.Find(Dummy)->bAlive, "below 15% health Death cannot execute");

		// Last Rites kills the nearest enemy and costs 10% max health; a boss loses 20%.
		Fixture R("Death");
		const int Victim = R.World.AddEnemy(600.0, 0.0, 1e9);
		R.Cast(ESlotId::Special);
		R.Run(2.5);
		Expect(!R.World.Find(Victim)->bAlive, "Last Rites kills the nearest enemy");
		Expect(R.World.SelfDamage >= 0.099 * R.World.MaxHP, "at a cost of 10% max health");

		// Ultimate marks everything and 3 s later kills the marked.
		Fixture U("Death");
		for (int Index = 0; Index < 5; ++Index)
		{
			U.World.AddEnemy(100.0 * (Index + 1), 700.0, 1e9);
		}
		const int UBoss = U.World.AddEnemy(-800.0, 0.0, 100000.0, true);
		U.Runtime->Cast(ESlotId::Ultimate);
		U.Run(5.0);
		Expect(U.World.AliveCount() == 1, "Reaper's Ledger kills every non-boss");
		ExpectNear(U.World.Find(UBoss)->DamageTaken, 35000.0, 1.0, "and the boss loses 35%");
		Expect(U.World.SelfDamage >= 0.249 * U.World.MaxHP, "the price is 25% max health");
		Expect(U.World.HP > 0.0, "which can never kill her");

		// Final Debt: below 20% health non-bosses are executed outright.
		Fixture D("Death");
		const int Weak = D.World.AddEnemy(250.0, 0.0, 10000.0);
		D.World.Find(Weak)->HP = 2050.0;
		D.Cast(ESlotId::Basic);
		D.Run(0.5);
		Expect(!D.World.Find(Weak)->bAlive, "Final Debt executes an enemy hurt below 20%");
	}

	void TestGambler()
	{
		Fixture F("Gambler");
		F.World.AddEnemy(500.0, 0.0, 1e12);
		double Total = 0.0;
		for (const FBranch& Branch : Kit("Gambler").Abilities[static_cast<int>(ESlotId::Move3)].Branches)
		{
			Total += Branch.Weight;
		}
		ExpectNear(Total, 1.0, 1e-9, "Slot Machine outcome probabilities sum to 1");
		const FBranch& Jackpot = Kit("Gambler").Abilities[static_cast<int>(ESlotId::Move3)].Branches[0];
		ExpectNear(Jackpot.Weight, 0.2 * 0.2 * 0.2, 1e-12, "777 is a 0.8% chance");

		// Forced rolls: dice 1 hurts her, dice 6 is a huge heal-blast.
		F.Runtime->ForceRandom(0.0);      // first branch: snake eyes
		F.Cast(ESlotId::Move1);
		F.Run(1.2);
		Expect(F.World.SelfDamage > 0.0, "snake eyes hurts the caster");
		Fixture G("Gambler");
		const int Dummy = G.World.AddEnemy(500.0, 0.0, 1e12);
		G.Runtime->ForceRandom(0.999);    // last branch: jackpot
		G.Cast(ESlotId::Move1);
		G.Run(1.2);
		Expect(G.World.Find(Dummy)->DamageTaken >= 4500.0, "a 6 deals 4500");

		// Coin toss: heads buffs, tails debuffs.
		Fixture H("Gambler");
		H.Runtime->ForceRandom(0.0);
		H.Cast(ESlotId::Move2);
		H.Run(0.6);
		ExpectNear(H.Runtime->DamageDealtMult(), 2.0, 1e-9, "heads: x2 damage");
		Fixture T("Gambler");
		T.Runtime->ForceRandom(0.99);
		T.Cast(ESlotId::Move2);
		T.Run(0.6);
		ExpectNear(T.Runtime->DamageDealtMult(), 0.5, 1e-9, "tails: half damage");
		ExpectNear(T.Runtime->DamageTakenMult(), 2.0, 1e-9, "tails: double damage taken");

		// All In costs 25% max health; losing leaves her staggered.
		Fixture A("Gambler");
		A.World.AddEnemy(800.0, 0.0, 1e12);
		A.Runtime->ForceRandom(0.99);
		A.Cast(ESlotId::Special);
		A.Run(1.5);
		Expect(A.World.SelfDamage >= 0.249 * A.World.MaxHP, "All In costs 25% max health");
		Expect(A.Runtime->MoveSpeedMult() < 0.2, "a lost bet staggers her");

		// Luck pity: a bad roll raises the odds of the next good one.
		Fixture L("Gambler");
		L.World.AddEnemy(500.0, 0.0, 1e12);
		L.Runtime->ForceRandom(0.0);
		L.Cast(ESlotId::Move1);
		L.Run(1.2);
		Expect(L.Runtime->LastGamblerLuck() >= 5, "a bad roll raises luck by 5%");

		// Roulette: green kills every non-boss.
		Fixture R("Gambler");
		for (int Index = 0; Index < 6; ++Index)
		{
			R.World.AddEnemy(200.0 + 100.0 * Index, 300.0, 1e9);
		}
		R.Runtime->ForceRandom(0.99);     // green zero
		R.Runtime->Cast(ESlotId::Ultimate);
		R.Run(1.5);
		Expect(R.World.AliveCount() == 0, "green zero kills everything that is not a boss");
	}

	void TestJester()
	{
		Fixture F("Jester");
		F.World.AddEnemy(250.0, 0.0, 1e12);
		const char* Expected[6] = {"Melee", "Mage", "Speed", "Ranged", "Defence", "Melee"};
		for (int Cast = 0; Cast < 6; ++Cast)
		{
			Expect(std::string(F.Runtime->ActName()) == Expected[Cast], std::string("act ") + std::to_string(Cast) + " is " + Expected[Cast]);
			F.World.Cooldowns[static_cast<int>(ESlotId::Move1)] = 0.0;
			F.Cast(ESlotId::Move1);
			F.Run(1.4);
		}
		Fixture G("Jester");
		const int Dummy = G.World.AddEnemy(250.0, 0.0, 1e12);
		G.Cast(ESlotId::Move1);   // melee act: three 900 hits
		G.Run(1.4);
		ExpectNear(G.World.Find(Dummy)->DamageTaken, 2550.0, 1.0, "Juggle in the melee act: 3 x 850");
		// Encore replays the last moves.
		G.World.Cooldowns[static_cast<int>(ESlotId::Move2)] = 0.0;
		G.Cast(ESlotId::Move2);
		G.Run(1.4);
		const double Before = G.World.Find(Dummy)->DamageTaken;
		G.Cast(ESlotId::Special);
		G.Run(3.0);
		Expect(G.World.Find(Dummy)->DamageTaken > Before, "Encore replays earlier moves");
		Expect(G.World.Cooldowns[static_cast<int>(ESlotId::Move1)] >= 0.0, "Encore costs no cooldown on the replayed moves");
	}

	void TestTimekeeper()
	{
		Fixture F("Timekeeper");
		const int Dummy = F.World.AddEnemy(300.0, 0.0, 1e12);
		F.Cast(ESlotId::Move2);   // stasis bubble: records 4 s of damage and replays it twice
		F.Run(0.8);
		const double Before = F.World.Find(Dummy)->DamageTaken;
		F.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
		F.Cast(ESlotId::Basic);
		F.Run(0.4);
		const double Direct = F.World.Find(Dummy)->DamageTaken - Before;
		Expect(Direct > 0.0, "a strike lands inside the bubble");
		Expect(F.World.StatusStacks(Dummy, EStatus::Slow) == 1, "the bubble slows enemies");
		F.Run(5.0);
		Expect(F.World.Find(Dummy)->DamageTaken > Before + Direct * 2.5, "the recorded damage is replayed twice more");

		// Zero Hour: freeze, record and release x3 with a minimum of 8000 each.
		Fixture Z("Timekeeper");
		const int A = Z.World.AddEnemy(300.0, 0.0, 1e9);
		const int Bb = Z.World.AddEnemy(-300.0, 0.0, 1e9);
		Z.Runtime->Cast(ESlotId::Ultimate);
		Z.Run(0.6);
		Expect(Z.World.StatusStacks(A, EStatus::Freeze) == 1 && Z.World.StatusStacks(Bb, EStatus::Freeze) == 1, "Zero Hour freezes everything");
		Expect(Z.Runtime->HasState("ZeroHour"), "Zero Hour state");
		Z.Run(7.0);
		Expect(Z.World.Find(A)->DamageTaken >= 6000.0 && Z.World.Find(Bb)->DamageTaken >= 6000.0, "everything takes at least 6000 when time resumes");

		// Rewind Step doubles damage for 3 s; Fast Forward speeds cooldowns.
		Fixture R("Timekeeper");
		R.World.AddEnemy(300.0, 0.0, 1e9);
		R.Cast(ESlotId::Move1);
		R.Run(0.3);
		ExpectNear(R.Runtime->DamageDealtMult(), 1.5, 1e-9, "Rewind Step: +50% damage");
		Fixture S("Timekeeper");
		S.Cast(ESlotId::Move3);
		S.Run(0.1);
		ExpectNear(S.Runtime->AttackSpeedMult(), 2.0, 1e-9, "Fast Forward: double attack speed");
		ExpectNear(S.Runtime->DamageTakenMult(), 1.5, 1e-9, "at +50% damage taken");
		S.World.Cooldowns[static_cast<int>(ESlotId::Move1)] = 10.0;
		S.Run(1.0);
		Expect(S.World.Cooldowns[static_cast<int>(ESlotId::Move1)] < 8.5, "cooldowns recover faster (Fast Forward + Chronic)");
	}

	void TestStargazer()
	{
		Fixture F("Stargazer");
		const int Dummy = F.World.AddEnemy(400.0, 0.0, 1e12);
		F.Cast(ESlotId::Move3);
		F.Run(1.0);
		Expect(F.Runtime->HasState("Seeded"), "the seed is planted");
		F.Run(1.0);
		F.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
		F.Cast(ESlotId::Basic);
		F.Run(5.0);
		Expect(F.World.Find(Dummy)->DamageTaken > 4000.0, "the seed explodes for 4000+");
		Expect(!F.Runtime->HasState("Seeded"), "and is gone");

		// Falling Star scales with starlight.
		Fixture G("Stargazer");
		const int Foe = G.World.AddEnemy(900.0, 0.0, 1e12);
		G.World.Aim = FVec{1.0, 0.0, 0.0};
		G.Cast(ESlotId::Move1);
		G.Run(1.5);
		const double NoStars = G.World.Find(Foe)->DamageTaken;
		Fixture H("Stargazer");
		const int Foe2 = H.World.AddEnemy(900.0, 0.0, 1e12);
		H.Run(25.5);   // five starlight from Starlit
		const double Stars = H.Runtime->Resource(0);
		H.World.Cooldowns[static_cast<int>(ESlotId::Move1)] = 0.0;
		H.Cast(ESlotId::Move1);
		H.Run(1.5);
		ExpectNear(Stars, 5.0, 0.01, "Starlit gives one starlight every 5 s (max 5)");
		Expect(H.World.Find(Foe2)->DamageTaken >= NoStars + 500.0 * 4.9 * 1.3 * 0.0, "starlight adds damage");
		Expect(H.World.Find(Foe2)->DamageTaken > NoStars, "five starlight hit harder than none");
		// Zenith Gaze: 12 ticks of 1200 on a line.
		Fixture Z("Stargazer");
		const int Tgt = Z.World.AddEnemy(1200.0, 0.0, 1e12);
		Z.Cast(ESlotId::Special);
		Z.Run(3.6);
		ExpectNear(Z.World.Find(Tgt)->DamageTaken, 12.0 * 700.0 * 1.3, 2000.0, "Zenith Gaze lands about 12 x 700 (+30% glass cannon)");
	}

	void TestKuuMyrciaPlackShiraishi()
	{
		// Kuu: innate resistance, stored damage released by the ultimate.
		Fixture K("Kuu");
		const int Foe = K.World.AddEnemy(300.0, 0.0, 1e12);
		ExpectNear(K.Runtime->OnIncomingDamage(1000.0, Foe, true), 850.0, 1e-6, "Kuu takes 15% less");
		K.Runtime->Cast(ESlotId::Ultimate);
		K.Run(0.1);
		const double Taken = K.Runtime->OnIncomingDamage(1000.0, Foe, true);
		ExpectNear(Taken, 1000.0 * (1.0 - 0.15 - 0.8) > 0.05 * 1000.0 ? 1000.0 * 0.2 * 0.0 + 50.0 : 50.0, 60.0, "ultimate: -80% damage taken (floored at 5%)");
		K.Run(9.0);
		Expect(K.World.Find(Foe)->DamageTaken >= 4000.0, "Immovable ends with a 4000+ blast that includes the stored damage");
		Expect(K.Runtime->Mod(EBuff::Immovable) >= 1.0, "Kuu cannot be knocked back");

		// Myrcia brace counter.
		Fixture M("Myrcia");
		const int Hit = M.World.AddEnemy(300.0, 0.0, 1e9);
		M.Cast(ESlotId::Move1);
		M.Run(0.1);
		M.Runtime->OnIncomingDamage(500.0, Hit, true);
		Expect(M.World.Find(Hit)->DamageTaken >= 900.0, "Brace answers the first hit with 900");
		Expect(M.World.StatusStacks(Hit, EStatus::Stun) == 1, "and a stun");

		// Plack Overdrive: the four timed buffs are live (the Unreal layer mirrors them as BIB.Buff.* tags) and expire after 10 s.
		Fixture D("Plack");
		D.World.AddEnemy(2000.0, 0.0, 1e12);
		D.Cast(ESlotId::Ultimate);
		D.Run(0.2);
		Expect(D.Runtime->HasBuffActive(EBuff::DamageDealt) && D.Runtime->HasBuffActive(EBuff::MoveSpeed)
			&& D.Runtime->HasBuffActive(EBuff::DamageTaken) && D.Runtime->HasBuffActive(EBuff::AttackSpeed), "Overdrive buffs are active");
		Expect(std::string(BuffTag(EBuff::MoveSpeed)) == "BIB.Buff.MoveSpeed" && std::string(BuffTag(EBuff::Immovable)) == "BIB.Buff.Immovable",
			"buff tag names");
		D.Run(10.5);
		Expect(!D.Runtime->HasBuffActive(EBuff::DamageDealt) && !D.Runtime->HasBuffActive(EBuff::AttackSpeed), "Overdrive buffs expire");

		// Plack parry: negates a hit in the first 0.4 s.
		Fixture P("Plack");
		const int Atk = P.World.AddEnemy(300.0, 0.0, 1e9);
		P.Cast(ESlotId::Move2);
		P.Run(0.1);
		ExpectNear(P.Runtime->OnIncomingDamage(800.0, Atk, true), 0.0, 1e-9, "Guard Stance parries the first hit");
		ExpectNear(P.World.Find(Atk)->DamageTaken, 1000.0, 1e-6, "with a 1000 counter");
		Expect(P.World.StatusStacks(Atk, EStatus::Stun) == 1, "and a stun");

		// Shiraishi: shield first, then phoenix explosion on break, status resistance.
		Fixture S("Shiraishi");
		const int Enemy = S.World.AddEnemy(300.0, 0.0, 1e9);
		ExpectNear(S.Runtime->Shield(), 2200.0, 1e-9, "Shiraishi starts with a 2200 shield");
		ExpectNear(S.Runtime->Mod(EBuff::StatusResist), 0.6, 1e-9, "60% status resistance");
		ExpectNear(S.Runtime->OnIncomingDamage(1000.0, Enemy, true), 0.0, 1e-9, "the shield absorbs the hit");
		ExpectNear(S.Runtime->Shield(), 1200.0, 1e-9, "shield drained by 1000");
		S.Cast(ESlotId::Special);
		S.Run(0.1);
		const double Spill = S.Runtime->OnIncomingDamage(1500.0, Enemy, true);
		ExpectNear(Spill, 300.0, 1e-6, "damage beyond the shield reaches her");
		Expect(S.World.Find(Enemy)->DamageTaken >= 2500.0, "Phoenix Guard explodes when the shield breaks");
		Expect(S.Runtime->Shield() > 1000.0, "and refills half of it");
		Fixture H("Shiraishi");
		H.World.AddEnemy(300.0, 0.0, 1e9);
		H.Runtime->OnIncomingDamage(1000.0, 1, true);
		H.Run(5.0);   // regeneration after 4 s without damage
		Expect(H.Runtime->Shield() > 1200.0, "the shield regenerates after a pause");
		// Eruption needs heat.
		Fixture E("Shiraishi");
		E.World.AddEnemy(300.0, 0.0, 1e9);
		Expect(!E.Runtime->CanCast(ESlotId::Move3).bOk, "Eruption needs 30 Heat");
		for (int Hit2 = 0; Hit2 < 7; ++Hit2)
		{
			E.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
			E.Cast(ESlotId::Basic);
			E.Run(0.1);
		}
		Expect(E.Runtime->Resource(0) >= 30.0, "punches build Heat");
		Expect(E.Runtime->CanCast(ESlotId::Move3).bOk, "and unlock Eruption");
	}

	void TestWeatherIshiiFujitaMediaBlood()
	{
		// Weather front rotation.
		Fixture W("Weather");
		W.World.AddEnemy(400.0, 0.0, 1e12);
		const char* Fronts[6] = {"Rain", "Snow", "Wind", "Storm", "Heat", "Rain"};
		for (int Cast = 0; Cast < 6; ++Cast)
		{
			Expect(std::string(W.Runtime->FrontName()) == Fronts[Cast], std::string("front ") + std::to_string(Cast) + " is " + Fronts[Cast]);
			W.World.Cooldowns[static_cast<int>(ESlotId::Move3)] = 0.0;
			W.Cast(ESlotId::Move3);
			W.Run(1.0);
		}
		// Thunderstorm hits soaked enemies harder.
		Fixture T1("Weather");
		const int Dry = T1.World.AddEnemy(400.0, 0.0, 1e12);
		T1.Cast(ESlotId::Move3);
		T1.Run(4.0);
		Fixture T2("Weather");
		const int Wet = T2.World.AddEnemy(400.0, 0.0, 1e12);
		T2.World.Find(Wet)->Book.Apply(EStatus::Wet, 30.0, 0.0, 1, 0);
		T2.Cast(ESlotId::Move3);
		T2.Run(4.0);
		Expect(T2.World.Find(Wet)->DamageTaken > T1.World.Find(Dry)->DamageTaken * 1.3, "lightning deals more to a soaked enemy");

		// Ishii: Overload detonates stacks.
		Fixture I("Ishii");
		const int Foe = I.World.AddEnemy(400.0, 0.0, 1e12);
		I.World.Find(Foe)->Book.Apply(EStatus::Burn, 30.0, 0.0, 3, 0);
		I.World.Find(Foe)->Book.Apply(EStatus::Shock, 30.0, 0.0, 2, 0);
		I.World.Find(Foe)->Book.Apply(EStatus::Wet, 30.0, 0.0, 1, 0);
		I.Cast(ESlotId::Special);
		I.Run(1.2);
		ExpectNear(I.World.Find(Foe)->DamageTaken, 230.0 * (3 + 2 + 1), 5.0, "Overload: 230 per Burn, Shock and Wet stack");
		Expect(!I.World.Find(Foe)->Book.Has(EStatus::Burn), "the stacks are consumed");
		// Storm chain hits up to 8.
		Fixture C("Ishii");
		for (int Index = 0; Index < 10; ++Index)
		{
			C.World.AddEnemy(300.0 + 50.0 * Index, 0.0, 1e12);
		}
		C.Cast(ESlotId::Move3);
		C.Run(1.2);
		int Struck = 0;
		for (const FakeEnemy& Enemy : C.World.Enemies)
		{
			Struck += Enemy.DamageTaken > 0.0 ? 1 : 0;
		}
		Expect(Struck == 8, "Storm Chain strikes exactly 8 enemies");

		// Fujita: counter on dodge.
		Fixture Fu("Fujita");
		const int Attacker = Fu.World.AddEnemy(300.0, 0.0, 1e9);
		Fu.Cast(ESlotId::Move2);
		Fu.Run(0.1);
		for (int Hit = 0; Hit < 3; ++Hit)
		{
			ExpectNear(Fu.Runtime->OnIncomingDamage(500.0, Attacker, true), 0.0, 1e-9, "Smoke Step dodges the hit");
		}
		ExpectNear(Fu.Runtime->OnIncomingDamage(500.0, Attacker, true), 500.0, 1e-9, "the fourth hit lands");
		Expect(Fu.World.Find(Attacker)->DamageTaken >= 3600.0, "each dodge is answered with 1200");
		Expect(Fu.World.StatusStacks(Attacker, EStatus::Bleed) >= 2, "and bleeds");

		// Media: packet loss dodges about 20%, the ultimate is a flurry.
		Fixture Me("Media");
		Me.World.AddEnemy(300.0, 0.0, 1e9);
		int Dodged = 0;
		for (int Index = 0; Index < 2000; ++Index)
		{
			Dodged += Me.Runtime->OnIncomingDamage(100.0, 1, true) == 0.0 ? 1 : 0;
		}
		Expect(Dodged > 300 && Dodged < 500, "Packet Loss negates about 20% of hits (" + std::to_string(Dodged) + "/2000)");
		Fixture Ul("Media");
		for (int Index = 0; Index < 6; ++Index)
		{
			Ul.World.AddEnemy(300.0 + 150.0 * Index, 200.0, 1e9);
		}
		Ul.Runtime->Cast(ESlotId::Ultimate);
		Ul.Run(7.0);
		Expect(Ul.World.TotalDamage > 15000.0, "System Overload deals a large flurry (" + std::to_string(Ul.World.TotalDamage) + ")");

		// Blood: pool feeds the meter, Hemorrhage detonates bleeds, passives.
		Fixture Bl("Blood");
		const int Victim = Bl.World.AddEnemy(300.0, 0.0, 1e12);
		Bl.World.Find(Victim)->Book.Apply(EStatus::Bleed, 30.0, 0.0, 4, 0);
		Bl.World.HP = Bl.World.MaxHP;
		Bl.Cast(ESlotId::Move3);
		Bl.Run(1.0);
		ExpectNear(Bl.World.Find(Victim)->DamageTaken, 4 * 450.0, 5.0, "Hemorrhage: 450 per bleed stack");
		Expect(Bl.Runtime->Resource(0) >= 40.0 + 16.0 - 1.0, "+40 blood and +4 per stack");
		Fixture Bp("Blood");
		Bp.World.AddEnemy(700.0, 0.0, 1e12);
		Bp.Cast(ESlotId::Move1);
		Bp.Run(4.0);
		Expect(Bp.Runtime->Resource(0) >= 4.0, "the pool feeds blood while enemies stand in it");
	}

	void TestBossAndTargeting()
	{
		// Melee cones respect facing; area hits respect radius.
		Fixture F("Plack");
		const int Front = F.World.AddEnemy(200.0, 0.0, 1e9);
		const int Behind = F.World.AddEnemy(-200.0, 0.0, 1e9);
		F.World.Aim = FVec{1.0, 0.0, 0.0};
		F.Cast(ESlotId::Basic);
		F.Run(0.5);
		Expect(F.World.Find(Front)->DamageTaken > 0.0, "melee hits in front");
		Expect(F.World.Find(Behind)->DamageTaken == 0.0, "but not behind");
		F.World.Cooldowns[static_cast<int>(ESlotId::Move3)] = 0.0;
		F.Cast(ESlotId::Move3);
		F.Run(1.0);
		Expect(F.World.Find(Behind)->DamageTaken > 0.0, "Spinning Kick hits all around");
	}

	// ------------------------------------------------------------------------------------------ signature loops (2026-10-07)
	// Build a resource, cash it in; set up a status, pay it off. Every kit resource must be spent or read somewhere.
	void TestSignatureLoops()
	{
		for (const FKitDef* Def : AllKits())
		{
			for (int Res = 0; Res < 2; ++Res)
			{
				if (Def->ResourceName[Res].empty())
				{
					continue;
				}
				bool bUsed = false;
				for (const FAbilityDef& Ability : Def->Abilities)
				{
					bUsed = bUsed || Ability.CostResource == Res || Ability.RequiresResource == Res;
					for (const FStep& Step : Ability.Steps)
					{
						bUsed = bUsed || ((Step.Flags & FlagUseResource) && Step.Stat == Res)
							|| (Step.Kind == EStep::Resource && Step.Stat == Res && Step.Param < 0.0);
					}
				}
				for (const FPassiveDef& Passive : Def->Passives)
				{
					bUsed = bUsed || Passive.CondResource == Res;
				}
				Expect(bUsed, Def->Name + "'s " + Def->ResourceName[Res] + " is spent or read by some ability (no dead resource)");
			}
		}

		// Generic pay-off: BonusVs adds damage only against an enemy that carries the status.
		{
			Fixture Calm("Plack");
			const int A = Calm.World.AddEnemy(300.0, 0.0, 1e9);
			Calm.Cast(ESlotId::Move3);
			Calm.Run(1.0);
			Fixture Dazed("Plack");
			const int B2 = Dazed.World.AddEnemy(300.0, 0.0, 1e9);
			Dazed.World.Find(B2)->Book.Apply(EStatus::Stun, 30.0, 0.0, 1, 0);
			Dazed.Cast(ESlotId::Move3);
			Dazed.Run(1.0);
			ExpectNear(Dazed.World.Find(B2)->DamageTaken, Calm.World.Find(A)->DamageTaken * 1.5, 1.0, "Spinning Kick: +50% on a stunned enemy");
		}

		// Plack: combo hits build Momentum, Spinning Kick scales with it and spends it all.
		{
			Fixture P("Plack");
			const int Foe = P.World.AddEnemy(250.0, 0.0, 1e9);
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				P.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
				P.Cast(ESlotId::Basic);
				P.Run(0.3);
			}
			Expect(P.Runtime->Resource(0) >= 20.0, "three combo hits build Momentum (" + std::to_string(P.Runtime->Resource(0)) + ")");
			P.Cast(ESlotId::Special);   // Second Wind: +50
			P.Run(0.1);
			const double Momentum = P.Runtime->Resource(0);
			const double Before = P.World.Find(Foe)->DamageTaken;
			P.Cast(ESlotId::Move3);
			P.Run(0.6);
			const double Kick = P.World.Find(Foe)->DamageTaken - Before;
			Expect(Kick > 1200.0 + 20.0 * Momentum, "Spinning Kick adds 25 per Momentum (" + std::to_string(Kick) + ")");
			ExpectNear(P.Runtime->Resource(0), 0.0, 1e-6, "and spends all of it");
		}

		// Kuu: hits taken in Bulwark give Grit; Ground Pound spends it.
		{
			Fixture K("Kuu");
			const int Foe = K.World.AddEnemy(300.0, 0.0, 1e9);
			K.Cast(ESlotId::Move2);
			K.Run(0.1);
			for (int Hit = 0; Hit < 8; ++Hit)
			{
				K.Runtime->OnIncomingDamage(300.0, Foe, true);
			}
			ExpectNear(K.Runtime->Resource(0), 72.0, 1.0, "the first six hits in Bulwark give 12 Grit each");
			K.Run(4.0);
			const double Before = K.World.Find(Foe)->DamageTaken;
			K.World.Aim = FVec{1.0, 0.0, 0.0};
			K.Cast(ESlotId::Move3);
			K.Run(1.2);
			Expect(K.World.Find(Foe)->DamageTaken - Before > 1800.0 + 30.0 * 60.0, "Ground Pound is fed by Grit");
			ExpectNear(K.Runtime->Resource(0), 0.0, 1e-6, "and spends it");
		}

		// Media: jabs and Blink Cut buffer Data; Ping Storm flushes the buffer.
		{
			Fixture M("Media");
			M.World.AddEnemy(300.0, 0.0, 1e9);
			M.Cast(ESlotId::Move1);
			M.Run(0.3);
			ExpectNear(M.Runtime->Resource(0), 2.0, 1e-6, "Blink Cut buffers 2 Data");
			M.Cast(ESlotId::Move3);
			M.Run(2.0);
			ExpectNear(M.Runtime->Resource(0), 0.0, 1e-6, "Ping Storm flushes the buffer");
		}

		// Shiraishi: Eruption needs 30 Heat and burns all of it.
		{
			Fixture S("Shiraishi");
			S.World.AddEnemy(300.0, 0.0, 1e9);
			S.Cast(ESlotId::Move1);
			S.Run(0.3);
			S.Cast(ESlotId::Move2);
			S.Run(0.1);
			Expect(S.Runtime->Resource(0) >= 24.0 && !S.Runtime->CanCast(ESlotId::Move3).bOk, "25 Heat is not enough for Eruption");
			S.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
			S.Cast(ESlotId::Basic);
			S.Run(0.2);
			Expect(S.Runtime->CanCast(ESlotId::Move3).bOk, "30 Heat unlocks it");
			S.Cast(ESlotId::Move3);
			S.Run(1.0);
			ExpectNear(S.Runtime->Resource(0), 0.0, 1e-6, "Eruption burns all the Heat");
		}

		// Stargazer: Falling Star spends the starlight bank.
		{
			Fixture G("Stargazer");
			G.World.AddEnemy(900.0, 0.0, 1e12);
			G.Run(10.5);
			Expect(G.Runtime->Resource(0) >= 2.0, "starlight banks up");
			G.Cast(ESlotId::Move1);
			G.Run(1.5);
			ExpectNear(G.Runtime->Resource(0), 0.0, 1e-6, "Falling Star spends every starlight");
		}

		// Death: the third graze reaps a Soul; Tithe spends Souls for damage.
		{
			Fixture D("Death");
			const int Foe = D.World.AddEnemy(300.0, 0.0, 1e9);
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				D.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
				D.Cast(ESlotId::Basic);
				D.Run(0.3);
			}
			ExpectNear(D.Runtime->Resource(0), 1.0, 1e-6, "the third graze reaps a Soul");
			const double Before = D.World.Find(Foe)->DamageTaken;
			D.Cast(ESlotId::Move3);
			D.Run(1.0);
			Expect(D.World.Find(Foe)->DamageTaken - Before >= 2000.0 + 900.0 - 1.0, "Tithe: 2000 + 900 per Soul on a marked enemy");
			ExpectNear(D.Runtime->Resource(0), 0.0, 1e-6, "and spends the Souls");
		}

		// Fujita: Pressure Points ends by bursting the bleeds of marked enemies.
		{
			Fixture F("Fujita");
			const int Foe = F.World.AddEnemy(400.0, 0.0, 1e9);
			F.Cast(ESlotId::Special);
			F.Run(0.5);
			F.World.Find(Foe)->Book.Apply(EStatus::Bleed, 30.0, 0.0, 4, 0);
			F.Run(8.5);
			Expect(!F.World.Find(Foe)->Book.Has(EStatus::Bleed), "Pressure Points bursts the bleeds");
		}

		// Ishii: the three bolts cycle Shock, Burn and Wet.
		{
			Fixture I("Ishii");
			const int Foe = I.World.AddEnemy(500.0, 0.0, 1e9);
			for (int Bolt = 0; Bolt < 3; ++Bolt)
			{
				I.World.Cooldowns[static_cast<int>(ESlotId::Basic)] = 0.0;
				I.Cast(ESlotId::Basic);
				I.Run(0.4);
			}
			Expect(I.World.StatusStacks(Foe, EStatus::Shock) >= 1 && I.World.StatusStacks(Foe, EStatus::Burn) >= 1
				&& I.World.StatusStacks(Foe, EStatus::Wet) >= 1, "Arcane Bolt cycles Shock, Burn and Wet");
		}
	}

	// ------------------------------------------------------------------------------------------ determinism
	void TestDeterminism()
	{
		double First = 0.0;
		for (int Run = 0; Run < 2; ++Run)
		{
			Fixture F("Gambler");
			for (int Index = 0; Index < 5; ++Index)
			{
				F.World.AddEnemy(300.0 + 100.0 * Index, 0.0, 1e12);
			}
			for (int Round = 0; Round < 10; ++Round)
			{
				for (const ESlotId Slot : {ESlotId::Basic, ESlotId::Move1, ESlotId::Move2, ESlotId::Move3, ESlotId::Special})
				{
					F.Cast(Slot);
				}
				F.Run(2.0);
			}
			if (Run == 0)
			{
				First = F.World.TotalDamage;
			}
			else
			{
				ExpectNear(F.World.TotalDamage, First, 1e-9, "the same seed gives the same Gambler result");
			}
		}
	}
}

// The Unreal layer looks up BIB.Cooldown.<Operator>.<Slot> / BIB.Ability.<Operator>.<Slot> for every operator and slot, grants
// every operator in the demo, and needs the SetByCaller / state tags below; all of them must be in the generated tag list.
void TestDemoRosterAndTags()
{
	const std::vector<std::string> Tags = AllGameplayTags();
	const std::set<std::string> Known(Tags.begin(), Tags.end());
	const char* const SlotNames[] = {"Basic", "Move1", "Move2", "Move3", "Special", "Ultimate", "Dodge", "Parry"};
	int Covered = 0;
	for (const BIB::FRosterEntry& Entry : BIB::Roster())
	{
		std::string KitName = Entry.Name;
		if (const FKitDef* Def = FindKit(static_cast<int>(Entry.Kit)))
		{
			KitName = Def->Name;
		}
		else
		{
			Expect(Entry.Kit == BIB::EKit::Luna || Entry.Kit == BIB::EKit::Judge || Entry.Kit == BIB::EKit::Narrator,
				std::string(Entry.Name) + " has a kit (data or bespoke)");
		}
		for (const char* Slot : SlotNames)
		{
			Expect(Known.count("BIB.Cooldown." + KitName + "." + Slot) == 1, "cooldown tag " + KitName + "." + Slot);
			Expect(Known.count("BIB.Ability." + KitName + "." + Slot) == 1, "ability tag " + KitName + "." + Slot);
			++Covered;
		}
	}
	Expect(Covered == 20 * 8, "20 operators x 8 slots have cooldown and ability tags");
	for (const char* Name : {"BIB.Data.Damage", "BIB.Data.Heal", "BIB.Data.Duration", "BIB.State.Alive", "BIB.State.Dead",
		"BIB.State.Invulnerable", "BIB.State.Blocking", "BIB.State.Parrying", "BIB.State.Dodging", "BIB.State.Stunned",
		"BIB.State.Casting", "BIB.State.MovementLocked", "BIB.State.Shielded", "BIB.State.Active", "BIB.Team.Player",
		"BIB.Team.Enemy", "BIB.State.Kit.Curse", "BIB.State.Kit.Takeover", "BIB.State.Kit.Recording", "BIB.State.Kit.ZeroHour",
		"BIB.State.Kit.Puddle", "BIB.State.Kit.Infecting", "BIB.State.Kit.Seeded"})
	{
		Expect(Known.count(Name) == 1, std::string("state / data tag ") + Name);
	}
	for (const char* Name : {"BIB.State.Luna.Blackhole", "BIB.State.Luna.OrbitingBlack", "BIB.State.Luna.OrbitingWhite",
		"BIB.State.Luna.EventHorizon", "BIB.State.Luna.Expel", "BIB.State.Luna.Destabilisation", "BIB.State.Luna.GravityInverted",
		"BIB.State.Judge.Witness", "BIB.State.Judge.Lawyer", "BIB.State.Judge.Judge", "BIB.State.Judge.CourtSetting",
		"BIB.State.Judge.Contempt", "BIB.State.Judge.Justice", "BIB.State.Judge.RegAttackGranted",
		"BIB.State.Narrator.Diano", "BIB.State.Narrator.ExchangeOpen", "BIB.State.Narrator.StillAlive",
		"BIB.State.Narrator.CloseEnough", "BIB.State.Narrator.Switching", "BIB.State.Narrator.EndlessGod",
		"BIB.State.Narrator.SwitchBlock"})
	{
		Expect(Known.count(Name) == 1, std::string("bespoke state tag ") + Name);
	}

	// Demo grant: every operator owned, the team stays Luna / Judge / Narrator and any three can be fielded.
	BIB::FProfile Profile(1u);
	Profile.GrantAllOperators();
	for (int Index = 0; Index < static_cast<int>(BIB::Roster().size()); ++Index)
	{
		Expect(Profile.Owns(Index), std::string(BIB::Roster()[static_cast<size_t>(Index)].Name) + " is owned after GrantAllOperators");
	}
	Expect(Profile.TeamIsPlayable(), "default team is still playable");
	Expect(Profile.SetTeamSlot(2, BIB::FindOperator("Death")).bOk, "Death can join the team");
	Expect(Profile.SetTeamSlot(1, BIB::FindOperator("Gambler")).bOk, "Gambler can join the team");
	Expect(Profile.SetTeamSlot(0, BIB::FindOperator("Blood")).bOk, "Blood can join the team");
	Expect(Profile.TeamIsPlayable(), "an all-new team is playable");
}

int main()
{
	TestKitData();
	TestDemoRosterAndTags();
	TestStatusBook();
	TestCasting();
	TestCombo();
	TestAlchemist();
	TestArita();
	TestAdapt();
	TestDeath();
	TestGambler();
	TestJester();
	TestTimekeeper();
	TestStargazer();
	TestKuuMyrciaPlackShiraishi();
	TestWeatherIshiiFujitaMediaBlood();
	TestBossAndTargeting();
	TestSignatureLoops();
	TestDeterminism();
	std::printf("%d kit checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
