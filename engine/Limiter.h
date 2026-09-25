#pragma once
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

constexpr float kLimiterCeiling = 0.966051f; // -0,3 dBFS

// Peak-Limiter ohne Lookahead: sofortiger Attack, 100 ms Release, harte Sicherung am Ceiling.
class Limiter
{
public:
    void prepare(double sampleRate)
    {
        releaseCoeff_ = onePoleCoeff(0.1f, sampleRate);
        reset();
    }

    void reset() { gain_ = 1.0f; }

    void process(float& l, float& r)
    {
        const float peak = std::max(std::abs(l), std::abs(r));
        const float desired = peak > kLimiterCeiling ? kLimiterCeiling / peak : 1.0f;
        if (desired < gain_)
            gain_ = desired;
        else
            gain_ += (desired - gain_) * releaseCoeff_;
        if (gain_ < 1.0f)
        {
            l *= gain_;
            r *= gain_;
        }
        l = std::clamp(l, -kLimiterCeiling, kLimiterCeiling);
        r = std::clamp(r, -kLimiterCeiling, kLimiterCeiling);
    }

private:
    float gain_ = 1.0f;
    float releaseCoeff_ = 0.001f;
};

} // namespace dg
