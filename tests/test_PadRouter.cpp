#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string>
#include <vector>
#include "engine/PadRouter.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

struct FakeVoices final : VoiceControl
{
    std::array<bool, kNumSlots> active {};
    std::array<bool, kNumSlots> releasing {};
    std::vector<std::string> log;

    void startVoice(int s) override { active[s] = true; releasing[s] = false; log.push_back("start " + std::to_string(s)); }
    void releaseVoice(int s) override { releasing[s] = true; log.push_back("release " + std::to_string(s)); }
    void killVoice(int s) override { active[s] = false; releasing[s] = false; log.push_back("kill " + std::to_string(s)); }
    bool isVoiceActive(int s) const override { return active[s]; }
    bool isVoiceReleasing(int s) const override { return releasing[s]; }
};

TriggerSettingsArray allMode(TriggerMode m)
{
    TriggerSettingsArray a {};
    for (auto& t : a)
        t = { m, 1.0f, 0 };
    return a;
}

PadRouter makeRouter()
{
    PadRouter r;
    r.prepare(kSr);
    return r;
}

using Log = std::vector<std::string>;
} // namespace

TEST_CASE("notes outside 36..51 are ignored", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Gate);
    r.noteOn(35, s, v);
    r.noteOn(52, s, v);
    r.noteOff(35, v);
    CHECK(v.log.empty());
    CHECK(slotForNote(36) == 0);
    CHECK(slotForNote(51) == 15);
    CHECK(slotForNote(52) == -1);
}

TEST_CASE("gate: note on starts, note off releases, retrigger restarts", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Gate);
    r.noteOn(36, s, v);
    r.noteOn(36, s, v);
    r.noteOff(36, v);
    CHECK(v.log == Log { "start 0", "start 0", "release 0" });
}

TEST_CASE("latch: toggles on note on, ignores note off", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Latch);
    r.noteOn(37, s, v);
    CHECK(r.isLatched(1));
    r.noteOff(37, v);
    r.noteOn(37, s, v);
    CHECK_FALSE(r.isLatched(1));
    CHECK(v.log == Log { "start 1", "release 1" });
}

TEST_CASE("latch: tapping again while releasing restarts the siren", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Latch);
    r.noteOn(37, s, v);
    r.noteOn(37, s, v); // Release
    r.noteOn(37, s, v); // noch im Release → Neustart
    CHECK(v.log == Log { "start 1", "release 1", "start 1" });
    CHECK(r.isLatched(1));
}

TEST_CASE("one-shot: releases automatically after its length and ignores note off", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::OneShot);
    s[2].oneShotS = 0.5f;
    r.noteOn(38, s, v);
    r.noteOff(38, v);
    r.advance(23999, v);
    CHECK(v.log == Log { "start 2" });
    r.advance(1, v);
    CHECK(v.log == Log { "start 2", "release 2" });
    r.advance(48000, v);
    CHECK(v.log.size() == 2);
}

TEST_CASE("one-shot: retrigger restarts the timer", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::OneShot);
    s[2].oneShotS = 0.5f;
    r.noteOn(38, s, v);
    r.advance(20000, v);
    r.noteOn(38, s, v);
    r.advance(20000, v);
    CHECK(v.log == Log { "start 2", "start 2" });
    r.advance(4000, v);
    CHECK(v.log.back() == "release 2");
}

TEST_CASE("choke kills active voices in the same group only", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Latch);
    s[0].chokeGroup = 1;
    s[1].chokeGroup = 1;
    s[2].chokeGroup = 2;
    s[3].chokeGroup = 0;
    r.noteOn(36, s, v); // slot 0
    r.noteOn(38, s, v); // slot 2
    r.noteOn(39, s, v); // slot 3
    r.noteOn(37, s, v); // slot 1 → würgt slot 0 ab
    CHECK(v.log == Log { "start 0", "start 2", "start 3", "kill 0", "start 1" });
    CHECK_FALSE(r.isLatched(0));
    CHECK(r.isLatched(2));
}

TEST_CASE("choke skips inactive voices and group 0 never chokes", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Gate);
    s[0].chokeGroup = 1;
    s[1].chokeGroup = 1;
    r.noteOn(37, s, v);
    CHECK(v.log == Log { "start 1" });

    auto r2 = makeRouter();
    FakeVoices v2;
    const auto s0 = allMode(TriggerMode::Gate);
    r2.noteOn(36, s0, v2);
    r2.noteOn(37, s0, v2);
    CHECK(v2.log == Log { "start 0", "start 1" });
}

TEST_CASE("focus follows the last note on, including latch release", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    const auto s = allMode(TriggerMode::Latch);
    CHECK(r.focusSlot() == 0);
    r.noteOn(40, s, v);
    CHECK(r.focusSlot() == 4);
    r.noteOn(45, s, v);
    CHECK(r.focusSlot() == 9);
    r.noteOn(40, s, v); // beendet slot 4
    CHECK(r.focusSlot() == 4);
    r.setFocusSlot(7);
    CHECK(r.focusSlot() == 7);
}

TEST_CASE("transport stop acts on latched voices only", "[router]")
{
    auto s = allMode(TriggerMode::Latch);
    s[1].mode = TriggerMode::Gate;

    SECTION("continue")
    {
        auto r = makeRouter();
        FakeVoices v;
        r.noteOn(36, s, v);
        r.noteOn(37, s, v);
        r.transportStopped(LatchStopAction::Continue, v);
        CHECK(v.log == Log { "start 0", "start 1" });
        CHECK(r.isLatched(0));
    }
    SECTION("release")
    {
        auto r = makeRouter();
        FakeVoices v;
        r.noteOn(36, s, v);
        r.noteOn(37, s, v);
        r.transportStopped(LatchStopAction::Release, v);
        CHECK(v.log == Log { "start 0", "start 1", "release 0" });
        CHECK_FALSE(r.isLatched(0));
    }
    SECTION("stop")
    {
        auto r = makeRouter();
        FakeVoices v;
        r.noteOn(36, s, v);
        r.noteOn(37, s, v);
        r.transportStopped(LatchStopAction::Stop, v);
        CHECK(v.log == Log { "start 0", "start 1", "kill 0" });
        CHECK(r.latchedMask() == 0u);
    }
}

TEST_CASE("panic kills everything and clears latches and one-shot timers", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Latch);
    s[1].mode = TriggerMode::OneShot;
    r.noteOn(36, s, v);
    r.noteOn(37, s, v);
    r.panic(v);
    CHECK(v.log == Log { "start 0", "start 1", "kill 0", "kill 1" });
    CHECK(r.latchedMask() == 0u);
    r.advance(480000, v);
    CHECK(v.log.size() == 4);
}

TEST_CASE("preview behaves like gate regardless of mode and sets focus", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Latch);
    s[5].chokeGroup = 1;
    s[6].chokeGroup = 1;
    r.noteOn(42, s, v); // slot 6, gelatcht
    r.previewOn(5, s, v);
    CHECK(r.focusSlot() == 5);
    CHECK_FALSE(r.isLatched(5));
    r.previewOff(5, v);
    CHECK(v.log == Log { "start 6", "kill 6", "start 5", "release 5" });
    r.previewOff(5, v);    // zweites Loslassen tut nichts
    r.previewOn(99, s, v); // ungültiger Slot
    CHECK(v.log.size() == 4);
}

TEST_CASE("note off uses the mode the voice was started with", "[router]")
{
    auto r = makeRouter();
    FakeVoices v;
    auto s = allMode(TriggerMode::Gate);
    r.noteOn(36, s, v);
    s[0].mode = TriggerMode::Latch; // Automation, während das Pad gehalten wird
    r.noteOff(36, v);
    CHECK(v.log == Log { "start 0", "release 0" });
}
