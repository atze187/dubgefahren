#pragma once

namespace dg {

enum class FilterType { Lowpass, Bandpass, Highpass, Notch };
constexpr int kNumFilterTypes = 4;

// Zustandsvariabler Filter (TPT, Zavalishin), 12 dB. Der Typ wird gewählt; bei einem Wechsel
// gleiten die Mischgewichte in etwa 10 ms zum neuen Typ, damit es nicht knackt.
class SvFilter
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(float cutoffHz, float resonance);
    // immediate: die Gewichte springen sofort (erster Aufruf, Reset); sonst gleiten sie über tick().
    void setType(FilterType type, bool immediate = false);
    // Einmal pro Abtastwert aufrufen, vor process() beider Kanäle: rückt die Typ-Gewichte vor.
    void tick();
    float process(float x, int channel);

private:
    double sampleRate_ = 44100.0;
    float g_ = 0.0f, k_ = 2.0f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
    float wLp_ = 1.0f, wBp_ = 0.0f, wHp_ = 0.0f; // aktuelle Gewichte
    float tLp_ = 1.0f, tBp_ = 0.0f, tHp_ = 0.0f; // Zielgewichte
    float weightCoeff_ = 0.01f;
    float ic1_[2] {};
    float ic2_[2] {};
};

} // namespace dg
