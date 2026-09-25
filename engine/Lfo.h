#pragma once
#include <cstdint>
#include "engine/SlotParams.h"

namespace dg {

class Lfo
{
public:
    void prepare(double sampleRate);
    void reset(std::uint32_t seed);
    // Liefert den aktuellen Wert in [0, 1] und rückt die Phase vor.
    float process(LfoShape shape, float rateHz);

private:
    float nextRandom();

    double sampleRate_ = 44100.0;
    float phase_ = 0.0f;
    float holdValue_ = 0.0f;
    std::uint32_t rng_ = 1;
};

float lfoRateFromSync(SyncDivision d, double bpm);

} // namespace dg
