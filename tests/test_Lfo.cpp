#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include "engine/Lfo.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;
std::vector<float> render(LfoShape s, float rate, int n, std::uint32_t seed = 1)
{
    Lfo lfo;
    lfo.prepare(kSr);
    lfo.reset(seed);
    std::vector<float> out(static_cast<std::size_t>(n));
    for (auto& v : out)
        v = lfo.process(s, rate);
    return out;
}
} // namespace

TEST_CASE("square LFO starts low and goes high after half a period", "[lfo]")
{
    const auto x = render(LfoShape::Square, 1.0f, 48000);
    CHECK(x[0] == 0.0f);
    CHECK(x[20000] == 0.0f);
    CHECK(x[30000] == 1.0f);
}

TEST_CASE("triangle, saw up and saw down shapes", "[lfo]")
{
    const auto tri = render(LfoShape::Triangle, 1.0f, 48000);
    CHECK_THAT(tri[0], WithinAbs(0.0, 1e-4));
    CHECK_THAT(tri[24000], WithinAbs(1.0, 1e-3));
    CHECK_THAT(tri[12000], WithinAbs(0.5, 1e-3));

    const auto up = render(LfoShape::SawUp, 1.0f, 48000);
    CHECK_THAT(up[0], WithinAbs(0.0, 1e-4));
    CHECK_THAT(up[36000], WithinAbs(0.75, 1e-3));

    const auto down = render(LfoShape::SawDown, 1.0f, 48000);
    CHECK_THAT(down[0], WithinAbs(1.0, 1e-4));
    CHECK_THAT(down[36000], WithinAbs(0.25, 1e-3));
}

TEST_CASE("LFO rate: saw up wraps at the expected rate", "[lfo]")
{
    // 4.5 Hz: Umläufe bei ~10667, 21333, 32000, 42667 Samples → 4 innerhalb 1 s
    const auto x = render(LfoShape::SawUp, 4.5f, 48000);
    int wraps = 0;
    for (std::size_t i = 1; i < x.size(); ++i)
        if (x[i] < x[i - 1])
            ++wraps;
    CHECK(wraps == 4);
}

TEST_CASE("sample and hold is constant within a period and in range", "[lfo]")
{
    const auto x = render(LfoShape::SampleHold, 10.0f, 48000, 42);
    for (int period = 0; period < 9; ++period)
    {
        const auto start = static_cast<std::size_t>(period * 4800 + 10);
        for (std::size_t i = start; i < start + 4700; ++i)
            REQUIRE(x[i] == x[start]);
        REQUIRE(x[start] >= 0.0f);
        REQUIRE(x[start] <= 1.0f);
    }
    bool changed = false;
    for (int period = 1; period < 9; ++period)
        changed |= x[static_cast<std::size_t>(period * 4800 + 10)] != x[10];
    CHECK(changed);
}

TEST_CASE("sample and hold is deterministic per seed", "[lfo]")
{
    CHECK(render(LfoShape::SampleHold, 10.0f, 9600, 7) == render(LfoShape::SampleHold, 10.0f, 9600, 7));
    CHECK(render(LfoShape::SampleHold, 10.0f, 9600, 7) != render(LfoShape::SampleHold, 10.0f, 9600, 8));
}

TEST_CASE("reset restarts at phase 0", "[lfo]")
{
    Lfo lfo;
    lfo.prepare(kSr);
    lfo.reset(1);
    for (int i = 0; i < 30000; ++i)
        lfo.process(LfoShape::SawUp, 1.0f);
    lfo.reset(1);
    CHECK_THAT(lfo.process(LfoShape::SawUp, 1.0f), WithinAbs(0.0, 1e-6));
}

TEST_CASE("sync rate from tempo", "[lfo]")
{
    CHECK_THAT(lfoRateFromSync(SyncDivision::D1_4, 120.0), WithinAbs(2.0, 1e-5));
    CHECK_THAT(lfoRateFromSync(SyncDivision::Bar1, 120.0), WithinAbs(0.5, 1e-5));
    CHECK_THAT(lfoRateFromSync(SyncDivision::D1_16T, 120.0), WithinAbs(12.0, 1e-4));
}
