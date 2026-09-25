#pragma once

namespace dg {

constexpr int kNumSlots = 16;
constexpr int kFirstNote = 36; // Slot 1 = Note 36 (C1 in Ableton)

enum class Waveform { Sine, Triangle, Saw, Square };
enum class LfoShape { Square, Triangle, SawUp, SawDown, SampleHold };
enum class TriggerMode { Gate, Latch, OneShot };
enum class SyncDivision { D1_32, D1_16T, D1_16, D1_8T, D1_8, D1_4T, D1_4, D1_2, Bar1, Bars2, Bars4 };

// Länge einer LFO-Periode in Viertelnoten (4/4-Takt).
float syncDivisionBeats(SyncDivision d);

struct SlotParams
{
    Waveform wave = Waveform::Square;
    float pulseWidth = 0.5f;       // 0.05 .. 0.95
    float pitchHz = 440.0f;        // 20 .. 5000
    LfoShape lfoShape = LfoShape::Square;
    float lfoRateHz = 2.0f;        // 0.05 .. 40
    bool lfoSync = false;
    SyncDivision lfoSyncDiv = SyncDivision::D1_8;
    float lfoDepthSemis = 12.0f;   // 0 .. 48
    float sweepSemis = 0.0f;       // -48 .. 48
    float sweepTimeS = 0.5f;       // 0.01 .. 10
    float attackS = 0.005f;        // 0 .. 5
    float releaseS = 0.3f;         // 0 .. 10
    TriggerMode trigMode = TriggerMode::Gate;
    float oneShotS = 1.0f;         // 0.05 .. 10
    int chokeGroup = 0;            // 0 = keine, 1 .. 4
    float volumeDb = -6.0f;        // -60 (stumm) .. 6
    float pan = 0.0f;              // -1 .. 1
    float fxSend = 0.3f;           // 0 .. 1

    bool operator==(const SlotParams&) const = default;
};

} // namespace dg
