#include "engine/FxParams.h"
#include <algorithm>

namespace dg {

double delayDivisionSeconds(DelayDivision d, double bpm)
{
    static constexpr double kBeats[kNumDelayDivisions] = {
        1.0 / 6.0, 0.25, 0.375,   // 1/16T, 1/16, 1/16D
        1.0 / 3.0, 0.5, 0.75,     // 1/8T,  1/8,  1/8D
        2.0 / 3.0, 1.0, 1.5,      // 1/4T,  1/4,  1/4D
        4.0 / 3.0, 2.0, 3.0,      // 1/2T,  1/2,  1/2D
        4.0                       // 1/1
    };
    const double clampedBpm = std::clamp(bpm, 30.0, 400.0);
    return kBeats[static_cast<int>(d)] * 60.0 / clampedBpm;
}

} // namespace dg
