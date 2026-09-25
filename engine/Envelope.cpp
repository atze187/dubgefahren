#include "engine/Envelope.h"
#include <algorithm>

namespace dg {

void Envelope::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    stage_ = Stage::Idle;
    level_ = 0.0f;
    step_ = 0.0f;
}

void Envelope::noteOn(float attackS)
{
    const float t = std::max(attackS, kMinEnvTimeS);
    step_ = 1.0f / (t * static_cast<float>(sampleRate_));
    stage_ = Stage::Attack;
}

void Envelope::noteOff(float releaseS)
{
    if (stage_ == Stage::Idle || stage_ == Stage::Kill)
        return;
    const float t = std::max(releaseS, kMinEnvTimeS);
    step_ = std::max(level_, 1.0e-6f) / (t * static_cast<float>(sampleRate_));
    stage_ = Stage::Release;
}

void Envelope::kill()
{
    if (stage_ == Stage::Idle)
        return;
    step_ = std::max(level_, 1.0e-6f) / (kKillTimeS * static_cast<float>(sampleRate_));
    stage_ = Stage::Kill;
}

float Envelope::process()
{
    switch (stage_)
    {
        case Stage::Idle:
        case Stage::Sustain:
            break;
        case Stage::Attack:
            level_ += step_;
            if (level_ >= 1.0f)
            {
                level_ = 1.0f;
                stage_ = Stage::Sustain;
            }
            break;
        case Stage::Release:
        case Stage::Kill:
            level_ -= step_;
            if (level_ <= 0.0f)
            {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
    }
    return level_;
}

} // namespace dg
