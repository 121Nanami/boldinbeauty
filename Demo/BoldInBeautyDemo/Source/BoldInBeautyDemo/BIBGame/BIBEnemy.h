// An angel. Since 2026-10-09 (owner request, Notion "Dev notes") angels fight like a Zenless Zone Zero crowd and adapt to the
// commander: a Daze meter (BIBRules/BIBStagger.h) that stuns them, flinches on hits ("stun locks"), attack telegraph flashes
// (gold = dodge / parry it, rose = unblockable), attack tokens so a few commit while the rest circle and flank, and moves
// chosen from what the shared angel mind (BIBRules/BIBAngelMind.h) has learned of the commander's habits. Bosses add the
// telegraphed slam, combo chains and a faster second phase.
//
// Gameplay Ability System: the enemy owns an ability system component and the shared attribute set; its health is the
// Health attribute, damage arrives as a GameplayEffect, and the statuses operators apply (BIB::Kits::FStatusBook) are
// mirrored onto the component as BIB.Status.* tags.
//
// Hit feel (2026-10-13, BIBRules/BIBHitFeel.h): every hit from the commander freezes the angel for the hit's weight (light
// 3 frames .. ultimate finisher 14) and plays a reaction scaled to the hit and to the angel (recoil, flinch, stagger,
// knockback, launch, knockdown) with the body tilting away from the hit. The meter is called STRAIN in game (code: Daze).
//
// Tactical brain (2026-10-13, goal 2 - BIBRules/BIBAngelBrain.h, BIBEnemyBrain.cpp): every angel perceives the commander with a
// reaction delay and a line of sight, remembers where it was, turns at a limited rate, keeps its role's spacing, waits for an
// attack token (UBIBAngelDirector), evades telegraphed heavies, punishes whiffs / heals / long casts, feints and varies combo
// enders by the commander's habits, retreats to regroup when hurt or close to BREAK and uses its whole kit. Every attack keeps
// a telegraph of at least the tier's MinTelegraph; ranged shots fly and can be sidestepped; melee strikes only land inside
// their swing cone. Knobs per tier: Config/DefaultGame.ini [BIB.AngelAI].
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "BIBRules/BIBAngelBrain.h"
#include "BIBRules/BIBAngelMind.h"
#include "BIBRules/BIBHitFeel.h"
#include "BIBRules/BIBKitStatus.h"
#include "BIBRules/BIBStagger.h"
#include "BIBGame/BIBTactics.h"
#include "BIBEnemy.generated.h"

class ABIBGameMode;
class ABIBPlayerCharacter;
class UAbilitySystemComponent;
class UBIBAbilitySystemComponent;
class UBIBAngelDirector;
class UBIBAttributeSet;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EBIBEnemyKind : uint8
{
	Grunt,
	Brute,
	MiniBoss,
	Boss,
	Ranged,
	Mage,
	Assassin
};

// Judge.md: Innocent (cannot be hurt, hurts Judge if attacked), Suspect (cannot be hurt), Guilty (can be hurt).
enum class EBIBJudgeState : uint8
{
	Innocent,
	Suspect,
	Guilty
};

/** A ranged angel's shot in flight (it flies at [BIB.AngelAI] ShotSpeed and can be sidestepped). */
struct FBIBAngelShot
{
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	float Life = 0.f;
	float Damage = 0.f;
	bool bUnblockable = false;
	TWeakObjectPtr<AActor> Visual;
};

UCLASS()
class BOLDINBEAUTYDEMO_API ABIBEnemy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABIBEnemy();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	void InitEnemy(EBIBEnemyKind InKind, ABIBGameMode* InMode);

	/** Multiplies health and damage by one number (tower floors, sub-stories). */
	void ApplyStatScale(float Scale);

	/** Campaign stages scale health and damage separately (BIBRules/BIBCampaign.h). SlamScale multiplies the chapter boss's slam. */
	void ApplyStatScales(float HpScale, float DamageScale, float SlamScale = 1.f);

	/** Kind used when spawned by the stage (also decides boss rules). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy")
	EBIBEnemyKind Kind = EBIBEnemyKind::Grunt;

	/** When true, health/damage/speed/size/timing come from the archetype table (BIBRules/BIBCampaign.cpp) for the kind; untick to use the values below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy")
	bool bUseTuningDefaults = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy", meta = (EditCondition = "!bUseTuningDefaults"))
	float CustomMaxHP = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy", meta = (EditCondition = "!bUseTuningDefaults"))
	float CustomAttackDamage = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy", meta = (EditCondition = "!bUseTuningDefaults"))
	float CustomMoveSpeed = 330.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy", meta = (EditCondition = "!bUseTuningDefaults"))
	float CustomRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy", meta = (EditCondition = "!bUseTuningDefaults"))
	float CustomHalfHeight = 90.f;

	/** Tint the body by state (Judge states, telegraphs) through the material's "Color" parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Enemy")
	bool bTintByState = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "BIB|Enemy")
	void OnEnemyDied();

	UFUNCTION(BlueprintImplementableEvent, Category = "BIB|Enemy")
	void OnAttackTelegraph();

	// Returns the damage actually removed from health (a damage GameplayEffect on this enemy's component).
	float ReceiveDamage(float Amount, AActor* Instigator = nullptr);
	/** A blow from the commander that deals no damage (Judge's stuns, a hit on an innocent): it still freezes, reacts and is felt. */
	void ReceiveImpact(ABIBPlayerCharacter* Commander, BIB::Feel::EWeight HitWeight);
	/** The next hit from the commander carries this Daze bonus (combo finisher, dodge / parry counter, chain attack). */
	void SetNextHitBonus(BIB::Stagger::EBonus Bonus) { NextHitBonus = Bonus; }

	// ---- Zenless-style state (BIBRules/BIBStagger.h)
	bool IsDazeStunned() const;
	bool IsChainOpen() const;
	void CloseChain() { Daze.ChainUntil = -1.0; }
	float GetDazeFraction() const;
	bool IsGuarding() const;
	/** Telegraphing or swinging: counts against the attack tokens. */
	bool IsCommitted() const { return AttackWindup > 0.f || SlamWindup > 0.f; }
	bool IsUnblockableWindup() const { return AttackWindup > 0.f && bCurrentUnblockable; }

	// ---- the tactical brain (BIBRules/BIBAngelBrain.h); UBIBAngelDirector and the tests read these
	int32 GetBrainId() const { return static_cast<int32>(GetUniqueID()); }
	BIB::Angel::ERole GetRole() const;
	float GetAttackRange() const { return AttackRange; }
	bool SeesCommander() const { return bSeesCommander; }
	/** Winding up, swinging, slamming or rushing: holds its attack token. */
	bool IsCommittedForCrowd() const;
	/** Read-only (tests): seconds until this angel's next strike can land on the commander - a windup, a slam or a shot in
	 *  flight toward it - or -1 when nothing is coming. Everything it reports is on screen as a telegraph. */
	float GetSecondsToStrike() const;
	void Stun(float Seconds);
	void Knockback(const FVector& FromLocation, float Strength);
	void AddPull(const FVector& VelocityCmPerSecond);
	void SetGravityInverted(bool bInverted);

	UFUNCTION(BlueprintPure, Category = "BIB|Enemy")
	bool IsAlive() const { return GetHP() > 0.f; }
	bool IsBoss() const { return Kind == EBIBEnemyKind::MiniBoss || Kind == EBIBEnemyKind::Boss; }
	float GetHP() const;
	float GetMaxHP() const;
	UFUNCTION(BlueprintPure, Category = "BIB|Enemy")
	float GetHealthFraction() const { const float Max = GetMaxHP(); return Max > 0.f ? GetHP() / Max : 0.f; }
	UBIBAbilitySystemComponent* GetBIBAbilitySystem() const { return AbilitySystem; }

	// ---- statuses applied by operator kits (rules layer: BIBRules/BIBKitStatus.h)
	BIB::Kits::FStatusBook& GetStatusBook() { return StatusBook; }
	const BIB::Kits::FStatusBook& GetStatusBook() const { return StatusBook; }
	/** SourceMember is the team slot that applied it (its ultimate charges from the damage over time). */
	bool ApplyKitStatus(BIB::Kits::EStatus Status, double Seconds, double Power, int32 Stacks, int32 SourceMember);
	/** Mirrors the status book (and the stun timer) into BIB.Status.* tags. */
	void RefreshStatusTags();
	/** Velocity impulse from a kit (bosses move a quarter as far). */
	void ApplyKitImpulse(const FVector& Velocity);
	float GetBodyRadius() const { return BodyRadius; }
	EBIBEnemyKind GetKind() const { return Kind; }
	FString GetKindName() const;

	/** Element (BIB::EElement as an int, -1 = none), set at spawn from the stage (owner request 2026-10-07, BIBRules/BIBElement.h).
	 *  Hits from an operator whose element is strong against it deal the advantage bonus. */
	void SetElement(int32 InElement);
	int32 GetElement() const { return ElementIndex; }

	// Judge bookkeeping
	EBIBJudgeState JudgeState = EBIBJudgeState::Innocent;
	int32 GavelStacks = 0;
	float GavelStacksExpireAt = 0.f;
	int32 HitsOnJudge = 0;
	float DamageOnJudge = 0.f;

	// Element system
	int32 ElementIndex = -1;

	// Luna bookkeeping
	float OrbitContactTime = 0.f;
	float OrbitNextTickAt = 0.f;
	float BlackholeTickAt = 0.f;
	bool bHitByBlackholeShot = false;

	// Narrator bookkeeping
	float LastAccelerationHitAt = -100.f;
	float LastBodyCheckAt = -100.f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> HeadComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextRenderComponent> LabelComp;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HeadMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBIBAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBIBAttributeSet> Attributes;

	BIB::Kits::FStatusBook StatusBook;

	// ---- Daze, hit reactions and the adaptive AI (2026-10-09)
	BIB::Stagger::FMeter Daze;
	BIB::Stagger::EWeight Weight = BIB::Stagger::EWeight::Light;
	BIB::Stagger::EBonus NextHitBonus = BIB::Stagger::EBonus::None;
	BIB::Angel::EMove CurrentMove = BIB::Angel::EMove::Strike;
	bool bCurrentUnblockable = false;
	float CurrentDamageMult = 1.f;
	int32 PendingFollowUps = 0;      // combo strikes still to come
	float GuardUntil = -1.f;
	int32 GuardHits = 0;
	float RecoverUntil = -1.f;
	double HitStopUntilReal = 0.0;
	double FlashUntilReal = 0.0;
	float FlashBrightness = 3.f;   // body colour multiplier during the hit flash (BIBCombatFeel settings)
	bool bPhaseTwo = false;
	bool bWasDazeStunned = false;
	// ---- hit reactions (BIBRules/BIBHitFeel.h, 2026-10-13)
	BIB::Feel::EReaction ReactionKind = BIB::Feel::EReaction::None;
	float ReactionStart = -100.f;
	FVector ReactionDir = FVector::ForwardVector;
	FVector BodyBaseLocation = FVector::ZeroVector;
	FVector HeadBaseLocation = FVector::ZeroVector;
	float PivotZ = -90.f;            // the feet: reactions tilt the body around them
	float LastTilt = 0.f;
	/** Plays a reaction away from From (the impulse and the tilt); bMove = false for the visual only. */
	void ApplyReaction(BIB::Feel::EReaction Reaction, const FVector& From, bool bMove = true);
	void UpdateReactionPose();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BarBackComp;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BarFillComp;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> DazeFillComp;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BarBackMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BarFillMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DazeFillMaterial;

	BIB::Angel::FMind* GetMind() const;
	void HitTarget(ABIBPlayerCharacter* Target, ABIBEnemy* Victim, float Damage, bool bMelee);
	FVector Separation() const;

	// ---- the tactical brain (BIBEnemyBrain.cpp)
	BIB::Angel::FBrainState Brain;
	BIB::Angel::FKnobs Knobs;
	BIB::Angel::FAction CurrentAction;   // the attack in flight (its enders, feint delay, armour)
	bool bSeesCommander = false;
	bool bAllyInLine = false;            // ranged: another angel stands in the line of fire
	float NextSightCheckAt = 0.f;
	float BaseWalkSpeed = 330.f;
	float DashUntil = -1.f;              // a sidestep, backstep, lunge, dash strike or charge in motion
	FVector DashDir = FVector::ZeroVector;
	float DashSpeed = 0.f;
	bool bDashStrikes = false;           // the rush hits the commander once on contact
	bool bDashHit = false;
	float SecondFlashAt = -1.f;          // feints and delayed enders flash again before the real hit
	int32 FollowUpIndex = 0;
	bool bSuperArmor = false;            // charge / slam: light hits do not interrupt it
	FVector AimPoint = FVector::ZeroVector;
	float AimLockAt = 0.f;               // ranged: the aim follows the (delayed) commander until the windup drops under this
	bool bAimLocked = false;
	TArray<FBIBAngelShot> Shots;            // ranged shots fly (ShotSpeed) and can be sidestepped

	UBIBAngelDirector* GetDirector() const;
	void TickBrain(float DeltaSeconds, ABIBPlayerCharacter* Target, float Distance, const FVector& Direction, float MoveMult, float DealtScale);
	/** Illusion: plain chase-and-strike against the nearest other angel. */
	void TickIllusion(float DeltaSeconds, ABIBEnemy* Victim, float Distance, const FVector& Direction, float MoveMult, float DealtScale);
	void Perceive(ABIBPlayerCharacter* Target, float Now);
	FVector LookPoint(const ABIBPlayerCharacter* Target) const;
	void TurnToward(const FVector& Point, float DeltaSeconds);
	void ExecuteAction(const BIB::Angel::FAction& Action, ABIBPlayerCharacter* Target, const FVector& Direction, float MoveMult);
	void StartAttack(const BIB::Angel::FAction& Action, ABIBPlayerCharacter* Target);
	void UpdateAim(ABIBPlayerCharacter* Target);
	void ResolveAttack(ABIBPlayerCharacter* Target, ABIBEnemy* Victim, float Distance, float DealtScale);
	void StartFollowUp();
	void FinishAttack();
	/** Stops the attack in flight (flinch, BREAK, stun) and gives the token back. */
	void CancelAttack();
	void StartDash(const FVector& Direction, float Speed, float Seconds, bool bStrikes);
	void TickDash(float DeltaSeconds, ABIBPlayerCharacter* Target, float DealtScale);
	void TickShots(float DeltaSeconds, ABIBPlayerCharacter* Target);
	void FireShot(const FVector& To, float Damage, float SpeedMult);
	void ReleaseToken();
	bool InStrikeCone(const AActor* Target, float Range, float HalfAngleDegrees) const;
	bool IsRangedRole() const { return Kind == EBIBEnemyKind::Ranged || Kind == EBIBEnemyKind::Mage; }
	void UpdateBars();
	bool bStatusTagsActive = false;
	TWeakObjectPtr<ABIBGameMode> Mode;
	float AttackDamage = 0.f;
	int32 AttackAbilityId = 1;   // BIB::Kits::EnemyAbilities id of the regular hit (Adapt copies it)
	float AttackRange = 150.f;
	float AttackIntervalS = 1.6f;   // seconds between attacks
	float AttackWindupS = 0.5f;     // telegraph before one lands
	float KeepMinCm = 0.f;          // ranged and mage enemies back off when the operator is closer than this ...
	float KeepMaxCm = 0.f;          // ... and approach until it is this close
	float AoeRadiusCm = 0.f;        // mage blast radius
	FVector AoeCenter = FVector::ZeroVector;   // where the current blast was aimed (fixed at the start of the telegraph)
	float SlamDamageMult = 1.f;     // chapter boss slam, see ApplyStatScales
	float BodyRadius = 40.f;
	float AttackCooldown = 0.f;
	float AttackWindup = -1.f;
	float StunTime = 0.f;
	float SlamWindup = -1.f;
	FVector PendingPull = FVector::ZeroVector;

	void Die();
	void UpdateLabelAndColour();
	void TickStatuses(float DeltaSeconds, ABIBPlayerCharacter* Target);
	ABIBPlayerCharacter* GetTarget() const;
	ABIBEnemy* FindNearestOtherEnemy() const;
};
