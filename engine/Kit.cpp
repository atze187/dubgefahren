#include "engine/Kit.h"

namespace dg {

namespace {

struct Preset
{
    const char* name;
    Waveform wave;
    float pitchHz;
    LfoShape shape;
    float rateHz;
    float depthSemis;
    float sweepSemis;
    float sweepTimeS;
    float attackS;
    float releaseS;
    TriggerMode mode;
    float oneShotS;
    int choke;
    float volumeDb;
    float send;
};

using W = Waveform;
using L = LfoShape;
using T = TriggerMode;

// Sweep > 0: startet höher und fällt auf die Grundtonhöhe; Sweep < 0: startet tiefer und steigt.
constexpr Preset kPresets[kNumSlots] = {
    { "Classic",     W::Square,   600.0f,  L::Square,     4.0f,  7.0f,  0.0f,  0.5f,  0.005f, 0.4f,  T::Gate,    1.0f,  0, -9.0f,  0.4f },
    { "Wail",        W::Square,   500.0f,  L::Triangle,   0.5f, 12.0f,  0.0f,  0.5f,  0.05f,  1.0f,  T::Latch,   1.0f,  0, -9.0f,  0.5f },
    { "Trill",       W::Square,   900.0f,  L::Square,    12.0f,  3.0f,  0.0f,  0.5f,  0.005f, 0.2f,  T::Gate,    1.0f,  0, -10.0f, 0.3f },
    { "Laser",       W::Saw,      250.0f,  L::Square,     1.0f,  0.0f, 36.0f,  0.25f, 0.001f, 0.05f, T::OneShot, 0.3f,  1, -10.0f, 0.5f },
    { "Riser",       W::Saw,     1200.0f,  L::Square,     1.0f,  0.0f, -24.0f, 4.0f,  0.5f,   0.5f,  T::OneShot, 4.0f,  1, -10.0f, 0.4f },
    { "Faller",      W::Saw,      300.0f,  L::Square,     1.0f,  0.0f, 24.0f,  4.0f,  0.01f,  0.5f,  T::OneShot, 4.0f,  1, -10.0f, 0.4f },
    { "Alarm",       W::Square,   700.0f,  L::Square,     2.0f,  7.0f,  0.0f,  0.5f,  0.005f, 0.3f,  T::Latch,   1.0f,  0, -10.0f, 0.3f },
    { "UFO",         W::Sine,     800.0f,  L::SampleHold, 8.0f, 24.0f,  0.0f,  0.5f,  0.02f,  0.6f,  T::Latch,   1.0f,  0, -8.0f,  0.5f },
    { "Bleep",       W::Sine,    1500.0f,  L::Square,     1.0f,  0.0f,  0.0f,  0.5f,  0.001f, 0.05f, T::OneShot, 0.12f, 1, -8.0f,  0.6f },
    { "Zap",         W::Saw,      190.0f,  L::Square,     1.0f,  0.0f, 48.0f,  0.08f, 0.001f, 0.03f, T::OneShot, 0.1f,  1, -10.0f, 0.5f },
    { "Siren Up",    W::Square,   400.0f,  L::SawUp,      1.5f, 12.0f,  0.0f,  0.5f,  0.005f, 0.4f,  T::Gate,    1.0f,  0, -10.0f, 0.4f },
    { "Siren Down",  W::Square,   400.0f,  L::SawDown,    1.5f, 12.0f,  0.0f,  0.5f,  0.005f, 0.4f,  T::Gate,    1.0f,  0, -10.0f, 0.4f },
    { "Deep Wobble", W::Triangle,  80.0f,  L::Triangle,   0.8f, 12.0f,  0.0f,  0.5f,  0.05f,  0.8f,  T::Latch,   1.0f,  0, -3.0f,  0.2f },
    { "Chirp",       W::Square,  2500.0f,  L::SawUp,     10.0f, 12.0f,  0.0f,  0.5f,  0.005f, 0.2f,  T::Gate,    1.0f,  0, -12.0f, 0.4f },
    { "Horn",        W::Saw,      220.0f,  L::Square,     1.0f,  0.0f,  5.0f,  0.15f, 0.01f,  0.2f,  T::Gate,    1.0f,  0, -9.0f,  0.3f },
    { "Drop",        W::Sine,     100.0f,  L::Square,     1.0f,  0.0f, 48.0f,  3.0f,  0.005f, 0.5f,  T::OneShot, 3.0f,  1, -6.0f,  0.4f },
};

} // namespace

Kit makeFactoryKit()
{
    Kit k;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const Preset& pr = kPresets[s];
        SlotParams p;
        p.wave = pr.wave;
        p.pitchHz = pr.pitchHz;
        p.lfoShape = pr.shape;
        p.lfoRateHz = pr.rateHz;
        p.lfoDepthSemis = pr.depthSemis;
        p.sweepSemis = pr.sweepSemis;
        p.sweepTimeS = pr.sweepTimeS;
        p.attackS = pr.attackS;
        p.releaseS = pr.releaseS;
        p.trigMode = pr.mode;
        p.oneShotS = pr.oneShotS;
        p.chokeGroup = pr.choke;
        p.volumeDb = pr.volumeDb;
        p.fxSend = pr.send;
        k.slots[static_cast<std::size_t>(s)] = p;
        k.names[static_cast<std::size_t>(s)] = pr.name;
    }
    return k;
}

} // namespace dg
