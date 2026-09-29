#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/SamplePlayer.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

SampleData sineData(float hz, double sampleRate, double seconds)
{
    SampleData d;
    d.sampleRate = sampleRate;
    d.samples = dgtest::sine(hz, sampleRate, static_cast<int>(seconds * sampleRate), 0.5f);
    return d;
}

SlotParams sampleParams()
{
    SlotParams p;
    p.source = SourceType::Sample;
    p.attackS = 0.0f;
    p.releaseS = 0.01f;
    p.tuneSemis = 0.0f;
    return p;
}

std::vector<float> renderN(SamplePlayer& v, const VoiceContext& ctx, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n), 1.0f);
    v.render(out.data(), n, ctx);
    return out;
}
} // namespace

TEST_CASE("sample player without data does not start", "[sampler]")
{
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, nullptr };
    v.start(ctx, 1);
    CHECK_FALSE(v.isActive());
    SampleData empty;
    ctx.sample = &empty;
    v.start(ctx, 1);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(renderN(v, ctx, 64)) == 0.0f);
}

TEST_CASE("sample player keeps the pitch at equal and different sample rates", "[sampler]")
{
    for (double fileRate : { 48000.0, 44100.0 })
    {
        const auto d = sineData(441.0f, fileRate, 1.0);
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 24000);
        CHECK_THAT(dgtest::estimateFrequency(out, kSr, 2400, out.size()), WithinAbs(441.0, 4.0));
        CHECK(dgtest::peakAbs(out, 2400) > 0.45f);
    }
}

TEST_CASE("tune and performance pitch transpose the sample", "[sampler]")
{
    const auto d = sineData(400.0f, kSr, 1.0);
    const auto freqWith = [&](float tune, float perf) {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        p.tuneSemis = tune;
        PerfOffsets po;
        po.pitchSemis = perf;
        VoiceContext ctx { &p, 120.0, po, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 9600);
        return dgtest::estimateFrequency(out, kSr, 960, out.size());
    };
    CHECK_THAT(freqWith(12.0f, 0.0f), WithinAbs(800.0, 8.0));
    CHECK_THAT(freqWith(-12.0f, 0.0f), WithinAbs(200.0, 4.0));
    CHECK_THAT(freqWith(0.0f, 12.0f), WithinAbs(800.0, 8.0));
}

TEST_CASE("sample player stops at the end of the sample", "[sampler]")
{
    const auto d = sineData(441.0f, kSr, 0.1); // 4800 Samples
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 9600);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 0, 4700) > 0.4f);
    CHECK(dgtest::peakAbs(out, 4802) == 0.0f);
}

TEST_CASE("a one-sample file and an attack longer than the sample end cleanly", "[sampler]")
{
    SampleData one;
    one.sampleRate = kSr;
    one.samples = { 0.5f };
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.attackS = 1.0f;
    VoiceContext ctx { &p, 120.0, {}, &one };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 64);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::allFinite(out));
    CHECK(dgtest::peakAbs(out, 3) == 0.0f);
}

TEST_CASE("release fades the sample out", "[sampler]")
{
    const auto d = sineData(441.0f, kSr, 1.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 4800);
    v.release(ctx);
    CHECK(v.isReleasing());
    const auto out = renderN(v, ctx, 4800);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 600) == 0.0f);
}

TEST_CASE("losing the data while playing stops the voice", "[sampler]")
{
    const auto d = sineData(441.0f, kSr, 1.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 480);
    ctx.sample = nullptr;
    const auto out = renderN(v, ctx, 480);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out) == 0.0f);
}

namespace {
SampleData dcData(double seconds)
{
    SampleData d;
    d.sampleRate = kSr;
    d.samples.assign(static_cast<std::size_t>(seconds * kSr), 1.0f);
    return d;
}

float maxStep(const std::vector<float>& x, std::size_t from, std::size_t to)
{
    float m = 0.0f;
    for (std::size_t i = from + 1; i < to; ++i)
        m = std::max(m, std::abs(x[i] - x[i - 1]));
    return m;
}
} // namespace

TEST_CASE("sample end ramps down without a click", "[sampler]")
{
    const auto d = dcData(0.1); // 4800 Samples
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 4900);
    CHECK_FALSE(v.isActive());
    CHECK(maxStep(out, 4000, 4900) < 0.05f);
    for (std::size_t i = 4704 + 1; i < 4800; ++i)
        CHECK(out[i] <= out[i - 1]);
    CHECK(out[4799] < 0.05f);
}

TEST_CASE("retrigger crossfades the old read head", "[sampler]")
{
    const auto d = dcData(1.0);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    auto out = renderN(v, ctx, 2400);
    v.start(ctx, 2);
    const auto out2 = renderN(v, ctx, 480);
    out.insert(out.end(), out2.begin(), out2.end());
    CHECK(maxStep(out, 0, out.size()) < 0.05f);
    CHECK(v.isActive());
}

TEST_CASE("retrigger of a tone has no jump at the restart", "[sampler]")
{
    // Sinus: ohne Überblendung springt der Ausgang beim Neustart.
    SampleData d;
    d.sampleRate = kSr;
    d.samples = dgtest::sine(200.0f, kSr, 48000, 0.9f);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    auto out = renderN(v, ctx, 300); // Periode 240 Samples: Position 300 liegt am Maximum
    v.start(ctx, 2); // Position 300 (Peak), Neustart bei Phase 0
    // Ohne Fade: Sprung von ~0.9 auf 0. Der Ausgang muss stetig bleiben.
    const auto more = renderN(v, ctx, 480);
    out.insert(out.end(), more.begin(), more.end());
    CHECK(maxStep(out, 0, out.size()) < 0.1f);
}

TEST_CASE("retrigger shortly before the sample end stays continuous", "[sampler]")
{
    for (const bool dc : { false, true })
    {
        SampleData d;
        if (dc)
            d = dcData(0.1);
        else
        {
            d.sampleRate = kSr;
            d.samples = dgtest::sine(200.0f, kSr, 4800, 0.5f);
        }
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        auto out = renderN(v, ctx, 4752); // 1 ms vor dem Ende
        v.start(ctx, 2);
        const auto more = renderN(v, ctx, 480); // 10 ms danach
        out.insert(out.end(), more.begin(), more.end());
        CHECK(maxStep(out, 1, out.size()) < 0.05f);
        CHECK(v.isActive());
    }
}
