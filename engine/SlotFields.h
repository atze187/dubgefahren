#pragma once
#include <optional>
#include <span>
#include <string_view>
#include "engine/SlotParams.h"

namespace dg {

enum class SlotField
{
    Wave, PulseWidth, Pitch,
    LfoShape, LfoRate, LfoSync, LfoSyncDiv, LfoDepth,
    SweepAmount, SweepTime,
    Attack, Release,
    TrigMode, OneShotLength, Choke,
    Volume, Pan, FxSend,
    Count
};
constexpr int kNumSlotFields = static_cast<int>(SlotField::Count);

enum class FieldKind { Float, Choice, Bool };

struct FieldSpec
{
    const char* key;      // Parameter-ID-Suffix und Kit-JSON-Schlüssel
    const char* name;     // Anzeigename (UTF-8)
    FieldKind kind;
    float min;
    float max;
    float def;
    float skewCentre;     // 0 = linear, sonst Wert in Mittelstellung
    const char* unit;
    std::span<const char* const> choices; // nur bei Choice
};

const FieldSpec& fieldSpec(SlotField f);
float getSlotField(const SlotParams& p, SlotField f);
void setSlotField(SlotParams& p, SlotField f, float value);
SlotParams makeDefaultSlotParams();
std::optional<SlotField> slotFieldFromKey(std::string_view key);

} // namespace dg
