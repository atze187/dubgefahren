#pragma once
#include <array>
#include <string>
#include "engine/SlotParams.h"

namespace dg {

struct Kit
{
    std::array<SlotParams, kNumSlots> slots {};
    std::array<std::string, kNumSlots> names {}; // UTF-8
};

Kit makeFactoryKit();
// Alle Slots leer und ohne Namen; die (unsichtbaren) Synth-Werte entsprechen dem Factory-Kit.
Kit makeEmptyKit();

} // namespace dg
