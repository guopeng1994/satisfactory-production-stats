// SPDX-License-Identifier: 0BSD
#include "ProductionStatsCollectors.h"
#include "ProductionStatsHistory.h"
#include <cassert>
#include <iostream>
#include <thread>
#ifdef NDEBUG
#error This check requires assertions.
#endif
using namespace FactoryProductionStats;
const std::string Machine = "/Game/Factory/Build_Machine.Build_Machine_C";
const PowerReading& Find(const PowerSnapshot& snapshot, Metric metric, Scope scope = Scope::World)
{
    for (const auto& reading : snapshot.readings) if (reading.series.metric == metric && reading.series.scope == scope) return reading;
    assert(false); std::abort();
}
int main()
{
    QuantityEvent item{{Category::Items, Metric::Quantity, Direction::Produced, Scope::World, "/Game/Items/Desc_Iron.Desc_Iron_C"}, 0, std::int64_t{0}, Machine};
    assert(NativeQuantity(item, 3, 0) == Error::None && std::get<std::int64_t>(item.quantity) == 3);
    auto fluid = item; fluid.series.category = Category::Fluids;
    assert(NativeQuantity(fluid, 120000, .001) == Error::None && std::get<double>(fluid.quantity) == 120);
    assert(NativeQuantity(fluid, 1, 0) == Error::InvalidValue);
    QuantityInbox inbox;
    std::thread first([&] { for (int i = 0; i < 1000; ++i) assert(inbox.Submit(item)); });
    std::thread second([&] { for (int i = 0; i < 1000; ++i) assert(inbox.Submit(item)); });
    first.join(); second.join();
    const auto batch = inbox.Drain(10);
    assert(batch.events.size() == 2000 && !batch.gap);
    History history(1); const WriteContext writer{true, 1};
    assert(history.SetQuantityCoverage(Category::Items, true, false, 4, writer) == Error::None);
    assert(history.AdvanceTo(10, writer) == Error::None);
    for (const auto& event : batch.events) assert(event.time == 10 && history.Record(event, writer) == Error::None);
    assert(history.AdvanceTo(11, writer) == Error::None);
    const auto quantity = history.QueryHistory({Category::Items, Window::All, 11, {item.series}, 600});
    assert(std::get<std::int64_t>(std::get<Quantity>(*quantity.series.front().summary.value)) == 6000);
    for (std::size_t i = 0; i < QuantityInbox::Limit; ++i) assert(inbox.Submit(item));
    assert(!inbox.Submit(item)); assert(inbox.Drain(11).gap);
    inbox.Close(); assert(!inbox.Submit(item) && inbox.Drain(12).events.empty());

    // Two circuits connected by a switch: local values sum, logical network counts once.
    std::vector<CircuitSample> circuits{{1, 7, false, {20., 50., 100., 30.}}, {2, 7, true, {30., 50., 100., 40.}}, {3, 9, false, {10., 0., 0., 15.}}};
    circuits.push_back(circuits.front());
    std::vector<DeviceSample> devices{
        {1, 11, Machine, true, false, 70, 0, 0, std::nullopt},
        {2, 12, Machine, false, true, 0, 100, 10, std::nullopt},
        {3, 13, Machine, false, true, 0, 999, 0, BatterySample{30, 5, 20, 100}},
        {4, 14, Machine, false, false, 0, 0, 0, BatterySample{40, -8, 30, 100}}};
    devices.push_back(devices.front()); devices.push_back(devices[2]);
    auto snapshot = AggregatePower(0, circuits, devices, true);
    assert(Validate(snapshot) == Error::None && snapshot.networkCount == 2 && snapshot.trippedNetworkCount == 1);
    assert(Find(snapshot, Metric::ActualConsumption).value == 60);
    assert(Find(snapshot, Metric::ActualProduction).value == 100); // boost not added again, battery not generation
    assert(Find(snapshot, Metric::ActualConsumption, Scope::Unclassified).value == -10);
    assert(Find(snapshot, Metric::ActualProduction, Scope::BuildingType).value == 100);
    assert(Find(snapshot, Metric::ChargePower).value == 5 && Find(snapshot, Metric::DischargePower).value == 8);
    assert(Find(snapshot, Metric::StoredEnergy).value == 50 && Find(snapshot, Metric::StorageCapacity).value == 200);
    circuits[1].network = 8;
    assert(AggregatePower(0, circuits, devices, false).networkCount == 3);
    circuits[1].values[0].reset();
    auto missing = AggregatePower(0, circuits, devices, false);
    assert(!Find(missing, Metric::ActualConsumption).value && !Find(missing, Metric::ActualConsumption, Scope::Unclassified).value);
    auto empty = AggregatePower(0, {}, {}, false);
    assert(empty.networkCount == 0 && empty.trippedNetworkCount == 0 && Find(empty, Metric::StoredEnergy).value == 0);
    assert(!Find(empty, Metric::StoredEnergy).completeSources);
    std::cout << "T04/T05 core handoff and power aggregation checks passed\n";
}
