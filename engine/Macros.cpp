#include "engine/Macros.h"
#include <algorithm>
#include <cmath>

namespace dg {

namespace {
// Aufschläge bei Knob = 1 und Obergrenzen. Hier nachjustieren, nicht im Code verteilt.
constexpr float kSpaceDelayMix = 0.35f;
constexpr float kSpaceFeedback = 0.30f;
constexpr float kSpaceFeedbackCap = 0.95f; // Space schwingt nie von selbst
constexpr float kSpaceReverbMix = 0.40f;
constexpr float kSpaceReverbDecay = 0.30f;

constexpr float kGritDrive = 0.70f;
constexpr float kGritWow = 0.50f;
constexpr float kGritToneDarker = 0.30f;
constexpr float kGritPhaserMix = 0.60f;
constexpr float kGritPhaserDepth = 0.30f;

constexpr float kThrowSend = 1.0f;
constexpr float kThrowFeedback = 0.50f;
constexpr float kThrowFeedbackCap = 1.10f; // nur Throw darf bis zur Selbstoszillation
constexpr float kThrowDelayMix = 0.50f;

float unit(float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }

// Hebt base um add an, höchstens bis cap; ein Grundwert über cap bleibt unverändert.
float raise(float base, float add, float cap) { return std::max(base, std::min(base + add, cap)); }
} // namespace

FxParams applyMacros(const FxParams& base, const MacroParams& m)
{
    const float space = unit(m.space);
    const float grit = unit(m.grit);
    const float thr = unit(m.throwAmount);

    FxParams e = base;

    e.delayMix = raise(e.delayMix, kSpaceDelayMix * space, 1.0f);
    e.delayFeedback = raise(e.delayFeedback, kSpaceFeedback * space, kSpaceFeedbackCap);
    e.reverbMix = raise(e.reverbMix, kSpaceReverbMix * space, 1.0f);
    e.reverbDecay = raise(e.reverbDecay, kSpaceReverbDecay * space, 1.0f);

    e.drive = raise(e.drive, kGritDrive * grit, 1.0f);
    e.delayWow = raise(e.delayWow, kGritWow * grit, 1.0f);
    e.delayTone = std::max(0.0f, e.delayTone - kGritToneDarker * grit);
    e.phaserMix = raise(e.phaserMix, kGritPhaserMix * grit, 1.0f);
    e.phaserDepth = raise(e.phaserDepth, kGritPhaserDepth * grit, 1.0f);

    e.delayFeedback = raise(e.delayFeedback, kThrowFeedback * thr, kThrowFeedbackCap);
    e.delayMix = raise(e.delayMix, kThrowDelayMix * thr, 1.0f);
    return e;
}

float throwSendBoost(const MacroParams& m) { return kThrowSend * unit(m.throwAmount); }

} // namespace dg
