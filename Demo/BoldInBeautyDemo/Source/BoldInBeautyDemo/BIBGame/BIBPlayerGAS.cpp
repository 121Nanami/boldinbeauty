// Gameplay Ability System plumbing of the player character: component set-up, attribute-backed health / ultimate,
// cooldowns as GameplayEffects, ability granting and activation, and the mirrored state tags.
#include "BIBGame/BIBPlayerCharacter.h"
#include "BIBGame/BIBMotionEvents.h"
#include "BIBGame/BIBFx.h"
#include "BIBGame/BIBPlayerController.h"

#include "BIBGAS/BIBAbilitySystemComponent.h"
#include "BIBGAS/BIBAttributeSet.h"
#include "BIBGAS/BIBGameplayAbility.h"
#include "BIBGAS/BIBGameplayTags.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBGame/BIBTuning.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	const TCHAR* SlotTagName(EBIBSlot Slot)
	{
		switch (Slot)
		{
		case EBIBSlot::Punch: return TEXT("Basic");
		case EBIBSlot::Move1: return TEXT("Move1");
		case EBIBSlot::Move2: return TEXT("Move2");
		case EBIBSlot::Move3: return TEXT("Move3");
		case EBIBSlot::Special: return TEXT("Special");
		case EBIBSlot::Ultimate: return TEXT("Ultimate");
		case EBIBSlot::Dodge: return TEXT("Dodge");
		case EBIBSlot::Parry: return TEXT("Parry");
		default: return TEXT("Basic");
		}
	}

	BIB::Kits::ESlotId ToKitSlot(EBIBSlot Slot)
	{
		switch (Slot)
		{
		case EBIBSlot::Move1: return BIB::Kits::ESlotId::Move1;
		case EBIBSlot::Move2: return BIB::Kits::ESlotId::Move2;
		case EBIBSlot::Move3: return BIB::Kits::ESlotId::Move3;
		case EBIBSlot::Special: return BIB::Kits::ESlotId::Special;
		case EBIBSlot::Ultimate: return BIB::Kits::ESlotId::Ultimate;
		default: return BIB::Kits::ESlotId::Basic;
		}
	}
}

// ============================================================================ component

UAbilitySystemComponent* ABIBPlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ABIBPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	InitGAS();
	// Hit-feel poses and the camera push-in work relative to the body / camera as set up (Blueprint defaults included).
	if (BodyComp && HeadComp)
	{
		BodyBaseLocation = BodyComp->GetRelativeLocation();
		BodyBaseScale = BodyComp->GetRelativeScale3D();
		HeadBaseLocation = HeadComp->GetRelativeLocation();
	}
	if (CameraBoom)
	{
		BaseArmLength = CameraBoom->TargetArmLength;
	}
}

void ABIBPlayerCharacter::InitGAS()
{
	if (!AbilitySystem)
	{
		return;
	}
	AbilitySystem->InitAbilityActorInfo(this, this);
	if (Attributes && !AbilitySystem->GetSet<UBIBAttributeSet>())
	{
		AbilitySystem->AddAttributeSetSubobject(Attributes.Get());
	}
	AbilitySystem->OnDamageTaken.AddUObject(this, &ABIBPlayerCharacter::OnAbilityDamageTaken);
	AbilitySystem->AddLooseGameplayTag(BIBTags::TeamPlayer());
}

void ABIBPlayerCharacter::OnAbilityDamageTaken(float Dealt, float Overkill, AActor* Instigator)
{
	SyncActiveFromAttributes();
}

// ============================================================================ tags and cooldowns

void ABIBPlayerCharacter::CacheMemberTags(FBIBMember& Member)
{
	for (int32 Slot = 0; Slot < static_cast<int32>(EBIBSlot::Count); ++Slot)
	{
		const TCHAR* SlotName = SlotTagName(static_cast<EBIBSlot>(Slot));
		Member.CooldownTags[Slot] = BIBTags::Cooldown(Member.KitName, SlotName);
		Member.AbilityTags[Slot] = BIBTags::Ability(Member.KitName, SlotName);
	}
}

FGameplayTag ABIBPlayerCharacter::CooldownTagFor(int32 MemberIndex, EBIBSlot Slot) const
{
	return (MemberIndex >= 0 && MemberIndex < 3) ? Members[MemberIndex].CooldownTags[static_cast<int32>(Slot)] : FGameplayTag();
}

FGameplayTag ABIBPlayerCharacter::AbilityTagFor(int32 MemberIndex, EBIBSlot Slot) const
{
	return (MemberIndex >= 0 && MemberIndex < 3) ? Members[MemberIndex].AbilityTags[static_cast<int32>(Slot)] : FGameplayTag();
}

bool ABIBPlayerCharacter::IsReady(EBIBSlot Slot) const
{
	return !AbilitySystem->IsOnCooldown(Active().CooldownTags[static_cast<int32>(Slot)]);
}

float ABIBPlayerCharacter::Remaining(EBIBSlot Slot) const
{
	return Remaining(ActiveIndex, Slot);
}

void ABIBPlayerCharacter::StartCooldown(EBIBSlot Slot, float Seconds)
{
	StartCooldown(ActiveIndex, Slot, Seconds);
}

float ABIBPlayerCharacter::Remaining(int32 Index, EBIBSlot Slot) const
{
	return AbilitySystem->GetCooldownRemaining(CooldownTagFor(Index, Slot));
}

void ABIBPlayerCharacter::StartCooldown(int32 Index, EBIBSlot Slot, float Seconds)
{
	if (Seconds <= 0.f)
	{
		AbilitySystem->ClearCooldown(CooldownTagFor(Index, Slot));
		return;
	}
	AbilitySystem->StartCooldown(CooldownTagFor(Index, Slot), Seconds);
}

void ABIBPlayerCharacter::ReduceCooldown(int32 Index, EBIBSlot Slot, float Seconds)
{
	AbilitySystem->ReduceCooldown(CooldownTagFor(Index, Slot), Seconds);
}

void ABIBPlayerCharacter::ClearAllMemberCooldowns()
{
	for (const FBIBMember& Member : Members)
	{
		for (int32 Slot = 0; Slot < static_cast<int32>(EBIBSlot::Count); ++Slot)
		{
			AbilitySystem->ClearCooldown(Member.CooldownTags[Slot]);
		}
	}
	AbilitySystem->RemoveTimedTag(BIBTags::StateInvulnerable());
	AbilitySystem->RemoveTimedTag(BIBTags::StateParrying());
	InvulnerableUntil = 0.f;
	ParryUntil = 0.f;
}

void ABIBPlayerCharacter::GrantInvulnerable(float Seconds)
{
	InvulnerableUntil = FMath::Max(InvulnerableUntil, Now() + Seconds);
	AbilitySystem->GrantTimedTag(BIBTags::StateInvulnerable(), Seconds);
}

void ABIBPlayerCharacter::GrantParry(float Seconds)
{
	ParryUntil = FMath::Max(ParryUntil, Now() + Seconds);
	AbilitySystem->GrantTimedTag(BIBTags::StateParrying(), Seconds);
}

bool ABIBPlayerCharacter::IsInvulnerableNow() const
{
	return bUltCinematic || Now() < InvulnerableUntil || AbilitySystem->HasMatchingGameplayTag(BIBTags::StateInvulnerable());
}

bool ABIBPlayerCharacter::IsParryingNow() const
{
	return Now() < ParryUntil || AbilitySystem->HasMatchingGameplayTag(BIBTags::StateParrying());
}

// ============================================================================ abilities

void ABIBPlayerCharacter::GrantMemberAbilities(int32 Index)
{
	FBIBMember& Member = Members[Index];
	for (int32 Slot = 0; Slot < static_cast<int32>(EBIBSlot::Count); ++Slot)
	{
		Member.Abilities[Slot] = FGameplayAbilitySpecHandle();
		if (Member.Kit == EBIBKitType::None)
		{
			continue;
		}
		const FGameplayAbilitySpec Spec(UBIBAbility_KitSlot::StaticClass(), 1, UBIBAbility_KitSlot::PackSlot(Index, Slot), this);
		Member.Abilities[Slot] = AbilitySystem->GiveAbility(Spec);
	}
}

FString ABIBPlayerCharacter::AbilityLabel(int32 Index, EBIBSlot Slot) const
{
	const FBIBMember& Member = Members[Index];
	switch (Slot)
	{
	case EBIBSlot::Dodge: return TEXT("Dodge");
	case EBIBSlot::Parry: return TEXT("Parry");
	default: break;
	}
	if (Member.KitDef)
	{
		const BIB::Kits::FAbilityDef& Def = Member.KitDef->Abilities[static_cast<int32>(ToKitSlot(Slot))];
		return FString(UTF8_TO_TCHAR(Def.Name.c_str()));
	}
	static const TCHAR* const Luna[] = {TEXT("Regular attack"), TEXT("Blackhole"), TEXT("Orbiting"), TEXT("Event horizon"), TEXT("Expel"), TEXT("Reality Destabilisation")};
	static const TCHAR* const Judge[] = {TEXT("Regular attack"), TEXT("Paperwork"), TEXT("Court setting"), TEXT("Gavel smash"), TEXT("Retribution"), TEXT("Justice")};
	static const TCHAR* const Narrator[] = {TEXT("Regular attack"), TEXT("Explosion"), TEXT("Global Exchange"), TEXT("I AM STILL ALIVE"), TEXT("Close enough"), TEXT("Switch")};
	static const TCHAR* const Diano[] = {TEXT("Regular attack"), TEXT("Acceleration"), TEXT("Endless god"), TEXT("Embodiment of imperfection"), TEXT("The TRUE STRONGEST"), TEXT("THE SOUL ONE")};
	const int32 Number = static_cast<int32>(Slot);
	switch (Member.Kit)
	{
	case EBIBKitType::Luna: return Luna[Number];
	case EBIBKitType::Judge: return Judge[Number];
	case EBIBKitType::Narrator: return NarratorData.bDiano ? Diano[Number] : Narrator[Number];
	default: return TEXT("Regular attack");
	}
}

EBIBInput ABIBPlayerCharacter::InputForSlot(EBIBSlot Slot) const
{
	const FBIBMember& Member = Active();
	switch (Slot)
	{
	case EBIBSlot::Punch: return EBIBInput::Attack;
	case EBIBSlot::Move1: return EBIBInput::Move1;
	case EBIBSlot::Move2: return EBIBInput::Move2;
	case EBIBSlot::Move3: return EBIBInput::Move3;
	case EBIBSlot::Special: return (Member.KitDef && Member.KitDef->bSpecialOnR) ? EBIBInput::UltimateR : EBIBInput::Special;
	case EBIBSlot::Ultimate: return Member.Kit == EBIBKitType::Judge ? EBIBInput::UltimateR : EBIBInput::UltimateE;
	default: return EBIBInput::Parry;
	}
}

bool ABIBPlayerCharacter::CanActivateKitSlot(int32 MemberIndex, EBIBSlot Slot, FString* OutReason) const
{
	if (MemberIndex < 0 || MemberIndex > 2 || MemberIndex != ActiveIndex || !bCombatActive || !Members[MemberIndex].bAlive)
	{
		return false;
	}
	const FGameplayTag& Cooldown = Members[MemberIndex].CooldownTags[static_cast<int32>(Slot)];
	if (AbilitySystem->IsOnCooldown(Cooldown))
	{
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("%s not ready (%.1fs)."), *AbilityLabel(MemberIndex, Slot), AbilitySystem->GetCooldownRemaining(Cooldown));
		}
		return false;
	}
	return true;
}

bool ABIBPlayerCharacter::TryActivateSlot(EBIBSlot Slot)
{
	FString Reason;
	const EBIBInput Input = Slot == EBIBSlot::Dodge ? EBIBInput::Parry : InputForSlot(Slot);
	if (!CanActivateKitSlot(ActiveIndex, Slot, &Reason))
	{
		// Only a fresh press explains why not (a buffered press retries quietly until it can fire).
		const ABIBPlayerController* PC = GetPC();
		if (!Reason.IsEmpty() && Slot != EBIBSlot::Punch && Slot != EBIBSlot::Dodge && Slot != EBIBSlot::Parry && PC && PC->WasInputPressed(Input))
		{
			Message(Reason);
		}
		return false;
	}
	const FGameplayAbilitySpecHandle Handle = Active().Abilities[static_cast<int32>(Slot)];
	bSlotPerformed = false;
	const bool bActivated = Handle.IsValid() && AbilitySystem->TryActivateAbility(Handle);
	if (bActivated && !bSlotPerformed)
	{
		// The kit refused (not enough ultimate, wrong Judge mode, ...): the press is spent, nothing else happens.
		ConsumeBuffer(Input);
	}
	else if (bActivated)
	{
		ConsumeBuffer(Input);
		ObserveSlot(Slot);
		FeelOnCast(Slot);   // anticipation, ultimate beat, sound hook; presentation only (BIBPlayerFeelFx.cpp)
		// Data kits' basic attack (2026-10-13): a kit with its own multi-hit string keeps it (its numbers, its finisher on its last
		// hit - BIBRules/BIBKitRuntime.h FHitInfo); a kit whose basic attack is one hit swings the engine's 4-step string.
		bool bFirstOfString = true;
		if (Slot == EBIBSlot::Punch && Active().Kit == EBIBKitType::Data && Active().Runtime)
		{
			const BIB::Kits::FRuntime& Runtime = *Active().Runtime;
			const int32 KitLength = Runtime.BasicComboLength();
			int32 KitIndex = 0;
			bool bFinisher = false;
			if (KitLength > 1)
			{
				ComboStepNow = -1;
				Combo.Reset();
				KitIndex = Runtime.BasicComboIndex();
				bFinisher = KitIndex == KitLength - 1;
				bFirstOfString = KitIndex == 0;
				PunchHitsUntil = Now() + 0.35f;
			}
			else
			{
				ComboStepNow = Combo.Advance(Now(), BIB::Feel::Tuning().ComboWindow);
				bFinisher = Combo.IsFinisher();
				bFirstOfString = ComboStepNow == 0;
				PunchHitsUntil = Now() + 0.35f;
			}
			float ImpactAt = 0.f;
			float Recovery = 0.f;
			KitPoseTiming(Runtime.Ability(BIB::Kits::ESlotId::Basic), KitIndex, ImpactAt, Recovery);
			StartPose(ImpactAt, Recovery, bFinisher ? 1.15f : 0.75f);
			ABIBFx::SlashStreak(GetWorld(), GetActorLocation() + AimForward() * 150.0 + FVector(0.0, 0.0, 30.0), AimForward(), Hud.Colour,
				bFinisher ? 420.f : 300.f, bFinisher ? 3 : (KitLength > 1 ? KitIndex : ComboStepNow));
		}
		else if (Slot != EBIBSlot::Punch && Slot != EBIBSlot::Dodge && Slot != EBIBSlot::Parry)
		{
			// Wind-up -> impact -> recovery pose for every cast (data kits from their timeline; the hand-written kits set their
			// own in their activation and may override this one).
			float ImpactAt = 0.12f;
			float Recovery = 0.25f;
			const bool bData = Active().Kit == EBIBKitType::Data && Active().Runtime;
			if (bData)
			{
				KitPoseTiming(Active().Runtime->Ability(ToKitSlot(Slot)), 0, ImpactAt, Recovery);
			}
			if (bData || PoseTime != 0.f)   // a hand-written kit that started its own pose this frame keeps it
			{
				StartPose(ImpactAt, Recovery, Slot == EBIBSlot::Ultimate ? 1.4f : (Slot == EBIBSlot::Special ? 1.2f : 1.f));
			}
		}
		// The pack's ability VFX for this operator and slot (sheets 12-18), pinned to the caster or the target angel.
		if (Slot != EBIBSlot::Dodge && Slot != EBIBSlot::Parry && (Slot != EBIBSlot::Punch || bFirstOfString || Active().Kit != EBIBKitType::Data))
		{
			static const TCHAR* const Keys[] = {TEXT("M1"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("S"), TEXT("U")};
			FString Key = Keys[FMath::Min(static_cast<int32>(Slot), 5)];
			if (Active().Kit == EBIBKitType::Narrator && NarratorData.bDiano && Slot >= EBIBSlot::Move1 && Slot <= EBIBSlot::Move3)
			{
				Key = FString::Printf(TEXT("D%d"), static_cast<int32>(Slot));   // the Diano form's three moves
			}
			BIBMotionEvents::Ability(this, Active().Name, Key, NearestEnemy(1600.f));
		}
		if (Slot == EBIBSlot::Ultimate)
		{
			// Every ultimate opens with a short, skippable cinematic beat (BIBPlayerFeel.cpp).
			BeginUltCinematic(AbilityLabel(ActiveIndex, Slot));
		}
	}
	return bActivated;
}

bool ABIBPlayerCharacter::PerformKitSlot(int32 MemberIndex, EBIBSlot Slot)
{
	if (MemberIndex != ActiveIndex || !bCombatActive)
	{
		return false;
	}
	if (Slot == EBIBSlot::Dodge)
	{
		bSlotPerformed = DodgeActivate();
		return bSlotPerformed;
	}
	if (Slot == EBIBSlot::Parry)
	{
		bSlotPerformed = ParryActivate();
		return bSlotPerformed;
	}
	switch (Active().Kit)
	{
	case EBIBKitType::Luna: bSlotPerformed = Slot == EBIBSlot::Punch ? DoPunch() : LunaActivate(Slot); break;
	case EBIBKitType::Judge: bSlotPerformed = Slot == EBIBSlot::Punch ? DoPunch() : JudgeActivate(Slot); break;
	case EBIBKitType::Narrator: bSlotPerformed = Slot == EBIBSlot::Punch ? DoPunch() : NarratorActivate(Slot); break;
	case EBIBKitType::Data: bSlotPerformed = DataKitActivate(Slot); break;
	default: bSlotPerformed = Slot == EBIBSlot::Punch ? DoPunch() : false; break;
	}
	return bSlotPerformed;
}

BIB::Kits::FRuntime* ABIBPlayerCharacter::GetActiveRuntime() const
{
	return Active().Runtime.Get();
}

void ABIBPlayerCharacter::DebugSelectUltimate(int32 Variant)
{
	if (BIB::Kits::FRuntime* Runtime = GetActiveRuntime())
	{
		Runtime->SelectUltimateVariant(Variant);
		Message(FString::Printf(TEXT("%s ultimate: %s"), *Active().Name, UTF8_TO_TCHAR(Runtime->Ability(BIB::Kits::ESlotId::Ultimate).Name.c_str())));
	}
	else
	{
		Message(TEXT("The active operator has no alternative ultimate."));
	}
}

FString ABIBPlayerCharacter::DescribeActiveTags() const
{
	FGameplayTagContainer Tags;
	AbilitySystem->GetOwnedGameplayTags(Tags);
	return Tags.ToStringSimple();
}

// ============================================================================ attribute-backed health and ultimate

void ABIBPlayerCharacter::SyncActiveFromAttributes()
{
	FBIBMember& Member = Active();
	Member.HP = AbilitySystem->GetHealthValue();
	Member.MaxHP = AbilitySystem->GetMaxHealthValue();
	Member.Ult = AbilitySystem->GetUltimateValue();
}

void ABIBPlayerCharacter::LoadMemberIntoAttributes(int32 Index)
{
	const FBIBMember& Member = Members[Index];
	AbilitySystem->SetMaxHealthValue(FMath::Max(1.f, Member.MaxHP));
	AbilitySystem->SetHealthValue(FMath::Clamp(Member.HP, 0.f, FMath::Max(1.f, Member.MaxHP)));
	AbilitySystem->SetUltimateValue(Member.Ult);
	SyncActiveFromAttributes();
}

void ABIBPlayerCharacter::SetMemberHP(int32 Index, float Value)
{
	FBIBMember& Member = Members[Index];
	const float Clamped = FMath::Clamp(Value, 0.f, FMath::Max(1.f, Member.MaxHP));
	if (Index == ActiveIndex)
	{
		AbilitySystem->SetHealthValue(Clamped);
		SyncActiveFromAttributes();
	}
	else
	{
		Member.HP = Clamped;
	}
}

void ABIBPlayerCharacter::SetMemberMaxHP(int32 Index, float Value)
{
	FBIBMember& Member = Members[Index];
	const float Max = FMath::Max(1.f, Value);
	if (Index == ActiveIndex)
	{
		AbilitySystem->SetMaxHealthValue(Max);
		SyncActiveFromAttributes();
	}
	else
	{
		Member.MaxHP = Max;
		Member.HP = FMath::Min(Member.HP, Max);
	}
}

void ABIBPlayerCharacter::SetMemberUlt(int32 Index, float Value)
{
	FBIBMember& Member = Members[Index];
	const float Clamped = FMath::Clamp(Value, 0.f, 100.f);
	if (Index == ActiveIndex)
	{
		AbilitySystem->SetUltimateValue(Clamped);
		SyncActiveFromAttributes();
	}
	else
	{
		Member.Ult = Clamped;
	}
}

void ABIBPlayerCharacter::HealMember(int32 Index, float Amount)
{
	if (Amount <= 0.f || !Members[Index].bAlive)
	{
		return;
	}
	if (Index == ActiveIndex)
	{
		AbilitySystem->ApplyHealing(Amount);
		SyncActiveFromAttributes();
	}
	else
	{
		Members[Index].HP = FMath::Min(Members[Index].MaxHP, Members[Index].HP + Amount);
	}
}

void ABIBPlayerCharacter::HealTeamFraction(float Fraction)
{
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Members)); ++Index)
	{
		HealMember(Index, Members[Index].MaxHP * FMath::Clamp(Fraction, 0.f, 1.f));
	}
}

void ABIBPlayerCharacter::ApplyKitHealthScale(int32 Index, float Mult, bool bFill)
{
	FBIBMember& Member = Members[Index];
	const float Ratio = Member.MaxHP > 0.f ? Member.HP / Member.MaxHP : 1.f;
	const float NewMax = FMath::Max(1.f, Member.BaseMaxHP * Mult);
	SetMemberMaxHP(Index, NewMax);
	SetMemberHP(Index, bFill ? NewMax : FMath::Max(1.f, NewMax * Ratio));
}

// ============================================================================ mirrored attributes and tags

void ABIBPlayerCharacter::MirrorKitAttributes()
{
	float Shield = 0.f;
	float ResourceA = 0.f;
	float ResourceB = 0.f;
	float DealtMult = 1.f;
	float TakenMult = 1.f;
	float MoveMult = 1.f;
	float AttackMult = 1.f;
	const FBIBMember& Member = Active();
	if (const BIB::Kits::FRuntime* Runtime = Member.Runtime.Get())
	{
		Shield = static_cast<float>(Runtime->Shield());
		ResourceA = static_cast<float>(Runtime->Resource(0));
		ResourceB = static_cast<float>(Runtime->Resource(1));
		DealtMult = static_cast<float>(Runtime->DamageDealtMult());
		TakenMult = static_cast<float>(Runtime->DamageTakenMult());
		MoveMult = static_cast<float>(Runtime->MoveSpeedMult());
		AttackMult = static_cast<float>(Runtime->AttackSpeedMult());
	}
	else if (Member.Kit == EBIBKitType::Narrator)
	{
		ResourceA = NarratorData.Speed;
		DealtMult = NarratorData.DamageX;
	}
	else if (Member.Kit == EBIBKitType::Judge)
	{
		ResourceA = static_cast<float>(JudgeData.RestrainingOrders);
	}
	auto Mirror = [this](const FGameplayAttribute& Attribute, float Value)
	{
		if (!FMath::IsNearlyEqual(AbilitySystem->GetNumericAttribute(Attribute), Value, 0.01f))
		{
			AbilitySystem->SetNumericAttributeBase(Attribute, Value);
		}
	};
	Mirror(UBIBAttributeSet::GetShieldAttribute(), Shield);
	Mirror(UBIBAttributeSet::GetResourceAAttribute(), ResourceA);
	Mirror(UBIBAttributeSet::GetResourceBAttribute(), ResourceB);
	Mirror(UBIBAttributeSet::GetDamageDealtMultAttribute(), DealtMult);
	Mirror(UBIBAttributeSet::GetDamageTakenMultAttribute(), TakenMult);
	Mirror(UBIBAttributeSet::GetMoveSpeedMultAttribute(), MoveMult);
	Mirror(UBIBAttributeSet::GetAttackSpeedMultAttribute(), AttackMult);
}

void ABIBPlayerCharacter::UpdateStateTags()
{
	TArray<FGameplayTag> Desired;
	auto Add = [&Desired](const char* TagName)
	{
		const FGameplayTag Tag = BIBTags::Find(TagName);
		if (Tag.IsValid())
		{
			Desired.Add(Tag);
		}
	};
	const FBIBMember& Member = Active();
	Add(bCombatActive && Member.bAlive ? "BIB.State.Alive" : "BIB.State.Dead");
	if (bCombatActive)
	{
		Add("BIB.State.Active");
	}
	if (bBlocking)
	{
		Add("BIB.State.Blocking");
	}
	if (Action == EBIBAction::Dodge)
	{
		Add("BIB.State.Dodging");
	}
	if (Action != EBIBAction::None || (Member.Runtime && Member.Runtime->IsCasting()))
	{
		Add("BIB.State.Casting");
	}
	if (IsMovementLocked())
	{
		Add("BIB.State.MovementLocked");
	}
	const float T = Now();
	switch (Member.Kit)
	{
	case EBIBKitType::Luna:
		if (Action == EBIBAction::LunaBlackhole || LunaData.OrbPhase != 0) { Add("BIB.State.Luna.Blackhole"); }
		if (T < LunaData.BlackUntil) { Add("BIB.State.Luna.OrbitingBlack"); }
		if (T < LunaData.WhiteUntil) { Add("BIB.State.Luna.OrbitingWhite"); }
		if (Action == EBIBAction::LunaEventHorizon) { Add("BIB.State.Luna.EventHorizon"); }
		if (Action == EBIBAction::LunaExpel) { Add("BIB.State.Luna.Expel"); }
		if (LunaData.DestabiliseStart >= 0.f) { Add("BIB.State.Luna.Destabilisation"); }
		if (LunaData.bGravityInverted) { Add("BIB.State.Luna.GravityInverted"); }
		break;
	case EBIBKitType::Judge:
		switch (JudgeData.Mode)
		{
		case EBIBJudgeMode::Witness: Add("BIB.State.Judge.Witness"); break;
		case EBIBJudgeMode::Lawyer: Add("BIB.State.Judge.Lawyer"); break;
		case EBIBJudgeMode::Judge: Add("BIB.State.Judge.Judge"); break;
		}
		if (JudgeData.bMenuOpen || Action == EBIBAction::JudgeCourtWindup) { Add("BIB.State.Judge.CourtSetting"); }
		if (T < JudgeData.CourtLockedUntil) { Add("BIB.State.Judge.Contempt"); }
		if (Action == EBIBAction::JudgeJustice) { Add("BIB.State.Judge.Justice"); }
		if (T < JudgeData.RegAttackBuffUntil) { Add("BIB.State.Judge.RegAttackGranted"); }
		break;
	case EBIBKitType::Narrator:
		if (NarratorData.bDiano) { Add("BIB.State.Narrator.Diano"); }
		if (NarratorData.bExchangeOpen) { Add("BIB.State.Narrator.ExchangeOpen"); }
		if (T < NarratorData.ParryUntil) { Add("BIB.State.Narrator.StillAlive"); }
		if (NarratorData.CloseEnoughAt > 0.f) { Add("BIB.State.Narrator.CloseEnough"); }
		if (Action == EBIBAction::NarratorSwitch) { Add("BIB.State.Narrator.Switching"); }
		if (Action == EBIBAction::DianoEndlessGod) { Add("BIB.State.Narrator.EndlessGod"); }
		if (NarratorData.bDiano && T < NarratorData.DianoBlockUntil) { Add("BIB.State.Narrator.SwitchBlock"); }
		break;
	case EBIBKitType::Data:
		if (Member.Runtime)
		{
			for (const std::string& State : Member.Runtime->ActiveStateTags())
			{
				Add(State == "Shielded" ? "BIB.State.Shielded" : ("BIB.State.Kit." + State).c_str());
			}
			// Timed buffs (Overdrive, Last Bastion, Immovable, ...) as BIB.Buff.<Stat>, so tools and cues can see them.
			for (int32 BuffIndex = 0; BuffIndex < BIB::Kits::BuffCount; ++BuffIndex)
			{
				const BIB::Kits::EBuff Buff = static_cast<BIB::Kits::EBuff>(BuffIndex);
				if (Member.Runtime->HasBuffActive(Buff))
				{
					Add(BIB::Kits::BuffTag(Buff));
				}
			}
		}
		break;
	default:
		break;
	}
	AbilitySystem->SyncMirroredTags(TEXT("State"), Desired);
}
