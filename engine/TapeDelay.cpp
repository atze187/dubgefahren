#include "engine/TapeDelay.h"
#include <algorithm>
#include <cmath>
#include "engine/DspMath.h"

namespace dg {

namespace {
// Klangkonstanten des Tape-Loops. Hier nachjustieren, nicht im Code verteilt.
constexpr float kLoopHighpassHz = 100.0f; // verhindert Bass-Aufstau im Loop
constexpr float kBumpHz = 120.0f;         // Kopf-Bump: sanftes Low-Shelf
constexpr float kBumpDb = 2.0f;
constexpr float kHeadLossHz = 9500.0f;    // feste leichte Höhenabsenkung
constexpr float kSatBias = 0.1f;          // Asymmetrie der Sättigung (gerade Obertöne)
const float kSatBiasOffset = std::tanh(kSatBias);

float onePoleHz(float hz, float sampleRate) { return 1.0f - std::exp(-kTwoPi * hz / sampleRate); }
} // namespace

void TapeDelay::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    const auto size = static_cast<std::size_t>(std::ceil(sampleRate * kMaxDelaySeconds)) + 4;
    for (auto& b : buf_)
        b.assign(size, 0.0f);
    glideCoeff_ = onePoleCoeff(0.25f, sampleRate);
    const float sr = static_cast<float>(sampleRate);
    hpCoeff_ = onePoleHz(kLoopHighpassHz, sr);
    bumpCoeff_ = onePoleHz(kBumpHz, sr);
    bumpGain_ = dbToGain(kBumpDb) - 1.0f;
    headCoeff_ = onePoleHz(kHeadLossHz, sr);
    reset();
}

void TapeDelay::reset()
{
    for (auto& b : buf_)
        std::fill(b.begin(), b.end(), 0.0f);
    write_ = 0;
    ch_.fill(ChannelState {});
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
    const std::size_t i0 = (write_ + size - (di % size)) % size;      // Sample bei ganzzahliger Verzögerung
    const float xm1 = buf[(i0 + 1) % size];                           // ein Sample neuer
    const float x0 = buf[i0];
    const float x1 = buf[(i0 + size - 1) % size];                     // ein Sample älter
    const float x2 = buf[(i0 + size - 2) % size];
    return cubicInterp(xm1, x0, x1, x2, frac);
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

    const float delay = std::max(2.0f, current_ + mod);
    const float in[2] = { inL, inR };
    float wet[2] = {};
    for (std::size_t ch = 0; ch < 2; ++ch)
    {
        auto& s = ch_[ch];
        float x = read(buf_[ch], delay);
        s.hp += hpCoeff_ * (x - s.hp); // Hochpass = Eingang minus Tiefpass
        x -= s.hp;
        s.bump += bumpCoeff_ * (x - s.bump);
        x += bumpGain_ * s.bump;
        s.tone += lpCoeff_ * (x - s.tone);
        s.head += headCoeff_ * (s.tone - s.head);
        wet[ch] = s.head;

        // Weicher, leicht asymmetrischer Clipper im Feedback-Weg: auch bei 110 % bleibt alles begrenzt.
        const float sat = std::tanh(feedback_ * wet[ch] + kSatBias) - kSatBiasOffset;
        buf_[ch][write_] = in[ch] + sat; // Gleichanteil der Asymmetrie fängt der Loop-Hochpass beim Lesen ab
    }
    write_ = (write_ + 1) % buf_[0].size();
    wetL = wet[0];
    wetR = wet[1];
}

} // namespace dg
