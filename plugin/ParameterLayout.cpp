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
                                                      const juce::String& unit)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, kParameterVersion }, name, rangeFor(min, max, skewCentre), def,
        juce::AudioParameterFloatAttributes().withLabel(unit));
}

std::unique_ptr<juce::RangedAudioParameter> makeChoice(const juce::String& id, const juce::String& name,
                                                       const juce::StringArray& choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, kParameterVersion }, name, choices, def);
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

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout(const Kit& defaults)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const FxParams fx {};

    layout.add(makeFloat(pid::drive, "Drive", 0.0f, 1.0f, fx.drive, 0.0f, ""));
    layout.add(makeFloat(pid::cutoff, "Filter Cutoff", 20.0f, 20000.0f, fx.cutoffHz, 1000.0f, "Hz"));
    layout.add(makeFloat(pid::resonance, "Filter Resonanz", 0.0f, 1.0f, fx.resonance, 0.0f, ""));
    layout.add(makeFloat(pid::filterType, "Filter Typ", 0.0f, 1.0f, fx.filterType, 0.0f, ""));
    layout.add(makeChoice(pid::delayTime, "Delay Zeit",
                          { "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" },
                          static_cast<int>(fx.delayDiv)));
    layout.add(makeFloat(pid::delayFeedback, "Delay Feedback", 0.0f, 1.1f, fx.delayFeedback, 0.0f, ""));
    layout.add(makeFloat(pid::delayTone, "Delay Tone", 0.0f, 1.0f, fx.delayTone, 0.0f, ""));
    layout.add(makeFloat(pid::delayWow, "Delay Wow", 0.0f, 1.0f, fx.delayWow, 0.0f, ""));
    layout.add(makeFloat(pid::delayMix, "Delay Mix", 0.0f, 1.0f, fx.delayMix, 0.0f, ""));
    layout.add(makeFloat(pid::reverbDecay, "Hall Decay", 0.0f, 1.0f, fx.reverbDecay, 0.0f, ""));
    layout.add(makeFloat(pid::reverbTone, "Hall Tone", 0.0f, 1.0f, fx.reverbTone, 0.0f, ""));
    layout.add(makeFloat(pid::reverbMix, "Hall Mix", 0.0f, 1.0f, fx.reverbMix, 0.0f, ""));
    layout.add(makeFloat(pid::masterVol, "Master", -60.0f, 6.0f, fx.masterDb, 0.0f, "dB"));

    layout.add(makeFloat(pid::perfPitch, "Perf Pitch", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeFloat(pid::perfRate, "Perf Rate", 0.25f, 4.0f, 1.0f, 1.0f, "x"));
    layout.add(makeFloat(pid::perfDepth, "Perf Tiefe", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeFloat(pid::perfSweep, "Perf Sweep", -24.0f, 24.0f, 0.0f, 0.0f, "st"));
    layout.add(makeChoice(pid::perfTarget, "Perf Ziel", { "Fokus", "Alle" }, 0));
    layout.add(makeChoice(pid::latchStop, "Latch bei Stopp", { "Weiterlaufen", "Ausklingen", "Sofort stoppen" }, 1));
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
                    layout.add(makeFloat(id, name, spec.min, spec.max, def, spec.skewCentre, u8(spec.unit)));
                    break;
                case FieldKind::Choice:
                    layout.add(makeChoice(id, name, toStringArray(spec.choices), static_cast<int>(std::lround(def))));
                    break;
                case FieldKind::Bool:
                    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, kParameterVersion }, name, def > 0.5f));
                    break;
            }
        }
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
}

ParamCache::ParamCache(juce::AudioProcessorValueTreeState& apvts)
    : drive_(raw(apvts, pid::drive)), cutoff_(raw(apvts, pid::cutoff)), resonance_(raw(apvts, pid::resonance)),
      filterType_(raw(apvts, pid::filterType)), delayTime_(raw(apvts, pid::delayTime)),
      delayFeedback_(raw(apvts, pid::delayFeedback)), delayTone_(raw(apvts, pid::delayTone)),
      delayWow_(raw(apvts, pid::delayWow)), delayMix_(raw(apvts, pid::delayMix)),
      reverbDecay_(raw(apvts, pid::reverbDecay)), reverbTone_(raw(apvts, pid::reverbTone)),
      reverbMix_(raw(apvts, pid::reverbMix)), masterVol_(raw(apvts, pid::masterVol)),
      perfPitch_(raw(apvts, pid::perfPitch)), perfRate_(raw(apvts, pid::perfRate)),
      perfDepth_(raw(apvts, pid::perfDepth)), perfSweep_(raw(apvts, pid::perfSweep)),
      perfTarget_(raw(apvts, pid::perfTarget)), latchStop_(raw(apvts, pid::latchStop)),
      panic_(raw(apvts, pid::panic))
{
    for (int s = 0; s < kNumSlots; ++s)
        for (int i = 0; i < kNumSlotFields; ++i)
            slots_[static_cast<std::size_t>(s)][static_cast<std::size_t>(i)] =
                raw(apvts, slotParamId(s, static_cast<SlotField>(i)));
}

void ParamCache::read(EngineParams& out) const
{
    for (std::size_t s = 0; s < slots_.size(); ++s)
        for (std::size_t i = 0; i < slots_[s].size(); ++i)
            setSlotField(out.slots[s], static_cast<SlotField>(i), load(slots_[s][i]));

    FxParams& fx = out.global.fx;
    fx.drive = load(drive_);
    fx.cutoffHz = load(cutoff_);
    fx.resonance = load(resonance_);
    fx.filterType = load(filterType_);
    fx.delayDiv = static_cast<DelayDivision>(loadIndex(delayTime_, kNumDelayDivisions - 1));
    fx.delayFeedback = load(delayFeedback_);
    fx.delayTone = load(delayTone_);
    fx.delayWow = load(delayWow_);
    fx.delayMix = load(delayMix_);
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
