#include "engine/SvFilter.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void SvFilter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
    setParams(20000.0f, 0.0f, 0.0f);
}

void SvFilter::reset()
{
    ic1_[0] = ic1_[1] = 0.0f;
    ic2_[0] = ic2_[1] = 0.0f;
}

void SvFilter::setParams(float cutoffHz, float resonance, float type)
{
    const float sr = static_cast<float>(sampleRate_);
    const float fc = std::clamp(cutoffHz, 20.0f, 0.49f * sr);
    g_ = std::tan(kPi * fc / sr);
    const float q = 0.5f * std::pow(40.0f, std::clamp(resonance, 0.0f, 1.0f));
    k_ = 1.0f / q;
    a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;

    const float t = std::clamp(type, 0.0f, 1.0f);
    if (t < 0.5f)
    {
        wLp_ = 1.0f - 2.0f * t;
        wBp_ = 2.0f * t;
        wHp_ = 0.0f;
    }
    else
    {
        wLp_ = 0.0f;
        wBp_ = 2.0f - 2.0f * t;
        wHp_ = 2.0f * t - 1.0f;
    }
}

float SvFilter::process(float x, int channel)
{
    float& ic1 = ic1_[channel];
    float& ic2 = ic2_[channel];
    const float v3 = x - ic2;
    const float v1 = a1_ * ic1 + a2_ * v3;
    const float v2 = ic2 + a2_ * ic1 + a3_ * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;

    const float lp = v2;
    const float bp = k_ * v1; // normiert: Spitzenverstärkung 1
    const float hp = x - k_ * v1 - v2;
    return wLp_ * lp + wBp_ * bp + wHp_ * hp;
}

} // namespace dg
