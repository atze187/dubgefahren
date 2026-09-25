#include "engine/SirenVoice.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

namespace {
constexpr float kPerfSmoothingS = 0.02f;
constexpr float kMinFreq = 10.0f;
constexpr float kMaxFreq = 18000.0f;
} // namespace

void SirenVoice::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    osc_.prepare(sampleRate);
    lfo_.prepare(sampleRate);
    env_.prepare(sampleRate);
    perfCoeff_ = onePoleCoeff(kPerfSmoothingS, sampleRate);
    lastFreq_ = 0.0f;
}

void SirenVoice::start(const VoiceContext& ctx, std::uint32_t seed)
{
    if (!env_.isActive())
        osc_.resetPhase(); // bei Neustart einer klingenden Stimme Phase behalten → kein Knacks
    lfo_.reset(seed);
    sweepElapsedS_ = 0.0;
    smPitch_ = ctx.perf.pitchSemis;
    smLogRate_ = std::log2(std::max(ctx.perf.rateFactor, 1.0e-3f));
    smDepth_ = ctx.perf.depthSemis;
    smSweep_ = ctx.perf.sweepSemis;
    env_.noteOn(ctx.params->attackS);
}

void SirenVoice::release(const VoiceContext& ctx) { env_.noteOff(ctx.params->releaseS); }

void SirenVoice::kill() { env_.kill(); }

void SirenVoice::render(float* out, int numSamples, const VoiceContext& ctx)
{
    if (!env_.isActive())
    {
        std::fill(out, out + numSamples, 0.0f);
        return;
    }

    const SlotParams& p = *ctx.params;
    const float targetLogRate = std::log2(std::max(ctx.perf.rateFactor, 1.0e-3f));
    const float baseRate = p.lfoSync ? lfoRateFromSync(p.lfoSyncDiv, ctx.bpm) : p.lfoRateHz;
    const double dtS = 1.0 / sampleRate_;

    for (int i = 0; i < numSamples; ++i)
    {
        smPitch_ += perfCoeff_ * (ctx.perf.pitchSemis - smPitch_);
        smLogRate_ += perfCoeff_ * (targetLogRate - smLogRate_);
        smDepth_ += perfCoeff_ * (ctx.perf.depthSemis - smDepth_);
        smSweep_ += perfCoeff_ * (ctx.perf.sweepSemis - smSweep_);

        const float rate = baseRate * std::exp2(smLogRate_);
        const float depth = std::max(0.0f, p.lfoDepthSemis + smDepth_);
        const float lfo = lfo_.process(p.lfoShape, rate);

        float sweep = 0.0f;
        if (sweepElapsedS_ < p.sweepTimeS)
            sweep = (p.sweepSemis + smSweep_) * static_cast<float>(1.0 - sweepElapsedS_ / p.sweepTimeS);
        sweepElapsedS_ += dtS;

        const float semis = lfo * depth + sweep + smPitch_;
        lastFreq_ = std::clamp(p.pitchHz * semitonesToRatio(semis), kMinFreq, kMaxFreq);

        out[i] = osc_.process(p.wave, lastFreq_, p.pulseWidth) * env_.process();
    }
}

} // namespace dg
