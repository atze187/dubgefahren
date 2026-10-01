#include "plugin/ParameterLayout.h"
#include <algorithm>
#include <cmath>

namespace dg {

namespace {

juce::NormalisableRange<float> rangeFor(float min, float max, float skewCentre)
{
    juce::NormalisableRange<float> r(min, max);
    if (skewCentre > min && skewCentre < max)
        r.setSkewForCentre(skewCentre);
    return r;
}

juce::StringArray toStringArray(std::span<const char* const> items)
{
    juce::StringArray a;
    for (const char* c : items)
        a.add(juce::String::fromUTF8(c));
    return a;
}

std::unique_ptr<juce::RangedAudioParameter> makeFloat(const juce::String& id, const juce::String& name,
                                                      float min, float max, float def, float skewCentre,
                                                      const juce::String& unit, int version = kParameterVersion)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, version }, name, rangeFor(min, max, skewCentre), def,
        juce::AudioParameterFloatAttributes().withLabel(unit));
}

std::unique_ptr<juce::RangedAudioParameter> makeChoice(const juce::String& id, const juce::String& name,
                                                       const juce::StringArray& choices, int def, int version = kParameterVersion)
{
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, version }, name, choices, def);
}

juce::String u8(const char* s) { return juce::String::fromUTF8(s); }

std::atomic<float>* raw(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    auto* p = apvts.getRawParameterValue(id);
    jassert(p != nullptr);
    return p;
}

float load(const std::atomic<float>* p) { return p->load(std::memory_order_relaxed); }

int loadIndex(const std::atomic<float>* p, int maxIndex)
{
    return std::clamp(static_cast<int>(std::lround(load(p))), 0, maxIndex);
}

} // namespace

juce::String slotParamId(int slot, SlotField f)
{
    return juce::String::formatted("s%02d_", slot + 1) + fieldSpec(f).key;
}

juce::String slotSourceParamId(int slot)
{
    return juce::String::formatted("s%02d_source", slot + 1);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(const Kit& defaults)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const FxParams fx {};

    layout.add(makeFloat(pid::drive, "Drive", 0.0f, 1.0f, fx.drive, 0.0f, ""));
    layout.add(makeChoice(pid::delayTime, "Delay Time",
                          { "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" },
                          static_cast<int>(fx.delayDiv)));
    layout.add(makeFloat(pid::delayFeedback, "Delay Feedback", 0.0f, 1.1f, fx.delayFeedback, 0.0f, ""));
    layout.add(makeFloat(pid::delayWow, "Delay Wow", 0.0f, 1.0f, fx.delayWow, 0.0f, ""));
    layout.add(makeFloat(pid::delayMix, "Delay Mix", 0.0f, 1.0f, fx.delayMix, 0.0f, ""));
    const auto fv = kDelayFilterParameterVersion;
    layout.add(makeChoice(pid::delayFltType, "Delay Filter Type", { "LP", "BP", "HP", "Notch" },
                          static_cast<int>(fx.delayFilterType), fv));
    layout.add(makeFloat(pid::delayFltCutoff, "Delay Filter Cutoff", 20.0f, 20000.0f, fx.delayFilterCutoffHz, 1000.0f, "Hz", fv));
    layout.add(makeFloat(pid::delayFltRes, "Delay Filter Resonance", 0.0f, 1.0f, fx.delayFilterRes, 0.0f, "", fv));
    layout.add(makeChoice(pid::delayLfoShape, "Delay LFO Shape", toStringArray(fieldSpec(SlotField::LfoShape).choices),
                          static_cast<int>(fx.delayLfoShape), fv));
    layout.add(makeFloat(pid::delayLfoRate, "Delay LFO Rate", 0.05f, 40.0f, fx.delayLfoRateHz, 2.0f, "Hz", fv));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { pid::delayLfoSync, fv }, "Delay LFO Sync",
                                                          fx.delayLfoSync));
    layout.add(makeChoice(pid::delayLfoSyncDiv, "Delay LFO Sync Rate", toStringArray(fieldSpec(SlotField::LfoSyncDiv).choices),
                          static_cast<int>(fx.delayLfoSyncDiv), fv));
    layout.add(makeFloat(pid::delayLfoCutDepth, "Delay LFO Cutoff Depth", 0.0f, 4.0f, fx.delayLfoCutDepthOct, 0.0f, "oct", fv));
    layout.add(makeFloat(pid::delayLfoResDepth, "Delay LFO Resonance Depth", 0.0f, 1.0f, fx.delayLfoResDepth, 0.0f, "", fv));
    layout.add(makeFloat(pid::reverbDecay, "Reverb Decay", 0.0f, 1.0f, fx.reverbDecay, 0.0f, ""));
    layout.add(makeFloat(pid::reverbTone, "Reverb Tone", 0.0f, 1.0f, fx.reverbTone, 0.0f, ""));
    layout.add(makeFloat(pid::reverbMix, "Reverb Mix", 0.0f, 1.0f, fx.reverbMix, 0.0f, ""));
    layout.add(makeFloat(pid::masterVol, "Master", -60.0f, 6.0f, fx.masterDb, 0.0f, "dB"));

    layout.add(makeFloat(pid::perfPitch, "Perf Pitch", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeFloat(pid::perfRate, "Perf Rate", 0.25f, 4.0f, 1.0f, 1.0f, "x"));
    layout.add(makeFloat(pid::perfDepth, "Perf Depth", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeFloat(pid::perfSweep, "Perf Sweep", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeChoice(pid::perfTarget, "Perf Target", { "Focus", "All" }, 0));
    layout.add(makeChoice(pid::latchStop, "Latch on Stop", { "Keep Playing", "Release", "Stop Immediately" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { pid::panic, kParameterVersion }, "Panic", false));

    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto prefix = juce::String::formatted("S%02d ", s + 1);
        for (int i = 0; i < kNumSlotFields; ++i)
        {
            const auto f = static_cast<SlotField>(i);
            const auto& spec = fieldSpec(f);
            const auto id = slotParamId(s, f);
            const auto name = prefix + u8(spec.name);
            const float def = getSlotField(defaults.slots[static_cast<std::size_t>(s)], f);
            switch (spec.kind)
            {
                case FieldKind::Float:
                    layout.add(makeFloat(id, name, spec.min, spec.max, def, spec.skewCentre, u8(spec.unit), spec.versionHint));
                    break;
                case FieldKind::Choice:
                    layout.add(makeChoice(id, name, toStringArray(spec.choices), static_cast<int>(std::lround(def)), spec.versionHint));
                    break;
                case FieldKind::Bool:
                    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, spec.versionHint }, name, def > 0.5f));
                    break;
            }
        }
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { slotSourceParamId(s), kSourceParameterVersion }, prefix + "Source",
            juce::StringArray { "Empty", "Synth", "Sample" },
            static_cast<int>(defaults.slots[static_cast<std::size_t>(s)].source),
            juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    }
    return layout;
}

SlotParams readSlotFromParameters(juce::AudioProcessorValueTreeState& apvts, int slot)
{
    SlotParams p;
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        setSlotField(p, f, raw(apvts, slotParamId(slot, f))->load());
    }
    p.source = static_cast<SourceType>(loadIndex(raw(apvts, slotSourceParamId(slot)), 2));
    return p;
}

void writeSlotToParameters(juce::AudioProcessorValueTreeState& apvts, int slot, const SlotParams& p)
{
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        auto* param = apvts.getParameter(slotParamId(slot, f));
        jassert(param != nullptr);
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(getSlotField(p, f)));
        param->endChangeGesture();
    }
    auto* source = apvts.getParameter(slotSourceParamId(slot));
    jassert(source != nullptr);
    source->beginChangeGesture();
    source->setValueNotifyingHost(source->convertTo0to1(static_cast<float>(p.source)));
    source->endChangeGesture();
}

ParamCache::ParamCache(juce::AudioProcessorValueTreeState& apvts)
    : drive_(raw(apvts, pid::drive)), delayTime_(raw(apvts, pid::delayTime)),
      delayFeedback_(raw(apvts, pid::delayFeedback)),
      delayWow_(raw(apvts, pid::delayWow)), delayMix_(raw(apvts, pid::delayMix)),
      delayFltType_(raw(apvts, pid::delayFltType)), delayFltCutoff_(raw(apvts, pid::delayFltCutoff)),
      delayFltRes_(raw(apvts, pid::delayFltRes)), delayLfoShape_(raw(apvts, pid::delayLfoShape)),
      delayLfoRate_(raw(apvts, pid::delayLfoRate)), delayLfoSync_(raw(apvts, pid::delayLfoSync)),
      delayLfoSyncDiv_(raw(apvts, pid::delayLfoSyncDiv)), delayLfoCutDepth_(raw(apvts, pid::delayLfoCutDepth)),
      delayLfoResDepth_(raw(apvts, pid::delayLfoResDepth)),
      reverbDecay_(raw(apvts, pid::reverbDecay)), reverbTone_(raw(apvts, pid::reverbTone)),
      reverbMix_(raw(apvts, pid::reverbMix)), masterVol_(raw(apvts, pid::masterVol)),
      perfPitch_(raw(apvts, pid::perfPitch)), perfRate_(raw(apvts, pid::perfRate)),
      perfDepth_(raw(apvts, pid::perfDepth)), perfSweep_(raw(apvts, pid::perfSweep)),
      perfTarget_(raw(apvts, pid::perfTarget)), latchStop_(raw(apvts, pid::latchStop)),
      panic_(raw(apvts, pid::panic))
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        for (int i = 0; i < kNumSlotFields; ++i)
            slots_[static_cast<std::size_t>(s)][static_cast<std::size_t>(i)] =
                raw(apvts, slotParamId(s, static_cast<SlotField>(i)));
        sources_[static_cast<std::size_t>(s)] = raw(apvts, slotSourceParamId(s));
    }
}

void ParamCache::read(EngineParams& out) const
{
    for (std::size_t s = 0; s < slots_.size(); ++s)
    {
        for (std::size_t i = 0; i < slots_[s].size(); ++i)
            setSlotField(out.slots[s], static_cast<SlotField>(i), load(slots_[s][i]));
        out.slots[s].source = static_cast<SourceType>(loadIndex(sources_[s], 2));
    }

    FxParams& fx = out.global.fx;
    fx.drive = load(drive_);
    fx.delayDiv = static_cast<DelayDivision>(loadIndex(delayTime_, kNumDelayDivisions - 1));
    fx.delayFeedback = load(delayFeedback_);
    fx.delayWow = load(delayWow_);
    fx.delayMix = load(delayMix_);
    fx.delayFilterType = static_cast<FilterType>(loadIndex(delayFltType_, kNumFilterTypes - 1));
    fx.delayFilterCutoffHz = load(delayFltCutoff_);
    fx.delayFilterRes = load(delayFltRes_);
    fx.delayLfoShape = static_cast<LfoShape>(loadIndex(delayLfoShape_, 4));
    fx.delayLfoRateHz = load(delayLfoRate_);
    fx.delayLfoSync = load(delayLfoSync_) > 0.5f;
    fx.delayLfoSyncDiv = static_cast<SyncDivision>(loadIndex(delayLfoSyncDiv_, 10));
    fx.delayLfoCutDepthOct = load(delayLfoCutDepth_);
    fx.delayLfoResDepth = load(delayLfoResDepth_);
    fx.reverbDecay = load(reverbDecay_);
    fx.reverbTone = load(reverbTone_);
    fx.reverbMix = load(reverbMix_);
    fx.masterDb = load(masterVol_);

    out.global.perf = PerfOffsets { load(perfPitch_), load(perfRate_), load(perfDepth_), load(perfSweep_) };
    out.global.perfTarget = loadIndex(perfTarget_, 1) == 1 ? PerfTarget::All : PerfTarget::Focus;
    out.global.latchStop = static_cast<LatchStopAction>(loadIndex(latchStop_, 2));
}

bool ParamCache::panicPressed() const { return load(panic_) > 0.5f; }

} // namespace dg
