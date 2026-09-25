#pragma once
#include <cstdint>
#include "engine/SlotParams.h"

namespace dg {

// Performance-Offsets, die zur Laufzeit auf eine Stimme wirken (Mitte = neutral).
struct PerfOffsets
{
    float pitchSemis = 0.0f;
    float rateFactor = 1.0f;
    float depthSemis = 0.0f;
    float sweepSemis = 0.0f;

    bool operator==(const PerfOffsets&) const = default;
};

struct VoiceContext
{
    const SlotParams* params = nullptr;
    double bpm = 120.0;
    PerfOffsets perf {};
};

// Klangquelle eines Slots. Heute SirenVoice, später z. B. ein SamplePlayer.
class SoundSource
{
public:
    virtual ~SoundSource() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void start(const VoiceContext& ctx, std::uint32_t seed) = 0;
    virtual void release(const VoiceContext& ctx) = 0;
    virtual void kill() = 0;
    virtual bool isActive() const = 0;
    virtual bool isReleasing() const = 0;
    // Überschreibt out[0..numSamples) mit Mono-Ausgabe (ohne Lautstärke und Pan).
    virtual void render(float* out, int numSamples, const VoiceContext& ctx) = 0;
};

} // namespace dg
