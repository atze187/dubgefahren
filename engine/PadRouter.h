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
};
using TriggerSettingsArray = std::array<TriggerSettings, kNumSlots>;

enum class LatchStopAction { Continue, Release, Stop };

int slotForNote(int note);

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

    int focusSlot() const { return focus_; }
    void setFocusSlot(int slot);
    bool isLatched(int slot) const;
    std::uint32_t latchedMask() const;

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
