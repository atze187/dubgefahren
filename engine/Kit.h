#pragma once
#include <array>
#include <string>
#include "engine/SlotParams.h"

namespace dg {

// Pad-Farben: Index in die Palette der Oberfläche; die Engine trägt ihn nur mit.
constexpr int kNumPadColours = 8;
inline int clampPadColour(int index) { return (index >= 0 && index < kNumPadColours) ? index : 0; }

struct Kit
{
    std::array<SlotParams, kNumSlots> slots {};
    std::array<std::string, kNumSlots> names {}; // UTF-8
    std::array<std::string, kNumSlots> samples {}; // UTF-8, relativ zu <Kit>/, nur bei Sample-Slots
    std::array<int, kNumSlots> colours {}; // Palettenindex 0 … kNumPadColours-1, 0 = Standard
};

Kit makeFactoryKit();
// Alle Slots leer und ohne Namen; die (unsichtbaren) Synth-Werte entsprechen dem Factory-Kit.
Kit makeEmptyKit();

} // namespace dg
