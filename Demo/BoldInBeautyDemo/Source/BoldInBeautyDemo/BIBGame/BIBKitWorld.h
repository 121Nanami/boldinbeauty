// Adapter between the engine-independent kit runtime (BIBRules/BIBKitRuntime.h) and the Unreal world, one per team
// member. Everything the runtime does to the game goes through here and lands in the Gameplay Ability System:
//   damage -> damage GameplayEffect on the enemy   healing -> healing GameplayEffect on the operator
//   cooldowns -> GameplayEffects granting BIB.Cooldown.<Operator>.<Slot>   invulnerability -> timed BIB.State.Invulnerable
//   statuses -> mirrored as BIB.Status.* tags on the enemy   cues -> GameplayCue.BIB.* (plus placeholder shapes)
#pragma once

#include "CoreMinimal.h"
#include "BIBRules/BIBHitFeel.h"
#include "BIBRules/BIBKitRuntime.h"

class ABIBEnemy;
class ABIBPlayerCharacter;

class FBIBKitWorld : public BIB::Kits::IWorld
{
public:
	FBIBKitWorld(ABIBPlayerCharacter* InOwner, int32 InMemberIndex);

	virtual double Now() const override;
	virtual BIB::Kits::FVec CasterPos() const override;
	virtual BIB::Kits::FVec CasterAim() const override;
	virtual double CasterAttack() const override;
	virtual double CasterDamageMult() const override;
	virtual double CasterHP() const override;
	virtual double CasterMaxHP() const override;
	virtual void CasterMove(const BIB::Kits::FVec& To, double Seconds) override;
	virtual void CasterInvulnerable(double Seconds) override;
	virtual void CasterHeal(double Amount) override;
	virtual void CasterSelfDamage(double Amount) override;
	virtual void CasterLock(double Seconds) override;
	virtual void CasterHealthScale(double Mult, bool bFill) override;

	virtual void GetEnemies(std::vector<BIB::Kits::FEnemyView>& Out) const override;
	virtual bool EnemyAlive(int Id) const override;
	virtual double DamageEnemy(int Id, double Amount, unsigned Flags) override;
	virtual double DamageEnemyHit(int Id, double Amount, unsigned Flags, const BIB::Kits::FHitInfo& Info) override;
	virtual void KillEnemy(int Id, bool bRespectBoss, double BossFraction) override;
	virtual void KillEnemyHit(int Id, bool bRespectBoss, double BossFraction, const BIB::Kits::FHitInfo& Info) override;
	virtual void Impulse(int Id, const BIB::Kits::FVec& Velocity) override;
	virtual bool ApplyStatus(int Id, BIB::Kits::EStatus Status, double Seconds, double Power, int Stacks) override;
	virtual int StatusStacks(int Id, BIB::Kits::EStatus Status) const override;
	virtual void ClearStatus(int Id, BIB::Kits::EStatus Status) override;
	virtual void RemoveStatusStacks(int Id, BIB::Kits::EStatus Status, int Count) override;

	virtual double CooldownRemaining(BIB::Kits::ESlotId Slot) const override;
	virtual void StartCooldown(BIB::Kits::ESlotId Slot, double Seconds) override;
	virtual void ReduceCooldown(BIB::Kits::ESlotId Slot, double Seconds) override;

	virtual void AddUltimate(double DamageDealt) override;
	virtual void Cue(int CueId, const BIB::Kits::FVec& Pos, double Radius, double Seconds) override;
	virtual void Message(const std::string& Text) override;
	virtual double Random01() override;

private:
	ABIBPlayerCharacter* Owner;
	int32 MemberIndex;

	ABIBEnemy* FindEnemy(int Id) const;
	/** The hit-feel source of a kit hit (BIBRules/BIBHitFeel.h). */
	BIB::Feel::FHitSource FeelSource(const BIB::Kits::FHitInfo& Info, unsigned Flags) const;
};
