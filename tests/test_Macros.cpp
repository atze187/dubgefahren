#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include "engine/FxParams.h"
#include "engine/Macros.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
FxParams sampleBase()
{
    FxParams p;
    p.drive = 0.2f;
    p.cutoffHz = 900.0f;
    p.resonance = 0.3f;
    p.filterType = 0.5f;
    p.delayDiv = DelayDivision::D1_4;
    p.delayFeedback = 0.45f;
    p.delayTone = 0.5f;
    p.delayWow = 0.2f;
    p.delayMix = 0.35f;
    p.reverbDecay = 0.5f;
    p.reverbTone = 0.4f;
    p.reverbMix = 0.25f;
    p.phaserRate = 0.7f;
    p.phaserDepth = 0.5f;
    p.phaserMix = 0.0f;
    p.masterDb = -3.0f;
    return p;
}

bool same(const FxParams& a, const FxParams& b)
{
    return a.drive == b.drive && a.cutoffHz == b.cutoffHz && a.resonance == b.resonance &&
           a.filterType == b.filterType && a.delayDiv == b.delayDiv && a.delayFeedback == b.delayFeedback &&
           a.delayTone == b.delayTone && a.delayWow == b.delayWow && a.delayMix == b.delayMix &&
           a.reverbDecay == b.reverbDecay && a.reverbTone == b.reverbTone && a.reverbMix == b.reverbMix &&
           a.phaserRate == b.phaserRate && a.phaserDepth == b.phaserDepth && a.phaserMix == b.phaserMix &&
           a.masterDb == b.masterDb;
}
} // namespace

TEST_CASE("knobs at zero leave every parameter untouched", "[macros]")
{
    const auto base = sampleBase();
    CHECK(same(applyMacros(base, MacroParams {}), base));
    CHECK(throwSendBoost(MacroParams {}) == 0.0f);
}

TEST_CASE("space opens delay and reverb but only up to feedback 0.95", "[macros]")
{
    const auto base = sampleBase();
    const auto e = applyMacros(base, MacroParams { 1.0f, 0.0f, 0.0f });
    CHECK_THAT(e.delayMix, WithinAbs(0.70, 1e-5));
    CHECK_THAT(e.delayFeedback, WithinAbs(0.75, 1e-5));
    CHECK_THAT(e.reverbMix, WithinAbs(0.65, 1e-5));
    CHECK_THAT(e.reverbDecay, WithinAbs(0.80, 1e-5));
    CHECK(e.drive == base.drive);
    CHECK(e.phaserMix == base.phaserMix);
    CHECK(e.masterDb == base.masterDb);

    const auto half = applyMacros(base, MacroParams { 0.5f, 0.0f, 0.0f });
    CHECK_THAT(half.delayMix, WithinAbs(0.525, 1e-5));
    CHECK_THAT(half.reverbMix, WithinAbs(0.45, 1e-5));

    auto high = base;
    high.delayFeedback = 0.9f;
    CHECK_THAT(applyMacros(high, MacroParams { 1.0f, 0.0f, 0.0f }).delayFeedback, WithinAbs(0.95, 1e-5));
}

TEST_CASE("grit adds saturation, wobble, darker tone and phaser", "[macros]")
{
    const auto base = sampleBase();
    const auto e = applyMacros(base, MacroParams { 0.0f, 1.0f, 0.0f });
    CHECK_THAT(e.drive, WithinAbs(0.90, 1e-5));
    CHECK_THAT(e.delayWow, WithinAbs(0.70, 1e-5));
    CHECK_THAT(e.delayTone, WithinAbs(0.20, 1e-5));
    CHECK_THAT(e.phaserMix, WithinAbs(0.60, 1e-5));
    CHECK_THAT(e.phaserDepth, WithinAbs(0.80, 1e-5));
    CHECK(e.delayMix == base.delayMix);
    CHECK(e.reverbMix == base.reverbMix);

    auto loud = base;
    loud.drive = 0.6f;
    loud.delayTone = 0.1f;
    const auto c = applyMacros(loud, MacroParams { 0.0f, 1.0f, 0.0f });
    CHECK_THAT(c.drive, WithinAbs(1.0, 1e-6));  // begrenzt
    CHECK_THAT(c.delayTone, WithinAbs(0.0, 1e-6)); // nie unter 0
}

TEST_CASE("throw pushes feedback up to self-oscillation and the send boost to 1", "[macros]")
{
    const auto base = sampleBase();
    const auto e = applyMacros(base, MacroParams { 0.0f, 0.0f, 1.0f });
    CHECK_THAT(e.delayFeedback, WithinAbs(0.95, 1e-5));
    CHECK_THAT(e.delayMix, WithinAbs(0.85, 1e-5));
    CHECK(e.reverbMix == base.reverbMix);
    CHECK_THAT(throwSendBoost(MacroParams { 0.0f, 0.0f, 1.0f }), WithinAbs(1.0, 1e-6));
    CHECK_THAT(throwSendBoost(MacroParams { 0.0f, 0.0f, 0.5f }), WithinAbs(0.5, 1e-6));

    auto fb = base;
    fb.delayFeedback = 0.8f;
    fb.delayMix = 0.8f;
    const auto c = applyMacros(fb, MacroParams { 0.0f, 0.0f, 1.0f });
    CHECK_THAT(c.delayFeedback, WithinAbs(1.10, 1e-5)); // Obergrenze
    CHECK_THAT(c.delayMix, WithinAbs(1.0, 1e-6));

    // Space und Throw zusammen: Space bis 0,95, dann Throw bis 1,10.
    const auto both = applyMacros(base, MacroParams { 1.0f, 0.0f, 1.0f });
    CHECK_THAT(both.delayFeedback, WithinAbs(1.10, 1e-5));
}

TEST_CASE("a knob never lowers a value that is already above its ceiling", "[macros]")
{
    auto base = sampleBase();
    base.delayFeedback = 1.05f;
    CHECK_THAT(applyMacros(base, MacroParams { 1.0f, 0.0f, 0.0f }).delayFeedback, WithinAbs(1.05, 1e-6));
    base.delayFeedback = 1.10f;
    CHECK_THAT(applyMacros(base, MacroParams { 1.0f, 0.0f, 0.0f }).delayFeedback, WithinAbs(1.10, 1e-6));

    // Über ein Raster: außer Tone wird nichts gesenkt, Tone wird nie angehoben.
    for (const float fb : { 0.0f, 0.45f, 0.95f, 1.1f })
        for (const float s : { 0.0f, 0.25f, 0.5f, 1.0f })
            for (const float g : { 0.0f, 0.25f, 0.5f, 1.0f })
                for (const float t : { 0.0f, 0.25f, 0.5f, 1.0f })
                {
                    auto b = sampleBase();
                    b.delayFeedback = fb;
                    const auto e = applyMacros(b, MacroParams { s, g, t });
                    CHECK(e.delayFeedback >= b.delayFeedback);
                    CHECK(e.delayMix >= b.delayMix);
                    CHECK(e.reverbMix >= b.reverbMix);
                    CHECK(e.reverbDecay >= b.reverbDecay);
                    CHECK(e.drive >= b.drive);
                    CHECK(e.delayWow >= b.delayWow);
                    CHECK(e.phaserMix >= b.phaserMix);
                    CHECK(e.phaserDepth >= b.phaserDepth);
                    CHECK(e.delayTone <= b.delayTone);
                    CHECK(e.delayTone >= 0.0f);
                    CHECK(e.delayFeedback <= std::max(b.delayFeedback, 1.10f));
                }
}

TEST_CASE("invalid knob values are clamped", "[macros]")
{
    const auto base = sampleBase();
    const auto one = applyMacros(base, MacroParams { 1.0f, 1.0f, 1.0f });
    CHECK(same(applyMacros(base, MacroParams { 5.0f, 5.0f, 5.0f }), one));
    CHECK(same(applyMacros(base, MacroParams { -1.0f, -2.0f, -3.0f }), base));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(same(applyMacros(base, MacroParams { nan, nan, nan }), base));
    CHECK(throwSendBoost(MacroParams { 0.0f, 0.0f, nan }) == 0.0f);
    CHECK(throwSendBoost(MacroParams { 0.0f, 0.0f, 7.0f }) == 1.0f);
}
