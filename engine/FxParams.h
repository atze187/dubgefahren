#pragma once

namespace dg {

enum class DelayDivision { D1_16T, D1_16, D1_16D, D1_8T, D1_8, D1_8D, D1_4T, D1_4, D1_4D, D1_2T, D1_2, D1_2D, D1_1 };
constexpr int kNumDelayDivisions = 13;

double delayDivisionSeconds(DelayDivision d, double bpm);

struct FxParams
{
    float drive = 0.0f;              // 0 .. 1
    float cutoffHz = 20000.0f;       // 20 .. 20000
    float resonance = 0.1f;          // 0 .. 1
    float filterType = 0.0f;         // 0 = LP, 0.5 = BP, 1 = HP
    DelayDivision delayDiv = DelayDivision::D1_8D;
    float delayFeedback = 0.45f;     // 0 .. 1.1
    float delayTone = 0.5f;          // 0 .. 1
    float delayWow = 0.2f;           // 0 .. 1
    float delayMix = 0.35f;          // 0 .. 1
    float reverbDecay = 0.5f;        // 0 .. 1
    float reverbTone = 0.5f;         // 0 .. 1
    float reverbMix = 0.25f;         // 0 .. 1
    float masterDb = 0.0f;           // -60 (stumm) .. 6
};

} // namespace dg
