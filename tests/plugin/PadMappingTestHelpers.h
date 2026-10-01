#pragma once
#include "plugin/PadMapping.h"

namespace dgtest {

// Stellt die globale Pad-Belegung beim Verlassen des Tests auf den Zustand davor.
class ScopedPadMapping
{
public:
    ScopedPadMapping() : saved_(dg::PadMapping::instance().settings()) {}
    ~ScopedPadMapping()
    {
        auto& m = dg::PadMapping::instance();
        m.setFirstNote(saved_.firstNote);
        m.setOrigin(saved_.origin);
    }
    ScopedPadMapping(const ScopedPadMapping&) = delete;
    ScopedPadMapping& operator=(const ScopedPadMapping&) = delete;

private:
    dg::PadMappingSettings saved_;
};

} // namespace dgtest
