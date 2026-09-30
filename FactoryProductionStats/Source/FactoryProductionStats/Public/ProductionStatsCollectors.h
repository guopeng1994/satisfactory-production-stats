// SPDX-License-Identifier: 0BSD
#pragma once
#include "ProductionStatsTypes.h"
#include <array>
#include <mutex>

namespace FactoryProductionStats
{
// World-owned handoff. Callbacks only submit copied facts, never touch History.
class QuantityInbox
{
public:
    static constexpr std::size_t Limit = 32768;
    static constexpr std::size_t MemoryLimit = 16 * 1024 * 1024;
    struct Batch { std::vector<QuantityEvent> events; bool gap = false; };
    bool Submit(QuantityEvent event);
    void MarkGap();
    Batch Drain(double frameBegin);
    void Close();
private:
    // ponytail: one mutex per world, at most 32768 pending facts; switch to an
    // MPSC queue only if T09 measures contention or allocation above budget.
    std::mutex mutex;
    std::vector<QuantityEvent> events;
    std::size_t retainedBytes = 0;
    bool active = true, gap = false;
};
Error NativeQuantity(QuantityEvent& event, std::int64_t rawCount, double cubicMetresPerUnit);

// Tokens deduplicate one sample only; they never become history IDs or save data.
struct CircuitSample
{
    std::uint64_t token = 0;
    std::int64_t network = 0;
    bool tripped = false;
    // Circuit-local actual consumption, actual production, capacity, maximum demand.
    std::array<std::optional<double>, 4> values;
};
struct BatterySample
{
    std::uint64_t token = 0;
    double inputMW = 0, storedMWh = 0, capacityMWh = 0;
};
struct DeviceSample
{
    std::uint64_t token = 0, owner = 0;
    std::string buildingClass;
    bool consumer = false, producer = false;
    double consumptionMW = 0, productionMW = 0, boostMW = 0;
    std::optional<BatterySample> battery;
};
PowerSnapshot AggregatePower(double time, const std::vector<CircuitSample>& circuits,
    const std::vector<DeviceSample>& devices, bool completeSources);
}
