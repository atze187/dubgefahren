#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <set>
#include <string>
#include "engine/SlotFields.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

TEST_CASE("there are 28 slot fields with unique keys", "[fields]")
{
    STATIC_CHECK(kNumSlotFields == 28);
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
    CHECK(keys.size() == 28);
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

TEST_CASE("the stretch fields default to off, 100 percent and 20 ms", "[fields][stretch]")
{
    const SlotParams p = makeDefaultSlotParams();
    CHECK(p.stretchMode == StretchMode::Off);
    CHECK(p.stretchPct == 100.0f);
    CHECK(p.grainMs == 20.0f);
    CHECK(SlotParams {} == p);

    for (const auto f : { SlotField::StretchMode, SlotField::StretchFactor, SlotField::StretchGrain })
        CHECK(fieldSpec(f).versionHint == 5);
    CHECK(std::string(fieldSpec(SlotField::StretchMode).key) == "stretch");
    CHECK(std::string(fieldSpec(SlotField::StretchFactor).key) == "stretchPct");
    CHECK(std::string(fieldSpec(SlotField::StretchGrain).key) == "grainMs");
    CHECK(fieldSpec(SlotField::StretchMode).kind == FieldKind::Choice);
    CHECK(fieldSpec(SlotField::StretchMode).choices.size() == 3);
    CHECK(fieldSpec(SlotField::StretchFactor).min == 50.0f);
    CHECK(fieldSpec(SlotField::StretchFactor).max == 800.0f);
    CHECK(fieldSpec(SlotField::StretchFactor).skewCentre == 100.0f); // Knob-Mitte = Originallänge
    CHECK(fieldSpec(SlotField::StretchGrain).min == 5.0f);
    CHECK(fieldSpec(SlotField::StretchGrain).max == 100.0f);
    CHECK(fieldSpec(SlotField::LoopXfade).versionHint == 4); // bestehende Hinweise bleiben
}

TEST_CASE("stretch fields clamp and convert", "[fields][stretch]")
{
    SlotParams p = makeDefaultSlotParams();
    setSlotField(p, SlotField::StretchMode, 2.0f);
    CHECK(p.stretchMode == StretchMode::Loop);
    setSlotField(p, SlotField::StretchMode, 9.0f);
    CHECK(p.stretchMode == StretchMode::Loop); // auf das Maximum begrenzt
    setSlotField(p, SlotField::StretchFactor, 650.0f);
    CHECK(p.stretchPct == 650.0f); // über dem alten Maximum von 200 % gültig
    setSlotField(p, SlotField::StretchFactor, 1000.0f);
    CHECK(p.stretchPct == 800.0f);
    setSlotField(p, SlotField::StretchGrain, 0.0f);
    CHECK(p.grainMs == 5.0f);
    CHECK(getSlotField(p, SlotField::StretchMode) == 2.0f);
}

TEST_CASE("resetSampleFields restores the region and the stretch fields only", "[fields]")
{
    SlotParams p = makeDefaultSlotParams();
    p.sampleStart = 0.3f;
    p.loopStart = 0.4f;
    p.sampleEnd = 0.6f;
    p.loop = true;
    p.reverse = true;
    p.loopXfadePct = 20.0f;
    p.stretchMode = StretchMode::All;
    p.stretchPct = 150.0f;
    p.grainMs = 60.0f;
    p.tuneSemis = 7.0f;
    p.volumeDb = -12.0f;
    resetSampleFields(p);
    SlotParams expected = makeDefaultSlotParams();
    expected.tuneSemis = 7.0f;
    expected.volumeDb = -12.0f;
    CHECK(p == expected);
}
