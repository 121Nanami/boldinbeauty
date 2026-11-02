// Boldinbeauty demo — kit registry, cue names, enemy abilities and the gameplay tag list.
#include "BIBKitData.h"
#include "BIBKitStatus.h"

#include <algorithm>
#include <set>

namespace BIB
{
	namespace Kits
	{
		const char* CueName(int CueId)
		{
			static const char* const Names[CueCount] = {
				"GameplayCue.BIB.None", "GameplayCue.BIB.Slash", "GameplayCue.BIB.Slam", "GameplayCue.BIB.Burst",
				"GameplayCue.BIB.Nova", "GameplayCue.BIB.Ring", "GameplayCue.BIB.Beam", "GameplayCue.BIB.Dash",
				"GameplayCue.BIB.Blink", "GameplayCue.BIB.Heal", "GameplayCue.BIB.Shield", "GameplayCue.BIB.Buff",
				"GameplayCue.BIB.Debuff", "GameplayCue.BIB.Mark", "GameplayCue.BIB.Execute", "GameplayCue.BIB.Zone",
				"GameplayCue.BIB.Meteor", "GameplayCue.BIB.Lightning", "GameplayCue.BIB.Explosion", "GameplayCue.BIB.Flame",
				"GameplayCue.BIB.Frost", "GameplayCue.BIB.Blood", "GameplayCue.BIB.Poison", "GameplayCue.BIB.Nature",
				"GameplayCue.BIB.Time", "GameplayCue.BIB.Star", "GameplayCue.BIB.Ultimate", "GameplayCue.BIB.Glitch",
				"GameplayCue.BIB.Splash", "GameplayCue.BIB.Wind"};
			return Names[(CueId >= 0 && CueId < CueCount) ? CueId : 0];
		}

		int CueTotal()
		{
			return static_cast<int>(CueCount);
		}

		const std::vector<FEnemyAbility>& EnemyAbilities()
		{
			// PLACEHOLDER enemy attacks (a melee hit, the boss slam and, from chapter 2, the ranged, mage and assassin attacks of
			// BIBCampaign.h). Adapt copies these. Keep the numbers in step with the archetype table: campaign_tests checks the ids.
			static const std::vector<FEnemyAbility> Abilities = {
				{1, "Claw strike", 40.0, 1.6, 0.0},
				{2, "Heavy smash", 80.0, 1.6, 0.0},
				{3, "Mini-boss cleave", 120.0, 1.6, 0.0},
				{4, "Boss slam", 300.0, 7.0, 700.0},
				{5, "Ranged shot", 32.0, 2.0, 0.0},
				{6, "Arcane blast", 95.0, 4.0, 380.0},
				{7, "Assassin strike", 75.0, 1.0, 0.0},
			};
			return Abilities;
		}

		const std::vector<const FKitDef*>& AllKits()
		{
			static std::vector<FKitDef> Storage;
			static std::vector<const FKitDef*> Pointers;
			if (Storage.empty())
			{
				Storage.reserve(17);
				Storage.push_back(MakeAlchemist());
				Storage.push_back(MakeArita());
				Storage.push_back(MakeAdapt());
				Storage.push_back(MakeBlood());
				Storage.push_back(MakeDeath());
				Storage.push_back(MakeFujita());
				Storage.push_back(MakeGambler());
				Storage.push_back(MakeIshii());
				Storage.push_back(MakeJester());
				Storage.push_back(MakeKuu());
				Storage.push_back(MakeMedia());
				Storage.push_back(MakeMyrcia());
				Storage.push_back(MakePlack());
				Storage.push_back(MakeShiraishi());
				Storage.push_back(MakeStargazer());
				Storage.push_back(MakeTimekeeper());
				Storage.push_back(MakeWeather());
				for (const FKitDef& Kit : Storage)
				{
					Pointers.push_back(&Kit);
				}
			}
			return Pointers;
		}

		const FKitDef* FindKit(int KitId)
		{
			for (const FKitDef* Kit : AllKits())
			{
				if (Kit->KitId == KitId)
				{
					return Kit;
				}
			}
			return nullptr;
		}

		const FKitDef* FindKitByName(const char* Name)
		{
			for (const FKitDef* Kit : AllKits())
			{
				if (Kit->Name == Name)
				{
					return Kit;
				}
			}
			return nullptr;
		}

		const std::vector<std::string>& BespokeKitTags()
		{
			static const std::vector<std::string> Tags = {
				// Luna
				"BIB.Ability.Luna.Basic", "BIB.Ability.Luna.Move1", "BIB.Ability.Luna.Move2", "BIB.Ability.Luna.Move3",
				"BIB.Ability.Luna.Special", "BIB.Ability.Luna.Ultimate",
				"BIB.Cooldown.Luna.Basic", "BIB.Cooldown.Luna.Move1", "BIB.Cooldown.Luna.Move2", "BIB.Cooldown.Luna.Move3",
				"BIB.Cooldown.Luna.Special", "BIB.Cooldown.Luna.Ultimate",
				"BIB.State.Luna.Blackhole", "BIB.State.Luna.OrbitingBlack", "BIB.State.Luna.OrbitingWhite",
				"BIB.State.Luna.EventHorizon", "BIB.State.Luna.Expel", "BIB.State.Luna.Destabilisation",
				"BIB.State.Luna.GravityInverted",
				// Judge
				"BIB.Ability.Judge.Basic", "BIB.Ability.Judge.Move1", "BIB.Ability.Judge.Move2", "BIB.Ability.Judge.Move3",
				"BIB.Ability.Judge.Special", "BIB.Ability.Judge.Ultimate",
				"BIB.Cooldown.Judge.Basic", "BIB.Cooldown.Judge.Move1", "BIB.Cooldown.Judge.Move2", "BIB.Cooldown.Judge.Move3",
				"BIB.Cooldown.Judge.Special", "BIB.Cooldown.Judge.Ultimate", "BIB.Cooldown.Judge.CourtLock",
				"BIB.State.Judge.Witness", "BIB.State.Judge.Lawyer", "BIB.State.Judge.Judge", "BIB.State.Judge.CourtSetting",
				"BIB.State.Judge.Contempt", "BIB.State.Judge.Justice", "BIB.State.Judge.RegAttackGranted",
				"BIB.Status.Judge.Innocent", "BIB.Status.Judge.Suspect", "BIB.Status.Judge.Guilty", "BIB.Status.Judge.Gavel",
				// Narrator / Diano
				"BIB.Ability.Narrator.Basic", "BIB.Ability.Narrator.Move1", "BIB.Ability.Narrator.Move2", "BIB.Ability.Narrator.Move3",
				"BIB.Ability.Narrator.Special", "BIB.Ability.Narrator.Ultimate",
				"BIB.Ability.Diano.Move1", "BIB.Ability.Diano.Move2", "BIB.Ability.Diano.Move3",
				"BIB.Cooldown.Narrator.Basic", "BIB.Cooldown.Narrator.Move1", "BIB.Cooldown.Narrator.Move2", "BIB.Cooldown.Narrator.Move3",
				"BIB.Cooldown.Narrator.Special", "BIB.Cooldown.Narrator.Ultimate",
				"BIB.State.Narrator.Diano", "BIB.State.Narrator.ExchangeOpen", "BIB.State.Narrator.StillAlive",
				"BIB.State.Narrator.CloseEnough", "BIB.State.Narrator.Switching", "BIB.State.Narrator.EndlessGod",
				"BIB.State.Narrator.SwitchBlock",
				// common
				"BIB.Ability.Common.Basic", "BIB.Ability.Common.Dodge", "BIB.Ability.Common.Parry", "BIB.Ability.Common.Block",
				"BIB.Cooldown.Common.Dodge", "BIB.Cooldown.Common.Parry",
			};
			return Tags;
		}

		std::vector<std::string> AllGameplayTags()
		{
			std::set<std::string> Tags;
			// vocabulary shared by every operator
			static const char* const Core[] = {
				"BIB", "BIB.State.Alive", "BIB.State.Dead", "BIB.State.Active", "BIB.State.Blocking", "BIB.State.Parrying",
				"BIB.State.Dodging", "BIB.State.Invulnerable", "BIB.State.Casting", "BIB.State.MovementLocked",
				"BIB.State.Stunned", "BIB.State.Shielded", "BIB.State.Switching",
				"BIB.State.Kit.Curse", "BIB.State.Kit.Takeover", "BIB.State.Kit.Recording", "BIB.State.Kit.ZeroHour",
				"BIB.State.Kit.Puddle", "BIB.State.Kit.Infecting", "BIB.State.Kit.Seeded",
				"BIB.Event.Hit", "BIB.Event.BasicHit", "BIB.Event.Kill", "BIB.Event.Damaged", "BIB.Event.Dodge",
				"BIB.Event.Parry", "BIB.Event.Cast", "BIB.Event.StatusApplied", "BIB.Event.SwitchIn", "BIB.Event.LowHealth",
				"BIB.Data.Damage", "BIB.Data.Duration", "BIB.Data.Cooldown", "BIB.Data.Heal", "BIB.Data.Magnitude",
				"BIB.Team.Player", "BIB.Team.Enemy",
				"BIB.Enemy.Grunt", "BIB.Enemy.Brute", "BIB.Enemy.MiniBoss", "BIB.Enemy.Boss", "BIB.Enemy.Ranged", "BIB.Enemy.Mage", "BIB.Enemy.Assassin",
				"BIB.Damage.Basic", "BIB.Damage.Ability", "BIB.Damage.Dot", "BIB.Damage.True", "BIB.Damage.Execute", "BIB.Damage.Reflect",
				"BIB.Rarity.3Star", "BIB.Rarity.4Star", "BIB.Rarity.5Star", "BIB.Rarity.6Star",
				"BIB.Slot.Basic", "BIB.Slot.Move1", "BIB.Slot.Move2", "BIB.Slot.Move3", "BIB.Slot.Special", "BIB.Slot.Ultimate",
				"BIB.Slot.Dodge", "BIB.Slot.Parry",
				"BIB.Type.Offensive", "BIB.Type.Control", "BIB.Type.Defensive", "BIB.Type.Mobility", "BIB.Type.Buff",
				"BIB.Type.Healing", "BIB.Type.Summoning", "BIB.Type.Utility", "BIB.Type.Illusion", "BIB.Type.Transformation",
				"BIB.Type.Resource",
			};
			for (const char* Tag : Core)
			{
				Tags.insert(Tag);
			}
			for (int Index = 1; Index < StatusCount; ++Index)
			{
				Tags.insert(StatusRule(static_cast<EStatus>(Index)).Tag);
			}
			for (int Index = 0; Index < BuffCount; ++Index)
			{
				Tags.insert(BuffTag(static_cast<EBuff>(Index)));
			}
			for (int Index = 0; Index < CueTotal(); ++Index)
			{
				Tags.insert(CueName(Index));
			}
			for (const std::string& Tag : BespokeKitTags())
			{
				Tags.insert(Tag);
			}
			// Dodge and Parry are common actions with a cooldown per operator: BIB.Cooldown.<Operator>.Dodge / .Parry
			for (const char* Bespoke : {"Luna", "Judge", "Narrator"})
			{
				Tags.insert(std::string("BIB.Kit.") + Bespoke);
				Tags.insert(std::string("BIB.Cooldown.") + Bespoke + ".Dodge");
				Tags.insert(std::string("BIB.Cooldown.") + Bespoke + ".Parry");
				Tags.insert(std::string("BIB.Ability.") + Bespoke + ".Dodge");
				Tags.insert(std::string("BIB.Ability.") + Bespoke + ".Parry");
			}
			for (const FKitDef* Kit : AllKits())
			{
				Tags.insert("BIB.Kit." + Kit->Name);
				Tags.insert("BIB.Cooldown." + Kit->Name + ".Dodge");
				Tags.insert("BIB.Cooldown." + Kit->Name + ".Parry");
				Tags.insert("BIB.Ability." + Kit->Name + ".Dodge");
				Tags.insert("BIB.Ability." + Kit->Name + ".Parry");
				auto AddAbility = [&Tags](const FAbilityDef& Def)
				{
					if (Def.Id.empty())
					{
						return;
					}
					Tags.insert("BIB.Ability." + Def.Id);
					Tags.insert("BIB.Cooldown." + Def.Id);
					for (const std::string& Extra : Def.Tags)
					{
						Tags.insert(Extra);
					}
				};
				for (const FAbilityDef& Def : Kit->Abilities)
				{
					AddAbility(Def);
				}
				for (const FAbilityDef& Def : Kit->UltimateVariants)
				{
					AddAbility(Def);
				}
				for (const FPassiveDef& Passive : Kit->Passives)
				{
					Tags.insert("BIB.Passive." + Passive.Id);
				}
			}
			return std::vector<std::string>(Tags.begin(), Tags.end());
		}
	}
}
