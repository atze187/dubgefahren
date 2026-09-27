#pragma once
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace dg::ui {

// Zeigt den Anteil der Rechenzeit von processBlock am Echtzeit-Budget eines Audio-Blocks.
// Liest die Quelle nur ein paar Mal pro Sekunde, damit die Zahl nicht flimmert.
class CpuMeter final : public juce::Label, private juce::Timer
{
public:
    CpuMeter();
    ~CpuMeter() override;

    void setSource(std::function<double()> source);
    void setLoad(double proportion);

private:
    void timerCallback() override;

    std::function<double()> source_;
};

} // namespace dg::ui
