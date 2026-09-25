#include "engine/SpringReverb.h"
#include <algorithm>
#include <cmath>

namespace dg {

void SpringReverb::Line::init(int length)
{
    buf.assign(static_cast<std::size_t>(std::max(1, length)), 0.0f);
    idx = 0;
}

void SpringReverb::Line::clear()
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    idx = 0;
}

float SpringReverb::Allpass::process(float x)
{
    const float b = line.buf[line.idx];
    const float y = -x + b;
    line.buf[line.idx] = x + b * g;
    if (++line.idx >= line.buf.size())
        line.idx = 0;
    return y;
}

float SpringReverb::Comb::process(float x, float feedback, float damp)
{
    const float y = line.buf[line.idx];
    store = y * (1.0f - damp) + store * damp;
    line.buf[line.idx] = x + store * feedback;
    if (++line.idx >= line.buf.size())
        line.idx = 0;
    return y;
}

void SpringReverb::prepare(double sampleRate)
{
    const double scale = sampleRate / 44100.0;
    static constexpr int kDispersion[8] = { 3, 5, 7, 11, 13, 17, 19, 23 };
    static constexpr double kCombMs[4] = { 29.7, 37.1, 41.1, 43.7 };
    static constexpr double kDiffuserMs[2] = { 5.0, 1.7 };

    for (std::size_t ch = 0; ch < channels_.size(); ++ch)
    {
        auto& c = channels_[ch];
        const double spreadMs = ch == 0 ? 0.0 : 0.53; // rechter Kanal leicht versetzt
        for (std::size_t i = 0; i < c.dispersion.size(); ++i)
        {
            c.dispersion[i].line.init(static_cast<int>(std::lround(kDispersion[i] * scale)) + static_cast<int>(ch));
            c.dispersion[i].g = 0.6f;
        }
        for (std::size_t i = 0; i < c.combs.size(); ++i)
            c.combs[i].line.init(static_cast<int>(std::lround((kCombMs[i] + spreadMs) * 0.001 * sampleRate)));
        for (std::size_t i = 0; i < c.diffusers.size(); ++i)
        {
            c.diffusers[i].line.init(static_cast<int>(std::lround((kDiffuserMs[i] + spreadMs * 0.5) * 0.001 * sampleRate)));
            c.diffusers[i].g = 0.5f;
        }
    }
    reset();
}

void SpringReverb::reset()
{
    for (auto& c : channels_)
    {
        for (auto& a : c.dispersion)
            a.line.clear();
        for (auto& cb : c.combs)
        {
            cb.line.clear();
            cb.store = 0.0f;
        }
        for (auto& a : c.diffusers)
            a.line.clear();
    }
}

void SpringReverb::setParams(float decay, float tone)
{
    feedback_ = 0.70f + 0.27f * std::clamp(decay, 0.0f, 1.0f);
    damp_ = 0.05f + 0.6f * (1.0f - std::clamp(tone, 0.0f, 1.0f));
}

float SpringReverb::processChannel(Channel& c, float x)
{
    float d = x;
    for (auto& a : c.dispersion)
        d = a.process(d);
    float s = 0.0f;
    for (auto& cb : c.combs)
        s += cb.process(d * 0.08f, feedback_, damp_); // Eingangspegel so gewählt, dass Wet ≈ Dry-Pegel
    for (auto& a : c.diffusers)
        s = a.process(s);
    return s;
}

void SpringReverb::process(float inL, float inR, float& wetL, float& wetR)
{
    wetL = processChannel(channels_[0], inL);
    wetR = processChannel(channels_[1], inR);
}

} // namespace dg
