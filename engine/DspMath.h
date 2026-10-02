#pragma once
#include <cmath>

namespace dg {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kMinVolumeDb = -60.0f; // -60 dB bedeutet stumm

inline float semitonesToRatio(float semis) { return std::exp2(semis / 12.0f); }

float dbToGain(float db);

// Wie dbToGain, aber kMinVolumeDb und darunter sind exakt 0.
float volumeDbToGain(float db);

// Koeffizient für y += a * (x - y) mit Zeitkonstante timeSeconds.
inline float onePoleCoeff(float timeSeconds, double sampleRate)
{
    if (timeSeconds <= 0.0f)
        return 1.0f;
    return 1.0f - std::exp(-1.0f / (timeSeconds * static_cast<float>(sampleRate)));
}

// Catmull-Rom zwischen x0 und x1 (t = 0..1); xm1 liegt vor x0, x2 hinter x1.
inline float cubicInterp(float xm1, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

} // namespace dg
