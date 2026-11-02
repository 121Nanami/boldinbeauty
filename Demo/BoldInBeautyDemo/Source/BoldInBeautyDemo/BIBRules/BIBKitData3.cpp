// Boldinbeauty demo — kit data, part 3: Media, Myrcia, Plack, Shiraishi, Stargazer, Timekeeper, Weather
// (every number PROPOSED, 2026-10-04; signature loops and rarity rebalance PROPOSED 2026-10-07, see docs/operator-kits.md).
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

			FBranch Branch(const char* Label)
			{
				FBranch Result;
				Result.Label = Label;
				return Result;
			}

			// Spends the whole kit resource just after a pay-off step has read it (FlagUseResource).
			FStep SpendAll(double At, int Resource)
			{
				return B::ResStep(At, Resource, -1000.0);
			}
		}

		// ====================================================================================== Media (6*)
		// Signature loop (2026-10-07): every jab and blink buffers Data (max 10); Ping Storm flushes the buffer, each strike
		// +30 per Data. Lag Spike makes enemies vulnerable, and Blink Cut hits vulnerable enemies 40% harder.
		FKitDef MakeMedia()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Media);
			K.Name = "Media";
			K.Stars = 6;
			K.BaseHP = 2000.0;
			K.BaseAttack = 335.0;      // 310 -> 335 (2026-10-09: back inside the 6-star band)
			K.WalkMult = 1.3;
			K.RunMult = 1.7;
			K.JumpMult = 1.3;
			K.DodgeCooldown = 0.4;
			K.DodgeDistanceMult = 1.6;
			K.ResourceName[0] = "Data";
			K.ResourceMax[0] = 10.0;
			K.Colour[0] = 0.10f; K.Colour[1] = 0.90f; K.Colour[2] = 0.90f;
			K.Playstyle = "Extreme speedster: buffers Data with teleporting jabs and flushes it in Ping Storm; 20% of hits never land.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Media.Basic", "Packet Jab", 0.25, 5, "Blinks to the nearest enemy and jabs: a five-hit chain, each hit buffers 1 Data; the fifth bursts (x1.6).");
			Basic.Steps.push_back(B::Blink(0.0, 650.0, FlagNearestEnemy));
			for (int Hit = 0; Hit < 5; ++Hit)
			{
				Basic.Steps.push_back(B::Melee(Hit == 4 ? 0.1 : 0.05, 280.0, -1.0, 0.0, Hit == 4 ? 0.8 : 0.5).WithCombo(Hit).WithCue(CueGlitch)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.05 s
				Basic.Steps.push_back(B::ResStep(0.05, 0, 1.0).WithCombo(Hit));
			}

			FAbilityDef& Cut = SlotDef(K, ESlotId::Move1);
			Cut = B::Def("Media.Move1", "Blink Cut", ESlotId::Move1, 3.0, 0.2, 0.2,
				"Teleports 800 forward through anything in the way (560 each, +40% on vulnerable) and buffers 2 Data.");
			FStep Slice = B::DashStep(0.0, 800.0, 0.12, 560.0, 140.0);
			Slice.Flags |= FlagInvulnerable;
			Slice.CueId = CueBlink;
			Slice.BonusStatus = EStatus::Vulnerable;
			Slice.BonusVs = 0.4;
			Cut.Steps.push_back(Slice);
			Cut.Steps.push_back(B::ResStep(0.12, 0, 2.0));
			Cut.Tags = {"BIB.Type.Mobility", "BIB.Type.Offensive"};

			FAbilityDef& Lag = SlotDef(K, ESlotId::Move2);
			Lag = B::Def("Media.Move2", "Lag Spike", ESlotId::Move2, 12.0, 0.3, 0.3,
				"A 900 pulse: 450, slows 50% and makes enemies 20% more vulnerable for 6 s.");
			Lag.Steps.push_back(B::Burst(0.3, 900.0, 450.0).WithStatus(EStatus::Slow, 6.0, 0.5).WithCue(CueGlitch));
			Lag.Steps.push_back(B::Burst(0.3, 900.0, 0.0).WithStatus(EStatus::Vulnerable, 6.0, 0.20).WithCue(CueNone));
			Lag.Tags = {"BIB.Type.Control"};

			FAbilityDef& Ping = SlotDef(K, ESlotId::Move3);
			Ping = B::Def("Media.Move3", "Ping Storm", ESlotId::Move3, 14.0, 1.5, 1.6,
				"Flushes the Data buffer: eight untouchable teleport strikes on random enemies (360 each, +30 per Data).");
			FStep Hop = B::Blink(0.0, 1400.0, FlagRandomEnemy | FlagInvulnerable, 0.2);
			Hop.Duration = 1.4;
			Hop.Interval = 0.175;
			Ping.Steps.push_back(Hop);
			FStep Jab = B::Melee(0.04, 330.0, -1.0, 360.0);
			Jab.Duration = 1.4;
			Jab.Interval = 0.175;
			Jab.Flags |= FlagUseResource;
			Jab.Stat = 0;
			Jab.Param2 = 30.0;
			Ping.Steps.push_back(Jab);
			Ping.Steps.push_back(SpendAll(1.5, 0));
			Ping.Tags = {"BIB.Type.Mobility", "BIB.Type.Offensive"};

			FAbilityDef& Wall = SlotDef(K, ESlotId::Special);
			Wall = B::Def("Media.Special", "Firewall", ESlotId::Special, 26.0, 0.6, 6.6, "A 6 s line of data fire ahead: 340 per half second and a slow.");
			for (const double Ahead : {500.0, 900.0, 1300.0})
			{
				Wall.Steps.push_back(B::ZoneAt(0.5, Ahead, 330.0, 6.0, 0.5, 340.0).WithStatus(EStatus::Slow, 1.0, 0.5).WithCue(CueFlame));
			}
			Wall.Tags = {"BIB.Type.Environmental", "BIB.Type.Control"};

			FAbilityDef& Overload = SlotDef(K, ESlotId::Ultimate);
			Overload = B::Def("Media.Ultimate", "System Overload", ESlotId::Ultimate, 0.0, 6.0, 6.5,
				"6 s unbound: a teleport strike every 0.15 s (560, stuns 0.3 s) - about 40 hits; ends with a full Data buffer.");
			FStep Warp = B::Blink(0.0, 2200.0, FlagRandomEnemy | FlagInvulnerable, 0.2);
			Warp.Duration = 6.0;
			Warp.Interval = 0.15;
			Overload.Steps.push_back(Warp);
			FStep Glitch = B::Melee(0.03, 340.0, -1.0, 560.0);
			Glitch.Duration = 6.0;
			Glitch.Interval = 0.15;
			Glitch.Status = EStatus::Stun;
			Glitch.StatusSeconds = 0.3;
			Glitch.CueId = CueGlitch;
			Overload.Steps.push_back(Glitch);
			Overload.Steps.push_back(B::ResStep(6.2, 0, 10.0));
			Overload.Tags = {"BIB.Type.Offensive", "BIB.Type.Mobility"};

			FPassiveDef Latency = B::Passive("Media.LowLatency", "Low Latency", ETrigger::OnAbilityCast,
				"PROPOSED: after Blink Cut the next hits deal +40% for 2 s.");
			Latency.Param = static_cast<double>(ESlotId::Move1);
			Latency.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.4, 2.0));
			K.Passives.push_back(Latency);
			FPassiveDef Clock = B::Passive("Media.Overclock", "Overclock", ETrigger::Always, "PROPOSED: +15% movement and attack speed.");
			Clock.Mods[static_cast<int>(EBuff::MoveSpeed)] = 0.15;
			Clock.Mods[static_cast<int>(EBuff::AttackSpeed)] = 0.15;
			K.Passives.push_back(Clock);
			FPassiveDef Loss = B::Passive("Media.PacketLoss", "Packet Loss", ETrigger::Always, "PROPOSED: 20% of incoming hits never arrive.");
			Loss.Mods[static_cast<int>(EBuff::EvadeChance)] = 0.2;
			K.Passives.push_back(Loss);
			FPassiveDef Dump = B::Passive("Media.BufferOverflow", "Buffer Overflow", ETrigger::OnDodge,
				"PROPOSED: a hit that never arrives buffers 2 Data.");
			Dump.Steps.push_back(B::ResStep(0.0, 0, 2.0));
			K.Passives.push_back(Dump);
			return K;
		}

		// ====================================================================================== Myrcia (3*)
		// Signature loop (2026-10-07): the riptide. Her third mace blow and Cleave soak enemies; Brace answers up to three hits;
		// Charge and Cleave pay off against stunned / soaked enemies; Last Bastion returns the damage it soaked.
		FKitDef MakeMyrcia()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Myrcia);
			K.Name = "Myrcia";
			K.Stars = 3;
			K.BaseHP = 3000.0;
			K.BaseAttack = 430.0;
			K.WalkMult = 0.95;
			K.Traits.DamageTakenMod = -0.20;
			K.Colour[0] = 0.60f; K.Colour[1] = 0.30f; K.Colour[2] = 0.20f;
			K.Playstyle = "Heavy defender: soaks enemies with tidal mace blows, answers hits from Brace and turns stuns into big cleaves.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Myrcia.Basic", "Mace Smash", 0.7, 3, "Three heavy mace blows; the third is a tidal slam that soaks (Wet).");
			Basic.Steps.push_back(B::Melee(0.2, 290.0, 0.2, 0.0, 1.1).WithCombo(0).WithKnock(300.0).WithCue(CueSlam));
			Basic.Steps.push_back(B::Melee(0.25, 300.0, 0.2, 0.0, 1.3).WithCombo(1).WithKnock(350.0).WithCue(CueSlam));
			Basic.Steps.push_back(B::Melee(0.3, 340.0, -0.2, 0.0, 1.9).WithCombo(2).WithKnock(450.0).WithStatus(EStatus::Wet, 6.0).WithCue(CueSplash));

			FAbilityDef& Brace = SlotDef(K, ESlotId::Move1);
			Brace = B::Def("Myrcia.Move1", "Brace", ESlotId::Move1, 9.0, 0.0, 3.0,
				"3 s guard (-50% damage); the first three hits are answered with 1100 and a stun.");
			Brace.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.5, 3.0));
			Brace.Reaction.Trigger = EReaction::OnDamaged;
			Brace.Reaction.Window = 3.0;
			Brace.Reaction.MaxTriggers = 3;
			Brace.Reaction.Steps.push_back(B::Burst(0.0, 1.0, 1100.0).WithStatus(EStatus::Stun, 0.8).WithFlags(FlagHitTarget));
			Brace.Tags = {"BIB.Type.Defensive"};

			FAbilityDef& Cleave = SlotDef(K, ESlotId::Move2);
			Cleave = B::Def("Myrcia.Move2", "Cleave", ESlotId::Move2, 7.0, 0.5, 0.5,
				"A full-circle swing (420): 1300 and a shove that soaks; +60% on stunned enemies.");
			Cleave.Steps.push_back(B::Melee(0.35, 420.0, -1.0, 1300.0).WithKnock(350.0).WithStatus(EStatus::Wet, 6.0).WithBonusVs(EStatus::Stun, 0.6).WithCue(CueSlam));
			Cleave.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Charge = SlotDef(K, ESlotId::Move3);
			Charge = B::Def("Myrcia.Move3", "Charge", ESlotId::Move3, 12.0, 0.5, 0.5,
				"Rushes 900 forward on a wave, knocking enemies aside (1400, +50% on soaked, 0.8 s stun).");
			FStep Rush = B::DashStep(0.0, 900.0, 0.5, 1400.0, 160.0);
			Rush.Flags |= FlagInvulnerable;
			Rush.Status = EStatus::Stun;
			Rush.StatusSeconds = 0.8;
			Rush.Knockback = 600.0;
			Rush.WetBonus = 0.5;
			Charge.Steps.push_back(Rush);
			Charge.Tags = {"BIB.Type.Mobility", "BIB.Type.Control"};

			FAbilityDef& Fort = SlotDef(K, ESlotId::Special);
			Fort = B::Def("Myrcia.Special", "Fortify", ESlotId::Special, 28.0, 0.0, 10.0,
				"10 s: -40% damage taken and 2% health regeneration per second; an undertow (1000) drags and soaks enemies around her for 700.");
			Fort.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.4, 10.0));
			Fort.Steps.push_back(B::BuffStep(0.0, EBuff::Regen, 0.02, 10.0));
			Fort.Steps.push_back(B::Burst(0.0, 1000.0, 700.0).WithKnock(-600.0).WithStatus(EStatus::Wet, 6.0).WithCue(CueSplash));
			Fort.Tags = {"BIB.Type.Defensive", "BIB.Type.Healing"};

			FAbilityDef& Bastion = SlotDef(K, ESlotId::Ultimate);
			Bastion = B::Def("Myrcia.Ultimate", "Last Bastion", ESlotId::Ultimate, 0.0, 0.0, 6.5,
				"6 s: -90% damage taken, cannot be moved, draws enemies in; ends with a 5000 shockwave plus twice the damage she soaked.");
			Bastion.bStoresDamageTaken = true;
			Bastion.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.9, 6.0));
			Bastion.Steps.push_back(B::BuffStep(0.0, EBuff::Immovable, 1.0, 6.0));
			FStep Pull = B::ZoneFollow(0.0, 1500.0, 6.0, 1.0, 0.0);
			Pull.Knockback = -150.0;
			Bastion.Steps.push_back(Pull);
			FStep Wave = B::Burst(6.0, 900.0, 5000.0).WithKnock(900.0).WithCue(CueUltimate);
			Wave.Flags |= FlagUseStored;
			Wave.Param2 = 2.0;
			Bastion.Steps.push_back(Wave);
			Bastion.Tags = {"BIB.Type.Defensive"};
			return K;
		}

		// ====================================================================================== Plack (4*)
		// Signature loop (2026-10-07): Momentum (max 100, fades 3 a second). Combo hits, Rush and a perfect parry build it;
		// Spinning Kick cashes it all in (+20 per Momentum, +50% on stunned enemies). Second Wind and Overdrive refill it.
		FKitDef MakePlack()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Plack);
			K.Name = "Plack";
			K.Stars = 4;
			K.BaseHP = 3800.0;
			K.BaseAttack = 540.0;
			K.ResourceName[0] = "Momentum";
			K.ResourceMax[0] = 100.0;
			K.ResourceRegen[0] = -3.0;
			K.Colour[0] = 0.90f; K.Colour[1] = 0.55f; K.Colour[2] = 0.15f;
			K.Playstyle = "Generalist: builds Momentum with combos, rushes and parries and cashes it in with Spinning Kick.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Plack.Basic", "Combo Strike", 0.45, 3, "A three-hit combo (+8 Momentum a hit) that finishes with a heavier, burning blow.");
			Basic.Steps.push_back(B::Melee(0.12, 280.0, 0.1, 0.0, 0.9).WithCombo(0));
			Basic.Steps.push_back(B::Melee(0.12, 280.0, 0.1, 0.0, 1.0).WithCombo(1));
			Basic.Steps.push_back(B::Melee(0.18, 300.0, 0.1, 0.0, 1.6).WithCombo(2).WithKnock(350.0).WithStatus(EStatus::Burn, 4.0, 60.0, 1).WithCue(CueSlam));
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				Basic.Steps.push_back(B::ResStep(0.12, 0, 8.0).WithCombo(Hit));
			}

			FAbilityDef& Rush = SlotDef(K, ESlotId::Move1);
			Rush = B::Def("Plack.Move1", "Rush", ESlotId::Move1, 6.0, 0.25, 0.25, "Dashes 600 forward (1000 to everything crossed) and gains 20 Momentum.");
			FStep Run = B::DashStep(0.0, 600.0, 0.2, 1000.0, 130.0);
			Run.Flags |= FlagInvulnerable;
			Rush.Steps.push_back(Run);
			Rush.Steps.push_back(B::ResStep(0.2, 0, 20.0));
			Rush.Tags = {"BIB.Type.Mobility", "BIB.Type.Offensive"};

			FAbilityDef& Guard = SlotDef(K, ESlotId::Move2);
			Guard = B::Def("Plack.Move2", "Guard Stance", ESlotId::Move2, 12.0, 0.0, 2.0,
				"2 s guard (-50%); a hit in the first 0.4 s is parried: 1000, a 1.5 s stun and +40 Momentum.");
			Guard.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.5, 2.0));
			Guard.Reaction.Trigger = EReaction::OnDamaged;
			Guard.Reaction.Window = 0.4;
			Guard.Reaction.MaxTriggers = 1;
			Guard.Reaction.bNegate = true;
			Guard.Reaction.Steps.push_back(B::Burst(0.0, 1.0, 1000.0).WithStatus(EStatus::Stun, 1.5).WithFlags(FlagHitTarget));
			Guard.Reaction.Steps.push_back(B::ResStep(0.0, 0, 40.0));
			Guard.Tags = {"BIB.Type.Defensive"};

			FAbilityDef& Kick = SlotDef(K, ESlotId::Move3);
			Kick = B::Def("Plack.Move3", "Spinning Kick", ESlotId::Move3, 9.0, 0.6, 0.6,
				"Spends all Momentum: a 500 spin for 1200 + 25 per Momentum (+50% on stunned enemies) and knockback.");
			FStep Spin = B::Burst(0.3, 500.0, 1200.0).WithKnock(600.0).WithBonusVs(EStatus::Stun, 0.5).WithCue(CueSlam);
			Spin.Flags |= FlagUseResource;
			Spin.Stat = 0;
			Spin.Param2 = 25.0;
			Kick.Steps.push_back(Spin);
			Kick.Steps.push_back(SpendAll(0.31, 0));
			Kick.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Wind = SlotDef(K, ESlotId::Special);
			Wind = B::Def("Plack.Special", "Second Wind", ESlotId::Special, 30.0, 0.0, 5.0, "5 s: heals 25% of max health, +20% speed and +50 Momentum.");
			Wind.Steps.push_back(B::BuffStep(0.0, EBuff::Regen, 0.05, 5.0));
			Wind.Steps.push_back(B::BuffStep(0.0, EBuff::MoveSpeed, 0.2, 5.0));
			Wind.Steps.push_back(B::ResStep(0.0, 0, 50.0));
			Wind.Tags = {"BIB.Type.Healing"};

			FAbilityDef& Drive = SlotDef(K, ESlotId::Ultimate);
			Drive = B::Def("Plack.Ultimate", "Overdrive", ESlotId::Ultimate, 0.0, 0.0, 10.5,
				"10 s: +30% damage, +40% speed, -30% damage taken, 60% faster attacks, full Momentum; ends with a 3000 burst.");
			Drive.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.3, 10.0));
			Drive.Steps.push_back(B::BuffStep(0.0, EBuff::MoveSpeed, 0.4, 10.0));
			Drive.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.3, 10.0));
			Drive.Steps.push_back(B::BuffStep(0.0, EBuff::AttackSpeed, 0.6, 10.0));
			Drive.Steps.push_back(B::ResStep(0.0, 0, 100.0));
			Drive.Steps.push_back(B::Burst(10.0, 1100.0, 3000.0).WithKnock(500.0).WithCue(CueUltimate));
			Drive.Tags = {"BIB.Type.Buff"};
			return K;
		}

		// ====================================================================================== Shiraishi (3*)
		// Signature loop (2026-10-07): Heat. Punches, Flame Dash and Cinder Shield build it; Eruption needs 30 and burns all of
		// it (1000 + 30 per Heat); her finisher and Eruption hit burning enemies harder. Phoenix Guard and Inferno Soul refill it.
		FKitDef MakeShiraishi()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Shiraishi);
			K.Name = "Shiraishi";
			K.Stars = 3;
			K.BaseHP = 1400.0;
			K.BaseAttack = 140.0;
			K.Traits.StatusResist = 0.6;
			K.Traits.bImmuneToBurn = true;
			K.Traits.InitialShield = 2200.0;
			K.Traits.MaxShield = 2200.0;
			K.Traits.ShieldRegenPerSecond = 220.0;
			K.Traits.ShieldRegenDelay = 4.0;
			K.ResourceName[0] = "Heat";
			K.ResourceMax[0] = 100.0;
			K.ResourceRegen[0] = -2.0;
			K.Colour[0] = 1.00f; K.Colour[1] = 0.40f; K.Colour[2] = 0.10f;
			K.Playstyle = "Close-range fire fighter: a regenerating shield, Heat from every punch, and Eruption that burns all of it.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Shiraishi.Basic", "Fire Fist", 0.45, 3, "Three burning punches; each adds a Burn stack and 6 Heat, the third hits burning enemies 40% harder.");
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				// 2026-10-13 hit feel: the third punch winds up longer (rhythm), was 0.1 s.
				FStep Punch = B::Melee(Hit == 2 ? 0.18 : 0.1, 270.0, 0.2, 0.0, Hit == 2 ? 1.4 : 1.0).WithCombo(Hit).WithStatus(EStatus::Burn, 5.0, 45.0, 1).WithCue(CueFlame);
				if (Hit == 2)
				{
					Punch.WithBonusVs(EStatus::Burn, 0.4);
				}
				Basic.Steps.push_back(Punch);
				Basic.Steps.push_back(B::ResStep(0.1, 0, 6.0).WithCombo(Hit));
			}

			FAbilityDef& Dash = SlotDef(K, ESlotId::Move1);
			Dash = B::Def("Shiraishi.Move1", "Flame Dash", ESlotId::Move1, 8.0, 0.25, 0.25, "Dashes 500 in a trail of fire: 550, two Burn stacks and 15 Heat.");
			FStep Trail = B::DashStep(0.0, 500.0, 0.2, 550.0, 140.0);
			Trail.Status = EStatus::Burn;
			Trail.StatusSeconds = 5.0;
			Trail.StatusPower = 70.0;
			Trail.StatusStacks = 2;
			Trail.CueId = CueFlame;
			Dash.Steps.push_back(Trail);
			Dash.Steps.push_back(B::ResStep(0.2, 0, 15.0));
			Dash.Tags = {"BIB.Type.Mobility", "BIB.Type.Offensive"};

			FAbilityDef& Cinder = SlotDef(K, ESlotId::Move2);
			Cinder = B::Def("Shiraishi.Move2", "Cinder Shield", ESlotId::Move2, 14.0, 0.0, 5.0, "+1500 shield for 5 s, a burning ring (350) around her and 10 Heat.");
			Cinder.Steps.push_back(B::ShieldStep(0.0, 1500.0, 5.0));
			Cinder.Steps.push_back(B::ZoneFollow(0.0, 350.0, 5.0, 1.0, 200.0).WithStatus(EStatus::Burn, 3.0, 60.0, 1).WithCue(CueFlame));
			Cinder.Steps.push_back(B::ResStep(0.0, 0, 10.0));
			Cinder.Tags = {"BIB.Type.Defensive"};

			FAbilityDef& Eruption = SlotDef(K, ESlotId::Move3);
			Eruption = B::Def("Shiraishi.Move3", "Eruption", ESlotId::Move3, 12.0, 0.7, 0.7,
				"Needs 30 Heat and burns all of it: a 600 eruption for 800 + 20 per Heat (+30% on burning enemies) and three Burn stacks.");
			Eruption.RequiresResource = 0;
			Eruption.RequiresAmount = 30.0;
			FStep Blast = B::Burst(0.6, 600.0, 800.0).WithStatus(EStatus::Burn, 5.0, 70.0, 3).WithBonusVs(EStatus::Burn, 0.3).WithCue(CueExplosion);
			Blast.Flags |= FlagUseResource;
			Blast.Stat = 0;
			Blast.Param2 = 20.0;
			Eruption.Steps.push_back(Blast);
			Eruption.Steps.push_back(SpendAll(0.61, 0));
			Eruption.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Phoenix = SlotDef(K, ESlotId::Special);
			Phoenix = B::Def("Shiraishi.Special", "Phoenix Guard", ESlotId::Special, 35.0, 0.0, 8.0,
				"For 8 s, if her shield breaks it explodes for 2500 (800), half refills and her Heat jumps by 50.");
			Phoenix.Reaction.Trigger = EReaction::OnShieldBreak;
			Phoenix.Reaction.Window = 8.0;
			Phoenix.Reaction.MaxTriggers = 1;
			Phoenix.Reaction.Steps.push_back(B::Burst(0.0, 800.0, 2500.0).WithStatus(EStatus::Burn, 5.0, 80.0, 2).WithCue(CueExplosion));
			Phoenix.Reaction.Steps.push_back(B::ShieldStep(0.0, 1100.0, 0.0));
			Phoenix.Reaction.Steps.push_back(B::ResStep(0.0, 0, 50.0));
			Phoenix.Steps.push_back(B::CueStep(0.0, CueFlame, 250.0, 8.0));
			Phoenix.Tags = {"BIB.Type.Defensive", "BIB.Type.Offensive"};

			FAbilityDef& Soul = SlotDef(K, ESlotId::Ultimate);
			Soul = B::Def("Shiraishi.Ultimate", "Inferno Soul", ESlotId::Ultimate, 0.0, 0.0, 8.0,
				"8 s: +2200 shield, -50% damage taken, a 700 aura burning 700 a second, and full Heat.");
			Soul.Steps.push_back(B::ShieldStep(0.0, 2200.0, 8.0));
			Soul.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, -0.5, 8.0));
			Soul.Steps.push_back(B::ZoneFollow(0.0, 700.0, 8.0, 1.0, 700.0).WithStatus(EStatus::Burn, 4.0, 80.0, 2).WithCue(CueUltimate));
			Soul.Steps.push_back(B::ResStep(0.0, 0, 100.0));
			Soul.Tags = {"BIB.Type.Offensive", "BIB.Type.Defensive"};
			return K;
		}

		// ====================================================================================== Stargazer (5*)
		// Signature loop (2026-10-07): starlight is now a bank. Starlit and kills add one (max 5); Falling Star spends all of
		// it (+600 per starlight), so waiting for a full bank is a choice. Constellation stars hit marked (seeded) enemies harder.
		FKitDef MakeStargazer()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Stargazer);
			K.Name = "Stargazer";
			K.Stars = 5;
			K.BaseHP = 1900.0;
			K.BaseAttack = 440.0;
			K.ResourceName[0] = "Starlight";
			K.ResourceMax[0] = 5.0;
			K.Colour[0] = 0.50f; K.Colour[1] = 0.40f; K.Colour[2] = 1.00f;
			K.Playstyle = "Glass cannon: long channels and seeds; banks starlight (up to 5) and spends it all on Falling Star.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Stargazer.Basic", "Starshot", 0.5, 1, "A starlight bolt (range 1800).");
			Basic.Steps.push_back(B::Proj(0.1, 1, 2400.0, 1800.0, 55.0, 0.0, 0.8).WithCue(CueStar));

			FAbilityDef& Falling = SlotDef(K, ESlotId::Move1);
			Falling = B::Def("Stargazer.Move1", "Falling Star", ESlotId::Move1, 9.0, 1.0, 1.0,
				"A meteor on the aim point (450): 1500, +600 per starlight - and every starlight is spent.");
			FStep Star = B::BurstAhead(1.0, 900.0, 450.0, 1500.0);
			Star.Flags |= FlagUseResource;
			Star.Param2 = 600.0;
			Star.Stat = 0;
			Star.CueId = CueMeteor;
			Falling.Steps.push_back(Star);
			Falling.Steps.push_back(SpendAll(1.01, 0));
			Falling.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Constellation = SlotDef(K, ESlotId::Move2);
			Constellation = B::Def("Stargazer.Move2", "Constellation", ESlotId::Move2, 14.0, 0.5, 0.5,
				"Three stars orbit her for 10 s and each shoots the nearest enemy (380 a second, +50% on a seeded enemy).");
			FStep Stars = B::ZoneFollow(0.5, 1200.0, 10.0, 1.0, 380.0).WithBonusVs(EStatus::Mark, 0.5);
			Stars.Flags |= FlagTurret;
			Stars.Hits = 3;
			Stars.CueId = CueStar;
			Constellation.Steps.push_back(Stars);
			Constellation.Tags = {"BIB.Type.Summoning", "BIB.Type.Offensive"};

			FAbilityDef& Seed = SlotDef(K, ESlotId::Move3);
			Seed = B::Def("Stargazer.Move3", "Supernova Seed", ESlotId::Move3, 20.0, 0.6, 0.6, "Plants a seed in the nearest enemy; after 4 s it explodes (700): 3000 + 25% of the damage the seed absorbed.");
			FStep Plant = B::HookStep(0.4, EHook::StarSeed, 1800.0);
			Plant.Flags |= FlagNearestEnemy;
			Plant.Duration = 4.0;
			Plant.Radius = 700.0;
			Plant.Damage = 3000.0;
			Plant.CueId = CueStar;
			Seed.Steps.push_back(Plant);
			Seed.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Gaze = SlotDef(K, ESlotId::Special);
			Gaze = B::Def("Stargazer.Special", "Zenith Gaze", ESlotId::Special, 35.0, 3.2, 3.4, "A 2.4 s beam (2500 long): 12 ticks of 600 - and she cannot move.");
			FStep Ray = B::Beam(0.8, 2500.0, 180.0, 600.0);
			Ray.Duration = 2.4;
			Ray.Interval = 0.2;
			Ray.CueId = CueBeam;
			Gaze.Steps.push_back(Ray);
			Gaze.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Collapse = SlotDef(K, ESlotId::Ultimate);
			Collapse = B::Def("Stargazer.Ultimate", "Cosmic Collapse", ESlotId::Ultimate, 0.0, 5.0, 5.6, "12 meteors (1800) on random enemies over 4.4 s, then a 9000 collapse.");
			FStep Rain = B::Burst(0.5, 500.0, 1800.0);
			Rain.Flags |= FlagRandomEnemy;
			Rain.Range = 2500.0;
			Rain.Duration = 4.4;
			Rain.Interval = 0.4;
			Rain.CueId = CueMeteor;
			Collapse.Steps.push_back(Rain);
			Collapse.Steps.push_back(B::Burst(5.0, 2500.0, 9000.0).WithCue(CueUltimate));
			Collapse.Tags = {"BIB.Type.Offensive"};

			FPassiveDef Light = B::Passive("Stargazer.Starlit", "Starlit", ETrigger::Periodic, "PROPOSED: gains one starlight every 5 s.");
			Light.Interval = 5.0;
			Light.Steps.push_back(B::ResStep(0.0, 0, 1.0));
			K.Passives.push_back(Light);
			FPassiveDef Cannon = B::Passive("Stargazer.GlassCannon", "Glass Cannon", ETrigger::Always, "PROPOSED: +30% damage dealt, +25% damage taken.");
			Cannon.Mods[static_cast<int>(EBuff::DamageDealt)] = 0.30;
			Cannon.Mods[static_cast<int>(EBuff::DamageTaken)] = 0.25;
			K.Passives.push_back(Cannon);
			FPassiveDef Wish = B::Passive("Stargazer.WishUponAStar", "Wish Upon a Star", ETrigger::OnKill, "PROPOSED: a kill gives one starlight and heals 3%.");
			Wish.Steps.push_back(B::ResStep(0.0, 0, 1.0));
			Wish.Steps.push_back(B::HealStep(0.0, 0.03));
			K.Passives.push_back(Wish);
			return K;
		}

		// ====================================================================================== Timekeeper (6*)
		// Signature loop (2026-10-07): set up a recording (Stasis Bubble, Temporal Echo), stuff it with damage (Fast Forward,
		// Second Hand echoes, Rewind Step's +50%) and let it replay. Hits on slowed (time-stopped) enemies deal +30%.
		FKitDef MakeTimekeeper()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Timekeeper);
			K.Name = "Timekeeper";
			K.Stars = 6;
			K.BaseHP = 2600.0;
			K.BaseAttack = 280.0;
			K.WalkMult = 1.2;
			K.RunMult = 1.5;
			K.DodgeCooldown = 0.5;
			K.Colour[0] = 0.90f; K.Colour[1] = 0.85f; K.Colour[2] = 0.40f;
			K.Playstyle = "Fast time mage: records damage in time warps and replays it multiplied; hits slowed enemies harder; risky.";

			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Timekeeper.Basic", "Second Hand", 0.35, 3, "Three quick strikes; each leaves an echo 1 s later at half strength (+30% on slowed enemies).");
			for (int Hit = 0; Hit < 3; ++Hit)
			{
				Basic.Steps.push_back(B::Melee(Hit == 2 ? 0.16 : 0.08, 260.0, 0.2, 0.0, 0.7).WithCombo(Hit).WithBonusVs(EStatus::Slow, 0.3).WithCue(CueTime)); // 2026-10-13 hit feel: the string's last hit winds up longer (rhythm), was 0.08 s
				Basic.Steps.push_back(B::Melee(1.0, 260.0, 0.2, 0.0, 0.35).WithCombo(Hit).WithBonusVs(EStatus::Slow, 0.3).WithCue(CueTime));
			}

			FAbilityDef& Slip = SlotDef(K, ESlotId::Move1);
			Slip = B::Def("Timekeeper.Move1", "Rewind Step", ESlotId::Move1, 5.0, 0.2, 0.2, "Slips 700 back in time (0.5 s untouchable); damage dealt is +50% for 3 s.");
			Slip.Steps.push_back(B::Blink(0.0, -700.0, FlagInvulnerable, 0.5).WithCue(CueTime));
			Slip.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.5, 3.0));
			Slip.Tags = {"BIB.Type.Mobility"};

			FAbilityDef& Bubble = SlotDef(K, ESlotId::Move2);
			Bubble = B::Def("Timekeeper.Move2", "Stasis Bubble", ESlotId::Move2, 12.0, 0.4, 4.6,
				"A 700 bubble for 4 s slows enemies by 90% (400 a second); all damage dealt meanwhile is replayed once more.");
			Bubble.Steps.push_back(B::HookStep(0.4, EHook::TimeRecord));
			FStep Field = B::ZoneAt(0.4, 700.0, 700.0, 4.0, 0.25, 100.0);
			Field.Status = EStatus::Slow;
			Field.StatusSeconds = 0.5;
			Field.StatusPower = 0.9;
			Field.CueId = CueTime;
			Bubble.Steps.push_back(Field);
			FStep Replay = B::HookStep(4.4, EHook::TimeRelease);
			Replay.Param = 1.0;
			Bubble.Steps.push_back(Replay);
			Bubble.Tags = {"BIB.Type.Control"};

			FAbilityDef& Forward = SlotDef(K, ESlotId::Move3);
			Forward = B::Def("Timekeeper.Move3", "Fast Forward", ESlotId::Move3, 16.0, 0.0, 4.0, "4 s: double attack speed and cooldowns recover twice as fast - at +50% damage taken.");
			Forward.Steps.push_back(B::BuffStep(0.0, EBuff::AttackSpeed, 1.0, 4.0));
			Forward.Steps.push_back(B::BuffStep(0.0, EBuff::CooldownFlow, 1.0, 4.0));
			Forward.Steps.push_back(B::BuffStep(0.0, EBuff::DamageTaken, 0.5, 4.0));
			Forward.Tags = {"BIB.Type.Buff"};

			FAbilityDef& Echo = SlotDef(K, ESlotId::Special);
			Echo = B::Def("Timekeeper.Special", "Temporal Echo", ESlotId::Special, 26.0, 0.0, 6.0, "Records the next 6 s of damage and replays half of it.");
			Echo.Steps.push_back(B::HookStep(0.0, EHook::TimeRecord).WithCue(CueTime));
			FStep Again = B::HookStep(6.0, EHook::TimeRelease);
			Again.Param = 0.5;
			Echo.Steps.push_back(Again);
			Echo.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Zero = SlotDef(K, ESlotId::Ultimate);
			Zero = B::Def("Timekeeper.Ultimate", "Zero Hour", ESlotId::Ultimate, 0.0, 0.8, 6.8, "6 s of stopped time (bosses only slow): everything frozen takes +20%, then all damage is released x2 (at least 6000 each).");
			Zero.Steps.push_back(B::Burst(0.3, 100.0, 0.0).WithFlags(FlagAllEnemies).WithStatus(EStatus::Freeze, 6.0).WithCue(CueTime));
			FStep Stop = B::HookStep(0.3, EHook::TimeRecord);
			Stop.Duration = 6.0;
			Zero.Steps.push_back(Stop);
			FStep Release = B::HookStep(6.3, EHook::TimeRelease);
			Release.Param = 2.0;
			Release.Param2 = 6000.0;
			Zero.Steps.push_back(Release);
			Zero.Tags = {"BIB.Type.Offensive", "BIB.Type.Control"};

			FPassiveDef Borrowed = B::Passive("Timekeeper.BorrowedTime", "Borrowed Time", ETrigger::OnDodge, "PROPOSED: dodging a hit gives +30% damage for 4 s.");
			Borrowed.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.3, 4.0));
			K.Passives.push_back(Borrowed);
			FPassiveDef Chronic = B::Passive("Timekeeper.Chronic", "Chronic", ETrigger::OnAbilityCast, "PROPOSED: every ability cast makes cooldowns recover twice as fast for 1 s.");
			Chronic.Steps.push_back(B::BuffStep(0.0, EBuff::CooldownFlow, 1.0, 1.0));
			K.Passives.push_back(Chronic);
			FPassiveDef Paradox = B::Passive("Timekeeper.Paradox", "Paradox", ETrigger::OnLowHealth,
				"PROPOSED: falling below 30% health rewinds her: heals 25% and 1.5 s untouchable (once every 90 s).");
			Paradox.Threshold = 0.30;
			Paradox.Interval = 90.0;
			Paradox.Steps.push_back(B::HealStep(0.0, 0.25));
			Paradox.Steps.push_back(B::BuffStep(0.0, EBuff::Evade, 3.0, 1.5));
			K.Passives.push_back(Paradox);
			return K;
		}

		// ====================================================================================== Weather (5*)
		// Signature loop (2026-10-07): the front. Every cast moves the front on (rain, snow, wind, storm, heat) and her bolts
		// follow it; Downpour and Perfect Storm soak, then Thunderstorm and the storm bolts pay off on soaked enemies.
		FKitDef MakeWeather()
		{
			FKitDef K;
			K.KitId = static_cast<int>(EKit::Weather);
			K.Name = "Weather";
			K.Stars = 5;
			K.BaseHP = 3000.0;
			K.BaseAttack = 330.0;
			K.Colour[0] = 0.40f; K.Colour[1] = 0.70f; K.Colour[2] = 1.00f;
			K.Playstyle = "Weather mage: rain, snow, wind, storm and heat; every cast moves the front on and her bolts follow it.";

			// Fronts rotate with every non-basic cast: Rain -> Snow -> Wind -> Storm -> Heat.
			FAbilityDef& Basic = SlotDef(K, ESlotId::Basic);
			Basic = B::Basic("Weather.Basic", "Gust Bolt", 0.5, 1, "A bolt of the current front: soaks, chills, shoves, shocks (+50% on soaked) or burns.");
			Basic.BranchMode = EBranchMode::ByFront;
			FBranch Rain = Branch("rain");
			Rain.Steps.push_back(B::Proj(0.1, 1, 2000.0, 1500.0, 60.0, 0.0, 0.8).WithStatus(EStatus::Wet, 6.0).WithCue(CueSplash));
			FBranch Snow = Branch("snow");
			Snow.Steps.push_back(B::Proj(0.1, 1, 1800.0, 1500.0, 60.0, 0.0, 0.8).WithStatus(EStatus::Slow, 2.0, 0.3).WithCue(CueFrost));
			FBranch Wind = Branch("wind");
			Wind.Steps.push_back(B::Proj(0.1, 1, 2400.0, 1500.0, 60.0, 0.0, 0.8).WithKnock(400.0).WithCue(CueWind));
			FBranch Storm = Branch("storm");
			Storm.Steps.push_back(B::Proj(0.1, 1, 2600.0, 1500.0, 60.0, 0.0, 0.8).WithStatus(EStatus::Shock, 6.0, 0.0, 1).WithWet(0.5).WithCue(CueLightning));
			FBranch Heat = Branch("heat");
			Heat.Steps.push_back(B::Proj(0.1, 1, 2000.0, 1500.0, 60.0, 0.0, 0.8).WithStatus(EStatus::Burn, 4.0, 60.0, 1).WithCue(CueFlame));
			Basic.Branches = {Rain, Snow, Wind, Storm, Heat};

			FAbilityDef& Downpour = SlotDef(K, ESlotId::Move1);
			Downpour = B::Def("Weather.Move1", "Downpour", ESlotId::Move1, 12.0, 0.6, 0.6, "A rain cloud (900, 8 s): 65 per half second, soaks and slows 20%.");
			Downpour.Steps.push_back(B::ZoneAt(0.5, 800.0, 900.0, 8.0, 0.5, 65.0).WithStatus(EStatus::Wet, 4.0).WithCue(CueSplash));
			Downpour.Steps.push_back(B::ZoneAt(0.5, 800.0, 900.0, 8.0, 0.5, 0.0).WithStatus(EStatus::Slow, 1.0, 0.2).WithCue(CueNone));
			Downpour.Tags = {"BIB.Type.Environmental", "BIB.Type.Control"};

			FAbilityDef& Blizzard = SlotDef(K, ESlotId::Move2);
			Blizzard = B::Def("Weather.Move2", "Blizzard", ESlotId::Move2, 18.0, 0.6, 0.6, "A blizzard (800, 6 s): 120 a second, slows 50%, freezes every 3 s.");
			Blizzard.Steps.push_back(B::ZoneAt(0.5, 800.0, 800.0, 6.0, 1.0, 120.0).WithStatus(EStatus::Slow, 2.0, 0.5).WithCue(CueFrost));
			FStep Freeze = B::ZoneAt(0.5, 800.0, 800.0, 6.0, 3.0, 0.0);
			Freeze.Status = EStatus::Freeze;
			Freeze.StatusSeconds = 2.0;
			Freeze.CueId = CueNone;
			Blizzard.Steps.push_back(Freeze);
			Blizzard.Tags = {"BIB.Type.Environmental", "BIB.Type.Control"};

			FAbilityDef& Thunder = SlotDef(K, ESlotId::Move3);
			Thunder = B::Def("Weather.Move3", "Thunderstorm", ESlotId::Move3, 22.0, 0.8, 3.6, "Six lightning strikes on random enemies over 3 s (480, +60% on soaked) that shock.");
			FStep Bolt = B::Burst(0.5, 220.0, 480.0);
			Bolt.Flags |= FlagRandomEnemy;
			Bolt.Range = 2200.0;
			Bolt.Duration = 2.5;
			Bolt.Interval = 0.5;
			Bolt.Status = EStatus::Shock;
			Bolt.StatusSeconds = 6.0;
			Bolt.WetBonus = 0.6;
			Bolt.CueId = CueLightning;
			Thunder.Steps.push_back(Bolt);
			Thunder.Tags = {"BIB.Type.Offensive"};

			FAbilityDef& Cyclone = SlotDef(K, ESlotId::Special);
			Cyclone = B::Def("Weather.Special", "Cyclone", ESlotId::Special, 35.0, 0.8, 5.8, "A tornado around her for 5 s pulls enemies in and tears at them (260 a second).");
			Cyclone.Steps.push_back(B::ZoneFollow(0.6, 500.0, 5.0, 0.5, 130.0).WithKnock(-450.0).WithCue(CueWind));
			Cyclone.Tags = {"BIB.Type.Environmental", "BIB.Type.Control"};

			FAbilityDef& Perfect = SlotDef(K, ESlotId::Ultimate);
			Perfect = B::Def("Weather.Ultimate", "Perfect Storm", ESlotId::Ultimate, 0.0, 1.0, 10.5, "10 s over 2500: rain and wind (340 a second), lightning every 0.8 s (650, +50% on soaked) and a pulling gale.");
			Perfect.Steps.push_back(B::ZoneFollow(1.0, 2500.0, 10.0, 1.0, 340.0).WithStatus(EStatus::Wet, 3.0).WithCue(CueUltimate));
			FStep Gale = B::ZoneFollow(1.0, 2500.0, 10.0, 0.5, 0.0);
			Gale.Knockback = -250.0;
			Gale.CueId = CueWind;
			Perfect.Steps.push_back(Gale);
			FStep Lightning = B::Burst(1.0, 300.0, 650.0);
			Lightning.Flags |= FlagRandomEnemy;
			Lightning.Range = 2500.0;
			Lightning.Duration = 9.0;
			Lightning.Interval = 0.8;
			Lightning.Status = EStatus::Shock;
			Lightning.StatusSeconds = 6.0;
			Lightning.WetBonus = 0.5;
			Lightning.CueId = CueLightning;
			Perfect.Steps.push_back(Lightning);
			Perfect.Tags = {"BIB.Type.Environmental", "BIB.Type.Offensive"};

			FPassiveDef Pressure = B::Passive("Weather.PressureSystem", "Pressure System", ETrigger::Always, "PROPOSED: every cast moves the front on (rain, snow, wind, storm, heat) and her bolts follow the front.");
			K.Passives.push_back(Pressure);
			FPassiveDef Baro = B::Passive("Weather.Barometer", "Barometer", ETrigger::Periodic, "PROPOSED: +10% damage while any enemy is soaked.");
			Baro.Interval = 0.5;
			Baro.CondEnemyStatus = EStatus::Wet;
			Baro.Steps.push_back(B::BuffStep(0.0, EBuff::DamageDealt, 0.10, 0.6));
			K.Passives.push_back(Baro);
			FPassiveDef Fair = B::Passive("Weather.FairWeather", "Fair Weather", ETrigger::Always, "PROPOSED: regenerates 0.5% max health per second.");
			Fair.Mods[static_cast<int>(EBuff::Regen)] = 0.005;
			K.Passives.push_back(Fair);
			return K;
		}
	}
}
