#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "engine/SpringReverb.h"
#include "TestHelpers.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

struct Stereo { std::vector<float> l, r; };

Stereo impulse(float decay, float tone, int n)
{
    SpringReverb rv;
    rv.prepare(kSr);
    rv.setParams(decay, tone);
    Stereo out { std::vector<float>(static_cast<std::size_t>(n)), std::vector<float>(static_cast<std::size_t>(n)) };
    for (int i = 0; i < n; ++i)
    {
        const float in = i == 0 ? 1.0f : 0.0f;
        rv.process(in, in, out.l[static_cast<std::size_t>(i)], out.r[static_cast<std::size_t>(i)]);
    }
    return out;
}
} // namespace

TEST_CASE("reverb is silent for silent input", "[reverb]")
{
    SpringReverb rv;
    rv.prepare(kSr);
    rv.setParams(1.0f, 0.5f);
    for (int i = 0; i < 4800; ++i)
    {
        float l = 1.0f, r = 1.0f;
        rv.process(0.0f, 0.0f, l, r);
        REQUIRE(l == 0.0f);
        REQUIRE(r == 0.0f);
    }
}

TEST_CASE("impulse produces a tail", "[reverb]")
{
    const auto y = impulse(0.5f, 0.5f, 48000);
    CHECK(dgtest::rms(y.l, 4800, 24000) > 1.0e-4f);
}

TEST_CASE("longer decay gives a longer tail", "[reverb]")
{
    const auto shortTail = impulse(0.0f, 0.5f, 96000);
    const auto longTail = impulse(1.0f, 0.5f, 96000);
    CHECK(dgtest::rms(longTail.l, 48000, 96000) > 2.0f * dgtest::rms(shortTail.l, 48000, 96000));
}

TEST_CASE("left and right differ", "[reverb]")
{
    const auto y = impulse(0.5f, 0.5f, 9600);
    CHECK(y.l != y.r);
}

TEST_CASE("reverb stays stable at maximum decay", "[reverb]")
{
    SpringReverb rv;
    rv.prepare(kSr);
    rv.setParams(1.0f, 1.0f);
    const auto x = dgtest::noise(48000 * 30, 0.5f);
    float peak = 0.0f;
    bool finite = true;
    for (float v : x)
    {
        float l = 0.0f, r = 0.0f;
        rv.process(v, -v, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 10.0f);
}

TEST_CASE("wet level is comparable to the input", "[reverb]")
{
    SpringReverb rv;
    rv.prepare(kSr);
    rv.setParams(0.5f, 0.5f);
    const auto x = dgtest::noise(48000 * 5, 0.5f);
    std::vector<float> wet_l(x.size()), wet_r(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        rv.process(x[i], x[i], wet_l[i], wet_r[i]);
    }
    const float input_rms = dgtest::rms(x);
    const float wet_rms = dgtest::rms(wet_l, 48000, 240000);
    const float ratio = wet_rms / input_rms;
    CHECK(ratio >= 0.5f);
    CHECK(ratio <= 2.0f);
}
