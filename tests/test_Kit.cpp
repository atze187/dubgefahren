#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>
#include "engine/Kit.h"
#include "engine/SlotFields.h"

using namespace dg;

TEST_CASE("factory kit has 16 unique non-empty names", "[kit]")
{
    const Kit k = makeFactoryKit();
    std::set<std::string> names;
    for (const auto& n : k.names)
    {
        CHECK_FALSE(n.empty());
        names.insert(n);
    }
    CHECK(names.size() == 16);
    CHECK(k.names[0] == "Classic");
    CHECK(k.names[15] == "Drop");
}

TEST_CASE("factory kit values are inside the parameter ranges", "[kit]")
{
    const Kit k = makeFactoryKit();
    for (const auto& p : k.slots)
    {
        for (int i = 0; i < kNumSlotFields; ++i)
        {
            const auto f = static_cast<SlotField>(i);
            SlotParams copy = p;
            setSlotField(copy, f, getSlotField(p, f));
            REQUIRE(copy == p);
        }
    }
}

TEST_CASE("factory kit trigger modes and choke groups follow the spec", "[kit]")
{
    const Kit k = makeFactoryKit();
    const std::set<int> oneShots { 3, 4, 5, 8, 9, 15 };
    const std::set<int> latches { 1, 6, 7, 12 };
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto& p = k.slots[static_cast<std::size_t>(s)];
        if (oneShots.count(s))
        {
            CHECK(p.trigMode == TriggerMode::OneShot);
            CHECK(p.chokeGroup == 1);
        }
        else
        {
            CHECK(p.chokeGroup == 0);
            CHECK(p.trigMode == (latches.count(s) ? TriggerMode::Latch : TriggerMode::Gate));
        }
    }
    CHECK(k.slots[3].sweepSemis > 0.0f);  // Laser fällt von oben
    CHECK(k.slots[4].sweepSemis < 0.0f);  // Riser steigt
    CHECK(k.slots[5].sweepSemis > 0.0f);  // Faller fällt
}
