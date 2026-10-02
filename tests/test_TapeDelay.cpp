#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <vector>
#include "engine/FxParams.h"
#include "engine/TapeDelay.h"
#include "TestHelpers.h"

using namespace dg;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kSr = 48000.0;

std::vector<float> impulseResponse(TapeDelay& d, int n)
{
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, l, r);
        out[static_cast<std::size_t>(i)] = l;
    }
    return out;
}

// RMS-Verhältnis zweite zu erster Wiederholung eines 50-ms-Sinusburst bei Feedback 0.5.
float echoRatio(float freq)
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.5f, 1.0f, 0.0f);
    const auto burst = dgtest::sine(freq, kSr, 2400, 0.5f);
    std::vector<float> y(48000);
    for (int i = 0; i < 48000; ++i)
    {
        const float in = i < 2400 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        y[static_cast<std::size_t>(i)] = l;
    }
    return dgtest::rms(y, 24200, 26200) / dgtest::rms(y, 12200, 14200);
}

// Mittlere Frequenz über interpolierte steigende Nulldurchgänge.
double meanFrequency(const std::vector<float>& y, std::size_t from, std::size_t to)
{
    double first = 0.0, last = 0.0;
    int n = 0;
    for (std::size_t i = from + 1; i < to; ++i)
    {
        if (y[i - 1] < 0.0f && y[i] >= 0.0f)
        {
            const double t = static_cast<double>(i - 1) + static_cast<double>(-y[i - 1]) / static_cast<double>(y[i] - y[i - 1]);
            if (n == 0)
                first = t;
            last = t;
            ++n;
        }
    }
    return n < 2 ? 0.0 : (n - 1) * kSr / (last - first);
}

// Pitch-Streuung (max - min) und größte Abweichung von 1 kHz über acht 0,5-s-Fenster der ersten Wiederholung.
struct PitchStats { double spread; double maxDeviation; float maxLeftRightDiff; };

PitchStats measurePitch(float wow)
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.0f, 1.0f, wow);
    const auto x = dgtest::sine(1000.0f, kSr, 240000, 0.5f);
    std::vector<float> yl(x.size()), yr(x.size());
    float diff = 0.0f;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(x[i], x[i], l, r);
        yl[i] = l;
        yr[i] = r;
        if (i > 24000)
            diff = std::max(diff, std::abs(l - r));
    }
    double lo = 1.0e9, hi = -1.0e9, dev = 0.0;
    for (std::size_t w = 0; w < 8; ++w)
    {
        const double f = meanFrequency(yl, 24000 + w * 24000, 48000 + w * 24000);
        lo = std::min(lo, f);
        hi = std::max(hi, f);
        dev = std::max(dev, std::abs(f - 1000.0));
    }
    return { hi - lo, dev, diff };
}

// Mittlere Leistungsdichte weit vom 1-kHz-Träger (3 bis 20 kHz) relativ zur Gesamtleistung in dB,
// erste Wiederholung. Misst, wie viel breitbandiges Modulationsrauschen auf der Bandgeschwindigkeit liegt.
double farSidebandDb(float wow)
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.0f, 1.0f, wow);
    const auto x = dgtest::sine(1000.0f, kSr, 96000, 0.5f);
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(x[i], x[i], l, r);
        y[i] = l;
    }
    constexpr int kN = 16384;
    constexpr std::size_t kStart = 30000;
    constexpr double kPi2 = 6.283185307179586;
    std::vector<double> v(kN);
    double total = 0.0;
    for (int n = 0; n < kN; ++n)
    {
        v[static_cast<std::size_t>(n)] = (0.5 - 0.5 * std::cos(kPi2 * n / kN)) * y[kStart + static_cast<std::size_t>(n)];
        total += v[static_cast<std::size_t>(n)] * v[static_cast<std::size_t>(n)];
    }
    double sum = 0.0;
    int bins = 0;
    for (double f = 3000.0; f <= 20000.0; f += 100.0, ++bins)
    {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < kN; ++n)
        {
            const double ph = kPi2 * f * n / kSr;
            re += v[static_cast<std::size_t>(n)] * std::cos(ph);
            im -= v[static_cast<std::size_t>(n)] * std::sin(ph);
        }
        sum += re * re + im * im;
    }
    return 10.0 * std::log10((sum / bins) / (kN * total));
}
} // namespace

TEST_CASE("delay division to seconds", "[delay]")
{
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_4, 120.0), WithinAbs(0.5, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_8D, 120.0), WithinAbs(0.375, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_4T, 120.0), WithinAbs(1.0 / 3.0, 1e-9));
    CHECK_THAT(delayDivisionSeconds(DelayDivision::D1_1, 20.0), WithinAbs(8.0, 1e-9)); // bpm auf 30 geklemmt
}

TEST_CASE("single echo at the delay time without feedback", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.0f, 1.0f, 0.0f);
    const auto y = impulseResponse(d, 48000);
    const auto peakIt = std::max_element(y.begin(), y.end());
    const auto peakIndex = std::distance(y.begin(), peakIt);
    CHECK(peakIndex >= 12000);
    CHECK(peakIndex <= 12002);
    CHECK(*peakIt > 0.4f);
    CHECK(dgtest::peakAbs(y, 13000) < 0.01f);
    CHECK(dgtest::peakAbs(y, 0, 11990) == 0.0f);
}

TEST_CASE("feedback produces a quieter second echo", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.5f, 1.0f, 0.0f);
    const auto y = impulseResponse(d, 48000);
    const float first = dgtest::peakAbs(y, 11990, 12500);
    const float second = dgtest::peakAbs(y, 23990, 24500);
    CHECK(second / first > 0.2f);
    CHECK(second / first < 0.6f);
}

TEST_CASE("110 percent feedback stays finite and bounded", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
    const auto burst = dgtest::noise(24000, 0.5f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 30; ++i)
    {
        const float in = i < 24000 ? burst[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 4.0f);
}

TEST_CASE("channels are independent", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.01f, 0.3f, 0.5f, 0.0f);
    float maxRight = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(i == 0 ? 1.0f : 0.0f, 0.0f, l, r);
        maxRight = std::max(maxRight, std::abs(r));
    }
    CHECK(maxRight < 1.0e-3f); // nur Bandrauschen (-75 dBFS), kein Übersprechen vom linken Kanal
}

TEST_CASE("no buffer overflow with edge case delay", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.250000089f, 0.3f, 0.5f, 0.0f);
    const auto x = dgtest::noise(48000 * 11, 0.5f);
    bool finite = true;
    for (float v : x)
    {
        float l = 0.0f, r = 0.0f;
        d.process(v, v, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
    }
    CHECK(finite);
}

TEST_CASE("every repeat loses more highs than lows", "[delay]")
{
    const float low = echoRatio(200.0f);
    const float high = echoRatio(8000.0f);
    CHECK(low > 0.35f);
    CHECK(high < 0.8f * low);
}

TEST_CASE("asymmetric saturation leaves no DC in the loop", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.05f, 0.9f, 1.0f, 0.0f);
    const auto x = dgtest::sine(200.0f, kSr, 96000, 0.4f);
    std::vector<float> y(96000);
    for (int i = 0; i < 96000; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(x[static_cast<std::size_t>(i)], x[static_cast<std::size_t>(i)], l, r);
        y[static_cast<std::size_t>(i)] = l;
    }
    double sum = 0.0;
    for (std::size_t i = 48000; i < 96000; ++i)
        sum += y[i];
    CHECK(std::abs(sum / 48000.0) < 0.005);
}

TEST_CASE("loop stays bounded at 44.1 and 96 kHz", "[delay]")
{
    for (const double sr : { 44100.0, 96000.0 })
    {
        TapeDelay d;
        d.prepare(sr);
        d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
        const int burstLen = static_cast<int>(sr / 2);
        const auto burst = dgtest::noise(burstLen, 0.5f);
        float peak = 0.0f;
        bool finite = true;
        for (int i = 0; i < static_cast<int>(sr * 10); ++i)
        {
            const float in = i < burstLen ? burst[static_cast<std::size_t>(i)] : 0.0f;
            float l = 0.0f, r = 0.0f;
            d.process(in, in, l, r);
            finite = finite && std::isfinite(l) && std::isfinite(r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(finite);
        CHECK(peak < 4.0f);
    }
}

TEST_CASE("extreme input level stays finite", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.1f, 1.1f, 1.0f, 1.0f);
    const auto loud = dgtest::noise(48000, 100.0f);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000 * 10; ++i)
    {
        const float in = i < 48000 ? loud[static_cast<std::size_t>(i)] : 0.0f;
        float l = 0.0f, r = 0.0f;
        d.process(in, in, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(finite);
    CHECK(peak < 200.0f);
}

TEST_CASE("tape noise is inaudible at moderate feedback but audible when the loop rings", "[delay]")
{
    {
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.05f, 0.45f, 0.5f, 0.0f);
        float peak = 0.0f;
        for (int i = 0; i < 48000 * 3; ++i)
        {
            float l = 0.0f, r = 0.0f;
            d.process(0.0f, 0.0f, l, r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(peak > 1.0e-5f);  // das Rauschen ist da
        CHECK(peak < 1.0e-3f);  // aber unter -60 dBFS
    }
    {
        TapeDelay d;
        d.prepare(kSr);
        d.setParams(0.05f, 1.1f, 0.5f, 0.0f);
        float peak = 0.0f;
        bool finite = true;
        for (int i = 0; i < 48000 * 30; ++i)
        {
            float l = 0.0f, r = 0.0f;
            d.process(0.0f, 0.0f, l, r);
            finite = finite && std::isfinite(l) && std::isfinite(r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
        }
        CHECK(finite);
        CHECK(peak > 0.01f);    // Selbstoszillation aus dem Rauschen
        CHECK(peak < 4.0f);
    }
}

TEST_CASE("reset silences the loop including the noise", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.25f, 0.9f, 0.5f, 0.5f);
    const auto burst = dgtest::noise(48000, 0.5f);
    for (int i = 0; i < 48000; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(burst[static_cast<std::size_t>(i)], burst[static_cast<std::size_t>(i)], l, r);
    }
    d.reset();
    float peak = 0.0f;
    for (int i = 0; i < 11990; ++i)
    {
        float l = 0.0f, r = 0.0f;
        d.process(0.0f, 0.0f, l, r);
        peak = std::max({ peak, std::abs(l), std::abs(r) });
    }
    CHECK(peak == 0.0f);
}

TEST_CASE("without wow the pitch is constant and both channels agree up to the noise", "[delay]")
{
    const auto s = measurePitch(0.0f);
    CHECK(s.spread < 0.05);
    CHECK(s.maxDeviation < 0.05);
    CHECK(s.maxLeftRightDiff < 1.0e-3f);
}

TEST_CASE("with full wow the pitch drifts within limits and the channels differ slightly", "[delay]")
{
    const auto s = measurePitch(1.0f);
    CHECK(s.spread > 1.0);          // hörbare Tonhöhenschwankung
    CHECK(s.maxDeviation < 20.0);   // unter 2 %
    CHECK(s.maxLeftRightDiff > 1.0e-2f);
    CHECK(s.maxLeftRightDiff < 0.8f);  // weit von Gegenphase (1.0): der Versatz bleibt klein
}

TEST_CASE("shortest delay with full wow stays finite", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    d.setParams(0.001f, 0.9f, 1.0f, 1.0f);
    const auto x = dgtest::noise(48000 * 5, 0.5f);
    bool finite = true;
    for (float v : x)
    {
        float l = 0.0f, r = 0.0f;
        d.process(v, v, l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
    }
    CHECK(finite);
}

TEST_CASE("time jumps while the tape is wobbling stay finite", "[delay]")
{
    TapeDelay d;
    d.prepare(kSr);
    const auto x = dgtest::noise(48000 * 4, 0.5f);
    bool finite = true;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        if (i == 0)
            d.setParams(0.1f, 0.8f, 0.5f, 1.0f);
        else if (i == 48000)
            d.setParams(5.0f, 0.8f, 0.5f, 1.0f);
        else if (i == 48000 * 3)
            d.setParams(0.001f, 0.8f, 0.5f, 1.0f);
        float l = 0.0f, r = 0.0f;
        d.process(x[i], x[i], l, r);
        finite = finite && std::isfinite(l) && std::isfinite(r);
    }
    CHECK(finite);
}

TEST_CASE("wow does not put broadband noise on the tape speed", "[delay]")
{
    const double still = farSidebandDb(0.0f);
    const double wobbling = farSidebandDb(1.0f);
    CAPTURE(still, wobbling);
    CHECK(wobbling < -95.0);
}
