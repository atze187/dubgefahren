#pragma once
#include "plugin/PadMapping.h"

namespace dgtest {

// Stellt die globale Pad-Belegung beim Verlassen des Tests auf den Zustand davor: Profile, die im Scope
// neu entstanden sind, werden gelöscht, das vorher aktive Profil samt Noten und Ursprung kommt zurück.
class ScopedPadMapping
{
public:
    ScopedPadMapping()
    {
        const auto& m = dg::PadMapping::instance();
        names_ = m.profileNames();
        saved_ = m.profile();
    }
    ~ScopedPadMapping()
    {
        auto& m = dg::PadMapping::instance();
        for (const auto& n : m.profileNames())
            if (!names_.contains(n) && m.selectProfile(n))
                m.deleteActiveProfile();
        if (!m.selectProfile(saved_.name))
            m.selectProfile("Default");
        for (int s = 0; s < dg::kNumSlots; ++s)
            m.setPadNote(s, -1); // erst alles frei, damit sich beim Zurücksetzen keine Noten gegenseitig verdrängen
        for (int s = 0; s < dg::kNumSlots; ++s)
            m.setPadNote(s, saved_.notes[static_cast<std::size_t>(s)]);
        m.setOrigin(saved_.origin);
    }
    ScopedPadMapping(const ScopedPadMapping&) = delete;
    ScopedPadMapping& operator=(const ScopedPadMapping&) = delete;

private:
    juce::StringArray names_;
    dg::PadProfile saved_;
};

} // namespace dgtest
