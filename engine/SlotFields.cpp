#include "engine/SlotFields.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace dg {

namespace {

constexpr const char* kWaveChoices[] = { "Sinus", "Dreieck", "Sägezahn", "Rechteck" };
constexpr const char* kLfoShapeChoices[] = { "Rechteck", "Dreieck", "Sägezahn auf", "Sägezahn ab", "Sample & Hold" };
constexpr const char* kSyncDivChoices[] = { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2",
                                            "1 Takt", "2 Takte", "4 Takte" };
constexpr const char* kTrigModeChoices[] = { "Gate", "Latch", "One-Shot" };
constexpr const char* kChokeChoices[] = { "Keine", "1", "2", "3", "4" };

using C = std::span<const char* const>;

const std::array<FieldSpec, kNumSlotFields> kSpecs { {
    { "wave",       "Welle",          FieldKind::Choice, 0.0f, 3.0f, 3.0f, 0.0f, "", C(kWaveChoices) },
    { "pw",         "Pulsbreite",     FieldKind::Float, 0.05f, 0.95f, 0.5f, 0.0f, "", {} },
    { "pitch",      "Tonhöhe",        FieldKind::Float, 20.0f, 5000.0f, 440.0f, 400.0f, "Hz", {} },
    { "lfoShape",   "LFO-Form",       FieldKind::Choice, 0.0f, 4.0f, 0.0f, 0.0f, "", C(kLfoShapeChoices) },
    { "lfoRate",    "LFO-Rate",       FieldKind::Float, 0.05f, 40.0f, 2.0f, 2.0f, "Hz", {} },
    { "lfoSync",    "LFO-Sync",       FieldKind::Bool, 0.0f, 1.0f, 0.0f, 0.0f, "", {} },
    { "lfoSyncDiv", "LFO-Sync-Rate",  FieldKind::Choice, 0.0f, 10.0f, 4.0f, 0.0f, "", C(kSyncDivChoices) },
    { "lfoDepth",   "LFO-Tiefe",      FieldKind::Float, 0.0f, 48.0f, 12.0f, 0.0f, "st", {} },
    { "sweepAmt",   "Sweep-Betrag",   FieldKind::Float, -48.0f, 48.0f, 0.0f, 0.0f, "st", {} },
    { "sweepTime",  "Sweep-Zeit",     FieldKind::Float, 0.01f, 10.0f, 0.5f, 0.5f, "s", {} },
    { "attack",     "Attack",         FieldKind::Float, 0.0f, 5.0f, 0.005f, 0.2f, "s", {} },
    { "release",    "Release",        FieldKind::Float, 0.0f, 10.0f, 0.3f, 0.5f, "s", {} },
    { "trigMode",   "Trigger-Modus",  FieldKind::Choice, 0.0f, 2.0f, 0.0f, 0.0f, "", C(kTrigModeChoices) },
    { "oneShot",    "One-Shot-Länge", FieldKind::Float, 0.05f, 10.0f, 1.0f, 1.0f, "s", {} },
    { "choke",      "Choke-Gruppe",   FieldKind::Choice, 0.0f, 4.0f, 0.0f, 0.0f, "", C(kChokeChoices) },
    { "vol",        "Lautstärke",     FieldKind::Float, -60.0f, 6.0f, -6.0f, 0.0f, "dB", {} },
    { "pan",        "Panorama",       FieldKind::Float, -1.0f, 1.0f, 0.0f, 0.0f, "", {} },
    { "send",       "FX-Send",        FieldKind::Float, 0.0f, 1.0f, 0.3f, 0.0f, "", {} },
} };

} // namespace

const FieldSpec& fieldSpec(SlotField f) { return kSpecs[static_cast<std::size_t>(f)]; }

float getSlotField(const SlotParams& p, SlotField f)
{
    switch (f)
    {
        case SlotField::Wave:          return static_cast<float>(p.wave);
        case SlotField::PulseWidth:    return p.pulseWidth;
        case SlotField::Pitch:         return p.pitchHz;
        case SlotField::LfoShape:      return static_cast<float>(p.lfoShape);
        case SlotField::LfoRate:       return p.lfoRateHz;
        case SlotField::LfoSync:       return p.lfoSync ? 1.0f : 0.0f;
        case SlotField::LfoSyncDiv:    return static_cast<float>(p.lfoSyncDiv);
        case SlotField::LfoDepth:      return p.lfoDepthSemis;
        case SlotField::SweepAmount:   return p.sweepSemis;
        case SlotField::SweepTime:     return p.sweepTimeS;
        case SlotField::Attack:        return p.attackS;
        case SlotField::Release:       return p.releaseS;
        case SlotField::TrigMode:      return static_cast<float>(p.trigMode);
        case SlotField::OneShotLength: return p.oneShotS;
        case SlotField::Choke:         return static_cast<float>(p.chokeGroup);
        case SlotField::Volume:        return p.volumeDb;
        case SlotField::Pan:           return p.pan;
        case SlotField::FxSend:        return p.fxSend;
        case SlotField::Count:         break;
    }
    return 0.0f;
}

void setSlotField(SlotParams& p, SlotField f, float value)
{
    const auto& s = fieldSpec(f);
    if (!std::isfinite(value))
        value = s.def;
    value = std::clamp(value, s.min, s.max);
    const int i = static_cast<int>(std::lround(value));

    switch (f)
    {
        case SlotField::Wave:          p.wave = static_cast<Waveform>(i); break;
        case SlotField::PulseWidth:    p.pulseWidth = value; break;
        case SlotField::Pitch:         p.pitchHz = value; break;
        case SlotField::LfoShape:      p.lfoShape = static_cast<LfoShape>(i); break;
        case SlotField::LfoRate:       p.lfoRateHz = value; break;
        case SlotField::LfoSync:       p.lfoSync = i != 0; break;
        case SlotField::LfoSyncDiv:    p.lfoSyncDiv = static_cast<SyncDivision>(i); break;
        case SlotField::LfoDepth:      p.lfoDepthSemis = value; break;
        case SlotField::SweepAmount:   p.sweepSemis = value; break;
        case SlotField::SweepTime:     p.sweepTimeS = value; break;
        case SlotField::Attack:        p.attackS = value; break;
        case SlotField::Release:       p.releaseS = value; break;
        case SlotField::TrigMode:      p.trigMode = static_cast<TriggerMode>(i); break;
        case SlotField::OneShotLength: p.oneShotS = value; break;
        case SlotField::Choke:         p.chokeGroup = i; break;
        case SlotField::Volume:        p.volumeDb = value; break;
        case SlotField::Pan:           p.pan = value; break;
        case SlotField::FxSend:        p.fxSend = value; break;
        case SlotField::Count:         break;
    }
}

SlotParams makeDefaultSlotParams()
{
    SlotParams p;
    for (int i = 0; i < kNumSlotFields; ++i)
    {
        const auto f = static_cast<SlotField>(i);
        setSlotField(p, f, fieldSpec(f).def);
    }
    return p;
}

std::optional<SlotField> slotFieldFromKey(std::string_view key)
{
    for (int i = 0; i < kNumSlotFields; ++i)
        if (key == kSpecs[static_cast<std::size_t>(i)].key)
            return static_cast<SlotField>(i);
    return std::nullopt;
}

} // namespace dg
