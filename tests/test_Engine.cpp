#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include "engine/Engine.h"
#include "engine/Kit.h"
#include "engine/Limiter.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

EngineParams testParams()
{
    EngineParams p;
    for (auto& s : p.slots)
    {
        s.wave = Waveform::Sine;
        s.pitchHz = 440.0f;
        s.lfoDepthSemis = 0.0f;
        s.sweepSemis = 0.0f;
        s.attackS = 0.0f;
        s.releaseS = 0.01f;
        s.volumeDb = 0.0f;
        s.pan = 0.0f;
        s.fxSend = 0.0f;
        s.trigMode = TriggerMode::Gate;
        s.chokeGroup = 0;
    }
    p.global.fx.drive = 0.0f;
    p.global.fx.cutoffHz = 20000.0f;
    p.global.fx.delayMix = 0.0f;
    p.global.fx.reverbMix = 0.0f;
    p.global.fx.masterDb = 0.0f;
    return p;
}

struct Out { std::vector<float> l, r; };

Out run(Engine& e, const EngineParams& p, int n, std::vector<EngineEvent> ev = {}, TransportInfo t = {})
{
    Out o { std::vector<float>(static_cast<std::size_t>(n)), std::vector<float>(static_cast<std::size_t>(n)) };
    e.process(o.l.data(), o.r.data(), n, p, ev.data(), static_cast<int>(ev.size()), t);
    return o;
}

EngineEvent noteOn(int note, int offset = 0) { return { EngineEvent::Type::NoteOn, offset, note }; }
EngineEvent noteOff(int note, int offset = 0) { return { EngineEvent::Type::NoteOff, offset, note }; }
EngineEvent panicEvent() { return { EngineEvent::Type::Panic, 0, 0 }; }
} // namespace

TEST_CASE("engine is silent without events", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto o = run(e, testParams(), 4800);
    CHECK(dgtest::peakAbs(o.l) == 0.0f);
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("note 36 plays slot 1 at its pitch with centre pan", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto p = testParams();
    const auto o = run(e, p, 48000, { noteOn(36) });
    CHECK_THAT(dgtest::estimateFrequency(o.l, kSr, 4800, o.l.size()), WithinAbs(440.0, 3.0));
    CHECK(dgtest::peakAbs(o.l, 4800) > 0.6f);
    CHECK(dgtest::peakAbs(o.l, 4800) < 0.8f);
    CHECK_THAT(dgtest::peakAbs(o.r, 4800), WithinAbs(dgtest::peakAbs(o.l, 4800), 1e-3));
    CHECK(e.activeMask() == 1u);
    CHECK(e.focusSlot() == 0);
}

TEST_CASE("notes outside the pad range are ignored", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto o = run(e, testParams(), 4800, { noteOn(35), noteOn(52) });
    CHECK(dgtest::peakAbs(o.l) == 0.0f);
}

TEST_CASE("events are sample accurate", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto o = run(e, testParams(), 512, { noteOn(36, 100) });
    CHECK(dgtest::peakAbs(o.l, 0, 100) == 0.0f);
    CHECK(dgtest::peakAbs(o.l, 101) > 0.0f);
}

TEST_CASE("gate note off releases the voice", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    const auto p = testParams();
    run(e, p, 4800, { noteOn(36) });
    run(e, p, 4800, { noteOff(36) });
    CHECK(e.activeMask() == 0u);
    const auto o = run(e, p, 4800);
    CHECK(dgtest::peakAbs(o.l) < 1.0e-4f);
}

TEST_CASE("panic silences latched voices within 5 ms", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::Latch;
    run(e, p, 4800, { noteOn(36) });
    CHECK(e.latchedMask() == 1u);
    const auto o = run(e, p, 4800, { panicEvent() });
    CHECK(e.activeMask() == 0u);
    CHECK(e.latchedMask() == 0u);
    CHECK(dgtest::peakAbs(o.l, 300) < 1.0e-4f);
}

TEST_CASE("choke fades the previous voice without a click", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].pitchHz = 200.0f;
    p.slots[0].chokeGroup = 1;
    p.slots[1].chokeGroup = 1;
    p.slots[1].volumeDb = -60.0f; // stumm, damit nur der Fade von Slot 1 zu hören ist
    run(e, p, 4800, { noteOn(36) });
    const auto o = run(e, p, 4800, { noteOn(37) });
    CHECK(dgtest::maxStep(o.l, 1, 400) <= 0.03f);
    CHECK(dgtest::peakAbs(o.l, 300) < 1.0e-4f);
    CHECK(e.activeMask() == 2u);
}

TEST_CASE("transport stop handles latched voices according to the setting", "[engine]")
{
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::Latch;
    const TransportInfo playing { true, 120.0 };
    const TransportInfo stopped { false, 120.0 };

    SECTION("continue")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Continue;
        run(e, p, 512, { noteOn(36) }, playing);
        run(e, p, 4800, {}, stopped);
        CHECK(e.activeMask() == 1u);
    }
    SECTION("release")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Release;
        run(e, p, 512, { noteOn(36) }, playing);
        run(e, p, 300, {}, stopped);
        CHECK(e.activeMask() == 1u); // Release (10 ms) läuft noch
        run(e, p, 300, {}, stopped);
        CHECK(e.activeMask() == 0u);
    }
    SECTION("stop")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Stop;
        run(e, p, 512, { noteOn(36) }, playing);
        run(e, p, 300, {}, stopped);
        CHECK(e.activeMask() == 0u);
    }
    SECTION("no action without a playing to stopped transition")
    {
        Engine e;
        e.prepare(kSr, 512);
        p.global.latchStop = LatchStopAction::Stop;
        run(e, p, 512, { noteOn(36) }, stopped);
        run(e, p, 512, {}, stopped);
        CHECK(e.activeMask() == 1u);
    }
}

TEST_CASE("performance offsets follow the focus target", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::Latch;
    p.slots[1].trigMode = TriggerMode::Latch;
    run(e, p, 64, { noteOn(36), noteOn(37) }); // Fokus = Slot 2 (Index 1)

    p.global.perf.pitchSemis = 12.0f;
    p.global.perfTarget = PerfTarget::Focus;
    run(e, p, 64);
    CHECK(e.appliedPerf(1).pitchSemis == 12.0f);
    CHECK(e.appliedPerf(0).pitchSemis == 0.0f);

    p.global.perfTarget = PerfTarget::All;
    run(e, p, 64);
    CHECK(e.appliedPerf(0).pitchSemis == 12.0f);

    p.global.perfTarget = PerfTarget::Focus;
    p.global.perf.pitchSemis = 5.0f;
    run(e, p, 64);
    CHECK(e.appliedPerf(0).pitchSemis == 12.0f); // behält seinen letzten Offset
    CHECK(e.appliedPerf(1).pitchSemis == 5.0f);
}

TEST_CASE("blocks larger than maxBlockSize are processed correctly", "[engine]")
{
    Engine e;
    e.prepare(kSr, 64);
    const auto o = run(e, testParams(), 1000, { noteOn(36, 900) });
    CHECK(dgtest::peakAbs(o.l, 0, 900) == 0.0f);
    CHECK(dgtest::peakAbs(o.l, 901) > 0.0f);
}

TEST_CASE("events beyond the block are applied at the end", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    run(e, testParams(), 512, { noteOn(36, 600) });
    CHECK(e.activeMask() == 1u);
}

TEST_CASE("preview events start and release a slot", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[4].trigMode = TriggerMode::Latch;
    run(e, p, 512, { { EngineEvent::Type::PreviewOn, 0, 4 } });
    CHECK(e.activeMask() == (1u << 4));
    CHECK(e.focusSlot() == 4);
    run(e, p, 2400, { { EngineEvent::Type::PreviewOff, 0, 4 } });
    CHECK(e.activeMask() == 0u);
}

TEST_CASE("one-shot release is sample accurate regardless of block size", "[engine]")
{
    Engine e;
    e.prepare(kSr, 8192);
    auto p = testParams();
    p.slots[0].trigMode = TriggerMode::OneShot;
    p.slots[0].oneShotS = 0.1f;
    p.slots[0].releaseS = 0.0f; // 1 ms Minimum = 48 Samples bei 48 kHz
    const auto o = run(e, p, 8192, { noteOn(36) });
    CHECK(dgtest::peakAbs(o.l, 0, 4700) > 0.5f);
    CHECK(dgtest::peakAbs(o.l, 4800 + 48 + 16) < 1.0e-4f);
}

TEST_CASE("the whole factory kit renders finite and below the ceiling", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    EngineParams p;
    const Kit k = makeFactoryKit();
    p.slots = k.slots;
    std::vector<EngineEvent> ev;
    for (int n = 36; n <= 51; ++n)
        ev.push_back(noteOn(n));

    std::vector<float> all;
    for (int block = 0; block < 188; ++block) // ~2 s
    {
        const auto o = run(e, p, 512, block == 0 ? ev : std::vector<EngineEvent> {});
        all.insert(all.end(), o.l.begin(), o.l.end());
    }
    CHECK(dgtest::allFinite(all));
    CHECK(dgtest::peakAbs(all) <= kLimiterCeiling + 1.0e-6f);
    CHECK(dgtest::rms(all) > 0.01f);
}

TEST_CASE("slot volume changes are smoothed", "[engine]")
{
    Engine e;
    e.prepare(kSr, 512);
    auto p = testParams();
    p.slots[0].pitchHz = 200.0f;

    // Beide Segmente landen im selben Puffer, damit der Übergang an der Blockgrenze
    // (wo ein ungeglätteter Gain-Sprung passieren würde) erfasst wird.
    std::vector<float> l(4800 + 512), r(4800 + 512);
    // Kleiner Offset, damit die Blockgrenze nicht zufällig auf einen Nulldurchgang der
    // 200-Hz-Sinuswelle (Periode 240 Samples, Teiler von 4800) fällt.
    auto ev = std::vector<EngineEvent> { noteOn(36, 5) };
    e.process(l.data(), r.data(), 4800, p, ev.data(), static_cast<int>(ev.size()), {});

    p.slots[0].volumeDb = -60.0f;
    e.process(l.data() + 4800, r.data() + 4800, 512, p, nullptr, 0, {});

    CHECK(dgtest::maxStep(l, 4790) < 0.03f);
}
