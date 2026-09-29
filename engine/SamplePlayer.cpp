#include "engine/SamplePlayer.h"
#include <algorithm>
#include <cstdint>
#include "engine/DspMath.h"

namespace dg {

namespace {
constexpr float kPerfSmoothingS = 0.02f;

bool usable(const SampleData* d) { return d != nullptr && !d->samples.empty() && d->sampleRate > 0.0; }

// Catmull-Rom-Interpolation; Werte außerhalb des Puffers gelten als 0.
float cubicAt(const std::vector<float>& x, double pos)
{
    const auto n = static_cast<std::int64_t>(x.size());
    const auto i = static_cast<std::int64_t>(pos);
    const float t = static_cast<float>(pos - static_cast<double>(i));
    const auto at = [&](std::int64_t k) { return (k < 0 || k >= n) ? 0.0f : x[static_cast<std::size_t>(k)]; };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    return y1 + 0.5f * t * (y2 - y0 + t * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + t * (3.0f * (y1 - y2) + y3 - y0)));
}
} // namespace

void SamplePlayer::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    env_.prepare(sampleRate);
    perfCoeff_ = onePoleCoeff(kPerfSmoothingS, sampleRate);
    pos_ = 0.0;
}

void SamplePlayer::start(const VoiceContext& ctx, std::uint32_t)
{
    if (!usable(ctx.sample))
        return;
    pos_ = 0.0;
    smPitch_ = ctx.perf.pitchSemis;
    env_.noteOn(ctx.params->attackS);
}

void SamplePlayer::release(const VoiceContext& ctx) { env_.noteOff(ctx.params->releaseS); }

void SamplePlayer::kill() { env_.kill(); }

void SamplePlayer::stop() { env_.prepare(sampleRate_); }

void SamplePlayer::render(float* out, int numSamples, const VoiceContext& ctx)
{
    const SampleData* d = ctx.sample;
    if (!env_.isActive() || !usable(d))
    {
        if (env_.isActive())
            stop(); // Daten weg: sofort still, nie auf alte Daten zugreifen
        std::fill(out, out + numSamples, 0.0f);
        return;
    }

    const double length = static_cast<double>(d->samples.size());
    const double baseStep = d->sampleRate / sampleRate_;
    for (int i = 0; i < numSamples; ++i)
    {
        if (pos_ >= length || !env_.isActive())
        {
            stop();
            std::fill(out + i, out + numSamples, 0.0f);
            return;
        }
        smPitch_ += perfCoeff_ * (ctx.perf.pitchSemis - smPitch_);
        out[i] = cubicAt(d->samples, pos_) * env_.process();
        pos_ += baseStep * semitonesToRatio(ctx.params->tuneSemis + smPitch_);
    }
}

} // namespace dg
