#pragma once
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Kit.h"
#include "engine/SlotFields.h"

namespace dg::pid {
inline constexpr const char* drive = "drive";
inline constexpr const char* cutoff = "fltCutoff";
inline constexpr const char* resonance = "fltRes";
inline constexpr const char* filterType = "fltType";
inline constexpr const char* delayTime = "dlyTime";
inline constexpr const char* delayFeedback = "dlyFeedback";
inline constexpr const char* delayTone = "dlyTone";
inline constexpr const char* delayWow = "dlyWow";
inline constexpr const char* delayMix = "dlyMix";
inline constexpr const char* reverbDecay = "revDecay";
inline constexpr const char* reverbTone = "revTone";
inline constexpr const char* reverbMix = "revMix";
inline constexpr const char* masterVol = "masterVol";
inline constexpr const char* perfPitch = "perfPitch";
inline constexpr const char* perfRate = "perfRate";
inline constexpr const char* perfDepth = "perfDepth";
inline constexpr const char* perfSweep = "perfSweep";
inline constexpr const char* perfTarget = "perfTarget";
inline constexpr const char* latchStop = "latchStop";
inline constexpr const char* panic = "panic";
} // namespace dg::pid

namespace dg {

constexpr int kParameterVersion = 1;

juce::String slotParamId(int slot, SlotField f);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(const Kit& defaults);

// Nur im Message-Thread aufrufen.
SlotParams readSlotFromParameters(juce::AudioProcessorValueTreeState& apvts, int slot);
void writeSlotToParameters(juce::AudioProcessorValueTreeState& apvts, int slot, const SlotParams& p);

// Hält die rohen Parameterzeiger für den Audio-Thread.
class ParamCache
{
public:
    explicit ParamCache(juce::AudioProcessorValueTreeState& apvts);
    void read(EngineParams& out) const;
    bool panicPressed() const;

private:
    using Ptr = std::atomic<float>*;
    std::array<std::array<Ptr, kNumSlotFields>, kNumSlots> slots_ {};
    Ptr drive_, cutoff_, resonance_, filterType_;
    Ptr delayTime_, delayFeedback_, delayTone_, delayWow_, delayMix_;
    Ptr reverbDecay_, reverbTone_, reverbMix_, masterVol_;
    Ptr perfPitch_, perfRate_, perfDepth_, perfSweep_, perfTarget_, latchStop_, panic_;
};

} // namespace dg
