// SPDX-License-Identifier: 0BSD
#pragma once
#include "ProductionStatsTypes.h"
#include <array>
#include <map>

namespace FactoryProductionStats
{
// Single-thread owner. The game adapter drains callbacks before advancing time.
class History
{
public:
    static constexpr std::array<double, 5> Resolutions{1, 10, 60, 600, 3600};
    static constexpr std::array<std::size_t, 5> Capacities{601, 361, 601, 301, 1001};
    static constexpr std::size_t AllTrendLimit = 512;
    static constexpr std::size_t MemoryLimit = 128 * 1024 * 1024;
    static constexpr double MaxClock = 0x1p52;

    explicit History(std::uint64_t epoch, double recordingStart = 0);
    Error SetQuantityCoverage(Category category, bool observed, bool completeSources,
        std::uint32_t reasons, WriteContext context);
    Error Record(const QuantityEvent& event, WriteContext context);
    Error Record(const PowerSnapshot& snapshot, WriteContext context);
    Error AdvanceTo(double endTime, WriteContext context);
    QueryResult QueryHistory(const Query& query) const;
    SaveData Export() const;
    Error Restore(const SaveData& data, WriteContext context);
    double Clock() const { return clock; }
    std::uint64_t Epoch() const { return epoch; }
    std::size_t AllocatedBytes() const;
    std::size_t SeriesCount() const { return series.size(); }

private:
    // Compact cells keep the 500-series baseline below the 128 MiB budget.
    struct Cell
    {
        double begin = 0, end = 0, covered = 0, value = 0, stateTime = 0;
        std::int64_t items = 0;
        std::uint32_t reasons = 0;
        bool complete = true;
    };
    struct Series
    {
        std::array<std::vector<Cell>, 5> rings;
        std::vector<Cell> all;
        Cell lifetime;
        Quantity pending = std::int64_t{0};
        std::optional<double> latest;
        double latestTime = 0;
        bool complete = false;
    };
    struct CategoryState { Series history; bool observed = false, complete = false; std::uint32_t reasons = 2; };
    std::uint64_t epoch;
    double start, clock;
    std::array<CategoryState, 2> quantities;
    std::map<SeriesId, Series> series;

    Series NewSeries(const SeriesId& id) const;
    static void ClearQuantities(Series& entry);
    static Cell Merge(Cell a, const Cell& b, const SeriesId& id);
    static Bucket ToBucket(const Cell& cell, const SeriesId& id);
    static Cell FromBucket(const Bucket& bucket);
    void AddInterval(Series& entry, const SeriesId& id, const Cell& span);
    SavedHistory ExportHistory(const Series& entry, const SeriesId& id) const;
    Error ImportHistory(Series& entry, const SeriesId& id, const SavedHistory& saved,
        double savedStart, double savedClock) const;
};
}
