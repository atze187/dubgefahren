#include "engine/FxChain.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"
#include "engine/Drive.h"

namespace dg {

void FxChain::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    delay_.prepare(sampleRate);
    reverb_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    reset();
}

void FxChain::reset()
{
    delay_.reset();
    reverb_.reset();
    limiter_.reset();
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

    delay_.setParams(static_cast<float>(delayDivisionSeconds(p.delayDiv, bpm)), p.delayFeedback, p.delayTone, p.delayWow);
    reverb_.setParams(p.reverbDecay, p.reverbTone);

    const float targetMaster = volumeDbToGain(p.masterDb);
    const float targetDelayMix = p.delayMix;
    const float targetReverbMix = p.reverbMix;
    const float targetDrive = p.drive;

    if (!smoothInit_)
    {
        smMaster_ = targetMaster;
        smDelayMix_ = targetDelayMix;
        smReverbMix_ = targetReverbMix;
        smDrive_ = targetDrive;
        smoothInit_ = true;
    }
    const float smCoeff = onePoleCoeff(0.02f, sampleRate_);

    for (int i = 0; i < numSamples; ++i)
    {
        smMaster_ += smCoeff * (targetMaster - smMaster_);
        smDelayMix_ += smCoeff * (targetDelayMix - smDelayMix_);
        smReverbMix_ += smCoeff * (targetReverbMix - smReverbMix_);
        smDrive_ += smCoeff * (targetDrive - smDrive_);

        if (needsReset_)
        {
            mainL[i] = 0.0f;
            mainR[i] = 0.0f;
            sendL[i] = 0.0f;
            sendR[i] = 0.0f;
            continue;
        }

        const float ml = driveSample(mainL[i], smDrive_);
        const float mr = driveSample(mainR[i], smDrive_);
        float sl = driveSample(sendL[i], smDrive_);
        float sr = driveSample(sendR[i], smDrive_);

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
