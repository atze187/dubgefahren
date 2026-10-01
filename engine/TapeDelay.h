#pragma once
#include <array>
#include <vector>
#include "engine/Lfo.h"
#include "engine/SlotParams.h"
#include "engine/SvFilter.h"

namespace dg {

// Filter im Feedback-Weg samt LFO; die Rate liegt bereits in Hz vor.
struct DelayFilterParams
{
    FilterType type = FilterType::Lowpass;
    float cutoffHz = 2400.0f;   // 20 .. 20000
    float resonance = 0.1f;     // 0 .. 1
    LfoShape lfoShape = LfoShape::Triangle;
    float lfoRateHz = 0.5f;     // 0 .. 40
    float cutDepthOct = 0.0f;   // 0 .. 4
    float resDepth = 0.0f;      // 0 .. 1
};

class TapeDelay
{
public:
    static constexpr float kMaxDelaySeconds = 10.0f;

    void prepare(double sampleRate);
    void reset();
    void setParams(float timeSeconds, float feedback, float wow, const DelayFilterParams& filter);
    // Liefert nur das Wet-Signal; es ist bereits gefiltert und speist das Feedback.
    void process(float inL, float inR, float& wetL, float& wetR);

private:
    float read(const std::vector<float>& buf, float delaySamples) const;
    void updateFilter(float lfo);

    static constexpr int kControlInterval = 8; // Filterkoeffizienten alle 8 Abtastwerte

    double sampleRate_ = 44100.0;
    std::array<std::vector<float>, 2> buf_;
    std::size_t write_ = 0;
    float target_ = 1.0f;
    float current_ = 1.0f;
    bool snapped_ = false;
    float glideCoeff_ = 0.001f;
    float feedback_ = 0.0f;
    float wow_ = 0.0f;
    float wowPhase1_ = 0.0f;
    float wowPhase2_ = 0.0f;

    SvFilter filter_;
    Lfo lfo_;
    DelayFilterParams filterParams_;
    bool filterSnapped_ = false;
    int controlCounter_ = 0;
    float controlCoeff_ = 0.1f;
    float cutLog2_ = 11.23f; // log2(2400)
    float resBase_ = 0.1f;
};

} // namespace dg
