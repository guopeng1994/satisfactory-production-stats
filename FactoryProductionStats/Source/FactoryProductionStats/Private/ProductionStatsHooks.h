// SPDX-License-Identifier: 0BSD
#pragma once
#include "CoreMinimal.h"
#include "ProductionStatsCollectors.h"
#include <memory>
class AActor;

// Friend access is declared by Config/AccessTransformers.ini, never by editing SDK headers.
class FProductionStatsHooks
{
public:
    static void Install();
    static void Remove();
    static bool IsSource(const AActor* Actor);
    static bool Register(AActor* Actor, const std::shared_ptr<FactoryProductionStats::QuantityInbox>& Inbox);
    static void Unregister(AActor* Actor);
    static void ReleaseDescriptorsIfIdle();
    static void RefreshDescriptors();
private:
    static TArray<TFunction<void()>> Unsubscribers;
};
