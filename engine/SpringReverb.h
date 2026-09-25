#pragma once
#include <array>
#include <cstddef>
#include <vector>

namespace dg {

class SpringReverb
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(float decay, float tone);
    // Liefert nur das Wet-Signal.
    void process(float inL, float inR, float& wetL, float& wetR);

private:
    struct Line
    {
        std::vector<float> buf;
        std::size_t idx = 0;
        void init(int length);
        void clear();
    };

    struct Allpass
    {
        Line line;
        float g = 0.5f;
        float process(float x);
    };

    struct Comb
    {
        Line line;
        float store = 0.0f;
        float process(float x, float feedback, float damp);
    };

    struct Channel
    {
        std::array<Allpass, 8> dispersion;
        std::array<Comb, 4> combs;
        std::array<Allpass, 2> diffusers;
    };

    float processChannel(Channel& c, float x);

    std::array<Channel, 2> channels_;
    float feedback_ = 0.8f;
    float damp_ = 0.3f;
};

} // namespace dg
