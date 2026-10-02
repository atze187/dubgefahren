#pragma once

namespace dg {

// Sättigung für Drive: weich (tanh) bei wenig Drive, ab der Mitte zunehmend hart (Clip), jeweils mit
// ADAA gegen Aliasing, kleinem Bias für gerade Obertöne, DC-Blocker und teilweisem Pegelausgleich.
// drive 0..1 (bereits geglättet); 0 ist bit-genau transparent.
class Saturator
{
public:
    void prepare(double sampleRate);
    void reset();
    float process(float x, float drive);

private:
    double uPrev_ = 0.0;
    double fSoftPrev_ = 0.0;
    double fHardPrev_ = 0.0;
    bool havePrev_ = false;
    double dcX_ = 0.0;
    double dcY_ = 0.0;
    double dcR_ = 0.999;
};

} // namespace dg
