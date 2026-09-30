// SPDX-License-Identifier: 0BSD
#include "ProductionStatsHistory.h"
#include "ProductionStatsView.h"
#include <cassert>
#include <iostream>

using namespace FactoryProductionStats;
int main()
{
    const SeriesId Item{Category::Items, Metric::Quantity, Direction::Produced, Scope::World, "/Mod/Item.Item_C"};
    const SeriesId Fluid{Category::Fluids, Metric::Quantity, Direction::Consumed, Scope::World, "/Mod/Fluid.Fluid_C"};
    const SeriesId Power{Category::Power, Metric::ActualProduction, Direction::None, Scope::World, {}};
    const SeriesId Storage{Category::Power, Metric::StoredEnergy, Direction::None, Scope::World, {}};
    const WriteContext Owner{true, 1};
    History Original(1);
    assert(Original.SetQuantityCoverage(Category::Items, true, true, 0, Owner) == Error::None);
    assert(Original.SetQuantityCoverage(Category::Fluids, true, true, 0, Owner) == Error::None);
    assert(Original.Record({Item, 0, std::int64_t{9007199254740993LL}, "/Mod/Source.Source_C"}, Owner) == Error::None);
    assert(Original.Record({Fluid, 0, 2.125, "/Mod/Source.Source_C"}, Owner) == Error::None);
    assert(Original.Record({0, {{Power, 100., 1, true}, {Storage, 12., 1, true}}, 1, 0}, Owner) == Error::None);
    assert(Original.AdvanceTo(60, Owner) == Error::None);
    assert(Original.Record({Item, 60, std::int64_t{7}, "/Mod/Source.Source_C"}, Owner) == Error::None);
    std::vector<std::uint8_t> Bytes;
    assert(Original.EncodeSave(Bytes) == Error::None);
    assert(Bytes.size() < History::SaveByteLimit);
    History Loaded(77);
    assert(Loaded.DecodeSave(Bytes, {false, 77}) == Error::NotAuthoritative);
    assert(Loaded.DecodeSave(Bytes, {true, 1}) == Error::NotAuthoritative);
    assert(Loaded.DecodeSave(Bytes, {true, 77}) == Error::None);
    assert(Loaded.Epoch() == 77 && Loaded.Clock() == 60);
    auto QueryResultValue = Loaded.QueryHistory({Category::Items, Window::All, 60, {}, 600});
    assert(QueryResultValue.series.size() == 1);
    assert(std::get<std::int64_t>(std::get<Quantity>(*QueryResultValue.series[0].summary.value)) == 9007199254740993LL);
    assert(Loaded.AdvanceTo(61, {true, 77}) == Error::None);
    auto NewTotal = Loaded.QueryHistory({Category::Items, Window::All, 61, {}, 600});
    assert(std::get<std::int64_t>(std::get<Quantity>(*NewTotal.series[0].summary.value)) == 9007199254741000LL);
    auto Copy = Bytes; Copy[4] = 99;
    assert(Loaded.DecodeSave(Copy, {true, 77}) == Error::UnsupportedSchema && Loaded.Clock() == 61);
    Copy = Bytes; Copy[Copy.size() / 2] ^= 1;
    assert(Loaded.DecodeSave(Copy, {true, 77}) == Error::InvalidValue && Loaded.Clock() == 61);
    for (auto Length : {std::size_t{0}, std::size_t{35}, Bytes.size() - 1})
        assert(Loaded.DecodeSave(std::span<const std::uint8_t>(Bytes).first(Length), {true, 77}) != Error::None);
    History Empty(3); std::vector<std::uint8_t> EmptyBytes;
    assert(Empty.EncodeSave(EmptyBytes) == Error::None);
    assert(Loaded.DecodeSave(EmptyBytes, {true, 77}) == Error::None && Loaded.SeriesCount() == 0 && Loaded.Clock() == 0);
    // Load an old save after future gameplay, then continue only the old timeline.
    assert(Loaded.DecodeSave(Bytes, {true, 77}) == Error::None && Loaded.Clock() == 60);
    auto PowerRows = Loaded.QueryHistory({Category::Power, Window::All, 60, {}, 600});
    assert(PowerRows.series.size() == 2);
    for (const auto& Row : PowerRows.series)
        assert(DisplayValue(Row.series, Row.summary) == (Row.series == Power ? 100. : 12.));
    SeriesResult Row; Row.series = Item; Row.displayName = "A";
    Row.summary = {{0, 60}, {60, true, 0}, Aggregate{Quantity{std::int64_t{300}}}};
    assert(DisplayValue(Item, Row.summary) == 300.);
    Row.points = {
        {{0, 1}, {1, true, 0}, Aggregate{Quantity{std::int64_t{1}}}},
        {{1, 2}, {0, false, 2}, std::nullopt},
        {{2, 3}, {1, true, 0}, Aggregate{Quantity{std::int64_t{0}}}},
        {{3, 4}, {.5, false, 2}, Aggregate{Quantity{std::int64_t{1}}}},
        {{4, 5}, {1, false, 4}, Aggregate{Quantity{std::int64_t{2}}}}
    };
    auto Plot = MakePlot(Row);
    assert(Plot.size() == 3 && Plot[1].size() == 1 && Plot[1][0].value == 0);
    assert(Plot[2][0].value == 120); // Known subtotal with provisional source coverage.
    auto Unknown = Row; Unknown.summary.value.reset();
    assert(!DisplayValue(Item, Unknown.summary));
    assert(SortBefore(Row, Unknown, false));
    assert(RelativeBar(-5., 10) == .5 && RelativeBar(std::nullopt, 10) == 0);
    assert(ColourKey(Item) == ColourKey(Item));
    Row.series = Storage; Row.points = {{{10, 20}, {10, true, 0}, Aggregate{EnergyState{5, 8}}}};
    assert(MakePlot(Row)[0][0].time == 20 && MakePlot(Row)[0][0].value == 5);
    History Large(10);
    assert(Large.SetQuantityCoverage(Category::Items, true, true, 0, {true, 10}) == Error::None);
    for (int i = 0; i < 500; ++i)
    {
        auto Id = Item; Id.key = "/Mod/Item.Item_" + std::to_string(i);
        assert(Large.Record({Id, 0, std::int64_t{1}, "/Mod/Source.Source_C"}, {true, 10}) == Error::None);
    }
    assert(Large.AdvanceTo(4000001, {true, 10}) == Error::None);
    std::vector<std::uint8_t> LargeBytes;
    assert(Large.EncodeSave(LargeBytes) == Error::None);
    History LargeLoaded(11);
    assert(LargeLoaded.DecodeSave(LargeBytes, {true, 11}) == Error::None && LargeLoaded.SeriesCount() == 500);
    assert(LargeLoaded.AllocatedBytes() <= History::MemoryLimit && LargeBytes.size() <= History::SaveByteLimit);
    std::cout << "T07 view and T08 persistence checks passed; 500-series encoded bytes=" << LargeBytes.size() << '\n';
}
