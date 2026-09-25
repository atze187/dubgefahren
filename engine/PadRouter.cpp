#include "engine/PadRouter.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace dg {

int slotForNote(int note)
{
    const int s = note - kFirstNote;
    return (s >= 0 && s < kNumSlots) ? s : -1;
}

void PadRouter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    focus_ = 0;
    for (int s = 0; s < kNumSlots; ++s)
        clearSlot(s);
}

void PadRouter::clearSlot(int s)
{
    latched_[s] = false;
    oneShotRemaining_[s] = -1;
    previewHeld_[s] = false;
    startedMode_[s] = TriggerMode::Gate;
}

void PadRouter::start(int slot, TriggerMode mode, const TriggerSettingsArray& settings, VoiceControl& voices)
{
    const int group = settings[slot].chokeGroup;
    if (group > 0)
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            if (s != slot && settings[s].chokeGroup == group && voices.isVoiceActive(s))
            {
                voices.killVoice(s);
                clearSlot(s);
            }
        }
    }

    voices.startVoice(slot);
    focus_ = slot;
    startedMode_[slot] = mode;
    latched_[slot] = mode == TriggerMode::Latch;
    previewHeld_[slot] = false;
    oneShotRemaining_[slot] = mode == TriggerMode::OneShot
        ? std::max<std::int64_t>(1, std::llround(settings[slot].oneShotS * sampleRate_))
        : -1;
}

void PadRouter::noteOn(int note, const TriggerSettingsArray& settings, VoiceControl& voices)
{
    const int slot = slotForNote(note);
    if (slot < 0)
        return;

    const TriggerMode mode = settings[slot].mode;
    if (mode == TriggerMode::Latch && latched_[slot] && voices.isVoiceActive(slot) && !voices.isVoiceReleasing(slot))
    {
        voices.releaseVoice(slot);
        latched_[slot] = false;
        focus_ = slot;
        return;
    }
    start(slot, mode, settings, voices);
}

void PadRouter::noteOff(int note, VoiceControl& voices)
{
    const int slot = slotForNote(note);
    if (slot < 0)
        return;
    if (startedMode_[slot] == TriggerMode::Gate && !previewHeld_[slot]
        && voices.isVoiceActive(slot) && !voices.isVoiceReleasing(slot))
        voices.releaseVoice(slot);
}

void PadRouter::previewOn(int slot, const TriggerSettingsArray& settings, VoiceControl& voices)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    start(slot, TriggerMode::Gate, settings, voices);
    previewHeld_[slot] = true;
}

void PadRouter::previewOff(int slot, VoiceControl& voices)
{
    if (slot < 0 || slot >= kNumSlots || !previewHeld_[slot])
        return;
    previewHeld_[slot] = false;
    if (voices.isVoiceActive(slot) && !voices.isVoiceReleasing(slot))
        voices.releaseVoice(slot);
}

void PadRouter::advance(int numSamples, VoiceControl& voices)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        if (oneShotRemaining_[s] < 0)
            continue;
        oneShotRemaining_[s] -= numSamples;
        if (oneShotRemaining_[s] <= 0)
        {
            oneShotRemaining_[s] = -1;
            if (voices.isVoiceActive(s) && !voices.isVoiceReleasing(s))
                voices.releaseVoice(s);
        }
    }
}

void PadRouter::transportStopped(LatchStopAction action, VoiceControl& voices)
{
    if (action == LatchStopAction::Continue)
        return;
    for (int s = 0; s < kNumSlots; ++s)
    {
        if (!latched_[s])
            continue;
        latched_[s] = false;
        if (!voices.isVoiceActive(s))
            continue;
        if (action == LatchStopAction::Stop)
            voices.killVoice(s);
        else if (!voices.isVoiceReleasing(s))
            voices.releaseVoice(s);
    }
}

void PadRouter::panic(VoiceControl& voices)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        if (voices.isVoiceActive(s))
            voices.killVoice(s);
        clearSlot(s);
    }
}

void PadRouter::setFocusSlot(int slot)
{
    if (slot >= 0 && slot < kNumSlots)
        focus_ = slot;
}

bool PadRouter::isLatched(int slot) const
{
    return slot >= 0 && slot < kNumSlots && latched_[slot];
}

std::uint32_t PadRouter::latchedMask() const
{
    std::uint32_t m = 0;
    for (int s = 0; s < kNumSlots; ++s)
        if (latched_[s])
            m |= 1u << s;
    return m;
}

int PadRouter::samplesUntilNextExpiry() const
{
    std::int64_t best = -1;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const std::int64_t r = oneShotRemaining_[s];
        if (r > 0 && (best < 0 || r < best))
            best = r;
    }
    if (best < 0)
        return std::numeric_limits<int>::max();
    return static_cast<int>(std::min<std::int64_t>(best, std::numeric_limits<int>::max()));
}

} // namespace dg
