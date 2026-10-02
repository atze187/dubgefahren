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
    float nextNoise();

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
    float wowPhase_ = 0.0f;
    float flutterPhase_ = 0.0f;
    std::array<float, 3> rwSlow_ {};  // Random Walk für das langsame Wow (3 Pole)
    std::array<float, 3> rwJit_ {};   // gefiltertes Rauschen für den Flutter-Jitter (3 Pole)
    float rwSlowCoeff_ = 0.0f;
    float rwSlowGain_ = 1.0f;
    float rwJitCoeff_ = 0.0f;
    float rwJitGain_ = 1.0f;
    std::uint32_t rng_ = 0x9E3779B9u; // fester Startwert, wird bei reset() nicht zurückgesetzt
};

} // namespace dg
