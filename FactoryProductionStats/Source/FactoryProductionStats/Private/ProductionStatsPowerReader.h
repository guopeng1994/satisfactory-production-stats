// SPDX-License-Identifier: 0BSD
#pragma once
#include "ProductionStatsCollectors.h"
class UWorld;
class FProductionStatsPowerReader
{
public:
    static FactoryProductionStats::PowerSnapshot Capture(UWorld* World, double Time);
};
