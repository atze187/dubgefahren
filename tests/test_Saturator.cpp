#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/Saturator.h"
#include "TestHelpers.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

std::vector<float> saturate(const std::vector<float>& x, float drive, double sr = kSr)
{
    Saturator s;
    s.prepare(sr);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        y[i] = s.process(x[i], drive);
    return y;
}
} // namespace

TEST_CASE("drive 0 is bit-exact transparent", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    for (float x : { -1.5f, -0.3f, 0.0f, 0.2f, 0.9f, 3.0f })
        CHECK(s.process(x, 0.0f) == x);
}

TEST_CASE("drive 1 stays finite and bounded for huge inputs", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 2000; ++i)
    {
        const float x = (i % 2 == 0 ? 1.0e6f : -1.0e6f) * (i % 7 == 0 ? 0.01f : 1.0f);
        const float y = s.process(x, 1.0f);
        finite = finite && std::isfinite(y);
        peak = std::max(peak, std::abs(y));
    }
    CHECK(finite);
    CHECK(peak < 2.0f);
}

TEST_CASE("louder input gives louder output", "[saturator]")
{
    Saturator a, b;
    a.prepare(kSr);
    b.prepare(kSr);
    CHECK(a.process(0.5f, 0.7f) > b.process(0.2f, 0.7f));
}

TEST_CASE("asymmetric saturation leaves no DC", "[saturator]")
{
    const auto y = saturate(dgtest::sine(200.0f, kSr, 48000, 0.5f), 1.0f);
    double sum = 0.0;
    for (std::size_t i = 24000; i < y.size(); ++i)
        sum += y[i];
    CHECK(std::abs(sum / 24000.0) < 0.01);
}

TEST_CASE("silence and constant input never produce NaN and settle at zero", "[saturator]")
{
    const std::vector<float> silence(48000, 0.0f);
    const auto ys = saturate(silence, 0.8f);
    CHECK(dgtest::allFinite(ys));
    CHECK(dgtest::peakAbs(ys) < 1.0e-6f);

    const std::vector<float> dc(48000, 0.3f);
    const auto yd = saturate(dc, 0.8f);
    CHECK(dgtest::allFinite(yd));
    CHECK(dgtest::peakAbs(yd, 40000) < 0.01f); // der DC-Blocker nimmt den Gleichanteil weg
}

TEST_CASE("a drive sweep does not click", "[saturator]")
{
    const auto x = dgtest::sine(500.0f, kSr, 48000, 0.3f);
    Saturator sweep;
    sweep.prepare(kSr);
    std::vector<float> ys(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        ys[i] = sweep.process(x[i], static_cast<float>(i) / static_cast<float>(x.size()));
    const auto fixed = saturate(x, 1.0f);
    CHECK(dgtest::allFinite(ys));
    CHECK(dgtest::maxStep(ys) <= 1.2f * dgtest::maxStep(fixed) + 0.02f);
}

TEST_CASE("no spike right after reset or when drive comes up from zero", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    for (int i = 0; i < 1000; ++i)
        s.process(0.5f * std::sin(0.05f * static_cast<float>(i)), 1.0f);
    s.reset();
    CHECK(std::abs(s.process(0.0f, 1.0f)) < 0.05f);

    Saturator t;
    t.prepare(kSr);
    for (int i = 0; i < 1000; ++i)
        t.process(0.2f, 0.0f);
    CHECK(std::abs(t.process(0.2f, 0.5f)) < 1.5f);
}

TEST_CASE("stable at 44.1 and 96 kHz", "[saturator]")
{
    for (const double sr : { 44100.0, 96000.0 })
    {
        const auto y = saturate(dgtest::sine(1000.0f, sr, static_cast<int>(sr), 0.5f), 1.0f, sr);
        CHECK(dgtest::allFinite(y));
        CHECK(dgtest::peakAbs(y) < 2.0f);
    }
}
