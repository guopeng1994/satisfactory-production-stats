// SPDX-License-Identifier: 0BSD
#pragma once
#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "ProductionStatsHistory.h"
#include "ProductionStatsCollectors.h"
#include <memory>
#include "ProductionStatsSubsystem.generated.h"

UCLASS(NotBlueprintable)
class FACTORYPRODUCTIONSTATS_API AProductionStatsSubsystem : public AModSubsystem, public IFGFactoryTickHandlerInterface
{
    GENERATED_BODY()
public:
    AProductionStatsSubsystem();
    void Tick(float DeltaSeconds) override;
    void PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime) override;
    FactoryProductionStats::QueryResult Query(const FactoryProductionStats::Query& Request) const;
    double RecordingTime() const { check(IsInGameThread()); return History ? History->Clock() : 0; }
    const FactoryProductionStats::PowerSnapshot& CurrentPower() const { check(IsInGameThread()); return Power; }
protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void TryRegister();
    void QueueActor(AActor* Actor);
    UFUNCTION() void OnBuildableAdded(AFGBuildable* Actor);
    UFUNCTION() void OnBuildableRemoved(AFGBuildable* Actor);
    UFUNCTION() void OnActorDestroyed(AActor* Actor);
    TWeakObjectPtr<AFGBuildableSubsystem> FactorySubsystem;
    TArray<TWeakObjectPtr<AActor>> PendingActors;
    TSet<TWeakObjectPtr<AActor>> RegisteredActors;
    FDelegateHandle SpawnHandle;
    std::shared_ptr<FactoryProductionStats::QuantityInbox> Inbox;
    std::unique_ptr<FactoryProductionStats::History> History;
    FactoryProductionStats::PowerSnapshot Power;
    std::set<FactoryProductionStats::SeriesId> KnownPowerSeries;
    double PendingDelta = 0, NextPowerTime = 0;
    bool Closing = false;
    bool DescriptorsDirty = false;
};
