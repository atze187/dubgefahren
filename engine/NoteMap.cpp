#include "engine/NoteMap.h"

namespace dg {

namespace {
constexpr auto kRelaxed = std::memory_order_relaxed;
}

NoteMap::NoteMap()
{
    for (auto& s : slotOfNote_)
        s.store(-1, kRelaxed);
    for (auto& n : noteOfSlot_)
        n.store(-1, kRelaxed);
}

int NoteMap::slotForNote(int note) const
{
    if (note < 0 || note > 127)
        return -1;
    return slotOfNote_[static_cast<std::size_t>(note)].load(kRelaxed);
}

int NoteMap::noteForSlot(int slot) const
{
    if (slot < 0 || slot >= kNumSlots)
        return -1;
    return noteOfSlot_[static_cast<std::size_t>(slot)].load(kRelaxed);
}

void NoteMap::assign(int slot, int note)
{
    if (slot < 0 || slot >= kNumSlots || note < -1 || note > 127)
        return;
    const auto s = static_cast<std::size_t>(slot);
    const int old = noteOfSlot_[s].load(kRelaxed);
    if (old >= 0)
        slotOfNote_[static_cast<std::size_t>(old)].store(-1, kRelaxed);
    noteOfSlot_[s].store(-1, kRelaxed);
    if (note < 0)
        return;
    const int owner = slotOfNote_[static_cast<std::size_t>(note)].load(kRelaxed);
    if (owner >= 0)
        noteOfSlot_[static_cast<std::size_t>(owner)].store(-1, kRelaxed);
    slotOfNote_[static_cast<std::size_t>(note)].store(static_cast<std::int8_t>(slot), kRelaxed);
    noteOfSlot_[s].store(static_cast<std::int8_t>(note), kRelaxed);
}

void NoteMap::setAll(const std::array<int, kNumSlots>& notes)
{
    for (int s = 0; s < kNumSlots; ++s)
        assign(s, -1);
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int n = notes[static_cast<std::size_t>(s)];
        if (n >= 0 && n <= 127 && slotForNote(n) < 0)
            assign(s, n);
    }
}

std::array<int, kNumSlots> NoteMap::notes() const
{
    std::array<int, kNumSlots> out {};
    for (int s = 0; s < kNumSlots; ++s)
        out[static_cast<std::size_t>(s)] = noteForSlot(s);
    return out;
}

} // namespace dg
