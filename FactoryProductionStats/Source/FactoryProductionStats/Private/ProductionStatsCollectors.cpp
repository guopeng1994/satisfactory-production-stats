// SPDX-License-Identifier: 0BSD
#include "ProductionStatsCollectors.h"
#include <algorithm>
#include <map>

namespace FactoryProductionStats
{
bool QuantityInbox::Submit(QuantityEvent event)
{
    std::lock_guard lock(mutex);
    if (!active) return false;
    if (Validate(event) != Error::None || event.series.key.size() > 4096 || event.sourceClassPath.size() > 4096 || events.size() == Limit) { gap = true; return false; }
    const auto bytes = sizeof(QuantityEvent) * 2 + event.series.key.capacity() + event.sourceClassPath.capacity();
    if (bytes > MemoryLimit - retainedBytes) { gap = true; return false; }
    retainedBytes += bytes; // Includes room for vector geometric capacity growth.
    events.push_back(std::move(event));
    return true;
}
void QuantityInbox::MarkGap() { std::lock_guard lock(mutex); if (active) gap = true; }
QuantityInbox::Batch QuantityInbox::Drain(double frameBegin)
{
    std::lock_guard lock(mutex);
    Batch batch{std::move(events), gap};
    events.clear(); gap = false; retainedBytes = 0;
    for (auto& event : batch.events) event.time = frameBegin;
    return batch;
}
void QuantityInbox::Close() { std::lock_guard lock(mutex); active = false; events.clear(); gap = false; retainedBytes = 0; }
Error NativeQuantity(QuantityEvent& event, std::int64_t count, double scale)
{
    if (count <= 0) return Error::InvalidValue;
    if (event.series.category == Category::Items) event.quantity = count;
    else if (event.series.category == Category::Fluids)
    {
        if (!std::isfinite(scale) || scale <= 0) return Error::InvalidValue;
        event.quantity = static_cast<double>(count) * scale;
    }
    else return Error::InvalidSeries;
    return Validate(event);
}

PowerSnapshot AggregatePower(double time, const std::vector<CircuitSample>& circuits,
    const std::vector<DeviceSample>& devices, bool complete)
{
    constexpr std::array metrics{Metric::ActualConsumption, Metric::ActualProduction, Metric::ProductionCapacity, Metric::MaximumDemand};
    PowerSnapshot result; result.time = time;
    std::array<std::optional<double>, 4> totals{0., 0., 0., 0.};
    std::map<std::uint64_t, CircuitSample> seenCircuits;
    std::map<std::int64_t, bool> networks;
    for (const auto& circuit : circuits)
    {
        if (auto found = seenCircuits.find(circuit.token); found != seenCircuits.end())
        {
            if (found->second.network != circuit.network || found->second.values != circuit.values || found->second.tripped != circuit.tripped)
                for (auto& total : totals) total.reset();
            continue;
        }
        seenCircuits.emplace(circuit.token, circuit);
        networks[circuit.network] = networks[circuit.network] || circuit.tripped;
        for (std::size_t i = 0; i < totals.size(); ++i)
        {
            if (!circuit.values[i] || !std::isfinite(*circuit.values[i]) || *circuit.values[i] < 0) totals[i].reset();
            else if (totals[i])
            {
                *totals[i] += *circuit.values[i];
                if (!std::isfinite(*totals[i])) totals[i].reset();
            }
        }
    }
    result.networkCount = static_cast<std::uint32_t>(networks.size());
    result.trippedNetworkCount = static_cast<std::uint32_t>(std::count_if(networks.begin(), networks.end(), [](const auto& pair) { return pair.second; }));
    struct Group { std::optional<double> value{0.}; std::set<std::uint64_t> owners; };
    std::map<SeriesId, Group> groups;
    const auto id = [](Metric metric, Scope scope, const std::string& key = std::string{})
    { return SeriesId{Category::Power, metric, Direction::None, scope, key}; };
    const auto add = [&](Metric metric, const DeviceSample& device, double value)
    {
        auto& group = groups[id(metric, Scope::BuildingType, device.buildingClass)];
        group.owners.insert(device.owner);
        if (!std::isfinite(value) || value < 0) group.value.reset();
        else if (group.value) { *group.value += value; if (!std::isfinite(*group.value)) group.value.reset(); }
    };
    std::set<std::uint64_t> seenDevices, seenBatteries;
    std::array<std::optional<double>, 4> storage{0., 0., 0., 0.};
    for (const auto& device : devices)
    {
        if (!seenDevices.insert(device.token).second) continue;
        const bool resolved = IsClassPath(device.buildingClass);
        if (resolved && device.consumer) add(Metric::ActualConsumption, device, device.consumptionMW);
        if (resolved && device.producer && !device.battery)
        {
            add(Metric::ActualProduction, device, device.productionMW);
            add(Metric::ProductionBoost, device, device.boostMW);
        }
        if (!device.battery || !seenBatteries.insert(device.battery->token).second) continue;
        const auto& battery = *device.battery;
        const std::array values{std::max(battery.inputMW, 0.), std::max(-battery.inputMW, 0.), battery.storedMWh, battery.capacityMWh};
        constexpr std::array storageMetrics{Metric::ChargePower, Metric::DischargePower, Metric::StoredEnergy, Metric::StorageCapacity};
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            const bool valid = std::isfinite(battery.inputMW) && std::isfinite(values[i]) && values[i] >= 0;
            if (!valid) storage[i].reset();
            else if (storage[i]) { *storage[i] += values[i]; if (!std::isfinite(*storage[i])) storage[i].reset(); }
            if (resolved) add(storageMetrics[i], device, valid ? values[i] : std::numeric_limits<double>::quiet_NaN());
        }
    }
    for (const auto& [series, group] : groups)
        result.readings.push_back({series, group.value, static_cast<std::uint32_t>(group.owners.size()), complete});
    for (std::size_t i = 0; i < metrics.size(); ++i)
    {
        result.readings.push_back({id(metrics[i], Scope::World), totals[i], std::nullopt, complete});
        if (i > 1) continue;
        std::optional<double> classified{0.};
        for (const auto& [series, group] : groups) if (series.metric == metrics[i])
        {
            if (!group.value) classified.reset();
            else if (classified) *classified += *group.value;
        }
        std::optional<double> residual;
        if (totals[i] && classified && std::isfinite(*totals[i] - *classified)) residual = *totals[i] - *classified;
        result.readings.push_back({id(metrics[i], Scope::Unclassified), residual, std::nullopt, complete});
    }
    constexpr std::array storageMetrics{Metric::ChargePower, Metric::DischargePower, Metric::StoredEnergy, Metric::StorageCapacity};
    for (std::size_t i = 0; i < storage.size(); ++i)
        result.readings.push_back({id(storageMetrics[i], Scope::World), storage[i], static_cast<std::uint32_t>(seenBatteries.size()), complete});
    // Boost is already part of the production reading; never add it to world production again.
    std::optional<double> boost{0.};
    for (const auto& [series, group] : groups) if (series.metric == Metric::ProductionBoost)
    { if (!group.value) boost.reset(); else if (boost) *boost += *group.value; }
    if (boost && !std::isfinite(*boost)) boost.reset();
    result.readings.push_back({id(Metric::ProductionBoost, Scope::World), boost, std::nullopt, false});
    return result;
}
}
