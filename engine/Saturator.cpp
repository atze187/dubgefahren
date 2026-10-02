#include "engine/Saturator.h"
#include <algorithm>
#include <cmath>

namespace dg {

namespace {
// Klangkonstanten. Hier nachjustieren, nicht im Code verteilt.
constexpr double kGainExponent = 1.4;   // g = 10^(1,4 d): bis ca. +28 dB
constexpr double kHardStart = 0.3;      // ab hier wird die Kennlinie zunehmend hart
constexpr double kBias = 0.1;           // Asymmetrie für gerade Obertöne
constexpr double kBeta = 0.32;          // Pegelausgleich g^(-beta): teilweise ausgeglichen
constexpr double kDcBlockHz = 10.0;
constexpr double kAdaaEps = 1.0e-6;
constexpr double kLn2 = 0.6931471805599453;
constexpr double kTwoPi = 6.283185307179586;

double smoothstep(double a, double b, double x)
{
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Stammfunktion von tanh, numerisch stabil für große |u|.
double logCosh(double u)
{
    const double a = std::abs(u);
    return a + std::log1p(std::exp(-2.0 * a)) - kLn2;
}

// Stammfunktion von clamp(u, -1, 1).
double hardAntiderivative(double u)
{
    const double a = std::abs(u);
    return a <= 1.0 ? 0.5 * u * u : a - 0.5;
}
} // namespace

void Saturator::prepare(double sampleRate)
{
    dcR_ = 1.0 - kTwoPi * kDcBlockHz / sampleRate;
    reset();
}

void Saturator::reset()
{
    uPrev_ = fSoftPrev_ = fHardPrev_ = 0.0;
    havePrev_ = false;
    dcX_ = dcY_ = 0.0;
}

float Saturator::process(float x, float drive)
{
    if (drive <= 0.0f)
    {
        reset(); // der erste Sample nach dem Aufdrehen beginnt ohne Altlasten
        return x;
    }

    const double d = std::min(static_cast<double>(drive), 1.0);
    const double g = std::pow(10.0, kGainExponent * d);
    const double m = smoothstep(kHardStart, 1.0, d);
    const double u = g * static_cast<double>(x) + kBias;

    const double fSoft = logCosh(u);
    const double fHard = hardAntiderivative(u);
    const double du = u - uPrev_;
    double ySoft, yHard;
    if (havePrev_ && std::abs(du) >= kAdaaEps)
    {
        ySoft = (fSoft - fSoftPrev_) / du;
        yHard = (fHard - fHardPrev_) / du;
    }
    else
    {
        const double um = havePrev_ ? 0.5 * (u + uPrev_) : u;
        ySoft = std::tanh(um);
        yHard = std::clamp(um, -1.0, 1.0);
    }
    uPrev_ = u;
    fSoftPrev_ = fSoft;
    fHardPrev_ = fHard;
    havePrev_ = true;

    // Offsets entfernen den Ruhe-Arbeitspunkt: bei Stille kommt 0 heraus.
    double y = (1.0 - m) * (ySoft - std::tanh(kBias)) + m * (yHard - std::clamp(kBias, -1.0, 1.0));
    y *= std::pow(g, -kBeta);

    const double blocked = y - dcX_ + dcR_ * dcY_;
    dcX_ = y;
    dcY_ = blocked;

    const double mix = std::min(1.0, 10.0 * d);
    return static_cast<float>(x + mix * (blocked - x));
}

} // namespace dg
