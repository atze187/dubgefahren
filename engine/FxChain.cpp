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
    smoothInit_ = false;
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

    const float targetMaster = volumeDbToGain(p.masterDb);
    const float targetDelayMix = p.delayMix;
    const float targetReverbMix = p.reverbMix;
    const float targetDrive = p.drive;
    const float targetFilterType = std::clamp(p.filterType, 0.0f, 1.0f);

    if (!smoothInit_)
    {
        smMaster_ = targetMaster;
        smDelayMix_ = targetDelayMix;
        smReverbMix_ = targetReverbMix;
        smDrive_ = targetDrive;
        smFilterType_ = targetFilterType;
        smoothInit_ = true;
    }
    const float smCoeff = onePoleCoeff(0.02f, sampleRate_);

    for (int i = 0; i < numSamples; ++i)
    {
        smMaster_ += smCoeff * (targetMaster - smMaster_);
        smDelayMix_ += smCoeff * (targetDelayMix - smDelayMix_);
        smReverbMix_ += smCoeff * (targetReverbMix - smReverbMix_);
        smDrive_ += smCoeff * (targetDrive - smDrive_);
        smFilterType_ += smCoeff * (targetFilterType - smFilterType_);
        filterMain_.setTypeWeights(smFilterType_);
        filterSend_.setTypeWeights(smFilterType_);

        if (needsReset_)
        {
            mainL[i] = 0.0f;
            mainR[i] = 0.0f;
            sendL[i] = 0.0f;
            sendR[i] = 0.0f;
            continue;
        }

        const float ml = filterMain_.process(driveSample(mainL[i], smDrive_), 0);
        const float mr = filterMain_.process(driveSample(mainR[i], smDrive_), 1);
        float sl = filterSend_.process(driveSample(sendL[i], smDrive_), 0);
        float sr = filterSend_.process(driveSample(sendR[i], smDrive_), 1);

        float dl = 0.0f, dr = 0.0f;
        delay_.process(sl, sr, dl, dr);
        sl += smDelayMix_ * dl;
        sr += smDelayMix_ * dr;

        float rl = 0.0f, rr = 0.0f;
        reverb_.process(sl, sr, rl, rr);
        sl += smReverbMix_ * rl;
        sr += smReverbMix_ * rr;

        float outL = (ml + sl) * smMaster_;
        float outR = (mr + sr) * smMaster_;
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
