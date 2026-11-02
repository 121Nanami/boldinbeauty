// Balance simulator for the 17 data-driven kits: a scripted rotation against stationary dummies, compared with the
// owner's three bespoke kits (Luna, Judge, Narrator) computed from BIBTuning.h. Run: Tests/run_core_tests.sh --balance
// The numbers are PROPOSED / PLACEHOLDER: this exists so the owner can see every kit on one scale and tune it.
#include "kit_fake_world.h"

#include "../Source/BoldInBeautyDemo/BIBRules/BIBConfig.h"
#include "../Source/BoldInBeautyDemo/BIBRules/BIBCore.h"
using int32 = int;   // BIBTuning.h is shared with the Unreal layer
#include "../Source/BoldInBeautyDemo/BIBGame/BIBTuning.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <cmath>
#include <vector>
#include <string>

namespace
{
	using namespace KitTest;

	struct Result
	{
		double SingleDps = 0.0;
		double PackDps = 0.0;
		double UltSingle = 0.0;
		double UltPack = 0.0;
		int UltCount = 0;
		double SelfCostPerSecond = 0.0;
		double Damage[SlotCount] = {};
	};

	// Casts whatever is ready (Special first, M1 last). A held ability (Arita's Spring time) is released after 2 s; a toggle
	// (Natures Curse) is switched on once and left on.
	void CastNext(FakeWorld& World, FRuntime& Runtime, double HoldStart[SlotCount])
	{
		for (int Slot = 0; Slot < SlotCount; ++Slot)
		{
			if (HoldStart[Slot] >= 0.0 && World.Time - HoldStart[Slot] >= 2.0)
			{
				Runtime.Release(static_cast<ESlotId>(Slot));
				HoldStart[Slot] = -1.0;
			}
		}
		if (Runtime.IsCasting())
		{
			return;
		}
		const ESlotId Order[5] = {ESlotId::Special, ESlotId::Move3, ESlotId::Move2, ESlotId::Move1, ESlotId::Basic};
		for (const ESlotId Slot : Order)
		{
			const FAbilityDef& Def = Runtime.Ability(Slot);
			if (Def.Cast == ECast::Toggle && Runtime.HasState("Curse"))
			{
				continue;
			}
			if (Runtime.Cast(Slot).bOk)
			{
				if (Def.Cast == ECast::Hold)
				{
					HoldStart[static_cast<int>(Slot)] = World.Time;
				}
				break;
			}
		}
	}

	// A player who keeps facing the nearest enemy, walks back into range after a dash and casts whatever is ready
	// (the Ultimate as soon as the bar is full).
	void Rotate(FakeWorld& World, FRuntime& Runtime, double Seconds, int* Ults)
	{
		const double Dt = 1.0 / 30.0;
		const int Steps = static_cast<int>(Seconds / Dt);
		double HoldStart[SlotCount] = {-1.0, -1.0, -1.0, -1.0, -1.0, -1.0};
		for (int Index = 0; Index < Steps; ++Index)
		{
			const FakeEnemy* Nearest = nullptr;
			double Best = 1e18;
			for (const FakeEnemy& Enemy : World.Enemies)
			{
				const double Distance = Dist2(World.Pos, Enemy.Pos);
				if (Enemy.bAlive && Distance < Best)
				{
					Best = Distance;
					Nearest = &Enemy;
				}
			}
			if (Nearest && !Runtime.IsCasting() && World.MoveLeft <= 0.0)
			{
				World.Aim = Norm2(Nearest->Pos - World.Pos);
				if (Best > 420.0)
				{
					World.Pos = Nearest->Pos - World.Aim * 230.0;
				}
			}
			if (World.Ult >= BIB::Config::UltDamageForFullBar && !Runtime.IsCasting() && Runtime.Cast(ESlotId::Ultimate).bOk)
			{
				World.Ult = 0.0;
				if (Ults)
				{
					++*Ults;
				}
			}
			CastNext(World, Runtime, HoldStart);
			World.Step(Dt, Runtime);
		}
	}

	// Stationary dummies that respawn: a boss (BIBTuning::BossHP) in front, and in the pack ten grunts plus two brutes around.
	void Populate(FakeWorld& World, bool bPack)
	{
		const int BossId = World.AddEnemy(260.0, 0.0, BIBTuning::BossHP, true);
		World.Find(BossId)->bRespawn = true;
		if (bPack)
		{
			const double Positions[12][2] = {{400, 200}, {500, -200}, {700, 100}, {650, -350}, {900, 0}, {300, 450}, {250, -450},
				{-300, 200}, {-350, -150}, {1100, 300}, {800, 500}, {-500, -450}};
			int Index = 0;
			for (const auto& Position : Positions)
			{
				const bool bBrute = Index >= 10;
				const int Id = World.AddEnemy(Position[0], Position[1], bBrute ? BIBTuning::BruteHP : BIBTuning::GruntHP);
				World.Find(Id)->bRespawn = true;
				++Index;
			}
		}
	}

	Result Measure(const FKitDef& Def)
	{
		Result Out;
		const double Seconds = 90.0;
		for (int Pack = 0; Pack < 2; ++Pack)
		{
			FakeWorld World;
			World.BaseMaxHP = Def.BaseHP;
			World.HP = World.MaxHP = Def.BaseHP;
			World.Attack = Def.BaseAttack;
			Populate(World, Pack == 1);
			FRuntime Runtime(Def, World);
			int Ults = 0;
			const double HpStart = World.HP;
			// The no-ultimate rotation first (bar starts empty and fills from damage; measure the whole run including ultimates)
			Rotate(World, Runtime, Seconds, &Ults);
			(Pack == 0 ? Out.SingleDps : Out.PackDps) = World.TotalDamage / Seconds;
			if (Pack == 0)
			{
				Out.UltCount = Ults;
				Out.SelfCostPerSecond = (HpStart - World.HP + World.Healed) / Seconds;
			}
		}
		for (int Pack = 0; Pack < 2; ++Pack)
		{
			FakeWorld World;
			World.BaseMaxHP = Def.BaseHP;
			World.HP = World.MaxHP = Def.BaseHP;
			World.Attack = Def.BaseAttack;
			Populate(World, Pack == 1);
			FRuntime Runtime(Def, World);
			Runtime.Cast(ESlotId::Ultimate);
			World.Run(14.0, Runtime);
			(Pack == 0 ? Out.UltSingle : Out.UltPack) = World.TotalDamage;
		}
		return Out;
	}

	// Setup kits (2026-10-09, owner: Alchemist "start the infection, mutate, then instantly swap out"): Infection then Mutation on
	// a crowd bunched around the team (the way enemies close in, so infection can spread), then she is swapped out for the rest
	// of the 90 s while her runtime only ticks what she left on the enemies (FRuntime::TickBackground). Returns that damage and
	// the seconds she was on the field.
	bool IsSetupKit(const FKitDef& Def)
	{
		return Def.Name == "Alchemist";
	}

	void SetupDamage(const FKitDef& Def, double& OutDamage, double& OutFieldSeconds)
	{
		FakeWorld World;
		World.BaseMaxHP = Def.BaseHP;
		World.HP = World.MaxHP = Def.BaseHP;
		World.Attack = Def.BaseAttack;
		const int BossId = World.AddEnemy(200.0, 0.0, BIBTuning::BossHP, true);
		World.Find(BossId)->bRespawn = true;
		for (int Index = 0; Index < 12; ++Index)
		{
			// a crowd: rings around the boss, neighbours close enough to touch
			const double Angle = Index * 0.5236;
			const double Ring = Index < 6 ? 110.0 : 190.0;
			const int Id = World.AddEnemy(200.0 + Ring * std::cos(Angle), Ring * std::sin(Angle), Index >= 10 ? BIBTuning::BruteHP : BIBTuning::GruntHP);
			World.Find(Id)->bRespawn = true;
		}
		World.Aim = FVec{1.0, 0.0};
		FRuntime Runtime(Def, World);
		const double Dt = 1.0 / 60.0;
		double Field = 0.0;
		// She swaps back in whenever Infection is ready again, casts Infection (and Mutation when ready) and swaps out.
		while (World.Time < 90.0)
		{
			if (World.CooldownRemaining(ESlotId::Move1) <= 0.0)
			{
				World.bBenched = false;
				for (const ESlotId Slot : {ESlotId::Move1, ESlotId::Move2})
				{
					if (!Runtime.Cast(Slot).bOk)
					{
						continue;
					}
					while (Runtime.IsCasting() && World.Time < 90.0)
					{
						World.Step(Dt, Runtime);
						Field += Dt;
					}
				}
				Runtime.CancelAll();               // the game's switch-out (ABIBPlayerCharacter::DataKitSwitchedOut)
				Runtime.ClearBattlefield(true);
				World.bBenched = true;
			}
			World.Step(Dt, Runtime);
		}
		OutDamage = World.TotalDamage;
		OutFieldSeconds = Field;
	}

	// Survivability: the pack fight again, but the nearest enemy now hits the operator for IncomingHit every half second
	// (IncomingDps raw damage per second) while the rotation runs. Effective health = the raw damage the operator soaks
	// before falling (capped at SurvivalCap seconds): health, shields, mitigation, evasion, healing and self-costs included.
	constexpr double IncomingHit = 100.0;
	constexpr double IncomingInterval = 0.5;
	constexpr double SurvivalCap = 120.0;

	double Survival(const FKitDef& Def)
	{
		FakeWorld World;
		World.BaseMaxHP = Def.BaseHP;
		World.HP = World.MaxHP = Def.BaseHP;
		World.Attack = Def.BaseAttack;
		Populate(World, true);
		FRuntime Runtime(Def, World);
		const double Dt = 1.0 / 30.0;
		double NextHit = IncomingInterval;
		double Soaked = 0.0;
		double HoldStart[SlotCount] = {-1.0, -1.0, -1.0, -1.0, -1.0, -1.0};
		while (World.Time < SurvivalCap && World.HP > 0.0)
		{
			if (World.Ult >= BIB::Config::UltDamageForFullBar && !Runtime.IsCasting() && Runtime.Cast(ESlotId::Ultimate).bOk)
			{
				World.Ult = 0.0;
			}
			CastNext(World, Runtime, HoldStart);
			World.Step(Dt, Runtime);
			if (World.Time >= NextHit)
			{
				NextHit += IncomingInterval;
				int Attacker = -1;
				for (const FakeEnemy& Enemy : World.Enemies)
				{
					if (Enemy.bAlive)
					{
						Attacker = Enemy.Id;
						break;
					}
				}
				Soaked += IncomingHit;
				if (World.InvulnerableFor <= 0.0)
				{
					World.HP -= Runtime.OnIncomingDamage(IncomingHit, Attacker, true);
				}
			}
		}
		return Soaked;
	}

	struct FRow
	{
		const FKitDef* Def = nullptr;
		Result R;
		double Ehp = 0.0;
		double Offence = 0.0;
		double Power = 0.0;
	};
}

// Each ability on its own: one cast against the dummies, then the damage over the following seconds.
void PerAbility(const FKitDef& Def)
{
	std::printf("  %s per ability (one cast; single target | pack):\n", Def.Name.c_str());
	for (int Slot = 0; Slot < SlotCount; ++Slot)
	{
		const FAbilityDef& Ability = Def.Abilities[Slot];
		if (Ability.bAutomatic)
		{
			continue;
		}
		double Damage[2] = {};
		for (int Pack = 0; Pack < 2; ++Pack)
		{
			FakeWorld World;
			World.BaseMaxHP = Def.BaseHP;
			World.HP = World.MaxHP = Def.BaseHP;
			World.Attack = Def.BaseAttack;
			Populate(World, Pack == 1);
			FRuntime Runtime(Def, World);
			World.Aim = FVec{1.0, 0.0, 0.0};
			const int Casts = Slot == 0 ? 6 : 1;   // a short M1 chain
			for (int Index = 0; Index < Casts; ++Index)
			{
				Runtime.Cast(static_cast<ESlotId>(Slot));
				World.Run(Slot == 0 ? Ability.Cooldown + 0.05 : std::max(12.0, Ability.TotalSeconds + 2.0), Runtime);
			}
			Damage[Pack] = World.TotalDamage;
		}
		std::printf("    %-9s %-24s cd %5.1f | %9.0f | %9.0f %s\n", SlotName(static_cast<ESlotId>(Slot)), Ability.Name.c_str(), Ability.Cooldown,
			Damage[0], Damage[1], Slot == 0 ? "(6 M1 hits)" : "");
	}
}

int main(int ArgCount, char** Args)
{
	const bool bDetail = ArgCount > 1;
	// Owner reference kits (bespoke), analytic single-target sustained damage from BIBTuning.h.
	{
		using namespace BIBTuning;
		const double LunaBlackhole = (BlackholeHitDamage + 3.0 * BlackholeTickDamage + BlackholeExplosionDamage) / BlackholeCooldown;
		const double LunaOrbit = OrbitingDamage / 1.0;
		const double LunaHorizon = (EventHorizonDps * 12.0 + EventHorizonNukeDamage) / EventHorizonCooldown;
		const double LunaExpel = (ExpelBaseDamage + ExpelDamagePerSecond * 8.0) * 1.5 / ExpelCooldown;
		// The 4-hit basic string (2026-10-09; factor and timing since 2026-10-13: Config/DefaultGame.ini [BIB.CombatFeel] Combo<n>.*).
		const double LunaBasic = 783.0 * LunaComboFactor * 4.25 / 1.34;
		std::printf("Reference (owner kits, analytic, single target):\n");
		const double LunaSustained = LunaBlackhole + LunaOrbit + LunaHorizon + LunaExpel + LunaBasic;
		std::printf("  Luna      4*  HP 4275  ~%5.0f dps without the ultimate (basic %.0f, blackhole %.0f, orbit %.0f, horizon %.0f, expel %.0f); ultimate %.0f within %.0f m\n",
			LunaSustained, LunaBasic, LunaBlackhole, LunaOrbit, LunaHorizon, LunaExpel, DestabilisationDamage, DestabilisationRadius / 100.0);
		// The ultimate charges from damage dealt at LunaUltGainMult (its own damage never recharges it) and one runs at a time.
		const double LunaUltCycle = std::max(BIB::Config::UltDamageForFullBar / (LunaSustained * LunaUltGainMult), DestabilisationDuration + 1.0);
		std::printf("            with the ultimate every %.1f s: ~%5.0f dps\n", LunaUltCycle, LunaSustained + DestabilisationDamage / LunaUltCycle);
		std::printf("  Judge     5*  HP 1666  ~%5.0f dps basic only (200 / %.1f s), kills the guilty outright; ultimate Justice = instant kills\n", 200.0 / JudgePunchCooldown, JudgePunchCooldown);
		const double NarratorSpeedRate = NarratorSpeedGain;   // +25 speed per second
		std::printf("  Narrator  6*  HP  999  ~%5.0f dps (basic 272/%.1f s + one %.0f explosion per %.0f speed); ultimate = Diano, explosion %.0f\n",
			272.0 / NarratorPunchCooldown + ExplosionDamage * NarratorSpeedRate / ExplosionSpeedCost, NarratorPunchCooldown, ExplosionDamage, ExplosionSpeedCost, DianoExplodeDamage);
	}
	std::printf("\nSimulated (90 s, perfect rotation, ultimate cast whenever the bar is full, stationary dummies that respawn: one boss of %.0f HP\n"
		"in front for 'ST', plus ten grunts and two brutes around it for 'pack'; no incoming damage). Relative scale only.\n", BIBTuning::BossHP);
	std::printf("%-11s %2s %6s %5s | %8s %8s | %5s %9s %9s | %s\n", "kit", "*", "HP", "ATK", "ST dps", "pack dps", "ults", "ult ST", "ult pack", "role");
	std::vector<FRow> Rows;
	for (const FKitDef* Def : AllKits())
	{
		const Result R = Measure(*Def);
		std::printf("%-11s %2d %6.0f %5.0f | %8.0f %8.0f | %5d %9.0f %9.0f | %s\n", Def->Name.c_str(), Def->Stars, Def->BaseHP, Def->BaseAttack,
			R.SingleDps, R.PackDps, R.UltCount, R.UltSingle, R.UltPack, Def->Playstyle.substr(0, 60).c_str());
		if (bDetail && std::string(Args[1]) == Def->Name)
		{
			PerAbility(*Def);
		}
		if (!(R.SingleDps > 0.0) || !(R.PackDps > 0.0) || std::isnan(R.SingleDps))
		{
			std::printf("  FAIL: %s deals no damage in the simulation\n", Def->Name.c_str());
			return 1;
		}
		FRow Row;
		Row.Def = Def;
		Row.R = R;
		Row.Ehp = Survival(*Def);
		// Offence weighs bosses (single target) above crowds; a crowd counts a third because pack numbers run much higher.
		Row.Offence = 0.6 * R.SingleDps + 0.4 * R.PackDps / 3.0;
		if (IsSetupKit(*Def))
		{
			// her value is what she leaves behind: infection damage over the 90 s plus her own rotation for the time she is out
			double Setup = 0.0;
			double Field = 0.0;
			SetupDamage(*Def, Setup, Field);
			const double SetupOffence = Setup / 90.0 + Row.Offence * Field / 90.0;
			std::printf("  %s setup and swap: %.0f damage in 90 s with %.1f s on the field -> offence %.0f (rotation %.0f)\n",
				Def->Name.c_str(), Setup, Field, SetupOffence, Row.Offence);
			Row.Offence = std::max(Row.Offence, SetupOffence);
		}
		Rows.push_back(Row);
	}

	// Effective power (PROPOSED yardstick, 2026-10-07): Offence x (effective health / the median)^0.4, normalised so that the
	// median 5-star is 1.00. A defender may trade damage for survival. Gacha rarity bands it is tuned to: 3* 0.55-0.75,
	// 4* 0.75-0.90, 5* about 1.00 (0.90-1.10), 6* 1.10-1.30.
	auto Median = [](std::vector<double> Values) {
		std::sort(Values.begin(), Values.end());
		const size_t Mid = Values.size() / 2;
		return Values.size() % 2 ? Values[Mid] : 0.5 * (Values[Mid - 1] + Values[Mid]);
	};
	std::vector<double> AllEhp;
	for (const FRow& Row : Rows)
	{
		AllEhp.push_back(Row.Ehp);
	}
	const double EhpMedian = Median(AllEhp);
	std::vector<double> FiveStar;
	for (FRow& Row : Rows)
	{
		Row.Power = Row.Offence * std::pow(Row.Ehp / EhpMedian, 0.4);
		if (Row.Def->Stars == 5)
		{
			FiveStar.push_back(Row.Power);
		}
	}
	const double FiveMedian = Median(FiveStar);
	std::printf("\nEffective power (offence = 0.6 ST + 0.4 pack/3; survival = raw damage soaked at %.0f dps before falling, cap %.0f s;\n"
		"power = offence x (survival / median)^0.4, 5-star median = 1.00). Bands: 3* 0.55-0.75, 4* 0.75-0.90, 5* 0.90-1.10, 6* 1.10-1.30.\n",
		IncomingHit / IncomingInterval, SurvivalCap);
	std::printf("%-11s %2s | %8s %8s | %6s %s\n", "kit", "*", "offence", "soaked", "power", "band");
	int OutOfBand = 0;
	for (int Stars = 3; Stars <= 6; ++Stars)
	{
		for (const FRow& Row : Rows)
		{
			if (Row.Def->Stars != Stars)
			{
				continue;
			}
			const double P = Row.Power / FiveMedian;
			const double Lo[7] = {0, 0, 0, 0.55, 0.75, 0.90, 1.10};
			const double Hi[7] = {0, 0, 0, 0.75, 0.90, 1.10, 1.30};
			const bool bIn = P >= Lo[Stars] - 1e-9 && P <= Hi[Stars] + 1e-9;
			OutOfBand += bIn ? 0 : 1;
			std::printf("%-11s %2d | %8.0f %8.0f | %6.2f %s\n", Row.Def->Name.c_str(), Stars, Row.Offence, Row.Ehp, P, bIn ? "ok" : "OUT OF BAND");
		}
	}
	std::printf("%d kit(s) outside their rarity band.\n", OutOfBand);
	{
		// Luna on the same yardstick (analytic, 2026-10-13 rebalance): single target as above; the pack scenario assumes her basic
		// string and orbits touch 2 angels, the Blackhole explosion 6, Event horizon and Expel (all-range) all 13, and the
		// ultimate's 16 m blast 10. Owner numbers before the rebalance gave 1.69 (band 0.75-0.90).
		using namespace BIBTuning;
		const double Basic = 783.0 * LunaComboFactor * 4.25 / 1.34;
		const double St = Basic + (BlackholeHitDamage + 3.0 * BlackholeTickDamage + BlackholeExplosionDamage) / BlackholeCooldown + OrbitingDamage
			+ (EventHorizonDps * 12.0 + EventHorizonNukeDamage) / EventHorizonCooldown + (ExpelBaseDamage + ExpelDamagePerSecond * 8.0) * 1.5 / ExpelCooldown;
		const double Pack = Basic * 2.0 + (BlackholeHitDamage * 2.0 + 3.0 * BlackholeTickDamage * 2.0 + BlackholeExplosionDamage * 6.0) / BlackholeCooldown
			+ OrbitingDamage * 2.0 + (EventHorizonDps * 12.0 * 13.0 + EventHorizonNukeDamage * 3.0) / EventHorizonCooldown
			+ (ExpelBaseDamage + ExpelDamagePerSecond * 8.0) * 1.5 * 13.0 / ExpelCooldown;
		auto Cycle = [](double Dps) { return std::max(BIB::Config::UltDamageForFullBar / (Dps * LunaUltGainMult), DestabilisationDuration + 1.0); };
		const double StUlt = St + DestabilisationDamage / Cycle(St);
		const double PackUlt = Pack + DestabilisationDamage * 10.0 / Cycle(Pack);
		const double Offence = 0.6 * StUlt + 0.4 * PackUlt / 3.0;
		const double Power = Offence * std::pow(4275.0 / EhpMedian, 0.4) / FiveMedian;
		std::printf("Luna (analytic) 4 | %8.0f %8.0f | %6.2f %s\n", Offence, 4275.0, Power, Power >= 0.75 && Power <= 0.90 ? "ok" : "OUT OF BAND");
	}
	return 0;
}
