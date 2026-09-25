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

} // namespace dg
