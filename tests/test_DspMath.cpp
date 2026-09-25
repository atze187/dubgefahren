#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/DspMath.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("semitonesToRatio", "[math]")
{
    CHECK_THAT(dg::semitonesToRatio(12.0f), WithinAbs(2.0, 1e-5));
    CHECK_THAT(dg::semitonesToRatio(-12.0f), WithinAbs(0.5, 1e-5));
    CHECK_THAT(dg::semitonesToRatio(0.0f), WithinAbs(1.0, 1e-6));
}

TEST_CASE("dbToGain and volumeDbToGain", "[math]")
{
    CHECK_THAT(dg::dbToGain(0.0f), WithinAbs(1.0, 1e-6));
    CHECK_THAT(dg::dbToGain(-6.0f), WithinAbs(0.501187, 1e-4));
    CHECK_THAT(dg::dbToGain(6.0f), WithinAbs(1.995262, 1e-4));
    CHECK(dg::volumeDbToGain(dg::kMinVolumeDb) == 0.0f);
    CHECK(dg::volumeDbToGain(-100.0f) == 0.0f);
    CHECK_THAT(dg::volumeDbToGain(-59.0f), WithinAbs(dg::dbToGain(-59.0f), 1e-9));
}

TEST_CASE("onePoleCoeff", "[math]")
{
    CHECK(dg::onePoleCoeff(0.0f, 48000.0) == 1.0f);
    const float a = dg::onePoleCoeff(0.02f, 48000.0);
    CHECK(a > 0.0f);
    CHECK(a < 0.01f);
}
