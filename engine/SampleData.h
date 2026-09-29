#pragma once
#include <vector>

namespace dg {

// Dekodiertes Sample (mono). Nach dem Erzeugen unveränderlich; geteilt per
// std::shared_ptr<const SampleData>, die Engine sieht nur einen nicht besitzenden Zeiger.
struct SampleData
{
    std::vector<float> samples;
    double sampleRate = 44100.0;
};

} // namespace dg
