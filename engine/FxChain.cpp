#include "engine/FxChain.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"
#include "engine/Drive.h"

namespace dg {

void FxChain::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    filterMain_.prepare(sampleRate);
    filterSend_.prepare(sampleRate);
    delay_.prepare(sampleRate);
    reverb_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    reset();
}

void FxChain::reset()
{
    filterMain_.reset();
    filterSend_.reset();
    delay_.reset();
    reverb_.reset();
    limiter_.reset();
    cutoffInit_ = false;
}

void FxChain::process(float* mainL, float* mainR, float* sendL, float* sendR, int numSamples,
                      const FxParams& p, double bpm)
{
    if (needsReset_)
    {
        reset();
        needsReset_ = false;
    }

    const float target = std::clamp(p.cutoffHz, 20.0f, 20000.0f);
    if (!cutoffInit_)
    {
        cutoff_ = target;
        cutoffInit_ = true;
    }
    const float a = 1.0f - std::exp(-static_cast<float>(numSamples) / (0.02f * static_cast<float>(sampleRate_)));
    cutoff_ = std::exp(std::log(cutoff_) + a * (std::log(target) - std::log(cutoff_)));

    filterMain_.setParams(cutoff_, p.resonance, p.filterType);
    filterSend_.setParams(cutoff_, p.resonance, p.filterType);
    delay_.setParams(static_cast<float>(delayDivisionSeconds(p.delayDiv, bpm)), p.delayFeedback, p.delayTone, p.delayWow);
    reverb_.setParams(p.reverbDecay, p.reverbTone);
    const float master = volumeDbToGain(p.masterDb);

    for (int i = 0; i < numSamples; ++i)
    {
        if (needsReset_)
        {
            mainL[i] = 0.0f;
            mainR[i] = 0.0f;
            sendL[i] = 0.0f;
            sendR[i] = 0.0f;
            continue;
        }

        const float ml = filterMain_.process(driveSample(mainL[i], p.drive), 0);
        const float mr = filterMain_.process(driveSample(mainR[i], p.drive), 1);
        float sl = filterSend_.process(driveSample(sendL[i], p.drive), 0);
        float sr = filterSend_.process(driveSample(sendR[i], p.drive), 1);

        float dl = 0.0f, dr = 0.0f;
        delay_.process(sl, sr, dl, dr);
        sl += p.delayMix * dl;
        sr += p.delayMix * dr;

        float rl = 0.0f, rr = 0.0f;
        reverb_.process(sl, sr, rl, rr);
        sl += p.reverbMix * rl;
        sr += p.reverbMix * rr;

        float outL = (ml + sl) * master;
        float outR = (mr + sr) * master;
        if (!std::isfinite(outL) || !std::isfinite(outR))
        {
            needsReset_ = true;
            outL = outR = 0.0f;
            sl = sr = 0.0f;
        }
        limiter_.process(outL, outR);
        mainL[i] = outL;
        mainR[i] = outR;
        sendL[i] = sl;
        sendR[i] = sr;
    }
}

} // namespace dg
