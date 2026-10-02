#include "engine/Phaser.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

namespace {
// Klangkonstanten. Hier nachjustieren, nicht im Code verteilt.
constexpr float kCenterHz = 700.0f;       // Mitte des Sweeps
constexpr float kWidthMinOct = 0.5f;      // Sweep-Breite bei depth 0 (± Oktaven)
constexpr float kWidthDepthOct = 2.0f;    // zusätzliche Breite bei depth 1
constexpr float kFeedbackMin = 0.15f;
constexpr float kFeedbackDepth = 0.35f;
constexpr double kRightPhase = 0.25;      // 90° Versatz des rechten Kanals (in Perioden)
constexpr float kMinRateHz = 0.05f;
constexpr float kMaxRateHz = 3.0f;
constexpr float kMixOff = 1.0e-6f;
constexpr float kMinHz = 20.0f;
constexpr float kMaxNyquistFraction = 0.45f;
} // namespace

void Phaser::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void Phaser::reset()
{
    ch_.fill(Channel {});
    phase_ = 0.0;
}

void Phaser::setParams(float rateHz, float depth, float mix)
{
    rateHz_ = std::clamp(rateHz, kMinRateHz, kMaxRateHz);
    depth_ = std::clamp(depth, 0.0f, 1.0f);
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

float Phaser::run(Channel& c, float x, float lfo) const
{
    const float sr = static_cast<float>(sampleRate_);
    const float fc = std::clamp(kCenterHz * std::exp2((kWidthMinOct + kWidthDepthOct * depth_) * lfo), kMinHz,
                                kMaxNyquistFraction * sr);
    const float t = std::tan(kPi * fc / sr);
    const float a = (t - 1.0f) / (t + 1.0f);
    const float fb = kFeedbackMin + kFeedbackDepth * depth_;

    float v = x + fb * std::tanh(c.fb);
    for (auto& z : c.z)
    {
        const float y = a * v + z; // Allpass 1. Ordnung (transponierte Direktform II)
        z = v - a * y;
        v = y;
    }
    c.fb = v;
    return v;
}

void Phaser::process(float& l, float& r)
{
    const float lfoL = std::sin(kTwoPi * static_cast<float>(phase_));
    const float lfoR = std::sin(kTwoPi * static_cast<float>(phase_ + kRightPhase));
    phase_ += static_cast<double>(rateHz_) / sampleRate_;
    if (phase_ >= 1.0)
        phase_ -= 1.0;

    const float apL = run(ch_[0], l, lfoL);
    const float apR = run(ch_[1], r, lfoR);
    if (mix_ < kMixOff)
        return; // bit-genau durchgereicht
    const float dry = 1.0f - 0.5f * mix_;
    const float wet = 0.5f * mix_;
    l = dry * l + wet * apL;
    r = dry * r + wet * apR;
}

} // namespace dg
