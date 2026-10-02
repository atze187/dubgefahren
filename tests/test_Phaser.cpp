#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/Phaser.h"
#include "TestHelpers.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

// Verstärkung (linker Kanal) bei freq: zuerst silenceSamples Stille (der LFO läuft weiter), dann 3000 Samples
// Sinus mit Amplitude 0,5; gemessen wird die Spitze der letzten 1000 Samples.
float gainAt(float freq, float rate, float depth, float mix, int silenceSamples = 0)
{
    Phaser p;
    p.prepare(kSr);
    p.setParams(rate, depth, mix);
    for (int i = 0; i < silenceSamples; ++i)
    {
        float l = 0.0f, r = 0.0f;
        p.process(l, r);
    }
    const auto x = dgtest::sine(freq, kSr, 3000, 0.5f);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        float l = x[i], r = x[i];
        p.process(l, r);
        y[i] = l;
    }
    return dgtest::peakAbs(y, 2000) / 0.5f;
}
} // namespace

TEST_CASE("mix 0 and a vanishing mix pass the signal through bit-exact", "[phaser]")
{
    for (const float mix : { 0.0f, 1.0e-7f })
    {
        Phaser p;
        p.prepare(kSr);
        p.setParams(0.4f, 0.5f, mix);
        const auto x = dgtest::noise(4800, 0.5f);
        for (float v : x)
        {
            float l = v, r = -v;
            p.process(l, r);
            CHECK(l == v);
            CHECK(r == -v);
        }
    }
}

TEST_CASE("static sweep position puts notches near 0.41 and 2.41 times the break frequency", "[phaser]")
{
    // Rate 0,05 Hz, Depth 0: der LFO steht am Anfang bei 0, also fc = 700 Hz; Notches bei ca. 290 Hz und 1,7 kHz.
    const float notch1 = gainAt(290.0f, 0.05f, 0.0f, 1.0f);
    const float ref1 = gainAt(500.0f, 0.05f, 0.0f, 1.0f);
    CHECK(notch1 < 0.2f);
    CHECK(notch1 < 0.3f * ref1);

    const float notch2 = gainAt(1700.0f, 0.05f, 0.0f, 1.0f);
    const float ref2 = gainAt(4000.0f, 0.05f, 0.0f, 1.0f);
    CHECK(notch2 < 0.25f);
    CHECK(notch2 < 0.35f * ref2);
}

TEST_CASE("the notch moves with the LFO", "[phaser]")
{
    // Depth 1, Rate 0,05 Hz: nach 5 s (Viertelperiode) steht fc am Maximum (ca. 3,96 kHz), die Notch wandert weit weg von 290 Hz.
    const float atStart = gainAt(290.0f, 0.05f, 1.0f, 1.0f, 0);
    const float atPeak = gainAt(290.0f, 0.05f, 1.0f, 1.0f, 240000);
    CHECK(atStart < 0.3f);
    CHECK(atPeak > 0.8f);
}

TEST_CASE("full depth and mix stay finite and bounded at several sample rates", "[phaser]")
{
    for (const double sr : { 44100.0, 96000.0, 192000.0 })
    {
        Phaser p;
        p.prepare(sr);
        p.setParams(3.0f, 1.0f, 1.0f);
        const auto x = dgtest::noise(static_cast<int>(sr * 10.0), 1.0f);
        float peak = 0.0f;
        bool finite = true;
        for (float v : x)
        {
            float l = v, r = v;
            p.process(l, r);
            finite = finite && std::isfinite(l) && std::isfinite(r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(finite);
        CHECK(peak < 3.0f);
    }
}

TEST_CASE("left and right sweeps are 90 degrees apart", "[phaser]")
{
    Phaser p;
    p.prepare(kSr);
    p.setParams(0.4f, 0.5f, 1.0f);
    const auto x = dgtest::noise(48000, 0.5f);
    float maxDiff = 0.0f;
    for (float v : x)
    {
        float l = v, r = v;
        p.process(l, r);
        maxDiff = std::max(maxDiff, std::abs(l - r));
    }
    CHECK(maxDiff > 0.05f);
}

TEST_CASE("out-of-range parameters are clamped", "[phaser]")
{
    Phaser p;
    p.prepare(kSr);
    p.setParams(-1.0f, -1.0f, -1.0f); // Mix wird auf 0 begrenzt: durchgereicht
    float l = 0.3f, r = 0.3f;
    p.process(l, r);
    CHECK(l == 0.3f);
    CHECK(r == 0.3f);

    p.setParams(100.0f, 5.0f, 5.0f); // Rate 3 Hz, Depth 1, Mix 1
    bool finite = true;
    const auto x = dgtest::noise(48000, 1.0f);
    for (float v : x)
    {
        float a = v, b = v;
        p.process(a, b);
        finite = finite && std::isfinite(a) && std::isfinite(b);
    }
    CHECK(finite);
}
