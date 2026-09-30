// SPDX-License-Identifier: 0BSD
#include "FactoryStatsProbe.h"
#include "Buildables/FGBuildable.h"
#include "ProductionStatsPowerReader.h"
#include "Engine/World.h"

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
        UE_LOG(LogFactoryStatsProbe, Warning, TEXT("probe=%s registration rejected: no authoritative game-thread buildable subsystem"), *GetPathName());
        Destroy();
        return;
    }
    RegisteredSubsystem = Subsystem;
    Subsystem->AddFactoryTickHandler(this);
    UE_LOG(LogFactoryStatsProbe, Display, TEXT("probe=%s handler registered"), *GetPathName());
    SetLifeSpan(10); // Also cleans up if no factory tick arrives.
}

void AFactoryStatsProbe::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Subsystem = RegisteredSubsystem.Get())
    {
        Subsystem->RemoveFactoryTickHandler(this);
        UE_LOG(LogFactoryStatsProbe, Display, TEXT("probe=%s handler removal requested captured=%d endReason=%d"),
            *GetPathName(), Captured, static_cast<int32>(Reason));
    }
    RegisteredSubsystem.Reset();
    Super::EndPlay(Reason);
}

void AFactoryStatsProbe::PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime)
{
    if (Captured || !IsInGameThread() || !HasAuthority() || Subsystem != RegisteredSubsystem.Get())
        return;
    Captured = true;
    TMap<FString, int32> ClassCounts;
    for (auto* Buildable : Subsystem->GetAllBuildablesRef())
    {
        if (!IsValid(Buildable)) continue;
        ++ClassCounts.FindOrAdd(Buildable->GetClass()->GetPathName());
    }
    UE_LOG(LogFactoryStatsProbe, Display, TEXT("T01 diagnostic gameThread=1 authority=1 dt=%g classes=%d; native registry circuit evidence follows (no verified world total)"),
        DeltaTime, ClassCounts.Num());
    for (const auto& Entry : ClassCounts)
        UE_LOG(LogFactoryStatsProbe, Display, TEXT("class=%s count=%d"), *Entry.Key, Entry.Value);
    // Diagnostic uses native world time; it does not advance or write save-local History.
    FProductionStatsPowerReader::Capture(GetWorld(), GetWorld()->GetTimeSeconds(), true);
    // Remove outside the handler iteration, never mutate its array mid-dispatch.
    SetLifeSpan(0.1f);
}
