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
    double dir_ = 1.0; // +1 vorwärts, -1 rückwärts
    // Zweiter Lesekopf: blendet beim Retrigger und bei einem Sprung die alte Position aus.
    double fadePos_ = 0.0;
    double fadeDir_ = 1.0;
    float fadeGain_ = 0.0f;
    // Der alte Kopf stammt von einem Sprung nach einer Marker-Änderung und liegt außerhalb des
    // Segments: Er liest ohne X-Fade weiter, sonst spränge er um eine Segmentlänge.
    bool fadeRaw_ = false;
    bool startedAsOneShot_ = false;
    // Der Bereich wurde unter der Stimme weggezogen: Sie blendet per Kill-Fade aus und
    // beachtet das Bereichsende nicht mehr.
    bool overrun_ = false;
    float perfCoeff_ = 1.0f;
    float smPitch_ = 0.0f;
};

} // namespace dg
