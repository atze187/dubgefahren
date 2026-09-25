#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <limits>
#include <vector>
#include "engine/FxChain.h"
#include "engine/Limiter.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

FxParams neutral()
{
    FxParams p;
    p.drive = 0.0f;
    p.cutoffHz = 20000.0f;
    p.resonance = 0.1f;
    p.filterType = 0.0f;
    p.delayMix = 0.0f;
    p.reverbMix = 0.0f;
    p.masterDb = 0.0f;
    return p;
}

struct Buses
{
    std::vector<float> ml, mr, sl, sr;
    explicit Buses(int n)
        : ml(static_cast<std::size_t>(n)), mr(static_cast<std::size_t>(n)),
          sl(static_cast<std::size_t>(n)), sr(static_cast<std::size_t>(n)) {}
};

void run(FxChain& fx, Buses& b, const FxParams& p, int block = 512)
{
    const int n = static_cast<int>(b.ml.size());
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min(block, n - pos);
        fx.process(b.ml.data() + pos, b.mr.data() + pos, b.sl.data() + pos, b.sr.data() + pos, len, p, 120.0);
    }
}
} // namespace

TEST_CASE("neutral settings pass the main bus through", "[fxchain]")
{
    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000);
    b.ml = dgtest::sine(440.0f, kSr, 48000, 0.5f);
    b.mr = b.ml;
    run(fx, b, neutral());
    CHECK_THAT(dgtest::peakAbs(b.ml, 24000), WithinAbs(0.5, 0.01));
}

TEST_CASE("only the send bus reaches the delay", "[fxchain]")
{
    auto p = neutral();
    p.delayMix = 1.0f;
    p.delayFeedback = 0.0f;
    p.delayTone = 1.0f;
    p.delayWow = 0.0f;
    p.delayDiv = DelayDivision::D1_4; // 0,5 s bei 120 bpm

    FxChain fx;
    fx.prepare(kSr);
    Buses viaSend(48000);
    viaSend.sl[0] = viaSend.sr[0] = 1.0f;
    run(fx, viaSend, p);
    CHECK(dgtest::peakAbs(viaSend.ml, 23990, 24200) > 0.3f);

    FxChain fx2;
    fx2.prepare(kSr);
    Buses viaMain(48000);
    viaMain.ml[0] = viaMain.mr[0] = 1.0f;
    run(fx2, viaMain, p);
    CHECK(dgtest::peakAbs(viaMain.ml, 23990, 24200) < 1.0e-3f);
}

TEST_CASE("extreme settings stay finite and below the ceiling", "[fxchain]")
{
    FxParams p;
    p.drive = 1.0f;
    p.cutoffHz = 1000.0f;
    p.resonance = 1.0f;
    p.delayFeedback = 1.1f;
    p.delayWow = 1.0f;
    p.delayMix = 1.0f;
    p.reverbDecay = 1.0f;
    p.reverbMix = 1.0f;
    p.masterDb = 6.0f;

    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000 * 30);
    b.ml = dgtest::noise(48000 * 30, 0.5f, 3);
    b.mr = dgtest::noise(48000 * 30, 0.5f, 4);
    b.sl = dgtest::noise(48000 * 30, 0.5f, 5);
    b.sr = dgtest::noise(48000 * 30, 0.5f, 6);
    run(fx, b, p);
    CHECK(dgtest::allFinite(b.ml));
    CHECK(dgtest::allFinite(b.mr));
    CHECK(dgtest::peakAbs(b.ml) <= kLimiterCeiling + 1.0e-6f);
    CHECK(dgtest::peakAbs(b.mr) <= kLimiterCeiling + 1.0e-6f);
}

TEST_CASE("NaN input is contained and processing recovers", "[fxchain]")
{
    FxChain fx;
    fx.prepare(kSr);
    Buses b(9600);
    b.ml = dgtest::sine(440.0f, kSr, 9600, 0.5f);
    b.mr = b.ml;
    b.ml[100] = std::numeric_limits<float>::quiet_NaN();
    run(fx, b, neutral());
    CHECK(dgtest::allFinite(b.ml));
    CHECK(dgtest::allFinite(b.mr));
    CHECK(dgtest::allFinite(b.sl));
    CHECK(dgtest::allFinite(b.sr));
    CHECK(dgtest::peakAbs(b.ml, 4800) > 0.4f);
}

TEST_CASE("master at -60 dB is silent", "[fxchain]")
{
    auto p = neutral();
    p.masterDb = -60.0f;
    FxChain fx;
    fx.prepare(kSr);
    Buses b(4800);
    b.ml = dgtest::sine(440.0f, kSr, 4800, 0.5f);
    run(fx, b, p);
    CHECK(dgtest::peakAbs(b.ml) == 0.0f);
}

TEST_CASE("master changes are smoothed", "[fxchain]")
{
    auto p = neutral();
    FxChain fx;
    fx.prepare(kSr);

    // Erster Block bei masterDb 0, dann ein Block bei masterDb -60 direkt anschließend
    // im selben Puffer, damit der Übergang zwischen den Blöcken erfasst wird.
    Buses combined(1024);
    const auto sineFull = dgtest::sine(440.0f, kSr, 1024, 0.5f);
    combined.ml = sineFull;
    combined.mr = sineFull;

    fx.process(combined.ml.data(), combined.mr.data(), combined.sl.data(), combined.sr.data(), 512, p, 120.0);
    p.masterDb = -60.0f;
    fx.process(combined.ml.data() + 512, combined.mr.data() + 512, combined.sl.data() + 512, combined.sr.data() + 512,
               512, p, 120.0);
    CHECK(dgtest::maxStep(combined.ml, 500) < 0.05f);

    // Der 20-ms-Einpol-Filter braucht rund 6 Zeitkonstanten, um auf < 1e-3 abzuklingen;
    // ein grosszuegiges weiteres Fenster stellt sicher, dass wirklich abgeklungen wurde.
    Buses tail(9600);
    tail.ml = dgtest::sine(440.0f, kSr, 9600, 0.5f);
    tail.mr = tail.ml;
    run(fx, tail, p, 512);
    CHECK(dgtest::peakAbs(tail.ml, 9500) < 1.0e-3f);
}

TEST_CASE("persistent NaN input is contained without per-sample resets", "[fxchain]")
{
    FxChain fx;
    fx.prepare(kSr);
    Buses b(48000 + 48000);

    // First 48000 samples: NaN on main bus
    for (int i = 0; i < 48000; ++i)
    {
        b.ml[i] = std::numeric_limits<float>::quiet_NaN();
        b.mr[i] = std::numeric_limits<float>::quiet_NaN();
    }

    // Next 48000 samples: 440 Hz sine
    auto sine = dgtest::sine(440.0f, kSr, 48000, 0.5f);
    std::copy(sine.begin(), sine.end(), b.ml.begin() + 48000);
    std::copy(sine.begin(), sine.end(), b.mr.begin() + 48000);

    run(fx, b, neutral());

    CHECK(dgtest::allFinite(b.ml));
    CHECK(dgtest::allFinite(b.mr));
    CHECK(dgtest::allFinite(b.sl));
    CHECK(dgtest::allFinite(b.sr));
    CHECK(dgtest::peakAbs(b.ml, 48000 + 24000) > 0.4f);
}
