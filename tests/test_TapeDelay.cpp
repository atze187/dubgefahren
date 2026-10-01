#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/FxParams.h"
#include "engine/TapeDelay.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

std::vector<float> impulseResponse(TapeDelay& d, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, l, r);
        out[static_cast<std::size_t>(i)] = l;
    }
    return out;
}

// Praktisch durchlässig: Hochpass bei 20 Hz, keine Modulation.
DelayFilterParams openFilter()
{
    DelayFilterParams f;
    f.type = FilterType::Highpass;
    f.cutoffHz = 20.0f;
    f.resonance = 0.0f;
    f.cutDepthOct = 0.0f;
    f.resDepth = 0.0f;
    return f;
}

std::vector<float> runDelay(TapeDelay& d, const std::vector<float>& in, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const float v = static_cast<std::size_t>(i) < in.size() ? in[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(v, v, l, r);
        out[static_cast<std::size_t>(i)] = l;
    }
    return out;
}
} // namespace

TEST_CASE("delay division to seconds", "[delay]")
{
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_4, 120.0), WithinAbs(0.5, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_8D, 120.0), WithinAbs(0.375, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_4T, 120.0), WithinAbs(1.0 / 3.0, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_1, 20.0), WithinAbs(8.0, 1e-9)); // bpm auf 30 geklemmt
}

TEST_CASE("single echo at the delay time without feedback", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.0f, 0.0f, openFilter());
    const auto y = impulseResponse(d, 48000);
    const auto peakIt = std::max_element(y.begin(), y.end());
    const auto peakIndex = std::distance(y.begin(), peakIt);
    CHECK(peakIndex >= 12000);
    CHECK(peakIndex <= 12002);
    CHECK(*peakIt > 0.5f);
    CHECK(dgtest::peakAbs(y, 13000) < 0.01f);
    CHECK(dgtest::peakAbs(y, 0, 11990) == 0.0f);
}

TEST_CASE("feedback produces a quieter second echo", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.5f, 0.0f, openFilter());
    const auto y = impulseResponse(d, 48000);
    const float first = dgtest::peakAbs(y, 11990, 12500);
    const float second = dgtest::peakAbs(y, 23990, 24500);
    CHECK(second / first > 0.3f);
    CHECK(second / first < 0.6f);
}

TEST_CASE("110 percent feedback stays finite and bounded", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.1f, 1.1f, 1.0f, openFilter());
    const auto burst = dgtest::noise(24000, 0.5f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 30; ++i)
    {
        const float in = i < 24000 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 4.0f);
}

TEST_CASE("channels are independent", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.01f, 0.3f, 0.0f, openFilter());
    float maxRight = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(i == 0 ? 1.0f : 0.0f, 0.0f, l, r);
        maxRight = std::max(maxRight, std::abs(r));
    }
    CHECK(maxRight == 0.0f);
}

TEST_CASE("no buffer overflow with edge case delay", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.250000089f, 0.3f, 0.0f, openFilter());
    const auto x = dgtest::noise(48000 * 11, 0.5f);
    bool finite = true;
    for (float v : x)
    {
        float l = 0.0f, r = 0.0f;
        d.process(v, v, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
    }
    CHECK(finite);
}

TEST_CASE("the first echo is already filtered", "[delay][filter]")
{
    DelayFilterParams lp = openFilter();
    lp.type = FilterType::Lowpass;
    lp.cutoffHz = 500.0f;
    const auto tone = dgtest::sine(8000.0f, kSr, 4800, 0.5f);

    TapeDelay open, closed;
    open.prepare(kSr);
    closed.prepare(kSr);
    open.setParams(0.25f, 0.0f, 0.0f, openFilter());
    closed.setParams(0.25f, 0.0f, 0.0f, lp);
    const auto a = runDelay(open, tone, 24000);
    const auto b = runDelay(closed, tone, 24000);
    const float rmsOpen = dgtest::rms(a, 12000, 16800);
    CHECK(rmsOpen > 0.3f);
    CHECK(dgtest::rms(b, 12000, 16800) < 0.05f * rmsOpen);
}

TEST_CASE("every repeat passes the filter again", "[delay][filter]")
{
    DelayFilterParams lp = openFilter();
    lp.type = FilterType::Lowpass;
    lp.cutoffHz = 2000.0f;
    const auto burst = dgtest::sine(4000.0f, kSr, 2400, 0.5f);

    const auto ratio = [&](const DelayFilterParams& f) {
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.1f, 0.8f, 0.0f, f);
        const auto y = runDelay(d, burst, 14400);
        return dgtest::rms(y, 9600, 12000) / dgtest::rms(y, 4800, 7200);
    };
    CHECK(ratio(openFilter()) > 0.6f);
    CHECK(ratio(lp) < 0.3f);
}

TEST_CASE("the filter starts at the first set cutoff", "[delay][filter]")
{
    DelayFilterParams lp = openFilter();
    lp.type = FilterType::Lowpass;
    lp.cutoffHz = 200.0f;
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.001f, 0.0f, 0.0f, lp);
    const auto y = runDelay(d, dgtest::sine(8000.0f, kSr, 4800, 0.5f), 4800);
    CHECK(dgtest::rms(y, 96, 4800) < 0.002f); // kein Gleiten von einem früheren Wert herab
}

TEST_CASE("an LFO with zero depth leaves the filter static", "[delay][filter]")
{
    DelayFilterParams a = openFilter();
    a.type = FilterType::Lowpass;
    a.cutoffHz = 1500.0f;
    a.resonance = 0.3f;
    a.lfoShape = LfoShape::Square;
    a.lfoRateHz = 7.0f;
    DelayFilterParams b = a;
    b.lfoShape = LfoShape::Triangle;
    b.lfoRateHz = 0.3f;

    TapeDelay da, db;
    da.prepare(kSr);
    db.prepare(kSr);
    da.setParams(0.05f, 0.5f, 0.0f, a);
    db.setParams(0.05f, 0.5f, 0.0f, b);
    const auto in = dgtest::noise(24000, 0.5f);
    const auto ya = runDelay(da, in, 24000);
    const auto yb = runDelay(db, in, 24000);
    float maxDiff = 0.0f;
    for (std::size_t i = 0; i < ya.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(ya[i] - yb[i]));
    CHECK(maxDiff < 1.0e-6f);
}

TEST_CASE("the LFO moves the cutoff up and down around the setting", "[delay][filter]")
{
    const auto in = dgtest::noise(48000, 0.5f);
    const auto run = [&](float depthOct) {
        DelayFilterParams f = openFilter();
        f.type = FilterType::Lowpass;
        f.cutoffHz = 1000.0f;
        f.lfoShape = LfoShape::Square;
        f.lfoRateHz = 1.0f;
        f.cutDepthOct = depthOct;
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.001f, 0.0f, 0.0f, f);
        return runDelay(d, in, 48000);
    };
    const auto moved = run(3.0f);
    const auto still = run(0.0f);
    // Square beginnt in der unteren Hälfte (c = -1): Cutoff 125 Hz, danach 8 kHz.
    const float low = dgtest::rms(moved, 2400, 21600);
    const float high = dgtest::rms(moved, 26400, 45600);
    const float middle = dgtest::rms(still, 2400, 45600);
    CHECK(low < middle);
    CHECK(middle < high);
    CHECK(high > 2.0f * low);
}

TEST_CASE("extreme cutoff and depth stay finite at every sample rate", "[delay][filter]")
{
    for (const double sr : { 44100.0, 96000.0, 192000.0 })
        for (const float cutoff : { 20.0f, 20000.0f })
        {
            DelayFilterParams f = openFilter();
            f.type = FilterType::Bandpass;
            f.cutoffHz = cutoff;
            f.resonance = 1.0f;
            f.lfoShape = LfoShape::SampleHold;
            f.lfoRateHz = 40.0f;
            f.cutDepthOct = 4.0f;
            f.resDepth = 1.0f;
            TapeDelay d;
            d.prepare(sr);
            d.setParams(0.05f, 0.9f, 1.0f, f);
            const auto in = dgtest::noise(static_cast<int>(sr), 0.5f);
            const auto y = runDelay(d, in, static_cast<int>(sr) * 2);
            CHECK(dgtest::allFinite(y));
            CHECK(dgtest::peakAbs(y) < 4.0f);
        }
}

TEST_CASE("changing the type while echoes circulate stays bounded", "[delay][filter]")
{
    DelayFilterParams f = openFilter();
    f.cutoffHz = 800.0f;
    f.resonance = 1.0f;
    f.lfoShape = LfoShape::Triangle;
    f.lfoRateHz = 3.0f;
    f.cutDepthOct = 2.0f;
    f.resDepth = 1.0f;
    TapeDelay d;
    d.prepare(kSr);
    const auto burst = dgtest::noise(24000, 0.5f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 20; ++i)
    {
        if (i % 4800 == 0)
        {
            f.type = static_cast<FilterType>((i / 4800) % kNumFilterTypes);
            d.setParams(0.1f, 1.1f, 1.0f, f);
        }
        const float in = i < 24000 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 4.0f);
}

TEST_CASE("the delay LFO rate follows the free setting or the tempo", "[delay][filter]")
{
    FxParams p;
    p.delayLfoRateHz = 3.0f;
    CHECK_THAT(resolveDelayLfoRate(p, 120.0), WithinAbs(3.0, 1e-6));
    p.delayLfoSync = true;
    p.delayLfoSyncDiv = SyncDivision::D1_4;
    CHECK_THAT(resolveDelayLfoRate(p, 120.0), WithinAbs(2.0, 1e-6));
    CHECK_THAT(resolveDelayLfoRate(p, 0.0), WithinAbs(0.5, 1e-6)); // bpm auf 30 geklemmt
}
