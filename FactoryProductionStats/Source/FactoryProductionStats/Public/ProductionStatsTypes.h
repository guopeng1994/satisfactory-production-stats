// SPDX-License-Identifier: 0BSD
#pragma once

// T02 draft: canonical values only. No UE objects, raw game units or network IDs.
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace FactoryProductionStats
{
enum class Category { Items, Fluids, Power };
enum class Direction { None, Produced, Consumed };
enum class Scope { World, BuildingType, Unclassified };
enum class Metric
{
    Quantity, ActualConsumption, ActualProduction, ProductionCapacity,
    MaximumDemand, ChargePower, DischargePower, ProductionBoost,
    StoredEnergy, StorageCapacity
};
enum class Unit { Items, CubicMetres, Megawatts, MegawattHours };
enum class Error
{
    None, InvalidSeries, InvalidTime, InvalidValue, UnitMismatch,
    Overflow, OutOfOrder, NoCoverage, UnsupportedSchema
};
enum class GapReason : std::uint32_t
{
    None = 0, BeforeRecording = 1, CollectorUnavailable = 2,
    UnsupportedSource = 4, ResourceUnavailable = 8, InvalidSample = 16
};

// Resource/building keys are full class paths, never display names or addresses.
struct SeriesId
{
    Category category = Category::Items;
    Metric metric = Metric::Quantity;
    Direction direction = Direction::Produced;
    Scope scope = Scope::World;
    std::string key;
    auto operator<=>(const SeriesId&) const = default;
};

inline bool IsClassPath(const std::string& key)
{
    return key.starts_with('/') && key.find('.') != std::string::npos;
}

inline Error Validate(const SeriesId& id)
{
    if (id.category != Category::Items && id.category != Category::Fluids &&
        id.category != Category::Power) return Error::InvalidSeries;
    if (id.category != Category::Power)
        return id.metric == Metric::Quantity && id.scope == Scope::World &&
            (id.direction == Direction::Produced || id.direction == Direction::Consumed) &&
            IsClassPath(id.key) ? Error::None : Error::InvalidSeries;
    if (id.direction != Direction::None || id.metric <= Metric::Quantity ||
        id.metric > Metric::StorageCapacity) return Error::InvalidSeries;
    if (id.scope == Scope::BuildingType)
        return IsClassPath(id.key) ? Error::None : Error::InvalidSeries;
    if (id.scope == Scope::Unclassified &&
        (id.metric == Metric::StoredEnergy || id.metric == Metric::StorageCapacity))
        return Error::InvalidSeries;
    return (id.scope == Scope::World || id.scope == Scope::Unclassified) && id.key.empty()
        ? Error::None : Error::InvalidSeries;
}

inline Unit UnitFor(const SeriesId& id)
{
    if (id.category == Category::Items) return Unit::Items;
    if (id.category == Category::Fluids) return Unit::CubicMetres;
    return id.metric == Metric::StoredEnergy || id.metric == Metric::StorageCapacity
        ? Unit::MegawattHours : Unit::Megawatts;
}

// Seconds on this save's simulation clock. Paused/offline time never advances it.
struct TimeRange
{
    double begin = 0;
    double end = 0;
};
inline bool IsTime(double time) { return std::isfinite(time) && time >= 0; }
inline Error Validate(TimeRange range)
{
    return IsTime(range.begin) && IsTime(range.end) && range.end > range.begin
        ? Error::None : Error::InvalidTime;
}

// The game adapter supplies this context from the authoritative world's owner.
// Epoch is transient and renewed after every world load, including an old save.
struct WriteContext { bool authoritative = false; std::uint64_t epoch = 0; };
inline bool CanWrite(WriteContext context, std::uint64_t activeEpoch)
{
    return context.authoritative && activeEpoch != 0 && context.epoch == activeEpoch;
}

using Quantity = std::variant<std::int64_t, double>; // Items / canonical m³.
inline Error ValidateQuantity(const SeriesId& id, const Quantity& value)
{
    if (Validate(id) != Error::None || id.category == Category::Power)
        return Error::InvalidSeries;
    if (id.category == Category::Items)
    {
        const auto* count = std::get_if<std::int64_t>(&value);
        return !count ? Error::UnitMismatch : (*count >= 0 ? Error::None : Error::InvalidValue);
    }
    const auto* volume = std::get_if<double>(&value);
    return !volume ? Error::UnitMismatch :
        (std::isfinite(*volume) && *volume >= 0 ? Error::None : Error::InvalidValue);
}

struct QuantityEvent
{
    SeriesId series;
    double time = 0;
    Quantity quantity = std::int64_t{0};
    std::string sourceClassPath;
};
inline Error Validate(const QuantityEvent& event)
{
    if (!IsTime(event.time)) return Error::InvalidTime;
    if (!IsClassPath(event.sourceClassPath)) return Error::InvalidSeries;
    const auto error = ValidateQuantity(event.series, event.quantity);
    if (error != Error::None) return error;
    return std::visit([](auto value) { return value > 0; }, event.quantity)
        ? Error::None : Error::InvalidValue;
}

// One metric per reading: missing is distinct from a known zero. Residuals may
// be signed; physical consumption/production/capacity/charge/discharge may not.
struct PowerReading
{
    SeriesId series;
    std::optional<double> value;
    std::optional<std::uint32_t> coveredDeviceCount; // Current count, not a mean.
};
inline Error Validate(const PowerReading& reading)
{
    if (Validate(reading.series) != Error::None || reading.series.category != Category::Power)
        return Error::InvalidSeries;
    if (!reading.value) return Error::None;
    return std::isfinite(*reading.value) &&
        (*reading.value >= 0 || reading.series.scope == Scope::Unclassified)
        ? Error::None : Error::InvalidValue;
}
struct PowerSnapshot
{
    double time = 0;
    std::vector<PowerReading> readings;
    std::optional<std::uint32_t> networkCount;
    std::optional<std::uint32_t> trippedNetworkCount;
};
inline Error Validate(const PowerSnapshot& snapshot)
{
    if (!IsTime(snapshot.time)) return Error::InvalidTime;
    if (snapshot.trippedNetworkCount && (!snapshot.networkCount ||
        *snapshot.trippedNetworkCount > *snapshot.networkCount)) return Error::InvalidValue;
    std::set<SeriesId> seen;
    for (const auto& reading : snapshot.readings)
    {
        const auto error = Validate(reading);
        if (error != Error::None) return error;
        if (!seen.insert(reading.series).second) return Error::InvalidSeries;
    }
    return Error::None;
}

struct CoverageGap
{
    Category category = Category::Items;
    TimeRange range;
    GapReason reason = GapReason::CollectorUnavailable;
    std::string sourceClassPath; // Empty: entire category; otherwise missing source.
};
inline Error Validate(const CoverageGap& gap)
{
    if (Validate(gap.range) != Error::None) return Error::InvalidTime;
    if (gap.category < Category::Items || gap.category > Category::Power ||
        gap.reason < GapReason::BeforeRecording || gap.reason > GapReason::InvalidSample ||
        (!gap.sourceClassPath.empty() && !IsClassPath(gap.sourceClassPath)))
        return Error::InvalidValue;
    return Error::None;
}
struct Coverage
{
    double observedSeconds = 0; // Union duration, not sum of machine durations.
    bool completeSources = false;
    std::uint32_t gapReasons = 0; // Bitwise GapReason flags; partial bins break lines.
};

// Three aggregation rules. Last energy is never added or averaged as power.
struct PowerIntegral { double megawattSeconds = 0; };
struct EnergyState { double megawattHours = 0; double time = 0; };
using Aggregate = std::variant<Quantity, PowerIntegral, EnergyState>;
struct Bucket
{
    TimeRange range; // Events: [begin,end); energy: last valid sample <= end.
    Coverage coverage;
    std::optional<Aggregate> value; // No observation -> no numeric value.
};
inline Error Validate(const SeriesId& series, const Bucket& bucket)
{
    if (Validate(series) != Error::None) return Error::InvalidSeries;
    if (Validate(bucket.range) != Error::None) return Error::InvalidTime;
    const auto& coverage = bucket.coverage;
    if (!std::isfinite(coverage.observedSeconds) || coverage.observedSeconds < 0 ||
        coverage.observedSeconds > bucket.range.end - bucket.range.begin ||
        coverage.gapReasons > 31) return Error::InvalidValue;
    if (coverage.observedSeconds == 0)
        return bucket.value ? Error::InvalidValue : Error::None;
    if (!bucket.value) return Error::InvalidValue;
    if (series.category != Category::Power)
    {
        const auto* quantity = std::get_if<Quantity>(&*bucket.value);
        return quantity ? ValidateQuantity(series, *quantity) : Error::UnitMismatch;
    }
    if (UnitFor(series) == Unit::MegawattHours)
    {
        const auto* state = std::get_if<EnergyState>(&*bucket.value);
        if (!state) return Error::UnitMismatch;
        return std::isfinite(state->megawattHours) && state->megawattHours >= 0 &&
            IsTime(state->time) && state->time >= bucket.range.begin &&
            state->time <= bucket.range.end ? Error::None : Error::InvalidValue;
    }
    const auto* integral = std::get_if<PowerIntegral>(&*bucket.value);
    if (!integral) return Error::UnitMismatch;
    return std::isfinite(integral->megawattSeconds) &&
        (integral->megawattSeconds >= 0 || series.scope == Scope::Unclassified)
        ? Error::None : Error::InvalidValue;
}

inline Error CheckedAddItems(std::int64_t& total, std::int64_t count)
{
    if (total < 0 || count < 0) return Error::InvalidValue;
    if (count > std::numeric_limits<std::int64_t>::max() - total) return Error::Overflow;
    total += count;
    return Error::None;
}
inline std::optional<double> RatePerMinute(double amount, double observedSeconds)
{
    if (!std::isfinite(amount) || amount < 0 || !std::isfinite(observedSeconds) ||
        observedSeconds <= 0) return std::nullopt;
    const double rate = (amount / observedSeconds) * 60;
    return std::isfinite(rate) ? std::optional<double>{rate} : std::nullopt;
}
inline std::optional<double> AverageMegawatts(PowerIntegral integral, double observedSeconds)
{
    if (!std::isfinite(integral.megawattSeconds) || !std::isfinite(observedSeconds) ||
        observedSeconds <= 0) return std::nullopt;
    const double average = integral.megawattSeconds / observedSeconds;
    return std::isfinite(average) ? std::optional<double>{average} : std::nullopt;
}

enum class Window { Seconds5, Minute1, Minutes10, Hour1, Hours10, Hours50, Hours250, Hours1000, All };
inline std::optional<double> WindowSeconds(Window window)
{
    switch (window)
    {
        case Window::Seconds5: return 5;
        case Window::Minute1: return 60;
        case Window::Minutes10: return 600;
        case Window::Hour1: return 3600;
        case Window::Hours10: return 36000;
        case Window::Hours50: return 180000;
        case Window::Hours250: return 900000;
        case Window::Hours1000: return 3600000;
        case Window::All: return std::nullopt;
    }
    return std::nullopt;
}
inline constexpr std::uint32_t MaxQueryPoints = 600;
struct Query
{
    Category category = Category::Items;
    Window window = Window::Minute1;
    double endTime = 0;
    std::vector<SeriesId> series; // Empty: observed series in category.
    std::uint32_t maxPoints = MaxQueryPoints;
};
inline Error Validate(const Query& query)
{
    if (!IsTime(query.endTime)) return Error::InvalidTime;
    if (query.maxPoints == 0 || query.maxPoints > MaxQueryPoints ||
        query.window < Window::Seconds5 || query.window > Window::All ||
        query.category < Category::Items || query.category > Category::Power)
        return Error::InvalidValue;
    std::set<SeriesId> seen;
    for (const auto& series : query.series)
        if (Validate(series) != Error::None || series.category != query.category ||
            !seen.insert(series).second)
            return Error::InvalidSeries;
    return Error::None;
}
struct SeriesResult
{
    SeriesId series;
    std::string displayName; // Missing resource preserves key/history.
    std::string iconPath;
    bool resourceResolved = false;
    Unit unit = Unit::Items;
    Bucket summary;
    std::vector<Bucket> points; // <= query.maxPoints; partial/unknown break lines.
};
struct QueryResult
{
    Error error = Error::None;
    TimeRange requestedRange;
    TimeRange actualRange;
    double resolutionSeconds = 0;
    bool approximateBoundary = false;
    std::vector<SeriesResult> series;
};

// Storage DTO only: T08 must implement the real game save chain. No SaveGame
// claim is implied. Draft 0 is not a released serialization format.
inline constexpr std::uint32_t DraftSchemaVersion = 0;
struct HistoryLevel { double resolutionSeconds = 0; std::vector<Bucket> buckets; };
struct SavedSeries
{
    SeriesId series;
    Bucket lifetime;
    std::vector<HistoryLevel> levels; // T03 caps every level; never raw event history.
    std::vector<Bucket> allTrend; // T03 bounded merge, candidate limit 512.
};
struct SaveData
{
    std::uint32_t schemaVersion = DraftSchemaVersion;
    double recordingStart = 0;
    double clock = 0;
    std::vector<SavedSeries> series;
};
inline Error CheckSchema(std::uint32_t schema)
{
    return schema == DraftSchemaVersion ? Error::None : Error::UnsupportedSchema;
}
} // namespace FactoryProductionStats
