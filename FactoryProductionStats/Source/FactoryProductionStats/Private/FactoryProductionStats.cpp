// SPDX-License-Identifier: 0BSD
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogFactoryProductionStats, Log, All);

class FFactoryProductionStatsModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        UE_LOG(LogFactoryProductionStats, Log, TEXT("FactoryProductionStats 0.1.0 runtime module started"));
    }

    void ShutdownModule() override
    {
        UE_LOG(LogFactoryProductionStats, Log, TEXT("FactoryProductionStats runtime module stopped"));
    }
};

IMPLEMENT_MODULE(FFactoryProductionStatsModule, FactoryProductionStats)
