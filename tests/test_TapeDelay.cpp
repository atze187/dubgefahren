#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
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

// RMS-Verhältnis zweite zu erster Wiederholung eines 50-ms-Sinusburst bei Feedback 0.5.
float echoRatio(float freq)
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.5f, 1.0f, 0.0f);
    const auto burst = dgtest::sine(freq, kSr, 2400, 0.5f);
    std::vector<float> y(48000);
    for (int i = 0; i < 48000; ++i)
    {
        const float in = i < 2400 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        y[static_cast<std::size_t>(i)] = l;
    }
    return dgtest::rms(y, 24200, 26200) / dgtest::rms(y, 12200, 14200);
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
    d.setParams(0.25f, 0.0f, 1.0f, 0.0f);
    const auto y = impulseResponse(d, 48000);
    const auto peakIt = std::max_element(y.begin(), y.end());
    const auto peakIndex = std::distance(y.begin(), peakIt);
    CHECK(peakIndex >= 12000);
    CHECK(peakIndex <= 12002);
    CHECK(*peakIt > 0.4f);
    CHECK(dgtest::peakAbs(y, 13000) < 0.01f);
    CHECK(dgtest::peakAbs(y, 0, 11990) == 0.0f);
}

TEST_CASE("feedback produces a quieter second echo", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.5f, 1.0f, 0.0f);
    const auto y = impulseResponse(d, 48000);
    const float first = dgtest::peakAbs(y, 11990, 12500);
    const float second = dgtest::peakAbs(y, 23990, 24500);
    CHECK(second / first > 0.2f);
    CHECK(second / first < 0.6f);
}

TEST_CASE("110 percent feedback stays finite and bounded", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
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
    d.setParams(0.01f, 0.3f, 0.5f, 0.0f);
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
    d.setParams(0.250000089f, 0.3f, 0.5f, 0.0f);
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

TEST_CASE("every repeat loses more highs than lows", "[delay]")
{
    const float low = echoRatio(200.0f);
    const float high = echoRatio(8000.0f);
    CHECK(low > 0.35f);
    CHECK(high < 0.8f * low);
}

TEST_CASE("asymmetric saturation leaves no DC in the loop", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.05f, 0.9f, 1.0f, 0.0f);
    const auto x = dgtest::sine(200.0f, kSr, 96000, 0.4f);
    std::vector<float> y(96000);
    for (int i = 0; i < 96000; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(x[static_cast<std::size_t>(i)], x[static_cast<std::size_t>(i)], l, r);
        y[static_cast<std::size_t>(i)] = l;
    }
    double sum = 0.0;
    for (std::size_t i = 48000; i < 96000; ++i)
        sum += y[i];
    CHECK(std::abs(sum / 48000.0) < 0.005);
}

TEST_CASE("loop stays bounded at 44.1 and 96 kHz", "[delay]")
{
    for (const double sr : { 44100.0, 96000.0 })
    {
        TapeDelay d;
        d.prepare(sr);
        d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
        const int burstLen = static_cast<int>(sr / 2);
        const auto burst = dgtest::noise(burstLen, 0.5f);
        float peak = 0.0f;
        bool finite = true;
        for (int i = 0; i < static_cast<int>(sr * 10); ++i)
        {
            const float in = i < burstLen ? burst[static_cast<std::size_t>(i)] : 0.0f;
            float l = 0.0f, r = 0.0f;
            d.process(in, in, l, r);
            finite = finite && std::isfinite(l) && std::isfinite(r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(finite);
        CHECK(peak < 4.0f);
    }
}

TEST_CASE("extreme input level stays finite", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
    const auto loud = dgtest::noise(48000, 100.0f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 10; ++i)
    {
        const float in = i < 48000 ? loud[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 200.0f);
}
