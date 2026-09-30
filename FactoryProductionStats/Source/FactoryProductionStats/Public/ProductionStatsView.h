// SPDX-License-Identifier: 0BSD
#pragma once
#include "ProductionStatsTypes.h"
#include <algorithm>

namespace FactoryProductionStats
{
// Shared by the native chart, list ordering and the runnable T07 check.
inline std::optional<double> DisplayValue(const SeriesId& id, const Bucket& bin)
{
    if (!bin.value || bin.coverage.observedSeconds <= 0) return std::nullopt;
    if (id.category != Category::Power)
    {
        const auto* quantity = std::get_if<Quantity>(&*bin.value);
        return quantity ? RatePerMinute(std::visit([](auto n) { return static_cast<double>(n); }, *quantity), bin.coverage.observedSeconds) : std::nullopt;
    }
    if (const auto* integral = std::get_if<PowerIntegral>(&*bin.value)) return AverageMegawatts(*integral, bin.coverage.observedSeconds);
    if (const auto* energy = std::get_if<EnergyState>(&*bin.value)) return energy->megawattHours;
    return std::nullopt;
}
inline bool HasTimeGap(const Bucket& bin)
{
    // Incomplete source coverage is labeled separately; known subtotal samples
    // still draw. Unknown positions inside a compressed bin break the line.
    return bin.coverage.observedSeconds < bin.range.end - bin.range.begin ||
        (bin.coverage.gapReasons & (1u | 2u | 16u)) != 0;
}
struct PlotPoint { double time = 0, value = 0; };
using PlotSegments = std::vector<std::vector<PlotPoint>>;
inline PlotSegments MakePlot(const SeriesResult& row)
{
    PlotSegments segments;
    std::vector<PlotPoint> active;
    double lastEnd = -1;
    const auto flush = [&] { if (!active.empty()) { segments.push_back(std::move(active)); active.clear(); } };
    for (const auto& bin : row.points)
    {
        const auto value = DisplayValue(row.series, bin);
        if (!value || HasTimeGap(bin) || !std::isfinite(*value)) { flush(); lastEnd = -1; continue; }
        if (lastEnd != bin.range.begin) flush();
        const double time = UnitFor(row.series) == Unit::MegawattHours
            ? bin.range.end : (bin.range.begin + bin.range.end) / 2;
        active.push_back({time, *value});
        lastEnd = bin.range.end;
    }
    flush();
    return segments;
}
inline bool SortBefore(const SeriesResult& a, const SeriesResult& b, bool byName)
{
    if (byName && a.displayName != b.displayName) return a.displayName < b.displayName;
    if (!byName)
    {
        const auto av = DisplayValue(a.series, a.summary), bv = DisplayValue(b.series, b.summary);
        if (av.has_value() != bv.has_value()) return av.has_value();
        if (av && *av != *bv) return *av > *bv;
    }
    return a.series < b.series;
}
inline double RelativeBar(std::optional<double> value, double maximum)
{
    return value && std::isfinite(*value) && std::isfinite(maximum) && maximum > 0
        ? std::clamp(std::abs(*value) / maximum, 0., 1.) : 0;
}
inline std::uint32_t ColourKey(const SeriesId& id)
{
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : id.key) { hash ^= c; hash *= 16777619u; }
    hash ^= static_cast<std::uint32_t>(id.category); hash *= 16777619u;
    return hash;
}
}
