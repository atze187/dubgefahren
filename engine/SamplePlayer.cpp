#include "engine/SamplePlayer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "engine/DspMath.h"
#include "engine/SampleRegion.h"

namespace dg {

namespace {
constexpr float kPerfSmoothingS = 0.02f;
constexpr double kEndRampS = 0.002;    // lineare Ausblendung am Ende des Bereichs
constexpr double kRetrigFadeS = 0.005; // Ausblendzeit des alten Lesekopfs beim Retrigger

bool usable(const SampleData* d) { return d != nullptr && !d->samples.empty() && d->sampleRate > 0.0; }

// Catmull-Rom-Interpolation; Werte außerhalb des Puffers gelten als 0.
float cubicAt(const std::vector<float>& x, double pos)
{
    const auto n = static_cast<std::int64_t>(x.size());
    const double fl = std::floor(pos); // auch für negative Positionen (Rückwärtslauf) abrunden
    const auto i = static_cast<std::int64_t>(fl);
    const float t = static_cast<float>(pos - fl);
    const auto at = [&](std::int64_t k) { return (k < 0 || k >= n) ? 0.0f : x[static_cast<std::size_t>(k)]; };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    return y1 + 0.5f * t * (y2 - y0 + t * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + t * (3.0f * (y1 - y2) + y3 - y0)));
}

// Die Stimme loopt nur, wenn Loop an ist und der Slot nicht als One Shot spielt.
bool loops(const SlotParams& p) { return p.loop && p.trigMode != TriggerMode::OneShot; }
} // namespace

void SamplePlayer::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    env_.prepare(sampleRate);
    perfCoeff_ = onePoleCoeff(kPerfSmoothingS, sampleRate);
    pos_ = 0.0;
    fadeGain_ = 0.0f;
    dir_ = 1.0;
    fadeDir_ = 1.0;
    overrun_ = false;
}

void SamplePlayer::start(const VoiceContext& ctx, std::uint32_t)
{
    if (!usable(ctx.sample))
        return;
    const SlotParams& p = *ctx.params;
    const auto region = resolveSampleRegion(p.sampleStart, p.loopStart, p.sampleEnd, ctx.sample->samples.size());
    // Retrigger: alte Position als auslaufenden Lesekopf weiterlaufen lassen.
    if (env_.isActive())
    {
        fadePos_ = pos_;
        fadeDir_ = dir_;
        fadeGain_ = 1.0f;
    }
    else
        fadeGain_ = 0.0f;
    dir_ = p.reverse ? -1.0 : 1.0;
    pos_ = p.reverse ? std::max(region.start, region.end - 1.0) : region.start;
    overrun_ = false;
    smPitch_ = ctx.perf.pitchSemis;
    env_.noteOn(p.attackS);
}

void SamplePlayer::release(const VoiceContext& ctx) { env_.noteOff(ctx.params->releaseS); }

void SamplePlayer::kill() { env_.kill(); }

void SamplePlayer::stop()
{
    env_.prepare(sampleRate_);
    fadeGain_ = 0.0f;
    overrun_ = false;
}

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

    const SlotParams& p = *ctx.params;
    const auto& x = d->samples;
    const SampleRegion region = resolveSampleRegion(p.sampleStart, p.loopStart, p.sampleEnd, x.size());
    dir_ = p.reverse ? -1.0 : 1.0; // wirkt sofort, auch während des Spielens
    const bool looping = loops(p);
    const LoopSegment seg = resolveLoopSegment(region, p.reverse, x.size());
    const double segLen = seg.hi - seg.lo;
    // X-Fade: Anteil der Segmentlänge, begrenzt auf das Material, das in Laufrichtung vor dem
    // Schleifenanfang liegt (vorwärts unterhalb von lo, rückwärts oberhalb von hi).
    const double n = static_cast<double>(x.size());
    const double xfWanted = looping ? std::clamp(static_cast<double>(p.loopXfadePct), 0.0, 50.0) / 100.0 * segLen : 0.0;
    const double xf = std::min(xfWanted, dir_ > 0.0 ? seg.lo : n - seg.hi);

    const double baseStep = d->sampleRate / sampleRate_;
    const double endRampLen = kEndRampS * sampleRate_;
    const float fadeDec = static_cast<float>(1.0 / (kRetrigFadeS * sampleRate_));
    for (int i = 0; i < numSamples; ++i)
    {
        if (!env_.isActive())
        {
            stop();
            std::fill(out + i, out + numSamples, 0.0f);
            return;
        }
        smPitch_ += perfCoeff_ * (ctx.perf.pitchSemis - smPitch_);
        const double step = baseStep * semitonesToRatio(p.tuneSemis + smPitch_);

        // Ende des Bereichs in Laufrichtung erreicht?
        const bool past = dir_ > 0.0 ? pos_ >= region.end : pos_ < region.start;
        if (past && !overrun_)
        {
            // Mehr als einen Schritt hinter dem Ende liegt die Position nur, wenn ein Marker
            // unter der Stimme verschoben wurde.
            const double over = dir_ > 0.0 ? pos_ - region.end : region.start - pos_;
            const bool far = over > step + 1.0;
            if (looping)
            {
                if (far)
                {
                    // Sprung an den Schleifenanfang; die alte Position blendet als zweiter Lesekopf aus.
                    fadePos_ = pos_;
                    fadeDir_ = dir_;
                    fadeGain_ = 1.0f;
                    pos_ = dir_ > 0.0 ? seg.lo : std::max(seg.lo, seg.hi - 1.0);
                }
                else if (dir_ > 0.0)
                    pos_ = seg.lo + std::fmod(pos_ - seg.lo, segLen); // auch wenn der Schritt länger als das Segment ist
                else
                    pos_ = seg.hi - std::fmod(seg.hi - pos_, segLen);
            }
            else if (far)
            {
                env_.kill(); // kurz ausblenden statt hart abbrechen
                overrun_ = true;
            }
            else
            {
                stop();
                std::fill(out + i, out + numSamples, 0.0f);
                return;
            }
        }

        // Am Bereichsende linear auf 0 ausblenden (Restlänge in Ausgabesamples / Rampenlänge).
        const auto endGain = [&](double pos, double dir) {
            if (looping || overrun_)
                return 1.0f;
            const double remaining = (dir > 0.0 ? region.end - pos : pos - region.start + 1.0) / step;
            return remaining < endRampLen ? static_cast<float>(std::max(0.0, remaining) / endRampLen) : 1.0f;
        };

        // Liest das Sample an pos. Kurz vor dem Rücksprung wird in das Material übergeblendet, das
        // eine Segmentlänge entfernt liegt; am Sprung ist das Signal dadurch stetig. Hinter dem
        // Segmentende (g = 1) liest der Kopf so weiter, als wäre er schon gesprungen.
        const auto read = [&](double pos, double dir) {
            float v = cubicAt(x, pos);
            if (xf > 0.0 && dir == dir_)
            {
                if (dir > 0.0 && pos >= seg.hi - xf)
                {
                    const float g = static_cast<float>(std::clamp((pos - (seg.hi - xf)) / xf, 0.0, 1.0));
                    v += (cubicAt(x, pos - segLen) - v) * g;
                }
                else if (dir < 0.0 && pos < seg.lo + xf)
                {
                    const float g = static_cast<float>(std::clamp((seg.lo + xf - pos) / xf, 0.0, 1.0));
                    v += (cubicAt(x, pos + segLen) - v) * g;
                }
            }
            return v;
        };

        // Beim Retrigger und beim Sprung Überblendung: neuer Kopf blendet ein, alter aus (Summe der Gewichte = 1).
        float y = read(pos_, dir_) * endGain(pos_, dir_) * (1.0f - fadeGain_);
        pos_ += dir_ * step;
        if (fadeGain_ > 0.0f)
        {
            // Außerhalb der Datei liest der alte Kopf Nullen; sein Gewicht läuft normal aus,
            // damit das Einblenden des neuen Kopfes stetig bleibt.
            // Der alte Kopf liest mit derselben Überblendung weiter, sonst spränge das Signal, wenn
            // der Retrigger mitten in die X-Fade-Zone fällt.
            y += read(fadePos_, fadeDir_) * endGain(fadePos_, fadeDir_) * fadeGain_;
            fadePos_ += fadeDir_ * step;
            fadeGain_ = std::max(0.0f, fadeGain_ - fadeDec);
        }
        out[i] = y * env_.process();
    }
}

} // namespace dg
