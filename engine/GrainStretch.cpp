#include "engine/GrainStretch.h"
#include <algorithm>
#include <cmath>
#include "engine/SampleInterp.h"

namespace dg {

namespace {
float readGrain(const std::vector<float>& x, double pos, const GrainFrame& f)
{
    const double span = f.hi - f.lo;
    if (f.cycle && span > 0.0)
    {
        pos = f.lo + std::fmod(pos - f.lo, span);
        if (pos < f.lo)
            pos += span;
    }
    else
        pos = std::clamp(pos, f.lo, std::max(f.lo, f.hi - 1.0));
    return cubicAt(x, pos);
}
} // namespace

void GrainStretch::reset()
{
    for (auto& g : grains_)
        g.active = false;
    untilNext_ = 0;
}

void GrainStretch::launch(double srcPos, double dir, int grainLen, bool fadeIn)
{
    // Freien Platz nehmen, sonst das älteste Stück ersetzen (nur bei plötzlich kleinerer Stückgröße).
    Grain* slot = &grains_[0];
    for (auto& g : grains_)
        if (!g.active)
        {
            slot = &g;
            break;
        }
        else if (g.age > slot->age)
            slot = &g;
    slot->active = true;
    slot->fadeIn = fadeIn;
    slot->pos = srcPos;
    slot->dir = dir;
    slot->age = 0;
    slot->len = grainLen;
    slot->fade = std::max(1, grainLen / 4);
    untilNext_ = grainLen;
}

void GrainStretch::start(double srcPos, double dir, int grainLen)
{
    reset();
    launch(srcPos, dir, grainLen, false);
}

float GrainStretch::process(const std::vector<float>& x, double srcPos, int grainLen, const GrainFrame& f)
{
    if (untilNext_ <= 0)
        launch(srcPos, f.dir, grainLen, true);
    --untilNext_;

    float y = 0.0f;
    for (auto& g : grains_)
    {
        if (!g.active)
            continue;
        float gain = 1.0f;
        if (g.fadeIn && g.age < g.fade)
            gain = static_cast<float>(g.age) / static_cast<float>(g.fade);
        if (g.age >= g.len)
            gain *= static_cast<float>(g.len + g.fade - g.age) / static_cast<float>(g.fade);
        y += gain * readGrain(x, g.pos, f);
        g.pos += g.dir * f.step;
        if (++g.age >= g.len + g.fade)
            g.active = false;
    }
    return y;
}

} // namespace dg
