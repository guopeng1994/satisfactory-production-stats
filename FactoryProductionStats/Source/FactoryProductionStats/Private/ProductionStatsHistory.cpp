// SPDX-License-Identifier: 0BSD
#include "ProductionStatsHistory.h"
#include <algorithm>
#include <numeric>

namespace FactoryProductionStats
{
namespace
{
SeriesId CoverageId(Category category)
{
    return {category, Metric::Quantity, Direction::Produced, Scope::World, "/FactoryProductionStats/Coverage.Internal"};
}
bool IsEnergy(const SeriesId& id) { return UnitFor(id) == Unit::MegawattHours; }
std::int64_t Index(double time, double resolution) { return static_cast<std::int64_t>(std::floor(time / resolution)); }
double QuantityDouble(const Quantity& value) { return std::visit([](auto number) { return static_cast<double>(number); }, value); }
constexpr std::uint32_t Unavailable = static_cast<std::uint32_t>(GapReason::CollectorUnavailable);
}

History::History(std::uint64_t epoch, double recordingStart) : epoch(epoch), start(recordingStart), clock(recordingStart)
{
    // Construction is local; boundary methods reject an invalid start/epoch.
    for (std::size_t i = 0; i < 2; ++i) quantities[i].history = NewSeries(CoverageId(static_cast<Category>(i)));
}

History::Series History::NewSeries(const SeriesId& id) const
{
    Series result;
    for (std::size_t i = 0; i < Capacities.size(); ++i) result.rings[i].resize(Capacities[i]);
    result.all.reserve(AllTrendLimit + 1);
    result.lifetime.begin = start;
    result.lifetime.end = clock;
    result.lifetime.complete = false;
    if (clock > start)
    {
        result.lifetime.reasons = Unavailable;
        result.all.push_back(result.lifetime);
    }
    if (id.category == Category::Fluids) result.pending = 0.0;
    return result;
}

void History::ClearQuantities(Series& entry)
{
    const auto clear = [](Cell& cell) { cell.items = 0; cell.value = 0; };
    clear(entry.lifetime);
    for (auto& ring : entry.rings) for (auto& cell : ring) clear(cell);
    for (auto& cell : entry.all) clear(cell);
}

std::size_t History::AllocatedBytes() const
{
    const auto bytes = [](const Series& entry)
    {
        std::size_t sum = sizeof(Series) + entry.all.capacity() * sizeof(Cell);
        for (const auto& ring : entry.rings) sum += ring.capacity() * sizeof(Cell);
        return sum;
    };
    std::size_t total = sizeof(*this);
    for (const auto& state : quantities) total += bytes(state.history);
    for (const auto& [id, entry] : series) total += bytes(entry) + sizeof(SeriesId) + id.key.capacity() + 64;
    return total;
}

Error History::SetQuantityCoverage(Category category, bool observed, bool complete, std::uint32_t reasons, WriteContext context)
{
    if (!CanWrite(context, epoch)) return Error::NotAuthoritative;
    if (!IsTime(start) || !IsTime(clock) || clock >= MaxClock) return Error::InvalidTime;
    if (category != Category::Items && category != Category::Fluids) return Error::InvalidSeries;
    if (reasons > 31 || (!observed && complete)) return Error::InvalidValue;
    auto& state = quantities[static_cast<std::size_t>(category)];
    state.observed = observed;
    state.complete = complete;
    state.reasons = observed ? reasons : reasons | Unavailable;
    return Error::None;
}

Error History::Record(const QuantityEvent& event, WriteContext context)
{
    if (!CanWrite(context, epoch)) return Error::NotAuthoritative;
    if (const auto error = Validate(event); error != Error::None) return error;
    if (event.time < clock) return Error::OutOfOrder;
    if (event.time != clock || event.time >= MaxClock) return Error::InvalidTime;
    const auto& category = quantities[static_cast<std::size_t>(event.series.category)];
    if (!category.observed) return Error::NoCoverage;
    auto found = series.find(event.series);
    if (found == series.end())
    {
        const auto perSeries = (std::accumulate(Capacities.begin(), Capacities.end(), std::size_t{0}) + AllTrendLimit + 1) * sizeof(Cell) + sizeof(Series) + event.series.key.size() + 256;
        if (event.series.key.size() > 4096 || AllocatedBytes() + perSeries > MemoryLimit) return Error::CapacityExceeded;
        auto entry = category.history; // Inherit category coverage before first event.
        entry.all.reserve(AllTrendLimit + 1);
        ClearQuantities(entry);
        entry.pending = event.series.category == Category::Items ? Quantity{std::int64_t{0}} : Quantity{0.0};
        found = series.emplace(event.series, std::move(entry)).first;
    }
    auto& entry = found->second;
    if (event.series.category == Category::Items)
    {
        auto count = std::get<std::int64_t>(entry.pending);
        const auto add = std::get<std::int64_t>(event.quantity);
        auto lifetime = entry.lifetime.items;
        if (CheckedAddItems(count, add) != Error::None || CheckedAddItems(lifetime, count) != Error::None) return Error::Overflow;
        entry.pending = count;
    }
    else
    {
        const double pending = std::get<double>(entry.pending) + std::get<double>(event.quantity);
        if (!std::isfinite(pending) || !std::isfinite(entry.lifetime.value + pending)) return Error::Overflow;
        entry.pending = pending;
    }
    return Error::None;
}

Error History::Record(const PowerSnapshot& snapshot, WriteContext context)
{
    if (!CanWrite(context, epoch)) return Error::NotAuthoritative;
    if (const auto error = Validate(snapshot); error != Error::None) return error;
    if (snapshot.time < clock) return Error::OutOfOrder;
    if (snapshot.time != clock || snapshot.time >= MaxClock) return Error::InvalidTime;
    std::size_t missing = 0;
    for (const auto& reading : snapshot.readings)
    {
        if (reading.series.key.size() > 4096) return Error::CapacityExceeded;
        if (reading.value && std::abs(*reading.value) > std::numeric_limits<double>::max() / MaxClock / 4) return Error::Overflow;
        missing += !series.contains(reading.series);
    }
    const auto perSeries = (std::accumulate(Capacities.begin(), Capacities.end(), std::size_t{0}) + AllTrendLimit + 1) * sizeof(Cell) + sizeof(Series) + 8192;
    if (missing > MemoryLimit / perSeries || AllocatedBytes() + missing * perSeries > MemoryLimit) return Error::CapacityExceeded;
    for (auto& [id, entry] : series) if (id.category == Category::Power) { entry.latest.reset(); entry.complete = false; }
    for (const auto& reading : snapshot.readings)
    {
        auto found = series.find(reading.series);
        if (found == series.end()) found = series.emplace(reading.series, NewSeries(reading.series)).first;
        found->second.latest = reading.value;
        found->second.latestTime = clock;
        found->second.complete = reading.completeSources;
    }
    return Error::None;
}

History::Cell History::Merge(Cell a, const Cell& b, const SeriesId& id)
{
    if (a.end <= a.begin) return b;
    a.end = b.end;
    a.covered = std::min(a.end - a.begin, a.covered + b.covered);
    a.complete = a.complete && b.complete;
    a.reasons |= b.reasons;
    if (id.category == Category::Items) a.items += b.items;
    else if (IsEnergy(id))
    {
        if (b.covered > 0 && (a.covered == b.covered || b.stateTime >= a.stateTime)) { a.value = b.value; a.stateTime = b.stateTime; }
    }
    else a.value += b.value;
    return a;
}

void History::AddInterval(Series& entry, const SeriesId& id, const Cell& span)
{
    const auto initialLifetime = entry.lifetime.end <= entry.lifetime.begin;
    entry.lifetime = initialLifetime ? span : Merge(entry.lifetime, span, id);
    for (std::size_t level = 0; level < Resolutions.size(); ++level)
    {
        const double resolution = Resolutions[level];
        const auto first = Index(span.begin, resolution);
        const auto last = Index(std::nextafter(span.end, -std::numeric_limits<double>::infinity()), resolution);
        // At most one ring's capacity even after a million-hour jump.
        for (auto index = std::max(first, last - static_cast<std::int64_t>(Capacities[level]) + 1); index <= last; ++index)
        {
            auto& cell = entry.rings[level][static_cast<std::size_t>(index % static_cast<std::int64_t>(Capacities[level]))];
            if (cell.end <= cell.begin || Index(cell.begin, resolution) != index)
            {
                cell = {};
                cell.begin = std::max(start, index * resolution);
                cell.end = cell.begin;
                if (cell.begin < span.begin) { cell.complete = false; cell.reasons = Unavailable; }
            }
            const double begin = std::max(span.begin, index * resolution);
            const double end = std::min(span.end, (index + 1) * resolution);
            cell.end = end;
            if (span.covered > 0) cell.covered = std::min(cell.end - cell.begin, cell.covered + end - begin);
            cell.complete = cell.complete && span.complete;
            cell.reasons |= span.reasons;
            if (id.category == Category::Items) { if (index == first) cell.items += span.items; }
            else if (id.category == Category::Fluids) { if (index == first) cell.value += span.value; }
            else if (IsEnergy(id))
            {
                if (span.covered > 0) { cell.value = span.value; cell.stateTime = span.stateTime; }
            }
            else if (span.covered > 0) cell.value += span.value * ((end - begin) / (span.end - span.begin));
        }
    }
    if (!entry.all.empty() && Index(entry.all.back().begin, 1) == Index(span.begin, 1) &&
        entry.all.back().end == span.begin && span.end <= std::floor(span.begin) + 1)
        entry.all.back() = Merge(entry.all.back(), span, id);
    else entry.all.push_back(span);
    if (entry.all.size() > AllTrendLimit)
    {
        std::size_t output = 0;
        for (std::size_t i = 0; i < entry.all.size(); i += 2)
            entry.all[output++] = i + 1 < entry.all.size() ? Merge(entry.all[i], entry.all[i + 1], id) : entry.all[i];
        entry.all.resize(output);
    }
}

Error History::AdvanceTo(double endTime, WriteContext context)
{
    if (!CanWrite(context, epoch)) return Error::NotAuthoritative;
    if (!IsTime(start) || !IsTime(endTime) || endTime >= MaxClock || endTime < start) return Error::InvalidTime;
    if (endTime < clock) return Error::OutOfOrder;
    if (endTime == clock) return Error::None; // Pause: no sample, coverage or time accrues.
    const double elapsed = endTime - clock;
    for (auto& [id, entry] : series)
    {
        Cell span;
        span.begin = clock; span.end = endTime;
        if (id.category == Category::Power)
        {
            span.complete = entry.complete && entry.latest.has_value();
            span.covered = entry.latest ? elapsed : 0;
            span.reasons = entry.latest ? (entry.complete ? 0 : static_cast<std::uint32_t>(GapReason::UnsupportedSource)) : Unavailable;
            span.value = entry.latest ? (IsEnergy(id) ? *entry.latest : *entry.latest * elapsed) : 0;
            span.stateTime = entry.latestTime;
        }
        else
        {
            const auto& state = quantities[static_cast<std::size_t>(id.category)];
            span.covered = state.observed ? elapsed : 0;
            span.complete = state.complete;
            span.reasons = state.reasons;
            if (id.category == Category::Items) span.items = std::get<std::int64_t>(entry.pending);
            else span.value = std::get<double>(entry.pending);
            entry.pending = id.category == Category::Items ? Quantity{std::int64_t{0}} : Quantity{0.0};
        }
        AddInterval(entry, id, span);
    }
    for (std::size_t i = 0; i < quantities.size(); ++i)
    {
        auto& state = quantities[i];
        Cell span;
        span.begin = clock; span.end = endTime;
        span.covered = state.observed ? elapsed : 0;
        span.complete = state.complete; span.reasons = state.reasons;
        AddInterval(state.history, CoverageId(static_cast<Category>(i)), span);
    }
    clock = endTime;
    return Error::None;
}

Bucket History::ToBucket(const Cell& cell, const SeriesId& id)
{
    Bucket bucket{{cell.begin, cell.end}, {cell.covered, cell.complete, cell.reasons}, std::nullopt};
    if (cell.covered > 0 || (id.category == Category::Items && cell.items > 0) ||
        (id.category == Category::Fluids && cell.value > 0))
    {
        if (id.category == Category::Items) bucket.value = Aggregate{Quantity{cell.items}};
        else if (id.category == Category::Fluids) bucket.value = Aggregate{Quantity{cell.value}};
        else if (IsEnergy(id)) bucket.value = Aggregate{EnergyState{cell.value, cell.stateTime}};
        else bucket.value = Aggregate{PowerIntegral{cell.value}};
    }
    return bucket;
}

History::Cell History::FromBucket(const Bucket& bucket)
{
    Cell cell;
    cell.begin = bucket.range.begin; cell.end = bucket.range.end;
    cell.covered = bucket.coverage.observedSeconds; cell.complete = bucket.coverage.completeSources;
    cell.reasons = bucket.coverage.gapReasons;
    if (bucket.value)
    {
        if (const auto* quantity = std::get_if<Quantity>(&*bucket.value))
        {
            if (const auto* count = std::get_if<std::int64_t>(quantity)) cell.items = *count;
            else cell.value = std::get<double>(*quantity);
        }
        else if (const auto* integral = std::get_if<PowerIntegral>(&*bucket.value)) cell.value = integral->megawattSeconds;
        else { const auto& state = std::get<EnergyState>(*bucket.value); cell.value = state.megawattHours; cell.stateTime = state.time; }
    }
    return cell;
}

QueryResult History::QueryHistory(const Query& query) const
{
    QueryResult result;
    if ((result.error = Validate(query)) != Error::None) return result;
    if (!IsTime(start) || !IsTime(clock) || clock < start || clock >= MaxClock)
    { result.error = Error::InvalidTime; return result; }
    if (query.endTime > clock || query.endTime <= start) { result.error = Error::NoCoverage; return result; }
    const auto window = WindowSeconds(query.window);
    const double requestedBegin = window ? std::max(start, query.endTime - *window) : start;
    result.requestedRange = {requestedBegin, query.endTime};
    std::size_t chosen = Resolutions.size() - 1;
    bool retained = false;
    if (window)
    {
        for (std::size_t level = 0; level < Resolutions.size(); ++level)
        {
            const auto last = Index(std::nextafter(clock, -std::numeric_limits<double>::infinity()), Resolutions[level]);
            const double oldest = std::max(start, (last - static_cast<std::int64_t>(Capacities[level]) + 1) * Resolutions[level]);
            if (requestedBegin >= oldest)
            {
                chosen = level; retained = true;
                if (std::ceil(query.endTime / Resolutions[level]) - std::floor(requestedBegin / Resolutions[level]) <= query.maxPoints) break;
            }
        }
        if (!retained) { result.error = Error::NoCoverage; return result; }
        result.resolutionSeconds = Resolutions[chosen];
        result.actualRange = {std::max(start, std::floor(requestedBegin / Resolutions[chosen]) * Resolutions[chosen]),
            std::min(clock, std::ceil(query.endTime / Resolutions[chosen]) * Resolutions[chosen])};
    }
    else result.actualRange = {start, query.endTime};
    const auto selected = [&](const SeriesId& id)
    {
        return id.category == query.category && (query.series.empty() || std::find(query.series.begin(), query.series.end(), id) != query.series.end());
    };
    std::vector<SeriesId> ids;
    if (query.series.empty()) { for (const auto& [id, entry] : series) if (selected(id)) ids.push_back(id); }
    else ids = query.series;
    for (const auto& id : ids)
    {
        const auto found = series.find(id);
        std::vector<Cell> cells;
        if (!window)
        {
            const Series* entry = found != series.end() ? &found->second :
                (id.category == Category::Power ? nullptr : &quantities[static_cast<std::size_t>(id.category)].history);
            if (!entry)
            {
                Cell unknown;
                unknown.begin = start; unknown.end = query.endTime; unknown.complete = false; unknown.reasons = Unavailable;
                cells.push_back(unknown);
            }
            else for (const auto& cell : entry->all)
            {
                if (cell.begin >= query.endTime) break;
                auto output = cell;
                if (found == series.end()) { output.items = 0; output.value = 0; }
                cells.push_back(output);
                result.actualRange.end = std::max(result.actualRange.end, cell.end);
                result.resolutionSeconds = std::max(result.resolutionSeconds, cell.end - cell.begin);
            }
        }
        else
        {
            const double resolution = Resolutions[chosen];
            const auto first = Index(result.actualRange.begin, resolution);
            const auto last = Index(std::nextafter(result.actualRange.end, -std::numeric_limits<double>::infinity()), resolution);
            const Series* entry = found != series.end() ? &found->second :
                (id.category == Category::Power ? nullptr : &quantities[static_cast<std::size_t>(id.category)].history);
            for (auto index = first; index <= last; ++index)
            {
                Cell cell;
                cell.begin = std::max(start, index * resolution); cell.end = std::min(clock, (index + 1) * resolution);
                cell.complete = false; cell.reasons = Unavailable;
                if (entry)
                {
                    const auto& stored = entry->rings[chosen][static_cast<std::size_t>(index % static_cast<std::int64_t>(Capacities[chosen]))];
                    if (stored.end > stored.begin && Index(stored.begin, resolution) == index) cell = stored;
                }
                if (found == series.end()) { cell.items = 0; cell.value = 0; }
                cells.push_back(cell);
            }
        }
        Cell summary;
        for (const auto& cell : cells) summary = Merge(summary, cell, id);
        if (!window && query.endTime == clock && found != series.end()) summary = found->second.lifetime;
        // Energy is a state at the endpoint; quantities and integrals remain half-open.
        if (IsEnergy(id) && query.endTime == clock && found != series.end() && found->second.latest)
        {
            if (summary.covered > 0) { summary.value = *found->second.latest; summary.stateTime = found->second.latestTime; }
            if (!cells.empty() && cells.back().covered > 0)
            { cells.back().value = *found->second.latest; cells.back().stateTime = found->second.latestTime; }
        }
        SeriesResult row;
        row.series = id; row.unit = UnitFor(id); row.summary = ToBucket(summary, id);
        const std::size_t stride = std::max<std::size_t>(1, (cells.size() + query.maxPoints - 1) / query.maxPoints);
        for (std::size_t i = 0; i < cells.size(); i += stride)
        {
            Cell point;
            for (std::size_t j = i; j < std::min(cells.size(), i + stride); ++j) point = Merge(point, cells[j], id);
            row.points.push_back(ToBucket(point, id));
            result.resolutionSeconds = std::max(result.resolutionSeconds, point.end - point.begin);
        }
        result.series.push_back(std::move(row));
    }
    result.approximateBoundary = result.actualRange.begin != result.requestedRange.begin || result.actualRange.end != result.requestedRange.end;
    if (result.series.empty() || std::none_of(result.series.begin(), result.series.end(), [](const auto& row) { return row.summary.coverage.observedSeconds > 0; }))
        result.error = Error::NoCoverage;
    return result;
}

SavedHistory History::ExportHistory(const Series& entry, const SeriesId& id) const
{
    SavedHistory saved;
    saved.lifetime = ToBucket(entry.lifetime, id);
    for (std::size_t level = 0; level < Resolutions.size(); ++level)
    {
        HistoryLevel output{Resolutions[level], {}};
        for (const auto& cell : entry.rings[level]) if (cell.end > cell.begin) output.buckets.push_back(ToBucket(cell, id));
        std::sort(output.buckets.begin(), output.buckets.end(), [](const auto& a, const auto& b) { return a.range.begin < b.range.begin; });
        saved.levels.push_back(std::move(output));
    }
    for (const auto& cell : entry.all) saved.allTrend.push_back(ToBucket(cell, id));
    return saved;
}

SaveData History::Export() const
{
    SaveData saved;
    saved.recordingStart = start; saved.clock = clock;
    for (const auto& [id, entry] : series)
        saved.series.push_back({id, ExportHistory(entry, id), entry.pending, entry.latest, entry.latestTime, entry.complete});
    for (std::size_t i = 0; i < quantities.size(); ++i)
    {
        const auto& state = quantities[i];
        saved.quantityCoverage.push_back({static_cast<Category>(i), ExportHistory(state.history, CoverageId(static_cast<Category>(i))), state.observed, state.complete, state.reasons});
    }
    return saved;
}

Error History::ImportHistory(Series& entry, const SeriesId& id, const SavedHistory& saved, double savedStart, double savedClock) const
{
    const auto valid = [&](const Bucket& bucket)
    {
        if (Validate(id, bucket) != Error::None || bucket.range.begin < savedStart || bucket.range.end > savedClock) return false;
        if (id.category == Category::Power && bucket.value)
        {
            const double limit = std::numeric_limits<double>::max() / MaxClock / 4;
            if (IsEnergy(id)) return std::get<EnergyState>(*bucket.value).megawattHours <= limit;
            return std::abs(std::get<PowerIntegral>(*bucket.value).megawattSeconds) <= limit * bucket.coverage.observedSeconds;
        }
        return true;
    };
    if (savedClock > savedStart)
    {
        if (!valid(saved.lifetime) || saved.lifetime.range.begin != savedStart || saved.lifetime.range.end != savedClock) return Error::InvalidValue;
    }
    else if (saved.lifetime.range.begin != savedStart || saved.lifetime.range.end != savedClock || saved.lifetime.value || saved.lifetime.coverage.observedSeconds != 0) return Error::InvalidValue;
    if (saved.levels.size() != Resolutions.size() || saved.allTrend.size() > AllTrendLimit) return Error::CapacityExceeded;
    entry.all.clear();
    entry.lifetime = FromBucket(saved.lifetime);
    for (std::size_t level = 0; level < Resolutions.size(); ++level)
    {
        const auto& input = saved.levels[level];
        if (input.resolutionSeconds != Resolutions[level] || input.buckets.size() > Capacities[level]) return Error::InvalidValue;
        double previousEnd = savedStart;
        std::set<std::size_t> slots;
        std::int64_t itemSum = 0;
        double absoluteSum = 0;
        const auto last = savedClock > savedStart ? Index(std::nextafter(savedClock, -std::numeric_limits<double>::infinity()), Resolutions[level]) : -1;
        for (const auto& bucket : input.buckets)
        {
            if (!valid(bucket) || bucket.range.begin < previousEnd) return Error::InvalidValue;
            const auto index = Index(bucket.range.begin, Resolutions[level]);
            if (index < last - static_cast<std::int64_t>(Capacities[level]) + 1 ||
                bucket.range.begin != std::max(savedStart, index * Resolutions[level]) ||
                bucket.range.end != std::min(savedClock, (index + 1) * Resolutions[level])) return Error::InvalidValue;
            const auto slot = static_cast<std::size_t>(index % static_cast<std::int64_t>(Capacities[level]));
            if (!slots.insert(slot).second) return Error::InvalidValue;
            const auto cell = FromBucket(bucket);
            if (CheckedAddItems(itemSum, cell.items) != Error::None) return Error::Overflow;
            if (!IsEnergy(id)) absoluteSum += std::abs(cell.value);
            if (!std::isfinite(absoluteSum) || absoluteSum > std::numeric_limits<double>::max() / 2) return Error::Overflow;
            entry.rings[level][slot] = cell;
            previousEnd = bucket.range.end;
        }
        if (id.category == Category::Items && itemSum > entry.lifetime.items) return Error::InvalidValue;
        if (id.category == Category::Fluids && absoluteSum > entry.lifetime.value + 1e-9 * std::max(1.0, entry.lifetime.value)) return Error::InvalidValue;
    }
    double previousEnd = savedStart;
    Cell total;
    for (const auto& bucket : saved.allTrend)
    {
        if (!valid(bucket) || bucket.range.begin != previousEnd) return Error::InvalidValue;
        const auto cell = FromBucket(bucket);
        if (id.category == Category::Items)
        {
            auto sum = total.items;
            if (CheckedAddItems(sum, cell.items) != Error::None) return Error::Overflow;
        }
        total = Merge(total, cell, id);
        if (!std::isfinite(total.value)) return Error::Overflow;
        entry.all.push_back(cell);
        previousEnd = cell.end;
    }
    if (previousEnd != savedClock) return Error::InvalidValue;
    const auto close = [](double a, double b) { return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)}); };
    if (total.items != entry.lifetime.items || !close(total.value, entry.lifetime.value) || !close(total.covered, entry.lifetime.covered) ||
        total.reasons != entry.lifetime.reasons || (savedClock > savedStart && total.complete != entry.lifetime.complete)) return Error::InvalidValue;
    return Error::None;
}

std::vector<SeriesId> History::SeriesIds(Category category) const
{
    std::vector<SeriesId> result;
    for (const auto& [id, entry] : series) if (id.category == category) result.push_back(id);
    return result;
}

Error History::ImportSeries(const SavedSeries& input)
{
    if (Validate(input.series) != Error::None || input.series.key.size() > 4096 || series.contains(input.series)) return Error::InvalidSeries;
    const auto perSeries = (std::accumulate(Capacities.begin(), Capacities.end(), std::size_t{0}) + AllTrendLimit + 1) * sizeof(Cell) + sizeof(Series) + sizeof(SeriesId) + input.series.key.size() + 256;
    if (AllocatedBytes() + perSeries > MemoryLimit) return Error::CapacityExceeded;
    if (!IsTime(input.latestPowerTime) || input.latestPowerTime > clock) return Error::InvalidTime;
    auto entry = NewSeries(input.series);
    if (auto error = ImportHistory(entry, input.series, input.history, start, clock); error != Error::None) return error;
    if (input.series.category != Category::Power)
    {
        if (auto error = ValidateQuantity(input.series, input.pending); error != Error::None) return error;
        if (input.latestPower) return Error::InvalidValue;
        if (input.series.category == Category::Items)
        {
            auto total = entry.lifetime.items;
            if (CheckedAddItems(total, std::get<std::int64_t>(input.pending)) != Error::None) return Error::Overflow;
        }
        else if (!std::isfinite(entry.lifetime.value + QuantityDouble(input.pending))) return Error::Overflow;
    }
    else
    {
        if (QuantityDouble(input.pending) != 0 || Validate(PowerReading{input.series, input.latestPower, std::nullopt, input.completeSources}) != Error::None ||
            !IsTime(input.latestPowerTime) || input.latestPowerTime > clock ||
            (input.latestPower && std::abs(*input.latestPower) > std::numeric_limits<double>::max() / MaxClock / 4)) return Error::InvalidValue;
        if (input.latestPower && (input.latestPowerTime < start ||
            (IsEnergy(input.series) && entry.lifetime.covered > 0 && input.latestPowerTime < entry.lifetime.stateTime))) return Error::InvalidTime;
    }
    entry.pending = input.pending; entry.latest = input.latestPower; entry.latestTime = input.latestPowerTime; entry.complete = input.completeSources;
    series.emplace(input.series, std::move(entry));
    return Error::None;
}

Error History::Restore(const SaveData& saved, WriteContext context)
{
    if (!CanWrite(context, epoch)) return Error::NotAuthoritative;
    if (const auto error = CheckSchema(saved.schemaVersion); error != Error::None) return error;
    if (!IsTime(saved.recordingStart) || !IsTime(saved.clock) || saved.clock < saved.recordingStart || saved.clock >= MaxClock) return Error::InvalidTime;
    const auto perSeries = (std::accumulate(Capacities.begin(), Capacities.end(), std::size_t{0}) + AllTrendLimit + 1) * sizeof(Cell) + sizeof(Series);
    if (saved.quantityCoverage.size() != 2 || saved.series.size() > MemoryLimit / perSeries - 2) return Error::CapacityExceeded;
    History restored(epoch, saved.recordingStart);
    restored.clock = saved.clock;
    for (std::size_t i = 0; i < 2; ++i)
    {
        const auto& input = saved.quantityCoverage[i];
        if (input.category != static_cast<Category>(i) || input.gapReasons > 31 || (!input.observed && input.completeSources)) return Error::InvalidValue;
        auto& state = restored.quantities[i];
        if (auto error = restored.ImportHistory(state.history, CoverageId(input.category), input.history, saved.recordingStart, saved.clock); error != Error::None) return error;
        state.observed = input.observed; state.complete = input.completeSources; state.reasons = input.gapReasons;
    }
    for (const auto& input : saved.series)
        if (auto error = restored.ImportSeries(input); error != Error::None) return error;
    if (restored.AllocatedBytes() > MemoryLimit) return Error::CapacityExceeded;
    *this = std::move(restored); // Invalid input never replaces the live history.
    return Error::None;
}
}
