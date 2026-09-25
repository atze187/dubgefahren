#include "engine/SlotParams.h"

namespace dg {

float syncDivisionBeats(SyncDivision d)
{
    switch (d)
    {
        case SyncDivision::D1_32:  return 0.125f;
        case SyncDivision::D1_16T: return 1.0f / 6.0f;
        case SyncDivision::D1_16:  return 0.25f;
        case SyncDivision::D1_8T:  return 1.0f / 3.0f;
        case SyncDivision::D1_8:   return 0.5f;
        case SyncDivision::D1_4T:  return 2.0f / 3.0f;
        case SyncDivision::D1_4:   return 1.0f;
        case SyncDivision::D1_2:   return 2.0f;
        case SyncDivision::Bar1:   return 4.0f;
        case SyncDivision::Bars2:  return 8.0f;
        case SyncDivision::Bars4:  return 16.0f;
    }
    return 1.0f;
}

} // namespace dg
