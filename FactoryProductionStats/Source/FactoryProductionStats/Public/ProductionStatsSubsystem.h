// SPDX-License-Identifier: 0BSD
#pragma once
#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "ProductionStatsHistory.h"
#include "ProductionStatsCollectors.h"
#include "FGSaveInterface.h"
#include "ProductionStatsSave.h"
#include <memory>
#include "ProductionStatsSubsystem.generated.h"

UCLASS(NotBlueprintable)
class FACTORYPRODUCTIONSTATS_API AProductionStatsSubsystem : public AModSubsystem, public IFGFactoryTickHandlerInterface, public IFGSaveInterface
{
    GENERATED_BODY()
public:
    AProductionStatsSubsystem();
    void Tick(float DeltaSeconds) override;
    void PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime) override;
    FactoryProductionStats::QueryResult Query(const FactoryProductionStats::Query& Request) const;
    double RecordingTime() const { check(IsInGameThread()); return History ? History->Clock() : 0; }
    double RecordingDuration() const { check(IsInGameThread()); return History ? History->Clock() - History->RecordingStart() : 0; }
    const FactoryProductionStats::PowerSnapshot& CurrentPower() const { check(IsInGameThread()); return Power; }
    FactoryProductionStats::Error PersistenceStatus() const { return SaveError; }
    void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
    void PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
    void PreLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
    void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
    void GatherDependencies_Implementation(TArray<UObject*>& Dependencies) override {}
    bool ShouldSave_Implementation() const override { return HasAuthority(); }
    bool NeedTransform_Implementation() override { return false; }
protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void TryRegister();
    void EnsureHistory();
    void StopCollection();
    bool FlushCompletedInterval();
    UPROPERTY(SaveGame) FProductionStatsSavePayload SavedPayload;
    FactoryProductionStats::Error SaveError = FactoryProductionStats::Error::None;
    bool PreservePayload = false;
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
