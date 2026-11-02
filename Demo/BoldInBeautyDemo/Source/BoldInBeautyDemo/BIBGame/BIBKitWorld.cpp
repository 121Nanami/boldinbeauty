#include "BIBGame/BIBKitWorld.h"

#include "BIBGAS/BIBAbilitySystemComponent.h"
#include "BIBGAS/BIBGameplayTags.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"

#include "BIBGame/BIBFx.h"
#include "Engine/World.h"

using namespace BIB::Kits;

namespace
{
	FVec ToKit(const FVector& V)
	{
		return FVec{V.X, V.Y, V.Z};
	}

	FVector ToWorld(const FVec& V)
	{
		return FVector(V.X, V.Y, V.Z);
	}

	int32 EnemyId(const ABIBEnemy* Enemy)
	{
		return static_cast<int32>(Enemy->GetUniqueID());
	}

	EBIBSlot ToSlot(ESlotId Slot)
	{
		switch (Slot)
		{
		case ESlotId::Basic: return EBIBSlot::Punch;
		case ESlotId::Move1: return EBIBSlot::Move1;
		case ESlotId::Move2: return EBIBSlot::Move2;
		case ESlotId::Move3: return EBIBSlot::Move3;
		case ESlotId::Special: return EBIBSlot::Special;
		default: return EBIBSlot::Ultimate;
		}
	}

	// Placeholder colour per cue so the owner can see every effect in the demo before real VFX exist.
	FColor CueColour(int CueId)
	{
		switch (CueId)
		{
		case CueSlash: return FColor(230, 230, 240);
		case CueSlam: return FColor(200, 140, 60);
		case CueBurst: return FColor(255, 170, 60);
		case CueNova: return FColor(255, 255, 160);
		case CueRing: return FColor(120, 220, 255);
		case CueBeam: return FColor(120, 200, 255);
		case CueDash: return FColor(170, 170, 255);
		case CueBlink: return FColor(180, 120, 255);
		case CueHeal: return FColor(80, 255, 120);
		case CueShield: return FColor(80, 160, 255);
		case CueBuff: return FColor(255, 220, 80);
		case CueDebuff: return FColor(160, 80, 200);
		case CueMark: return FColor(255, 60, 120);
		case CueExecute: return FColor(255, 0, 0);
		case CueZone: return FColor(120, 255, 200);
		case CueMeteor: return FColor(255, 100, 20);
		case CueLightning: return FColor(250, 250, 90);
		case CueExplosion: return FColor(255, 120, 40);
		case CueFlame: return FColor(255, 90, 10);
		case CueFrost: return FColor(150, 230, 255);
		case CueBlood: return FColor(170, 10, 30);
		case CuePoison: return FColor(110, 210, 60);
		case CueNature: return FColor(90, 200, 90);
		case CueTime: return FColor(200, 160, 255);
		case CueStar: return FColor(255, 240, 150);
		case CueUltimate: return FColor(255, 255, 255);
		case CueGlitch: return FColor(0, 255, 220);
		case CueSplash: return FColor(80, 150, 255);
		case CueWind: return FColor(200, 255, 230);
		default: return FColor(220, 220, 220);
		}
	}
}

FBIBKitWorld::FBIBKitWorld(ABIBPlayerCharacter* InOwner, int32 InMemberIndex)
	: Owner(InOwner)
	, MemberIndex(InMemberIndex)
{
}

ABIBEnemy* FBIBKitWorld::FindEnemy(int Id) const
{
	if (const ABIBGameMode* GameMode = Owner->GetMode())
	{
		for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
		{
			if (EnemyId(Enemy) == Id)
			{
				return Enemy;
			}
		}
	}
	return nullptr;
}

double FBIBKitWorld::Now() const { return Owner->Now(); }
FVec FBIBKitWorld::CasterPos() const { return ToKit(Owner->GetActorLocation()); }
FVec FBIBKitWorld::CasterAim() const { return ToKit(Owner->AimForward()); }
double FBIBKitWorld::CasterAttack() const { return Owner->Members[MemberIndex].BaseAttack; }
double FBIBKitWorld::CasterDamageMult() const { return Owner->Members[MemberIndex].DamageMult; }
double FBIBKitWorld::CasterHP() const { return Owner->Members[MemberIndex].HP; }
double FBIBKitWorld::CasterMaxHP() const { return Owner->Members[MemberIndex].MaxHP; }

void FBIBKitWorld::CasterMove(const FVec& To, double Seconds)
{
	Owner->BeginKitMove(ToWorld(To), static_cast<float>(Seconds));
}

void FBIBKitWorld::CasterInvulnerable(double Seconds)
{
	Owner->GrantInvulnerable(static_cast<float>(Seconds));
}

void FBIBKitWorld::CasterHeal(double Amount)
{
	Owner->HealMember(MemberIndex, static_cast<float>(Amount));
}

void FBIBKitWorld::CasterSelfDamage(double Amount)
{
	Owner->TakeSelfDamage(static_cast<float>(Amount));
}

void FBIBKitWorld::CasterLock(double Seconds)
{
	Owner->KitLockedUntil = FMath::Max(Owner->KitLockedUntil, Owner->Now() + static_cast<float>(Seconds));
}

void FBIBKitWorld::CasterHealthScale(double Mult, bool bFill)
{
	Owner->ApplyKitHealthScale(MemberIndex, static_cast<float>(Mult), bFill);
}

void FBIBKitWorld::GetEnemies(std::vector<FEnemyView>& Out) const
{
	const ABIBGameMode* GameMode = Owner->GetMode();
	if (!GameMode)
	{
		return;
	}
	for (ABIBEnemy* Enemy : GameMode->GetLiveEnemies())
	{
		FEnemyView View;
		View.Id = EnemyId(Enemy);
		View.Pos = ToKit(Enemy->GetActorLocation());
		View.Radius = Enemy->GetBodyRadius();
		View.HP = Enemy->GetHP();
		View.MaxHP = Enemy->GetMaxHP();
		View.bBoss = Enemy->IsBoss();
		Out.push_back(View);
	}
}

bool FBIBKitWorld::EnemyAlive(int Id) const
{
	const ABIBEnemy* Enemy = FindEnemy(Id);
	return Enemy && Enemy->IsAlive();
}

double FBIBKitWorld::DamageEnemy(int Id, double Amount, unsigned Flags)
{
	ABIBEnemy* Enemy = FindEnemy(Id);
	if (!Enemy || Amount <= 0.0)
	{
		return 0.0;
	}
	const double Scaled = (Flags & DmgTrue) ? Amount : Amount * Enemy->GetStatusBook().DamageTakenMult();
	return Enemy->ReceiveDamage(static_cast<float>(Scaled), Owner);
}

BIB::Feel::FHitSource FBIBKitWorld::FeelSource(const FHitInfo& Info, unsigned Flags) const
{
	BIB::Feel::FHitSource Hit;
	switch (Info.Slot)
	{
	case ESlotId::Basic: Hit.Source = BIB::Feel::ESource::Basic; break;
	case ESlotId::Move1:
	case ESlotId::Move2:
	case ESlotId::Move3: Hit.Source = BIB::Feel::ESource::Move; break;
	case ESlotId::Special: Hit.Source = BIB::Feel::ESource::Special; break;
	case ESlotId::Ultimate: Hit.Source = BIB::Feel::ESource::Ultimate; break;
	default: Hit.Source = BIB::Feel::ESource::Unknown; break;
	}
	Hit.bTick = Info.bTick || (Flags & DmgDot) != 0;
	// A single-hit basic attack swings the engine's 4-step string: its finisher is the engine string's last step.
	Hit.bFinal = Info.Slot == ESlotId::Basic && Info.ComboLength <= 1 ? Owner->IsEngineComboFinisher() : Info.bFinal;
	Hit.Override = Info.FeelWeight;
	// A teammate's lingering effect (benched operator) is not the active operator's cast: classified by size.
	if (MemberIndex != Owner->ActiveIndex)
	{
		Hit.Source = BIB::Feel::ESource::Unknown;
		Hit.bFinal = false;
	}
	return Hit;
}

double FBIBKitWorld::DamageEnemyHit(int Id, double Amount, unsigned Flags, const FHitInfo& Info)
{
	const FBIBScopedHit Scope(Owner, FeelSource(Info, Flags));
	return DamageEnemy(Id, Amount, Flags);
}

void FBIBKitWorld::KillEnemyHit(int Id, bool bRespectBoss, double BossFraction, const FHitInfo& Info)
{
	const FBIBScopedHit Scope(Owner, FeelSource(Info, DmgNone));
	KillEnemy(Id, bRespectBoss, BossFraction);
}

void FBIBKitWorld::KillEnemy(int Id, bool bRespectBoss, double BossFraction)
{
	ABIBEnemy* Enemy = FindEnemy(Id);
	if (!Enemy)
	{
		return;
	}
	const double Amount = (bRespectBoss && Enemy->IsBoss()) ? Enemy->GetMaxHP() * BossFraction : Enemy->GetHP();
	Enemy->ReceiveDamage(static_cast<float>(Amount), Owner);
}

void FBIBKitWorld::Impulse(int Id, const FVec& Velocity)
{
	if (ABIBEnemy* Enemy = FindEnemy(Id))
	{
		Enemy->ApplyKitImpulse(ToWorld(Velocity));
	}
}

bool FBIBKitWorld::ApplyStatus(int Id, EStatus Status, double Seconds, double Power, int Stacks)
{
	ABIBEnemy* Enemy = FindEnemy(Id);
	return Enemy && Enemy->ApplyKitStatus(Status, Seconds, Power, Stacks, MemberIndex);
}

int FBIBKitWorld::StatusStacks(int Id, EStatus Status) const
{
	const ABIBEnemy* Enemy = FindEnemy(Id);
	return Enemy ? Enemy->GetStatusBook().Stacks(Status) : 0;
}

void FBIBKitWorld::ClearStatus(int Id, EStatus Status)
{
	if (ABIBEnemy* Enemy = FindEnemy(Id))
	{
		Enemy->GetStatusBook().Clear(Status);
		Enemy->RefreshStatusTags();
	}
}

void FBIBKitWorld::RemoveStatusStacks(int Id, EStatus Status, int Count)
{
	if (ABIBEnemy* Enemy = FindEnemy(Id))
	{
		Enemy->GetStatusBook().RemoveStacks(Status, Count);
		Enemy->RefreshStatusTags();
	}
}

double FBIBKitWorld::CooldownRemaining(ESlotId Slot) const
{
	return Owner->Remaining(MemberIndex, ToSlot(Slot));
}

void FBIBKitWorld::StartCooldown(ESlotId Slot, double Seconds)
{
	Owner->StartCooldown(MemberIndex, ToSlot(Slot), static_cast<float>(Seconds));
}

void FBIBKitWorld::ReduceCooldown(ESlotId Slot, double Seconds)
{
	Owner->ReduceCooldown(MemberIndex, ToSlot(Slot), static_cast<float>(Seconds));
}

void FBIBKitWorld::AddUltimate(double DamageDealt)
{
	Owner->GainUlt(MemberIndex, static_cast<float>(DamageDealt));
}

void FBIBKitWorld::Cue(int CueId, const FVec& Pos, double Radius, double Seconds)
{
	const FVector Location = ToWorld(Pos);
	if (UBIBAbilitySystemComponent* Component = Owner->GetBIBAbilitySystem())
	{
		Component->ExecuteBIBCue(BIBTags::Cue(CueId), Location, static_cast<float>(Radius));
	}
	// Visual (2026-10-09, replaces the old wireframe debug spheres that made every kit look the same): a glowing effect shaped by
	// the cue type, in the cue's colour mixed with the operator's own (BIBGame/BIBFx.h). GameplayCue tags can still replace it.
	if (UWorld* World = Owner->GetWorld())
	{
		const float Life = static_cast<float>(FMath::Clamp(Seconds, 0.15, 3.0));
		const float Size = static_cast<float>(FMath::Clamp(Radius, 40.0, 3000.0));
		const FLinearColor Colour = FLinearColor::LerpUsingHSV(FLinearColor(CueColour(CueId)), Owner->GetHud().Colour, 0.3f);
		const FVector Ground = Location - FVector(0.0, 0.0, 85.0);
		const FVector From = Owner->GetActorLocation();
		switch (CueId)
		{
		case CueSlash:
			ABIBFx::SlashStreak(World, Location, Location - From, Colour, FMath::Max(260.f, Size * 2.f), FMath::RandRange(0, 3));
			break;
		case CueBeam:
		{
			const FVector Mid = (From + Location) * 0.5;
			const float Length = static_cast<float>(FVector::Dist(From, Location));
			ABIBFx::Spawn(World, EBIBFxShape::Slash, Mid, (Location - From).Rotation(), FVector(Length, 60.f, 60.f), FVector(Length, 8.f, 8.f), Colour, 0.35f, 9.f);
			ABIBFx::HitSpark(World, Location, Colour, 2.f);
			break;
		}
		case CueDash:
		case CueBlink:
			ABIBFx::Spawn(World, EBIBFxShape::Pillar, Location, FRotator::ZeroRotator, FVector(140.f, 140.f, 60.f), FVector(20.f, 20.f, 520.f), Colour, 0.3f, 8.f);
			break;
		case CueHeal:
		case CueShield:
		case CueBuff:
			ABIBFx::Spawn(World, EBIBFxShape::Pillar, Location, FRotator::ZeroRotator, FVector(Size * 0.6f, Size * 0.6f, 20.f), FVector(Size * 1.2f, Size * 1.2f, 420.f), Colour, 0.6f, 4.f);
			ABIBFx::ShockRing(World, Ground, Size, Colour, 0.5f);
			break;
		case CueRing:
		case CueZone:
			// A lingering zone: its edge stays on the ground for the effect's life.
			ABIBFx::Spawn(World, EBIBFxShape::Ring, Ground + FVector(0.0, 0.0, 4.0), FRotator::ZeroRotator, FVector(Size * 2.f, Size * 2.f, 3.f),
				FVector(Size * 2.f, Size * 2.f, 3.f), Colour, Life, 2.5f);
			ABIBFx::ShockRing(World, Ground, Size, Colour, 0.4f);
			break;
		case CueLightning:
			ABIBFx::Spawn(World, EBIBFxShape::Pillar, Location + FVector(0.0, 0.0, 600.0), FRotator::ZeroRotator, FVector(60.f, 60.f, 1400.f),
				FVector(8.f, 8.f, 1400.f), Colour, 0.25f, 14.f);
			ABIBFx::HitSpark(World, Location, Colour, 3.f);
			break;
		case CueMark:
		case CueDebuff:
			ABIBFx::Spawn(World, EBIBFxShape::Ring, Location + FVector(0.0, 0.0, 120.0), FRotator(0.f, 0.f, 0.f), FVector(160.f, 160.f, 4.f),
				FVector(90.f, 90.f, 4.f), Colour, FMath::Min(Life, 0.8f), 6.f);
			break;
		default:
			// Bursts, slams, novas, meteors, executes: a blast that grows to the effect's radius and a shock ring on the ground.
			ABIBFx::Spawn(World, EBIBFxShape::Sphere, Location, FRotator::ZeroRotator, FVector(Size * 0.3f), FVector(Size * 2.f), Colour, FMath::Min(Life, 0.5f), 5.f);
			ABIBFx::ShockRing(World, Ground, Size, Colour, 0.45f);
			if (Size >= 500.f)
			{
				Owner->Shake(FMath::Clamp(Size / 400.f, 1.f, 3.f), 0.22f);
			}
			break;
		}
	}
}

void FBIBKitWorld::Message(const std::string& Text)
{
	Owner->Message(FString(UTF8_TO_TCHAR(Text.c_str())));
}

double FBIBKitWorld::Random01()
{
	return FMath::FRand();
}
