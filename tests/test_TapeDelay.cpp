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
    CHECK(*peakIt > 0.5f);
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
    CHECK(second / first > 0.3f);
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
