#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/Saturator.h"
#include "TestHelpers.h"

using namespace dg;

namespace {
constexpr double kSr = 48000.0;

std::vector<float> saturate(const std::vector<float>& x, float drive, double sr = kSr)
{
    Saturator s;
    s.prepare(sr);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        y[i] = s.process(x[i], drive);
    return y;
}

// Amplitude der k-ten Harmonischen von 1 kHz über die zweiten 0,5 s (ganze Perioden, daher exakt).
double harmonicAmp(const std::vector<float>& y, int k)
{
    constexpr std::size_t kFrom = 24000;
    double re = 0.0, im = 0.0;
    for (std::size_t i = kFrom; i < y.size(); ++i)
    {
        const double ph = 6.283185307179586 * 1000.0 * k * static_cast<double>(i) / kSr;
        re += y[i] * std::cos(ph);
        im -= y[i] * std::sin(ph);
    }
    return 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(y.size() - kFrom);
}

// Leistung bei Frequenz f über 1 s (ganze Perioden bei ganzzahligem f).
double powerAt(const std::vector<float>& y, double f)
{
    double re = 0.0, im = 0.0;
    for (std::size_t i = 0; i < y.size(); ++i)
    {
        const double ph = 6.283185307179586 * f * static_cast<double>(i) / kSr;
        re += y[i] * std::cos(ph);
        im -= y[i] * std::sin(ph);
    }
    return re * re + im * im;
}

constexpr double kTestF0 = 4100.0;

// Alias-Energie relativ zur Energie der echten Harmonischen von kTestF0 in dB.
// Echte Harmonische: m * f0 unterhalb von 24 kHz. Alias-Produkte: ungerade Harmonische k * f0 (k >= 7)
// oberhalb von 24 kHz, an der Nyquistfrequenz zurück in 0..24 kHz gefaltet. Bins, die auf eine echte
// Harmonische fallen (innerhalb 2 Hz) oder doppelt vorkommen, zählen nicht.
double aliasDb(const std::vector<float>& y)
{
    constexpr double kNyquist = kSr / 2.0;
    std::vector<double> wantedBins;
    double wanted = 0.0;
    for (int m = 1; m * kTestF0 < kNyquist; ++m)
    {
        wantedBins.push_back(m * kTestF0);
        wanted += powerAt(y, m * kTestF0);
    }

    std::vector<double> aliasBins;
    for (int k = 7; k <= 41; k += 2)
    {
        double f = std::fmod(k * kTestF0, kSr);
        if (f > kNyquist)
            f = kSr - f;
        const auto close = [f](double b) { return std::abs(b - f) <= 2.0; };
        if (std::none_of(wantedBins.begin(), wantedBins.end(), close)
            && std::none_of(aliasBins.begin(), aliasBins.end(), close))
            aliasBins.push_back(f);
    }

    double alias = 0.0;
    for (const double f : aliasBins)
        alias += powerAt(y, f);
    return 10.0 * std::log10(alias / wanted);
}

// Dieselbe Mischkennlinie wie Saturator bei drive 1 (rein hart), aber ohne ADAA.
std::vector<float> naiveHard(const std::vector<float>& x)
{
    const double g = std::pow(10.0, 1.4);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        y[i] = static_cast<float>(std::clamp(g * x[i] + 0.1, -1.0, 1.0));
    return y;
}
} // namespace

TEST_CASE("drive 0 is bit-exact transparent", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    for (float x : { -1.5f, -0.3f, 0.0f, 0.2f, 0.9f, 3.0f })
        CHECK(s.process(x, 0.0f) == x);
}

TEST_CASE("a vanishing drive is bit-exact transparent", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    for (float x : { -1.5f, -0.3f, 0.0f, 0.2f, 0.9f, 3.0f })
    {
        CHECK(s.process(x, 1.0e-7f) == x);
        CHECK(s.process(x, 1.0e-35f) == x);
    }
}

TEST_CASE("drive 1 stays finite and bounded for huge inputs", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 2000; ++i)
    {
        const float x = (i % 2 == 0 ? 1.0e6f : -1.0e6f) * (i % 7 == 0 ? 0.01f : 1.0f);
        const float y = s.process(x, 1.0f);
        finite = finite && std::isfinite(y);
        peak = std::max(peak, std::abs(y));
    }
    CHECK(finite);
    CHECK(peak < 2.0f);
}

TEST_CASE("louder input gives louder output", "[saturator]")
{
    Saturator a, b;
    a.prepare(kSr);
    b.prepare(kSr);
    CHECK(a.process(0.5f, 0.7f) > b.process(0.2f, 0.7f));
}

TEST_CASE("asymmetric saturation leaves no DC", "[saturator]")
{
    const auto y = saturate(dgtest::sine(200.0f, kSr, 48000, 0.5f), 1.0f);
    double sum = 0.0;
    for (std::size_t i = 24000; i < y.size(); ++i)
        sum += y[i];
    CHECK(std::abs(sum / 24000.0) < 0.01);
}

TEST_CASE("silence and constant input never produce NaN and settle at zero", "[saturator]")
{
    const std::vector<float> silence(48000, 0.0f);
    const auto ys = saturate(silence, 0.8f);
    CHECK(dgtest::allFinite(ys));
    CHECK(dgtest::peakAbs(ys) < 1.0e-6f);

    const std::vector<float> dc(48000, 0.3f);
    const auto yd = saturate(dc, 0.8f);
    CHECK(dgtest::allFinite(yd));
    CHECK(dgtest::peakAbs(yd, 40000) < 0.01f); // der DC-Blocker nimmt den Gleichanteil weg
}

TEST_CASE("a drive sweep does not click", "[saturator]")
{
    const auto x = dgtest::sine(500.0f, kSr, 48000, 0.3f);
    Saturator sweep;
    sweep.prepare(kSr);
    std::vector<float> ys(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        ys[i] = sweep.process(x[i], static_cast<float>(i) / static_cast<float>(x.size()));
    const auto fixed = saturate(x, 1.0f);
    CHECK(dgtest::allFinite(ys));
    CHECK(dgtest::maxStep(ys) <= 1.2f * dgtest::maxStep(fixed) + 0.02f);
}

TEST_CASE("no spike right after reset or when drive comes up from zero", "[saturator]")
{
    Saturator s;
    s.prepare(kSr);
    for (int i = 0; i < 1000; ++i)
        s.process(0.5f * std::sin(0.05f * static_cast<float>(i)), 1.0f);
    s.reset();
    CHECK(std::abs(s.process(0.0f, 1.0f)) < 0.05f);

    Saturator t;
    t.prepare(kSr);
    for (int i = 0; i < 1000; ++i)
        t.process(0.2f, 0.0f);
    CHECK(std::abs(t.process(0.2f, 0.5f)) < 1.5f);
}

TEST_CASE("stable at 44.1 and 96 kHz", "[saturator]")
{
    for (const double sr : { 44100.0, 96000.0 })
    {
        const auto y = saturate(dgtest::sine(1000.0f, sr, static_cast<int>(sr), 0.5f), 1.0f, sr);
        CHECK(dgtest::allFinite(y));
        CHECK(dgtest::peakAbs(y) < 2.0f);
    }
}

TEST_CASE("full drive is about 6 dB louder than no drive for a -12 dBFS sine", "[saturator]")
{
    const auto x = dgtest::sine(1000.0f, kSr, 48000, 0.251f);
    const float off = dgtest::rms(saturate(x, 0.0f), 24000);
    const float on = dgtest::rms(saturate(x, 1.0f), 24000);
    const double gainDb = 20.0 * std::log10(static_cast<double>(on) / static_cast<double>(off));
    CHECK(gainDb > 4.5);
    CHECK(gainDb < 7.5);
}

TEST_CASE("low drive adds even harmonics, high drive gets harder", "[saturator]")
{
    const auto x = dgtest::sine(1000.0f, kSr, 48000, 0.5f);
    const auto soft = saturate(x, 0.3f);
    const auto hard = saturate(x, 1.0f);
    const double soft1 = harmonicAmp(soft, 1), hard1 = harmonicAmp(hard, 1);
    CHECK(harmonicAmp(soft, 2) / soft1 > 0.01);                          // gerade Obertöne (Bias)
    CHECK(harmonicAmp(hard, 3) / hard1 > harmonicAmp(soft, 3) / soft1);  // härter: stärkere 3. Harmonische
}

TEST_CASE("ADAA cuts alias energy by at least 6 dB compared with naive clipping", "[saturator]")
{
    const auto x = dgtest::sine(static_cast<float>(kTestF0), kSr, 48000, 0.5f);
    const double naive = aliasDb(naiveHard(x));
    const double adaa = aliasDb(saturate(x, 1.0f));
    CAPTURE(naive, adaa);
    CHECK(adaa < naive - 6.0);
}
