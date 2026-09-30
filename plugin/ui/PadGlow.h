#pragma once
#include <algorithm>
#include <cmath>

namespace dg::ui {

inline constexpr float kPadGlowHold = 0.7f;         // Helligkeit eines gehaltenen Pads
inline constexpr float kPadGlowTau = 0.08f;         // Zeitkonstante des Abfalls auf den Haltewert (s)
inline constexpr float kPadGlowFadeSeconds = 0.2f;  // Ausklingen von 1 auf 0 (s)
inline constexpr float kPadGlowSnap = 0.005f;       // ab diesem Abstand rastet der Haltewert ein

// Schreibt die Helligkeit (0…1) eines Pads um dtSeconds fort.
// active: das Pad klingt gerade; wasActive: Zustand im vorigen Schritt.
// Ungültige Eingaben (NaN, negatives dt, Helligkeit außerhalb 0…1) werden entschärft.
inline float advancePadGlow(float brightness, bool active, bool wasActive, float dtSeconds)
{
    const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f; // fängt auch NaN
    const float b = std::isfinite(brightness) ? std::clamp(brightness, 0.0f, 1.0f) : (active ? kPadGlowHold : 0.0f);

    if (active)
    {
        if (!wasActive)
            return 1.0f;
        const float next = kPadGlowHold + (b - kPadGlowHold) * std::exp(-dt / kPadGlowTau);
        return std::abs(next - kPadGlowHold) < kPadGlowSnap ? kPadGlowHold : next;
    }
    return std::max(0.0f, b - dt / kPadGlowFadeSeconds);
}

} // namespace dg::ui
