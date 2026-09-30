// SPDX-License-Identifier: 0BSD
#include "ProductionStatsTypes.h"
#include <cassert>
#include <iostream>
#ifdef NDEBUG
#error This check requires enabled assertions.
#endif

using namespace FactoryProductionStats;

int main()
{
    const SeriesId solid{Category::Items, Metric::Quantity, Direction::Produced,
        Scope::World, "/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"};
    const SeriesId fluid{Category::Fluids, Metric::Quantity, Direction::Consumed,
        Scope::World, "/Game/FactoryGame/Resource/RawResources/Water/Desc_Water.Desc_Water_C"};
    const SeriesId power{Category::Power, Metric::ActualConsumption, Direction::None, Scope::World, ""};
    const SeriesId energy{Category::Power, Metric::StoredEnergy, Direction::None, Scope::World, ""};
    const SeriesId residual{Category::Power, Metric::ActualProduction, Direction::None, Scope::Unclassified, ""};
    const std::string source = "/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C";
    assert(Validate(QuantityEvent{solid, 5, std::int64_t{300}, source}) == Error::None);
    assert(Validate(QuantityEvent{fluid, 5, 120.0, source}) == Error::None);
    assert(ValidateQuantity(solid, 300.0) == Error::UnitMismatch);
    assert(ValidateQuantity(fluid, std::int64_t{120}) == Error::UnitMismatch);
    assert(Validate(QuantityEvent{solid, 5, std::int64_t{0}, source}) == Error::InvalidValue);
    assert(ValidateQuantity(fluid, -1.0) == Error::InvalidValue);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    assert(ValidateQuantity(fluid, nan) == Error::InvalidValue);
    assert(ValidateQuantity(fluid, inf) == Error::InvalidValue);
    assert(Validate(QuantityEvent{solid, nan, std::int64_t{1}, source}) == Error::InvalidTime);
    assert(!CanWrite({false, 2}, 2) && !CanWrite({true, 1}, 2));
    assert(!CanWrite({true, 0}, 0) && CanWrite({true, 2}, 2));

    PowerSnapshot snapshot{60, {{power, 100.0, 1}, {energy, 20.0, 1}, {residual, -5.0, std::nullopt}}, 2, 1};
    assert(Validate(snapshot) == Error::None);
    assert(Validate(PowerReading{power, -5.0, 1}) == Error::InvalidValue);
    assert(Validate(PowerReading{power, std::nullopt, std::nullopt}) == Error::None);
    snapshot.readings.push_back(snapshot.readings.front());
    assert(Validate(snapshot) == Error::InvalidSeries);
    snapshot.readings.pop_back();
    snapshot.trippedNetworkCount = 3;
    assert(Validate(snapshot) == Error::InvalidValue);

    // Every category shares Query/SeriesResult/Bucket; payload semantics differ.
    const Coverage known{60, true, 0};
    const Bucket itemBucket{{0, 60}, known, Aggregate{Quantity{std::int64_t{300}}}};
    const Bucket fluidBucket{{0, 60}, known, Aggregate{Quantity{120.0}}};
    const Bucket powerBucket{{0, 60}, known, Aggregate{PowerIntegral{6000}}};
    const Bucket energyBucket{{0, 60}, known, Aggregate{EnergyState{20, 60}}};
    const Bucket unknown{{20, 30}, {0, false, static_cast<std::uint32_t>(GapReason::CollectorUnavailable)}, std::nullopt};
    assert(Validate(solid, itemBucket) == Error::None);
    assert(Validate(fluid, fluidBucket) == Error::None);
    assert(Validate(power, powerBucket) == Error::None);
    assert(Validate(energy, energyBucket) == Error::None);
    assert(Validate(solid, unknown) == Error::None);
    const Bucket partial{{0, 60}, {20, false, static_cast<std::uint32_t>(GapReason::UnsupportedSource)},
        Aggregate{Quantity{std::int64_t{100}}}};
    assert(Validate(solid, partial) == Error::None);
    assert(Validate(power, energyBucket) == Error::UnitMismatch);
    assert(Validate(energy, powerBucket) == Error::UnitMismatch);
    assert(Validate(CoverageGap{Category::Items, {20, 30}, GapReason::CollectorUnavailable, source}) == Error::None);
    Bucket stopped{{0, 60}, known, Aggregate{Quantity{std::int64_t{0}}}};
    assert(Validate(solid, stopped) == Error::None && stopped.value && !unknown.value);
    stopped.coverage.observedSeconds = 61;
    assert(Validate(solid, stopped) == Error::InvalidValue);
    stopped.coverage.observedSeconds = 0;
    assert(Validate(solid, stopped) == Error::InvalidValue);

    assert(RatePerMinute(300, 60) == 300 && RatePerMinute(120, 60) == 120);
    assert(RatePerMinute(300, 20) == 900); // Only 20 recorded seconds, not 60.
    assert(RatePerMinute(25, 60) == 25); // Five seconds active + 55 stopped.
    assert(RatePerMinute(0, 60) == 0 && !RatePerMinute(0, 0));
    assert(!RatePerMinute(inf, 60) && !RatePerMinute(1, nan));
    assert(AverageMegawatts({6000}, 60) == 100);
    assert(std::abs(6000.0 / 3600 - 5.0 / 3) < 1e-12);
    assert(AverageMegawatts({-300}, 60) == -5); // Signed residual kept.
    assert(!AverageMegawatts({0}, 0));
    std::int64_t exact = 9007199254740993LL; // Preserve values beyond double integer precision.
    assert(CheckedAddItems(exact, 1) == Error::None && exact == 9007199254740994LL);
    exact = std::numeric_limits<std::int64_t>::max();
    assert(CheckedAddItems(exact, 1) == Error::Overflow && exact == std::numeric_limits<std::int64_t>::max());
    assert(CheckedAddItems(exact, -1) == Error::InvalidValue);

    for (Category category : {Category::Items, Category::Fluids, Category::Power})
    {
        Query query{category, Window::Minute1, 60, {}, 600};
        assert(Validate(query) == Error::None);
        query.maxPoints = 601;
        assert(Validate(query) == Error::InvalidValue);
    }
    assert(Validate(Query{Category::Fluids, Window::Minute1, 60, {solid}, 600}) == Error::InvalidSeries);
    assert(Validate(Query{Category::Items, Window::Minute1, 60, {solid, solid}, 600}) == Error::InvalidSeries);
    assert(Validate(Query{Category::Items, Window::All, nan, {}, 600}) == Error::InvalidTime);
    assert(WindowSeconds(Window::Hours1000) == 3600000 && !WindowSeconds(Window::All));
    assert(UnitFor(solid) == Unit::Items && UnitFor(fluid) == Unit::CubicMetres);
    assert(UnitFor(power) == Unit::Megawatts && UnitFor(energy) == Unit::MegawattHours);
    assert(CheckSchema(DraftSchemaVersion) == Error::None && CheckSchema(1) == Error::UnsupportedSchema);
    std::cout << "T02 canonical contract checks passed\n";
}
