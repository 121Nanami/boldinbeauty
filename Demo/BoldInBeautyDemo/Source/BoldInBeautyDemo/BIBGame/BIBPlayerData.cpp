// The 17 data-driven operators (BIBRules/BIBKitData*.cpp): input, activation, hit handling, HUD and movement. The kit
// logic itself lives in BIB::Kits::FRuntime; this file connects it to keys, the Gameplay Ability System and the HUD.
#include "BIBGame/BIBPlayerCharacter.h"

#include "BIBGAS/BIBAbilitySystemComponent.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerController.h"
#include "BIBGame/BIBTuning.h"

#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
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

	FString Text(const std::string& Value)
	{
		return FString(UTF8_TO_TCHAR(Value.c_str()));
	}
}

// ============================================================================ set-up

void ABIBPlayerCharacter::SetupDataKit(FBIBMember& Member, int32 Index)
{
	Member.KitWorld = MakeShared<FBIBKitWorld>(this, Index);
	Member.Runtime = MakeShared<BIB::Kits::FRuntime>(*Member.KitDef, *Member.KitWorld);
	Member.Runtime->ResetForFight();
}

void ABIBPlayerCharacter::ShareTeammatePassives()
{
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Members[Index].Runtime)
		{
			continue;
		}
		std::vector<const BIB::Kits::FPassiveDef*> Passives;
		for (int32 Other = 0; Other < 3; ++Other)
		{
			if (Other == Index || !Members[Other].KitDef)
			{
				continue;
			}
			for (const BIB::Kits::FPassiveDef& Def : Members[Other].KitDef->Passives)
			{
				Passives.push_back(&Def);
			}
		}
		Members[Index].Runtime->SetTeammatePassives(Passives);
	}
}

void ABIBPlayerCharacter::DataKitSwitchedOut(int32 Index)
{
	FBIBMember& Member = Members[Index];
	for (bool& bHeld : bKitHeld)
	{
		bHeld = false;
	}
	KitMoveLeft = 0.f;
	if (Member.Runtime)
	{
		Member.Runtime->CancelAll();
		Member.Runtime->ClearBattlefield(true);   // her infections keep running while she is benched
	}
}

void ABIBPlayerCharacter::DataKitSwitchedIn(int32 Index)
{
	if (Members[Index].Runtime)
	{
		Members[Index].Runtime->OnSwitchIn();
	}
}

// ============================================================================ input and activation

void ABIBPlayerCharacter::DataKitInput()
{
	FBIBMember& Member = Active();
	BIB::Kits::FRuntime* Runtime = Member.Runtime.Get();
	if (!Runtime || !Member.KitDef)
	{
		return;
	}
	static const EBIBSlot KeySlots[] = {EBIBSlot::Move1, EBIBSlot::Move2, EBIBSlot::Move3, EBIBSlot::Special, EBIBSlot::Ultimate};

	// Hold abilities (Arita's Spring time) keep running until the key is released.
	for (const EBIBSlot Slot : KeySlots)
	{
		const int32 Number = static_cast<int32>(Slot);
		const bool bDown = Held(InputForSlot(Slot));
		if (bKitHeld[Number] && !bDown && Runtime->Ability(ToKitSlot(Slot)).Cast == BIB::Kits::ECast::Hold)
		{
			Runtime->Release(ToKitSlot(Slot));
		}
		bKitHeld[Number] = bDown;
	}

	// R cycles the alternative ultimate of operators that have one (Alchemist), unless R is the Special key (Arita).
	if (!Member.KitDef->bSpecialOnR && !Member.KitDef->UltimateVariants.empty() && Pressed(EBIBInput::UltimateR))
	{
		const int32 Count = static_cast<int32>(Member.KitDef->UltimateVariants.size()) + 1;
		Runtime->SelectUltimateVariant((Runtime->UltimateVariant() + 1) % Count);
		Message(FString::Printf(TEXT("%s ultimate: %s (R changes it)"), *Member.Name, *Text(Runtime->Ability(BIB::Kits::ESlotId::Ultimate).Name)));
		return;
	}
	for (const EBIBSlot Slot : KeySlots)
	{
		if (Pressed(InputForSlot(Slot)))
		{
			TryActivateSlot(Slot);
			return;
		}
	}
}

bool ABIBPlayerCharacter::DataKitActivate(EBIBSlot Slot)
{
	FBIBMember& Member = Active();
	BIB::Kits::FRuntime* Runtime = Member.Runtime.Get();
	if (!Runtime)
	{
		return false;
	}
	const BIB::Kits::ESlotId Id = ToKitSlot(Slot);
	const FString Name = Text(Runtime->Ability(Id).Name);
	if (Slot == EBIBSlot::Ultimate && Member.Ult < 99.99f)
	{
		Message(FString::Printf(TEXT("%s needs 100%% ult (%d%%)."), *Name, FMath::FloorToInt(Member.Ult)));
		return false;
	}
	FaceAim();
	const BIB::Kits::FCastResult Result = Runtime->Cast(Id);
	if (!Result.bOk)
	{
		const float T = Now();
		if (Slot != EBIBSlot::Punch && T - LastKitMessageAt > 0.4f)
		{
			LastKitMessageAt = T;
			Message(FString::Printf(TEXT("%s: %s."), *Name, *Text(Result.Reason)));
		}
		return false;
	}
	if (Slot == EBIBSlot::Ultimate)
	{
		SetMemberUlt(ActiveIndex, 0.f);
		Message(FString::Printf(TEXT("%s: %s!"), *Member.Name, *Name));
	}
	return true;
}

void ABIBPlayerCharacter::DataKitTick(float DeltaSeconds)
{
	// Benched operators: what they left on the enemies keeps running (2026-10-09; Alchemist infects, mutates and swaps out).
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Members); ++Index)
	{
		if (Index != ActiveIndex && Members[Index].Runtime)
		{
			Members[Index].Runtime->TickBackground(DeltaSeconds);
		}
	}
	FBIBMember& Member = Active();
	if (!Member.Runtime || !Member.bAlive)
	{
		PendingKills.Reset();
		return;
	}
	TickKitMove(DeltaSeconds);
	for (const int32 EnemyId : PendingKills)
	{
		Member.Runtime->NotifyEnemyKilled(EnemyId);
	}
	PendingKills.Reset();
	Member.Runtime->Tick(DeltaSeconds);
}

// ============================================================================ movement requested by the runtime

void ABIBPlayerCharacter::BeginKitMove(const FVector& To, float Seconds)
{
	if (Seconds <= 0.f)
	{
		SetActorLocation(To, false, nullptr, ETeleportType::TeleportPhysics);
		KitMoveLeft = 0.f;
		return;
	}
	GetCharacterMovement()->StopMovementImmediately();
	KitMoveFrom = GetActorLocation();
	KitMoveTo = To;
	KitMoveTotal = Seconds;
	KitMoveLeft = Seconds;
}

void ABIBPlayerCharacter::TickKitMove(float DeltaSeconds)
{
	if (KitMoveLeft <= 0.f)
	{
		return;
	}
	KitMoveLeft = FMath::Max(0.f, KitMoveLeft - DeltaSeconds);
	const float Alpha = KitMoveTotal > 0.f ? 1.f - KitMoveLeft / KitMoveTotal : 1.f;
	FHitResult Hit;
	SetActorLocation(FMath::Lerp(KitMoveFrom, KitMoveTo, Alpha), true, &Hit);
	if (Hit.bBlockingHit)
	{
		KitMoveLeft = 0.f; // a wall ends the dash
	}
}

// ============================================================================ damage in and out

float ABIBPlayerCharacter::DataKitReceiveHit(float Amount, ABIBEnemy* Attacker, int32 EnemyAbilityId, bool bMelee, bool& bNegated)
{
	BIB::Kits::FRuntime* Runtime = Active().Runtime.Get();
	bNegated = false;
	if (!Runtime)
	{
		return Amount;
	}
	if (EnemyAbilityId > 0)
	{
		Runtime->NotifyEnemyAbilityHit(EnemyAbilityId, Amount);
	}
	const int AttackerId = Attacker ? static_cast<int>(Attacker->GetUniqueID()) : -1;
	const double Final = Runtime->OnIncomingDamage(Amount, AttackerId, bMelee);
	bNegated = Final <= 0.0;
	return static_cast<float>(Final);
}

void ABIBPlayerCharacter::OnKitStatusDamage(ABIBEnemy* Enemy, double Amount, int32 SourceMember)
{
	if (!Enemy || !Enemy->IsAlive() || Amount <= 0.0)
	{
		return;
	}
	const float Scaled = static_cast<float>(Amount * Enemy->GetStatusBook().DamageTakenMult());
	const float Dealt = Enemy->ReceiveDamage(Scaled, this);
	if (Dealt <= 0.f)
	{
		return;
	}
	const int32 Source = (SourceMember >= 0 && SourceMember < 3) ? SourceMember : ActiveIndex;
	if (Source == ActiveIndex && Members[Source].Runtime)
	{
		Members[Source].Runtime->NotifyDamageDealt(static_cast<int>(Enemy->GetUniqueID()), Dealt, false);
	}
	else
	{
		GainUlt(Source, Dealt);
	}
}

void ABIBPlayerCharacter::OnEnemyDied(ABIBEnemy* Enemy)
{
	if (Enemy)
	{
		PendingKills.Add(static_cast<int32>(Enemy->GetUniqueID()));
	}
}

// ============================================================================ HUD

void ABIBPlayerCharacter::DataKitBuildHud(const FBIBMember& Member)
{
	const BIB::Kits::FRuntime* Runtime = Member.Runtime.Get();
	if (!Runtime || !Member.KitDef)
	{
		return;
	}
	const BIB::Kits::FKitDef& Kit = *Member.KitDef;
	Hud.Colour = FLinearColor(Kit.Colour[0] + 0.25f, Kit.Colour[1] + 0.25f, Kit.Colour[2] + 0.25f);
	const FString UltNote = Member.Ult >= 99.99f ? FString(TEXT("READY")) : FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Member.Ult));
	for (int32 Number = 0; Number < BIB::Kits::SlotCount; ++Number)
	{
		const BIB::Kits::ESlotId Id = static_cast<BIB::Kits::ESlotId>(Number);
		const BIB::Kits::FAbilityDef& Def = Runtime->Ability(Id);
		if (Def.Id.empty())
		{
			continue;
		}
		const EBIBSlot Slot = Id == BIB::Kits::ESlotId::Basic ? EBIBSlot::Punch : static_cast<EBIBSlot>(Number);
		FString Key = Text(BIB::Kits::SlotKeyLabel(Id));
		if (Id == BIB::Kits::ESlotId::Special && Kit.bSpecialOnR)
		{
			Key = TEXT("R");
		}
		FString Name = Text(Def.Name);
		if (Def.Cast == BIB::Kits::ECast::Hold)
		{
			Name += TEXT(" (hold)");
		}
		else if (Def.Cast == BIB::Kits::ECast::Toggle)
		{
			Name += TEXT(" (toggle)");
		}
		if (Id == BIB::Kits::ESlotId::Basic)
		{
			Name = FString::Printf(TEXT("Reg attack: %s"), *Name);
		}
		else if (Id == BIB::Kits::ESlotId::Special)
		{
			Name = FString::Printf(TEXT("Special: %s"), *Name);
		}
		else if (Id == BIB::Kits::ESlotId::Ultimate)
		{
			Name = FString::Printf(TEXT("Ult: %s"), *Name);
		}
		FString Note = Text(Runtime->AbilityNote(Id));
		if (Id == BIB::Kits::ESlotId::Ultimate)
		{
			Note = Note.IsEmpty() ? UltNote : FString::Printf(TEXT("%s (%s)"), *UltNote, *Note);
		}
		AddAbilityHud(Key, Name, Slot, Note);
	}
	if (!Kit.UltimateVariants.empty() && !Kit.bSpecialOnR)
	{
		FBIBAbilityHud Entry;
		Entry.Key = TEXT("R");
		Entry.Name = FString::Printf(TEXT("Change ultimate (%d/%d)"), Runtime->UltimateVariant() + 1, static_cast<int32>(Kit.UltimateVariants.size()) + 1);
		Hud.Abilities.Add(Entry);
	}
	Hud.KitLine = Text(Runtime->KitLine());
	if (Hud.KitLine.IsEmpty())
	{
		Hud.KitLine = Text(Kit.Playstyle);
	}
	if (Runtime->IsCasting())
	{
		Hud.ActionLine = TEXT("Casting...");
	}
}
