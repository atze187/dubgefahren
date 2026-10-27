#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include "engine/SlotParams.h"

namespace dg {

// Tabelle Note → Pad. Geschrieben wird nur im Message-Thread, gelesen darf der Audio-Thread
// (relaxed atomics: ein Block sieht entweder die alte oder die neue Belegung einer Note).
class NoteMap
{
public:
    NoteMap();

    int slotForNote(int note) const;
    int noteForSlot(int slot) const;
    // note -1 gibt das Pad frei; sonst verliert der bisherige Besitzer der Note sie, die alte Note des Pads wird frei.
    void assign(int slot, int note);
    // Ungültige Werte gelten als unbelegt; bei Duplikaten behält das frühere Pad die Note.
    void setAll(const std::array<int, kNumSlots>& notes);
    std::array<int, kNumSlots> notes() const;

private:
    std::array<std::atomic<std::int8_t>, 128> slotOfNote_;
    std::array<std::atomic<std::int8_t>, kNumSlots> noteOfSlot_;
};

} // namespace dg
