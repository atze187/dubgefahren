#include "engine/SvFilter.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void SvFilter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    weightCoeff_ = onePoleCoeff(0.01f, sampleRate);
    reset();
    setParams(20000.0f, 0.0f);
    setType(FilterType::Lowpass, true);
}

void SvFilter::reset()
{
    ic1_[0] = ic1_[1] = 0.0f;
    ic2_[0] = ic2_[1] = 0.0f;
}

void SvFilter::setParams(float cutoffHz, float resonance)
{
    const float sr = static_cast<float>(sampleRate_);
    const float fc = std::clamp(cutoffHz, 20.0f, 0.49f * sr);
    g_ = std::tan(kPi * fc / sr);
    const float q = 0.5f * std::pow(40.0f, std::clamp(resonance, 0.0f, 1.0f));
    k_ = 1.0f / q;
    a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;
}

void SvFilter::setType(FilterType type, bool immediate)
{
    // Notch ist die Summe aus Tief- und Hochpass.
    switch (type)
    {
        case FilterType::Lowpass:  tLp_ = 1.0f; tBp_ = 0.0f; tHp_ = 0.0f; break;
        case FilterType::Bandpass: tLp_ = 0.0f; tBp_ = 1.0f; tHp_ = 0.0f; break;
        case FilterType::Highpass: tLp_ = 0.0f; tBp_ = 0.0f; tHp_ = 1.0f; break;
        case FilterType::Notch:    tLp_ = 1.0f; tBp_ = 0.0f; tHp_ = 1.0f; break;
    }
    if (immediate)
    {
        wLp_ = tLp_;
        wBp_ = tBp_;
        wHp_ = tHp_;
    }
}

void SvFilter::tick()
{
    wLp_ += weightCoeff_ * (tLp_ - wLp_);
    wBp_ += weightCoeff_ * (tBp_ - wBp_);
    wHp_ += weightCoeff_ * (tHp_ - wHp_);
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
