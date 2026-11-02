#include "BIBGame/BIBTacticsLibrary.h"
#include "BIBGame/BIBEnemy.h"
#include "BIBGame/BIBGameMode.h"
#include "BIBGame/BIBPlayerCharacter.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	struct FCrowdStats
	{
		int64 Samples = 0;
		int32 MaxCommittedMelee = 0;
		int64 FramesOverCap = 0;      // frames with more than TokenCap non-boss melee angels committed
		int64 CommittedSum = 0;
		int64 Pairs = 0;
		int64 StackedPairs = 0;       // two angels closer than their bodies (one point)
		double MinPair = 1.0e9;
		double NearestSum = 0.0;      // distance of the nearest angel to the commander
		double MeanDistSum = 0.0;
		int32 MaxLive = 0;
	};
	FCrowdStats GStats;
}

void UBIBTacticsLibrary::ResetCrowdStats()
{
	GStats = FCrowdStats();
}

void UBIBTacticsLibrary::SampleCrowd(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const ABIBGameMode* Mode = World ? Cast<ABIBGameMode>(UGameplayStatics::GetGameMode(World)) : nullptr;
	const ABIBPlayerCharacter* Player = Mode ? Mode->GetPlayerCharacter() : nullptr;
	if (!Player)
	{
		return;
	}
	const TArray<ABIBEnemy*> Live = Mode->GetLiveEnemies();
	if (Live.Num() == 0)
	{
		return;
	}
	++GStats.Samples;
	GStats.MaxLive = FMath::Max(GStats.MaxLive, Live.Num());
	int32 Committed = 0;
	double Nearest = 1.0e9;
	double Sum = 0.0;
	for (const ABIBEnemy* Enemy : Live)
	{
		const bool bMelee = Enemy->GetKind() != EBIBEnemyKind::Ranged && Enemy->GetKind() != EBIBEnemyKind::Mage;
		Committed += (bMelee && !Enemy->IsBoss() && Enemy->IsCommitted()) ? 1 : 0;
		const double Distance = FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation());
		Nearest = FMath::Min(Nearest, Distance);
		Sum += Distance;
	}
	for (int32 A = 0; A < Live.Num(); ++A)
	{
		for (int32 B = A + 1; B < Live.Num(); ++B)
		{
			const double Distance = FVector::Dist2D(Live[A]->GetActorLocation(), Live[B]->GetActorLocation());
			++GStats.Pairs;
			GStats.MinPair = FMath::Min(GStats.MinPair, Distance);
			GStats.StackedPairs += Distance < (Live[A]->GetBodyRadius() + Live[B]->GetBodyRadius()) * 0.8 ? 1 : 0;
		}
	}
	GStats.MaxCommittedMelee = FMath::Max(GStats.MaxCommittedMelee, Committed);
	GStats.FramesOverCap += Committed > 3 ? 1 : 0;
	GStats.CommittedSum += Committed;
	GStats.NearestSum += Nearest;
	GStats.MeanDistSum += Sum / Live.Num();
}

FString UBIBTacticsLibrary::GetCrowdStats()
{
	const double N = FMath::Max<int64>(1, GStats.Samples);
	return FString::Printf(TEXT("samples=%lld maxLive=%d maxCommittedMelee=%d framesOverCap=%lld meanCommitted=%.2f stackedPairShare=%.4f minPair=%.0f meanNearest=%.0f meanDist=%.0f"),
		GStats.Samples, GStats.MaxLive, GStats.MaxCommittedMelee, GStats.FramesOverCap, GStats.CommittedSum / N,
		GStats.Pairs > 0 ? static_cast<double>(GStats.StackedPairs) / static_cast<double>(GStats.Pairs) : 0.0,
		GStats.MinPair > 1.0e8 ? -1.0 : GStats.MinPair, GStats.NearestSum / N, GStats.MeanDistSum / N);
}
