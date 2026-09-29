#pragma once
#include "engine/Envelope.h"
#include "engine/SoundSource.h"

namespace dg {

// Spielt SampleData (mono) mit Tune und Performance-Pitch ab. Die Daten kommen bei jedem
// Aufruf über VoiceContext::sample; der Player besitzt sie nicht.
class SamplePlayer final : public SoundSource
{
public:
    void prepare(double sampleRate) override;
    void start(const VoiceContext& ctx, std::uint32_t seed) override;
    void release(const VoiceContext& ctx) override;
    void kill() override;
    bool isActive() const override { return env_.isActive(); }
    bool isReleasing() const override { return env_.isReleasing(); }
    void render(float* out, int numSamples, const VoiceContext& ctx) override;

private:
    void stop();

    double sampleRate_ = 44100.0;
    Envelope env_;
    double pos_ = 0.0;
    float perfCoeff_ = 1.0f;
    float smPitch_ = 0.0f;
};

} // namespace dg
