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
constexpr float kNoiseGain = 1.78e-4f;    // ca. -75 dBFS Spitze
const float kSatBiasOffset = std::tanh(kSatBias);

// Modulationstiefen in Sekunden bei Wow = 1 (Summe der Maxima 0.00286 s, unter dem früheren Maximum 0.0029 s).
constexpr float kWowSinSec = 0.0015f;   // langsamer Sinus
constexpr float kWowRandSec = 0.0009f;  // Random Walk
constexpr float kFlutterSec = 0.0003f;  // schneller Sinus
constexpr float kJitterSec = 0.0001f;   // feiner Jitter
constexpr float kWowHz = 0.55f;
constexpr float kFlutterHz = 7.5f;
constexpr float kFlutterHzSpread = 1.5f;
constexpr float kRandWalkHz = 0.5f;
constexpr float kJitterHz = 25.0f;
constexpr float kRightOffsetRad = 0.1f; // fester Versatz des rechten Kanals

float onePoleHz(float hz, float sampleRate) { return 1.0f - std::exp(-kTwoPi * hz / sampleRate); }

// Drei gleiche Einpol-Tiefpässe hintereinander: das Zufallssignal fällt oberhalb der Grenzfrequenz
// mit 18 dB/Okt. ab. Ein einzelner Pol ließe Rauschen bis Nyquist auf der Bandgeschwindigkeit liegen
// und erzeugte breitbandige Seitenbänder um jedes Echo.
float cascade3(std::array<float, 3>& st, float coeff, float x)
{
    st[0] += coeff * (x - st[0]);
    st[1] += coeff * (st[0] - st[1]);
    st[2] += coeff * (st[1] - st[2]);
    return st[2];
}

// Verstärkung, die gleichverteiltes Rauschen (Std 0,577) nach cascade3 auf Std 0,5 bringt.
// Varianz der Kaskade: a^6 * (1 + 4q + q^2) / (1 - q)^5 mit q = (1 - a)^2.
float randomWalkGain(float coeff)
{
    const double a = coeff;
    const double q = (1.0 - a) * (1.0 - a);
    const double oneMinusQ = a * (2.0 - a);
    const double variance = std::pow(a, 6.0) * (1.0 + 4.0 * q + q * q) / std::pow(oneMinusQ, 5.0);
    return static_cast<float>(0.5 / (0.5774 * std::sqrt(variance)));
}
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
    rwSlowCoeff_ = onePoleHz(kRandWalkHz, sr);
    rwSlowGain_ = randomWalkGain(rwSlowCoeff_);
    rwJitCoeff_ = onePoleHz(kJitterHz, sr);
    rwJitGain_ = randomWalkGain(rwJitCoeff_);
    reset();
}

void TapeDelay::reset()
{
    for (auto& b : buf_)
        std::fill(b.begin(), b.end(), 0.0f);
    write_ = 0;
    ch_.fill(ChannelState {});
    wowPhase_ = flutterPhase_ = 0.0f;
    rwSlow_.fill(0.0f);
    rwJit_.fill(0.0f);
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

float TapeDelay::nextNoise()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 2147483648.0f - 1.0f; // -1 .. 1
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

    const float slow = std::clamp(cascade3(rwSlow_, rwSlowCoeff_, nextNoise()) * rwSlowGain_, -1.0f, 1.0f);
    const float jit = std::clamp(cascade3(rwJit_, rwJitCoeff_, nextNoise()) * rwJitGain_, -1.0f, 1.0f);
    wowPhase_ += kTwoPi * kWowHz / sr;
    flutterPhase_ += kTwoPi * (kFlutterHz + kFlutterHzSpread * slow) / sr;
    if (wowPhase_ >= kTwoPi) wowPhase_ -= kTwoPi;
    if (flutterPhase_ >= kTwoPi) flutterPhase_ -= kTwoPi;

    const float in[2] = { inL, inR };
    float wet[2] = {};
    for (std::size_t ch = 0; ch < 2; ++ch)
    {
        const float off = ch == 0 ? 0.0f : kRightOffsetRad;
        const float modSeconds = kWowSinSec * std::sin(wowPhase_ + off)
                               + kWowRandSec * slow
                               + kFlutterSec * (1.0f + 0.2f * jit) * std::sin(flutterPhase_ + 2.0f * off)
                               + kJitterSec * jit;
        const float delay = std::max(2.0f, current_ + wow_ * sr * modSeconds);

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
        buf_[ch][write_] = in[ch] + sat + kNoiseGain * nextNoise(); // Gleichanteil der Asymmetrie fängt der Loop-Hochpass beim Lesen ab
    }
    write_ = (write_ + 1) % buf_[0].size();
    wetL = wet[0];
    wetR = wet[1];
}

} // namespace dg
