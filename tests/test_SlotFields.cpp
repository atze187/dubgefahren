#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <set>
#include <string>
#include "engine/SlotFields.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

TEST_CASE("there are 18 slot fields with unique keys", "[fields]")
{
    STATIC_CHECK(kNumSlotFields == 18);
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
    CHECK(keys.size() == 18);
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
