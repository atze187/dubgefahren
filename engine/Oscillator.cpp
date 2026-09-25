#include "engine/Oscillator.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

namespace {
// Polynomielle Korrektur an Sprungstellen (PolyBLEP).
float polyBlep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
} // namespace

void Oscillator::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    phase_ = 0.0f;
}

float Oscillator::process(Waveform w, float freqHz, float pulseWidth)
{
    const float dt = std::clamp(freqHz / static_cast<float>(sampleRate_), 0.0f, 0.5f);
    const float p = phase_;
    float out = 0.0f;

    switch (w)
    {
        case Waveform::Sine:
            out = std::sin(kTwoPi * p);
            break;
        case Waveform::Triangle:
            out = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            break;
        case Waveform::Saw:
            out = 2.0f * p - 1.0f - polyBlep(p, dt);
            break;
        case Waveform::Square:
        {
            const float pw = std::clamp(pulseWidth, 0.05f, 0.95f);
            out = p < pw ? 1.0f : -1.0f;
            out += polyBlep(p, dt);
            float t2 = p - pw + 1.0f;
            if (t2 >= 1.0f)
                t2 -= 1.0f;
            out -= polyBlep(t2, dt);
            break;
        }
    }

    phase_ += dt;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;
    return out;
}

} // namespace dg
