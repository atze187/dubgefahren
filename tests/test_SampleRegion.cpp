#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include "engine/SampleRegion.h"

using namespace dg;

TEST_CASE("default markers resolve to the whole file", "[region]")
{
    const auto r = resolveSampleRegion(0.0f, 0.0f, 1.0f, 4800);
    CHECK(r.start == 0.0);
    CHECK(r.loopStart == 0.0);
    CHECK(r.end == 4800.0);
}

TEST_CASE("markers resolve to positions in samples", "[region]")
{
    const auto r = resolveSampleRegion(0.25f, 0.5f, 0.75f, 4800);
    CHECK(r.start == 1200.0);
    CHECK(r.loopStart == 2400.0);
    CHECK(r.end == 3600.0);
}

TEST_CASE("invalid marker order still leaves the minimum region", "[region]")
{
    // End vor Start: End gewinnt, Start rückt auf die Mindestlänge davor.
    // (Anteile mit exakter Binärdarstellung, damit die Positionen ganzzahlig sind.)
    auto r = resolveSampleRegion(0.875f, 0.5f, 0.125f, 4800);
    CHECK(r.end == 600.0);
    CHECK(r.start == 584.0);
    CHECK(r.loopStart == 600.0); // in den Bereich geklemmt
    CHECK(r.end - r.start == kMinRegionSamples);

    // Start = End
    r = resolveSampleRegion(0.5f, 0.5f, 0.5f, 4800);
    CHECK(r.end == 2400.0);
    CHECK(r.start == 2384.0);
    CHECK(r.loopStart == 2400.0);

    // End ganz links
    r = resolveSampleRegion(0.0f, 0.0f, 0.0f, 4800);
    CHECK(r.start == 0.0);
    CHECK(r.end == 16.0);

    // Loop-Start außerhalb
    r = resolveSampleRegion(0.25f, 0.0f, 0.5f, 4800);
    CHECK(r.loopStart == 1200.0);
    r = resolveSampleRegion(0.25f, 1.0f, 0.5f, 4800);
    CHECK(r.loopStart == 2400.0);
}

TEST_CASE("out-of-range and non-finite markers are made safe", "[region]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    auto r = resolveSampleRegion(-3.0f, 7.0f, 9.0f, 4800);
    CHECK(r.start == 0.0);
    CHECK(r.loopStart == 4800.0);
    CHECK(r.end == 4800.0);
    r = resolveSampleRegion(nan, nan, nan, 4800); // Standardwerte
    CHECK(r.start == 0.0);
    CHECK(r.loopStart == 0.0);
    CHECK(r.end == 4800.0);
    r = resolveSampleRegion(inf, -inf, inf, 4800);
    CHECK(std::isfinite(r.start));
    CHECK(std::isfinite(r.loopStart));
    CHECK(std::isfinite(r.end));
    CHECK(r.start <= r.loopStart);
    CHECK(r.loopStart <= r.end);
}

TEST_CASE("files shorter than the minimum region use their whole length", "[region]")
{
    auto r = resolveSampleRegion(0.5f, 0.5f, 0.5f, 1);
    CHECK(r.start == 0.0);
    CHECK(r.end == 1.0);
    r = resolveSampleRegion(0.875f, 0.0f, 0.125f, 8);
    CHECK(r.start == 0.0);
    CHECK(r.end == 8.0);
    r = resolveSampleRegion(0.0f, 0.0f, 1.0f, 0);
    CHECK(r.start == 0.0);
    CHECK(r.end == 0.0);
}

TEST_CASE("the loop segment depends on the direction and falls back to the whole region", "[region]")
{
    const auto r = resolveSampleRegion(0.25f, 0.5f, 0.75f, 4800);
    auto s = resolveLoopSegment(r, false, 4800);
    CHECK(s.lo == 2400.0);
    CHECK(s.hi == 3600.0);
    s = resolveLoopSegment(r, true, 4800);
    CHECK(s.lo == 1200.0);
    CHECK(s.hi == 2400.0);

    // Loop-Start = End: vorwärts wäre das Segment leer, also der ganze Bereich.
    const auto atEnd = resolveSampleRegion(0.25f, 0.75f, 0.75f, 4800);
    s = resolveLoopSegment(atEnd, false, 4800);
    CHECK(s.lo == 1200.0);
    CHECK(s.hi == 3600.0);

    // Loop-Start = Start: rückwärts wäre das Segment leer, also der ganze Bereich.
    const auto atStart = resolveSampleRegion(0.25f, 0.25f, 0.75f, 4800);
    s = resolveLoopSegment(atStart, true, 4800);
    CHECK(s.lo == 1200.0);
    CHECK(s.hi == 3600.0);

    // Knapp unter der Mindestlänge zählt als leer.
    const SampleRegion tight { 1200.0, 3590.0, 3600.0 };
    s = resolveLoopSegment(tight, false, 4800);
    CHECK(s.lo == 1200.0);
}
