#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace dgtest {

inline std::size_t clampEnd(const std::vector<float>& x, std::size_t to)
{
    return std::min(to, x.size());
}

inline int countRisingZeroCrossings(const std::vector<float>& x, std::size_t from = 0,
                                    std::size_t to = std::numeric_limits<std::size_t>::max())
{
    int count = 0;
    to = clampEnd(x, to);
    for (std::size_t i = std::max<std::size_t>(from, 1); i < to; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            ++count;
    return count;
}

inline float estimateFrequency(const std::vector<float>& x, double sampleRate, std::size_t from, std::size_t to)
{
    to = clampEnd(x, to);
    const double seconds = static_cast<double>(to - from) / sampleRate;
    return static_cast<float>(countRisingZeroCrossings(x, from, to) / seconds);
}

inline float peakAbs(const std::vector<float>& x, std::size_t from = 0,
                     std::size_t to = std::numeric_limits<std::size_t>::max())
{
    float p = 0.0f;
    to = clampEnd(x, to);
    for (std::size_t i = from; i < to; ++i)
        p = std::max(p, std::abs(x[i]));
    return p;
}

inline float rms(const std::vector<float>& x, std::size_t from = 0,
                 std::size_t to = std::numeric_limits<std::size_t>::max())
{
    to = clampEnd(x, to);
    if (to <= from)
        return 0.0f;
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
}

inline float maxStep(const std::vector<float>& x, std::size_t from = 1,
                     std::size_t to = std::numeric_limits<std::size_t>::max())
{
    float m = 0.0f;
    to = clampEnd(x, to);
    for (std::size_t i = std::max<std::size_t>(from, 1); i < to; ++i)
        m = std::max(m, std::abs(x[i] - x[i - 1]));
    return m;
}

inline bool allFinite(const std::vector<float>& x)
{
    return std::all_of(x.begin(), x.end(), [](float v) { return std::isfinite(v); });
}

inline std::vector<float> sine(float freq, double sampleRate, int numSamples, float amp = 1.0f)
{
    std::vector<float> out(static_cast<std::size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<std::size_t>(i)] =
            amp * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * freq * i / sampleRate));
    return out;
}

inline std::vector<float> noise(int numSamples, float amp, std::uint32_t seed = 1)
{
    std::vector<float> out(static_cast<std::size_t>(numSamples));
    std::uint32_t s = seed == 0 ? 1u : seed;
    for (auto& v : out)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        v = amp * (static_cast<float>(s) / 4294967295.0f * 2.0f - 1.0f);
    }
    return out;
}

} // namespace dgtest
