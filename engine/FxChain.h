#pragma once
#include "engine/FxParams.h"
#include "engine/Limiter.h"
#include "engine/SpringReverb.h"
#include "engine/SvFilter.h"
#include "engine/TapeDelay.h"

namespace dg {

class FxChain
{
public:
    void prepare(double sampleRate);
    void reset();
    // Ergebnis steht danach in mainL/mainR; sendL/sendR werden überschrieben.
    void process(float* mainL, float* mainR, float* sendL, float* sendR, int numSamples,
                 const FxParams& p, double bpm);

private:
    double sampleRate_ = 44100.0;
    SvFilter filterMain_;
    SvFilter filterSend_;
    TapeDelay delay_;
    SpringReverb reverb_;
    Limiter limiter_;
    float cutoff_ = 20000.0f;
    bool cutoffInit_ = false;
};

} // namespace dg
