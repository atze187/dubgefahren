#include "engine/DspMath.h"

namespace dg {

float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

float volumeDbToGain(float db) { return db <= kMinVolumeDb ? 0.0f : dbToGain(db); }

} // namespace dg
