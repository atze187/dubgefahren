#pragma once
#include <array>
#include <cstdint>
#include "engine/SlotParams.h"

namespace dg {

class VoiceControl
{
public:
    virtual ~VoiceControl() = default;
    virtual void startVoice(int slot) = 0;
    virtual void releaseVoice(int slot) = 0;
    virtual void killVoice(int slot) = 0;
    virtual bool isVoiceActive(int slot) const = 0;
    virtual bool isVoiceReleasing(int slot) const = 0;
};

struct TriggerSettings
{
    TriggerMode mode = TriggerMode::Gate;
    float oneShotS = 1.0f;
    int chokeGroup = 0;
    bool untilEnd = false; // One-Shot ohne Längen-Timer: die Stimme endet selbst (Sample)
};
using TriggerSettingsArray = std::array<TriggerSettings, kNumSlots>;

enum class LatchStopAction { Continue, Release, Stop };

int slotForNote(int note);

// Höchste Note für das erste Pad, bei der alle 16 Pads noch im MIDI-Bereich 0..127 liegen.
constexpr int kMaxPadFirstNote = 127 - (kNumSlots - 1);

// Rechnet eine MIDI-Note anhand der Note des ersten Pads in die Engine-Note (kFirstNote + Slot) um.
// Liefert -1, wenn die Note keinem der 16 Pads entspricht.
int translateNote(int note, int firstNote);

class PadRouter
{
public:
    void prepare(double sampleRate);

    void noteOn(int note, const TriggerSettingsArray& settings, VoiceControl& voices);
    void noteOff(int note, VoiceControl& voices);
    void previewOn(int slot, const TriggerSettingsArray& settings, VoiceControl& voices);
    void previewOff(int slot, VoiceControl& voices);
    void advance(int numSamples, VoiceControl& voices);
    void transportStopped(LatchStopAction action, VoiceControl& voices);
    void panic(VoiceControl& voices);
    // Stoppt die Stimme des Slots sofort und vergisst Latch-, Halte- und One-Shot-Zustand.
    void killSlot(int slot, VoiceControl& voices);

    int focusSlot() const { return focus_; }
    void setFocusSlot(int slot);
    bool isLatched(int slot) const;
    std::uint32_t latchedMask() const;
    // Kleinste positive verbleibende One-Shot-Zeit über alle Slots, oder INT_MAX ohne laufenden Timer.
    int samplesUntilNextExpiry() const;

private:
    void start(int slot, TriggerMode mode, const TriggerSettingsArray& settings, VoiceControl& voices);
    void clearSlot(int slot);

    double sampleRate_ = 44100.0;
    int focus_ = 0;
    std::array<TriggerMode, kNumSlots> startedMode_ {};
    std::array<bool, kNumSlots> latched_ {};
    std::array<std::int64_t, kNumSlots> oneShotRemaining_ {};
    std::array<bool, kNumSlots> previewHeld_ {};
};

} // namespace dg
