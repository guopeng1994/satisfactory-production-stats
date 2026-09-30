// SPDX-License-Identifier: 0BSD
#pragma once
#include "ProductionStatsCollectors.h"
class UWorld;
class FProductionStatsPowerReader
{
public:
    // Raw evidence is opt-in for the one-shot T01 probe, never regular collection.
    static FactoryProductionStats::PowerSnapshot Capture(UWorld* World, double Time, bool LogEvidence = false);
};
