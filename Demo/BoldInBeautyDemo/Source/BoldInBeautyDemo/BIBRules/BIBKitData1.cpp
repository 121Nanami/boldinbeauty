// Boldinbeauty demo — kit data, part 1: the owner's Alchemist, Arita and Adapt, then Blood and Death.
//
// Source 1 = written by the owner in Notion (GDD_Characters, read 2026-10-04); Source 2 = PROPOSED by the assistant
// on the owner's request. Where the owner's text is ambiguous the interpretation is noted as WORKING.
#include "BIBCore.h"
#include "BIBKitBuild.h"
#include "BIBKitData.h"

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

			// The owner's "Infection" move: shared by Alchemist and Arita (it is the same Notion page).
			FAbilityDef InfectionMove(ESlotId Slot, const char* Id)
			{
				// "Simply touch a single enemy causing infection, 2 second windup and the animation lasts 1 second with the
				//  goo covering her entire hand, 60 second cd, press 1 to use"
				FAbilityDef D = B::Def(Id, "Infection", Slot, 60.0, 3.0, 3.0, "Touch one enemy: infection (2 s windup, 1 s animation, 60 s cooldown).", 1);
				FStep Touch = B::HookStep(2.0, EHook::InfectTouched, 320.0);
				Touch.CueId = CuePoison;
				D.Steps.push_back(Touch);
				D.Tags = {"BIB.Type.Control", "BIB.Status.Infection"};
				return D;
			}
		}

		// ====================================================================================== Alchemist (5*)
		FKitDef MakeAlchemist()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Alchemist);
			K.Name = "Alchemist";
			K.Stars = 5;
			K.BaseHP = 5200.0;        // PLACEHOLDER (owner has not written stats); 4600 -> 5200 in the balancing pass 2026-10-09
			K.BaseAttack = 1300.0;    // PLACEHOLDER; 800 -> 1300 (2026-10-09: power 0.66 -> ~1.0, in band from 1275 to 1400)
			K.Colour[0] = 0.25f; K.Colour[1] = 0.70f; K.Colour[2] = 0.30f;
			K.Playstyle = "Owner kit: spreads and mutates the infection; saw combo; illusions; soil puddle.";
			// Balancing pass 2026-10-09 (owner: "start the infection, mutate, then instantly swap out ... balance her"): her
			// infection pays off sooner than the shared default (goo from 45 s, explosion at 75 s) and keeps running while she is
			// benched (FRuntime::TickBackground). PLACEHOLDER numbers; Tests/kit_balance.cpp "setup and swap".
			K.InfectionTuning.GooStartsAt = 10.0;
			K.InfectionTuning.GooDamage = 1000.0;
			K.InfectionTuning.ExplodeAt = 40.0;
			K.InfectionTuning.ExplodeDamage = 3500.0;

			// M1 (PROPOSED - the owner wrote no regular attack): "Unarmed" goo slap.
			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Alchemist.Basic", "Goo Slap", 0.6, 2, "Two goo-covered slaps (PROPOSED M1).");
			Basic.Steps.push_back(B::Melee(0.15, 270.0, 0.2, 0.0, 1.2).WithCombo(0));
			Basic.Steps.push_back(B::Melee(0.22, 270.0, 0.2, 0.0, 1.6).WithCombo(1).WithCue(CuePoison)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.15 s

			SlotDef(K, ESlotId::Move1) = InfectionMove(ESlotId::Move1, "Alchemist.Move1");

			// Mutation: "Grabs a syringe and stabs it into the closest enemy causing mutation, 1.5 second windup 0.5 second
			// anim, 45 second cd" (the Notion text stops there; the prerequisites say 45 s cooldown + press 2).
			FAbilityDef& Mutation = SlotDef(K, ESlotId::Move2);
			Mutation = B::Def("Alchemist.Move2", "Mutation", ESlotId::Move2, 45.0, 2.0, 2.0,
				"Syringe into the closest enemy: the uninfected explode (5000), the infected mutate.", 1);
			Mutation.Steps.push_back(B::HookStep(1.5, EHook::MutateNearest, 520.0).WithCue(CueExplosion));
			Mutation.Tags = {"BIB.Type.Control"};

			// Transmutation: 4-part saw attack. Owner decision 2026-10-04: the cooldown is 40 s (Notion also says 50 s in the prerequisites).
			FAbilityDef& Trans = SlotDef(K, ESlotId::Move3);
			Trans = B::Def("Alchemist.Move3", "Transmutation", ESlotId::Move3, 40.0, 9.0, 9.0,
				"Saw charge (866/s stuck), 360 spin (1950), 3 slices (1100), leap smash (2275).", 1);
			// Owner's numbers x1.3 (balancing pass 2026-10-09, owner: "balance her"): 666/s, 1500, 850, 1750 -> 866/s, 1950, 1100, 2275.
			// part 1: charge forward for 5 seconds, 866 damage per second to anything stuck in the saw
			Trans.Steps.push_back(B::DashStep(0.0, 2400.0, 5.0, 0.0, 0.0));
			// (866 per second = 86.6 every 0.1 s, so what is stuck on the saw is carried along with her)
			FStep Saw = B::Beam(0.0, 320.0, 170.0, 86.6);
			Saw.Duration = 5.0;
			Saw.Interval = 0.1;
			Saw.Param = 480.0;   // carry speed (cm/s) = the charge speed
			Saw.CueId = CueSlash;
			Trans.Steps.push_back(Saw);
			// part 2: spin around for 1 second: 360 degree AoE, 1500 if hit
			Trans.Steps.push_back(B::Burst(5.6, 460.0, 1950.0).WithKnock(300.0).WithCue(CueSlam));
			// part 3: three quick slices forward across 2 seconds, 850 per slice
			Trans.Steps.push_back(B::Melee(6.3, 480.0, 0.45, 1100.0));
			Trans.Steps.push_back(B::Melee(7.0, 480.0, 0.45, 1100.0));
			Trans.Steps.push_back(B::Melee(7.7, 480.0, 0.45, 1100.0));
			// part 4: leaps high and smashes the ground, 1750 if hit
			Trans.Steps.push_back(B::Burst(8.6, 560.0, 2275.0).WithKnock(500.0).WithCue(CueSlam));
			Trans.Tags = {"BIB.Type.Offensive"};

			// Special: Ring leader. Owner decision 2026-10-04: it is the Special (Notion also lists it as an Ultimate).
			FAbilityDef& Ring = SlotDef(K, ESlotId::Special);
			Ring = B::Def("Alchemist.Special", "Ring leader", ESlotId::Special, 90.0, 2.0, 5.0,
				"Every infected enemy gains Illusion for 5 s (they attack each other). Needs an infected enemy.", 1);
			Ring.RequiresStatusOnEnemy = EStatus::Infection;
			Ring.Steps.push_back(B::HookStep(2.0, EHook::RingLeader).WithCue(CueGlitch));
			Ring.Tags = {"BIB.Type.Illusion", "BIB.Type.Control", "BIB.Status.Illusion"};

			// Ultimate: "One with the soil" (unlinked in Notion but its modifiers are Infection / Mutation / Immunity: WORKING).
			FAbilityDef& Soil = SlotDef(K, ESlotId::Ultimate);
			Soil = B::Def("Alchemist.Ultimate", "One with the soil", ESlotId::Ultimate, 0.0, 0.0, 20.0,
				"A small puddle leaks around her for 20 s; every enemy that touches it rolls infection / mutation / double damage / lose immunity.", 1);
			FStep Puddle = B::HookStep(0.0, EHook::SoilPuddle);
			Puddle.Duration = 20.0;
			Puddle.CueId = CuePoison;
			Soil.Steps.push_back(Puddle);
			Soil.Tags = {"BIB.Type.Environmental", "BIB.Type.Control"};

			// Passives (Source 1): the infection rules themselves run in the runtime (BIBKitHooks.cpp, TickInfection).
			K.Passives.push_back(B::Passive("Alchemist.Immunity", "Immunity", ETrigger::Always,
				"Is unable to contract infection again unless removed.", 1));
			K.Passives.push_back(B::Passive("Alchemist.Mutation", "Mutation", ETrigger::Always,
				"Mutated enemies: -20% damage and defence, infection attacks +50%, mutation spreads like infection.", 1));
			K.Passives.push_back(B::Passive("Alchemist.Infection", "Infection", ETrigger::Always,
				"Every 10 s a 5 s spreading window; goo 500 per 5 s after 45 s; explodes for 2000 at 75 s and leaves immunity.", 1));
			return K;
		}

		// ====================================================================================== Arita (3*)
		FKitDef MakeArita()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Arita);
			K.Name = "Arita";
			K.Stars = 3;
			K.BaseHP = 2000.0;        // PLACEHOLDER (rarity rebalance 2026-10-07)
			K.BaseAttack = 240.0;
			K.bSpecialOnR = true;     // the owner wrote "R" for Natures Curse
			K.Colour[0] = 0.92f; K.Colour[1] = 0.62f; K.Colour[2] = 0.80f;
			K.Playstyle = "Owner kit: flower field, repelling dome, damage-sharing curse, giant flower creature.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Arita.Basic", "Petal Swipe", 0.55, 2, "Two sweeping swipes (PROPOSED M1).");
			Basic.Steps.push_back(B::Melee(0.12, 250.0, 0.2, 0.0, 1.0).WithCombo(0).WithCue(CueNature));
			Basic.Steps.push_back(B::Melee(0.18, 250.0, 0.2, 0.0, 1.3).WithCombo(1).WithCue(CueNature)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.12 s

			SlotDef(K, ESlotId::Move1) = InfectionMove(ESlotId::Move1, "Arita.Move1");

			// Friend zone: "Creates a mini dome for 3 seconds instantly that repels enemies that would've been inside and
			// repels all attacks cannot be broken 20 second cd" (key 2).
			FAbilityDef& Friend = SlotDef(K, ESlotId::Move2);
			Friend = B::Def("Arita.Move2", "Friend zone", ESlotId::Move2, 20.0, 0.0, 3.0,
				"Mini dome for 3 s: repels enemies and every attack, cannot be broken.", 1);
			FStep Dome = B::ZoneFollow(0.0, 330.0, 3.0, 0.1, 0.0);
			Dome.Knockback = 900.0;
			Dome.CueId = CueShield;
			Friend.Steps.push_back(Dome);
			FStep Immune = B::BuffStep(0.0, EBuff::DamageTaken, -1.0, 3.0);
			Immune.Flags |= FlagInvulnerable;
			Friend.Steps.push_back(Immune);
			Friend.Tags = {"BIB.Type.Defensive", "BIB.Type.Control"};

			// Spring time (key 3, 35 s cd): held; "for every second held increase aoe then when let go releases flower
			// semi densely across the area": thorns 100 on tread, poison flower 200 over 10 s, pheromone stuns 0.5 s.
			FAbilityDef& Spring = SlotDef(K, ESlotId::Move3);
			Spring = B::Def("Arita.Move3", "Spring time", ESlotId::Move3, 35.0, 0.0, 6.0,
				"Hold to grow the area, release to scatter thorns, poison flowers and stunning pheromone flowers.", 1);
			Spring.Cast = ECast::Hold;
			Spring.Steps.push_back(B::HookStep(0.0, EHook::SpringTime));
			Spring.Tags = {"BIB.Type.Environmental", "BIB.Type.Control"};

			// Natures Curse: Special, "R", toggle. Any attack that hits her splits 50/50 with the attacker (soul link),
			// who then takes 1% health per second.
			FAbilityDef& Curse = SlotDef(K, ESlotId::Special);
			Curse = B::Def("Arita.Special", "Natures Curse", ESlotId::Special, 2.0, 0.0, 0.2,
				"Toggle: damage taken splits 50/50 with the attacker; soul-linked attackers lose 1% health per second.", 1);
			Curse.Cast = ECast::Toggle;
			Curse.Steps.push_back(B::HookStep(0.0, EHook::NaturesCurse).WithCue(CueNature));
			Curse.Tags = {"BIB.Type.Defensive", "BIB.Type.Transformation"};

			// Full takeover: turns into a giant flower creature. Owner decision 2026-10-04: it is read as a takeover (a transformation), not a death.
			FAbilityDef& Take = SlotDef(K, ESlotId::Ultimate);
			Take = B::Def("Arita.Ultimate", "Full takeover", ESlotId::Ultimate, 0.0, 0.0, 40.0,
				"40 s giant flower creature: +75% health and damage, M1 x5 damage, 50% faster, x3.5 hitbox.", 1);
			FStep Grow = B::HookStep(0.0, EHook::FullTakeover);
			Grow.Duration = 40.0;
			Grow.CueId = CueNature;
			Take.Steps.push_back(Grow);
			Take.Tags = {"BIB.Type.Transformation"};
			return K;
		}

		// ====================================================================================== Adapt (6*)
		FKitDef MakeAdapt()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Adapt);
			K.Name = "Adapt";
			K.Stars = 6;
			K.BaseHP = 5000.0;        // PLACEHOLDER (attack raised in the rarity rebalance 2026-10-07)
			K.BaseAttack = 640.0;
			K.Colour[0] = 0.92f; K.Colour[1] = 0.92f; K.Colour[2] = 0.96f;
			K.Playstyle = "Owner kit: copies enemy abilities (8 stored), copies teammate passives, drifting stats.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Adapt.Basic", "Glitch Jab", 0.45, 3, "Three quick jabs with a slight screen glitch (PROPOSED M1).");
			Basic.Steps.push_back(B::Melee(0.1, 260.0, 0.2, 0.0, 1.0).WithCombo(0));
			Basic.Steps.push_back(B::Melee(0.1, 260.0, 0.2, 0.0, 1.0).WithCombo(1));
			Basic.Steps.push_back(B::Melee(0.18, 280.0, 0.2, 0.0, 1.6).WithCombo(2).WithCue(CueGlitch)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.1 s

			// "Adaption Ability": being hit by an enemy ability copies it (identical cooldown and damage), 8 stored.
			FAbilityDef& Copy = SlotDef(K, ESlotId::Move1);
			Copy = B::Def("Adapt.Move1", "Adaption Ability", ESlotId::Move1, 1.0, 0.4, 0.4,
				"Casts the selected adapted ability (copied when an enemy ability hits him; 8 stored).", 1);
			Copy.Steps.push_back(B::HookStep(0.2, EHook::AdaptCastCopied));
			Copy.Tags = {"BIB.Type.Utility"};

			// "Adaption Passives": every 30 s copy a passive from a teammate, or gain 25% hp.
			FAbilityDef& Pass = SlotDef(K, ESlotId::Move2);
			Pass = B::Def("Adapt.Move2", "Adaption Passives", ESlotId::Move2, 0.0, 0.0, 0.0,
				"Automatic: every 30 s copies a teammate passive (or heals 25% when none can be copied).", 1);
			Pass.bAutomatic = true;
			Pass.AutoInterval = 30.0;
			Pass.Steps.push_back(B::HookStep(0.0, EHook::AdaptPassiveCopy));
			Pass.Tags = {"BIB.Type.Utility"};

			// "Adaption stats": speed / damage / health drift 1-1 at 5% per second toward what he is doing most (runtime).
			FAbilityDef& Stats = SlotDef(K, ESlotId::Move3);
			Stats = B::Def("Adapt.Move3", "Adaption stats", ESlotId::Move3, 0.0, 0.0, 0.0,
				"Automatic: speed, damage and health shift 1-1 at 5% per second toward the stat he uses most.", 1);
			Stats.bAutomatic = true;
			Stats.Tags = {"BIB.Type.Buff"};

			// Special: Switch (cd 5, duration 1, startup 1, needs at least 2 adapted abilities).
			FAbilityDef& Switch = SlotDef(K, ESlotId::Special);
			Switch = B::Def("Adapt.Special", "Switch", ESlotId::Special, 5.0, 1.0, 1.0,
				"Switch between the adapted abilities 1-8 (needs 2).", 1);
			Switch.Steps.push_back(B::HookStep(1.0, EHook::AdaptSwitch));
			Switch.Tags = {"BIB.Type.Utility"};

			// Ultimate: "Why do i bother". Owner decision 2026-10-04: it is Adapt's ultimate (it was first read as Alchemist's):
			// gives up on anything unique and pulls out an AK-47, 10 shots over 1 second, 2000 each.
			FAbilityDef& Ult = SlotDef(K, ESlotId::Ultimate);
			Ult = B::Def("Adapt.Ultimate", "Why do i bother", ESlotId::Ultimate, 0.0, 1.0, 1.0,
				"Gives up on anything unique and pulls out an AK-47: 10 shots over 1 second, 2000 each.", 1);
			for (int Shot = 0; Shot < 10; ++Shot)
			{
				Ult.Steps.push_back(B::Beam(0.1 * static_cast<double>(Shot), 3000.0, 70.0, 2000.0).WithCue(CueSplash));
			}
			Ult.Tags = {"BIB.Type.Offensive"};

			// Passives (PROPOSED).
			FPassiveDef Study = B::Passive("Adapt.QuickStudy", "Quick Study", ETrigger::OnDamaged,
				"PROPOSED: after being hit (every 8 s) he heals 3% and dodges the next hit.");
			Study.Interval = 8.0;
			Study.Steps.push_back(B::HealStep(0.0, 0.03));
			Study.Steps.push_back(B::BuffStep(0.0, EBuff::Evade, 1.0, 3.0));
			K.Passives.push_back(Study);
			FPassiveDef Step = B::Passive("Adapt.GlitchStep", "Glitch Step", ETrigger::OnDodge,
				"PROPOSED: dodging a hit gives +25% damage for 4 s.");
			Step.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.25, 4.0));
			K.Passives.push_back(Step);
			return K;
		}

		// ====================================================================================== Blood (5*)
		FKitDef MakeBlood()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Blood);
			K.Name = "Blood";
			K.Stars = 5;
			K.BaseHP = 3600.0;        // PROPOSED
			K.BaseAttack = 420.0;
			K.ResourceName[0] = "Blood";
			K.ResourceMax[0] = 100.0;
			K.ResourceRegen[0] = -0.5;   // blood slowly coagulates
			K.Colour[0] = 0.70f; K.Colour[1] = 0.05f; K.Colour[2] = 0.10f;
			K.Playstyle = "Controller: pools, chains and bleeds; manages a blood meter fed by bleeding enemies and sacrifice.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Blood.Basic", "Lancet", 0.5, 3, "Three lancet cuts; each opens a bleed and feeds the blood meter.");
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				const double Mult = Hit == 2 ? 1.2 : 0.8;
				Basic.Steps.push_back(B::Melee(Hit == 2 ? 0.18 : 0.1, 290.0, 0.2, 0.0, Mult).WithCombo(Hit).WithStatus(EStatus::Bleed, 5.0, 90.0, 1).WithCue(CueBlood)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.1 s
				Basic.Steps.push_back(B::ResStep(0.1, 0, 4.0).WithCombo(Hit));
			}

			FAbilityDef& Pool = SlotDef(K, ESlotId::Move1);
			Pool = B::Def("Blood.Move1", "Crimson Pool", ESlotId::Move1, 12.0, 0.6, 0.6,
				"A blood pool (8 s): 130 per half second, slows 30%, every enemy tick feeds +1 blood.");
			FStep Pit = B::ZoneAt(0.6, 700.0, 600.0, 8.0, 0.5, 130.0);
			Pit.Status = EStatus::Slow;
			Pit.StatusSeconds = 1.5;
			Pit.StatusPower = 0.3;
			Pit.Param = 1.0;
			Pit.Stat = 0;
			Pit.CueId = CueBlood;
			Pool.Steps.push_back(Pit);
			Pool.Tags = {"BIB.Type.Control", "BIB.Type.Environmental"};

			FAbilityDef& Chains = SlotDef(K, ESlotId::Move2);
			Chains = B::Def("Blood.Move2", "Clot Chains", ESlotId::Move2, 18.0, 0.8, 0.8,
				"Costs 20 blood: chains pull everything within 1100 into one spot and root it for 2 s (700).");
			Chains.CostResource = 0;
			Chains.CostAmount = 20.0;
			Chains.Steps.push_back(B::BurstAhead(0.5, 600.0, 1100.0, 700.0).WithStatus(EStatus::Root, 2.0).WithKnock(-1400.0).WithCue(CueBlood));
			Chains.Tags = {"BIB.Type.Control"};

			FAbilityDef& Hemo = SlotDef(K, ESlotId::Move3);
			Hemo = B::Def("Blood.Move3", "Hemorrhage", ESlotId::Move3, 25.0, 0.8, 0.8,
				"Sacrifices 10% max health for +40 blood, then detonates every bleed within 1400 (450 per stack, +4 blood each).");
			Hemo.HealthCostFraction = 0.10;
			Hemo.Steps.push_back(B::ResStep(0.0, 0, 40.0));
			Hemo.Steps.push_back(B::DetonateStep(0.4, 1400.0, EStatus::Bleed, 450.0, true, 0, 4.0));
			Hemo.Tags = {"BIB.Type.Resource", "BIB.Type.Offensive"};

			FAbilityDef& Rite = SlotDef(K, ESlotId::Special);
			Rite = B::Def("Blood.Special", "Blood Rite", ESlotId::Special, 35.0, 1.2, 1.2,
				"Costs 50 blood: a 1800 wave deals 1600, weakens 8 s, adds 3 bleeds, pulls in and heals 15%.");
			Rite.CostResource = 0;
			Rite.CostAmount = 50.0;
			Rite.Steps.push_back(B::Burst(1.0, 1800.0, 1600.0).WithStatus(EStatus::Weaken, 8.0, 0.3).WithKnock(-600.0).WithCue(CueBlood));
			Rite.Steps.push_back(B::Burst(1.0, 1800.0, 0.0).WithStatus(EStatus::Bleed, 6.0, 90.0, 3));
			Rite.Steps.push_back(B::HealStep(1.0, 0.15));
			Rite.Tags = {"BIB.Type.Control", "BIB.Type.Healing"};

			FAbilityDef& Sea = SlotDef(K, ESlotId::Ultimate);
			Sea = B::Def("Blood.Ultimate", "Crimson Sea", ESlotId::Ultimate, 0.0, 0.8, 10.5,
				"10 s flood (2500): slows 60%, 700 per second, stacking bleeds, -25% damage taken; the tide ends it for 5200.");
			FStep Flood = B::ZoneFollow(0.8, 2500.0, 10.0, 1.0, 700.0);
			Flood.Status = EStatus::Slow;
			Flood.StatusSeconds = 1.5;
			Flood.StatusPower = 0.6;
			Flood.CueId = CueBlood;
			Sea.Steps.push_back(Flood);
			FStep Bleeds = B::Burst(0.8, 2500.0, 0.0);
			Bleeds.Duration = 10.0;
			Bleeds.Interval = 1.0;
			Bleeds.Status = EStatus::Bleed;
			Bleeds.StatusSeconds = 5.0;
			Bleeds.StatusPower = 110.0;
			Bleeds.CueId = CueNone;
			Sea.Steps.push_back(Bleeds);
			Sea.Steps.push_back(B::BuffStep(0.8, EBuff::DamageTaken, -0.25, 10.0));
			Sea.Steps.push_back(B::Burst(10.8, 2500.0, 5200.0).WithKnock(300.0).WithCue(CueUltimate));
			Sea.Tags = {"BIB.Type.Control", "BIB.Type.Environmental"};

			FPassiveDef Thick = B::Passive("Blood.ThickerThanWater", "Thicker Than Water", ETrigger::Periodic,
				"PROPOSED: with 80+ blood she deals +15% damage.");
			Thick.Interval = 0.5;
			Thick.CondResource = 0;
			Thick.CondMin = 80.0;
			Thick.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.15, 0.6));
			K.Passives.push_back(Thick);
			FPassiveDef Hemophilia = B::Passive("Blood.Hemophilia", "Hemophilia", ETrigger::OnStatusApplied,
				"PROPOSED: a bleed she applies has a 25% chance to add one more stack.");
			Hemophilia.Param = static_cast<double>(EStatus::Bleed);
			Hemophilia.Chance = 0.25;
			FStep Extra = B::Burst(0.0, 1.0, 0.0).WithStatus(EStatus::Bleed, 5.0, 90.0, 1);
			Extra.Flags |= FlagHitTarget;
			Extra.CueId = CueNone;
			Hemophilia.Steps.push_back(Extra);
			K.Passives.push_back(Hemophilia);
			FPassiveDef Scent = B::Passive("Blood.ScentOfBlood", "Scent of Blood", ETrigger::Periodic,
				"PROPOSED: +2 blood per second while any enemy is bleeding.");
			Scent.Interval = 1.0;
			Scent.CondEnemyStatus = EStatus::Bleed;
			Scent.Steps.push_back(B::ResStep(0.0, 0, 2.0));
			K.Passives.push_back(Scent);
			FPassiveDef Transfusion = B::Passive("Blood.Transfusion", "Transfusion", ETrigger::OnKill,
				"PROPOSED: a kill heals 4% max health and gives +10 blood.");
			Transfusion.Steps.push_back(B::HealStep(0.0, 0.04));
			Transfusion.Steps.push_back(B::ResStep(0.0, 0, 10.0));
			K.Passives.push_back(Transfusion);
			return K;
		}

		// ====================================================================================== Death (6*)
		FKitDef MakeDeath()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Death);
			K.Name = "Death";
			K.Stars = 6;
			K.BaseHP = 3400.0;        // PROPOSED
			K.BaseAttack = 950.0;
			K.ResourceName[0] = "Souls";
			K.ResourceMax[0] = 10.0;
			K.Colour[0] = 0.25f; K.Colour[1] = 0.20f; K.Colour[2] = 0.32f;
			K.Playstyle = "One-shots marked enemies (bosses take a fraction) at a price in health; reaps Souls and spends them on Tithe.";

			// Limitations: only enemies she has MARKED (Doom) die outright; each execution costs 3% max health and she
			// cannot execute below 15% health; bosses lose 15% of max health instead.
			// Signature loop (2026-10-07): Souls (max 10) come from kills and every third graze; Tithe spends them all.
			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Death.Basic", "Scythe Graze", 0.55, 3, "Wide grazing cuts that mark the enemy (Doom, 8 s); the third reaps a Soul.");
			Basic.Steps.push_back(B::Melee(0.12, 400.0, -0.1, 0.0, 0.6).WithCombo(0).WithStatus(EStatus::Doom, 8.0).WithCue(CueMark));
			Basic.Steps.push_back(B::Melee(0.12, 400.0, -0.1, 0.0, 0.7).WithCombo(1).WithStatus(EStatus::Doom, 8.0).WithCue(CueMark));
			Basic.Steps.push_back(B::Melee(0.2, 420.0, -0.1, 0.0, 1.1).WithCombo(2).WithStatus(EStatus::Doom, 8.0).WithCue(CueExecute)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.15 s
			Basic.Steps.push_back(B::ResStep(0.15, 0, 1.0).WithCombo(2));

			FAbilityDef& Harvest = SlotDef(K, ESlotId::Move1);
			Harvest = B::Def("Death.Move1", "Harvest", ESlotId::Move1, 6.0, 0.7, 0.7,
				"270 degree sweep: marked enemies die (3% health each; bosses lose 9%); the rest take 1.5x attack and are marked.");
			FStep Reap = B::ExecStep(0.5, FlagMarkedOnly | FlagRespectBoss, 0.0, 0.03, 0.15, 0.09);
			Reap.Range = 520.0;
			Reap.ArcDot = -0.3;
			Harvest.Steps.push_back(Reap);
			Harvest.Steps.push_back(B::Melee(0.52, 520.0, -0.3, 0.0, 1.5).WithStatus(EStatus::Doom, 8.0).WithCue(CueExecute));
			Harvest.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Step = SlotDef(K, ESlotId::Move2);
			Step = B::Def("Death.Move2", "Shadow Step", ESlotId::Move2, 8.0, 0.6, 0.6,
				"Blinks behind the nearest enemy (0.5 s untouchable) and strikes: finishes anything under 35% health.");
			FStep Slip = B::Blink(0.0, 1500.0, FlagNearestEnemy | FlagBehindTarget | FlagStoreTarget | FlagInvulnerable, 0.5);
			Step.Steps.push_back(Slip);
			FStep Finish = B::ExecStep(0.2, FlagStoredTarget | FlagRespectBoss, 0.35, 0.03, 0.15, 0.05);
			Step.Steps.push_back(Finish);
			FStep Stab = B::Melee(0.22, 320.0, -1.0, 0.0, 2.5);
			Stab.Flags |= FlagStoredTarget;
			Stab.Status = EStatus::Doom;
			Stab.StatusSeconds = 8.0;
			Step.Steps.push_back(Stab);
			Step.Tags = {"BIB.Type.Mobility", "BIB.Type.Offensive"};

			FAbilityDef& Tithe = SlotDef(K, ESlotId::Move3);
			Tithe = B::Def("Death.Move3", "Tithe", ESlotId::Move3, 14.0, 0.6, 0.6,
				"Spends every Soul: each marked enemy within 1500 is dragged in and takes 2000 + 900 per Soul.");
			FStep Collect = B::Burst(0.5, 1500.0, 2000.0);
			Collect.Flags |= FlagMarkedOnly | FlagUseResource;
			Collect.Stat = 0;
			Collect.Param2 = 900.0;
			Collect.Knockback = -500.0;
			Collect.CueId = CueMark;
			Tithe.Steps.push_back(Collect);
			Tithe.Steps.push_back(B::ResStep(0.51, 0, -1000.0));
			Tithe.Tags = {"BIB.Type.Control"};

			FAbilityDef& Rites = SlotDef(K, ESlotId::Special);
			Rites = B::Def("Death.Special", "Last Rites", ESlotId::Special, 30.0, 2.0, 2.0,
				"Pays 10% max health: after 2 s the nearest enemy dies (a boss loses 25%).");
			Rites.HealthCostFraction = 0.10;
			FStep Final = B::ExecStep(2.0, FlagNearestEnemy | FlagRespectBoss, 1.0, 0.0, 0.0, 0.25);
			Final.Range = 1400.0;
			Rites.Steps.push_back(Final);
			Rites.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Ledger = SlotDef(K, ESlotId::Ultimate);
			Ledger = B::Def("Death.Ultimate", "Reaper's Ledger", ESlotId::Ultimate, 0.0, 4.0, 4.5,
				"Marks every enemy; 3 s later every marked enemy dies (bosses lose 35%). The price: 25% of her max health.");
			Ledger.Steps.push_back(B::Burst(0.5, 100.0, 0.0).WithFlags(FlagAllEnemies).WithStatus(EStatus::Doom, 8.0).WithCue(CueMark));
			Ledger.Steps.push_back(B::SelfCostStep(0.5, 0.25));
			Ledger.Steps.push_back(B::ExecStep(3.5, FlagAllEnemies | FlagMarkedOnly | FlagRespectBoss, 0.0, 0.0, 0.0, 0.35).WithCue(CueUltimate));
			Ledger.Tags = {"BIB.Type.Offensive"};

			FPassiveDef Debt = B::Passive("Death.FinalDebt", "Final Debt", ETrigger::OnHit,
				"PROPOSED: any non-boss enemy she hurts below 20% health is executed outright.");
			FStep Collect2 = B::ExecStep(0.0, FlagHitTarget | FlagIgnoreBoss, 0.20, 0.0, 0.0);
			Debt.Steps.push_back(Collect2);
			K.Passives.push_back(Debt);
			FPassiveDef Memento = B::Passive("Death.Memento", "Memento", ETrigger::OnKill,
				"PROPOSED: every kill returns 1.5% health (half the toll) and a soul.");
			Memento.Steps.push_back(B::HealStep(0.0, 0.015));
			Memento.Steps.push_back(B::ResStep(0.0, 0, 1.0));
			K.Passives.push_back(Memento);
			FPassiveDef Rider = B::Passive("Death.PaleRider", "Pale Rider", ETrigger::Periodic,
				"PROPOSED: +25% movement speed and +20% damage while any enemy is marked.");
			Rider.Interval = 0.5;
			Rider.CondEnemyStatus = EStatus::Doom;
			Rider.Steps.push_back(B::BuffStep(0.0, EBuff::MoveSpeed, 0.25, 0.6));
			Rider.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.20, 0.6));
			K.Passives.push_back(Rider);
			FPassiveDef Due = B::Passive("Death.ReapersDue", "Reaper's Due", ETrigger::Periodic,
				"PROPOSED: while any enemy is marked she regenerates 1.2% max health per second - the toll is paid back by fighting.");
			Due.Interval = 0.5;
			Due.CondEnemyStatus = EStatus::Doom;
			Due.Steps.push_back(B::BuffStep(0.0, EBuff::Regen, 0.012, 0.6));
			K.Passives.push_back(Due);
			return K;
		}
	}
}
