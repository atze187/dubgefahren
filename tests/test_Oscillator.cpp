#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <numeric>
#include <vector>
#include "engine/Oscillator.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
std::vector<float> render(Waveform w, float freq, float pw, int n, double sr = 48000.0)
{
    Oscillator osc;
    osc.prepare(sr);
    std::vector<float> out(static_cast<std::size_t>(n));
    for (auto& v : out)
        v = osc.process(w, freq, pw);
    return out;
}
} // namespace

TEST_CASE("oscillator frequency matches for all waveforms", "[osc]")
{
    for (auto w : { Waveform::Sine, Waveform::Triangle, Waveform::Saw, Waveform::Square })
    {
        const auto x = render(w, 440.0f, 0.5f, 48000);
        CHECK_THAT(dgtest::estimateFrequency(x, 48000.0, 0, x.size()), WithinAbs(440.0, 3.0));
    }
}

TEST_CASE("oscillator output is bounded", "[osc]")
{
    for (auto w : { Waveform::Sine, Waveform::Triangle, Waveform::Saw, Waveform::Square })
    {
        for (float f : { 20.0f, 1000.0f, 18000.0f, 40000.0f })
        {
            const auto x = render(w, f, 0.3f, 4800);
            CHECK(dgtest::allFinite(x));
            CHECK(dgtest::peakAbs(x) <= 1.3f);
        }
    }
}

TEST_CASE("square pulse width sets the duty cycle", "[osc]")
{
    const auto x = render(Waveform::Square, 100.0f, 0.25f, 48000);
    const double mean = std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
    CHECK_THAT(mean, WithinAbs(-0.5, 0.02));
}

TEST_CASE("resetPhase restarts the waveform", "[osc]")
{
    Oscillator osc;
    osc.prepare(48000.0);
    for (int i = 0; i < 123; ++i)
        osc.process(Waveform::Sine, 440.0f, 0.5f);
    osc.resetPhase();
    CHECK_THAT(osc.process(Waveform::Sine, 440.0f, 0.5f), WithinAbs(0.0, 1e-6));
}
