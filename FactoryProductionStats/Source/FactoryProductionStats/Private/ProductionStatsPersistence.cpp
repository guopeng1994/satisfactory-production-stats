// SPDX-License-Identifier: 0BSD
#include "ProductionStatsHistory.h"
#include <algorithm>
#include <bit>
#include <numeric>

namespace FactoryProductionStats
{
namespace
{
constexpr std::uint32_t Magic = 0x48535046; // FPSH, little endian.
constexpr std::uint32_t FormatVersion = 1; // Unreleased; schema 0 remains provisional.
std::uint32_t Checksum(std::span<const std::uint8_t> bytes)
{
    static constexpr auto Table = []
    {
        std::array<std::uint32_t, 256> table{};
        for (std::uint32_t i = 0; i < table.size(); ++i)
        {
            auto value = i;
            for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1u)));
            table[i] = value;
        }
        return table;
    }();
    std::uint32_t crc = ~0u;
    for (auto byte : bytes) crc = (crc >> 8) ^ Table[(crc ^ byte) & 255u];
    return ~crc;
}
struct Writer
{
    std::vector<std::uint8_t> bytes;
    bool ok = true;
    void Integer(std::uint64_t value, std::size_t width)
    {
        if (bytes.size() > History::SaveByteLimit - width) { ok = false; return; }
        for (std::size_t i = 0; i < width; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
    void Number(double value) { Integer(std::bit_cast<std::uint64_t>(value), 8); }
    void Bool(bool value) { Integer(value ? 1 : 0, 1); }
    void Id(const SeriesId& id)
    {
        Integer(static_cast<unsigned>(id.category), 1); Integer(static_cast<unsigned>(id.metric), 1);
        Integer(static_cast<unsigned>(id.direction), 1); Integer(static_cast<unsigned>(id.scope), 1);
        Integer(id.key.size(), 4);
        for (auto c : id.key) Integer(static_cast<unsigned char>(c), 1);
    }
    void Amount(const Quantity& quantity)
    {
        Bool(std::holds_alternative<double>(quantity));
        if (const auto* items = std::get_if<std::int64_t>(&quantity)) Integer(static_cast<std::uint64_t>(*items), 8);
        else Number(std::get<double>(quantity));
    }
    void Bin(const Bucket& bin, const SeriesId& id)
    {
        Number(bin.range.begin); Number(bin.range.end); Number(bin.coverage.observedSeconds);
        Bool(bin.coverage.completeSources); Integer(bin.coverage.gapReasons, 4); Bool(bin.value.has_value());
        if (!bin.value) return;
        if (id.category != Category::Power) Amount(std::get<Quantity>(*bin.value));
        else if (UnitFor(id) == Unit::MegawattHours)
        { const auto& value = std::get<EnergyState>(*bin.value); Number(value.megawattHours); Number(value.time); }
        else Number(std::get<PowerIntegral>(*bin.value).megawattSeconds);
    }
    void Entry(const SavedHistory& saved, const SeriesId& id)
    {
        Bin(saved.lifetime, id);
        for (const auto& level : saved.levels)
        {
            Integer(level.buckets.size(), 4);
            for (const auto& bin : level.buckets) Bin(bin, id);
        }
        Integer(saved.allTrend.size(), 4);
        for (const auto& bin : saved.allTrend) Bin(bin, id);
    }
};
struct Reader
{
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
    bool ok = true;
    std::uint64_t Integer(std::size_t width)
    {
        if (width > bytes.size() - position) { ok = false; return 0; }
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < width; ++i) value |= static_cast<std::uint64_t>(bytes[position++]) << (8 * i);
        return value;
    }
    double Number() { return std::bit_cast<double>(Integer(8)); }
    bool Bool() { auto value = Integer(1); if (value > 1) ok = false; return value == 1; }
    SeriesId Id()
    {
        SeriesId id;
        id.category = static_cast<Category>(Integer(1)); id.metric = static_cast<Metric>(Integer(1));
        id.direction = static_cast<Direction>(Integer(1)); id.scope = static_cast<Scope>(Integer(1));
        const auto size = Integer(4);
        if (size > 4096 || size > bytes.size() - position) { ok = false; return id; }
        id.key.assign(reinterpret_cast<const char*>(bytes.data() + position), static_cast<std::size_t>(size));
        position += static_cast<std::size_t>(size);
        if (id.key.find('\0') != std::string::npos || Validate(id) != Error::None) ok = false;
        return id;
    }
    Quantity Amount()
    {
        if (Bool()) return Number();
        const auto value = Integer(8);
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) ok = false;
        return static_cast<std::int64_t>(value);
    }
    Bucket Bin(const SeriesId& id)
    {
        Bucket bin;
        bin.range = {Number(), Number()}; bin.coverage.observedSeconds = Number();
        bin.coverage.completeSources = Bool(); bin.coverage.gapReasons = static_cast<std::uint32_t>(Integer(4));
        if (Bool())
        {
            if (id.category != Category::Power) bin.value = Aggregate{Amount()};
            else if (UnitFor(id) == Unit::MegawattHours) bin.value = Aggregate{EnergyState{Number(), Number()}};
            else bin.value = Aggregate{PowerIntegral{Number()}};
        }
        return bin;
    }
    SavedHistory Entry(const SeriesId& id)
    {
        SavedHistory saved;
        saved.lifetime = Bin(id);
        for (std::size_t i = 0; i < History::Resolutions.size(); ++i)
        {
            const auto size = Integer(4);
            if (!ok || size > History::Capacities[i]) { ok = false; return saved; }
            HistoryLevel level{History::Resolutions[i], {}};
            for (std::size_t j = 0; j < size && ok; ++j) level.buckets.push_back(Bin(id));
            saved.levels.push_back(std::move(level));
        }
        const auto size = Integer(4);
        if (!ok || size > History::AllTrendLimit) { ok = false; return saved; }
        for (std::size_t j = 0; j < size && ok; ++j) saved.allTrend.push_back(Bin(id));
        return saved;
    }
};
SeriesId CoverageSeries(Category category)
{ return {category, Metric::Quantity, Direction::Produced, Scope::World, "/FactoryProductionStats/Coverage.Internal"}; }
}

Error History::EncodeSave(std::vector<std::uint8_t>& bytes) const
{
    if (!IsTime(start) || !IsTime(clock) || clock < start || clock >= MaxClock) return Error::InvalidTime;
    Writer output;
    output.Integer(Magic, 4); output.Integer(FormatVersion, 4); output.Integer(DraftSchemaVersion, 4);
    output.Number(start); output.Number(clock); output.Integer(series.size(), 4);
    // One series DTO at a time: never allocate an entire second history in DTO form.
    for (std::size_t i = 0; i < quantities.size(); ++i)
    {
        const auto& state = quantities[i];
        output.Bool(state.observed); output.Bool(state.complete); output.Integer(state.reasons, 4);
        const auto id = CoverageSeries(static_cast<Category>(i));
        output.Entry(ExportHistory(state.history, id), id);
    }
    for (const auto& [id, entry] : series)
    {
        output.Id(id); output.Amount(entry.pending); output.Bool(entry.latest.has_value());
        if (entry.latest) output.Number(*entry.latest);
        output.Number(entry.latestTime); output.Bool(entry.complete);
        output.Entry(ExportHistory(entry, id), id);
        if (!output.ok) return Error::CapacityExceeded;
    }
    output.Integer(Checksum(output.bytes), 4);
    if (!output.ok) return Error::CapacityExceeded;
    bytes = std::move(output.bytes);
    return Error::None;
}

Error History::DecodeSave(std::span<const std::uint8_t> bytes, WriteContext context)
{
    if (!CanWrite(context, epoch)) return Error::NotAuthoritative;
    if (bytes.size() > SaveByteLimit) return Error::CapacityExceeded;
    if (bytes.size() < 36) return Error::InvalidValue;
    Reader input{bytes.first(bytes.size() - 4)};
    if (input.Integer(4) != Magic) return Error::InvalidValue;
    if (input.Integer(4) != FormatVersion || CheckSchema(static_cast<std::uint32_t>(input.Integer(4))) != Error::None)
        return Error::UnsupportedSchema;
    Reader trailer{bytes.last(4)};
    if (trailer.Integer(4) != Checksum(input.bytes)) return Error::InvalidValue;
    const double savedStart = input.Number(), savedClock = input.Number();
    if (!IsTime(savedStart) || !IsTime(savedClock) || savedClock < savedStart || savedClock >= MaxClock) return Error::InvalidTime;
    const auto count = input.Integer(4);
    const auto perSeries = (std::accumulate(Capacities.begin(), Capacities.end(), std::size_t{0}) + AllTrendLimit + 1) * sizeof(Cell) + sizeof(Series);
    if (count > MemoryLimit / perSeries - 2) return Error::CapacityExceeded;
    History restored(epoch, savedStart);
    restored.clock = savedClock;
    for (std::size_t i = 0; i < quantities.size(); ++i)
    {
        auto& state = restored.quantities[i];
        state.observed = input.Bool(); state.complete = input.Bool(); state.reasons = static_cast<std::uint32_t>(input.Integer(4));
        if (state.reasons > 31 || (!state.observed && state.complete)) return Error::InvalidValue;
        const auto id = CoverageSeries(static_cast<Category>(i));
        const auto saved = input.Entry(id);
        if (!input.ok) return Error::InvalidValue;
        if (auto error = restored.ImportHistory(state.history, id, saved, savedStart, savedClock); error != Error::None) return error;
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        SavedSeries entry;
        entry.series = input.Id(); entry.pending = input.Amount();
        if (input.Bool()) entry.latestPower = input.Number();
        entry.latestPowerTime = input.Number(); entry.completeSources = input.Bool();
        if (!input.ok) return Error::InvalidValue;
        entry.history = input.Entry(entry.series);
        if (!input.ok) return Error::InvalidValue;
        if (auto error = restored.ImportSeries(entry); error != Error::None) return error;
    }
    if (!input.ok || input.position != input.bytes.size()) return Error::InvalidValue;
    if (restored.AllocatedBytes() > MemoryLimit) return Error::CapacityExceeded;
    *this = std::move(restored); // Includes epoch of this world; no epoch is read from disk.
    return Error::None;
}
}
