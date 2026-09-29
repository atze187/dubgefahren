#pragma once
#include <array>
#include <string>
#include "engine/SlotParams.h"

namespace dg {

struct Kit
{
    std::array<SlotParams, kNumSlots> slots {};
    std::array<std::string, kNumSlots> names {}; // UTF-8
    std::array<std::string, kNumSlots> samples {}; // UTF-8, relativ zu <Kit>/, nur bei Sample-Slots
};

Kit makeFactoryKit();
// Alle Slots leer und ohne Namen; die (unsichtbaren) Synth-Werte entsprechen dem Factory-Kit.
Kit makeEmptyKit();

} // namespace dg
