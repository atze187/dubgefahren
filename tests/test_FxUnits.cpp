#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include "engine/Drive.h"
#include "engine/Limiter.h"
#include "engine/SvFilter.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

float filterGain(float freq, float cutoff, float res, FilterType type)
{
    SvFilter f;
    f.prepare(kSr);
    f.setParams(cutoff, res);
    f.setType(type, true);
    const auto x = dgtest::sine(freq, kSr, 48000);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        f.tick();
        y[i] = f.process(x[i], 0);
    }
    return dgtest::peakAbs(y, 24000);
}
} // namespace

TEST_CASE("drive 0 is transparent, drive 1 saturates and stays bounded", "[fx]")
{
    for (float x : { -1.5f, -0.3f, 0.0f, 0.2f, 0.9f })
        CHECK(driveSample(x, 0.0f) == x);
    CHECK(std::abs(driveSample(10.0f, 1.0f)) < 0.3f);
    CHECK(std::abs(driveSample(1.0e6f, 1.0f)) < 0.3f);
    CHECK(driveSample(0.5f, 0.7f) > driveSample(0.2f, 0.7f));
}

TEST_CASE("filter low pass, band pass and high pass responses", "[fx]")
{
    CHECK_THAT(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Lowpass), WithinAbs(1.0, 0.05));
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Lowpass) < 0.05f);

    CHECK_THAT(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Highpass), WithinAbs(1.0, 0.05));
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Highpass) < 0.05f);

    CHECK_THAT(filterGain(1000.0f, 1000.0f, 0.0f, FilterType::Bandpass), WithinAbs(1.0, 0.05));
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Bandpass) < 0.3f);
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Bandpass) < 0.3f);
}

TEST_CASE("filter notch removes the cutoff and keeps the rest", "[fx]")
{
    CHECK(filterGain(1000.0f, 1000.0f, 0.0f, FilterType::Notch) < 0.05f);
    CHECK(filterGain(100.0f, 1000.0f, 0.0f, FilterType::Notch) > 0.9f);
    CHECK(filterGain(10000.0f, 1000.0f, 0.0f, FilterType::Notch) > 0.9f);
}

TEST_CASE("switching the filter type glides without a click", "[fx]")
{
    // Sinus 1 kHz, Amplitude 0,5: ohne Wechsel ändert er sich pro Abtastwert um höchstens ~0,07.
    SvFilter f;
    f.prepare(kSr);
    f.setParams(1000.0f, 0.0f);
    f.setType(FilterType::Lowpass, true);
    const auto x = dgtest::sine(1000.0f, kSr, 48000, 0.5f);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        if (i == 24000)
            f.setType(FilterType::Highpass);
        f.tick();
        y[i] = f.process(x[i], 0);
    }
    CHECK(dgtest::allFinite(y));
    CHECK(dgtest::maxStep(y, 24000, 24000 + 2400) < 0.15f);

    // Ohne immediate gleiten die Gewichte: nach 0,5 s arbeitet der Hochpass.
    SvFilter g;
    g.prepare(kSr);
    g.setParams(1000.0f, 0.0f);
    g.setType(FilterType::Lowpass, true);
    g.setType(FilterType::Highpass);
    const auto low = dgtest::sine(100.0f, kSr, 48000);
    std::vector<float> z(low.size());
    for (std::size_t i = 0; i < low.size(); ++i)
    {
        g.tick();
        z[i] = g.process(low[i], 0);
    }
    CHECK(dgtest::peakAbs(z, 24000) < 0.1f);
}

TEST_CASE("filter stays stable at maximum resonance and extreme cutoff", "[fx]")
{
    for (const auto type : { FilterType::Lowpass, FilterType::Bandpass, FilterType::Highpass, FilterType::Notch })
        for (const float cutoff : { 20.0f, 1000.0f, 30000.0f })
        {
            SvFilter f;
            f.prepare(kSr);
            f.setParams(cutoff, 1.0f);
            f.setType(type, true);
            const auto x = dgtest::noise(48000 * 30, 0.5f);
            std::vector<float> y(x.size());
            for (std::size_t i = 0; i < x.size(); ++i)
            {
                f.tick();
                y[i] = f.process(x[i], 1);
            }
            CHECK(dgtest::allFinite(y));
            CHECK(dgtest::peakAbs(y) < 50.0f);
        }
}

TEST_CASE("limiter never exceeds the ceiling and recovers", "[fx]")
{
    Limiter lim;
    lim.prepare(kSr);
    const auto loud = dgtest::sine(200.0f, kSr, 4800, 4.0f);
    float peak = 0.0f;
    for (float v : loud)
    {
        float l = v, r = -v;
        lim.process(l, r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(peak <= kLimiterCeiling + 1e-6f);

    const auto quiet = dgtest::sine(200.0f, kSr, 48000, 0.5f);
    std::vector<float> out(quiet.size());
    for (std::size_t i = 0; i < quiet.size(); ++i)
    {
        float l = quiet[i], r = quiet[i];
        lim.process(l, r);
        out[i] = l;
    }
    CHECK_THAT(dgtest::peakAbs(out, 43200), WithinAbs(0.5, 0.01));
}

TEST_CASE("limiter leaves quiet signals untouched", "[fx]")
{
    Limiter lim;
    lim.prepare(kSr);
    for (float v : dgtest::sine(300.0f, kSr, 4800, 0.1f))
    {
        float l = v, r = v;
        lim.process(l, r);
        REQUIRE(l == v);
    }
}
