#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <set>
#include <string>
#include "engine/SlotFields.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

TEST_CASE("there are 25 slot fields with unique keys", "[fields]")
{
    STATIC_CHECK(kNumSlotFields == 25);
    std::set<std::string> keys;
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        const auto& s = fieldSpec(f);
        REQUIRE(std::string(s.key).size() > 0);
        keys.insert(s.key);
        REQUIRE(slotFieldFromKey(s.key) == f);
        REQUIRE(s.min <= s.def);
        REQUIRE(s.def <= s.max);
        if (s.kind == FieldKind::Choice)
            REQUIRE(static_cast<int>(s.choices.size()) == static_cast<int>(s.max) + 1);
    }
    CHECK(keys.size() == 25);
    CHECK_FALSE(slotFieldFromKey("doesNotExist").has_value());
}

TEST_CASE("default SlotParams match the field table", "[fields]")
{
    CHECK(makeDefaultSlotParams() == SlotParams{});
}

TEST_CASE("get and set round-trip every field at min, max and default", "[fields]")
{
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        const auto& s = fieldSpec(f);
        for (float v : { s.min, s.max, s.def })
        {
            SlotParams p;
            setSlotField(p, f, v);
            CHECK_THAT(getSlotField(p, f), WithinAbs(v, 1e-5));
        }
    }
}

TEST_CASE("setSlotField clamps, rounds and rejects NaN", "[fields]")
{
    SlotParams p;
    setSlotField(p, SlotField::Pitch, 1.0e6f);
    CHECK(p.pitchHz == fieldSpec(SlotField::Pitch).max);
    setSlotField(p, SlotField::Pitch, -5.0f);
    CHECK(p.pitchHz == fieldSpec(SlotField::Pitch).min);
    setSlotField(p, SlotField::Wave, 2.6f);
    CHECK(p.wave == Waveform::Square);
    setSlotField(p, SlotField::Choke, 99.0f);
    CHECK(p.chokeGroup == 4);
    setSlotField(p, SlotField::LfoSync, 0.7f);
    CHECK(p.lfoSync);
    setSlotField(p, SlotField::Volume, std::numeric_limits<float>::quiet_NaN());
    CHECK(p.volumeDb == fieldSpec(SlotField::Volume).def);
}

TEST_CASE("sync division beats", "[fields]")
{
    CHECK_THAT(syncDivisionBeats(SyncDivision::D1_32), WithinAbs(0.125, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::D1_16T), WithinAbs(1.0 / 6.0, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::D1_4), WithinAbs(1.0, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::Bar1), WithinAbs(4.0, 1e-6));
    CHECK_THAT(syncDivisionBeats(SyncDivision::Bars4), WithinAbs(16.0, 1e-6));
}

TEST_CASE("tune spans two octaves and is newer than the siren fields", "[fields]")
{
    const auto& t = fieldSpec(SlotField::Tune);
    CHECK(std::string(t.key) == "tune");
    CHECK(t.min == -24.0f);
    CHECK(t.max == 24.0f);
    CHECK(t.def == 0.0f);
    CHECK(std::string(t.unit) == "st");
    CHECK(t.versionHint == 3);
    for (int i = 0; i < static_cast<int>(SlotField::Tune); ++i) // alle Felder vor Tune sind die ursprünglichen
        if (static_cast<SlotField>(i) != SlotField::Tune)
            CHECK(fieldSpec(static_cast<SlotField>(i)).versionHint == 1);

    SlotParams p;
    setSlotField(p, SlotField::Tune, 7.0f);
    CHECK(p.tuneSemis == 7.0f);
}

TEST_CASE("the sample region fields default to the whole file without loop", "[fields]")
{
    const SlotParams p = makeDefaultSlotParams();
    CHECK(p.sampleStart == 0.0f);
    CHECK(p.loopStart == 0.0f);
    CHECK(p.sampleEnd == 1.0f);
    CHECK_FALSE(p.loop);
    CHECK_FALSE(p.reverse);
    CHECK(p.loopXfadePct == 5.0f);
    CHECK(SlotParams {} == p); // die Standardwerte der Struktur stimmen mit der Tabelle überein

    for (const auto f : { SlotField::SampleStart, SlotField::LoopStart, SlotField::SampleEnd, SlotField::Loop, SlotField::Reverse,
                          SlotField::LoopXfade })
        CHECK(fieldSpec(f).versionHint == 4);
    CHECK(std::string(fieldSpec(SlotField::SampleStart).key) == "smpStart");
    CHECK(std::string(fieldSpec(SlotField::LoopStart).key) == "loopStart");
    CHECK(std::string(fieldSpec(SlotField::SampleEnd).key) == "smpEnd");
    CHECK(std::string(fieldSpec(SlotField::Loop).key) == "loop");
    CHECK(std::string(fieldSpec(SlotField::Reverse).key) == "reverse");
    CHECK(std::string(fieldSpec(SlotField::LoopXfade).key) == "xfade");
    CHECK(fieldSpec(SlotField::LoopXfade).max == 50.0f);
    CHECK(fieldSpec(SlotField::Tune).versionHint == 3); // bestehende Hinweise bleiben
}

TEST_CASE("resetSampleRegionFields restores only the six region fields", "[fields]")
{
    SlotParams p = makeDefaultSlotParams();
    p.sampleStart = 0.3f;
    p.loopStart = 0.4f;
    p.sampleEnd = 0.6f;
    p.loop = true;
    p.reverse = true;
    p.loopXfadePct = 20.0f;
    p.tuneSemis = 7.0f;
    p.volumeDb = -12.0f;
    resetSampleRegionFields(p);
    SlotParams expected = makeDefaultSlotParams();
    expected.tuneSemis = 7.0f;
    expected.volumeDb = -12.0f;
    CHECK(p == expected);
}
