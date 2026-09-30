#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include "plugin/ui/PadGlow.h"

using namespace dg::ui;
using Catch::Approx;

namespace {
constexpr float kTick = 1.0f / 30.0f;
} // namespace

TEST_CASE("a trigger sets full brightness", "[padglow]")
{
    CHECK(advancePadGlow(0.0f, true, false, kTick) == 1.0f);
    CHECK(advancePadGlow(0.3f, true, false, kTick) == 1.0f); // erneuter Trigger im Ausklingen
    CHECK(advancePadGlow(0.0f, true, false, 0.0f) == 1.0f);
}

TEST_CASE("a held pad falls to the hold level and stays there", "[padglow]")
{
    float b = 1.0f;
    float previous = b;
    for (int i = 0; i < 30; ++i) // 1 s
    {
        b = advancePadGlow(b, true, true, kTick);
        CHECK(b <= previous);
        CHECK(b >= kPadGlowHold);
        previous = b;
    }
    CHECK(b == kPadGlowHold); // eingerastet, nicht nur nahe dran
    CHECK(advancePadGlow(b, true, true, kTick) == kPadGlowHold);
}

TEST_CASE("the hold level is practically reached after 250 ms", "[padglow]")
{
    const float b = advancePadGlow(1.0f, true, true, 0.25f);
    CHECK(b == Approx(kPadGlowHold).margin(0.02));
}

TEST_CASE("a released pad fades to zero within 200 ms", "[padglow]")
{
    float b = kPadGlowHold;
    b = advancePadGlow(b, false, true, 0.1f);
    CHECK(b == Approx(0.2f).margin(0.001));
    b = advancePadGlow(b, false, false, 0.1f);
    CHECK(b == 0.0f);
    CHECK(advancePadGlow(1.0f, false, true, 0.2f) == 0.0f);
    CHECK(advancePadGlow(0.0f, false, false, kTick) == 0.0f);
}

TEST_CASE("huge, zero and negative time steps keep the brightness valid", "[padglow]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(advancePadGlow(1.0f, true, true, 3600.0f) == kPadGlowHold);
    CHECK(advancePadGlow(1.0f, false, true, 3600.0f) == 0.0f);
    CHECK(advancePadGlow(0.9f, true, true, 0.0f) == 0.9f);
    CHECK(advancePadGlow(0.9f, true, true, -1.0f) == 0.9f);
    CHECK(advancePadGlow(0.5f, false, false, -1.0f) == 0.5f);
    CHECK(advancePadGlow(0.9f, true, true, nan) == 0.9f);
    CHECK(advancePadGlow(nan, true, true, kTick) == kPadGlowHold);
    CHECK(advancePadGlow(nan, false, false, kTick) == 0.0f);
    CHECK(advancePadGlow(7.0f, false, false, 0.0f) == 1.0f);  // Eingabe wird auf 0…1 begrenzt
    CHECK(advancePadGlow(-3.0f, false, false, 0.0f) == 0.0f);
}

TEST_CASE("a pad that becomes active below the hold level rises to it", "[padglow]")
{
    // Kann nur auftreten, wenn wasActive schon gesetzt war (kein Trigger): nie über den Haltewert hinaus.
    float b = 0.2f;
    for (int i = 0; i < 30; ++i)
    {
        b = advancePadGlow(b, true, true, kTick);
        CHECK(b <= kPadGlowHold);
    }
    CHECK(b == kPadGlowHold);
}
