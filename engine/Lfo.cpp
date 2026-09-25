#include "engine/Lfo.h"
#include <algorithm>

namespace dg {

void Lfo::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset(1);
}

void Lfo::reset(std::uint32_t seed)
{
    std::uint32_t z = seed + 0x9E3779B9u;
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    z ^= z >> 16;
    rng_ = z == 0 ? 1u : z;
    phase_ = 0.0f;
    holdValue_ = nextRandom();
}

float Lfo::nextRandom()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 4294967295.0f;
}

float Lfo::process(LfoShape shape, float rateHz)
{
    const float p = phase_;
    float out = 0.0f;
    switch (shape)
    {
        case LfoShape::Square:     out = p < 0.5f ? 0.0f : 1.0f; break;
        case LfoShape::Triangle:   out = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p; break;
        case LfoShape::SawUp:      out = p; break;
        case LfoShape::SawDown:    out = 1.0f - p; break;
        case LfoShape::SampleHold: out = holdValue_; break;
    }

    phase_ += std::max(0.0f, rateHz) / static_cast<float>(sampleRate_);
    if (phase_ >= 1.0f)
    {
        phase_ -= static_cast<float>(static_cast<int>(phase_));
        holdValue_ = nextRandom();
    }
    return out;
}

float lfoRateFromSync(SyncDivision d, double bpm)
{
    return static_cast<float>(bpm / 60.0) / syncDivisionBeats(d);
}

} // namespace dg
