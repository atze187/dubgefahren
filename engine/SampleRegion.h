#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dg {

// Aufgelöster Bereich eines Samples in Positionen (Samples der Datei).
struct SampleRegion
{
    double start = 0.0;     // erste gespielte Position
    double loopStart = 0.0; // Marker L
    double end = 0.0;       // Position hinter dem letzten gespielten Sample
};

// Gelooptes Segment [lo, hi).
struct LoopSegment
{
    double lo = 0.0;
    double hi = 0.0;
};

inline constexpr double kMinRegionSamples = 16.0;

// Rechnet die drei Anteile (0..1) in gültige Positionen um. Es gilt danach immer
// 0 <= start <= loopStart <= end <= n und end - start >= min(kMinRegionSamples, n).
// Nicht endliche Anteile gelten als ihr Standard (0, 0, 1).
inline SampleRegion resolveSampleRegion(float start01, float loopStart01, float end01, std::size_t numSamples)
{
    const double n = static_cast<double>(numSamples);
    const double m = std::min(kMinRegionSamples, n);
    const auto frac = [](float v, double def) { return std::isfinite(v) ? std::clamp(static_cast<double>(v), 0.0, 1.0) : def; };

    SampleRegion r;
    r.end = std::min(n, std::max(frac(end01, 1.0) * n, m));
    r.start = std::min(frac(start01, 0.0) * n, r.end - m);
    r.loopStart = std::clamp(frac(loopStart01, 0.0) * n, r.start, r.end);
    return r;
}

// Vorwärts kreist loopStart..end, rückwärts start..loopStart. Ist das Segment kürzer als die
// Mindestlänge, kreist der ganze Bereich.
inline LoopSegment resolveLoopSegment(const SampleRegion& r, bool reverse, std::size_t numSamples)
{
    const double m = std::min(kMinRegionSamples, static_cast<double>(numSamples));
    LoopSegment s = reverse ? LoopSegment { r.start, r.loopStart } : LoopSegment { r.loopStart, r.end };
    if (s.hi - s.lo < m)
        s = { r.start, r.end };
    return s;
}

} // namespace dg
