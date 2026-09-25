#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "engine/SirenVoice.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
constexpr double kSr = 48000.0;

SlotParams plainSine(float hz)
{
    SlotParams p;
    p.wave = Waveform::Sine;
    p.pitchHz = hz;
    p.lfoDepthSemis = 0.0f;
    p.sweepSemis = 0.0f;
    p.attackS = 0.0f;
    p.releaseS = 0.05f;
    return p;
}

std::vector<float> renderN(SirenVoice& v, const VoiceContext& ctx, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n));
    v.render(out.data(), n, ctx);
    return out;
}
} // namespace

TEST_CASE("inactive voice renders silence", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(440.0f);
    VoiceContext ctx { &p, 120.0, {} };
    std::vector<float> out(512, 1.0f);
    v.render(out.data(), 512, ctx);
    CHECK(dgtest::peakAbs(out) == 0.0f);
    CHECK_FALSE(v.isActive());
}

TEST_CASE("plain voice plays its base pitch", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(440.0f);
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    const auto x = renderN(v, ctx, 48000);
    CHECK_THAT(dgtest::estimateFrequency(x, kSr, 480, x.size()), WithinAbs(440.0, 3.0));
    CHECK(dgtest::peakAbs(x, 4800) > 0.95f);
}

TEST_CASE("sweep starts at base plus amount and ends at base", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(400.0f);
    p.sweepSemis = 12.0f;
    p.sweepTimeS = 0.5f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 1);
    CHECK_THAT(v.currentFrequency(), WithinRel(800.0, 0.01));
    renderN(v, ctx, 12000); // letztes Sample bei 0,25 s → Hälfte des Sweeps
    CHECK_THAT(v.currentFrequency(), WithinRel(400.0 * std::exp2(6.0 / 12.0), 0.01));
    renderN(v, ctx, 13000);
    CHECK_THAT(v.currentFrequency(), WithinRel(400.0, 0.001));
}

TEST_CASE("square LFO jumps between base and base plus depth", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(300.0f);
    p.lfoShape = LfoShape::Square;
    p.lfoRateHz = 1.0f;
    p.lfoDepthSemis = 12.0f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 19200); // 0.4 s
    CHECK_THAT(v.currentFrequency(), WithinRel(300.0, 0.001));
    renderN(v, ctx, 14400); // 0.7 s
    CHECK_THAT(v.currentFrequency(), WithinRel(600.0, 0.001));
}

TEST_CASE("synced LFO follows tempo", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(300.0f);
    p.lfoShape = LfoShape::Square;
    p.lfoSync = true;
    p.lfoSyncDiv = SyncDivision::D1_4; // 2 Hz bei 120 bpm
    p.lfoDepthSemis = 12.0f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 4800); // 0.1 s
    CHECK_THAT(v.currentFrequency(), WithinRel(300.0, 0.001));
    renderN(v, ctx, 9600); // 0.3 s
    CHECK_THAT(v.currentFrequency(), WithinRel(600.0, 0.001));
}

TEST_CASE("performance pitch offset is applied immediately on start and smoothed afterwards", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(200.0f);
    VoiceContext ctx { &p, 120.0, {} };
    ctx.perf.pitchSemis = 12.0f;
    v.start(ctx, 1);
    renderN(v, ctx, 1);
    CHECK_THAT(v.currentFrequency(), WithinRel(400.0, 0.001));

    ctx.perf.pitchSemis = 0.0f;
    renderN(v, ctx, 48);  // 1 ms: noch nicht angekommen
    CHECK(v.currentFrequency() > 380.0f);
    renderN(v, ctx, 9600); // 200 ms: angekommen
    CHECK_THAT(v.currentFrequency(), WithinRel(200.0, 0.001));
}

TEST_CASE("negative depth offset clamps LFO depth at zero", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(300.0f);
    p.lfoDepthSemis = 5.0f;
    p.lfoRateHz = 1.0f;
    VoiceContext ctx { &p, 120.0, {} };
    ctx.perf.depthSemis = -24.0f;
    v.start(ctx, 1);
    renderN(v, ctx, 33600); // 0.7 s, LFO wäre oben
    CHECK_THAT(v.currentFrequency(), WithinRel(300.0, 0.001));
}

TEST_CASE("frequency is clamped to 18 kHz", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(5000.0f);
    p.sweepSemis = 48.0f;
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 1);
    CHECK(v.currentFrequency() == 18000.0f);
}

TEST_CASE("voice ends after release", "[voice]")
{
    SirenVoice v;
    v.prepare(kSr);
    SlotParams p = plainSine(440.0f);
    VoiceContext ctx { &p, 120.0, {} };
    v.start(ctx, 1);
    renderN(v, ctx, 480);
    v.release(ctx);
    CHECK(v.isReleasing());
    renderN(v, ctx, 2400 + 10);
    CHECK_FALSE(v.isActive());
}
