#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include "engine/Envelope.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;
float run(Envelope& e, int n)
{
    float v = 0.0f;
    for (int i = 0; i < n; ++i)
        v = e.process();
    return v;
}
} // namespace

TEST_CASE("attack reaches full level after attack time and sustains", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    CHECK_FALSE(e.isActive());
    e.noteOn(0.01f);
    CHECK(e.isActive());
    CHECK(run(e, 240) < 0.6f);
    CHECK_THAT(run(e, 250), WithinAbs(1.0, 1e-6));
    CHECK(e.stage() == Envelope::Stage::Sustain);
    CHECK_THAT(run(e, 10000), WithinAbs(1.0, 1e-6));
}

TEST_CASE("release reaches zero after release time and becomes idle", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    run(e, 100);
    e.noteOff(0.1f);
    CHECK(e.isReleasing());
    CHECK(run(e, 4700) > 0.0f);
    run(e, 110);
    CHECK(e.level() == 0.0f);
    CHECK_FALSE(e.isActive());
}

TEST_CASE("kill fades out within 5 ms without big steps", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    run(e, 100);
    e.kill();
    float prev = e.level();
    float maxStep = 0.0f;
    for (int i = 0; i < 241; ++i)
    {
        const float v = e.process();
        maxStep = std::max(maxStep, prev - v);
        prev = v;
    }
    CHECK_FALSE(e.isActive());
    CHECK(maxStep <= 1.0f / 240.0f + 1e-5f);
}

TEST_CASE("retrigger during release continues from current level", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    run(e, 100);
    e.noteOff(1.0f);
    const float before = run(e, 24000); // ~0.5
    e.noteOn(1.0f);
    const float after = e.process();
    CHECK(after >= before);
    CHECK(after - before < 0.001f);
    CHECK(e.stage() == Envelope::Stage::Attack);
}

TEST_CASE("zero attack uses the 1 ms minimum", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOn(0.0f);
    CHECK(e.process() < 0.1f);
    CHECK_THAT(run(e, 48), WithinAbs(1.0, 1e-6));
}

TEST_CASE("noteOff and kill while idle do nothing", "[env]")
{
    Envelope e;
    e.prepare(kSr);
    e.noteOff(0.1f);
    e.kill();
    CHECK_FALSE(e.isActive());
}
