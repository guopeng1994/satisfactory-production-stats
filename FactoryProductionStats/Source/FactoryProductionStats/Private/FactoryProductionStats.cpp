// SPDX-License-Identifier: 0BSD
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Diagnostics/FactoryStatsProbe.h"
#include "ProductionStatsSubsystem.h"
#include "ProductionStatsHooks.h"
#include "ProductionStatsPlayerComponent.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "Subsystem/SubsystemActorManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFactoryProductionStats, Log, All);

class FFactoryProductionStatsModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        UE_LOG(LogFactoryProductionStats, Log, TEXT("FactoryProductionStats 0.1.0 runtime module started"));
        FProductionStatsHooks::Install();
        InputHandle = AFGCharacterPlayer::OnPlayerInputInitialized.AddLambda([this](AFGCharacterPlayer* Character, UInputComponent* Input)
        {
            if (!IsValid(Character)) return;
            if (auto* Component = UProductionStatsPlayerComponent::Attach(Cast<AFGPlayerController>(Character->GetController())))
            { ActivePlayers.AddUnique(Component); Component->BindInput(Character, Input); }
        });
        WorldHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda([this](const UWorld::FActorsInitializedParams& Params)
        {
            auto* World = Params.World;
            if (!IsValid(World) || !World->IsGameWorld()) return;
            for (auto It = World->GetPlayerControllerIterator(); It; ++It)
                if (auto* Player = Cast<AFGPlayerController>(It->Get()))
                    if (auto* Component = UProductionStatsPlayerComponent::Attach(Player))
                    {
                        ActivePlayers.AddUnique(Component);
                        if (auto* Character = Cast<AFGCharacterPlayer>(Player->GetPawn())) Component->BindInput(Character, Character->InputComponent);
                    }
            if (World->GetNetMode() == NM_Client) return;
            if (auto* Manager = World->GetSubsystem<USubsystemActorManager>())
            {
                Manager->RegisterSubsystemActor(AProductionStatsSubsystem::StaticClass());
                if (auto* Stats = Manager->GetSubsystemActor<AProductionStatsSubsystem>()) ActiveSubsystems.AddUnique(Stats);
            }
        });
        ProbeCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("fps.Probe"), TEXT("T01: capture class census and raw circuit values on the next safe factory tick."),
            FConsoleCommandWithWorldDelegate::CreateLambda([this](UWorld* World)
            {
                if (!IsInGameThread() || !IsValid(World) || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
                {
                    UE_LOG(LogFactoryProductionStats, Warning, TEXT("T01 probe requires the game thread and an authoritative game world"));
                    return;
                }
                ActiveProbes.RemoveAll([](const auto& Probe) { return !Probe.IsValid(); });
                auto* Probe = World->SpawnActor<AFactoryStatsProbe>();
                if (IsValid(Probe)) ActiveProbes.Add(Probe);
                else UE_LOG(LogFactoryProductionStats, Warning, TEXT("T01 probe actor could not be spawned"));
            }), ECVF_Default);
        StatsCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("fps.Stats"),
            TEXT("Print actual All quantity totals and the current provisional power snapshot for T04/T05 validation."),
            FConsoleCommandWithWorldDelegate::CreateStatic(&FFactoryProductionStatsModule::DumpStats), ECVF_Default);
    }

    void ShutdownModule() override
    {
        AFGCharacterPlayer::OnPlayerInputInitialized.Remove(InputHandle);
        for (const auto& Component : ActivePlayers) if (Component.IsValid()) Component->DestroyComponent();
        ActivePlayers.Reset();
        if (StatsCommand) { IConsoleManager::Get().UnregisterConsoleObject(StatsCommand, false); StatsCommand = nullptr; }
        FWorldDelegates::OnWorldInitializedActors.Remove(WorldHandle);
        for (const auto& Stats : ActiveSubsystems) if (Stats.IsValid()) Stats->Destroy();
        ActiveSubsystems.Reset();
        FProductionStatsHooks::Remove();
        if (ProbeCommand)
        {
            IConsoleManager::Get().UnregisterConsoleObject(ProbeCommand, false);
            ProbeCommand = nullptr;
        }
        for (const auto& Probe : ActiveProbes)
            if (Probe.IsValid()) Probe->Destroy();
        ActiveProbes.Reset();
        UE_LOG(LogFactoryProductionStats, Log, TEXT("FactoryProductionStats runtime module stopped"));
    }
private:
    static void DumpStats(UWorld* World)
    {
        using namespace FactoryProductionStats;
        if (!IsInGameThread() || !IsValid(World) || !World->IsGameWorld() || World->GetNetMode() == NM_Client) return;
        auto* Manager = World->GetSubsystem<USubsystemActorManager>();
        auto* Stats = Manager ? Manager->GetSubsystemActor<AProductionStatsSubsystem>() : nullptr;
        if (!IsValid(Stats)) { UE_LOG(LogFactoryProductionStats, Warning, TEXT("fps.Stats: no authoritative stats subsystem")); return; }
        UE_LOG(LogFactoryProductionStats, Display, TEXT("fps.Stats time=%g coverage=PROVISIONAL (T01 game validation required)"), Stats->RecordingTime());
        for (auto CategoryValue : {Category::Items, Category::Fluids})
        {
            const auto Result = Stats->Query({CategoryValue, Window::All, Stats->RecordingTime(), {}, 1});
            for (const auto& Row : Result.series)
            {
                if (!Row.summary.value) continue;
                const auto& Value = std::get<Quantity>(*Row.summary.value);
                const auto Path = FString(UTF8_TO_TCHAR(Row.series.key.c_str()));
                const auto* DirectionText = Row.series.direction == Direction::Produced ? TEXT("produced") : TEXT("consumed");
                if (const auto* Items = std::get_if<std::int64_t>(&Value))
                    UE_LOG(LogFactoryProductionStats, Display, TEXT("%s %s count=%lld items observedSeconds=%g complete=%d gaps=%u"),
                        *Path, DirectionText, static_cast<long long>(*Items), Row.summary.coverage.observedSeconds, Row.summary.coverage.completeSources, Row.summary.coverage.gapReasons);
                else UE_LOG(LogFactoryProductionStats, Display, TEXT("%s %s count=%.17g m3 observedSeconds=%g complete=%d gaps=%u"),
                    *Path, DirectionText, std::get<double>(Value), Row.summary.coverage.observedSeconds, Row.summary.coverage.completeSources, Row.summary.coverage.gapReasons);
            }
        }
        const auto& Power = Stats->CurrentPower();
        if (Power.networkCount && Power.trippedNetworkCount)
            UE_LOG(LogFactoryProductionStats, Display, TEXT("snapshotTime=%g networks=%u tripped=%u"), Power.time, *Power.networkCount, *Power.trippedNetworkCount);
        else UE_LOG(LogFactoryProductionStats, Display, TEXT("power network snapshot unavailable"));
        constexpr const TCHAR* Names[]{TEXT("Quantity"), TEXT("ActualConsumption"), TEXT("ActualProduction"), TEXT("ProductionCapacity"),
            TEXT("MaximumDemand"), TEXT("ChargePower"), TEXT("DischargePower"), TEXT("ProductionBoost"), TEXT("StoredEnergy"), TEXT("StorageCapacity")};
        for (const auto& Reading : Power.readings)
        {
            const auto Path = FString(UTF8_TO_TCHAR(Reading.series.key.c_str()));
            const auto* UnitText = UnitFor(Reading.series) == Unit::MegawattHours ? TEXT("MWh") : TEXT("MW");
            if (Reading.value) UE_LOG(LogFactoryProductionStats, Display, TEXT("%s scope=%d key=%s value=%.17g %s complete=%d"),
                Names[static_cast<int32>(Reading.series.metric)], static_cast<int32>(Reading.series.scope), *Path, *Reading.value, UnitText, Reading.completeSources);
            else UE_LOG(LogFactoryProductionStats, Display, TEXT("%s scope=%d key=%s unknown"),
                Names[static_cast<int32>(Reading.series.metric)], static_cast<int32>(Reading.series.scope), *Path);
        }
    }
    IConsoleCommand* ProbeCommand = nullptr;
    IConsoleCommand* StatsCommand = nullptr;
    FDelegateHandle WorldHandle, InputHandle;
    TArray<TWeakObjectPtr<UProductionStatsPlayerComponent>> ActivePlayers;
    TArray<TWeakObjectPtr<AProductionStatsSubsystem>> ActiveSubsystems;
    TArray<TWeakObjectPtr<AFactoryStatsProbe>> ActiveProbes;
};

IMPLEMENT_MODULE(FFactoryProductionStatsModule, FactoryProductionStats)
