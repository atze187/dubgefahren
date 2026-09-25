#pragma once

namespace dg {

// Zustandsvariabler Filter (TPT, Zavalishin) mit stufenloser Überblendung LP → BP → HP.
class SvFilter
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(float cutoffHz, float resonance, float type);
    float process(float x, int channel);

private:
    double sampleRate_ = 44100.0;
    float g_ = 0.0f, k_ = 2.0f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
    float wLp_ = 1.0f, wBp_ = 0.0f, wHp_ = 0.0f;
    float ic1_[2] {};
    float ic2_[2] {};
};

} // namespace dg
