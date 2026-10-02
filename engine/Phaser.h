#pragma once
#include <array>

namespace dg {

// Sanfter 4-Stufen-Phaser (Phase-90-Charakter) für die Delay-Rückläufer: vier Allpässe 1. Ordnung mit
// LFO-modulierter Grenzfrequenz, weichem Feedback und 50/50-Mischung. Bei mix unter 1e-6 wird das Signal
// bit-genau durchgereicht. depth und mix werden vom Aufrufer geglättet.
class Phaser
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(float rateHz, float depth, float mix);
    void process(float& l, float& r); // in place

private:
    struct Channel
    {
        std::array<float, 4> z {}; // Allpass-Zustände
        float fb = 0.0f;           // letzter Ausgang der Kette (für das Feedback)
    };

    float run(Channel& c, float x, float lfo) const;

    double sampleRate_ = 44100.0;
    double phase_ = 0.0; // LFO-Phase 0..1
    float rateHz_ = 0.4f;
    float depth_ = 0.5f;
    float mix_ = 0.0f;
    std::array<Channel, 2> ch_ {};
};

} // namespace dg
