#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/GrainStretch.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr float kUnit = 1.0e-4f;

std::vector<float> ramp(int n)
{
    std::vector<float> x(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        x[static_cast<std::size_t>(i)] = static_cast<float>(i) * kUnit;
    return x;
}

float rampAt(double pos) { return static_cast<float>(pos) * kUnit; }
} // namespace

TEST_CASE("the first grain reads from its start position without a fade-in", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(1000.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    CHECK_THAT(g.process(x, 1000.0, 960, f), WithinAbs(rampAt(1000), 1e-5));
    CHECK_THAT(g.process(x, 1000.5, 960, f), WithinAbs(rampAt(1001), 1e-5));
}

TEST_CASE("a new grain starts at the source position every grain length", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(0.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    float last = 0.0f;
    // Quellposition läuft mit 0,5 pro Sample (Faktor 2): nach 960 Samples bei 480.
    for (int i = 0; i < 1261; ++i)
        last = g.process(x, i * 0.5, 960, f);
    // Neues Stück startete bei Aufruf 960 an der Quellposition 480; 300 Samples später (Aufruf 1260)
    // liest es bei 780, das alte Stück ist verklungen.
    CHECK_THAT(last, WithinAbs(rampAt(780), 1e-4));
}

TEST_CASE("the crossfade between two grains sums to one", "[stretch]")
{
    std::vector<float> ones(48000, 1.0f);
    GrainStretch g;
    g.start(100.0, 1.0, 400);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    for (int i = 0; i < 2000; ++i)
        CHECK_THAT(g.process(ones, 100.0 + i * 0.5, 400, f), WithinAbs(1.0, 1e-4));
}

TEST_CASE("reading wraps inside the segment when cycling", "[stretch]")
{
    const auto x = ramp(4000);
    GrainStretch g;
    g.start(1900.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, true, 1000.0, 2000.0 };
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 6000; ++i)
    {
        const float v = g.process(x, 1000.0 + std::fmod(900.0 + i * 0.5, 1000.0), 960, f);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    CHECK(lo >= rampAt(1000) - 1e-4f);
    CHECK(hi <= rampAt(2000) + 1e-4f);
}

TEST_CASE("without cycling the read position is held inside the bounds", "[stretch]")
{
    const auto x = ramp(4000);
    GrainStretch g;
    g.start(3990.0, 1.0, 960);
    const GrainFrame f { 1.0, 1.0, false, 0.0, 4000.0 };
    float v = 0.0f;
    for (int i = 0; i < 100; ++i)
        v = g.process(x, 3990.0, 960, f);
    CHECK_THAT(v, WithinAbs(rampAt(3999), 1e-5)); // gehalten am letzten Sample
}

TEST_CASE("a backwards grain reads backwards", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(5000.0, -1.0, 960);
    const GrainFrame f { 1.0, -1.0, false, 0.0, 48000.0 };
    CHECK_THAT(g.process(x, 5000.0, 960, f), WithinAbs(rampAt(5000), 1e-5));
    CHECK_THAT(g.process(x, 4999.5, 960, f), WithinAbs(rampAt(4999), 1e-5));
}

TEST_CASE("reset silences the grains", "[stretch]")
{
    const auto x = ramp(48000);
    GrainStretch g;
    g.start(1000.0, 1.0, 960);
    g.reset();
    // Nach reset startet das erste Stück erst beim nächsten process() mit Einblendung.
    const GrainFrame f { 1.0, 1.0, false, 0.0, 48000.0 };
    CHECK_THAT(g.process(x, 1000.0, 960, f), WithinAbs(0.0, 1e-6));
}
