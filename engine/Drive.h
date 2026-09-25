#pragma once
#include <algorithm>
#include <cmath>

namespace dg {

// drive 0..1: 0 = unverändert, 1 = kräftige tanh-Sättigung mit Pegelausgleich.
inline float driveSample(float x, float drive)
{
    if (drive <= 0.0f)
        return x;
    const float g = 1.0f + 19.0f * std::min(drive, 1.0f);
    const float wet = std::tanh(g * x) / std::sqrt(g);
    const float mix = std::min(1.0f, drive * 10.0f);
    return x + mix * (wet - x);
}

} // namespace dg
