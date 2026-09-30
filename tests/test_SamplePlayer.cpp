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

constexpr float kRampUnit = 1.0e-4f;

// samples[i] = i * kRampUnit: Der Ausgabewert verrät die gelesene Position.
SampleData rampData(int numSamples, double sampleRate = kSr)
{
    SampleData d;
    d.sampleRate = sampleRate;
    d.samples.resize(static_cast<std::size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        d.samples[static_cast<std::size_t>(i)] = static_cast<float>(i) * kRampUnit;
    return d;
}

float rampAt(double pos) { return static_cast<float>(pos) * kRampUnit; }
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

TEST_CASE("start and end markers limit the playback", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.sampleStart = 0.25f; // 1200
    p.sampleEnd = 0.5f;    // 2400
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 2000);
    CHECK_THAT(out[200], WithinAbs(rampAt(1400), 1e-3));
    CHECK_THAT(out[1000], WithinAbs(rampAt(2200), 1e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 1202) == 0.0f);
    CHECK(out[1199] < out[1150]); // 2-ms-Rampe vor dem End-Marker
}

TEST_CASE("reverse plays the region backwards", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.sampleStart = 0.25f;
    p.sampleEnd = 0.5f;
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 2000);
    CHECK_THAT(out[200], WithinAbs(rampAt(2399 - 200), 1e-3));
    CHECK_THAT(out[1000], WithinAbs(rampAt(2399 - 1000), 1e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 1202) == 0.0f);
    CHECK(dgtest::allFinite(out));
}

TEST_CASE("reverse of the whole file ends at its first sample", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 6000);
    CHECK_THAT(out[100], WithinAbs(rampAt(4799 - 100), 1e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 4802) == 0.0f);
    CHECK(dgtest::allFinite(out));
}

TEST_CASE("tune applies in reverse as well", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.sampleStart = 0.25f;
    p.sampleEnd = 0.5f;
    p.reverse = true;
    p.tuneSemis = 12.0f; // doppelte Geschwindigkeit
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 1000);
    CHECK_THAT(out[200], WithinAbs(rampAt(2399 - 400), 2e-3));
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 602) == 0.0f);
}

TEST_CASE("crossed markers still play the minimum region", "[sampler]")
{
    const auto d = rampData(4800);
    for (const bool reverse : { false, true })
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = sampleParams();
        p.sampleStart = 0.9f;
        p.sampleEnd = 0.1f; // Bereich 464..480
        p.reverse = reverse;
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        REQUIRE(v.isActive());
        const auto out = renderN(v, ctx, 200);
        CHECK_FALSE(v.isActive());
        CHECK(dgtest::allFinite(out));
        CHECK(dgtest::peakAbs(out, 0, 16) > 0.0f);
        CHECK(dgtest::peakAbs(out, 18) == 0.0f);
    }
}

TEST_CASE("retrigger in reverse crossfades without a jump", "[sampler]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    auto out = renderN(v, ctx, 4700); // kurz vor dem Dateianfang
    v.start(ctx, 2);
    const auto after = renderN(v, ctx, 600);
    out.insert(out.end(), after.begin(), after.end());
    CHECK(dgtest::allFinite(out));
    CHECK(maxStep(out, 4600, 5300) < 0.005f);
    CHECK(v.isActive());
}

namespace {
SlotParams loopParams(float start, float loopStart, float end, float xfade = 0.0f)
{
    SlotParams p = sampleParams();
    p.sampleStart = start;
    p.loopStart = loopStart;
    p.sampleEnd = end;
    p.loop = true;
    p.loopXfadePct = xfade;
    return p;
}
} // namespace

TEST_CASE("a forward loop cycles between loop start and end", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f); // L 2400, E 3600
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 6000);
    CHECK_THAT(out[3000], WithinAbs(rampAt(3000), 1e-3));
    CHECK_THAT(out[3600], WithinAbs(rampAt(2400), 1e-3)); // Rücksprung
    CHECK_THAT(out[3700], WithinAbs(rampAt(2500), 1e-3));
    CHECK_THAT(out[4800], WithinAbs(rampAt(2400), 1e-3)); // zweiter Durchlauf
    CHECK(v.isActive());
}

TEST_CASE("a reverse loop cycles between start and loop start", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.25f, 0.5f, 0.75f); // S 1200, L 2400, E 3600
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 6000);
    CHECK_THAT(out[100], WithinAbs(rampAt(3499), 1e-3));   // erster Durchlauf von End abwärts
    CHECK_THAT(out[2399], WithinAbs(rampAt(1200), 1e-3));  // unten angekommen
    CHECK_THAT(out[2400], WithinAbs(rampAt(2399), 1e-3));  // Sprung zu Loop-Start
    CHECK_THAT(out[2500], WithinAbs(rampAt(2299), 1e-3));
    CHECK_THAT(out[3600], WithinAbs(rampAt(2399), 1e-3));  // zweiter Durchlauf
    CHECK(v.isActive());
}

TEST_CASE("an empty loop segment loops the whole region", "[sampler][loop]")
{
    const auto d = rampData(4800);
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.25f, 0.75f, 0.75f); // Loop-Start = End
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 3000);
        CHECK_THAT(out[2400], WithinAbs(rampAt(1200), 1e-3));
        CHECK(v.isActive());
    }
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.0f, 1.0f); // Standard-Marker, rückwärts
        p.reverse = true;
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 5000);
        CHECK_THAT(out[4800], WithinAbs(rampAt(4799), 1e-3));
        CHECK(v.isActive());
    }
}

TEST_CASE("one shot ignores the loop", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f);
    p.trigMode = TriggerMode::OneShot;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 5000);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 3602) == 0.0f);
}

TEST_CASE("x-fade 0 jumps hard, a larger value makes the seam continuous", "[sampler][loop]")
{
    const auto d = rampData(4800);
    const auto render = [&](float xfade) {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.5f, 0.75f, xfade);
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        return renderN(v, ctx, 5000);
    };
    const auto hard = render(0.0f);
    CHECK(maxStep(hard, 3500, 3700) > 0.1f); // Sprung von 3600 auf 2400

    const auto soft = render(25.0f); // 300 Samples
    CHECK(maxStep(soft, 3000, 4000) < 0.002f);
    CHECK_THAT(soft[3600], WithinAbs(rampAt(2400), 1e-3));
    CHECK_THAT(soft[3299], WithinAbs(rampAt(3299), 1e-3)); // vor der Zone unverändert
    CHECK(dgtest::allFinite(soft));
}

TEST_CASE("x-fade shrinks when there is not enough material before the loop start", "[sampler][loop]")
{
    const auto d = rampData(4800);
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.01f, 0.5f, 50.0f); // L 48, E 2400: gewünscht 1176, möglich 48
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 3000);
        CHECK(dgtest::allFinite(out));
        CHECK(maxStep(out, 2300, 2500) < 0.01f);
        CHECK_THAT(out[2300], WithinAbs(rampAt(2300), 1e-3)); // außerhalb der verkürzten Zone unverändert
    }
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.0f, 0.5f, 50.0f); // kein Material davor: harter Schnitt
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto out = renderN(v, ctx, 3000);
        CHECK(dgtest::allFinite(out));
        CHECK(maxStep(out, 2300, 2500) > 0.2f);
    }
}

TEST_CASE("a reverse loop crossfades into the material above its segment", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.25f, 0.5f, 0.75f, 25.0f); // Segment 1200..2400, xf 300
    p.reverse = true;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 5000);
    CHECK(dgtest::allFinite(out));
    CHECK(maxStep(out, 2000, 2800) < 0.002f); // Naht bei 2400
    CHECK(v.isActive());
}

TEST_CASE("moving the end marker behind the position jumps to the loop start without a click", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.25f, 0.75f); // L 1200, E 3600
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto before = renderN(v, ctx, 3000); // Position 3000
    p.sampleEnd = 0.5f;                        // End 2400: Position liegt dahinter
    const auto after = renderN(v, ctx, 1000);
    CHECK(std::abs(after[0] - before.back()) < 0.002f);
    CHECK(maxStep(after, 1, 1000) < 0.002f);
    CHECK_THAT(after[300], WithinAbs(rampAt(1500), 1e-3)); // 5-ms-Überblendung ist vorbei
    CHECK(v.isActive());
}

TEST_CASE("moving the end marker behind the position without a loop fades the voice out", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    const auto before = renderN(v, ctx, 3000);
    p.sampleEnd = 0.5f;
    const auto after = renderN(v, ctx, 600);
    CHECK_FALSE(v.isActive());
    CHECK(std::abs(after[0] - before.back()) < 0.005f);
    CHECK(maxStep(after, 1, 600) < 0.005f);
    CHECK(dgtest::peakAbs(after, 300) == 0.0f);
    CHECK(dgtest::allFinite(after));
}

TEST_CASE("moving the start marker past the position keeps playing", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 1000);
    p.sampleStart = 0.5f;
    const auto after = renderN(v, ctx, 1000);
    CHECK(v.isActive());
    CHECK_THAT(after[100], WithinAbs(rampAt(1100), 1e-3));
}

TEST_CASE("switching reverse while playing turns around at the current position", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = sampleParams();
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 1000);
    p.reverse = true;
    const auto after = renderN(v, ctx, 500);
    CHECK_THAT(after[0], WithinAbs(rampAt(1000), 1e-3));
    CHECK_THAT(after[100], WithinAbs(rampAt(900), 1e-3));
    CHECK(v.isActive());
}

TEST_CASE("a loop shorter than one step stays inside its segment", "[sampler][loop]")
{
    const auto d = rampData(4800, 96000.0); // doppelte Dateirate: Grundschritt 2
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 2424.0f / 4800.0f, 50.0f); // Segment 2400..2424 (24 Samples)
    p.tuneSemis = 24.0f;
    PerfOffsets perf;
    perf.pitchSemis = 24.0f; // zusammen Faktor 16, Schritt 32: länger als das Segment
    VoiceContext ctx { &p, 120.0, perf, &d };
    v.start(ctx, 1);
    const auto out = renderN(v, ctx, 4800);
    CHECK(v.isActive());
    CHECK(dgtest::allFinite(out));
    CHECK(dgtest::peakAbs(out) <= rampAt(2430));
    CHECK(dgtest::peakAbs(out, 2400) >= rampAt(2380));
}

TEST_CASE("retrigger in the middle of the crossfade stays smooth", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f, 25.0f); // Zone 3300..3600
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    auto out = renderN(v, ctx, 3450);
    v.start(ctx, 2);
    const auto after = renderN(v, ctx, 600);
    out.insert(out.end(), after.begin(), after.end());
    CHECK(dgtest::allFinite(out));
    CHECK(maxStep(out, 3400, 4050) < 0.005f);
    CHECK(dgtest::peakAbs(out) < 0.4f); // keine Pegelüberhöhung
}

TEST_CASE("a loop keeps running during the release and then ends", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f);
    p.releaseS = 0.05f; // 2400 Samples
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 3500);
    v.release(ctx);
    const auto tail = renderN(v, ctx, 1200); // läuft über das Bereichsende hinweg
    CHECK(v.isActive());
    CHECK(dgtest::peakAbs(tail, 600) > 0.0f);
    renderN(v, ctx, 2400);
    CHECK_FALSE(v.isActive());
}

TEST_CASE("moving a marker behind the position does not click when the loop crossfades", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SECTION("forward: end marker")
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.0f, 0.25f, 0.75f, 25.0f); // L 1200, E 3600
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto before = renderN(v, ctx, 3000);
        p.sampleEnd = 0.5f; // End 2400: die Position 3000 liegt dahinter
        const auto after = renderN(v, ctx, 1000);
        CHECK(std::abs(after[0] - before.back()) < 0.002f);
        CHECK(maxStep(after, 1, 1000) < 0.002f);
        CHECK_THAT(after[300], WithinAbs(rampAt(1500), 1e-3));
        CHECK(v.isActive());
    }
    SECTION("reverse: start marker")
    {
        SamplePlayer v;
        v.prepare(kSr);
        SlotParams p = loopParams(0.25f, 0.5f, 0.75f, 25.0f); // S 1200, L 2400, E 3600
        p.reverse = true;
        VoiceContext ctx { &p, 120.0, {}, &d };
        v.start(ctx, 1);
        const auto before = renderN(v, ctx, 600); // Position 2999
        p.sampleStart = 0.6875f;                  // Start 3300: die Position liegt in Laufrichtung dahinter
        const auto after = renderN(v, ctx, 1000);
        CHECK(std::abs(after[0] - before.back()) < 0.002f);
        CHECK(maxStep(after, 1, 1000) < 0.002f);
        CHECK(dgtest::peakAbs(after, 300) <= rampAt(3600));
        CHECK(dgtest::peakAbs(after, 300) >= rampAt(3300) - 1e-3f);
        CHECK(v.isActive());
    }
}

TEST_CASE("a voice started as one shot never starts looping", "[sampler][loop]")
{
    const auto d = rampData(4800);
    SamplePlayer v;
    v.prepare(kSr);
    SlotParams p = loopParams(0.0f, 0.5f, 0.75f);
    p.trigMode = TriggerMode::OneShot;
    VoiceContext ctx { &p, 120.0, {}, &d };
    v.start(ctx, 1);
    renderN(v, ctx, 1000);
    p.trigMode = TriggerMode::Gate; // Modus während des Spielens umgestellt: keine Note hält die Stimme
    const auto out = renderN(v, ctx, 5000);
    CHECK_FALSE(v.isActive());
    CHECK(dgtest::peakAbs(out, 2602) == 0.0f);

    // Eine danach neu gestartete Stimme loopt wie eingestellt.
    v.start(ctx, 2);
    renderN(v, ctx, 6000);
    CHECK(v.isActive());
}
