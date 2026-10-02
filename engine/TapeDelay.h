#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace dg {

class TapeDelay
{
public:
    static constexpr float kMaxDelaySeconds = 10.0f;

    void prepare(double sampleRate);
    void reset();
    void setParams(float timeSeconds, float feedback, float tone, float wow);
    // Liefert nur das Wet-Signal.
    void process(float inL, float inR, float& wetL, float& wetR);

private:
    struct ChannelState
    {
        float hp = 0.0f;     // Tiefpass-Zustand für den Loop-Hochpass
        float bump = 0.0f;   // Tiefpass-Zustand für den Bass-Bump
        float tone = 0.0f;   // Tone-Tiefpass
        float head = 0.0f;   // feste Höhenabsenkung (Kopfverlust)
    };

    float read(const std::vector<float>& buf, float delaySamples) const;

    double sampleRate_ = 44100.0;
    std::array<std::vector<float>, 2> buf_;
    std::size_t write_ = 0;
    float target_ = 1.0f;
    float current_ = 1.0f;
    bool snapped_ = false;
    float glideCoeff_ = 0.001f;
    float feedback_ = 0.0f;
    float wow_ = 0.0f;
    float lpCoeff_ = 1.0f;
    float hpCoeff_ = 0.0f;
    float bumpCoeff_ = 0.0f;
    float bumpGain_ = 0.0f;
    float headCoeff_ = 1.0f;
    std::array<ChannelState, 2> ch_ {};
    float wowPhase1_ = 0.0f;
    float wowPhase2_ = 0.0f;
};

} // namespace dg
