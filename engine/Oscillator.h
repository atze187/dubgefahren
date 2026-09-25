#pragma once
#include "engine/SlotParams.h"

namespace dg {

class Oscillator
{
public:
    void prepare(double sampleRate);
    void resetPhase() { phase_ = 0.0f; }
    // Liefert den aktuellen Wert und rückt die Phase vor.
    float process(Waveform w, float freqHz, float pulseWidth);

private:
    double sampleRate_ = 44100.0;
    float phase_ = 0.0f;
};

} // namespace dg
