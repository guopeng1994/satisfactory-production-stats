// SPDX-License-Identifier: 0BSD
#include "FactoryStatsProbe.h"
#include "Buildables/FGBuildableFactory.h"
#include "FGPowerCircuit.h"
#include "FGPowerInfoComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFactoryStatsProbe, Log, All);

AFactoryStatsProbe::AFactoryStatsProbe()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AFactoryStatsProbe::BeginPlay()
{
    Super::BeginPlay();
    auto* Subsystem = AFGBuildableSubsystem::Get(GetWorld());
    if (!IsInGameThread() || !HasAuthority() || !IsValid(Subsystem))
    {
        Destroy();
        return;
    }
    RegisteredSubsystem = Subsystem;
    Subsystem->AddFactoryTickHandler(this);
    SetLifeSpan(10); // Also cleans up if no factory tick arrives.
}

void AFactoryStatsProbe::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Subsystem = RegisteredSubsystem.Get())
        Subsystem->RemoveFactoryTickHandler(this);
    RegisteredSubsystem.Reset();
    Super::EndPlay(Reason);
}

void AFactoryStatsProbe::PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime)
{
    if (Captured || !IsInGameThread() || !HasAuthority() || Subsystem != RegisteredSubsystem.Get())
        return;
    Captured = true;
    TMap<FString, int32> ClassCounts;
    TSet<UFGPowerCircuit*> Circuits;
    for (auto* Buildable : Subsystem->GetAllBuildablesRef())
    {
        if (!IsValid(Buildable)) continue;
        ++ClassCounts.FindOrAdd(Buildable->GetClass()->GetPathName());
        auto* Factory = Cast<AFGBuildableFactory>(Buildable);
        auto* Info = Factory ? Factory->GetPowerInfo() : nullptr;
        auto* Circuit = IsValid(Info) ? Info->GetPowerCircuit() : nullptr;
        if (IsValid(Circuit)) Circuits.Add(Circuit);
    }
    UE_LOG(LogFactoryStatsProbe, Display, TEXT("T01 diagnostic gameThread=1 authority=1 dt=%g classes=%d discoveredCircuits=%d (factory-attached only; no world total)"),
        DeltaTime, ClassCounts.Num(), Circuits.Num());
    for (const auto& Entry : ClassCounts)
        UE_LOG(LogFactoryStatsProbe, Display, TEXT("class=%s count=%d"), *Entry.Key, Entry.Value);
    for (auto* Circuit : Circuits)
    {
        FPowerCircuitStats Stats;
        Circuit->GetStats(Stats);
        UE_LOG(LogFactoryStatsProbe, Display, TEXT("circuit=%d group=%d fuse=%d consumed=%g produced=%g capacity=%g demand=%g batteryNet=%g boost=%g (raw native values)"),
            Circuit->GetCircuitID(), Circuit->GetCircuitGroupID(), Circuit->IsFuseTriggered(),
            Stats.PowerConsumed, Stats.PowerProduced, Stats.PowerProductionCapacity,
            Stats.MaximumPowerConsumption, Stats.BatteryPowerInput, Stats.BoostProduced);
    }
    // Remove outside the handler iteration, never mutate its array mid-dispatch.
    SetLifeSpan(0.1f);
}
