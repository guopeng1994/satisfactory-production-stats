// SPDX-License-Identifier: 0BSD
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Diagnostics/FactoryStatsProbe.h"

DEFINE_LOG_CATEGORY_STATIC(LogFactoryProductionStats, Log, All);

class FFactoryProductionStatsModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        UE_LOG(LogFactoryProductionStats, Log, TEXT("FactoryProductionStats 0.1.0 runtime module started"));
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
    }

    void ShutdownModule() override
    {
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
    IConsoleCommand* ProbeCommand = nullptr;
    TArray<TWeakObjectPtr<AFactoryStatsProbe>> ActiveProbes;
};

IMPLEMENT_MODULE(FFactoryProductionStatsModule, FactoryProductionStats)
