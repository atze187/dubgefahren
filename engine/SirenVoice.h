#pragma once
#include "engine/Envelope.h"
#include "engine/Lfo.h"
#include "engine/Oscillator.h"
#include "engine/SoundSource.h"

namespace dg {

class SirenVoice final : public SoundSource
{
public:
    void prepare(double sampleRate) override;
    void start(const VoiceContext& ctx, std::uint32_t seed) override;
    void release(const VoiceContext& ctx) override;
    void kill() override;
    bool isActive() const override { return env_.isActive(); }
    bool isReleasing() const override { return env_.isReleasing(); }
    void render(float* out, int numSamples, const VoiceContext& ctx) override;

    float currentFrequency() const { return lastFreq_; }

private:
    double sampleRate_ = 44100.0;
    Oscillator osc_;
    Lfo lfo_;
    Envelope env_;
    float perfCoeff_ = 1.0f;
    float smPitch_ = 0.0f;
    float smLogRate_ = 0.0f;
    float smDepth_ = 0.0f;
    float smSweep_ = 0.0f;
    double sweepElapsedS_ = 0.0;
    float lastFreq_ = 0.0f;
};

} // namespace dg
