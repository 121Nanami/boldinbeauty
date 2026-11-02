// Boldinbeauty demo — kit data, part 2: Fujita, Gambler, Ishii, Jester, Kuu (every number PROPOSED, 2026-10-04).
#include "BIBCore.h"
#include "BIBKitBuild.h"
#include "BIBKitData.h"

#include <array>

namespace BIB
{
	namespace Kits
	{
		namespace
		{
			FAbilityDef& SlotDef(FKitDef& Kit, ESlotId Slot)
			{
				return Kit.Abilities[static_cast<int>(Slot)];
			}

			FBranch Branch(const char* Label, double Weight, bool bGood)
			{
				FBranch Result;
				Result.Label = Label;
				Result.Weight = Weight;
				Result.bGood = bGood;
				return Result;
			}
		}

		// ====================================================================================== Fujita (3*)
		FKitDef MakeFujita()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Fujita);
			K.Name = "Fujita";
			K.Stars = 3;
			// Signature loop (2026-10-07): open wounds, then cash them in. Jabs and Quick Slash stack bleeds; Quick Slash and the
			// Smoke Step counter cut bleeding enemies deeper; Pressure Points ends by bursting every bleed on the marked.
			K.BaseHP = 1700.0;
			K.BaseAttack = 220.0;
			K.WalkMult = 1.1;
			K.RunMult = 1.35;
			K.DodgeCooldown = 0.5;
			K.DodgeDistanceMult = 1.3;
			K.Colour[0] = 0.90f; K.Colour[1] = 0.80f; K.Colour[2] = 0.20f;
			K.Playstyle = "Fast chip damage: stacks bleeds and poison, cuts bleeding enemies deeper and bursts every bleed at once.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Fujita.Basic", "Flicker Jabs", 0.3, 4, "Four rapid nicks; each leaves a small bleed.");
			for (int Hit = 0; Hit < 4; ++Hit)
			{
				Basic.Steps.push_back(B::Melee(Hit == 3 ? 0.14 : 0.05, 230.0, 0.3, 0.0, 0.45).WithCombo(Hit).WithStatus(EStatus::Bleed, 4.0, 25.0, 1)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.05 s
			}

			FAbilityDef& Slash = SlotDef(K, ESlotId::Move1);
			Slash = B::Def("Fujita.Move1", "Quick Slash", ESlotId::Move1, 4.0, 0.3, 0.3, "Dashes 700 through enemies (400 each, +40% on bleeding) and cuts them open.");
			FStep Cut = B::DashStep(0.0, 700.0, 0.2, 400.0, 120.0).WithBonusVs(EStatus::Bleed, 0.4);
			Cut.Flags |= FlagInvulnerable;
			Cut.Status = EStatus::Bleed;
			Cut.StatusSeconds = 5.0;
			Cut.StatusPower = 40.0;
			Slash.Steps.push_back(Cut);
			Slash.Tags = {"BIB.Type.Mobility", "BIB.Type.Offensive"};

			FAbilityDef& Smoke = SlotDef(K, ESlotId::Move2);
			Smoke = B::Def("Fujita.Move2", "Smoke Step", ESlotId::Move2, 9.0, 0.0, 1.2,
				"1.2 s evasive stance: up to 3 hits are dodged and each is answered with 1100 (+50% on bleeding) and two bleeds.");
			Smoke.Reaction.Trigger = EReaction::OnDamaged;
			Smoke.Reaction.Window = 1.2;
			Smoke.Reaction.MaxTriggers = 3;
			Smoke.Reaction.bNegate = true;
			FStep Counter = B::Burst(0.0, 1.0, 1100.0).WithBonusVs(EStatus::Bleed, 0.5).WithStatus(EStatus::Bleed, 5.0, 40.0, 2).WithFlags(FlagHitTarget);
			Counter.CueId = CueBlink;
			Smoke.Reaction.Steps.push_back(Counter);
			Smoke.Steps.push_back(B::CueStep(0.0, CueGlitch, 200.0, 1.2));
			Smoke.Tags = {"BIB.Type.Defensive"};

			FAbilityDef& Needles = SlotDef(K, ESlotId::Move3);
			Needles = B::Def("Fujita.Move3", "Needle Rain", ESlotId::Move3, 14.0, 0.5, 0.5, "A fan of 12 poisoned needles (150 each, stacking poison).");
			Needles.Steps.push_back(B::Proj(0.3, 12, 2400.0, 900.0, 70.0, 150.0, 0.0, 70.0).WithStatus(EStatus::Poison, 6.0, 30.0, 1));
			Needles.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Points = SlotDef(K, ESlotId::Special);
			Points = B::Def("Fujita.Special", "Pressure Points", ESlotId::Special, 30.0, 0.6, 8.6,
				"Marks everything within 1000; for 8 s every marked enemy takes 250 true damage per second, then all its bleeds burst (150 each).");
			Points.Steps.push_back(B::Burst(0.4, 1000.0, 0.0).WithStatus(EStatus::Mark, 8.5).WithCue(CueMark));
			FStep Chip = B::Burst(0.6, 100.0, 250.0);
			Chip.Flags |= FlagAllEnemies | FlagMarkedOnly | FlagTrueDamage;
			Chip.Duration = 8.0;
			Chip.Interval = 1.0;
			Chip.CueId = CueNone;
			Points.Steps.push_back(Chip);
			FStep Burst = B::DetonateStep(8.5, 100.0, EStatus::Bleed, 150.0, true);
			Burst.Flags |= FlagAllEnemies | FlagMarkedOnly;
			Burst.CueId = CueBlood;
			Points.Steps.push_back(Burst);
			Points.Tags = {"BIB.Type.Offensive", "BIB.Type.Control"};

			FAbilityDef& Cuts = SlotDef(K, ESlotId::Ultimate);
			Cuts = B::Def("Fujita.Ultimate", "Hundred Cuts", ESlotId::Ultimate, 0.0, 3.2, 3.6,
				"3 s of blinking between enemies: 24 cuts of 450, then everything hit bleeds for 10 s.");
			FStep Hop = B::Blink(0.0, 2500.0, FlagNearestEnemy | FlagBehindTarget | FlagInvulnerable, 0.4);
			Hop.Duration = 3.0;
			Hop.Interval = 0.375;
			Cuts.Steps.push_back(Hop);
			FStep Flurry = B::Melee(0.06, 320.0, -1.0, 450.0);
			Flurry.Duration = 3.0;
			Flurry.Interval = 0.125;
			Cuts.Steps.push_back(Flurry);
			Cuts.Steps.push_back(B::Burst(3.2, 100.0, 0.0).WithFlags(FlagAllEnemies).WithStatus(EStatus::Bleed, 10.0, 80.0, 3).WithCue(CueUltimate));
			Cuts.Tags = {"BIB.Type.Offensive", "BIB.Type.Mobility"};
			return K;
		}

		// ====================================================================================== Gambler (5*)
		FKitDef MakeGambler()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Gambler);
			K.Name = "Gambler";
			K.Stars = 5;
			K.BaseHP = 2600.0;
			K.BaseAttack = 440.0;      // 400 -> 440 (2026-10-09: back inside the 5-star band after Alchemist moved the median)
			K.Colour[0] = 0.80f; K.Colour[1] = 0.10f; K.Colour[2] = 0.50f;
			K.Playstyle = "RNG: huge damage or nothing, and a quick death on bad luck.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Gambler.Basic", "Card Flick", 0.45, 1, "Throws a card: 10% dud, 70% hit, 20% Joker (x3 a hit).");
			Basic.BranchMode = EBranchMode::Weighted;
			FBranch Dud = Branch("dud", 10.0, false);
			FBranch Hit = Branch("hit", 70.0, false);
			Hit.Steps.push_back(B::Proj(0.05, 1, 2200.0, 1400.0, 50.0, 0.0, 0.8));
			FBranch Joker = Branch("Joker", 20.0, true);
			Joker.Steps.push_back(B::Proj(0.05, 1, 2200.0, 1400.0, 70.0, 0.0, 1.8).WithCue(CueStar));
			Basic.Branches = {Dud, Hit, Joker};

			// Dice Roll: a d6.
			FAbilityDef& Dice = SlotDef(K, ESlotId::Move1);
			Dice = B::Def("Gambler.Move1", "Dice Roll", ESlotId::Move1, 6.0, 0.5, 1.0,
				"d6: 1 hurts her (8%), 2-3 a cone (900), 4-5 an area (2200), 6 a huge blast (5500) and heals 15%.");
			Dice.BranchMode = EBranchMode::Weighted;
			Dice.BranchAt = 0.5;
			FBranch One = Branch("snake eyes (1)", 1.0, false);
			One.Steps.push_back(B::SelfCostStep(0.0, 0.08));
			One.Steps.push_back(B::Burst(0.0, 300.0, 300.0));
			FBranch Two = Branch("2", 1.0, false);
			Two.Steps.push_back(B::Melee(0.0, 700.0, 0.0, 900.0));
			FBranch Three = Branch("3", 1.0, false);
			Three.Steps.push_back(B::Melee(0.0, 700.0, 0.0, 900.0));
			FBranch Four = Branch("4", 1.0, true);
			Four.Steps.push_back(B::BurstAhead(0.0, 500.0, 600.0, 2200.0));
			FBranch Five = Branch("5", 1.0, true);
			Five.Steps.push_back(B::BurstAhead(0.0, 500.0, 600.0, 2200.0));
			FBranch Six = Branch("JACKPOT (6)", 1.0, true);
			Six.Steps.push_back(B::BurstAhead(0.0, 500.0, 700.0, 5500.0).WithCue(CueExplosion));
			Six.Steps.push_back(B::HealStep(0.0, 0.15));
			Dice.Branches = {One, Two, Three, Four, Five, Six};
			Dice.Tags = {"BIB.Type.Offensive"};

			// Coin Toss.
			FAbilityDef& Coin = SlotDef(K, ESlotId::Move2);
			Coin = B::Def("Gambler.Move2", "Coin Toss", ESlotId::Move2, 12.0, 0.4, 5.4,
				"Heads: 5 s of x2 damage and 10% life steal. Tails: 5 s of double damage taken and half damage dealt.");
			Coin.BranchMode = EBranchMode::Weighted;
			Coin.BranchAt = 0.4;
			FBranch Heads = Branch("heads", 1.0, true);
			Heads.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 1.0, 5.0));
			Heads.Steps.push_back(B::BuffStep(0.0, EBuff::LifeSteal, 0.10, 5.0));
			FBranch Tails = Branch("tails", 1.0, false);
			Tails.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, 1.0, 5.0));
			Tails.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, -0.5, 5.0));
			Coin.Branches = {Heads, Tails};
			Coin.Tags = {"BIB.Type.Buff"};

			// Slot Machine: three reels, symbol weights cherry .35, bell .30, seven .20, skull .15.
			FAbilityDef& Slots = SlotDef(K, ESlotId::Move3);
			Slots = B::Def("Gambler.Move3", "Slot Machine", ESlotId::Move3, 16.0, 1.2, 1.8,
				"Three reels: 777 = 11000 to everything near + heal, bells 4600, cherries 2900, pairs 1400-2300, skulls hurt her.");
			Slots.BranchMode = EBranchMode::Weighted;
			Slots.BranchAt = 1.2;
			const std::array<double, 4> Weights = {0.35, 0.30, 0.20, 0.15};   // cherry, bell, seven, skull
			double ClassProbability[8] = {};   // 0 jackpot, 1 bells, 2 cherries, 3 two sevens, 4 other pair, 5 skull pair, 6 skulls, 7 nothing
			for (int A = 0; A < 4; ++A)
			{
				for (int Bb = 0; Bb < 4; ++Bb)
				{
					for (int C = 0; C < 4; ++C)
					{
						const double P = Weights[static_cast<size_t>(A)] * Weights[static_cast<size_t>(Bb)] * Weights[static_cast<size_t>(C)];
						int Sevens = 0;
						int Skulls = 0;
						for (const int Reel : {A, Bb, C})
						{
							Sevens += Reel == 2 ? 1 : 0;
							Skulls += Reel == 3 ? 1 : 0;
						}
						const bool bTriple = A == Bb && Bb == C;
						const bool bPair = A == Bb || Bb == C || A == C;
						int Class = 7;
						if (bTriple && A == 2) Class = 0;
						else if (bTriple && A == 1) Class = 1;
						else if (bTriple && A == 0) Class = 2;
						else if (bTriple && A == 3) Class = 6;
						else if (Sevens == 2) Class = 3;
						else if (bPair && Skulls == 2) Class = 5;
						else if (bPair) Class = 4;
						ClassProbability[Class] += P;
					}
				}
			}
			FBranch Jackpot = Branch("777 JACKPOT", ClassProbability[0], true);
			Jackpot.Steps.push_back(B::Burst(0.0, 1500.0, 11000.0).WithCue(CueUltimate));
			Jackpot.Steps.push_back(B::HealStep(0.0, 0.25));
			FBranch Bells = Branch("three bells", ClassProbability[1], true);
			Bells.Steps.push_back(B::Burst(0.0, 1000.0, 4600.0));
			FBranch Cherries = Branch("three cherries", ClassProbability[2], true);
			Cherries.Steps.push_back(B::Burst(0.0, 800.0, 2900.0));
			FBranch Sevens = Branch("two sevens", ClassProbability[3], true);
			Sevens.Steps.push_back(B::Burst(0.0, 700.0, 2300.0));
			FBranch Pair = Branch("a pair", ClassProbability[4], false);
			Pair.Steps.push_back(B::Melee(0.0, 700.0, 0.0, 1400.0));
			FBranch SkullPair = Branch("two skulls", ClassProbability[5], false);
			SkullPair.Steps.push_back(B::SelfCostStep(0.0, 0.05));
			FBranch Skulls3 = Branch("three skulls", ClassProbability[6], false);
			Skulls3.Steps.push_back(B::SelfCostStep(0.0, 0.15));
			FBranch Nothing = Branch("nothing", ClassProbability[7], false);
			Nothing.Steps.push_back(B::Burst(0.0, 400.0, 500.0));
			Slots.Branches = {Jackpot, Bells, Cherries, Sevens, Pair, SkullPair, Skulls3, Nothing};
			Slots.Tags = {"BIB.Type.Offensive"};

			// All In.
			FAbilityDef& AllIn = SlotDef(K, ESlotId::Special);
			AllIn = B::Def("Gambler.Special", "All In", ESlotId::Special, 40.0, 1.0, 2.5,
				"Bets 25% max health: 45% for a 14000 blast and the bet back doubled, 55% to lose it and stagger for 1.5 s.");
			AllIn.HealthCostFraction = 0.25;
			AllIn.BranchMode = EBranchMode::Weighted;
			AllIn.BranchAt = 1.0;
			FBranch Win = Branch("the bet pays", 0.45, true);
			Win.Steps.push_back(B::BurstAhead(0.0, 800.0, 900.0, 14000.0).WithCue(CueExplosion));
			Win.Steps.push_back(B::HealStep(0.0, 0.5));
			FBranch Lose = Branch("the bet is lost", 0.55, false);
			Lose.Steps.push_back(B::BuffStep(0.0, EBuff::MoveSpeed, -0.9, 1.5));
			Lose.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, 0.5, 1.5));
			AllIn.Branches = {Win, Lose};
			AllIn.Tags = {"BIB.Type.Offensive"};

			// Ultimate: one roulette spin that decides the next 8 seconds.
			FAbilityDef& Roulette = SlotDef(K, ESlotId::Ultimate);
			Roulette = B::Def("Gambler.Ultimate", "House Always Wins", ESlotId::Ultimate, 0.0, 1.0, 9.0,
				"Roulette: red 46% = 8 s of 1400/s everywhere, black 46% = 8 s of 8%/s regeneration and a shield, green 8% = every non-boss dies.");
			Roulette.BranchMode = EBranchMode::Weighted;
			Roulette.BranchAt = 1.0;
			FBranch Red = Branch("red", 0.46, true);
			Red.Steps.push_back(B::ZoneFollow(0.0, 2600.0, 8.0, 1.0, 1400.0).WithCue(CueUltimate));
			FBranch Black = Branch("black", 0.46, false);
			Black.Steps.push_back(B::BuffStep(0.0, EBuff::Regen, 0.08, 8.0));
			Black.Steps.push_back(B::ShieldStep(0.0, 1200.0, 8.0));
			FBranch Green = Branch("green zero", 0.08, true);
			Green.Steps.push_back(B::ExecStep(0.0, FlagAllEnemies | FlagRespectBoss, 1.0, 0.0, 0.0, 0.40).WithCue(CueUltimate));
			Roulette.Branches = {Red, Black, Green};
			Roulette.Tags = {"BIB.Type.Offensive"};

			FPassiveDef Lady = B::Passive("Gambler.LadyLuck", "Lady Luck", ETrigger::Always,
				"PROPOSED: every unlucky roll raises the odds of the next good one by 5% (reset on a good roll).");
			K.Passives.push_back(Lady);
			FPassiveDef Roller = B::Passive("Gambler.HighRoller", "High Roller", ETrigger::OnHit,
				"PROPOSED: 10% of her hits deal double damage.");
			Roller.Chance = 0.10;
			FStep Double = B::Burst(0.0, 1.0, 0.0);
			Double.Flags |= FlagHitTarget | FlagHitAmount;
			Double.Param2 = 1.0;
			Double.CueId = CueStar;
			Roller.Steps.push_back(Double);
			K.Passives.push_back(Roller);
			FPassiveDef Beat = B::Passive("Gambler.BadBeat", "Bad Beat", ETrigger::OnLowHealth,
				"PROPOSED: below 30% health she gets a 1000 shield for 6 s (once per minute).");
			Beat.Threshold = 0.30;
			Beat.Interval = 60.0;
			Beat.Steps.push_back(B::ShieldStep(0.0, 1000.0, 6.0));
			K.Passives.push_back(Beat);
			return K;
		}

		// ====================================================================================== Ishii (3*)
		FKitDef MakeIshii()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Ishii);
			K.Name = "Ishii";
			K.Stars = 3;
			// Signature loop (2026-10-07): an elemental rotation. Her bolts cycle Shock -> Burn -> Wet, her moves add Slow, Wet,
			// Burn and Shock, and Overload (shorter cooldown) detonates every stack of all four at once.
			K.BaseHP = 3000.0;
			K.BaseAttack = 330.0;
			K.Colour[0] = 0.40f; K.Colour[1] = 0.50f; K.Colour[2] = 1.00f;
			K.Playstyle = "Mage: cycles Shock / Burn / Wet bolts, stacks Slow and Burn with area moves, then Overload detonates it all.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Ishii.Basic", "Arcane Bolt", 0.5, 3, "Three bolts that cycle the elements: Shock (five stacks discharge), Burn, then Wet.");
			Basic.Steps.push_back(B::Proj(0.1, 1, 2000.0, 1600.0, 60.0, 0.0, 1.0).WithCombo(0).WithStatus(EStatus::Shock, 6.0, 0.0, 1).WithCue(CueLightning));
			Basic.Steps.push_back(B::Proj(0.1, 1, 2000.0, 1600.0, 60.0, 0.0, 1.0).WithCombo(1).WithStatus(EStatus::Burn, 5.0, 40.0, 1).WithCue(CueFlame));
			Basic.Steps.push_back(B::Proj(0.18, 1, 2000.0, 1600.0, 60.0, 0.0, 1.0).WithCombo(2).WithStatus(EStatus::Wet, 6.0).WithCue(CueSplash)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.1 s

			FAbilityDef& Ring = SlotDef(K, ESlotId::Move1);
			Ring = B::Def("Ishii.Move1", "Frost Ring", ESlotId::Move1, 10.0, 0.6, 0.6, "A 900 ring: 700, slows 40% and soaks (Wet).");
			Ring.Steps.push_back(B::Burst(0.5, 900.0, 700.0).WithStatus(EStatus::Slow, 4.0, 0.4).WithKnock(200.0).WithCue(CueFrost));
			Ring.Steps.push_back(B::Burst(0.5, 900.0, 0.0).WithStatus(EStatus::Wet, 8.0).WithCue(CueNone));
			Ring.Tags = {"BIB.Type.Control"};

			FAbilityDef& Field = SlotDef(K, ESlotId::Move2);
			Field = B::Def("Ishii.Move2", "Ember Field", ESlotId::Move2, 16.0, 0.6, 0.6, "A burning field (6 s): 400 a second and a Burn stack per tick.");
			Field.Steps.push_back(B::ZoneAt(0.5, 800.0, 700.0, 6.0, 1.0, 400.0).WithStatus(EStatus::Burn, 5.0, 40.0, 1).WithCue(CueFlame));
			Field.Tags = {"BIB.Type.Environmental", "BIB.Type.Offensive"};

			FAbilityDef& Storm = SlotDef(K, ESlotId::Move3);
			Storm = B::Def("Ishii.Move3", "Storm Chain", ESlotId::Move3, 22.0, 0.8, 0.8, "Lightning jumps to 8 enemies (700, +50% on Wet) and adds two Shock stacks.");
			Storm.Steps.push_back(B::ChainStep(0.6, 8, 1200.0, 700.0).WithStatus(EStatus::Shock, 6.0, 0.0, 2).WithWet(0.5));
			Storm.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Overload = SlotDef(K, ESlotId::Special);
			Overload = B::Def("Ishii.Special", "Overload", ESlotId::Special, 25.0, 1.0, 1.0,
				"Detonates every Burn, Shock, Slow and Wet stack within 2000 for 230 each.");
			for (const EStatus Status : {EStatus::Burn, EStatus::Shock, EStatus::Slow, EStatus::Wet})
			{
				FStep Pop = B::DetonateStep(1.0, 2000.0, Status, 230.0, true);
				Pop.Flags |= FlagAllEnemies;
				Overload.Steps.push_back(Pop);
			}
			Overload.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Cataclysm = SlotDef(K, ESlotId::Ultimate);
			Cataclysm = B::Def("Ishii.Ultimate", "Elemental Cataclysm", ESlotId::Ultimate, 0.0, 6.0, 6.5,
				"A 6 s channel: a meteor (1100, burning) every 0.4 s on random enemies within 2500.");
			FStep Meteor = B::Burst(0.5, 500.0, 1100.0);
			Meteor.Flags |= FlagRandomEnemy;
			Meteor.Range = 2500.0;
			Meteor.Duration = 5.6;
			Meteor.Interval = 0.4;
			Meteor.Status = EStatus::Burn;
			Meteor.StatusSeconds = 5.0;
			Meteor.StatusPower = 100.0;
			Meteor.CueId = CueMeteor;
			Cataclysm.Steps.push_back(Meteor);
			Cataclysm.Tags = {"BIB.Type.Offensive"};
			return K;
		}

		// ====================================================================================== Jester (6*)
		FKitDef MakeJester()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Jester);
			K.Name = "Jester";
			K.Stars = 6;
			K.BaseHP = 4200.0;
			K.BaseAttack = 560.0;
			K.Colour[0] = 0.90f; K.Colour[1] = 0.30f; K.Colour[2] = 0.70f;
			K.Playstyle = "Rotating acts (not random): melee, mage, speed, ranged, defence - every cast uses the current act.";

			// Act ids (branch index): 0 Melee, 1 Ranged, 2 Defence, 3 Mage, 4 Speed. The order of play is fixed:
			// Melee -> Mage -> Speed -> Ranged -> Defence (BIBKitRuntime.cpp, ActOrder).
			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Jester.Basic", "Pratfall", 0.5, 1, "A prop gag that follows the current act.");
			Basic.BranchMode = EBranchMode::ByAct;
			FBranch BaMelee = Branch("melee", 1.0, false);
			BaMelee.Steps.push_back(B::Melee(0.1, 300.0, 0.2, 0.0, 1.1));
			FBranch BaRanged = Branch("ranged", 1.0, false);
			BaRanged.Steps.push_back(B::Proj(0.1, 1, 2200.0, 1500.0, 50.0, 0.0, 0.8));
			FBranch BaDefence = Branch("defence", 1.0, false);
			BaDefence.Steps.push_back(B::Melee(0.1, 250.0, 0.2, 0.0, 0.7));
			BaDefence.Steps.push_back(B::ShieldStep(0.1, 300.0, 2.0));
			FBranch BaMage = Branch("mage", 1.0, false);
			BaMage.Steps.push_back(B::Proj(0.1, 1, 1600.0, 1400.0, 60.0, 0.0, 0.9).WithStatus(EStatus::Slow, 1.0, 0.0));
			BaMage.Steps.back().Param2 = 350.0;
			FBranch BaSpeed = Branch("speed", 1.0, false);
			BaSpeed.Steps.push_back(B::DashStep(0.0, 350.0, 0.12, 0.0, 120.0));
			BaSpeed.Steps.back().AtkMult = 0.8;
			Basic.Branches = {BaMelee, BaRanged, BaDefence, BaMage, BaSpeed};

			FAbilityDef& Juggle = SlotDef(K, ESlotId::Move1);
			Juggle = B::Def("Jester.Move1", "Juggle", ESlotId::Move1, 6.0, 0.5, 1.2, "The current act: flurry / knives / reflect shield / confetti orbs / dash.");
			Juggle.BranchMode = EBranchMode::ByAct;
			Juggle.BranchAt = 0.2;
			FBranch JuMelee = Branch("melee flurry", 1.0, false);
			JuMelee.Steps = {B::Melee(0.0, 320.0, 0.2, 850.0), B::Melee(0.25, 320.0, 0.2, 850.0), B::Melee(0.5, 320.0, 0.2, 850.0)};
			FBranch JuRanged = Branch("knife volley", 1.0, false);
			JuRanged.Steps = {B::Proj(0.0, 5, 2400.0, 1500.0, 60.0, 650.0, 0.0, 30.0)};
			FBranch JuDefence = Branch("reflecting umbrella", 1.0, false);
			JuDefence.Steps = {B::ShieldStep(0.0, 1500.0, 2.5), B::BuffStep(0.0, EBuff::Reflect, 0.5, 2.5)};
			FBranch JuMage = Branch("confetti orbs", 1.0, false);
			JuMage.Steps = {B::Proj(0.0, 3, 1500.0, 1300.0, 60.0, 700.0, 0.0, 40.0)};
			JuMage.Steps.back().Param2 = 450.0;
			FBranch JuSpeed = Branch("cartwheel dash", 1.0, false);
			JuSpeed.Steps = {B::DashStep(0.0, 900.0, 0.2, 1400.0, 150.0)};
			JuSpeed.Steps.back().Flags |= FlagInvulnerable;
			Juggle.Branches = {JuMelee, JuRanged, JuDefence, JuMage, JuSpeed};
			Juggle.Tags = {"BIB.Type.Utility"};

			FAbilityDef& Spot = SlotDef(K, ESlotId::Move2);
			Spot = B::Def("Jester.Move2", "Spotlight", ESlotId::Move2, 14.0, 0.6, 1.2, "The current act: stunning bash / beam / taunting guard / fear burst / haste.");
			Spot.BranchMode = EBranchMode::ByAct;
			Spot.BranchAt = 0.4;
			FBranch SpMelee = Branch("stunning bash", 1.0, false);
			SpMelee.Steps = {B::Melee(0.0, 450.0, 0.3, 1900.0).WithStatus(EStatus::Stun, 1.5)};
			FBranch SpRanged = Branch("beam", 1.0, false);
			SpRanged.Steps = {B::Beam(0.0, 2200.0, 140.0, 2500.0)};
			FBranch SpDefence = Branch("taunting guard", 1.0, false);
			SpDefence.Steps = {B::ZoneFollow(0.0, 700.0, 4.0, 0.5, 0.0), B::BuffStep(0.0, EBuff::DamageTaken, -0.4, 4.0)};
			SpDefence.Steps.front().Knockback = -300.0;
			FBranch SpMage = Branch("fear burst", 1.0, false);
			SpMage.Steps = {B::BurstAhead(0.0, 600.0, 800.0, 1800.0).WithStatus(EStatus::Fear, 3.0)};
			FBranch SpSpeed = Branch("haste", 1.0, false);
			SpSpeed.Steps = {B::BuffStep(0.0, EBuff::MoveSpeed, 0.5, 5.0), B::BuffStep(0.0, EBuff::AttackSpeed, 0.3, 5.0)};
			Spot.Branches = {SpMelee, SpRanged, SpDefence, SpMage, SpSpeed};
			Spot.Tags = {"BIB.Type.Utility"};

			FAbilityDef& Punch = SlotDef(K, ESlotId::Move3);
			Punch = B::Def("Jester.Move3", "Punchline", ESlotId::Move3, 20.0, 0.8, 1.5, "The current act's finisher: slam / piercing shot / bulwark / nova / blitz.");
			Punch.BranchMode = EBranchMode::ByAct;
			Punch.BranchAt = 0.5;
			FBranch PuMelee = Branch("slam", 1.0, false);
			PuMelee.Steps = {B::Burst(0.0, 600.0, 3200.0).WithKnock(500.0)};
			FBranch PuRanged = Branch("piercing shot", 1.0, false);
			PuRanged.Steps = {B::Proj(0.0, 1, 3000.0, 2400.0, 140.0, 4000.0)};
			PuRanged.Steps.back().Pierce = 99;
			FBranch PuDefence = Branch("bulwark", 1.0, false);
			PuDefence.Steps = {B::ShieldStep(0.0, 3000.0, 4.0), B::HealStep(0.0, 0.10)};
			FBranch PuMage = Branch("nova", 1.0, false);
			PuMage.Steps = {B::BurstAhead(0.0, 400.0, 1000.0, 3700.0)};
			FBranch PuSpeed = Branch("blitz", 1.0, false);
			PuSpeed.Steps = {B::DashStep(0.0, 1200.0, 0.25, 2900.0, 160.0)};
			PuSpeed.Steps.back().Flags |= FlagInvulnerable;
			Punch.Branches = {PuMelee, PuRanged, PuDefence, PuMage, PuSpeed};
			Punch.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Encore = SlotDef(K, ESlotId::Special);
			Encore = B::Def("Jester.Special", "Encore", ESlotId::Special, 32.0, 0.4, 0.5, "Replays the last three moves for free.");
			Encore.Steps.push_back(B::HookStep(0.3, EHook::JesterEncore).WithCue(CueStar));
			Encore.Tags = {"BIB.Type.Utility"};

			FAbilityDef& Finale = SlotDef(K, ESlotId::Ultimate);
			Finale = B::Def("Jester.Ultimate", "Grand Finale", ESlotId::Ultimate, 0.0, 8.0, 8.6,
				"All five acts in a row (melee, ranged, defence, mage, speed), then a 9000 curtain call.");
			Finale.Steps.push_back(B::Burst(0.5, 700.0, 2500.0).WithKnock(400.0));
			Finale.Steps.push_back(B::Proj(2.1, 5, 2600.0, 1800.0, 80.0, 2000.0, 0.0, 40.0));
			Finale.Steps.push_back(B::ShieldStep(3.7, 4000.0, 4.0));
			Finale.Steps.push_back(B::HealStep(3.7, 0.20));
			Finale.Steps.push_back(B::BurstAhead(5.3, 500.0, 1400.0, 5500.0));
			FStep Blitz = B::DashStep(6.9, 1500.0, 0.3, 3000.0, 180.0);
			Blitz.Flags |= FlagInvulnerable;
			Finale.Steps.push_back(Blitz);
			Finale.Steps.push_back(B::Burst(8.2, 2500.0, 9000.0).WithCue(CueUltimate));
			Finale.Tags = {"BIB.Type.Offensive"};

			FPassiveDef Ovation = B::Passive("Jester.StandingOvation", "Standing Ovation", ETrigger::OnAbilityCast,
				"PROPOSED: after a full set of five casts the next gets +30% damage for 6 s.");
			Ovation.EveryN = 5;
			Ovation.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.30, 6.0));
			K.Passives.push_back(Ovation);
			FPassiveDef Rubber = B::Passive("Jester.RubberFace", "Rubber Face", ETrigger::Always, "PROPOSED: takes 20% less damage from everything.");
			Rubber.Mods[static_cast<int>(EBuff::DamageTaken)] = -0.2;
			K.Passives.push_back(Rubber);
			return K;
		}

		// ====================================================================================== Kuu (4*)
		FKitDef MakeKuu()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Kuu);
			K.Name = "Kuu";
			K.Stars = 4;
			// Signature loop (2026-10-07): Grit (max 100, fades 1 a second). Kuu turns punishment into power: every hit Bulwark
			// takes and every punch builds Grit; Ground Pound spends all of it (+30 per Grit, +50% on stunned enemies).
			K.BaseHP = 5200.0;
			K.BaseAttack = 500.0;
			K.WalkMult = 0.9;
			K.RunMult = 1.0;
			K.bCanSprint = false;
			K.Traits.DamageTakenMod = -0.15;
			K.Traits.bKnockbackImmune = true;
			K.ResourceName[0] = "Grit";
			K.ResourceMax[0] = 100.0;
			K.ResourceRegen[0] = -1.0;
			K.Colour[0] = 0.40f; K.Colour[1] = 0.35f; K.Colour[2] = 0.30f;
			K.Playstyle = "Heavy defender: soaks hits into Grit, stuns with heavy blows and spends the Grit on a crushing Ground Pound.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Kuu.Basic", "Haymaker", 0.8, 3, "Three heavy punches that shove enemies back (+6 Grit each); the third stuns for 0.6 s.");
			Basic.Steps.push_back(B::Melee(0.25, 300.0, 0.2, 0.0, 1.2).WithCombo(0).WithKnock(450.0).WithCue(CueSlam));
			Basic.Steps.push_back(B::Melee(0.3, 310.0, 0.2, 0.0, 1.5).WithCombo(1).WithKnock(550.0).WithCue(CueSlam));
			Basic.Steps.push_back(B::Melee(0.35, 330.0, 0.0, 0.0, 2.2).WithCombo(2).WithKnock(750.0).WithStatus(EStatus::Stun, 0.6).WithCue(CueSlam));
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				Basic.Steps.push_back(B::ResStep(0.25, 0, 6.0).WithCombo(Hit));
			}

			FAbilityDef& Bash = SlotDef(K, ESlotId::Move1);
			Bash = B::Def("Kuu.Move1", "Shield Bash", ESlotId::Move1, 7.0, 0.6, 0.6, "Short heavy bash: 1600, a 1.2 s stun and 15 Grit.");
			Bash.Steps.push_back(B::Melee(0.4, 450.0, 0.3, 1600.0).WithStatus(EStatus::Stun, 1.2).WithKnock(500.0).WithCue(CueSlam));
			Bash.Steps.push_back(B::ResStep(0.4, 0, 15.0));
			Bash.Tags = {"BIB.Type.Control"};

			FAbilityDef& Bulwark = SlotDef(K, ESlotId::Move2);
			Bulwark = B::Def("Kuu.Move2", "Bulwark", ESlotId::Move2, 14.0, 0.0, 4.0,
				"4 s: takes 60% less, reflects 30%, draws enemies in; each of the first 6 hits it takes gives 12 Grit.");
			Bulwark.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.6, 4.0));
			Bulwark.Steps.push_back(B::BuffStep(0.0, EBuff::Reflect, 0.3, 4.0));
			Bulwark.Steps.push_back(B::Burst(0.0, 1000.0, 0.0).WithKnock(-250.0).WithCue(CueShield));
			Bulwark.Reaction.Trigger = EReaction::OnDamaged;
			Bulwark.Reaction.Window = 4.0;
			Bulwark.Reaction.MaxTriggers = 6;
			Bulwark.Reaction.Steps.push_back(B::ResStep(0.0, 0, 12.0));
			Bulwark.Tags = {"BIB.Type.Defensive"};

			FAbilityDef& Pound = SlotDef(K, ESlotId::Move3);
			Pound = B::Def("Kuu.Move3", "Ground Pound", ESlotId::Move3, 12.0, 1.0, 1.0,
				"Spends all Grit: leaps and slams (650) for 1800 + 30 per Grit (+50% on stunned enemies) and a 3 s slow.");
			FStep Leap = B::DashStep(0.0, 500.0, 0.4, 0.0, 0.0);
			Leap.Flags |= FlagInvulnerable;
			Pound.Steps.push_back(Leap);
			FStep Slam = B::Burst(0.5, 650.0, 1800.0).WithStatus(EStatus::Slow, 3.0, 0.4).WithKnock(400.0).WithBonusVs(EStatus::Stun, 0.5).WithCue(CueSlam);
			Slam.Flags |= FlagUseResource;
			Slam.Stat = 0;
			Slam.Param2 = 30.0;
			Pound.Steps.push_back(Slam);
			Pound.Steps.push_back(B::ResStep(0.51, 0, -1000.0));
			Pound.Tags = {"BIB.Type.Offensive", "BIB.Type.Control"};

			FAbilityDef& Wall = SlotDef(K, ESlotId::Special);
			Wall = B::Def("Kuu.Special", "Iron Wall", ESlotId::Special, 30.0, 0.0, 6.0,
				"6 s: a 4000 shield and a barrier that shoves enemies away (400 a half second); heals 10% and gives 30 Grit when it ends.");
			Wall.Steps.push_back(B::ShieldStep(0.0, 4000.0, 6.0));
			FStep Barrier = B::ZoneAt(0.0, 400.0, 400.0, 6.0, 0.5, 400.0);
			Barrier.Knockback = 500.0;
			Barrier.CueId = CueShield;
			Wall.Steps.push_back(Barrier);
			Wall.Steps.push_back(B::HealStep(6.0, 0.10));
			Wall.Steps.push_back(B::ResStep(6.0, 0, 30.0));
			Wall.Tags = {"BIB.Type.Defensive"};

			FAbilityDef& Immovable = SlotDef(K, ESlotId::Ultimate);
			Immovable = B::Def("Kuu.Ultimate", "Immovable", ESlotId::Ultimate, 0.0, 0.0, 8.5,
				"8 s: takes 80% less, cannot be moved, draws everyone in; the hits it took come back as a 1400 blast (4000 + twice the damage), then full Grit.");
			Immovable.bStoresDamageTaken = true;
			Immovable.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.8, 8.0));
			Immovable.Steps.push_back(B::BuffStep(0.0, EBuff::Immovable, 1.0, 8.0));
			FStep Magnet = B::ZoneFollow(0.0, 1800.0, 8.0, 1.0, 0.0);
			Magnet.Knockback = -200.0;
			Immovable.Steps.push_back(Magnet);
			FStep Release = B::Burst(8.0, 1400.0, 4000.0);
			Release.Flags |= FlagUseStored;
			Release.Param2 = 2.0;
			Release.Knockback = 800.0;
			Release.CueId = CueUltimate;
			Immovable.Steps.push_back(Release);
			Immovable.Steps.push_back(B::ResStep(8.0, 0, 100.0));
			Immovable.Tags = {"BIB.Type.Defensive"};
			return K;
		}
	}
}
