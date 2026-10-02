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

namespace {
// Verarbeitet identisches Rauschen (links = rechts) durch ein frisches Phaser-Objekt.
std::vector<float> runOutputs(float rate, float depth, float mix)
{
    Phaser p;
    p.prepare(kSr);
    p.setParams(rate, depth, mix);
    const auto x = dgtest::noise(48000, 0.5f);
    std::vector<float> y;
    y.reserve(x.size() * 2);
    for (float v : x)
    {
        float l = v, r = v;
        p.process(l, r);
        y.push_back(l);
        y.push_back(r);
    }
    return y;
}
} // namespace

TEST_CASE("out-of-range parameters are clamped", "[phaser]")
{
    // Mix wird auf 0 begrenzt: durchgereicht
    Phaser p;
    p.prepare(kSr);
    p.setParams(-1.0f, -1.0f, -1.0f);
    float l = 0.3f, r = 0.3f;
    p.process(l, r);
    CHECK(l == 0.3f);
    CHECK(r == 0.3f);

    // Rate 100 -> 3 Hz, Depth 5 -> 1, Mix 5 -> 1: bit-genau wie die Grenzwerte
    CHECK(runOutputs(100.0f, 5.0f, 5.0f) == runOutputs(3.0f, 1.0f, 1.0f));
    // Rate 0 -> 0,05 Hz, Depth -1 -> 0
    CHECK(runOutputs(0.0f, -1.0f, 1.0f) == runOutputs(0.05f, 0.0f, 1.0f));
}

TEST_CASE("reset clears a poisoned phaser state", "[phaser]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Phaser p;
    p.prepare(48000.0);
    p.setParams(0.4f, 0.5f, 1.0f);
    for (int i = 0; i < 16; ++i)
    {
        float l = nan, r = nan;
        p.process(l, r);
    }
    p.reset();

    Phaser q;
    q.prepare(48000.0);
    q.setParams(0.4f, 0.5f, 1.0f);

    const auto x = dgtest::noise(4800, 0.5f);
    std::vector<float> pl, pr, ql, qr;
    for (float s : x)
    {
        float a = s, b = s, c = s, d = s;
        p.process(a, b);
        q.process(c, d);
        pl.push_back(a);
        pr.push_back(b);
        ql.push_back(c);
        qr.push_back(d);
    }
    CHECK(pl == ql);
    CHECK(pr == qr);
}
