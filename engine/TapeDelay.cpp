#include "engine/TapeDelay.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

void TapeDelay::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    const auto size = static_cast<std::size_t>(std::ceil(sampleRate * kMaxDelaySeconds)) + 4;
    for (auto& b : buf_)
        b.assign(size, 0.0f);
    glideCoeff_ = onePoleCoeff(0.25f, sampleRate);
    reset();
}

void TapeDelay::reset()
{
    for (auto& b : buf_)
        std::fill(b.begin(), b.end(), 0.0f);
    write_ = 0;
    lp_ = { 0.0f, 0.0f };
    wowPhase1_ = wowPhase2_ = 0.0f;
    snapped_ = false;
}

void TapeDelay::setParams(float timeSeconds, float feedback, float tone, float wow)
{
    const float sr = static_cast<float>(sampleRate_);
    target_ = std::clamp(timeSeconds, 0.001f, kMaxDelaySeconds - 0.1f) * sr;
    if (!snapped_)
    {
        current_ = target_;
        snapped_ = true;
    }
    feedback_ = std::clamp(feedback, 0.0f, 1.1f);
    wow_ = std::clamp(wow, 0.0f, 1.0f);
    const float cutoff = 500.0f * std::pow(24.0f, std::clamp(tone, 0.0f, 1.0f)); // 500 Hz .. 12 kHz
    lpCoeff_ = 1.0f - std::exp(-kTwoPi * cutoff / sr);
}

float TapeDelay::read(const std::vector<float>& buf, float delaySamples) const
{
    const std::size_t size = buf.size();
    const auto di = static_cast<std::size_t>(delaySamples);
    const float frac = delaySamples - static_cast<float>(di);
    const std::size_t i0 = (write_ + size - (di % size)) % size;      // sample at integer delay
    const std::size_t i1 = (i0 + size - 1) % size;                    // one sample older
    return buf[i0] + frac * (buf[i1] - buf[i0]);
}

void TapeDelay::process(float inL, float inR, float& wetL, float& wetR)
{
    const float sr = static_cast<float>(sampleRate_);
    current_ += (target_ - current_) * glideCoeff_; // Zeitänderung gleitet wie beim Band

    const float mod = wow_ * sr * (0.0025f * std::sin(wowPhase1_) + 0.0004f * std::sin(wowPhase2_));
    wowPhase1_ += kTwoPi * 0.55f / sr;
    wowPhase2_ += kTwoPi * 6.5f / sr;
    if (wowPhase1_ >= kTwoPi) wowPhase1_ -= kTwoPi;
    if (wowPhase2_ >= kTwoPi) wowPhase2_ -= kTwoPi;

    const float delay = std::max(1.0f, current_ + mod);
    const float in[2] = { inL, inR };
    float wet[2] = {};
    for (int ch = 0; ch < 2; ++ch)
    {
        const float r = read(buf_[static_cast<std::size_t>(ch)], delay);
        lp_[static_cast<std::size_t>(ch)] += lpCoeff_ * (r - lp_[static_cast<std::size_t>(ch)]);
        wet[ch] = lp_[static_cast<std::size_t>(ch)];
        // Soft-Clipper im Feedback-Weg: auch bei 110 % bleibt alles begrenzt.
        buf_[static_cast<std::size_t>(ch)][write_] = in[ch] + std::tanh(feedback_ * wet[ch]);
    }
    write_ = (write_ + 1) % buf_[0].size();
    wetL = wet[0];
    wetR = wet[1];
}

} // namespace dg
