// Prints every data-driven operator kit as JSON (one document on stdout). Tools/gen_kit_docs.py turns it into
// docs/operator-kits.md; the same dump feeds the Notion write-back. Build & run via Tools/gen_kit_docs.py.
#include "../Source/BoldInBeautyDemo/BIBRules/BIBKitData.h"

#include <cstdio>
#include <string>

namespace
{
	std::string Escape(const std::string& Text)
	{
		std::string Out;
		for (const char C : Text)
		{
			switch (C)
			{
			case '"': Out += "\\\""; break;
			case '\\': Out += "\\\\"; break;
			case '\n': Out += "\\n"; break;
			default: Out += C; break;
			}
		}
		return Out;
	}

	const char* Trigger(BIB::Kits::ETrigger Value)
	{
		using BIB::Kits::ETrigger;
		switch (Value)
		{
		case ETrigger::Always: return "Always";
		case ETrigger::OnHit: return "OnHit";
		case ETrigger::OnBasicHit: return "OnBasicHit";
		case ETrigger::OnKill: return "OnKill";
		case ETrigger::OnDamaged: return "OnDamaged";
		case ETrigger::OnDodge: return "OnDodge";
		case ETrigger::OnParry: return "OnParry";
		case ETrigger::OnAbilityCast: return "OnAbilityCast";
		case ETrigger::OnLowHealth: return "OnLowHealth";
		case ETrigger::Periodic: return "Periodic";
		case ETrigger::OnStatusApplied: return "OnStatusApplied";
		case ETrigger::OnSwitchIn: return "OnSwitchIn";
		}
		return "?";
	}

	const char* Cast(BIB::Kits::ECast Value)
	{
		switch (Value)
		{
		case BIB::Kits::ECast::Instant: return "instant";
		case BIB::Kits::ECast::Hold: return "hold";
		case BIB::Kits::ECast::Toggle: return "toggle";
		}
		return "?";
	}

	void PrintAbility(const BIB::Kits::FAbilityDef& Def, bool bFirst)
	{
		std::printf("%s      {\"id\":\"%s\",\"name\":\"%s\",\"slot\":\"%s\",\"cast\":\"%s\",\"cooldown\":%.2f,\"lock\":%.2f,\"total\":%.2f,\"source\":%d,\"note\":\"%s\",\"tags\":[",
			bFirst ? "" : ",\n", Escape(Def.Id).c_str(), Escape(Def.Name).c_str(), BIB::Kits::SlotName(Def.Slot), Cast(Def.Cast),
			Def.Cooldown, Def.LockSeconds, Def.TotalSeconds, Def.Source, Escape(Def.Note).c_str());
		for (size_t Index = 0; Index < Def.Tags.size(); ++Index)
		{
			std::printf("%s\"%s\"", Index ? "," : "", Escape(Def.Tags[Index]).c_str());
		}
		std::printf("]}");
	}
}

int main()
{
	std::printf("{\"kits\":[\n");
	bool bFirstKit = true;
	for (const BIB::Kits::FKitDef* Kit : BIB::Kits::AllKits())
	{
		std::printf("%s  {\"name\":\"%s\",\"stars\":%d,\"hp\":%.0f,\"attack\":%.0f,\"playstyle\":\"%s\",\"resources\":[",
			bFirstKit ? "" : ",\n", Escape(Kit->Name).c_str(), Kit->Stars, Kit->BaseHP, Kit->BaseAttack, Escape(Kit->Playstyle).c_str());
		bFirstKit = false;
		bool bFirstResource = true;
		for (int Index = 0; Index < 2; ++Index)
		{
			if (!Kit->ResourceName[Index].empty())
			{
				std::printf("%s{\"name\":\"%s\",\"max\":%.0f,\"start\":%.0f,\"regen\":%.2f}", bFirstResource ? "" : ",",
					Escape(Kit->ResourceName[Index]).c_str(), Kit->ResourceMax[Index], Kit->ResourceStart[Index], Kit->ResourceRegen[Index]);
				bFirstResource = false;
			}
		}
		std::printf("],\"walk\":%.2f,\"run\":%.2f,\"jump\":%.2f,\"canSprint\":%s,\"dodgeCooldown\":%.2f,\"specialOnR\":%s,\"damageTaken\":%.2f,\"statusResist\":%.2f,\"shield\":%.0f,\"abilities\":[\n",
			Kit->WalkMult, Kit->RunMult, Kit->JumpMult, Kit->bCanSprint ? "true" : "false", Kit->DodgeCooldown,
			Kit->bSpecialOnR ? "true" : "false", Kit->Traits.DamageTakenMod, Kit->Traits.StatusResist, Kit->Traits.InitialShield);
		bool bFirstAbility = true;
		for (const BIB::Kits::FAbilityDef& Def : Kit->Abilities)
		{
			PrintAbility(Def, bFirstAbility);
			bFirstAbility = false;
		}
		for (const BIB::Kits::FAbilityDef& Def : Kit->UltimateVariants)
		{
			PrintAbility(Def, false);
		}
		std::printf("\n    ],\"passives\":[\n");
		bool bFirstPassive = true;
		for (const BIB::Kits::FPassiveDef& Passive : Kit->Passives)
		{
			std::printf("%s      {\"id\":\"%s\",\"name\":\"%s\",\"trigger\":\"%s\",\"source\":%d,\"note\":\"%s\"}", bFirstPassive ? "" : ",\n",
				Escape(Passive.Id).c_str(), Escape(Passive.Name).c_str(), Trigger(Passive.Trigger), Passive.Source, Escape(Passive.Note).c_str());
			bFirstPassive = false;
		}
		std::printf("\n    ]}");
	}
	std::printf("\n]}\n");
	return 0;
}
