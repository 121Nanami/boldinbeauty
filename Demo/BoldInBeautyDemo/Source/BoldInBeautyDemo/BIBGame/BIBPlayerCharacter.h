// The commander's team in the field: three operators, one active at a time (MAIN §6).
//
// Kits follow the owner's mapping for the demo: file Ability 1-3 -> Move 1-3, Ability 4 -> Special,
// Ultimate unchanged. Luna, Judge and Narrator are written by hand (BIBKitLuna.cpp / BIBKitJudge.cpp /
// BIBKitNarrator.cpp); the other 17 operators are data (BIBRules/BIBKitData*.cpp) run by BIB::Kits::FRuntime through
// FBIBKitWorld (BIBPlayerData.cpp).
//
// Gameplay Ability System: the character owns an ability system component. Health, ultimate, shield and the kit
// resources of the active operator are attributes (UBIBAttributeSet); damage and healing are GameplayEffects; cooldowns
// are GameplayEffects granting BIB.Cooldown.<Operator>.<Slot>; every slot is a GameplayAbility
// (UBIBAbility_KitSlot) granted per team member; kit and combat state is mirrored as BIB.State.* tags (BIBPlayerGAS.cpp).
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "BIBGame/BIBCombatFeel.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBInput.h"
#include "BIBGame/BIBKitWorld.h"
#include "BIBRules/BIBHitFeel.h"
#include "BIBRules/BIBKitRuntime.h"
#include "BIBRules/BIBMeta.h"
#include "BIBRules/BIBStagger.h"
#include "BIBPlayerCharacter.generated.h"

class ABIBBlock;
class ABIBGameMode;
class ABIBPlayerController;
class UAbilitySystemComponent;
class UBIBAbilitySystemComponent;
class UBIBAttributeSet;
class UCameraComponent;
class UMaterialInstanceDynamic;
class USoundBase;
class USpringArmComponent;
class UStaticMeshComponent;

namespace BIB
{
	class FProfile;
}

enum class EBIBKitType : uint8
{
	None,
	Luna,
	Judge,
	Narrator,
	Data,       // one of the 17 data-driven kits (BIBRules/BIBKitData*.cpp), run by BIB::Kits::FRuntime
	Placeholder // no written kit: regular attack, block, parry, dodge, switching
};

enum class EBIBJudgeMode : uint8
{
	Witness,
	Lawyer,
	Judge
};

// Narrator's Global Exchange stats.
enum class EBIBStat : uint8
{
	Damage,
	Health,
	Speed,
	Ultimate,
	Cooldown,
	Count
};

enum class EBIBSlot : uint8
{
	Punch,
	Move1,
	Move2,
	Move3,
	Special,
	Ultimate,
	Dodge,
	Parry,
	Count
};

enum class EBIBAction : uint8
{
	None,
	Punch,
	Dodge,
	LunaBlackhole,
	LunaOrbitingWindup,
	LunaEventHorizon,
	LunaExpel,
	LunaCutscene,
	JudgePaperwork,
	JudgeCourtWindup,
	JudgeGavel,
	JudgeRetribution,
	JudgeJustice,
	NarratorSwitch,
	DianoAcceleration,
	DianoEndlessGod
};

struct FBIBMember
{
	int32 RosterIndex = -1;
	EBIBKitType Kit = EBIBKitType::None;
	FString Name;
	FString KitName;          // tag name: Luna, Judge, Narrator or the data kit's name (BIB.Ability.<KitName>.<Slot>)
	const BIB::Kits::FKitDef* KitDef = nullptr;   // data kits only
	float BaseMaxHP = 1.f;    // from the profile (level, stars, equipment)
	// HP / MaxHP / Ult mirror the attribute set while the member is active; the stored values belong to teammates.
	float MaxHP = 1.f;
	float HP = 1.f;
	float BaseAttack = 0.f;
	float DamageMult = 1.f;
	float DefencePercent = 0.f;
	float Ult = 0.f;          // 0..100
	bool bAlive = false;
	TSharedPtr<FBIBKitWorld> KitWorld;               // data kits: the runtime's window into the game
	TSharedPtr<BIB::Kits::FRuntime> Runtime;
	FGameplayAbilitySpecHandle Abilities[static_cast<int32>(EBIBSlot::Count)];
	FGameplayTag CooldownTags[static_cast<int32>(EBIBSlot::Count)];
	FGameplayTag AbilityTags[static_cast<int32>(EBIBSlot::Count)];
};

struct FBIBAbilityHud
{
	FString Key;
	FString Name;
	float Remaining = 0.f;
	FString Note;
};

struct FBIBTeamHud
{
	FString Name;
	float HPFraction = 0.f;
	bool bAlive = false;
	bool bActive = false;
};

struct FBIBHud
{
	bool bValid = false;
	FString ActiveName;
	float HP = 0.f;
	float MaxHP = 1.f;
	float Ult = 0.f;
	FLinearColor Colour = FLinearColor::White;
	TArray<FBIBAbilityHud> Abilities;
	TArray<FBIBTeamHud> Team;
	FString KitLine;
	FString ActionLine;
	bool bJudgeModeMenu = false;
	bool bExchangeOpen = false;
	float ExchangeValues[static_cast<int32>(EBIBStat::Count)] = {};
	int32 ExchangeFrom = 0;
	int32 ExchangeTo = 1;
	// Zenless-style combat feedback (2026-10-09)
	int32 ComboHits = 0;               // hits in the current chain (resets after 2.5 s without a hit)
	float ComboAge = 0.f;              // seconds since the last hit
	FString Banner;                    // PERFECT DODGE, CHAIN ATTACK, ANGELS ADAPTED ...
	FString BannerSub;
	FLinearColor BannerColour = FLinearColor::White;
	float BannerAlpha = 0.f;
	bool bChainReady = false;          // an enemy is stunned: Tab = chain attack
	bool bDodgeCounterReady = false;   // after a perfect dodge: LMB = dodge counter
	FString BossName;                  // the boss / mini boss in the fight, if any
	float BossHP = 0.f;
	float BossDaze = 0.f;
	bool bBossStunned = false;
	FString AngelLine;                 // what the angels have learned of you
	float AngelAdaptation = 0.f;
	FString MoveState;                 // CLIMBING, SWIMMING ... (traversal)
};

struct FBIBLunaState
{
	// Move 1 — Blackhole orb (0 none, 1 shot out, 2 orbiting, 3 final shot)
	int32 OrbPhase = 0;
	float OrbPhaseTime = 0.f;
	FVector OrbPos = FVector::ZeroVector;
	FVector OrbDir = FVector::ForwardVector;
	float OrbAngle = 0.f;
	TWeakObjectPtr<ABIBBlock> OrbVisual;
	// Move 2 — Orbiting holes
	float BlackUntil = 0.f;
	float WhiteUntil = 0.f;
	float OrbitAngle = 0.f;
	TWeakObjectPtr<ABIBBlock> BlackVisual;
	TWeakObjectPtr<ABIBBlock> WhiteVisual;
	// Move 3 / Special visuals
	TWeakObjectPtr<ABIBBlock> HorizonVisual;
	TWeakObjectPtr<ABIBBlock> ExpelVisual;
	// Ultimate — Reality Destabilisation
	float DestabiliseStart = -1.f;
	FVector DestabiliseLocation = FVector::ZeroVector;
	bool bGravityInverted = false;
	float NextFlipAt = 0.f;
	TWeakObjectPtr<ABIBBlock> DestabiliseVisual;
};

struct FBIBJudgeState
{
	EBIBJudgeMode Mode = EBIBJudgeMode::Witness;
	int32 RestrainingOrders = 0;
	bool bMenuOpen = false;
	float MenuOpenedAt = 0.f;
	EBIBJudgeMode PendingMode = EBIBJudgeMode::Witness;
	float CourtLockedUntil = 0.f;
	bool bForceJudgeAfterLock = false;
	float LastHitAt = -100.f;
	float NextLawyerConversionAt = 0.f;
	float JusticeRadius = 0.f;
	float JusticeNextGrowAt = 0.f;
	float RegAttackBuffUntil = 0.f;
	int32 GavelSmashesDone = 0;
	bool bActionEffectDone = false;
};

struct FBIBNarratorState
{
	float Speed = 25.f;
	float SecondTimer = 0.f;
	bool bHitThisSecond = false;
	float DamageX = 1.f;
	float HealthX = 1.f;
	float UltX = 1.f;
	float CooldownX = 1.f;
	float BonusAttack = 0.f;
	float BonusHP = 0.f;
	bool bDiano = false;
	bool bDianoPermanent = false;
	float DianoUntil = 0.f;
	float DianoBlockUntil = 0.f;
	float SavedNarratorSpeed = 25.f;
	bool bExchangeOpen = false;
	int32 ExchangeFrom = 0;
	int32 ExchangeTo = 1;
	bool bCloseEnoughUsed = false;
	float CloseEnoughAt = -1.f;
	float ParryUntil = 0.f;
	bool bAccelHit = false;
	bool bAccelMimic = false;
	float AccelChainUntil = 0.f;
	bool bAccelChainPending = false;
	float EndlessHealthGiven = 0.f;
	float EndlessDamageGiven = 0.f;
	float EndlessNextTickAt = 0.f;
	float LastWallHitAt = -100.f;
};

UCLASS()
class BOLDINBEAUTYDEMO_API ABIBPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	friend class FBIBKitWorld;

public:
	/** Settings > Controls: mouse sensitivity, inverted vertical look and the camera's field of view. */
	void ApplyViewSettings(const BIB::FSettings& Settings);

	ABIBPlayerCharacter();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UBIBAbilitySystemComponent* GetBIBAbilitySystem() const { return AbilitySystem; }
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
		FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** TeamOverride (3 roster indices, -1 = empty slot) replaces the profile's team, e.g. for the tower's rarity modes. */
	void SetupTeam(const BIB::FProfile& Profile, const int32* TeamOverride = nullptr);
	/** Heals every living team member by a share of their maximum health (Boss Rush, between rounds). */
	void HealTeamFraction(float Fraction);
	void SetCombatActive(bool bActive);
	bool IsCombatActive() const { return bCombatActive; }
	/** Walking the Ark (the hub since 2026-10-07): move, look and jump with combat off (no attacks, no HUD). */
	void SetWalkOnly(bool bWalk) { bWalkOnly = bWalk; }
	bool IsWalkOnly() const { return bWalkOnly; }
	bool IsJudgeActive() const;
	/** Roster index of the operator on the field (-1 = none); the element system reads it (BIBRules/BIBElement.h). */
	int32 GetActiveRosterIndex() const;
	/** EnemyAbilityId feeds Adapt's copy mechanic (BIB::Kits::EnemyAbilities), 0 = none. */
	void ReceiveEnemyHit(ABIBEnemy* Attacker, float Damage, int32 EnemyAbilityId = 0, bool bMelee = true, bool bUnblockable = false);

	// ---- Zenless-style combat feel and the angels' view of the commander (BIBPlayerFeel.cpp, 2026-10-09)
	/** The active operator's attack: hits are measured against it for Daze and hit stop. */
	float GetActiveAttack() const;
	bool IsAirborneForAngels() const;
	bool IsBlockingForAngels() const { return bBlocking; }
	/** Read-only: what anyone watching could see of the commander this frame - position, movement, the attack being wound up
	 *  or recovered from, dodges, blocks, long casts, health (the angels' perception, BIBRules/BIBAngelBrain.h; BIBAngelView.cpp). */
	BIB::Angel::FPlayerView GetAngelView() const;
	float SecondsSinceSwitch() const;
	float SecondsSinceAttack() const;
	void OnGuardBroken(ABIBEnemy* Enemy);
	void OnEnemyDazeStunned(ABIBEnemy* Enemy);
	/** Every hit of the commander's team on an angel ends here (sparks, combo counter, hit stop, camera, rumble, sound,
	 *  the operator's impact pose, what the angels learn). Weight comes from ClassifyHit (BIBRules/BIBHitFeel.h). */
	void OnHitLanded(ABIBEnemy* Enemy, float Dealt, bool bGuarded, BIB::Feel::EWeight Weight);
	/** The felt part of a hit (once per frame at the heaviest weight): hit stop of the operator, impact frame, camera kick,
	 *  push-in, rumble, sound, impact pose; the ultimate finisher's big impact. Also used by hits that deal no damage. */
	void ApplyHitFeel(ABIBEnemy* Enemy, BIB::Feel::EWeight Weight, const FVector& At);
	/** The weight of the hit being dealt now: its source (set by FBIBScopedHit) and its size (Ratio = damage / attack). */
	BIB::Feel::EWeight ClassifyHit(float Ratio) const;
	/** Where the hits dealt from now on come from (FBIBScopedHit sets and restores it around a hit). */
	void SetHitSource(const BIB::Feel::FHitSource& Source) { PendingHit = Source; }
	const BIB::Feel::FHitSource& GetHitSource() const { return PendingHit; }
	/** True while the basic string's finisher of the engine string (hand-written kits, single-hit data kits) is swinging. */
	bool IsEngineComboFinisher() const;
	/** The ultimate's cinematic beat is playing (the world nearly stops; the operator cannot be hit). */
	bool IsUltCinematic() const { return bUltCinematic; }
	/** Damage multiplier of the hit being dealt now (the basic-attack combo string). */
	float OutgoingHitMult() const;
	/** Combat feel (BIBPlayerFeelFx.cpp, 2026-10-12): the player's presentation settings and how heavy the ability behind the hit
	 *  being dealt feels. Presentation only; the angel scales its own hit stop and flash with them. */
	const BIBCombatFeel::FConfig& GetFeelConfig() const { return FeelCfg; }
	float FeelHitStopMult() const;
	/** Console: BIBAngels <expertise 0..1> teaches the angels everything (or forgets, 0). */
	void DebugSetAngelExpertise(float Value);
	/** Mobile: the touch layer feeds movement and buttons through these (BIBTouch). */
	void SetTouchMove(const FVector2D& Axis) { TouchMove = Axis; }
	/** An enemy status (burn, bleed, poison, shock) dealt damage; SourceMember is the team slot that applied it. */
	void OnKitStatusDamage(ABIBEnemy* Enemy, double Amount, int32 SourceMember);
	void OnEnemyDied(ABIBEnemy* Enemy);

	// ---- Gameplay Ability System: every operator slot is a GameplayAbility (UBIBAbility_KitSlot)
	bool CanActivateKitSlot(int32 MemberIndex, EBIBSlot Slot, FString* OutReason) const;
	bool PerformKitSlot(int32 MemberIndex, EBIBSlot Slot);
	FGameplayTag AbilityTagFor(int32 MemberIndex, EBIBSlot Slot) const;
	FGameplayTag CooldownTagFor(int32 MemberIndex, EBIBSlot Slot) const;
	/** Active operator's data-kit runtime (nullptr for Luna, Judge and Narrator). */
	BIB::Kits::FRuntime* GetActiveRuntime() const;
	/** Console: BIBUltVariant <n> picks the active operator's alternative ultimate (Alchemist). */
	void DebugSelectUltimate(int32 Variant);
	/** Console: BIBTags prints the active gameplay tags of the operator. */
	FString DescribeActiveTags() const;
	const FBIBHud& GetHud() const { return Hud; }
	void ClearKitEffects();

	// Test hooks (console: BIBDamageEnemies <amount>, BIBFillUlt). Damage goes through the normal enemy path.
	void DebugDamageEnemies(float Amount);
	void DebugFillUlt();
	void DebugSetNarratorSpeed(float Speed);

	/** Narrator/Diano: cm/s of automatic forward movement once Speed is above the base (starting) speed. */
	float NarratorForcedSpeed() const;

	// Narrator's Global Exchange panel (clicked in the UI).
	void ExchangeSelect(bool bFrom, int32 StatIndex);
	void ExchangeApply();
	void CloseExchange();
	static FString StatName(int32 StatIndex);

	UFUNCTION(BlueprintPure, Category = "BIB")
	FString GetActiveOperatorName() const { return Active().Name; }

	/** Called when the active operator changes (stage start, Tab, or a knocked-out operator). Swap meshes here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BIB")
	void OnActiveOperatorChanged(const FString& OperatorName, int32 TeamSlot);

	/** Tint the body by operator / Judge mode through the material's "Color" parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BIB|Visuals")
	bool bTintByOperator = true;

private:
	bool bWalkOnly = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> HeadComp;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HeadMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBIBAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BIB|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBIBAttributeSet> Attributes;

	FBIBMember Members[3];
	int32 ActiveIndex = 0;
	bool bCombatActive = false;
	float SwitchReadyAt = 0.f;
	float LookSensitivityScale = 1.f; // from Settings > Controls (1.0 = default 5/10)
	float LookPitchSign = 1.f;        // -1 when Settings > Controls inverts the vertical look

	EBIBAction Action = EBIBAction::None;
	float ActionTime = 0.f;
	float DamageTakenThisAction = 0.f;
	FVector ActionDirection = FVector::ForwardVector;
	bool bBlocking = false;
	// Invulnerability and parry windows are timed GameplayEffects (BIB.State.Invulnerable / BIB.State.Parrying); the
	// timestamps are the fallback if a tag is ever missing from the project settings.
	float InvulnerableUntil = 0.f;
	float ParryUntil = 0.f;

	// Data kits: timed movement requested by the runtime (dashes, blinks), movement lock and held keys.
	FVector KitMoveFrom = FVector::ZeroVector;
	FVector KitMoveTo = FVector::ZeroVector;
	float KitMoveTotal = 0.f;
	float KitMoveLeft = 0.f;
	float KitLockedUntil = 0.f;
	bool bKitHeld[static_cast<int32>(EBIBSlot::Count)] = {};
	float LastKitMessageAt = -10.f;
	TArray<int32> PendingKills; // enemy ids killed since the last tick; handed to the runtime outside its own steps

	// ---- hit feel (BIBRules/BIBHitFeel.h, BIBPlayerFeel.cpp; owner request 2026-10-13)
	BIB::Feel::FHitSource PendingHit;
	uint64 FeelFrame = 0;             // the frame whose hit feel was applied ...
	int32 FeelWeightThisFrame = -1;   // ... and the heaviest weight applied in it (several hits in a frame feel like one)
	// procedural pose of the body meshes (no animation clips exist yet): wind-up -> impact -> recovery
	float PoseTime = -1.f;            // seconds into the current attack (-1 = none); advances with the operator's own time
	float PoseImpactAt = 0.f;
	float PoseRecovery = 0.f;
	float PoseStrength = 0.f;
	float PulseAge = -1.f;            // seconds since the latest hit landed (-1 = none)
	float PulseLean = 0.f;
	FVector BodyBaseLocation = FVector(0.0, 0.0, -20.0);
	FVector BodyBaseScale = FVector(0.8, 0.8, 1.35);
	FVector HeadBaseLocation = FVector(25.0, 0.0, 70.0);
	// camera
	float BaseArmLength = 650.f;
	float PushInPeak = 0.f;
	double PushInStartReal = 0.0;
	// ultimate cinematic beat
	bool bUltCinematic = false;
	double UltCinematicStartReal = 0.0;
	float UltCameraYawStart = 0.f;
	uint64 InputSwallowFrame = 0;     // the press that skipped the cinematic does nothing else
	bool bSlotPerformed = false;      // the last PerformKitSlot really cast (the GAS ability "activates" even when the kit refuses)
	FString UltName;
	void StartPose(float ImpactAt, float Recovery, float Strength);
	void UpdatePose(float DeltaSeconds);
	void UpdateCameraFeel();
	void BeginUltCinematic(const FString& Name);
	void TickUltCinematic();
	void EndUltCinematic();
	void PlayFeelSounds(const TArray<USoundBase*>& Layers, const FVector& Location, float Volume = 1.f) const;
	void Rumble(float Intensity, float Seconds) const;
	/** Seconds from a data-kit ability's start to its first damaging step, and its recovery after it (for the pose). */
	static void KitPoseTiming(const BIB::Kits::FAbilityDef& Def, int32 ComboIndex, float& OutImpactAt, float& OutRecovery);
	/** Dodge input during a cast's recovery (after its last hit) cancels the recovery. */
	bool TryRecoveryCancel();
	bool IsActionInRecovery() const;

	// ---- combat feel (BIBPlayerFeel.cpp)
	float BufferedAt[static_cast<int32>(EBIBInput::Count)];
	BIB::Stagger::FCombo Combo;
	int32 ComboStepNow = -1;          // step of the basic attack being swung
	float PunchHitsUntil = -1.f;      // data kits: hits until then belong to the basic attack
	float LastAttackAt = -100.f;
	float LastSwitchAt = -100.f;
	float DodgeCounterUntil = -1.f;
	float PerfectDodgeReadyAt = 0.f;
	TWeakObjectPtr<ABIBEnemy> ChainTarget;
	float ChainUntil = -1.f;
	double SlowMoUntilReal = 0.0;
	double SelfHitStopUntilReal = 0.0;
	uint64 HitFrame = 0;
	int32 HitsThisFrame = 0;
	bool bAreaCounted = false;
	float BlockSeconds = 0.f;
	float KiteSeconds = 0.f;
	int32 ComboHits = 0;
	float LastHitAt = -100.f;
	/** The last time a real strike landed (damage-over-time ticks excluded): the angels' whiff check (BIBAngelView.cpp). */
	float LastStrikeHitAt = -100.f;
	FString BannerText;
	FString BannerSubText;
	FLinearColor BannerColour = FLinearColor::White;
	double BannerUntilReal = 0.0;
	FVector2D TouchMove = FVector2D::ZeroVector;
	// traversal
	bool bClimbing = false;
	FVector ClimbNormal = FVector::ZeroVector;
	float ClimbCooldownUntil = 0.f;
	float MantleUntil = -1.f;
	float LandedLockUntil = -1.f;
	float FallPeakZ = 0.f;
	bool bWasFalling = false;

	void FeelTickStart(float DeltaSeconds);
	void FeelTick(float DeltaSeconds);

	// ---- presentation layer (BIBPlayerFeelFx.cpp): glow, field-of-view punch, vignette pulse, ground telegraphs and sound hooks.
	// It is called from events that already exist (a cast, an action starting, a hit, a block / parry / dodge) and never reads or
	// writes damage, cooldowns, durations or hit detection.
	BIBCombatFeel::FConfig FeelCfg;
	float FovBase = 90.f;
	float FovPunchPeak = 0.f;
	double FovPunchStartReal = 0.0;
	double FovPunchEndReal = 0.0;
	double BeatStartReal = 0.0;         // the ultimate's vignette pulse
	double BeatEndReal = 0.0;
	bool bBeatActive = false;
	EBIBSlot FeelSlot = EBIBSlot::Punch;
	float FeelSlotAt = -100.f;          // Now() of the last cast: hits within FeelSlotWindow belong to that slot
	double TrailNextReal = 0.0;
	void FeelApplySettings(const BIB::FSettings& Settings);
	void FeelOnCast(EBIBSlot Slot);
	void FeelOnActionStart(EBIBAction NewAction);
	void FeelOnHit(ABIBEnemy* Enemy, float Ratio, const FVector& At, const FLinearColor& Colour);
	void FeelOnDefence(int32 Kind);
	void FeelUltimateBeat();
	void FeelTickView();
	void FeelTrail(const FVector& At, const FVector& Direction, const FLinearColor& Colour);
	void FeelDataAreaTelegraphs(EBIBSlot Slot);
	void FovPunch(float Degrees, float Seconds);
	int32 FeelSlotForHit() const;
	bool IsBufferable(EBIBInput Input) const;
	void ConsumeBuffer(EBIBInput Input);
	void ClearBuffers();
	void ObserveSlot(EBIBSlot Slot);
	void ShowBanner(const FString& Text, const FString& Sub, const FLinearColor& Colour, double Seconds = 1.2);
	void SetSlowMo(float Dilation, double RealSeconds);
	void Shake(float Strength, float Seconds);
	/** The "13th frame": an extra fresh frame now (and an impact frame for heavy moments), see ABIBAnimeStyleVolume::RequestAccent. */
	void Accent(bool bImpact = false);
	// motion pack moments (BIBMotionEvents.h)
	bool ScreenPoint(const FVector& World, FVector2D& Out) const;
	TWeakObjectPtr<ABIBEnemy> LastLockTarget;
	int32 NextComboMilestone = 10;
	bool bUltWasReady = false;
	// Set 2 motion VFX (BIBMotionEvents): periodic traversal effects, the angels' overlays, Judge mode changes, operator loops
	float VfxClimbAt = 0.f;
	float VfxSwimAt = 0.f;
	float VfxEnemySyncAt = 0.f;
	bool bVfxSwimming = false;
	int32 VfxJudgeMode = -1;
	bool bVfxJudgeLocked = false;
	bool bVfxBurningOut = false;
	FString VfxLoopOperator;
	void MotionVfxTick();
	bool bWasLowHP = false;
	bool bBossSeen = false;
	bool bMiniBossSeen = false;
	float TurnYawLast = 0.f;
	float TurnAccum = 0.f;
	float TurnWindowStart = 0.f;
	bool bWasMoving = false;
	void PerfectDodge(ABIBEnemy* Attacker);
	bool TryDodgeCounter();
	bool TryDashAttack();
	float DashAttackReadyAt = 0.f;
	void SwitchStrike(bool bChain);
	ABIBEnemy* NearestEnemy(float MaxDistance) const;
	void BuildFeelHud();
	// traversal (BIBPlayerFeel.cpp)
	void TraversalTick(float DeltaSeconds);
	bool TryStartClimb();
	void StopClimb(bool bJumpOff);
	void HandleSwim();
	bool CanClimbOrSwim() const;

	FBIBLunaState LunaData;
	FBIBJudgeState JudgeData;
	FBIBNarratorState NarratorData;
	FBIBHud Hud;

	// ---------------------------------------------------------------- common
	float Now() const;
	FBIBMember& Active() { return Members[ActiveIndex]; }
	const FBIBMember& Active() const { return Members[ActiveIndex]; }
	int32 FindMember(EBIBKitType Kit) const;
	ABIBPlayerController* GetPC() const;
	ABIBGameMode* GetMode() const;
	bool Pressed(EBIBInput Input) const;
	bool Held(EBIBInput Input) const;
	void NotifyOperatorChanged();
	void Message(const FString& Text) const;

	// ---- GAS plumbing (BIBPlayerGAS.cpp)
	void InitGAS();
	void CacheMemberTags(FBIBMember& Member);
	void OnAbilityDamageTaken(float Dealt, float Overkill, AActor* Instigator);
	void GrantMemberAbilities(int32 Index);
	void ClearAllMemberCooldowns();
	bool TryActivateSlot(EBIBSlot Slot);
	EBIBInput InputForSlot(EBIBSlot Slot) const;
	void SetMemberHP(int32 Index, float Value);
	void SetMemberMaxHP(int32 Index, float Value);
	void SetMemberUlt(int32 Index, float Value);
	void HealMember(int32 Index, float Amount);
	void SyncActiveFromAttributes();
	void LoadMemberIntoAttributes(int32 Index);
	void MirrorKitAttributes();
	void UpdateStateTags();
	void GrantInvulnerable(float Seconds);
	void GrantParry(float Seconds);
	bool IsInvulnerableNow() const;
	bool IsParryingNow() const;
	void ApplyKitHealthScale(int32 Index, float Mult, bool bFill);
	FString AbilityLabel(int32 Index, EBIBSlot Slot) const;

	bool IsReady(EBIBSlot Slot) const;
	float Remaining(EBIBSlot Slot) const;
	void StartCooldown(EBIBSlot Slot, float Seconds);
	float Remaining(int32 Index, EBIBSlot Slot) const;
	void StartCooldown(int32 Index, EBIBSlot Slot, float Seconds);
	void ReduceCooldown(int32 Index, EBIBSlot Slot, float Seconds);
	void StartAction(EBIBAction NewAction);
	void EndAction();
	void CancelAction();
	bool IsMovementLocked() const;
	FVector AimForward() const;
	void FaceAim();

	void HandleLookAndMove();
	void HandleCommonInput();
	void HandleKitInput();
	void UpdateAction(float DeltaSeconds);
	void UpdateMovementStats();
	void UpdateVisuals();
	void BuildHud();
	void AddAbilityHud(const FString& Key, const FString& Name, EBIBSlot Slot, const FString& Note = FString());

	void TrySwitch(int32 NewIndex, bool bForced);
	void OnActiveDown();
	void ResetJudgeWorld();

	TArray<ABIBEnemy*> EnemiesInRadius(const FVector& Centre, float Radius, bool bIncludeAirborne = true) const;
	TArray<ABIBEnemy*> EnemiesInFront(float Range, float MinDot = 0.35f) const;
	// Damage dealt by team member SourceIndex (-1 = active). Applies Judge targeting rules when the source is Judge.
	float DealDamage(ABIBEnemy* Enemy, float Amount, int32 SourceIndex = -1);
	// Instant kill; bosses (incl. mini-bosses) take BossFraction of max health instead when bRespectBosses.
	void KillEnemy(ABIBEnemy* Enemy, bool bRespectBosses, int32 SourceIndex = -1);
	void GainUlt(int32 SourceIndex, float DamageDealt);
	void Explode(const FVector& Centre, float Radius, float Damage, float KnockbackStrength, const FColor& Colour, int32 SourceIndex = -1);
	void TakeSelfDamage(float Amount);
	bool DoPunch();
	bool DodgeActivate();
	bool ParryActivate();

	// ---------------------------------------------------------------- data-driven kits (BIBPlayerData.cpp)
	void DataKitInput();
	bool DataKitActivate(EBIBSlot Slot);
	void DataKitTick(float DeltaSeconds);
	void DataKitSwitchedOut(int32 Index);
	void DataKitSwitchedIn(int32 Index);
	void DataKitBuildHud(const FBIBMember& Member);
	void SetupDataKit(FBIBMember& Member, int32 Index);
	void BeginKitMove(const FVector& To, float Seconds);
	void TickKitMove(float DeltaSeconds);
	void ShareTeammatePassives();
	float DataKitReceiveHit(float Amount, ABIBEnemy* Attacker, int32 EnemyAbilityId, bool bMelee, bool& bNegated);

	// ---------------------------------------------------------------- Luna
	void LunaInput();
	bool LunaActivate(EBIBSlot Slot);
	void LunaUpdateAction(float DeltaSeconds);
	void LunaTick(float DeltaSeconds);
	void LunaFireOrb();
	void LunaExplodeOrb();
	void LunaReleaseExpel();
	void LunaClearOrbits();
	FString LunaKitLine() const;

	// ---------------------------------------------------------------- Judge
	void JudgeInput();
	bool JudgeActivate(EBIBSlot Slot);
	void JudgeUpdateAction(float DeltaSeconds);
	void JudgeTick(float DeltaSeconds);
	void JudgeOnHit(ABIBEnemy* Attacker, float Damage);
	void JudgeJusticeTrigger(ABIBEnemy* Attacker);
	bool JudgeAnyInputPressed() const;
	FString JudgeKitLine() const;
	static FString JudgeModeName(EBIBJudgeMode InMode);

	// ---------------------------------------------------------------- Narrator / Diano
	void NarratorInput();
	bool NarratorActivate(EBIBSlot Slot);
	void NarratorUpdateAction(float DeltaSeconds);
	void NarratorTick(float DeltaSeconds);
	void NarratorRecalcHealth();
	void NarratorEndDiano(const FString& Reason);
	void NarratorStartAcceleration();
	float NarratorAttack() const;
	float NarratorCooldown(float Seconds) const;
	FString NarratorKitLine() const;
};

/** Marks the hits dealt while it lives as coming from Source (BIBRules/BIBHitFeel.h), then restores the previous source. */
struct FBIBScopedHit
{
	FBIBScopedHit(ABIBPlayerCharacter* InOwner, BIB::Feel::ESource Source, bool bFinal = false, int32 Override = -1)
		: Owner(InOwner)
	{
		if (Owner)
		{
			Saved = Owner->GetHitSource();
			BIB::Feel::FHitSource Hit;
			Hit.Source = Source;
			Hit.bFinal = bFinal;
			Hit.Override = Override;
			Owner->SetHitSource(Hit);
		}
	}
	FBIBScopedHit(ABIBPlayerCharacter* InOwner, const BIB::Feel::FHitSource& Hit)
		: Owner(InOwner)
	{
		if (Owner)
		{
			Saved = Owner->GetHitSource();
			Owner->SetHitSource(Hit);
		}
	}
	~FBIBScopedHit()
	{
		if (Owner)
		{
			Owner->SetHitSource(Saved);
		}
	}
	FBIBScopedHit(const FBIBScopedHit&) = delete;
	FBIBScopedHit& operator=(const FBIBScopedHit&) = delete;

private:
	ABIBPlayerCharacter* Owner = nullptr;
	BIB::Feel::FHitSource Saved;
};
