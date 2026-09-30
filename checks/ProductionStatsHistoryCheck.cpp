// SPDX-License-Identifier: 0BSD
#include "ProductionStatsHistory.h"
#include <cassert>
#include <iostream>
#ifdef NDEBUG
#error This check requires assertions.
#endif
using namespace FactoryProductionStats;

const WriteContext Writer{true, 42};
const std::string Source = "/Game/Factory/Build_Machine.Build_Machine_C";
SeriesId Item(std::string key = "/Game/Items/Desc_Iron.Desc_Iron_C")
{ return {Category::Items, Metric::Quantity, Direction::Produced, Scope::World, std::move(key)}; }
SeriesId Fluid() { return {Category::Fluids, Metric::Quantity, Direction::Consumed, Scope::World, "/Game/Fluids/Desc_Water.Desc_Water_C"}; }
SeriesId Power(Metric metric) { return {Category::Power, metric, Direction::None, Scope::World, ""}; }
const SeriesResult& Row(const QueryResult& result) { assert(result.series.size() == 1); return result.series.front(); }
std::int64_t Count(const Bucket& bucket) { assert(bucket.value); return std::get<std::int64_t>(std::get<Quantity>(*bucket.value)); }
double Integral(const Bucket& bucket) { assert(bucket.value); return std::get<PowerIntegral>(*bucket.value).megawattSeconds; }
void Known(History& history)
{
    assert(history.SetQuantityCoverage(Category::Items, true, true, 0, Writer) == Error::None);
    assert(history.SetQuantityCoverage(Category::Fluids, true, true, 0, Writer) == Error::None);
}
QueryResult Ask(const History& history, const SeriesId& id, Window window = Window::Minute1, std::uint32_t points = 600)
{ return history.QueryHistory({id.category, window, history.Clock(), {id}, points}); }
int main()
{
    History invalid(42, std::numeric_limits<double>::quiet_NaN());
    assert(invalid.QueryHistory({Category::Items, Window::Minute1, 1, {Item()}, 600}).error == Error::InvalidTime);
    assert(invalid.SetQuantityCoverage(Category::Items, true, true, 0, Writer) == Error::InvalidTime);
    History history(42); Known(history);
    assert(history.Record({Item(), 0, std::int64_t{300}, Source}, Writer) == Error::None);
    assert(history.Record({Fluid(), 0, 120.0, Source}, Writer) == Error::None);
    assert(history.Record(PowerSnapshot{0, {{Power(Metric::ActualConsumption), 100.0, 1, true}, {Power(Metric::StoredEnergy), 20.0, 1, true}}, 1, 0}, Writer) == Error::None);
    assert(history.AdvanceTo(60, Writer) == Error::None);
    auto items = Ask(history, Item()); auto fluids = Ask(history, Fluid()); auto powers = Ask(history, Power(Metric::ActualConsumption));
    assert(Count(Row(items).summary) == 300 && Row(items).summary.coverage.observedSeconds == 60);
    assert(RatePerMinute(300, Row(items).summary.coverage.observedSeconds) == 300);
    assert(std::get<double>(std::get<Quantity>(*Row(fluids).summary.value)) == 120);
    assert(Integral(Row(powers).summary) == 6000);
    assert(std::abs(Integral(Row(powers).summary) / 3600 - 5.0 / 3) < 1e-12);
    auto energy = Ask(history, Power(Metric::StoredEnergy));
    assert(std::get<EnergyState>(*Row(energy).summary.value).megawattHours == 20);
    assert(history.Record(PowerSnapshot{60, {{Power(Metric::ActualConsumption), 100.0, 1, true}, {Power(Metric::StoredEnergy), 30.0, 1, true}}, 1, 0}, Writer) == Error::None);
    assert(std::get<EnergyState>(*Row(Ask(history, Power(Metric::StoredEnergy))).summary.value).megawattHours == 30);
    assert(history.Record({Item(), 59, std::int64_t{1}, Source}, Writer) == Error::OutOfOrder);
    assert(history.AdvanceTo(59, Writer) == Error::OutOfOrder);
    assert(history.AdvanceTo(60, {false, 42}) == Error::NotAuthoritative);
    assert(history.AdvanceTo(60, Writer) == Error::None && Count(Row(Ask(history, Item())).summary) == 300);
    // Half-open boundary: event at 60 is not in [0,60), survives a save at 60.
    assert(history.Record({Item(), 60, std::int64_t{2}, Source}, Writer) == Error::None);
    assert(Count(Row(Ask(history, Item())).summary) == 300);
    auto saved = history.Export();
    History restored(42);
    const auto restoreError = restored.Restore(saved, Writer);
    if (restoreError != Error::None) std::cerr << "restore error=" << static_cast<int>(restoreError) << '\n';
    assert(restoreError == Error::None);
    assert(std::get<EnergyState>(*Row(Ask(restored, Power(Metric::StoredEnergy))).summary.value).megawattHours == 30);
    assert(Count(Row(Ask(restored, Item(), Window::All)).summary) == 300);
    assert(restored.AdvanceTo(61, Writer) == Error::None);
    assert(Count(Row(Ask(restored, Item(), Window::All)).summary) == 302);
    auto corrupted = saved; corrupted.schemaVersion = 100;
    assert(restored.Restore(corrupted, Writer) == Error::UnsupportedSchema && restored.Clock() == 61);
    corrupted = saved; corrupted.series.front().history.allTrend.front().coverage.observedSeconds = 1000;
    assert(restored.Restore(corrupted, Writer) == Error::InvalidValue && restored.Clock() == 61);
    corrupted = saved;
    auto& corruptBucket = corrupted.series.front().history.levels.front().buckets.front();
    corruptBucket.value = Aggregate{Quantity{std::numeric_limits<std::int64_t>::max()}};
    assert(restored.Restore(corrupted, Writer) != Error::None && restored.Clock() == 61);

    History gaps(42); Known(gaps);
    assert(gaps.AdvanceTo(10, Writer) == Error::None);
    assert(gaps.Record({Item(), 10, std::int64_t{100}, Source}, Writer) == Error::None);
    assert(gaps.AdvanceTo(20, Writer) == Error::None);
    assert(gaps.SetQuantityCoverage(Category::Items, false, false, 2, Writer) == Error::None);
    assert(gaps.AdvanceTo(30, Writer) == Error::None);
    Known(gaps); assert(gaps.AdvanceTo(40, Writer) == Error::None);
    const auto gapResult = Ask(gaps, Item());
    assert(Count(Row(gapResult).summary) == 100 && Row(gapResult).summary.coverage.observedSeconds == 30);
    assert(std::any_of(Row(gapResult).points.begin(), Row(gapResult).points.end(), [](const auto& point) { return !point.value; }));
    const auto zero = Ask(gaps, Item("/Game/Items/Desc_New.Desc_New_C"));
    assert(Count(Row(zero).summary) == 0 && Row(zero).summary.coverage.observedSeconds == 30);
    const auto unknownPower = Ask(gaps, Power(Metric::ActualProduction), Window::All);
    assert(unknownPower.error == Error::NoCoverage && !Row(unknownPower).summary.value);
    assert(gaps.Record({Item("/Game/Items/Desc_New.Desc_New_C"), 40, std::int64_t{20}, Source}, Writer) == Error::None);
    assert(gaps.AdvanceTo(41, Writer) == Error::None);
    assert(Row(Ask(gaps, Item("/Game/Items/Desc_New.Desc_New_C"))).summary.coverage.observedSeconds == 31);

    History uneven(42);
    assert(uneven.Record(PowerSnapshot{0, {{Power(Metric::ActualConsumption), 100.0, 1, true}}, 1, 0}, Writer) == Error::None);
    assert(uneven.AdvanceTo(10, Writer) == Error::None);
    assert(uneven.Record(PowerSnapshot{10, {{Power(Metric::ActualConsumption), 200.0, 1, true}}, 1, 0}, Writer) == Error::None);
    assert(uneven.AdvanceTo(40, Writer) == Error::None);
    assert(Integral(Row(Ask(uneven, Power(Metric::ActualConsumption))).summary) == 7000);
    assert(uneven.Record(PowerSnapshot{40, {}, std::nullopt, std::nullopt}, Writer) == Error::None);
    assert(uneven.AdvanceTo(50, Writer) == Error::None);
    assert(Row(Ask(uneven, Power(Metric::ActualConsumption))).summary.coverage.observedSeconds == 40);
    assert(AverageMegawatts({7000}, 40) == 175);
    History shortHistory(42); Known(shortHistory);
    assert(shortHistory.Record({Item(), 0, std::int64_t{100}, Source}, Writer) == Error::None);
    assert(shortHistory.AdvanceTo(20, Writer) == Error::None);
    assert(Row(Ask(shortHistory, Item())).summary.coverage.observedSeconds == 20);

    // More than 1000 hours; All is exact while finite windows use retained cells.
    History longHistory(42); Known(longHistory);
    assert(longHistory.Record({Item(), 0, std::int64_t{7}, Source}, Writer) == Error::None);
    assert(longHistory.AdvanceTo(4000000, Writer) == Error::None);
    const auto beforeBytes = longHistory.AllocatedBytes();
    for (int i = 0; i < 3000; ++i)
    {
        assert(longHistory.Record({Item(), longHistory.Clock(), std::int64_t{1}, Source}, Writer) == Error::None);
        assert(longHistory.AdvanceTo(longHistory.Clock() + 1, Writer) == Error::None);
    }
    assert(longHistory.AllocatedBytes() == beforeBytes);
    assert(Count(Row(Ask(longHistory, Item(), Window::All)).summary) == 3007);
    for (Window window : {Window::Seconds5, Window::Minute1, Window::Minutes10, Window::Hour1, Window::Hours10,
        Window::Hours50, Window::Hours250, Window::Hours1000, Window::All})
    {
        auto answer = Ask(longHistory, Item(), window, 17);
        assert(answer.error == Error::None && Row(answer).points.size() <= 17);
        for (const auto& point : Row(answer).points) assert(Validate(Item(), point) == Error::None);
        assert(Validate(Item(), Row(answer).summary) == Error::None);
        std::int64_t total = 0;
        for (const auto& point : Row(answer).points) total += Count(point);
        assert(total == Count(Row(answer).summary));
    }
    auto longSave = longHistory.Export();
    History longRestored(42);
    assert(longRestored.Restore(longSave, Writer) == Error::None);
    assert(Count(Row(Ask(longRestored, Item(), Window::All)).summary) == 3007);
    assert(longSave.series.front().history.allTrend.size() <= 512);
    // Coarse unaligned boundaries report whole buckets; verify against event times.
    History boundary(42); Known(boundary);
    for (int i = 0; i < 120; ++i)
    {
        assert(boundary.Record({Item(), boundary.Clock(), std::int64_t{1}, Source}, Writer) == Error::None);
        assert(boundary.AdvanceTo(boundary.Clock() + 1, Writer) == Error::None);
    }
    const auto aligned = boundary.QueryHistory({Category::Items, Window::Minute1, 113, {Item()}, 8});
    assert(aligned.approximateBoundary && aligned.actualRange.begin == 50 && aligned.actualRange.end == 120);
    assert(Count(Row(aligned).summary) == 70 && Row(aligned).points.size() == 7);
    // Pending counts and ring merges remain integer-exact above 2^53.
    History wide(42); Known(wide);
    const auto wideCount = (std::int64_t{1} << 53) + 7;
    assert(wide.Record({Item(), 0, wideCount, Source}, Writer) == Error::None);
    assert(wide.AdvanceTo(1, Writer) == Error::None);
    assert(Count(Row(Ask(wide, Item())).summary) == wideCount);
    assert(wide.Record({Item(), 1, std::numeric_limits<std::int64_t>::max(), Source}, Writer) == Error::Overflow);
    assert(Count(Row(Ask(wide, Item())).summary) == wideCount);
    // Known successful counts survive a coverage failure; the rate stays unknown.
    History interrupted(42); Known(interrupted);
    assert(interrupted.Record({Item(), 0, std::int64_t{3}, Source}, Writer) == Error::None);
    assert(interrupted.SetQuantityCoverage(Category::Items, false, false, 2, Writer) == Error::None);
    assert(interrupted.AdvanceTo(1, Writer) == Error::None);
    assert(Count(Row(Ask(interrupted, Item())).summary) == 3);
    assert(!RatePerMinute(3, Row(Ask(interrupted, Item())).summary.coverage.observedSeconds));
    History capacity(42); Known(capacity);
    for (int i = 0; i < 500; ++i)
        assert(capacity.Record({Item("/Game/Items/Desc_" + std::to_string(i) + ".Desc_C"), 0, std::int64_t{1}, Source}, Writer) == Error::None);
    assert(capacity.AllocatedBytes() < History::MemoryLimit);
    std::cout << "T03 history checks passed; 500-series allocated bytes=" << capacity.AllocatedBytes() << "; Bucket bytes=" << sizeof(Bucket) << '\n';
}
