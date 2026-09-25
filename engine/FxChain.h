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
    bool needsReset_ = false;

    // 20 ms Ein-Pol-Glättung gegen Zipper-Rauschen bei Live-Reglern.
    float smMaster_ = 1.0f;
    float smDelayMix_ = 0.0f;
    float smReverbMix_ = 0.0f;
    float smDrive_ = 0.0f;
    float smFilterType_ = 0.0f;
    bool smoothInit_ = false;
};

} // namespace dg
