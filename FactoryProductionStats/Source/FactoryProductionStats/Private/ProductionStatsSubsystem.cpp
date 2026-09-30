// SPDX-License-Identifier: 0BSD
#include "ProductionStatsSubsystem.h"
#include "ProductionStatsHooks.h"
#include "ProductionStatsPowerReader.h"
#include "FGPortableMiner.h"
#include "EngineUtils.h"
#include <atomic>

using namespace FactoryProductionStats;
DEFINE_LOG_CATEGORY_STATIC(LogProductionStats, Log, All);
namespace { std::atomic<std::uint64_t> NextEpoch{1}; }
AProductionStatsSubsystem::AProductionStatsSubsystem()
{
    ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
    PrimaryActorTick.bCanEverTick = true; // Startup retry only; factory ticks own data.
}
void AProductionStatsSubsystem::BeginPlay()
{
    Super::BeginPlay();
    if (WITH_EDITOR || !HasAuthority() || !GetWorld()->IsGameWorld()) { SetActorTickEnabled(false); return; }
    History = std::make_unique<FactoryProductionStats::History>(NextEpoch.fetch_add(1));
    Inbox = std::make_shared<QuantityInbox>();
    TryRegister();
}
void AProductionStatsSubsystem::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); TryRegister(); }
void AProductionStatsSubsystem::TryRegister()
{
    if (!History || Closing || FactorySubsystem.IsValid()) return;
    auto* Factory = AFGBuildableSubsystem::Get(GetWorld());
    if (!IsValid(Factory)) return;
    FactorySubsystem = Factory;
    Factory->mBuildableAddedDelegate.AddUniqueDynamic(this, &AProductionStatsSubsystem::OnBuildableAdded);
    Factory->mBuildableRemovedDelegate.AddUniqueDynamic(this, &AProductionStatsSubsystem::OnBuildableRemoved);
    SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AProductionStatsSubsystem::QueueActor));
    // Native registry plus one initial portable-miner census. No per-frame world scan.
    for (auto* Actor : Factory->GetAllBuildablesRef()) QueueActor(Actor);
    for (TActorIterator<AFGPortableMiner> It(GetWorld()); It; ++It) QueueActor(*It);
    Factory->AddFactoryTickHandler(this);
    SetActorTickEnabled(false);
    UE_LOG(LogProductionStats, Display, TEXT("T03-T05 source integration active; quantity/power coverage is provisional pending Windows T01 validation"));
}
void AProductionStatsSubsystem::QueueActor(AActor* Actor)
{
    if (!IsInGameThread()) { if (Inbox) Inbox->MarkGap(); return; }
    if (!IsValid(Actor) || Closing || !FProductionStatsHooks::IsSource(Actor)) return;
    const TWeakObjectPtr<AActor> Key(Actor);
    if (!RegisteredActors.Contains(Key) && !PendingActors.Contains(Key))
    { PendingActors.Add(Key); DescriptorsDirty = true; }
}
void AProductionStatsSubsystem::OnBuildableAdded(AFGBuildable* Actor) { QueueActor(Actor); }
void AProductionStatsSubsystem::OnBuildableRemoved(AFGBuildable* Actor) { OnActorDestroyed(Actor); }
void AProductionStatsSubsystem::OnActorDestroyed(AActor* Actor)
{
    if (!IsInGameThread()) { if (Inbox) Inbox->MarkGap(); return; }
    FProductionStatsHooks::Unregister(Actor);
    RegisteredActors.Remove(TWeakObjectPtr<AActor>(Actor));
    PendingActors.Remove(TWeakObjectPtr<AActor>(Actor));
    if (IsValid(Actor)) Actor->OnDestroyed.RemoveDynamic(this, &AProductionStatsSubsystem::OnActorDestroyed);
}
void AProductionStatsSubsystem::PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime)
{
    if (Closing || !IsInGameThread() || !HasAuthority() || !History || Subsystem != FactorySubsystem.Get()) return;
    const WriteContext Writer{true, History->Epoch()};
    auto Batch = Inbox->Drain(History->Clock());
    const auto Unsupported = static_cast<std::uint32_t>(GapReason::UnsupportedSource);
    for (auto CategoryValue : {Category::Items, Category::Fluids})
        History->SetQuantityCoverage(CategoryValue, true, false, Unsupported, Writer);
    bool Gap = Batch.gap;
    for (const auto& Event : Batch.events) if (History->Record(Event, Writer) != Error::None) Gap = true;
    if (Gap)
    {
        for (auto CategoryValue : {Category::Items, Category::Fluids})
            History->SetQuantityCoverage(CategoryValue, false, false, Unsupported | static_cast<std::uint32_t>(GapReason::CollectorUnavailable), Writer);
        FProductionStatsHooks::RefreshDescriptors();
    }
    // Flush the previous factory interval with its own dt, before publishing this
    // interval's bindings/snapshot. Workers never advance time or mutate History.
    if (History->AdvanceTo(History->Clock() + PendingDelta, Writer) != Error::None) { Inbox->MarkGap(); PendingDelta = 0; return; }
    PendingDelta = std::isfinite(DeltaTime) && DeltaTime > 0 ? DeltaTime : 0;
    if (DescriptorsDirty) { FProductionStatsHooks::RefreshDescriptors(); DescriptorsDirty = false; }
    for (int32 Index = PendingActors.Num() - 1; Index >= 0; --Index)
    {
        auto* Actor = PendingActors[Index].Get();
        if (!IsValid(Actor)) { PendingActors.RemoveAtSwap(Index); continue; }
        if (!FProductionStatsHooks::Register(Actor, Inbox)) continue;
        RegisteredActors.Add(TWeakObjectPtr<AActor>(Actor));
        Actor->OnDestroyed.AddUniqueDynamic(this, &AProductionStatsSubsystem::OnActorDestroyed);
        PendingActors.RemoveAtSwap(Index);
    }
    if (History->Clock() >= NextPowerTime)
    {
        Power = FProductionStatsPowerReader::Capture(GetWorld(), History->Clock());
        // A previously classified type with no devices now contributes observed
        // zero only when a complete registry snapshot was acquired successfully.
        std::set<SeriesId> Present;
        for (const auto& Reading : Power.readings)
        {
            Present.insert(Reading.series);
            if (KnownPowerSeries.size() < 600) KnownPowerSeries.insert(Reading.series);
        }
        if (Power.networkCount)
            for (const auto& Id : KnownPowerSeries) if (!Present.contains(Id)) Power.readings.push_back({Id, 0., 0, false});
        if (History->Record(Power, Writer) != Error::None)
        {
            Power = {History->Clock(), {}, std::nullopt, std::nullopt};
            History->Record(Power, Writer); // Clear stale held readings, never silently keep last success.
        }
        NextPowerTime = History->Clock() + 1;
    }
}
QueryResult AProductionStatsSubsystem::Query(const FactoryProductionStats::Query& Request) const
{
    check(IsInGameThread());
    if (History && !Closing) return History->QueryHistory(Request);
    QueryResult Result; Result.error = Error::NoCoverage; return Result;
}
void AProductionStatsSubsystem::EndPlay(const EEndPlayReason::Type Reason)
{
    Closing = true;
    if (Inbox) Inbox->Close(); // Outstanding callback scopes now fail harmlessly.
    if (auto* Factory = FactorySubsystem.Get())
    {
        Factory->RemoveFactoryTickHandler(this);
        Factory->mBuildableAddedDelegate.RemoveDynamic(this, &AProductionStatsSubsystem::OnBuildableAdded);
        Factory->mBuildableRemovedDelegate.RemoveDynamic(this, &AProductionStatsSubsystem::OnBuildableRemoved);
    }
    if (SpawnHandle.IsValid()) GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
    for (const auto& Key : RegisteredActors) if (auto* Actor = Key.Get())
    {
        FProductionStatsHooks::Unregister(Actor);
        Actor->OnDestroyed.RemoveDynamic(this, &AProductionStatsSubsystem::OnActorDestroyed);
    }
    FProductionStatsHooks::ReleaseDescriptorsIfIdle();
    RegisteredActors.Empty(); PendingActors.Empty(); FactorySubsystem.Reset();
    Inbox.reset(); History.reset(); KnownPowerSeries.clear(); Power = {};
    Super::EndPlay(Reason);
}
